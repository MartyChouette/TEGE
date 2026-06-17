#include "EnjinTest.h"
#include "Enjin/Renderer/Camera.h"

using namespace Enjin;
using namespace Enjin::Renderer;

// Transform a point by a column-major Matrix4 (element [r,c] = m[c*4 + r]).
static Math::Vector3 TransformPoint(const Math::Matrix4& m, const Math::Vector3& p) {
    return Math::Vector3(
        m.m[0]*p.x + m.m[4]*p.y + m.m[8]*p.z  + m.m[12],
        m.m[1]*p.x + m.m[5]*p.y + m.m[9]*p.z  + m.m[13],
        m.m[2]*p.x + m.m[6]*p.y + m.m[10]*p.z + m.m[14]);
}

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

// ===========================================================================
// Camera Matrix Correctness (computed expected values, not "non-zero")
// ===========================================================================

ENJIN_TEST(RendererCamera, ViewProjectionIsProjectionTimesView) {
    // Arrange
    Camera cam;
    cam.SetPerspective(60.0f, 16.0f / 9.0f, 0.1f, 100.0f);
    cam.SetPosition(Math::Vector3(2.0f, 3.0f, 10.0f));
    // Act
    Math::Matrix4 vp = cam.GetViewProjectionMatrix();
    Math::Matrix4 pv = cam.GetProjectionMatrix() * cam.GetViewMatrix();
    // Assert: composition order is projection * view.
    for (int i = 0; i < 16; ++i) {
        ENJIN_EXPECT_FLOAT_NEAR(vp.m[i], pv.m[i], 0.001f);
    }
}

ENJIN_TEST(RendererCamera, LookAtPlacesWorldOriginInFront) {
    // Arrange: eye 5 units down +Z, looking at the origin.
    Camera cam;
    cam.SetLookAt(Math::Vector3(0.0f, 0.0f, 5.0f),
                  Math::Vector3(0.0f, 0.0f, 0.0f),
                  Math::Vector3(0.0f, 1.0f, 0.0f));
    // Act: world origin into view space.
    Math::Vector3 v = TransformPoint(cam.GetViewMatrix(), Math::Vector3(0.0f, 0.0f, 0.0f));
    // Assert: 5 units in front of the camera (looks down -Z).
    ENJIN_EXPECT_FLOAT_NEAR(v.x, 0.0f, 0.01f);
    ENJIN_EXPECT_FLOAT_NEAR(v.y, 0.0f, 0.01f);
    ENJIN_EXPECT_FLOAT_NEAR(v.z, -5.0f, 0.01f);
}

ENJIN_TEST(RendererCamera, RotationDrivesForwardVector) {
    // Arrange: a 90-degree yaw (non-identity rotation). GetForward extracts the
    // rotation-matrix row (camera world-space basis convention), so assert the
    // geometric result rather than a column-based quaternion rotation.
    Camera cam;
    cam.SetRotation(Math::Quaternion::FromEulerDegrees(Math::Vector3(0.0f, 90.0f, 0.0f)));
    // Act
    Math::Vector3 fwd = cam.GetForward();
    Math::Vector3 right = cam.GetRight();
    Math::Vector3 up = cam.GetUp();
    // Assert: forward rotated 90 deg out of -Z into the X axis, unit length.
    f32 flen = std::sqrt(fwd.x*fwd.x + fwd.y*fwd.y + fwd.z*fwd.z);
    ENJIN_EXPECT_FLOAT_NEAR(flen, 1.0f, 0.01f);
    ENJIN_EXPECT_FLOAT_NEAR(fwd.y, 0.0f, 0.01f);
    ENJIN_EXPECT_FLOAT_NEAR(fwd.z, 0.0f, 0.01f);
    ENJIN_EXPECT_TRUE(std::fabs(fwd.x) > 0.99f);  // moved onto X (rotation applied)
    // And the basis stays orthonormal.
    ENJIN_EXPECT_FLOAT_NEAR(fwd.x*right.x + fwd.y*right.y + fwd.z*right.z, 0.0f, 0.01f);
    ENJIN_EXPECT_FLOAT_NEAR(fwd.x*up.x + fwd.y*up.y + fwd.z*up.z, 0.0f, 0.01f);
}

ENJIN_TEST_MAIN()
