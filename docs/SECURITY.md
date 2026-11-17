# Security & Input Limits

All functions validate inputs and throw `trysac::software_rasterizer::RasterError` with a machine-readable `code()`.

- **Dimensions**: `Image` and `Renderer` width/height are limited to 1..8192. Exceeding this throws `invalid_size`.
- **OBJ Loading**: `load_obj` enforces `ObjLimits` (defaulting to 64 MiB input, 1M vertices, 2M triangles). Exceeding these throws `input_too_large` or `too_large`. Malformed syntax throws `malformed` with the 1-based line number.
- **PPM Decoding**: `read_ppm` enforces a 256 MiB input limit. Validates the P6 magic header and 255 maxval. Mismatches throw `unsupported_format`, `malformed`, or `input_too_large`.
- **Access**: Pixel and depth accessors perform bounds checks and throw `invalid_index`.