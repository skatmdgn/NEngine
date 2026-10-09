#!/usr/bin/env python3
import argparse
import os
import pathlib
import shutil
import subprocess
import sys


def shader_stage(path: pathlib.Path) -> str:
    name = path.name.lower()
    for suffix, stage in (
        (".vert.glsl", "vert"),
        (".frag.glsl", "frag"),
        (".comp.glsl", "comp"),
        (".geom.glsl", "geom"),
        (".tesc.glsl", "tesc"),
        (".tese.glsl", "tese"),
        (".vert.hlsl", "vert"),
        (".frag.hlsl", "frag"),
        (".comp.hlsl", "comp"),
    ):
        if name.endswith(suffix):
            return stage
    raise ValueError(f"cannot infer shader stage from {path.name}")


def candidates(explicit: str | None):
    if explicit:
        yield pathlib.Path(explicit)

    sdk = os.environ.get("VULKAN_SDK")
    if sdk:
        for base in ("Bin", "bin"):
            for exe in ("glslangValidator", "glslangValidator.exe", "glslc", "glslc.exe"):
                yield pathlib.Path(sdk) / base / exe

    for name in ("glslangValidator", "glslc"):
        found = shutil.which(name)
        if found:
            yield pathlib.Path(found)


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Compile one GLSL/HLSL shader to SPIR-V using Vulkan SDK tools.")
    parser.add_argument("source", type=pathlib.Path)
    parser.add_argument("output", type=pathlib.Path)
    parser.add_argument("--compiler")
    parser.add_argument("--target-env", default="vulkan1.0")
    args = parser.parse_args()

    source = args.source.resolve()
    output = args.output.resolve()

    if not source.is_file():
        parser.error(f"shader source does not exist: {source}")

    stage = shader_stage(source)

    compiler = None
    for candidate in candidates(args.compiler):
        if candidate.is_file():
            compiler = candidate.resolve()
            break

    if compiler is None:
        print(
            "No shader compiler found. Install the Vulkan SDK/glslang-tools, "
            "or pass --compiler.", file=sys.stderr)
        return 2

    output.parent.mkdir(parents=True, exist_ok=True)

    lower = compiler.name.lower()
    if "glslangvalidator" in lower:
        command = [
            str(compiler),
            "-V",
            "--target-env",
            args.target_env,
            "-S",
            stage,
            "-o",
            str(output),
            str(source),
        ]
        if source.suffix.lower() == ".hlsl":
            command.insert(1, "-D")
    elif "glslc" in lower:
        command = [
            str(compiler),
            f"-fshader-stage={stage}",
            f"--target-env={args.target_env}",
            "-o",
            str(output),
            str(source),
        ]
    else:
        print(f"Unsupported compiler executable: {compiler}", file=sys.stderr)
        return 2

    print("Shader compiler:", compiler)
    print("Command:", " ".join(command))
    completed = subprocess.run(command, check=False)
    if completed.returncode != 0:
        return completed.returncode

    data = output.read_bytes()
    if len(data) < 20 or len(data) % 4 != 0:
        print("Compiler produced an invalid SPIR-V byte stream.", file=sys.stderr)
        return 3

    magic = int.from_bytes(data[:4], "little")
    if magic != 0x07230203:
        print(f"Unexpected SPIR-V magic: 0x{magic:08x}", file=sys.stderr)
        return 3

    print(f"Wrote {output} ({len(data)} bytes, {len(data)//4} words)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
