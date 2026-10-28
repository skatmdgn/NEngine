#!/usr/bin/env python3
"""PLUG-012 Android ABI-native .so staging, not APK/NDK/device validation."""
from __future__ import annotations
import argparse
from pathlib import Path
import re
import shutil
import tempfile

SUPPORTED_ABI = ("arm64-v8a",)
SAFE_NAME = re.compile(r"lib[A-Za-z0-9_.-]{1,110}\\.so\\Z")


class NativePluginError(ValueError):
    pass


def stage_native_plugins(plugin_root: Path, android_project: Path, abi="arm64-v8a"):
    """Copy only verified ABI-specific flat libraries into Gradle jniLibs.

    No ABI is inferred from the filename: callers must supply a path laid out
    as Plugins/Android/<ABI>/lib*.so. Transactional publication protects the
    previous jniLibs selection if a validation or staging operation fails.
    """
    plugin_root = Path(plugin_root)
    android_project = Path(android_project)
    if abi not in SUPPORTED_ABI:
        raise NativePluginError("unsupported native ABI")
    sources = plugin_root / abi
    if plugin_root.is_symlink() or sources.is_symlink() or not sources.is_dir():
        raise NativePluginError("native ABI input directory missing or unsafe")
    target_root = android_project / "src" / "main" / "jniLibs"
    if target_root.is_symlink() or not target_root.is_dir():
        raise NativePluginError("Gradle jniLibs root missing or unsafe")
    target = target_root / abi
    if target.is_symlink() or (target.exists() and not target.is_dir()):
        raise NativePluginError("ABI output path invalid")
    candidates = []
    for item in sources.iterdir():
        if item.is_symlink() or not item.is_file() or not SAFE_NAME.fullmatch(item.name):
            raise NativePluginError("invalid or symlinked native library")
        if item.stat().st_size == 0 or item.stat().st_size > 64 * 1024 * 1024:
            raise NativePluginError("invalid native library byte length")
        candidates.append(item)
    if len(candidates) > 128:
        raise NativePluginError("native library count exceeds limit")
    # Each ABI is published separately; never remove other ABIs.
    temp = Path(tempfile.mkdtemp(dir=target_root, prefix=".nengine-native-"))
    backup = None
    try:
        for source in sorted(candidates):
            shutil.copyfile(source, temp / source.name)
        if target.exists():
            backup = Path(tempfile.mkdtemp(dir=target_root, prefix=".nengine-native-backup-"))
            backup.rmdir()
            target.rename(backup)
        try:
            temp.rename(target)
        except OSError:
            if backup is not None:
                backup.rename(target)
                backup = None
            raise
        if backup is not None:
            shutil.rmtree(backup)
            backup = None
        return tuple(p.name for p in sorted(candidates))
    finally:
        if temp.exists():
            shutil.rmtree(temp)


def main():
    parser = argparse.ArgumentParser(description="NEngine Android ABI-native staging (not APK)")
    parser.add_argument("plugin_root", type=Path)
    parser.add_argument("android_project", type=Path)
    parser.add_argument("--abi", choices=SUPPORTED_ABI, default="arm64-v8a")
    args = parser.parse_args()
    try:
        names = stage_native_plugins(args.plugin_root, args.android_project, args.abi)
    except (NativePluginError, OSError) as exc:
        parser.error(str(exc))
    print(f"Staged {len(names)} native {args.abi} libraries. APK not built.")


if __name__ == "__main__":
    main()
