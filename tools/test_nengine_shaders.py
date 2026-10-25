import json
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

import nengine_shaders as shaders

GUID = "a"*32
VERSION = "Test shader compiler v1"
SPIRV = bytes([3,2,35,7])+bytes(16)


def descriptor():
    return {"schema":1,"guid":GUID,"source":"effects/flame.frag.glsl",
            "compiler_version":VERSION,"target_env":"vulkan1.0"}


class ShaderRegistryTests(unittest.TestCase):
    def setUp(self):
        self.folder=tempfile.TemporaryDirectory()
        self.addCleanup(self.folder.cleanup)
        self.project=Path(self.folder.name)
        (self.project/"Assets"/"effects").mkdir(parents=True)
        self.source=self.project/"Assets"/"effects"/"flame.frag.glsl"
        self.source.write_text("void main() { }\n")
        self.metadata=self.project/"flame.nshader"
        self.metadata.write_text(json.dumps(descriptor()))
        self.compiler=self.project/"fake-compiler"
        self.compiler.write_bytes(b"test tool")

    def build(self,callback):
        with patch.object(shaders,"compiler_identity",return_value=VERSION):
            return shaders.compile_registered(self.project,self.metadata,
                                               self.compiler,callback)

    def test_cache_key_version_pin_and_last_good_fallback(self):
        def compile_ok(source,output,settings):
            self.assertEqual(source,self.source)
            self.assertEqual(settings["compiler_version"],VERSION)
            output.write_bytes(SPIRV)
        path,compiled=self.build(compile_ok)
        self.assertTrue(compiled)
        self.assertEqual(path.read_bytes(),SPIRV)
        self.assertEqual(shaders.resolve_last_good(self.project,GUID),path)
        path2,again=self.build(lambda *_:self.fail("should be cached"))
        self.assertEqual(path,path2)
        self.assertFalse(again)
        self.source.write_text("broken shader")
        def compile_fail(*_):
            raise shaders.ShaderError("GLSL syntax error near brace")
        with self.assertRaisesRegex(shaders.ShaderError,"GLSL syntax"):
            self.build(compile_fail)
        self.assertEqual(shaders.resolve_last_good(self.project,GUID),path)
        self.assertEqual(path.read_bytes(),SPIRV)
        self.assertFalse(list(path.parent.glob(".nengine-shader-*")))
        updated,new=self.build(compile_ok)
        self.assertTrue(new)
        self.assertNotEqual(path,updated)
        self.assertEqual(shaders.resolve_last_good(self.project,GUID),updated)

    def test_reject_source_traversal_and_external_scheme(self):
        for name in ("../secret.frag.glsl","/tmp/e.frag.glsl",
                     "effects/../../secret.frag.glsl","effects/x.exe","C:\\Windows\\bad.frag.glsl"):
            with self.subTest(name=name):
                data=descriptor();data["source"]=name
                self.metadata.write_text(json.dumps(data))
                with self.assertRaises(shaders.ShaderError):
                    shaders.register_shader(self.project,self.metadata)

    def test_compiler_exact_identity_mismatch(self):
        with patch.object(shaders,"compiler_identity",return_value="v2"):
            with self.assertRaisesRegex(shaders.ShaderError,"compiler mismatch"):
                shaders.compile_registered(self.project,self.metadata,self.compiler)
        self.assertFalse((self.project/"Library"/"Shaders"/f"{GUID}.current").exists())

    def test_bad_spirv_cannot_replace_last_good(self):
        path,_=self.build(lambda source,output,settings:output.write_bytes(SPIRV))
        self.source.write_text("bad output")
        with self.assertRaisesRegex(shaders.ShaderError,"SPIR-V"):
            self.build(lambda source,output,settings:output.write_bytes(b"invalid"))
        self.assertEqual(shaders.resolve_last_good(self.project,GUID),path)

    def test_cache_corruption_rejected(self):
        path,_=self.build(lambda source,output,settings:output.write_bytes(SPIRV))
        path.write_bytes(b"corrupt")
        with self.assertRaisesRegex(shaders.ShaderError,"corruption"):
            self.build(lambda *_:self.fail("must not recompile corrupted artifact"))
        with self.assertRaisesRegex(shaders.ShaderError,"corrupt"):
            shaders.resolve_last_good(self.project,GUID)

    def test_reject_duplicate_metadata_fields_and_bad_version(self):
        self.metadata.write_text('{"schema":1,"schema":1}')
        with self.assertRaises(shaders.ShaderError):
            shaders.register_shader(self.project,self.metadata)
        data=descriptor()
        data["compiler_version"]=""
        self.metadata.write_text(json.dumps(data))
        with self.assertRaises(shaders.ShaderError):
            shaders.register_shader(self.project,self.metadata)

    def test_shader_stage_and_glslc_hlsl_language_supported(self):
        from compile_shader import shader_stage
        self.assertEqual(shader_stage(Path("flat.comp.hlsl")),"comp")
        self.assertEqual(shader_stage(Path("flat.frag.glsl")),"frag")
        with self.assertRaises(ValueError):
            shader_stage(Path("flat.txt"))


if __name__=="__main__":
    unittest.main()
