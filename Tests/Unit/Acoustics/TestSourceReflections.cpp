// Reflections traced from where the SOURCE is, not from where you are.
//
// The shared reverb bus renders one pattern, traced with the source placed at
// the listener's own position. That is exactly right for a sound made where you
// stand -- a clap, a footstep, your own voice -- and it is increasingly wrong
// the further away a sound is, because which surfaces answer and how long after
// the direct sound they arrive are properties of the path from THAT source to
// your ears.
//
// The acceptance case for the whole feature is a projector a metre from the
// back wall: the direct sound travels one metre less than the bounce behind it,
// so a reflection lands about six milliseconds later, arriving from the wall. A
// pattern traced from the listener's position cannot produce that, because the
// listener is not a metre from that wall.
//
// These tests are about the GEOMETRY of the answer. Whether it is audible is
// what the demo is for.

#include "EnjinTest.h"
#include "RoomBuilder.h"
#include "Enjin/Acoustics/AcousticBVH.h"
#include "Enjin/Acoustics/EarlyReflections.h"
#include "Enjin/Acoustics/AcousticsSystem.h"
#include "Enjin/Audio/AudioEngine.h"

#include <cstdio>

using namespace Enjin;
using namespace Enjin::Acoustics;
using Enjin::Math::Vector3;
using Enjin::ECS::SurfaceMaterial;
using RoomBuilder::Dimensions;
using RoomBuilder::Surfaces;

namespace {

f32 EarliestTap(const EarlyReflectionResult& r) {
    f32 earliest = 1.0e9f;
    for (const auto& t : r.taps) if (t.delay < earliest) earliest = t.delay;
    return r.Any() ? earliest : 0.0f;
}

} // namespace

ENJIN_TEST(SourceReflections, ASourceNearAWallAnswersDifferentlyFromOneAtYourFeet) {
    // Arrange: a hard room 20 x 6 x 16. One source a metre from the back wall,
    // one at the listener, listener in the middle both times.
    const Dimensions dim{ 20.0f, 6.0f, 16.0f };
    const Surfaces surf = Surfaces::All(SurfaceMaterial::Tile);
    const Audio::AcousticScene scene = RoomBuilder::Box(dim, surf);
    AcousticBVH bvh;
    bvh.Build(scene);

    const Vector3 listener(0.0f, 1.6f, 0.0f);
    const Vector3 nearWall(0.0f, 1.6f, -7.0f);     // 1 m from the -Z wall at z = -8
    const Vector3 middle(0.0f, 1.6f, 0.0f);

    // Act
    const EarlyReflectionResult fromWall = TraceEarlyReflections(bvh, scene, nearWall, listener);
    const EarlyReflectionResult fromMiddle = TraceEarlyReflections(bvh, scene, middle, listener);

    ENJIN_ASSERT_TRUE(fromWall.Any());
    ENJIN_ASSERT_TRUE(fromMiddle.Any());

    std::printf("    source 1 m from back wall: %zu taps, direct %.2f m, earliest %.1f ms\n",
                fromWall.taps.size(), fromWall.directDistance,
                EarliestTap(fromWall) * 1000.0f);
    std::printf("    source at the listener:    %zu taps, direct %.2f m, earliest %.1f ms\n",
                fromMiddle.taps.size(), fromMiddle.directDistance,
                EarliestTap(fromMiddle) * 1000.0f);

    // Assert: the two patterns must not be the same. If they were, per-source
    // tracing would be an elaborate way of computing the listener's own answer
    // twice -- which is exactly what the engine did before this existed.
    ENJIN_EXPECT_TRUE(fromWall.directDistance > 6.5f);
    ENJIN_EXPECT_FLOAT_NEAR(fromMiddle.directDistance, 0.0f, 0.1f);
}

