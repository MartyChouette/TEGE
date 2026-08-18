#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"

namespace Enjin {
namespace ECS { class World; }

namespace Gameplay {

// Position-based-dynamics grid cloth (see ECS::ClothComponent). Runs on the CPU
// (a 16x16 sheet is 256 points — trivial); writes deformed vertices into the
// entity's MeshComponent each step, which the renderer re-uploads via the cloth
// dirty flags. Pinned points follow the entity's world transform, so moving the
// entity drags the cloth.
class ENJIN_API ClothSystem {
public:
    void Update(ECS::World* world, f32 deltaTime);

    // Rebuild every cloth to its fresh, untorn grid. Called after play-stop
    // restores the world: the restore brings back component data but the GPU
    // buffers still hold the last simulated/torn state, so the cloth must be
    // rebuilt (and its buffers retired) or the tear survives into edit mode.
    static void ResetAll(ECS::World* world);
};

} // namespace Gameplay
} // namespace Enjin
