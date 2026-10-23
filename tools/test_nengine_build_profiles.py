import json
import tempfile
import unittest
from pathlib import Path

from nengine_build_profiles import (
    BuildError, default_profiles, validate_profile,
    save_project_profiles, read_json, profile_for, android_preflight
)


class BuildSettingsTests(unittest.TestCase):
    def test_all_four_project_profiles_persist(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp)
            data=default_profiles("C:/sdk","C:/ndk","C:/jdk")
            path=save_project_profiles(root,data)
            restored=read_json(path)
            self.assertEqual(restored,data)
            self.assertEqual(profile_for(restored,"windows","Debug")["out"],
                             "Build/windows/Debug")
            self.assertEqual(profile_for(restored,"windows","Release")["out"],
                             "Build/windows/Release")
            self.assertEqual(profile_for(restored,"android","Debug")["android"]["abi"],
                             "arm64-v8a")
            self.assertEqual(profile_for(restored,"android","Release")["android"]["api"],35)
            self.assertFalse(list(root.glob(".nengine-build-*")))

    def test_prevent_build_directory_escape_and_collisions(self):
        for bad in ("../secret","Build/../bad","/absolute","Build\\evil"):
            data=default_profiles()
            data["profiles"]["windows-debug"]["out"]=bad
            with self.subTest(bad=bad),self.assertRaises(BuildError):
                validate_profile(data)
        data=default_profiles()
        data["profiles"]["windows-release"]["out"]=data["profiles"]["windows-debug"]["out"]
        with self.assertRaisesRegex(BuildError,"share"):
            validate_profile(data)

    def test_android_profile_requires_versions_and_arm64(self):
        for key,value in (("api",34),("ndk_major",26),("java_major",11),("abi","armeabi-v7a")):
            data=default_profiles()
            data["profiles"]["android-debug"]["android"][key]=value
            with self.subTest(key=key),self.assertRaises(BuildError):
                validate_profile(data)

    def test_no_mutation_on_invalid_save(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp)
            path=save_project_profiles(root,default_profiles())
            initial=path.read_bytes()
            modified=default_profiles()
            modified["profiles"]["windows-debug"]["configuration"]="Bad"
            with self.assertRaises(BuildError):
                save_project_profiles(root,modified)
            self.assertEqual(path.read_bytes(),initial)

    def test_android_preflight_fake_installed_layout(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp)
            sdk=root/"sdk"; ndk=root/"ndk"; jdk=root/"jdk"
            (sdk/"platforms"/"android-35").mkdir(parents=True)
            (sdk/"platforms"/"android-35"/"android.jar").write_bytes(b"fixture")
            (ndk/"build"/"cmake").mkdir(parents=True)
            (ndk/"source.properties").write_text("Pkg.Revision = 27.2.12479018\n")
            (ndk/"build"/"cmake"/"android.toolchain.cmake").write_text("fixture")
            (jdk/"bin").mkdir(parents=True)
            from os import name
            (jdk/"bin"/("java.exe" if name=="nt" else "java")).write_bytes(b"fixture")
            data=default_profiles(str(sdk),str(ndk),str(jdk))
            self.assertEqual([],android_preflight(data))
            (ndk/"source.properties").write_text("Pkg.Revision = 26.1.1\n")
            self.assertTrue(any("revision" in e for e in android_preflight(data)))
            (ndk/"source.properties").write_text("Pkg.Revision = 27.2.12479018\n")
            (sdk/"platforms"/"android-35"/"android.jar").unlink()
            self.assertTrue(any("platform" in e for e in android_preflight(data)))

    def test_does_not_trust_symlink_profile(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp)
            target=root/"outside.json"
            target.write_text(json.dumps(default_profiles()))
            link=root/"NEngine.nbuild.json"
            try: link.symlink_to(target)
            except OSError: self.skipTest("symlinks disallowed on this host")
            with self.assertRaises(BuildError):
                read_json(link)
            with self.assertRaises(BuildError):
                save_project_profiles(root,default_profiles())
            self.assertEqual(json.loads(target.read_text())["schema"],1)

    def test_reject_unknown_profile_fields_and_version(self):
        data=default_profiles()
        data["schema"]=2
        with self.assertRaises(BuildError):validate_profile(data)
        data=default_profiles()
        data["profiles"]["windows-debug"]["surprise"]="bad"
        with self.assertRaises(BuildError):validate_profile(data)


if __name__=="__main__":
    unittest.main()
