#!/bin/bash
#
# build-rc.sh - build the M1-safe, generic arm64 Release llama-server.
#
# Produces a self-contained binary with embedded Metal shaders and only system
# library dependencies. Requires the patched llama.cpp checkout (patches 003+004).
#
# Env: MOE_DEPLOYMENT_TARGET (default 12.0), JOBS (default 5)
#
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="${MOE_BUILD_DIR:-$ROOT/llama.cpp/build-m1-rc}"
DEPLOY="${MOE_DEPLOYMENT_TARGET:-12.0}"
JOBS="${JOBS:-5}"
CM="$ROOT/.tools/venv/bin/cmake"
[ -x "$CM" ] || CM=cmake

NINJA_BIN="${MOE_NINJA:-$ROOT/.tools/venv/bin/ninja}"
[ -x "$NINJA_BIN" ] || NINJA_BIN="$(command -v ninja)"

echo "configuring M1-safe build: arm64, deployment target $DEPLOY, GGML_NATIVE=OFF"
"$CM" -S "$ROOT/llama.cpp" -B "$BUILD_DIR" -G Ninja \
  -DCMAKE_MAKE_PROGRAM="$NINJA_BIN" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_OSX_DEPLOYMENT_TARGET="$DEPLOY" \
  -DCMAKE_C_FLAGS="-ffile-prefix-map=$ROOT=." \
  -DCMAKE_CXX_FLAGS="-ffile-prefix-map=$ROOT=." \
  -DGGML_NATIVE=OFF \
  -DGGML_METAL=ON \
  -DGGML_METAL_EMBED_LIBRARY=ON \
  -DBUILD_SHARED_LIBS=OFF \
  -DLLAMA_CURL=OFF \
  -DLLAMA_OPENSSL=OFF \
  -DLLAMA_BUILD_TESTS=OFF \
  -DLLAMA_BUILD_TOOLS=ON \
  -DLLAMA_BUILD_SERVER=ON \
  -DGGML_CCACHE=OFF

echo "building llama-server with -j $JOBS"
"$CM" --build "$BUILD_DIR" --target llama-server -j "$JOBS"
echo "OK: $BUILD_DIR/bin/llama-server"
