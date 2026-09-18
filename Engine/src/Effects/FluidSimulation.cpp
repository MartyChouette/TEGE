#include "Enjin/Effects/FluidSimulation.h"
#include "Enjin/Effects/FluidObstacles.h"
#include "Enjin/ECS/Components/FluidVolume.h"
#include "Enjin/ECS/Components/Transform.h"
#include <algorithm>
#include <cstring>
#include <chrono>
#include <cmath>
#include <vector>

namespace Enjin {
namespace Effects {

namespace {
// FNV-1a over a float's bits. Same mixer FluidObstacleFingerprint uses, so a
// volume's key and the world's fingerprint combine without a second scheme.
u64 MixF32(u64 h, f32 v) {
    u32 bits = 0;
    std::memcpy(&bits, &v, sizeof(bits));
    return (h ^ static_cast<u64>(bits)) * 1099511628211ull;
}

// A sphere is culled when it lies entirely behind any one plane. Planes point
// INWARD, so "behind" is a signed distance below -radius.
bool SphereOutsideFrustum(const Math::Vector4 planes[6],
                          const Math::Vector3& centre, f32 radius) {
    for (int i = 0; i < 6; ++i) {
        const f32 d = planes[i].x * centre.x + planes[i].y * centre.y
                    + planes[i].z * centre.z + planes[i].w;
        if (d < -radius) return true;
    }
    return false;
}

} // namespace

namespace {

// Apply the wall rule to INTERIOR solid cells, the same way SetBoundary
// applies it to the grid's own shell.
//
// Putting it here rather than in each solve stage is deliberate: SetBoundary
// is already called after every relaxation sweep, every advection and every
// projection stage, so obstacles get enforced everywhere they need to be by
// changing one function instead of five, and a new stage cannot forget.
//
//   b != 0 is a velocity component -> zero inside a solid. No-slip, and it is
//   what stops the projection pushing flow into the wall.
//   b == 0 is a scalar (pressure, divergence) -> copy the average of the fluid
//   neighbours, which is a zero-gradient Neumann condition across the face and
//   therefore no flux through it. A solid with no fluid neighbour at all is
//   interior to a thick wall and never read, so 0 is fine there.
//
// Density is NOT handled here even though it is also a b == 0 scalar: averaging
// fluid neighbours into a wall cell would make the wall glow with smoke. It is
// cleared separately after advection.
template <typename IndexFn>
void EnforceSolidCells(FluidGridData& grid, i32 b, std::vector<f32>& x,
                       u32 N, bool is3D, IndexFn idx) {
    if (!grid.HasObstacles()) return;

    const u32 kHi = is3D ? N : 1;
    for (u32 k = 1; k <= kHi; ++k) {
        for (u32 j = 1; j <= N; ++j) {
            for (u32 i = 1; i <= N; ++i) {
                const usize c = idx(i, j, k);
                if (!grid.IsSolid(c)) continue;

                if (b != 0) { x[c] = 0.0f; continue; }

                f32 sum = 0.0f;
                i32 n = 0;
                auto take = [&](usize nb) {
                    if (!grid.IsSolid(nb)) { sum += x[nb]; ++n; }
                };
                take(idx(i - 1, j, k));
                take(idx(i + 1, j, k));
                take(idx(i, j - 1, k));
                take(idx(i, j + 1, k));
                if (is3D) {
                    take(idx(i, j, k - 1));
                    take(idx(i, j, k + 1));
                }
                x[c] = n ? sum / static_cast<f32>(n) : 0.0f;
            }
        }
    }
}

// Smoke that advected into a wall is deleted rather than averaged away, so a
// wall never lights up with the density of what hit it.
void ClearSolidDensity(FluidGridData& grid) {
    if (!grid.HasObstacles()) return;
    for (usize c = 0; c < grid.density.size(); ++c) {
        if (grid.solid[c]) grid.density[c] = 0.0f;
    }
}

} // namespace


void FluidSimulation::Update(f32 dt, ECS::World* world) {
    if (!world || dt <= 0.0f) return;
    if (dt > 0.5f) return;  // Skip on long frames (loading/pause)
    dt = std::min(dt, 1.0f / 30.0f);  // Clamp for stability

    // Release grids whose volume is gone. OnEntityRemoved does exactly this and
    // has never had a caller, so deleting a chimney left its grid allocated for
    // the life of the process -- about 5 MB for a 48^3 volume, leaked again
    // every time a level streamed that chunk back in. Pruning here rather than
    // adding a destruction hook keeps it self-healing: any way a volume can
    // disappear is covered, including World::Clear on a scene change.
    for (auto it = m_Grids.begin(); it != m_Grids.end(); ) {
        const ECS::Entity e = it->first;
        if (!world->IsValid(e) || !world->HasComponent<ECS::FluidVolumeComponent>(e)) {
            m_PendingTime.erase(e);
            it = m_Grids.erase(it);
        } else {
            ++it;
        }
    }

    // Volumes take turns inside a time budget. See SetFrameBudgetMs: every
    // volume used to solve in full every frame, and three campfires was 4 fps.
    const auto& all = world->GetEntitiesWithComponent<ECS::FluidVolumeComponent>();
    std::vector<ECS::Entity> order(all.begin(), all.end());
    m_DeferredVolumes = 0;
    m_CulledVolumes = 0;
    if (order.empty()) return;
    if (m_RoundRobinStart >= order.size()) m_RoundRobinStart = 0;
    std::rotate(order.begin(), order.begin() + static_cast<long long>(m_RoundRobinStart),
                order.end());

    // The collider fingerprint is per WORLD, so it is gathered at most once per
    // dimension per frame rather than once per volume. Lazily, because a scene
    // with no fluid in view should not pay for a scan at all.
    u64 worldKey[2] = {0, 0};
    bool worldKeyValid[2] = {false, false};

    const auto frameStart = std::chrono::high_resolution_clock::now();
    auto elapsedMs = [&]() {
        return std::chrono::duration<f64, std::milli>(
            std::chrono::high_resolution_clock::now() - frameStart).count();
    };

    usize index = 0;
    for (ECS::Entity entity : order) {
        ++index;
        // Baking records one volume; solving the other nine costs the same
        // wait for output nobody asked for.
        if (m_SoloEntity != ECS::INVALID_ENTITY && entity != m_SoloEntity) continue;

        auto* vol = world->GetComponent<ECS::FluidVolumeComponent>(entity);
        if (!vol || !vol->isActive) continue;

        // A played-back volume is not simulated. Stepping it would overwrite
        // the recorded frame with a frame of real simulation before anything
        // could draw it, so the recording would never be seen.
        {
            auto it = m_Grids.find(entity);
            if (it != m_Grids.end() && it->second.playbackDriven) continue;
        }

        // Nothing the camera can see is ever culled; see FluidViewer for why
        // this stays off until a runtime opts in. The sphere is the volume's
        // own box, so a plume that rises past the top of its grid is not a
        // case this can get wrong -- the grid IS the extent.
        if (m_HasViewer) {
            auto* cullXf = world->GetComponent<ECS::TransformComponent>(entity);
            const Math::Vector3 c = cullXf ? cullXf->position
                                           : Math::Vector3(0.0f, 0.0f, 0.0f);
            const Math::Vector3& h = vol->halfExtents;
            const f32 radius = std::sqrt(h.x * h.x + h.y * h.y + h.z * h.z);

            bool culled = false;
            if (m_Viewer.cullDistance > 0.0f) {
                const f32 dx = c.x - m_Viewer.position.x;
                const f32 dy = c.y - m_Viewer.position.y;
                const f32 dz = c.z - m_Viewer.position.z;
                const f32 dist = std::sqrt(dx * dx + dy * dy + dz * dz) - radius;
                if (dist > m_Viewer.cullDistance) culled = true;
            }
            if (!culled && m_Viewer.hasFrustum) {
                culled = SphereOutsideFrustum(m_Viewer.frustumPlanes, c, radius);
            }
            if (culled) {
                ++m_CulledVolumes;
                // Drop what it is owed rather than banking it: a volume that
                // was off screen for a minute must not fast-forward through
                // that minute the moment it comes back into view.
                m_PendingTime[entity] = 0.0f;
                continue;
            }
        }

        // Everything this volume is owed, including frames it sat out, so it
        // moves at the right speed rather than in slow motion.
        f32& owed = m_PendingTime[entity];
        owed += dt;

        // The FIRST volume always runs. A budget that can starve everything
        // would freeze all the smoke on a slow frame, which reads as broken
        // rather than as degraded.
        if (index > 1 && elapsedMs() >= m_FrameBudgetMs) {
            ++m_DeferredVolumes;
            // Resume here next frame so the same volumes are not always last.
            m_RoundRobinStart = (m_RoundRobinStart + index - 1) % order.size();
            continue;
        }

        // Clamped for stability exactly as a single frame's dt is: a volume
        // that sat out ten frames must not take one enormous step.
        f32 stepDt = std::min(owed, 1.0f / 30.0f);
        owed = 0.0f;
        const f32 dtSaved = dt;
        dt = stepDt;

        // Initialize grid if needed
        auto& grid = m_Grids[entity];
        bool is3D = vol->dimension == ECS::FluidDimension::Mode3D;
        u32 clampedSize = vol->gridSize;
        if (is3D) clampedSize = std::min(clampedSize, 48u);
        else clampedSize = std::min(clampedSize, 128u);
        clampedSize = std::max(clampedSize, 8u);

        // Hysteresis: only reallocate if dimension mode changed or size differs by >10%
        bool needRealloc = (grid.is3D != is3D) || (grid.N == 0);
        if (!needRealloc && grid.N != clampedSize) {
            f32 ratio = static_cast<f32>(clampedSize) / static_cast<f32>(grid.N);
            needRealloc = (ratio > 1.1f || ratio < 0.9f);
        }
        if (needRealloc) {
            grid.Allocate(clampedSize, is3D);
            vol->simulationInitialized = true;
        }

        // Obstacles, in the LIVE path and not only inside a bake.
        //
        // BuildFluidObstacleMask had exactly one caller -- BakeFluid -- so a
        // running volume knew about the six walls of its own grid and nothing
        // else. Smoke went straight through every crate, in the editor and in
        // a shipped game alike, while a BAKE of the same scene flowed around
        // them correctly: the same feature, present or absent depending on
        // which of the two you happened to look at.
        {
            auto* xf = world->GetComponent<ECS::TransformComponent>(entity);
            const Math::Vector3 centre = xf ? xf->position
                                            : Math::Vector3(0.0f, 0.0f, 0.0f);
            const int dim = is3D ? 1 : 0;
            if (!worldKeyValid[dim]) {
                worldKey[dim] = FluidObstacleFingerprint(world, is3D);
                worldKeyValid[dim] = true;
            }
            u64 key = worldKey[dim];
            key = MixF32(key, centre.x);
            key = MixF32(key, centre.y);
            key = MixF32(key, centre.z);
            key = MixF32(key, vol->halfExtents.x);
            key = MixF32(key, vol->halfExtents.y);
            key = MixF32(key, vol->halfExtents.z);
            // 0 is the "never built" value Allocate resets to, so a key that
            // hashes to it simply rebuilds once more. Cheaper than a flag.
            if (key != grid.obstacleKey) {
                std::vector<u8> mask;
                BuildFluidObstacleMask(world, centre, vol->halfExtents,
                                       grid.N, is3D, mask);
                // An all-zero mask is kept EMPTY rather than stored: HasObstacles
                // is what skips the per-cell solid test, and a scene with no
                // colliders should not pay for one.
                const bool any = std::any_of(mask.begin(), mask.end(),
                                             [](u8 v) { return v != 0; });
                grid.solid = any ? std::move(mask) : std::vector<u8>();
                grid.obstacleKey = key;
            }
        }

        // Emit from the volume's source each frame. The sourceDensity / sourceRadius /
        // sourceVelocityScale params existed but nothing ever injected density, so every
        // fluid volume simulated an empty grid and rendered nothing. Emit a plume from
        // the bottom-centre so buoyant types (smoke/steam/gas) billow up and dense types
        // (water/lava) pool and spread.
        if (vol->sourceDensity > 0.0f) {
            f32 rate = vol->sourceDensity * dt;
            f32 r = std::max(1.0f, vol->sourceRadius);
            f32 up = vol->sourceVelocityScale;
            if (is3D) {
                Math::Vector3 c(clampedSize * 0.5f, clampedSize * 0.15f, clampedSize * 0.5f);
                AddDensityAtWorldPos(entity, c, rate, r);
                AddVelocityAtWorldPos(entity, c, Math::Vector3(0.0f, up, 0.0f), r);
            } else {
                Math::Vector3 c(clampedSize * 0.5f, clampedSize * 0.15f, 0.0f);
                AddDensityAtWorldPos(entity, c, rate, r);
                AddVelocityAtWorldPos(entity, c, Math::Vector3(0.0f, up, 0.0f), r);
            }
        }

        i32 iterations = vol->solverIterations;
        if (is3D && clampedSize > 32) iterations = std::min(iterations, 10);

        if (is3D) {
            Step3D(grid, dt, vol->viscosity, vol->diffusion, vol->dissipation,
                   vol->velocityDissipation, iterations, vol->buoyancy);
        } else {
            Step2D(grid, dt, vol->viscosity, vol->diffusion, vol->dissipation,
                   vol->velocityDissipation, iterations, vol->buoyancy);
        }

        dt = dtSaved;
    }
}

// One step of one grid, with no world and no member state. See FluidStepParams.
void FluidSimulation::StepGrid(FluidGridData& grid, const FluidStepParams& p, f32 dt) {
    if (grid.N == 0 || dt <= 0.0f) return;

    // The same emission Update performs, spelled out here rather than shared
    // through AddDensityAtWorldPos: that one is keyed by entity, and an
    // isolated step has no entity.
    if (p.sourceDensity > 0.0f) {
        const f32 rate = p.sourceDensity * dt;
        const f32 r = std::max(1.0f, p.sourceRadius);
        const i32 ir = static_cast<i32>(r);
        const i32 N = static_cast<i32>(grid.N);
        const i32 cx = static_cast<i32>(static_cast<f32>(grid.N) * 0.5f);
        const i32 cy = static_cast<i32>(static_cast<f32>(grid.N) * 0.15f);
        const i32 cz = grid.is3D ? cx : 0;

        for (i32 k = grid.is3D ? std::max(1, cz - ir) : 0;
             k <= (grid.is3D ? std::min(N, cz + ir) : 0); ++k) {
            for (i32 j = std::max(1, cy - ir); j <= std::min(N, cy + ir); ++j) {
                for (i32 i = std::max(1, cx - ir); i <= std::min(N, cx + ir); ++i) {
                    const f32 dx = static_cast<f32>(i - cx);
                    const f32 dy = static_cast<f32>(j - cy);
                    const f32 dz = grid.is3D ? static_cast<f32>(k - cz) : 0.0f;
                    const f32 dist = std::sqrt(dx * dx + dy * dy + dz * dz);
                    if (dist > r) continue;
                    const f32 falloff = 1.0f - dist / r;
                    const usize idx = grid.is3D ? grid.IX3(static_cast<u32>(i), static_cast<u32>(j),
                                                           static_cast<u32>(k))
                                                : grid.IX(static_cast<u32>(i), static_cast<u32>(j));
                    grid.density[idx] += rate * falloff;
                    grid.velocityY[idx] += p.sourceVelocityScale * falloff;
                }
            }
        }
    }

    if (grid.is3D) {
        Step3D(grid, dt, p.viscosity, p.diffusion, p.dissipation,
               p.velocityDissipation, p.iterations, p.buoyancy);
    } else {
        Step2D(grid, dt, p.viscosity, p.diffusion, p.dissipation,
               p.velocityDissipation, p.iterations, p.buoyancy);
    }
}

void FluidSimulation::AddDensityAtWorldPos(ECS::Entity entity, const Math::Vector3& worldPos, f32 amount, f32 radius) {
    auto it = m_Grids.find(entity);
    if (it == m_Grids.end()) return;
    auto& grid = it->second;
    u32 N = grid.N;

    // World pos needs to be converted to grid coords by the caller or we do a simple center-based mapping
    // For now, treat worldPos as grid-space coordinates normalized to [0,N]
    if (!grid.is3D) {
        i32 cx = static_cast<i32>(worldPos.x);
        i32 cy = static_cast<i32>(worldPos.y);
        i32 r = static_cast<i32>(radius);
        for (i32 j = std::max(1, cy - r); j <= std::min(static_cast<i32>(N), cy + r); ++j) {
            for (i32 i = std::max(1, cx - r); i <= std::min(static_cast<i32>(N), cx + r); ++i) {
                f32 dx = static_cast<f32>(i - cx);
                f32 dy = static_cast<f32>(j - cy);
                f32 dist = std::sqrt(dx * dx + dy * dy);
                if (dist <= radius) {
                    f32 falloff = 1.0f - dist / radius;
                    grid.density[grid.IX(i, j)] += amount * falloff;
                }
            }
        }
    } else {
        i32 cx = static_cast<i32>(worldPos.x);
        i32 cy = static_cast<i32>(worldPos.y);
        i32 cz = static_cast<i32>(worldPos.z);
        i32 r = static_cast<i32>(radius);
        for (i32 k = std::max(1, cz - r); k <= std::min(static_cast<i32>(N), cz + r); ++k) {
            for (i32 j = std::max(1, cy - r); j <= std::min(static_cast<i32>(N), cy + r); ++j) {
                for (i32 i = std::max(1, cx - r); i <= std::min(static_cast<i32>(N), cx + r); ++i) {
                    f32 dx = static_cast<f32>(i - cx);
                    f32 dy = static_cast<f32>(j - cy);
                    f32 dz = static_cast<f32>(k - cz);
                    f32 dist = std::sqrt(dx * dx + dy * dy + dz * dz);
                    if (dist <= radius) {
                        f32 falloff = 1.0f - dist / radius;
                        grid.density[grid.IX3(i, j, k)] += amount * falloff;
                    }
                }
            }
        }
    }
}

void FluidSimulation::AddVelocityAtWorldPos(ECS::Entity entity, const Math::Vector3& worldPos, const Math::Vector3& velocity, f32 radius) {
    auto it = m_Grids.find(entity);
    if (it == m_Grids.end()) return;
    auto& grid = it->second;
    u32 N = grid.N;

    if (!grid.is3D) {
        i32 cx = static_cast<i32>(worldPos.x);
        i32 cy = static_cast<i32>(worldPos.y);
        i32 r = static_cast<i32>(radius);
        for (i32 j = std::max(1, cy - r); j <= std::min(static_cast<i32>(N), cy + r); ++j) {
            for (i32 i = std::max(1, cx - r); i <= std::min(static_cast<i32>(N), cx + r); ++i) {
                f32 dx = static_cast<f32>(i - cx);
                f32 dy = static_cast<f32>(j - cy);
                f32 dist = std::sqrt(dx * dx + dy * dy);
                if (dist <= radius) {
                    f32 falloff = 1.0f - dist / radius;
                    grid.velocityX[grid.IX(i, j)] += velocity.x * falloff;
                    grid.velocityY[grid.IX(i, j)] += velocity.y * falloff;
                }
            }
        }
    } else {
        i32 cx = static_cast<i32>(worldPos.x);
        i32 cy = static_cast<i32>(worldPos.y);
        i32 cz = static_cast<i32>(worldPos.z);
        i32 r = static_cast<i32>(radius);
        for (i32 k = std::max(1, cz - r); k <= std::min(static_cast<i32>(N), cz + r); ++k) {
            for (i32 j = std::max(1, cy - r); j <= std::min(static_cast<i32>(N), cy + r); ++j) {
                for (i32 i = std::max(1, cx - r); i <= std::min(static_cast<i32>(N), cx + r); ++i) {
                    f32 dx = static_cast<f32>(i - cx);
                    f32 dy = static_cast<f32>(j - cy);
                    f32 dz = static_cast<f32>(k - cz);
                    f32 dist = std::sqrt(dx * dx + dy * dy + dz * dz);
                    if (dist <= radius) {
                        f32 falloff = 1.0f - dist / radius;
                        grid.velocityX[grid.IX3(i, j, k)] += velocity.x * falloff;
                        grid.velocityY[grid.IX3(i, j, k)] += velocity.y * falloff;
                        grid.velocityZ[grid.IX3(i, j, k)] += velocity.z * falloff;
                    }
                }
            }
        }
    }
}

void FluidSimulation::Reset(ECS::Entity entity) {
    auto it = m_Grids.find(entity);
    if (it != m_Grids.end()) {
        it->second.Clear();
    }
}

void FluidSimulation::SetObstacleMask(ECS::Entity entity, std::vector<u8> mask) {
    auto it = m_Grids.find(entity);
    if (it == m_Grids.end()) return;   // no grid yet; the volume has not stepped

    // A mask sized for a different resolution would index cells that do not
    // exist. Refusing is better than clamping: a silently half-applied set of
    // obstacles looks like the solver ignoring geometry.
    if (mask.size() != it->second.density.size()) return;
    it->second.solid = std::move(mask);
}

bool FluidSimulation::SetPlaybackDensity(ECS::Entity entity, u32 gridSize, bool is3D,
                                         const std::vector<f32>& density) {
    if (gridSize == 0) return false;
    const usize s = static_cast<usize>(gridSize) + 2;
    const usize expected = is3D ? s * s * s : s * s;
    if (density.size() != expected) return false;

    auto& grid = m_Grids[entity];
    if (grid.N != gridSize || grid.is3D != is3D) grid.Allocate(gridSize, is3D);
    grid.playbackDriven = true;
    grid.density = density;
    return true;
}

void FluidSimulation::ClearObstacleMask(ECS::Entity entity) {
    auto it = m_Grids.find(entity);
    if (it != m_Grids.end()) it->second.solid.clear();
}

void FluidSimulation::OnEntityRemoved(ECS::Entity entity) {
    m_Grids.erase(entity);
}

const FluidGridData* FluidSimulation::GetGridData(ECS::Entity entity) const {
    auto it = m_Grids.find(entity);
    return it != m_Grids.end() ? &it->second : nullptr;
}

FluidGridData* FluidSimulation::GetMutableGridData(ECS::Entity entity) {
    auto it = m_Grids.find(entity);
    return it != m_Grids.end() ? &it->second : nullptr;
}

// ============================================================================
// 2D Stable Fluids
// ============================================================================

void FluidSimulation::Step2D(FluidGridData& grid, f32 dt, f32 visc, f32 diff,
                              f32 dissipation, f32 velDissipation, i32 iterations, f32 buoyancy) {
    u32 N = grid.N;
    u32 size = (N + 2) * (N + 2);

    // Apply velocity dissipation to previous buffers
    for (u32 i = 0; i < size; ++i) {
        grid.velocityXPrev[i] = grid.velocityX[i] * velDissipation;
        grid.velocityYPrev[i] = grid.velocityY[i] * velDissipation;
        grid.densityPrev[i] = grid.density[i] * dissipation;
    }

    // Velocity step
    Diffuse2D(grid, 1, grid.velocityX, grid.velocityXPrev, visc, dt, iterations);
    Diffuse2D(grid, 2, grid.velocityY, grid.velocityYPrev, visc, dt, iterations);
    Project2D(grid, grid.velocityX, grid.velocityY, grid.pressure, grid.divergence, iterations);

    std::swap(grid.velocityXPrev, grid.velocityX);
    std::swap(grid.velocityYPrev, grid.velocityY);

    Advect2D(grid, 1, grid.velocityX, grid.velocityXPrev, grid.velocityXPrev, grid.velocityYPrev, dt);
    Advect2D(grid, 2, grid.velocityY, grid.velocityYPrev, grid.velocityXPrev, grid.velocityYPrev, dt);
    Project2D(grid, grid.velocityX, grid.velocityY, grid.pressure, grid.divergence, iterations);

    // Density step
    Diffuse2D(grid, 0, grid.density, grid.densityPrev, diff, dt, iterations);
    std::swap(grid.densityPrev, grid.density);
    Advect2D(grid, 0, grid.density, grid.densityPrev, grid.velocityX, grid.velocityY, dt);
    ClearSolidDensity(grid);

    // Density-proportional vertical force. POSITIVE rises (smoke, steam, fire),
    // NEGATIVE sinks (water, lava) -- and negative used to be discarded by a
    // `> 0.0f` gate, so no fluid in the engine could ever fall. That is what
    // stood between this solver and a liquid that pools: obstacles contain it,
    // the projection keeps it incompressible, and gravity is what makes it
    // settle in the container rather than hang in the air.
    if (buoyancy != 0.0f) {
        for (u32 j = 1; j <= N; ++j) {
            for (u32 i = 1; i <= N; ++i) {
                grid.velocityY[grid.IX(i, j)] += buoyancy * grid.density[grid.IX(i, j)] * dt;
            }
        }
    }
}

void FluidSimulation::Diffuse2D(FluidGridData& grid, i32 b, std::vector<f32>& x,
                                  const std::vector<f32>& x0, f32 diff, f32 dt, i32 iterations) {
    u32 N = grid.N;
    f32 a = dt * diff * N * N;
    f32 divisor = 1.0f + 4.0f * a;

    for (i32 iter = 0; iter < iterations; ++iter) {
        for (u32 j = 1; j <= N; ++j) {
            for (u32 i = 1; i <= N; ++i) {
                x[grid.IX(i, j)] = (x0[grid.IX(i, j)] +
                    a * (x[grid.IX(i - 1, j)] + x[grid.IX(i + 1, j)] +
                         x[grid.IX(i, j - 1)] + x[grid.IX(i, j + 1)])) / divisor;
            }
        }
        SetBoundary2D(grid, b, x);
    }
}

void FluidSimulation::Advect2D(FluidGridData& grid, i32 b, std::vector<f32>& d,
                                 const std::vector<f32>& d0, const std::vector<f32>& u,
                                 const std::vector<f32>& v, f32 dt) {
    u32 N = grid.N;
    f32 dt0 = dt * N;

    for (u32 j = 1; j <= N; ++j) {
        for (u32 i = 1; i <= N; ++i) {
            // Backtrace
            f32 x = static_cast<f32>(i) - dt0 * u[grid.IX(i, j)];
            f32 y = static_cast<f32>(j) - dt0 * v[grid.IX(i, j)];

            // Clamp to grid
            x = std::max(0.5f, std::min(static_cast<f32>(N) + 0.5f, x));
            y = std::max(0.5f, std::min(static_cast<f32>(N) + 0.5f, y));

            u32 i0 = static_cast<u32>(x);
            u32 i1 = i0 + 1;
            u32 j0 = static_cast<u32>(y);
            u32 j1 = j0 + 1;

            f32 s1 = x - static_cast<f32>(i0);
            f32 s0 = 1.0f - s1;
            f32 t1 = y - static_cast<f32>(j0);
            f32 t0 = 1.0f - t1;

            d[grid.IX(i, j)] = s0 * (t0 * d0[grid.IX(i0, j0)] + t1 * d0[grid.IX(i0, j1)]) +
                               s1 * (t0 * d0[grid.IX(i1, j0)] + t1 * d0[grid.IX(i1, j1)]);
        }
    }
    SetBoundary2D(grid, b, d);
}

void FluidSimulation::Project2D(FluidGridData& grid, std::vector<f32>& u, std::vector<f32>& v,
                                  std::vector<f32>& p, std::vector<f32>& div, i32 iterations) {
    u32 N = grid.N;
    f32 h = 1.0f / N;

    // Compute divergence
    for (u32 j = 1; j <= N; ++j) {
        for (u32 i = 1; i <= N; ++i) {
            div[grid.IX(i, j)] = -0.5f * h * (u[grid.IX(i + 1, j)] - u[grid.IX(i - 1, j)] +
                                                v[grid.IX(i, j + 1)] - v[grid.IX(i, j - 1)]);
            p[grid.IX(i, j)] = 0.0f;
        }
    }
    SetBoundary2D(grid, 0, div);
    SetBoundary2D(grid, 0, p);

    // Solve pressure (Gauss-Seidel)
    for (i32 iter = 0; iter < iterations; ++iter) {
        for (u32 j = 1; j <= N; ++j) {
            for (u32 i = 1; i <= N; ++i) {
                p[grid.IX(i, j)] = (div[grid.IX(i, j)] +
                    p[grid.IX(i - 1, j)] + p[grid.IX(i + 1, j)] +
                    p[grid.IX(i, j - 1)] + p[grid.IX(i, j + 1)]) / 4.0f;
            }
        }
        SetBoundary2D(grid, 0, p);
    }

    // Subtract pressure gradient
    for (u32 j = 1; j <= N; ++j) {
        for (u32 i = 1; i <= N; ++i) {
            u[grid.IX(i, j)] -= 0.5f * N * (p[grid.IX(i + 1, j)] - p[grid.IX(i - 1, j)]);
            v[grid.IX(i, j)] -= 0.5f * N * (p[grid.IX(i, j + 1)] - p[grid.IX(i, j - 1)]);
        }
    }
    SetBoundary2D(grid, 1, u);
    SetBoundary2D(grid, 2, v);
}

void FluidSimulation::SetBoundary2D(FluidGridData& grid, i32 b, std::vector<f32>& x) {
    u32 N = grid.N;
    for (u32 i = 1; i <= N; ++i) {
        x[grid.IX(0, i)]     = b == 1 ? -x[grid.IX(1, i)] : x[grid.IX(1, i)];
        x[grid.IX(N + 1, i)] = b == 1 ? -x[grid.IX(N, i)] : x[grid.IX(N, i)];
        x[grid.IX(i, 0)]     = b == 2 ? -x[grid.IX(i, 1)] : x[grid.IX(i, 1)];
        x[grid.IX(i, N + 1)] = b == 2 ? -x[grid.IX(i, N)] : x[grid.IX(i, N)];
    }
    x[grid.IX(0, 0)]         = 0.5f * (x[grid.IX(1, 0)] + x[grid.IX(0, 1)]);
    x[grid.IX(0, N + 1)]     = 0.5f * (x[grid.IX(1, N + 1)] + x[grid.IX(0, N)]);
    x[grid.IX(N + 1, 0)]     = 0.5f * (x[grid.IX(N, 0)] + x[grid.IX(N + 1, 1)]);
    x[grid.IX(N + 1, N + 1)] = 0.5f * (x[grid.IX(N, N + 1)] + x[grid.IX(N + 1, N)]);

    EnforceSolidCells(grid, b, x, N, false,
                      [&](u32 i, u32 j, u32) { return static_cast<usize>(grid.IX(i, j)); });
}

// ============================================================================
// 3D Stable Fluids
// ============================================================================

void FluidSimulation::Step3D(FluidGridData& grid, f32 dt, f32 visc, f32 diff,
                              f32 dissipation, f32 velDissipation, i32 iterations, f32 buoyancy) {
    u32 N = grid.N;
    u32 size = (N + 2) * (N + 2) * (N + 2);

    // Apply dissipation
    for (u32 i = 0; i < size; ++i) {
        grid.velocityXPrev[i] = grid.velocityX[i] * velDissipation;
        grid.velocityYPrev[i] = grid.velocityY[i] * velDissipation;
        grid.velocityZPrev[i] = grid.velocityZ[i] * velDissipation;
        grid.densityPrev[i] = grid.density[i] * dissipation;
    }

    // Velocity step
    Diffuse3D(grid, 1, grid.velocityX, grid.velocityXPrev, visc, dt, iterations);
    Diffuse3D(grid, 2, grid.velocityY, grid.velocityYPrev, visc, dt, iterations);
    Diffuse3D(grid, 3, grid.velocityZ, grid.velocityZPrev, visc, dt, iterations);
    Project3D(grid, grid.velocityX, grid.velocityY, grid.velocityZ, grid.pressure, grid.divergence, iterations);

    std::swap(grid.velocityXPrev, grid.velocityX);
    std::swap(grid.velocityYPrev, grid.velocityY);
    std::swap(grid.velocityZPrev, grid.velocityZ);

    Advect3D(grid, 1, grid.velocityX, grid.velocityXPrev, grid.velocityXPrev, grid.velocityYPrev, grid.velocityZPrev, dt);
    Advect3D(grid, 2, grid.velocityY, grid.velocityYPrev, grid.velocityXPrev, grid.velocityYPrev, grid.velocityZPrev, dt);
    Advect3D(grid, 3, grid.velocityZ, grid.velocityZPrev, grid.velocityXPrev, grid.velocityYPrev, grid.velocityZPrev, dt);
    Project3D(grid, grid.velocityX, grid.velocityY, grid.velocityZ, grid.pressure, grid.divergence, iterations);

    // Density step
    Diffuse3D(grid, 0, grid.density, grid.densityPrev, diff, dt, iterations);
    std::swap(grid.densityPrev, grid.density);
    Advect3D(grid, 0, grid.density, grid.densityPrev, grid.velocityX, grid.velocityY, grid.velocityZ, dt);
    ClearSolidDensity(grid);

    // Y-up, and see the 2D note: negative sinks, and used to be discarded.
    if (buoyancy != 0.0f) {
        for (u32 k = 1; k <= N; ++k) {
            for (u32 j = 1; j <= N; ++j) {
                for (u32 i = 1; i <= N; ++i) {
                    grid.velocityY[grid.IX3(i, j, k)] += buoyancy * grid.density[grid.IX3(i, j, k)] * dt;
                }
            }
        }
    }
}

void FluidSimulation::Diffuse3D(FluidGridData& grid, i32 b, std::vector<f32>& x,
                                  const std::vector<f32>& x0, f32 diff, f32 dt, i32 iterations) {
    u32 N = grid.N;
    f32 a = dt * diff * N * N;
    f32 divisor = 1.0f + 6.0f * a;

    for (i32 iter = 0; iter < iterations; ++iter) {
        for (u32 k = 1; k <= N; ++k) {
            for (u32 j = 1; j <= N; ++j) {
                for (u32 i = 1; i <= N; ++i) {
                    x[grid.IX3(i, j, k)] = (x0[grid.IX3(i, j, k)] +
                        a * (x[grid.IX3(i - 1, j, k)] + x[grid.IX3(i + 1, j, k)] +
                             x[grid.IX3(i, j - 1, k)] + x[grid.IX3(i, j + 1, k)] +
                             x[grid.IX3(i, j, k - 1)] + x[grid.IX3(i, j, k + 1)])) / divisor;
                }
            }
        }
        SetBoundary3D(grid, b, x);
    }
}

void FluidSimulation::Advect3D(FluidGridData& grid, i32 b, std::vector<f32>& d,
                                 const std::vector<f32>& d0, const std::vector<f32>& u,
                                 const std::vector<f32>& v, const std::vector<f32>& w, f32 dt) {
    u32 N = grid.N;
    f32 dt0 = dt * N;

    for (u32 k = 1; k <= N; ++k) {
        for (u32 j = 1; j <= N; ++j) {
            for (u32 i = 1; i <= N; ++i) {
                f32 x = static_cast<f32>(i) - dt0 * u[grid.IX3(i, j, k)];
                f32 y = static_cast<f32>(j) - dt0 * v[grid.IX3(i, j, k)];
                f32 z = static_cast<f32>(k) - dt0 * w[grid.IX3(i, j, k)];

                x = std::max(0.5f, std::min(static_cast<f32>(N) + 0.5f, x));
                y = std::max(0.5f, std::min(static_cast<f32>(N) + 0.5f, y));
                z = std::max(0.5f, std::min(static_cast<f32>(N) + 0.5f, z));

                u32 i0 = static_cast<u32>(x); u32 i1 = i0 + 1;
                u32 j0 = static_cast<u32>(y); u32 j1 = j0 + 1;
                u32 k0 = static_cast<u32>(z); u32 k1 = k0 + 1;

                f32 s1 = x - static_cast<f32>(i0); f32 s0 = 1.0f - s1;
                f32 t1 = y - static_cast<f32>(j0); f32 t0 = 1.0f - t1;
                f32 r1 = z - static_cast<f32>(k0); f32 r0 = 1.0f - r1;

                d[grid.IX3(i, j, k)] =
                    r0 * (s0 * (t0 * d0[grid.IX3(i0, j0, k0)] + t1 * d0[grid.IX3(i0, j1, k0)]) +
                           s1 * (t0 * d0[grid.IX3(i1, j0, k0)] + t1 * d0[grid.IX3(i1, j1, k0)])) +
                    r1 * (s0 * (t0 * d0[grid.IX3(i0, j0, k1)] + t1 * d0[grid.IX3(i0, j1, k1)]) +
                           s1 * (t0 * d0[grid.IX3(i1, j0, k1)] + t1 * d0[grid.IX3(i1, j1, k1)]));
            }
        }
    }
    SetBoundary3D(grid, b, d);
}

void FluidSimulation::Project3D(FluidGridData& grid, std::vector<f32>& u, std::vector<f32>& v,
                                  std::vector<f32>& w, std::vector<f32>& p, std::vector<f32>& div, i32 iterations) {
    u32 N = grid.N;
    f32 h = 1.0f / N;

    for (u32 k = 1; k <= N; ++k) {
        for (u32 j = 1; j <= N; ++j) {
            for (u32 i = 1; i <= N; ++i) {
                div[grid.IX3(i, j, k)] = -0.5f * h * (
                    u[grid.IX3(i + 1, j, k)] - u[grid.IX3(i - 1, j, k)] +
                    v[grid.IX3(i, j + 1, k)] - v[grid.IX3(i, j - 1, k)] +
                    w[grid.IX3(i, j, k + 1)] - w[grid.IX3(i, j, k - 1)]);
                p[grid.IX3(i, j, k)] = 0.0f;
            }
        }
    }
    SetBoundary3D(grid, 0, div);
    SetBoundary3D(grid, 0, p);

    for (i32 iter = 0; iter < iterations; ++iter) {
        for (u32 k = 1; k <= N; ++k) {
            for (u32 j = 1; j <= N; ++j) {
                for (u32 i = 1; i <= N; ++i) {
                    p[grid.IX3(i, j, k)] = (div[grid.IX3(i, j, k)] +
                        p[grid.IX3(i - 1, j, k)] + p[grid.IX3(i + 1, j, k)] +
                        p[grid.IX3(i, j - 1, k)] + p[grid.IX3(i, j + 1, k)] +
                        p[grid.IX3(i, j, k - 1)] + p[grid.IX3(i, j, k + 1)]) / 6.0f;
                }
            }
        }
        SetBoundary3D(grid, 0, p);
    }

    for (u32 k = 1; k <= N; ++k) {
        for (u32 j = 1; j <= N; ++j) {
            for (u32 i = 1; i <= N; ++i) {
                u[grid.IX3(i, j, k)] -= 0.5f * N * (p[grid.IX3(i + 1, j, k)] - p[grid.IX3(i - 1, j, k)]);
                v[grid.IX3(i, j, k)] -= 0.5f * N * (p[grid.IX3(i, j + 1, k)] - p[grid.IX3(i, j - 1, k)]);
                w[grid.IX3(i, j, k)] -= 0.5f * N * (p[grid.IX3(i, j, k + 1)] - p[grid.IX3(i, j, k - 1)]);
            }
        }
    }
    SetBoundary3D(grid, 1, u);
    SetBoundary3D(grid, 2, v);
    SetBoundary3D(grid, 3, w);
}

void FluidSimulation::SetBoundary3D(FluidGridData& grid, i32 b, std::vector<f32>& x) {
    u32 N = grid.N;

    // Faces
    for (u32 j = 1; j <= N; ++j) {
        for (u32 i = 1; i <= N; ++i) {
            x[grid.IX3(0, i, j)]     = b == 1 ? -x[grid.IX3(1, i, j)] : x[grid.IX3(1, i, j)];
            x[grid.IX3(N + 1, i, j)] = b == 1 ? -x[grid.IX3(N, i, j)] : x[grid.IX3(N, i, j)];
            x[grid.IX3(i, 0, j)]     = b == 2 ? -x[grid.IX3(i, 1, j)] : x[grid.IX3(i, 1, j)];
            x[grid.IX3(i, N + 1, j)] = b == 2 ? -x[grid.IX3(i, N, j)] : x[grid.IX3(i, N, j)];
            x[grid.IX3(i, j, 0)]     = b == 3 ? -x[grid.IX3(i, j, 1)] : x[grid.IX3(i, j, 1)];
            x[grid.IX3(i, j, N + 1)] = b == 3 ? -x[grid.IX3(i, j, N)] : x[grid.IX3(i, j, N)];
        }
    }

    // Edges (average of two adjacent face cells)
    for (u32 i = 1; i <= N; ++i) {
        x[grid.IX3(0, 0, i)]         = 0.5f * (x[grid.IX3(1, 0, i)] + x[grid.IX3(0, 1, i)]);
        x[grid.IX3(0, N + 1, i)]     = 0.5f * (x[grid.IX3(1, N + 1, i)] + x[grid.IX3(0, N, i)]);
        x[grid.IX3(N + 1, 0, i)]     = 0.5f * (x[grid.IX3(N, 0, i)] + x[grid.IX3(N + 1, 1, i)]);
        x[grid.IX3(N + 1, N + 1, i)] = 0.5f * (x[grid.IX3(N, N + 1, i)] + x[grid.IX3(N + 1, N, i)]);

        x[grid.IX3(0, i, 0)]         = 0.5f * (x[grid.IX3(1, i, 0)] + x[grid.IX3(0, i, 1)]);
        x[grid.IX3(0, i, N + 1)]     = 0.5f * (x[grid.IX3(1, i, N + 1)] + x[grid.IX3(0, i, N)]);
        x[grid.IX3(N + 1, i, 0)]     = 0.5f * (x[grid.IX3(N, i, 0)] + x[grid.IX3(N + 1, i, 1)]);
        x[grid.IX3(N + 1, i, N + 1)] = 0.5f * (x[grid.IX3(N, i, N + 1)] + x[grid.IX3(N + 1, i, N)]);

        x[grid.IX3(i, 0, 0)]         = 0.5f * (x[grid.IX3(i, 1, 0)] + x[grid.IX3(i, 0, 1)]);
        x[grid.IX3(i, 0, N + 1)]     = 0.5f * (x[grid.IX3(i, 1, N + 1)] + x[grid.IX3(i, 0, N)]);
        x[grid.IX3(i, N + 1, 0)]     = 0.5f * (x[grid.IX3(i, N, 0)] + x[grid.IX3(i, N + 1, 1)]);
        x[grid.IX3(i, N + 1, N + 1)] = 0.5f * (x[grid.IX3(i, N, N + 1)] + x[grid.IX3(i, N + 1, N)]);
    }

    // Corners (average of three adjacent edge cells)
    x[grid.IX3(0, 0, 0)]             = (x[grid.IX3(1, 0, 0)] + x[grid.IX3(0, 1, 0)] + x[grid.IX3(0, 0, 1)]) / 3.0f;
    x[grid.IX3(0, N + 1, 0)]         = (x[grid.IX3(1, N + 1, 0)] + x[grid.IX3(0, N, 0)] + x[grid.IX3(0, N + 1, 1)]) / 3.0f;
    x[grid.IX3(N + 1, 0, 0)]         = (x[grid.IX3(N, 0, 0)] + x[grid.IX3(N + 1, 1, 0)] + x[grid.IX3(N + 1, 0, 1)]) / 3.0f;
    x[grid.IX3(N + 1, N + 1, 0)]     = (x[grid.IX3(N, N + 1, 0)] + x[grid.IX3(N + 1, N, 0)] + x[grid.IX3(N + 1, N + 1, 1)]) / 3.0f;
    x[grid.IX3(0, 0, N + 1)]         = (x[grid.IX3(1, 0, N + 1)] + x[grid.IX3(0, 1, N + 1)] + x[grid.IX3(0, 0, N)]) / 3.0f;
    x[grid.IX3(0, N + 1, N + 1)]     = (x[grid.IX3(1, N + 1, N + 1)] + x[grid.IX3(0, N, N + 1)] + x[grid.IX3(0, N + 1, N)]) / 3.0f;
    x[grid.IX3(N + 1, 0, N + 1)]     = (x[grid.IX3(N, 0, N + 1)] + x[grid.IX3(N + 1, 1, N + 1)] + x[grid.IX3(N + 1, 0, N)]) / 3.0f;
    x[grid.IX3(N + 1, N + 1, N + 1)] = (x[grid.IX3(N, N + 1, N + 1)] + x[grid.IX3(N + 1, N, N + 1)] + x[grid.IX3(N + 1, N + 1, N)]) / 3.0f;

    EnforceSolidCells(grid, b, x, N, true,
                      [&](u32 i, u32 j, u32 k) { return static_cast<usize>(grid.IX3(i, j, k)); });
}

} // namespace Effects
} // namespace Enjin
