#pragma once
#include "Enjin/Platform/Platform.h"

namespace Enjin {
namespace Scripting { class ScriptEventBus; struct EventData; }
namespace ECS {

class EntityEventBus;
class VisualScriptSystem;
struct EntityEvent;

// Hands every engine event to scripts and visual scripts.
//
// The engine has two buses: EntityEventBus, which components send on
// (ActionTrigger's Emit Event, dialogue, water_enter, timers), and the
// ScriptEventBus that Events_Listen subscribes to. Nothing joined them, so a
// script could never hear an engine event, and visual script Custom Event
// nodes had no caller at all.
//
// A script hears it through Events_Listen(name, ...), reading the payload with
// the Events_Current* bindings; "sender" and "target" are added as entities.
// Every enabled visual script runs its Custom Event node of the same name.
// Either pointer may be null. Call once per runtime; it replaces any earlier
// forwarder on the bus.
ENJIN_API void ForwardEntityEventsToScripts(EntityEventBus& bus,
                                            Scripting::ScriptEventBus* scripts,
                                            VisualScriptSystem* visualScripts);

// The script payload for an engine event. Exposed for tests.
ENJIN_API Scripting::EventData ToScriptEventData(const EntityEvent& event);

} // namespace ECS
} // namespace Enjin
