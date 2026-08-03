#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Math/Vector.h"

namespace Enjin {
namespace ECS {

// Director Styles — Preset bundling cross-department visual signatures
enum class CineDirectorStyle : u8 {
    Kubrick = 0,     // 1-point perspective, constant velocity dolly, deep focus
    Hitchcock,       // Vertigo dolly zoom, triad coverage, proximity holds
    Polanski,        // Claustrophobic tracking, wide lens proximity, eye-level lock
    Welles,          // Extreme low angles, deep focus crane oners, dynamic shadow ratios
    Spielberg,       // Retargeted oners, soft entrance/exit ramps, emotional zoom reveals
    Lucas,           // Graphic tableaux, dual-vcam wipe transitions, geometric staging
    Ford,            // Static frame holds, absolute look direction, subject frame exit
    Kurosawa,        // Telephoto compression, A/B/C multicam coverage, dynamic group blocking
    COUNT
};

// Rig Constraint Archetypes — Closed-form second-order dynamics without PhysX
enum class CineRigArchetype : u8 {
    Fixed = 0,
    HumanCarried,    // Steadicam / Handheld (body sway, breathing noise)
    Tracked,         // Spline / Dolly track (constant velocity, mass acceleration)
    Arm,             // 2-joint Crane Arm (rotational inertia, arc trajectories)
    Suspended,       // Catenary cable / volume suspension (pendulum sway)
    FreeFlying,      // Velocity/acceleration capped drone
    COUNT
};

// CineComponent — Virtual Cinematography Department Controller for TEGE ECS
struct CineComponent {
    bool enabled = true;
    CineDirectorStyle directorStyle = CineDirectorStyle::Kubrick;
    CineRigArchetype rigArchetype = CineRigArchetype::Tracked;

    // Optical & Lens setup
    f32 focalLengthMm = 35.0f;
    f32 apertureTStop = 2.8f;
    f32 focusDistanceMeters = 5.0f;
    f32 squeezeRatio = 1.0f; // 1.33f for Anamorphic

    // Electric & Lighting Ratios (Key : Fill : Rim)
    f32 keyIntensityEv = 1.0f;
    f32 keyToFillRatio = 4.0f; // 4:1 = 2 stops under
    f32 keyToRimRatio = 2.0f;  // 2:1 = 1 stop under

    // Second-Order Dynamics parameters (f, zeta, r)
    f32 frequency = 2.0f;     // Spring frequency
    f32 dampingRatio = 1.0f;  // Damping ratio (1.0 = critically damped)
    f32 initialResponse = 0.0f;

    // Target Subject entity slot
    u64 targetSubjectEntityId = 0;

    // Framing intent offset (Target-Relative framing by default)
    Math::Vector3 framingOffset = Math::Vector3(0.0f, 0.0f, 0.0f);
};

} // namespace ECS
} // namespace Enjin
