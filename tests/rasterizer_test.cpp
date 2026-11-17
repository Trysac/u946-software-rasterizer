#include "trysac/software_rasterizer/rasterizer.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_adapters.hpp>
#include <catch2/generators/catch_generators_random.hpp>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <string>

namespace sr = trysac::software_rasterizer;

namespace {

sr::Mesh make_quad_ndc(float x0, float y0, float x1, float y1, float z = 0.0F) {
  sr::Mesh m;
  m.positions = {
      sr::Vec3{x0, y1, z},
      sr::Vec3{x1, y1, z},
      sr::Vec3{x1, y0, z},
      sr::Vec3{x0, y0, z},
  };
  m.uvs = {
      sr::Vec2{0.0F, 0.0F},
      sr::Vec2{1.0F, 0.0F},
      sr::Vec2{1.0F, 1.0F},
      sr::Vec2{0.0F, 1.0F},
  };
  m.normals = {sr::Vec3{0.0F, 0.0F, 1.0F}};
  m.triangles = {
      sr::Triangle{{sr::Corner{0, 0, 0}, sr::Corner{1, 1, 0}, sr::Corner{2, 2, 0}}},
      sr::Triangle{{sr::Corner{0, 0, 0}, sr::Corner{2, 2, 0}, sr::Corner{3, 3, 0}}},
  };
  return m;
}

}  // namespace

TEST_CASE("Fill rule: shared diagonal draws each covered pixel once", "[rasterizer][fill_rule]") {
  // WHEN two triangles sharing the diagonal cover the NDC square x in [-1, 0], y in [0, 1] on an 8 x 8 target
  // THEN exactly 16 pixels (x 0..3, y 0..3) are colored and pixels_written() == 16
  sr::Renderer renderer(8, 8);
  renderer.clear(sr::Rgba8{0, 0, 0, 255});

  const sr::Mesh mesh = make_quad_ndc(-1.0F, 0.0F, 0.0F, 1.0F, 0.0F);
  sr::DrawState state;
  state.base_color = sr::Rgba8{255, 128, 64, 255};

  renderer.draw(mesh, state);
  CHECK(renderer.pixels_written() == 16);

  int colored_count = 0;
  for (int y = 0; y < 8; ++y) {
    for (int x = 0; x < 8; ++x) {
      const sr::Rgba8 c = renderer.color().at(x, y);
      if (x >= 0 && x <= 3 && y >= 0 && y <= 3) {
        CHECK(c == state.base_color);
        ++colored_count;
      } else {
        CHECK(c == sr::Rgba8{0, 0, 0, 255});
      }
    }
  }
  CHECK(colored_count == 16);
}

TEST_CASE("Top-left rule handles horizontal bottom and right edges and both windings", "[rasterizer][fill_rule]") {
  // Cover right half [0, 1] x [-1, 0] with reverse winding to test both windings and bottom/right exclusion.
  sr::Renderer renderer(8, 8);
  renderer.clear(sr::Rgba8{0, 0, 0, 255});

  sr::Mesh mesh = make_quad_ndc(0.0F, -1.0F, 1.0F, 0.0F, 0.0F);
  // Reverse winding of second triangle to verify both windings are drawn without culling.
  std::swap(mesh.triangles.at(1).corners.at(1), mesh.triangles.at(1).corners.at(2));

  sr::DrawState state;
  state.base_color = sr::Rgba8{10, 200, 30, 255};
  renderer.draw(mesh, state);

  CHECK(renderer.pixels_written() == 16);
  for (int y = 4; y < 8; ++y) {
    for (int x = 4; x < 8; ++x) {
      CHECK(renderer.color().at(x, y) == state.base_color);
    }
  }
}

