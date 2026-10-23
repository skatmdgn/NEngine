import hashlib
import json
from pathlib import Path
import tempfile
import unittest

from nengine_packages import (
    PackageError, validate_manifest, resolve, build_lock, lock_json, atomic_write,
    read_manifest, cache_archive, verify_lock,
)


def item(name, deps=None, version="1.0.0", sha=None):
    return {"schema": 1, "name": name, "version": version,
            "dependencies": deps or {}, "sha256": sha or "a"*64}


class PackageManifestTests(unittest.TestCase):
    def test_name_and_version_validation(self):
        self.assertEqual(validate_manifest(item("com.example.foo"))["name"], "com.example.foo")
        for name in ("../escape", "space bad", "_private", "a"*97):
            with self.subTest(name=name), self.assertRaises(PackageError):
                validate_manifest(item(name))
        for version in ("latest", "^1.0.0", "1.2", "1.2.3+build", "01.2.3"):
            with self.subTest(version=version), self.assertRaises(PackageError):
                validate_manifest(item("foo", version=version))
        with self.assertRaises(PackageError):
            validate_manifest(item("self", {"self": "1.0.0"}))

    def test_v1_json_bounds_and_duplicate_fields(self):
        with tempfile.TemporaryDirectory() as tmp:
            p = Path(tmp) / "foo.npackage.json"
            p.write_text(json.dumps(item("foo")), encoding="utf-8")
            self.assertEqual(read_manifest(p)["name"], "foo")
            p.write_text('{"schema":1,"schema":1}', encoding="utf-8")
            with self.assertRaises(PackageError):
                read_manifest(p)
            p.write_bytes(b"x"*65537)
            with self.assertRaises(PackageError):
                read_manifest(p)


class PackageResolverTests(unittest.TestCase):
    def test_transitive_order_deterministic(self):
        catalog = [item("app", {"z": "1.0.0", "a": "1.0.0"}),
                   item("z", {"base": "1.0.0"}),
                   item("a", {"base": "1.0.0"}), item("base")]
        output = resolve(catalog, {"app":"1.0.0"})
        self.assertEqual([x["name"] for x in output], ["base", "a", "z", "app"])
        self.assertEqual(lock_json(build_lock(catalog, {"app":"1.0.0"})),
                         lock_json(build_lock(catalog[::-1], {"app":"1.0.0"})))

    def test_conflicting_versions_not_picked_silently(self):
        catalog = [item("app", {"a":"1.0.0","b":"1.0.0"}),
                   item("a", {"shared":"1.0.0"}), item("b", {"shared":"2.0.0"}),
                   item("shared", version="1.0.0"),
                   item("shared", version="2.0.0")]
        with self.assertRaisesRegex(PackageError, "version conflict"):
            resolve(catalog, {"app":"1.0.0"})

    def test_cycle_and_missing_package_errors(self):
        with self.assertRaisesRegex(PackageError,"cycle"):
            resolve([item("a",{"b":"1.0.0"}), item("b",{"a":"1.0.0"})],
                    {"a":"1.0.0"})
        with self.assertRaisesRegex(PackageError,"missing"):
            resolve([item("a",{"b":"1.0.0"})], {"a":"1.0.0"})
        with self.assertRaisesRegex(PackageError,"duplicate catalog"):
            resolve([item("a"),item("a")], {"a":"1.0.0"})


class PackageCacheTests(unittest.TestCase):
    def test_verified_content_addressed_cache(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            archive = root/"lib.bin"
            archive.write_bytes(b"\x00hello\npackage\xff")
            sha = hashlib.sha256(archive.read_bytes()).hexdigest()
            cache_root = root/"cache"
            cache_root.mkdir()
            stored = cache_archive(archive, sha, cache_root)
            self.assertEqual(stored.read_bytes(), archive.read_bytes())
            self.assertEqual(cache_archive(archive, sha, cache_root), stored)
            lock = build_lock([item("lib", sha=sha)], {"lib":"1.0.0"})
            self.assertTrue(verify_lock(lock, cache_root))
            stored.write_bytes(b"tamper!")
            with self.assertRaises(PackageError):
                cache_archive(archive, sha, cache_root)
            with self.assertRaises(PackageError):
                verify_lock(lock, cache_root)

    def test_rejects_bad_archive_hash_and_oversize(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp)
            archive=root/"lib.bin"
            archive.write_bytes(b"evil")
            cache_root=root/"cache";cache_root.mkdir()
            with self.assertRaisesRegex(PackageError,"mismatch"):
                cache_archive(archive,"0"*64,cache_root)
            archive.write_bytes(b"\0"*(64*1024*1024+1))
            with self.assertRaisesRegex(PackageError,"exceeds"):
                cache_archive(archive,hashlib.sha256(b"evil").hexdigest(),cache_root)

    def test_lock_atomic_transactional(self):
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp)/"lock.json"
            atomic_write(p,b"old")
            atomic_write(p,b"new")
            self.assertEqual(p.read_bytes(),b"new")
            self.assertFalse(list(Path(tmp).glob(".nengine-lock-*")))

    @unittest.skipUnless(hasattr(Path, "symlink_to"), "symlink unavailable")
    def test_refuse_symlink_cache_target(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp)
            source=root/"source"
            source.write_bytes(b"source")
            link=root/"link"
            try:
                link.symlink_to(source)
            except OSError:
                self.skipTest("host prohibits symlink fixture")
            with self.assertRaises(PackageError):
                read_manifest(link)
            with self.assertRaises(PackageError):
                atomic_write(link,b"data")
            self.assertEqual(source.read_bytes(), b"source")


if __name__ == "__main__":
    unittest.main()
