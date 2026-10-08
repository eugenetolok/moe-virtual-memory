#!/usr/bin/env bash
#
# build-tools.sh - build the three native rc14 test/compiler tools from the
# reconstructed llama.cpp static libraries.
#
# Prerequisite: release/build-rc.sh has configured and built the static
# libraries in MOE_BUILD_DIR (default llama.cpp/build-m1-rc).
#
# Outputs (default MOE_TOOLS_OUT=$BUILD_DIR/bin):
#   moe-tiled-selftest   native graph differential (synthetic exactness)
#   moe-tiled-benchmark  real-weight integrated benchmark / byte differential
#   moe-tile16-compiler  offline lossless tile16 sidecar compiler
#
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="${MOE_BUILD_DIR:-$ROOT/llama.cpp/build-m1-rc}"
OUT="${MOE_TOOLS_OUT:-$BUILD_DIR/bin}"
DEPLOY="${MOE_DEPLOYMENT_TARGET:-12.0}"
CXX="${CXX:-clang++}"

SRC="$ROOT/llama.cpp"
SIM="$ROOT/simulator"

# Reproducible path hygiene: keep build-host absolute paths out of diagnostics.
MAP=(-ffile-prefix-map="$ROOT"=. -ffile-prefix-map="$SRC"=. -ffile-prefix-map="$SIM"=. -ffile-prefix-map=../llama.cpp=.)
INC=(-I "$SRC/src" -I "$SRC/include" -I "$SRC/ggml/include" -I "$SRC/ggml/src" -I "$SRC/ggml/src/ggml-cpu")
LIBS=(
  "$BUILD_DIR/src/libllama.a"
  "$BUILD_DIR/ggml/src/libggml.a"
  "$BUILD_DIR/ggml/src/libggml-cpu.a"
  "$BUILD_DIR/ggml/src/ggml-metal/libggml-metal.a"
  "$BUILD_DIR/ggml/src/ggml-blas/libggml-blas.a"
  "$BUILD_DIR/ggml/src/libggml-base.a"
)
FRAMEWORKS=(-framework Accelerate -framework Foundation -framework Metal -framework MetalKit)

for f in "${LIBS[@]}"; do
  [[ -f "$f" ]] || { echo "missing $f; run release/build-rc.sh first" >&2; exit 1; }
done
mkdir -p "$OUT"

echo "building moe-tiled-selftest"
"$CXX" -O2 -std=c++17 -arch arm64 -mmacosx-version-min="$DEPLOY" "${MAP[@]}" "${INC[@]}" \
  "$SIM/exec023_differential.cpp" "${LIBS[@]}" "${FRAMEWORKS[@]}" -o "$OUT/moe-tiled-selftest"

echo "building moe-tiled-benchmark"
"$CXX" -O3 -std=c++17 -arch arm64 -mmacosx-version-min="$DEPLOY" "${MAP[@]}" "${INC[@]}" \
  "$SIM/exec024_model.cpp" "${LIBS[@]}" "${FRAMEWORKS[@]}" -o "$OUT/moe-tiled-benchmark"

echo "building moe-tile16-compiler"
"$CXX" -O2 -std=c++17 -arch arm64 -mmacosx-version-min="$DEPLOY" "${MAP[@]}" "${INC[@]}" \
  "$SIM/exec023_compile.cpp" "$SRC/src/llama-moe-tiled.cpp" -o "$OUT/moe-tile16-compiler"

echo "OK: $OUT"
for b in moe-tiled-selftest moe-tiled-benchmark moe-tile16-compiler; do
  file "$OUT/$b"
done