TEST_CASE("Z-buffer: overlapping quads at depth 0.5 and 0.2 are order-independent", "[rasterizer][zbuffer]") {
  const sr::Mesh red_quad = make_quad_ndc(-0.75F, -0.75F, 0.5F, 0.5F, 0.5F);
  const sr::Mesh blue_quad = make_quad_ndc(-0.5F, -0.5F, 0.75F, 0.75F, 0.2F);

  sr::DrawState red_state;
  red_state.base_color = sr::Rgba8{255, 0, 0, 255};
  sr::DrawState blue_state;
  blue_state.base_color = sr::Rgba8{0, 0, 255, 255};

  sr::Renderer r1(8, 8);
  r1.clear(sr::Rgba8{0, 0, 0, 255}, 1.0F);
  r1.draw(red_quad, red_state);
  r1.draw(blue_quad, blue_state);

  sr::Renderer r2(8, 8);
  r2.clear(sr::Rgba8{0, 0, 0, 255}, 1.0F);
  r2.draw(blue_quad, blue_state);
  r2.draw(red_quad, red_state);

  // Overlapping center pixel (3, 3) must be blue (0, 0, 255, 255) and depth (0.2 + 1) / 2 = 0.6.
  CHECK(r1.color().at(3, 3) == sr::Rgba8{0, 0, 255, 255});
  CHECK(r2.color().at(3, 3) == sr::Rgba8{0, 0, 255, 255});
  CHECK(std::fabs(r1.depth_at(3, 3) - 0.6F) < 1e-5F);

  const sr::CompareResult cmp = sr::compare(r1.color(), r2.color(), 0, 0);
  CHECK(cmp.equal);
  CHECK(cmp.differing_pixels == 0);
  CHECK(cmp.max_channel_delta == 0);
}

TEST_CASE("Texturing: 4x4 target with 2x2 nearest texture gives 2x2 quadrants", "[rasterizer][texture]") {
  sr::Image tex(2, 2);
  const sr::Rgba8 red{255, 0, 0, 255};
  const sr::Rgba8 green{0, 255, 0, 255};
  const sr::Rgba8 blue{0, 0, 255, 255};
  const sr::Rgba8 white{255, 255, 255, 255};
  tex.set(0, 0, red);
  tex.set(1, 0, green);
  tex.set(0, 1, blue);
  tex.set(1, 1, white);

  sr::Renderer renderer(4, 4);
  renderer.clear(sr::Rgba8{0, 0, 0, 0});

  const sr::Mesh quad = make_quad_ndc(-1.0F, -1.0F, 1.0F, 1.0F, 0.0F);
  sr::DrawState state;
  state.texture = &tex;
  state.filter = sr::Filter::nearest;
  renderer.draw(quad, state);

  for (int y = 0; y < 4; ++y) {
    for (int x = 0; x < 4; ++x) {
      sr::Rgba8 want = white;
      if (y < 2) {
        want = (x < 2) ? red : green;
      } else {
        want = (x < 2) ? blue : white;
      }
      CHECK(renderer.color().at(x, y) == want);
    }
  }
}

