#pragma once

#include "Enjin/Platform/Types.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/Effects/GPUParticleTypes.h"
#include <string>

namespace Enjin {
namespace ECS {

// How particles are distributed at spawn. Point = all from the origin; the
// volumes scatter position (and steer velocity) within a shape. Disc2D/Line2D
// keep spawns on a plane for 2D-style effects.
enum class EmitShape : u8 { Point, Sphere, Hemisphere, Cone, Box, Disc2D, Line2D };

// Drop this on an entity to emit GPU-compute particles from its position.
// The heavy work (simulate + draw) stays entirely on the GPU; this component
// just tells the emitter system what to spawn, how fast, and where.
//
// The tuning fields below are ALWAYS the live spawn params. A preset is just a
// one-click way to fill them (the editor's Load Preset dropdown), so gravity,
// size, lifetime, etc. are always visible and editable.
struct GPUParticleEmitterComponent {
    // The last preset loaded (display only; the fields below are authoritative).
    Effects::GPUParticlePreset preset = Effects::GPUParticlePreset::Fire;

    bool emitting = true;          // continuous emission on/off
    f32 spawnRate = 200.0f;        // particles per second (continuous)
    Math::Vector3 direction = {0.0f, 1.0f, 0.0f};  // emission direction
    EmitShape shape = EmitShape::Cone;
    f32 shapeSize = 0.5f;          // radius / half-extent of the emission volume

    // One-shot burst: set burstCount and flip burstNow (the system fires it
    // once and clears the flag). Great for hits/explosions from script/nodes.
    u32 burstCount = 0;
    bool burstNow = false;

    // Live spawn params (defaults = Fire). Always used by ResolveParams.
    Math::Vector4 customColor = {1.0f, 0.55f, 0.12f, 0.9f};
    f32 customSize = 0.5f;
    f32 customLifetime = 1.0f;
    f32 customSpeed = 2.5f;
    f32 customSpread = 0.35f;
    f32 customGravityScale = -0.4f;   // <0 = rise (fire/smoke), 0 = float, 1 = fall
    f32 customDrag = 1.2f;

    // Sprite card: what each particle looks like.
    // 0=Soft Glow (classic), 1=Hard Circle, 2=Square, 3=Streak, 4=Star, 5=Texture
    u8 sprite = 0;
    f32 softness = 1.0f;              // 0 = hard edge, 1 = fully soft
    std::string spriteTexturePath;    // image used when sprite == 5 (desktop)
    // Resolved bindless index for spriteTexturePath (-2 = unresolved, -1 = failed).
    // Runtime cache, not serialized.
    mutable i32 cachedSpriteTexIndex = -2;

    // Collision vs world Box/Sphere/Capsule colliders (non-trigger, cap 32).
    bool collide = true;
    f32 bounciness = 0.35f;      // 0 = splat, 1 = superball
    f32 friction = 0.3f;         // 0 = slide freely, 1 = stop dead on contact
    f32 collisionRadius = 0.0f;  // 0 = collide as a point (+ small skin)

    // Runtime accumulator for fractional continuous spawns (not serialized).
    f32 accumulator = 0.0f;

    // The live spawn params are always the fields (a preset just fills them).
    Effects::ParticleSpawnParams ResolveParams() const {
        Effects::ParticleSpawnParams p;
        p.color = customColor;
        p.size = customSize;
        p.lifetime = customLifetime;
        p.speed = customSpeed;
        p.spread = customSpread;
        p.gravityScale = customGravityScale;
        p.drag = customDrag;
        p.sprite = sprite;
        p.softness = softness;
        p.collide = collide;
        p.bounciness = bounciness;
        p.friction = friction;
        p.collisionRadius = collisionRadius;
        return p;   // texIndex resolved by the render system (needs the texture cache)
    }

    // Copy a preset into the live fields (used by the editor's Load Preset).
    void LoadPreset(Effects::GPUParticlePreset ps) {
        preset = ps;
        Effects::ParticleSpawnParams p = Effects::PresetSpawnParams(ps);
        customColor = p.color; customSize = p.size; customLifetime = p.lifetime;
        customSpeed = p.speed; customSpread = p.spread;
        customGravityScale = p.gravityScale; customDrag = p.drag;
    }
};

} // namespace ECS
} // namespace Enjin
