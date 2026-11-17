# Deterministic Double-Precision Edge Equations and Homogeneous Near-Plane Clipping

Status: accepted (generated, pending human review)

## Context

CPU software rasterizers used in headless tests and reference comparisons often suffer from seams or double-drawn pixels along shared edges due to floating-point tie-breaking inconsistencies, as well as division-by-zero or inverted projection artifacts when triangles straddle the camera near plane.

## Decision

Clip triangles against the near plane in homogeneous clip space before perspective divide, normalize screen-space triangle winding to clockwise so both windings render without culling, evaluate edge equations in IEEE-754 double precision at pixel centers (x + 0.5, y + 0.5) using the D3D10 top-left rule, and interpolate attributes perspective-correctly with 1/w.

## Consequences

Adjacent triangles sharing an edge cover every pixel along the shared boundary at most once with zero seams or double writes, receding surfaces interpolate textures perspective-correctly, and outputs are deterministic across platforms.
