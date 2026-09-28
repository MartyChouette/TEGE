#pragma once
// Water volumes freezing and thawing with the weather.
//
// One copy for every runtime. The desktop player and the editor each carried
// the same loop, and the web player carried none, so a pond froze in snow on
// desktop and stayed open water in a browser.

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"

namespace Enjin {
namespace ECS { class World; }
namespace Effects {

// Advance every WaterVolumeComponent's freezeProgress by dt. The highest-
// priority TemperatureZone containing the water decides: freezing freezes,
// near-freezing settles at a partial freeze of 0.3, anything else thaws.
// Snow heavier than 0.25 freezes water with no zone at all (a zone still
// overrides). isFrozen follows freezeProgress >= 0.99.
ENJIN_API void UpdateWaterFreeze(ECS::World* world, f32 snowIntensity, f32 dt);

} // namespace Effects
} // namespace Enjin
