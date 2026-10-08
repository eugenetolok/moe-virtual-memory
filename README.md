# MoE Virtual Memory

**35B MoE inference on an 8 GB Apple M1 — 7.185 tokens/second (measured)**

Experimental, **exact-with-respect-to-the-specified GGUF** inference using on-demand expert paging, direct resident-expert computation, causal prefetch, cache-aware eviction, and compute/I/O overlap.

> **Status:** research prototype; Apple M1 result reproduced in 64 fresh 600-token timing processes across 16 configurations. This is **not** a claim of a universal speedup over stock llama.cpp or of BF16-model equivalence.
>
> **Build status:** public source reconstruction and CI are being prepared. The historical rc14 binary was produced from a separate private research checkout; its measurements and hashes are documented here, but **there is not yet a public release binary**. Public CI builds must independently pass the [source-provenance and release gates](docs/build.md) before we claim that they reproduce rc14. Do not treat new CI artifacts as byte-identical to the benchmarked rc14 binary until the source-provenance gate passes.

## Measured result

| | Physical measurement / scope |
|---|---|
| Hardware | Apple M1 MacBook Air (MacBookAir10,1), **8 GiB unified memory** |
| Model | Qwen3.6-35B-A3B, fixed GGUF, **22.294 GiB** (23,938,321,664 bytes) |
| Best observed profile | Raw packed weights, frequency-age cache, A0/P2 prefetch, resident-compute overlap |
| Decode throughput | **7.1850 tokens/s median** (four fresh 600-token runs; range 7.1298–7.2337) |
| Controlled local comparison | Raw LRU/no-prefetch/no-overlap: **6.2078 tokens/s** median; observed paired gain **+15.78%** |
| Exactness | All 16 native profile comparisons passed; 9,625,927,680 compared output bytes, 0 mismatches; same generated tokens and ordered expert routes |
| Peak process RSS | About **4.05 GiB**; this is **not total system memory** |
| Peak system swap | About **431 MiB**; swapping was observed |

See [measurement protocol, all 16 profiles and limitations](docs/benchmarks.md).

**Important:** The optional tile16 sidecar was *not* faster than the best raw profile in this M1 experiment (7.1651 vs 7.1850 tokens/s, effectively tied). Our main engineering result is demand-paged **exact GGUF execution**, not a proven universal sidecar acceleration. Some prefetch profiles request *more*, not fewer, file bytes; requested I/O is not the same as physical NAND traffic.

## How it works

1. **Keep the small non-expert portions resident.** The 40 routed-MoE layers contain most of the model's weights, so loading the entire 22.3 GiB file into an 8 GiB machine is unnecessary.
2. **Route first, read only selected experts.** Each layer has 256 routed experts; the model itself selects eight per token. The native router and ordered Top-8 selection remain authoritative.
3. **Bound the expert cache.** A fixed-capacity resident cache, an exact demand loader, and optional frequency-age admission/eviction control the working set.
4. **Overlap useful work.** Causal prefetch and computations using already resident experts can overlap with cold-data waits.
5. **Preserve the computation.** No retraining, re-quantization, altered routing, approximate dot product or skipped experts. The native quantized path and ordered accumulation are checked against a canonical reference.

Architecture details: [docs/architecture.md](docs/architecture.md).

## Repository layout

- [`patches/`](patches/) — versioned C/C++ changes to the upstream `llama.cpp` runtime; **source, not model weights**
- [`release/`](release/) — macOS launchers, benchmark driver, model manifest and build helpers
- [`simulator/`](simulator/) — selected native test and compiler drivers
- [`docs/`](docs/) — architecture, benchmarks, reproducibility, limitations and source provenance
- [`.github/workflows/`](.github/workflows/) — source checks and macOS build/release pipeline (subject to exact-source gate)

**No 22 GiB GGUF, 21 GiB tile sidecar, model weights, private traces or experimental Research 58 files are included.** The model must be obtained separately and its SHA-256 verified.

## Model identity

```text
GGUF bytes:  23938321664
SHA-256:     f5ee307a2982106a6eb82b62b2c00b575c9072145a759ae4660378acda8dcf2d
Layers:      40 routed MoE
Experts:     256 per layer; 8 selected
```

The on-disk expert-offset table and manifest are **model-specific**. Other model files are not automatically supported, even if they share a marketing name.

## Build and reproduction

See [build and source verification](docs/build.md) and [M1 experiment guide](docs/reproduce.md). The public source archive and automated build must be validated against the historical native binary and exactness tests *before* an official release is promoted; a green syntax check alone is not evidence of matching performance.

## Research boundaries

The private research branch investigates executable mathematical expert programs, function-level paging and hardware-aware scheduling. Those approaches are **not** responsible for the numbers above. This public repository reports the measured paging/runtime baseline, not unpublished or speculative improvements.

## Acknowledgements and licensing

Built as modifications to [ggml-org/llama.cpp](https://github.com/ggml-org/llama.cpp). The upstream project and its dependencies retain their own license requirements. A public code mirror alone does **not** grant a license to reuse original project-specific code; [source and license notes](docs/provenance.md) document what remains to be finalized before broad redistribution.

Contributions and independent reproduction reports are welcome. Please include the model SHA, system configuration, chosen profile, complete timing methodology and any observed swap.
