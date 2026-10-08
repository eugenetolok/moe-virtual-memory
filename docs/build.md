# Build and CI/CD

## Current state

This public repository contains the author-maintained **patch sources**, launchers, benchmark drivers and evidence. The source checkout used to compile historical rc14 was an **ignored local `llama.cpp` tree**, not committed in the private Git project.

The upstream base and the cumulative rc14 patch have since been recovered and independently verified: see [`source/`](../source/README.md). `source/upstream.sha` pins upstream commit `81bc6b83f827df746eb129235488d325c49cae52`, and `source/patch-series.txt` applies the audited cumulative patch that reproduces all 21 changed files byte-for-byte.

Recovering the source tree does **not** by itself qualify a binary. A green workflow still cannot claim to reproduce the historical 7.185 TPS artifact until the rebuilt source passes independent exactness and a physical M1 run, recorded in `source/RELEASE_APPROVED.json`.

For that reason, automated workflows distinguish **source checks** from **native release qualification**. Missing `source/upstream.sha` is an explicit release blocker, not a hidden fallback to the latest upstream.

## Requirements

- macOS with Apple Clang / Xcode command-line tools, `cmake`, `ninja`;
- a pinned compatible `llama.cpp` commit as a full 40-character SHA in `source/upstream.sha`;
- an audited `source/patch-series.txt` covering the complete relevant patch state, or a verified standalone public upstream/fork commit;
- a passing native exactness validation suite, not just syntax/build checks.

`scripts/bootstrap-upstream.sh` is the source reconstruction entry point. The native source is not considered reproducible until the pin and patch manifest are reviewed. The default current state is **not yet release-qualified**.

## CI/CD design

- `.github/workflows/source-checks.yml`: always available; checks shell syntax, Python syntax and public source/reference-file presence.
- `.github/workflows/macos-release.yml`: automatic on version tags *after* a source pin and reconstruction are in place. It checks out the pinned upstream tree, applies the audited patch manifest with fail-closed checks, cross-builds arm64 with macOS native toolchain and creates a checksum-bearing archive.
- The release workflow must **refuse** publication if a pinned upstream commit or a complete audited patch series is missing, if patch application fails, or if native validation is not established. A compilation success is **not** equivalent to rc14 byte-for-byte exactness.

## Why not clone `llama.cpp` HEAD?

The research backend hooks internal `llama.cpp` and `ggml` symbols, memory layouts, source locations and quantization kernels. Upstream changes may silently break the integration or exact floating-point semantics. A floating Git revision is unacceptable for a claimed reproducible result.

The historical rc14 artifact checksum is in [benchmarks.md](benchmarks.md), but that checksum cannot be expected from a fresh compiler/SDK build unless the entire toolchain/environment is also fixed.

## Source provenance and licensing

See [provenance.md](provenance.md) before redistributing a binary. Only original project-specific patches/launchers and sanitized research results should be exported. No model weights or privately captured traces are allowed in the public release.
