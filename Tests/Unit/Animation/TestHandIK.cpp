// Hands that settle on a surface, finger by finger.
//
// The solver takes positions and gives positions, and asks for surface hits
// through an interface, so these tests put a hand against a mathematically
// exact plane and check the answer to the millimetre. No skeleton, no scene, no
// physics backend, no renderer.
//
// Worth stating what is NOT covered: whether the result looks right on a rigged
// character. That is what the demo is for. What is covered is every claim the
// solver makes about where a fingertip ends up.

#include "EnjinTest.h"
#include "Enjin/Animation/HandIK.h"
#include "Enjin/ECS/Components/HandIKComponent.h"
#include "Enjin/ECS/World.h"
#include "Enjin/Scene/SceneSerializer.h"
#include "Enjin/ECS/Components/Transform.h"
#include <string>

#include <cstdio>

using namespace Enjin;
using namespace Enjin::Animation;
using Enjin::Math::Vector3;

namespace {

// An infinite horizontal plane at a given height. Exact, so a disagreement is
// the solver's.
class Plane : public ISurfaceQuery {
public:
    explicit Plane(f32 height) : m_Height(height) {}

    bool Cast(const Vector3& origin, const Vector3& direction,
              f32 maxDistance, SurfaceHit& out) const override {
        if (direction.y >= -1.0e-6f) return false;          // not pointing down
        const f32 t = (origin.y - m_Height) / -direction.y;
        if (t < 0.0f || t > maxDistance) return false;
        out.hit = true;
        out.distance = t;
        out.point = Vector3(origin.x + direction.x * t,
                            m_Height,
                            origin.z + direction.z * t);
        out.normal = Vector3(0.0f, 1.0f, 0.0f);
        return true;
    }

private:
    f32 m_Height;
};

// Nothing anywhere. A hand over a gap.
class Empty : public ISurfaceQuery {
public:
    bool Cast(const Vector3&, const Vector3&, f32, SurfaceHit&) const override {
        return false;
    }
};

// A hand held palm-down with five fingers of DIFFERENT lengths, which is the
// whole point: a single hand target with a canned pose cannot put five
// differently sized fingers on one plane.
HandPose FlatHand(f32 palmHeight, f32 reachPerFinger[kFingerCount]) {
    HandPose hand;
    hand.palmPosition = Vector3(0.0f, palmHeight, 0.0f);
    hand.palmNormal = Vector3(0.0f, -1.0f, 0.0f);

    for (u32 f = 0; f < kFingerCount; ++f) {
        FingerChain& finger = hand.fingers[f];
        const f32 bone = reachPerFinger[f] / static_cast<f32>(kFingerJoints);
        const f32 x = -0.04f + 0.02f * static_cast<f32>(f);
        for (u32 j = 0; j <= kFingerJoints; ++j) {
            // Straight down from the knuckle, so an unconstrained finger would
            // drive its tip through anything below it.
            finger.joints[j] = Vector3(x, palmHeight - bone * static_cast<f32>(j), 0.0f);
        }
        finger.approachWeight = 1.0f;
        finger.releaseWeight = 0.0f;
    }
    return hand;
}

f32 Abs(f32 v) { return v < 0.0f ? -v : v; }

} // namespace

ENJIN_TEST(HandIK, EveryFingerStopsAtTheSurfaceWhateverItsLength) {
    // Arrange: a counter at y = 1.00, a palm at 1.30, and five fingers whose
    // reaches straddle the 0.30 gap. Two are too short to touch it, three would
    // drive straight through.
    f32 reaches[kFingerCount] = { 0.18f, 0.34f, 0.38f, 0.33f, 0.25f };
    HandPose hand = FlatHand(1.30f, reaches);
    const Plane counter(1.00f);

    // Act
    const HandIKResult r = HandIK::SolveSurface(hand, counter);

    // Assert
    std::printf("    %u of 5 fingers contacted, mean penetration resolved %.3f m\n",
                r.fingersContacted, r.meanPenetrationResolved);

    for (u32 f = 0; f < kFingerCount; ++f) {
        const FingerChain& finger = hand.fingers[f];
        const f32 tipY = finger.joints[kFingerJoints].y;
        std::printf("    finger %u reach %.2f  tip y %.4f  %s\n",
                    f, reaches[f], tipY, finger.contacted ? "contact" : "no contact");

        if (finger.contacted) {
            // ON the surface, not above it. A visible gap is the one thing that
            // makes a conformed hand look wrong.
            ENJIN_EXPECT_FLOAT_NEAR(tipY, 1.00f, 0.002f);
        } else {
            // A finger that cannot reach must be left where the animation put
            // it, not stretched down to the plane.
            ENJIN_EXPECT_TRUE(tipY > 1.00f);
        }
    }

    // The three that can reach do; the two that cannot, do not. This is the
    // assertion a single hand target cannot satisfy.
    ENJIN_EXPECT_EQ(r.fingersContacted, 3u);
}

