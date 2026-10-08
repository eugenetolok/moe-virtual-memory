# Audited source reconstruction

**Not yet qualified.** The measured rc14 runtime came from an ignored local `llama.cpp` checkout. The repository provides individual patches, but a complete compatible upstream SHA and a tested patch order have not been recovered from the committed evidence.

Required to enable native release CI:

1. Populate `source/upstream.sha` with the verified upstream 40-character commit SHA (no moving branch/tag).
2. Populate `source/patch-series.txt` with the **audited** ordered patches to apply. Early patches may be cumulative; do not simply apply every filename alphabetically.
3. Audit and check final source hashes against historical rc14 native/source pins. Reproduce native exactness tests.
4. Supply a reviewed redistribution license and third-party notices.
5. Only then cut a version tag to trigger automated macOS arm64 release packaging.

See [build](../docs/build.md) and [release process](../docs/release-process.md). A failure or skip due to missing source locks is intentional; it prevents a new unchecked binary from being misrepresented as the measured artifact.
