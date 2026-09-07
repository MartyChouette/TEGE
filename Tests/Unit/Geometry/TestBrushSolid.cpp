// BrushSolidComponent: the brush list, and what rebuilding it produces.
//
// The component stores brushes, never the mesh. So the properties worth
// asserting are that the derived geometry actually appears, that it is derived
// AGAIN when the list changes, and that a doorway edited away closes the wall
// back up -- which is the whole claim of non-destructive editing.
#include "EnjinTest.h"
#include "Enjin/ECS/Components/BrushSolid.h"
#include "Enjin/ECS/Systems/BrushSolidSystem.h"
#include "Enjin/ECS/Components/Mesh.h"
#include "Enjin/ECS/Components/ProceduralMesh.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/ECS/World.h"

using namespace Enjin;
using namespace Enjin::Math;

namespace {

ECS::BrushSolidComponent::Brush Box(const Vector3& c, const Vector3& he,
                                    Geometry::BrushOp op = Geometry::BrushOp::Add) {
    ECS::BrushSolidComponent::Brush b;
    b.shape = ECS::BrushSolidComponent::Shape::Box;
    b.center = c;
    b.halfExtents = he;
    b.op = op;
    return b;
}

// A wall with a doorway cut through it, as an entity.
ECS::Entity MakeWall(ECS::World& w) {
    ECS::Entity e = w.CreateEntity();
    ECS::BrushSolidComponent solid;
    solid.brushes.push_back(Box(Vector3(0.0f), Vector3(4.0f, 3.0f, 0.25f)));
    solid.brushes.push_back(Box(Vector3(0.0f, -1.0f, 0.0f), Vector3(1.0f, 2.0f, 1.0f),
                                Geometry::BrushOp::Subtract));
    w.AddComponent<ECS::BrushSolidComponent>(e, solid);
    return e;
}

} // namespace

ENJIN_TEST(BrushSolid, RebuildProducesGeometryFromTheBrushList) {
    ECS::World w;
    ECS::Entity e = MakeWall(w);

    // Nothing exists until it is built: the component holds brushes, not a mesh.
    ENJIN_EXPECT_FALSE(w.HasComponent<ECS::MeshComponent>(e));

    ENJIN_ASSERT_TRUE(ECS::BrushSolidSystem::Rebuild(&w, e));

    ENJIN_ASSERT_TRUE(w.HasComponent<ECS::MeshComponent>(e));
    auto* mesh = w.GetComponent<ECS::MeshComponent>(e);
    ENJIN_EXPECT_TRUE(mesh->vertices.size() > 0);
    ENJIN_EXPECT_TRUE(mesh->indices.size() > 0);
    ENJIN_EXPECT_EQ(mesh->indices.size() % 3, (usize)0);
}

ENJIN_TEST(BrushSolid, RebuildAddsCollisionThatMatches) {
    ECS::World w;
    ECS::Entity e = MakeWall(w);
    ECS::BrushSolidSystem::Rebuild(&w, e);

    ENJIN_ASSERT_TRUE(w.HasComponent<ECS::MeshColliderComponent>(e));
    auto* col = w.GetComponent<ECS::MeshColliderComponent>(e);
    ENJIN_EXPECT_TRUE(col->indices.size() > 0);
    ENJIN_EXPECT_FALSE(col->convex);        // a hull would fill the doorway in
    ENJIN_EXPECT_TRUE(col->generated);

    // Welded, so fewer vertices than the render mesh, which duplicates per face.
    auto* mesh = w.GetComponent<ECS::MeshComponent>(e);
    ENJIN_EXPECT_TRUE(col->vertices.size() < mesh->vertices.size());
}

ENJIN_TEST(BrushSolid, CollisionCanBeTurnedOff) {
    ECS::World w;
    ECS::Entity e = MakeWall(w);
    w.GetComponent<ECS::BrushSolidComponent>(e)->generateCollider = false;

    ECS::BrushSolidSystem::Rebuild(&w, e);
    ENJIN_EXPECT_FALSE(w.HasComponent<ECS::MeshColliderComponent>(e));
}

