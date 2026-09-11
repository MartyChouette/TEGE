// The Flash timeline converted one of four animated properties, and ran layers
// that said they were reference-only.
//
// FlashKeyframe carries position, rotation, scale and alpha. The panel lets you
// key all four. ConvertToTimeline emitted a position track and nothing else, so
// a tween authored as a fade or a grow played back as an object sitting still at
// full size -- and nothing said so.
//
// `isGuide` and `isMask` were two checkboxes with no consumer anywhere in the
// engine. isGuide has carried the comment "Guide layer (not rendered at
// runtime)" since it was written, and the conversion ignored it, so a reference
// layer -- a motion path to trace, a photo underlay -- animated its entity in the
// shipped game.
//
// And the closing log said "Converted Flash timeline to N TimelineComponents"
// with N = layers.size(), counting layers the loop had skipped for having no
// entity or no keyframes. It reported work that did not happen.
#include "EnjinTest.h"
#include "Enjin/Editor/FlashTimeline.h"
#include "Enjin/Animation/Timeline.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"
#include <string>

using namespace Enjin;
using namespace Enjin::Editor;

namespace {

// A layer whose keyframes move, grow and fade, so every track that should be
// emitted has something to carry.
FlashTimelineLayer MakeAnimatedLayer(ECS::Entity e, const char* name) {
    FlashTimelineLayer layer;
    layer.name = name;
    layer.entity = e;

    FlashKeyframe a;
    a.frameIndex = 0;
    a.position = Math::Vector3(0.0f, 0.0f, 0.0f);
    a.scale = Math::Vector3(1.0f, 1.0f, 1.0f);
    a.alpha = 1.0f;
    layer.keyframes.push_back(a);

    FlashKeyframe b;
    b.frameIndex = 24;
    b.position = Math::Vector3(10.0f, 0.0f, 0.0f);
    b.scale = Math::Vector3(2.0f, 2.0f, 2.0f);
    b.alpha = 0.0f;
    layer.keyframes.push_back(b);
    return layer;
}

ECS::Entity MakeEntity(ECS::World& world) {
    ECS::Entity e = world.CreateEntity();
    world.AddComponent<ECS::TransformComponent>(e);
    return e;
}

bool HasTrack(const Animation::TimelineComponent& tl, const char* prop) {
    for (const auto& t : tl.propertyTracks) {
        if (t.targetProperty == prop) return true;
    }
    return false;
}

usize TrackCount(const Animation::TimelineComponent& tl, const char* prop) {
    usize n = 0;
    for (const auto& t : tl.propertyTracks) {
        if (t.targetProperty == prop) ++n;
    }
    return n;
}

} // namespace

// ---------------------------------------------------------------------------
// The dropped properties
// ---------------------------------------------------------------------------

ENJIN_TEST(FlashTimelineConvert, ScaleAndAlphaAreConvertedNotJustPosition) {
    // Arrange
    ECS::World world;
    ECS::Entity e = MakeEntity(world);

    FlashTimelineData data;
    data.frameRate = 24.0f;
    data.totalFrames = 48;
    data.layers.push_back(MakeAnimatedLayer(e, "Mover"));

    FlashTimelineEditor editor;
    editor.SetTimeline(&data);

    // Act
    editor.ConvertToTimeline(&world);

    // Assert
    auto* tl = world.GetComponent<Animation::TimelineComponent>(e);
    ENJIN_ASSERT_NOT_NULL(tl);
    ENJIN_EXPECT_TRUE(HasTrack(*tl, "position"));
    // The regression: these two were never emitted.
    ENJIN_EXPECT_TRUE(HasTrack(*tl, "scale"));
    ENJIN_EXPECT_TRUE(HasTrack(*tl, "material.opacity"));
}

