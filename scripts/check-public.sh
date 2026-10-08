#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
for f in release/moe-serve release/moe-prepare-tiled release/build-rc.sh scripts/bootstrap-upstream.sh scripts/package-public.sh scripts/check-public.sh; do
  bash -n "$f"
done
python3 -m py_compile release/moe-m1-combinations
python3 - <<'PY'
import csv, hashlib, json, pathlib, re
root = pathlib.Path(".")
m = json.loads((root/"release/model-manifest.json").read_text())
o = (root/"results/gguf_expert_offsets.csv").read_bytes()
assert hashlib.sha256(o).hexdigest() == m["offsets_sha256"], "model offset table SHA mismatch"
rows = list(csv.DictReader(o.decode().splitlines()))
assert len(rows) == m["offsets_rows"] == m["n_layer"] == 40
patches = list(root.glob("patches/*.patch"))
assert len(patches) >= 16, "missing source patches"
for p in patches:
    b = p.read_bytes()
    assert b"--- a/" in b and b"+++ b/" in b, p
    assert not re.search(rb'github_pat_[A-Za-z0-9_]+|ghp_[A-Za-z0-9_]{20,}|-----BEGIN [^-]+ PRIVATE KEY-----',b), p
print("PASS: syntax, model offsets, source patch structure and secret-pattern checks")
PY
