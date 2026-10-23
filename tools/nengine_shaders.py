#!/usr/bin/env python3
"""User shader registry with reproducible compiler-version keyed SPIR-V caching.

No code runs from shader manifests. On compile failure the previously valid
SPIR-V remains intact and a reason is returned; invalid shaders cannot replace
last-known-good assets.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile

import compile_shader

class ShaderError(ValueError): pass

GUID = re.compile(r"[0-9a-f]{32}\Z")
VERS = re.compile(r"[A-Za-z0-9][A-Za-z0-9._+ -]{2,127}\Z")
SHADER_TYPES = (".vert.glsl",".frag.glsl",".comp.glsl",
                ".vert.hlsl",".frag.hlsl",".comp.hlsl")
MAX_SHADER = 1024*1024
MAGIC = bytes([3,2,35,7])


def _no_duplicate(pairs):
    result={}
    for key,value in pairs:
        if key in result: raise ShaderError(f"duplicate shader metadata field: {key}")
        result[key]=value
    return result


def register_shader(project: Path, descriptor: Path):
    if not project.is_dir() or project.is_symlink() or descriptor.is_symlink() or not descriptor.is_file() or descriptor.stat().st_size>8192:
        raise ShaderError("shader project or descriptor unavailable/unsafe")
    try:
        record=json.loads(descriptor.read_text(encoding="utf-8"),object_pairs_hook=_no_duplicate)
    except (ValueError,UnicodeError) as exc:raise ShaderError(f"invalid shader metadata: {exc}") from exc
    if not isinstance(record,dict) or set(record)!={"schema","guid","source","compiler_version","target_env"} or record["schema"]!=1:
        raise ShaderError("invalid shader descriptor schema")
    guid=record["guid"]
    if not isinstance(guid,str) or not GUID.fullmatch(guid):
        raise ShaderError("shader GUID must be 32 lowercase hex characters")
    version=record["compiler_version"]
    if not isinstance(version,str) or not VERS.fullmatch(version):
        raise ShaderError("compiler version must be explicitly pinned")
    target=record["target_env"]
    if target not in ("vulkan1.0","vulkan1.1","vulkan1.2","vulkan1.3"):
        raise ShaderError("unknown Vulkan target environment")
    name=record["source"]
    if not isinstance(name,str) or "\\" in name or "\0" in name or len(name)>180:
        raise ShaderError("unsafe shader source")
    relative=Path(name)
    if relative.is_absolute() or any(p in ("..",".") for p in relative.parts) or not name.endswith(SHADER_TYPES):
        raise ShaderError("shader source must be relative within Assets and have known stage")
    source=project/"Assets"/relative
    if source.is_symlink() or not source.is_file() or source.stat().st_size>MAX_SHADER:
        raise ShaderError("missing/symlinked/oversized shader source")
    assets=(project/"Assets").resolve(strict=True)
    if not source.resolve(strict=True).is_relative_to(assets):
        raise ShaderError("shader escaped project Assets through symlink")
    # Also reject any symlink in the source's ancestor chain.
    chain=source.parent
    while chain!=project/"Assets":
        if chain.is_symlink():raise ShaderError("symlinked shader parent rejected")
        chain=chain.parent
    return record,source


def compiler_identity(compiler: Path):
    if not compiler.is_file() or compiler.is_symlink():
        raise ShaderError("shader compiler binary unavailable/unsafe")
    result=subprocess.run([str(compiler),"--version"],capture_output=True,text=True,
                          timeout=20,check=False)
    if result.returncode or not (result.stdout.strip() or result.stderr.strip()):
        raise ShaderError("shader compiler version cannot be verified")
    return (result.stdout+result.stderr).strip().splitlines()[0].strip()


def cache_key(contents: bytes, record: dict, compiler: str):
    stable=json.dumps({"guid":record["guid"],"target_env":record["target_env"],
                       "compiler_version":record["compiler_version"],"compiler":compiler},
                      sort_keys=True,separators=(",",":")).encode("utf-8")
    return hashlib.sha256(contents+b"\0"+stable).hexdigest()


def valid_spirv(blob: bytes):
    return len(blob)>=20 and len(blob)%4==0 and blob[:4]==MAGIC


def compile_registered(project: Path, descriptor: Path, compiler: Path,
                       compile_callback=None):
    record,source=register_shader(project,descriptor)
    identity=compiler_identity(compiler)
    # Pinned compiler identity prevents a silently changed backend affecting
    # caches or production shader binaries between users.
    if identity!=record["compiler_version"]:
        raise ShaderError(f"shader compiler mismatch: wanted {record['compiler_version']}; got {identity}")
    key=cache_key(source.read_bytes(),record,identity)
    root=project/"Library"/"Shaders"
    root.mkdir(parents=True,exist_ok=True)
    if root.is_symlink():raise ShaderError("shader cache root is a symlink")
    cache=root/(record["guid"]+"-"+key+".spv")
    if cache.is_symlink():raise ShaderError("shader cache entry is symlinked")
    if cache.is_file():
        blob=cache.read_bytes()
        if not valid_spirv(blob):
            raise ShaderError("shader cache corruption (invalid SPIR-V)")
        return cache,False
    temp=None
    try:
        with tempfile.NamedTemporaryFile(dir=root,prefix=".nengine-shader-",suffix=".spv",
                                         delete=False) as stream:
            temp=Path(stream.name)
        if compile_callback is None:
            command=[str(compiler),"-V","--target-env",record["target_env"],
                     "-S",compile_shader.shader_stage(source),"-o",str(temp),str(source)]
            if "glslc" in compiler.name.lower():
                command=[str(compiler),f"-fshader-stage={compile_shader.shader_stage(source)}",
                         f"--target-env={record['target_env']}","-o",str(temp),str(source)]
                if source.suffix.lower()==".hlsl":command[1:1]=["-x","hlsl"]
            elif source.suffix.lower()==".hlsl":command.insert(1,"-D")
            completed=subprocess.run(command,capture_output=True,text=True,
                                     timeout=60,check=False)
            if completed.returncode:
                raise ShaderError("shader compilation failed: "+(completed.stderr or completed.stdout)[-2048:])
        else:
            compile_callback(source,temp,record)
        blob=temp.read_bytes()
        if not valid_spirv(blob):
            raise ShaderError("compiler output missing SPIR-V header")
        os.replace(temp,cache)
        temp=None
        return cache,True
    finally:
        if temp is not None:temp.unlink(missing_ok=True)


def main(argv=None):
    parser=argparse.ArgumentParser(description="Compile registered user shader safely")
    parser.add_argument("project",type=Path)
    parser.add_argument("descriptor",type=Path)
    parser.add_argument("--compiler",required=True,type=Path)
    args=parser.parse_args(argv)
    try:
        path,compiled=compile_registered(args.project,args.descriptor,args.compiler)
        print(("compiled " if compiled else "cache hit ")+str(path))
        return 0
    except (ShaderError,OSError,subprocess.SubprocessError) as exc:
        print(f"Shader error: {exc}",file=sys.stderr)
        return 1


if __name__=="__main__":
    raise SystemExit(main())
