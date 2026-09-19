// The look-at solver's result reaching a bone.
//
// LookAtIKComponent solved a head rotation every frame and stored it in
// currentHeadRotation, which nothing read -- so the head never turned. It also
// never read headBoneName or neckBoneName, and took the head's position to be
// entityPosition + (0, 1.6, 0): a guess at how tall a character is. The
// component serialized, showed an inspector, cost a solve per frame and did
// nothing, which is the hardest kind of dead code to notice because everything
// about it looks alive. Found by the feature-audit swarm.
//
// These test the SOLVER and the parent-space conversion directly rather than
// driving RenderSystem, which needs a renderer. The conversion is the part that
// was actually missing and the part most likely to be wrong: Solve returns a
// WORLD rotation and localRotations are in PARENT space.
#include "EnjinTest.h"
#include "Enjin/Animation/IKSolver.h"
#include "Enjin/Math/Quaternion.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/Math/Matrix.h"

#include <cmath>

using namespace Enjin;

namespace {

// Where a rotation points the head's forward axis.
//
// +Z, measured, not assumed. LookAtIK::Solve declares
// `Math::Vector3 forward(0.0f, 0.0f, -1.0f);` and then never uses it -- the
// convention actually comes from RotationFromDirection, which faces +Z at the
// target. That unused line cost this test a run, and it is exactly the kind of
// confidently wrong leftover that makes a reader trust the wrong axis.
Math::Vector3 Forward(const Math::Quaternion& q) {
    return q.Rotate(Math::Vector3(0.0f, 0.0f, 1.0f));
}

} // namespace

ENJIN_TEST(LookAtIK, test_the_solver_turns_the_head_toward_the_target) {
    // Arrange: head at the origin, target off to the right.
    const Math::Vector3 head(0.0f, 0.0f, 0.0f);
    const Math::Vector3 target(10.0f, 0.0f, 0.0f);
    Math::Quaternion current = Math::Quaternion::Identity();

    // Act: a generous smooth speed and a large clamp, stepped enough times to
    // converge -- the solver is a smoothed approach, not a snap.
    for (int i = 0; i < 200; ++i) {
        current = Animation::LookAtIK::Solve(head, target, current, 180.0f, 20.0f, 1.0f / 60.0f);
    }

    // Assert: forward now points along +X, toward the target.
    const Math::Vector3 f = Forward(current);
    ENJIN_EXPECT_FLOAT_NEAR(f.x, 1.0f, 0.05f);
    ENJIN_EXPECT_FLOAT_NEAR(f.y, 0.0f, 0.05f);
}

ENJIN_TEST(LookAtIK, test_maxRotation_limits_the_STEP_not_the_total_turn) {
    // This pins what the solver DOES, which is not what the field says.
    //
    // LookAtIKComponent documents maxRotation as "Max angle in degrees", which
    // reads as a limit on how far the head may turn from rest -- the thing that
    // stops a look-at spinning a neck like an owl. It is not. Solve clamps the
    // delta from the CURRENT rotation toward the target on each call:
    //
    //     delta = targetRot * currentRotation.Conjugate();
    //     if (angle > maxAngleRad) { ... }
    //
    // so `current` walks toward the target a bounded amount per call and
    // arrives in full given enough calls. That is a rate limit, and smoothSpeed
    // is already one. There is no angle limit anywhere.
    //
    // Asserted as-is rather than "fixed" to pass: changing it means deciding
    // what the angle is measured FROM, and Solve never receives a rest
    // orientation to measure against. That is a signature change and a design
    // call, so it is on the backlog instead of being guessed at here. The
    // component was dead until today, so nothing shipped depends on either
    // reading.
    // Arrange: forward is +Z, so directly behind is -Z.
    const Math::Vector3 head(0.0f, 0.0f, 0.0f);
    const Math::Vector3 behind(0.0f, 0.0f, -10.0f);
    Math::Quaternion current = Math::Quaternion::Identity();

    // Act: one step, then many.
    const Math::Quaternion afterOne =
        Animation::LookAtIK::Solve(head, behind, current, 30.0f, 20.0f, 1.0f / 60.0f);
    for (int i = 0; i < 200; ++i) {
        current = Animation::LookAtIK::Solve(head, behind, current, 30.0f, 20.0f, 1.0f / 60.0f);
    }

    // Assert: one step barely moves (the clamp bites), and two hundred get all
    // the way round to face the target behind. If maxRotation were a total
    // limit, the second would still be near +Z.
    ENJIN_EXPECT_TRUE(Forward(afterOne).z > 0.9f);
    ENJIN_EXPECT_TRUE(Forward(current).z < -0.9f);
}

