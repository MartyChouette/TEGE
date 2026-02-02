#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/ECS/Entity.h"

#include <string>
#include <vector>
#include <functional>
#include <unordered_map>

namespace Enjin::GUI {

struct UIEventData {
    u32 elementId = 0;
    ECS::Entity canvasEntity = 0;
    std::string eventName;

    // Payload (caller sets whichever fields are relevant)
    f32 floatValue  = 0.0f;
    i32 intValue    = 0;
    bool boolValue  = false;
    std::string stringValue;
};

class ENJIN_API UIEventBus {
public:
    using Callback = std::function<void(const UIEventData&)>;

    // Listen for a named event, returns listener ID
    u32 Listen(const std::string& eventName, Callback callback);

    // Remove a listener by ID
    void RemoveListener(u32 listenerId);

    // Dispatch an event to all listeners for that event name
    void Dispatch(const UIEventData& event);

    // Clear all listeners
    void Clear();

private:
    struct Listener {
        u32 id = 0;
        std::string eventName;
        Callback callback;
    };

    std::vector<Listener> m_Listeners;
    u32 m_NextListenerId = 1;
};

} // namespace Enjin::GUI
