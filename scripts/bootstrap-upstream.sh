#!/usr/bin/env bash
# Clean source reconstruction; never guess an upstream commit or patch order.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PIN_FILE="$ROOT/source/upstream.sha"
SERIES="$ROOT/source/patch-series.txt"
if [[ ! -f "$PIN_FILE" || ! -f "$SERIES" ]]; then
  echo "BLOCKED: source/upstream.sha and source/patch-series.txt need source audit." >&2
  exit 65
fi
PIN="$(tr -d '[:space:]' < "$PIN_FILE")"
[[ "$PIN" =~ ^[0-9a-f]{40}$ ]] || { echo "Invalid immutable upstream SHA" >&2; exit 65; }
[[ -s "$SERIES" ]] || { echo "Empty patch series" >&2; exit 65; }
[[ ! -e "$ROOT/llama.cpp" ]] || { echo "Refusing to overwrite existing tree" >&2; exit 65; }
git clone --no-checkout https://github.com/ggml-org/llama.cpp "$ROOT/llama.cpp"
git -C "$ROOT/llama.cpp" fetch --no-tags origin "$PIN"
git -C "$ROOT/llama.cpp" checkout --detach "$PIN"
COUNT=0
while IFS= read -r line || [[ -n "$line" ]]; do
  [[ -z "$line" || "$line" == \#* ]] && continue
  [[ "$line" =~ ^patches/[A-Za-z0-9._-]+\.patch$ ]] || { echo "Unsafe patch path: $line" >&2; exit 65; }
  [[ -f "$ROOT/$line" ]] || { echo "Missing patch: $line" >&2; exit 65; }
  git -C "$ROOT/llama.cpp" apply --check "$ROOT/$line"
  git -C "$ROOT/llama.cpp" apply "$ROOT/$line"
  COUNT=$((COUNT+1))
done < "$SERIES"
[[ "$COUNT" -gt 0 ]] || { echo "No patches applied" >&2; exit 65; }
git -C "$ROOT/llama.cpp" diff --check
test -f "$ROOT/llama.cpp/src/llama-moe-paging.cpp" || {
  echo "Missing native paging source after patch series" >&2; exit 65;
}
echo "Reconstructed llama.cpp $PIN with $COUNT audited patches"
