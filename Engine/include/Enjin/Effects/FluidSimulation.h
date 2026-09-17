#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Entity.h"
#include <vector>
#include <unordered_map>

namespace Enjin {
namespace Effects {

// Runtime grid data for a fluid volume (not serialized)
struct FluidGridData {
    u32 N = 0;           // Grid resolution (cells per side, excluding boundary)
    bool is3D = false;

    // Double-buffered arrays: current and previous
    std::vector<f32> density;
    std::vector<f32> densityPrev;
    std::vector<f32> velocityX;
    std::vector<f32> velocityXPrev;
    std::vector<f32> velocityY;
    std::vector<f32> velocityYPrev;
    std::vector<f32> velocityZ;      // 3D only
    std::vector<f32> velocityZPrev;  // 3D only

    // Scratch buffers for pressure projection
    std::vector<f32> pressure;
    std::vector<f32> divergence;

    // One byte per cell: 1 = inside solid geometry, 0 = fluid. EMPTY means the
    // volume has no obstacles, which is the old behaviour and costs nothing to
    // check. Filled by Effects::BuildFluidObstacleMask from the scene's
    // colliders; see FluidObstacles.h for why this exists at all.
    std::vector<u8> solid;

    // True when this grid's contents come from a RECORDING rather than from
    // the solver. The solver skips it entirely: re-solving a played-back frame
    // would immediately overwrite it with a step of real simulation, so the
    // recording would be visible for no frames at all.
    bool playbackDriven = false;

    bool HasObstacles() const { return !solid.empty(); }
    bool IsSolid(usize index) const { return !solid.empty() && solid[index] != 0; }

    void Allocate(u32 gridSize, bool mode3D) {
        N = gridSize;
        is3D = mode3D;
        u32 totalSize = is3D ? (N + 2) * (N + 2) * (N + 2) : (N + 2) * (N + 2);
        density.assign(totalSize, 0.0f);
        densityPrev.assign(totalSize, 0.0f);
        velocityX.assign(totalSize, 0.0f);
        velocityXPrev.assign(totalSize, 0.0f);
        velocityY.assign(totalSize, 0.0f);
        velocityYPrev.assign(totalSize, 0.0f);
        pressure.assign(totalSize, 0.0f);
        divergence.assign(totalSize, 0.0f);
        // Reallocating drops any obstacle mask: it was sized for the OLD
        // resolution, and keeping it would index cells that no longer exist.
        solid.clear();
        if (is3D) {
            velocityZ.assign(totalSize, 0.0f);
            velocityZPrev.assign(totalSize, 0.0f);
        } else {
            velocityZ.clear();
            velocityZPrev.clear();
        }
    }

    void Clear() {
        std::fill(density.begin(), density.end(), 0.0f);
        std::fill(densityPrev.begin(), densityPrev.end(), 0.0f);
        std::fill(velocityX.begin(), velocityX.end(), 0.0f);
        std::fill(velocityXPrev.begin(), velocityXPrev.end(), 0.0f);
        std::fill(velocityY.begin(), velocityY.end(), 0.0f);
        std::fill(velocityYPrev.begin(), velocityYPrev.end(), 0.0f);
        std::fill(pressure.begin(), pressure.end(), 0.0f);
        std::fill(divergence.begin(), divergence.end(), 0.0f);
        if (is3D) {
            std::fill(velocityZ.begin(), velocityZ.end(), 0.0f);
            std::fill(velocityZPrev.begin(), velocityZPrev.end(), 0.0f);
        }
    }

    // 2D indexing: i,j in [0..N+1]
    u32 IX(u32 i, u32 j) const { return i + (N + 2) * j; }
    // 3D indexing: i,j,k in [0..N+1]
    u32 IX3(u32 i, u32 j, u32 k) const { return i + (N + 2) * (j + (N + 2) * k); }
};

// Stable Fluids solver (Jos Stam, unconditionally stable)
class ENJIN_API FluidSimulation {
public:
    void Update(f32 dt, ECS::World* world);

    void AddDensityAtWorldPos(ECS::Entity entity, const Math::Vector3& worldPos, f32 amount, f32 radius);
    void AddVelocityAtWorldPos(ECS::Entity entity, const Math::Vector3& worldPos, const Math::Vector3& velocity, f32 radius);
    void Reset(ECS::Entity entity);

    // Obstacles for one volume: one byte per cell, 1 = solid, sized to match
    // the grid's own arrays. Build it with Effects::BuildFluidObstacleMask.
    // An empty mask means no obstacles, which is the behaviour every volume
    // had before obstacles existed.
    //
    // Set rather than derived, because the cases differ: a bake voxelises once
    // against static level geometry, and a live volume would want to refresh
    // only when a collider actually moves. Neither wants it recomputed per
    // step -- the mask costs cells-times-colliders to build.
    void SetObstacleMask(ECS::Entity entity, std::vector<u8> mask);
    void ClearObstacleMask(ECS::Entity entity);

