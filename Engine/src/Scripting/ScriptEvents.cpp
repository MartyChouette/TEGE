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

// Payload of the event currently being dispatched (see GetCurrentDispatchEventData)
static const EventData* s_CurrentDispatchData = nullptr;

const EventData* GetCurrentDispatchEventData() {
    return s_CurrentDispatchData;
}

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

    // EventCallback(this.Method) arrives as a DELEGATE: it carries its own
    // bound object. Preparing the delegate and then calling SetObject with the
    // registering script's `this` broke the prepared state — every authored-UI
    // listener silently failed with asCONTEXT_NOT_PREPARED on Execute (the
    // Accessibility Demo "UI does nothing" bug, 2026-08-08). Unwrap delegates:
    // prepare the underlying method and set the delegate's OWN object.
    asIScriptFunction* func = listener.callback;
    asIScriptObject* callObj = listener.object;
    if (func->GetFuncType() == asFUNC_DELEGATE) {
        callObj = static_cast<asIScriptObject*>(func->GetDelegateObject());
        func = func->GetDelegateFunction();
    }

    int pr = ctx->Prepare(func);
    if (pr < 0) {
        ENJIN_LOG_ERROR(Script, "Event listener prepare failed for '%s' (err=%d, fn=%s)",
            eventName.c_str(), pr, func->GetName() ? func->GetName() : "?");
        m_ScriptEngine->ReturnContext(ctx);
        return;
    }

    if (callObj) {
        ctx->SetObject(callObj);
    }

    // Pass event name as first argument (if the callback accepts it)
    if (func->GetParamCount() >= 1) {
        // First param is the event name string
        ctx->SetArgObject(0, const_cast<std::string*>(&eventName));
    }

    // Expose the payload to Events_Current* bindings for the duration of the
    // callback. Save/restore handles nested dispatch (event firing an event).
    const EventData* prevData = s_CurrentDispatchData;
    s_CurrentDispatchData = &data;

    int r = ctx->Execute();
    if (r == asEXECUTION_EXCEPTION) {
        ENJIN_LOG_ERROR(Script, "Event callback exception for '%s': %s",
            eventName.c_str(), ctx->GetExceptionString());
    }

    s_CurrentDispatchData = prevData;

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
