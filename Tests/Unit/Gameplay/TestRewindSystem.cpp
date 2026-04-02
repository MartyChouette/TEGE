#include "EnjinTest.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Gameplay/StateRingBuffer.h"
#include "Enjin/Gameplay/RewindChannel.h"
#include "Enjin/ECS/Components/Gameplay.h"

using namespace Enjin;
using namespace Enjin::Gameplay;

// ===========================================================================
// StateRingBuffer
// ===========================================================================

ENJIN_TEST(StateRingBuffer, DefaultEmpty) {
    StateRingBuffer<i32> buf;
    ENJIN_EXPECT_EQ(buf.Count(), 0u);
    ENJIN_EXPECT_EQ(buf.Capacity(), 0u);
    ENJIN_EXPECT_TRUE(buf.Empty());
}

ENJIN_TEST(StateRingBuffer, ReserveAndPush) {
    StateRingBuffer<i32> buf(4);
    ENJIN_EXPECT_EQ(buf.Capacity(), 4u);
    buf.Push(10);
    buf.Push(20);
    buf.Push(30);
    ENJIN_EXPECT_EQ(buf.Count(), 3u);
    ENJIN_EXPECT_FALSE(buf.Empty());
}

ENJIN_TEST(StateRingBuffer, AtAccess) {
    StateRingBuffer<i32> buf(4);
    buf.Push(10);
    buf.Push(20);
    buf.Push(30);
    ENJIN_EXPECT_EQ(buf.At(0), 10);  // oldest
    ENJIN_EXPECT_EQ(buf.At(1), 20);
    ENJIN_EXPECT_EQ(buf.At(2), 30);  // newest
}

ENJIN_TEST(StateRingBuffer, BackAndFront) {
    StateRingBuffer<i32> buf(4);
    buf.Push(10);
    buf.Push(20);
    buf.Push(30);
    ENJIN_EXPECT_EQ(buf.Front(), 10);
    ENJIN_EXPECT_EQ(buf.Back(), 30);
}

ENJIN_TEST(StateRingBuffer, OverwritesOldest) {
    StateRingBuffer<i32> buf(3);
    buf.Push(10);
    buf.Push(20);
    buf.Push(30);
    ENJIN_EXPECT_EQ(buf.Count(), 3u);
    buf.Push(40);  // Overwrites 10
    ENJIN_EXPECT_EQ(buf.Count(), 3u);
    ENJIN_EXPECT_EQ(buf.Front(), 20);  // Oldest is now 20
    ENJIN_EXPECT_EQ(buf.Back(), 40);   // Newest is 40
}

ENJIN_TEST(StateRingBuffer, OverwriteMultiple) {
    StateRingBuffer<i32> buf(3);
    for (i32 i = 0; i < 10; ++i) buf.Push(i);
    ENJIN_EXPECT_EQ(buf.Count(), 3u);
    ENJIN_EXPECT_EQ(buf.At(0), 7);
    ENJIN_EXPECT_EQ(buf.At(1), 8);
    ENJIN_EXPECT_EQ(buf.At(2), 9);
}

ENJIN_TEST(StateRingBuffer, PopBack) {
    StateRingBuffer<i32> buf(5);
    buf.Push(1); buf.Push(2); buf.Push(3); buf.Push(4);
    buf.PopBack(2);
    ENJIN_EXPECT_EQ(buf.Count(), 2u);
    ENJIN_EXPECT_EQ(buf.Back(), 2);
}

ENJIN_TEST(StateRingBuffer, PopBackAll) {
    StateRingBuffer<i32> buf(5);
    buf.Push(1); buf.Push(2);
    buf.PopBack(5);  // Pop more than count
    ENJIN_EXPECT_TRUE(buf.Empty());
    ENJIN_EXPECT_EQ(buf.Count(), 0u);
}

ENJIN_TEST(StateRingBuffer, Clear) {
    StateRingBuffer<i32> buf(5);
    buf.Push(1); buf.Push(2); buf.Push(3);
    buf.Clear();
    ENJIN_EXPECT_TRUE(buf.Empty());
    ENJIN_EXPECT_EQ(buf.Capacity(), 5u);
}

