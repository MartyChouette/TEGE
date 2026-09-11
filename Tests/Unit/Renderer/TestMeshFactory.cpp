// Validates every MeshFactory primitive the editor can create. Triggered by a
// report that adding a Pyramid crashed a project. Checks each mesh is non-empty,
// triangulated, has all indices in range, and has finite positions/normals --
// the conditions that would make the GPU upload or render path fault.

#include "EnjinTest.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/Renderer/MeshFactory.h"
#include "Enjin/ECS/Components/Mesh.h"

#include <cmath>

using namespace Enjin;
using R = Renderer::MeshFactory;

namespace {
void Validate(const ECS::MeshComponent& m) {
    ENJIN_ASSERT_TRUE(!m.vertices.empty());
    ENJIN_ASSERT_TRUE(!m.indices.empty());
    ENJIN_EXPECT_EQ(m.indices.size() % 3, (size_t)0);  // triangulated

    // Every index must reference a real vertex (out-of-range = GPU crash).
    bool inRange = true;
    for (u32 idx : m.indices) {
        if (idx >= m.vertices.size()) { inRange = false; break; }
    }
    ENJIN_EXPECT_TRUE(inRange);

    // No NaN/Inf in positions or normals.
    bool finite = true;
    for (const auto& v : m.vertices) {
        if (!std::isfinite(v.position.x) || !std::isfinite(v.position.y) || !std::isfinite(v.position.z) ||
            !std::isfinite(v.normal.x)   || !std::isfinite(v.normal.y)   || !std::isfinite(v.normal.z)) {
            finite = false; break;
        }
    }
    ENJIN_EXPECT_TRUE(finite);
}
} // namespace

// --- 3D primitives (Entity > 3D Object menu) ---
ENJIN_TEST(Primitives, Cube)     { Validate(R::CreateCube(1.0f)); }
ENJIN_TEST(Primitives, Sphere)   { Validate(R::CreateSphere(0.5f)); }
ENJIN_TEST(Primitives, Plane)    { Validate(R::CreatePlane(10.0f, 10.0f)); }
ENJIN_TEST(Primitives, Cylinder) { Validate(R::CreateCylinder(0.5f, 1.0f)); }
ENJIN_TEST(Primitives, Cone)     { Validate(R::CreateCone(0.5f, 1.0f)); }
ENJIN_TEST(Primitives, Capsule)  { Validate(R::CreateCapsule(0.3f, 1.0f)); }
ENJIN_TEST(Primitives, Pyramid)  { Validate(R::CreatePyramid(1.0f, 1.0f)); }

// --- 2D primitives (Entity > 2D Object menu) ---
ENJIN_TEST(Primitives, Triangle)  { Validate(R::CreateTriangle(1.0f)); }
ENJIN_TEST(Primitives, Quad)      { Validate(R::CreateQuad(1.0f, 1.0f)); }
ENJIN_TEST(Primitives, Capsule2D) { Validate(R::CreateCapsule2D(1.0f, 2.0f)); }

// ---------------------------------------------------------------------------
// Tilemaps
// ---------------------------------------------------------------------------
//
// Tilemaps were listed as "missing on web" in the parity audit. The geometry was
// never the problem: CreateTilemapMesh is pure vertex data and was compiled into
// the web build the whole time with nothing calling it. There are TWO
// RenderSystem::Update bodies, one per backend, and the tilemap mesh generation
// only ever existed in the Vulkan one -- so a tilemap on web produced no
// MeshComponent at all, the scene loaded, reported its entities, and showed an
// empty view. It lives in RenderSystem::EnsureTilemapMeshes now, which both
// bodies call.
//
// These cover the geometry that fix depends on.

namespace {

Enjin::ECS::TilemapComponent MakeTilemap(Enjin::u32 w, Enjin::u32 h,
                                         Enjin::u32 columns, int fill) {
    Enjin::ECS::TilemapComponent tm;
    tm.width = w;
    tm.height = h;
    tm.tilesetColumns = columns;
    tm.worldTileWidth = 1.0f;
    tm.worldTileHeight = 1.0f;
    tm.tiles.assign(static_cast<Enjin::usize>(w) * h, fill);
    return tm;
}

} // namespace

