/// @file
/// @brief Public API for the deterministic CPU software rasterizer.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

/// @brief The public API of the software rasterizer library.
namespace trysac::software_rasterizer {

/// @brief Base exception thrown by all operations in the library on invalid input or limits.
class RasterError : public std::invalid_argument {
 public:
  /// @brief Constructs a RasterError with a stable machine-readable code and descriptive message.
  /// @param code Stable machine-readable error code.
  /// @param message Human-readable error message.
  RasterError(std::string code, const std::string& message);

  /// @brief Returns the machine-readable error code.
  /// @return The error code string view.
  [[nodiscard]] std::string_view code() const noexcept;

 private:
  std::string code_;
};

/// @brief A 2D single-precision floating-point vector.
struct Vec2 {
  /// @brief Default constructor.
  constexpr Vec2() noexcept = default;

  /// @brief Constructs a 2D vector with components.
  /// @param x_val The X (or U) component.
  /// @param y_val The Y (or V) component.
  constexpr Vec2(float x_val, float y_val) noexcept : x(x_val), y(y_val) {}

  /// @brief The X (or U) component.
  float x = 0.0F;
  /// @brief The Y (or V) component.
  float y = 0.0F;

  /// @brief Compares two 2D vectors for exact equality.
  /// @param other The vector to compare with.
  /// @return True if both components are equal.
  [[nodiscard]] bool operator==(const Vec2& other) const noexcept = default;
};

/// @brief A 3D single-precision floating-point vector.
struct Vec3 {
  /// @brief Default constructor.
  constexpr Vec3() noexcept = default;

  /// @brief Constructs a 3D vector with components.
  /// @param x_val The X component.
  /// @param y_val The Y component.
  /// @param z_val The Z component.
  constexpr Vec3(float x_val, float y_val, float z_val) noexcept : x(x_val), y(y_val), z(z_val) {}

  /// @brief The X component.
  float x = 0.0F;
  /// @brief The Y component.
  float y = 0.0F;
  /// @brief The Z component.
  float z = 0.0F;

  /// @brief Compares two 3D vectors for exact equality.
  /// @param other The vector to compare with.
  /// @return True if all components are equal.
  [[nodiscard]] bool operator==(const Vec3& other) const noexcept = default;
};

/// @brief A 4D single-precision floating-point homogeneous vector.
struct Vec4 {
  /// @brief Default constructor.
  constexpr Vec4() noexcept = default;

  /// @brief Constructs a 4D vector with components.
  /// @param x_val The X component.
  /// @param y_val The Y component.
  /// @param z_val The Z component.
  /// @param w_val The W (homogeneous) component.
  constexpr Vec4(float x_val, float y_val, float z_val, float w_val) noexcept
      : x(x_val), y(y_val), z(z_val), w(w_val) {}

  /// @brief The X component.
  float x = 0.0F;
  /// @brief The Y component.
  float y = 0.0F;
  /// @brief The Z component.
  float z = 0.0F;
  /// @brief The W component.
  float w = 0.0F;

  /// @brief Compares two 4D vectors for exact equality.
  /// @param other The vector to compare with.
  /// @return True if all components are equal.
  [[nodiscard]] bool operator==(const Vec4& other) const noexcept = default;
};

/// @brief A 4x4 single-precision column-major matrix.
struct Mat4 {
  /// @brief Column-major matrix elements (m[col * 4 + row]). Defaults to identity.
  std::array<float, 16> m = {
      1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F,
  };

  /// @brief Returns the 4x4 identity matrix.
  /// @return The identity matrix.
  [[nodiscard]] static Mat4 identity() noexcept;

  /// @brief Builds a right-handed perspective projection matrix mapping depth to [-1, 1] in NDC.
  /// @param fovy_radians Vertical field of view in radians.
  /// @param aspect Viewport width divided by height.
  /// @param z_near Distance to the near clipping plane (must be > 0).
  /// @param z_far Distance to the far clipping plane.
  /// @return The perspective projection matrix.
  [[nodiscard]] static Mat4 perspective(float fovy_radians, float aspect, float z_near, float z_far) noexcept;