ENJIN_TEST(StateRingBuffer, PushAfterClear) {
    StateRingBuffer<i32> buf(3);
    buf.Push(1); buf.Push(2); buf.Push(3);
    buf.Clear();
    buf.Push(10); buf.Push(20);
    ENJIN_EXPECT_EQ(buf.Count(), 2u);
    ENJIN_EXPECT_EQ(buf.Front(), 10);
    ENJIN_EXPECT_EQ(buf.Back(), 20);
}

ENJIN_TEST(StateRingBuffer, MemoryUsage) {
    StateRingBuffer<i32> buf(100);
    ENJIN_EXPECT_TRUE(buf.MemoryUsage() >= 100 * sizeof(i32));
}

ENJIN_TEST(StateRingBuffer, SingleCapacity) {
    StateRingBuffer<i32> buf(1);
    buf.Push(10);
    ENJIN_EXPECT_EQ(buf.Count(), 1u);
    ENJIN_EXPECT_EQ(buf.Back(), 10);
    buf.Push(20);
    ENJIN_EXPECT_EQ(buf.Count(), 1u);
    ENJIN_EXPECT_EQ(buf.Back(), 20);
}

ENJIN_TEST(StateRingBuffer, MoveSemantics) {
    StateRingBuffer<std::string> buf(3);
    std::string s = "hello";
    buf.Push(std::move(s));
    ENJIN_EXPECT_EQ(buf.Back(), std::string("hello"));
}

// ===========================================================================
// RewindChannel flags
// ===========================================================================

ENJIN_TEST(RewindChannel, DefaultFlags) {
    u32 def = static_cast<u32>(RewindChannelFlags::Default);
    ENJIN_EXPECT_TRUE(HasChannel(def, RewindChannelFlags::Transform));
    ENJIN_EXPECT_TRUE(HasChannel(def, RewindChannelFlags::Velocity));
    ENJIN_EXPECT_TRUE(HasChannel(def, RewindChannelFlags::Health));
    ENJIN_EXPECT_FALSE(HasChannel(def, RewindChannelFlags::Animation));
    ENJIN_EXPECT_FALSE(HasChannel(def, RewindChannelFlags::Physics));
    ENJIN_EXPECT_FALSE(HasChannel(def, RewindChannelFlags::Material));
}

ENJIN_TEST(RewindChannel, AllFlags) {
    u32 all = static_cast<u32>(RewindChannelFlags::All);
    ENJIN_EXPECT_TRUE(HasChannel(all, RewindChannelFlags::Transform));
    ENJIN_EXPECT_TRUE(HasChannel(all, RewindChannelFlags::Animation));
    ENJIN_EXPECT_TRUE(HasChannel(all, RewindChannelFlags::Material));
}

ENJIN_TEST(RewindChannel, CombineFlags) {
    auto combined = RewindChannelFlags::Transform | RewindChannelFlags::Physics;
    u32 mask = static_cast<u32>(combined);
    ENJIN_EXPECT_TRUE(HasChannel(mask, RewindChannelFlags::Transform));
    ENJIN_EXPECT_TRUE(HasChannel(mask, RewindChannelFlags::Physics));
    ENJIN_EXPECT_FALSE(HasChannel(mask, RewindChannelFlags::Health));
}

ENJIN_TEST(RewindChannel, NoneFlag) {
    u32 none = static_cast<u32>(RewindChannelFlags::None);
    ENJIN_EXPECT_FALSE(HasChannel(none, RewindChannelFlags::Transform));
    ENJIN_EXPECT_FALSE(HasChannel(none, RewindChannelFlags::Health));
}

// ===========================================================================
// EntitySnapshot
// ===========================================================================

ENJIN_TEST(RewindChannel, EntitySnapshotDefaults) {
    EntitySnapshot snap;
    ENJIN_EXPECT_EQ(snap.entity, ECS::INVALID_ENTITY);
    ENJIN_EXPECT_EQ(snap.channelMask, 0u);
    ENJIN_EXPECT_TRUE(snap.visible);
    ENJIN_EXPECT_FALSE(snap.isDead);
}

// ===========================================================================
// DeltaFrame
// ===========================================================================

