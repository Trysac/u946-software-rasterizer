# Testing

## Running

```sh
cmake -P scripts/check.cmake -- tests
```

## Coverage

- `tests/rasterizer_test.cpp` contains the test suite:
- **Fill rule**: Verifies 16 pixels covered for two triangles sharing a diagonal, ensuring no double-drawing along the edge.
- **Z-buffer**: Confirms that overlapping quads result in the same output regardless of draw order.
- **Texturing**: Exercises nearest/bilinear filters and repeat/clamp wrapping.
- **Lighting**: Validates Lambertian shading and the default face normal fallback.
- **Perspective**: Ensures texel boundary placement at projected midpoints.
- **Edge cases**: Covers tiny/degenerate triangles and near-plane clipping.
- **I/O**: Tests PPM round-trips, error handling, and OBJ parsing with limit enforcement.