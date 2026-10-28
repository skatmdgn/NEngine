import tempfile
import unittest
from pathlib import Path
from nengine_android_native import stage_native_plugins, NativePluginError


class AndroidNativeStagingTests(unittest.TestCase):
    def test_abi_packaging_and_stale_plugin_removal(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp)
            source=root/"Plugins"/"Android"/"arm64-v8a"
            target=root/"gradle"/"src"/"main"/"jniLibs"
            source.mkdir(parents=True);target.mkdir(parents=True)
            (source/"libalpha.so").write_bytes(b"fixture-elf")
            self.assertEqual(stage_native_plugins(source.parent,root/"gradle"),
                             ("libalpha.so",))
            self.assertEqual((target/"arm64-v8a"/"libalpha.so").read_bytes(),
                             b"fixture-elf")
            (source/"libalpha.so").unlink()
            (source/"libbeta.so").write_bytes(b"new")
            self.assertEqual(stage_native_plugins(source.parent,root/"gradle"),
                             ("libbeta.so",))
            self.assertFalse((target/"arm64-v8a"/"libalpha.so").exists())
            self.assertEqual((target/"arm64-v8a"/"libbeta.so").read_bytes(),b"new")
            self.assertFalse(list(target.glob(".nengine-native-*")))

    def test_fail_closed_on_bad_names_and_wrong_abi(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);source=root/"Plugins"/"Android"/"arm64-v8a"
            target=root/"gradle"/"src"/"main"/"jniLibs"
            source.mkdir(parents=True);target.mkdir(parents=True)
            (source/"libgood.so").write_bytes(b"old")
            stage_native_plugins(source.parent,root/"gradle")
            with self.assertRaises(NativePluginError):
                stage_native_plugins(source.parent,root/"gradle","armeabi-v7a")
            (source/"../escaped.so").exists() # No path traversal allowed by listing
            (source/"bad.txt").write_text("not a native library")
            with self.assertRaises(NativePluginError):
                stage_native_plugins(source.parent,root/"gradle")
            self.assertEqual((target/"arm64-v8a"/"libgood.so").read_bytes(),b"old")


if __name__=="__main__":
    unittest.main()
