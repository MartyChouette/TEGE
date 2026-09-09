// The bake, tested against scenes whose answer is known by hand.
//
// A lightmap is judged by eye in practice, which is exactly why the properties
// worth pinning are the ones an eye is bad at: that a shadow is actually a
// shadow rather than a dark texture, that three basis maps really do differ
// (baking the same value three times turns the technique off while looking
// completely normal), and that a rebake of unchanged geometry produces
// identical bytes.
#include "EnjinTest.h"
#include "Enjin/Renderer/LightmapBake.h"
#include "Enjin/Renderer/LightmapUnwrap.h"

#include <cmath>
#include <vector>

using namespace Enjin;
using namespace Enjin::Renderer;

namespace {

// A quad in the XZ plane at height y, facing up, as two BakeTriangles with
// lightmap UVs from the real unwrap.
std::vector<BakeTriangle> MakeFloor(f32 size, f32 y, u32 atlas, bool faceUp = true) {
    std::vector<Math::Vector3> pos = {
        Math::Vector3(0.0f, y, 0.0f),
        Math::Vector3(size, y, 0.0f),
        Math::Vector3(size, y, size),
        Math::Vector3(0.0f, y, size),
    };
    std::vector<u32> idx = { 0, 1, 2, 0, 2, 3 };

    LightmapUnwrapOptions uo;
    uo.atlasSize = atlas;
    uo.texelsPerUnit = 4.0f;
    std::vector<Math::Vector2> uv;
    BuildLightmapUVs(pos, idx, uo, uv);

    const Math::Vector3 n(0.0f, faceUp ? 1.0f : -1.0f, 0.0f);
    std::vector<BakeTriangle> tris;
    for (u32 t = 0; t < 2; ++t) {
        BakeTriangle bt;
        for (u32 k = 0; k < 3; ++k) {
            bt.position[k] = pos[idx[t * 3 + k]];
            bt.normal[k] = n;
            bt.tangent[k] = Math::Vector4(1.0f, 0.0f, 0.0f, 1.0f);
            bt.uv1[k] = uv.empty() ? Math::Vector2(0.0f, 0.0f) : uv[t * 3 + k];
        }
        tris.push_back(bt);
    }
    return tris;
}

// Brightest texel across an atlas, as a rough "is there light here" probe.
u32 PeakLuma(const std::vector<u8>& atlas) {
    u32 peak = 0;
    for (usize i = 0; i + 2 < atlas.size(); i += 3) {
        const u32 l = (static_cast<u32>(atlas[i]) + atlas[i + 1] + atlas[i + 2]) / 3;
        peak = std::max(peak, l);
    }
    return peak;
}

BakeLight SunFromAbove(f32 intensity = 1.0f) {
    BakeLight l;
    l.type = BakeLight::Type::Directional;
    l.vector = Math::Vector3(0.0f, -1.0f, 0.0f);   // travelling downward
    l.color = Math::Vector3(1.0f, 1.0f, 1.0f);
    l.intensity = intensity;
    return l;
}

LightmapBakeOptions QuickOptions(u32 atlas) {
    LightmapBakeOptions o;
    o.atlasSize = atlas;
    o.skySamples = 0;          // direct light only, so tests stay fast and exact
    o.skyIntensity = 0.0f;
    return o;
}

} // namespace

// --- The ray scene -----------------------------------------------------------

ENJIN_TEST(LightmapBake, ARayAtATriangleIsOccludedAndOneBesideItIsNot) {
    BakeRayScene scene;
    scene.Build(MakeFloor(4.0f, 0.0f, 128));

    // Straight up at the floor from below.
    ENJIN_EXPECT_TRUE(scene.Occluded(Math::Vector3(1.0f, -1.0f, 1.0f),
                                     Math::Vector3(0.0f, 1.0f, 0.0f), 10.0f));
    // Beside it: the floor spans 0..4, so x = -5 misses entirely.
    ENJIN_EXPECT_FALSE(scene.Occluded(Math::Vector3(-5.0f, -1.0f, 1.0f),
                                      Math::Vector3(0.0f, 1.0f, 0.0f), 10.0f));
}