// The GPU side is somebody else's job. This system only flags it, the way every
// other generated-geometry system does.
ENJIN_TEST(BrushSolid, RebuildFlagsTheProceduralMeshForUpload) {
    ECS::World w;
    ECS::Entity e = MakeWall(w);
    ECS::BrushSolidSystem::Rebuild(&w, e);

    ENJIN_ASSERT_TRUE(w.HasComponent<ECS::ProceduralMeshComponent>(e));
    auto* pm = w.GetComponent<ECS::ProceduralMeshComponent>(e);
    ENJIN_EXPECT_EQ((int)pm->source, (int)ECS::ProceduralMeshComponent::Source::Csg);
    ENJIN_EXPECT_TRUE(pm->topologyDirty);
}

ENJIN_TEST(BrushSolid, UpdateOnlyRebuildsWhatIsDirty) {
    ECS::World w;
    ECS::Entity e = MakeWall(w);

    ENJIN_EXPECT_EQ(ECS::BrushSolidSystem::Update(&w), (u32)1);   // dirty by default
    ENJIN_EXPECT_FALSE(w.GetComponent<ECS::BrushSolidComponent>(e)->dirty);
    ENJIN_EXPECT_EQ(ECS::BrushSolidSystem::Update(&w), (u32)0);   // nothing to do

    w.GetComponent<ECS::BrushSolidComponent>(e)->dirty = true;
    ENJIN_EXPECT_EQ(ECS::BrushSolidSystem::Update(&w), (u32)1);
}

// The claim of non-destructive editing: removing the cut restores the wall,
// because the wall was never cut, only described as cut.
ENJIN_TEST(BrushSolid, RemovingTheCutClosesTheWallBackUp) {
    ECS::World w;
    ECS::Entity e = MakeWall(w);
    ECS::BrushSolidSystem::Rebuild(&w, e);
    const usize withHole = w.GetComponent<ECS::MeshComponent>(e)->indices.size();

    auto* solid = w.GetComponent<ECS::BrushSolidComponent>(e);
    solid->brushes.pop_back();      // drop the doorway
    solid->dirty = true;
    ECS::BrushSolidSystem::Update(&w);

    const usize plain = w.GetComponent<ECS::MeshComponent>(e)->indices.size();
    ENJIN_EXPECT_TRUE(plain < withHole);            // the cut geometry is gone
    ENJIN_EXPECT_EQ(plain, (usize)36);              // a plain box: 6 quads, 12 tris
}

// A brush switched off stays in the list and stops contributing, which is how
// you check what a cut is doing without losing it.
ENJIN_TEST(BrushSolid, DisabledBrushesDoNotContribute) {
    ECS::World w;
    ECS::Entity e = MakeWall(w);
    ECS::BrushSolidSystem::Rebuild(&w, e);
    const usize withHole = w.GetComponent<ECS::MeshComponent>(e)->indices.size();

    auto* solid = w.GetComponent<ECS::BrushSolidComponent>(e);
    solid->brushes[1].enabled = false;
    solid->dirty = true;
    ECS::BrushSolidSystem::Update(&w);

    ENJIN_EXPECT_EQ(solid->brushes.size(), (usize)2);   // still there
    ENJIN_EXPECT_TRUE(w.GetComponent<ECS::MeshComponent>(e)->indices.size() < withHole);
}

// An empty list is a legal state -- a fresh component has no brushes yet -- and
// must not blank a mesh that is already there, or an in-progress edit flashes
// the model away and back.
ENJIN_TEST(BrushSolid, EmptyListLeavesAnyExistingMeshAlone) {
    ECS::World w;
    ECS::Entity e = MakeWall(w);
    ECS::BrushSolidSystem::Rebuild(&w, e);
    const usize before = w.GetComponent<ECS::MeshComponent>(e)->indices.size();

    auto* solid = w.GetComponent<ECS::BrushSolidComponent>(e);
    solid->brushes.clear();
    solid->dirty = true;

    ENJIN_EXPECT_FALSE(ECS::BrushSolidSystem::Rebuild(&w, e));   // nothing built
    ENJIN_EXPECT_EQ(w.GetComponent<ECS::MeshComponent>(e)->indices.size(), before);
    ENJIN_EXPECT_FALSE(solid->dirty);   // still consumed, so it does not retry forever
}

