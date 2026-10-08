# Source provenance, safety and licensing

This is a **curated export** from the author's private research repository, not a mirror of its whole Git history. Its purpose is to make the measured paging/runtime design inspectable without exposing future research branches, raw local traces, internal build directories, full model weights or identity-bearing logs.

## Source artifacts

- `patches/`: human-readable C/C++ changes to `llama.cpp` and `ggml`. Some early patches are **cumulative snapshots**, not a blindly sequential `git apply` series.
- `release/`: model-specific launchers, manifest and experiment drivers. Some legacy scripts reference package-native binaries not present in the source export; do not claim their end-to-end function before compiling those binaries.
- `simulator/`: selected test/benchmark source for experimental tools.
- `source/`: exact-source lock and reviewed patch order *to be filled only after passing the source-provenance audit*.

The historical reviewed release source commit was `0a1e4616954dea4ec2cd68f7f8a060364e02b9e4` in the private project; that SHA does **not** identify an upstream `llama.cpp` commit.

## Licensing

`llama.cpp` is a separately maintained project with its own license and third-party notices. Preserving relevant upstream notices is required when redistributing derived runtime binaries. The author must select and publish an explicit license for the project-specific modifications; **publicly visible source is not automatically OSI-licensed**. Until an owner-approved license and dependency notices are added, third-party redistribution rights should not be assumed.

The GGUF's weights and the model's licenses are independent of this code and are **not included**.

## Accuracy and publication policy

Use only measured data from the completed 16-profile physical M1 audit; do not infer 80B performance, 10+/14+ M1 throughput or mathematical-program speedups. Any new binary needs new native correctness and speed evidence.

Never push: model GGUF, large precomputed tile16 sidecar, API credentials, full private research branch/history, private route/prompt captures, local `/Users/...` or `/private/tmp/...` build logs or private M1 user data.
