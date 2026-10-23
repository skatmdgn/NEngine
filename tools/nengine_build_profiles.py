#!/usr/bin/env python3
"""Project-relative, versioned Build Settings and Android toolchain preflight.

Profiles are portable JSON, not a Windows-specific Registry format.
No APK/NDK build or device success is claimed by a successful preflight.
"""
from __future__ import annotations
import argparse
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile

FORMAT = 1
MAX_SIZE = 32 * 1024
# Supported initial toolchain floor; higher versions may require validation
# against the target Gradle and NDK release before deployment.
MIN_ANDROID_API = 35
MIN_NDK_MAJOR = 27
MIN_JAVA_MAJOR = 17
SUPPORTED_ABIS = ("arm64-v8a",)


class BuildError(ValueError):
    pass


def _unique_pairs(pairs):
    seen = {}
    for k,v in pairs:
        if k in seen:
            raise BuildError(f"duplicate build setting: {k}")
        seen[k]=v
    return seen


def read_json(path):
    if path.is_symlink() or not path.is_file() or path.stat().st_size > MAX_SIZE:
        raise BuildError(f"missing/symlinked/oversized build file: {path}")
    try:
        return json.loads(path.read_text(encoding="utf-8"), object_pairs_hook=_unique_pairs)
    except (ValueError, UnicodeError) as exc:
        raise BuildError(f"invalid build JSON: {exc}") from exc


def validate_profile(data):
    """Require explicit Windows and Android profiles for each config."""
    if not isinstance(data, dict) or set(data) != {"schema","profiles"} or type(data["schema"]) is not int or data["schema"] != FORMAT:
        raise BuildError("invalid build settings version or fields")
    profiles = data["profiles"]
    if not isinstance(profiles, dict) or set(profiles) != {
        "windows-debug","windows-release","android-debug","android-release"
    }:
        raise BuildError("all four target/config profiles are required")
    for key, item in profiles.items():
        target, config = key.split("-")
        if not isinstance(item, dict) or set(item) != {"target","configuration","out","android"}:
            raise BuildError(f"invalid profile fields: {key}")
        if item["target"] != target or item["configuration"] != config.capitalize():
            raise BuildError(f"profile target/config mismatch: {key}")
        out = item["out"]
        if not isinstance(out,str) or len(out)>160 or not out or "\\" in out:
            raise BuildError(f"invalid build output: {key}")
        path=Path(out)
        if path.is_absolute() or any(p in (".","..","") for p in path.parts) or path.parts[0] != "Build":
            raise BuildError(f"output must be contained under project Build/: {key}")
        android=item["android"]
        if target=="windows":
            if android is not None: raise BuildError("Windows profile cannot contain Android toolchain")
        else:
            if not isinstance(android,dict) or set(android) != {
                "api","abi","ndk_major","java_major","sdk_root","ndk_root","jdk_root"
            }:
                raise BuildError("Android profile requires pinned toolchain selection")
            if any(type(android[n]) is not int for n in ("api","ndk_major","java_major")):
                raise BuildError("toolchain versions must be integer")
            if (android["api"] < MIN_ANDROID_API or
                android["ndk_major"] < MIN_NDK_MAJOR or
                android["java_major"] < MIN_JAVA_MAJOR or
                android["abi"] not in SUPPORTED_ABIS):
                raise BuildError("Android minimum API/NDK/JDK or ABI mismatch")
            for field in ("sdk_root","ndk_root","jdk_root"):
                value=android[field]
                if not isinstance(value,str) or len(value)>512 or "\0" in value:
                    raise BuildError(f"invalid Android {field}")
    outputs=[p["out"].casefold() for p in profiles.values()]
    if len(outputs)!=len(set(outputs)):
        raise BuildError("build configurations cannot share the same output directory")
    return data


def default_profiles(sdk_root="",ndk_root="",jdk_root=""):
    android={"api":MIN_ANDROID_API,"abi":"arm64-v8a",
             "ndk_major":MIN_NDK_MAJOR,"java_major":MIN_JAVA_MAJOR,
             "sdk_root":sdk_root,"ndk_root":ndk_root,"jdk_root":jdk_root}
    return {"schema":1,"profiles":{
        f"{target}-{config.lower()}":{
            "target":target,"configuration":config,
            "out":f"Build/{target}/{config}",
            "android":dict(android) if target=="android" else None}
        for target in ("windows","android") for config in ("Debug","Release")
    }}