ENJIN_TEST(FlashTimelineConvert, AConstantPropertyGetsNoTrack) {
    // A track that never changes still writes its value every frame, which
    // overwrites whatever else set that property -- that is how a timeline ends
    // up fighting a script over the same transform. Only what animates is
    // emitted.
    ECS::World world;
    ECS::Entity e = MakeEntity(world);

    FlashTimelineLayer layer;
    layer.entity = e;
    FlashKeyframe a; a.frameIndex = 0;  a.position = Math::Vector3(0.0f);
    FlashKeyframe b; b.frameIndex = 24; b.position = Math::Vector3(5.0f, 0.0f, 0.0f);
    // scale and alpha left at their defaults on both, so neither varies.
    layer.keyframes.push_back(a);
    layer.keyframes.push_back(b);

    FlashTimelineData data;
    data.layers.push_back(layer);

    FlashTimelineEditor editor;
    editor.SetTimeline(&data);
    editor.ConvertToTimeline(&world);

    auto* tl = world.GetComponent<Animation::TimelineComponent>(e);
    ENJIN_ASSERT_NOT_NULL(tl);
    ENJIN_EXPECT_TRUE(HasTrack(*tl, "position"));
    ENJIN_EXPECT_FALSE(HasTrack(*tl, "scale"));
    ENJIN_EXPECT_FALSE(HasTrack(*tl, "material.opacity"));
}

ENJIN_TEST(FlashTimelineConvert, RotationIsLeftOutOnPurpose) {
    // Timeline's only rotation properties are rotation.x/y/z, and they assign
    // straight into TransformComponent::rotation, which is a QUATERNION. A track
    // carrying euler degrees would write 90 into a quaternion component and
    // produce a rotation nobody asked for. Not converting is the correct answer
    // until that is fixed: a dropped feature beats a corrupted one.
    ECS::World world;
    ECS::Entity e = MakeEntity(world);

    FlashTimelineLayer layer = MakeAnimatedLayer(e, "Spinner");
    layer.keyframes[1].rotation = Math::Vector3(0.0f, 90.0f, 0.0f);

    FlashTimelineData data;
    data.layers.push_back(layer);

    FlashTimelineEditor editor;
    editor.SetTimeline(&data);
    editor.ConvertToTimeline(&world);

    auto* tl = world.GetComponent<Animation::TimelineComponent>(e);
    ENJIN_ASSERT_NOT_NULL(tl);
    ENJIN_EXPECT_FALSE(HasTrack(*tl, "rotation"));
    ENJIN_EXPECT_FALSE(HasTrack(*tl, "rotation.x"));
    ENJIN_EXPECT_FALSE(HasTrack(*tl, "rotation.y"));
    ENJIN_EXPECT_FALSE(HasTrack(*tl, "rotation.z"));
}

// ---------------------------------------------------------------------------
// Guide layers
// ---------------------------------------------------------------------------

ENJIN_TEST(FlashTimelineConvert, AGuideLayerDoesNotReachTheGame) {
    // "Guide layer (not rendered at runtime)" has been the comment on this flag
    // since it was written, and the conversion ignored it entirely.
    ECS::World world;
    ECS::Entity guide = MakeEntity(world);
    ECS::Entity real = MakeEntity(world);

    FlashTimelineLayer guideLayer = MakeAnimatedLayer(guide, "Reference");
    guideLayer.isGuide = true;

    FlashTimelineData data;
    data.layers.push_back(guideLayer);
    data.layers.push_back(MakeAnimatedLayer(real, "Character"));

    FlashTimelineEditor editor;
    editor.SetTimeline(&data);
    editor.ConvertToTimeline(&world);

    // The guide's entity gets nothing at all.
    ENJIN_EXPECT_NULL(world.GetComponent<Animation::TimelineComponent>(guide));
    // The real layer is unaffected by the guide being there.
    ENJIN_EXPECT_NOT_NULL(world.GetComponent<Animation::TimelineComponent>(real));
}

