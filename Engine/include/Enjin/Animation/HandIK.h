#pragma once
//
// Hands that find a surface and settle on it, finger by finger.
//
// This header is deliberately free of ECS, of the renderer, and of the physics
// backend. It takes positions in and gives positions out, and it asks for
// surface hits through an interface the caller supplies. That is what makes it
// testable without a skeleton, a scene, or a device, and it is why the tests can
// put a hand against a mathematically exact plane and check the answer to the
// millimetre.
//
// WHAT A HAND ON A SURFACE ACTUALLY IS
//
// Not one target. Five. A hand laid on a counter has each fingertip resting
// where its own finger happens to reach, and the fingers are different lengths,
// so a single hand target with a canned pose gives you a glove pressed into a
// shape. The cue that reads as "resting on" is that each fingertip stops AT the
// surface, and the ones that would have gone through it are the ones that bend.
//
// A shelf EDGE is the other case and it is not the same problem. Fingers curl
// over it: the tips go past the edge and down the far side while the middle
// joints rest on the top. A point target cannot express that, which is why
// SurfaceEdge exists alongside SurfacePoint rather than being approximated by
// it.
//
// APPROACH AND RELEASE ARE NOT ONE WEIGHT
//
// A hand reaching for a counter and a hand leaving it do not look alike. The
// reach leads with the fingertips and arrives; the release lets go from the tip
// backwards while the wrist has already moved on. One blend value run forwards
// and backwards gives a hand that retraces its own approach, which reads as a
// video played in reverse. So each finger carries its own approach and release
// weight and they are driven independently.

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/Animation/IKSolver.h"

#include <array>
#include <vector>

namespace Enjin {
namespace Animation {

// Five fingers, named, because "finger 3" is a bug waiting to happen when
// somebody reorders a rig.
enum class Finger : u8 { Thumb = 0, Index = 1, Middle = 2, Ring = 3, Little = 4 };
constexpr u32 kFingerCount = 5;
constexpr u32 kFingerJoints = 3;        // proximal, intermediate, distal

// What the hand is trying to settle on.
enum class HandTargetMode : u8 {
    // The historic behaviour: one entity, resolved by proximity and tag. Kept
    // because things that ARE objects (a lever, a doorknob) are targeted this
    // way and always were.
    Entity = 0,

    // A point on a surface with a normal: a counter top, a wall, a table. The
    // palm faces along -normal and fingertips settle where they land.
    SurfacePoint = 1,

    // A straight edge with a direction and a normal: a shelf lip, a railing, a
    // windowsill. Fingers curl OVER it rather than pressing onto it, which is a
    // different pose and not a special case of the point solve.
    SurfaceEdge = 2,

    // Work it out from what is actually under the hand, per frame.
    //
    // The two modes above are a property of the PROP, not of the hand, and
    // authoring them on the hand means a hand can only ever rest on one kind of
    // thing. Walk from a worktop to a railing and a SurfacePoint hand puts five
    // straight fingers through the bar; a SurfaceEdge hand curls over a flat
    // counter. Neither is a bug anyone can see in a scene file.
    //
    // Measured cost: four extra raycasts per hand per frame. See
    // HandIK::ClassifyContact.
    Auto = 3,
};

// One fingertip cast result.
struct SurfaceHit {
    bool hit = false;
    Math::Vector3 point;
    Math::Vector3 normal;
    f32 distance = 0.0f;
};

// How the caller answers "what is under this fingertip".
//
// An interface rather than a direct physics call because the IK step runs
// inside the renderer's pose pass, which has no physics backend and should not
// grow one. The owner injects an implementation; tests inject a plane. Same
// shape as the engine's other injected resolvers.
class ENJIN_API ISurfaceQuery {
public:
    virtual ~ISurfaceQuery() = default;

    // Cast from `origin` along `direction` (expected normalised) up to
    // `maxDistance`. Return false for a miss rather than a zero-distance hit:
    // a fingertip with nothing under it must be distinguishable from one
    // touching something at range zero.
    virtual bool Cast(const Math::Vector3& origin, const Math::Vector3& direction,
                      f32 maxDistance, SurfaceHit& out) const = 0;
};

// One finger, as the solver sees it.
//
// Positions are WORLD space and ordered from the knuckle outward: [0] is the
// joint at the palm, [kFingerJoints] is the tip. That is kFingerJoints + 1
// points for kFingerJoints bones, which is the convention FABRIK already uses.
struct FingerChain {
    std::array<Math::Vector3, kFingerJoints + 1> joints{};

    // 0 = follow the animation, 1 = fully on the surface. Separate because a
    // hand takes a counter and lets it go at different rates and in different
    // orders; see the header note.
    f32 approachWeight = 0.0f;
    f32 releaseWeight = 0.0f;

    // Which way this finger bends.
    //
    // Not decoration, and not optional in practice: FABRIK cannot bend a chain
    // that is perfectly straight and pointed exactly at its target. Every
    // direction it computes lies on the one axis, the forward pass pulls the
    // tip to the target, the backward pass walks straight back out again, and
    // it oscillates forever without converging. A flat hand held palm-down over
    // a counter is EXACTLY that configuration, so it is the common case rather
    // than a corner one.
    //
    // A real finger has no such ambiguity, because a knuckle is a hinge and
    // bends one way. Naming that direction resolves the degeneracy and gives a
    // rigger control over which way a finger bows. Leave it zero and the solver
    // picks a stable perpendicular, which is defined but arbitrary.
    Math::Vector3 curlDirection = Math::Vector3(0.0f, 0.0f, 0.0f);

    // Set by the solve: where this fingertip ended up and whether it found
    // anything. Reported so a caller can tell "the hand is resting" from "the
    // hand is reaching at nothing", which look identical from the pose alone.
    bool contacted = false;
    Math::Vector3 contactPoint;
    Math::Vector3 contactNormal;

