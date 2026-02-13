#include "Enjin/ECS/EntityEventBus.h"

namespace Enjin {
namespace ECS {

u32 EntityEventBus::Listen(const std::string& eventName, EventCallback callback, Entity entity) {
    u32 id = m_NextId++;
    m_Listeners[eventName].push_back({id, eventName, std::move(callback), entity});
    return id;
}

void EntityEventBus::RemoveListener(u32 id) {
    for (auto& [name, listeners] : m_Listeners) {
        for (auto it = listeners.begin(); it != listeners.end(); ++it) {
            if (it->id == id) {
                listeners.erase(it);
                return;
            }
        }
    }
}

void EntityEventBus::RemoveAllForEntity(Entity entity) {
    for (auto& [name, listeners] : m_Listeners) {
        listeners.erase(
            std::remove_if(listeners.begin(), listeners.end(),
                [entity](const Listener& l) { return l.entity == entity; }),
            listeners.end());
    }
}

void EntityEventBus::Send(const std::string& eventName, const EntityEvent& event) {
    auto it = m_Listeners.find(eventName);
    if (it == m_Listeners.end()) return;
    // Copy the listener vector before iterating so that callbacks that modify
    // the listener list (add/remove) don't invalidate iterators or references
    auto listenersCopy = it->second;
    for (const auto& listener : listenersCopy) {
        listener.callback(event);
    }
}

void EntityEventBus::Broadcast(const EntityEvent& event) {
    Send(event.name, event);
}

void EntityEventBus::SendDeferred(const std::string& eventName, const EntityEvent& event) {
    m_DeferredQueue.emplace_back(eventName, event);
}

void EntityEventBus::ProcessDeferred() {
    // Swap to local to allow new events during processing
    auto queue = std::move(m_DeferredQueue);
    m_DeferredQueue.clear();
    for (const auto& [name, event] : queue) {
        Send(name, event);
    }
}

void EntityEventBus::Clear() {
    m_Listeners.clear();
    m_DeferredQueue.clear();
    m_NextId = 1;
}

} // namespace ECS
} // namespace Enjin