TEST_CASE("Texturing: wrap repeat vs clamp and bilinear at texel center equals nearest", "[rasterizer][texture]") {
  sr::Image tex(2, 2);
  tex.set(0, 0, sr::Rgba8{255, 0, 0, 255});
  tex.set(1, 0, sr::Rgba8{0, 255, 0, 255});
  tex.set(0, 1, sr::Rgba8{0, 0, 255, 255});
  tex.set(1, 1, sr::Rgba8{255, 255, 255, 255});

  // 2x2 target: pixel centers are at (0.25, 0.25), (0.75, 0.25), (0.25, 0.75), (0.75, 0.75),
  // which are the exact texel centers of a 2x2 texture when UVs span [0, 1].
  const sr::Mesh quad = make_quad_ndc(-1.0F, -1.0F, 1.0F, 1.0F, 0.0F);
  sr::Renderer r_nearest(2, 2);
  sr::Renderer r_bilinear(2, 2);

  sr::DrawState s_near;
  s_near.texture = &tex;
  s_near.filter = sr::Filter::nearest;
  s_near.wrap = sr::Wrap::clamp;
  r_nearest.draw(quad, s_near);

  sr::DrawState s_bi = s_near;
  s_bi.filter = sr::Filter::bilinear;
  r_bilinear.draw(quad, s_bi);

  CHECK(r_nearest.color() == r_bilinear.color());

  // Bilinear blending halfway between texel centers on a 1x1 target with UVs all (0.5, 0.5).
  sr::Mesh center_quad = quad;
  center_quad.uvs = {
      sr::Vec2{0.5F, 0.5F},
      sr::Vec2{0.5F, 0.5F},
      sr::Vec2{0.5F, 0.5F},
      sr::Vec2{0.5F, 0.5F},
  };
  sr::Renderer r_blend(1, 1);
  r_blend.draw(center_quad, s_bi);
  // Average of (255,0,0), (0,255,0), (0,0,255), (255,255,255) is 510/4 = 127.5 -> 128.
  CHECK(r_blend.color().at(0, 0) == sr::Rgba8{128, 128, 128, 255});

  // Wrap repeat vs clamp for UV = (1.25, 0.25) -> repeat gives texel (0, 0) (red), clamp gives texel (1, 0) (green).
  sr::Mesh uv_outside = quad;
  uv_outside.uvs = {
      sr::Vec2{1.25F, 0.25F},
      sr::Vec2{1.25F, 0.25F},
      sr::Vec2{1.25F, 0.25F},
      sr::Vec2{1.25F, 0.25F},
  };
  sr::Renderer r_rep(1, 1);
  sr::DrawState s_rep = s_near;
  s_rep.wrap = sr::Wrap::repeat;
  r_rep.draw(uv_outside, s_rep);
  CHECK(r_rep.color().at(0, 0) == sr::Rgba8{255, 0, 0, 255});

  sr::Renderer r_clmp(1, 1);
  sr::DrawState s_clmp = s_near;
  s_clmp.wrap = sr::Wrap::clamp;
  r_clmp.draw(uv_outside, s_clmp);
  CHECK(r_clmp.color().at(0, 0) == sr::Rgba8{0, 255, 0, 255});
}

TEST_CASE("Lighting: Lambertian and face normal fallback", "[rasterizer][lighting]") {
  // A white surface with normal (0, 0, 1), directional light towards (0, 0.6, 0.8) and ambient 0
  // renders (204, 204, 204, 255); with the light towards (0, 0, -1) it renders (0, 0, 0, 255).
  const sr::Mesh quad = make_quad_ndc(-1.0F, -1.0F, 1.0F, 1.0F, 0.0F);

  sr::Renderer renderer(2, 2);
  renderer.clear(sr::Rgba8{0, 0, 0, 255});

  sr::DrawState state;
  state.lighting = true;
  state.ambient = 0.0F;
  state.light_towards = sr::Vec3{0.0F, 0.6F, 0.8F};
  renderer.draw(quad, state);
  CHECK(renderer.color().at(0, 0) == sr::Rgba8{204, 204, 204, 255});

  renderer.clear(sr::Rgba8{10, 10, 10, 255});
  state.light_towards = sr::Vec3{0.0F, 0.0F, -1.0F};
  renderer.draw(quad, state);
  CHECK(renderer.color().at(0, 0) == sr::Rgba8{0, 0, 0, 255});

  // Without vertex normals, face normal is computed from cross(p1 - p0, p2 - p0).
  sr::Mesh no_norm_quad = quad;
  no_norm_quad.normals.clear();
  for (sr::Triangle& t : no_norm_quad.triangles) {
    for (sr::Corner& corner : t.corners) {
      corner.normal = -1;
    }
  }
  // Triangle 0 has p0=(-1,1,0), p1=(1,1,0), p2=(1,-1,0) -> (p1-p0)=(2,0,0), (p2-p0)=(2,-2,0) -> cross=(0,0,-4).
  // Swap winding so face normal points towards +Z (0, 0, 1).
  std::swap(no_norm_quad.triangles.at(0).corners.at(1), no_norm_quad.triangles.at(0).corners.at(2));
  std::swap(no_norm_quad.triangles.at(1).corners.at(1), no_norm_quad.triangles.at(1).corners.at(2));

  renderer.clear(sr::Rgba8{0, 0, 0, 255});
  state.light_towards = sr::Vec3{0.0F, 0.6F, 0.8F};
  renderer.draw(no_norm_quad, state);
  CHECK(renderer.color().at(0, 0) == sr::Rgba8{204, 204, 204, 255});
}

