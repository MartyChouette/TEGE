#include "Enjin/GUI/UIEvents.h"

#include <algorithm>

namespace Enjin::GUI {

u32 UIEventBus::Listen(const std::string& eventName, Callback callback) {
    Listener listener;
    listener.id = m_NextListenerId++;
    listener.eventName = eventName;
    listener.callback = std::move(callback);
    m_Listeners.push_back(std::move(listener));
    return listener.id;
}

void UIEventBus::RemoveListener(u32 listenerId) {
    m_Listeners.erase(
        std::remove_if(m_Listeners.begin(), m_Listeners.end(),
            [listenerId](const Listener& l) { return l.id == listenerId; }),
        m_Listeners.end()
    );
}

void UIEventBus::Dispatch(const UIEventData& event) {
    if (event.eventName.empty()) return;

    if (m_Forwarder) m_Forwarder(event);

    for (const auto& listener : m_Listeners) {
        if (listener.eventName == event.eventName) {
            listener.callback(event);
        }
    }
}

void UIEventBus::Clear() {
    m_Listeners.clear();
    m_NextListenerId = 1;
}

} // namespace Enjin::GUI
