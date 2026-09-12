// A spatial index over the room, so sound can be traced through it.
//
// Everything above this is ray work: early reflections mirror a source across
// surfaces and ask whether the path is clear, late reverb fires thousands of
// rays and follows them until their energy is gone. If the index is wrong, the
// room is wrong, and the symptom is a reverb that sounds plausible and is not
// the room you are standing in -- which nobody can debug by ear.
//
// So the load-bearing test here is that it agrees with brute force on every
// ray. An accelerator that is merely FAST is worthless; one that is fast and
// identical to the slow answer is the whole point.

#include "EnjinTest.h"
#include "Enjin/Acoustics/AcousticBVH.h"
#include "Enjin/Audio/AcousticScene.h"

#include <cmath>
#include <random>

using namespace Enjin;
using namespace Enjin::Acoustics;
using Enjin::Math::Vector3;

namespace {

// A closed box room, built as an AcousticScene directly so these tests do not
// depend on the ECS. `inward` puts the normals facing into the room, which is
// how a room you stand inside is built.
Audio::AcousticScene ShoeboxRoom(f32 width, f32 height, f32 depth,
                                 ECS::SurfaceMaterial surface = ECS::SurfaceMaterial::Concrete) {
    Audio::AcousticScene s;
    const f32 x = width * 0.5f, y = height * 0.5f, z = depth * 0.5f;
    const Vector3 corner[8] = {
        {-x,-y,-z}, { x,-y,-z}, { x, y,-z}, {-x, y,-z},
        {-x,-y, z}, { x,-y, z}, { x, y, z}, {-x, y, z},
    };
    for (const auto& c : corner) s.vertices.push_back(c);

    static const i32 kTris[36] = {
        0,1,2, 0,2,3,   // -z
        5,4,7, 5,7,6,   // +z
        4,5,1, 4,1,0,   // -y  (floor)
        3,2,6, 3,6,7,   // +y  (ceiling)
        4,0,3, 4,3,7,   // -x
        1,5,6, 1,6,2,   // +x
    };
    const i32 mat = static_cast<i32>(s.materials.IndexFor(surface));
    for (i32 i = 0; i < 36; ++i) s.indices.push_back(kTris[i]);
    for (i32 t = 0; t < 12; ++t) s.materialIndices.push_back(mat);
    return s;
}

// The answer the BVH has to match.
RayHit BruteForce(const Audio::AcousticScene& scene, const Vector3& o, const Vector3& d,
                  f32 maxDistance) {
    RayHit best;
    f32 nearest = maxDistance;
    const f32 len = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
    const Vector3 dir(d.x / len, d.y / len, d.z / len);

    for (usize t = 0; t < scene.TriangleCount(); ++t) {
        const Vector3& a = scene.vertices[static_cast<usize>(scene.indices[t * 3 + 0])];
        const Vector3& b = scene.vertices[static_cast<usize>(scene.indices[t * 3 + 1])];
        const Vector3& c = scene.vertices[static_cast<usize>(scene.indices[t * 3 + 2])];
        const Vector3 e1(b.x - a.x, b.y - a.y, b.z - a.z);
        const Vector3 e2(c.x - a.x, c.y - a.y, c.z - a.z);
        const Vector3 p(dir.y * e2.z - dir.z * e2.y, dir.z * e2.x - dir.x * e2.z,
                        dir.x * e2.y - dir.y * e2.x);
        const f32 det = e1.x * p.x + e1.y * p.y + e1.z * p.z;
        if (std::fabs(det) < 1.0e-12f) continue;
        const f32 inv = 1.0f / det;
        const Vector3 tv(o.x - a.x, o.y - a.y, o.z - a.z);
        const f32 u = (tv.x * p.x + tv.y * p.y + tv.z * p.z) * inv;
        if (u < 0.0f || u > 1.0f) continue;
        const Vector3 q(tv.y * e1.z - tv.z * e1.y, tv.z * e1.x - tv.x * e1.z,
                        tv.x * e1.y - tv.y * e1.x);
        const f32 v = (dir.x * q.x + dir.y * q.y + dir.z * q.z) * inv;
        if (v < 0.0f || u + v > 1.0f) continue;
        const f32 hitT = (e2.x * q.x + e2.y * q.y + e2.z * q.z) * inv;
        if (hitT <= 1.0e-4f || hitT >= nearest) continue;
        nearest = hitT;
        best.hit = true;
        best.distance = hitT;
        best.triangle = static_cast<u32>(t);
    }
    return best;
}

} // namespace

