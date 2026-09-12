// The first few echoes, and where they come from.
//
// The late tail says how big and how hard a room is. The EARLY part says where
// you are standing in it -- a source two metres from a back wall sends a
// reflection off that wall a few milliseconds after the direct sound, arriving
// from behind it, and that pair of facts is what places the source against the
// wall. Marty's bar: "the projector running in the basement audibly sits
// against the back wall."
//
// Image-source reflections are EXACT, which is unusual and useful: the delay is
// a distance and the direction is a direction, both computable by hand. So
// these tests do not check that plausible numbers come out. They check the
// numbers against arithmetic done in the comments.

#include "EnjinTest.h"
#include "Enjin/Acoustics/EarlyReflections.h"
#include "Enjin/Acoustics/AcousticBVH.h"
#include "Enjin/Audio/AcousticScene.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

using namespace Enjin;
using namespace Enjin::Acoustics;
using Enjin::Math::Vector3;

namespace {

Audio::AcousticScene ShoeboxRoom(f32 width, f32 height, f32 depth,
                                 ECS::SurfaceMaterial surface) {
    Audio::AcousticScene s;
    const f32 x = width * 0.5f, y = height * 0.5f, z = depth * 0.5f;
    const Vector3 corner[8] = {
        {-x,-y,-z}, { x,-y,-z}, { x, y,-z}, {-x, y,-z},
        {-x,-y, z}, { x,-y, z}, { x, y, z}, {-x, y, z},
    };
    for (const auto& c : corner) s.vertices.push_back(c);
    static const i32 kTris[36] = {
        0,1,2, 0,2,3,  5,4,7, 5,7,6,  4,5,1, 4,1,0,
        3,2,6, 3,6,7,  4,0,3, 4,3,7,  1,5,6, 1,6,2,
    };
    const i32 mat = static_cast<i32>(s.materials.IndexFor(surface));
    for (i32 i = 0; i < 36; ++i) s.indices.push_back(kTris[i]);
    for (i32 t = 0; t < 12; ++t) s.materialIndices.push_back(mat);
    return s;
}

// The tap whose arrival direction is closest to `want`.
//
// Only useful when the reflecting surface is roughly on the axis between source
// and listener. A floor bounce between two points four metres apart in a room
// four metres tall arrives at 45 degrees, not from straight down -- which is
// geometry doing its job, and which this helper would call a miss.
const EarlyReflection* TapFrom(const EarlyReflectionResult& r, const Vector3& want) {
    const EarlyReflection* best = nullptr;
    f32 bestDot = -2.0f;
    for (const auto& t : r.taps) {
        const f32 d = t.direction.x * want.x + t.direction.y * want.y + t.direction.z * want.z;
        if (d > bestDot) { bestDot = d; best = &t; }
    }
    return (bestDot > 0.9f) ? best : nullptr;
}

// The tap that bounced off a given surface, chosen by WHERE it bounced.
// Unambiguous whatever angle it arrives from.
const EarlyReflection* TapOffPlaneY(const EarlyReflectionResult& r, f32 y, f32 tolerance = 0.05f) {
    for (const auto& t : r.taps) {
        if (std::fabs(t.reflectionPoint.y - y) < tolerance && t.order == 1) return &t;
    }
    return nullptr;
}

} // namespace

