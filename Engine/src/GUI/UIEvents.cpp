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

    // Over a copy of the matching callbacks: a listener may add or remove
    // listeners (a screen that closes itself from its own Back handler), and
    // either would invalidate an iterator into m_Listeners
    std::vector<Callback> matching;
    for (const auto& listener : m_Listeners) {
        if (listener.eventName == event.eventName) matching.push_back(listener.callback);
    }
    for (const auto& cb : matching) cb(event);
}

void UIEventBus::Clear() {
    m_Listeners.clear();
    m_NextListenerId = 1;
}

} // namespace Enjin::GUI
