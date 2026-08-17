#pragma once

// Backend-agnostic GPU particle data + presets. Shared by the Vulkan
// GPUParticleSystem and the WebGPU particle path (and the emitter component), so
// this header has NO renderer guard. The per-backend systems own the pipelines.

#include "Enjin/Platform/Platform.h"   // ENJIN_FORCE_INLINE, used by Math headers
#include "Enjin/Platform/Types.h"
#include "Enjin/Math/Vector.h"

namespace Enjin {
namespace Effects {

// GPU particle data (must match the GLSL/WGSL struct exactly — 64 bytes).
struct GPUParticle {
    Math::Vector3 position;
    f32 lifetime;
    Math::Vector3 velocity;
    f32 age;
    Math::Vector4 color;
    f32 size;
    f32 rotation;
    f32 gravityScale;   // per-particle gravity multiplier (0 float, <0 rise)
    f32 drag;           // per-particle extra velocity damping / sec
};
static_assert(sizeof(GPUParticle) == 64, "GPUParticle must match the GLSL/WGSL layout");

// Named looks for the emitter (maps to a ParticleSpawnParams). "Custom" uses
// the emitter component's own fields.
enum class GPUParticlePreset : u8 {
    Custom = 0, Smoke, Fire, Sparks, Blood, Mist, Spray, Dust, Magic, Snow, Count
};

// Per-particle spawn appearance + physics, baked into each particle at spawn.
// This is what makes presets look distinct within one shared simulation.
struct ParticleSpawnParams {
    Math::Vector4 color = {1, 1, 1, 1};
    f32 size = 0.3f;
    f32 lifetime = 3.0f;
    f32 speed = 2.0f;          // initial speed along direction
    f32 spread = 0.5f;         // cone/random spread
    f32 gravityScale = 1.0f;   // 0 = float, <0 = rise (smoke)
    f32 drag = 0.0f;           // extra per-particle damping
    f32 sizeJitter = 0.3f;     // 0-1 random size variation
};

// The canonical look for each preset.
ParticleSpawnParams PresetSpawnParams(GPUParticlePreset preset);
const char* GPUParticlePresetName(GPUParticlePreset preset);

// Per-particle spawn position offset for an emission shape (see ECS::EmitShape):
// 0=Point,1=Sphere,2=Hemisphere,3=Cone,4=Box,5=Disc2D,6=Line2D. Point/Cone = no
// offset (Cone is a velocity spread, not a volume). `size` is radius / half-extent.
Math::Vector3 ShapeSpawnOffset(u8 shape, f32 size, u32 index);

// Emitter configuration for GPU particles (uploaded to the sim's params UBO).
struct GPUEmitterConfig {
    Math::Vector3 position = {0, 0, 0};
    Math::Vector3 direction = {0, 1, 0};
    f32 spread = 0.5f;            // Cone half-angle (radians)
    Math::Vector3 gravity = {0, -9.8f, 0};
    f32 damping = 0.1f;
    Math::Vector4 startColor = {1, 1, 1, 1};
    Math::Vector4 endColor = {1, 1, 1, 0};
    f32 startSize = 0.1f;
    f32 endSize = 0.3f;
    f32 maxLifetime = 3.0f;
    f32 spawnRate = 100.0f;        // Particles per second
    f32 turbulenceStrength = 0.0f;
    f32 turbulenceFrequency = 1.0f;
    u32 maxParticles = 65536;
};

} // namespace Effects
} // namespace Enjin
