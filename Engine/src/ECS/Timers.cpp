#include "Enjin/ECS/Timers.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/EntityEventBus.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include <cmath>
#include <vector>

namespace Enjin {
namespace ECS {

void UpdateTimers(World* world, f32 deltaTime, EntityEventBus* bus) {
    if (!world) return;
    // A copy: a completion listener may add or remove timers
    const auto& live = world->GetEntitiesWithComponent<TimerComponent>();
    const std::vector<Entity> timers(live.begin(), live.end());
    for (const Entity e : timers) {
        auto* t = world->GetComponent<TimerComponent>(e);
        if (!t) continue;

        if (t->autoStart && !t->autoStarted) {
            t->autoStarted = true;
            t->isRunning = true;
            t->elapsed = 0.0f;
        }
        if (!t->isRunning || deltaTime <= 0.0f) continue;

        t->elapsed += deltaTime;
        if (t->elapsed < t->duration) continue;

        if (t->loop && t->duration > 0.0f) {
            const f32 laps = std::floor(t->elapsed / t->duration);
            t->loopCount += static_cast<i32>(laps);
            t->elapsed -= laps * t->duration;
        } else {
            // One-shot, or a zero duration that would otherwise loop forever
            t->loopCount += 1;
            t->elapsed = t->duration;
            t->isRunning = false;
        }

        if (bus && !t->completeEvent.empty()) {
            EntityEvent ev;
            ev.name = t->completeEvent;
            ev.sender = e;
            if (t->onCompleteNotify != INVALID_ENTITY && world->IsValid(t->onCompleteNotify)) {
                ev.target = t->onCompleteNotify;
            }
            ev.ints["loops"] = t->loopCount;
            // t is not touched after this: the listener may add timers and move storage
            bus->Send(ev.name, ev);
        }
    }
}

} // namespace ECS
} // namespace Enjin
