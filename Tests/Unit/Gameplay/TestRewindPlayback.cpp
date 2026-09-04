// Hold-to-rewind must play back at rewindSpeed, not accelerate.
//
// rewindPlayhead accumulated as an offset from currentRecordedTime, and then
// the pop loop moved currentRecordedTime -- the very anchor the offset was
// measured against. So the offset compounded and a five second buffer emptied
// in a fraction of a second.
//
// Underneath that: EntitySnapshot had no timestamp, so frame times were
// reconstructed from index arithmetic in three places, one of which read
// position.x under a comment calling it a timestamp. Scene rewind pops by
// DeltaFrame::timestamp and was correct, which is what made the entity path's
// drift look like a mystery rather than a missing field.
//
// These tests drive the real hold path through Input's replay injection, so
// they exercise the same code a held key does.
#include "EnjinTest.h"
#include "Enjin/Gameplay/RecordRewindSystem.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/ECS/Components/Material.h"
#include "Enjin/Platform/Input.h"

using namespace Enjin;

namespace {

constexpr i32 kRewindKey = 82;   // 'R', the RecordRewindComponent default

// Hold (or release) the rewind key for the frames that follow.
void HoldRewindKey(bool held) {
    bool keys[512] = {};
    bool mouse[8] = {};
    keys[kRewindKey] = held;
    Input::InjectFrameState(keys, mouse, Math::Vector2(0.0f, 0.0f));
    Input::Update();
}

struct Recorder {
    ECS::World world;
    ECS::Entity entity = ECS::INVALID_ENTITY;
    Gameplay::RecordRewindSystem sys;

    Recorder() {
        Input::SetReplayInjection(true);
        entity = world.CreateEntity();
        world.AddComponent<ECS::TransformComponent>(entity);
        world.AddComponent<ECS::RecordRewindComponent>(entity);
        sys.SetWorld(&world);
    }
    ~Recorder() {
        Input::SetReplayInjection(false);
    }

    f32 X() const {
        return world.GetComponent<ECS::TransformComponent>(entity)->position.x;
    }
    usize Frames() const {
        return world.GetComponent<ECS::RecordRewindComponent>(entity)->history.Count();
    }

    // Record `steps` snapshots with position.x tracking the step index.
    void Record(int steps, f32 dt) {
        HoldRewindKey(false);
        for (int i = 0; i < steps; ++i) {
            world.GetComponent<ECS::TransformComponent>(entity)->position.x =
                static_cast<f32>(i);
            sys.Update(dt);
        }
    }
};

} // namespace

ENJIN_TEST(RewindPlayback, HoldingRewindConsumesHistoryAtRewindSpeed) {
    // Arrange: 40 frames at 20 Hz, so two seconds of history, and a rewind
    // speed of 1 so one second of holding should consume one second of it.
    Recorder r;
    const f32 dt = 1.0f / 20.0f;
    r.world.GetComponent<ECS::RecordRewindComponent>(r.entity)->rewindSpeed = 1.0f;
    r.Record(40, dt);
    const usize recorded = r.Frames();
    ENJIN_ASSERT_TRUE(recorded >= 39);

    // Act: hold for one second of frames.
    for (int i = 0; i < 20; ++i) {
        HoldRewindKey(true);
        r.sys.Update(dt);
    }

    // Assert: about half the buffer is gone, not all of it. The compounding
    // anchor drained the whole thing in a handful of frames.
    const usize left = r.Frames();
    ENJIN_EXPECT_TRUE(left > recorded / 4);
    ENJIN_EXPECT_TRUE(left < recorded);
}

ENJIN_TEST(RewindPlayback, DoubleSpeedConsumesRoughlyTwiceAsMuch) {
    // Arrange: the same hold at 1x and at 2x. If the playhead compounds, both
    // empty the buffer and the two are indistinguishable -- which is exactly
    // what the bug looked like.
    const f32 dt = 1.0f / 20.0f;

    usize leftAt1x = 0, leftAt2x = 0;
    {
        Recorder r;
        r.world.GetComponent<ECS::RecordRewindComponent>(r.entity)->rewindSpeed = 1.0f;
        r.Record(60, dt);
        for (int i = 0; i < 10; ++i) { HoldRewindKey(true); r.sys.Update(dt); }
        leftAt1x = r.Frames();
    }
    {
        Recorder r;
        r.world.GetComponent<ECS::RecordRewindComponent>(r.entity)->rewindSpeed = 2.0f;
        r.Record(60, dt);
        for (int i = 0; i < 10; ++i) { HoldRewindKey(true); r.sys.Update(dt); }
        leftAt2x = r.Frames();
    }

    // Assert: faster rewind leaves strictly less history behind.
    ENJIN_EXPECT_TRUE(leftAt2x < leftAt1x);
}