TEST_CASE("Perspective correctness: receding textured quad splits at projected midpoint", "[rasterizer][perspective]") {
  // 2-texel horizontal checker texture: left texel u in [0, 0.5) is red, right texel u in [0.5, 1] is blue.
  sr::Image checker(2, 1);
  const sr::Rgba8 red{255, 0, 0, 255};
  const sr::Rgba8 blue{0, 0, 255, 255};
  checker.set(0, 0, red);
  checker.set(1, 0, blue);

  // Quad in view space receding in depth: left edge at z = -2 (w = 2), right edge at z = -6 (w = 6).
  // Choose x0 and x1 so that projected x_ndc at left is -1 and at right is +1.
  // With fovy = pi/2 and aspect = 1, projection scales x by 1.0, so x_ndc = x_view / (-z_view).
  // Therefore at z = -2, x = -2 gives x_ndc = -1; at z = -6, x = +6 gives x_ndc = +1.
  // In 3D world/view space, the midpoint of the quad (where u = 0.5) is at:
  // x_mid = (-2 + 6) / 2 = +2, z_mid = (-2 + -6) / 2 = -4 => w_mid = 4.
  // Its projected NDC coordinate is x_ndc_mid = 2 / 4 = +0.5 (not 0.0, which affine interpolation gives!).
  // On a 64 x 4 target, x_ndc = +0.5 maps to pixel x_px = (0.5 + 1) / 2 * 64 = 48.0.
  // Affine interpolation would place the boundary at x_px = 32.0.
  const float pi = std::numbers::pi_v<float>;
  const sr::Mat4 proj = sr::Mat4::perspective(pi * 0.5F, 1.0F, 1.0F, 10.0F);

  sr::Mesh mesh;
  mesh.positions = {
      sr::Vec3{-2.0F, 2.0F, -2.0F},
      sr::Vec3{6.0F, 6.0F, -6.0F},
      sr::Vec3{6.0F, -6.0F, -6.0F},
      sr::Vec3{-2.0F, -2.0F, -2.0F},
  };
  mesh.uvs = {
      sr::Vec2{0.0F, 0.0F},
      sr::Vec2{1.0F, 0.0F},
      sr::Vec2{1.0F, 1.0F},
      sr::Vec2{0.0F, 1.0F},
  };
  mesh.triangles = {
      sr::Triangle{{sr::Corner{0, 0, -1}, sr::Corner{1, 1, -1}, sr::Corner{2, 2, -1}}},
      sr::Triangle{{sr::Corner{0, 0, -1}, sr::Corner{2, 2, -1}, sr::Corner{3, 3, -1}}},
  };

  // Compute projected midpoint directly from the projection matrix:
  const sr::Vec4 mid_clip = proj * sr::Vec4{2.0F, 0.0F, -4.0F, 1.0F};
  const float mid_ndc_x = mid_clip.x / mid_clip.w;
  const int width = 64;
  const int boundary_px = static_cast<int>(std::round((mid_ndc_x + 1.0F) * 0.5F * static_cast<float>(width)));
  REQUIRE(boundary_px == 48);

  sr::Renderer renderer(width, 4);
  renderer.clear(sr::Rgba8{0, 0, 0, 255});

  sr::DrawState state;
  state.projection = proj;
  state.texture = &checker;
  state.filter = sr::Filter::nearest;
  state.wrap = sr::Wrap::clamp;
  renderer.draw(mesh, state);

  // Every pixel before boundary_px (including pixels 32..47 where affine would be blue) is red,
  // and every pixel from boundary_px onwards is blue.
  for (int x = 0; x < width; ++x) {
    const sr::Rgba8 want = (x < boundary_px) ? red : blue;
    CHECK(renderer.color().at(x, 2) == want);
  }
}

