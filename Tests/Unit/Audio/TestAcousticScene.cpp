// The room, as sound sees it.
//
// This gathering used to live inside `#ifdef ENJIN_AUDIO_STEAM_AUDIO`, which
// meant it could not be tested at all: there is no way to assert that a
// carpeted floor came through as carpet when the function only exists on a
// machine with a proprietary SDK installed. It is plain engine code now, and
// this file is what that bought.
//
// Two claims matter most. That different surfaces arrive as different
// materials -- the whole point. And that MESH colliders are collected, because
// brush solids and voxel caves produce nothing else, so a whole carved cave
// system used to be acoustically invisible: a sound inside it reflected off
// nothing at all.

#include "EnjinTest.h"
#include "Enjin/Audio/AcousticScene.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Material.h"
#include "Enjin/ECS/Components/Gameplay.h"

#include <cmath>
#include <set>

using namespace Enjin;
using namespace Enjin::Audio;
using namespace Enjin::ECS;

namespace {

Entity MakeBox(World& w, const Math::Vector3& pos, const Math::Vector3& size,
               SurfaceMaterial surface) {
    Entity e = w.CreateEntity();
    auto& xf = w.AddComponent<TransformComponent>(e);
    xf.position = pos;
    auto& box = w.AddComponent<BoxColliderComponent>(e);
    box.size = size;
    auto& mat = w.AddComponent<MaterialComponent>(e);
    mat.surfaceMaterial = surface;
    return e;
}

// The set of distinct material indices actually used by triangles.
std::set<i32> UsedMaterials(const AcousticScene& s) {
    return std::set<i32>(s.materialIndices.begin(), s.materialIndices.end());
}

} // namespace

ENJIN_TEST(AcousticScene, AnEmptyWorldMakesAnEmptyScene) {
    // Arrange
    World w;

    // Act
    const AcousticScene scene = BuildAcousticScene(&w);

    // Assert
    ENJIN_EXPECT_TRUE(scene.Empty());
    ENJIN_EXPECT_EQ(scene.materials.Count(), (usize)0);
}

ENJIN_TEST(AcousticScene, ANullWorldIsRefusedRatherThanCrashed) {
    // Arrange / Act / Assert
    ENJIN_EXPECT_TRUE(BuildAcousticScene(nullptr).Empty());
}

ENJIN_TEST(AcousticScene, EveryTriangleGetsExactlyOneMaterialIndex) {
    // Arrange
    World w;
    MakeBox(w, Math::Vector3(0, 0, 0), Math::Vector3(4, 3, 4), SurfaceMaterial::Concrete);
    MakeBox(w, Math::Vector3(20, 0, 0), Math::Vector3(4, 3, 4), SurfaceMaterial::Carpet);

    // Act
    const AcousticScene scene = BuildAcousticScene(&w);

    // Assert: the simulator indexes materials per TRIANGLE. One short and it
    // reads off the end of the array; one long and the last surfaces are
    // silently the wrong material.
    ENJIN_EXPECT_EQ(scene.materialIndices.size(), scene.TriangleCount());
    ENJIN_EXPECT_EQ(scene.TriangleCount(), (usize)24);   // 12 per box
}

// The complaint, as a test.
ENJIN_TEST(AcousticScene, AConcreteWallAndACarpetedFloorArriveAsDifferentMaterials) {
    // Arrange
    World w;
    MakeBox(w, Math::Vector3(0, 3, 0), Math::Vector3(8, 6, 1), SurfaceMaterial::Concrete);
    MakeBox(w, Math::Vector3(0, 0, 0), Math::Vector3(8, 1, 8), SurfaceMaterial::Carpet);

    // Act
    const AcousticScene scene = BuildAcousticScene(&w);

    // Assert: two entries, and they really are the two materials asked for.
    // Before this, every triangle in the world shared one default and both
    // floors sounded the same.
    ENJIN_ASSERT_EQ(scene.materials.Count(), (usize)2);
    ENJIN_EXPECT_EQ(UsedMaterials(scene).size(), (usize)2);

    bool sawConcrete = false, sawCarpet = false;
    for (usize i = 0; i < scene.materials.Count(); ++i) {
        if (AcousticsEqual(scene.materials.At(i), AcousticsFor(SurfaceMaterial::Concrete)))
            sawConcrete = true;
        if (AcousticsEqual(scene.materials.At(i), AcousticsFor(SurfaceMaterial::Carpet)))
            sawCarpet = true;
    }
    ENJIN_EXPECT_TRUE(sawConcrete);
    ENJIN_EXPECT_TRUE(sawCarpet);
}

