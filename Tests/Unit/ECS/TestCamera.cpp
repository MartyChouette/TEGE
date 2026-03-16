#include "EnjinTest.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Camera.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/Math/Math.h"

using namespace Enjin;
using namespace Enjin::ECS;
using namespace Enjin::Math;

// ===========================================================================
// CameraComponent Defaults
// ===========================================================================

ENJIN_TEST(CameraDefaults, ProjectionType) {
    CameraComponent cam;
    ENJIN_EXPECT_EQ((int)cam.projectionType, (int)ProjectionType::Perspective);
}

ENJIN_TEST(CameraDefaults, FieldOfView) {
    CameraComponent cam;
    ENJIN_EXPECT_FLOAT_EQ(cam.fieldOfView, 60.0f);
}

ENJIN_TEST(CameraDefaults, ClipPlanes) {
    CameraComponent cam;
    ENJIN_EXPECT_FLOAT_EQ(cam.nearPlane, 0.1f);
    ENJIN_EXPECT_FLOAT_EQ(cam.farPlane, 1000.0f);
}

ENJIN_TEST(CameraDefaults, OrthoSize) {
    CameraComponent cam;
    ENJIN_EXPECT_FLOAT_EQ(cam.orthoSize, 10.0f);
}

ENJIN_TEST(CameraDefaults, Priority) {
    CameraComponent cam;
    ENJIN_EXPECT_EQ(cam.priority, 0);
    ENJIN_EXPECT_TRUE(cam.isActive);
}

ENJIN_TEST(CameraDefaults, ClearSettings) {
    CameraComponent cam;
    ENJIN_EXPECT_TRUE(cam.clearDepth);
    ENJIN_EXPECT_TRUE(cam.clearColor);
}

ENJIN_TEST(CameraDefaults, Viewport) {
    CameraComponent cam;
    ENJIN_EXPECT_FLOAT_EQ(cam.viewportX, 0.0f);
    ENJIN_EXPECT_FLOAT_EQ(cam.viewportY, 0.0f);
    ENJIN_EXPECT_FLOAT_EQ(cam.viewportWidth, 1.0f);
    ENJIN_EXPECT_FLOAT_EQ(cam.viewportHeight, 1.0f);
}

ENJIN_TEST(CameraDefaults, CullingMask) {
    CameraComponent cam;
    ENJIN_EXPECT_EQ(cam.cullingMask, 0xFFFFFFFFu);
}

// ===========================================================================
// AspectRatio Helper
// ===========================================================================

ENJIN_TEST(CameraAspect, StandardScreen) {
    CameraComponent cam;
    f32 aspect = cam.GetAspectRatio(1920, 1080);
    ENJIN_EXPECT_FLOAT_NEAR(aspect, 16.0f / 9.0f, 0.01f);
}

ENJIN_TEST(CameraAspect, SquareScreen) {
    CameraComponent cam;
    f32 aspect = cam.GetAspectRatio(512, 512);
    ENJIN_EXPECT_FLOAT_NEAR(aspect, 1.0f, 0.01f);
}

ENJIN_TEST(CameraAspect, PartialViewport) {
    CameraComponent cam;
    cam.viewportWidth = 0.5f;
    cam.viewportHeight = 1.0f;
    f32 aspect = cam.GetAspectRatio(1920, 1080);
    // width = 0.5 * 1920 = 960, height = 1080
    ENJIN_EXPECT_FLOAT_NEAR(aspect, 960.0f / 1080.0f, 0.01f);
}

ENJIN_TEST(CameraAspect, ZeroHeightReturnOne) {
    CameraComponent cam;
    cam.viewportHeight = 0.0f;
    f32 aspect = cam.GetAspectRatio(1920, 1080);
    ENJIN_EXPECT_FLOAT_EQ(aspect, 1.0f);
}

// ===========================================================================
// Camera Presets
// ===========================================================================

ENJIN_TEST(CameraPresets, Isometric45) {
    auto result = ApplyCameraPreset(CameraPreset::Isometric45);
    ENJIN_EXPECT_EQ((int)result.camera.projectionType, (int)ProjectionType::Orthographic);
    ENJIN_EXPECT_FLOAT_EQ(result.camera.orthoSize, 10.0f);
    ENJIN_EXPECT_FLOAT_EQ(result.rotation.x, -45.0f);
    ENJIN_EXPECT_FLOAT_EQ(result.rotation.y, 45.0f);
}

