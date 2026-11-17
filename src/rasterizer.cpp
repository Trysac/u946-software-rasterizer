#include "trysac/software_rasterizer/rasterizer.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace trysac::software_rasterizer {

namespace {

constexpr std::size_t kMaxPpmBytes = 256ULL * 1024ULL * 1024ULL;

[[nodiscard]] Vec3 sub(Vec3 a, Vec3 b) noexcept { return Vec3{a.x - b.x, a.y - b.y, a.z - b.z}; }

[[nodiscard]] float dot(Vec3 a, Vec3 b) noexcept { return (a.x * b.x) + (a.y * b.y) + (a.z * b.z); }

[[nodiscard]] Vec3 cross(Vec3 a, Vec3 b) noexcept {
  return Vec3{
      (a.y * b.z) - (a.z * b.y),
      (a.z * b.x) - (a.x * b.z),
      (a.x * b.y) - (a.y * b.x),
  };
}

[[nodiscard]] Vec3 normalize_vec3(Vec3 v) noexcept {
  const float len_sq = dot(v, v);
  if (len_sq <= 1e-24F) {
    return Vec3{0.0F, 0.0F, 0.0F};
  }
  const float inv_len = 1.0F / std::sqrt(len_sq);
  return Vec3{v.x * inv_len, v.y * inv_len, v.z * inv_len};
}

[[nodiscard]] std::uint8_t float_to_u8(float x) noexcept {
  const float clamped = std::clamp(x, 0.0F, 1.0F);
  const float scaled = std::floor((clamped * 255.0F) + 0.5F);
  return static_cast<std::uint8_t>(std::clamp(scaled, 0.0F, 255.0F));
}

[[nodiscard]] int wrap_texel_index(int coord, int size, Wrap wrap) noexcept {
  if (wrap == Wrap::clamp) {
    return std::clamp(coord, 0, size - 1);
  }
  const int m = coord % size;
  return m < 0 ? m + size : m;
}

[[nodiscard]] float wrap_uv_unit(float u, Wrap wrap) noexcept {
  if (!std::isfinite(u)) {
    return 0.0F;
  }
  if (wrap == Wrap::clamp) {
    return std::clamp(u, 0.0F, 1.0F);
  }
  float r = u - std::floor(u);
  if (r >= 1.0F || r < 0.0F) {
    r = 0.0F;
  }
  return r;
}

[[nodiscard]] Rgba8 sample_texture(const Image& tex, Vec2 uv, Filter filter, Wrap wrap) {
  const int w = tex.width();
  const int h = tex.height();
  const auto fw = static_cast<float>(w);
  const auto fh = static_cast<float>(h);

  if (filter == Filter::nearest) {
    const float u_w = wrap_uv_unit(uv.x, wrap);
    const float v_w = wrap_uv_unit(uv.y, wrap);
    int tx = static_cast<int>(std::floor(u_w * fw));
    int ty = static_cast<int>(std::floor(v_w * fh));
    tx = std::clamp(tx, 0, w - 1);
    ty = std::clamp(ty, 0, h - 1);
    return tex.at(tx, ty);
  }

  const float u_coord = ((wrap == Wrap::clamp ? std::clamp(uv.x, 0.0F, 1.0F) : uv.x) * fw) - 0.5F;
  const float v_coord = ((wrap == Wrap::clamp ? std::clamp(uv.y, 0.0F, 1.0F) : uv.y) * fh) - 0.5F;

  const int x0 = static_cast<int>(std::floor(u_coord));
  const int y0 = static_cast<int>(std::floor(v_coord));
  const float fx = u_coord - static_cast<float>(x0);
  const float fy = v_coord - static_cast<float>(y0);

  const int ix0 = wrap_texel_index(x0, w, wrap);
  const int ix1 = wrap_texel_index(x0 + 1, w, wrap);
  const int iy0 = wrap_texel_index(y0, h, wrap);
  const int iy1 = wrap_texel_index(y0 + 1, h, wrap);

  const Rgba8 c00 = tex.at(ix0, iy0);
  const Rgba8 c10 = tex.at(ix1, iy0);
  const Rgba8 c01 = tex.at(ix0, iy1);
  const Rgba8 c11 = tex.at(ix1, iy1);

  const auto bilerp = [fx, fy](std::uint8_t v00, std::uint8_t v10, std::uint8_t v01, std::uint8_t v11) noexcept {
    const float top = ((1.0F - fx) * static_cast<float>(v00)) + (fx * static_cast<float>(v10));
    const float bot = ((1.0F - fx) * static_cast<float>(v01)) + (fx * static_cast<float>(v11));
    const float val = (((1.0F - fy) * top) + (fy * bot)) / 255.0F;
    return float_to_u8(val);
  };

  return Rgba8{
      bilerp(c00.r, c10.r, c01.r, c11.r),
      bilerp(c00.g, c10.g, c01.g, c11.g),
      bilerp(c00.b, c10.b, c01.b, c11.b),
      bilerp(c00.a, c10.a, c01.a, c11.a),
  };
}

struct ClipVertex {
  Vec4 clip;
  Vec2 uv;
  Vec3 normal;
};

[[nodiscard]] ClipVertex lerp_vertex(const ClipVertex& a, const ClipVertex& b, float t) noexcept {
  const auto lerp_f = [t](float x, float y) noexcept { return x + (t * (y - x)); };
  return ClipVertex{
      .clip =
          Vec4{
              lerp_f(a.clip.x, b.clip.x),
              lerp_f(a.clip.y, b.clip.y),
              lerp_f(a.clip.z, b.clip.z),
              lerp_f(a.clip.w, b.clip.w),
          },
      .uv = Vec2{lerp_f(a.uv.x, b.uv.x), lerp_f(a.uv.y, b.uv.y)},
      .normal =
          Vec3{
              lerp_f(a.normal.x, b.normal.x),
              lerp_f(a.normal.y, b.normal.y),
              lerp_f(a.normal.z, b.normal.z),
          },
  };
}

// Determines the near plane w threshold from the projection matrix.
// For standard perspective projection, m[11] == -1 and m[14] == -2*n*f/(f-n),
// and clip.z = -z_near * clip.w at the near plane, where z_near = m[14] / (m[10] - 1).
// For orthographic / identity matrices where m[11] == 0, clip.w == 1 and clip.z >= -clip.w.
struct NearPlane {
  bool is_perspective = false;
  float z_near = 0.0F;

