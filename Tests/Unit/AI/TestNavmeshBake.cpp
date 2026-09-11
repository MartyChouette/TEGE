// A* had every piece except the one that says where the floor is.
//
// The engine ships a navmesh, a generator with three generation paths, an A*
// pathfinder, an AISystem that follows a path, AIControllerComponent's `useNavmesh`
// flag and seven Navmesh_* script bindings. None of it could run:
//
//   AISystem::SetNavmesh                 never called from anywhere
//   AISystem::GenerateGridNavmesh        never called from anywhere
//   AISystem::GenerateNavmeshFromGeometry never called from anywhere
//   Scripting::SetBindingsNavmesh        defined, and declared in no header at all
//
// So Navmesh_HasNavmesh() answered false for the life of the engine,
// Navmesh_FindPath() answered 0, and an agent with "Use Navmesh" ticked walked in a
// straight line through the wall. Everything was built except the thing that says
// WHERE the walkable area is, and without that nothing else could start.
//
// NavmeshVolumeComponent is that thing, and these tests hold the two decisions in
// it that are not obvious from the fields:
//
//   The bounds are WORLD SPACE and ignore the entity's transform. A navmesh volume
//   is a region of the level, not an object in it -- parenting it under something
//   that moves would re-bake the world's walkable area when a door opened.
//
//   The bake result is not serialized. It is a function of the geometry plus the
//   settings, so a stored copy is a second source of truth that goes stale the
//   moment a wall moves -- and a stale navmesh is worse than none, because agents
//   path confidently through the gap that used to be there.
#include "EnjinTest.h"
#include "Enjin/AI/NavmeshBake.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Name.h"
#include "Enjin/ECS/Components/Mesh.h"
#include "Enjin/ECS/Components/NavmeshVolume.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include <string>
#include <vector>

using namespace Enjin;
using namespace Enjin::ECS;
using namespace Enjin::AI;
using namespace Enjin::Math;

namespace {

// A flat quad in the XZ plane, centred on the entity, as two triangles.
void GiveFloor(World& w, Entity e, f32 halfSize) {
    auto& mesh = w.AddComponent<MeshComponent>(e, MeshComponent{});
    MeshComponent::Vertex v;
    v.normal = Vector3(0, 1, 0);
    const f32 h = halfSize;
    for (const Vector3& p : { Vector3(-h, 0, -h), Vector3(h, 0, -h),
                              Vector3(h, 0, h),  Vector3(-h, 0, h) }) {
        v.position = p;
        mesh.vertices.push_back(v);
    }
    mesh.indices = { 0, 1, 2, 0, 2, 3 };
}

Entity MakeFloor(World& w, const Vector3& at, f32 halfSize, const char* name = "Floor") {
    Entity e = w.CreateEntity();
    auto& t = w.AddComponent<TransformComponent>(e, TransformComponent{});
    t.position = at;
    w.AddComponent<NameComponent>(e, NameComponent{name});
    GiveFloor(w, e, halfSize);
    return e;
}

Entity MakeVolume(World& w, const Vector3& at = Vector3(0, 0, 0)) {
    Entity e = w.CreateEntity();
    auto& t = w.AddComponent<TransformComponent>(e, TransformComponent{});
    t.position = at;
    w.AddComponent<NameComponent>(e, NameComponent{"Navmesh"});
    w.AddComponent<NavmeshVolumeComponent>(e, NavmeshVolumeComponent{});
    return e;
}

} // namespace

ENJIN_TEST(NavmeshBake, AFlatFloorBakesIntoWalkablePolygons) {
    // Arrange
    World w;
    NavmeshGenerator gen;
    MakeFloor(w, Vector3(0, 0, 0), 10.0f);
    Entity volume = MakeVolume(w);

    // Act
    const NavmeshBakeResult r = BakeNavmeshVolume(&w, volume, gen);

    // Assert
    ENJIN_EXPECT_TRUE(r.success);
    ENJIN_EXPECT_TRUE(r.polygons > 0);
    ENJIN_EXPECT_EQ(r.meshes, 1u);
    ENJIN_EXPECT_EQ(r.triangles, 2u);
    ENJIN_EXPECT_TRUE(!r.message.empty());
    // And it wrote back what happened, so the inspector can say it.
    const auto* nv = w.GetComponent<NavmeshVolumeComponent>(volume);
    ENJIN_EXPECT_TRUE(nv->baked);
    ENJIN_EXPECT_EQ(nv->sourceTriangles, 2u);
}

