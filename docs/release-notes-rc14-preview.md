# rc14 public preview — release notes

**Test pre-release. Not a stable release. Physical Apple M1 verification pending.**

This package contains the native paging runtime rebuilt from the pinned upstream
`llama.cpp` commit plus the audited cumulative rc14 patch. It is published so the
maintainer and other Apple-silicon users can run the same exactness and timing
protocol on real hardware.

## Contents

| Path | What |
|---|---|
| `bin/llama-server` | patched arm64 server (static, Metal embedded) |
| `bin/moe-tiled-selftest` | native graph differential (synthetic exactness) |
| `bin/moe-tiled-benchmark` | real-weight integrated benchmark / byte differential |
| `bin/moe-tile16-compiler` | offline lossless tile16 sidecar compiler |
| `bin/moe-serve` | CPU-only paging launcher with M1 8 GB defaults |
| `bin/moe-prepare-tiled` | sidecar preparation + SHA verification |
| `bin/moe-m1-combinations` | 16-profile exactness + balanced timing driver |
| `bin/moe-m1-tiled-sweep` | raw-vs-tile16 exactness + timing driver |
| `share/` | model manifest, expert-offset table, tiling plan, source manifest, patch |
| `licenses/` | project MIT license, upstream llama.cpp license, third-party notices |
| `SHA256SUMS` | per-file checksums |
| `SOURCE-COMMIT`, `SOURCE-UPSTREAM`, `SOURCE-PATCH-SHA256` | reproducibility pins |

Model weights and the tile16 sidecar are **not** included.

## Source identity

- Upstream base: `81bc6b83f827df746eb129235488d325c49cae52` (`ggml-org/llama.cpp`).
- Patch: `025-rc14-complete-native-source.patch` (21 files); see `share/rc14-source-manifest.json`.
- The historical per-execution patches are cumulative snapshots and do not chain; a
  single verified cumulative patch is used.

## Verification status (this preview)

| Test | Result |
|---|---|
| Synthetic native graph differential (`moe-tiled-selftest`) | **PASS** — 6408 cases, 32040 operator comparisons, 256 ready masks, threads 1/4, zero mismatches |
| Real-weight production-path byte reproducibility (reference write vs read, raw layout) | **PASS** — 41 passes (25 prompt + 16 generated), 4,920 operator outputs, 161,218,560 bytes compared, 0 mismatches; identical tokens and ordered routes |
| Real-weight raw vs tile16 comparison | **NOT RUN** — the 20.9 GB sidecar is not present on the build host |
| Full 16-profile combinations matrix | **NOT RUN** — requires the sidecar and several hours |
| Physical Apple M1 8 GB | **PENDING the user's run** |

A compilation or synthetic pass is not a physical-M1 result. The stable release gate
remains closed: `source/RELEASE_APPROVED.json` is intentionally absent, and this
artifact is published only as a GitHub **pre-release**.

## Known limitations

- Do not compare these numbers directly with the historical rc14 medians; this is a new
  build on a different host and compiler.
- Requested I/O bytes are not physical NAND traffic; `F_NOCACHE` is best effort.
- Process RSS below the machine's RAM does not prove zero system memory pressure.
- See [benchmarks](../docs/benchmarks.md) for the historical measured rc14 matrix and
  its scoping caveats.
