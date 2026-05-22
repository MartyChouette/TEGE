#include "Enjin/Editor/EditorProtocol.h"
#include <cstring>
#include <chrono>

namespace Enjin {
namespace Editor {

EditorMessage EditorMessage::MakeString(EditorMessageType type, const std::string& data, u64 seqId) {
    EditorMessage msg;
    msg.header.type = static_cast<u16>(type);
    msg.header.flags = 0;
    msg.header.payloadSize = static_cast<u32>(data.size());
    msg.header.sequenceId = seqId;
    msg.header.timestamp = static_cast<u64>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch()).count());
    msg.payload.assign(data.begin(), data.end());
    return msg;
}

EditorMessage EditorMessage::MakeJson(EditorMessageType type, const std::string& json, u64 seqId) {
    return MakeString(type, json, seqId);
}

std::string EditorMessage::PayloadAsString() const {
    return std::string(payload.begin(), payload.end());
}

void EditorMessageDispatcher::RegisterHandler(EditorMessageType type, MessageHandler handler) {
    m_Handlers[static_cast<u16>(type)].push_back(std::move(handler));
}

void EditorMessageDispatcher::DispatchAll(IEditorTransport& transport) {
    EditorMessage msg;
    while (transport.Receive(msg)) {
        auto it = m_Handlers.find(msg.header.type);
        if (it != m_Handlers.end()) {
            for (auto& handler : it->second) {
                handler(msg);
            }
        }
    }
}

} // namespace Editor
} // namespace Enjin