ENJIN_TEST(AcousticBVH, AnEmptySceneBuildsNothingAndMissesEverything) {
    // Arrange
    Audio::AcousticScene empty;
    AcousticBVH bvh;

    // Act
    bvh.Build(empty);

    // Assert: a miss, not a crash. A level with no colliders is an ordinary
    // state, not an error.
    ENJIN_EXPECT_FALSE(bvh.IsBuilt());
    ENJIN_EXPECT_FALSE(bvh.Raycast(Vector3(0, 0, 0), Vector3(0, 0, 1)).hit);
    ENJIN_EXPECT_FALSE(bvh.Occluded(Vector3(0, 0, 0), Vector3(10, 0, 0)));
}

ENJIN_TEST(AcousticBVH, ARayFromInsideAShoeboxHitsTheWallAtTheRightDistance) {
    // Arrange: an 8 x 4 x 6 room, listener in the middle.
    const Audio::AcousticScene room = ShoeboxRoom(8.0f, 4.0f, 6.0f);
    AcousticBVH bvh;
    bvh.Build(room);
    ENJIN_ASSERT_TRUE(bvh.IsBuilt());

    // Act / Assert: four metres to the +x wall, two to the ceiling, three to
    // the +z wall. These are the distances the early reflection delays are
    // computed from, so an error here is an error in every reflection time.
    ENJIN_EXPECT_FLOAT_NEAR(bvh.Raycast(Vector3(0, 0, 0), Vector3(1, 0, 0)).distance, 4.0f, 0.01f);
    ENJIN_EXPECT_FLOAT_NEAR(bvh.Raycast(Vector3(0, 0, 0), Vector3(0, 1, 0)).distance, 2.0f, 0.01f);
    ENJIN_EXPECT_FLOAT_NEAR(bvh.Raycast(Vector3(0, 0, 0), Vector3(0, 0, 1)).distance, 3.0f, 0.01f);
    ENJIN_EXPECT_FLOAT_NEAR(bvh.Raycast(Vector3(0, 0, 0), Vector3(-1, 0, 0)).distance, 4.0f, 0.01f);
}

ENJIN_TEST(AcousticBVH, ADirectionNeedNotBeNormalised) {
    // Arrange
    const Audio::AcousticScene room = ShoeboxRoom(8.0f, 4.0f, 6.0f);
    AcousticBVH bvh;
    bvh.Build(room);

    // Act / Assert: distance is in metres regardless of how long the direction
    // vector was. A caller passing an unnormalised direction and getting a
    // distance scaled by its length would produce reflection delays that are
    // wrong by an arbitrary factor.
    ENJIN_EXPECT_FLOAT_NEAR(bvh.Raycast(Vector3(0, 0, 0), Vector3(17, 0, 0)).distance, 4.0f, 0.01f);
}

ENJIN_TEST(AcousticBVH, TheNormalFacesBackAlongTheRay) {
    // Arrange
    const Audio::AcousticScene room = ShoeboxRoom(8.0f, 4.0f, 6.0f);
    AcousticBVH bvh;
    bvh.Build(room);

    // Act
    const RayHit hit = bvh.Raycast(Vector3(0, 0, 0), Vector3(1, 0, 0));

    // Assert: a room's surfaces have no reliable winding -- a floor built by
    // one tool and a wall by another rarely agree on which way is out -- so
    // "outward" is decided by where the sound came from. A normal pointing the
    // other way reflects sound INTO the wall.
    ENJIN_ASSERT_TRUE(hit.hit);
    ENJIN_EXPECT_TRUE(hit.normal.x < -0.9f);
    ENJIN_EXPECT_FLOAT_NEAR(std::sqrt(hit.normal.x * hit.normal.x + hit.normal.y * hit.normal.y +
                                      hit.normal.z * hit.normal.z), 1.0f, 0.001f);
}

