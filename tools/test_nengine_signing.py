import contextlib
import io
import json
import os
from pathlib import Path
import tempfile
from unittest.mock import patch
import unittest
from nengine_build_profiles import (
    BuildError, default_profiles, save_project_profiles,
    signing_preflight, main,
)


class SigningPreflightTests(unittest.TestCase):
    def test_missing_release_keystore_reports_action(self):
        with self.assertRaisesRegex(BuildError,"--signing-file"):
            signing_preflight(None)
        with tempfile.TemporaryDirectory() as root:
            save_project_profiles(Path(root),default_profiles())
            err=io.StringIO()
            with contextlib.redirect_stderr(err):
                result=main(["preflight",root,"--config","Release"])
            self.assertEqual(result,1)
            self.assertIn("--signing-file",err.getvalue())

    def test_present_valid_keystore_and_secret_names(self):
        with tempfile.TemporaryDirectory() as root:
            base=Path(root)
            key=base/"signed.jks";key.write_bytes(b"fixture keystore")
            config=base/"signing.json"
            data={"schema":1,"keystore":str(key),"alias":"production",
                  "store_password_env":"NENGINE_STORE_SECRET",
                  "key_password_env":"NENGINE_KEY_SECRET"}
            config.write_text(json.dumps(data))
            with patch.dict(os.environ,{"NENGINE_STORE_SECRET":"testing",
                                         "NENGINE_KEY_SECRET":"testing"}):
                self.assertTrue(signing_preflight(config))
            with patch.dict(os.environ,{"NENGINE_STORE_SECRET":"",
                                         "NENGINE_KEY_SECRET":"testing"}):
                with self.assertRaisesRegex(BuildError,"NENGINE_STORE_SECRET"):
                    signing_preflight(config)

    def test_missing_key_and_invalid_alias(self):
        with tempfile.TemporaryDirectory() as root:
            base=Path(root);config=base/"signing.json"
            data={"schema":1,"keystore":str(base/"missing.jks"),
                  "alias":"alias","store_password_env":"NENGINE_STORE_SECRET",
                  "key_password_env":"NENGINE_KEY_SECRET"}
            config.write_text(json.dumps(data))
            with self.assertRaisesRegex(BuildError,"keystore file not found"):
                signing_preflight(config)
            key=base/"existing.jks";key.write_bytes(b"test")
            data["keystore"]=str(key);data["alias"]="../wrong"
            config.write_text(json.dumps(data))
            with self.assertRaisesRegex(BuildError,"alias"):
                signing_preflight(config)

    def test_password_values_not_present_in_errors(self):
        with tempfile.TemporaryDirectory() as root:
            base=Path(root);key=base/"signed.jks";key.write_bytes(b"ok")
            config=base/"signing.json"
            config.write_text(json.dumps({"schema":1,"keystore":str(key),
              "alias":"prod","store_password_env":"NENGINE_STORE_SECRET",
              "key_password_env":"NENGINE_KEY_SECRET"}))
            with patch.dict(os.environ,{"NENGINE_STORE_SECRET":"private-password",
                                         "NENGINE_KEY_SECRET":""}):
                with self.assertRaises(BuildError) as problem:
                    signing_preflight(config)
            self.assertNotIn("private-password",str(problem.exception))


if __name__=="__main__":unittest.main()