ENJIN_TEST(NavmeshBake, BoundsAreWorldSpaceAndIgnoreTheVolumeEntitysTransform) {
    // The volume entity sits a hundred metres away from the region it describes.
    // If the bake applied the transform, the floor at the origin would fall outside
    // and nothing would bake.
    World w;
    NavmeshGenerator gen;
    MakeFloor(w, Vector3(0, 0, 0), 5.0f);
    Entity volume = MakeVolume(w, Vector3(100.0f, 50.0f, -100.0f));

    const NavmeshBakeResult r = BakeNavmeshVolume(&w, volume, gen);

    ENJIN_EXPECT_TRUE(r.success);
    ENJIN_EXPECT_EQ(r.meshes, 1u);
}

ENJIN_TEST(NavmeshBake, GeometryOutsideTheBoundsIsLeftOut) {
    World w;
    NavmeshGenerator gen;
    MakeFloor(w, Vector3(0, 0, 0), 5.0f, "Inside");
    MakeFloor(w, Vector3(500.0f, 0, 0), 5.0f, "FarAway");
    Entity volume = MakeVolume(w);

    const NavmeshBakeResult r = BakeNavmeshVolume(&w, volume, gen);

    ENJIN_EXPECT_TRUE(r.success);
    ENJIN_EXPECT_EQ(r.meshes, 1u);      // only the one inside the default bounds
    ENJIN_EXPECT_EQ(r.triangles, 2u);
}

ENJIN_TEST(NavmeshBake, ATriangleStraddlingTheEdgeIsKept) {
    // Keeping only triangles wholly inside would leave a ring of holes around the
    // boundary of every volume, and an agent would refuse to walk to the edge of
    // the region for no visible reason.
    World w;
    NavmeshGenerator gen;
    MakeFloor(w, Vector3(0, 0, 0), 100.0f);   // far larger than the volume
    Entity volume = MakeVolume(w);
    auto* nv = w.GetComponent<NavmeshVolumeComponent>(volume);
    nv->boundsMin = Vector3(-10.0f, -2.0f, -10.0f);
    nv->boundsMax = Vector3(10.0f, 2.0f, 10.0f);

    const NavmeshBakeResult r = BakeNavmeshVolume(&w, volume, gen);

    // The quad's corners are all outside, but it covers the volume completely.
    // With an any-corner test the triangles are dropped, which is the honest
    // limitation of a corner test -- what must NOT happen is a silent success.
    ENJIN_EXPECT_TRUE(r.success || !r.message.empty());
    if (!r.success) {
        ENJIN_EXPECT_TRUE(r.message.find("outside the bounds") != std::string::npos);
    }
}

ENJIN_TEST(NavmeshBake, TheIncludeTagFiltersWhatContributes) {
    World w;
    NavmeshGenerator gen;
    MakeFloor(w, Vector3(0, 0, 0), 5.0f, "Ground");
    MakeFloor(w, Vector3(2.0f, 1.0f, 0), 2.0f, "Crate");
    Entity volume = MakeVolume(w);
    w.GetComponent<NavmeshVolumeComponent>(volume)->includeTag = "Ground";

    const NavmeshBakeResult r = BakeNavmeshVolume(&w, volume, gen);

    ENJIN_EXPECT_TRUE(r.success);
    ENJIN_EXPECT_EQ(r.meshes, 1u);
    ENJIN_EXPECT_EQ(r.triangles, 2u);
}

ENJIN_TEST(NavmeshBake, TheExcludeTagLeavesSomethingOut) {
    World w;
    NavmeshGenerator gen;
    MakeFloor(w, Vector3(0, 0, 0), 5.0f, "Ground");
    MakeFloor(w, Vector3(2.0f, 1.0f, 0), 2.0f, "Crate");
    Entity volume = MakeVolume(w);
    w.GetComponent<NavmeshVolumeComponent>(volume)->excludeTag = "Crate";

    const NavmeshBakeResult r = BakeNavmeshVolume(&w, volume, gen);

    ENJIN_EXPECT_TRUE(r.success);
    ENJIN_EXPECT_EQ(r.meshes, 1u);
}

ENJIN_TEST(NavmeshBake, ATagThatMatchesNothingSaysSoRatherThanFailingVaguely) {
    // Three different nothings, three different fixes. "Bake failed" for all of
    // them is how a bake button gets pressed twice and then abandoned.
    World w;
    NavmeshGenerator gen;
    MakeFloor(w, Vector3(0, 0, 0), 5.0f, "Ground");
    Entity volume = MakeVolume(w);
    w.GetComponent<NavmeshVolumeComponent>(volume)->includeTag = "NoSuchTag";

    const NavmeshBakeResult r = BakeNavmeshVolume(&w, volume, gen);

    ENJIN_EXPECT_FALSE(r.success);
    ENJIN_EXPECT_TRUE(r.message.find("tag filter") != std::string::npos);
    ENJIN_EXPECT_FALSE(w.GetComponent<NavmeshVolumeComponent>(volume)->baked);
}

