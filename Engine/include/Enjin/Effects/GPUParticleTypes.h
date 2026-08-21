#pragma once

// Backend-agnostic GPU particle data + presets. Shared by the Vulkan
// GPUParticleSystem and the WebGPU particle path (and the emitter component), so
// this header has NO renderer guard. The per-backend systems own the pipelines.

#include "Enjin/Platform/Platform.h"   // ENJIN_FORCE_INLINE, used by Math headers
#include "Enjin/Platform/Types.h"
#include "Enjin/Math/Vector.h"

namespace Enjin {
namespace Effects {

// GPU particle data (must match the GLSL/WGSL struct exactly — 80 bytes).
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
    // Sprite card: which mask the fragment shader draws for this particle.
    // 0=SoftGlow 1=HardCircle 2=Square 3=Streak 4=Star 5=Textured(texIndex)
    f32 sprite;
    f32 softness;       // 0 = hard edge, 1 = fully soft falloff
    f32 texIndex;       // bindless texture index for sprite==5 (<0 = none; desktop only)
    f32 emitterKey;     // EntityIndex of the owning emitter (impact-event routing)
    // Collision response: x=bounciness, y=slide keep (1-friction), z=extra
    // radius beyond the 0.02 skin, w=collide flag (0 = ignore world colliders).
    Math::Vector4 collision;
};
static_assert(sizeof(GPUParticle) == 96, "GPUParticle must match the GLSL/WGSL layout");

// Named looks for the emitter (maps to a ParticleSpawnParams). "Custom" uses
// the emitter component's own fields.
enum class GPUParticlePreset : u8 {
    // NOTE: append new presets BEFORE Count (values are serialized as u8).
    // Impact/Pickup are one-shot event effects (hits, collisions, item pickups).
    Custom = 0, Smoke, Fire, Sparks, Blood, Mist, Spray, Dust, Magic, Snow, Liquid, Impact, Pickup, Count
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
    u8 sprite = 0;             // sprite card (see GPUParticle::sprite)
    f32 softness = 1.0f;       // edge softness (1 = the classic soft glow)
    f32 texIndex = -1.0f;      // bindless texture index when sprite==5 (desktop)

    f32 emitterKey = -1.0f;    // EntityIndex of the owning emitter entity
    f32 fixedRotation = -1.0f; // billboard rotation in radians; < 0 = random per particle

    // Collision vs world colliders (see GPUParticle::collision).
    bool collide = true;
    f32 bounciness = 0.35f;    // 0 = splat, 1 = superball
    f32 friction = 0.3f;       // 0 = slide freely on contact, 1 = stop dead
    f32 collisionRadius = 0.0f;  // extra collision radius (0 = point + skin)
};

// A collider strike reported by the GPU sim (read back two frames later).
struct ParticleImpactEvent {
    Math::Vector3 position;
    f32 speed;        // impact speed along the surface normal (m/s)
    Math::Vector3 normal;   // contact normal (stain orientation)
    u32 emitterKey;   // EntityIndex of the emitter that spawned the particle
};
// Max strikes recorded per frame (matches the shader-side cap).
constexpr u32 kMaxImpactEvents = 64;

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
