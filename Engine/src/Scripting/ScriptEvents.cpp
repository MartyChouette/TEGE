#include "Enjin/Scripting/ScriptEvents.h"
#include "Enjin/Scripting/ScriptEngine.h"
#include "Enjin/Logging/Log.h"
#include <angelscript.h>

namespace Enjin {
namespace Scripting {

ScriptEventBus::ScriptEventBus() = default;

ScriptEventBus::~ScriptEventBus() {
    Clear();
}

static constexpr u32 kMaxListenersPerEvent = 1024;

u32 ScriptEventBus::Listen(const std::string& eventName, asIScriptObject* obj,
                           asIScriptFunction* callback, u64 entityId) {
    // S2 fix: Cap listener count per event to prevent unbounded memory growth
    auto& list = m_Listeners[eventName];
    if (list.size() >= kMaxListenersPerEvent) {
        ENJIN_LOG_WARN(Script, "ScriptEventBus: listener cap (%u) reached for event '%s'",
                       kMaxListenersPerEvent, eventName.c_str());
        return 0;
    }

    EventListener listener;
    listener.object = obj;
    listener.callback = callback;
    listener.entityId = entityId;
    // S8 fix: skip ID 0 on wrap-around (0 is used as error return)
    if (m_NextId == 0) m_NextId = 1;
    listener.id = m_NextId++;

    // AddRef to prevent the object and function from being garbage collected
    if (obj) obj->AddRef();
    if (callback) callback->AddRef();

    list.push_back(listener);
    return listener.id;
}

void ScriptEventBus::RemoveListener(u32 id) {
    for (auto& [name, listeners] : m_Listeners) {
        for (auto it = listeners.begin(); it != listeners.end(); ++it) {
            if (it->id == id) {
                if (it->object) it->object->Release();
                if (it->callback) it->callback->Release();
                listeners.erase(it);
                return;
            }
        }
    }
}

void ScriptEventBus::RemoveAllForEntity(u64 entityId) {
    for (auto& [name, listeners] : m_Listeners) {
        for (auto it = listeners.begin(); it != listeners.end();) {
            if (it->entityId == entityId) {
                if (it->object) it->object->Release();
                if (it->callback) it->callback->Release();
                it = listeners.erase(it);
            } else {
                ++it;
            }
        }
    }
}

// S5 fix: Guard against infinite event recursion (e.g. event A fires event B fires event A)
static constexpr u32 kMaxEventRecursionDepth = 8;
static u32 s_EventDispatchDepth = 0;

void ScriptEventBus::Send(const std::string& eventName, const EventData& data) {
    if (s_EventDispatchDepth >= kMaxEventRecursionDepth) {
        ENJIN_LOG_WARN(Script, "ScriptEventBus: recursion depth limit (%u) reached for event '%s'",
                       kMaxEventRecursionDepth, eventName.c_str());
        return;
    }

    auto it = m_Listeners.find(eventName);
    if (it == m_Listeners.end()) return;

    // Iterate a copy in case listeners modify the list during dispatch
    auto listenersCopy = it->second;
    s_EventDispatchDepth++;
    for (auto& listener : listenersCopy) {
        DispatchToListener(listener, eventName, data);
    }
    s_EventDispatchDepth--;
}

void ScriptEventBus::Broadcast(const EventData& data) {
    if (s_EventDispatchDepth >= kMaxEventRecursionDepth) {
        ENJIN_LOG_WARN(Script, "ScriptEventBus: recursion depth limit (%u) reached during broadcast",
                       kMaxEventRecursionDepth);
        return;
    }

    s_EventDispatchDepth++;
    for (auto& [name, listeners] : m_Listeners) {
        auto listenersCopy = listeners;
        for (auto& listener : listenersCopy) {
            DispatchToListener(listener, name, data);
        }
    }
    s_EventDispatchDepth--;
}

void ScriptEventBus::DispatchToListener(EventListener& listener, const std::string& eventName,
                                         const EventData& data) {
    if (!listener.callback || !m_ScriptEngine) return;

    asIScriptContext* ctx = m_ScriptEngine->AcquireContext();
    if (!ctx) return;

    ctx->Prepare(listener.callback);

    // If it's an object method, set the object
    if (listener.object) {
        ctx->SetObject(listener.object);
    }

    // Pass event name as first argument (if the callback accepts it)
    asIScriptFunction* func = listener.callback;
    if (func->GetParamCount() >= 1) {
        // First param is the event name string
        ctx->SetArgObject(0, const_cast<std::string*>(&eventName));
    }

    int r = ctx->Execute();
    if (r == asEXECUTION_EXCEPTION) {
        ENJIN_LOG_ERROR(Script, "Event callback exception for '%s': %s",
            eventName.c_str(), ctx->GetExceptionString());
    }

    m_ScriptEngine->ReturnContext(ctx);
}

void ScriptEventBus::Clear() {
    for (auto& [name, listeners] : m_Listeners) {
        for (auto& listener : listeners) {
            if (listener.object) listener.object->Release();
            if (listener.callback) listener.callback->Release();
        }
    }
    m_Listeners.clear();
}

} // namespace Scripting
} // namespace Enjin