def profile_for(settings,target,configuration):
    validate_profile(settings)
    key=f"{target.lower()}-{configuration.lower()}"
    if key not in settings["profiles"]: raise BuildError("unknown build configuration")
    return settings["profiles"][key]


def save_project_profiles(project_root,settings):
    validate_profile(settings)
    if not project_root.is_dir() or project_root.is_symlink():
        raise BuildError("project root must exist and cannot be a symlink")
    dest=project_root/"NEngine.nbuild.json"
    if dest.is_symlink(): raise BuildError("refusing symlinked Build Settings")
    data=(json.dumps(settings,sort_keys=True,indent=2,ensure_ascii=False)+"\n").encode("utf-8")
    temp=None
    try:
        with tempfile.NamedTemporaryFile(dir=project_root,prefix=".nengine-build-",
                                         delete=False) as stream:
            temp=Path(stream.name)
            stream.write(data)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temp,dest)
        temp=None
    finally:
        if temp is not None: temp.unlink(missing_ok=True)
    return dest


def android_preflight(settings, profile_key="android-debug"):
    validate_profile(settings)
    if profile_key not in ("android-debug","android-release"):
        raise BuildError("Android preflight requires Android profile")
    android=settings["profiles"][profile_key]["android"]
    paths = {k:Path(android[k]) for k in ("sdk_root","ndk_root","jdk_root")}
    failures=[]
    for name,path in paths.items():
        if not android[name] or path.is_symlink() or not path.is_dir():
            failures.append(f"{name} missing/invalid path: {path}")
    sdk=paths["sdk_root"]
    if sdk.is_dir():
        platform=sdk/"platforms"/f"android-{android['api']}"/"android.jar"
        if not platform.is_file() or platform.is_symlink():
            failures.append(f"missing Android platform SDK android-{android['api']}")
    ndk=paths["ndk_root"]
    if ndk.is_dir():
        meta=ndk/"source.properties"
        if not meta.is_file() or meta.is_symlink():
            failures.append("NDK source.properties missing")
        else:
            text=meta.read_text(encoding="utf-8")
            match=re.search(r"(?m)^Pkg\.Revision\s*=\s*(\d+)\.",text)
            if not match or int(match.group(1))!=android["ndk_major"]:
                failures.append(f"NDK revision does not match selected {android['ndk_major']}")
        cmake_toolchain=ndk/"build"/"cmake"/"android.toolchain.cmake"
        if not cmake_toolchain.is_file() or cmake_toolchain.is_symlink():
            failures.append("NDK CMake toolchain missing")
    jdk=paths["jdk_root"]
    java_exe=jdk/"bin"/("java.exe" if os.name=="nt" else "java")
    if jdk.is_dir() and (not java_exe.is_file() or java_exe.is_symlink()):
        failures.append("JDK java executable missing")
    # No shell invocation or untrusted SDK execution during discovery.
    # Java/Gradle actual execution belongs to crosscompile integration CI.
    return failures


def main(argv=None):
    parser=argparse.ArgumentParser(description="NEngine project build profiles")
    sub=parser.add_subparsers(dest="command",required=True)
    init=sub.add_parser("init");init.add_argument("project",type=Path)
    for field in ("sdk_root","ndk_root","jdk_root"):init.add_argument("--"+field.replace("_","-"),default="")
    inspect=sub.add_parser("inspect");inspect.add_argument("project",type=Path)
    inspect.add_argument("--target",choices=("windows","android"),required=True)
    inspect.add_argument("--config",choices=("Debug","Release"),required=True)
    preflight=sub.add_parser("preflight");preflight.add_argument("project",type=Path)
    preflight.add_argument("--config",choices=("Debug","Release"),default="Debug")
    args=parser.parse_args(argv)
    try:
        if args.command=="init":
            print(save_project_profiles(args.project,default_profiles(
                args.sdk_root,args.ndk_root,args.jdk_root)))
        else:
            conf=read_json(args.project/"NEngine.nbuild.json")
            if args.command=="inspect":
                print(json.dumps(profile_for(conf,args.target,args.config),indent=2))
            else:
                issues=android_preflight(conf,"android-"+args.config.lower())
                if issues: raise BuildError("; ".join(issues))
                print("Android toolchain preflight PASS (not APK/device validation)")
        return 0
    except (OSError,BuildError) as exc:
        print(f"NEngine Build Settings: {exc}",file=sys.stderr)
        return 1


if __name__=="__main__":
    raise SystemExit(main())
