#!/usr/bin/env python3
"""Fail closed: verify the recovered rc14 native source against its recorded pins.

Always checks the immutable upstream pin, the patch-series fingerprint and the
per-file manifest. If llama.cpp/ has been reconstructed (scripts/bootstrap-upstream.sh),
it also checks every patched file byte-for-byte.

This does NOT certify a compiled binary or rerun any benchmark; the binary release
gate stays in scripts/check-release-gate.py.
"""
import hashlib
import json
import pathlib
import re
import sys

root = pathlib.Path(__file__).resolve().parents[1]
failures = []


def sha256(path: pathlib.Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


pin = (root / "source/upstream.sha").read_text().strip()
if not re.fullmatch(r"[0-9a-f]{40}", pin):
    failures.append(f"invalid upstream pin: {pin!r}")

manifest = json.loads((root / "source/rc14-source-manifest.json").read_text())
if manifest["upstream_sha"] != pin:
    failures.append("manifest upstream_sha disagrees with source/upstream.sha")

series = []
for line in (root / "source/patch-series.txt").read_text().splitlines():
    line = line.strip()
    if not line or line.startswith("#"):
        continue
    if not re.fullmatch(r"patches/[A-Za-z0-9._-]+\.patch", line):
        failures.append(f"unsafe patch path: {line!r}")
        continue
    series.append(line)

if not series:
    failures.append("empty patch series")

for rel in series:
    patch = root / rel
    if not patch.is_file():
        failures.append(f"missing patch: {rel}")
        continue
    if patch.name == pathlib.Path(manifest["patch_file"]).name and sha256(patch) != manifest["patch_sha256"]:
        failures.append(f"patch fingerprint mismatch: {rel}")

tree = root / "llama.cpp"
if tree.is_dir():
    for rel, expected in sorted(manifest["files"].items()):
        path = tree / rel
        if not path.is_file():
            failures.append(f"missing reconstructed file: {rel}")
        elif sha256(path) != expected:
            failures.append(f"source hash mismatch: {rel}")
    checked = len(manifest["files"])
    print(f"Reconstructed tree present: verified {checked} patched files against the manifest.")
else:
    print("llama.cpp/ absent: verified pins and patch fingerprints only (run scripts/bootstrap-upstream.sh to check files).")

if failures:
    for f in failures:
        print("FAIL:", f, file=sys.stderr)
    sys.exit(1)
print(f"PASS: rc14 source pins verified against upstream {pin}")
