import unittest
from nengine_android_contributions import (
    ContributionError,merge_contributions,validate
)


def contrib(plugin,permissions=None,features=None,maven=None,api=35,min_sdk=23):
    return {"schema":1,"id":plugin,"min_sdk":min_sdk,"target_sdk":api,
            "permissions":permissions or [],"features":features or {},
            "maven":maven or []}


class AndroidContributionTests(unittest.TestCase):
    def test_valid_merge_order_deterministic(self):
        first=contrib("com.example.audio",["android.permission.INTERNET"],
                      {"android.hardware.microphone":False},
                      ["org.example:codec:1.0.0"])
        other=contrib("com.example.ads",["android.permission.INTERNET"],
                      {"android.hardware.camera":False},
                      ["com.example:ads:2.0.0"],min_sdk=26)
        a=merge_contributions([first,other])
        self.assertEqual(a,merge_contributions([other,first]))
        self.assertEqual(len(a["permissions"]),1)
        self.assertEqual(a["min_sdk"],26)
        self.assertEqual(a["maven"],["com.example:ads:2.0.0","org.example:codec:1.0.0"])

    def test_maven_conflict_rejected(self):
        with self.assertRaisesRegex(ContributionError,"conflicting Gradle"):
            merge_contributions([
                contrib("com.example.a",maven=["org.example:lib:1.0"]),
                contrib("com.example.b",maven=["org.example:lib:2.0"])
            ])

    def test_features_conflict_rejected(self):
        with self.assertRaisesRegex(ContributionError,"conflicting uses-feature"):
            merge_contributions([
                contrib("com.example.a",features={"android.hardware.camera":True}),
                contrib("com.example.b",features={"android.hardware.camera":False})
            ])

    def test_sdk_and_duplicate_rejected(self):
        with self.assertRaisesRegex(ContributionError,"higher target SDK"):
            merge_contributions([contrib("com.example.a",api=36)])
        with self.assertRaisesRegex(ContributionError,"duplicate plugin"):
            merge_contributions([contrib("com.example.a"),contrib("com.example.a")])
        with self.assertRaises(ContributionError):
            merge_contributions([contrib("com.example.a")],app_target=34)

    def test_malformed_permissions_and_artifacts(self):
        for x in ("android.permission.INTERNET;", "../com.example", "adb shell"):
            with self.subTest(x=x),self.assertRaises(ContributionError):
                validate(contrib("com.example.a",permissions=[x]))
        with self.assertRaises(ContributionError):
            validate(contrib("com.example.a",maven=["bad:artifact"]))
        with self.assertRaises(ContributionError):
            validate(contrib("com.example.a",features={"android.hardware.camera":"false"}))

if __name__=="__main__":unittest.main()