ENJIN_TEST(Tilemap, AFilledGridProducesAQuadPerTile) {
    const auto tm = MakeTilemap(4, 3, 4, 0);
    const auto mesh = R::CreateTilemapMesh(tm);

    // Four verts and six indices per tile, twelve tiles.
    ENJIN_EXPECT_EQ(mesh.vertices.size(), static_cast<Enjin::usize>(12 * 4));
    ENJIN_EXPECT_EQ(mesh.indices.size(), static_cast<Enjin::usize>(12 * 6));
}

ENJIN_TEST(Tilemap, EmptyTilesProduceNoGeometry) {
    // -1 is the empty tile. A grid of them is a legitimately blank tilemap and
    // must not cost a single vertex.
    const auto tm = MakeTilemap(8, 8, 4, -1);
    const auto mesh = R::CreateTilemapMesh(tm);

    ENJIN_EXPECT_TRUE(mesh.vertices.empty());
    ENJIN_EXPECT_TRUE(mesh.indices.empty());
}

ENJIN_TEST(Tilemap, ADegenerateTilemapProducesNothingRatherThanGarbage) {
    // Zero columns would divide by zero working out the UV tile size.
    Enjin::ECS::TilemapComponent tm = MakeTilemap(4, 4, 0, 0);
    ENJIN_EXPECT_TRUE(R::CreateTilemapMesh(tm).vertices.empty());

    tm = MakeTilemap(0, 4, 4, 0);
    ENJIN_EXPECT_TRUE(R::CreateTilemapMesh(tm).vertices.empty());
}

ENJIN_TEST(Tilemap, DifferentTileIndicesAddressDifferentPartsOfTheTileset) {
    // The whole point of a tileset: tile 0 and tile 5 must sample different
    // regions. UVs that came out identical would render one tile everywhere and
    // look like a broken atlas rather than a broken mesh.
    Enjin::ECS::TilemapComponent tm = MakeTilemap(2, 1, 4, 0);
    tm.tiles[0] = 0;
    tm.tiles[1] = 5;

    const auto mesh = R::CreateTilemapMesh(tm);
    ENJIN_ASSERT_EQ(mesh.vertices.size(), static_cast<Enjin::usize>(8));

    const auto& first = mesh.vertices[0];
    const auto& second = mesh.vertices[4];
    const bool differs = (first.uv.x != second.uv.x) ||
                         (first.uv.y != second.uv.y);
    ENJIN_EXPECT_TRUE(differs);

    // And every UV stays inside the tileset.
    for (const auto& v : mesh.vertices) {
        ENJIN_ASSERT_TRUE(v.uv.x >= -0.0001f && v.uv.x <= 1.0001f);
        ENJIN_ASSERT_TRUE(v.uv.y >= -0.0001f && v.uv.y <= 1.0001f);
    }
}

ENJIN_TEST(Tilemap, TilesAreLaidOutByTheirWorldTileSize) {
    Enjin::ECS::TilemapComponent tm = MakeTilemap(2, 1, 4, 0);
    tm.worldTileWidth = 3.0f;
    tm.worldTileHeight = 3.0f;

    const auto mesh = R::CreateTilemapMesh(tm);
    ENJIN_ASSERT_EQ(mesh.vertices.size(), static_cast<Enjin::usize>(8));

    Enjin::f32 minX = 1e9f, maxX = -1e9f;
    for (const auto& v : mesh.vertices) {
        if (v.position.x < minX) minX = v.position.x;
        if (v.position.x > maxX) maxX = v.position.x;
    }
    // Two tiles at three units each.
    ENJIN_EXPECT_FLOAT_NEAR(maxX - minX, 6.0f, 0.001f);
}

ENJIN_TEST_MAIN()
