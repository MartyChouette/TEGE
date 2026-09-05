#pragma once
#include "Enjin/Platform/Platform.h"
#include "Enjin/Effects/Water.h"

namespace Enjin { namespace ECS {

struct Water3DComponent {
    Effects::Water3DSettings settings;
    bool meshCreated = false;  // Initial mesh built by RenderSystem
    // Vertices moved this frame and the GPU copy is stale. Water3D animates by
    // rewriting its MeshComponent on the CPU every frame (PlayMode drives it),
    // but until this existed nothing re-uploaded the result, so the surface on
    // screen stayed the flat mesh built at startup and every wave setting -
    // gerstnerWaves, waveHeight, waveSteepness - was dead on the GPU.
    // Same protocol cloth, rope, jelly and procedural meshes already use.
    bool meshDirty = false;

    // ---- written by the water system each frame, read by physics ----
    // The wave clock and the settings the surface was actually built from this
    // frame (wind folded in). Buoyancy samples these so a boat floats to the
    // surface that is on screen, instead of to the flat plane at the entity's
    // Y while the waves pass straight through it.
    //
    // Not serialized: it is this frame's state, not authored data.
    f32 runtimeWaveTime = 0.0f;
    Effects::Water3DSettings runtimeSettings;
    bool runtimeValid = false;
};

} }
