#!/usr/bin/env python3
"""Deterministic Android SDK contributions merger.

Pure-data preflight: rejects contradictory package metadata, permissions,
features, Gradle dependencies or SDK ranges before running Gradle.
Does not execute untrusted scripts or fetch Maven packages.
"""
from __future__ import annotations
import argparse
import json
from pathlib import Path
import re
import sys

ID=re.compile(r"[A-Za-z_][A-Za-z0-9_]*(?:\.[A-Za-z_][A-Za-z0-9_]*)+\Z")
MAVEN=re.compile(r"[A-Za-z0-9_.-]+:[A-Za-z0-9_.-]+:[A-Za-z0-9][A-Za-z0-9_.+-]*\Z")
MAX_BYTES=65536

class ContributionError(ValueError):pass


def read_data(path:Path):
    if path.is_symlink() or not path.is_file() or path.stat().st_size>MAX_BYTES:
        raise ContributionError(f"unsafe or large Android contribution: {path}")
    def no_duplicates(pairs):
        data={}
        for key,value in pairs:
            if key in data:raise ContributionError(f"duplicate contribution field: {key}")
            data[key]=value
        return data
    try:return json.loads(path.read_text(encoding="utf-8"),object_pairs_hook=no_duplicates)
    except (ValueError,UnicodeError) as exc:
        raise ContributionError(f"malformed contribution JSON: {path}: {exc}") from exc


def validate(data):
    fields={"schema","id","min_sdk","target_sdk","permissions","features","maven"}
    if not isinstance(data,dict) or set(data)!=fields or type(data["schema"]) is not int or data["schema"]!=1:
        raise ContributionError("Android contribution schema/fields mismatch")
    if not isinstance(data["id"],str) or not ID.fullmatch(data["id"]):
        raise ContributionError("invalid plugin identifier")
    for key in ("min_sdk","target_sdk"):
        if type(data[key]) is not int or data[key]<21 or data[key]>100:
            raise ContributionError(f"invalid {key}")
    if data["min_sdk"]>data["target_sdk"]:
        raise ContributionError("minimum SDK newer than target SDK")
    perms=data["permissions"]
    if not isinstance(perms,list) or len(perms)>128 or any(
        not isinstance(item,str) or not ID.fullmatch(item) for item in perms):
        raise ContributionError("invalid Android permission")
    if len(set(perms))!=len(perms):raise ContributionError("duplicate permission")
    features=data["features"]
    if not isinstance(features,dict) or len(features)>128 or any(
        not isinstance(k,str) or not ID.fullmatch(k) or type(v) is not bool
        for k,v in features.items()):raise ContributionError("invalid feature requirements")
    maven=data["maven"]
    if not isinstance(maven,list) or len(maven)>128 or any(
        not isinstance(x,str) or not MAVEN.fullmatch(x) for x in maven):
        raise ContributionError("invalid Maven artifact coordinates")
    if len(set(maven))!=len(maven):raise ContributionError("duplicate Maven dependency")
    return data


def merge_contributions(items,app_target=35):
    if not items:raise ContributionError("no contributions")
    if type(app_target) is not int or app_target<35 or app_target>100:
        raise ContributionError("app target API must be explicitly >= 35")
    names=set()
    permissions=set()
    features={}
    artifacts={}
    minimum=21
    for item in sorted(items,key=lambda x:x.get("id","")):
        validate(item)
        identifier=item["id"]
        if identifier in names:raise ContributionError(f"duplicate plugin ID: {identifier}")
        names.add(identifier)
        minimum=max(minimum,item["min_sdk"])
        if app_target<item["target_sdk"]:
            raise ContributionError(f"{identifier} requires a higher target SDK: {item['target_sdk']}")
        permissions.update(item["permissions"])
        for feature,required in item["features"].items():
            if feature in features and features[feature]!=required:
                raise ContributionError(f"conflicting uses-feature required flag: {feature}")
            features[feature]=required
        for coordinate in item["maven"]:
            group,name,version=coordinate.split(":")
            key=(group,name)
            if key in artifacts and artifacts[key]!=version:
                raise ContributionError(f"conflicting Gradle/Maven dependency {group}:{name}: "
                                        f"{artifacts[key]} vs {version}")
            artifacts[key]=version
    if minimum>app_target:
        raise ContributionError("minimum SDK exceeds app target")
    return {"schema":1,"target_sdk":app_target,"min_sdk":minimum,
            "plugin_ids":sorted(names),"permissions":sorted(permissions),
            "features":dict(sorted(features.items())),
            "maven":[f"{g}:{n}:{v}" for (g,n),v in sorted(artifacts.items())]}


def main(argv=None):
    p=argparse.ArgumentParser(description="NEngine Android manifest and Gradle contribution audit")
    p.add_argument("--target-api",type=int,default=35)
    p.add_argument("--output",type=Path,required=True)
    p.add_argument("contributions",nargs="+",type=Path)
    args=p.parse_args(argv)
    try:
        data=merge_contributions([read_data(x) for x in args.contributions],
                                 args.target_api)
        if args.output.is_symlink() or not args.output.parent.is_dir():
            raise ContributionError("unsafe Android merge output")
        # Explicitly fail if output exists: no partially applied Gradle changes.
        with args.output.open("x",encoding="utf-8") as stream:
            json.dump(data,stream,indent=2,sort_keys=True)
            stream.write("\n")
        print(f"Android contributions PASS: {len(data['plugin_ids'])} plugins, "
              f"{len(data['maven'])} Gradle dependencies")
        return 0
    except (ContributionError,OSError) as exc:
        print(f"Android contribution error: {exc}",file=sys.stderr)
        return 1

if __name__=="__main__":raise SystemExit(main())