TEST_CASE("Edge cases: sub-pixel triangle, degenerate triangle, and near-plane clipping", "[rasterizer][clipping]") {
  sr::Renderer renderer(8, 8);
  renderer.clear(sr::Rgba8{0, 0, 0, 255});

  // 1. Triangle smaller than a pixel that covers no pixel center (0 pixels).
  // On an 8x8 target, pixel (4, 4) center is at x_px = 4.5, y_px = 4.5.
  // Place a tiny triangle inside x_px in [4.1, 4.3], y_px in [4.1, 4.3] -> x_ndc in [0.025, 0.075].
  sr::Mesh tiny;
  tiny.positions = {
      sr::Vec3{0.025F, -0.025F, 0.0F},
      sr::Vec3{0.075F, -0.025F, 0.0F},
      sr::Vec3{0.025F, -0.075F, 0.0F},
  };
  tiny.triangles = {sr::Triangle{{sr::Corner{0}, sr::Corner{1}, sr::Corner{2}}}};
  sr::DrawState state;
  renderer.draw(tiny, state);
  CHECK(renderer.pixels_written() == 0);

  // 2. Degenerate collinear triangle (0 pixels, no error).
  sr::Mesh degen;
  degen.positions = {
      sr::Vec3{-1.0F, -1.0F, 0.0F},
      sr::Vec3{0.0F, 0.0F, 0.0F},
      sr::Vec3{1.0F, 1.0F, 0.0F},
  };
  degen.triangles = {sr::Triangle{{sr::Corner{0}, sr::Corner{1}, sr::Corner{2}}}};
  renderer.draw(degen, state);
  CHECK(renderer.pixels_written() == 0);

  // 3. Triangle completely behind the near plane (all w <= z_near) is dropped.
  const float pi = std::numbers::pi_v<float>;
  state.projection = sr::Mat4::perspective(pi * 0.5F, 1.0F, 1.0F, 10.0F);
  sr::Mesh behind;
  behind.positions = {
      sr::Vec3{-1.0F, -1.0F, -0.5F},
      sr::Vec3{1.0F, -1.0F, -0.5F},
      sr::Vec3{0.0F, 1.0F, -0.5F},
  };
  behind.triangles = {sr::Triangle{{sr::Corner{0}, sr::Corner{1}, sr::Corner{2}}}};
  renderer.draw(behind, state);
  CHECK(renderer.pixels_written() == 0);

  // 4. Triangle crossing the near plane (1 vertex behind z = -0.5, 2 vertices in front z = -2.0 -> clipped into 2
  // tris).
  sr::Mesh crossing1;
  crossing1.positions = {
      sr::Vec3{-2.0F, -2.0F, -2.0F},
      sr::Vec3{2.0F, -2.0F, -2.0F},
      sr::Vec3{0.0F, 2.0F, -0.5F},
  };
  crossing1.triangles = {sr::Triangle{{sr::Corner{0}, sr::Corner{1}, sr::Corner{2}}}};
  renderer.draw(crossing1, state);
  CHECK(renderer.pixels_written() > 0);

  // 5. Triangle crossing the near plane with 2 vertices behind and 1 in front (clipped into 1 triangle).
  renderer.clear(sr::Rgba8{0, 0, 0, 255});
  sr::Mesh crossing2;
  crossing2.positions = {
      sr::Vec3{-2.0F, -2.0F, -0.5F},
      sr::Vec3{2.0F, -2.0F, -0.5F},
      sr::Vec3{0.0F, 0.0F, -2.0F},
  };
  crossing2.triangles = {sr::Triangle{{sr::Corner{0}, sr::Corner{1}, sr::Corner{2}}}};
  renderer.draw(crossing2, state);
  CHECK(renderer.pixels_written() > 0);
}

TEST_CASE("Mat4::look_at and matrix multiplication", "[rasterizer][math]") {
  const sr::Mat4 view =
      sr::Mat4::look_at(sr::Vec3{0.0F, 0.0F, 5.0F}, sr::Vec3{0.0F, 0.0F, 0.0F}, sr::Vec3{0.0F, 1.0F, 0.0F});
  const sr::Vec4 origin_in_view = view * sr::Vec4{0.0F, 0.0F, 0.0F, 1.0F};
  CHECK(std::fabs(origin_in_view.x) < 1e-6F);
  CHECK(std::fabs(origin_in_view.y) < 1e-6F);
  CHECK(std::fabs(origin_in_view.z - (-5.0F)) < 1e-6F);
  CHECK(std::fabs(origin_in_view.w - 1.0F) < 1e-6F);

  const sr::Mat4 id = sr::Mat4::identity();
  CHECK((view * id) == view);
}