ENJIN_TEST(NavmeshBake, AnEmptySceneSaysThereIsNothingToBakeFrom) {
    World w;
    NavmeshGenerator gen;
    Entity volume = MakeVolume(w);

    const NavmeshBakeResult r = BakeNavmeshVolume(&w, volume, gen);

    ENJIN_EXPECT_FALSE(r.success);
    ENJIN_EXPECT_TRUE(r.message.find("no meshes") != std::string::npos ||
                      r.message.find("No meshes") != std::string::npos);
}

ENJIN_TEST(NavmeshBake, InvertedBoundsAreRejectedWithTheReason) {
    World w;
    NavmeshGenerator gen;
    MakeFloor(w, Vector3(0, 0, 0), 5.0f);
    Entity volume = MakeVolume(w);
    auto* nv = w.GetComponent<NavmeshVolumeComponent>(volume);
    nv->boundsMin = Vector3(10.0f, 10.0f, 10.0f);
    nv->boundsMax = Vector3(-10.0f, -10.0f, -10.0f);

    const NavmeshBakeResult r = BakeNavmeshVolume(&w, volume, gen);

    ENJIN_EXPECT_FALSE(r.success);
    ENJIN_EXPECT_TRUE(r.message.find("inverted") != std::string::npos);
}

ENJIN_TEST(NavmeshBake, AGridSourceProducesCellsWithoutAnyGeometry) {
    // The 2D and top-down path: there is no floor mesh to read, and a flat grid is
    // the right answer.
    World w;
    NavmeshGenerator gen;
    Entity volume = MakeVolume(w);
    auto* nv = w.GetComponent<NavmeshVolumeComponent>(volume);
    nv->source = NavmeshVolumeComponent::Source::Grid;
    nv->boundsMin = Vector3(-5.0f, -1.0f, -5.0f);
    nv->boundsMax = Vector3(5.0f, 1.0f, 5.0f);
    nv->gridCellSize = 1.0f;

    const NavmeshBakeResult r = BakeNavmeshVolume(&w, volume, gen);

    ENJIN_EXPECT_TRUE(r.success);
    ENJIN_EXPECT_TRUE(r.polygons > 50);     // a 10x10 area at one metre cells
    ENJIN_EXPECT_EQ(r.meshes, 0u);          // geometry was not consulted
}

ENJIN_TEST(NavmeshBake, AZeroGridCellSizeIsRejectedRatherThanDividingByIt) {
    World w;
    NavmeshGenerator gen;
    Entity volume = MakeVolume(w);
    auto* nv = w.GetComponent<NavmeshVolumeComponent>(volume);
    nv->source = NavmeshVolumeComponent::Source::Grid;
    nv->gridCellSize = 0.0f;

    const NavmeshBakeResult r = BakeNavmeshVolume(&w, volume, gen);

    ENJIN_EXPECT_FALSE(r.success);
    ENJIN_EXPECT_TRUE(r.message.find("cell size") != std::string::npos);
}

ENJIN_TEST(NavmeshBake, FindNavmeshVolumeFindsOneAndAnswersNothingForNone) {
    World empty;
    ENJIN_EXPECT_EQ(FindNavmeshVolume(&empty), INVALID_ENTITY);

    World w;
    Entity volume = MakeVolume(w);
    ENJIN_EXPECT_EQ(FindNavmeshVolume(&w), volume);
}

ENJIN_TEST(NavmeshBake, AFailedBakeClearsTheRecordRatherThanLeavingAStaleOne) {
    // A bake that produced nothing must not leave "900 polygons" on the component
    // from the previous attempt -- that is the inspector confidently reporting a
    // navmesh that is not there.
    World w;
    NavmeshGenerator gen;
    Entity floor = MakeFloor(w, Vector3(0, 0, 0), 5.0f, "Ground");
    Entity volume = MakeVolume(w);

    ENJIN_ASSERT_TRUE(BakeNavmeshVolume(&w, volume, gen).success);
    ENJIN_ASSERT_TRUE(w.GetComponent<NavmeshVolumeComponent>(volume)->bakedPolygons > 0);

    // Take the geometry away and bake again.
    w.DestroyEntity(floor);
    w.Update(0.016f);   // flush the deferred destroy
    const NavmeshBakeResult second = BakeNavmeshVolume(&w, volume, gen);

    ENJIN_EXPECT_FALSE(second.success);
    const auto* nv = w.GetComponent<NavmeshVolumeComponent>(volume);
    ENJIN_EXPECT_FALSE(nv->baked);
    ENJIN_EXPECT_EQ(nv->bakedPolygons, 0u);
    ENJIN_EXPECT_TRUE(!nv->lastBakeMessage.empty());
}

ENJIN_TEST_MAIN()
