#include "Enjin/ECS/EntityEventBridge.h"
#include "Enjin/ECS/EntityEventBus.h"
#include "Enjin/ECS/Systems/VisualScriptSystem.h"
#include "Enjin/Scripting/ScriptEvents.h"

namespace Enjin {
namespace ECS {

Scripting::EventData ToScriptEventData(const EntityEvent& event) {
    Scripting::EventData data;
    data.floats = event.floats;
    data.ints = event.ints;
    data.strings = event.strings;
    for (const auto& [key, e] : event.entities) data.entities[key] = static_cast<u64>(e);
    // Don't overwrite a payload key the sender chose itself
    if (event.sender != INVALID_ENTITY) data.entities.emplace("sender", static_cast<u64>(event.sender));
    if (event.target != INVALID_ENTITY) data.entities.emplace("target", static_cast<u64>(event.target));
    return data;
}

void ForwardEntityEventsToScripts(EntityEventBus& bus,
                                  Scripting::ScriptEventBus* scripts,
                                  VisualScriptSystem* visualScripts) {
    bus.SetForwarder([scripts, visualScripts](const std::string& name, const EntityEvent& event) {
        if (scripts) scripts->Send(name, ToScriptEventData(event));
        if (visualScripts) visualScripts->BroadcastEvent(name, 0.0f);
    });
}

} // namespace ECS
} // namespace Enjin
