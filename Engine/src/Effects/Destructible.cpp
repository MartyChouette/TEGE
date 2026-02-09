#include "Enjin/Effects/Destructible.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Material.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include <cmath>
#include <algorithm>

namespace Enjin {
namespace Effects {

// ============================================================================
// Lifecycle
// ============================================================================

DestructibleSystem::DestructibleSystem() = default;
DestructibleSystem::~DestructibleSystem() = default;

void DestructibleSystem::Initialize(ECS::World* world) {
    m_World = world;
    m_DestructionQueue.clear();
    m_ActiveDebris.clear();
    m_EntityConfigs.clear();
}

void DestructibleSystem::Update(f32 deltaTime) {
    if (!m_World || deltaTime <= 0.0f) return;

    // Clamp delta for stability during long frames
    deltaTime = std::min(deltaTime, 1.0f / 30.0f);

    // Handle respawn timers on destroyed entities
    for (ECS::Entity entity : m_World->GetEntitiesWithComponent<ECS::DestructibleComponent>()) {
        auto* dc = m_World->GetComponent<ECS::DestructibleComponent>(entity);
        if (!dc || !dc->isDestroyed || !dc->canRespawn) continue;

        dc->respawnTimer -= deltaTime;
        if (dc->respawnTimer <= 0.0f) {
            dc->isDestroyed = false;
            dc->health = 1.0f;
            dc->respawnTimer = 0.0f;
            // Make visible again
            auto* transform = m_World->GetComponent<ECS::TransformComponent>(entity);
            if (transform) {
                transform->visible = true;
            }
        }
    }

    ProcessDestructionQueue();
    UpdateDebris(deltaTime);
}

void DestructibleSystem::Shutdown() {
    m_DestructionQueue.clear();
    m_ActiveDebris.clear();
    m_EntityConfigs.clear();
    m_World = nullptr;
}

// ============================================================================
// Public API
// ============================================================================

void DestructibleSystem::Destroy(ECS::Entity entity, const Math::Vector3& impactPoint,
                                  const Math::Vector3& impactDirection, f32 force) {
    if (!m_World || !m_World->IsValid(entity)) return;

    auto* dc = m_World->GetComponent<ECS::DestructibleComponent>(entity);
    if (!dc || dc->isDestroyed) return;

    DestructionEvent event;
    event.entity = entity;
    event.impactPoint = impactPoint;
    event.impactDirection = impactDirection;
    event.impactForce = force;
    event.chainDamage = 1.0f;

    m_DestructionQueue.push_back(event);
}

void DestructibleSystem::ApplyDamage(ECS::Entity entity, f32 damage,
                                      const Math::Vector3& impactPoint,
                                      const Math::Vector3& impactDir) {
    if (!m_World || !m_World->IsValid(entity)) return;

    auto* dc = m_World->GetComponent<ECS::DestructibleComponent>(entity);
    if (!dc || dc->isDestroyed) return;

    // One-hit destroy mode
    if (dc->destroyOnHit) {
        Destroy(entity, impactPoint, impactDir, 5.0f);
        return;
    }

    dc->health -= damage;
    if (dc->health <= 0.0f) {
        dc->health = 0.0f;
        Destroy(entity, impactPoint, impactDir, damage * 2.0f);
    }
}

void DestructibleSystem::SetFractureConfig(ECS::Entity entity, const FractureConfig& config) {
    m_EntityConfigs[entity] = config;
}

FractureConfig DestructibleSystem::GetFractureConfig(ECS::Entity entity) const {
    auto it = m_EntityConfigs.find(entity);
    if (it != m_EntityConfigs.end()) {
        return it->second;
    }
    return m_DefaultConfig;
}

void DestructibleSystem::SetDefaultFractureConfig(const FractureConfig& config) {
    m_DefaultConfig = config;
}

u32 DestructibleSystem::GetActiveDebrisCount() const {
    return static_cast<u32>(m_ActiveDebris.size());
}

// ============================================================================
// Destruction processing
// ============================================================================

void DestructibleSystem::ProcessDestructionQueue() {
    if (m_DestructionQueue.empty()) return;

    // Process a copy so chain destruction can append new events
    std::vector<DestructionEvent> queue;
    queue.swap(m_DestructionQueue);

    for (const auto& event : queue) {
        if (!m_World->IsValid(event.entity)) continue;

        auto* dc = m_World->GetComponent<ECS::DestructibleComponent>(event.entity);
        if (!dc || dc->isDestroyed) continue;

        // Mark as destroyed
        dc->isDestroyed = true;
        if (dc->canRespawn) {
            dc->respawnTimer = dc->respawnTime;
        }

        // Hide the original entity
        auto* transform = m_World->GetComponent<ECS::TransformComponent>(event.entity);
        if (transform) {
            transform->visible = false;
        }

        // Get fracture config
        FractureConfig config = GetFractureConfig(event.entity);

        // Generate fragments based on pattern
        std::vector<DebrisFragment> fragments;
        fragments.reserve(config.fragmentCount);

        switch (config.pattern) {
            case FracturePattern::Voronoi:
                GenerateVoronoiFragments(event, fragments);
                break;
            case FracturePattern::Grid:
                GenerateGridFragments(event, fragments);
                break;
            case FracturePattern::Radial:
                GenerateRadialFragments(event, fragments);
                break;
            case FracturePattern::Shatter:
                GenerateShatterFragments(event, fragments);
                break;
        }

        // Add all fragments to active debris
        for (auto& frag : fragments) {
            m_ActiveDebris.push_back(std::move(frag));
        }

        // Check for chain destruction
        if (config.chainDestruction) {
            CheckChainDestruction(event);
        }
    }
}

// ============================================================================
// Fragment generation
// ============================================================================

void DestructibleSystem::GenerateVoronoiFragments(const DestructionEvent& event,
                                                    std::vector<DebrisFragment>& outFragments) {
    FractureConfig config = GetFractureConfig(event.entity);
    auto* transform = m_World->GetComponent<ECS::TransformComponent>(event.entity);
    if (!transform) return;

    Math::Vector3 entityPos = transform->position;
    Math::Vector3 entityScale = transform->scale;

    // Try to get material color for debris tinting
    Math::Vector3 baseColor(0.6f, 0.6f, 0.6f);
    auto* mat = m_World->GetComponent<ECS::MaterialComponent>(event.entity);
    if (mat) {
        baseColor = mat->baseColor;
    }

    // Generate N random seed points within entity bounds, create a fragment for each
    for (u32 i = 0; i < config.fragmentCount; ++i) {
        DebrisFragment frag;

        // Random seed point within the entity AABB
        f32 ox = RandomRange(-0.5f, 0.5f) * entityScale.x;
        f32 oy = RandomRange(-0.5f, 0.5f) * entityScale.y;
        f32 oz = RandomRange(-0.5f, 0.5f) * entityScale.z;
        frag.position = Math::Vector3(entityPos.x + ox, entityPos.y + oy, entityPos.z + oz);

        // Fragment scale varies
        f32 s = RandomRange(config.fragmentScaleMin, config.fragmentScaleMax);
        frag.scale = Math::Vector3(s * entityScale.x * 0.5f, s * entityScale.y * 0.5f, s * entityScale.z * 0.5f);

        // Velocity: outward from impact + explosion force
        Math::Vector3 dir = frag.position - event.impactPoint;
        f32 dirLen = dir.Length();
        if (dirLen > 0.001f) {
            dir = dir / dirLen;
        } else {
            dir = Math::Vector3(RandomRange(-1.0f, 1.0f), RandomRange(0.2f, 1.0f), RandomRange(-1.0f, 1.0f));
            dir.Normalize();
        }
        f32 forceMag = config.explosionForce * RandomRange(0.5f, 1.5f);
        frag.velocity = dir * forceMag + event.impactDirection * (event.impactForce * 0.3f);

        // Random angular velocity
        frag.angularVelocity = Math::Vector3(RandomRange(-5.0f, 5.0f),
                                              RandomRange(-5.0f, 5.0f),
                                              RandomRange(-5.0f, 5.0f));

        frag.lifetime = config.debrisLifetime * RandomRange(0.7f, 1.3f);
        frag.age = 0.0f;

        // Simple box mesh for the fragment (8 vertices, 36 indices for a cube)
        f32 hs = 0.5f; // half-size, scaled later by frag.scale
        frag.vertices = {
            Math::Vector3(-hs, -hs, -hs), Math::Vector3( hs, -hs, -hs),
            Math::Vector3( hs,  hs, -hs), Math::Vector3(-hs,  hs, -hs),
            Math::Vector3(-hs, -hs,  hs), Math::Vector3( hs, -hs,  hs),
            Math::Vector3( hs,  hs,  hs), Math::Vector3(-hs,  hs,  hs)
        };
        frag.indices = {
            0,1,2, 0,2,3,  // front
            4,6,5, 4,7,6,  // back
            0,3,7, 0,7,4,  // left
            1,5,6, 1,6,2,  // right
            3,2,6, 3,6,7,  // top
            0,4,5, 0,5,1   // bottom
        };

        // Color: slight random variation from base
        frag.color = Math::Vector3(
            std::max(0.0f, std::min(1.0f, baseColor.x + RandomRange(-0.1f, 0.1f))),
            std::max(0.0f, std::min(1.0f, baseColor.y + RandomRange(-0.1f, 0.1f))),
            std::max(0.0f, std::min(1.0f, baseColor.z + RandomRange(-0.1f, 0.1f)))
        );

        outFragments.push_back(std::move(frag));
    }
}

void DestructibleSystem::GenerateGridFragments(const DestructionEvent& event,
                                                std::vector<DebrisFragment>& outFragments) {
    FractureConfig config = GetFractureConfig(event.entity);
    auto* transform = m_World->GetComponent<ECS::TransformComponent>(event.entity);
    if (!transform) return;

    Math::Vector3 entityPos = transform->position;
    Math::Vector3 entityScale = transform->scale;

    Math::Vector3 baseColor(0.6f, 0.6f, 0.6f);
    auto* mat = m_World->GetComponent<ECS::MaterialComponent>(event.entity);
    if (mat) {
        baseColor = mat->baseColor;
    }

    // Determine grid dimensions: cube root of fragmentCount for NxNxN
    u32 gridN = std::max(1u, static_cast<u32>(std::cbrt(static_cast<f32>(config.fragmentCount))));
    f32 cellX = entityScale.x / static_cast<f32>(gridN);
    f32 cellY = entityScale.y / static_cast<f32>(gridN);
    f32 cellZ = entityScale.z / static_cast<f32>(gridN);

    Math::Vector3 origin = entityPos - entityScale * 0.5f;

    for (u32 iz = 0; iz < gridN; ++iz) {
        for (u32 iy = 0; iy < gridN; ++iy) {
            for (u32 ix = 0; ix < gridN; ++ix) {
                DebrisFragment frag;

                // Center of this grid cell
                frag.position = Math::Vector3(
                    origin.x + (static_cast<f32>(ix) + 0.5f) * cellX,
                    origin.y + (static_cast<f32>(iy) + 0.5f) * cellY,
                    origin.z + (static_cast<f32>(iz) + 0.5f) * cellZ
                );

                frag.scale = Math::Vector3(cellX * 0.48f, cellY * 0.48f, cellZ * 0.48f);

                // Velocity: outward from impact
                Math::Vector3 dir = frag.position - event.impactPoint;
                f32 dirLen = dir.Length();
                if (dirLen > 0.001f) dir = dir / dirLen;
                else dir = Math::Vector3(0.0f, 1.0f, 0.0f);

                frag.velocity = dir * config.explosionForce * RandomRange(0.6f, 1.2f);
                frag.angularVelocity = Math::Vector3(RandomRange(-3.0f, 3.0f),
                                                      RandomRange(-3.0f, 3.0f),
                                                      RandomRange(-3.0f, 3.0f));
                frag.lifetime = config.debrisLifetime * RandomRange(0.8f, 1.2f);
                frag.age = 0.0f;

                // Box mesh
                f32 hs = 0.5f;
                frag.vertices = {
                    Math::Vector3(-hs, -hs, -hs), Math::Vector3( hs, -hs, -hs),
                    Math::Vector3( hs,  hs, -hs), Math::Vector3(-hs,  hs, -hs),
                    Math::Vector3(-hs, -hs,  hs), Math::Vector3( hs, -hs,  hs),
                    Math::Vector3( hs,  hs,  hs), Math::Vector3(-hs,  hs,  hs)
                };
                frag.indices = {
                    0,1,2, 0,2,3,  4,6,5, 4,7,6,
                    0,3,7, 0,7,4,  1,5,6, 1,6,2,
                    3,2,6, 3,6,7,  0,4,5, 0,5,1
                };

                frag.color = Math::Vector3(
                    std::max(0.0f, std::min(1.0f, baseColor.x + RandomRange(-0.05f, 0.05f))),
                    std::max(0.0f, std::min(1.0f, baseColor.y + RandomRange(-0.05f, 0.05f))),
                    std::max(0.0f, std::min(1.0f, baseColor.z + RandomRange(-0.05f, 0.05f)))
                );

                outFragments.push_back(std::move(frag));
            }
        }
    }
}

void DestructibleSystem::GenerateRadialFragments(const DestructionEvent& event,
                                                   std::vector<DebrisFragment>& outFragments) {
    FractureConfig config = GetFractureConfig(event.entity);
    auto* transform = m_World->GetComponent<ECS::TransformComponent>(event.entity);
    if (!transform) return;

    Math::Vector3 entityPos = transform->position;
    Math::Vector3 entityScale = transform->scale;

    Math::Vector3 baseColor(0.6f, 0.6f, 0.6f);
    auto* mat = m_World->GetComponent<ECS::MaterialComponent>(event.entity);
    if (mat) {
        baseColor = mat->baseColor;
    }

    f32 maxExtent = std::max({entityScale.x, entityScale.y, entityScale.z}) * 0.5f;
    u32 count = std::max(3u, config.fragmentCount);
    f32 angleStep = 6.283185f / static_cast<f32>(count); // 2*PI / count
    constexpr f32 PI = 3.14159265f;

    for (u32 i = 0; i < count; ++i) {
        DebrisFragment frag;

        f32 angle = static_cast<f32>(i) * angleStep + RandomRange(-0.1f, 0.1f);
        f32 cosA = std::cos(angle);
        f32 sinA = std::sin(angle);

        // Wedge-shaped fragment radiating outward from impact point
        f32 dist = maxExtent * RandomRange(0.3f, 0.7f);
        frag.position = Math::Vector3(
            event.impactPoint.x + cosA * dist,
            event.impactPoint.y + RandomRange(-0.2f, 0.2f) * entityScale.y,
            event.impactPoint.z + sinA * dist
        );

        f32 s = RandomRange(config.fragmentScaleMin, config.fragmentScaleMax);
        frag.scale = Math::Vector3(s * maxExtent * 0.3f, s * entityScale.y * 0.4f, s * maxExtent * 0.3f);

        // Velocity: radially outward
        Math::Vector3 radialDir(cosA, RandomRange(0.2f, 0.8f), sinA);
        radialDir.Normalize();
        frag.velocity = radialDir * config.explosionForce * RandomRange(0.7f, 1.4f);

        frag.angularVelocity = Math::Vector3(RandomRange(-4.0f, 4.0f),
                                              RandomRange(-4.0f, 4.0f),
                                              RandomRange(-4.0f, 4.0f));
        frag.lifetime = config.debrisLifetime * RandomRange(0.6f, 1.4f);
        frag.age = 0.0f;

        // Wedge mesh: triangular prism (6 vertices, 8 triangles = 24 indices)
        f32 innerR = 0.1f;
        f32 outerR = 0.5f;
        f32 halfH = 0.5f;
        f32 halfAngle = angleStep * 0.45f;
        f32 c0 = std::cos(-halfAngle), s0 = std::sin(-halfAngle);
        f32 c1 = std::cos(halfAngle), s1 = std::sin(halfAngle);

        frag.vertices = {
            // Bottom ring
            Math::Vector3(c0 * innerR, -halfH, s0 * innerR),  // 0
            Math::Vector3(c0 * outerR, -halfH, s0 * outerR),  // 1
            Math::Vector3(c1 * outerR, -halfH, s1 * outerR),  // 2
            Math::Vector3(c1 * innerR, -halfH, s1 * innerR),  // 3
            // Top ring
            Math::Vector3(c0 * innerR,  halfH, s0 * innerR),  // 4
            Math::Vector3(c0 * outerR,  halfH, s0 * outerR),  // 5
            Math::Vector3(c1 * outerR,  halfH, s1 * outerR),  // 6
            Math::Vector3(c1 * innerR,  halfH, s1 * innerR)   // 7
        };
        frag.indices = {
            // Bottom
            0,1,2, 0,2,3,
            // Top
            4,6,5, 4,7,6,
            // Outer
            1,5,6, 1,6,2,
            // Inner
            0,3,7, 0,7,4,
            // Side A
            0,4,5, 0,5,1,
            // Side B
            3,2,6, 3,6,7
        };

        frag.color = Math::Vector3(
            std::max(0.0f, std::min(1.0f, baseColor.x + RandomRange(-0.1f, 0.1f))),
            std::max(0.0f, std::min(1.0f, baseColor.y + RandomRange(-0.1f, 0.1f))),
            std::max(0.0f, std::min(1.0f, baseColor.z + RandomRange(-0.1f, 0.1f)))
        );

        outFragments.push_back(std::move(frag));
    }
}

void DestructibleSystem::GenerateShatterFragments(const DestructionEvent& event,
                                                    std::vector<DebrisFragment>& outFragments) {
    FractureConfig config = GetFractureConfig(event.entity);
    auto* transform = m_World->GetComponent<ECS::TransformComponent>(event.entity);
    if (!transform) return;

    Math::Vector3 entityPos = transform->position;
    Math::Vector3 entityScale = transform->scale;

    Math::Vector3 baseColor(0.6f, 0.6f, 0.6f);
    auto* mat = m_World->GetComponent<ECS::MaterialComponent>(event.entity);
    if (mat) {
        baseColor = mat->baseColor;
    }

    // Glass-like shatter: random triangular fragments from impact point
    for (u32 i = 0; i < config.fragmentCount; ++i) {
        DebrisFragment frag;

        // Random triangle near impact point
        Math::Vector3 center(
            event.impactPoint.x + RandomRange(-0.5f, 0.5f) * entityScale.x,
            event.impactPoint.y + RandomRange(-0.5f, 0.5f) * entityScale.y,
            event.impactPoint.z + RandomRange(-0.5f, 0.5f) * entityScale.z
        );
        frag.position = center;

        f32 s = RandomRange(config.fragmentScaleMin, config.fragmentScaleMax);
        f32 fragSize = s * std::max({entityScale.x, entityScale.y, entityScale.z}) * 0.15f;
        frag.scale = Math::Vector3(fragSize, fragSize, fragSize);

        // Velocity: from impact outward with more randomness for glass effect
        Math::Vector3 dir = center - event.impactPoint;
        f32 dirLen = dir.Length();
        if (dirLen > 0.001f) dir = dir / dirLen;
        else dir = Math::Vector3(RandomRange(-1.0f, 1.0f), RandomRange(0.0f, 1.0f), RandomRange(-1.0f, 1.0f)).Normalized();

        // Glass fragments fly faster and more erratically
        frag.velocity = dir * config.explosionForce * RandomRange(0.8f, 2.0f)
                       + event.impactDirection * event.impactForce * 0.5f;
        frag.velocity.y += RandomRange(1.0f, 3.0f); // Upward scatter

        frag.angularVelocity = Math::Vector3(RandomRange(-8.0f, 8.0f),
                                              RandomRange(-8.0f, 8.0f),
                                              RandomRange(-8.0f, 8.0f));
        frag.lifetime = config.debrisLifetime * RandomRange(0.5f, 1.0f);
        frag.age = 0.0f;

        // Thin triangular shard (triangular prism with minimal depth)
        f32 depth = 0.05f; // Very thin for glass-like appearance
        frag.vertices = {
            // Front face triangle
            Math::Vector3(0.0f,  0.5f, -depth),
            Math::Vector3(-0.4f, -0.3f + RandomRange(-0.2f, 0.0f), -depth),
            Math::Vector3( 0.4f, -0.3f + RandomRange(-0.2f, 0.0f), -depth),
            // Back face triangle
            Math::Vector3(0.0f,  0.5f,  depth),
            Math::Vector3(-0.4f, -0.3f + RandomRange(-0.2f, 0.0f),  depth),
            Math::Vector3( 0.4f, -0.3f + RandomRange(-0.2f, 0.0f),  depth)
        };
        frag.indices = {
            // Front
            0, 1, 2,
            // Back
            3, 5, 4,
            // Edges
            0, 3, 4, 0, 4, 1,
            1, 4, 5, 1, 5, 2,
            2, 5, 3, 2, 3, 0
        };

        frag.color = Math::Vector3(
            std::max(0.0f, std::min(1.0f, baseColor.x + RandomRange(-0.15f, 0.15f))),
            std::max(0.0f, std::min(1.0f, baseColor.y + RandomRange(-0.15f, 0.15f))),
            std::max(0.0f, std::min(1.0f, baseColor.z + RandomRange(-0.15f, 0.15f)))
        );

        outFragments.push_back(std::move(frag));
    }
}

// ============================================================================
// Debris simulation
// ============================================================================

void DestructibleSystem::UpdateDebris(f32 deltaTime) {
    if (m_ActiveDebris.empty()) return;

    // Update each fragment and remove expired ones
    for (auto it = m_ActiveDebris.begin(); it != m_ActiveDebris.end(); ) {
        DebrisFragment& frag = *it;
        frag.age += deltaTime;

        if (frag.age >= frag.lifetime) {
            it = m_ActiveDebris.erase(it);
            continue;
        }

        // Determine gravity from the config of the source (use default as fallback)
        f32 gravity = m_DefaultConfig.debrisGravity;

        // Apply gravity
        frag.velocity.y -= gravity * deltaTime;

        // Apply velocity
        frag.position = frag.position + frag.velocity * deltaTime;

        // Simple floor collision at y=0
        if (frag.position.y < 0.0f) {
            frag.position.y = 0.0f;
            frag.velocity.y = -frag.velocity.y * 0.3f; // Bounce with damping
            frag.velocity.x *= 0.8f; // Friction
            frag.velocity.z *= 0.8f;
            frag.angularVelocity = frag.angularVelocity * 0.5f; // Slow spin on bounce
        }

        // Dampen velocity over time (air resistance)
        f32 drag = 1.0f - 0.5f * deltaTime;
        frag.velocity = frag.velocity * drag;

        ++it;
    }
}

// ============================================================================
// Chain destruction
// ============================================================================

void DestructibleSystem::CheckChainDestruction(const DestructionEvent& event) {
    if (!m_World) return;

    FractureConfig config = GetFractureConfig(event.entity);
    f32 radiusSq = config.chainRadius * config.chainRadius;
    f32 chainDamage = event.chainDamage * config.chainDamageFalloff;

    // Don't propagate if damage is negligible
    if (chainDamage < 0.01f) return;

    auto* srcTransform = m_World->GetComponent<ECS::TransformComponent>(event.entity);
    if (!srcTransform) return;
    Math::Vector3 srcPos = srcTransform->position;

    for (ECS::Entity other : m_World->GetEntitiesWithComponent<ECS::DestructibleComponent>()) {
        if (other == event.entity) continue;

        auto* otherDc = m_World->GetComponent<ECS::DestructibleComponent>(other);
        if (!otherDc || otherDc->isDestroyed) continue;

        auto* otherTransform = m_World->GetComponent<ECS::TransformComponent>(other);
        if (!otherTransform) continue;

        Math::Vector3 diff = otherTransform->position - srcPos;
        f32 distSq = diff.x * diff.x + diff.y * diff.y + diff.z * diff.z;

        if (distSq <= radiusSq) {
            // Queue chain destruction event
            DestructionEvent chainEvent;
            chainEvent.entity = other;
            chainEvent.impactPoint = otherTransform->position;

            // Impact direction is away from the source
            f32 dist = std::sqrt(distSq);
            if (dist > 0.001f) {
                chainEvent.impactDirection = diff / dist;
            } else {
                chainEvent.impactDirection = Math::Vector3(0.0f, 1.0f, 0.0f);
            }

            chainEvent.impactForce = config.explosionForce * chainDamage;
            chainEvent.chainDamage = chainDamage;

            // Apply chain damage to health
            if (!otherDc->destroyOnHit) {
                otherDc->health -= chainDamage;
                if (otherDc->health > 0.0f) continue; // Not enough to destroy
            }

            m_DestructionQueue.push_back(chainEvent);
        }
    }
}

// ============================================================================
// Random helpers (LCG)
// ============================================================================

u32 DestructibleSystem::Random() {
    m_RandomState = m_RandomState * 1664525u + 1013904223u;
    return m_RandomState;
}

f32 DestructibleSystem::RandomFloat() {
    return static_cast<f32>(Random() & 0x00FFFFFFu) / static_cast<f32>(0x00FFFFFFu);
}

f32 DestructibleSystem::RandomRange(f32 min, f32 max) {
    return min + RandomFloat() * (max - min);
}

} // namespace Effects
} // namespace Enjin