    // Drive a volume's grid from a recorded frame instead of solving it.
    // Allocates the grid on first use and marks it playback-driven, so Update
    // leaves it alone from then on. Returns false if the density does not
    // match the grid the given size implies.
    bool SetPlaybackDensity(ECS::Entity entity, u32 gridSize, bool is3D,
                            const std::vector<f32>& density);

    // Solve ONLY this volume and ignore every other one in the world.
    //
    // For baking. A bake records one volume, but Update iterates the world, so
    // without this a bake in a ten-volume level solves all ten for the whole
    // take -- ten times the wait for the same output. INVALID_ENTITY (the
    // default) means solve everything, which is what a running game wants.
    void SetSoloEntity(ECS::Entity entity) { m_SoloEntity = entity; }
    ECS::Entity GetSoloEntity() const { return m_SoloEntity; }
    void OnEntityRemoved(ECS::Entity entity);

    const FluidGridData* GetGridData(ECS::Entity entity) const;
    FluidGridData* GetMutableGridData(ECS::Entity entity);

    // How long the whole fluid step may take, in milliseconds. Volumes take
    // turns: whatever does not fit this frame is simulated next frame with the
    // time it missed, so motion runs at the right SPEED even when a volume gets
    // fewer updates.
    //
    // There was no budget at all, and a 3D volume measured ~100 ms per step at
    // the shipped default of 20 iterations (Tests/Integration/FluidBench.cpp).
    // Three campfires in a scene was 4 fps, because every one of them solved in
    // full, every frame, on the main thread. Nothing culled distance, nothing
    // skipped off-screen, nothing capped the total.
    //
    // 8 ms is half a 60 fps frame and still far too much for one system; it is
    // set here because the alternative today is 300. Lower it as the solver gets
    // cheaper.
    void SetFrameBudgetMs(f64 ms) { m_FrameBudgetMs = ms; }
    f64 GetFrameBudgetMs() const { return m_FrameBudgetMs; }

    // Volumes whose step was deferred this frame, for the profiler and for
    // tests -- a budget that silently drops work is indistinguishable from a
    // budget that is never hit.
    u32 GetDeferredVolumeCount() const { return m_DeferredVolumes; }

private:
    std::unordered_map<ECS::Entity, FluidGridData> m_Grids;

    // Time each volume is owed because the budget ran out before its turn.
    std::unordered_map<ECS::Entity, f32> m_PendingTime;
    // Where the round-robin resumes, so the same volumes are not always the
    // ones that get skipped.
    usize m_RoundRobinStart = 0;
    f64 m_FrameBudgetMs = 8.0;
    u32 m_DeferredVolumes = 0;
    ECS::Entity m_SoloEntity = ECS::INVALID_ENTITY;

    // 2D solver steps
    void Step2D(FluidGridData& grid, f32 dt, f32 visc, f32 diff, f32 dissipation, f32 velDissipation, i32 iterations, f32 buoyancy);
    void Diffuse2D(FluidGridData& grid, i32 b, std::vector<f32>& x, const std::vector<f32>& x0, f32 diff, f32 dt, i32 iterations);
    void Advect2D(FluidGridData& grid, i32 b, std::vector<f32>& d, const std::vector<f32>& d0, const std::vector<f32>& u, const std::vector<f32>& v, f32 dt);
    void Project2D(FluidGridData& grid, std::vector<f32>& u, std::vector<f32>& v, std::vector<f32>& p, std::vector<f32>& div, i32 iterations);
    void SetBoundary2D(FluidGridData& grid, i32 b, std::vector<f32>& x);

    // 3D solver steps
    void Step3D(FluidGridData& grid, f32 dt, f32 visc, f32 diff, f32 dissipation, f32 velDissipation, i32 iterations, f32 buoyancy);
    void Diffuse3D(FluidGridData& grid, i32 b, std::vector<f32>& x, const std::vector<f32>& x0, f32 diff, f32 dt, i32 iterations);
    void Advect3D(FluidGridData& grid, i32 b, std::vector<f32>& d, const std::vector<f32>& d0, const std::vector<f32>& u, const std::vector<f32>& v, const std::vector<f32>& w, f32 dt);
    void Project3D(FluidGridData& grid, std::vector<f32>& u, std::vector<f32>& v, std::vector<f32>& w, std::vector<f32>& p, std::vector<f32>& div, i32 iterations);
    void SetBoundary3D(FluidGridData& grid, i32 b, std::vector<f32>& x);
};

} // namespace Effects
} // namespace Enjin
