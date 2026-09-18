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

    // Fingerprint of everything `solid` was built FROM: the scene's colliders
    // plus this volume's own placement (the mask is world space, so moving the
    // volume invalidates it just as moving a crate does). The live solver
    // rebuilds when this changes and not otherwise -- voxelising every volume
    // every frame is the honest alternative and costs far more than a hash.
    u64 obstacleKey = 0;

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
        obstacleKey = 0;   // the mask is gone, so the cache key must go too
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

// Where the camera is, so a volume nobody can see need not be solved.
//
// Fed in by each runtime rather than read from the world: the solver has no
// business knowing which camera is the active one, and the editor's answer
// (edit camera, or the game camera in play mode) is not the player's. This is
// the shape ParticleSystem::SetSceneWind already uses for scene state a system
// does not own.
//
// UNSET means nothing is culled -- the behaviour before this existed. A viewer
// that might be stale or wrong would freeze smoke in front of the player,
// which is far worse than paying for a volume that is off screen.
struct ENJIN_API FluidViewer {
    Math::Vector3 position;

    // Six planes as (nx, ny, nz, d), inward-facing, from
    // RenderSystem::ExtractFrustumPlanes. A volume whose bounding sphere is
    // fully behind any one of them is not solved this frame.
    Math::Vector4 frustumPlanes[6];
    bool hasFrustum = false;

    // Beyond this distance a volume is not solved, measured to its bounding
    // sphere. 0 means no distance limit, which is the honest default: what
    // counts as "far" depends on the scene's scale and guessing it would
    // freeze a chimney in a cramped level.
    f32 cullDistance = 0.0f;
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

    // Everything one step needs from a volume, COPIED.
    //
    // This is what lets a bake run off the main thread. ECS reads are only
    // lock-free because structural mutation is owner-thread-only (adr-0004), so
    // a solver that walked the world from a worker would be reading components
    // while the editor added and removed them. A bake takes this snapshot on
    // the owner thread and then never touches the world again.
    struct FluidStepParams {
        f32 viscosity = 0.0f;
        f32 diffusion = 0.0f;
        f32 dissipation = 1.0f;
        f32 velocityDissipation = 1.0f;
        f32 buoyancy = 0.0f;
        i32 iterations = 20;

        // The plume injected each step, in GRID coordinates -- the same
        // emission Update does, kept here so an isolated step reproduces a
        // live volume exactly rather than approximately.
        f32 sourceDensity = 0.0f;
        f32 sourceRadius = 1.0f;
        f32 sourceVelocityScale = 0.0f;
    };

    // Step ONE grid from a snapshot, touching no world and no member state.
    static void StepGrid(FluidGridData& grid, const FluidStepParams& params, f32 dt);

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

    // Cull volumes the camera cannot see. Nothing is culled until a runtime
    // sets this; see FluidViewer.
    //
    // A culled volume FREEZES: it keeps its density and resumes from it, and
    // the time it missed is dropped rather than owed, so walking back to a
    // campfire shows the plume it had rather than a fast-forward of the
    // minutes you were away. Frozen-and-resuming is the compromise this buys;
    // the alternative is paying 56 ms for a volume behind the player.
    void SetViewer(const FluidViewer& viewer) { m_Viewer = viewer; m_HasViewer = true; }
    void ClearViewer() { m_HasViewer = false; }
    bool HasViewer() const { return m_HasViewer; }

    // Volumes skipped this frame because the viewer could not see them. Same
    // reason the deferred count exists: work that vanishes silently cannot be
    // told apart from work that was never needed.
    u32 GetCulledVolumeCount() const { return m_CulledVolumes; }

private:
    std::unordered_map<ECS::Entity, FluidGridData> m_Grids;

    // Time each volume is owed because the budget ran out before its turn.
    std::unordered_map<ECS::Entity, f32> m_PendingTime;
    // Where the round-robin resumes, so the same volumes are not always the
    // ones that get skipped.
    usize m_RoundRobinStart = 0;
    f64 m_FrameBudgetMs = 8.0;
    u32 m_DeferredVolumes = 0;
    u32 m_CulledVolumes = 0;
    ECS::Entity m_SoloEntity = ECS::INVALID_ENTITY;
    FluidViewer m_Viewer;
    bool m_HasViewer = false;

    // 2D solver steps
    static void Step2D(FluidGridData& grid, f32 dt, f32 visc, f32 diff, f32 dissipation, f32 velDissipation, i32 iterations, f32 buoyancy);
    static void Diffuse2D(FluidGridData& grid, i32 b, std::vector<f32>& x, const std::vector<f32>& x0, f32 diff, f32 dt, i32 iterations);
    static void Advect2D(FluidGridData& grid, i32 b, std::vector<f32>& d, const std::vector<f32>& d0, const std::vector<f32>& u, const std::vector<f32>& v, f32 dt);
    static void Project2D(FluidGridData& grid, std::vector<f32>& u, std::vector<f32>& v, std::vector<f32>& p, std::vector<f32>& div, i32 iterations);
    static void SetBoundary2D(FluidGridData& grid, i32 b, std::vector<f32>& x);

    // 3D solver steps
    static void Step3D(FluidGridData& grid, f32 dt, f32 visc, f32 diff, f32 dissipation, f32 velDissipation, i32 iterations, f32 buoyancy);
    static void Diffuse3D(FluidGridData& grid, i32 b, std::vector<f32>& x, const std::vector<f32>& x0, f32 diff, f32 dt, i32 iterations);
    static void Advect3D(FluidGridData& grid, i32 b, std::vector<f32>& d, const std::vector<f32>& d0, const std::vector<f32>& u, const std::vector<f32>& v, const std::vector<f32>& w, f32 dt);
    static void Project3D(FluidGridData& grid, std::vector<f32>& u, std::vector<f32>& v, std::vector<f32>& w, std::vector<f32>& p, std::vector<f32>& div, i32 iterations);
    static void SetBoundary3D(FluidGridData& grid, i32 b, std::vector<f32>& x);
};

} // namespace Effects
} // namespace Enjin
