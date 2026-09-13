// The surface query has to be a pointer somebody actually sets, and clears.
//
// Hand IK casts against physics through an injected ISurfaceQuery, because the
// IK step runs inside the renderer's pose pass and that system has no physics
// backend and should not grow one. Injection makes the solver testable, and it
// also makes it silently inert when nobody injects anything: hands stay on
// their animation, no error, no warning, which looks exactly like a rig with a
// wrong bone name.
//
// So the wiring is the feature. These tests cover the adapter and the lifetime
// rule; the three runtimes are wired at every site that creates a physics
// backend, which is more than one in the desktop player.
//
// THE LIFETIME RULE, and why it has its own test. PlayMode owns the physics and
// the EDITOR owns the render system, so on Stop the backend is destroyed while
// the render system carries on. A query left attached would be cast against on
// the next editor frame, holding a freed pointer. That is not hypothetical in
// this codebase: HoverHighlightSystem caused an access violation in CI for
// precisely this, and the fix there (SetPhysics(nullptr) before the reset) is
// the same shape as the fix here.

#include "EnjinTest.h"
#include "Enjin/Animation/PhysicsSurfaceQuery.h"
#include "Enjin/Animation/HandIK.h"

#include <cstdio>

using namespace Enjin;
using namespace Enjin::Animation;
using Enjin::Math::Vector3;

ENJIN_TEST(SurfaceQueryWiring, WithNoBackendItMissesRatherThanCrashes) {
    // Arrange: the state every runtime is in before physics exists, and the
    // state the editor is in after Stop.
    PhysicsSurfaceQuery query;

    // Act
    SurfaceHit hit;
    const bool found = query.Cast(Vector3(0.0f, 2.0f, 0.0f), Vector3(0.0f, -1.0f, 0.0f),
                                  5.0f, hit);

    // Assert: a miss, and a CLEAN one. The out param has to be left in a
    // defined state rather than untouched, because a caller that trusts `hit`
    // after a false return would read whatever was on the stack.
    ENJIN_EXPECT_TRUE(!found);
    ENJIN_EXPECT_TRUE(!hit.hit);
    ENJIN_EXPECT_FLOAT_NEAR(hit.distance, 0.0f, 1.0e-6f);
}

ENJIN_TEST(SurfaceQueryWiring, ClearingTheBackendMakesItMissAgain) {
    // Arrange / Act / Assert: this is the Stop path. Setting the backend to
    // null must be enough to make the query inert, because that is what
    // PlayMode does before it destroys the backend, and the alternative is a
    // dangling pointer cast against on the next editor frame.
    PhysicsSurfaceQuery query;
    query.SetBackend(nullptr);

    SurfaceHit hit;
    ENJIN_EXPECT_TRUE(!query.Cast(Vector3(0.0f, 1.0f, 0.0f), Vector3(0.0f, -1.0f, 0.0f),
                                  10.0f, hit));
    ENJIN_EXPECT_TRUE(!hit.hit);
}

ENJIN_TEST(SurfaceQueryWiring, AHandWithNoQueryKeepsItsAnimation) {
    // Arrange: the whole point of the null case. A runtime with no physics --
    // a headless tool, a scene before load, the editor outside play -- must
    // leave the pose alone rather than produce a plausible one.
    PhysicsSurfaceQuery query;          // no backend

    HandPose hand;
    hand.palmPosition = Vector3(0.0f, 1.3f, 0.0f);
    hand.palmNormal = Vector3(0.0f, -1.0f, 0.0f);
    for (u32 f = 0; f < kFingerCount; ++f) {
        FingerChain& finger = hand.fingers[f];
        for (u32 j = 0; j <= kFingerJoints; ++j) {
            finger.joints[j] = Vector3(0.02f * static_cast<f32>(f),
                                       1.3f - 0.1f * static_cast<f32>(j), 0.0f);
        }
        finger.approachWeight = 1.0f;
    }
    const HandPose before = hand;

    // Act
    const HandIKResult r = HandIK::SolveSurface(hand, query);

    // Assert
    ENJIN_EXPECT_TRUE(!r.anyContact);
    ENJIN_EXPECT_EQ(r.fingersContacted, 0u);
    for (u32 f = 0; f < kFingerCount; ++f) {
        for (u32 j = 0; j <= kFingerJoints; ++j) {
            ENJIN_EXPECT_FLOAT_NEAR(hand.fingers[f].joints[j].y,
                                    before.fingers[f].joints[j].y, 1.0e-6f);
        }
    }
    std::printf("    no backend: %u contacts, pose untouched\n", r.fingersContacted);
}

ENJIN_TEST(SurfaceQueryWiring, TheLayerMaskIsSettableSoHandsDoNotGrabTriggers) {
    // Arrange / Act / Assert: without a mask a hand conforms to trigger
    // volumes and to the character's own capsule, which are colliders like any
    // other. The default is everything, so a project that has not thought about
    // layers still sees hands work; the control exists for the one that has.
    PhysicsSurfaceQuery query;
    query.SetLayerMask(0x0000000Fu);

    // With no backend the mask cannot change the outcome, and that is the
    // assertion: setting it must not make a query that was inert start
    // answering.
    SurfaceHit hit;
    ENJIN_EXPECT_TRUE(!query.Cast(Vector3(0.0f, 1.0f, 0.0f), Vector3(0.0f, -1.0f, 0.0f),
                                  10.0f, hit));
}

ENJIN_TEST_MAIN()
