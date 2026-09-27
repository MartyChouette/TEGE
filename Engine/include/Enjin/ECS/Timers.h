#pragma once
#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"

namespace Enjin {
namespace ECS {

class World;
class EntityEventBus;

// Ticks every TimerComponent. Nothing ever did (SD-27): Duration, Loop and Auto
// Start were in the inspector and saved, and a timer never moved.
//
// Auto Start starts a timer on its first tick in play. A running timer adds dt
// (the game's scaled dt, so bullet time slows it) and completes at Duration:
// loopCount goes up and its Complete Event is sent with the timer as "sender",
// onCompleteNotify as "target" when that entity exists, and the loop count as
// the "loops" int. A looping timer carries the overshoot into the next lap; a
// timer shorter than a frame completes at most once per frame, and loopCount
// still counts every lap. A one-shot timer stops at Duration.
//
// The event goes to scripts (Events_Listen) and visual scripts (Custom Event)
// through the runtime's EntityEventBus bridge. bus may be null (headless).
ENJIN_API void UpdateTimers(World* world, f32 deltaTime, EntityEventBus* bus);

} // namespace ECS
} // namespace Enjin
