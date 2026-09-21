#pragma once

// The browser half of adr-0007 Track B step 3.
//
// WHY THIS IS A SEPARATE CLASS, not an #ifdef inside WebSocketTransport:
// almost nothing is shared. WebSocketTransport is a BSD-socket server that
// implements RFC 6455 itself -- accept, handshake, masking, frame codec. A
// browser has none of that to do, because the JS WebSocket object already did
// it: the engine hands over bytes and gets messages back. Folding the two
// together would mean an #ifdef around every member as well as every function,
// and the shared surface would be the class name.
//
// A browser also cannot LISTEN. There is no accept, no server mode, and no way
// to host from a page -- a browser can only ever dial out. `Bind(port)` with a
// non-zero port therefore FAILS rather than quietly behaving like Bind(0): a
// web build that calls HostGame should find out, not host a game nobody can
// reach and report success.
//
// Reachability: a page served over HTTPS may not open a ws:// connection
// (mixed content), so this reaches a plain-HTTP host on a LAN or a dev machine
// today. wss:// through the relay is step 4, and this class needs no change
// for it -- only the URL does.

#include "Enjin/Networking/INetworkTransport.h"
#include <deque>
#include <string>
#include <vector>

namespace Enjin {
namespace Networking {

class ENJIN_API WebSocketTransportWeb : public INetworkTransport {
public:
    WebSocketTransportWeb() = default;
    ~WebSocketTransportWeb() override;

    // port must be 0. A browser cannot listen; see the header comment.
    bool Bind(u16 port) override;

    // Opens a connection to `addr` on first use and queues until the browser
    // reports it open. Same shape NetworkSystem::JoinGame already relies on.
    bool SendTo(const NetworkAddress& addr, const u8* data, u32 size) override;

    // Pops one message delivered by the browser's onmessage callback. Never
    // blocks: the browser fills the inbox between frames, not during this call.
    i32 ReceiveFrom(NetworkAddress& sender, u8* buffer, u32 bufferSize) override;

    void Close() override;
    bool IsOpen() const override { return m_Open; }
    u16 GetBoundPort() const override { return 0; }

private:
    struct Socket {
        i32 handle = 0;              // EMSCRIPTEN_WEBSOCKET_T
        NetworkAddress address;
        bool open = false;
        std::deque<std::vector<u8>> pendingOut;
    };

    Socket* FindByAddress(const NetworkAddress& addr);
    Socket* FindByHandle(i32 handle);
    void FlushPending(Socket& s);

public:
    // Entry points for the C callback thunks, which are free functions and so
    // cannot reach private members. Public because the browser calls in from
    // outside the object; not part of the transport interface and not for
    // anyone else to call.
    static void OnOpen(i32 handle, void* user);
    static void OnMessage(i32 handle, const u8* data, u32 size, bool isText, void* user);
    static void OnClose(i32 handle, void* user);

private:

    std::vector<Socket> m_Sockets;
    std::deque<std::pair<NetworkAddress, std::vector<u8>>> m_Inbox;
    bool m_Open = false;
};

} // namespace Networking
} // namespace Enjin