ENJIN_TEST(AcousticScene, ARoomOfOneMaterialCarriesOneMaterialEntry) {
    // Arrange: twenty concrete walls.
    World w;
    for (u32 i = 0; i < 20; ++i) {
        MakeBox(w, Math::Vector3(static_cast<f32>(i) * 10.0f, 0, 0),
                Math::Vector3(2, 4, 2), SurfaceMaterial::Concrete);
    }

    // Act
    const AcousticScene scene = BuildAcousticScene(&w);

    // Assert: twenty times the triangles, one material.
    ENJIN_EXPECT_EQ(scene.TriangleCount(), (usize)(20 * 12));
    ENJIN_EXPECT_EQ(scene.materials.Count(), (usize)1);
}

ENJIN_TEST(AcousticScene, CollidersAreEmittedInWorldSpace) {
    // Arrange: a box twenty metres away from the origin.
    World w;
    MakeBox(w, Math::Vector3(20, 5, -8), Math::Vector3(2, 2, 2), SurfaceMaterial::Concrete);

    // Act
    const AcousticScene scene = BuildAcousticScene(&w);

    // Assert: its vertices are around where the entity is, not around the
    // origin. Geometry left in local space would put every room in the level on
    // top of every other one.
    ENJIN_ASSERT_TRUE(!scene.vertices.empty());
    Math::Vector3 centre(0, 0, 0);
    for (const auto& v : scene.vertices) {
        centre.x += v.x; centre.y += v.y; centre.z += v.z;
    }
    const f32 n = static_cast<f32>(scene.vertices.size());
    ENJIN_EXPECT_FLOAT_NEAR(centre.x / n, 20.0f, 0.01f);
    ENJIN_EXPECT_FLOAT_NEAR(centre.y / n, 5.0f, 0.01f);
    ENJIN_EXPECT_FLOAT_NEAR(centre.z / n, -8.0f, 0.01f);
}

ENJIN_TEST(AcousticScene, ColliderSizeIsWorldSpaceAndDoesNotScaleWithTheEntity) {
    // Arrange: the same collider on an entity scaled up five times.
    World w;
    Entity e = MakeBox(w, Math::Vector3(0, 0, 0), Math::Vector3(2, 2, 2),
                       SurfaceMaterial::Concrete);
    w.GetComponent<TransformComponent>(e)->scale = Math::Vector3(5, 5, 5);

    // Act
    const AcousticScene scene = BuildAcousticScene(&w);

    // Assert: one metre from the centre on each axis, because collider sizes are
    // WORLD space in this engine and the physics backends do not multiply by
    // transform scale either. A wall that was one size to Jolt and another to
    // the audio scene would put the sound of it somewhere the wall is not.
    f32 maxAbs = 0.0f;
    for (const auto& v : scene.vertices) {
        maxAbs = std::max(maxAbs, std::fabs(v.x));
    }
    ENJIN_EXPECT_FLOAT_NEAR(maxAbs, 1.0f, 0.01f);
}

ENJIN_TEST(AcousticScene, CollisionAudioMaterialBeatsTheRenderMaterial) {
    // Arrange
    World w;
    Entity e = MakeBox(w, Math::Vector3(0, 0, 0), Math::Vector3(2, 2, 2),
                       SurfaceMaterial::Carpet);
    auto& collision = w.AddComponent<AudioCollisionComponent>(e);
    collision.material = SurfaceMaterial::Metal;

    // Act
    const AcousticScene scene = BuildAcousticScene(&w);

    // Assert: the more specific statement wins, and the precedence lives in one
    // function so it cannot be decided differently here than it is elsewhere.
    ENJIN_ASSERT_EQ(scene.materials.Count(), (usize)1);
    ENJIN_EXPECT_TRUE(AcousticsEqual(scene.materials.At(0), AcousticsFor(SurfaceMaterial::Metal)));
}

// The gap that made a whole carved cave silent.
ENJIN_TEST(AcousticScene, MeshCollidersAreCollected) {
    // Arrange: a mesh collider, which is all a brush solid or a voxel cave
    // produces. The previous gatherer collected boxes and spheres only, so a
    // sound inside a cave reflected off nothing.
    World w;
    Entity e = w.CreateEntity();
    w.AddComponent<TransformComponent>(e);
    auto& mat = w.AddComponent<MaterialComponent>(e);
    mat.surfaceMaterial = SurfaceMaterial::Stone;
    auto& col = w.AddComponent<MeshColliderComponent>(e);
    col.convex = false;
    col.vertices = { {0,0,0}, {4,0,0}, {4,0,4}, {0,0,4}, {0,3,0} };
    col.indices  = { 0,1,2,  0,2,3,  0,1,4 };

    // Act
    const AcousticScene scene = BuildAcousticScene(&w);

    // Assert
    ENJIN_EXPECT_EQ(scene.TriangleCount(), (usize)3);
    ENJIN_ASSERT_EQ(scene.materials.Count(), (usize)1);
    ENJIN_EXPECT_TRUE(AcousticsEqual(scene.materials.At(0), AcousticsFor(SurfaceMaterial::Stone)));
}

