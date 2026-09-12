#pragma once

// The first few echoes, and where they come from.
//
// The late tail tells you how big and how hard a room is. The EARLY part tells
// you where you are standing in it. A source two metres from a back wall sends
// a reflection off that wall arriving a few milliseconds after the direct
// sound, from behind it -- and that pair of facts, the short delay and the
// direction, is what makes a listener place the source against the wall. A
// reverb tail alone cannot say it: the tail sounds the same wherever in the
// room you stand.
//
// Marty's bar: "the projector running in the basement audibly sits against the
// back wall."
//
// Computed by the image-source method, which is exact rather than sampled. A
// surface reflects a source as though a mirror image of it stood the same
// distance behind the surface; the reflection a listener hears is the straight
// line to that image, folded at the wall. So the delay is a distance and the
// direction is a direction, both exactly, with no rays to converge.
//
// The rays are only used to decide WHICH surfaces are worth mirroring across.
// A carved cave is two hundred thousand triangles and almost none of them face
// the listener; firing a few dozen rays finds the handful that actually
// surround the source, and the exact method runs on those.

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/Audio/AcousticMaterial.h"
#include "Enjin/Acoustics/AcousticBVH.h"
#include "Enjin/Acoustics/RoomResponse.h"   // kSpeedOfSound

#include <vector>

namespace Enjin {
namespace Acoustics {

struct ENJIN_API EarlyReflection {
    // Seconds after the sound was emitted, not after the direct arrival. The
    // caller usually wants it relative to the direct sound, and computing that
    // here would bake in an assumption about whether the direct path exists --
    // it does not, when a wall is in the way and the only thing a listener
    // hears is reflections.
    f32 delay = 0.0f;

    // Total path length in metres, source to surface to listener.
    f32 distance = 0.0f;

    // Amplitude per band, absorption and distance included.
    f32 gain[Audio::kAcousticBands] = {0.0f, 0.0f, 0.0f};

    // Unit vector from the LISTENER towards where this arrives from. This is
    // the half that places the source in the room: a reflection off the wall
    // behind a projector arrives from the wall, not from the projector.
    Math::Vector3 direction;

    // Where it bounced, which is what a debug draw needs to show a person why
    // a room sounds the way it does.
    Math::Vector3 reflectionPoint;

    u32 order = 1;
};

struct ENJIN_API EarlyReflectionResult {
    std::vector<EarlyReflection> taps;

    // The straight line, when there is one.
    f32 directDelay = 0.0f;
    f32 directDistance = 0.0f;
    f32 directGain = 0.0f;
    bool directOccluded = false;

    bool Any() const { return !taps.empty(); }
};

struct ENJIN_API EarlyReflectionSettings {
    // How many rays to fire looking for surfaces worth mirroring. This is not
    // the accuracy of the reflections -- those are exact -- only the chance of
    // noticing a surface at all.
    u32 candidateRays = 128;

    // A cap on the surfaces kept, so a cluttered room does not turn into a
    // combinatorial second-order search.
    u32 maxCandidates = 48;

    // 1 = single bounces. 2 adds double bounces, which is what gives a corner
    // its character, at the cost of a pass over every pair of candidates.
    u32 maxOrder = 2;

    // Keep the strongest this many. Beyond a couple of dozen, taps stop being
    // heard individually and become the tail, which the room response already
    // handles.
    u32 maxTaps = 24;

    // Anything quieter than this against the direct sound is inaudible under
    // it. Without a floor, a room contributes hundreds of taps that cost real
    // time and change nothing anyone can hear.
    f32 gainFloor = 0.001f;

    // Deterministic, for the same reason the room trace is.
    u32 seed = 5150u;
};

ENJIN_API EarlyReflectionResult TraceEarlyReflections(
    const AcousticBVH& bvh, const Audio::AcousticScene& scene,
    const Math::Vector3& source, const Math::Vector3& listener,
    const EarlyReflectionSettings& settings = {});

} // namespace Acoustics
} // namespace Enjin
