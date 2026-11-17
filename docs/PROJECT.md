# Project record

## Objective

Rasterize triangles on the CPU with a top-left fill rule, z-buffer, perspective-correct texturing, Lambert lighting, OBJ loading and tolerance-based image comparison.

## Scope

- In scope: Rasterization pipeline (near-plane clipping, viewport transform, edge functions, depth buffer), Wavefront OBJ loading (v, vt, vn, f), PPM P6 serialization, and tolerance-based pixel comparison.
- Out of scope: MTL materials, mipmaps, anti-aliasing, alpha blending, shadow mapping, multithreading, non-near-plane clipping, and output formats other than binary P6 PPM.

## Timeline

| Date | Event |
|---|---|
| 2026 | Repository created from the factory utility template. |

## Effort metrics

| Metric | Value |
|---|---|
| Tokens (input / output) | 8933747 / 81835 |
| Hours (human) | pending measurement |
| Cost (USD) | 2.1047 |

## Factory run u946 (measured from API usage)

| Date (UTC) | Phase | Model | Batch | Input | Output | Cache write | Cache read | Cost (USD) |
|---|---|---|---|---|---|---|---|---|
| 2026-11-17 | generation | google/gemini-4-argon | yes | 7260 | 30948 | 0 | 0 | 0.6480 |
| 2026-11-17 | fix-1 | google/gemini-3.8-flash | no | 235822 | 21776 | 0 | 2856331 | 0.4728 |
| 2026-11-17 | fix-2 | google/gemini-3.8-flash | no | 195427 | 9589 | 0 | 1878939 | 0.3235 |
| 2026-11-17 | fix-3 | google/gemini-3.8-flash | no | 176802 | 8843 | 0 | 1601663 | 0.2859 |
| 2026-11-17 | fix-4 | google/gemini-3.8-flash | no | 136160 | 7896 | 0 | 1045116 | 0.2101 |
| 2026-11-17 | audit | google/gemini-3.8-flash | no | 137148 | 1007 | 0 | 631618 | 0.1540 |
| 2026-11-17 | docs | google/gemini-3.1-flash-lite | no | 31461 | 1776 | 0 | 0 | 0.0105 |

Total: $2.1047. Prices: vertex line, as measured by its adapter. Providers serve the current version of each model id, so the date tells which version answered; versions, launch stages and price sources per line are in the factory's model register (docs/MODELS.md).
