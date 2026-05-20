#pragma once

#include "Enjin/Networking/INetworkTransport.h"
#include "Enjin/Networking/UDPTransport.h"
#include <memory>

namespace Enjin {
namespace Networking {

enum class TransportType : u8 {
    UDP,        // Direct UDP sockets (desktop, LAN)
    WebSocket,  // WebSocket framed (web builds, NAT traversal)
    Auto        // UDP on desktop, WebSocket on web
};

// Create a transport of the requested type.
// Auto selects UDP on desktop platforms, WebSocket on web (Emscripten).
inline std::unique_ptr<INetworkTransport> CreateTransport(TransportType type = TransportType::Auto) {
    if (type == TransportType::Auto) {
#ifdef ENJIN_PLATFORM_WEB
        type = TransportType::WebSocket;
#else
        type = TransportType::UDP;
#endif
    }

    switch (type) {
        case TransportType::UDP:
            return std::make_unique<UDPTransport>();
        case TransportType::WebSocket:
            // WebSocket transport will be implemented in a follow-up.
            // For now, fall back to UDP on desktop, nullptr on web.
#ifdef ENJIN_PLATFORM_WEB
            return nullptr;  // TODO: WebSocketTransport
#else
            return std::make_unique<UDPTransport>();
#endif
        default:
            return nullptr;
    }
}

} // namespace Networking
} // namespace Enjin
