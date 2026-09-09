// Lightmap UVs: the one property that matters is that no two triangles share a
// texel. A lightmap with overlapping islands does not look slightly wrong, it
// lights one room with another room's light, and the cause is invisible in the
// picture. So overlap is tested exhaustively rather than sampled.
#include "EnjinTest.h"
#include "Enjin/Renderer/LightmapUnwrap.h"

#include <cmath>
#include <vector>

using namespace Enjin;
using namespace Enjin::Renderer;

namespace {

// A quad in the XZ plane, `size` units on a side, as two triangles.
void MakeQuad(f32 size, std::vector<Math::Vector3>& pos, std::vector<u32>& idx) {
    pos = {
        Math::Vector3(0.0f, 0.0f, 0.0f),
        Math::Vector3(size, 0.0f, 0.0f),
        Math::Vector3(size, 0.0f, size),
        Math::Vector3(0.0f, 0.0f, size),
    };
    idx = { 0, 1, 2, 0, 2, 3 };
}

// A grid of quads, which is what a floor or a wall actually is.
void MakeGrid(u32 cells, f32 cellSize, std::vector<Math::Vector3>& pos, std::vector<u32>& idx) {
    pos.clear();
    idx.clear();
    for (u32 z = 0; z <= cells; ++z) {
        for (u32 x = 0; x <= cells; ++x) {
            pos.push_back(Math::Vector3(static_cast<f32>(x) * cellSize, 0.0f,
                                        static_cast<f32>(z) * cellSize));
        }
    }
    const u32 stride = cells + 1;
    for (u32 z = 0; z < cells; ++z) {
        for (u32 x = 0; x < cells; ++x) {
            const u32 a = z * stride + x;
            idx.insert(idx.end(), { a, a + 1, a + stride + 1, a, a + stride + 1, a + stride });
        }
    }
}

struct Box { f32 x0, y0, x1, y1; };

Box TriBounds(const std::vector<Math::Vector2>& uv, u32 t) {
    Box b{ uv[t * 3].x, uv[t * 3].y, uv[t * 3].x, uv[t * 3].y };
    for (u32 k = 1; k < 3; ++k) {
        b.x0 = std::min(b.x0, uv[t * 3 + k].x);
        b.y0 = std::min(b.y0, uv[t * 3 + k].y);
        b.x1 = std::max(b.x1, uv[t * 3 + k].x);
        b.y1 = std::max(b.y1, uv[t * 3 + k].y);
    }
    return b;
}

bool Overlap(const Box& a, const Box& b) {
    return a.x0 < b.x1 && b.x0 < a.x1 && a.y0 < b.y1 && b.y0 < a.y1;
}

} // namespace

ENJIN_TEST(LightmapUnwrap, EveryUVLandsInsideTheAtlas) {
    std::vector<Math::Vector3> pos; std::vector<u32> idx;
    MakeGrid(6, 1.0f, pos, idx);

    std::vector<Math::Vector2> uv;
    const auto r = BuildLightmapUVs(pos, idx, LightmapUnwrapOptions{}, uv);

    ENJIN_ASSERT_TRUE(r.ok);
    ENJIN_ASSERT_EQ(uv.size(), idx.size());
    for (const auto& c : uv) {
        ENJIN_EXPECT_TRUE(c.x >= 0.0f && c.x <= 1.0f);
        ENJIN_EXPECT_TRUE(c.y >= 0.0f && c.y <= 1.0f);
    }
}

ENJIN_TEST(LightmapUnwrap, NoTwoTrianglesShareAnyAtlasSpace) {
    // The whole reason this pass exists. Checked between every pair, not
    // sampled: one overlapping pair in a scene is one light leak.
    std::vector<Math::Vector3> pos; std::vector<u32> idx;
    MakeGrid(5, 1.0f, pos, idx);

    std::vector<Math::Vector2> uv;
    const auto r = BuildLightmapUVs(pos, idx, LightmapUnwrapOptions{}, uv);
    ENJIN_ASSERT_TRUE(r.ok);

    const u32 tris = r.triangleCount;
    for (u32 a = 0; a < tris; ++a) {
        for (u32 b = a + 1; b < tris; ++b) {
            ENJIN_EXPECT_FALSE(Overlap(TriBounds(uv, a), TriBounds(uv, b)));
        }
    }
}

ENJIN_TEST(LightmapUnwrap, IslandsKeepAMarginSoBilinearCannotBleed) {
    // Two triangles that touch in the world must NOT touch in the atlas. A
    // sampler reading the edge of one island would otherwise mix in its
    // neighbour, which is the classic seam of light across a corner.
    std::vector<Math::Vector3> pos; std::vector<u32> idx;
    MakeQuad(4.0f, pos, idx);

    LightmapUnwrapOptions opt;
    opt.padding = 2;
    opt.atlasSize = 256;
    std::vector<Math::Vector2> uv;
    const auto r = BuildLightmapUVs(pos, idx, opt, uv);
    ENJIN_ASSERT_TRUE(r.ok);

    const Box a = TriBounds(uv, 0);
    const Box b = TriBounds(uv, 1);
    const f32 texel = 1.0f / static_cast<f32>(opt.atlasSize);
    // Separated along at least one axis by at least the padding.
    const bool apartX = (b.x0 - a.x1) >= texel * 2.0f || (a.x0 - b.x1) >= texel * 2.0f;
    const bool apartY = (b.y0 - a.y1) >= texel * 2.0f || (a.y0 - b.y1) >= texel * 2.0f;
    ENJIN_EXPECT_TRUE(apartX || apartY);
}