ENJIN_TEST(EarlyReflections, WithNoGeometryThereIsOnlyTheDirectSound) {
    // Arrange
    AcousticBVH empty;
    Audio::AcousticScene scene;

    // Act
    const EarlyReflectionResult r =
        TraceEarlyReflections(empty, scene, Vector3(0, 0, 0), Vector3(3, 0, 4));

    // Assert: five metres, and nothing else. An empty level is not an error.
    ENJIN_EXPECT_FALSE(r.Any());
    ENJIN_EXPECT_FLOAT_NEAR(r.directDistance, 5.0f, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(r.directDelay, 5.0f / kSpeedOfSound, 0.0001f);
    ENJIN_EXPECT_FALSE(r.directOccluded);
}

ENJIN_TEST(EarlyReflections, AFloorBounceArrivesWhenTheGeometrySaysItShould) {
    // Arrange: a room 4 m tall, so the floor is 2 m below the centre line.
    // Source and listener both at y = 0, four metres apart along x.
    //
    // By hand: the image of the source is 2 m below the floor, i.e. 4 m below
    // the listener's height. The reflected path is the straight line from the
    // listener to that image: sqrt(4^2 + 4^2) = 5.657 m.
    const Audio::AcousticScene room = ShoeboxRoom(20.0f, 4.0f, 20.0f,
                                                  ECS::SurfaceMaterial::Concrete);
    AcousticBVH bvh;
    bvh.Build(room);

    // Act
    const EarlyReflectionResult r =
        TraceEarlyReflections(bvh, room, Vector3(-2, 0, 0), Vector3(2, 0, 0));

    // Assert
    const EarlyReflection* floor = TapOffPlaneY(r, -2.0f);
    ENJIN_ASSERT_TRUE(floor != nullptr);
    ENJIN_EXPECT_FLOAT_NEAR(floor->distance, std::sqrt(32.0f), 0.01f);

    // It arrives from below and from the source's side, at 45 degrees -- which
    // is what the geometry says and is why a direction test alone is the wrong
    // way to find this tap.
    ENJIN_EXPECT_TRUE(floor->direction.y < -0.5f);
    ENJIN_EXPECT_FLOAT_NEAR(floor->delay, std::sqrt(32.0f) / kSpeedOfSound, 0.0005f);
    // It bounced halfway between them, on the floor.
    ENJIN_EXPECT_FLOAT_NEAR(floor->reflectionPoint.y, -2.0f, 0.01f);
    ENJIN_EXPECT_FLOAT_NEAR(floor->reflectionPoint.x, 0.0f, 0.01f);
}

ENJIN_TEST(EarlyReflections, TheCeilingAndFloorBouncesAreSymmetric) {
    // Arrange: a symmetric room, so the two must match exactly. An asymmetry
    // here would mean the mirroring depends on which way a normal happens to
    // face, which is how half a room goes quiet.
    const Audio::AcousticScene room = ShoeboxRoom(20.0f, 4.0f, 20.0f,
                                                  ECS::SurfaceMaterial::Concrete);
    AcousticBVH bvh;
    bvh.Build(room);

    // Act
    const EarlyReflectionResult r =
        TraceEarlyReflections(bvh, room, Vector3(-2, 0, 0), Vector3(2, 0, 0));

    // Assert
    const EarlyReflection* floor = TapOffPlaneY(r, -2.0f);
    const EarlyReflection* ceiling = TapOffPlaneY(r, 2.0f);
    ENJIN_ASSERT_TRUE(floor != nullptr);
    ENJIN_ASSERT_TRUE(ceiling != nullptr);
    ENJIN_EXPECT_FLOAT_NEAR(floor->distance, ceiling->distance, 0.01f);
    ENJIN_EXPECT_FLOAT_NEAR(floor->gain[1], ceiling->gain[1], 0.0001f);
}

// The acceptance criterion, as arithmetic.
ENJIN_TEST(EarlyReflections, ASourceAgainstTheBackWallReflectsOffThatWallFromBehindIt) {
    // Arrange: a 20 m room, so the +x wall is at x = 10. The projector sits 1 m
    // in front of it at x = 9, and the listener is well out in the room at
    // x = 0, all on the centre line.
    //
    // By hand: direct path 9 m. The image of the source is 1 m behind the wall
    // at x = 11, so the reflected path is 11 m. The difference is 2 m, which is
    // 5.8 ms -- a distinct early reflection, arriving from the direction of the
    // wall, which is what puts the projector against it.
    const Audio::AcousticScene room = ShoeboxRoom(20.0f, 4.0f, 12.0f,
                                                  ECS::SurfaceMaterial::Concrete);
    AcousticBVH bvh;
    bvh.Build(room);

    // Act
    const EarlyReflectionResult r =
        TraceEarlyReflections(bvh, room, Vector3(9, 0, 0), Vector3(0, 0, 0));

    // Assert
    ENJIN_ASSERT_TRUE(r.Any());
    ENJIN_EXPECT_FLOAT_NEAR(r.directDistance, 9.0f, 0.01f);

    const EarlyReflection* wall = TapFrom(r, Vector3(1, 0, 0));
    ENJIN_ASSERT_TRUE(wall != nullptr);
    ENJIN_EXPECT_FLOAT_NEAR(wall->distance, 11.0f, 0.05f);
    ENJIN_EXPECT_FLOAT_NEAR(wall->reflectionPoint.x, 10.0f, 0.05f);

    // It arrives from the SAME side as the source, not from the listener's
    // side, and just under six milliseconds after the direct sound.
    const f32 gap = wall->delay - r.directDelay;
    std::printf("    back wall: direct %.1f m, reflected %.1f m, gap %.2f ms\n",
                r.directDistance, wall->distance, gap * 1000.0f);
    ENJIN_EXPECT_TRUE(wall->direction.x > 0.9f);
    ENJIN_EXPECT_FLOAT_NEAR(gap, 2.0f / kSpeedOfSound, 0.0005f);
}

ENJIN_TEST(EarlyReflections, MovingTheSourceOffTheWallMovesTheReflectionLater) {
    // Arrange: the same room, the projector pulled out to the middle. The cue
    // has to CHANGE, or it is not telling a listener anything about where the
    // source is.
    const Audio::AcousticScene room = ShoeboxRoom(20.0f, 4.0f, 12.0f,
                                                  ECS::SurfaceMaterial::Concrete);
    AcousticBVH bvh;
    bvh.Build(room);

    // Act
    // Not `near` and `far`: those are windef.h macros on Windows, and the
    // compiler error they produce names the line after the one at fault.
    const EarlyReflectionResult againstWall =
        TraceEarlyReflections(bvh, room, Vector3(9, 0, 0), Vector3(0, 0, 0));
    const EarlyReflectionResult outInTheRoom =
        TraceEarlyReflections(bvh, room, Vector3(4, 0, 0), Vector3(0, 0, 0));

    // Assert: against the wall, 2 m of extra path. Five metres out, 12 m of it.
    const EarlyReflection* a = TapFrom(againstWall, Vector3(1, 0, 0));
    const EarlyReflection* b = TapFrom(outInTheRoom, Vector3(1, 0, 0));
    ENJIN_ASSERT_TRUE(a != nullptr && b != nullptr);
    const f32 gapNear = a->delay - againstWall.directDelay;
    const f32 gapFar = b->delay - outInTheRoom.directDelay;
    ENJIN_EXPECT_FLOAT_NEAR(gapNear, 2.0f / kSpeedOfSound, 0.0005f);
    ENJIN_EXPECT_FLOAT_NEAR(gapFar, 12.0f / kSpeedOfSound, 0.0005f);
    ENJIN_EXPECT_TRUE(gapFar > gapNear * 4.0f);
}

ENJIN_TEST(EarlyReflections, ACarpetedRoomReflectsMoreQuietlyThanAConcreteOne) {
    // Arrange: identical geometry, different material.
    const Audio::AcousticScene hard = ShoeboxRoom(20.0f, 4.0f, 12.0f,
                                                  ECS::SurfaceMaterial::Concrete);
    const Audio::AcousticScene soft = ShoeboxRoom(20.0f, 4.0f, 12.0f,
                                                  ECS::SurfaceMaterial::Carpet);
    AcousticBVH hardBvh, softBvh;
    hardBvh.Build(hard);
    softBvh.Build(soft);

    // Act
    const EarlyReflectionResult a =
        TraceEarlyReflections(hardBvh, hard, Vector3(9, 0, 0), Vector3(0, 0, 0));
    const EarlyReflectionResult b =
        TraceEarlyReflections(softBvh, soft, Vector3(9, 0, 0), Vector3(0, 0, 0));

    const EarlyReflection* hardWall = TapFrom(a, Vector3(1, 0, 0));
    const EarlyReflection* softWall = TapFrom(b, Vector3(1, 0, 0));
    ENJIN_ASSERT_TRUE(hardWall != nullptr && softWall != nullptr);

    // Assert: same path length, different gain -- so the difference really is
    // the material and not the geometry.
    ENJIN_EXPECT_FLOAT_NEAR(hardWall->distance, softWall->distance, 0.05f);
    ENJIN_EXPECT_TRUE(softWall->gain[2] < hardWall->gain[2] * 0.7f);
}

ENJIN_TEST(EarlyReflections, AbsorptionIsAppliedToEnergyAndNotToAmplitude) {
    // Arrange: concrete absorbs 2% of the mid band, so the amplitude left is
    // sqrt(0.98) = 0.99, not 0.98. Getting this wrong makes every surface about
    // twice as absorbent as it is, and a concrete room sounds carpeted.
    const Audio::AcousticScene room = ShoeboxRoom(20.0f, 4.0f, 12.0f,
                                                  ECS::SurfaceMaterial::Concrete);
    AcousticBVH bvh;
    bvh.Build(room);

    // Act
    const EarlyReflectionResult r =
        TraceEarlyReflections(bvh, room, Vector3(9, 0, 0), Vector3(0, 0, 0));
    const EarlyReflection* wall = TapFrom(r, Vector3(1, 0, 0));
    ENJIN_ASSERT_TRUE(wall != nullptr);

    // Assert: gain is 1/distance times sqrt(1 - alpha).
    const f32 alpha = Audio::AcousticsFor(ECS::SurfaceMaterial::Concrete).absorption[1];
    const f32 expected = (1.0f / wall->distance) * std::sqrt(1.0f - alpha);
    ENJIN_EXPECT_FLOAT_NEAR(wall->gain[1], expected, 0.0005f);
}

ENJIN_TEST(EarlyReflections, AWallInTheWayRemovesTheDirectPathButNotTheRoom) {
    // Arrange: a partition between source and listener, with gaps above and
    // below so reflections can still get around it.
    Audio::AcousticScene room = ShoeboxRoom(20.0f, 8.0f, 12.0f,
                                            ECS::SurfaceMaterial::Concrete);
    const i32 base = static_cast<i32>(room.vertices.size());
    room.vertices.push_back({0.0f, -1.0f, -6.0f});
    room.vertices.push_back({0.0f, -1.0f,  6.0f});
    room.vertices.push_back({0.0f,  1.0f,  6.0f});
    room.vertices.push_back({0.0f,  1.0f, -6.0f});
    room.indices.push_back(base + 0); room.indices.push_back(base + 1); room.indices.push_back(base + 2);
    room.indices.push_back(base + 0); room.indices.push_back(base + 2); room.indices.push_back(base + 3);
    room.materialIndices.push_back(0);
    room.materialIndices.push_back(0);

    AcousticBVH bvh;
    bvh.Build(room);

    // Act
    const EarlyReflectionResult r =
        TraceEarlyReflections(bvh, room, Vector3(-6, 0, 0), Vector3(6, 0, 0));

    // Assert: the straight line is blocked, and the listener still hears the
    // room. This is the case a direct-path-only spatializer gets wrong -- it
    // goes silent when it should go muffled and roomy.
    ENJIN_EXPECT_TRUE(r.directOccluded);
    ENJIN_EXPECT_TRUE(r.Any());
}

ENJIN_TEST(EarlyReflections, AReflectionThroughAWallIsNotReported) {
    // Arrange: two sealed rooms side by side sharing no opening. A listener in
    // one must not hear reflections off surfaces in the other.
    Audio::AcousticScene world = ShoeboxRoom(10.0f, 4.0f, 10.0f,
                                             ECS::SurfaceMaterial::Concrete);
    // A second box, well away.
    const i32 base = static_cast<i32>(world.vertices.size());
    const f32 ox = 40.0f;
    const Vector3 corner[8] = {
        {ox-5,-2,-5}, {ox+5,-2,-5}, {ox+5,2,-5}, {ox-5,2,-5},
        {ox-5,-2, 5}, {ox+5,-2, 5}, {ox+5,2, 5}, {ox-5,2, 5},
    };
    for (const auto& c : corner) world.vertices.push_back(c);
    static const i32 kTris[36] = {
        0,1,2, 0,2,3,  5,4,7, 5,7,6,  4,5,1, 4,1,0,
        3,2,6, 3,6,7,  4,0,3, 4,3,7,  1,5,6, 1,6,2,
    };
    for (i32 i = 0; i < 36; ++i) world.indices.push_back(base + kTris[i]);
    for (i32 t = 0; t < 12; ++t) world.materialIndices.push_back(0);

    AcousticBVH bvh;
    bvh.Build(world);

    // Act: source and listener both in the first room.
    const EarlyReflectionResult r =
        TraceEarlyReflections(bvh, world, Vector3(-2, 0, 0), Vector3(2, 0, 0));

    // Assert: every bounce happened in this room. A tap off the far box would
    // be a reflection arriving through a solid wall, and would place the source
    // forty metres away.
    ENJIN_ASSERT_TRUE(r.Any());
    for (const auto& tap : r.taps) {
        ENJIN_EXPECT_TRUE(tap.reflectionPoint.x < 20.0f);
    }
}

ENJIN_TEST(EarlyReflections, SecondOrderBouncesAreFoundAndArriveAfterFirstOrderOnes) {
    // Arrange
    const Audio::AcousticScene room = ShoeboxRoom(12.0f, 4.0f, 10.0f,
                                                  ECS::SurfaceMaterial::Concrete);
    AcousticBVH bvh;
    bvh.Build(room);

    EarlyReflectionSettings settings;
    settings.maxOrder = 2;
    settings.maxTaps = 64;

    // Act
    const EarlyReflectionResult r =
        TraceEarlyReflections(bvh, room, Vector3(-3, 0, 1), Vector3(3, 0, -1), settings);

    // Assert: a corner sends back a reflection neither wall would alone.
    usize second = 0;
    f32 earliestSecond = 1.0e9f, latestFirst = 0.0f;
    for (const auto& t : r.taps) {
        if (t.order == 2) { ++second; earliestSecond = std::min(earliestSecond, t.delay); }
        else              { latestFirst = std::max(latestFirst, t.delay); }
    }
    ENJIN_EXPECT_TRUE(second > 0);
    // A double bounce is a longer path than the shortest single one.
    f32 earliestFirst = 1.0e9f;
    for (const auto& t : r.taps) if (t.order == 1) earliestFirst = std::min(earliestFirst, t.delay);
    ENJIN_EXPECT_TRUE(earliestSecond > earliestFirst);
}

ENJIN_TEST(EarlyReflections, TapsComeOutInArrivalOrder) {
    // Arrange
    const Audio::AcousticScene room = ShoeboxRoom(12.0f, 4.0f, 10.0f,
                                                  ECS::SurfaceMaterial::Concrete);
    AcousticBVH bvh;
    bvh.Build(room);

    // Act
    const EarlyReflectionResult r =
        TraceEarlyReflections(bvh, room, Vector3(-3, 0, 1), Vector3(3, 0, -1));

    // Assert: anything rendering these into a delay line wants them in time
    // order, and the cap is applied by LOUDNESS before this -- so a strong late
    // reflection is kept and a weak early one is dropped.
    ENJIN_ASSERT_TRUE(r.taps.size() > 1);
    for (usize i = 1; i < r.taps.size(); ++i) {
        ENJIN_EXPECT_TRUE(r.taps[i].delay >= r.taps[i - 1].delay);
    }
}

ENJIN_TEST(EarlyReflections, TheSameSceneTracedTwiceGivesTheSameTaps) {
    // Arrange
    const Audio::AcousticScene room = ShoeboxRoom(12.0f, 4.0f, 10.0f,
                                                  ECS::SurfaceMaterial::Brick);
    AcousticBVH bvh;
    bvh.Build(room);

    // Act
    const EarlyReflectionResult a =
        TraceEarlyReflections(bvh, room, Vector3(-3, 0, 1), Vector3(3, 0, -1));
    const EarlyReflectionResult b =
        TraceEarlyReflections(bvh, room, Vector3(-3, 0, 1), Vector3(3, 0, -1));

    // Assert: a room whose reflections shifted between two identical frames
    // would crackle.
    ENJIN_ASSERT_EQ(a.taps.size(), b.taps.size());
    for (usize i = 0; i < a.taps.size(); ++i) {
        ENJIN_EXPECT_FLOAT_NEAR(a.taps[i].delay, b.taps[i].delay, 0.00001f);
        ENJIN_EXPECT_FLOAT_NEAR(a.taps[i].gain[1], b.taps[i].gain[1], 0.00001f);
    }
}

ENJIN_TEST_MAIN()