  [[nodiscard]] float dist(const Vec4& c) const noexcept {
    if (is_perspective) {
      // Both w > z_near and z >= -w hold at the perspective near plane.
      return c.w - z_near;
    }
    if (c.w <= 0.0F) {
      return c.w - 1e-6F;
    }
    return c.z + c.w;
  }
};

[[nodiscard]] NearPlane extract_near_plane(const Mat4& proj) noexcept {
  // Column-major: m[10] is row 2 col 2, m[11] is row 3 col 2, m[14] is row 2 col 3, m[15] is row 3 col 3.
  if (std::fabs(proj.m.at(11) + 1.0F) < 1e-5F && std::fabs(proj.m.at(15)) < 1e-5F &&
      std::fabs(proj.m.at(10) - 1.0F) > 1e-6F) {
    const float zn = proj.m.at(14) / (proj.m.at(10) - 1.0F);
    if (zn > 0.0F && std::isfinite(zn)) {
      return NearPlane{.is_perspective = true, .z_near = zn};
    }
  }
  return NearPlane{.is_perspective = false, .z_near = 0.0F};
}

[[nodiscard]] bool is_top_or_left_edge(double x0, double y0, double x1, double y1) noexcept {
  const double dx = x1 - x0;
  const double dy = y1 - y0;
  // In screen coordinates (x right, y down) with clockwise winding (V0 -> V1 -> V2),
  // the interior lies to the right of each directed edge:
  // E(P) = (x1 - x0) * (py - y0) - (y1 - y0) * (px - x0) >= 0.
  // A top edge is horizontal (dy == 0) with the interior below it (py > y0 => dx > 0).
  // A left edge has the interior to its right (px > x(py) => -dy > 0 => dy < 0, going up).
  return (dy == 0.0 && dx > 0.0) || (dy < 0.0);
}

[[nodiscard]] Vec3 transform_normal(const Mat4& model, Vec3 n) noexcept {
  // Transform direction by upper-left 3x3 of model matrix.
  // For orthogonal + uniform scale transforms, this preserves the normal direction after normalization.
  return Vec3{
      (model.m.at(0) * n.x) + (model.m.at(4) * n.y) + (model.m.at(8) * n.z),
      (model.m.at(1) * n.x) + (model.m.at(5) * n.y) + (model.m.at(9) * n.z),
      (model.m.at(2) * n.x) + (model.m.at(6) * n.y) + (model.m.at(10) * n.z),
  };
}

[[nodiscard]] Vec3 transform_point3(const Mat4& model, Vec3 p) noexcept {
  const Vec4 r = model * Vec4{p.x, p.y, p.z, 1.0F};
  return Vec3{r.x, r.y, r.z};
}

[[nodiscard]] bool is_ascii_space(char c) noexcept {
  return c == ' ' || c == '\t' || c == '\r' || c == '\v' || c == '\f';
}

[[nodiscard]] std::string_view trim_spaces(std::string_view sv) noexcept {
  std::size_t start = 0;
  while (start < sv.size() && is_ascii_space(sv.at(start))) {
    ++start;
  }
  std::size_t end = sv.size();
  while (end > start && is_ascii_space(sv.at(end - 1))) {
    --end;
  }
  return sv.substr(start, end - start);
}

[[nodiscard]] std::string_view next_token(std::string_view& line) noexcept {
  std::size_t i = 0;
  while (i < line.size() && is_ascii_space(line.at(i))) {
    ++i;
  }
  line.remove_prefix(i);
  if (line.empty()) {
    return {};
  }
  std::size_t j = 0;
  while (j < line.size() && !is_ascii_space(line.at(j))) {
    ++j;
  }
  const std::string_view tok = line.substr(0, j);
  line.remove_prefix(j);
  return tok;
}

[[nodiscard]] float parse_float_token(std::string_view tok, std::size_t line_num) {
  if (tok.empty()) {
    throw RasterError("malformed", "missing float on line " + std::to_string(line_num));
  }
  // Reject leading '+' ONLY if we use std::from_chars, or strip leading '+' safely.
  std::string_view clean = tok;
  if (clean.front() == '+') {
    clean.remove_prefix(1);
  }
  if (clean.empty()) {
    throw RasterError("malformed", "invalid float on line " + std::to_string(line_num));
  }
  float value = 0.0F;
  // NOLINTNEXTLINE(bugprone-suspicious-stringview-data-usage,cppcoreguidelines-pro-bounds-pointer-arithmetic)
  const auto [ptr, ec] = std::from_chars(clean.data(), clean.data() + clean.size(), value);
  // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  if (ec != std::errc{} || ptr != clean.data() + clean.size() || !std::isfinite(value)) {
    throw RasterError("malformed", "invalid float on line " + std::to_string(line_num));
  }
  return value;
}

