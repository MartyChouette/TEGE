#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"

namespace Enjin {
namespace ECS { class World; }
namespace Effects { class WindSystem; }

namespace Gameplay {

// Position-based-dynamics grid cloth (see ECS::ClothComponent). Runs on the CPU
// (a 16x16 sheet is 256 points — trivial); writes deformed vertices into the
// entity's MeshComponent each step, which the renderer re-uploads via the cloth
// dirty flags. Pinned points follow the entity's world transform, so moving the
// entity drags the cloth.
class ENJIN_API ClothSystem {
public:
    // wind (optional) = the scene's live wind field; cloths with useWeatherWind
    // sample it at their position unless a WeatherZone covers them (zone wins).
    void Update(ECS::World* world, f32 deltaTime, const Effects::WindSystem* wind = nullptr);

private:
    f32 m_Time = 0.0f;   // gust phase clock

public:
    // Rebuild every cloth to its fresh, untorn grid. Called after play-stop
    // restores the world: the restore brings back component data but the GPU
    // buffers still hold the last simulated/torn state, so the cloth must be
    // rebuilt (and its buffers retired) or the tear survives into edit mode.
    static void ResetAll(ECS::World* world);
};

} // namespace Gameplay
} // namespace Enjin
