# Architecture

`software-rasterizer` is a C++20 CPU rendering library (`trysac::software_rasterizer`) without external dependencies.

1. **Math (`Vec2`, `Vec3`, `Vec4`, `Mat4`)**: Column-major 4x4 float matrices.
2. **Target (`Rgba8`, `Image`)**: Bounded 2D RGBA8 storage.
3. **Geometry (`Mesh`, `load_obj`)**: Parses Wavefront OBJ (`v`, `vt`, `vn`, `f`) and performs fan-triangulation.
4. **Rasterization Pipeline (`Renderer`, `DrawState`)**: Homogeneous clipping, viewport transformation, double-precision edge functions (top-left rule), less-than depth testing, perspective-correct 1/w interpolation, and Lambertian lighting.
5. **I/O (`write_ppm`, `read_ppm`)**: Binary P6 PPM serialization.
6. **Comparison (`compare`)**: Tolerance-based image equality check.