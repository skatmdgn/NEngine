#!/usr/bin/env python3
"""NEngine v1 dependency manifest, resolver and immutable SHA256 artifact cache.

No network access, arbitrary process execution or code loading is performed.
Editor/NuGet integration is separate; this module produces verified lock data.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import sys
import tempfile

NAME = re.compile(r"[A-Za-z][A-Za-z0-9_.-]{0,95}\Z")
VERSION = re.compile(r"(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)(?:-([A-Za-z0-9.-]+))?\Z")
HEX = re.compile(r"[0-9a-f]{64}\Z")
MAX_MANIFEST_BYTES = 65536
MAX_PACKAGE_BYTES = 64 * 1024 * 1024
MAX_DEPENDENCIES = 256


class PackageError(ValueError):
    pass


def _pair_unique(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise PackageError(f"duplicate field: {key}")
        result[key] = value
    return result


def _read_json(path: Path):
    if path.is_symlink() or not path.is_file() or path.stat().st_size > MAX_MANIFEST_BYTES:
        raise PackageError(f"unavailable, symlinked or oversized manifest: {path}")
    try:
        return json.loads(path.read_text(encoding="utf-8"), object_pairs_hook=_pair_unique)
    except (ValueError, UnicodeError) as exc:
        raise PackageError(f"invalid manifest JSON: {path}: {exc}") from exc


def _valid_name(name):
    return isinstance(name, str) and bool(NAME.fullmatch(name))


def _valid_version(version):
    return isinstance(version, str) and bool(VERSION.fullmatch(version))


def validate_manifest(doc):
    if not isinstance(doc, dict) or set(doc) != {"schema", "name", "version", "dependencies", "sha256"}:
        raise PackageError("manifest fields must be exactly schema,name,version,dependencies,sha256")
    if type(doc["schema"]) is not int or doc["schema"] != 1:
        raise PackageError("unsupported manifest schema")
    if not _valid_name(doc["name"]) or not _valid_version(doc["version"]):
        raise PackageError("invalid package name or semantic version")
    deps = doc["dependencies"]
    if not isinstance(deps, dict) or len(deps) > MAX_DEPENDENCIES:
        raise PackageError("dependencies must be a bounded name-to-version map")
    for name, version in deps.items():
        if not _valid_name(name) or not _valid_version(version):
            raise PackageError(f"invalid dependency: {name}: {version}")
        if name == doc["name"]:
            raise PackageError("direct self-dependency")
    if not isinstance(doc["sha256"], str) or not HEX.fullmatch(doc["sha256"]):
        raise PackageError("package sha256 must be 64 lowercase hex digits")
    return doc


def read_manifest(path: Path):
    return validate_manifest(_read_json(path))


def resolve(manifests, roots):
    """Return deterministic post-order, with full version conflict/cycle diagnostics."""
    catalog = {}
    for item in manifests:
        validate_manifest(item)
        name = item["name"]
        key = (name, item["version"])
        if key in catalog:
            raise PackageError(f"duplicate catalog entry {name}@{item['version']}")
        catalog[key] = item
    if len(catalog) > 8192:
        raise PackageError("catalog exceeds 8192 packages")

    selected = {}
    states = {}
    order = []

    def visit(name, version, chain):
        if not _valid_name(name) or not _valid_version(version):
            raise PackageError("invalid requested package")
        if name in selected and selected[name]["version"] != version:
            raise PackageError(
                f"version conflict for {name}: {selected[name]['version']} vs {version}, "
                f"dependency chain: {' -> '.join(chain)}")
        key = (name, version)
        if key not in catalog:
            raise PackageError(f"missing package {name}@{version} via {' -> '.join(chain)}")
        status = states.get(key, 0)
        if status == 1:
            raise PackageError(f"dependency cycle: {' -> '.join(chain + [name])}")
        if status == 2:
            return
        selected[name] = catalog[key]
        states[key] = 1
        for child, needed in sorted(catalog[key]["dependencies"].items()):
            visit(child, needed, chain + [name])
        states[key] = 2
        order.append(catalog[key])

    if not isinstance(roots, dict) or not roots or len(roots) > MAX_DEPENDENCIES:
        raise PackageError("roots must be a nonempty name-to-version map")
    for name, version in sorted(roots.items()):
        visit(name, version, [])
    return order


def build_lock(manifests, roots):
    ordered = resolve(manifests, roots)
    return {"schema": 1, "roots": dict(sorted(roots.items())), "packages": [
        {"name": p["name"], "version": p["version"], "sha256": p["sha256"]}
        for p in ordered]}


def lock_json(lock):
    return (json.dumps(lock, ensure_ascii=False, sort_keys=True,
                       indent=2, separators=(",", ": ")) + "\n").encode("utf-8")


def atomic_write(path: Path, data: bytes):
    """Do not follow symlink destination, never leave half-written lock files."""
    if path.is_symlink():
        raise PackageError("refusing symlink destination")
    if not path.parent.is_dir():
        raise PackageError("output parent directory does not exist")
    temp = None
    try:
        with tempfile.NamedTemporaryFile(dir=path.parent, prefix=".nengine-lock-",
                                         delete=False) as stream:
            temp = Path(stream.name)
            stream.write(data)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temp, path)
        temp = None
    finally:
        if temp is not None:
            temp.unlink(missing_ok=True)


def _sha256_file(source: Path):
    if source.is_symlink() or not source.is_file():
        raise PackageError("package source must be a regular non-symlinked file")
    if source.stat().st_size > MAX_PACKAGE_BYTES:
        raise PackageError("package archive exceeds 64 MiB")
    digest = hashlib.sha256()
    with source.open("rb") as f:
        while block := f.read(65536):
            digest.update(block)
    return digest.hexdigest()


def cache_archive(archive: Path, expected: str, root: Path):
    """Content-addressed, no overwrite of mismatched existing blobs."""
    if not isinstance(expected, str) or not HEX.fullmatch(expected):
        raise PackageError("invalid expected SHA256")
    if root.is_symlink() or not root.is_dir():
        raise PackageError("cache root missing or symlinked")
    if _sha256_file(archive) != expected:
        raise PackageError("archive SHA256 mismatch")
    destination = root / expected[:2] / expected
    if destination.parent.is_symlink():
        raise PackageError("cache shard cannot be a symlink")
    destination.parent.mkdir(parents=True, exist_ok=True)
    if destination.parent.is_symlink():
        raise PackageError("cache shard cannot be a symlink")
    if destination.exists() or destination.is_symlink():
        if _sha256_file(destination) != expected:
            raise PackageError("existing cache entry does not match SHA256")
        return destination
    temp = None
    try:
        with tempfile.NamedTemporaryFile(dir=destination.parent, prefix=".nengine-cache-",
                                         delete=False) as output:
            temp = Path(output.name)
            with archive.open("rb") as source:
                while block := source.read(65536):
                    output.write(block)
            output.flush()
            os.fsync(output.fileno())
        if _sha256_file(temp) != expected:
            raise PackageError("cache copy integrity failure")
        # os.link atomically creates a new inode but never replaces an
        # existing entry, unlike rename/replace. The source temp remains ours.
        try:
            os.link(temp, destination)
        except FileExistsError:
            if _sha256_file(destination) != expected:
                raise PackageError("concurrent cache entry differs")
        return destination
    finally:
        if temp is not None:
            temp.unlink(missing_ok=True)


def verify_lock(lock, cache_root):
    if cache_root.is_symlink() or not cache_root.is_dir():
        raise PackageError("invalid cache root")
    if not isinstance(lock, dict) or set(lock) != {"schema", "roots", "packages"} or lock["schema"] != 1:
        raise PackageError("invalid lockfile")
    packages = lock["packages"]
    if not isinstance(packages, list) or len(packages) > 8192:
        raise PackageError("invalid lock packages")
    unique = set()
    for item in packages:
        if not isinstance(item, dict) or set(item) != {"name", "version", "sha256"}:
            raise PackageError("bad lock entry")
        if not _valid_name(item["name"]) or not _valid_version(item["version"]) or not isinstance(item["sha256"], str) or not HEX.fullmatch(item["sha256"]):
            raise PackageError("malformed lock entry")
        if item["name"] in unique:
            raise PackageError("duplicate name in lockfile")
        unique.add(item["name"])
        expected = item["sha256"]
        blob = cache_root / expected[:2] / expected
        if blob.parent.is_symlink():
            raise PackageError("symlink cache shard rejected")
        if _sha256_file(blob) != expected:
            raise PackageError(f"cache corruption: {item['name']}")
    return True


def sync_managed_plugins(lock, cache_root: Path, managed_dir: Path):
    """PLUG-018: transactionally update/remove package-owned managed DLLs.

    Installed files are the byte-for-byte SHA-verified archives (DLL payloads),
    not executed by this tool. Non-package-managed plugin DLLs survive upgrades.
    A failed validation/copy leaves the previous installed directory intact.
    """
    verify_lock(lock, cache_root)
    managed_dir = Path(managed_dir)
    if managed_dir.is_symlink() or managed_dir.parent.is_symlink() or not managed_dir.parent.is_dir():
        raise PackageError("unsafe managed plugin installation root")
    if managed_dir.exists() and not managed_dir.is_dir():
        raise PackageError("managed plugin target is not a directory")
    state_name = ".nengine-managed-packages.json"
    old_files = set()
    if managed_dir.exists():
        for entry in managed_dir.iterdir():
            if entry.is_symlink() or not entry.is_file():
                raise PackageError("unsafe existing managed plugin entry")
        state = managed_dir / state_name
        if state.exists():
            recorded = _read_json(state)
            if not isinstance(recorded, dict) or set(recorded) != {"schema", "managed"} or recorded["schema"] != 1:
                raise PackageError("invalid existing managed plugin state")
            if not isinstance(recorded["managed"], list) or len(recorded["managed"]) > MAX_DEPENDENCIES:
                raise PackageError("invalid installed package inventory")
            for name in recorded["managed"]:
                if not isinstance(name, str) or not name.endswith(".dll") or not _valid_name(name[:-4]):
                    raise PackageError("malformed installed DLL name")
                if name in old_files:
                    raise PackageError("duplicate installed DLL name")
                old_files.add(name)
    new_files = {}
    for item in lock["packages"]:
        name = item["name"] + ".dll"
        if not _valid_name(item["name"]) or name in new_files:
            raise PackageError("invalid or duplicate managed plugin name")
        source = cache_root / item["sha256"][:2] / item["sha256"]
        if _sha256_file(source) != item["sha256"]:
            raise PackageError("managed plugin cache corruption")
        if managed_dir.exists() and (managed_dir / name).exists() and name not in old_files:
            raise PackageError("package would overwrite unmanaged user DLL")
        new_files[name] = source
    stage = Path(tempfile.mkdtemp(dir=managed_dir.parent, prefix=".nengine-plugins-stage-"))
    backup = None
    try:
        if managed_dir.exists():
            for entry in managed_dir.iterdir():
                if entry.name != state_name and entry.name not in old_files:
                    shutil.copy2(entry, stage / entry.name, follow_symlinks=False)
        for name, source in sorted(new_files.items()):
            shutil.copyfile(source, stage / name)
            if _sha256_file(stage / name) != _sha256_file(source):
                raise PackageError("staged managed plugin integrity failure")
        atomic_write(stage / state_name, (json.dumps({
            "schema": 1, "managed": sorted(new_files)
        }, sort_keys=True, indent=2) + "\n").encode("utf-8"))
        if managed_dir.exists():
            backup = Path(tempfile.mkdtemp(dir=managed_dir.parent, prefix=".nengine-plugins-backup-"))
            backup.rmdir()
            managed_dir.rename(backup)
        try:
            stage.rename(managed_dir)
        except OSError:
            if backup is not None:
                backup.rename(managed_dir)
                backup = None
            raise
        if backup is not None:
            shutil.rmtree(backup)
            backup = None
        return sorted(new_files)
    finally:
        if stage.exists():
            shutil.rmtree(stage)
        # If publication succeeds but backup deletion fails, retaining a
        # backup is safer than deleting the installed directory.

def main(argv=None):
    ap = argparse.ArgumentParser(description="NEngine offline package metadata and cache")
    sub = ap.add_subparsers(dest="command", required=True)
    validate = sub.add_parser("validate")
    validate.add_argument("manifest", type=Path)
    solve = sub.add_parser("lock")
    solve.add_argument("catalog_dir", type=Path)
    solve.add_argument("roots_file", type=Path)
    solve.add_argument("output", type=Path)
    cache = sub.add_parser("cache")
    cache.add_argument("archive", type=Path)
    cache.add_argument("sha256")
    cache.add_argument("directory", type=Path)
    verify = sub.add_parser("verify")
    verify.add_argument("lock_file", type=Path)
    verify.add_argument("directory", type=Path)
    installed = sub.add_parser("sync-managed")
    installed.add_argument("lock_file", type=Path)
    installed.add_argument("cache_directory", type=Path)
    installed.add_argument("managed_plugin_directory", type=Path)
    args = ap.parse_args(argv)
    try:
        if args.command == "validate":
            m = read_manifest(args.manifest)
            print(f"PASS {m['name']}@{m['version']}")
        elif args.command == "lock":
            files = sorted(args.catalog_dir.glob("*.npackage.json"))
            manifests = [read_manifest(p) for p in files]
            roots = _read_json(args.roots_file)
            atomic_write(args.output, lock_json(build_lock(manifests, roots)))
            print(f"PASS lockfile: {args.output}")
        elif args.command == "cache":
            print(cache_archive(args.archive, args.sha256, args.directory))
        elif args.command == "verify":
            if verify_lock(_read_json(args.lock_file), args.directory):
                print("PASS cached package archive integrity")
        elif args.command == "sync-managed":
            installed_files = sync_managed_plugins(_read_json(args.lock_file),
                args.cache_directory, args.managed_plugin_directory)
            print(f"PASS installed {len(installed_files)} managed plug-in DLLs")
        return 0
    except (PackageError, OSError) as exc:
        print(f"Package error: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
