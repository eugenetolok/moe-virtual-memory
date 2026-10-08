# Release qualification checklist

A GitHub Actions build is not necessarily a **validated inference release**. Before creating a public version tag and promoting it:

- [ ] Confirm the project's chosen distribution license and preserve upstream dependency notices.
- [ ] Pin exactly one upstream `llama.cpp` SHA and every imported source patch.
- [ ] Verify a clean checkout + audited patch reconstruction, both `git apply --check` and `--reverse --check` as appropriate; independently check final source hashes.
- [ ] Build macOS arm64 binaries; check architecture, deployment target, dylibs, toolchain, SHA-256.
- [ ] Build and include the native expert benchmark, self-test and (if advertising it) optional tile16 compiler. Never package placeholder tooling.
- [ ] Run synthetic exactness including signed8 extremes and overflow guards, model-weight native differential, 16-profile byte-exact outputs, ordered native Top8 routes and correct cache/P0/prefetch behavior.
- [ ] Measure a *fresh* Apple M1 8 GiB host using the actual release package; record decode timings, swap and RSS, startup separate from generated-token TPS.
- [ ] Publish checksum file, source+binary manifest, all measured configurations and limitations.
- [ ] If a gate fails, ship only a labeled `development` artifact; do not state that it reproduces historical rc14 or release measured TPS as new CI performance.

Historical rc14 was a prerelease; evidence demonstrates the run but not a ready-for-everyone source build. See [benchmarks](benchmarks.md).
