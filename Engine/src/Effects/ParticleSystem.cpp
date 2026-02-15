#include "Enjin/Effects/ParticleSystem.h"
#include "Enjin/Logging/Log.h"
#include <cmath>
#include <algorithm>

namespace Enjin {
namespace Effects {

// Fast xorshift32 random float in [0, 1]
static u32 s_RandState = 2463534242u;
static f32 RandFloat() {
    s_RandState ^= s_RandState << 13;
    s_RandState ^= s_RandState >> 17;
    s_RandState ^= s_RandState << 5;
    return static_cast<f32>(s_RandState & 0x7FFFFFFF) * (1.0f / 2147483647.0f);
}

// Random float in [-1, 1]
static f32 RandFloatSigned() {
    return RandFloat() * 2.0f - 1.0f;
}

// Random float in [min, max]
static f32 RandRange(f32 min, f32 max) {
    return min + RandFloat() * (max - min);
}

// Piecewise linear interpolation: start -> mid -> end
static f32 PiecewiseLerp(f32 t, f32 start, f32 mid, f32 end) {
    if (t < 0.5f) {
        f32 localT = t * 2.0f;
        return start + (mid - start) * localT;
    } else {
        f32 localT = (t - 0.5f) * 2.0f;
        return mid + (end - mid) * localT;
    }
}

void ParticleSystem::InitPool(ECS::ParticleEmitterComponent& emitter) {
    auto& pool = emitter.pool;
    pool.maxParticles = emitter.maxParticles;
    pool.particles.resize(pool.maxParticles);
    pool.activeCount = 0;
    pool.spawnAccumulator = 0.0f;
    pool.burstTimer = 0.0f;
    pool.systemAge = 0.0f;
    pool.initialized = true;
}

void ParticleSystem::SpawnParticle(ECS::ParticleEmitterComponent& emitter, const Math::Vector3& emitterPos) {
    auto& pool = emitter.pool;
    if (pool.activeCount >= pool.maxParticles) return;

    ECS::Particle& p = pool.particles[pool.activeCount];

    // Compute spawn position and direction based on shape
    Math::Vector3 spawnPos = emitterPos;
    Math::Vector3 direction(0.0f, 1.0f, 0.0f);

    switch (emitter.shape) {
        case ECS::ParticleEmitterComponent::EmitterShape::Point: {
            // Random direction on unit sphere
            f32 theta = RandFloat() * 2.0f * 3.14159265f;
            f32 phi = std::acos(RandFloatSigned());
            direction.x = std::sin(phi) * std::cos(theta);
            direction.y = std::sin(phi) * std::sin(theta);
            direction.z = std::cos(phi);
            break;
        }
        case ECS::ParticleEmitterComponent::EmitterShape::Sphere: {
            f32 theta = RandFloat() * 2.0f * 3.14159265f;
            f32 phi = std::acos(RandFloatSigned());
            direction.x = std::sin(phi) * std::cos(theta);
            direction.y = std::sin(phi) * std::sin(theta);
            direction.z = std::cos(phi);
            spawnPos.x += direction.x * emitter.shapeRadius;
            spawnPos.y += direction.y * emitter.shapeRadius;
            spawnPos.z += direction.z * emitter.shapeRadius;
            break;
        }
        case ECS::ParticleEmitterComponent::EmitterShape::Hemisphere: {
            f32 theta = RandFloat() * 2.0f * 3.14159265f;
            f32 phi = std::acos(RandFloat()); // Only upper hemisphere (Y >= 0)
            direction.x = std::sin(phi) * std::cos(theta);
            direction.y = std::cos(phi); // Always >= 0
            direction.z = std::sin(phi) * std::sin(theta);
            spawnPos.x += direction.x * emitter.shapeRadius;
            spawnPos.y += direction.y * emitter.shapeRadius;
            spawnPos.z += direction.z * emitter.shapeRadius;
            break;
        }
        case ECS::ParticleEmitterComponent::EmitterShape::Cone: {
            f32 angleRad = emitter.coneAngle * (3.14159265f / 180.0f);
            f32 theta = RandFloat() * 2.0f * 3.14159265f;
            f32 r = RandFloat() * std::sin(angleRad);
            direction.x = r * std::cos(theta);
            direction.z = r * std::sin(theta);
            direction.y = std::cos(angleRad * RandFloat());
            // Normalize
            f32 len = std::sqrt(direction.x * direction.x + direction.y * direction.y + direction.z * direction.z);
            if (len > 0.0001f) {
                direction.x /= len;
                direction.y /= len;
                direction.z /= len;
            }
            break;
        }
        case ECS::ParticleEmitterComponent::EmitterShape::Box: {
            spawnPos.x += RandFloatSigned() * emitter.shapeRadius;
            spawnPos.y += RandFloatSigned() * emitter.shapeRadius;
            spawnPos.z += RandFloatSigned() * emitter.shapeRadius;
            // Random direction
            f32 theta = RandFloat() * 2.0f * 3.14159265f;
            f32 phi = std::acos(RandFloatSigned());
            direction.x = std::sin(phi) * std::cos(theta);
            direction.y = std::sin(phi) * std::sin(theta);
            direction.z = std::cos(phi);
            break;
        }
    }

    // Set particle properties
    f32 lifetimeVar = emitter.lifetimeVariance > 0.0f
        ? RandRange(-emitter.lifetimeVariance, emitter.lifetimeVariance)
        : 0.0f;
    p.maxLifetime = std::max(0.01f, emitter.lifetime + lifetimeVar);
    p.lifetime = p.maxLifetime;

    f32 speed = emitter.startSpeed + RandRange(-emitter.speedVariance, emitter.speedVariance);
    speed = std::max(0.0f, speed);
    p.velocity.x = direction.x * speed;
    p.velocity.y = direction.y * speed;
    p.velocity.z = direction.z * speed;

    p.position = spawnPos;
    p.size = emitter.startSize;
    p.alpha = emitter.startAlpha;
    p.color = emitter.startColor;

    p.rotation = emitter.startRotation + RandRange(-emitter.rotationVariance, emitter.rotationVariance);
    p.rotationSpeed = emitter.rotationSpeed + RandRange(-emitter.rotationSpeedVariance, emitter.rotationSpeedVariance);

    pool.activeCount++;
}

void ParticleSystem::UpdateEmitter(ECS::ParticleEmitterComponent& emitter, const Math::Vector3& emitterPos, f32 deltaTime) {
    auto& pool = emitter.pool;

    if (!emitter.isPlaying) return;

    pool.systemAge += deltaTime;

    // --- Spawning ---
    // Continuous emission via accumulator (capped for stability)
    if (emitter.emissionRate > 0.0f) {
        constexpr f32 MAX_EMISSION_RATE = 5000.0f;
        f32 clampedRate = std::min(emitter.emissionRate, MAX_EMISSION_RATE);
        pool.spawnAccumulator += clampedRate * deltaTime;
        // Cap accumulator to prevent massive catch-up spawns after lag spikes
        pool.spawnAccumulator = std::min(pool.spawnAccumulator, 128.0f);
        while (pool.spawnAccumulator >= 1.0f && pool.activeCount < pool.maxParticles) {
            SpawnParticle(emitter, emitterPos);
            pool.spawnAccumulator -= 1.0f;
        }
    }

    // Burst spawning
    if (emitter.burstCount > 0 && emitter.burstInterval > 0.0f) {
        pool.burstTimer += deltaTime;
        if (pool.burstTimer >= emitter.burstInterval) {
            pool.burstTimer -= emitter.burstInterval;
            for (i32 i = 0; i < emitter.burstCount && pool.activeCount < pool.maxParticles; ++i) {
                SpawnParticle(emitter, emitterPos);
            }
        }
    }

    // --- Update particles ---
    f32 sizeMid = emitter.sizeMid;
    if (sizeMid < 0.0f) {
        sizeMid = (emitter.startSize + emitter.endSize) * 0.5f;
    }

    u32 i = 0;
    while (i < pool.activeCount) {
        ECS::Particle& p = pool.particles[i];

        p.lifetime -= deltaTime;
        if (p.lifetime <= 0.0f) {
            // Swap-remove dead particle
            pool.particles[i] = pool.particles[pool.activeCount - 1];
            pool.activeCount--;
            continue;
        }

        // Normalized lifetime (0 = just spawned, 1 = about to die)
        f32 t = 1.0f - (p.lifetime / p.maxLifetime);

        // Speed curve multiplier
        f32 speedMult = PiecewiseLerp(t, 1.0f, emitter.speedMultiplierMid, emitter.speedMultiplierEnd);

        // Apply gravity
        p.velocity.x += emitter.gravity.x * deltaTime;
        p.velocity.y += emitter.gravity.y * deltaTime;
        p.velocity.z += emitter.gravity.z * deltaTime;

        // Apply drag
        if (emitter.drag > 0.0f) {
            f32 dragFactor = 1.0f - emitter.drag * deltaTime;
            if (dragFactor < 0.0f) dragFactor = 0.0f;
            p.velocity.x *= dragFactor;
            p.velocity.y *= dragFactor;
            p.velocity.z *= dragFactor;
        }

        // Move
        p.position.x += p.velocity.x * speedMult * deltaTime;
        p.position.y += p.velocity.y * speedMult * deltaTime;
        p.position.z += p.velocity.z * speedMult * deltaTime;

        // Interpolate size
        p.size = PiecewiseLerp(t, emitter.startSize, sizeMid, emitter.endSize);

        // Interpolate alpha
        p.alpha = emitter.startAlpha + (emitter.endAlpha - emitter.startAlpha) * t;

        // Interpolate color
        p.color.x = emitter.startColor.x + (emitter.endColor.x - emitter.startColor.x) * t;
        p.color.y = emitter.startColor.y + (emitter.endColor.y - emitter.startColor.y) * t;
        p.color.z = emitter.startColor.z + (emitter.endColor.z - emitter.startColor.z) * t;

        // Rotation
        p.rotation += p.rotationSpeed * deltaTime;

        ++i;
    }

    // Handle non-looping emitter: stop when no particles are alive and systemAge > lifetime
    if (!emitter.loop && pool.activeCount == 0 && pool.systemAge > emitter.lifetime + emitter.lifetimeVariance) {
        emitter.isPlaying = false;
    }
}

void ParticleSystem::Update(f32 deltaTime, ECS::World* world) {
    if (!world) return;

    m_TotalActiveParticles = 0;
    m_TotalEmitterCount = 0;

    for (ECS::Entity entity : world->GetEntitiesWithComponent<ECS::ParticleEmitterComponent>()) {
        if (!world->HasComponent<ECS::TransformComponent>(entity)) continue;

        auto* emitter = world->GetComponent<ECS::ParticleEmitterComponent>(entity);
        auto* transform = world->GetComponent<ECS::TransformComponent>(entity);
        if (!emitter || !transform) continue;

        // Initialize pool on first encounter
        if (!emitter->pool.initialized) {
            InitPool(*emitter);
            if (emitter->playOnAwake) {
                emitter->isPlaying = true;
            }
        }

        // Resize pool if maxParticles changed (capped for stability)
        constexpr u32 MAX_EMITTER_PARTICLES = 16384;
        if (emitter->pool.maxParticles != emitter->maxParticles) {
            u32 newMax = std::min(emitter->maxParticles, MAX_EMITTER_PARTICLES);
            // Gracefully expire particles beyond new limit before resizing
            if (newMax < emitter->pool.activeCount) {
                for (u32 j = newMax; j < emitter->pool.activeCount; ++j) {
                    emitter->pool.particles[j].lifetime = 0.0f;
                }
            }
            emitter->pool.maxParticles = newMax;
            emitter->pool.particles.resize(newMax);
            if (emitter->pool.activeCount > emitter->pool.maxParticles) {
                emitter->pool.activeCount = emitter->pool.maxParticles;
            }
        }

        UpdateEmitter(*emitter, transform->position, deltaTime);

        m_TotalActiveParticles += emitter->pool.activeCount;
        m_TotalEmitterCount++;
    }
}

} // namespace Effects
} // namespace Enjin