[[nodiscard]] int parse_obj_index(std::string_view tok, std::size_t current_count, std::size_t line_num) {
  if (tok.empty()) {
    throw RasterError("malformed", "empty index on line " + std::to_string(line_num));
  }
  long long raw = 0;
  // NOLINTNEXTLINE(bugprone-suspicious-stringview-data-usage,cppcoreguidelines-pro-bounds-pointer-arithmetic)
  const auto [ptr, ec] = std::from_chars(tok.data(), tok.data() + tok.size(), raw);
  // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  if (ec != std::errc{} || ptr != tok.data() + tok.size()) {
    throw RasterError("malformed", "invalid integer index on line " + std::to_string(line_num));
  }
  if (raw == 0) {
    throw RasterError("invalid_index", "zero index in OBJ on line " + std::to_string(line_num));
  }
  const auto count_ll = static_cast<long long>(current_count);
  const long long resolved = raw > 0 ? (raw - 1) : (count_ll + raw);
  if (resolved < 0 || resolved >= count_ll) {
    throw RasterError("invalid_index", "OBJ index out of bounds on line " + std::to_string(line_num));
  }
  return static_cast<int>(resolved);
}

[[nodiscard]] Corner parse_face_corner(std::string_view tok, std::size_t num_pos, std::size_t num_uv,
                                       std::size_t num_norm, std::size_t line_num) {
  Corner c{};
  const std::size_t s1 = tok.find('/');
  if (s1 == std::string_view::npos) {
    if (tok.empty()) {
      throw RasterError("malformed", "empty face corner on line " + std::to_string(line_num));
    }
    long long raw = 0;
    // NOLINTNEXTLINE(bugprone-suspicious-stringview-data-usage,cppcoreguidelines-pro-bounds-pointer-arithmetic)
    const auto [ptr, ec] = std::from_chars(tok.data(), tok.data() + tok.size(), raw);
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    if (ec != std::errc{} || ptr != tok.data() + tok.size()) {
      throw RasterError("malformed", "invalid integer index on line " + std::to_string(line_num));
    }
    c.position = parse_obj_index(tok, num_pos, line_num);
    return c;
  }

  const std::string_view pos_part = tok.substr(0, s1);
  if (pos_part.empty()) {
    throw RasterError("malformed", "missing position index in face corner on line " + std::to_string(line_num));
  }
  {
    long long raw = 0;
    // NOLINTNEXTLINE(bugprone-suspicious-stringview-data-usage,cppcoreguidelines-pro-bounds-pointer-arithmetic)
    const auto [ptr, ec] = std::from_chars(pos_part.data(), pos_part.data() + pos_part.size(), raw);
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    if (ec != std::errc{} || ptr != pos_part.data() + pos_part.size()) {
      throw RasterError("malformed", "invalid integer index on line " + std::to_string(line_num));
    }
  }

  const std::string_view rest = tok.substr(s1 + 1);
  const std::size_t s2 = rest.find('/');
  if (s2 == std::string_view::npos) {
    if (rest.empty()) {
      throw RasterError("malformed", "missing vt index after slash on line " + std::to_string(line_num));
    }
    {
      long long raw = 0;
      // NOLINTNEXTLINE(bugprone-suspicious-stringview-data-usage,cppcoreguidelines-pro-bounds-pointer-arithmetic)
      const auto [ptr, ec] = std::from_chars(rest.data(), rest.data() + rest.size(), raw);
      // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
      if (ec != std::errc{} || ptr != rest.data() + rest.size()) {
        throw RasterError("malformed", "invalid integer index on line " + std::to_string(line_num));
      }
    }
    c.position = parse_obj_index(pos_part, num_pos, line_num);
    c.uv = parse_obj_index(rest, num_uv, line_num);
    return c;
  }

  const std::string_view uv_part = rest.substr(0, s2);
  const std::string_view norm_part = rest.substr(s2 + 1);
  if (norm_part.find('/') != std::string_view::npos || (uv_part.empty() && norm_part.empty()) || norm_part.empty()) {
    throw RasterError("malformed", "malformed face vertex on line " + std::to_string(line_num));
  }
  if (!uv_part.empty()) {
    long long raw = 0;
    // NOLINTNEXTLINE(bugprone-suspicious-stringview-data-usage,cppcoreguidelines-pro-bounds-pointer-arithmetic)
    const auto [ptr, ec] = std::from_chars(uv_part.data(), uv_part.data() + uv_part.size(), raw);
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    if (ec != std::errc{} || ptr != uv_part.data() + uv_part.size()) {
      throw RasterError("malformed", "invalid integer index on line " + std::to_string(line_num));
    }
  }
  {
    long long raw = 0;
    // NOLINTNEXTLINE(bugprone-suspicious-stringview-data-usage,cppcoreguidelines-pro-bounds-pointer-arithmetic)
    const auto [ptr, ec] = std::from_chars(norm_part.data(), norm_part.data() + norm_part.size(), raw);
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    if (ec != std::errc{} || ptr != norm_part.data() + norm_part.size()) {
      throw RasterError("malformed", "invalid integer index on line " + std::to_string(line_num));
    }
  }

  c.position = parse_obj_index(pos_part, num_pos, line_num);
  if (!uv_part.empty()) {
    c.uv = parse_obj_index(uv_part, num_uv, line_num);
  }
  c.normal = parse_obj_index(norm_part, num_norm, line_num);
  return c;
}

}  // namespace

