#!/usr/bin/env python3
"""Generate the exact tile16 sidecar plan from the public expert-offset table.

The plan is a pure function of results/gguf_expert_offsets.csv and the fixed
Qwen3.6-35B-A3B expert geometry. It contains no research data and no model bytes.

Format (one row per 40 layers x 256 experts x 3 parts = 30720 lines):
    <layer> <expert> <part> <offset> <bytes> <rows> <blocks>

part 0 = gate, part 1 = up, part 2 = down.
"""
import argparse
import csv
import pathlib

GEOMETRY = {0: (512, 8), 1: (512, 8), 2: (2048, 2)}


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--offsets", required=True)
    ap.add_argument("--output", required=True)
    args = ap.parse_args()

    rows = list(csv.DictReader(pathlib.Path(args.offsets).read_text().splitlines()))
    assert len(rows) == 40, f"expected 40 layers, got {len(rows)}"
    out = []
    for layer, row in enumerate(rows):
        cols = [(int(row["gate_off"]), int(row["gate_bytes"])),
                (int(row["up_off"]), int(row["up_bytes"])),
                (int(row["down_off"]), int(row["down_bytes"]))]
        for expert in range(256):
            for part in range(3):
                base, size = cols[part]
                r, b = GEOMETRY[part]
                assert size == r * b * (210 if part == 2 else 144), (layer, expert, part)
                out.append(f"{layer} {expert} {part} {base + expert * size} {size} {r} {b}")
    pathlib.Path(args.output).write_text("\n".join(out) + "\n")
    print(f"wrote {len(out)} plan rows")


if __name__ == "__main__":
    main()
