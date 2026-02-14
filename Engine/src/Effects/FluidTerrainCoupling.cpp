#include "Enjin/Effects/FluidTerrainCoupling.h"
#include "Enjin/ECS/Components/FluidVolume.h"
#include "Enjin/ECS/Components/Terrain.h"
#include "Enjin/ECS/Components/Transform.h"
#include <algorithm>
#include <cmath>

namespace Enjin {
namespace Effects {

void FluidTerrainCoupling::Update(f32 dt, ECS::World* world, FluidSimulation& fluidSim) {
    if (!world || dt <= 0.0f || !m_Config.enabled) return;

    // Find entities that have both FluidVolumeComponent and TerrainComponent
    for (ECS::Entity entity : world->GetEntitiesWithComponent<ECS::FluidVolumeComponent>()) {
        auto* terrain = world->GetComponent<ECS::TerrainComponent>(entity);
        if (!terrain) continue;

        auto* vol = world->GetComponent<ECS::FluidVolumeComponent>(entity);
        if (!vol || !vol->isActive) continue;

        const FluidGridData* gridConst = fluidSim.GetGridData(entity);
        if (!gridConst || gridConst->N == 0) continue;

        // Ensure terrain heightmap is initialized
        if (terrain->heightmap.empty()) {
            terrain->InitializeFlat(0.0f);
        }

        u32 fluidN = gridConst->N;  // Fluid grid interior cells [1..N]
        u32 terrainW = terrain->gridWidth;
        u32 terrainH = terrain->gridHeight;

        if (terrainW == 0 || terrainH == 0) continue;

        bool terrainModified = false;

        // Iterate over fluid grid interior cells (1..N inclusive, excluding boundaries)
        for (u32 fj = 1; fj <= fluidN; ++fj) {
            for (u32 fi = 1; fi <= fluidN; ++fi) {
                u32 idx = gridConst->IX(fi, fj);

                // Map fluid cell (fi, fj) in [1..N] to terrain cell (tx, tz) in [0..terrainW-1, 0..terrainH-1]
                // Fluid interior is [1..N], terrain is [0..W-1]
                f32 fluidU = static_cast<f32>(fi - 1) / static_cast<f32>(fluidN - 1);  // [0..1]
                f32 fluidV = static_cast<f32>(fj - 1) / static_cast<f32>(fluidN - 1);  // [0..1]

                // Clamp to prevent out-of-bounds
                fluidU = std::clamp(fluidU, 0.0f, 1.0f);
                fluidV = std::clamp(fluidV, 0.0f, 1.0f);

                f32 terrainX = fluidU * static_cast<f32>(terrainW - 1);
                f32 terrainZ = fluidV * static_cast<f32>(terrainH - 1);

                u32 tx = static_cast<u32>(std::clamp(terrainX, 0.0f, static_cast<f32>(terrainW - 1)));
                u32 tz = static_cast<u32>(std::clamp(terrainZ, 0.0f, static_cast<f32>(terrainH - 1)));

                f32 density = gridConst->density[idx];

                // Compute velocity magnitude at this cell
                f32 vx = gridConst->velocityX[idx];
                f32 vy = gridConst->velocityY[idx];
                f32 velMag = std::sqrt(vx * vx + vy * vy);

                if (m_Config.accumulateMode) {
                    // Additive mode (lava building up terrain)
                    // Deposit height proportional to density
                    if (density > 0.01f) {
                        f32 deposit = density * m_Config.depositionRate * m_Config.heightScale * dt;
                        deposit = std::min(deposit, m_Config.maxErosionPerFrame * dt);
                        f32 currentH = terrain->GetHeight(tx, tz);
                        f32 newH = std::min(currentH + deposit, terrain->maxHeight);
                        if (newH != currentH) {
                            terrain->SetHeight(tx, tz, newH);
                            terrainModified = true;
                        }
                    }
                } else {
                    // Erosion mode: erode where velocity is high
                    if (velMag > 0.001f) {
                        f32 erosion = velMag * m_Config.erosionRate * (1.0f - m_Config.hardness) * dt;
                        erosion = std::min(erosion, m_Config.maxErosionPerFrame * dt);
                        f32 currentH = terrain->GetHeight(tx, tz);
                        f32 newH = std::max(currentH - erosion, 0.0f);
                        if (newH != currentH) {
                            terrain->SetHeight(tx, tz, newH);
                            terrainModified = true;
                        }
                    }

                    // Deposit sediment where velocity is low (slow-moving water deposits)
                    if (velMag < 0.5f && density > 0.01f) {
                        f32 slowFactor = 1.0f - (velMag / 0.5f);  // 1 when still, 0 at threshold
                        f32 deposit = density * m_Config.depositionRate * slowFactor * dt;
                        deposit = std::min(deposit, m_Config.maxErosionPerFrame * 0.5f * dt);
                        f32 currentH = terrain->GetHeight(tx, tz);
                        f32 newH = std::min(currentH + deposit, terrain->maxHeight);
                        if (newH != currentH) {
                            terrain->SetHeight(tx, tz, newH);
                            terrainModified = true;
                        }
                    }
                }
            }
        }

        // Bidirectional: terrain slope feeds back into fluid velocity
        if (m_Config.bidirectional) {
            FluidGridData* grid = fluidSim.GetMutableGridData(entity);
            if (grid) {
                for (u32 fj = 1; fj <= fluidN; ++fj) {
                    for (u32 fi = 1; fi <= fluidN; ++fi) {
                        u32 idx = grid->IX(fi, fj);

                        // Map fluid cell to terrain-space coordinates
                        f32 fluidU = static_cast<f32>(fi - 1) / static_cast<f32>(fluidN - 1);
                        f32 fluidV = static_cast<f32>(fj - 1) / static_cast<f32>(fluidN - 1);
                        fluidU = std::clamp(fluidU, 0.0f, 1.0f);
                        fluidV = std::clamp(fluidV, 0.0f, 1.0f);

                        f32 terrainX = fluidU * static_cast<f32>(terrainW - 1);
                        f32 terrainZ = fluidV * static_cast<f32>(terrainH - 1);

                        // Compute terrain slope at this position
                        Math::Vector3 slope = ComputeTerrainSlope(*terrain, terrainX, terrainZ);

                        // Only apply if there's fluid density present (don't push empty fluid)
                        f32 density = grid->density[idx];
                        if (density > 0.001f) {
                            // Inject velocity pointing downhill, scaled by slope steepness and density
                            f32 densityFactor = std::min(density, 10.0f) / 10.0f;  // Normalize
                            // Fluid X maps to terrain X slope, Fluid Y maps to terrain Z slope
                            // (2D fluid XY corresponds to world XZ on the terrain plane)
                            grid->velocityX[idx] += slope.x * m_Config.slopeVelocityScale * densityFactor * dt;
                            grid->velocityY[idx] += slope.z * m_Config.slopeVelocityScale * densityFactor * dt;
                        }
                    }
                }
            }
        }

        if (terrainModified) {
            terrain->meshDirty = true;
        }
    }
}

f32 FluidTerrainCoupling::SampleTerrainHeight(const ECS::TerrainComponent& terrain, f32 tx, f32 tz) const {
    // Bilinear interpolation for sub-cell accuracy
    f32 maxX = static_cast<f32>(terrain.gridWidth - 1);
    f32 maxZ = static_cast<f32>(terrain.gridHeight - 1);
    tx = std::clamp(tx, 0.0f, maxX);
    tz = std::clamp(tz, 0.0f, maxZ);

    u32 x0 = static_cast<u32>(tx);
    u32 z0 = static_cast<u32>(tz);
    u32 x1 = std::min(x0 + 1, terrain.gridWidth - 1);
    u32 z1 = std::min(z0 + 1, terrain.gridHeight - 1);

    f32 fx = tx - static_cast<f32>(x0);
    f32 fz = tz - static_cast<f32>(z0);

    f32 h00 = terrain.GetHeight(x0, z0);
    f32 h10 = terrain.GetHeight(x1, z0);
    f32 h01 = terrain.GetHeight(x0, z1);
    f32 h11 = terrain.GetHeight(x1, z1);

    // Bilinear interpolation
    f32 h0 = h00 + fx * (h10 - h00);
    f32 h1 = h01 + fx * (h11 - h01);
    return h0 + fz * (h1 - h0);
}

Math::Vector3 FluidTerrainCoupling::ComputeTerrainSlope(const ECS::TerrainComponent& terrain, f32 tx, f32 tz) const {
    // Compute gradient using central differences
    // Returns a vector pointing downhill (negative gradient of height)
    f32 step = 1.0f;  // One terrain cell step

    f32 hCenter = SampleTerrainHeight(terrain, tx, tz);
    f32 hRight  = SampleTerrainHeight(terrain, tx + step, tz);
    f32 hLeft   = SampleTerrainHeight(terrain, tx - step, tz);
    f32 hFwd    = SampleTerrainHeight(terrain, tx, tz + step);
    f32 hBack   = SampleTerrainHeight(terrain, tx, tz - step);

    // Gradient (dh/dx, 0, dh/dz) — positive means uphill
    f32 dhdx = (hRight - hLeft) / (2.0f * step * terrain.cellSize);
    f32 dhdz = (hFwd - hBack) / (2.0f * step * terrain.cellSize);

    // Return negative gradient (pointing downhill)
    return Math::Vector3(-dhdx, 0.0f, -dhdz);
}

} // namespace Effects
} // namespace Enjin