ENJIN_TEST(BrushSolid, PrismBrushesBuildToo) {
    ECS::World w;
    ECS::Entity e = w.CreateEntity();
    ECS::BrushSolidComponent solid;
    ECS::BrushSolidComponent::Brush p;
    p.shape = ECS::BrushSolidComponent::Shape::Prism;
    p.radius = 2.0f;
    p.halfHeight = 1.0f;
    p.sides = 6;
    solid.brushes.push_back(p);
    w.AddComponent<ECS::BrushSolidComponent>(e, solid);

    ENJIN_ASSERT_TRUE(ECS::BrushSolidSystem::Rebuild(&w, e));
    // 6 sides + 2 caps, and the face count is recorded for the inspector.
    ENJIN_EXPECT_EQ(w.GetComponent<ECS::BrushSolidComponent>(e)->lastFaceCount, (u32)8);
}

ENJIN_TEST(BrushSolid, RebuildOnAnEntityWithoutOneIsHarmless) {
    ECS::World w;
    ECS::Entity e = w.CreateEntity();
    ENJIN_EXPECT_FALSE(ECS::BrushSolidSystem::Rebuild(&w, e));
    ENJIN_EXPECT_FALSE(ECS::BrushSolidSystem::Rebuild(nullptr, e));
}

// The flag is a fast path, not the correctness mechanism. Undo writes an old
// value straight back through a raw pointer without touching dirty, and so
// would a script or any future tool. If rebuild depended on the flag alone,
// every one of those would leave stale geometry on screen.
ENJIN_TEST(BrushSolid, EditWithoutSettingDirtyStillRebuilds) {
    ECS::World w;
    ECS::Entity e = MakeWall(w);
    ECS::BrushSolidSystem::Update(&w);
    const usize before = w.GetComponent<ECS::MeshComponent>(e)->indices.size();

    auto* solid = w.GetComponent<ECS::BrushSolidComponent>(e);
    ENJIN_ASSERT_FALSE(solid->dirty);

    // Edit the list and deliberately do NOT set dirty, the way undo does.
    solid->brushes.pop_back();

    ENJIN_EXPECT_EQ(ECS::BrushSolidSystem::Update(&w), (u32)1);
    ENJIN_EXPECT_TRUE(w.GetComponent<ECS::MeshComponent>(e)->indices.size() != before);
}

ENJIN_TEST(BrushSolid, UnchangedSolidsAreNotRebuilt) {
    ECS::World w;
    ECS::Entity e = MakeWall(w);
    ECS::BrushSolidSystem::Update(&w);

    // Hash matches and the flag is clear, so there is nothing to do. Without
    // this the hash check would rebuild every solid every frame.
    ENJIN_EXPECT_EQ(ECS::BrushSolidSystem::Update(&w), (u32)0);
    ENJIN_EXPECT_EQ(ECS::BrushSolidSystem::Update(&w), (u32)0);
    (void)e;
}

// Output fields must not feed the hash, or recording a rebuild's own results
// changes the hash and asks for another rebuild, forever.
ENJIN_TEST(BrushSolid, OutputCountsDoNotFeedTheHash) {
    ECS::World w;
    ECS::Entity e = MakeWall(w);
    auto* solid = w.GetComponent<ECS::BrushSolidComponent>(e);

    const u64 h = solid->ContentHash();
    solid->lastFaceCount = 999;
    solid->lastTriangleCount = 12345;
    ENJIN_EXPECT_EQ(solid->ContentHash(), h);
}

ENJIN_TEST_MAIN()
