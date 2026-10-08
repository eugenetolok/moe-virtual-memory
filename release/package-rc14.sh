#!/usr/bin/env bash
#
# package-rc14.sh - assemble the self-contained public-preview macOS arm64 package.
#
# Prerequisites:
#   release/build-rc.sh    -> llama.cpp/build-m1-rc/bin/llama-server (+ static libs)
#   release/build-tools.sh -> bin/moe-tiled-selftest, moe-tiled-benchmark, moe-tile16-compiler
#
# Output (dist/, not committed):
#   dist/<name>.tar.gz
#   dist/<name>.tar.gz.sha256
#
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="${MOE_BUILD_DIR:-$ROOT/llama.cpp/build-m1-rc}"
BIN_DIR="$BUILD_DIR/bin"
DIST="$ROOT/dist"
NAME="${MOE_PKG_NAME:-moe-virtual-memory-macos-arm64-rc14}"
VERSION="${MOE_VERSION:-0.7.0-m1-rc14}"
STAGE="$DIST/$NAME"

for b in llama-server moe-tiled-selftest moe-tiled-benchmark moe-tile16-compiler; do
  [[ -x "$BIN_DIR/$b" ]] || { echo "missing $BIN_DIR/$b; run release/build-rc.sh and release/build-tools.sh first" >&2; exit 1; }
done
for f in release/moe-serve release/moe-prepare-tiled release/moe-m1-combinations release/moe-m1-tiled-sweep release/model-manifest.json results/gguf_expert_offsets.csv; do
  [[ -e "$ROOT/$f" ]] || { echo "missing $ROOT/$f" >&2; exit 1; }
done

rm -rf "$STAGE"
mkdir -p "$STAGE/bin" "$STAGE/share" "$STAGE/licenses"

# Native executables (the four audited arm64 binaries)
install -m 0755 "$BIN_DIR/llama-server"          "$STAGE/bin/llama-server"
install -m 0755 "$BIN_DIR/moe-tiled-selftest"    "$STAGE/bin/moe-tiled-selftest"
install -m 0755 "$BIN_DIR/moe-tiled-benchmark"   "$STAGE/bin/moe-tiled-benchmark"
install -m 0755 "$BIN_DIR/moe-tile16-compiler"   "$STAGE/bin/moe-tile16-compiler"

# Helper scripts
install -m 0755 "$ROOT/release/moe-serve"            "$STAGE/bin/moe-serve"
install -m 0755 "$ROOT/release/moe-prepare-tiled"    "$STAGE/bin/moe-prepare-tiled"
install -m 0755 "$ROOT/release/moe-m1-combinations"  "$STAGE/bin/moe-m1-combinations"
install -m 0755 "$ROOT/release/moe-m1-tiled-sweep"   "$STAGE/bin/moe-m1-tiled-sweep"

# Data / manifests
install -m 0644 "$ROOT/release/model-manifest.json"       "$STAGE/share/model-manifest.json"
install -m 0644 "$ROOT/results/gguf_expert_offsets.csv"   "$STAGE/share/gguf_expert_offsets.csv"
install -m 0644 "$ROOT/source/rc14-source-manifest.json"  "$STAGE/share/rc14-source-manifest.json"
install -m 0644 "$ROOT/patches/025-rc14-complete-native-source.patch" "$STAGE/share/025-rc14-complete-native-source.patch"
install -m 0644 "$ROOT/scripts/verify-rc14-source.py"      "$STAGE/share/verify-rc14-source.py"
install -m 0644 "$ROOT/docs/exactness-rc14-preview.json"   "$STAGE/share/exactness-report.json"

# Tile16 tiling plan is a deterministic function of the public expert-offset table
# (40 layers x 256 experts x 3 parts = 30720 rows). No model bytes or private data.
python3 "$ROOT/scripts/gen-tile16-plan.py" \
  --offsets "$ROOT/results/gguf_expert_offsets.csv" \
  --output "$STAGE/share/tile16-plan.txt"

# Licenses / notices
install -m 0644 "$ROOT/LICENSE"               "$STAGE/licenses/PROJECT-LICENSE"
install -m 0644 "$ROOT/THIRD_PARTY_NOTICES.md" "$STAGE/licenses/THIRD_PARTY_NOTICES.md"
if [[ -f "$ROOT/llama.cpp/LICENSE" ]]; then
  install -m 0644 "$ROOT/llama.cpp/LICENSE"   "$STAGE/licenses/UPSTREAM-LLAMA-LICENSE"
fi

# Reproducibility metadata (no absolute personal paths)
git -C "$ROOT" rev-parse HEAD > "$STAGE/SOURCE-COMMIT" 2>/dev/null || echo unknown > "$STAGE/SOURCE-COMMIT"
cat "$ROOT/source/upstream.sha" > "$STAGE/SOURCE-UPSTREAM"
shasum -a 256 "$ROOT/patches/025-rc14-complete-native-source.patch" | awk '{print $1}' > "$STAGE/SOURCE-PATCH-SHA256"
"${CXX:-clang++}" --version 2>/dev/null | head -2 > "$STAGE/BUILD-COMPILER" || true
printf 'macOS %s\n' "$(sw_vers -productVersion 2>/dev/null || echo unknown)" > "$STAGE/BUILD-OS"
cat > "$STAGE/VERSION" <<EOF
moe-virtual-memory $VERSION
model Qwen3.6-35B-A3B (exact GGUF, not included)
native runtime: pinned upstream + audited cumulative rc14 patch
status: public preview / test pre-release (physical M1 verification pending)
EOF

if [[ -f "$ROOT/docs/prerelease-rc14.md" ]]; then
  install -m 0644 "$ROOT/docs/prerelease-rc14.md" "$STAGE/README.md"
fi
if [[ -f "$ROOT/docs/release-notes-rc14-preview.md" ]]; then
  install -m 0644 "$ROOT/docs/release-notes-rc14-preview.md" "$STAGE/RELEASE-NOTES.md"
fi

# Offset-table checksum gate (matches the public check-public.sh invariant)
WANT=$(sed -n 's/.*"offsets_sha256":[[:space:]]*"\([^"]*\)".*/\1/p' "$STAGE/share/model-manifest.json" | head -1)
GOT=$(shasum -a 256 "$STAGE/share/gguf_expert_offsets.csv" | awk '{print $1}')
[[ "$WANT" = "$GOT" ]] || { echo "offset-table checksum mismatch in package" >&2; exit 1; }

( cd "$STAGE" && find . -type f ! -name SHA256SUMS | sort | while read -r f; do shasum -a 256 "$f"; done > SHA256SUMS )
( cd "$DIST" && COPYFILE_DISABLE=1 tar czf "$NAME.tar.gz" "$NAME" )
( cd "$DIST" && shasum -a 256 "$NAME.tar.gz" > "$NAME.tar.gz.sha256" )

echo "OK: $DIST/$NAME.tar.gz"
cat "$DIST/$NAME.tar.gz.sha256"
