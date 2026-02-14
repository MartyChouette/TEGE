#include "Enjin/Effects/InteractiveWater.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include <algorithm>
#include <cmath>

namespace Enjin {
namespace Effects {

// ============================================================================
// Initialization
// ============================================================================

void InteractiveWaterSystem::Initialize(InteractiveWaterComponent& water) {
    // Clamp grid resolution to valid range
    water.gridResolution = static_cast<i32>(Math::Clamp(static_cast<f32>(water.gridResolution), 16.0f, 256.0f));
    i32 res = water.gridResolution;
    i32 totalCells = res * res;

    water.heightField.assign(totalCells, water.baseHeight);
    water.previousField.assign(totalCells, water.baseHeight);
    water.velocityField.assign(totalCells, 0.0f);
    water.accumulatedScrollTime = 0.0f;
    water.initialized = true;
}

// ============================================================================
// Grid Helpers
// ============================================================================

f32 InteractiveWaterSystem::GetCellSize(const InteractiveWaterComponent& water) const {
    return water.gridSize / static_cast<f32>(water.gridResolution - 1);
}

bool InteractiveWaterSystem::WorldToGrid(const InteractiveWaterComponent& water,
                                          const ECS::TransformComponent& transform,
                                          f32 worldX, f32 worldZ,
                                          f32& gridX, f32& gridZ) const {
    // Water is centered at transform position, extends gridSize/2 in each direction
    f32 halfSize = water.gridSize * 0.5f;
    f32 localX = worldX - transform.position.x + halfSize;
    f32 localZ = worldZ - transform.position.z + halfSize;

    f32 cellSize = GetCellSize(water);
    if (cellSize < Math::EPSILON) return false;

    gridX = localX / cellSize;
    gridZ = localZ / cellSize;

    i32 res = water.gridResolution;
    return (gridX >= 0.0f && gridX < static_cast<f32>(res - 1) &&
            gridZ >= 0.0f && gridZ < static_cast<f32>(res - 1));
}

// ============================================================================
// Wave Propagation (Spring-Damper Model)
// ============================================================================

void InteractiveWaterSystem::PropagateWaves(InteractiveWaterComponent& water, f32 dt) {
    i32 res = water.gridResolution;
    if (res < 3) return;

    f32 tension = water.tension;
    f32 damping = water.damping;
    f32 speed = water.waveSpeed;

    // Interior cells only (edges handled by boundary step)
    for (i32 j = 1; j < res - 1; ++j) {
        for (i32 i = 1; i < res - 1; ++i) {
            i32 idx = j * res + i;

            // Four-neighbor average
            f32 left  = water.heightField[idx - 1];
            f32 right = water.heightField[idx + 1];
            f32 up    = water.heightField[(j - 1) * res + i];
            f32 down  = water.heightField[(j + 1) * res + i];
            f32 neighborAvg = (left + right + up + down) * 0.25f;

            // Spring-damper: acceleration toward neighbor average
            water.velocityField[idx] += tension * (neighborAvg - water.heightField[idx]);
            water.velocityField[idx] *= damping;

            // Integrate
            water.heightField[idx] += water.velocityField[idx] * speed * dt;
        }
    }
}

// ============================================================================
// Boundary Conditions
// ============================================================================

void InteractiveWaterSystem::ApplyBoundary(InteractiveWaterComponent& water) {
    i32 res = water.gridResolution;
    if (res < 2) return;

    if (water.boundaryMode == InteractiveWaterComponent::BoundaryMode::Absorbing) {
        // Absorbing: clamp edges to base height, zero velocity
        for (i32 i = 0; i < res; ++i) {
            // Top and bottom rows
            water.heightField[i] = water.baseHeight;                    // row 0
            water.heightField[(res - 1) * res + i] = water.baseHeight;  // last row
            water.velocityField[i] = 0.0f;
            water.velocityField[(res - 1) * res + i] = 0.0f;

            // Left and right columns
            water.heightField[i * res] = water.baseHeight;
            water.heightField[i * res + (res - 1)] = water.baseHeight;
            water.velocityField[i * res] = 0.0f;
            water.velocityField[i * res + (res - 1)] = 0.0f;
        }
    } else {
        // Reflecting: copy neighbor values so waves bounce back
        for (i32 i = 1; i < res - 1; ++i) {
            // Top row mirrors second row
            water.heightField[i] = water.heightField[res + i];
            water.velocityField[i] = water.velocityField[res + i];

            // Bottom row mirrors second-to-last row
            water.heightField[(res - 1) * res + i] = water.heightField[(res - 2) * res + i];
            water.velocityField[(res - 1) * res + i] = water.velocityField[(res - 2) * res + i];

            // Left column mirrors second column
            water.heightField[i * res] = water.heightField[i * res + 1];
            water.velocityField[i * res] = water.velocityField[i * res + 1];

            // Right column mirrors second-to-last column
            water.heightField[i * res + (res - 1)] = water.heightField[i * res + (res - 2)];
            water.velocityField[i * res + (res - 1)] = water.velocityField[i * res + (res - 2)];
        }
    }
}

// ============================================================================
// Water Height Query (Bilinear Interpolation)
// ============================================================================

f32 InteractiveWaterSystem::GetWaterHeight(const InteractiveWaterComponent& water,
                                            const ECS::TransformComponent& transform,
                                            f32 worldX, f32 worldZ) const {
    if (!water.initialized || water.heightField.empty()) {
        return water.baseHeight + transform.position.y;
    }

    f32 gx, gz;
    if (!WorldToGrid(water, transform, worldX, worldZ, gx, gz)) {
        return water.baseHeight + transform.position.y;
    }

    i32 res = water.gridResolution;
    i32 x0 = static_cast<i32>(gx);
    i32 z0 = static_cast<i32>(gz);
    i32 x1 = (x0 + 1 < res - 1) ? x0 + 1 : res - 1;
    i32 z1 = (z0 + 1 < res - 1) ? z0 + 1 : res - 1;

    f32 fx = gx - static_cast<f32>(x0);
    f32 fz = gz - static_cast<f32>(z0);

    // Bilinear interpolation
    f32 h00 = water.heightField[z0 * res + x0];
    f32 h10 = water.heightField[z0 * res + x1];
    f32 h01 = water.heightField[z1 * res + x0];
    f32 h11 = water.heightField[z1 * res + x1];

    f32 h0 = h00 + (h10 - h00) * fx;
    f32 h1 = h01 + (h11 - h01) * fx;
    f32 localHeight = h0 + (h1 - h0) * fz;

    return localHeight + transform.position.y;
}

// ============================================================================
// Splash & Wake
// ============================================================================

void InteractiveWaterSystem::CreateSplash(InteractiveWaterComponent& water,
                                           const ECS::TransformComponent& transform,
                                           f32 worldX, f32 worldZ, f32 strength) {
    if (!water.initialized) return;

    f32 gx, gz;
    if (!WorldToGrid(water, transform, worldX, worldZ, gx, gz)) return;

    i32 res = water.gridResolution;
    i32 cx = static_cast<i32>(gx);
    i32 cz = static_cast<i32>(gz);

    // Apply impulse to cells within interaction radius
    i32 radius = static_cast<i32>(Math::Ceil(water.interactionRadius));
    if (radius < 1) radius = 1;

    f32 radiusSq = water.interactionRadius * water.interactionRadius;

    for (i32 dz = -radius; dz <= radius; ++dz) {
        for (i32 dx = -radius; dx <= radius; ++dx) {
            i32 ix = cx + dx;
            i32 iz = cz + dz;

            // Bounds check (skip boundary cells)
            if (ix < 1 || ix >= res - 1 || iz < 1 || iz >= res - 1) continue;

            f32 distSq = static_cast<f32>(dx * dx + dz * dz);
            if (distSq > radiusSq) continue;

            // Gaussian-like falloff
            f32 falloff = 1.0f - (distSq / (radiusSq + 0.001f));
            falloff = falloff * falloff; // Quadratic falloff

            i32 idx = iz * res + ix;
            water.velocityField[idx] -= strength * falloff * water.interactionStrength;
        }
    }
}

void InteractiveWaterSystem::CreateWake(InteractiveWaterComponent& water,
                                         const ECS::TransformComponent& transform,
                                         f32 worldX, f32 worldZ,
                                         f32 velocityX, f32 velocityZ,
                                         f32 wakeWidth) {
    if (!water.initialized) return;

    f32 speed = Math::Sqrt(velocityX * velocityX + velocityZ * velocityZ);
    if (speed < 0.01f) return; // No wake for nearly stationary objects

    f32 gx, gz;
    if (!WorldToGrid(water, transform, worldX, worldZ, gx, gz)) return;

    i32 res = water.gridResolution;
    i32 cx = static_cast<i32>(gx);
    i32 cz = static_cast<i32>(gz);

    // Normalize velocity direction
    f32 invSpeed = 1.0f / speed;
    f32 dirX = velocityX * invSpeed;
    f32 dirZ = velocityZ * invSpeed;

    // Perpendicular direction for wake width
    f32 perpX = -dirZ;
    f32 perpZ = dirX;

    // Wake strength proportional to speed
    f32 wakeStrength = Math::Min(speed * 0.1f, 1.0f) * water.interactionStrength;

    // Create V-shaped wake: two arms angled behind the object
    i32 wakeLength = static_cast<i32>(Math::Ceil(water.interactionRadius * 2.0f));
    if (wakeLength < 2) wakeLength = 2;

    for (i32 d = 1; d <= wakeLength; ++d) {
        f32 t = static_cast<f32>(d) / static_cast<f32>(wakeLength);
        f32 spread = t * wakeWidth; // Wake widens with distance behind

        // Two points on the V-wake arms
        for (i32 side = -1; side <= 1; side += 2) {
            f32 wx = gx - dirX * static_cast<f32>(d) + perpX * spread * static_cast<f32>(side);
            f32 wz = gz - dirZ * static_cast<f32>(d) + perpZ * spread * static_cast<f32>(side);

            i32 ix = static_cast<i32>(wx);
            i32 iz = static_cast<i32>(wz);

            if (ix < 1 || ix >= res - 1 || iz < 1 || iz >= res - 1) continue;

            f32 falloff = 1.0f - t; // Weaker further behind
            i32 idx = iz * res + ix;
            water.velocityField[idx] -= wakeStrength * falloff * 0.5f;
        }
    }

    // Central disturbance at current position
    if (cx >= 1 && cx < res - 1 && cz >= 1 && cz < res - 1) {
        water.velocityField[cz * res + cx] -= wakeStrength * 0.3f;
    }
}

// ============================================================================
// Main Update
// ============================================================================

void InteractiveWaterSystem::Update(ECS::World* world, f32 dt) {
    if (!world || dt <= 0.0f) return;

    const auto& waterEntities = world->GetEntitiesWithComponent<InteractiveWaterComponent>();
    if (waterEntities.empty()) return;

    // Cache interactor entities for reuse across water bodies
    m_InteractorCache.clear();
    {
        const auto& interactors = world->GetEntitiesWithComponent<WaterInteractorComponent>();
        m_InteractorCache.assign(interactors.begin(), interactors.end());
    }

    for (ECS::Entity waterEntity : waterEntities) {
        auto* water = world->GetComponent<InteractiveWaterComponent>(waterEntity);
        auto* waterTransform = world->GetComponent<ECS::TransformComponent>(waterEntity);
        if (!water || !waterTransform) continue;

        // Initialize if needed
        if (!water->initialized) {
            Initialize(*water);
        }

        // 1. Wave propagation (spring-damper model)
        PropagateWaves(*water, dt);

        // 2. Object interaction (splashes and wakes)
        for (ECS::Entity interactor : m_InteractorCache) {
            auto* interactorComp = world->GetComponent<WaterInteractorComponent>(interactor);
            auto* transform = world->GetComponent<ECS::TransformComponent>(interactor);
            if (!interactorComp || !transform) continue;

            f32 entityX = transform->position.x;
            f32 entityY = transform->position.y;
            f32 entityZ = transform->position.z;

            // Check if entity is near the water plane
            f32 waterHeight = GetWaterHeight(*water, *waterTransform, entityX, entityZ);

            // Only interact if within reasonable distance of water surface
            f32 heightDiff = entityY - waterHeight;
            if (Math::Abs(heightDiff) > water->gridSize * 0.5f) continue;

            // Get entity velocity from RigidbodyComponent if available
            auto* rb = world->GetComponent<ECS::RigidbodyComponent>(interactor);
            Math::Vector3 entityVel(0, 0, 0);
            if (rb) {
                entityVel = rb->velocity;
            }

            // Apply splash when entity enters water (moving downward through surface)
            if (heightDiff < 0.0f) {
                // Entity is below water surface
                f32 impactStrength = Math::Abs(entityVel.y) * interactorComp->splashMultiplier;
                if (impactStrength > 0.01f) {
                    CreateSplash(*water, *waterTransform, entityX, entityZ, impactStrength);
                }

                // Generate wake from horizontal movement
                if (interactorComp->generateWake) {
                    CreateWake(*water, *waterTransform, entityX, entityZ,
                              entityVel.x, entityVel.z, interactorComp->wakeWidth);
                }
            }

            // 3. Buoyancy
            if (water->enableBuoyancy && interactorComp->applyBuoyancy && rb) {
                if (entityY < waterHeight) {
                    f32 submergedDepth = waterHeight - entityY;
                    submergedDepth = Math::Min(submergedDepth, water->gridSize * 0.25f);

                    // Upward buoyancy force
                    f32 buoyancy = water->buoyancyForce * submergedDepth;
                    rb->velocity.y += buoyancy * dt;

                    // Water drag on horizontal velocity
                    f32 dragFactor = 1.0f - water->waterDrag * dt;
                    dragFactor = Math::Max(dragFactor, 0.0f);
                    rb->velocity.x *= dragFactor;
                    rb->velocity.z *= dragFactor;

                    // Slight vertical drag too
                    rb->velocity.y *= 1.0f - (water->waterDrag * 0.3f * dt);
                }
            }
        }

        // 4. Apply boundary conditions
        ApplyBoundary(*water);

        // Update UV scroll time
        water->accumulatedScrollTime += dt;
    }
}

// ============================================================================
// Mesh Generation
// ============================================================================

ECS::MeshComponent InteractiveWaterSystem::GenerateMesh(
    const InteractiveWaterComponent& water,
    const ECS::TransformComponent& transform) const {

    ECS::MeshComponent mesh;
    i32 res = water.gridResolution;
    if (res < 2 || !water.initialized) return mesh;

    f32 cellSize = GetCellSize(water);
    f32 halfSize = water.gridSize * 0.5f;
    f32 uvScroll = water.accumulatedScrollTime * water.uvScrollSpeed;
    f32 scrollU = uvScroll * water.uvScrollDir.x;
    f32 scrollV = uvScroll * water.uvScrollDir.y;
    f32 tiling = water.uvTiling;

    // --- Generate vertices ---
    mesh.vertices.reserve(static_cast<usize>(res * res));

    for (i32 j = 0; j < res; ++j) {
        for (i32 i = 0; i < res; ++i) {
            ECS::MeshComponent::Vertex v;

            // Position: local space grid, centered
            f32 x = static_cast<f32>(i) * cellSize - halfSize;
            f32 z = static_cast<f32>(j) * cellSize - halfSize;

            i32 idx = j * res + i;
            f32 height = water.heightField[idx];

            v.position = Math::Vector3(x, height, z);

            // Normal: cross product of neighbors
            f32 hL = (i > 0) ? water.heightField[j * res + (i - 1)] : height;
            f32 hR = (i < res - 1) ? water.heightField[j * res + (i + 1)] : height;
            f32 hD = (j > 0) ? water.heightField[(j - 1) * res + i] : height;
            f32 hU = (j < res - 1) ? water.heightField[(j + 1) * res + i] : height;

            Math::Vector3 tangentX(cellSize * 2.0f, hR - hL, 0.0f);
            Math::Vector3 tangentZ(0.0f, hU - hD, cellSize * 2.0f);
            v.normal = tangentZ.Cross(tangentX).Normalized();

            // UV with animated scroll
            f32 u = (static_cast<f32>(i) / static_cast<f32>(res - 1)) * tiling + scrollU;
            f32 vCoord = (static_cast<f32>(j) / static_cast<f32>(res - 1)) * tiling + scrollV;
            v.uv = Math::Vector2(u, vCoord);

            // Vertex color: blend shallow/deep based on height deviation + foam
            f32 heightDelta = height - water.baseHeight;
            f32 depthT = Math::Clamp(Math::Abs(heightDelta) / water.depthColorThreshold, 0.0f, 1.0f);

            // Base color: lerp from shallow to deep
            Math::Vector3 baseColor = Math::Lerp(water.shallowColor, water.deepColor, depthT);

            // Foam: where wave height deviates significantly
            f32 foamT = 0.0f;
            if (Math::Abs(heightDelta) > water.foamThreshold) {
                foamT = Math::Clamp(
                    (Math::Abs(heightDelta) - water.foamThreshold) / water.foamThreshold,
                    0.0f, 1.0f);
            }

            // Shoreline foam near edges
            if (water.enableShoreline && water.shorelineDistance > 0.0f) {
                f32 edgeDistX = Math::Min(static_cast<f32>(i), static_cast<f32>(res - 1 - i));
                f32 edgeDistZ = Math::Min(static_cast<f32>(j), static_cast<f32>(res - 1 - j));
                f32 edgeDist = Math::Min(edgeDistX, edgeDistZ);
                f32 shoreT = Math::Clamp(1.0f - edgeDist / (water.shorelineDistance / cellSize), 0.0f, 1.0f);
                foamT = Math::Max(foamT, shoreT);
            }

            Math::Vector3 finalColor = Math::Lerp(baseColor, water.foamColor, foamT);
            v.color = Math::Vector4(finalColor.x, finalColor.y, finalColor.z, water.opacity);

            // Tangent (for normal mapping if needed)
            v.tangent = Math::Vector4(1.0f, 0.0f, 0.0f, 1.0f);

            mesh.vertices.push_back(v);
        }
    }

    // --- Generate indices (triangle strip as indexed triangles) ---
    mesh.indices.reserve(static_cast<usize>((res - 1) * (res - 1) * 6));

    for (i32 j = 0; j < res - 1; ++j) {
        for (i32 i = 0; i < res - 1; ++i) {
            u32 topLeft = static_cast<u32>(j * res + i);
            u32 topRight = topLeft + 1;
            u32 bottomLeft = static_cast<u32>((j + 1) * res + i);
            u32 bottomRight = bottomLeft + 1;

            // Triangle 1
            mesh.indices.push_back(topLeft);
            mesh.indices.push_back(bottomLeft);
            mesh.indices.push_back(topRight);

            // Triangle 2
            mesh.indices.push_back(topRight);
            mesh.indices.push_back(bottomLeft);
            mesh.indices.push_back(bottomRight);
        }
    }

    mesh.aabbDirty = true;
    return mesh;
}

} // namespace Effects
} // namespace Enjin
