#pragma once
//
// A hand that finds a surface and settles on it, finger by finger.
//
// The solving lives in Animation::HandIK, which knows nothing about ECS. This
// is the authored half: which bones, which surface, how fast to take hold and
// how fast to let go.

#include "Enjin/Platform/Platform.h"
#include "Enjin/ECS/Entity.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/Animation/HandIK.h"

#include <array>
#include <string>

namespace Enjin {
namespace ECS {

// One finger's bones, root to tip.
//
// Names rather than indices because a rig gets re-exported and indices move,
// and because "LeftHandIndex1" is checkable by a person reading the inspector
// while "bone 27" is not.
struct FingerBones {
    std::string proximal;        // knuckle to first joint
    std::string intermediate;
    std::string distal;          // last joint to tip

    // The fingertip, if the rig has one.
    //
    // Three bones give three joint positions, and the solver needs four points
    // to describe three segments. Many rigs carry a leaf bone for exactly this
    // ("LeftHandIndex4"); where one exists, name it and the tip is read rather
    // than guessed. Where it does not, the tip is extrapolated past the distal
    // joint along the last segment, which is right for a straight finger and
    // drifts as the finger curls. That is a real approximation and worth
    // knowing about when a fingertip sits a few millimetres off a counter on a
    // rig with no leaf bones.
    std::string tip;

    // Which way this finger bends, in the hand's local space.
    //
    // Needed, not optional: a perfectly straight finger aimed at its target is
    // a configuration FABRIK cannot resolve, and a flat hand over a counter is
    // exactly that. See the note in Animation/HandIK.h.
    Math::Vector3 curlDirection = Math::Vector3(0.0f, 0.0f, 1.0f);

    bool IsSet() const { return !proximal.empty(); }
};

struct HandIKComponent {
    // The wrist. Everything else is found relative to this.
    std::string handBoneName;

    // Which way the palm faces, in the hand bone's own space.
    //
    // Rigs disagree about this and there is no way to infer it: the same hand
    // is -Y in one export and +Z in another. Getting it wrong does not throw,
    // it casts the fingertip rays out of the back of the hand, and every finger
    // quietly reports no contact. So it is authored, and the inspector says
    // what it is for.
    Math::Vector3 palmNormalLocal = Math::Vector3(0.0f, -1.0f, 0.0f);

    // Thumb, index, middle, ring, little. A finger with no proximal bone name
    // is skipped, so a three-fingered rig is a supported rig rather than a
    // crash.
    std::array<FingerBones, Animation::kFingerCount> fingers{};

    // What to settle on.
    Animation::HandTargetMode mode = Animation::HandTargetMode::SurfacePoint;

    // Entity mode: the historic behaviour, kept because a lever and a doorknob
    // ARE objects and were always targeted this way.
    std::string interactionTag;
    f32 interactionRadius = 1.5f;
    Entity currentTarget = INVALID_ENTITY;

    // SurfacePoint / SurfaceEdge: where the hand looks for something to rest
    // on. The cast runs from each knuckle along the palm normal, so this is
    // about which surface, not which point.
    //
    // engageDistance is how close the palm has to get before the hand starts
    // reaching at all. Too large and hands grab at surfaces across the room;
    // too small and the reach starts too late to look deliberate.
    f32 engageDistance = 0.35f;

    // SurfaceEdge only. Direction the edge runs; the surface normal is taken
    // from what the fingertip cast reports. Ignored in Auto mode, which
    // measures the direction off the geometry every frame.
    Math::Vector3 edgeDirection = Math::Vector3(1.0f, 0.0f, 0.0f);

    // LIVE, not authored: which solver Auto mode picked this frame. A hand that
    // curls when it should press, or presses when it should curl, is one wrong
    // classification and there is no other way to see which one happened.
    Animation::HandTargetMode resolvedMode = Animation::HandTargetMode::SurfacePoint;

    // Taking hold and letting go are not one control.
    //
    // A hand reaching for a counter leads with the fingertips and arrives; a
    // hand leaving lets go from the tip backwards while the wrist has already
    // moved on. Driving both from one value run forwards and backwards gives a
    // hand that retraces its own approach, which reads as a video in reverse.
    //
    // Units are weight per second, so 4.0 takes a quarter second to close.
    f32 approachRate = 4.0f;
    f32 releaseRate = 6.0f;      // letting go is faster than taking hold

    // Master blend for the whole hand. 0 disables without removing anything,
    // which is what an animator wants while checking a clip.
    f32 weight = 1.0f;

    // Live state, written by the system. Readable so a script or the inspector
    // can tell "resting" from "reaching at nothing", which look identical from
    // the pose alone.
    std::array<f32, Animation::kFingerCount> approachWeights{};
    std::array<f32, Animation::kFingerCount> releaseWeights{};
    std::array<bool, Animation::kFingerCount> contacted{};
    u32 fingersContacted = 0;
    bool engaged = false;
};

} // namespace ECS
} // namespace Enjin
