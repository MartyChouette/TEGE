// Keeping a measurement of the room the listener is standing in.
//
// The tracing is not cheap and does not need to be frequent. A room does not
// change while you walk across it. So almost everything here is a decision
// about WHEN to spend that cost, and those decisions are exactly what fails
// silently: a system that retraced every frame would be correct and unusable,
// and one that never retraced would be cheap and wrong. Both look identical
// from the outside until you profile it or walk into another room.

#include "EnjinTest.h"
#include "Enjin/Acoustics/AcousticsSystem.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Material.h"
#include "Enjin/ECS/Components/Gameplay.h"

#include <cmath>

using namespace Enjin;
using namespace Enjin::Acoustics;
using namespace Enjin::ECS;
using Enjin::Math::Vector3;

namespace {

// Six box colliders making a sealed room, which is what a level actually
// contains -- the tracer sees colliders, not a tidy mesh.
void BuildRoom(World& w, const Vector3& centre, f32 size, SurfaceMaterial surface) {
    const f32 h = size * 0.5f;
    const f32 t = 0.5f;
    struct Wall { Vector3 offset, extent; };
    const Wall walls[6] = {
        {{0, -h, 0}, {size, t, size}},   // floor
        {{0,  h, 0}, {size, t, size}},   // ceiling
        {{-h, 0, 0}, {t, size, size}},
        {{ h, 0, 0}, {t, size, size}},
        {{0, 0, -h}, {size, size, t}},
        {{0, 0,  h}, {size, size, t}},
    };
    for (const Wall& wall : walls) {
        Entity e = w.CreateEntity();
        auto& xf = w.AddComponent<TransformComponent>(e);
        xf.position = Vector3(centre.x + wall.offset.x, centre.y + wall.offset.y,
                              centre.z + wall.offset.z);
        w.AddComponent<BoxColliderComponent>(e).size = wall.extent;
        w.AddComponent<MaterialComponent>(e).surfaceMaterial = surface;
    }
}

// Settings that trace quickly, so a test that runs a hundred updates does not
// take a minute.
AcousticsSettings FastSettings() {
    AcousticsSettings s;
    s.trace.rayCount = 128;
    // Bounces are NOT cut down here, and that is deliberate. Setting them to 24
    // made a tiled room measure 0.61 s and a carpeted one 0.69 s -- backwards --
    // because tile absorbs 1% per bounce, so a ray still holds 79% of its energy
    // after twenty-four of them and the decay curve was being cut off while
    // still loud. The tracer reports that case as unreliable now, but the right
    // fix for a test is to give it the bounces, not to lower the bar.
    return s;
}

} // namespace

ENJIN_TEST(AcousticsSystem, WithNoWorldNothingHappens) {
    // Arrange / Act
    AcousticsSystem system;
    system.Update(Vector3(0, 0, 0), 1.0f);

    // Assert: not a crash, and no claim to have measured anything.
    ENJIN_EXPECT_FALSE(system.HasMeasurement());
    ENJIN_EXPECT_EQ(system.TraceCount(), (u32)0);
}

ENJIN_TEST(AcousticsSystem, AnEmptySceneHasNoRoomAndSaysSo) {
    // Arrange
    World w;
    AcousticsSystem system;
    system.SetWorld(&w);
    system.SetSettings(FastSettings());

    // Act: enough updates for any rebuild to happen.
    for (u32 i = 0; i < 5; ++i) system.Update(Vector3(0, 0, 0), 1.0f);

    // Assert: no measurement, rather than a made-up one. An empty scene has no
    // room in it, and the caller falls back to whatever a person authored.
    ENJIN_EXPECT_FALSE(system.HasMeasurement());
}

ENJIN_TEST(AcousticsSystem, ARoomGetsMeasured) {
    // Arrange
    World w;
    BuildRoom(w, Vector3(0, 0, 0), 10.0f, SurfaceMaterial::Concrete);
    AcousticsSystem system;
    system.SetWorld(&w);
    system.SetSettings(FastSettings());

    // Act: one update to rebuild, one to trace.
    system.Update(Vector3(0, 0, 0), 2.0f);
    system.Update(Vector3(0, 0, 0), 0.016f);

    // Assert
    ENJIN_EXPECT_TRUE(system.HasMeasurement());
    ENJIN_EXPECT_TRUE(system.Measurement().rt60[1] > 0.0f);
    ENJIN_EXPECT_EQ(system.RebuildCount(), (u32)1);
    ENJIN_EXPECT_EQ(system.TraceCount(), (u32)1);
}