ENJIN_TEST(HandIK, AFingerOverNothingKeepsItsAnimatedPose) {
    // Arrange
    f32 reaches[kFingerCount] = { 0.30f, 0.30f, 0.30f, 0.30f, 0.30f };
    HandPose hand = FlatHand(1.30f, reaches);
    const HandPose before = hand;
    const Empty nothing;

    // Act
    const HandIKResult r = HandIK::SolveSurface(hand, nothing);

    // Assert: a hand over the gap between two shelves has fingers that hang.
    // Forcing them onto an invented plane is worse than not trying, and a
    // solver that quietly produced a plausible pose here would be the exact
    // failure this codebase keeps finding.
    ENJIN_EXPECT_TRUE(!r.anyContact);
    ENJIN_EXPECT_EQ(r.fingersContacted, 0u);
    for (u32 f = 0; f < kFingerCount; ++f) {
        for (u32 j = 0; j <= kFingerJoints; ++j) {
            ENJIN_EXPECT_FLOAT_NEAR(hand.fingers[f].joints[j].y, before.fingers[f].joints[j].y,
                                    1.0e-5f);
        }
    }
}

ENJIN_TEST(HandIK, WeightZeroLeavesTheHandCompletelyAlone) {
    // Arrange: a surface well within reach, and every finger switched off.
    f32 reaches[kFingerCount] = { 0.35f, 0.35f, 0.35f, 0.35f, 0.35f };
    HandPose hand = FlatHand(1.30f, reaches);
    for (auto& finger : hand.fingers) finger.approachWeight = 0.0f;
    const HandPose before = hand;
    const Plane counter(1.10f);

    // Act
    const HandIKResult r = HandIK::SolveSurface(hand, counter);

    // Assert: an off switch has to be off. A weight of zero that still nudged
    // the pose would make "disable this" and "reduce this" the same control.
    ENJIN_EXPECT_TRUE(!r.anyContact);
    for (u32 f = 0; f < kFingerCount; ++f) {
        for (u32 j = 0; j <= kFingerJoints; ++j) {
            ENJIN_EXPECT_FLOAT_NEAR(hand.fingers[f].joints[j].y,
                                    before.fingers[f].joints[j].y, 1.0e-5f);
        }
    }
}

ENJIN_TEST(HandIK, HalfWeightPutsTheTipHalfWayThere) {
    // Arrange: one configuration solved at full weight and again at half.
    f32 reaches[kFingerCount] = { 0.35f, 0.35f, 0.35f, 0.35f, 0.35f };
    const Plane counter(1.10f);

    HandPose full = FlatHand(1.30f, reaches);
    HandIK::SolveSurface(full, counter);

    HandPose half = FlatHand(1.30f, reaches);
    for (auto& finger : half.fingers) finger.approachWeight = 0.5f;
    HandIK::SolveSurface(half, counter);

    const HandPose animated = FlatHand(1.30f, reaches);

    // Assert: halfway between the animated tip and the fully solved one.
    for (u32 f = 0; f < kFingerCount; ++f) {
        const f32 a = animated.fingers[f].joints[kFingerJoints].y;
        const f32 s = full.fingers[f].joints[kFingerJoints].y;
        const f32 h = half.fingers[f].joints[kFingerJoints].y;
        ENJIN_EXPECT_FLOAT_NEAR(h, (a + s) * 0.5f, 0.005f);
    }
    std::printf("    animated %.3f, half %.3f, full %.3f (finger 2)\n",
                animated.fingers[2].joints[kFingerJoints].y,
                half.fingers[2].joints[kFingerJoints].y,
                full.fingers[2].joints[kFingerJoints].y);
}

