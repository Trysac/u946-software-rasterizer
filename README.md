# software-rasterizer

Deterministic C++20 CPU triangle rasterizer (`trysac::software_rasterizer`) implementing a D3D10 top-left fill rule, z-buffer, perspective-correct 1/w interpolation, nearest/bilinear texture sampling, Lambertian lighting, Wavefront OBJ loading, binary PPM (P6) serialization, and tolerance-based comparison.

## Usage

```cpp
#include "trysac/software_rasterizer/rasterizer.hpp"

int main() {
  namespace sr = trysac::software_rasterizer;
  const sr::Mesh mesh = sr::load_obj("v -1 0 0\nv 0 0 0\nv 0 1 0\nv -1 1 0\nf 1 2 3 4\n");
  sr::Renderer renderer(8, 8);
  renderer.clear(sr::Rgba8{0, 0, 0, 255});
  renderer.draw(mesh, sr::DrawState{.base_color = sr::Rgba8{255, 255, 255, 255}});
  return renderer.pixels_written() == 16 ? 0 : 1;
}
```

## API Summary

- **Types**: `Vec2`, `Vec3`, `Vec4`, `Mat4`, `Rgba8` (RGBA8), `Image` (bounded 8192x8192), `Mesh`, `Triangle`, `Corner`, `DrawState`, `Renderer`.
- **Functions**: `load_obj`, `write_ppm`, `read_ppm`, `compare`.
- **Errors**: `RasterError` (code(): `invalid_index`, `input_too_large`, `too_large`, `malformed`, `unsupported_format`, `size_mismatch`, `invalid_size`).

## Why not existing alternatives?

Existing rasterizers are often large, opaque, or assume a hardware GPU backend. This library provides exact mathematical specification for fill rules and interpolation so that the output is deterministic and comparable across platforms.

[Security & Input Limits](docs/SECURITY.md)