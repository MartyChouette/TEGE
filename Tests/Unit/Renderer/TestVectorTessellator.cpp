#include "EnjinTest.h"
#include "Enjin/Renderer/VectorTessellator.h"
#include <algorithm>
#include <cmath>

using namespace Enjin;
using namespace Enjin::Renderer;

// P2 of the unified display system: SVG -> triangles, consumed by both the
// world mount (MeshComponent) and the canvas mount (ImGui draw list).

namespace {
// Sum of unsigned triangle areas.
f32 TriangleArea(const TessellatedGraphic& g) {
    f32 area = 0.0f;
    for (usize i = 0; i + 2 < g.indices.size(); i += 3) {
        const auto& a = g.vertices[g.indices[i]].pos;
        const auto& b = g.vertices[g.indices[i + 1]].pos;
        const auto& c = g.vertices[g.indices[i + 2]].pos;
        area += 0.5f * std::fabs((b.x - a.x) * (c.y - a.y) - (c.x - a.x) * (b.y - a.y));
    }
    return area;
}
} // namespace

ENJIN_TEST(VectorTessellator, RectAndEllipse) {
    // A 100x50 red rect and a green circle r=20; document 200x100.
    const std::string svg =
        "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"200\" height=\"100\">"
        "<rect x=\"10\" y=\"10\" width=\"100\" height=\"50\" fill=\"#ff0000\"/>"
        "<circle cx=\"160\" cy=\"50\" r=\"20\" fill=\"#00ff00\"/>"
        "</svg>";
    TessellatedGraphic g = TessellateSVGFromString(svg, 0.25f);
    ENJIN_ASSERT_TRUE(g.valid);
    ENJIN_EXPECT_EQ(g.shapeCount, 2u);
    ENJIN_EXPECT_TRUE(std::fabs(g.width - 200.0f) < 0.01f);
    ENJIN_EXPECT_TRUE(std::fabs(g.height - 100.0f) < 0.01f);

    // Total fill area = 5000 (rect) + pi*400 (circle) within flattening error.
    f32 area = TriangleArea(g);
    f32 expected = 5000.0f + 3.14159265f * 400.0f;
    ENJIN_EXPECT_TRUE(std::fabs(area - expected) < expected * 0.02f);

    // Colors: red verts and green verts both present, correct shape order.
    bool sawRed = false, sawGreen = false;
    for (const auto& v : g.vertices) {
        if (v.color.x > 0.9f && v.color.y < 0.1f) { sawRed = true;  ENJIN_EXPECT_EQ(v.shapeIndex, 0u); }
        if (v.color.y > 0.9f && v.color.x < 0.1f) { sawGreen = true; ENJIN_EXPECT_EQ(v.shapeIndex, 1u); }
    }
    ENJIN_EXPECT_TRUE(sawRed && sawGreen);
}

ENJIN_TEST(VectorTessellator, StrokeOnly) {
    // An open stroked polyline: no fill triangles, stroke quads only.
    const std::string svg =
        "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"100\" height=\"100\">"
        "<polyline points=\"10,10 90,10 90,90\" fill=\"none\" stroke=\"#0000ff\" stroke-width=\"4\"/>"
        "</svg>";
    TessellatedGraphic g = TessellateSVGFromString(svg, 0.25f);
    ENJIN_ASSERT_TRUE(g.valid);
    // Two segments of length 80 at width 4 = 640 area, within tolerance
    // (miterless corners overlap or gap slightly).
    f32 area = TriangleArea(g);
    ENJIN_EXPECT_TRUE(area > 500.0f && area < 800.0f);
    for (const auto& v : g.vertices) {
        ENJIN_EXPECT_TRUE(v.color.z > 0.9f);   // blue
    }
}

ENJIN_TEST(VectorTessellator, ConcaveFill) {
    // A concave L-shape exercises real ear clipping (a convex fan would leak
    // outside the polygon and overshoot the area).
    const std::string svg =
        "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"100\" height=\"100\">"
        "<path d=\"M10 10 L90 10 L90 40 L40 40 L40 90 L10 90 Z\" fill=\"#ffffff\"/>"
        "</svg>";
    TessellatedGraphic g = TessellateSVGFromString(svg, 0.25f);
    ENJIN_ASSERT_TRUE(g.valid);
    // L area = 80*30 + 30*50 = 3900.
    f32 area = TriangleArea(g);
    ENJIN_EXPECT_TRUE(std::fabs(area - 3900.0f) < 40.0f);
}

ENJIN_TEST(VectorTessellator, GarbageAndEmpty) {
    ENJIN_EXPECT_FALSE(TessellateSVGFromString("not svg at all", 0.25f).valid);
    ENJIN_EXPECT_FALSE(TessellateSVG("does/not/exist.svg", 0.25f).valid);
}

ENJIN_TEST_MAIN()