ENJIN_TEST(HandIK, ReleaseSubtractsFromApproachRatherThanReplacingIt) {
    // Arrange
    FingerChain finger;

    // Act / Assert: a hand 80% onto a counter and 30% through letting go is at
    // 0.5, not at 0.3. Treating release as its own blend makes a releasing hand
    // jump back to the animation and then leave again.
    finger.approachWeight = 0.8f;
    finger.releaseWeight = 0.0f;
    ENJIN_EXPECT_FLOAT_NEAR(HandIK::EffectiveWeight(finger), 0.8f, 1.0e-5f);

    finger.releaseWeight = 0.3f;
    ENJIN_EXPECT_FLOAT_NEAR(HandIK::EffectiveWeight(finger), 0.5f, 1.0e-5f);

    finger.releaseWeight = 1.0f;
    ENJIN_EXPECT_FLOAT_NEAR(HandIK::EffectiveWeight(finger), 0.0f, 1.0e-5f);

    // And it never goes negative, which would invert the blend and pull the
    // finger further from the surface than the animation ever had it.
    finger.approachWeight = 0.2f;
    finger.releaseWeight = 0.9f;
    ENJIN_EXPECT_FLOAT_NEAR(HandIK::EffectiveWeight(finger), 0.0f, 1.0e-5f);
}

ENJIN_TEST(HandIK, WeightsAdvanceAtAFixedRateAndActuallyArrive) {
    // Arrange
    FingerChain finger;
    finger.approachWeight = 0.0f;

    // Act: 2.0 per second for 0.25 s should land exactly on 0.5.
    HandIK::AdvanceWeights(finger, 1.0f, 0.0f, 2.0f, 4.0f, 0.25f);

    // Assert
    ENJIN_EXPECT_FLOAT_NEAR(finger.approachWeight, 0.5f, 1.0e-5f);

    // And it arrives. An exponential approach never does, so a finger would sit
    // permanently a fraction off the counter and the contact would never read
    // as solid.
    for (int i = 0; i < 10; ++i) HandIK::AdvanceWeights(finger, 1.0f, 0.0f, 2.0f, 4.0f, 0.1f);
    ENJIN_EXPECT_FLOAT_NEAR(finger.approachWeight, 1.0f, 1.0e-6f);

    // Overshoot is clamped rather than wrapped.
    HandIK::AdvanceWeights(finger, 1.0f, 0.0f, 100.0f, 100.0f, 1.0f);
    ENJIN_EXPECT_FLOAT_NEAR(finger.approachWeight, 1.0f, 1.0e-6f);
}

ENJIN_TEST(HandIK, FingersCurlOverAnEdgeInsteadOfPressingOntoIt) {
    // Arrange: a shelf lip running along X at y = 1.20, z = 0.30, with the hand
    // approaching from z = 0 (the near side).
    f32 reaches[kFingerCount] = { 0.32f, 0.36f, 0.38f, 0.35f, 0.30f };
    HandPose hand = FlatHand(1.30f, reaches);
    hand.palmPosition = Vector3(0.0f, 1.30f, 0.0f);

    const Vector3 edgePoint(0.0f, 1.20f, 0.30f);
    const Vector3 edgeDir(1.0f, 0.0f, 0.0f);
    const Vector3 topNormal(0.0f, 1.0f, 0.0f);

    // Act
    const HandIKResult r = HandIK::SolveEdge(hand, edgePoint, edgeDir, topNormal);

    // Assert
    std::printf("    %u of 5 fingers took the edge\n", r.fingersContacted);
    ENJIN_EXPECT_TRUE(r.anyContact);

    u32 past = 0;
    for (u32 f = 0; f < kFingerCount; ++f) {
        const FingerChain& finger = hand.fingers[f];
        if (!finger.contacted) continue;
        const Vector3 tip = finger.joints[kFingerJoints];
        std::printf("    finger %u tip (%.3f, %.3f, %.3f)\n", f, tip.x, tip.y, tip.z);

        // At the edge at worst, and below the top face. Reaching the edge and
        // curling past it compete for the same finger length, so a finger that
        // only just makes it has almost nothing left to curl with and its tip
        // ends AT the lip. That is correct, and the first version of this
        // assertion demanded every contacted finger clear the edge, which would
        // only have been satisfiable by letting short fingers stretch.
        ENJIN_EXPECT_TRUE(tip.z > 0.295f);
        ENJIN_EXPECT_TRUE(tip.y < 1.20f);
        if (tip.z > 0.30f) ++past;
    }

    // The fingers with length to spare do clear it. If none did, the curl is
    // not happening at all and this is a point solve wearing an edge's name.
    std::printf("    %u fingers cleared the lip\n", past);
    ENJIN_EXPECT_TRUE(past >= 3);
}

