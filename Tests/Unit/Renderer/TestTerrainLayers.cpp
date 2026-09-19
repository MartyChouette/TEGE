// Terrain texture layers: authored, saved, and still not rendered -- but they
// no longer tint the ground on their way to nowhere.
//
// TerrainComponent carries four TextureLayers (a path and a tile scale each)
// and a splatmap of four weights per cell. The inspector lets you pick the
// textures and set the scales. The serializer writes them. TerrainGeneratorSystem
// fills the splatmap with base/rock/snow/shore weights.
//
// Nothing binds those textures. Grep the renderer for `layers[` and there is no
// hit outside the inspector and the serializer. That is still true, and it is
// still a component with an authoring surface and no system.
//
// What this file used to pin was worse than that. The weights did not go
// nowhere -- they were baked into the mesh's VERTEX COLOUR, and the fragment
// shader multiplies albedo by vertex colour for every material except water:
//
//     if ((mat_flags & FLAG_WATER_SURFACE) == 0) albedo *= fragVertColor.rgb;
//
// So a cell that was entirely layer 0 rendered with green and blue scaled to
// zero: RED ground. Layer 1 was green, layer 2 blue, layer 3 black. Choosing
// which texture a patch of ground wanted changed what COLOUR it was.
//
// FIXED 2026-09-19 by not writing the weights into vertex colour at all.
// terrain.splatmap stays the authoritative store -- still painted, still
// generated, still serialized -- and a real splat shader will read it from
// there. Shipping it through a channel that means "tint", years before the
// consumer exists, was the bug.
//
// The old version of this file said so and asked to be rewritten rather than
// deleted, so that the same painted terrain would assert it is NOT tinted.
// That is what these now do.
//
// One correction to what it claimed. It also asserted that the fourth weight
// landing in alpha "fades the ground out", because the shader computes
// `alpha = mat_opacity * fragVertColor.a * texAlpha`. Reading further, opaque
// and mask materials both force `alpha = 1.0` a few lines later, so opaque
// terrain was never transparent -- only a Blend-mode material would have been.
// The tint was real; the fade was overstated.
//
// Web never had either symptom: its PBR shader has no vertex-colour multiply,
// so the browser rendered plain untinted terrain the whole time and the two
// backends disagreed.

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

bool IsWhite(const Math::Vector4& c) {
    return std::fabs(c.x - 1.0f) < 0.001f && std::fabs(c.y - 1.0f) < 0.001f &&
           std::fabs(c.z - 1.0f) < 0.001f && std::fabs(c.w - 1.0f) < 0.001f;
}

} // namespace

ENJIN_TEST(TerrainLayers, PaintedWeightsDoNotReachVertexColour) {
    // The inverse of what this test asserted before the fix: the two halves of
    // a painted terrain used to come back as (1,0,0,0) and (0,1,0,0).
    // Arrange
    const TerrainComponent t = PaintedTerrain();

    // Act
    const MeshComponent mesh = Renderer::MeshFactory::CreateTerrain(t);

    // Assert
    ENJIN_ASSERT_TRUE(mesh.vertices.size() == static_cast<usize>(t.gridWidth) * t.gridHeight);
    const MeshComponent::Vertex& left = mesh.vertices[0];                 // x = 0, layer 0
    const MeshComponent::Vertex& right = mesh.vertices[t.gridWidth - 1];  // x = 7, layer 1
    ENJIN_EXPECT_TRUE(IsWhite(left.color));
    ENJIN_EXPECT_TRUE(IsWhite(right.color));
}

ENJIN_TEST(TerrainLayers, EveryVertexIsWhiteHoweverTheGroundIsPainted) {
    // White is the identity for `albedo *= fragVertColor.rgb`, so this is the
    // assertion that the ground takes its material's colour and nothing else.
    // Arrange
    const TerrainComponent t = PaintedTerrain();

    // Act
    const MeshComponent mesh = Renderer::MeshFactory::CreateTerrain(t);

    // Assert
    ENJIN_ASSERT_TRUE(!mesh.vertices.empty());
    for (const auto& v : mesh.vertices) {
        ENJIN_EXPECT_TRUE(IsWhite(v.color));
    }
}

