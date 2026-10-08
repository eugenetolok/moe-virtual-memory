# rc14 public preview (macOS ARM64) — test pre-release

This is a **test pre-release**, not a validated stable release. It packages the native
runtime reconstructed from the pinned upstream commit and the audited rc14 patch. The
binary has **not** been verified on physical Apple M1 8 GB by the maintainer; the
historical rc14 measurements were produced from a separate private build. Treat this
as an experimental artifact and report results with the exact hashes.

## Requirements

- Apple silicon Mac running macOS 12 or newer (built for arm64, deployment target 12.0).
- The exact Qwen3.6-35B-A3B GGUF, **not** another similarly named file:

  ```sh
  shasum -a 256 /absolute/path/model.gguf
  # f5ee307a2982106a6eb82b62b2c00b575c9072145a759ae4660378acda8dcf2d
  ```

  Model weights are **not** included and remain subject to their own license.
- For the optional tile16 sidecar: about 21 GB of additional free disk.

## 1. Verify the download

```sh
shasum -a 256 -c moe-virtual-memory-macos-arm64-rc14.tar.gz.sha256
tar xzf moe-virtual-memory-macos-arm64-rc14.tar.gz
cd moe-virtual-memory-macos-arm64-rc14
cat VERSION SHA256SUMS
( cd . && shasum -a 256 -c SHA256SUMS )
```

## 2. Native synthetic exactness (no model needed)

```sh
./bin/moe-tiled-selftest
# TILED OVERLAP D1 PASS cases=6408 op_compares=32040 ... mismatches=0
```

## 3. Serve the paged runtime (CPU-only, M1 8 GB defaults)

```sh
./bin/moe-serve --model /absolute/path/model.gguf --profile bringup
```

On machines with about 9.6 GiB or less unified memory `moe-serve` selects CPU-only
`-ngl 0`, M=16, QD=4, ctx 1024, batch 16, ubatch 1 and best-effort `F_NOCACHE`.
Inspect `--help` for explicit flags; do not assume the default profile is the fastest.

## 4. Optional tile16 sidecar

```sh
./bin/moe-prepare-tiled --model /absolute/path/model.gguf --output /absolute/path/experts.tile16
./bin/moe-m1-tiled-sweep --model /absolute/path/model.gguf --sidecar /absolute/path/experts.tile16 \
  --output "$HOME/Desktop/m1-tiled-results"
```

`moe-prepare-tiled` verifies the model SHA, the shipped tiling plan and the resulting
sidecar SHA. The sidecar is not included.

## 5. Full 16-profile combinations experiment (optional, several hours)

```sh
caffeinate -i ./bin/moe-m1-combinations \
  --model /absolute/path/model.gguf \
  --sidecar /absolute/path/experts.tile16 \
  --output "$HOME/Desktop/m1-combinations-rc14" \
  --stage all
```

## Reporting

Include host model, RAM, macOS version, archive SHA-256, model SHA-256, the exact
command, tokens/routes and RSS/swap observations. Do not upload model weights, the
sidecar, or raw private logs. See [reproduce](reproduce.md) and [benchmarks](benchmarks.md).

## Status

- Source identity: pinned upstream `81bc6b83f827df746eb129235488d325c49cae52` + audited
  cumulative rc14 patch (see `share/rc14-source-manifest.json`).
- Synthetic native graph differential: PASS on the build host.
- Real-weight raw-path byte reproducibility: recorded in the release notes.
- Real-weight raw-vs-tile16 and the full 16-profile matrix: **not run by the maintainer**
  for this preview (no sidecar on the build host).
- Physical Apple M1 8 GB verification: **pending the user's run**.
- The stable release gate remains closed; `source/RELEASE_APPROVED.json` is intentionally absent.