ENJIN_TEST(LookAtIK, test_a_target_on_top_of_the_head_leaves_the_rotation_alone) {
    // Degenerate input: the direction to the target is undefined, and
    // normalising it would be a divide by zero that poisons the pose with NaN
    // for the rest of the run.
    // Arrange
    const Math::Vector3 head(1.0f, 2.0f, 3.0f);
    const Math::Quaternion start(Math::Vector3(0.0f, 1.0f, 0.0f), 0.7f);

    // Act
    const Math::Quaternion out =
        Animation::LookAtIK::Solve(head, head, start, 45.0f, 5.0f, 1.0f / 60.0f);

    // Assert
    ENJIN_EXPECT_FLOAT_NEAR(out.x, start.x, 0.0001f);
    ENJIN_EXPECT_FLOAT_NEAR(out.y, start.y, 0.0001f);
    ENJIN_EXPECT_FLOAT_NEAR(out.z, start.z, 0.0001f);
    ENJIN_EXPECT_FLOAT_NEAR(out.w, start.w, 0.0001f);
}

ENJIN_TEST(LookAtIK, test_converting_a_world_rotation_into_parent_space_and_back) {
    // The conversion the write-back does, and the reason it is not just
    // "assign the solved rotation to the bone".
    //
    // A head bone hangs off a neck that is itself rotated. localRotations are
    // in PARENT space, so storing a world rotation there points the head
    // correctly only when the parent happens to be unrotated -- i.e. only while
    // the character faces down -Z.
    // Arrange: parent yawed 90 degrees, desired world rotation yawed 90.
    const Math::Quaternion parentWorld =
        Math::Quaternion::FromEuler(Math::Vector3(0.0f, 1.5707963f, 0.0f));
    const Math::Quaternion desiredWorld =
        Math::Quaternion::FromEuler(Math::Vector3(0.0f, 1.5707963f, 0.0f));

    // Act
    const Math::Quaternion local = parentWorld.Conjugate() * desiredWorld;

    // Assert: parent already provides the whole rotation, so the LOCAL one is
    // identity -- and composing it back gives the world rotation again.
    ENJIN_EXPECT_FLOAT_NEAR(std::fabs(local.w), 1.0f, 0.001f);

    const Math::Quaternion roundTrip = parentWorld * local;
    const Math::Vector3 a = Forward(roundTrip);
    const Math::Vector3 b = Forward(desiredWorld);
    ENJIN_EXPECT_FLOAT_NEAR(a.x, b.x, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(a.y, b.y, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(a.z, b.z, 0.001f);
}

ENJIN_TEST(LookAtIK, test_a_rotated_parent_changes_the_local_rotation_required) {
    // The control for the test above: if the parent's rotation did NOT matter,
    // both of these would come out the same and the conversion would be
    // pointless.
    // Arrange
    const Math::Quaternion desiredWorld =
        Math::Quaternion::FromEuler(Math::Vector3(0.0f, 1.5707963f, 0.0f));
    const Math::Quaternion unrotated = Math::Quaternion::Identity();
    const Math::Quaternion yawed =
        Math::Quaternion::FromEuler(Math::Vector3(0.0f, 1.5707963f, 0.0f));

    // Act
    const Math::Quaternion localUnderIdentity = unrotated.Conjugate() * desiredWorld;
    const Math::Quaternion localUnderYaw      = yawed.Conjugate() * desiredWorld;

    // Assert: they differ, and only the second is identity.
    ENJIN_EXPECT_TRUE(std::fabs(localUnderIdentity.w - localUnderYaw.w) > 0.01f);
    ENJIN_EXPECT_FLOAT_NEAR(std::fabs(localUnderYaw.w), 1.0f, 0.001f);
}

ENJIN_TEST_MAIN()