ENJIN_TEST(LightmapBake, ARayThatStopsShortIsNotOccluded) {
    // maxDistance is what makes a point light's shadow test correct: geometry
    // BEHIND the light must not shadow it.
    BakeRayScene scene;
    scene.Build(MakeFloor(4.0f, 0.0f, 128));

    ENJIN_EXPECT_FALSE(scene.Occluded(Math::Vector3(1.0f, -5.0f, 1.0f),
                                      Math::Vector3(0.0f, 1.0f, 0.0f), 2.0f));
    ENJIN_EXPECT_TRUE(scene.Occluded(Math::Vector3(1.0f, -5.0f, 1.0f),
                                     Math::Vector3(0.0f, 1.0f, 0.0f), 10.0f));
}

ENJIN_TEST(LightmapBake, AnEmptySceneOccludesNothingRatherThanCrashing) {
    BakeRayScene scene;
    scene.Build({});
    ENJIN_EXPECT_FALSE(scene.Occluded(Math::Vector3(0, 0, 0), Math::Vector3(0, 1, 0), 100.0f));
    ENJIN_EXPECT_EQ(scene.TriangleCount(), 0u);
}

ENJIN_TEST(LightmapBake, EveryTriangleIsReachableThroughTheHierarchy) {
    // A BVH that drops triangles produces shadows with holes in them, which
    // looks like light leaking rather than like a broken tree. Fire a ray at
    // each triangle in a grid and require every one of them to be found.
    std::vector<BakeTriangle> tris;
    for (int i = 0; i < 40; ++i) {
        auto quad = MakeFloor(1.0f, 0.0f, 256);
        for (auto& t : quad) {
            for (u32 k = 0; k < 3; ++k) t.position[k].x += static_cast<f32>(i) * 2.0f;
        }
        tris.insert(tris.end(), quad.begin(), quad.end());
    }
    BakeRayScene scene;
    scene.Build(tris);
    ENJIN_ASSERT_EQ(scene.TriangleCount(), static_cast<u32>(tris.size()));

    for (int i = 0; i < 40; ++i) {
        const f32 x = static_cast<f32>(i) * 2.0f + 0.5f;
        ENJIN_EXPECT_TRUE(scene.Occluded(Math::Vector3(x, -1.0f, 0.5f),
                                         Math::Vector3(0.0f, 1.0f, 0.0f), 10.0f));
    }
}

// --- The bake ----------------------------------------------------------------

ENJIN_TEST(LightmapBake, AFloorUnderASunIsLit) {
    const auto r = BakeLightmap(MakeFloor(4.0f, 0.0f, 128), { SunFromAbove() }, QuickOptions(128));

    ENJIN_ASSERT_TRUE(r.ok);
    ENJIN_EXPECT_TRUE(r.litTexels > 0);
    for (u32 b = 0; b < kRNMBasisCount; ++b) {
        ENJIN_EXPECT_TRUE(PeakLuma(r.basis[b]) > 40);
    }
}

ENJIN_TEST(LightmapBake, AFloorUnderALidIsInShadow) {
    // The reason to bake at all. Same floor, same sun, with a second quad above
    // it: the floor must come out dark.
    const auto lid = MakeFloor(4.0f, 2.0f, 128);

    // Bake ONLY the floor's texels, with the lid present as geometry. Sharing
    // an atlas layout would put both surfaces on the same texels.
    auto floorOnly = MakeFloor(4.0f, 0.0f, 128);
    LightmapBakeResult lit = BakeLightmap(floorOnly, { SunFromAbove() }, QuickOptions(128));
    ENJIN_ASSERT_TRUE(lit.ok);
    const u32 openPeak = PeakLuma(lit.basis[0]);

    // Now with the lid in the geometry list, occupying atlas space of its own.
    auto both = MakeFloor(4.0f, 0.0f, 128);
    for (const auto& t : lid) both.push_back(t);
    // Give the lid its own island so it does not overwrite the floor's texels.
    for (usize i = 2; i < both.size(); ++i) {
        for (u32 k = 0; k < 3; ++k) both[i].uv1[k].y += 0.5f;
    }
    LightmapBakeResult shadowed = BakeLightmap(both, { SunFromAbove() }, QuickOptions(128));
    ENJIN_ASSERT_TRUE(shadowed.ok);

    // Measure the floor's OWN island, not "the top half of the atlas". The lid
    // sits at v + 0.5, and dilation deliberately smears every island a couple
    // of texels outward -- so a naive half-and-half split reads the lid's halo
    // and reports the floor as brightly lit. The floor's island is ~21 texels
    // tall at this density; 40 clears it and its halo with room to spare.
    u32 floorPeak = 0;
    const u32 size = 128;
    for (u32 y = 0; y < 40; ++y) {
        for (u32 x = 0; x < size; ++x) {
            const usize t = (static_cast<usize>(y) * size + x) * 3;
            const u32 l = (static_cast<u32>(shadowed.basis[0][t]) +
                           shadowed.basis[0][t + 1] + shadowed.basis[0][t + 2]) / 3;
            floorPeak = std::max(floorPeak, l);
        }
    }
    ENJIN_EXPECT_TRUE(openPeak > 40);
    ENJIN_EXPECT_TRUE(floorPeak < openPeak / 2);
}