ENJIN_TEST(AcousticsSystem, ARebuildAndATraceNeverHappenOnTheSameUpdate) {
    // Arrange
    World w;
    BuildRoom(w, Vector3(0, 0, 0), 10.0f, SurfaceMaterial::Concrete);
    AcousticsSystem system;
    system.SetWorld(&w);
    system.SetSettings(FastSettings());

    // Act
    system.Update(Vector3(0, 0, 0), 2.0f);

    // Assert: collecting every collider and then tracing thousands of rays
    // through the result is two expensive things. Doing them on the same frame
    // makes one visible hitch out of two invisible ones.
    ENJIN_EXPECT_EQ(system.RebuildCount(), (u32)1);
    ENJIN_EXPECT_EQ(system.TraceCount(), (u32)0);
}

ENJIN_TEST(AcousticsSystem, StandingStillDoesNotRetrace) {
    // Arrange
    World w;
    BuildRoom(w, Vector3(0, 0, 0), 10.0f, SurfaceMaterial::Concrete);
    AcousticsSystem system;
    system.SetWorld(&w);
    system.SetSettings(FastSettings());
    system.Update(Vector3(0, 0, 0), 2.0f);
    system.Update(Vector3(0, 0, 0), 0.016f);
    ENJIN_ASSERT_EQ(system.TraceCount(), (u32)1);

    // Act: a second of frames without moving.
    for (u32 i = 0; i < 60; ++i) system.Update(Vector3(0, 0, 0), 0.016f);

    // Assert: still one. A room does not change while you stand in it, and a
    // retrace per frame would be correct and unusable.
    ENJIN_EXPECT_EQ(system.TraceCount(), (u32)1);
}

ENJIN_TEST(AcousticsSystem, WalkingIntoAnotherRoomRetraces) {
    // Arrange
    World w;
    BuildRoom(w, Vector3(0, 0, 0), 10.0f, SurfaceMaterial::Concrete);
    AcousticsSystem system;
    system.SetWorld(&w);
    AcousticsSettings settings = FastSettings();
    settings.retraceDistance = 3.0f;
    system.SetSettings(settings);
    system.Update(Vector3(0, 0, 0), 2.0f);
    system.Update(Vector3(0, 0, 0), 0.016f);
    ENJIN_ASSERT_EQ(system.TraceCount(), (u32)1);

    // Act: a small step, then a real one.
    system.Update(Vector3(1, 0, 0), 0.016f);
    ENJIN_EXPECT_EQ(system.TraceCount(), (u32)1);
    system.Update(Vector3(4, 0, 0), 0.016f);

    // Assert
    ENJIN_EXPECT_EQ(system.TraceCount(), (u32)2);
    ENJIN_EXPECT_FLOAT_NEAR(system.MeasuredAt().x, 4.0f, 0.001f);
}

ENJIN_TEST(AcousticsSystem, AMeasurementAgesOutEvenIfNobodyMoves) {
    // Arrange
    World w;
    BuildRoom(w, Vector3(0, 0, 0), 10.0f, SurfaceMaterial::Concrete);
    AcousticsSystem system;
    system.SetWorld(&w);
    AcousticsSettings settings = FastSettings();
    settings.retraceInterval = 1.0f;
    system.SetSettings(settings);
    system.Update(Vector3(0, 0, 0), 2.0f);
    system.Update(Vector3(0, 0, 0), 0.016f);
    ENJIN_ASSERT_EQ(system.TraceCount(), (u32)1);

    // Act: time passes, nobody moves.
    system.Update(Vector3(0, 0, 0), 1.5f);

    // Assert: distance cannot see a door opening or a wall being carved away,
    // so a measurement also ages out.
    ENJIN_EXPECT_EQ(system.TraceCount(), (u32)2);
}

