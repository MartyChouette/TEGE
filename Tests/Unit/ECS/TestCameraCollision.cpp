// The camera must not end up on the far side of a wall.
//
// ControllerComponent has carried enableCameraCollision (defaulting ON) and
// cameraCollisionRadius since the component was written, both serialized, both
// shown in the inspector -- and nothing in the engine read either one. A
// third-person camera walked straight through geometry in every runtime while
// the editor said collision was enabled.
//
// ResolveCameraCollision casts from the look target toward the camera; this
// exercises the geometry it applies to the result, which is where the rules
// that matter live.
#include "EnjinTest.h"
#include "Enjin/ECS/Systems/ControllerSystem.h"

using namespace Enjin;
using namespace Enjin::ECS;

namespace {

// Player's eyes at the origin, camera four units back along +Z. The shape every
// case below starts from.
const Math::Vector3 kLook(0.0f, 0.0f, 0.0f);
const Math::Vector3 kDesired(0.0f, 0.0f, 4.0f);

f32 DistanceFromLook(const Math::Vector3& p) {
    return (p - kLook).Length();
}

} // namespace

ENJIN_TEST(CameraCollision, NothingInTheWayLeavesTheCameraWhereItWanted) {
    // Arrange / Act: no hit at all, the common case every frame in the open.
    const Math::Vector3 got = PullCameraToHit(kLook, kDesired, false, 0.0f, 0.3f);

    // Assert
    ENJIN_EXPECT_TRUE(std::fabs(got.z - 4.0f) < 1e-4f);
}

ENJIN_TEST(CameraCollision, AWallPullsTheCameraInFrontOfIt) {
    // Arrange: a wall 2 units back, camera radius 0.3. This is the bug: before
    // the fix the camera stayed at 4 and rendered from inside the wall.
    const f32 radius = 0.3f;

    // Act
    const Math::Vector3 got = PullCameraToHit(kLook, kDesired, true, 2.0f, radius);

    // Assert: short of the wall by the radius.
    ENJIN_EXPECT_TRUE(std::fabs(DistanceFromLook(got) - 1.7f) < 1e-3f);
}

ENJIN_TEST(CameraCollision, ThePulledCameraStaysOnTheOriginalSightLine) {
    // Arrange: an off-axis camera, so a bug that rebuilt the direction wrong
    // would show as a swung camera rather than a wrong distance.
    const Math::Vector3 desired(3.0f, 4.0f, 0.0f);   // length 5

    // Act
    const Math::Vector3 got = PullCameraToHit(kLook, desired, true, 2.5f, 0.5f);

    // Assert: same direction (2.0 along the 3-4-5 line = 1.2, 1.6), pulled to 2.0.
    ENJIN_EXPECT_TRUE(std::fabs(got.x - 1.2f) < 1e-3f);
    ENJIN_EXPECT_TRUE(std::fabs(got.y - 1.6f) < 1e-3f);
    ENJIN_EXPECT_TRUE(std::fabs(DistanceFromLook(got) - 2.0f) < 1e-3f);
}

ENJIN_TEST(CameraCollision, AWallFlushAgainstThePlayerDoesNotPutTheCameraBehindTheEyes) {
    // Arrange: hit distance smaller than the radius. Naive subtraction gives a
    // NEGATIVE distance, which flips the camera through the look target and
    // renders the player's own face from inside.
    // Act
    const Math::Vector3 got = PullCameraToHit(kLook, kDesired, true, 0.1f, 0.5f);

    // Assert: still in front, on the near side.
    ENJIN_EXPECT_TRUE(got.z > 0.0f);
    ENJIN_EXPECT_TRUE(DistanceFromLook(got) >= 0.05f);
}

ENJIN_TEST(CameraCollision, AHitBeyondTheCameraIsNotInTheWay) {
    // Arrange: the ray is capped at the camera distance, but a backend that
    // reports a hit further out must not PUSH the camera outward.
    // Act
    const Math::Vector3 got = PullCameraToHit(kLook, kDesired, true, 9.0f, 0.3f);

    // Assert: unchanged, not moved to 8.7.
    ENJIN_EXPECT_TRUE(std::fabs(got.z - 4.0f) < 1e-4f);
}

ENJIN_TEST(CameraCollision, ACameraSittingOnTheLookTargetIsNotADivideByZero) {
    // Arrange: first-person distance, which the zoom-to-first-person work makes
    // a real state rather than a theoretical one.
    // Act
    const Math::Vector3 got = PullCameraToHit(kLook, kLook, true, 1.0f, 0.3f);

    // Assert: finite, and left alone.
    ENJIN_EXPECT_TRUE(got.x == got.x && got.y == got.y && got.z == got.z);
    ENJIN_EXPECT_TRUE(DistanceFromLook(got) < 1e-3f);
}

ENJIN_TEST_MAIN()