ENJIN_TEST(LightmapBake, LightFromOneSideMakesTheThreeBasisMapsDiffer) {
    // If the three came out identical the technique would be off while looking
    // entirely normal -- a normal map over it would simply do nothing, and the
    // cause would be invisible. This is the assertion that catches that.
    BakeLight side;
    side.type = BakeLight::Type::Directional;
    side.vector = Math::Vector3(-1.0f, -0.3f, 0.0f);   // strongly from one side
    side.intensity = 1.0f;

    const auto r = BakeLightmap(MakeFloor(4.0f, 0.0f, 128), { side }, QuickOptions(128));
    ENJIN_ASSERT_TRUE(r.ok);

    const u32 a = PeakLuma(r.basis[0]);
    const u32 b = PeakLuma(r.basis[1]);
    const u32 c = PeakLuma(r.basis[2]);
    ENJIN_EXPECT_TRUE(a != c || b != c);
}

ENJIN_TEST(LightmapBake, LightFromStraightAboveTreatsAllThreeAlike) {
    // The complement of the test above, and the sanity check on the basis: a
    // symmetric light must not favour a direction. If it does, the basis is
    // lopsided and every surface will lean.
    const auto r = BakeLightmap(MakeFloor(4.0f, 0.0f, 128), { SunFromAbove() }, QuickOptions(128));
    ENJIN_ASSERT_TRUE(r.ok);

    const i32 a = static_cast<i32>(PeakLuma(r.basis[0]));
    const i32 b = static_cast<i32>(PeakLuma(r.basis[1]));
    const i32 c = static_cast<i32>(PeakLuma(r.basis[2]));
    ENJIN_EXPECT_TRUE(std::abs(a - b) <= 2);
    ENJIN_EXPECT_TRUE(std::abs(b - c) <= 2);
}

ENJIN_TEST(LightmapBake, ABakeOfUnchangedGeometryIsByteIdentical) {
    // A bake is an offline artifact checked into a project. If it wandered
    // between runs, every rebake would produce a diff nobody could review --
    // and the sky term is sampled randomly, so this is a real risk rather than
    // a theoretical one.
    LightmapBakeOptions o = QuickOptions(64);
    o.skySamples = 8;
    o.skyIntensity = 1.0f;

    const auto a = BakeLightmap(MakeFloor(4.0f, 0.0f, 64), { SunFromAbove() }, o);
    const auto b = BakeLightmap(MakeFloor(4.0f, 0.0f, 64), { SunFromAbove() }, o);

    ENJIN_ASSERT_TRUE(a.ok && b.ok);
    for (u32 i = 0; i < kRNMBasisCount; ++i) {
        ENJIN_ASSERT_EQ(a.basis[i].size(), b.basis[i].size());
        bool same = true;
        for (usize k = 0; k < a.basis[i].size(); ++k) {
            if (a.basis[i][k] != b.basis[i][k]) { same = false; break; }
        }
        ENJIN_EXPECT_TRUE(same);
    }
}

ENJIN_TEST(LightmapBake, APointLightBeyondItsRangeContributesNothing) {
    BakeLight p;
    p.type = BakeLight::Type::Point;
    p.vector = Math::Vector3(2.0f, 50.0f, 2.0f);   // far above
    p.range = 5.0f;
    p.intensity = 5.0f;

    const auto r = BakeLightmap(MakeFloor(4.0f, 0.0f, 128), { p }, QuickOptions(128));
    ENJIN_ASSERT_TRUE(r.ok);
    ENJIN_EXPECT_EQ(PeakLuma(r.basis[0]), 0u);
}

ENJIN_TEST(LightmapBake, NothingToBakeIsAnErrorRatherThanAnEmptyAtlas) {
    const auto r = BakeLightmap({}, { SunFromAbove() }, QuickOptions(64));
    ENJIN_EXPECT_FALSE(r.ok);
    ENJIN_EXPECT_TRUE(!r.error.empty());
}

ENJIN_TEST_MAIN()