  /// @brief Builds a right-handed look-at view matrix.
  /// @param eye Camera position in world space.
  /// @param target Target position the camera looks at.
  /// @param up Up direction vector in world space.
  /// @return The view transformation matrix.
  [[nodiscard]] static Mat4 look_at(Vec3 eye, Vec3 target, Vec3 up) noexcept;

  /// @brief Multiplies two 4x4 matrices.
  /// @param rhs The right-hand side matrix.
  /// @return The product matrix.
  [[nodiscard]] Mat4 operator*(const Mat4& rhs) const noexcept;

  /// @brief Multiplies the 4x4 matrix by a 4D column vector.
  /// @param v The column vector.
  /// @return The transformed 4D vector.
  [[nodiscard]] Vec4 operator*(const Vec4& v) const noexcept;

  /// @brief Compares two matrices for exact equality.
  /// @param other The matrix to compare with.
  /// @return True if all 16 elements are equal.
  [[nodiscard]] bool operator==(const Mat4& other) const noexcept = default;
};

/// @brief An 8-bit per channel RGBA color.
struct Rgba8 {
  /// @brief Default constructor.
  constexpr Rgba8() noexcept = default;

  /// @brief Constructs an RGBA color with components.
  /// @param r_val Red channel (0..255).
  /// @param g_val Green channel (0..255).
  /// @param b_val Blue channel (0..255).
  /// @param a_val Alpha channel (0..255).
  constexpr Rgba8(std::uint8_t r_val, std::uint8_t g_val, std::uint8_t b_val, std::uint8_t a_val) noexcept
      : r(r_val), g(g_val), b(b_val), a(a_val) {}

  /// @brief Red channel (0..255).
  std::uint8_t r = 0;
  /// @brief Green channel (0..255).
  std::uint8_t g = 0;
  /// @brief Blue channel (0..255).
  std::uint8_t b = 0;
  /// @brief Alpha channel (0..255).
  std::uint8_t a = 0;

  /// @brief Compares two RGBA colors for equality.
  /// @param other The color to compare with.
  /// @return True if all four channels match.
  [[nodiscard]] bool operator==(const Rgba8& other) const noexcept = default;
};

/// @brief A 2D image of 8-bit RGBA pixels with dimensions bounded to 1..8192.
class Image {
 public:
  /// @brief Maximum allowed width or height of an image.
  static constexpr int kMaxDimension = 8192;

  /// @brief Constructs an image filled with the given color.
  /// @param width Image width in pixels (1..8192).
  /// @param height Image height in pixels (1..8192).
  /// @param fill Initial color for every pixel.
  /// @throws RasterError with code `invalid_size` if width or height is outside 1..8192.
  Image(int width, int height, Rgba8 fill = {});

  /// @brief Returns the image width in pixels.
  /// @return Width in pixels.
  [[nodiscard]] int width() const noexcept;

  /// @brief Returns the image height in pixels.
  /// @return Height in pixels.
  [[nodiscard]] int height() const noexcept;

  /// @brief Reads the pixel at (x, y).
  /// @param x Column index (0..width-1).
  /// @param y Row index (0..height-1).
  /// @return The RGBA color at (x, y).
  /// @throws RasterError with code `invalid_index` if (x, y) is out of bounds.
  [[nodiscard]] Rgba8 at(int x, int y) const;

  /// @brief Sets the pixel at (x, y).
  /// @param x Column index (0..width-1).
  /// @param y Row index (0..height-1).
  /// @param c The RGBA color to write.
  /// @throws RasterError with code `invalid_index` if (x, y) is out of bounds.
  void set(int x, int y, Rgba8 c);

  /// @brief Compares two images for identical dimensions and pixel values.
  /// @param other The image to compare with.
  /// @return True if width, height and all pixels match.
  [[nodiscard]] bool operator==(const Image& other) const noexcept;

 private:
  int width_;
  int height_;
  std::vector<Rgba8> pixels_;
};

/// @brief Zero-based vertex attribute indices for one corner of a triangle.
struct Corner {
  /// @brief Zero-based index into `Mesh::positions`.
  int position = 0;
  /// @brief Zero-based index into `Mesh::uvs`, or -1 if absent.
  int uv = -1;
  /// @brief Zero-based index into `Mesh::normals`, or -1 if absent.
  int normal = -1;