ENJIN_TEST(HandIK, EachFingerTakesTheEdgeAtItsOwnPointAlongIt) {
    // Arrange: the same shelf, but the fingers spread along the edge, which is
    // what a hand laid on a railing looks like.
    f32 reaches[kFingerCount] = { 0.34f, 0.34f, 0.34f, 0.34f, 0.34f };
    HandPose hand = FlatHand(1.30f, reaches);

    const Vector3 edgePoint(0.0f, 1.20f, 0.30f);
    const Vector3 edgeDir(1.0f, 0.0f, 0.0f);

    // Act
    HandIK::SolveEdge(hand, edgePoint, edgeDir, Vector3(0.0f, 1.0f, 0.0f));

    // Assert: contact X values must be spread, not converged on one spot. All
    // five meeting the edge at the same point is the failure mode of treating
    // an edge as a point, and it reads as a fist.
    f32 minX = 1.0e9f, maxX = -1.0e9f;
    for (const auto& finger : hand.fingers) {
        if (!finger.contacted) continue;
        minX = finger.contactPoint.x < minX ? finger.contactPoint.x : minX;
        maxX = finger.contactPoint.x > maxX ? finger.contactPoint.x : maxX;
    }
    std::printf("    contacts spread from x %.3f to %.3f\n", minX, maxX);
    ENJIN_EXPECT_TRUE(Abs(maxX - minX) > 0.05f);
}

ENJIN_TEST(HandIK, TheChainNeverStretchesPastItsBoneLengths) {
    // Arrange: a surface deliberately further than the fingers can reach, with
    // the weights fully on.
    f32 reaches[kFingerCount] = { 0.20f, 0.20f, 0.20f, 0.20f, 0.20f };
    HandPose hand = FlatHand(1.30f, reaches);
    const HandPose before = hand;
    const Plane farCounter(0.60f);          // 0.70 m away, reach is 0.20

    // Act
    HandIK::SolveSurface(hand, farCounter);

    // Assert: bone lengths are preserved to a millimetre. A stretched finger is
    // the classic IK tell, and FABRIK only preserves length if the target is
    // reachable, so refusing the cast beyond reach is what protects this.
    for (u32 f = 0; f < kFingerCount; ++f) {
        for (u32 j = 0; j < kFingerJoints; ++j) {
            const f32 originalLen =
                (before.fingers[f].joints[j + 1] - before.fingers[f].joints[j]).Length();
            const f32 solvedLen =
                (hand.fingers[f].joints[j + 1] - hand.fingers[f].joints[j]).Length();
            ENJIN_EXPECT_FLOAT_NEAR(solvedLen, originalLen, 0.001f);
        }
    }
}

