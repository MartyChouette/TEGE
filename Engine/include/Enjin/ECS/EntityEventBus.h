#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/ECS/Entity.h"
#include <functional>
#include <unordered_map>
#include <vector>
#include <string>

namespace Enjin {
namespace ECS {

struct EntityEvent {
    std::string name;
    Entity sender = INVALID_ENTITY;
    Entity target = INVALID_ENTITY;
    std::unordered_map<std::string, f32> floats;
    std::unordered_map<std::string, i32> ints;
    std::unordered_map<std::string, std::string> strings;
    std::unordered_map<std::string, Entity> entities;
};

using EventCallback = std::function<void(const EntityEvent&)>;

class ENJIN_API EntityEventBus {
public:
    // Register a listener for a specific event name.
    // If entity != INVALID_ENTITY, listener is associated with that entity (for cleanup).
    u32 Listen(const std::string& eventName, EventCallback callback, Entity entity = INVALID_ENTITY);

    // Remove a listener by its registration ID.
    void RemoveListener(u32 id);

    // Remove all listeners associated with a given entity.
    void RemoveAllForEntity(Entity entity);

    // Send an event immediately to all matching listeners.
    void Send(const std::string& eventName, const EntityEvent& event);

    // Broadcast an event to ALL listeners (uses event.name).
    void Broadcast(const EntityEvent& event);

    // Queue an event to be dispatched at end of frame.
    void SendDeferred(const std::string& eventName, const EntityEvent& event);

    // Dispatch all queued deferred events. Call once per frame.
    void ProcessDeferred();

    // Remove all listeners and pending events. The forwarder is wiring, not
    // state, so it survives: editor Stop clears the bus and the next Play must
    // still reach scripts.
    void Clear();

    // Sees every event this bus dispatches, after its own listeners, whether or
    // not anything listened. The runtimes point it at scripts and visual
    // scripts (ECS::ForwardEntityEventsToScripts). Without it an engine event
    // reached only C++ listeners, and there were none, so ActionTrigger's Emit
    // Event, dialogue events and water_enter went nowhere a game could hear.
    using Forwarder = std::function<void(const std::string& eventName, const EntityEvent& event)>;
    void SetForwarder(Forwarder forwarder) { m_Forwarder = std::move(forwarder); }

private:
    struct Listener {
        u32 id;
        std::string eventName;
        EventCallback callback;
        Entity entity;
    };

    std::unordered_map<std::string, std::vector<Listener>> m_Listeners;
    std::vector<std::pair<std::string, EntityEvent>> m_DeferredQueue;
    u32 m_NextId = 1;
    Forwarder m_Forwarder;
};

} // namespace ECS
} // namespace Enjin