RasterError::RasterError(std::string code, const std::string& message)
    : std::invalid_argument(message), code_(std::move(code)) {}

std::string_view RasterError::code() const noexcept { return code_; }

Mat4 Mat4::identity() noexcept { return Mat4{}; }

Mat4 Mat4::perspective(float fovy_radians, float aspect, float z_near, float z_far) noexcept {
  const float half_tan = std::tan(fovy_radians * 0.5F);
  Mat4 out{};
  out.m.fill(0.0F);
  if (half_tan == 0.0F || aspect == 0.0F || z_near == z_far) {
    return out;
  }
  out.m.at(0) = 1.0F / (aspect * half_tan);
  out.m.at(5) = 1.0F / half_tan;
  out.m.at(10) = -(z_far + z_near) / (z_far - z_near);
  out.m.at(11) = -1.0F;
  out.m.at(14) = -(2.0F * z_far * z_near) / (z_far - z_near);
  return out;
}

Mat4 Mat4::look_at(Vec3 eye, Vec3 target, Vec3 up) noexcept {
  const Vec3 f = normalize_vec3(sub(target, eye));
  const Vec3 s = normalize_vec3(cross(f, up));
  const Vec3 u = cross(s, f);

  Mat4 out = Mat4::identity();
  out.m.at(0) = s.x;
  out.m.at(4) = s.y;
  out.m.at(8) = s.z;
  out.m.at(1) = u.x;
  out.m.at(5) = u.y;
  out.m.at(9) = u.z;
  out.m.at(2) = -f.x;
  out.m.at(6) = -f.y;
  out.m.at(10) = -f.z;
  out.m.at(12) = -dot(s, eye);
  out.m.at(13) = -dot(u, eye);
  out.m.at(14) = dot(f, eye);
  return out;
}

Mat4 Mat4::operator*(const Mat4& rhs) const noexcept {
  Mat4 out{};
  out.m.fill(0.0F);
  for (std::size_t col = 0; col < 4; ++col) {
    for (std::size_t row = 0; row < 4; ++row) {
      float sum = 0.0F;
      for (std::size_t k = 0; k < 4; ++k) {
        sum += m.at((k * 4) + row) * rhs.m.at((col * 4) + k);
      }
      out.m.at((col * 4) + row) = sum;
    }
  }
  return out;
}

Vec4 Mat4::operator*(const Vec4& v) const noexcept {
  return Vec4{
      (m.at(0) * v.x) + (m.at(4) * v.y) + (m.at(8) * v.z) + (m.at(12) * v.w),
      (m.at(1) * v.x) + (m.at(5) * v.y) + (m.at(9) * v.z) + (m.at(13) * v.w),
      (m.at(2) * v.x) + (m.at(6) * v.y) + (m.at(10) * v.z) + (m.at(14) * v.w),
      (m.at(3) * v.x) + (m.at(7) * v.y) + (m.at(11) * v.z) + (m.at(15) * v.w),
  };
}

Image::Image(int width, int height, Rgba8 fill) : width_(width), height_(height) {
  if (width <= 0 || height <= 0 || width > kMaxDimension || height > kMaxDimension) {
    throw RasterError("invalid_size", "image dimensions must be in 1..8192");
  }
  pixels_.assign(static_cast<std::size_t>(width) * static_cast<std::size_t>(height), fill);
}

int Image::width() const noexcept { return width_; }

int Image::height() const noexcept { return height_; }

Rgba8 Image::at(int x, int y) const {
  if (x < 0 || x >= width_ || y < 0 || y >= height_) {
    throw RasterError("invalid_index", "pixel coordinates out of bounds");
  }
  return pixels_.at((static_cast<std::size_t>(y) * static_cast<std::size_t>(width_)) + static_cast<std::size_t>(x));
}

void Image::set(int x, int y, Rgba8 c) {
  if (x < 0 || x >= width_ || y < 0 || y >= height_) {
    throw RasterError("invalid_index", "pixel coordinates out of bounds");
  }
  pixels_.at((static_cast<std::size_t>(y) * static_cast<std::size_t>(width_)) + static_cast<std::size_t>(x)) = c;
}

bool Image::operator==(const Image& other) const noexcept {
  return width_ == other.width_ && height_ == other.height_ && pixels_ == other.pixels_;
}

Renderer::Renderer(int width, int height)
    : color_(width, height, Rgba8{0, 0, 0, 0}),
      depth_(static_cast<std::size_t>(width) * static_cast<std::size_t>(height), 1.0F) {}

void Renderer::clear(Rgba8 color, float depth) {
  const int w = color_.width();
  const int h = color_.height();
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      color_.set(x, y, color);
    }
  }
  std::ranges::fill(depth_, depth);
  pixels_written_ = 0;
}

const Image& Renderer::color() const noexcept { return color_; }

float Renderer::depth_at(int x, int y) const {
  if (x < 0 || x >= color_.width() || y < 0 || y >= color_.height()) {
    throw RasterError("invalid_index", "depth pixel coordinates out of bounds");
  }
  return depth_.at((static_cast<std::size_t>(y) * static_cast<std::size_t>(color_.width())) +
                   static_cast<std::size_t>(x));
}