ENJIN_TEST(AcousticBVH, AHitCarriesTheMaterialOfTheSurfaceItStruck) {
    // Arrange: a room with a carpeted floor and concrete everywhere else.
    Audio::AcousticScene room = ShoeboxRoom(8.0f, 4.0f, 6.0f, ECS::SurfaceMaterial::Concrete);
    const i32 carpet = static_cast<i32>(room.materials.IndexFor(ECS::SurfaceMaterial::Carpet));
    room.materialIndices[4] = carpet;   // the two floor triangles
    room.materialIndices[5] = carpet;

    AcousticBVH bvh;
    bvh.Build(room);

    // Act
    const RayHit down = bvh.Raycast(Vector3(0, 0, 0), Vector3(0, -1, 0));
    const RayHit up = bvh.Raycast(Vector3(0, 0, 0), Vector3(0, 1, 0));

    // Assert: this is the whole reason the scene carries per-triangle materials.
    // A reflection has to know it came off carpet rather than concrete at the
    // moment it bounces.
    ENJIN_ASSERT_TRUE(down.hit && up.hit);
    ENJIN_EXPECT_EQ(down.material, carpet);
    ENJIN_EXPECT_TRUE(up.material != carpet);
}

ENJIN_TEST(AcousticBVH, MaxDistanceStopsTheSearch) {
    // Arrange
    const Audio::AcousticScene room = ShoeboxRoom(8.0f, 4.0f, 6.0f);
    AcousticBVH bvh;
    bvh.Build(room);

    // Act / Assert: a reverb ray that has run out of energy should stop, not
    // keep traversing the level.
    ENJIN_EXPECT_FALSE(bvh.Raycast(Vector3(0, 0, 0), Vector3(1, 0, 0), 2.0f).hit);
    ENJIN_EXPECT_TRUE(bvh.Raycast(Vector3(0, 0, 0), Vector3(1, 0, 0), 6.0f).hit);
}

// The load-bearing test.
ENJIN_TEST(AcousticBVH, EveryRayAgreesWithBruteForce) {
    // Arrange: a room with clutter in it, so the tree is more than one node
    // deep and rays have real choices to get wrong.
    Audio::AcousticScene scene = ShoeboxRoom(20.0f, 6.0f, 16.0f);
    std::mt19937 rng(20260912u);
    std::uniform_real_distribution<f32> pos(-7.0f, 7.0f);
    for (u32 i = 0; i < 40; ++i) {
        const f32 cx = pos(rng), cy = pos(rng) * 0.3f, cz = pos(rng);
        const i32 base = static_cast<i32>(scene.vertices.size());
        scene.vertices.push_back({cx - 0.6f, cy - 0.6f, cz});
        scene.vertices.push_back({cx + 0.6f, cy - 0.6f, cz});
        scene.vertices.push_back({cx, cy + 0.6f, cz + 0.4f});
        scene.indices.push_back(base + 0);
        scene.indices.push_back(base + 1);
        scene.indices.push_back(base + 2);
        scene.materialIndices.push_back(0);
    }

    AcousticBVH bvh;
    bvh.Build(scene);
    ENJIN_ASSERT_TRUE(bvh.IsBuilt());

    // Act / Assert: a thousand random rays, each compared against the slow
    // answer. An accelerator that is merely fast is worthless.
    std::uniform_real_distribution<f32> dir(-1.0f, 1.0f);
    usize checked = 0, mismatches = 0;
    for (u32 i = 0; i < 1000; ++i) {
        const Vector3 o(pos(rng) * 0.8f, pos(rng) * 0.25f, pos(rng) * 0.8f);
        Vector3 d(dir(rng), dir(rng), dir(rng));
        const f32 len = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
        if (len < 0.1f) continue;
        d = Vector3(d.x / len, d.y / len, d.z / len);

        const RayHit fast = bvh.Raycast(o, d, 1000.0f);
        const RayHit slow = BruteForce(scene, o, d, 1000.0f);
        ++checked;
        if (fast.hit != slow.hit) { ++mismatches; continue; }
        if (fast.hit && std::fabs(fast.distance - slow.distance) > 0.001f) ++mismatches;
    }

    ENJIN_EXPECT_TRUE(checked > 900);
    ENJIN_EXPECT_EQ(mismatches, (usize)0);
}