ENJIN_TEST(AcousticScene, AnEnormousMeshContributesItsShapeRatherThanNothing) {
    // Arrange: more triangles than the budget allows.
    World w;
    Entity e = w.CreateEntity();
    w.AddComponent<TransformComponent>(e);
    w.AddComponent<MaterialComponent>(e).surfaceMaterial = SurfaceMaterial::Stone;
    auto& col = w.AddComponent<MeshColliderComponent>(e);
    for (u32 i = 0; i < 300; ++i) {
        const f32 f = static_cast<f32>(i);
        col.vertices.push_back({f * 0.1f, 0.0f, 0.0f});
        col.vertices.push_back({f * 0.1f, 6.0f, 0.0f});
        col.vertices.push_back({f * 0.1f, 0.0f, 9.0f});
        col.indices.push_back(i * 3 + 0);
        col.indices.push_back(i * 3 + 1);
        col.indices.push_back(i * 3 + 2);
    }

    AcousticSceneOptions tiny;
    tiny.meshTriangleBudget = 10;

    // Act
    const AcousticScene scene = BuildAcousticScene(&w, tiny);

    // Assert: a box, not silence. Sound does not care about a centimetre of
    // surface detail, and an approximate room beats an absent one.
    ENJIN_EXPECT_EQ(scene.TriangleCount(), (usize)12);

    // And it is the RIGHT box -- the bounds of the mesh it stood in for.
    f32 hiX = -1e9f, hiY = -1e9f, hiZ = -1e9f;
    for (const auto& v : scene.vertices) {
        hiX = std::max(hiX, v.x); hiY = std::max(hiY, v.y); hiZ = std::max(hiZ, v.z);
    }
    ENJIN_EXPECT_FLOAT_NEAR(hiX, 29.9f, 0.1f);
    ENJIN_EXPECT_FLOAT_NEAR(hiY, 6.0f, 0.1f);
    ENJIN_EXPECT_FLOAT_NEAR(hiZ, 9.0f, 0.1f);
}

ENJIN_TEST(AcousticScene, ACorruptMeshColliderIsSkippedRatherThanHandedToTheSimulator) {
    // Arrange: an index past the end of the vertex list.
    World w;
    Entity e = w.CreateEntity();
    w.AddComponent<TransformComponent>(e);
    w.AddComponent<MaterialComponent>(e);
    auto& col = w.AddComponent<MeshColliderComponent>(e);
    col.vertices = { {0,0,0}, {1,0,0}, {0,0,1} };
    col.indices  = { 0,1,2,  0,1,99 };

    // Act
    const AcousticScene scene = BuildAcousticScene(&w);

    // Assert: the good triangle survives and the bad one is dropped. Passing it
    // on would be a read off the end of an array inside somebody else's library.
    ENJIN_EXPECT_EQ(scene.TriangleCount(), (usize)1);
    ENJIN_EXPECT_EQ(scene.materialIndices.size(), (usize)1);
}

ENJIN_TEST(AcousticScene, EveryMaterialIndexPointsAtARealEntry) {
    // Arrange: a mixed room.
    World w;
    MakeBox(w, Math::Vector3(0, 0, 0), Math::Vector3(8, 1, 8), SurfaceMaterial::Tile);
    MakeBox(w, Math::Vector3(0, 3, 4), Math::Vector3(8, 6, 1), SurfaceMaterial::Drywall);
    MakeBox(w, Math::Vector3(4, 3, 0), Math::Vector3(1, 6, 8), SurfaceMaterial::Brick);
    Entity ball = w.CreateEntity();
    w.AddComponent<TransformComponent>(ball);
    w.AddComponent<SphereColliderComponent>(ball).radius = 1.0f;
    w.AddComponent<MaterialComponent>(ball).surfaceMaterial = SurfaceMaterial::Fabric;

    // Act
    const AcousticScene scene = BuildAcousticScene(&w);

    // Assert: no index is out of range. This is the check that stops a wrong
    // index becoming an out-of-bounds read inside the simulator rather than a
    // wrong sound.
    ENJIN_ASSERT_TRUE(scene.materials.Count() > 0);
    for (i32 idx : scene.materialIndices) {
        ENJIN_EXPECT_TRUE(idx >= 0);
        ENJIN_EXPECT_TRUE(static_cast<usize>(idx) < scene.materials.Count());
    }
    ENJIN_EXPECT_EQ(scene.materials.Count(), (usize)4);
}

ENJIN_TEST_MAIN()