TEST_CASE("load_obj parses Wavefront subset, flips v, and enforces limits", "[rasterizer][obj]") {
  // Acceptance literal 1: fan triangulation of quad
  const sr::Mesh quad = sr::load_obj("v 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 0\nf 1 2 3 4\n");
  REQUIRE(quad.positions.size() == 4);
  REQUIRE(quad.triangles.size() == 2);
  CHECK(quad.triangles.at(0).corners.at(0).position == 0);
  CHECK(quad.triangles.at(0).corners.at(1).position == 1);
  CHECK(quad.triangles.at(0).corners.at(2).position == 2);
  CHECK(quad.triangles.at(1).corners.at(0).position == 0);
  CHECK(quad.triangles.at(1).corners.at(1).position == 2);
  CHECK(quad.triangles.at(1).corners.at(2).position == 3);

  // Acceptance literal 2: negative indices "f -4 -3 -2" resolves to 1 2 3 (0-based 0, 1, 2)
  const sr::Mesh neg = sr::load_obj("v 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 0\nf -4 -3 -2\n");
  REQUIRE(neg.triangles.size() == 1);
  CHECK(neg.triangles.at(0).corners.at(0).position == 0);
  CHECK(neg.triangles.at(0).corners.at(1).position == 1);
  CHECK(neg.triangles.at(0).corners.at(2).position == 2);

  // Forms v/vt, v//vn, v/vt/vn, comments and ignored statements (o, g, s, usemtl, mtllib), plus v flip to 1 - v
  const std::string complex_obj =
      "# comment\n"
      "mtllib test.mtl\n"
      "o Cube\n"
      "g main\n"
      "s 1\n"
      "usemtl shiny\n"
      "v 0 0 0 1.0\n"
      "v 1 0 0\n"
      "v 0 1 0\n"
      "vt 0.25 0.2\n"
      "vt 0.75 0.8 0.0\n"
      "vn 0 0 1\n"
      "f 1/1 2/2/1 3//1\n";
  const sr::Mesh cm = sr::load_obj(complex_obj);
  REQUIRE(cm.positions.size() == 3);
  REQUIRE(cm.uvs.size() == 2);
  CHECK(std::fabs(cm.uvs.at(0).x - 0.25F) < 1e-6F);
  CHECK(std::fabs(cm.uvs.at(0).y - 0.8F) < 1e-6F);
  CHECK(std::fabs(cm.uvs.at(1).y - 0.2F) < 1e-6F);
  REQUIRE(cm.normals.size() == 1);
  REQUIRE(cm.triangles.size() == 1);
  CHECK(cm.triangles.at(0).corners.at(0) == sr::Corner{0, 0, -1});
  CHECK(cm.triangles.at(0).corners.at(1) == sr::Corner{1, 1, 0});
  CHECK(cm.triangles.at(0).corners.at(2) == sr::Corner{2, -1, 0});

  // Acceptance literal 3: "f 1 2 9" throws RasterError with code() "invalid_index"
  try {
    (void)sr::load_obj("v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 9\n");
    FAIL("Expected RasterError");
  } catch (const sr::RasterError& e) {
    CHECK(e.code() == "invalid_index");
  }

  // Zero index also throws "invalid_index"
  try {
    (void)sr::load_obj("v 0 0 0\nv 1 0 0\nv 0 1 0\nf 0 1 2\n");
    FAIL("Expected RasterError");
  } catch (const sr::RasterError& e) {
    CHECK(e.code() == "invalid_index");
  }

  // Acceptance literal 4: text of 67_108_865 bytes throws "input_too_large"
  std::string huge_obj;
  huge_obj.resize(67'108'865, ' ');
  try {
    (void)sr::load_obj(huge_obj);
    FAIL("Expected RasterError");
  } catch (const sr::RasterError& e) {
    CHECK(e.code() == "input_too_large");
  }

  // Vertex and triangle limits throw "too_large"
  sr::ObjLimits small_limits;
  small_limits.max_vertices = 2;
  small_limits.max_triangles = 1;
  try {
    (void)sr::load_obj("v 0 0 0\nv 1 0 0\nv 0 1 0\n", small_limits);
    FAIL("Expected RasterError");
  } catch (const sr::RasterError& e) {
    CHECK(e.code() == "too_large");
  }

  small_limits.max_vertices = 10;
  try {
    (void)sr::load_obj("v 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 0\nf 1 2 3 4\n", small_limits);
    FAIL("Expected RasterError");
  } catch (const sr::RasterError& e) {
    CHECK(e.code() == "too_large");
  }

  // Malformed numbers include the line number and code "malformed"
  try {
    (void)sr::load_obj("v 0 0 0\nv 1 abc 0\n");
    FAIL("Expected RasterError");
  } catch (const sr::RasterError& e) {
    CHECK(e.code() == "malformed");
    CHECK(std::string_view(e.what()).find('2') != std::string_view::npos);
  }

  // Additional malformed OBJ syntax probes
  for (const std::string_view bad : {
           std::string_view{"v 1 2\n"},
           std::string_view{"vt 1\n"},
           std::string_view{"vn 1 2 3 4\n"},
           std::string_view{"f 1 2\n"},
           std::string_view{"f 1/ 2/ 3/\n"},
           std::string_view{"f 1// 2// 3//\n"},
           std::string_view{"f 1/1/1/1 2 3\n"},
           std::string_view{"unknown_stmt 1 2 3\n"},
       }) {
    try {
      (void)sr::load_obj(bad);
      FAIL("Expected RasterError for malformed OBJ");
    } catch (const sr::RasterError& e) {
      CHECK(e.code() == "malformed");
    }
  }
}

