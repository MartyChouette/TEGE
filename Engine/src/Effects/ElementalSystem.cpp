#include "Enjin/Effects/ElementalSystem.h"
#include "Enjin/Effects/Wind.h"
#include "Enjin/Effects/Weather.h"
#include "Enjin/Effects/SeasonalWeather.h"
#include "Enjin/ECS/Components/Elemental.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/Logging/Log.h"
#include "Enjin/Math/Math.h"
#include <cmath>
#include <algorithm>

namespace Enjin {
namespace Effects {

// ============================================================================
// Spatial Hash
// ============================================================================

static constexpr f32 CELL_SIZE = 2.0f;
static constexpr f32 INV_CELL_SIZE = 1.0f / CELL_SIZE;

static u32 HashCell(i32 cx, i32 cz) {
    // Pack two 16-bit signed coords into u32
    return (static_cast<u32>(static_cast<u16>(cx)) << 16) | static_cast<u32>(static_cast<u16>(cz));
}

void ElementalPool::RebuildSpatialHash() {
    for (auto& [key, indices] : spatialHash) {
        indices.clear();
    }

    for (u32 i = 0; i < activeCount; ++i) {
        i32 cx = static_cast<i32>(std::floor(positions[i].x * INV_CELL_SIZE));
        i32 cz = static_cast<i32>(std::floor(positions[i].z * INV_CELL_SIZE));
        u32 key = HashCell(cx, cz);
        spatialHash[key].push_back(static_cast<u16>(i));
    }
}

void ElementalPool::GetNeighbors(const Math::Vector3& pos, f32 radius, std::vector<u16>& out) const {
    out.clear();
    i32 minCX = static_cast<i32>(std::floor((pos.x - radius) * INV_CELL_SIZE));
    i32 maxCX = static_cast<i32>(std::floor((pos.x + radius) * INV_CELL_SIZE));
    i32 minCZ = static_cast<i32>(std::floor((pos.z - radius) * INV_CELL_SIZE));
    i32 maxCZ = static_cast<i32>(std::floor((pos.z + radius) * INV_CELL_SIZE));
    f32 radiusSq = radius * radius;

    for (i32 cx = minCX; cx <= maxCX; ++cx) {
        for (i32 cz = minCZ; cz <= maxCZ; ++cz) {
            u32 key = HashCell(cx, cz);
            auto it = spatialHash.find(key);
            if (it == spatialHash.end()) continue;
            for (u16 idx : it->second) {
                Math::Vector3 diff = positions[idx] - pos;
                f32 distSq = diff.x * diff.x + diff.y * diff.y + diff.z * diff.z;
                if (distSq <= radiusSq) {
                    out.push_back(idx);
                }
            }
        }
    }
}

// ============================================================================
// ElementalSystem
// ============================================================================

void ElementalSystem::Initialize(WindSystem* wind, WeatherSystem* weather, SeasonalWeatherSystem* seasonal) {
    m_Wind = wind;
    m_Weather = weather;
    m_Seasonal = seasonal;
    m_Pool.activeCount = 0;
    m_Pool.spatialHash.clear();
    m_RainSpawnAccumulator = 0.0f;
    m_SnowSpawnAccumulator = 0.0f;
    m_Initialized = true;
    ENJIN_LOG_INFO(Renderer, "ElementalSystem initialized (max {} particles)", ElementalPool::MAX_PARTICLES);
}

void ElementalSystem::Update(ECS::World* world, f32 deltaTime, const Math::Vector3& cameraPos) {
    if (!m_Initialized) return;

    // Clamp dt to prevent explosion on lag spikes
    f32 dt = Math::Min(deltaTime, 0.05f);

    UpdateEmitters(world, dt);
    UpdateWeatherInput(dt, cameraPos);
    UpdatePhysics(dt);
    m_Pool.RebuildSpatialHash();
    UpdateVolumes(world, dt);
    UpdateInteractions(dt);
    UpdateSurfaceAccumulation(world, dt);
    UpdateFireSpread(dt);
    KillExpired();
}

// ============================================================================
// Spawn API
// ============================================================================

u32 ElementalSystem::AllocateParticle() {
    if (m_Pool.activeCount >= ElementalPool::MAX_PARTICLES) return ElementalPool::MAX_PARTICLES;
    return m_Pool.activeCount++;
}

void ElementalSystem::FreeParticle(u32 index) {
    if (index >= m_Pool.activeCount || m_Pool.activeCount == 0) return;
    u32 last = m_Pool.activeCount - 1;
    if (index != last) {
        // Swap with last
        m_Pool.positions[index] = m_Pool.positions[last];
        m_Pool.velocities[index] = m_Pool.velocities[last];
        m_Pool.elements[index] = m_Pool.elements[last];
        m_Pool.intensities[index] = m_Pool.intensities[last];
        m_Pool.lifetimes[index] = m_Pool.lifetimes[last];
        m_Pool.maxLifetimes[index] = m_Pool.maxLifetimes[last];
        m_Pool.sizes[index] = m_Pool.sizes[last];
        m_Pool.flags[index] = m_Pool.flags[last];
    }
    m_Pool.activeCount--;
}

u32 ElementalSystem::SpawnFire(const Math::Vector3& pos, f32 intensity, f32 lifetime) {
    if (CountElementParticles(0) >= m_MaxFireParticles) return ElementalPool::MAX_PARTICLES;
    u32 idx = AllocateParticle();
    if (idx >= ElementalPool::MAX_PARTICLES) return idx;

    m_Pool.positions[idx] = pos;
    m_Pool.velocities[idx] = Math::Vector3(0.0f, intensity * 1.5f, 0.0f); // fire rises
    m_Pool.elements[idx] = Math::Vector4(1.0f, 0.0f, 0.0f, 0.0f);
    m_Pool.intensities[idx] = Math::Clamp(intensity, 0.0f, 1.0f);
    m_Pool.lifetimes[idx] = lifetime;
    m_Pool.maxLifetimes[idx] = lifetime;
    m_Pool.sizes[idx] = intensity * 0.5f;
    m_Pool.flags[idx] = 0;
    return idx;
}

u32 ElementalSystem::SpawnWater(const Math::Vector3& pos, const Math::Vector3& velocity, f32 intensity) {
    u32 idx = AllocateParticle();
    if (idx >= ElementalPool::MAX_PARTICLES) return idx;

    m_Pool.positions[idx] = pos;
    m_Pool.velocities[idx] = velocity;
    m_Pool.elements[idx] = Math::Vector4(0.0f, 1.0f, 0.0f, 0.0f);
    m_Pool.intensities[idx] = Math::Clamp(intensity, 0.0f, 1.0f);
    m_Pool.lifetimes[idx] = 3.0f;
    m_Pool.maxLifetimes[idx] = 3.0f;
    m_Pool.sizes[idx] = 0.1f;
    m_Pool.flags[idx] = 0;
    return idx;
}

u32 ElementalSystem::SpawnEarth(const Math::Vector3& pos, const Math::Vector3& velocity, f32 size) {
    u32 idx = AllocateParticle();
    if (idx >= ElementalPool::MAX_PARTICLES) return idx;

    m_Pool.positions[idx] = pos;
    m_Pool.velocities[idx] = velocity;
    m_Pool.elements[idx] = Math::Vector4(0.0f, 0.0f, 1.0f, 0.0f);
    m_Pool.intensities[idx] = 0.8f;
    m_Pool.lifetimes[idx] = 2.0f;
    m_Pool.maxLifetimes[idx] = 2.0f;
    m_Pool.sizes[idx] = Math::Clamp(size, 0.05f, 1.0f);
    m_Pool.flags[idx] = 0;
    return idx;
}

u32 ElementalSystem::SpawnSnow(const Math::Vector3& pos, f32 size) {
    u32 idx = AllocateParticle();
    if (idx >= ElementalPool::MAX_PARTICLES) return idx;

    m_Pool.positions[idx] = pos;
    m_Pool.velocities[idx] = Math::Vector3(0.0f, -0.5f, 0.0f); // gentle fall
    m_Pool.elements[idx] = Math::Vector4(0.0f, 0.4f, 0.0f, 0.6f); // cold water + air
    m_Pool.intensities[idx] = 0.6f;
    m_Pool.lifetimes[idx] = 5.0f;
    m_Pool.maxLifetimes[idx] = 5.0f;
    m_Pool.sizes[idx] = Math::Clamp(size, 0.02f, 0.3f);
    m_Pool.flags[idx] = 0;
    return idx;
}

u32 ElementalSystem::SpawnSteam(const Math::Vector3& pos, f32 intensity) {
    u32 idx = AllocateParticle();
    if (idx >= ElementalPool::MAX_PARTICLES) return idx;

    m_Pool.positions[idx] = pos;
    m_Pool.velocities[idx] = Math::Vector3(0.0f, 1.0f, 0.0f); // rises
    m_Pool.elements[idx] = Math::Vector4(0.5f, 0.5f, 0.0f, 0.0f); // fire+water blend
    m_Pool.intensities[idx] = Math::Clamp(intensity, 0.0f, 1.0f);
    m_Pool.lifetimes[idx] = 1.5f;
    m_Pool.maxLifetimes[idx] = 1.5f;
    m_Pool.sizes[idx] = 0.3f;
    m_Pool.flags[idx] = EPF_DECAYING;
    return idx;
}

u32 ElementalSystem::SpawnCustom(const Math::Vector3& pos, const Math::Vector4& element, f32 intensity, f32 lifetime) {
    u32 idx = AllocateParticle();
    if (idx >= ElementalPool::MAX_PARTICLES) return idx;

    m_Pool.positions[idx] = pos;
    m_Pool.velocities[idx] = Math::Vector3(0.0f, 0.0f, 0.0f);
    m_Pool.elements[idx] = element;
    m_Pool.intensities[idx] = Math::Clamp(intensity, 0.0f, 1.0f);
    m_Pool.lifetimes[idx] = lifetime;
    m_Pool.maxLifetimes[idx] = lifetime;
    m_Pool.sizes[idx] = 0.2f;
    m_Pool.flags[idx] = 0;
    return idx;
}

void ElementalSystem::SpawnRainBurst(const Math::Vector3& center, f32 radius, u32 count) {
    for (u32 i = 0; i < count && m_Pool.activeCount < ElementalPool::MAX_PARTICLES; ++i) {
        // Random position in circle around center
        f32 angle = static_cast<f32>(i) / static_cast<f32>(count) * Math::PI * 2.0f;
        f32 dist = radius * (0.3f + 0.7f * static_cast<f32>((i * 7 + 13) % count) / static_cast<f32>(count));
        Math::Vector3 pos = center + Math::Vector3(std::cos(angle) * dist, 0.0f, std::sin(angle) * dist);
        SpawnWater(pos, Math::Vector3(0.0f, -5.0f, 0.0f), 0.5f);
    }
}

void ElementalSystem::SpawnDebrisBurst(const Math::Vector3& center, const Math::Vector3& force, u32 count) {
    for (u32 i = 0; i < count && m_Pool.activeCount < ElementalPool::MAX_PARTICLES; ++i) {
        f32 t = static_cast<f32>(i) / static_cast<f32>(Math::Max(count, 1u));
        f32 angle = t * Math::PI * 2.0f;
        Math::Vector3 vel = force + Math::Vector3(std::cos(angle) * 2.0f, 1.0f + t, std::sin(angle) * 2.0f);
        SpawnEarth(center, vel, 0.1f + t * 0.2f);
    }
}

// ============================================================================
// Query
// ============================================================================

Math::Vector4 ElementalSystem::SampleElementAt(const Math::Vector3& pos, f32 radius) const {
    Math::Vector4 sum(0.0f, 0.0f, 0.0f, 0.0f);
    f32 totalWeight = 0.0f;
    f32 radiusSq = radius * radius;

    for (u32 i = 0; i < m_Pool.activeCount; ++i) {
        Math::Vector3 diff = m_Pool.positions[i] - pos;
        f32 distSq = diff.x * diff.x + diff.y * diff.y + diff.z * diff.z;
        if (distSq <= radiusSq) {
            f32 weight = m_Pool.intensities[i] * (1.0f - distSq / radiusSq);
            sum += m_Pool.elements[i] * weight;
            totalWeight += weight;
        }
    }

    return totalWeight > 0.0f ? sum * (1.0f / totalWeight) : Math::Vector4(0.0f, 0.0f, 0.0f, 0.0f);
}

f32 ElementalSystem::GetFireIntensityAt(const Math::Vector3& pos, f32 radius) const {
    Math::Vector4 sample = SampleElementAt(pos, radius);
    return Math::Max(0.0f, sample.x);
}

f32 ElementalSystem::GetMoistureAt(const Math::Vector3& pos, f32 radius) const {
    Math::Vector4 sample = SampleElementAt(pos, radius);
    return Math::Max(0.0f, sample.y);
}

u32 ElementalSystem::GetFireCount() const { return CountElementParticles(0); }
u32 ElementalSystem::GetWaterCount() const { return CountElementParticles(1); }

void ElementalSystem::BuildFireLights(f32 time, std::vector<FireLight>& out) {
    out.clear();

    // Fire-light clustering uses a finer grid than the simulation spatial hash so
    // a single bonfire reads as one light rather than a smear of point sources.
    constexpr f32 FIRE_CELL_SIZE = 1.5f;
    constexpr f32 INV_FIRE_CELL = 1.0f / FIRE_CELL_SIZE;
    constexpr f32 FIRE_THRESHOLD = 0.05f;   // ignore near-zero fire signatures
    constexpr f32 INTENSITY_SCALE = 0.35f;  // accumulated mass -> light intensity
    constexpr f32 MAX_INTENSITY   = 6.0f;

    m_FireBucketCount = 0;

    // --- 1. Bucket fire particles into coarse XZ cells (weighted centroid + mass) ---
    for (u32 i = 0; i < m_Pool.activeCount; ++i) {
        f32 fire = m_Pool.elements[i].x; // fire channel
        if (fire < FIRE_THRESHOLD) continue;
        f32 weight = fire * m_Pool.intensities[i];
        if (weight <= 0.0f) continue;

        i32 cx = static_cast<i32>(std::floor(m_Pool.positions[i].x * INV_FIRE_CELL));
        i32 cz = static_cast<i32>(std::floor(m_Pool.positions[i].z * INV_FIRE_CELL));

        // Linear probe over active buckets (bounded by MAX_FIRE_BUCKETS).
        FireBucket* bucket = nullptr;
        for (u32 b = 0; b < m_FireBucketCount; ++b) {
            if (m_FireBuckets[b].cx == cx && m_FireBuckets[b].cz == cz) {
                bucket = &m_FireBuckets[b];
                break;
            }
        }
        if (!bucket) {
            if (m_FireBucketCount >= MAX_FIRE_BUCKETS) continue; // graceful: drop overflow cells
            bucket = &m_FireBuckets[m_FireBucketCount++];
            bucket->cx = cx;
            bucket->cz = cz;
            bucket->weightedPos = Math::Vector3(0.0f);
            bucket->mass = 0.0f;
            bucket->maxY = -1.0e30f;
        }
        bucket->weightedPos = bucket->weightedPos + m_Pool.positions[i] * weight;
        bucket->mass += weight;
        if (m_Pool.positions[i].y > bucket->maxY) bucket->maxY = m_Pool.positions[i].y;
    }

    if (m_FireBucketCount == 0) return;

    // --- 2. Emit the top MAX_FIRE_LIGHTS buckets by mass (partial selection) ---
    u32 emitCount = m_FireBucketCount < MAX_FIRE_LIGHTS ? m_FireBucketCount : MAX_FIRE_LIGHTS;
    bool used[MAX_FIRE_BUCKETS] = {};
    for (u32 k = 0; k < emitCount; ++k) {
        i32 best = -1;
        f32 bestMass = -1.0f;
        for (u32 b = 0; b < m_FireBucketCount; ++b) {
            if (used[b]) continue;
            if (m_FireBuckets[b].mass > bestMass) { bestMass = m_FireBuckets[b].mass; best = static_cast<i32>(b); }
        }
        if (best < 0) break;
        used[best] = true;
        const FireBucket& fb = m_FireBuckets[best];

        // --- 3. Build the representative light ---
        Math::Vector3 centroid = fb.weightedPos / fb.mass;
        // Lift toward the top of the flame so the glow reads above the fuel base.
        centroid.y = (centroid.y + fb.maxY) * 0.5f;

        f32 intensity = fb.mass * INTENSITY_SCALE;
        if (intensity > MAX_INTENSITY) intensity = MAX_INTENSITY;
        f32 t = intensity / MAX_INTENSITY; // 0..1 hotness

        // Blackbody-ish ramp: deep red -> orange -> yellow-white.
        Math::Vector3 cool(1.0f, 0.25f, 0.05f);
        Math::Vector3 warm(1.0f, 0.55f, 0.15f);
        Math::Vector3 hot (1.0f, 0.85f, 0.55f);
        Math::Vector3 color = t < 0.5f
            ? cool + (warm - cool) * (t * 2.0f)
            : warm + (hot - warm) * ((t - 0.5f) * 2.0f);

        // Per-cluster flicker, decorrelated by a cell hash so fires do not pulse together.
        f32 cellPhase = static_cast<f32>((fb.cx * 73856093) ^ (fb.cz * 19349663)) * 1.0e-4f;
        f32 flicker = 0.85f + 0.15f * std::sin(time * 11.0f + cellPhase)
                            + 0.07f * std::sin(time * 23.0f + cellPhase * 2.3f);
        intensity *= flicker;

        FireLight fl;
        fl.position = centroid;
        fl.range = 3.0f + 5.0f * t; // larger fires reach further
        fl.color = color;
        fl.intensity = intensity;
        out.push_back(fl);
    }
}

u32 ElementalSystem::CountElementParticles(u32 channel) const {
    u32 count = 0;
    for (u32 i = 0; i < m_Pool.activeCount; ++i) {
        if (m_Pool.elements[i][channel] > 0.5f) ++count;
    }
    return count;
}

// ============================================================================
// Emitter Update
// ============================================================================

void ElementalSystem::UpdateEmitters(ECS::World* world, f32 dt) {
    if (!world) return;

    for (ECS::Entity entity : world->GetEntitiesWithComponent<ECS::ElementalEmitterComponent>()) {
        auto* emitter = world->GetComponent<ECS::ElementalEmitterComponent>(entity);
        if (!emitter || !emitter->active) continue;
        auto* transform = world->GetComponent<ECS::TransformComponent>(entity);
        if (!transform || !transform->visible) continue;

        emitter->spawnAccumulator += dt * emitter->emissionRate;
        while (emitter->spawnAccumulator >= 1.0f && m_Pool.activeCount < ElementalPool::MAX_PARTICLES) {
            emitter->spawnAccumulator -= 1.0f;

            u32 idx = AllocateParticle();
            if (idx >= ElementalPool::MAX_PARTICLES) break;

            // Simple spread: offset direction by spread angle using deterministic variation
            f32 spreadAngle = emitter->spread;
            f32 t = static_cast<f32>(idx) * 1.618f; // golden ratio for distribution
            f32 cosA = std::cos(t);
            f32 sinA = std::sin(t);
            Math::Vector3 dir = emitter->direction;
            Math::Vector3 perp(dir.z * cosA - dir.x * sinA * dir.y,
                               dir.x * cosA + dir.z * cosA * (1.0f - std::abs(dir.y)),
                               -dir.x * sinA - dir.z * cosA * dir.y);
            // Simplified spread: just add some perpendicular offset
            f32 spreadFactor = std::sin(spreadAngle) * (std::sin(t * 3.7f) * 0.5f + 0.5f);
            Math::Vector3 finalDir = dir + Math::Vector3(cosA * spreadFactor, 0.0f, sinA * spreadFactor);
            f32 len = finalDir.Length();
            if (len > 0.001f) finalDir = finalDir * (1.0f / len);

            m_Pool.positions[idx] = transform->position;
            m_Pool.velocities[idx] = finalDir * emitter->speed;
            m_Pool.elements[idx] = emitter->element;
            m_Pool.intensities[idx] = emitter->intensity;
            m_Pool.lifetimes[idx] = emitter->lifetime;
            m_Pool.maxLifetimes[idx] = emitter->lifetime;
            m_Pool.sizes[idx] = emitter->intensity * 0.3f;
            m_Pool.flags[idx] = 0;
        }
    }
}

// ============================================================================
// Volume Update
// ============================================================================

void ElementalSystem::UpdateVolumes(ECS::World* world, f32 dt) {
    if (!world) return;

    for (ECS::Entity entity : world->GetEntitiesWithComponent<ECS::ElementalVolumeComponent>()) {
        auto* vol = world->GetComponent<ECS::ElementalVolumeComponent>(entity);
        if (!vol) continue;
        auto* transform = world->GetComponent<ECS::TransformComponent>(entity);
        if (!transform) continue;

        Math::Vector3 center = transform->position;
        Math::Vector3 he = vol->halfExtents;

        for (u32 i = 0; i < m_Pool.activeCount; ++i) {
            Math::Vector3 p = m_Pool.positions[i];
            // AABB containment check
            if (std::abs(p.x - center.x) <= he.x &&
                std::abs(p.y - center.y) <= he.y &&
                std::abs(p.z - center.z) <= he.z) {

                // Apply element bias
                m_Pool.elements[i] += vol->elementBias * dt;

                // Wind multiplier
                if (vol->windMultiplier != 1.0f) {
                    m_Pool.velocities[i] = m_Pool.velocities[i] * (1.0f + (vol->windMultiplier - 1.0f) * dt);
                }

                // Kill on contact (e.g. water zone kills fire particles)
                if (vol->killOnContact) {
                    // Kill particles whose dominant element opposes the volume bias
                    f32 reaction = ComputeReaction(m_Pool.elements[i], vol->elementBias);
                    if (reaction < -0.3f) {
                        m_Pool.lifetimes[i] = 0.0f;
                    }
                }
            }
        }
    }
}

// ============================================================================
// Physics
// ============================================================================

void ElementalSystem::UpdatePhysics(f32 dt) {
    for (u32 i = 0; i < m_Pool.activeCount; ++i) {
        Math::Vector4 elem = m_Pool.elements[i];
        Math::Vector3& vel = m_Pool.velocities[i];
        Math::Vector3& pos = m_Pool.positions[i];

        // Wind influence
        if (m_Wind) {
            Math::Vector3 wind = m_Wind->GetWindAt(pos);
            // Wind sensitivity by element type
            f32 windSensitivity = elem.x * 0.6f   // fire: moderate wind push
                                + elem.y * 0.2f   // water: light wind drift
                                + elem.z * 0.05f  // earth: very little wind
                                + elem.w * 1.0f;  // air: full wind
            vel += wind * windSensitivity * dt;
        }

        // Gravity (element-weighted)
        f32 gravityStrength = -elem.x * 0.5f   // fire: negative gravity (rises)
                            + elem.y * 9.8f    // water: full gravity
                            + elem.z * 12.0f   // earth: heavy
                            + elem.w * 0.2f;   // air: near weightless
        vel.y -= gravityStrength * dt;

        // Fire buoyancy
        if (elem.x > 0.5f) {
            vel.y += m_Pool.intensities[i] * 2.0f * dt;
        }

        // Drag (light)
        vel = vel * (1.0f - 0.5f * dt);

        // Move
        pos += vel * dt;

        // Ground collision
        if (pos.y < 0.0f) {
            pos.y = 0.0f;
            vel.y = 0.0f;
            m_Pool.flags[i] |= EPF_GROUNDED;

            // Water and snow on ground start decaying
            if (elem.y > 0.3f || elem.w > 0.3f) {
                m_Pool.flags[i] |= EPF_DECAYING;
            }
        }

        // Decay lifetime
        m_Pool.lifetimes[i] -= dt;

        // Decaying particles lose intensity
        if (m_Pool.flags[i] & EPF_DECAYING) {
            m_Pool.intensities[i] -= dt * 0.3f;
        }

        // Steam/smoke expands
        if (elem.x > 0.2f && elem.y > 0.2f) { // steam-like
            m_Pool.sizes[i] += dt * 0.5f;
            m_Pool.intensities[i] -= dt * 0.2f;
        }
    }
}

// ============================================================================
// Interaction Engine (dot-product)
// ============================================================================

f32 ElementalSystem::ComputeReaction(const Math::Vector4& a, const Math::Vector4& b) const {
    // reaction = dot(a, M * b)
    // M * b computed manually for 4x4 matrix
    Math::Vector4 Mb(
        m_InteractionMatrix[0][0] * b.x + m_InteractionMatrix[0][1] * b.y + m_InteractionMatrix[0][2] * b.z + m_InteractionMatrix[0][3] * b.w,
        m_InteractionMatrix[1][0] * b.x + m_InteractionMatrix[1][1] * b.y + m_InteractionMatrix[1][2] * b.z + m_InteractionMatrix[1][3] * b.w,
        m_InteractionMatrix[2][0] * b.x + m_InteractionMatrix[2][1] * b.y + m_InteractionMatrix[2][2] * b.z + m_InteractionMatrix[2][3] * b.w,
        m_InteractionMatrix[3][0] * b.x + m_InteractionMatrix[3][1] * b.y + m_InteractionMatrix[3][2] * b.z + m_InteractionMatrix[3][3] * b.w
    );
    return a.Dot(Mb);
}

void ElementalSystem::SpawnInteractionProduct(const Math::Vector3& pos, const Math::Vector4& elemA, const Math::Vector4& elemB) {
    // Fire + Water -> Steam
    if ((elemA.x > 0.5f && elemB.y > 0.5f) || (elemA.y > 0.5f && elemB.x > 0.5f)) {
        SpawnSteam(pos, 0.5f);
        return;
    }
    // Water + Earth -> Mud (earth particle with water signature)
    if ((elemA.y > 0.5f && elemB.z > 0.5f) || (elemA.z > 0.5f && elemB.y > 0.5f)) {
        SpawnCustom(pos, Math::Vector4(0.0f, 0.5f, 0.5f, 0.0f), 0.4f, 4.0f); // mud
        return;
    }
    // Fire + Air -> Smoke
    if ((elemA.x > 0.5f && elemB.w > 0.5f) || (elemA.w > 0.5f && elemB.x > 0.5f)) {
        u32 idx = SpawnCustom(pos, Math::Vector4(0.3f, 0.0f, 0.0f, 0.7f), 0.3f, 2.0f);
        if (idx < ElementalPool::MAX_PARTICLES) {
            m_Pool.velocities[idx] = Math::Vector3(0.0f, 0.8f, 0.0f);
            m_Pool.flags[idx] |= EPF_DECAYING;
        }
        return;
    }
}

void ElementalSystem::UpdateInteractions(f32 dt) {
    u32 interactionsThisFrame = 0;
    u32 spawnsThisFrame = 0;
    static constexpr u32 MAX_SPAWNS_PER_FRAME = 8;
    static constexpr f32 INTERACTION_RADIUS = 1.5f;

    std::vector<u16> neighbors;

    // Process fire particles first (highest priority)
    for (u32 i = 0; i < m_Pool.activeCount && interactionsThisFrame < m_MaxInteractionsPerFrame; ++i) {
        if (m_Pool.elements[i].x < 0.3f && m_Pool.elements[i].y < 0.3f) continue; // skip non-fire/water

        m_Pool.GetNeighbors(m_Pool.positions[i], INTERACTION_RADIUS, neighbors);

        for (u16 j : neighbors) {
            if (j == static_cast<u16>(i)) continue;
            if (interactionsThisFrame >= m_MaxInteractionsPerFrame) break;

            f32 reaction = ComputeReaction(m_Pool.elements[i], m_Pool.elements[j]);
            interactionsThisFrame++;

            if (reaction < -0.5f) {
                // Opposing: kill both, spawn product
                Math::Vector3 midpoint = (m_Pool.positions[i] + m_Pool.positions[j]) * 0.5f;
                if (spawnsThisFrame < MAX_SPAWNS_PER_FRAME) {
                    SpawnInteractionProduct(midpoint, m_Pool.elements[i], m_Pool.elements[j]);
                    spawnsThisFrame++;
                }
                m_Pool.lifetimes[i] = 0.0f;
                m_Pool.lifetimes[j] = 0.0f;
                break; // this particle is dead, move on
            } else if (reaction > 0.3f) {
                // Amplifying: boost the stronger particle
                if (m_Pool.intensities[i] > m_Pool.intensities[j]) {
                    m_Pool.intensities[i] = Math::Min(1.0f, m_Pool.intensities[i] + reaction * dt * 0.5f);
                } else {
                    m_Pool.intensities[j] = Math::Min(1.0f, m_Pool.intensities[j] + reaction * dt * 0.5f);
                }
            }
        }
    }
}

// ============================================================================
// Surface Accumulation
// ============================================================================

void ElementalSystem::UpdateSurfaceAccumulation(ECS::World* world, f32 dt) {
    if (!world) return;

    // Decay existing surface accumulations
    for (ECS::Entity entity : world->GetEntitiesWithComponent<ECS::ElementalSurfaceComponent>()) {
        auto* surface = world->GetComponent<ECS::ElementalSurfaceComponent>(entity);
        if (!surface) continue;

        // Natural decay
        surface->accumulation -= Math::Vector4(surface->decayRate) * dt;
        surface->accumulation.x = Math::Max(0.0f, surface->accumulation.x);
        surface->accumulation.y = Math::Max(0.0f, surface->accumulation.y);
        surface->accumulation.z = Math::Max(0.0f, surface->accumulation.z);
        surface->accumulation.w = Math::Max(0.0f, surface->accumulation.w);
    }

    // Deposit from grounded particles near entities
    for (u32 i = 0; i < m_Pool.activeCount; ++i) {
        if (!(m_Pool.flags[i] & EPF_GROUNDED) && m_Pool.positions[i].y > 2.0f) continue;

        for (ECS::Entity entity : world->GetEntitiesWithComponent<ECS::ElementalSurfaceComponent>()) {
            auto* surface = world->GetComponent<ECS::ElementalSurfaceComponent>(entity);
            auto* transform = world->GetComponent<ECS::TransformComponent>(entity);
            if (!surface || !transform) continue;

            // Simple proximity check (3m radius around entity)
            Math::Vector3 diff = m_Pool.positions[i] - transform->position;
            f32 distSq = diff.x * diff.x + diff.y * diff.y + diff.z * diff.z;
            if (distSq > 9.0f) continue; // 3m radius

            // Deposit element
            Math::Vector4 deposit = m_Pool.elements[i] * (m_Pool.intensities[i] * surface->accumulationRate * dt);
            surface->accumulation += deposit;

            // Clamp to max
            surface->accumulation.x = Math::Min(surface->accumulation.x, surface->maxAccumulation);
            surface->accumulation.y = Math::Min(surface->accumulation.y, surface->maxAccumulation);
            surface->accumulation.z = Math::Min(surface->accumulation.z, surface->maxAccumulation);
            surface->accumulation.w = Math::Min(surface->accumulation.w, surface->maxAccumulation);

            m_Pool.flags[i] |= EPF_ON_SURFACE;
        }
    }

    // Compute derived visual state from accumulation
    f32 temperature = 15.0f;
    if (m_Seasonal) {
        temperature = m_Seasonal->GetCurrentTemperature();
    }

    for (ECS::Entity entity : world->GetEntitiesWithComponent<ECS::ElementalSurfaceComponent>()) {
        auto* surface = world->GetComponent<ECS::ElementalSurfaceComponent>(entity);
        if (!surface) continue;

        surface->charAmount = Math::Clamp(surface->accumulation.x, 0.0f, 1.0f);
        surface->wetness = Math::Clamp(surface->accumulation.y, 0.0f, 1.0f);

        // Snow: requires cold temperature + water/air accumulation
        if (temperature <= 0.0f) {
            surface->snowCoverage = Math::Clamp(
                (surface->accumulation.y * 0.5f + surface->accumulation.w * 0.5f),
                0.0f, 1.0f);
            surface->frostAmount = Math::Clamp(surface->accumulation.y * 0.7f, 0.0f, 1.0f);
            // Cold makes wetness become frost instead
            surface->wetness *= Math::Clamp((temperature + 5.0f) / 5.0f, 0.0f, 1.0f);
        } else {
            // Warm: snow decays faster
            surface->snowCoverage = Math::Max(0.0f, surface->snowCoverage - dt * 0.1f);
            surface->frostAmount = Math::Max(0.0f, surface->frostAmount - dt * 0.1f);
        }
    }
}

// ============================================================================
// Weather Input
// ============================================================================

void ElementalSystem::UpdateWeatherInput(f32 dt, const Math::Vector3& cameraPos) {
    if (!m_Weather) return;

    f32 rainIntensity = m_Weather->GetRainIntensity();
    f32 snowIntensity = m_Weather->GetSnowIntensity();

    // Spawn rain particles
    if (rainIntensity > 0.01f) {
        f32 spawnRate = rainIntensity * 30.0f; // max 30/sec at full intensity
        m_RainSpawnAccumulator += spawnRate * dt;
        u32 maxRainParticles = 200;
        u32 currentWater = CountElementParticles(1);

        while (m_RainSpawnAccumulator >= 1.0f && currentWater < maxRainParticles) {
            m_RainSpawnAccumulator -= 1.0f;
            // Random position around camera
            f32 t = m_RainSpawnAccumulator * 1.618f + static_cast<f32>(m_Pool.activeCount) * 0.37f;
            f32 angle = t * Math::PI * 2.0f;
            f32 dist = 10.0f + std::sin(t * 7.3f) * 8.0f;
            Math::Vector3 pos = cameraPos + Math::Vector3(
                std::cos(angle) * dist,
                15.0f + std::sin(t * 3.1f) * 5.0f,
                std::sin(angle) * dist
            );

            Math::Vector3 vel(0.0f, -8.0f - rainIntensity * 4.0f, 0.0f);
            if (m_Wind) {
                vel += m_Wind->GetWindAt(pos) * 0.3f;
            }
            SpawnWater(pos, vel, rainIntensity * 0.6f);
            currentWater++;
        }
    } else {
        m_RainSpawnAccumulator = 0.0f;
    }

    // Spawn snow particles
    if (snowIntensity > 0.01f) {
        f32 spawnRate = snowIntensity * 20.0f;
        m_SnowSpawnAccumulator += spawnRate * dt;
        u32 maxSnowParticles = 150;
        u32 snowCount = 0;
        for (u32 i = 0; i < m_Pool.activeCount; ++i) {
            if (m_Pool.elements[i].w > 0.4f && m_Pool.elements[i].y > 0.2f) snowCount++;
        }

        while (m_SnowSpawnAccumulator >= 1.0f && snowCount < maxSnowParticles) {
            m_SnowSpawnAccumulator -= 1.0f;
            f32 t = m_SnowSpawnAccumulator * 2.718f + static_cast<f32>(m_Pool.activeCount) * 0.53f;
            f32 angle = t * Math::PI * 2.0f;
            f32 dist = 8.0f + std::sin(t * 5.1f) * 7.0f;
            Math::Vector3 pos = cameraPos + Math::Vector3(
                std::cos(angle) * dist,
                12.0f + std::sin(t * 2.3f) * 3.0f,
                std::sin(angle) * dist
            );
            SpawnSnow(pos, 0.05f + std::sin(t * 11.0f) * 0.03f);
            snowCount++;
        }
    } else {
        m_SnowSpawnAccumulator = 0.0f;
    }
}

// ============================================================================
// Fire Spread
// ============================================================================

void ElementalSystem::UpdateFireSpread(f32 dt) {
    // Fire on flammable surfaces can spread
    static f32 spreadTimer = 0.0f;
    spreadTimer += dt;
    if (spreadTimer < 0.5f) return; // check twice per second
    spreadTimer = 0.0f;

    u32 fireCount = CountElementParticles(0);
    if (fireCount >= m_MaxFireParticles) return;

    u32 newFires = 0;
    static constexpr u32 MAX_NEW_FIRES_PER_CHECK = 2;

    for (u32 i = 0; i < m_Pool.activeCount && newFires < MAX_NEW_FIRES_PER_CHECK; ++i) {
        if (m_Pool.elements[i].x < 0.5f) continue;
        if (!(m_Pool.flags[i] & EPF_ON_SURFACE)) continue;
        if (m_Pool.intensities[i] < 0.3f) continue;

        // Spread in wind direction with some randomness
        Math::Vector3 spreadDir(0.0f, 0.0f, 0.0f);
        if (m_Wind) {
            spreadDir = m_Wind->GetWindAt(m_Pool.positions[i]) * 0.3f;
        }
        // Add deterministic variation
        f32 t = static_cast<f32>(i) * 1.618f;
        spreadDir += Math::Vector3(std::cos(t) * 0.5f, 0.0f, std::sin(t) * 0.5f);

        Math::Vector3 newPos = m_Pool.positions[i] + spreadDir;
        SpawnFire(newPos, m_Pool.intensities[i] * 0.7f, m_Pool.maxLifetimes[i] * 0.8f);
        newFires++;
        fireCount++;
        if (fireCount >= m_MaxFireParticles) break;
    }
}

// ============================================================================
// Kill & Compact
// ============================================================================

void ElementalSystem::KillExpired() {
    u32 i = 0;
    while (i < m_Pool.activeCount) {
        if (m_Pool.lifetimes[i] <= 0.0f || m_Pool.intensities[i] <= 0.0f) {
            FreeParticle(i);
            // Don't increment i — the swapped particle at i needs checking too
        } else {
            ++i;
        }
    }
}

} // namespace Effects
} // namespace Enjin
