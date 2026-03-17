#include "EnjinTest.h"
#include "Enjin/Renderer/CameraController.h"

using namespace Enjin;
using namespace Enjin::Renderer;

// ===========================================================================
// CameraMode Enum
// ===========================================================================

ENJIN_TEST(CameraModeEnum, Values) {
    ENJIN_EXPECT_EQ((int)CameraMode::Fly, 0);
    ENJIN_EXPECT_EQ((int)CameraMode::Orbit, 1);
    ENJIN_EXPECT_EQ((int)CameraMode::FirstPerson, 2);
}

// ===========================================================================
// ViewPreset Enum
// ===========================================================================

ENJIN_TEST(ViewPresetEnum, Values) {
    ENJIN_EXPECT_EQ((int)ViewPreset::Perspective, 0);
    ENJIN_EXPECT_EQ((int)ViewPreset::Top, 1);
    ENJIN_EXPECT_EQ((int)ViewPreset::Bottom, 2);
    ENJIN_EXPECT_EQ((int)ViewPreset::Front, 3);
    ENJIN_EXPECT_EQ((int)ViewPreset::Back, 4);
    ENJIN_EXPECT_EQ((int)ViewPreset::Right, 5);
    ENJIN_EXPECT_EQ((int)ViewPreset::Left, 6);
}

// ===========================================================================
// CameraController Defaults (null camera)
// ===========================================================================

ENJIN_TEST(CameraCtrl, DefaultNullCamera) {
    CameraController ctrl;
    ENJIN_EXPECT_NULL(ctrl.GetCamera());
}

ENJIN_TEST(CameraCtrl, DefaultMode) {
    CameraController ctrl;
    ENJIN_EXPECT_EQ((int)ctrl.GetMode(), (int)CameraMode::Fly);
}

ENJIN_TEST(CameraCtrl, DefaultEnabled) {
    CameraController ctrl;
    ENJIN_EXPECT_TRUE(ctrl.IsEnabled());
}

ENJIN_TEST(CameraCtrl, DefaultMoveSpeed) {
    CameraController ctrl;
    ENJIN_EXPECT_FLOAT_EQ(ctrl.GetMoveSpeed(), 5.0f);
}

ENJIN_TEST(CameraCtrl, DefaultSensitivity) {
    CameraController ctrl;
    ENJIN_EXPECT_FLOAT_NEAR(ctrl.GetLookSensitivity(), 0.1f, 0.001f);
}

ENJIN_TEST(CameraCtrl, DefaultSprintMultiplier) {
    CameraController ctrl;
    ENJIN_EXPECT_FLOAT_EQ(ctrl.GetSprintMultiplier(), 2.5f);
}

ENJIN_TEST(CameraCtrl, DefaultOrbitDistance) {
    CameraController ctrl;
    ENJIN_EXPECT_FLOAT_EQ(ctrl.GetOrbitDistance(), 5.0f);
}

ENJIN_TEST(CameraCtrl, DefaultOrbitTarget) {
    CameraController ctrl;
    Math::Vector3 target = ctrl.GetOrbitTarget();
    ENJIN_EXPECT_FLOAT_EQ(target.x, 0.0f);
    ENJIN_EXPECT_FLOAT_EQ(target.y, 0.0f);
    ENJIN_EXPECT_FLOAT_EQ(target.z, 0.0f);
}

ENJIN_TEST(CameraCtrl, DefaultYawPitch) {
    CameraController ctrl;
    ENJIN_EXPECT_FLOAT_EQ(ctrl.GetYaw(), -90.0f);
    ENJIN_EXPECT_FLOAT_EQ(ctrl.GetPitch(), 0.0f);
}

ENJIN_TEST(CameraCtrl, NotMouseCaptured) {
    CameraController ctrl;
    ENJIN_EXPECT_FALSE(ctrl.IsMouseCaptured());
}

ENJIN_TEST(CameraCtrl, DefaultViewPreset) {
    CameraController ctrl;
    ENJIN_EXPECT_EQ((int)ctrl.GetViewPreset(), (int)ViewPreset::Perspective);
}

ENJIN_TEST(CameraCtrl, DefaultNotOrthographic) {
    CameraController ctrl;
    ENJIN_EXPECT_FALSE(ctrl.IsOrthographic());
}

ENJIN_TEST(CameraCtrl, DefaultOrthoSize) {
    CameraController ctrl;
    ENJIN_EXPECT_FLOAT_EQ(ctrl.GetOrthoSize(), 10.0f);
}

// ===========================================================================
// CameraController Setters
// ===========================================================================

ENJIN_TEST(CameraCtrlSetters, SetMode) {
    CameraController ctrl;
    ctrl.SetMode(CameraMode::Orbit);
    ENJIN_EXPECT_EQ((int)ctrl.GetMode(), (int)CameraMode::Orbit);
    ctrl.SetMode(CameraMode::FirstPerson);
    ENJIN_EXPECT_EQ((int)ctrl.GetMode(), (int)CameraMode::FirstPerson);
}

ENJIN_TEST(CameraCtrlSetters, SetMoveSpeed) {
    CameraController ctrl;
    ctrl.SetMoveSpeed(10.0f);
    ENJIN_EXPECT_FLOAT_EQ(ctrl.GetMoveSpeed(), 10.0f);
}

ENJIN_TEST(CameraCtrlSetters, SetSensitivity) {
    CameraController ctrl;
    ctrl.SetLookSensitivity(0.5f);
    ENJIN_EXPECT_FLOAT_EQ(ctrl.GetLookSensitivity(), 0.5f);
}

ENJIN_TEST(CameraCtrlSetters, SetSprintMultiplier) {
    CameraController ctrl;
    ctrl.SetSprintMultiplier(3.0f);
    ENJIN_EXPECT_FLOAT_EQ(ctrl.GetSprintMultiplier(), 3.0f);
}

ENJIN_TEST(CameraCtrlSetters, SetOrbitTarget) {
    CameraController ctrl;
    ctrl.SetOrbitTarget(Math::Vector3(1.0f, 2.0f, 3.0f));
    Math::Vector3 t = ctrl.GetOrbitTarget();
    ENJIN_EXPECT_FLOAT_EQ(t.x, 1.0f);
    ENJIN_EXPECT_FLOAT_EQ(t.y, 2.0f);
    ENJIN_EXPECT_FLOAT_EQ(t.z, 3.0f);
}

ENJIN_TEST(CameraCtrlSetters, SetOrbitDistance) {
    CameraController ctrl;
    ctrl.SetOrbitDistance(15.0f);
    ENJIN_EXPECT_FLOAT_EQ(ctrl.GetOrbitDistance(), 15.0f);
}

ENJIN_TEST(CameraCtrlSetters, SetEnabled) {
    CameraController ctrl;
    ctrl.SetEnabled(false);
    ENJIN_EXPECT_FALSE(ctrl.IsEnabled());
    ctrl.SetEnabled(true);
    ENJIN_EXPECT_TRUE(ctrl.IsEnabled());
}

ENJIN_TEST(CameraCtrlSetters, SetOrthoSize) {
    CameraController ctrl;
    ctrl.SetOrthoSize(20.0f);
    ENJIN_EXPECT_FLOAT_EQ(ctrl.GetOrthoSize(), 20.0f);
}

ENJIN_TEST_MAIN()
