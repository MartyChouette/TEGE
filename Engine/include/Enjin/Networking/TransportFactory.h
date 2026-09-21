#pragma once

#include "Enjin/Networking/INetworkTransport.h"
#include "Enjin/Networking/UDPTransport.h"
#include "Enjin/Networking/WebSocketTransport.h"
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
#ifdef ENJIN_PLATFORM_WEB
            // A browser still has NO network transport, and this returns
            // nullptr rather than something that cannot work.
            //
            // WebSocketTransportWeb exists and did compile and link for web,
            // but browser networking is PARKED (2026-09-20) and it is compiled
            // out. What was never demonstrated is a browser completing a game
            // join against a live host, and shipping a transport on the
            // strength of "it compiles" is how a silent absence gets made.
            return nullptr;
#else
            // Desktop: a real WebSocket server (and a client, opened lazily on
            // the first SendTo). Proven against an independent RFC 6455 client
            // and against headless Chrome, so a host CAN serve a browser -- it
            // is the browser end that is parked.
            //
            // ws:// only, not wss://. A page served over HTTPS refuses a ws://
            // connection as mixed content, so this means a LAN or a developer
            // machine. TLS and NAT traversal are the relay's job in step 4.
            return std::make_unique<WebSocketTransport>();
#endif
        default:
            return nullptr;
    }
}

} // namespace Networking
} // namespace Enjin
