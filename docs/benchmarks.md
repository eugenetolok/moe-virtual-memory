# Physical Apple M1 benchmark — rc14

This page reports **measured** rc14 results and limitations. No result here comes from theoretical estimates or Research 58.

## Test identity

- Host: Apple M1 MacBook Air `MacBookAir10,1`, **8 GiB unified memory**.
- Model: Qwen3.6-35B-A3B model GGUF, **23,938,321,664 B**.
- GGUF SHA-256: `f5ee307a2982106a6eb82b62b2c00b575c9072145a759ae4660378acda8dcf2d`.
- Same fixed prompt, original routing and 22 expert-cache slots/layer.
- All 16 configurations: raw or tile16; LRU or frequency-age; prefetch off/on (A0/P2); resident-compute overlap off/on.
- 16 complete output comparisons plus **64 fresh 600-token decode timing processes** (four balanced rounds per configuration).
- The timing comparison includes generation, **not startup/prefill/sidecar SHA validation**. I/O and overlap counters normalized by 625 model passes include 25 prompt/prefill plus 600 generated passes. Do not subtract them directly from decode-only token time.
- Source release identity: private engineering source commit `0a1e4616954dea4ec2cd68f7f8a060364e02b9e4`; recorded rc14 archive SHA-256 `dcd2f3d85f364cbaec987c18297d990340f3399afeb213524a3101b62fbd9eb0`. The public build is **not yet demonstrated binary-identical**.

## All 16 configurations

| Cache policy | Prefetch | Overlap | Raw TPS median | Tile16 TPS median | Paired tile/raw |
|---|---|---|---:|---:|---:|
| LRU | Off | Off | 6.2078 | 6.3903 | +2.94% |
| LRU | Off | On | 6.4810 | 6.6348 | +2.39% |
| LRU | On | Off | 6.5998 | 6.7795 | +2.05% |
| LRU | On | On | 6.9030 | 7.0168 | +2.72% |
| Frequency-age | Off | Off | 6.4514 | 6.5595 | +1.72% |
| Frequency-age | Off | On | 6.7058 | 6.8675 | +1.69% |
| Frequency-age | On | Off | 6.7795 | 7.0642 | +3.83% |
| **Frequency-age** | **On** | **On** | **7.1850** | **7.1651** | **−0.20%** |

Best raw profile: median **7.1850 TPS**, observed range **7.1298–7.2337 TPS**. The corresponding tile16 profile was 7.1651 TPS; their four paired ratios had median approximately 0.998 and are effectively tied, **not proof of a sidecar speedup**.

The raw LRU/no-prefetch/no-overlap reference was 6.2078 TPS. The observed within-sweep best-raw versus that baseline paired median gain is **+15.78%**. This is **not** a comparison to unmodified upstream `llama.cpp`, another machine, or another quantization.

## Correctness and resource observations

- Native graph differential: **6,408 cases**, 32,040 operator comparisons, zero mismatches.
- 16 output comparisons: **9,625,927,680 bytes**, zero mismatches. All 64 speed processes generated identical token sequences and ordered expert routes.
- Maximum process RSS: **4,349,673,472 B (~4.05 GiB)**. System swap sampled peak: **452,062,085 B (~431 MiB)**; nonzero swapout events occurred in early process windows. Some swap may not be attributable to the benchmark.
- Best raw profile model-pass authoritative wait median: **84.7687 ms**, achieved overlap **16.2169 ms**, generation CPU/wall proxy **4.1120** core equivalents. These timing counters include prompt passes; do not present them as decode-only ms/token.
- Best profile requested expert bytes per 625-pass run: **192,007,127,040 B** (ordinary demand **90,018,766,848**, prefetch **101,988,360,192**). The no-prefetch baseline requested **172,522,881,024 B**. **Prefetch improved TPS while requesting more bytes.**

## Evidence scope and limitations

Exact output/tokens were verified for the recorded implementation and workload. Full cross-machine logits/matmul dump equivalence was not tested; positive claims are scoped accordingly. Four fresh runs per cell provide an informative median, not a tight general performance distribution. Thermal stability, reboot provenance and equal physical SSD/page-cache states have not been independently established.

`F_NOCACHE` is best effort. Requested I/O bytes/calls are not independently verified physical flash traffic. A process RSS smaller than 8 GiB does not prove there was zero system memory pressure. The optional sidecar includes startup whole-file hashing and extra disk consumption, outside throughput timing.

The public packaging/rebuild chain requires an upstream-source pin and reproducible validation; its existence alone does not replicate the native rc14 benchmark. See [build](build.md).