// A component the system reads and nothing saves is a component that does not
// exist.
//
// This engine shipped exactly that within the last day: MaterialComponent's
// surfaceMaterial was read by the acoustics, drove the whole room-materials
// demo, and was serialized by nothing, so a scene could not author it and it
// did not survive a save. The demo it was built for loaded as three identical
// rooms and proved the opposite of its point.
//
// HandIKComponent is the same shape of risk, only worse: ten bone names and
// five curl directions per hand, none of which can be re-derived, all of which
// are tedious to re-enter, and whose absence shows up not as an error but as a
// hand that simply stops conforming.
ENJIN_TEST(HandIK, TheComponentSurvivesASave) {
    // Arrange: distinctive values in every field, so a default-valued field
    // cannot pass by coincidence.
    ECS::World source;
    const ECS::Entity e = source.CreateEntity();
    source.AddComponent<ECS::TransformComponent>(e, ECS::TransformComponent{});

    ECS::HandIKComponent& hand = source.AddComponent<ECS::HandIKComponent>(e);
    hand.handBoneName = "LeftHand";
    hand.palmNormalLocal = Vector3(0.0f, 0.0f, -1.0f);
    hand.mode = Animation::HandTargetMode::SurfaceEdge;
    hand.interactionTag = "shelf";
    hand.interactionRadius = 2.25f;
    hand.engageDistance = 0.42f;
    hand.edgeDirection = Vector3(0.0f, 0.0f, 1.0f);
    hand.approachRate = 3.5f;
    hand.releaseRate = 7.25f;
    hand.weight = 0.75f;

    const char* names[Animation::kFingerCount] =
        { "Thumb", "Index", "Middle", "Ring", "Little" };
    for (u32 f = 0; f < Animation::kFingerCount; ++f) {
        hand.fingers[f].proximal = std::string("LeftHand") + names[f] + "1";
        hand.fingers[f].intermediate = std::string("LeftHand") + names[f] + "2";
        hand.fingers[f].distal = std::string("LeftHand") + names[f] + "3";
        hand.fingers[f].tip = std::string("LeftHand") + names[f] + "4";
        hand.fingers[f].curlDirection = Vector3(0.1f * static_cast<f32>(f + 1), 0.5f, -0.25f);
    }

    // Act
    Scene::SceneSerializer writer(&source);
    const std::string text = writer.SaveToString();

    ECS::World loaded;
    Scene::SceneSerializer reader(&loaded);
    ENJIN_ASSERT_TRUE(reader.LoadFromString(text).success);

    // Assert
    ECS::Entity found = ECS::INVALID_ENTITY;
    for (ECS::Entity candidate : loaded.GetEntitiesWithComponent<ECS::HandIKComponent>()) {
        found = candidate;
        break;
    }
    ENJIN_ASSERT_TRUE(found != ECS::INVALID_ENTITY);

    const auto* out = loaded.GetComponent<ECS::HandIKComponent>(found);
    ENJIN_ASSERT_TRUE(out != nullptr);

    ENJIN_EXPECT_TRUE(out->handBoneName == "LeftHand");
    ENJIN_EXPECT_TRUE(out->interactionTag == "shelf");
    ENJIN_EXPECT_TRUE(out->mode == Animation::HandTargetMode::SurfaceEdge);
    ENJIN_EXPECT_FLOAT_NEAR(out->palmNormalLocal.z, -1.0f, 1.0e-5f);
    ENJIN_EXPECT_FLOAT_NEAR(out->interactionRadius, 2.25f, 1.0e-4f);
    ENJIN_EXPECT_FLOAT_NEAR(out->engageDistance, 0.42f, 1.0e-4f);
    ENJIN_EXPECT_FLOAT_NEAR(out->edgeDirection.z, 1.0f, 1.0e-5f);
    ENJIN_EXPECT_FLOAT_NEAR(out->approachRate, 3.5f, 1.0e-4f);
    ENJIN_EXPECT_FLOAT_NEAR(out->releaseRate, 7.25f, 1.0e-4f);
    ENJIN_EXPECT_FLOAT_NEAR(out->weight, 0.75f, 1.0e-4f);

    for (u32 f = 0; f < Animation::kFingerCount; ++f) {
        const std::string expected = std::string("LeftHand") + names[f] + "1";
        ENJIN_EXPECT_TRUE(out->fingers[f].proximal == expected);
        ENJIN_EXPECT_TRUE(out->fingers[f].tip == std::string("LeftHand") + names[f] + "4");
        ENJIN_EXPECT_FLOAT_NEAR(out->fingers[f].curlDirection.x,
                                0.1f * static_cast<f32>(f + 1), 1.0e-4f);
        ENJIN_EXPECT_FLOAT_NEAR(out->fingers[f].curlDirection.z, -0.25f, 1.0e-4f);
    }

    std::printf("    round trip kept %u fingers of bone names and curl directions\n",
                Animation::kFingerCount);
}

ENJIN_TEST_MAIN()