std::uint64_t Renderer::pixels_written() const noexcept { return pixels_written_; }

void Renderer::draw(const Mesh& mesh, const DrawState& state) {
  const Mat4 mvp = state.projection * (state.view * state.model);
  const NearPlane near_plane = extract_near_plane(state.projection);
  const Vec3 light_dir = normalize_vec3(state.light_towards);

  const int width = color_.width();
  const int height = color_.height();
  const auto f_width = static_cast<double>(width);
  const auto f_height = static_cast<double>(height);

  for (const Triangle& tri : mesh.triangles) {
    for (int i = 0; i < 3; ++i) {
      const Corner& c = tri.corners.at(static_cast<std::size_t>(i));
      if (c.position < 0 || static_cast<std::size_t>(c.position) >= mesh.positions.size()) {
        throw RasterError("invalid_index", "triangle position index out of bounds");
      }
      if (c.uv != -1 && (c.uv < 0 || static_cast<std::size_t>(c.uv) >= mesh.uvs.size())) {
        throw RasterError("invalid_index", "triangle uv index out of bounds");
      }
      if (c.normal != -1 && (c.normal < 0 || static_cast<std::size_t>(c.normal) >= mesh.normals.size())) {
        throw RasterError("invalid_index", "triangle normal index out of bounds");
      }
    }

    const Vec3 p0 = mesh.positions.at(static_cast<std::size_t>(tri.corners.at(0).position));
    const Vec3 p1 = mesh.positions.at(static_cast<std::size_t>(tri.corners.at(1).position));
    const Vec3 p2 = mesh.positions.at(static_cast<std::size_t>(tri.corners.at(2).position));

    const Vec3 wp0 = transform_point3(state.model, p0);
    const Vec3 wp1 = transform_point3(state.model, p1);
    const Vec3 wp2 = transform_point3(state.model, p2);
    const Vec3 face_normal = normalize_vec3(cross(sub(wp1, wp0), sub(wp2, wp0)));

    std::array<ClipVertex, 3> in_verts{};
    for (std::size_t i = 0; i < 3; ++i) {
      const Corner& c = tri.corners.at(i);
      const Vec3 pos = mesh.positions.at(static_cast<std::size_t>(c.position));
      in_verts.at(i).clip = mvp * Vec4{pos.x, pos.y, pos.z, 1.0F};
      in_verts.at(i).uv = (c.uv >= 0) ? mesh.uvs.at(static_cast<std::size_t>(c.uv)) : Vec2{0.0F, 0.0F};
      in_verts.at(i).normal = (c.normal >= 0)
                                  ? transform_normal(state.model, mesh.normals.at(static_cast<std::size_t>(c.normal)))
                                  : face_normal;
    }

    // Clip triangle against the near plane.
    // Vertices with dist > 0 are strictly in front of the near plane.
    const std::array<float, 3> dists = {
        near_plane.dist(in_verts.at(0).clip),
        near_plane.dist(in_verts.at(1).clip),
        near_plane.dist(in_verts.at(2).clip),
    };
    const std::array<bool, 3> inside = {
        dists.at(0) > 0.0F && in_verts.at(0).clip.w > 0.0F,
        dists.at(1) > 0.0F && in_verts.at(1).clip.w > 0.0F,
        dists.at(2) > 0.0F && in_verts.at(2).clip.w > 0.0F,
    };

    if (!inside.at(0) && !inside.at(1) && !inside.at(2)) {
      continue;
    }

    std::vector<ClipVertex> poly;
    poly.reserve(4);
    for (std::size_t i = 0; i < 3; ++i) {
      const std::size_t prev = (i + 2) % 3;
      const bool curr_in = inside.at(i);
      const bool prev_in = inside.at(prev);
      if (curr_in != prev_in) {
        const float denom = dists.at(prev) - dists.at(i);
        const float t = (std::fabs(denom) > 1e-12F) ? std::clamp(dists.at(prev) / denom, 0.0F, 1.0F) : 0.0F;
        poly.push_back(lerp_vertex(in_verts.at(prev), in_verts.at(i), t));
      }
      if (curr_in) {
        poly.push_back(in_verts.at(i));
      }
    }

    if (poly.size() < 3) {
      continue;
    }

    for (std::size_t ti = 1; ti + 1 < poly.size(); ++ti) {
      std::array<ClipVertex, 3> tv = {poly.at(0), poly.at(ti), poly.at(ti + 1)};
      if (tv.at(0).clip.w <= 0.0F || tv.at(1).clip.w <= 0.0F || tv.at(2).clip.w <= 0.0F) {
        continue;
      }

      std::array<double, 3> sx{};
      std::array<double, 3> sy{};
      std::array<double, 3> sz{};
      std::array<double, 3> inv_w{};
      for (std::size_t k = 0; k < 3; ++k) {
        const auto w = static_cast<double>(tv.at(k).clip.w);
        inv_w.at(k) = 1.0 / w;
        const double x_ndc = static_cast<double>(tv.at(k).clip.x) * inv_w.at(k);
        const double y_ndc = static_cast<double>(tv.at(k).clip.y) * inv_w.at(k);
        const double z_ndc = static_cast<double>(tv.at(k).clip.z) * inv_w.at(k);
        sx.at(k) = ((x_ndc + 1.0) * 0.5) * f_width;
        sy.at(k) = ((1.0 - y_ndc) * 0.5) * f_height;
        sz.at(k) = (z_ndc + 1.0) * 0.5;
      }

      // Normalize winding to clockwise in screen space (x right, y down).
      // Signed area = (x1 - x0)*(y2 - y0) - (y1 - y0)*(x2 - x0).
      // In screen space (y down), positive signed area corresponds to clockwise winding.
      double area = ((sx.at(1) - sx.at(0)) * (sy.at(2) - sy.at(0))) - ((sy.at(1) - sy.at(0)) * (sx.at(2) - sx.at(0)));
      if (area == 0.0 || !std::isfinite(area)) {
        continue;
      }
      if (area < 0.0) {
        std::swap(tv.at(1), tv.at(2));
        std::swap(sx.at(1), sx.at(2));
        std::swap(sy.at(1), sy.at(2));
        std::swap(sz.at(1), sz.at(2));
        std::swap(inv_w.at(1), inv_w.at(2));
        area = -area;
      }

      const double min_x_d = std::min({sx.at(0), sx.at(1), sx.at(2)});
      const double max_x_d = std::max({sx.at(0), sx.at(1), sx.at(2)});
      const double min_y_d = std::min({sy.at(0), sy.at(1), sy.at(2)});
      const double max_y_d = std::max({sy.at(0), sy.at(1), sy.at(2)});

      const int min_x = std::max(0, static_cast<int>(std::floor(min_x_d - 0.5)));
      const int max_x = std::min(width - 1, static_cast<int>(std::ceil(max_x_d - 0.5)));
      const int min_y = std::max(0, static_cast<int>(std::floor(min_y_d - 0.5)));
      const int max_y = std::min(height - 1, static_cast<int>(std::ceil(max_y_d - 0.5)));

      if (min_x > max_x || min_y > max_y) {
        continue;
      }

      const bool tl01 = is_top_or_left_edge(sx.at(0), sy.at(0), sx.at(1), sy.at(1));
      const bool tl12 = is_top_or_left_edge(sx.at(1), sy.at(1), sx.at(2), sy.at(2));
      const bool tl20 = is_top_or_left_edge(sx.at(2), sy.at(2), sx.at(0), sy.at(0));
      const double inv_area = 1.0 / area;

      for (int py = min_y; py <= max_y; ++py) {
        const double cy = static_cast<double>(py) + 0.5;
        for (int px = min_x; px <= max_x; ++px) {
          const double cx = static_cast<double>(px) + 0.5;

          const double e01 = ((sx.at(1) - sx.at(0)) * (cy - sy.at(0))) - ((sy.at(1) - sy.at(0)) * (cx - sx.at(0)));
          const double e12 = ((sx.at(2) - sx.at(1)) * (cy - sy.at(1))) - ((sy.at(2) - sy.at(1)) * (cx - sx.at(1)));
          const double e20 = ((sx.at(0) - sx.at(2)) * (cy - sy.at(2))) - ((sy.at(0) - sy.at(2)) * (cx - sx.at(2)));

          const bool in01 = (e01 > 0.0) || (e01 == 0.0 && tl01);
          const bool in12 = (e12 > 0.0) || (e12 == 0.0 && tl12);
          const bool in20 = (e20 > 0.0) || (e20 == 0.0 && tl20);

          if (!in01 || !in12 || !in20) {
            continue;
          }

          const double b0 = e12 * inv_area;
          const double b1 = e20 * inv_area;
          const double b2 = e01 * inv_area;

          const auto depth_val = static_cast<float>((b0 * sz.at(0)) + (b1 * sz.at(1)) + (b2 * sz.at(2)));
          const std::size_t pixel_idx =
              (static_cast<std::size_t>(py) * static_cast<std::size_t>(width)) + static_cast<std::size_t>(px);

          if (state.depth_test) {
            if (!(depth_val < depth_.at(pixel_idx))) {
              continue;
            }
            depth_.at(pixel_idx) = depth_val;
          }

          const double w0 = b0 * inv_w.at(0);
          const double w1 = b1 * inv_w.at(1);
          const double w2 = b2 * inv_w.at(2);
          const double w_sum = w0 + w1 + w2;
          const double w_corr = (w_sum > 0.0) ? (1.0 / w_sum) : 0.0;

          const auto pc0 = static_cast<float>(w0 * w_corr);
          const auto pc1 = static_cast<float>(w1 * w_corr);
          const auto pc2 = static_cast<float>(w2 * w_corr);

          Rgba8 tex_color{255, 255, 255, 255};
          if (state.texture != nullptr) {
            const Vec2 uv{
                (pc0 * tv.at(0).uv.x) + (pc1 * tv.at(1).uv.x) + (pc2 * tv.at(2).uv.x),
                (pc0 * tv.at(0).uv.y) + (pc1 * tv.at(1).uv.y) + (pc2 * tv.at(2).uv.y),
            };
            tex_color = sample_texture(*state.texture, uv, state.filter, state.wrap);
          }

          float light_factor = 1.0F;
          if (state.lighting) {
            const Vec3 interp_n = normalize_vec3(Vec3{
                (pc0 * tv.at(0).normal.x) + (pc1 * tv.at(1).normal.x) + (pc2 * tv.at(2).normal.x),
                (pc0 * tv.at(0).normal.y) + (pc1 * tv.at(1).normal.y) + (pc2 * tv.at(2).normal.y),
                (pc0 * tv.at(0).normal.z) + (pc1 * tv.at(1).normal.z) + (pc2 * tv.at(2).normal.z),
            });
            const float ndotl = std::max(0.0F, dot(interp_n, light_dir));
            light_factor = std::clamp(state.ambient + ndotl, 0.0F, 1.0F);
          }

          const float r_f =
              ((static_cast<float>(state.base_color.r) / 255.0F) * (static_cast<float>(tex_color.r) / 255.0F)) *
              light_factor;
          const float g_f =
              ((static_cast<float>(state.base_color.g) / 255.0F) * (static_cast<float>(tex_color.g) / 255.0F)) *
              light_factor;
          const float b_f =
              ((static_cast<float>(state.base_color.b) / 255.0F) * (static_cast<float>(tex_color.b) / 255.0F)) *
              light_factor;
          const float a_f =
              (static_cast<float>(state.base_color.a) / 255.0F) * (static_cast<float>(tex_color.a) / 255.0F);

          color_.set(px, py, Rgba8{float_to_u8(r_f), float_to_u8(g_f), float_to_u8(b_f), float_to_u8(a_f)});
          ++pixels_written_;
        }
      }
    }
  }
}

