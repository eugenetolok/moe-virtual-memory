#!/usr/bin/env bash
# Build-only package. Not a validated rc14 release without independent gates.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="$ROOT/llama.cpp/build-m1-rc/bin/llama-server"
[[ -x "$SRC" ]] || { echo "Run release/build-rc.sh first" >&2; exit 1; }
NAME="moe-virtual-memory-macos-arm64-${GITHUB_REF_NAME:-dev}"
DIR="$ROOT/dist/$NAME"
mkdir -p "$DIR/bin" "$DIR/share"
cp "$SRC" "$DIR/bin/llama-server"
cp "$ROOT/release/moe-serve" "$DIR/bin/moe-serve"
chmod +x "$DIR/bin/"*
cp "$ROOT/release/model-manifest.json" "$DIR/share/model-manifest.json"
cp "$ROOT/results/gguf_expert_offsets.csv" "$DIR/share/gguf_expert_offsets.csv"
[[ ! -f "$ROOT/llama.cpp/LICENSE" ]] || cp "$ROOT/llama.cpp/LICENSE" "$DIR/UPSTREAM-LLAMA-LICENSE"
[[ ! -f "$ROOT/LICENSE" ]] || cp "$ROOT/LICENSE" "$DIR/PROJECT-LICENSE"
[[ ! -f "$ROOT/THIRD_PARTY_NOTICES.md" ]] || cp "$ROOT/THIRD_PARTY_NOTICES.md" "$DIR/THIRD_PARTY_NOTICES.md"
cp "$ROOT/docs/benchmarks.md" "$DIR/BENCHMARKS-HISTORICAL.md"
cp "$ROOT/docs/reproduce.md" "$DIR/REPRODUCE.md"
echo "Public repo revision: $(git -C "$ROOT" rev-parse HEAD)" > "$DIR/SOURCE-COMMIT"
echo "Development source build: historical M1 benchmark is NOT automatically reproduced." > "$DIR/BUILD-STATUS"
(cd "$DIR"; find . -type f ! -name SHA256SUMS -print0 | sort -z | xargs -0 shasum -a 256 > SHA256SUMS)
(cd "$ROOT/dist"; COPYFILE_DISABLE=1 tar -czf "$NAME.tar.gz" "$NAME"; shasum -a 256 "$NAME.tar.gz" > "$NAME.tar.gz.sha256")
echo "Built archive: dist/$NAME.tar.gz"
