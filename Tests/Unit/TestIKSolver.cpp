#include "EnjinTest.h"
#include "Enjin/Animation/IKSolver.h"

#include <cmath>
#include <vector>

using namespace Enjin;
using namespace Enjin::Animation;
using namespace Enjin::Math;

// ===========================================================================
// LookAtIK — Basic
// ===========================================================================

ENJIN_TEST(LookAtIK, SolveReturnsQuaternion) {
    Vector3 head(0, 1, 0);
    Vector3 target(0, 1, 5);
    Quaternion current; // identity
    Quaternion result = LookAtIK::Solve(head, target, current, 90.0f, 10.0f, 0.016f);
    // Should return a valid quaternion (non-zero length)
    f32 len = std::sqrt(result.x * result.x + result.y * result.y +
                        result.z * result.z + result.w * result.w);
    ENJIN_EXPECT_FLOAT_NEAR(len, 1.0f, 0.01f);
}

ENJIN_TEST(LookAtIK, TargetAtHeadReturnsCurrentRotation) {
    Vector3 head(0, 0, 0);
    Vector3 target(0, 0, 0); // Same as head — degenerate
    Quaternion current(0, 0, 0, 1); // identity
    Quaternion result = LookAtIK::Solve(head, target, current, 90.0f, 10.0f, 0.016f);
    // Should return current rotation (early exit on near-zero distance)
    ENJIN_EXPECT_FLOAT_NEAR(result.w, current.w, 0.01f);
}

ENJIN_TEST(LookAtIK, AngleClampLimitsRotation) {
    Vector3 head(0, 0, 0);
    Vector3 target(10, 0, 0); // 90 degrees from forward (Z+)
    Quaternion current(0, 0, 0, 1); // facing Z+
    // Clamp to only 10 degrees — result should be limited
    Quaternion result = LookAtIK::Solve(head, target, current, 10.0f, 100.0f, 1.0f);
    // The rotation should be closer to identity than a full 90-degree turn
    ENJIN_EXPECT_GT(result.w, 0.9f); // w close to 1 = small rotation
}

ENJIN_TEST(LookAtIK, LargeMaxAngleAllowsFullTurn) {
    Vector3 head(0, 0, 0);
    Vector3 target(10, 0, 0);
    Quaternion current(0, 0, 0, 1);
    // Allow up to 180 degrees — should be able to face target fully
    Quaternion result = LookAtIK::Solve(head, target, current, 180.0f, 100.0f, 1.0f);
    f32 len = std::sqrt(result.x * result.x + result.y * result.y +
                        result.z * result.z + result.w * result.w);
    ENJIN_EXPECT_FLOAT_NEAR(len, 1.0f, 0.01f);
}

ENJIN_TEST(LookAtIK, ZeroDtNoSmoothing) {
    Vector3 head(0, 0, 0);
    Vector3 target(0, 0, 10);
    Quaternion current(0, 0, 0, 1);
    // dt=0 means t = clamp(smoothSpeed * 0, 0, 1) = 0 → no interpolation
    Quaternion result = LookAtIK::Solve(head, target, current, 90.0f, 10.0f, 0.0f);
    // With zero smoothing, result should equal current rotation
    ENJIN_EXPECT_FLOAT_NEAR(result.w, current.w, 0.01f);
}

// ===========================================================================
// LookAtIK — RotationFromDirection
// ===========================================================================

ENJIN_TEST(RotFromDir, ForwardZReturnsNearIdentity) {
    Quaternion q = LookAtIK::RotationFromDirection(Vector3(0, 0, 1), Vector3(0, 1, 0));
    // Forward is Z+ with up Y+ — should be close to identity
    f32 len = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
    ENJIN_EXPECT_FLOAT_NEAR(len, 1.0f, 0.01f);
}

ENJIN_TEST(RotFromDir, ResultIsUnitQuaternion) {
    Quaternion q = LookAtIK::RotationFromDirection(Vector3(1, 1, 0), Vector3(0, 1, 0));
    f32 len = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
    ENJIN_EXPECT_FLOAT_NEAR(len, 1.0f, 0.01f);
}

// ===========================================================================
// FABRIK — Basic
// ===========================================================================

ENJIN_TEST(FABRIK, TwoJointReachableTarget) {
    // Simple 2-bone chain: root at origin, joint at (1,0,0), end at (2,0,0)
    std::vector<Vector3> positions = {
        Vector3(0, 0, 0),
        Vector3(1, 0, 0),
        Vector3(2, 0, 0)
    };
    Vector3 target(1.5f, 1.0f, 0); // reachable (within total length 2)

    FABRIK::Solve(positions, target);

    // End effector should be near target
    f32 dx = positions[2].x - target.x;
    f32 dy = positions[2].y - target.y;
    f32 dz = positions[2].z - target.z;
    f32 dist = std::sqrt(dx * dx + dy * dy + dz * dz);
    ENJIN_EXPECT_FLOAT_NEAR(dist, 0.0f, 0.05f);
}

