#!/usr/bin/env python3
"""Fail closed: compiling is not independent benchmark certification."""
import hashlib,json,pathlib,re,sys
root=pathlib.Path(__file__).resolve().parents[1]
required=["source/upstream.sha","source/patch-series.txt","source/RELEASE_APPROVED.json","LICENSE"]
missing=[x for x in required if not (root/x).is_file()]
if missing:
    sys.exit("Release BLOCKED. Owner-reviewed requirements missing: "+", ".join(missing))
pin=(root/"source/upstream.sha").read_text().strip()
assert re.fullmatch(r"[0-9a-f]{40}",pin)
patch=(root/"source/patch-series.txt").read_bytes()
meta=json.loads((root/"source/RELEASE_APPROVED.json").read_text())
for field in ("upstream_sha","patch_series_sha256","project_license_approved","native_differential_status","m1_release_run_status"):
    assert field in meta, "Incomplete release approval: "+field
assert meta["upstream_sha"]==pin
assert meta["patch_series_sha256"]==hashlib.sha256(patch).hexdigest()
assert meta["project_license_approved"] is True
assert meta["native_differential_status"]=="PASS"
assert meta["m1_release_run_status"]=="PASS"
print("Release qualification manifest passes (does not independently rerun physical M1).")