    f32 Reach() const {
        f32 total = 0.0f;
        for (u32 i = 0; i < kFingerJoints; ++i) {
            total += (joints[i + 1] - joints[i]).Length();
        }
        return total;
    }
};

// Everything the solve needs that is not a finger.
struct HandPose {
    Math::Vector3 palmPosition;
    Math::Vector3 palmNormal = Math::Vector3(0.0f, -1.0f, 0.0f);   // points AT the surface
    std::array<FingerChain, kFingerCount> fingers{};
};

struct HandIKSettings {
    // How far past its own reach a fingertip will look for a surface. Beyond
    // this the finger stays on the animation: a hand is not a tape measure.
    f32 castMargin = 0.08f;

    // How far the palm can be from a surface and still be considered to be
    // resting on it at all.
    f32 engageDistance = 0.25f;

    // FABRIK passes per finger.
    //
    // Sixteen rather than the solver's default of five. A three-joint chain
    // converges fast when it has somewhere obvious to bend and slowly when the
    // required bend is small, which is exactly the fingertip-just-reaches case:
    // at six passes a finger needing to shed three centimetres stopped twelve
    // millimetres short of the counter, which is a visible float. Sixteen
    // passes over five bones is nothing next to a single draw call.
    u32 iterations = 16;

    // Curl applied past an edge, as a fraction of each bone's length. Zero
    // makes SurfaceEdge behave exactly like SurfacePoint, which is the wrong
    // pose, so this having a real default matters.
    f32 edgeCurl = 0.6f;
};

// The result of one hand solve, for callers that want to know what happened
// without diffing the pose.
struct HandIKResult {
    u32 fingersContacted = 0;
    bool anyContact = false;
    f32 meanPenetrationResolved = 0.0f;   // how far tips were pulled back, metres
};

class ENJIN_API HandIK {
public:
    // Settle every finger onto whatever is under it.
    //
    // For each fingertip: cast from the knuckle along the palm normal, no
    // further than that finger can actually reach plus a small margin. Where
    // something is found, the tip target is the hit point lifted off the
    // surface by nothing at all (a fingertip rests ON a counter, not above it),
    // and FABRIK bends the finger to put it there. Where nothing is found, the
    // finger is left alone, because a finger reaching for a surface that is not
    // there should keep doing whatever the animation said.
    //
    // The per-finger weight blends between the animated joints and the solved
    // ones, so a half-weighted hand is half-way onto the counter rather than
    // snapping.
    static HandIKResult SolveSurface(HandPose& hand,
                                     const ISurfaceQuery& surfaces,
                                     const HandIKSettings& settings = {});

    // What is under this hand: a face it can press on, or a bar it has to
    // curl over?
    //
    // Five fingers each casting straight down works on anything with a top
    // wider than a hand and fails silently on anything narrower. Measured on
    // the demo room: the worktops and the shelf give 3 of 5 fingers, the 7 cm
    // railing gives ZERO, because every finger's ray passes either side of it.
    // The edge solver already handled that case and had to be switched on by
    // hand, with a direction typed in, which is knowledge about the PROP stored
    // on the HAND.
    //
    // So ask the world instead. Cast the palm ray, then four more offset by
    // `probeRadius` along the hand's own right and forward axes. A face catches
    // all four; a bar catches the two that run ALONG it and misses the two
    // across it, which also hands back the direction the bar runs in. Hits that
    // come back at a very different range are a different object and are not
    // counted, so standing at the edge of a counter with a wall beyond it does
    // not read as a railing.
    struct ContactClassification {
        bool hit = false;
        HandTargetMode mode = HandTargetMode::SurfacePoint;
        Math::Vector3 edgeDirection{1.0f, 0.0f, 0.0f};
        SurfaceHit centre;
    };
    static ContactClassification ClassifyContact(const Math::Vector3& palmPosition,
                                                 const Math::Vector3& palmNormal,
                                                 const Math::Vector3& handRight,
                                                 const Math::Vector3& handForward,
                                                 f32 probeRadius,
                                                 f32 maxDistance,
                                                 const ISurfaceQuery& surfaces);

    // Settle onto a straight edge.
    //
    // `edgePoint` is any point on the edge, `edgeDirection` runs along it, and
    // `surfaceNormal` is the normal of the face the hand is coming from (the
    // top of a shelf). Fingers reach the edge and then curl past it, which is
    // what a hand on a shelf lip does and what a point solve cannot produce.
    static HandIKResult SolveEdge(HandPose& hand,
                                  const Math::Vector3& edgePoint,
                                  const Math::Vector3& edgeDirection,
                                  const Math::Vector3& surfaceNormal,
                                  const HandIKSettings& settings = {});

    // Move one finger's weights toward their goals.
    //
    // Separate from the solve because approach and release run on their own
    // clocks and a caller may want to drive them from an animation curve, a
    // distance, or a script. `dt` in seconds; rates in units of weight per
    // second.
    static void AdvanceWeights(FingerChain& finger, f32 approachGoal, f32 releaseGoal,
                               f32 approachRate, f32 releaseRate, f32 dt);

    // The blend a caller should actually apply, given both weights.
    //
    // Release SUBTRACTS from approach rather than replacing it, so a hand that
    // is 80% onto a counter and 30% through letting go is at 0.5 and not at
    // 0.3. Treating release as its own blend makes a releasing hand jump.
    static f32 EffectiveWeight(const FingerChain& finger) {
        const f32 w = finger.approachWeight - finger.releaseWeight;
        return w < 0.0f ? 0.0f : (w > 1.0f ? 1.0f : w);
    }
};

} // namespace Animation
} // namespace Enjin
