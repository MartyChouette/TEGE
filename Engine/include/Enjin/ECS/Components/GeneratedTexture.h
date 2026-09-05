#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/Effects/ReactionDiffusion.h"
#include "Enjin/Effects/PhysarumSimulation.h"

namespace Enjin {
namespace ECS {

// ---------------------------------------------------------------------------
// Generated texture components
// ---------------------------------------------------------------------------
// The two CPU simulations that bake to RGBA8 rather than to geometry. Both
// shipped with full implementations, preset sets and bake functions, and had no
// component, no inspector, no serializer and no tick. GeneratedGeometrySystem
// runs them and writes the result into ProceduralTextureComponent.
//
// Resolution is the cost driver for both: the grid is stepped on the CPU every
// update and the whole image is re-uploaded, so these are authored small (128
// to 512) and throttled, not run at texture resolution.

// --- Gray-Scott reaction-diffusion -----------------------------------------
struct ENJIN_API ReactionDiffusionComponent {
    Effects::RDPreset preset = Effects::RDPreset::MitosisSpots;

    u32 width = 256;
    u32 height = 256;

    // Custom preset only: the classic Gray-Scott feed/kill pair. Every other
    // preset overwrites these from its own table, so editing them with a preset
    // selected does nothing (the inspector greys them out for that reason).
    f32 feedRate = 0.055f;
    f32 killRate = 0.062f;
    f32 diffusionU = 1.0f;
    f32 diffusionV = 0.5f;

    f32 deltaTime = 1.0f;      // Simulation time step, not frame delta
    u32 stepsPerFrame = 10;    // Sub-steps per update: the pattern's growth rate
    bool wrapEdges = true;
    u32 seed = 0;              // 0 = fresh random perturbation

    // Seeding. A Gray-Scott field left uniform never develops anything, so it
    // needs a perturbation to grow from.
    bool seedCentreCircle = true;
    f32 seedRadius = 0.08f;    // Fraction of the smaller grid axis
    u32 seedRandomSpots = 0;   // Extra random spots on top of the centre circle

    Math::Vector3 colorLow = {0.02f, 0.05f, 0.09f};
    Math::Vector3 colorHigh = {0.10f, 1.00f, 0.80f};

    // Simulation steps run before the single bake. The engine has no per-frame
    // texture upload path (see GeneratedGeometrySystem), so these bake once and
    // hold rather than animating; the settle count is what decides how developed
    // the pattern is when it lands.
    u32 settleSteps = 1200;
    bool resetRequested = false;   // re-settle and re-bake

    // Runtime readback for the inspector. Not serialized.
    u32 stepCount = 0;
};

// --- Physarum (slime mould) ------------------------------------------------
struct ENJIN_API PhysarumComponent {
    Effects::PhysarumPreset preset = Effects::PhysarumPreset::ClassicSlime;

    u32 width = 512;
    u32 height = 512;
    u32 agentCount = 50000;

    // Custom preset only, same rule as reaction-diffusion above.
    f32 sensorAngle = 22.5f;      // Degrees between the left and right sensors
    f32 sensorDistance = 9.0f;    // Pixels ahead the sensors sample
    f32 turnSpeed = 45.0f;        // Degrees per step
    f32 moveSpeed = 1.0f;         // Pixels per step
    f32 trailDecay = 0.02f;       // Evaporation per step
    f32 trailDeposit = 5.0f;      // Deposited per agent per step
    f32 diffuseRadius = 1.0f;     // 0 = none, 1 = 3x3, 2 = 5x5

    bool wrapEdges = true;
    u32 stepsPerFrame = 1;
    u32 seed = 0;

    // Initial agent placement. A ring is the classic starting condition: agents
    // walk inward and the network forms from the collisions.
    enum class Seeding : u8 { Ring = 0, Circle, Point, Count };
    Seeding seeding = Seeding::Ring;
    f32 seedInnerRadius = 0.25f;  // Fraction of the smaller axis
    f32 seedOuterRadius = 0.45f;

    Math::Vector3 trailColor = {0.85f, 0.95f, 0.35f};
    Math::Vector3 backgroundColor = {0.02f, 0.03f, 0.05f};

    // Same bake-once rule as reaction-diffusion above.
    u32 settleSteps = 600;
    bool resetRequested = false;

    // Runtime readback. Not serialized.
    u32 stepCount = 0;
};

inline const char* RDPresetName(Effects::RDPreset p) {
    switch (p) {
        case Effects::RDPreset::MitosisSpots:  return "Mitosis Spots";
        case Effects::RDPreset::CoralGrowth:   return "Coral Growth";
        case Effects::RDPreset::Fingerprints:  return "Fingerprints";
        case Effects::RDPreset::Leopard:       return "Leopard";
        case Effects::RDPreset::Labyrinth:     return "Labyrinth";
        case Effects::RDPreset::WormHoles:     return "Worm Holes";
        case Effects::RDPreset::BubblePacking: return "Bubble Packing";
        case Effects::RDPreset::Spirals:       return "Spirals";
        case Effects::RDPreset::Custom:        return "Custom";
        default:                               return "Unknown";
    }
}

inline const char* PhysarumPresetName(Effects::PhysarumPreset p) {
    switch (p) {
        case Effects::PhysarumPreset::ClassicSlime:     return "Classic Slime";
        case Effects::PhysarumPreset::BranchingNetwork: return "Branching Network";
        case Effects::PhysarumPreset::DenseWeb:         return "Dense Web";
        case Effects::PhysarumPreset::Tendrils:         return "Tendrils";
        case Effects::PhysarumPreset::Pulsating:        return "Pulsating";
        case Effects::PhysarumPreset::Custom:           return "Custom";
        default:                                        return "Unknown";
    }
}

} // namespace ECS
} // namespace Enjin
