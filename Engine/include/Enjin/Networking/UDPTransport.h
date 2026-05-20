#pragma once

#include "Enjin/Networking/INetworkTransport.h"

namespace Enjin {
namespace Networking {

// Cross-platform non-blocking UDP socket transport
class UDPTransport : public INetworkTransport {
public:
    UDPTransport() = default;
    ~UDPTransport() override;

    bool Bind(u16 port) override;
    bool SendTo(const NetworkAddress& addr, const u8* data, u32 size) override;
    i32 ReceiveFrom(NetworkAddress& sender, u8* buffer, u32 bufferSize) override;
    void Close() override;
    bool IsOpen() const override { return m_Open; }
    u16 GetBoundPort() const override { return m_BoundPort; }

private:
    u64 m_Socket = 0;
    u16 m_BoundPort = 0;
    bool m_Open = false;
    bool m_WsaInitialized = false;
};

} // namespace Networking
} // namespace Enjin
