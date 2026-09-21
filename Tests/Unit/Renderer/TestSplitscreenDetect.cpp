// When does a scene split the screen?
//
// The rule lived inline in the desktop Player's frame loop and nowhere else, so
// the web player never split the screen at all -- it drew camera 0 across the
// whole canvas and player two had no view. Moving it into RenderSystem is what
// lets both runtimes ask the same question, and these tests are what stop the
// answer from drifting back apart.
//
// The trigger is narrow on purpose. Two active cameras is NOT enough: a scene
// can hold a spare camera at the default full-screen rect (a cutscene camera, a
// render-target camera, a disabled-then-enabled one), and treating that as
// splitscreen would halve everyone's view for no reason. At least one camera has
// to have been GIVEN a rect.
//
// No GPU: the render system is constructed with a null backend, which is safe
// because Shutdown early-returns when it was never initialised.
#include "EnjinTest.h"
#include "Enjin/ECS/Systems/RenderSystem.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Camera.h"
#include "Enjin/ECS/Components/Transform.h"

using namespace Enjin;

namespace {

// A camera entity with an explicit viewport rect. The defaults (0,0,1,1) are
// what "no rect was authored" looks like.
ECS::Entity MakeCamera(ECS::World& world, f32 x, f32 y, f32 w, f32 h,
                       bool active = true) {
    ECS::Entity e = world.CreateEntity();
    world.AddComponent<ECS::TransformComponent>(e, ECS::TransformComponent{});
    ECS::CameraComponent cc;
    cc.isActive = active;
    cc.viewportX = x;
    cc.viewportY = y;
    cc.viewportWidth = w;
    cc.viewportHeight = h;
    world.AddComponent<ECS::CameraComponent>(e, cc);
    return e;
}

}  // namespace

ENJIN_TEST(SplitscreenDetect, test_a_single_camera_never_splits) {
    // Arrange — even one carrying a rect: there is nobody to share with.
    ECS::World world;
    MakeCamera(world, 0.0f, 0.0f, 0.5f, 1.0f);
    ECS::RenderSystem rs(&world, nullptr);

    // Act
    rs.ApplySplitscreenFromWorld();

    // Assert
    ENJIN_EXPECT_TRUE(rs.GetMainPassViewports().empty());
}

ENJIN_TEST(SplitscreenDetect, test_two_full_screen_cameras_do_not_split) {
    // Arrange — the spare-camera case. Both at the default rect.
    ECS::World world;
    MakeCamera(world, 0.0f, 0.0f, 1.0f, 1.0f);
    MakeCamera(world, 0.0f, 0.0f, 1.0f, 1.0f);
    ECS::RenderSystem rs(&world, nullptr);

    // Act
    rs.ApplySplitscreenFromWorld();

    // Assert
    ENJIN_EXPECT_TRUE(rs.GetMainPassViewports().empty());
}

ENJIN_TEST(SplitscreenDetect, test_two_cameras_with_an_authored_rect_split) {
    // Arrange — a left/right split, the ordinary two-player case.
    ECS::World world;
    ECS::Entity left = MakeCamera(world, 0.0f, 0.0f, 0.5f, 1.0f);
    ECS::Entity right = MakeCamera(world, 0.5f, 0.0f, 0.5f, 1.0f);
    ECS::RenderSystem rs(&world, nullptr);

    // Act
    rs.ApplySplitscreenFromWorld();

    // Assert — both viewports, carrying the rects they were authored with.
    const auto& vps = rs.GetMainPassViewports();
    ENJIN_ASSERT_EQ(vps.size(), 2u);
    ENJIN_EXPECT_EQ(vps[0].entity, left);
    ENJIN_EXPECT_FLOAT_EQ(vps[0].viewportWidth, 0.5f);
    ENJIN_EXPECT_EQ(vps[1].entity, right);
    ENJIN_EXPECT_FLOAT_EQ(vps[1].viewportX, 0.5f);
}

ENJIN_TEST(SplitscreenDetect, test_one_authored_rect_is_enough_to_split) {
    // Arrange — only the second camera was given a rect. The first still gets a
    // viewport entry, because rendering one camera and dropping the other would
    // be worse than rendering both at whatever they say.
    ECS::World world;
    MakeCamera(world, 0.0f, 0.0f, 1.0f, 1.0f);
    MakeCamera(world, 0.0f, 0.5f, 1.0f, 0.5f);
    ECS::RenderSystem rs(&world, nullptr);

    // Act
    rs.ApplySplitscreenFromWorld();

    // Assert
    ENJIN_ASSERT_EQ(rs.GetMainPassViewports().size(), 2u);
}

ENJIN_TEST(SplitscreenDetect, test_an_inactive_camera_is_not_counted) {
    // Arrange — a disabled second camera must not turn on splitscreen, or
    // switching a cutscene camera off would split the screen instead.
    ECS::World world;
    MakeCamera(world, 0.0f, 0.0f, 1.0f, 1.0f);
    MakeCamera(world, 0.0f, 0.0f, 0.5f, 1.0f, /*active=*/false);
    ECS::RenderSystem rs(&world, nullptr);

    // Act
    rs.ApplySplitscreenFromWorld();

    // Assert
    ENJIN_EXPECT_TRUE(rs.GetMainPassViewports().empty());
}

ENJIN_TEST(SplitscreenDetect, test_more_cameras_than_viewports_are_clamped) {
    // Arrange — five quarter-screen cameras against MAX_SPLITSCREEN_VIEWPORTS.
    // The cap is a hard resource limit on the Vulkan side (per-viewport uniform
    // buffers are allocated for exactly that many), so it must not be exceeded.
    ECS::World world;
    for (int i = 0; i < 5; ++i) MakeCamera(world, 0.0f, i * 0.2f, 1.0f, 0.2f);
    ECS::RenderSystem rs(&world, nullptr);

    // Act
    rs.ApplySplitscreenFromWorld();

    // Assert
    ENJIN_EXPECT_EQ(rs.GetMainPassViewports().size(),
                    static_cast<usize>(ECS::RenderSystem::MAX_SPLITSCREEN_VIEWPORTS));
}

ENJIN_TEST(SplitscreenDetect, test_splitscreen_is_cleared_when_the_scene_stops_splitting) {
    // Arrange — the regression that matters most: a previously-set viewport list
    // surviving a scene change means every later frame renders through cameras
    // that no longer exist.
    ECS::World world;
    MakeCamera(world, 0.0f, 0.0f, 0.5f, 1.0f);
    ECS::Entity second = MakeCamera(world, 0.5f, 0.0f, 0.5f, 1.0f);
    ECS::RenderSystem rs(&world, nullptr);
    rs.ApplySplitscreenFromWorld();
    ENJIN_ASSERT_EQ(rs.GetMainPassViewports().size(), 2u);

    // Act — drop back to one camera.
    world.DestroyEntity(second);
    world.Update(0.0f);            // flushes the deferred destroy
    rs.ApplySplitscreenFromWorld();

    // Assert
    ENJIN_EXPECT_TRUE(rs.GetMainPassViewports().empty());
}

ENJIN_TEST_MAIN()
