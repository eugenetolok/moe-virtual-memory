# Third-party notices

This repository contains project-specific patches, launchers, benchmark tooling, and validation code for a modified `llama.cpp` runtime.

## llama.cpp / ggml

The upstream runtime is maintained by the ggml authors and is distributed under the MIT License.

A release archive built from a pinned upstream tree must include the exact upstream `LICENSE` from that tree as `UPSTREAM-LLAMA-LICENSE`. The project license in this repository does not replace or remove upstream copyright and license notices.

## Platform components

The macOS-only native utilities may use Apple system frameworks such as CommonCrypto. Those platform components are not redistributed by this repository.

## Models and weights

No model weights, GGUF files, or large tile sidecars are distributed by this repository. Model and weight licenses are independent of this project's code license and must be reviewed separately by users who obtain them.