Mesh load_obj(std::string_view text, const ObjLimits& limits) {
  if (text.size() > limits.max_bytes) {
    throw RasterError("input_too_large", "OBJ input exceeds max_bytes limit");
  }

  Mesh mesh;
  std::size_t pos = 0;
  std::size_t line_num = 0;

  while (pos <= text.size()) {
    ++line_num;
    const std::size_t nl = text.find('\n', pos);
    std::string_view line = (nl == std::string_view::npos) ? text.substr(pos) : text.substr(pos, nl - pos);

    const std::size_t comment_pos = line.find('#');
    if (comment_pos != std::string_view::npos) {
      line = line.substr(0, comment_pos);
    }
    line = trim_spaces(line);

    if (!line.empty()) {
      const std::string_view keyword = next_token(line);
      if (keyword == "v") {
        if (mesh.positions.size() >= limits.max_vertices) {
          throw RasterError("too_large", "vertex position limit exceeded on line " + std::to_string(line_num));
        }
        const float x = parse_float_token(next_token(line), line_num);
        const float y = parse_float_token(next_token(line), line_num);
        const float z = parse_float_token(next_token(line), line_num);
        const std::string_view extra = next_token(line);
        if (!extra.empty()) {
          // Optional 4th w coordinate or trailing garbage: validate if present.
          (void)parse_float_token(extra, line_num);
          if (!next_token(line).empty()) {
            throw RasterError("malformed", "extra tokens in v statement on line " + std::to_string(line_num));
          }
        }
        mesh.positions.emplace_back(x, y, z);
      } else if (keyword == "vt") {
        if (mesh.uvs.size() >= limits.max_vertices) {
          throw RasterError("too_large", "vertex uv limit exceeded on line " + std::to_string(line_num));
        }
        const float u = parse_float_token(next_token(line), line_num);
        const float v = parse_float_token(next_token(line), line_num);
        const std::string_view extra = next_token(line);
        if (!extra.empty()) {
          (void)parse_float_token(extra, line_num);
          if (!next_token(line).empty()) {
            throw RasterError("malformed", "extra tokens in vt statement on line " + std::to_string(line_num));
          }
        }
        mesh.uvs.emplace_back(u, 1.0F - v);
      } else if (keyword == "vn") {
        if (mesh.normals.size() >= limits.max_vertices) {
          throw RasterError("too_large", "vertex normal limit exceeded on line " + std::to_string(line_num));
        }
        const float x = parse_float_token(next_token(line), line_num);
        const float y = parse_float_token(next_token(line), line_num);
        const float z = parse_float_token(next_token(line), line_num);
        if (!next_token(line).empty()) {
          throw RasterError("malformed", "extra tokens in vn statement on line " + std::to_string(line_num));
        }
        mesh.normals.emplace_back(x, y, z);
      } else if (keyword == "f") {
        if (mesh.positions.empty()) {
          throw RasterError("malformed", "face definition before any vertices on line " + std::to_string(line_num));
        }
        std::vector<Corner> corners;
        for (std::string_view tok = next_token(line); !tok.empty(); tok = next_token(line)) {
          corners.push_back(
              parse_face_corner(tok, mesh.positions.size(), mesh.uvs.size(), mesh.normals.size(), line_num));
        }
        if (corners.size() < 3) {
          throw RasterError("malformed", "face has fewer than 3 vertices on line " + std::to_string(line_num));
        }
        const std::size_t new_tris = corners.size() - 2;
        if (mesh.triangles.size() + new_tris > limits.max_triangles) {
          throw RasterError("too_large", "triangle limit exceeded on line " + std::to_string(line_num));
        }
        for (std::size_t i = 1; i + 1 < corners.size(); ++i) {
          mesh.triangles.push_back(Triangle{{corners.at(0), corners.at(i), corners.at(i + 1)}});
        }
      } else if (keyword == "o" || keyword == "g" || keyword == "s" || keyword == "usemtl" || keyword == "mtllib") {
        // Ignored statements per specification.
      } else {
        throw RasterError("malformed", "unknown OBJ statement on line " + std::to_string(line_num));
      }
    }

    if (nl == std::string_view::npos) {
      break;
    }
    pos = nl + 1;
  }

  return mesh;
}

