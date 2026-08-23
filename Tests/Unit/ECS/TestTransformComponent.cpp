#include "EnjinTest.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Hierarchy.h"
#include "Enjin/ECS/World.h"
#include <cmath>

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
// Hierarchy composition — repro net for the "parented entities render script
// euler rotations with Y and Z swapped" bug. If these pass, ComputeWorldMatrix
// and the math layer are exonerated and the swap happens in a higher layer.
// ===========================================================================

ENJIN_TEST(TransformHierarchy, ChildYawUnderIdentityParentMatchesUnparented) {
    // Arrange: same script-style yaw on a parented and an unparented entity.
    World world;
    Entity parent = world.CreateEntity();
    world.AddComponent<TransformComponent>(parent);
    Entity child = world.CreateEntity();
    world.AddComponent<TransformComponent>(child);
    SetParent(&world, child, parent);
    Entity solo = world.CreateEntity();
    world.AddComponent<TransformComponent>(solo);

    Math::Quaternion yaw = Math::Quaternion::FromEulerDegrees(Math::Vector3(0.0f, 90.0f, 0.0f));
    world.GetComponent<TransformComponent>(child)->rotation = yaw;
    world.GetComponent<TransformComponent>(solo)->rotation = yaw;

    // Act
    Math::Matrix4 childWorld = ComputeWorldMatrix(&world, child);
    Math::Matrix4 soloWorld = ComputeWorldMatrix(&world, solo);

    // Assert: identical matrices, element for element.
    for (usize i = 0; i < 16; ++i) {
        ENJIN_EXPECT_FLOAT_NEAR(childWorld.m[i], soloWorld.m[i], 0.0001f);
    }
}

ENJIN_TEST(TransformHierarchy, TranslatedParentPreservesChildRotationBasis) {
    // Arrange: parent only translated (no rotation), child yawed 90.
    World world;
    Entity parent = world.CreateEntity();
    world.AddComponent<TransformComponent>(parent);
    world.GetComponent<TransformComponent>(parent)->position = Math::Vector3(5.0f, 6.0f, 7.0f);
    Entity child = world.CreateEntity();
    world.AddComponent<TransformComponent>(child);
    SetParent(&world, child, parent);
    Math::Quaternion yaw = Math::Quaternion::FromEulerDegrees(Math::Vector3(0.0f, 90.0f, 0.0f));
    world.GetComponent<TransformComponent>(child)->rotation = yaw;

    // Act
    Math::Matrix4 childWorld = ComputeWorldMatrix(&world, child);

    // Assert: rotation basis identical to the raw quaternion's, translation is
    // the parent's. A yaw about Y must leave the Y axis untouched — a Y/Z swap
    // would move it.
    TransformComponent reference;
    reference.rotation = yaw;
    Math::Matrix4 rotOnly = reference.ToMatrix();
    for (usize col = 0; col < 3; ++col) {
        for (usize rowIdx = 0; rowIdx < 3; ++rowIdx) {
            ENJIN_EXPECT_FLOAT_NEAR(childWorld.m[col * 4 + rowIdx], rotOnly.m[col * 4 + rowIdx], 0.0001f);
        }
    }
    ENJIN_EXPECT_FLOAT_NEAR(childWorld.m[12], 5.0f, 0.0001f);
    ENJIN_EXPECT_FLOAT_NEAR(childWorld.m[13], 6.0f, 0.0001f);
    ENJIN_EXPECT_FLOAT_NEAR(childWorld.m[14], 7.0f, 0.0001f);
}

