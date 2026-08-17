#include "Enjin/Effects/GPUParticleTypes.h"
#include <cmath>
#include <algorithm>

// Backend-agnostic (no renderer guard): the preset table is shared by the Vulkan
// and WebGPU particle systems and by the emitter component's ResolveParams().

namespace Enjin {
namespace Effects {

static f32 ShapeHash(u32 n) {
    n = (n << 13u) ^ n;
    n = n * (n * n * 15731u + 789221u) + 1376312589u;
    return static_cast<f32>(n & 0x7FFFFFFFu) / static_cast<f32>(0x7FFFFFFF);
}

Math::Vector3 ShapeSpawnOffset(u8 shape, f32 size, u32 index) {
    if (shape == 0 || shape == 3 || size <= 0.0f) return Math::Vector3(0, 0, 0);  // Point / Cone
    f32 a = ShapeHash(index * 3u + 1u);
    f32 b = ShapeHash(index * 3u + 2u);
    f32 c = ShapeHash(index * 3u + 3u);
    switch (shape) {
        case 1:   // Sphere
        case 2: { // Hemisphere (y >= 0)
            f32 u = a * 2.0f - 1.0f;
            f32 t = b * 6.2831853f;
            f32 r = size * std::cbrt(c);
            f32 s = std::sqrt(std::max(0.0f, 1.0f - u * u));
            f32 y = (shape == 2) ? std::fabs(u) : u;
            return Math::Vector3(r * s * std::cos(t), r * y, r * s * std::sin(t));
        }
        case 4:   // Box
            return Math::Vector3((a * 2 - 1) * size, (b * 2 - 1) * size, (c * 2 - 1) * size);
        case 5:   // Disc (2D, XY plane)
            return Math::Vector3((a * 2 - 1) * size, (b * 2 - 1) * size, 0.0f);
        case 6:   // Line (2D, X axis)
            return Math::Vector3((a * 2 - 1) * size, 0.0f, 0.0f);
        default:
            return Math::Vector3(0, 0, 0);
    }
}

const char* GPUParticlePresetName(GPUParticlePreset p) {
    switch (p) {
        case GPUParticlePreset::Custom: return "Custom";
        case GPUParticlePreset::Smoke:  return "Smoke";
        case GPUParticlePreset::Fire:   return "Fire";
        case GPUParticlePreset::Sparks: return "Sparks";
        case GPUParticlePreset::Blood:  return "Blood";
        case GPUParticlePreset::Mist:   return "Mist";
        case GPUParticlePreset::Spray:  return "Spray";
        case GPUParticlePreset::Dust:   return "Dust";
        case GPUParticlePreset::Magic:  return "Magic";
        case GPUParticlePreset::Snow:   return "Snow";
        default: return "Custom";
    }
}

ParticleSpawnParams PresetSpawnParams(GPUParticlePreset preset) {
    ParticleSpawnParams p;
    switch (preset) {
        case GPUParticlePreset::Smoke:
            p.color = {0.35f, 0.35f, 0.38f, 0.5f}; p.size = 0.8f; p.lifetime = 4.0f;
            p.speed = 1.2f; p.spread = 0.4f; p.gravityScale = -0.15f; p.drag = 0.8f; p.sizeJitter = 0.5f; break;
        case GPUParticlePreset::Fire:
            p.color = {1.0f, 0.55f, 0.12f, 0.9f}; p.size = 0.5f; p.lifetime = 1.0f;
            p.speed = 2.5f; p.spread = 0.35f; p.gravityScale = -0.4f; p.drag = 1.2f; p.sizeJitter = 0.4f; break;
        case GPUParticlePreset::Sparks:
            p.color = {1.0f, 0.85f, 0.4f, 1.0f}; p.size = 0.08f; p.lifetime = 0.8f;
            p.speed = 6.0f; p.spread = 1.0f; p.gravityScale = 1.4f; p.drag = 0.2f; p.sizeJitter = 0.6f; break;
        case GPUParticlePreset::Blood:
            p.color = {0.55f, 0.02f, 0.02f, 1.0f}; p.size = 0.12f; p.lifetime = 1.2f;
            p.speed = 4.0f; p.spread = 0.6f; p.gravityScale = 1.8f; p.drag = 0.1f; p.sizeJitter = 0.5f; break;
        case GPUParticlePreset::Mist:
            p.color = {0.85f, 0.88f, 0.92f, 0.3f}; p.size = 1.2f; p.lifetime = 5.0f;
            p.speed = 0.6f; p.spread = 0.9f; p.gravityScale = 0.0f; p.drag = 1.5f; p.sizeJitter = 0.6f; break;
        case GPUParticlePreset::Spray:
            p.color = {0.7f, 0.85f, 1.0f, 0.7f}; p.size = 0.15f; p.lifetime = 1.5f;
            p.speed = 5.0f; p.spread = 0.5f; p.gravityScale = 1.0f; p.drag = 0.4f; p.sizeJitter = 0.5f; break;
        case GPUParticlePreset::Dust:
            p.color = {0.7f, 0.62f, 0.5f, 0.45f}; p.size = 0.4f; p.lifetime = 3.0f;
            p.speed = 0.8f; p.spread = 0.8f; p.gravityScale = 0.1f; p.drag = 1.2f; p.sizeJitter = 0.6f; break;
        case GPUParticlePreset::Magic:
            p.color = {0.6f, 0.35f, 1.0f, 1.0f}; p.size = 0.18f; p.lifetime = 2.0f;
            p.speed = 1.5f; p.spread = 1.0f; p.gravityScale = -0.2f; p.drag = 0.6f; p.sizeJitter = 0.7f; break;
        case GPUParticlePreset::Snow:
            p.color = {1.0f, 1.0f, 1.0f, 0.9f}; p.size = 0.12f; p.lifetime = 6.0f;
            p.speed = 0.4f; p.spread = 0.6f; p.gravityScale = 0.15f; p.drag = 1.8f; p.sizeJitter = 0.4f; break;
        case GPUParticlePreset::Custom:
        default: break;   // defaults on the struct
    }
    return p;
}

} // namespace Effects
} // namespace Enjin
