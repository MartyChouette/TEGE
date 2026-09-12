// Terrain texture layers: authored, saved, and never rendered.
//
// TerrainComponent carries four TextureLayers -- a path and a tile scale each --
// and a splatmap of four weights per cell. The inspector lets you pick the
// textures and set the scales. The serializer writes them. TerrainGeneratorSystem
// fills the splatmap with base/rock/snow/shore weights.
//
// Nothing binds those textures. Grep the renderer for `layers[` and there is no
// hit outside the inspector and the serializer.
//
// That alone would be a component with an authoring surface and no system,
// which this project already calls not-shipped. It is worse than that, because
// the weights do not go nowhere -- they go into the mesh's VERTEX COLOUR, and
// the fragment shader multiplies albedo by vertex colour for every material
// except water:
//
//     if ((mat_flags & FLAG_WATER_SURFACE) == 0) albedo *= fragVertColor.rgb;
//     ...
//     float alpha = mat_opacity * fragVertColor.a * texAlpha;
//
// So a generated terrain is tinted by which layer it wanted, and faded out by
// its fourth weight. A cell that is entirely "base" renders with green and blue
// scaled to zero and alpha near zero.
//
// These tests pin the DATA half of that, which is the half that can be checked
// without a GPU: the weights really do end up in vertex colour, they really are
// not white, and the fourth one really does land in alpha.
//
// THEY DESCRIBE A BUG, NOT A CONTRACT. Two of them assert that painting weights
// produces non-white vertex colours and that the fourth weight becomes alpha,
// which is exactly what must STOP being true once the layers actually render.
// They are here so the failure is written down and measurable rather than
// argued about, and they are expected to be rewritten by whoever fixes it --
// not deleted, rewritten, because the same painted terrain should then assert
// that it is NOT tinted and NOT faded.

#include "EnjinTest.h"
#include "Enjin/ECS/Components/Terrain.h"
#include "Enjin/ECS/Components/TerrainGenerator.h"
#include "Enjin/ECS/Systems/TerrainGeneratorSystem.h"
#include "Enjin/ECS/World.h"
#include "Enjin/Renderer/MeshFactory.h"

#include <cmath>

using namespace Enjin;
using namespace Enjin::ECS;

namespace {

// A terrain with a splatmap that asks for one layer in one place and another
// layer somewhere else, which is what painting weights means.
TerrainComponent PaintedTerrain() {
    TerrainComponent t;
    t.gridWidth = 8;
    t.gridHeight = 8;
    t.cellSize = 1.0f;
    t.InitializeFlat(0.0f);
    t.splatmap.assign(static_cast<usize>(t.gridWidth) * t.gridHeight * 4, 0.0f);
    for (u32 z = 0; z < t.gridHeight; ++z) {
        for (u32 x = 0; x < t.gridWidth; ++x) {
            const usize i = (static_cast<usize>(z) * t.gridWidth + x) * 4;
            // Left half entirely layer 0, right half entirely layer 1.
            if (x < 4) t.splatmap[i + 0] = 1.0f;
            else       t.splatmap[i + 1] = 1.0f;
        }
    }
    return t;
}

} // namespace

ENJIN_TEST(TerrainLayers, SplatWeightsAreBakedIntoVertexColour) {
    // Arrange
    const TerrainComponent t = PaintedTerrain();

    // Act
    const MeshComponent mesh = Renderer::MeshFactory::CreateTerrain(t);

    // Assert: the weights are in vertex colour, which is where the shader reads
    // a COLOUR from. This is the mechanism, not yet the complaint.
    ENJIN_ASSERT_TRUE(mesh.vertices.size() == static_cast<usize>(t.gridWidth) * t.gridHeight);
    const MeshComponent::Vertex& left = mesh.vertices[0];                 // x = 0
    const MeshComponent::Vertex& right = mesh.vertices[t.gridWidth - 1];  // x = 7
    ENJIN_EXPECT_FLOAT_NEAR(left.color.x, 1.0f, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(left.color.y, 0.0f, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(right.color.x, 0.0f, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(right.color.y, 1.0f, 0.001f);
}

ENJIN_TEST(TerrainLayers, PaintingWeightsProducesVertexColoursThatAreNotWhite) {
    // Arrange
    const TerrainComponent t = PaintedTerrain();

    // Act
    const MeshComponent mesh = Renderer::MeshFactory::CreateTerrain(t);

    // Assert: THIS is the complaint. The shader multiplies albedo by vertex
    // colour for every material except water, so a vertex colour that is not
    // white is a tint nobody asked for. Choosing which texture a patch of
    // ground uses currently changes what COLOUR it is.
    usize white = 0;
    for (const auto& v : mesh.vertices) {
        const bool isWhite = std::fabs(v.color.x - 1.0f) < 0.001f &&
                             std::fabs(v.color.y - 1.0f) < 0.001f &&
                             std::fabs(v.color.z - 1.0f) < 0.001f;
        if (isWhite) ++white;
    }
    ENJIN_EXPECT_EQ(white, (usize)0);
}

ENJIN_TEST(TerrainLayers, TheFourthWeightLandsInAlphaAndFadesTheGroundOut) {
    // Arrange: weights that sum to one across four layers, as a real splat
    // does, with nothing in the fourth.
    const TerrainComponent t = PaintedTerrain();

    // Act
    const MeshComponent mesh = Renderer::MeshFactory::CreateTerrain(t);

    // Assert: alpha is the fourth weight. The shader computes
    // `alpha = mat_opacity * fragVertColor.a * texAlpha`, so ground that wants
    // none of layer four is ground that is transparent.
    for (const auto& v : mesh.vertices) {
        ENJIN_EXPECT_FLOAT_NEAR(v.color.w, 0.0f, 0.001f);
    }
}

ENJIN_TEST(TerrainLayers, AGeneratedTerrainHitsThisWithoutAnybodyPaintingAnything) {
    // Arrange: the generator is the normal way a terrain gets a splatmap, so
    // this is not an edge case reached by hand-authoring.
    World w;
    Entity e = w.CreateEntity();
    auto& terrain = w.AddComponent<TerrainComponent>(e);
    terrain.gridWidth = terrain.gridHeight = 16;
    terrain.cellSize = 1.0f;
    terrain.InitializeFlat(0.0f);
    auto& gen = w.AddComponent<TerrainGeneratorComponent>(e);
    gen.maxHeight = 10.0f;

    // Act
    TerrainGeneratorSystem::Generate(gen, terrain);
    const MeshComponent mesh = Renderer::MeshFactory::CreateTerrain(terrain);

    // Assert: a splatmap exists and it is not all-white, so a terrain that was
    // generated and never touched renders tinted and faded.
    ENJIN_ASSERT_TRUE(terrain.splatmap.size() ==
                      static_cast<usize>(terrain.gridWidth) * terrain.gridHeight * 4);
    ENJIN_ASSERT_TRUE(!mesh.vertices.empty());

    usize notWhite = 0;
    for (const auto& v : mesh.vertices) {
        const bool isWhite = std::fabs(v.color.x - 1.0f) < 0.01f &&
                             std::fabs(v.color.y - 1.0f) < 0.01f &&
                             std::fabs(v.color.z - 1.0f) < 0.01f &&
                             std::fabs(v.color.w - 1.0f) < 0.01f;
        if (!isWhite) ++notWhite;
    }
    ENJIN_EXPECT_TRUE(notWhite > 0);
}

ENJIN_TEST_MAIN()