std::string write_ppm(const Image& image) {
  const int w = image.width();
  const int h = image.height();
  std::string out = "P6\n" + std::to_string(w) + " " + std::to_string(h) + "\n255\n";
  out.reserve(out.size() + ((static_cast<std::size_t>(w) * static_cast<std::size_t>(h)) * 3ULL));
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      const Rgba8 px = image.at(x, y);
      out.push_back(static_cast<char>(px.r));
      out.push_back(static_cast<char>(px.g));
      out.push_back(static_cast<char>(px.b));
    }
  }
  return out;
}

Image read_ppm(std::string_view bytes) {
  if (bytes.size() > kMaxPpmBytes) {
    throw RasterError("input_too_large", "PPM byte buffer exceeds maximum size");
  }
  if (bytes.size() < 2) {
    throw RasterError("malformed", "PPM input too short");
  }
  if (bytes.front() == 'P' && bytes.at(1) != '6') {
    throw RasterError("unsupported_format", "only binary P6 PPM format is supported");
  }
  if (bytes.front() != 'P' || bytes.at(1) != '6') {
    throw RasterError("malformed", "missing P6 magic header");
  }

  std::size_t idx = 2;
  const auto is_ppm_ws = [](char c) noexcept {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
  };

  if (idx >= bytes.size() || (!is_ppm_ws(bytes.at(idx)) && bytes.at(idx) != '#')) {
    throw RasterError("malformed", "expected whitespace after P6 magic");
  }

  const auto skip_ws_and_comments = [&] {
    while (idx < bytes.size()) {
      if (is_ppm_ws(bytes.at(idx))) {
        ++idx;
      } else if (bytes.at(idx) == '#') {
        while (idx < bytes.size() && bytes.at(idx) != '\n') {
          ++idx;
        }
        if (idx < bytes.size() && bytes.at(idx) == '\n') {
          ++idx;
        }
      } else {
        break;
      }
    }
  };

  const auto read_header_int = [&]() -> int {
    skip_ws_and_comments();
    if (idx >= bytes.size()) {
      throw RasterError("malformed", "unexpected end of PPM header");
    }
    const std::size_t start = idx;
    while (idx < bytes.size() && !is_ppm_ws(bytes.at(idx))) {
      ++idx;
    }
    const std::string_view tok = bytes.substr(start, idx - start);
    int val = 0;
    // NOLINTNEXTLINE(bugprone-suspicious-stringview-data-usage,cppcoreguidelines-pro-bounds-pointer-arithmetic)
    const auto [ptr, ec] = std::from_chars(tok.data(), tok.data() + tok.size(), val);
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    if (ec != std::errc{} || ptr != tok.data() + tok.size()) {
      throw RasterError("malformed", "invalid integer in PPM header");
    }
    return val;
  };

  const int width = read_header_int();
  const int height = read_header_int();
  const int maxval = read_header_int();

  if (maxval != 255) {
    throw RasterError("unsupported_format", "PPM maxval must be 255");
  }

  // After maxval, skip optional comments and whitespace before pixel data.
  // PPM specification requires exactly a single whitespace character separating the header from the binary data.
  // But if there are comments after maxval, each comment line ends with newline.
  while (idx < bytes.size()) {
    if (bytes.at(idx) == '#') {
      while (idx < bytes.size() && bytes.at(idx) != '\n') {
        ++idx;
      }
      if (idx < bytes.size() && bytes.at(idx) == '\n') {
        ++idx;
      }
    } else {
      break;
    }
  }
  if (idx >= bytes.size() || !is_ppm_ws(bytes.at(idx))) {
    throw RasterError("malformed", "missing single whitespace after PPM maxval");
  }
  ++idx;

  Image img(width, height);
  const std::size_t expected_payload = static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 3ULL;
  if (bytes.size() - idx != expected_payload) {
    throw RasterError("malformed", "PPM pixel data size does not match dimensions");
  }

  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      const auto r = static_cast<std::uint8_t>(bytes.at(idx));
      const auto g = static_cast<std::uint8_t>(bytes.at(idx + 1));
      const auto b = static_cast<std::uint8_t>(bytes.at(idx + 2));
      idx += 3;
      img.set(x, y, Rgba8{r, g, b, 255});
    }
  }
  return img;
}