  /// @brief Default constructor.
  constexpr Corner() noexcept = default;

  /// @brief Constructs a Corner with position, uv, and normal indices.
  /// @param p Zero-based index into `Mesh::positions`.
  /// @param u Zero-based index into `Mesh::uvs`, or -1 if absent.
  /// @param n Zero-based index into `Mesh::normals`, or -1 if absent.
  constexpr Corner(int p, int u, int n) noexcept : position(p), uv(u), normal(n) {}

  /// @brief Constructs a Corner with position index, defaults uv and normal to -1.
  /// @param p Zero-based index into `Mesh::positions`.
  explicit constexpr Corner(int p) noexcept : position(p) {}

  /// @brief Compares two corners for exact equality.
  /// @param other The corner to compare with.
  /// @return True if position, uv, and normal indices are equal.
  [[nodiscard]] bool operator==(const Corner& other) const noexcept = default;
};

/// @brief A triangle defined by three corner attribute index triples.
struct Triangle {
  /// @brief The three corners of the triangle.
  std::array<Corner, 3> corners{};

  /// @brief Accesses the i-th corner (0..2).
  /// @param i Corner index.
  /// @return Reference to the corner.
  [[nodiscard]] Corner& operator[](std::size_t i) noexcept { return corners.at(i); }

  /// @brief Accesses the i-th corner (0..2) const.
  /// @param i Corner index.
  /// @return Const reference to the corner.
  [[nodiscard]] const Corner& operator[](std::size_t i) const noexcept { return corners.at(i); }

  /// @brief Compares two triangles for exact equality.
  /// @param other The triangle to compare with.
  /// @return True if all three corners are equal.
  [[nodiscard]] bool operator==(const Triangle& other) const noexcept = default;
};

/// @brief Indexed triangle mesh holding positions, texture coordinates, normals, and triangles.
struct Mesh {
  /// @brief Vertex 3D positions.
  std::vector<Vec3> positions;
  /// @brief Vertex 2D texture coordinates.
  std::vector<Vec2> uvs;
  /// @brief Vertex 3D normals.
  std::vector<Vec3> normals;
  /// @brief Indexed triangles.
  std::vector<Triangle> triangles;
};

/// @brief Texture sampling filter mode.
enum class Filter : std::uint8_t {
  /// @brief Nearest-neighbor texel sampling.
  nearest,
  /// @brief Bilinear filtering across the four nearest texel centers.
  bilinear,
};

/// @brief Texture coordinate wrapping mode.
enum class Wrap : std::uint8_t {
  /// @brief Fractional repeat wrapping into [0, 1).
  repeat,
  /// @brief Clamp coordinates to the valid range.
  clamp,
};

/// @brief Pipeline state and uniforms for a draw call.
struct DrawState {
  /// @brief Model-to-world transformation matrix.
  Mat4 model = Mat4::identity();
  /// @brief World-to-view transformation matrix.
  Mat4 view = Mat4::identity();
  /// @brief View-to-clip projection matrix.
  Mat4 projection = Mat4::identity();
  /// @brief Optional 2D texture to sample.
  const Image* texture = nullptr;
  /// @brief Texture minification/magnification filter.
  Filter filter = Filter::nearest;
  /// @brief Texture wrapping mode in U and V.
  Wrap wrap = Wrap::repeat;
  /// @brief Base surface color multiplied with texture and lighting.
  Rgba8 base_color{255, 255, 255, 255};
  /// @brief Direction vector pointing towards the directional light source.
  Vec3 light_towards{0.0F, 0.0F, 1.0F};
  /// @brief Ambient lighting intensity added to diffuse Lambertian lighting.
  float ambient = 0.0F;
  /// @brief Whether to compute Lambertian + ambient lighting.
  bool lighting = false;
  /// @brief Whether to perform less-than depth testing and depth writes.
  bool depth_test = true;
};

/// @brief Deterministic CPU triangle rasterizer with color and depth buffers.
class Renderer {
 public:
  /// @brief Constructs a renderer with a target of the given width and height.
  /// @param width Target width in pixels (1..8192).
  /// @param height Target height in pixels (1..8192).
  /// @throws RasterError with code `invalid_size` if width or height is outside 1..8192.
  Renderer(int width, int height);

