#include "Enjin/Effects/GPUParticleTypes.h"

// Backend-agnostic (no renderer guard): the preset table is shared by the Vulkan
// and WebGPU particle systems and by the emitter component's ResolveParams().

namespace Enjin {
namespace Effects {

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
