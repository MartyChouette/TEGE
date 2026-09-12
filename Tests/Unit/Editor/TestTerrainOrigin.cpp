// The terrain mesh is centred on its transform; the editor's tools indexed from
// the corner.
//
// MeshFactory::CreateTerrain builds vertices at `x * cellSize - halfW`, under a
// comment saying "Center the terrain". ApplyBrush and RaycastTerrain both did
// `worldHit.x - transform.x` and bounds-checked against [0, width) -- the corner
// convention. The creative tool's placement believed the corner too, and said so
// in a comment: "A TerrainComponent's grid runs from its transform out to +X/+Z
// rather than straddling it."
//
// Three places agreeing with each other and disagreeing with the renderer. Every
// sculpt landed half a terrain from the cursor -- 31.5 metres diagonally on the
// 64x64 grid the creative tool makes -- with the brush ring drawn in exactly the
// right place, which is what made it look like the brush was broken rather than
// the mapping.
#include "EnjinTest.h"
#include "Enjin/ECS/Components/Terrain.h"
#include "Enjin/Renderer/MeshFactory.h"

using namespace Enjin;

namespace {

ECS::TerrainComponent MakeTerrain(u32 grid, f32 cell) {
    ECS::TerrainComponent t;
    t.gridWidth = grid;
    t.gridHeight = grid;
    t.cellSize = cell;
    t.InitializeFlat(0.0f);
    return t;
}

} // namespace

ENJIN_TEST(TerrainOrigin, GridOriginIsHalfATerrainFromTheTransform) {
    const ECS::TerrainComponent t = MakeTerrain(64, 1.0f);
    const Math::Vector3 at(10.0f, 3.0f, -4.0f);
    const Math::Vector3 o = t.GridOrigin(at);

    // (64 - 1) * 1.0 * 0.5
    ENJIN_EXPECT_FLOAT_NEAR(o.x, 10.0f - 31.5f, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(o.z, -4.0f - 31.5f, 0.001f);
    // Height is untouched: the centring is horizontal.
    ENJIN_EXPECT_FLOAT_NEAR(o.y, 3.0f, 0.001f);
}

ENJIN_TEST(TerrainOrigin, TheHelperAgreesWithTheMeshTheRendererBuilds) {
    // The whole bug in one assertion. The mesh's first vertex is grid cell
    // (0,0); GridOrigin has to land on it, or the tools point somewhere the
    // renderer is not drawing.
    const ECS::TerrainComponent t = MakeTerrain(16, 2.0f);
    const ECS::MeshComponent mesh = Renderer::MeshFactory::CreateTerrain(t);
    ENJIN_ASSERT_TRUE(!mesh.vertices.empty());

    const Math::Vector3 origin = t.GridOrigin(Math::Vector3(0.0f, 0.0f, 0.0f));
    ENJIN_EXPECT_FLOAT_NEAR(mesh.vertices[0].position.x, origin.x, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(mesh.vertices[0].position.z, origin.z, 0.001f);
}

ENJIN_TEST(TerrainOrigin, TheTransformLandsOnTheMiddleCell) {
    // What a person expects: the terrain is centred where its entity is. An
    // odd grid has an exact middle vertex, so this is checkable rather than
    // approximate.
    const ECS::TerrainComponent t = MakeTerrain(17, 1.0f);
    const ECS::MeshComponent mesh = Renderer::MeshFactory::CreateTerrain(t);
    ENJIN_ASSERT_TRUE(mesh.vertices.size() == 17u * 17u);

    const usize middle = (17u / 2u) * 17u + (17u / 2u);
    ENJIN_EXPECT_FLOAT_NEAR(mesh.vertices[middle].position.x, 0.0f, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(mesh.vertices[middle].position.z, 0.0f, 0.001f);
}

ENJIN_TEST(TerrainOrigin, AHitAtTheTransformIsTheMiddleOfTheGrid) {
    // The conversion the brush does. A stroke aimed at the entity's position
    // must reach the centre cells, not cell (0,0) -- which is what the corner
    // convention gave it, and why a sculpt appeared down-left of the cursor.
    const ECS::TerrainComponent t = MakeTerrain(64, 1.0f);
    const Math::Vector3 transformPos(0.0f, 0.0f, 0.0f);
    const Math::Vector3 origin = t.GridOrigin(transformPos);

    const f32 gx = (transformPos.x - origin.x) / t.cellSize;
    const f32 gz = (transformPos.z - origin.z) / t.cellSize;

    ENJIN_EXPECT_FLOAT_NEAR(gx, 31.5f, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(gz, 31.5f, 0.001f);
    // And firmly inside the grid, which the old mapping was not: it put this
    // hit at cell 0 and clamped, so half of every brush fell off the edge.
    ENJIN_EXPECT_TRUE(gx > 1.0f && gx < 63.0f);
}

ENJIN_TEST(TerrainOrigin, ACellSizeOtherThanOneStillLinesUp) {
    // cellSize divides into the conversion twice -- once for the half-extent and
    // once for the grid index -- so a non-unit cell is where an off-by-a-factor
    // would show.
    const ECS::TerrainComponent t = MakeTerrain(32, 4.0f);
    const ECS::MeshComponent mesh = Renderer::MeshFactory::CreateTerrain(t);
    const Math::Vector3 origin = t.GridOrigin(Math::Vector3(100.0f, 0.0f, 100.0f));

    ENJIN_EXPECT_FLOAT_NEAR(origin.x, 100.0f - 62.0f, 0.001f);   // (32-1)*4*0.5
    ENJIN_ASSERT_TRUE(!mesh.vertices.empty());
    ENJIN_EXPECT_FLOAT_NEAR(mesh.vertices[0].position.x,
                            t.GridOrigin(Math::Vector3(0.0f)).x, 0.001f);
}

ENJIN_TEST_MAIN()
