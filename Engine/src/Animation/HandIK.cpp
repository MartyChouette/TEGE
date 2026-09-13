#include "Enjin/Animation/HandIK.h"

#include <algorithm>
#include <cmath>

namespace Enjin {
namespace Animation {

namespace {

Math::Vector3 MixPoint(const Math::Vector3& a, const Math::Vector3& b, f32 t) {
    return Math::Vector3(a.x + (b.x - a.x) * t,
                         a.y + (b.y - a.y) * t,
                         a.z + (b.z - a.z) * t);
}

Math::Vector3 SafeNormalize(const Math::Vector3& v, const Math::Vector3& fallback) {
    const f32 len = v.Length();
    if (len < 1.0e-6f) return fallback;
    return Math::Vector3(v.x / len, v.y / len, v.z / len);
}

// Blend a solved chain back over the animated one, joint by joint.
//
// Blending the TARGET and re-solving would be cheaper and wrong: FABRIK is not
// linear in its target, so a half-way target does not give a half-way pose, and
// the knuckle would lag differently from the tip. Blending the solved positions
// gives a finger that travels smoothly from where the animation put it to where
// the surface wants it.
void BlendChain(FingerChain& finger,
                const std::array<Math::Vector3, kFingerJoints + 1>& solved,
                f32 weight) {
    for (u32 i = 0; i <= kFingerJoints; ++i) {
        finger.joints[i] = MixPoint(finger.joints[i], solved[i], weight);
    }
}

// Break the straight-chain degeneracy before handing FABRIK the target.
//
// FABRIK cannot bend a chain that is perfectly straight and aimed exactly at
// its target: every direction it computes lies on the one axis, so the forward
// pass pulls the tip in and the backward pass walks straight back out, forever.
// A flat hand held palm-down over a counter is precisely that, which makes it
// the common case and not a corner one.
//
// The fix is to give the chain somewhere to bend. Nudging the INTERMEDIATE
// joints a millimetre off the axis costs nothing visually (FABRIK immediately
// re-solves the exact bone lengths) and it is the difference between a finger
// that bends and a finger that never converges.
void BreakColinearity(std::array<Math::Vector3, kFingerJoints + 1>& chain,
                      const Math::Vector3& target,
                      const Math::Vector3& curlHint) {
    const Math::Vector3 axis = target - chain[0];
    const f32 axisLen = axis.Length();
    if (axisLen < 1.0e-6f) return;
    const Math::Vector3 unit(axis.x / axisLen, axis.y / axisLen, axis.z / axisLen);

    // Only a chain that is already essentially straight along the axis needs
    // this. A finger that is even slightly bent already tells FABRIK which way
    // to go, and nudging it would fight the animation.
    f32 maxOffAxis = 0.0f;
    for (u32 i = 1; i < kFingerJoints; ++i) {
        const Math::Vector3 rel = chain[i] - chain[0];
        const f32 along = rel.Dot(unit);
        const Math::Vector3 perp(rel.x - unit.x * along,
                                 rel.y - unit.y * along,
                                 rel.z - unit.z * along);
        maxOffAxis = std::max(maxOffAxis, perp.Length());
    }
    if (maxOffAxis > 1.0e-3f) return;

    // The bend direction: the rigger's, made perpendicular to the axis. A hint
    // that is zero or parallel to the axis carries no information, so fall back
    // to any stable perpendicular. Arbitrary, but deterministic, which is what
    // matters: a finger that bowed a different way each frame would shimmer.
    Math::Vector3 bend = curlHint;
    const f32 along = bend.Dot(unit);
    bend = Math::Vector3(bend.x - unit.x * along,
                         bend.y - unit.y * along,
                         bend.z - unit.z * along);
    if (bend.Length() < 1.0e-5f) {
        const Math::Vector3 seed = (std::fabs(unit.y) < 0.9f)
            ? Math::Vector3(0.0f, 1.0f, 0.0f) : Math::Vector3(1.0f, 0.0f, 0.0f);
        bend = unit.Cross(seed);
    }
    bend = SafeNormalize(bend, Math::Vector3(1.0f, 0.0f, 0.0f));

    for (u32 i = 1; i < kFingerJoints; ++i) {
        chain[i] = Math::Vector3(chain[i].x + bend.x * 1.0e-3f,
                                 chain[i].y + bend.y * 1.0e-3f,
                                 chain[i].z + bend.z * 1.0e-3f);
    }
}

} // namespace

HandIKResult HandIK::SolveSurface(HandPose& hand,
                                  const ISurfaceQuery& surfaces,
                                  const HandIKSettings& settings) {
    HandIKResult result;
    const Math::Vector3 down = SafeNormalize(hand.palmNormal, Math::Vector3(0.0f, -1.0f, 0.0f));

    f32 penetrationTotal = 0.0f;
    u32 penetrationSamples = 0;

    for (u32 f = 0; f < kFingerCount; ++f) {
        FingerChain& finger = hand.fingers[f];
        finger.contacted = false;

        const f32 weight = HandIK::EffectiveWeight(finger);
        if (weight <= 0.0001f) continue;

        // Cast from the knuckle, not from the tip.
        //
        // The tip is wherever the animation currently has it, which may already
        // be through the counter. Casting from there finds the far side of the
        // worktop, or nothing, and either way it is asking about the wrong
        // place. The knuckle is the fixed end of this chain and the only point
        // that is reliably on the correct side of the surface.
        const Math::Vector3 knuckle = finger.joints[0];
        const f32 reach = finger.Reach();
        if (reach < 1.0e-5f) continue;

        SurfaceHit hit;
        if (!surfaces.Cast(knuckle, down, reach + settings.castMargin, hit) || !hit.hit) {
            // Nothing under this finger. Leave it on the animation: a hand
            // over the gap between two shelves has fingers that hang, and
            // forcing them to a made-up plane is worse than not trying.
            continue;
        }

        // Found, but out of reach.
        //
        // The cast deliberately looks a little past the finger's own length, so
        // that a surface just barely out of range is DETECTED rather than
        // invisible. But detecting it is not touching it: a 0.25 m finger over
        // a counter 0.30 m below cannot rest on it, and reporting contact there
        // is a lie that shows up as a stretched finger and a hand that claims
        // to be resting on something it is hovering over.
        if (hit.distance > reach) continue;

        // The tip goes ON the surface. Not above it by an epsilon: a fingertip
        // resting on a counter is touching the counter, and a visible gap is
        // the single thing that makes a conformed hand look wrong.
        std::array<Math::Vector3, kFingerJoints + 1> solved = finger.joints;
        const Math::Vector3 tipTarget = hit.point;

        // How far the animation was pushing the tip past the surface. Reported
        // because it is the honest measure of how much work the IK is doing,
        // and a hand that is resolving twenty centimetres every frame is a hand
        // whose animation is fighting the surface.
        const f32 before = (finger.joints[kFingerJoints] - knuckle).Length();
        const f32 after = (tipTarget - knuckle).Length();
        if (before > after) {
            penetrationTotal += (before - after);
            ++penetrationSamples;
        }

        BreakColinearity(solved, tipTarget, finger.curlDirection);
        std::vector<Math::Vector3> chain(solved.begin(), solved.end());
        FABRIK::Solve(chain, tipTarget, settings.iterations);
        for (u32 i = 0; i <= kFingerJoints; ++i) solved[i] = chain[i];

        BlendChain(finger, solved, weight);

        finger.contacted = true;
        finger.contactPoint = hit.point;
        finger.contactNormal = hit.normal;
        ++result.fingersContacted;
    }

    result.anyContact = result.fingersContacted > 0;
    result.meanPenetrationResolved =
        penetrationSamples ? (penetrationTotal / static_cast<f32>(penetrationSamples)) : 0.0f;
    return result;
}

HandIKResult HandIK::SolveEdge(HandPose& hand,
                               const Math::Vector3& edgePoint,
                               const Math::Vector3& edgeDirection,
                               const Math::Vector3& surfaceNormal,
                               const HandIKSettings& settings) {
    HandIKResult result;

    const Math::Vector3 along = SafeNormalize(edgeDirection, Math::Vector3(1.0f, 0.0f, 0.0f));
    const Math::Vector3 up = SafeNormalize(surfaceNormal, Math::Vector3(0.0f, 1.0f, 0.0f));

    // The direction fingers curl once they pass the edge: along the face that
    // drops away from it, which is perpendicular to both the edge and the top.
    Math::Vector3 over = along.Cross(up);
    over = SafeNormalize(over, Math::Vector3(0.0f, 0.0f, 1.0f));

    // Point the curl AWAY from the hand, so a hand approaching a shelf from
    // either side curls its fingers down the far face rather than back into its
    // own palm.
    const Math::Vector3 palmToEdge = edgePoint - hand.palmPosition;
    if (over.Dot(palmToEdge) < 0.0f) {
        over = Math::Vector3(-over.x, -over.y, -over.z);
    }

    for (u32 f = 0; f < kFingerCount; ++f) {
        FingerChain& finger = hand.fingers[f];
        finger.contacted = false;

        const f32 weight = HandIK::EffectiveWeight(finger);
        if (weight <= 0.0001f) continue;

        const Math::Vector3 knuckle = finger.joints[0];
        const f32 reach = finger.Reach();
        if (reach < 1.0e-5f) continue;

        // Where on the edge this particular finger meets it: the closest point
        // along the line, so a hand laid at an angle has each finger take the
        // edge at its own spot instead of all five converging on one.
        const Math::Vector3 toKnuckle = knuckle - edgePoint;
        const f32 t = toKnuckle.Dot(along);
        const Math::Vector3 contact(edgePoint.x + along.x * t,
                                    edgePoint.y + along.y * t,
                                    edgePoint.z + along.z * t);

        const f32 distance = (contact - knuckle).Length();
        if (distance > reach + settings.castMargin) {
            // The edge is out of this finger's reach. A short finger on a long
            // hand genuinely does not make it, and pretending otherwise is how
            // you get a stretched little finger.
            continue;
        }

        // Fingers curl OVER the edge. The middle joint rests on the top face
        // near the edge and the tip continues down the far side, which is the
        // shape a hand on a shelf lip actually makes.
        std::array<Math::Vector3, kFingerJoints + 1> solved = finger.joints;
        const f32 boneLength = reach / static_cast<f32>(kFingerJoints);

        // Curl as far as the finger can actually afford.
        //
        // Reaching the edge and curling past it are two costs on one budget, and
        // checking only the first is how a short finger ends up with a target
        // it cannot reach: FABRIK then stretches toward it, the tip lands short
        // on the near side, and the solve reports contact for a finger that
        // never got over the lip. So the curl shrinks until the TARGET is
        // reachable, which is also what a real hand does -- a little finger on
        // a deep shelf curls less than a middle finger, because it has less
        // finger left after getting there.
        f32 curl = boneLength * settings.edgeCurl;
        Math::Vector3 tipTarget;
        for (int attempt = 0; attempt < 8; ++attempt) {
            tipTarget = Math::Vector3(contact.x + over.x * curl - up.x * curl,
                                      contact.y + over.y * curl - up.y * curl,
                                      contact.z + over.z * curl - up.z * curl);
            if ((tipTarget - knuckle).Length() <= reach) break;
            curl *= 0.6f;
        }
        if ((tipTarget - knuckle).Length() > reach) {
            // Even with no curl at all the edge is out of range. Leave the
            // finger on the animation rather than stretching it.
            continue;
        }

        BreakColinearity(solved, tipTarget, finger.curlDirection);
        std::vector<Math::Vector3> chain(solved.begin(), solved.end());
        FABRIK::Solve(chain, tipTarget, settings.iterations);
        for (u32 i = 0; i <= kFingerJoints; ++i) solved[i] = chain[i];

        BlendChain(finger, solved, weight);

        finger.contacted = true;
        finger.contactPoint = contact;
        finger.contactNormal = up;
        ++result.fingersContacted;
    }

    result.anyContact = result.fingersContacted > 0;
    return result;
}

void HandIK::AdvanceWeights(FingerChain& finger, f32 approachGoal, f32 releaseGoal,
                            f32 approachRate, f32 releaseRate, f32 dt) {
    const f32 clampedDt = dt < 0.0f ? 0.0f : dt;

    auto approach = [](f32 current, f32 goal, f32 rate, f32 step) {
        const f32 maxDelta = rate * step;
        const f32 delta = goal - current;
        if (delta > maxDelta) return current + maxDelta;
        if (delta < -maxDelta) return current - maxDelta;
        return goal;
    };

    // Rate-limited rather than exponential. An exponential approach never
    // arrives, so a finger would be permanently a fraction off the counter and
    // the contact would never read as solid; and the time to "close enough"
    // would depend on the frame rate.
    finger.approachWeight = std::clamp(
        approach(finger.approachWeight, approachGoal, approachRate, clampedDt), 0.0f, 1.0f);
    finger.releaseWeight = std::clamp(
        approach(finger.releaseWeight, releaseGoal, releaseRate, clampedDt), 0.0f, 1.0f);
}

} // namespace Animation
} // namespace Enjin