ENJIN_TEST(AcousticsSystem, ChangedGeometryInvalidatesTheMeasurement) {
    // Arrange
    World w;
    BuildRoom(w, Vector3(0, 0, 0), 10.0f, SurfaceMaterial::Concrete);
    AcousticsSystem system;
    system.SetWorld(&w);
    system.SetSettings(FastSettings());
    system.Update(Vector3(0, 0, 0), 2.0f);
    system.Update(Vector3(0, 0, 0), 0.016f);
    ENJIN_ASSERT_TRUE(system.HasMeasurement());

    // Act: carve the room open and say so.
    system.MarkGeometryDirty();
    system.Update(Vector3(0, 0, 0), 2.0f);   // rebuild
    system.Update(Vector3(0, 0, 0), 0.016f); // retrace

    // Assert: the room changed shape, so the old measurement described somewhere
    // that no longer exists.
    ENJIN_EXPECT_EQ(system.RebuildCount(), (u32)2);
    ENJIN_EXPECT_EQ(system.TraceCount(), (u32)2);
}

ENJIN_TEST(AcousticsSystem, MarkingGeometryDirtyEveryFrameDoesNotRebuildEveryFrame) {
    // Arrange: something that dirties the scene constantly -- a physics object
    // settling, a door swinging -- must not turn a BVH build into a per-frame
    // cost.
    World w;
    BuildRoom(w, Vector3(0, 0, 0), 10.0f, SurfaceMaterial::Concrete);
    AcousticsSystem system;
    system.SetWorld(&w);
    AcousticsSettings settings = FastSettings();
    settings.rebuildInterval = 1.0f;
    system.SetSettings(settings);

    // Act: two seconds of frames, dirtied every one.
    for (u32 i = 0; i < 120; ++i) {
        system.MarkGeometryDirty();
        system.Update(Vector3(0, 0, 0), 1.0f / 60.0f);
    }

    // Assert: rate limited to roughly one rebuild per interval, not 120.
    ENJIN_EXPECT_TRUE(system.RebuildCount() <= 3);
    ENJIN_EXPECT_TRUE(system.RebuildCount() >= 1);
}

// The acceptance criterion, through the system a game would actually use.
ENJIN_TEST(AcousticsSystem, TwoRoomsOfDifferentMaterialsMeasureDifferently) {
    // Arrange: a tiled room and a carpeted one, same size, in one world. This
    // is "the main floor and the basement" -- no parameter is set by hand.
    World w;
    BuildRoom(w, Vector3(0, 0, 0), 10.0f, SurfaceMaterial::Tile);
    BuildRoom(w, Vector3(100, 0, 0), 10.0f, SurfaceMaterial::Carpet);

    AcousticsSystem system;
    system.SetWorld(&w);
    AcousticsSettings settings = FastSettings();
    settings.trace.rayCount = 512;
    system.SetSettings(settings);

    // Act: stand in the tiled room.
    system.Update(Vector3(0, 0, 0), 2.0f);
    system.Update(Vector3(0, 0, 0), 0.016f);
    ENJIN_ASSERT_TRUE(system.HasMeasurement());
    const f32 tiled = system.Measurement().rt60[1];

    // Walk to the carpeted one.
    system.Update(Vector3(100, 0, 0), 0.016f);
    ENJIN_ASSERT_TRUE(system.HasMeasurement());
    const f32 carpeted = system.Measurement().rt60[1];

    // Assert
    std::printf("    tiled room %.2f s, carpeted room %.2f s\n", tiled, carpeted);
    ENJIN_EXPECT_TRUE(tiled > carpeted * 2.0f);
}

ENJIN_TEST(AcousticsSystem, ChangingWorldsForgetsTheOldOne) {
    // Arrange
    World a, b;
    BuildRoom(a, Vector3(0, 0, 0), 10.0f, SurfaceMaterial::Concrete);
    AcousticsSystem system;
    system.SetWorld(&a);
    system.SetSettings(FastSettings());
    system.Update(Vector3(0, 0, 0), 2.0f);
    system.Update(Vector3(0, 0, 0), 0.016f);
    ENJIN_ASSERT_TRUE(system.HasMeasurement());

    // Act: load another level.
    system.SetWorld(&b);

    // Assert: a measurement of the previous level would be the last room you
    // stood in following you into the next one.
    ENJIN_EXPECT_FALSE(system.HasMeasurement());
}

ENJIN_TEST_MAIN()
