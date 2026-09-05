#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Entity.h"
#include "Enjin/Effects/Metaballs.h"
#include "Enjin/Effects/CellularAutomataGeometry.h"
#include "Enjin/Effects/Projection4D.h"
#include "Enjin/Effects/FourierMesh.h"
#include "Enjin/Effects/ReactionDiffusion.h"
#include "Enjin/Effects/PhysarumSimulation.h"
#include <memory>
#include <unordered_map>

namespace Enjin {
namespace ECS {

// ---------------------------------------------------------------------------
// GeneratedGeometrySystem
// ---------------------------------------------------------------------------
// Drives the four CPU geometry generators and writes their output into the
// owning entity's MeshComponent. Each generator kept its own per-instance state
// (an automaton grid, accumulated 4D rotation angles, a DFT coefficient set),
// so the system holds one generator instance per entity, keyed by entity index,
// and drops it when the entity goes away.
//
// Runs on the owner thread during the normal system tick. It adds MeshComponent
// and ProceduralMeshComponent when they are missing, which is structural
// mutation and therefore main-thread only (ADR-0004).
class ENJIN_API GeneratedGeometrySystem {
public:
    void SetWorld(World* world) { m_World = world; }

    void Update(World* world, f32 deltaTime);

    // Drop all cached generator state. Call on play stop so a second play
    // session starts an automaton from generation 0 rather than mid-run.
    void Reset();

    // Number of entities currently generating geometry. Editor readout.
    usize ActiveCount() const;

private:
    void UpdateMetaballs(World* world, f32 deltaTime);
    void UpdateCellularAutomata(World* world, f32 deltaTime);
    void UpdateProjection4D(World* world, f32 deltaTime);
    void UpdateFourierMeshes(World* world, f32 deltaTime);
    void UpdateReactionDiffusion(World* world, f32 deltaTime);
    void UpdatePhysarum(World* world, f32 deltaTime);

    // Per-entity generator state.
    struct CAState {
        Effects::CellularAutomataGeometry sim;
        u64 configHash = 0;   // re-Initialize when the authored config changes
    };
    struct P4DState {
        Effects::Projection4D projector;
        Effects::Polytope4D polytope;
        u8 builtFrom = 0xFF;  // which Polytope enum the cached polytope came from
        f32 accum = 0.0f;     // rebuild throttle
    };
    struct FourierState {
        Effects::FourierMeshDecomposition dft;
        u64 configHash = 0;
    };
    // The two texture simulations. Both hold a whole grid (and 50k agents for
    // Physarum), so they are kept per entity and dropped with the entity rather
    // than rebuilt each frame.
    struct RDState {
        Effects::ReactionDiffusion sim;
        u64 configHash = 0;
        bool baked = false;   // one upload per configuration, see the note below
    };
    struct PhysarumState {
        Effects::PhysarumSimulation sim;
        u64 configHash = 0;
        bool baked = false;
    };

    World* m_World = nullptr;
    Effects::MetaballSystem m_Metaballs;
    f32 m_MetaballAccum = 0.0f;

    std::unordered_map<u32, CAState> m_CAStates;
    std::unordered_map<u32, P4DState> m_P4DStates;
    std::unordered_map<u32, FourierState> m_FourierStates;
    std::unordered_map<u32, RDState> m_RDStates;
    std::unordered_map<u32, PhysarumState> m_PhysarumStates;
};

} // namespace ECS
} // namespace Enjin