ENJIN_TEST(AcousticBVH, OcclusionSeesAWallBetweenTwoPoints) {
    // Arrange: a room divided by a partition at x = 0.
    Audio::AcousticScene scene = ShoeboxRoom(20.0f, 6.0f, 16.0f);
    const i32 base = static_cast<i32>(scene.vertices.size());
    scene.vertices.push_back({0.0f, -3.0f, -8.0f});
    scene.vertices.push_back({0.0f, -3.0f,  8.0f});
    scene.vertices.push_back({0.0f,  3.0f,  8.0f});
    scene.vertices.push_back({0.0f,  3.0f, -8.0f});
    scene.indices.push_back(base + 0); scene.indices.push_back(base + 1); scene.indices.push_back(base + 2);
    scene.indices.push_back(base + 0); scene.indices.push_back(base + 2); scene.indices.push_back(base + 3);
    scene.materialIndices.push_back(0);
    scene.materialIndices.push_back(0);

    AcousticBVH bvh;
    bvh.Build(scene);

    // Act / Assert
    ENJIN_EXPECT_TRUE(bvh.Occluded(Vector3(-5, 0, 0), Vector3(5, 0, 0)));
    ENJIN_EXPECT_FALSE(bvh.Occluded(Vector3(-5, 0, 0), Vector3(-2, 0, 0)));
    // Past the end of the partition, the path is clear.
    ENJIN_EXPECT_FALSE(bvh.Occluded(Vector3(-5, 0, 12), Vector3(5, 0, 12)));
}

ENJIN_TEST(AcousticBVH, ASurfaceDoesNotOccludeItself) {
    // Arrange: a reflection point sits ON a wall by construction, so the test
    // from it back to the listener starts on geometry. Without an epsilon at
    // both ends, every candidate reflection reports the wall occluding itself
    // and a room has no early reflections at all.
    const Audio::AcousticScene room = ShoeboxRoom(8.0f, 4.0f, 6.0f);
    AcousticBVH bvh;
    bvh.Build(room);

    const Vector3 onTheWall(4.0f, 0.0f, 0.0f);   // exactly on the +x wall
    ENJIN_EXPECT_FALSE(bvh.Occluded(onTheWall, Vector3(0, 0, 0)));
    ENJIN_EXPECT_FALSE(bvh.Occluded(Vector3(0, 0, 0), onTheWall));
}

ENJIN_TEST(AcousticBVH, CoincidentGeometryDoesNotHangTheBuild) {
    // Arrange: forty identical quads stacked in the same place, which happens
    // in hand-built rooms and makes a median split put everything on one side.
    // A builder that recursed on the same set would never return.
    Audio::AcousticScene scene;
    for (u32 i = 0; i < 40; ++i) {
        const i32 base = static_cast<i32>(scene.vertices.size());
        scene.vertices.push_back({-1, 0, -1});
        scene.vertices.push_back({ 1, 0, -1});
        scene.vertices.push_back({ 1, 0,  1});
        scene.indices.push_back(base + 0);
        scene.indices.push_back(base + 1);
        scene.indices.push_back(base + 2);
        scene.materialIndices.push_back(0);
    }

    // Act
    AcousticBVH bvh;
    bvh.Build(scene);

    // Assert: it built, and it still finds them.
    ENJIN_ASSERT_TRUE(bvh.IsBuilt());
    ENJIN_EXPECT_EQ(bvh.TriangleCount(), (usize)40);
    ENJIN_EXPECT_TRUE(bvh.Raycast(Vector3(0, 5, 0), Vector3(0, -1, 0)).hit);
}

ENJIN_TEST_MAIN()
