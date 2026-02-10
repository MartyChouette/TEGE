#pragma once

#include "Enjin/Networking/NetworkTypes.h"

namespace Enjin {
namespace Networking {

// Cross-platform non-blocking UDP socket wrapper
class NetworkTransport {
public:
    NetworkTransport() = default;
    ~NetworkTransport();

    // Bind to a local port (0 = OS picks). Returns true on success.
    bool Bind(u16 port);

    // Send data to a remote address. Returns true on success.
    bool SendTo(const NetworkAddress& addr, const u8* data, u32 size);

    // Non-blocking receive. Returns bytes received, 0 if none, -1 on error.
    // sender is filled with the source address.
    i32 ReceiveFrom(NetworkAddress& sender, u8* buffer, u32 bufferSize);

    // Close the socket
    void Close();

    bool IsOpen() const { return m_Open; }
    u16 GetBoundPort() const { return m_BoundPort; }

private:
    // Use u64 to hold both SOCKET (Windows, unsigned pointer) and int (POSIX)
    u64 m_Socket = 0;
    u16 m_BoundPort = 0;
    bool m_Open = false;
    bool m_WsaInitialized = false;
};

} // namespace Networking
} // namespace Enjin
