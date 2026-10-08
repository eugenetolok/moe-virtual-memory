# Architecture: exact demand-paged MoE

## The memory problem

The tested quantized GGUF occupies **23,938,321,664 bytes (22.294 GiB)**. The physical Apple M1 MacBook Air has **8 GiB unified memory**, shared by the operating system, CPU and GPU. Attempting to make the complete model resident is not the strategy.

For this specific GGUF, around **19.453 GiB** of model tensors are routed experts in 40 main MoE layers, plus a separate **0.486 GiB** routed MTP layer. Non-expert weights total about **2.344 GiB**. The MTP layer is not one of the 40 traced main MoE layers.

Each main MoE layer has 256 routed experts. The authoritative model router selects **eight** in a fixed order, **per token and per layer**. The hidden state changes from layer to layer; expert IDs are not fixed topic labels. Each selected expert includes Q4_K gate, Q4_K up and Q6_K down matrices; combined model-specific packed size is **2,039,808 bytes** per expert.

## Runtime path

```text
Input token / hidden state
         |
    native router
         |  exact ordered Top-8
         v
 Resident expert cache  <--- causal prefetch
         |                       |
      ready? ----no----> bounded SSD reads (original GGUF)
         |                       |
         +---------- join --------+
                    |
       native Q4 gate + Q4 up
                    |
             original SwiGLU
                    |
              canonical Q8
                    |
              native Q6 down
                    |
          unchanged FP accumulation
```

The runtime keeps a bounded number of **expert slots per layer**, not all 256 experts. For the rc14 fair-comparison sweep, there are 22 slots per layer: demand-only M22/P0 or M20/P2 with two probation/prefetch slots. Expert cache alone is **1,795,031,040 bytes** across 40 layers; this excludes non-expert model weights, threads, KV/cache, scratch, OS and swap.

Main implemented techniques:

- **Direct GGUF reads** using recorded expert offsets and a bounded queue of I/O requests (QD4 in the M1 sweep).
- **Resident direct_sparse quantized compute**: compute from resident expert slots rather than repeatedly materializing K=8 expert-weight staging copies.
- **Causal A0 prefetch**, with no future route oracle. Prefetch can request bytes that are never used; its benefit is measured as latency/throughput, not automatically traffic reduction.
- **LRU / frequency-age** expert-cache policies.
- **Resident-compute overlap**: start useful work on already-ready selected experts while other selected experts still require I/O.
- **Optional tile16 sidecar**: an offline *lossless* alternate weight layout allowing vector-oriented kernels. Sidecar increases disk space, requires original GGUF and did not beat the fastest raw profile in the published M1 sweep.

## What “exact” means

The model file is never retrained, requantized or changed. Expert routing and order, quantized dot-product semantics, scales/mins, canonical Q8 activation quantization, SwiGLU and floating-point accumulation remain authoritative. Exactness gates compare byte output tensors for all 16 configurations and generated tokens/routes; the published independent audit reported zero native mismatches.

**Exact relative to the original quantized GGUF** does **not** mean identical to the original unquantized BF16 model. Cross-machine full intermediate/logit byte dumps were not exhaustively compared.

## What this design does not imply

- RSS is **not** total Mac memory usage: the file cache, system allocations and swap are distinct.
- Requested bytes are not measured NAND bytes; macOS `F_NOCACHE` is best effort.
- Paging can make an oversized model runnable while still paying SSD latency on a miss.
- The result is for one model/quantization/host/workload. A 70B/80B model, another GGUF, Linux or Windows is **not validated** by this experiment.
- Experimental exact-program compilation/Research 58 is not in the publicly measured rc14 runtime.

See [benchmark details](benchmarks.md), [build provenance](provenance.md).