ENJIN_TEST(SourceReflections, TheBackWallBounceArrivesWhenTheGeometrySaysItShould) {
    // Arrange: the acceptance case, with the arithmetic done by hand.
    //
    // Room is 16 deep, so the -Z wall is at z = -8. Source at z = -7, one metre
    // off it. Listener at z = 0.
    //
    //   direct path      7.00 m
    //   via the wall     1 + 8 = 9.00 m
    //   difference       2.00 m  ->  2 / 343 = 5.8 ms after the direct sound
    //
    // That delay and that direction are the whole cue. Anything that puts this
    // reflection at the wrong time puts the projector in the wrong place.
    const Dimensions dim{ 20.0f, 6.0f, 16.0f };
    const Surfaces surf = Surfaces::All(SurfaceMaterial::Tile);
    const Audio::AcousticScene scene = RoomBuilder::Box(dim, surf);
    AcousticBVH bvh;
    bvh.Build(scene);

    const Vector3 listener(0.0f, 1.6f, 0.0f);
    const Vector3 source(0.0f, 1.6f, -7.0f);

    // Act
    const EarlyReflectionResult r = TraceEarlyReflections(bvh, scene, source, listener);
    ENJIN_ASSERT_TRUE(r.Any());

    // The tap whose path length is closest to 9 m, and how far behind the
    // direct sound it lands.
    f32 bestErr = 1.0e9f;
    f32 backWallDelay = 0.0f;
    for (const auto& t : r.taps) {
        const f32 err = t.distance > 9.0f ? t.distance - 9.0f : 9.0f - t.distance;
        if (err < bestErr) { bestErr = err; backWallDelay = t.delay; }
    }

    const f32 directDelay = r.directDistance / kSpeedOfSound;
    const f32 gap = (backWallDelay - directDelay) * 1000.0f;

    std::printf("    direct %.2f m (%.1f ms), back-wall path within %.2f m of 9.00\n",
                r.directDistance, directDelay * 1000.0f, bestErr);
    std::printf("    back-wall reflection lands %.1f ms after the direct sound "
                "(geometry says 5.8)\n", gap);

    // Assert
    ENJIN_EXPECT_TRUE(bestErr < 0.5f);
    ENJIN_EXPECT_FLOAT_NEAR(gap, 5.8f, 2.0f);
}

ENJIN_TEST(SourceReflections, MovingTheSourceMovesThePattern) {
    // Arrange: one listener, the same room, a source walked across it.
    const Dimensions dim{ 20.0f, 6.0f, 16.0f };
    const Surfaces surf = Surfaces::All(SurfaceMaterial::Concrete);
    const Audio::AcousticScene scene = RoomBuilder::Box(dim, surf);
    AcousticBVH bvh;
    bvh.Build(scene);

    const Vector3 listener(0.0f, 1.6f, 0.0f);
    const f32 positions[] = { -7.0f, -3.0f, 0.0f, 3.0f, 7.0f };

    std::printf("    %8s %8s %10s %8s\n", "source z", "direct", "earliest", "taps");

    for (f32 z : positions) {
        // Act
        const EarlyReflectionResult r =
            TraceEarlyReflections(bvh, scene, Vector3(0.0f, 1.6f, z), listener);
        ENJIN_ASSERT_TRUE(r.Any());

        std::printf("    %8.1f %8.2f %9.1fms %8zu\n",
                    z, r.directDistance, EarliestTap(r) * 1000.0f, r.taps.size());

        // Assert: the direct distance has to track the source. A pattern that
        // did not move with it would be the listener's own answer relabelled.
        const f32 expectedDirect = z < 0.0f ? -z : z;
        ENJIN_EXPECT_FLOAT_NEAR(r.directDistance, expectedDirect, 0.2f);
    }
}

ENJIN_TEST(SourceReflections, SlotsStartEmptyAndRefuseHandlesThatWereNeverPlayed) {
    // Arrange
    Audio::AudioEngine engine;
    if (!engine.Initialize()) {
        ENJIN_SKIP("no audio device available to initialize against");
        return;
    }
    if (engine.SourceReflectionSlotCount() == 0) {
        ENJIN_SKIP("per-source reflection slots not available in this build");
        engine.Shutdown();
        return;
    }

    std::printf("    %u slots, %u in use at rest\n",
                engine.SourceReflectionSlotCount(), engine.SourceReflectionSlotsInUse());

    // Assert: nothing is playing, so nothing holds a slot. A pool that started
    // partly claimed would run out after a few dozen sounds and quietly stop
    // giving anything its own pattern -- audible as nothing at all, because the
    // shared pattern keeps answering.
    ENJIN_EXPECT_EQ(engine.SourceReflectionSlotsInUse(), 0u);

    EarlyReflectionResult fake;
    EarlyReflection tap;
    tap.delay = 0.01f;
    tap.gain[0] = tap.gain[1] = tap.gain[2] = 0.4f;
    fake.taps.push_back(tap);

    // A handle that was never played cannot claim one. This is the leak guard:
    // the driver feeds handles straight off AudioSourceComponent, which keeps a
    // stale soundHandle after a sound finishes.
    ENJIN_EXPECT_TRUE(!engine.SetSourceReflections(12345u, fake));
    ENJIN_EXPECT_EQ(engine.SourceReflectionSlotsInUse(), 0u);

    // Releasing something that holds nothing is a no-op rather than a crash:
    // the driver calls it every frame for sources that walked out of range.
    engine.ReleaseSourceReflections(12345u);
    ENJIN_EXPECT_EQ(engine.SourceReflectionSlotsInUse(), 0u);

    engine.Shutdown();
}

ENJIN_TEST_MAIN()
