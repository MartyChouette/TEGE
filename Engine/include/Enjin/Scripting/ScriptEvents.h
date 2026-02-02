#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>

class asIScriptFunction;
class asIScriptObject;

namespace Enjin {
namespace Scripting {

class ScriptEngine;

// Event data payload — key/value pairs that scripts can send/receive
struct EventData {
    std::unordered_map<std::string, f32> floats;
    std::unordered_map<std::string, i32> ints;
    std::unordered_map<std::string, std::string> strings;
    std::unordered_map<std::string, u64> entities;

    void SetFloat(const std::string& key, f32 value) { floats[key] = value; }
    f32 GetFloat(const std::string& key, f32 defaultVal = 0.0f) const {
        auto it = floats.find(key);
        return it != floats.end() ? it->second : defaultVal;
    }

    void SetInt(const std::string& key, i32 value) { ints[key] = value; }
    i32 GetInt(const std::string& key, i32 defaultVal = 0) const {
        auto it = ints.find(key);
        return it != ints.end() ? it->second : defaultVal;
    }

    void SetString(const std::string& key, const std::string& value) { strings[key] = value; }
    std::string GetString(const std::string& key, const std::string& defaultVal = "") const {
        auto it = strings.find(key);
        return it != strings.end() ? it->second : defaultVal;
    }

    void SetEntity(const std::string& key, u64 value) { entities[key] = value; }
    u64 GetEntity(const std::string& key, u64 defaultVal = 0) const {
        auto it = entities.find(key);
        return it != entities.end() ? it->second : defaultVal;
    }
};

// Listener entry
struct EventListener {
    asIScriptObject* object = nullptr;   // Script object that registered the listener
    asIScriptFunction* callback = nullptr; // Callback function
    u64 entityId = 0;                     // Owning entity
    u32 id = 0;                           // Unique listener ID
};

// Event bus for script-to-script communication
// Scripts can Listen for named events and Send/Broadcast them.
class ENJIN_API ScriptEventBus {
public:
    ScriptEventBus();
    ~ScriptEventBus();

    void SetScriptEngine(ScriptEngine* engine) { m_ScriptEngine = engine; }

    // Register a listener for a named event
    u32 Listen(const std::string& eventName, asIScriptObject* obj, asIScriptFunction* callback, u64 entityId);

    // Remove a listener by ID
    void RemoveListener(u32 id);

    // Remove all listeners for an entity
    void RemoveAllForEntity(u64 entityId);

    // Send an event (dispatches to all listeners for that event name)
    void Send(const std::string& eventName, const EventData& data);

    // Broadcast an event to ALL listeners regardless of event name
    void Broadcast(const EventData& data);

    // Clear all listeners
    void Clear();

private:
    void DispatchToListener(EventListener& listener, const std::string& eventName, const EventData& data);

    ScriptEngine* m_ScriptEngine = nullptr;
    std::unordered_map<std::string, std::vector<EventListener>> m_Listeners;
    u32 m_NextId = 1;
};

} // namespace Scripting
} // namespace Enjin