CompareResult compare(const Image& a, const Image& b, int channel_tolerance, std::size_t max_differing_pixels) {
  if (a.width() != b.width() || a.height() != b.height()) {
    throw RasterError("size_mismatch", "cannot compare images of different dimensions");
  }
  if (channel_tolerance < 0) {
    throw RasterError("malformed", "channel_tolerance must be non-negative");
  }

  CompareResult res{};
  const int w = a.width();
  const int h = a.height();

  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      const Rgba8 pa = a.at(x, y);
      const Rgba8 pb = b.at(x, y);
      const int dr = std::abs(static_cast<int>(pa.r) - static_cast<int>(pb.r));
      const int dg = std::abs(static_cast<int>(pa.g) - static_cast<int>(pb.g));
      const int db = std::abs(static_cast<int>(pa.b) - static_cast<int>(pb.b));
      const int da = std::abs(static_cast<int>(pa.a) - static_cast<int>(pb.a));
      const int pixel_max = std::max({dr, dg, db, da});
      res.max_channel_delta = std::max(res.max_channel_delta, pixel_max);
      if (pixel_max > channel_tolerance) {
        ++res.differing_pixels;
      }
    }
  }

  res.equal = (res.differing_pixels <= max_differing_pixels);
  return res;
}

}  // namespace trysac::software_rasterizer