ENJIN_TEST(RewindPlayback, RewindingWalksThePositionBackwards) {
    // Arrange
    Recorder r;
    const f32 dt = 1.0f / 20.0f;
    r.world.GetComponent<ECS::RecordRewindComponent>(r.entity)->rewindSpeed = 1.0f;
    r.Record(40, dt);

    // Act: a few frames of holding.
    for (int i = 0; i < 5; ++i) { HoldRewindKey(true); r.sys.Update(dt); }
    const f32 afterFive = r.X();
    for (int i = 0; i < 5; ++i) { HoldRewindKey(true); r.sys.Update(dt); }
    const f32 afterTen = r.X();

    // Assert: monotonically back down the recorded ramp, and not straight to
    // the start of the buffer.
    ENJIN_EXPECT_TRUE(afterFive < 39.0f);
    ENJIN_EXPECT_TRUE(afterTen < afterFive);
    ENJIN_EXPECT_TRUE(afterTen > 0.0f);
}

ENJIN_TEST(RewindPlayback, SnapshotsCarryTheirOwnTimestamp) {
    // Arrange / Act: the field whose absence forced the index arithmetic.
    Recorder r;
    r.Record(10, 1.0f / 20.0f);

    // Assert: timestamps increase with the recording, and the newest is the
    // anchor the playhead measures from.
    auto* rr = r.world.GetComponent<ECS::RecordRewindComponent>(r.entity);
    ENJIN_ASSERT_TRUE(rr->history.Count() >= 2);
    const f32 first = rr->history.At(0).timestamp;
    const f32 last = rr->history.Back().timestamp;
    ENJIN_EXPECT_TRUE(last > first);
}

ENJIN_TEST(RewindPlayback, ThePhysicsChannelActuallyRecordsVelocity) {
    // Arrange: the Physics channel was declared in RewindChannelFlags, drawn in
    // the inspector and serialized, and captured by nothing -- while
    // RestoreEntitySnapshot read the two fields it should have filled. So every
    // restore pushed zeroed velocity into the body.
    ECS::World world;
    const ECS::Entity e = world.CreateEntity();
    world.AddComponent<ECS::TransformComponent>(e);
    ECS::RigidbodyComponent rb;
    rb.velocity = Math::Vector3(3.0f, 4.0f, 5.0f);
    rb.angularVelocity = Math::Vector3(0.0f, 1.0f, 0.0f);
    world.AddComponent<ECS::RigidbodyComponent>(e, rb);

    ECS::RecordRewindComponent rr;
    rr.channels = static_cast<u32>(Gameplay::RewindChannelFlags::Transform) |
                  static_cast<u32>(Gameplay::RewindChannelFlags::Physics);
    world.AddComponent<ECS::RecordRewindComponent>(e, rr);

    Gameplay::RecordRewindSystem sys;
    sys.SetWorld(&world);

    // Act
    for (int i = 0; i < 5; ++i) sys.Update(1.0f / 20.0f);

    // Assert
    auto* rec = world.GetComponent<ECS::RecordRewindComponent>(e);
    ENJIN_ASSERT_TRUE(rec->history.Count() >= 1);
    const auto& snap = rec->history.Back();
    ENJIN_EXPECT_TRUE(snap.linearVelocity.x == 3.0f);
    ENJIN_EXPECT_TRUE(snap.linearVelocity.y == 4.0f);
    ENJIN_EXPECT_TRUE(snap.angularVelocity.y == 1.0f);
}

ENJIN_TEST(RewindPlayback, TheMaterialChannelActuallyRecordsColour) {
    // Arrange: same shape as the physics channel -- a checkbox for a channel
    // nothing captured.
    ECS::World world;
    const ECS::Entity e = world.CreateEntity();
    world.AddComponent<ECS::TransformComponent>(e);
    ECS::MaterialComponent mat;
    mat.opacity = 0.25f;
    mat.baseColor = Math::Vector3(0.1f, 1.0f, 0.8f);
    world.AddComponent<ECS::MaterialComponent>(e, mat);

    ECS::RecordRewindComponent rr;
    rr.channels = static_cast<u32>(Gameplay::RewindChannelFlags::Transform) |
                  static_cast<u32>(Gameplay::RewindChannelFlags::Material);
    world.AddComponent<ECS::RecordRewindComponent>(e, rr);

    Gameplay::RecordRewindSystem sys;
    sys.SetWorld(&world);

    // Act
    for (int i = 0; i < 5; ++i) sys.Update(1.0f / 20.0f);

    // Assert
    auto* rec = world.GetComponent<ECS::RecordRewindComponent>(e);
    ENJIN_ASSERT_TRUE(rec->history.Count() >= 1);
    const auto& snap = rec->history.Back();
    ENJIN_EXPECT_TRUE(snap.opacity == 0.25f);
    ENJIN_EXPECT_TRUE(snap.baseColor.y == 1.0f);
}

ENJIN_TEST_MAIN()
