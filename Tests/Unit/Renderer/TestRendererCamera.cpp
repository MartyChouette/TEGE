#include "EnjinTest.h"
#include "Enjin/Renderer/Camera.h"

using namespace Enjin;
using namespace Enjin::Renderer;

// ===========================================================================
// Camera Defaults
// ===========================================================================

ENJIN_TEST(RendererCamera, DefaultPosition) {
    Camera cam;
    Math::Vector3 pos = cam.GetPosition();
    ENJIN_EXPECT_FLOAT_EQ(pos.x, 0.0f);
    ENJIN_EXPECT_FLOAT_EQ(pos.y, 0.0f);
    ENJIN_EXPECT_FLOAT_EQ(pos.z, 0.0f);
}

ENJIN_TEST(RendererCamera, DefaultRotation) {
    Camera cam;
    Math::Quaternion rot = cam.GetRotation();
    Math::Quaternion id = Math::Quaternion::Identity();
    ENJIN_EXPECT_FLOAT_NEAR(rot.x, id.x, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(rot.y, id.y, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(rot.z, id.z, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(rot.w, id.w, 0.001f);
}

ENJIN_TEST(RendererCamera, DefaultFOV) {
    Camera cam;
    ENJIN_EXPECT_FLOAT_EQ(cam.GetFOV(), 45.0f);
}

ENJIN_TEST(RendererCamera, DefaultNearFar) {
    Camera cam;
    ENJIN_EXPECT_FLOAT_NEAR(cam.GetNearPlane(), 0.1f, 0.001f);
    ENJIN_EXPECT_FLOAT_EQ(cam.GetFarPlane(), 100.0f);
}

ENJIN_TEST(RendererCamera, DefaultIsPerspective) {
    Camera cam;
    ENJIN_EXPECT_TRUE(cam.IsPerspective());
}

// ===========================================================================
// Camera SetPosition
// ===========================================================================

ENJIN_TEST(RendererCamera, SetPosition) {
    Camera cam;
    cam.SetPosition(Math::Vector3(1.0f, 2.0f, 3.0f));
    Math::Vector3 pos = cam.GetPosition();
    ENJIN_EXPECT_FLOAT_EQ(pos.x, 1.0f);
    ENJIN_EXPECT_FLOAT_EQ(pos.y, 2.0f);
    ENJIN_EXPECT_FLOAT_EQ(pos.z, 3.0f);
}

// ===========================================================================
// Camera SetPerspective
// ===========================================================================

ENJIN_TEST(RendererCamera, SetPerspective) {
    Camera cam;
    cam.SetPerspective(60.0f, 16.0f / 9.0f, 0.5f, 500.0f);
    ENJIN_EXPECT_FLOAT_EQ(cam.GetFOV(), 60.0f);
    ENJIN_EXPECT_FLOAT_NEAR(cam.GetAspect(), 16.0f / 9.0f, 0.001f);
    ENJIN_EXPECT_FLOAT_EQ(cam.GetNearPlane(), 0.5f);
    ENJIN_EXPECT_FLOAT_EQ(cam.GetFarPlane(), 500.0f);
    ENJIN_EXPECT_TRUE(cam.IsPerspective());
}

// ===========================================================================
// Camera Orthographic
// ===========================================================================

ENJIN_TEST(RendererCamera, SetOrthographic) {
    Camera cam;
    cam.SetOrthographic(-10.0f, 10.0f, -10.0f, 10.0f, 0.1f, 100.0f);
    ENJIN_EXPECT_FALSE(cam.IsPerspective());
}

// ===========================================================================
// Camera Direction Vectors
// ===========================================================================

ENJIN_TEST(RendererCamera, ForwardVector) {
    Camera cam;
    Math::Vector3 fwd = cam.GetForward();
    // Default forward should be roughly (0, 0, -1) for identity rotation
    ENJIN_EXPECT_FLOAT_NEAR(fwd.z, -1.0f, 0.01f);
}

ENJIN_TEST(RendererCamera, UpVector) {
    Camera cam;
    Math::Vector3 up = cam.GetUp();
    // Default up should be roughly (0, 1, 0) for identity rotation
    ENJIN_EXPECT_FLOAT_NEAR(up.y, 1.0f, 0.01f);
}

ENJIN_TEST(RendererCamera, RightVector) {
    Camera cam;
    Math::Vector3 right = cam.GetRight();
    // Default right should be roughly (1, 0, 0) for identity rotation
    ENJIN_EXPECT_FLOAT_NEAR(right.x, 1.0f, 0.01f);
}

// ===========================================================================
// Camera Matrices
// ===========================================================================

ENJIN_TEST(RendererCamera, ViewProjectionNotZero) {
    Camera cam;
    cam.SetPerspective(45.0f, 1.0f, 0.1f, 100.0f);
    Math::Matrix4 vp = cam.GetViewProjectionMatrix();
    // VP matrix should not be all zeros
    bool anyNonZero = false;
    for (int i = 0; i < 16; ++i) {
        if (vp.m[i] != 0.0f) { anyNonZero = true; break; }
    }
    ENJIN_EXPECT_TRUE(anyNonZero);
}

ENJIN_TEST(RendererCamera, ViewMatrixNotZero) {
    Camera cam;
    Math::Matrix4 v = cam.GetViewMatrix();
    bool anyNonZero = false;
    for (int i = 0; i < 16; ++i) {
        if (v.m[i] != 0.0f) { anyNonZero = true; break; }
    }
    ENJIN_EXPECT_TRUE(anyNonZero);
}

ENJIN_TEST(RendererCamera, ProjectionMatrixNotZero) {
    Camera cam;
    cam.SetPerspective(45.0f, 1.0f, 0.1f, 100.0f);
    Math::Matrix4 p = cam.GetProjectionMatrix();
    bool anyNonZero = false;
    for (int i = 0; i < 16; ++i) {
        if (p.m[i] != 0.0f) { anyNonZero = true; break; }
    }
    ENJIN_EXPECT_TRUE(anyNonZero);
}

ENJIN_TEST_MAIN()
