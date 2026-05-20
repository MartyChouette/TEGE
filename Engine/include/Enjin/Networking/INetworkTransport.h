#pragma once

#include "Enjin/Networking/NetworkTypes.h"

namespace Enjin {
namespace Networking {

// Abstract transport interface — implementations provide the actual socket layer.
// UDP (default desktop), WebSocket (web builds / NAT traversal), or custom.
class INetworkTransport {
public:
    virtual ~INetworkTransport() = default;

    // Bind to a local port (0 = OS picks). Returns true on success.
    virtual bool Bind(u16 port) = 0;

    // Send data to a remote address. Returns true on success.
    virtual bool SendTo(const NetworkAddress& addr, const u8* data, u32 size) = 0;

    // Non-blocking receive. Returns bytes received, 0 if none, -1 on error.
    virtual i32 ReceiveFrom(NetworkAddress& sender, u8* buffer, u32 bufferSize) = 0;

    // Close the transport
    virtual void Close() = 0;

    virtual bool IsOpen() const = 0;
    virtual u16 GetBoundPort() const = 0;
};

} // namespace Networking
} // namespace Enjin