TEST_CASE("PPM write_ppm and read_ppm round-trip and error handling", "[rasterizer][ppm]") {
  // Acceptance: write_ppm of a 2 x 1 image (red, blue) returns "P6\n2 1\n255\n" followed by FF 00 00 00 00 FF
  sr::Image img(2, 1);
  img.set(0, 0, sr::Rgba8{255, 0, 0, 255});
  img.set(1, 0, sr::Rgba8{0, 0, 255, 255});

  const std::string ppm = sr::write_ppm(img);
  CHECK(ppm.size() == 17);
  CHECK(ppm.substr(0, 11) == "P6\n2 1\n255\n");
  CHECK(static_cast<std::uint8_t>(ppm.at(11)) == 0xFF);
  CHECK(static_cast<std::uint8_t>(ppm.at(12)) == 0x00);
  CHECK(static_cast<std::uint8_t>(ppm.at(13)) == 0x00);
  CHECK(static_cast<std::uint8_t>(ppm.at(14)) == 0x00);
  CHECK(static_cast<std::uint8_t>(ppm.at(15)) == 0x00);
  CHECK(static_cast<std::uint8_t>(ppm.at(16)) == 0xFF);

  // read_ppm(write_ppm(img)) equals img
  CHECK(sr::read_ppm(ppm) == img);

  // Header comments are accepted when reading
  const std::string commented_ppm =
      std::string("P6\n# comment 1\n2 # width\n1 # height\n255\n") + std::string("\xFF\x00\x00\x00\x00\xFF", 6);
  CHECK(sr::read_ppm(commented_ppm) == img);

  // A "P3" header throws "unsupported_format"
  try {
    (void)sr::read_ppm("P3\n1 1\n255\n255 0 0\n");
    FAIL("Expected RasterError");
  } catch (const sr::RasterError& e) {
    CHECK(e.code() == "unsupported_format");
  }

  // Maxval != 255 throws "unsupported_format"
  try {
    (void)sr::read_ppm(std::string("P6\n1 1\n65535\n\xFF\x00\x00", 16));
    FAIL("Expected RasterError");
  } catch (const sr::RasterError& e) {
    CHECK(e.code() == "unsupported_format");
  }

  // Invalid dimensions in PPM throw "invalid_size"
  try {
    (void)sr::read_ppm("P6\n0 1\n255\n");
    FAIL("Expected RasterError");
  } catch (const sr::RasterError& e) {
    CHECK(e.code() == "invalid_size");
  }

  // Truncated or malformed PPM throws "malformed"
  for (const std::string_view bad : {
           std::string_view{"P"},
           std::string_view{"XX\n1 1\n255\n"},
           std::string_view{"P61 1\n255\n"},
           std::string_view{"P6\n1 abc\n255\n"},
           std::string_view{"P6\n1 1\n255\n\xFF"},
       }) {
    try {
      (void)sr::read_ppm(bad);
      FAIL("Expected RasterError");
    } catch (const sr::RasterError& e) {
      CHECK(e.code() == "malformed");
    }
  }
}