ENJIN_TEST(FlashTimelineConvert, AMaskLayerStillAnimates) {
    // A mask marks intent; the clipping needs a stencil pass the renderer does
    // not have. Skipping the layer would be worse than not clipping it -- the
    // mask shape would stop moving as well, and the author would see two things
    // broken instead of one that is documented.
    ECS::World world;
    ECS::Entity e = MakeEntity(world);

    FlashTimelineLayer layer = MakeAnimatedLayer(e, "Masker");
    layer.isMask = true;

    FlashTimelineData data;
    data.layers.push_back(layer);

    FlashTimelineEditor editor;
    editor.SetTimeline(&data);
    editor.ConvertToTimeline(&world);

    ENJIN_EXPECT_NOT_NULL(world.GetComponent<Animation::TimelineComponent>(e));
}

// ---------------------------------------------------------------------------
// Not converting twice
// ---------------------------------------------------------------------------

ENJIN_TEST(FlashTimelineConvert, TheTimelineDurationComesFromTheFrameRate) {
    ECS::World world;
    ECS::Entity e = MakeEntity(world);

    FlashTimelineData data;
    data.frameRate = 24.0f;
    data.totalFrames = 48;       // two seconds
    data.layers.push_back(MakeAnimatedLayer(e, "Mover"));

    FlashTimelineEditor editor;
    editor.SetTimeline(&data);
    editor.ConvertToTimeline(&world);

    auto* tl = world.GetComponent<Animation::TimelineComponent>(e);
    ENJIN_ASSERT_NOT_NULL(tl);
    ENJIN_EXPECT_FLOAT_NEAR(tl->duration, 2.0f, 0.0001f);

    // And a keyframe at frame 24 lands at one second, not at frame 24.
    for (const auto& t : tl->propertyTracks) {
        if (t.targetProperty != "position") continue;
        ENJIN_ASSERT_TRUE(t.keyframes.size() == 2);
        ENJIN_EXPECT_FLOAT_NEAR(t.keyframes[0].time, 0.0f, 0.0001f);
        ENJIN_EXPECT_FLOAT_NEAR(t.keyframes[1].time, 1.0f, 0.0001f);
    }
}

ENJIN_TEST(FlashTimelineConvert, ALayerWithNoEntityIsSkippedAndNothingElseBreaks) {
    // These are the layers the old count included in its total, reporting work
    // it had skipped.
    ECS::World world;
    ECS::Entity real = MakeEntity(world);

    FlashTimelineLayer orphan = MakeAnimatedLayer(0, "No entity");
    FlashTimelineLayer empty;
    empty.name = "No keyframes";
    empty.entity = MakeEntity(world);

    FlashTimelineData data;
    data.layers.push_back(orphan);
    data.layers.push_back(empty);
    data.layers.push_back(MakeAnimatedLayer(real, "Character"));

    FlashTimelineEditor editor;
    editor.SetTimeline(&data);
    editor.ConvertToTimeline(&world);

    ENJIN_EXPECT_NOT_NULL(world.GetComponent<Animation::TimelineComponent>(real));
    ENJIN_EXPECT_NULL(world.GetComponent<Animation::TimelineComponent>(empty.entity));
}

ENJIN_TEST(FlashTimelineConvert, ConvertingTwiceDoesNotStackDuplicateTracks) {
    // Pressing the button again is something people do. Each press appended a
    // fresh position track to the same component, so the second conversion left
    // two tracks writing the same property every frame.
    ECS::World world;
    ECS::Entity e = MakeEntity(world);

    FlashTimelineData data;
    data.layers.push_back(MakeAnimatedLayer(e, "Mover"));

    FlashTimelineEditor editor;
    editor.SetTimeline(&data);
    editor.ConvertToTimeline(&world);
    const usize afterFirst = TrackCount(*world.GetComponent<Animation::TimelineComponent>(e),
                                        "position");
    editor.ConvertToTimeline(&world);
    const usize afterSecond = TrackCount(*world.GetComponent<Animation::TimelineComponent>(e),
                                         "position");

    ENJIN_ASSERT_EQ(afterFirst, static_cast<usize>(1));
    ENJIN_EXPECT_EQ(afterSecond, static_cast<usize>(1));
}

ENJIN_TEST_MAIN()
