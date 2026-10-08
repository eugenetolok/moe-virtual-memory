# Reproduce the physical M1 experiment

**Audience:** users with macOS ARM64, preferably the Apple M1 8 GiB hardware used in rc14. This is a research-level experiment, not a one-click consumer installer.

## Model

Obtain the licensed model separately; model weights are **not hosted in this repository**.

```sh
shasum -a 256 /absolute/path/to/model.gguf
# expected:
# f5ee307a2982106a6eb82b62b2c00b575c9072145a759ae4660378acda8dcf2d
```

The exact original model is required, not another similarly named GGUF.

## The release pipeline

See [build.md](build.md). The historical rc14 binary has a recorded checksum and audit, but the **new public CI must first recreate a verified source tree and pass correctness checks** before a public version is called validated. CI-synthesized binaries are new builds, not necessarily byte-identical to the historical native binary.

After the release is available, unpack its published macOS ARM64 archive and verify the accompanying SHA-256.

## Benchmark environment

- Keep model on an internal SSD, with substantial free space for macOS swap and results.
- Connect power, close other model servers/heavy applications, and prevent sleep.
- Use the documented same model, context, batch/ubatch, CPU/thread settings and original cache-slot budget. Do not interpret P0/M22 versus P2/M20 as different total slot capacities.
- Run correctness before speed; if any output/token/route differs, **stop**.
- Run fresh-process balanced timing comparisons; record host model, RAM, source/binary hashes, model checksum, run order, swap and process RSS.
- The optional tile16 sidecar is **large**: about 20.89 GB physical payload / 23.94 GB logical file. It must be generated from the original model and verified. It is not needed for the best *raw* result and is not included in the public source tree.

The original rc14 benchmark driver source is available as [`release/moe-m1-combinations`](../release/moe-m1-combinations). It requires **native benchmark, self-test and tile16 compiler executables** in a matching validated package. Copying the Python driver alone is **not** sufficient to reproduce the benchmark. The public release must not claim the rc14 16-way test is operational until these tools are rebuilt and validated.

When the full rc14 runner is available in a verified build, its original invocation is:

```sh
caffeinate -i ./bin/moe-m1-combinations \
  --model /absolute/path/to/model.gguf \
  --sidecar /absolute/path/to/experts.tile16 \
  --output "$HOME/Desktop/m1-combinations-rc14" \
  --stage all
```

The runner tests all 16 configurations; for normal operation the faster measured variant was the **raw** frequency-age + A0/P2 + overlap profile. The default `moe-serve` launch policy is not asserted to be that profile; inspect and set flags explicitly.

Do not upload model weights, tile16 sidecar or raw private user logs as issues; share only sanitized results and hashes.