TEST_CASE("compare and Image validation and bounds errors", "[rasterizer][compare]") {
  // Acceptance: compare(a, b, 2, 0) on images that differ in one pixel by 3 per channel
  // returns equal false, differing_pixels 1 and max_channel_delta 3, while compare(a, b, 3, 0) returns equal true
  sr::Image a(4, 4, sr::Rgba8{100, 100, 100, 255});
  sr::Image b(4, 4, sr::Rgba8{100, 100, 100, 255});
  b.set(2, 1, sr::Rgba8{103, 97, 103, 255});

  const sr::CompareResult r1 = sr::compare(a, b, 2, 0);
  CHECK(!r1.equal);
  CHECK(r1.differing_pixels == 1);
  CHECK(r1.max_channel_delta == 3);

  const sr::CompareResult r2 = sr::compare(a, b, 3, 0);
  CHECK(r2.equal);
  CHECK(r2.differing_pixels == 0);
  CHECK(r2.max_channel_delta == 3);

  // Size mismatch throws "size_mismatch"
  const sr::Image c(4, 5);
  try {
    (void)sr::compare(a, c, 0, 0);
    FAIL("Expected RasterError");
  } catch (const sr::RasterError& e) {
    CHECK(e.code() == "size_mismatch");
  }

  // Image dimension limits 1..8192
  for (const auto [w, h] : {std::pair{0, 4}, std::pair{4, -1}, std::pair{8193, 1}, std::pair{1, 8193}}) {
    try {
      const sr::Image bad(w, h);
      FAIL("Expected invalid_size");
    } catch (const sr::RasterError& e) {
      CHECK(e.code() == "invalid_size");
    }
  }

  // Out-of-bounds Image::at, Image::set, Renderer::depth_at, and Renderer::draw indices throw "invalid_index"
  try {
    (void)a.at(4, 0);
    FAIL("Expected invalid_index");
  } catch (const sr::RasterError& e) {
    CHECK(e.code() == "invalid_index");
  }
  try {
    a.set(-1, 0, sr::Rgba8{});
    FAIL("Expected invalid_index");
  } catch (const sr::RasterError& e) {
    CHECK(e.code() == "invalid_index");
  }

  sr::Renderer ren(4, 4);
  try {
    (void)ren.depth_at(0, 4);
    FAIL("Expected invalid_index");
  } catch (const sr::RasterError& e) {
    CHECK(e.code() == "invalid_index");
  }

  sr::Mesh bad_mesh;
  bad_mesh.positions = {sr::Vec3{}, sr::Vec3{}, sr::Vec3{}};
  bad_mesh.triangles = {sr::Triangle{{sr::Corner{0}, sr::Corner{1}, sr::Corner{5}}}};
  try {
    ren.draw(bad_mesh, sr::DrawState{});
    FAIL("Expected invalid_index");
  } catch (const sr::RasterError& e) {
    CHECK(e.code() == "invalid_index");
  }
}

TEST_CASE("Property: two triangles tiling the full target cover every pixel exactly once", "[rasterizer][property]") {
  const int size = GENERATE(take(5, random(2, 16)));
  sr::Renderer renderer(size, size);
  renderer.clear(sr::Rgba8{0, 0, 0, 255});

  const sr::Mesh full_quad = make_quad_ndc(-1.0F, -1.0F, 1.0F, 1.0F, 0.0F);
  sr::DrawState state;
  state.base_color = sr::Rgba8{42, 84, 126, 255};
  renderer.draw(full_quad, state);

  CHECK(renderer.pixels_written() == static_cast<std::uint64_t>(size) * static_cast<std::uint64_t>(size));
}
