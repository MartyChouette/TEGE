#pragma once

// How long a room rings, measured rather than dialled in.
//
// Reverb in this engine is a Freeverb bus driven by ReverbZoneComponent, whose
// fields are roomSize, damping, decayTime and preDelay, with presets called
// SmallRoom and Cathedral. Every one of those numbers is set by a person, which
// means a room sounds like whatever somebody guessed, and two rooms sound
// different only if somebody remembered to make them different.
//
// Marty's bar: "the same sound source placed on the main floor reads as a
// different room without changing any parameters by hand."
//
// So: fire rays into the room, follow them until their energy is gone, and read
// the decay off the result. The output is an RT60 per frequency band -- the time
// the room takes to fall by 60 dB -- which is the number acousticians actually
// use to describe a space, and which drives the reverb directly.
//
// The method is standard: an energy histogram, Schroeder backward integration,
// and a line fitted to the decay. It is what a measurement microphone does to a
// starter pistol, done in simulation. That matters because it means the result
// can be checked against SABINE'S EQUATION, which predicts RT60 from a room's
// volume and its total absorption. A simulation that agrees with the analytic
// answer on a shoebox is a simulation that is right, and the test says so.

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/Audio/AcousticMaterial.h"
#include "Enjin/Acoustics/AcousticBVH.h"

#include <vector>

namespace Enjin {
namespace Acoustics {

// Metres per second. Everything that turns a path length into a delay uses
// this one constant.
constexpr f32 kSpeedOfSound = 343.0f;

struct ENJIN_API RoomResponse {
    // Seconds to fall 60 dB, per band. Zero means the trace found no room --
    // an open field, or a source outside the geometry.
    f32 rt60[Audio::kAcousticBands] = {0.0f, 0.0f, 0.0f};

    // How much of the energy came back at all. An open field returns almost
    // nothing; a sealed concrete box returns nearly everything. This is what
    // sets how WET the reverb is, and it is the difference between outdoors
    // and indoors that a decay time alone cannot express.
    f32 reflectedEnergy = 0.0f;

    // Mean free path: the average distance a ray travels between surfaces. For
    // a convex room this is 4V/S, and it is a direct measure of how big the
    // space feels regardless of what it is made of.
    f32 meanFreePath = 0.0f;

    // Seconds before the first reflection arrives anywhere. The gap between the
    // direct sound and the room answering is most of how big a space reads.
    f32 firstReflection = 0.0f;

    u32 raysTraced = 0;
    u32 raysEscaped = 0;   // left through a gap and never came back

    bool Valid() const { return raysTraced > 0 && rt60[1] > 0.0f; }
};

struct ENJIN_API RoomTraceSettings {
    // More rays is a smoother decay curve and a steadier number. A few hundred
    // is enough to be stable; this is not a per-frame cost.
    u32 rayCount = 512;

    // A ray that has bounced this many times has nothing left to say.
    u32 maxBounces = 64;

    // Below this fraction of its starting energy a ray is finished.
    f32 energyFloor = 1.0e-6f;

    // How long a tail to measure, in seconds. A cathedral is about ten.
    f32 maxTime = 8.0f;

    // Histogram resolution. Finer resolves the early part better; coarser is a
    // steadier curve from the same number of rays.
    f32 binSeconds = 0.005f;

    // Deterministic by default. A reverb that changed slightly every time the
    // level loaded would be a bug nobody could reproduce.
    u32 seed = 8675309u;
};

// Trace the room around a point.
//
// Source-centric rather than listener-centric on purpose: the late tail of a
// room is a property of the ROOM, not of where you stand in it, so one trace
// describes the space and can be shared by every source inside it.
ENJIN_API RoomResponse TraceRoomResponse(const AcousticBVH& bvh,
                                         const Audio::AcousticScene& scene,
                                         const Math::Vector3& origin,
                                         const RoomTraceSettings& settings = {});

// Sabine's prediction for a room of this volume and absorption.
//
// Exposed because it is the cross-check the trace is validated against, and
// because it is a reasonable fallback when a room is too open to trace: a
// simulation and a century-old formula that agree are worth more than either
// alone.
ENJIN_API f32 SabineRT60(f32 volumeCubicMetres, f32 surfaceAreaSquareMetres,
                         f32 averageAbsorption);

} // namespace Acoustics
} // namespace Enjin