ENJIN_TEST(RewindChannel, DeltaFrameDefaults) {
    DeltaFrame frame;
    ENJIN_EXPECT_FALSE(frame.isKeyframe);
    ENJIN_EXPECT_TRUE(frame.snapshots.empty());
}

ENJIN_TEST(RewindChannel, DeltaFrameKeyframe) {
    DeltaFrame frame;
    frame.isKeyframe = true;
    frame.timestamp = 1.5f;
    EntitySnapshot s;
    s.entity = 42;
    s.channelMask = static_cast<u32>(RewindChannelFlags::Transform);
    s.position = Math::Vector3(1, 2, 3);
    frame.snapshots.push_back(s);
    ENJIN_EXPECT_EQ(frame.snapshots.size(), (size_t)1);
    ENJIN_EXPECT_EQ(frame.snapshots[0].entity, (ECS::Entity)42);
}

// ===========================================================================
// RecordRewindComponent
// ===========================================================================

ENJIN_TEST(RewindComponent, EntityDefaults) {
    ECS::RecordRewindComponent rr;
    ENJIN_EXPECT_TRUE(rr.enabled);
    ENJIN_EXPECT_TRUE(rr.history.Empty());
    ENJIN_EXPECT_FALSE(rr.rewinding);
    ENJIN_EXPECT_EQ(rr.rewindKey, 82);  // R
    ENJIN_EXPECT_TRUE(rr.maxDuration > 0.0f);
}

ENJIN_TEST(RewindComponent, SceneDefaults) {
    ECS::SceneRewindComponent sr;
    ENJIN_EXPECT_TRUE(sr.enabled);
    ENJIN_EXPECT_TRUE(sr.deltaCompression);
    ENJIN_EXPECT_EQ(sr.keyframeInterval, 30u);
    ENJIN_EXPECT_EQ(sr.charges, 0);
    ENJIN_EXPECT_EQ(sr.rewindKey, 84);  // T
}

ENJIN_TEST(RewindComponent, ChannelDefaults) {
    ECS::RecordRewindComponent rr;
    u32 ch = rr.channels;
    ENJIN_EXPECT_TRUE(HasChannel(ch, RewindChannelFlags::Transform));
    ENJIN_EXPECT_TRUE(HasChannel(ch, RewindChannelFlags::Velocity));
    ENJIN_EXPECT_TRUE(HasChannel(ch, RewindChannelFlags::Health));
}

// ===========================================================================
// StateRingBuffer with EntitySnapshot (realistic usage)
// ===========================================================================

ENJIN_TEST(StateRingBuffer, EntitySnapshotRingBuffer) {
    StateRingBuffer<EntitySnapshot> buf(5);
    for (u32 i = 0; i < 8; ++i) {
        EntitySnapshot snap;
        snap.entity = static_cast<ECS::Entity>(i);
        snap.position = Math::Vector3(static_cast<f32>(i), 0, 0);
        buf.Push(std::move(snap));
    }
    ENJIN_EXPECT_EQ(buf.Count(), 5u);
    // Oldest should be entity 3 (0,1,2 were overwritten)
    ENJIN_EXPECT_EQ(buf.Front().entity, (ECS::Entity)3);
    ENJIN_EXPECT_EQ(buf.Back().entity, (ECS::Entity)7);
}

ENJIN_TEST(StateRingBuffer, DeltaFrameRingBuffer) {
    StateRingBuffer<DeltaFrame> buf(10);
    for (u32 i = 0; i < 10; ++i) {
        DeltaFrame f;
        f.timestamp = static_cast<f32>(i) * 0.1f;
        f.isKeyframe = (i % 5 == 0);
        buf.Push(std::move(f));
    }
    ENJIN_EXPECT_EQ(buf.Count(), 10u);
    ENJIN_EXPECT_TRUE(buf.Front().isKeyframe);   // frame 0 is keyframe
    ENJIN_EXPECT_TRUE(buf.At(5).isKeyframe);     // frame 5 is keyframe
    ENJIN_EXPECT_FALSE(buf.At(1).isKeyframe);
}

ENJIN_TEST_MAIN()
