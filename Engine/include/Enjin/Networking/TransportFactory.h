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
            // NOT IMPLEMENTED. This returns nullptr on web, which means a browser
            // build has NO network transport at all -- web multiplayer does not
            // work, it is not merely limited. Said plainly here because the
            // enum above and INetworkTransport's header both describe WebSocket
            // as the NAT-traversal path, which reads as though it exists.
            // Planned in adr-0007 (hosted relay + matchmaking); until that lands,
            // multiplayer is desktop UDP on a LAN or a port-forwarded direct IP.
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