ENJIN_TEST(TerrainLayers, AlphaIsOneSoTheGroundIsNeverFadedByItsWeights) {
    // The fourth weight used to land in alpha. Opaque materials clamp alpha to
    // 1.0 anyway, so this never showed -- but a Blend-mode terrain would have
    // gone see-through, and the transport is gone either way.
    // Arrange
    const TerrainComponent t = PaintedTerrain();

    // Act
    const MeshComponent mesh = Renderer::MeshFactory::CreateTerrain(t);

    // Assert
    for (const auto& v : mesh.vertices) {
        ENJIN_EXPECT_FLOAT_NEAR(v.color.w, 1.0f, 0.001f);
    }
}

ENJIN_TEST(TerrainLayers, AGeneratedTerrainIsUntintedToo) {
    // The generator is the normal way a terrain gets a splatmap, so this was
    // never an edge case reached only by hand-authoring: auto-splat produced
    // base/rock/snow/shore weights and the ground came out in primary colours.
    // Arrange
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

    // Assert
    ENJIN_ASSERT_TRUE(!mesh.vertices.empty());
    for (const auto& v : mesh.vertices) {
        ENJIN_EXPECT_TRUE(IsWhite(v.color));
    }
}

ENJIN_TEST(TerrainLayers, TheSplatmapSurvivesMeshingAndIsStillTheStore) {
    // The weights are not deleted, only un-shipped. A real splat shader reads
    // them from the component, and the generator still writes them, so the
    // feature's data is intact and waiting for its consumer.
    // Arrange
    World w;
    Entity e = w.CreateEntity();
    auto& terrain = w.AddComponent<TerrainComponent>(e);
    terrain.gridWidth = terrain.gridHeight = 16;
    terrain.cellSize = 1.0f;
    terrain.InitializeFlat(0.0f);
    auto& gen = w.AddComponent<TerrainGeneratorComponent>(e);
    gen.maxHeight = 10.0f;
    gen.autoSplat = true;

    // Act
    TerrainGeneratorSystem::Generate(gen, terrain);
    const MeshComponent mesh = Renderer::MeshFactory::CreateTerrain(terrain);

    // Assert: still there, still sized, still summing to one per cell.
    const usize cells = static_cast<usize>(terrain.gridWidth) * terrain.gridHeight;
    ENJIN_ASSERT_TRUE(terrain.splatmap.size() == cells * 4);
    ENJIN_ASSERT_TRUE(!mesh.vertices.empty());
    for (usize i = 0; i < cells; ++i) {
        const f32 sum = terrain.splatmap[i * 4 + 0] + terrain.splatmap[i * 4 + 1] +
                        terrain.splatmap[i * 4 + 2] + terrain.splatmap[i * 4 + 3];
        ENJIN_EXPECT_FLOAT_NEAR(sum, 1.0f, 0.01f);
    }
}

ENJIN_TEST(TerrainLayers, LayerPathsAndScalesStillAuthorAndPersist) {
    // What makes this easy to mistake for a working texturing system: every
    // part of it works except the part that draws.
    // Arrange
    TerrainComponent t = PaintedTerrain();
    t.layers[0].texturePath = "textures/grass.png";
    t.layers[1].texturePath = "textures/rock.png";
    t.layers[1].tileScale = 8.0f;

    // Act
    const MeshComponent mesh = Renderer::MeshFactory::CreateTerrain(t);

    // Assert
    ENJIN_ASSERT_TRUE(!mesh.vertices.empty());
    ENJIN_EXPECT_TRUE(t.layers[0].texturePath == "textures/grass.png");
    ENJIN_EXPECT_TRUE(t.layers[1].texturePath == "textures/rock.png");
    ENJIN_EXPECT_FLOAT_NEAR(t.layers[1].tileScale, 8.0f, 0.001f);
}

ENJIN_TEST_MAIN()