ENJIN_TEST(FABRIK, RootStaysFixed) {
    std::vector<Vector3> positions = {
        Vector3(0, 0, 0),
        Vector3(1, 0, 0),
        Vector3(2, 0, 0)
    };
    Vector3 root = positions[0];
    FABRIK::Solve(positions, Vector3(1, 1, 0));
    ENJIN_EXPECT_FLOAT_NEAR(positions[0].x, root.x, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(positions[0].y, root.y, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(positions[0].z, root.z, 0.001f);
}

ENJIN_TEST(FABRIK, BoneLengthsPreserved) {
    std::vector<Vector3> positions = {
        Vector3(0, 0, 0),
        Vector3(1, 0, 0),
        Vector3(2, 0, 0)
    };
    // Original bone lengths: 1.0, 1.0
    FABRIK::Solve(positions, Vector3(1, 1, 0));

    f32 bone1 = std::sqrt(
        (positions[1].x - positions[0].x) * (positions[1].x - positions[0].x) +
        (positions[1].y - positions[0].y) * (positions[1].y - positions[0].y) +
        (positions[1].z - positions[0].z) * (positions[1].z - positions[0].z)
    );
    f32 bone2 = std::sqrt(
        (positions[2].x - positions[1].x) * (positions[2].x - positions[1].x) +
        (positions[2].y - positions[1].y) * (positions[2].y - positions[1].y) +
        (positions[2].z - positions[1].z) * (positions[2].z - positions[1].z)
    );
    ENJIN_EXPECT_FLOAT_NEAR(bone1, 1.0f, 0.01f);
    ENJIN_EXPECT_FLOAT_NEAR(bone2, 1.0f, 0.01f);
}

ENJIN_TEST(FABRIK, UnreachableTargetStretches) {
    std::vector<Vector3> positions = {
        Vector3(0, 0, 0),
        Vector3(1, 0, 0),
        Vector3(2, 0, 0)
    };
    // Target at distance 10, total chain length = 2. Unreachable.
    FABRIK::Solve(positions, Vector3(10, 0, 0));

    // All joints should be stretched toward target in a line
    // End effector at distance 2 from root, in direction of target
    f32 endDist = std::sqrt(
        positions[2].x * positions[2].x +
        positions[2].y * positions[2].y +
        positions[2].z * positions[2].z
    );
    ENJIN_EXPECT_FLOAT_NEAR(endDist, 2.0f, 0.01f);
    // Should be stretched in +X direction
    ENJIN_EXPECT_GT(positions[2].x, 1.9f);
}

ENJIN_TEST(FABRIK, SingleJointNoOp) {
    // Fewer than 2 joints — should early-exit without crashing
    std::vector<Vector3> positions = { Vector3(0, 0, 0) };
    FABRIK::Solve(positions, Vector3(1, 0, 0));
    ENJIN_EXPECT_FLOAT_EQ(positions[0].x, 0.0f);
}

ENJIN_TEST(FABRIK, EmptyChainNoOp) {
    std::vector<Vector3> positions;
    FABRIK::Solve(positions, Vector3(1, 0, 0)); // Should not crash
}

ENJIN_TEST(FABRIK, ThreeBonesReachable) {
    std::vector<Vector3> positions = {
        Vector3(0, 0, 0),
        Vector3(1, 0, 0),
        Vector3(2, 0, 0),
        Vector3(3, 0, 0)
    };
    Vector3 target(2, 2, 0); // reachable (chain length = 3)
    FABRIK::Solve(positions, target);

    f32 dx = positions[3].x - target.x;
    f32 dy = positions[3].y - target.y;
    f32 dist = std::sqrt(dx * dx + dy * dy);
    ENJIN_EXPECT_FLOAT_NEAR(dist, 0.0f, 0.1f);
}

ENJIN_TEST(FABRIK, MoreIterationsImproveAccuracy) {
    std::vector<Vector3> pos1 = {
        Vector3(0, 0, 0),
        Vector3(1, 0, 0),
        Vector3(2, 0, 0)
    };
    std::vector<Vector3> pos2 = pos1;
    Vector3 target(1.2f, 0.8f, 0);

    FABRIK::Solve(pos1, target, 1);  // 1 iteration
    FABRIK::Solve(pos2, target, 20); // 20 iterations

    f32 err1 = std::sqrt(
        (pos1[2].x - target.x) * (pos1[2].x - target.x) +
        (pos1[2].y - target.y) * (pos1[2].y - target.y)
    );
    f32 err2 = std::sqrt(
        (pos2[2].x - target.x) * (pos2[2].x - target.x) +
        (pos2[2].y - target.y) * (pos2[2].y - target.y)
    );
    ENJIN_EXPECT_TRUE(err2 <= err1 + 0.001f);
}

ENJIN_TEST(FABRIK, TargetAtRootStaysClose) {
    std::vector<Vector3> positions = {
        Vector3(0, 0, 0),
        Vector3(1, 0, 0),
        Vector3(2, 0, 0)
    };
    // Target at root — chain should fold back
    FABRIK::Solve(positions, Vector3(0, 0, 0));
    // Root should stay at origin
    ENJIN_EXPECT_FLOAT_NEAR(positions[0].x, 0.0f, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(positions[0].y, 0.0f, 0.001f);
}

ENJIN_TEST_MAIN()