ENJIN_TEST(LightmapUnwrap, TexelDensityFollowsWorldSizeNotTriangleCount) {
    // A wall twice as big must get twice the texels across it, or lighting
    // detail would depend on how a mesh happened to be subdivided.
    std::vector<Math::Vector3> smallPos, bigPos; std::vector<u32> smallIdx, bigIdx;
    MakeQuad(2.0f, smallPos, smallIdx);
    MakeQuad(4.0f, bigPos, bigIdx);

    LightmapUnwrapOptions opt;
    opt.atlasSize = 1024;
    std::vector<Math::Vector2> smallUV, bigUV;
    ENJIN_ASSERT_TRUE(BuildLightmapUVs(smallPos, smallIdx, opt, smallUV).ok);
    ENJIN_ASSERT_TRUE(BuildLightmapUVs(bigPos, bigIdx, opt, bigUV).ok);

    const Box s = TriBounds(smallUV, 0);
    const Box b = TriBounds(bigUV, 0);
    const f32 sw = s.x1 - s.x0;
    const f32 bw = b.x1 - b.x0;
    // Twice the world size, near enough twice the atlas footprint.
    ENJIN_EXPECT_TRUE(bw > sw * 1.8f && bw < sw * 2.2f);
}

ENJIN_TEST(LightmapUnwrap, RunningOutOfAtlasFailsLoudlyAndSaysWhy) {
    // The alternative is islands silently stacking on top of each other, which
    // produces a lightmap that is wrong in a way nobody can see the cause of.
    std::vector<Math::Vector3> pos; std::vector<u32> idx;
    MakeGrid(20, 4.0f, pos, idx);

    LightmapUnwrapOptions opt;
    opt.atlasSize = 64;
    opt.texelsPerUnit = 32.0f;
    std::vector<Math::Vector2> uv;
    const auto r = BuildLightmapUVs(pos, idx, opt, uv);

    ENJIN_EXPECT_FALSE(r.ok);
    ENJIN_EXPECT_TRUE(!r.error.empty());
    ENJIN_EXPECT_TRUE(uv.empty());
}

ENJIN_TEST(LightmapUnwrap, ADegenerateTriangleDoesNotBreakTheLayout) {
    // Imported meshes carry zero-area triangles. They have no light to receive,
    // but they must not consume the atlas or knock the packing out of step.
    std::vector<Math::Vector3> pos = {
        Math::Vector3(0.0f, 0.0f, 0.0f),
        Math::Vector3(1.0f, 0.0f, 0.0f),
        Math::Vector3(1.0f, 0.0f, 0.0f),   // duplicate: no area
        Math::Vector3(0.0f, 0.0f, 1.0f),
    };
    std::vector<u32> idx = { 0, 1, 2, 0, 1, 3 };

    std::vector<Math::Vector2> uv;
    const auto r = BuildLightmapUVs(pos, idx, LightmapUnwrapOptions{}, uv);

    ENJIN_ASSERT_TRUE(r.ok);
    ENJIN_ASSERT_EQ(r.triangleCount, 2u);
    // The real triangle still got a real island.
    const Box b = TriBounds(uv, 1);
    ENJIN_EXPECT_TRUE((b.x1 - b.x0) > 0.0f && (b.y1 - b.y0) > 0.0f);
}

ENJIN_TEST(LightmapUnwrap, TheSameMeshAlwaysUnwrapsIdentically) {
    // A bake is an offline artifact checked into a project. If the layout
    // wandered between runs, every rebake would invalidate the textures beside
    // it and produce a diff nobody could review.
    std::vector<Math::Vector3> pos; std::vector<u32> idx;
    MakeGrid(4, 1.5f, pos, idx);

    std::vector<Math::Vector2> a, b;
    ENJIN_ASSERT_TRUE(BuildLightmapUVs(pos, idx, LightmapUnwrapOptions{}, a).ok);
    ENJIN_ASSERT_TRUE(BuildLightmapUVs(pos, idx, LightmapUnwrapOptions{}, b).ok);

    ENJIN_ASSERT_EQ(a.size(), b.size());
    for (usize i = 0; i < a.size(); ++i) {
        ENJIN_EXPECT_TRUE(a[i].x == b[i].x && a[i].y == b[i].y);
    }
}

ENJIN_TEST(LightmapUnwrap, BadInputIsRejectedRatherThanIndexedInto) {
    std::vector<Math::Vector3> pos = { Math::Vector3(0,0,0), Math::Vector3(1,0,0), Math::Vector3(0,0,1) };
    std::vector<Math::Vector2> uv;

    // Not a whole number of triangles.
    ENJIN_EXPECT_FALSE(BuildLightmapUVs(pos, { 0, 1 }, LightmapUnwrapOptions{}, uv).ok);
    // An index past the end -- a scene file is editable text, and this would
    // otherwise read whatever sits after the vertex buffer.
    ENJIN_EXPECT_FALSE(BuildLightmapUVs(pos, { 0, 1, 7 }, LightmapUnwrapOptions{}, uv).ok);
}

ENJIN_TEST_MAIN()