ENJIN_TEST(CameraPresets, TopDown) {
    auto result = ApplyCameraPreset(CameraPreset::TopDown);
    ENJIN_EXPECT_EQ((int)result.camera.projectionType, (int)ProjectionType::Orthographic);
    ENJIN_EXPECT_FLOAT_EQ(result.rotation.x, -90.0f);
}

ENJIN_TEST(CameraPresets, FirstPerson) {
    auto result = ApplyCameraPreset(CameraPreset::FirstPerson);
    ENJIN_EXPECT_EQ((int)result.camera.projectionType, (int)ProjectionType::Perspective);
    ENJIN_EXPECT_FLOAT_EQ(result.camera.fieldOfView, 75.0f);
}

ENJIN_TEST(CameraPresets, CinematicWide) {
    auto result = ApplyCameraPreset(CameraPreset::CinematicWide);
    ENJIN_EXPECT_EQ((int)result.camera.projectionType, (int)ProjectionType::Perspective);
    ENJIN_EXPECT_FLOAT_EQ(result.camera.fieldOfView, 40.0f);
}

ENJIN_TEST(CameraPresets, SecurityCam) {
    auto result = ApplyCameraPreset(CameraPreset::SecurityCam);
    ENJIN_EXPECT_FLOAT_EQ(result.camera.fieldOfView, 90.0f);
    ENJIN_EXPECT_FLOAT_EQ(result.camera.farPlane, 50.0f);
}

ENJIN_TEST(CameraPresets, PresetNameLookup) {
    ENJIN_EXPECT_STR_EQ(CameraPresetName(CameraPreset::Custom), "Custom");
    ENJIN_EXPECT_STR_EQ(CameraPresetName(CameraPreset::TopDown), "Top-Down");
    ENJIN_EXPECT_STR_EQ(CameraPresetName(CameraPreset::BirdsEye), "Bird's Eye");
}

// ===========================================================================
// CameraManager (Virtual Camera System)
// ===========================================================================

ENJIN_TEST(CameraManager, GetActiveCameraHighestPriority) {
    World world;
    Entity e1 = world.CreateEntity();
    Entity e2 = world.CreateEntity();
    auto& cam1 = world.AddComponent<CameraComponent>(e1);
    cam1.priority = 0;
    auto& cam2 = world.AddComponent<CameraComponent>(e2);
    cam2.priority = 10;

    Entity active = CameraManager::GetActiveCamera(&world);
    ENJIN_EXPECT_EQ(active, e2);
}

ENJIN_TEST(CameraManager, InactiveCameraSkipped) {
    World world;
    Entity e1 = world.CreateEntity();
    Entity e2 = world.CreateEntity();
    auto& cam1 = world.AddComponent<CameraComponent>(e1);
    cam1.priority = 100;
    cam1.isActive = false;
    auto& cam2 = world.AddComponent<CameraComponent>(e2);
    cam2.priority = 1;

    Entity active = CameraManager::GetActiveCamera(&world);
    ENJIN_EXPECT_EQ(active, e2);
}

ENJIN_TEST(CameraManager, NoCamerasReturnsInvalid) {
    World world;
    world.CreateEntity(); // No camera component
    Entity active = CameraManager::GetActiveCamera(&world);
    ENJIN_EXPECT_EQ(active, (u64)0);
}

ENJIN_TEST(CameraManager, GetAllActiveSortedByPriority) {
    World world;
    Entity e1 = world.CreateEntity();
    Entity e2 = world.CreateEntity();
    Entity e3 = world.CreateEntity();
    world.AddComponent<CameraComponent>(e1).priority = 5;
    world.AddComponent<CameraComponent>(e2).priority = 20;
    world.AddComponent<CameraComponent>(e3).priority = 10;

    auto cameras = CameraManager::GetAllActiveCameras(&world);
    ENJIN_ASSERT_EQ(cameras.size(), (size_t)3);
    ENJIN_EXPECT_EQ(cameras[0], e2); // highest priority first
    ENJIN_EXPECT_EQ(cameras[1], e3);
    ENJIN_EXPECT_EQ(cameras[2], e1);
}

ENJIN_TEST_MAIN()
