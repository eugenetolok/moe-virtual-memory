# Audited source reconstruction

**Source state recovered and verified; binary release still gated.**

This directory pins the upstream `llama.cpp` base and the exact patch that turns it
into the audited rc14 native source tree used for the recorded M1 measurements. The
native *binary* release gate remains closed until an independent exactness run and a
physical M1 release run are recorded in `source/RELEASE_APPROVED.json`.

## Source lock

| File | Purpose |
|---|---|
| [`upstream.sha`](upstream.sha) | Immutable upstream commit `81bc6b83f827df746eb129235488d325c49cae52` from `ggml-org/llama.cpp` |
| [`patch-series.txt`](patch-series.txt) | Ordered patch list; one cumulative patch, `patches/025-rc14-complete-native-source.patch` |
| [`rc14-source-manifest.json`](rc14-source-manifest.json) | Per-file SHA-256 for all 21 changed files, plus the patch SHA-256 |
| [`rc14-build-environment.json`](rc14-build-environment.json) | Historical build options, toolchain versions and recorded rc14 artifact identities |

`scripts/bootstrap-upstream.sh` clones the pinned commit and applies the series.
`scripts/verify-rc14-source.py` checks the pin, the patch fingerprint and, when
`llama.cpp/` exists, every patched file byte-for-byte against the manifest.

## Provenance recovered

- The private `llama.cpp` checkout was a **squashed local snapshot** (`ddce98d`, "llama.cpp upstream snapshot"), not an upstream clone; it has no upstream remote. Its full tree matches upstream `81bc6b83...` on all 3,643 entries, minus three upstream-tracked files (a `.log`, `build-xcframework.sh`, and an IDE plist) that were excluded when the local repository was initialized. The upstream SHA is established by whole-tree comparison, not by a guessed revision.
- The rc14 working tree differs from that base by 21 files: the qwen35moe compatibility commit, the passive routing tracer, and the paging/tiled runtime.
- The historical per-execution patches under `patches/` are **not** a sequential series: applying the public set to the pinned base chains only through `0001` and `004` and fails at `009`, and `0002`/`0003`/`008` are absent from the public export. This is why the audited series is one cumulative patch.
- Recorded rc14 pins match: `src/llama-moe-paging.cpp` = `61ce1356...`, and the rc14 source manifest's `patch_sha256` `7773cdea...` is the private `patches/024-independent-cache-prefetch-overlap.patch`.
- The newest tracked source file in the audited tree is `src/llama-moe-paging.cpp` at the recorded rc14 build time; no tracked or untracked non-ignored source file postdates the rc14 build.

## Verification performed

1. Clean checkout of `81bc6b83...`; `patch -p1` applies the cumulative patch without fuzz.
2. Every one of the 21 patched files matches the audited private rc14 tree byte-for-byte (SHA-256 recorded in the manifest).
3. `scripts/bootstrap-upstream.sh` reconstructs the same tree end-to-end from GitHub.

## Still required before a public binary release

- A fresh native differential/exactness run on the reconstructed source.
- A physical Apple M1 run of a package built from it.
- `source/RELEASE_APPROVED.json` with `native_differential_status: PASS` and `m1_release_run_status: PASS`, as enforced by `scripts/check-release-gate.py`.

Do not create `source/RELEASE_APPROVED.json` without those runs.