  /// @brief Clears the color buffer, depth buffer, and resets the pixels_written counter.
  /// @param color Fill color for the color buffer.
  /// @param depth Initial depth value for every pixel (default 1.0f).
  void clear(Rgba8 color, float depth = 1.0F);

  /// @brief Rasterizes a mesh onto the target using the given draw state.
  /// @param mesh The indexed triangle mesh to draw.
  /// @param state Transformation, texturing, lighting, and depth state.
  /// @throws RasterError with code `invalid_index` if any triangle index is out of bounds.
  void draw(const Mesh& mesh, const DrawState& state);

  /// @brief Returns a const reference to the rendered color buffer.
  /// @return The color Image.
  [[nodiscard]] const Image& color() const noexcept;

  /// @brief Returns the depth value stored at pixel (x, y).
  /// @param x Column index (0..width-1).
  /// @param y Row index (0..height-1).
  /// @return The stored depth value.
  /// @throws RasterError with code `invalid_index` if (x, y) is out of bounds.
  [[nodiscard]] float depth_at(int x, int y) const;

  /// @brief Returns the total number of pixel writes since construction or the last clear().
  /// @return Count of pixels written.
  [[nodiscard]] std::uint64_t pixels_written() const noexcept;

 private:
  Image color_;
  std::vector<float> depth_;
  std::uint64_t pixels_written_ = 0;
};

/// @brief Resource limits enforced when parsing Wavefront OBJ text.
struct ObjLimits {
  /// @brief Maximum input text size in bytes (default 64 MiB).
  std::size_t max_bytes = 64ULL * 1024ULL * 1024ULL;
  /// @brief Maximum count of positions, uvs, or normals (default 1,000,000).
  std::size_t max_vertices = 1'000'000;
  /// @brief Maximum count of triangulated faces (default 2,000,000).
  std::size_t max_triangles = 2'000'000;
};

/// @brief Parses a Wavefront OBJ text string into a Mesh, flipping V texture coordinates to 1 - v.
/// @param text The OBJ file contents.
/// @param limits Byte, vertex, and triangle safety limits.
/// @return The parsed Mesh.
/// @throws RasterError with code `input_too_large`, `too_large`, `invalid_index`, or `malformed`.
[[nodiscard]] Mesh load_obj(std::string_view text, const ObjLimits& limits = {});

/// @brief Serializes an Image into binary PPM (P6) format with maxval 255.
/// @param image The image to serialize.
/// @return Binary string containing the P6 header and RGB byte payload.
[[nodiscard]] std::string write_ppm(const Image& image);

/// @brief Parses a binary PPM (P6) byte buffer into an Image with alpha set to 255.
/// @param bytes The raw PPM file bytes.
/// @return The decoded Image.
/// @throws RasterError with code `input_too_large`, `unsupported_format`, `invalid_size`, or `malformed`.
[[nodiscard]] Image read_ppm(std::string_view bytes);

/// @brief Result of a tolerance-based pixel-by-pixel image comparison.
struct CompareResult {
  /// @brief True if differing_pixels <= max_differing_pixels.
  bool equal = false;
  /// @brief Count of pixels where at least one RGBA channel differs by more than channel_tolerance.
  std::size_t differing_pixels = 0;
  /// @brief Maximum absolute single-channel difference observed across the entire image.
  int max_channel_delta = 0;
};

/// @brief Compares two images of identical dimensions within a per-channel and pixel-count tolerance.
/// @param a First image.
/// @param b Second image.
/// @param channel_tolerance Maximum allowed absolute difference per channel (R, G, B, A) for a pixel to match.
/// @param max_differing_pixels Maximum allowed count of pixels exceeding `channel_tolerance`.
/// @return Comparison metrics and equality result.
/// @throws RasterError with code `size_mismatch` if `a` and `b` have different dimensions.
[[nodiscard]] CompareResult compare(const Image& a, const Image& b, int channel_tolerance,
                                    std::size_t max_differing_pixels);

}  // namespace trysac::software_rasterizer
