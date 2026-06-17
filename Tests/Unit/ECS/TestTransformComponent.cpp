#include "EnjinTest.h"
#include "Enjin/ECS/Components/Transform.h"

using namespace Enjin;
using namespace Enjin::ECS;

// ===========================================================================
// TransformComponent Defaults
// ===========================================================================

ENJIN_TEST(TransformDefaults, Position) {
    TransformComponent t;
    ENJIN_EXPECT_FLOAT_EQ(t.position.x, 0.0f);
    ENJIN_EXPECT_FLOAT_EQ(t.position.y, 0.0f);
    ENJIN_EXPECT_FLOAT_EQ(t.position.z, 0.0f);
}

ENJIN_TEST(TransformDefaults, Scale) {
    TransformComponent t;
    ENJIN_EXPECT_FLOAT_EQ(t.scale.x, 1.0f);
    ENJIN_EXPECT_FLOAT_EQ(t.scale.y, 1.0f);
    ENJIN_EXPECT_FLOAT_EQ(t.scale.z, 1.0f);
}

ENJIN_TEST(TransformDefaults, Visible) {
    TransformComponent t;
    ENJIN_EXPECT_TRUE(t.visible);
}

ENJIN_TEST(TransformDefaults, TeleportedThisFrame) {
    TransformComponent t;
    ENJIN_EXPECT_FALSE(t.teleportedThisFrame);
}

ENJIN_TEST(TransformDefaults, IdentityRotation) {
    TransformComponent t;
    Math::Quaternion id = Math::Quaternion::Identity();
    ENJIN_EXPECT_FLOAT_EQ(t.rotation.x, id.x);
    ENJIN_EXPECT_FLOAT_EQ(t.rotation.y, id.y);
    ENJIN_EXPECT_FLOAT_EQ(t.rotation.z, id.z);
    ENJIN_EXPECT_FLOAT_EQ(t.rotation.w, id.w);
}

// ===========================================================================
// ToMatrix
// ===========================================================================

ENJIN_TEST(TransformMatrix, IdentityAtOrigin) {
    TransformComponent t;
    Math::Matrix4 mat = t.ToMatrix();
    Math::Matrix4 identity = Math::Matrix4::Identity();
    // All 16 elements should match identity
    for (int i = 0; i < 16; ++i) {
        ENJIN_EXPECT_FLOAT_NEAR(mat.m[i], identity.m[i], 0.0001f);
    }
}

ENJIN_TEST(TransformMatrix, TranslationOnly) {
    TransformComponent t;
    t.position = Math::Vector3(3.0f, 5.0f, 7.0f);
    Math::Matrix4 mat = t.ToMatrix();
    // Column-major: translation is in m[12], m[13], m[14]
    ENJIN_EXPECT_FLOAT_NEAR(mat.m[12], 3.0f, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(mat.m[13], 5.0f, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(mat.m[14], 7.0f, 0.001f);
}

ENJIN_TEST(TransformMatrix, ScaleOnly) {
    TransformComponent t;
    t.scale = Math::Vector3(2.0f, 3.0f, 4.0f);
    Math::Matrix4 mat = t.ToMatrix();
    // Diagonal should reflect scale (column-major)
    // col0.length = scaleX, col1.length = scaleY, col2.length = scaleZ
    f32 sx = std::sqrt(mat.m[0]*mat.m[0] + mat.m[1]*mat.m[1] + mat.m[2]*mat.m[2]);
    f32 sy = std::sqrt(mat.m[4]*mat.m[4] + mat.m[5]*mat.m[5] + mat.m[6]*mat.m[6]);
    f32 sz = std::sqrt(mat.m[8]*mat.m[8] + mat.m[9]*mat.m[9] + mat.m[10]*mat.m[10]);
    ENJIN_EXPECT_FLOAT_NEAR(sx, 2.0f, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(sy, 3.0f, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(sz, 4.0f, 0.001f);
}

ENJIN_TEST(TransformMatrix, UniformScale) {
    TransformComponent t;
    t.scale = Math::Vector3(5.0f, 5.0f, 5.0f);
    Math::Matrix4 mat = t.ToMatrix();
    f32 sx = std::sqrt(mat.m[0]*mat.m[0] + mat.m[1]*mat.m[1] + mat.m[2]*mat.m[2]);
    ENJIN_EXPECT_FLOAT_NEAR(sx, 5.0f, 0.001f);
}

ENJIN_TEST(TransformMatrix, RotationAppliedToBasis) {
    // Arrange: a 90-degree yaw about Y, no translation, unit scale.
    TransformComponent t;
    Math::Quaternion q = Math::Quaternion::FromEulerDegrees(Math::Vector3(0.0f, 90.0f, 0.0f));
    t.rotation = q;
    // Act
    Math::Matrix4 mat = t.ToMatrix();
    // Assert: each basis column equals the quaternion-rotated axis.
    Math::Vector3 ex = q.Rotate(Math::Vector3(1.0f, 0.0f, 0.0f));
    Math::Vector3 ey = q.Rotate(Math::Vector3(0.0f, 1.0f, 0.0f));
    Math::Vector3 ez = q.Rotate(Math::Vector3(0.0f, 0.0f, 1.0f));
    ENJIN_EXPECT_FLOAT_NEAR(mat.m[0], ex.x, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(mat.m[1], ex.y, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(mat.m[2], ex.z, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(mat.m[4], ey.x, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(mat.m[5], ey.y, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(mat.m[6], ey.z, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(mat.m[8], ez.x, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(mat.m[9], ez.y, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(mat.m[10], ez.z, 0.001f);
    // Guard against "rotation silently ignored": the X axis must have moved.
    ENJIN_EXPECT_TRUE(std::fabs(ex.x - 1.0f) > 0.1f);
}

ENJIN_TEST(TransformMatrix, FullTRSComposition) {
    // Arrange: translate (1,2,3), yaw 90, uniform scale 2.
    TransformComponent t;
    t.position = Math::Vector3(1.0f, 2.0f, 3.0f);
    t.rotation = Math::Quaternion::FromEulerDegrees(Math::Vector3(0.0f, 90.0f, 0.0f));
    t.scale = Math::Vector3(2.0f, 2.0f, 2.0f);
    // Act
    Math::Matrix4 mat = t.ToMatrix();
    // Assert: translation in the last column, scale recovered from column length.
    ENJIN_EXPECT_FLOAT_NEAR(mat.m[12], 1.0f, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(mat.m[13], 2.0f, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(mat.m[14], 3.0f, 0.001f);
    f32 col0len = std::sqrt(mat.m[0]*mat.m[0] + mat.m[1]*mat.m[1] + mat.m[2]*mat.m[2]);
    ENJIN_EXPECT_FLOAT_NEAR(col0len, 2.0f, 0.001f);
}

// ===========================================================================
// Teleport Flag
// ===========================================================================

ENJIN_TEST(TransformTeleport, SetFlag) {
    TransformComponent t;
    t.teleportedThisFrame = true;
    ENJIN_EXPECT_TRUE(t.teleportedThisFrame);
}

// ===========================================================================
// Visibility Flag
// ===========================================================================

ENJIN_TEST(TransformVisibility, SetInvisible) {
    TransformComponent t;
    t.visible = false;
    ENJIN_EXPECT_FALSE(t.visible);
}

ENJIN_TEST_MAIN()