ENJIN_TEST(TransformHierarchy, RotatedParentComposesInParentSpace) {
    // Arrange: parent pitched 90 about X, child yawed 90 about its local Y.
    World world;
    Entity parent = world.CreateEntity();
    world.AddComponent<TransformComponent>(parent);
    Math::Quaternion parentPitch = Math::Quaternion::FromEulerDegrees(Math::Vector3(90.0f, 0.0f, 0.0f));
    world.GetComponent<TransformComponent>(parent)->rotation = parentPitch;
    Entity child = world.CreateEntity();
    world.AddComponent<TransformComponent>(child);
    SetParent(&world, child, parent);
    Math::Quaternion childYaw = Math::Quaternion::FromEulerDegrees(Math::Vector3(0.0f, 90.0f, 0.0f));
    world.GetComponent<TransformComponent>(child)->rotation = childYaw;

    // Act
    Math::Matrix4 childWorld = ComputeWorldMatrix(&world, child);

    // Assert: world basis = parentR * childR applied to the unit axes.
    Math::Quaternion composed = parentPitch * childYaw;
    Math::Vector3 ex = composed.Rotate(Math::Vector3(1.0f, 0.0f, 0.0f));
    Math::Vector3 ey = composed.Rotate(Math::Vector3(0.0f, 1.0f, 0.0f));
    ENJIN_EXPECT_FLOAT_NEAR(childWorld.m[0], ex.x, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(childWorld.m[1], ex.y, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(childWorld.m[2], ex.z, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(childWorld.m[4], ey.x, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(childWorld.m[5], ey.y, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(childWorld.m[6], ey.z, 0.001f);
}

// ===========================================================================
// Gizmo euler convention — the viewport decomposes matrices with ImGuizmo and
// rebuilds the quaternion. ImGuizmo's extraction (replicated below against the
// engine's column-major layout) is the exact inverse of Quaternion::FromEuler
// (ZYX). The write-back used to rebuild with a hand-rolled Y*X*Z product
// instead, silently corrupting any compound local rotation (typically a child
// under a rotated parent) on every gizmo drag - even pure translations.
// ===========================================================================

namespace {
// ImGuizmo::DecomposeMatrixToComponents' rotation extraction, expressed
// against Matrix4's column-major storage (m[col*4 + row]), in radians.
Math::Vector3 ImGuizmoStyleDecompose(const Math::Matrix4& m) {
    auto M = [&](usize row, usize col) { return m.m[col * 4 + row]; };
    Math::Vector3 r;
    r.x = std::atan2(M(2, 1), M(2, 2));
    r.y = std::atan2(-M(2, 0), std::sqrt(M(2, 1) * M(2, 1) + M(2, 2) * M(2, 2)));
    r.z = std::atan2(M(1, 0), M(0, 0));
    return r;
}
}

ENJIN_TEST(GizmoEulerConvention, FromEulerInvertsImGuizmoDecompose) {
    // Arrange: a compound rotation - all three axes non-trivial.
    Math::Quaternion q = Math::Quaternion::FromEulerDegrees(Math::Vector3(30.0f, 45.0f, 60.0f));
    TransformComponent t;
    t.rotation = q;

    // Act: decompose the matrix the way the viewport does, rebuild both ways.
    Math::Vector3 e = ImGuizmoStyleDecompose(t.ToMatrix());
    Math::Quaternion rebuilt = Math::Quaternion::FromEuler(e);
    Math::Quaternion oldYXZ = Math::Quaternion(Math::Vector3(0, 1, 0), e.y)
                            * Math::Quaternion(Math::Vector3(1, 0, 0), e.x)
                            * Math::Quaternion(Math::Vector3(0, 0, 1), e.z);

    // Assert: FromEuler round-trips (allowing q == -q double cover)...
    f32 dotNew = rebuilt.x * q.x + rebuilt.y * q.y + rebuilt.z * q.z + rebuilt.w * q.w;
    ENJIN_EXPECT_TRUE(std::fabs(dotNew) > 0.9999f);
    // ...and the old Y*X*Z product demonstrably did not.
    f32 dotOld = oldYXZ.x * q.x + oldYXZ.y * q.y + oldYXZ.z * q.z + oldYXZ.w * q.w;
    ENJIN_EXPECT_TRUE(std::fabs(dotOld) < 0.9999f);
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
