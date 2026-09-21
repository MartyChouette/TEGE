#include "Enjin/Networking/WebSocketTransport.h"
#include "Enjin/Logging/Log.h"
#include "Enjin/Encoding/Base64.h"
#include <atomic>
#include <cstddef>
#include <cstring>
#include <random>

#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <WinSock2.h>
    #include <WS2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    using SocketType = SOCKET;
    static constexpr SocketType INVALID_SOCK = INVALID_SOCKET;
    #define ENJIN_WOULD_BLOCK (WSAGetLastError() == WSAEWOULDBLOCK)
    #define ENJIN_IN_PROGRESS (WSAGetLastError() == WSAEWOULDBLOCK)
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <netinet/tcp.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <errno.h>
    using SocketType = int;
    static constexpr SocketType INVALID_SOCK = -1;
    #define ENJIN_WOULD_BLOCK (errno == EAGAIN || errno == EWOULDBLOCK)
    #define ENJIN_IN_PROGRESS (errno == EINPROGRESS)
#endif

namespace Enjin {
namespace Networking {

namespace {

#ifdef _WIN32
std::atomic<int> s_WsaRefCount{0};
#endif

constexpr usize kReadChunk = 8192;

// A connection that has buffered this much without yielding a frame is not
// making progress. The frame cap already bounds one message; this bounds the
// SUM of a header plus a partial payload plus whatever else is queued, so a
// peer cannot dribble bytes forever and hold the memory open.
constexpr usize kMaxConnectionBuffer = WebSocket::kMaxFramePayload + 64u * 1024u;

// Likewise for the HTTP handshake, which is text and arrives before anything
// has been authenticated at all.
constexpr usize kMaxHandshakeBytes = 16u * 1024u;

void SetNonBlocking(SocketType s) {
#ifdef _WIN32
    u_long mode = 1;
    ioctlsocket(s, FIONBIO, &mode);
#else
    const int flags = fcntl(s, F_GETFL, 0);
    fcntl(s, F_SETFL, flags | O_NONBLOCK);
#endif
}

void DisableNagle(SocketType s) {
    // Game traffic is small and latency-sensitive, and Nagle exists to coalesce
    // exactly that into fewer, later packets. Leaving it on adds up to 40ms to
    // a message for no benefit anyone here wants.
    int one = 1;
    setsockopt(s, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&one), sizeof(one));
}

void CloseSocket(SocketType s) {
#ifdef _WIN32
    closesocket(s);
#else
    close(s);
#endif
}

std::string RandomClientKey() {
    // 16 random bytes, base64'd, per RFC 6455. Its only job is to make the
    // server's accept value unpredictable, so it must not be a constant.
    static thread_local std::mt19937 rng{std::random_device{}()};
    u8 nonce[16];
    for (u8& b : nonce) b = static_cast<u8>(rng() & 0xFF);
    return Encoding::Base64Encode(nonce, 16);
}

} // namespace

WebSocketTransport::~WebSocketTransport() {
    Close();
}

// ---------------------------------------------------------------------------
// LIFECYCLE
// ---------------------------------------------------------------------------

bool WebSocketTransport::Bind(u16 port) {
    if (m_Open) Close();

#ifdef _WIN32
    if (!m_WsaInitialized) {
        if (s_WsaRefCount.fetch_add(1) == 0) {
            WSADATA wsa;
            if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
                s_WsaRefCount.fetch_sub(1);
                ENJIN_LOG_ERROR(Network, "WebSocketTransport: WSAStartup failed");
                return false;
            }
        }
        m_WsaInitialized = true;
    }
#endif

    // Client mode: no listen socket. Connections are opened by SendTo.
    if (port == 0) {
        m_Listening = false;
        m_BoundPort = 0;
        m_Open = true;
        return true;
    }

    const SocketType sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCK) {
        ENJIN_LOG_ERROR(Network, "WebSocketTransport: failed to create listen socket");
        return false;
    }

    // Without SO_REUSEADDR a restarted server cannot re-bind its own port until
    // the kernel's TIME_WAIT expires, which reads as "port already in use" for
    // a couple of minutes after a clean shutdown.
    int reuse = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,
               reinterpret_cast<const char*>(&reuse), sizeof(reuse));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        ENJIN_LOG_ERROR(Network, "WebSocketTransport: failed to bind port %u", port);
        CloseSocket(sock);
        return false;
    }
    if (listen(sock, 16) != 0) {
        ENJIN_LOG_ERROR(Network, "WebSocketTransport: failed to listen on port %u", port);
        CloseSocket(sock);
        return false;
    }

    SetNonBlocking(sock);
    m_ListenSocket = static_cast<u64>(sock);
    m_Listening = true;
    m_BoundPort = port;
    m_Open = true;
    ENJIN_LOG_INFO(Network, "WebSocketTransport: listening on ws://0.0.0.0:%u", port);
    return true;
}

void WebSocketTransport::Close() {
    for (Connection& c : m_Connections) {
        if (!c.dead) CloseConnection(c);
    }
    m_Connections.clear();
    m_Inbox.clear();

    if (m_Listening) {
        CloseSocket(static_cast<SocketType>(m_ListenSocket));
        m_Listening = false;
    }
    m_ListenSocket = 0;
    m_BoundPort = 0;
    m_Open = false;

#ifdef _WIN32
    if (m_WsaInitialized) {
        if (s_WsaRefCount.fetch_sub(1) == 1) WSACleanup();
        m_WsaInitialized = false;
    }
#endif
}

usize WebSocketTransport::GetConnectionCount() const {
    usize n = 0;
    for (const Connection& c : m_Connections) {
        if (!c.dead && c.handshakeDone) n++;
    }
    return n;
}

// ---------------------------------------------------------------------------
// CONNECTIONS
// ---------------------------------------------------------------------------

WebSocketTransport::Connection* WebSocketTransport::FindConnection(const NetworkAddress& addr) {
    for (Connection& c : m_Connections) {
        if (!c.dead && c.address == addr) return &c;
    }
    return nullptr;
}

WebSocketTransport::Connection* WebSocketTransport::OpenClientConnection(const NetworkAddress& addr) {
    const SocketType sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCK) return nullptr;

    sockaddr_in dest{};
    dest.sin_family = AF_INET;
    dest.sin_port = htons(addr.port);
    dest.sin_addr.s_addr = addr.ip;

    // Connect BEFORE going non-blocking. A non-blocking connect returns
    // EINPROGRESS and then needs a writability check to find out whether it
    // actually succeeded; doing it blocking here costs one round trip at join
    // time and removes a whole state from the machine below. The socket goes
    // non-blocking immediately after, which is what the pump needs.
    if (connect(sock, reinterpret_cast<sockaddr*>(&dest), sizeof(dest)) != 0) {
        ENJIN_LOG_WARN(Network, "WebSocketTransport: could not connect to %s:%u",
                       NetworkAddress::IPToString(addr.ip).c_str(), addr.port);
        CloseSocket(sock);
        return nullptr;
    }
    SetNonBlocking(sock);
    DisableNagle(sock);

    Connection c;
    c.socket = static_cast<u64>(sock);
    c.address = addr;
    c.isServerSide = false;
    c.clientKey = RandomClientKey();

    // The opening handshake. Host is required by the RFC; the rest is the
    // minimum a server needs to answer.
    const std::string req =
        "GET / HTTP/1.1\r\n"
        "Host: " + NetworkAddress::IPToString(addr.ip) + ":" + std::to_string(addr.port) + "\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: " + c.clientKey + "\r\n"
        "Sec-WebSocket-Version: 13\r\n"
        "\r\n";

    m_Connections.push_back(std::move(c));
    Connection& ref = m_Connections.back();
    if (!SendRaw(ref, reinterpret_cast<const u8*>(req.data()), req.size())) {
        CloseConnection(ref);
        return nullptr;
    }
    return &ref;
}

void WebSocketTransport::AcceptPending() {
    if (!m_Listening) return;

    // Bounded per pump, like the UDP receive loop: a flood of connects must not
    // turn into an unbounded frame.
    for (int i = 0; i < 16; i++) {
        sockaddr_in peer{};
#ifdef _WIN32
        int peerLen = sizeof(peer);
#else
        socklen_t peerLen = sizeof(peer);
#endif
        const SocketType s = accept(static_cast<SocketType>(m_ListenSocket),
                                    reinterpret_cast<sockaddr*>(&peer), &peerLen);
        if (s == INVALID_SOCK) break;

        SetNonBlocking(s);
        DisableNagle(s);

        Connection c;
        c.socket = static_cast<u64>(s);
        // The TCP peer address IS the datagram address everything above uses.
        // It is unique per connection and stable for its lifetime.
        c.address.ip = peer.sin_addr.s_addr;
        c.address.port = ntohs(peer.sin_port);
        c.isServerSide = true;
        m_Connections.push_back(std::move(c));
    }
}

void WebSocketTransport::CloseConnection(Connection& c) {
    if (c.dead) return;
    CloseSocket(static_cast<SocketType>(c.socket));
    c.dead = true;
    c.pendingOut.clear();
    c.inBuffer.clear();
    c.handshakeBuffer.clear();
}

void WebSocketTransport::ReapDead() {
    for (usize i = m_Connections.size(); i-- > 0; ) {
        if (m_Connections[i].dead) {
            m_Connections.erase(m_Connections.begin() + static_cast<std::ptrdiff_t>(i));
        }
    }
}

// ---------------------------------------------------------------------------
// I/O
// ---------------------------------------------------------------------------

bool WebSocketTransport::SendRaw(Connection& c, const u8* data, usize size) {
    usize sent = 0;
    while (sent < size) {
        const int n = send(static_cast<SocketType>(c.socket),
                           reinterpret_cast<const char*>(data + sent),
                           static_cast<int>(size - sent), 0);
        if (n > 0) { sent += static_cast<usize>(n); continue; }
        if (n < 0 && ENJIN_WOULD_BLOCK) {
            // The kernel buffer is full. Spinning here would block the frame,
            // and a game that cannot keep up with its own send rate has a
            // problem this cannot paper over: drop the message and say so.
            ENJIN_LOG_WARN(Network, "WebSocketTransport: send buffer full, dropped %zu bytes",
                           size - sent);
            return false;
        }
        CloseConnection(c);
        return false;
    }
    return true;
}

bool WebSocketTransport::SendFrame(Connection& c, const u8* data, usize size) {
    // A server must NOT mask and a client MUST. A browser closes the connection
    // on a masked server frame rather than tolerating it.
    const std::vector<u8> framed =
        WebSocket::EncodeFrame(WebSocket::Opcode::Binary, data, size, !c.isServerSide);
    return SendRaw(c, framed.data(), framed.size());
}

void WebSocketTransport::ReadIntoBuffer(Connection& c) {
    u8 chunk[kReadChunk];
    for (int i = 0; i < 16 && !c.dead; i++) {
        const int n = recv(static_cast<SocketType>(c.socket),
                           reinterpret_cast<char*>(chunk), static_cast<int>(sizeof(chunk)), 0);
        if (n > 0) {
            if (!c.handshakeDone) {
                if (c.handshakeBuffer.size() + static_cast<usize>(n) > kMaxHandshakeBytes) {
                    ENJIN_LOG_WARN(Network, "WebSocketTransport: oversized handshake, dropping peer");
                    CloseConnection(c);
                    return;
                }
                c.handshakeBuffer.append(reinterpret_cast<const char*>(chunk), static_cast<usize>(n));
            } else {
                if (c.inBuffer.size() + static_cast<usize>(n) > kMaxConnectionBuffer) {
                    ENJIN_LOG_WARN(Network, "WebSocketTransport: peer buffered past the cap, dropping");
                    CloseConnection(c);
                    return;
                }
                c.inBuffer.insert(c.inBuffer.end(), chunk, chunk + n);
            }
            if (static_cast<usize>(n) < sizeof(chunk)) return;   // drained
            continue;
        }
        if (n == 0) { CloseConnection(c); return; }              // peer closed
        if (ENJIN_WOULD_BLOCK) return;                            // nothing more
        CloseConnection(c);
        return;
    }
}

void WebSocketTransport::AdvanceHandshake(Connection& c) {
    if (!WebSocket::HandshakeIsComplete(c.handshakeBuffer)) return;

    if (c.isServerSide) {
        const WebSocket::HandshakeRequest req =
            WebSocket::ParseHandshakeRequest(c.handshakeBuffer);
        if (!req.valid) {
            // Answer with a real HTTP error rather than dropping the socket:
            // that is what makes a mistyped URL diagnosable from the browser's
            // own console instead of looking like the server being down.
            const std::string rejection = WebSocket::BuildHandshakeRejection();
            SendRaw(c, reinterpret_cast<const u8*>(rejection.data()), rejection.size());
            CloseConnection(c);
            return;
        }
        const std::string response = WebSocket::BuildHandshakeResponse(req.key);
        if (!SendRaw(c, reinterpret_cast<const u8*>(response.data()), response.size())) return;
    } else {
        // Client side: the server must have echoed the accept value derived
        // from OUR key. Anything else is not a WebSocket server, however
        // cheerfully it answered.
        const std::string expected = "Sec-WebSocket-Accept: " +
                                     WebSocket::ComputeAcceptKey(c.clientKey);
        if (c.handshakeBuffer.find("101") == std::string::npos ||
            c.handshakeBuffer.find(expected) == std::string::npos) {
            ENJIN_LOG_WARN(Network, "WebSocketTransport: handshake refused or malformed");
            CloseConnection(c);
            return;
        }
    }

    // Anything the peer sent after the blank line is already frames.
    const usize end = c.handshakeBuffer.find("\r\n\r\n") + 4;
    if (end < c.handshakeBuffer.size()) {
        const std::string rest = c.handshakeBuffer.substr(end);
        c.inBuffer.insert(c.inBuffer.end(), rest.begin(), rest.end());
    }
    c.handshakeBuffer.clear();
    c.handshakeDone = true;

    // Whatever was handed to SendTo while we were still shaking hands.
    while (!c.pendingOut.empty() && !c.dead) {
        const std::vector<u8>& p = c.pendingOut.front();
        SendFrame(c, p.data(), p.size());
        c.pendingOut.pop_front();
    }
}

void WebSocketTransport::DrainFrames(Connection& c) {
    usize offset = 0;
    while (!c.dead) {
        WebSocket::Frame f;
        usize consumed = 0;
        const WebSocket::DecodeResult r = WebSocket::DecodeFrame(
            c.inBuffer.data() + offset, c.inBuffer.size() - offset, f, consumed);

        if (r == WebSocket::DecodeResult::NeedMore) break;
        if (r == WebSocket::DecodeResult::Error) {
            ENJIN_LOG_WARN(Network, "WebSocketTransport: malformed frame, dropping peer");
            CloseConnection(c);
            return;
        }
        offset += consumed;

        switch (f.opcode) {
            case WebSocket::Opcode::Ping: {
                // Answer with the same payload, per the RFC. A browser that
                // pings an unanswering server eventually gives up on a
                // connection that is otherwise perfectly healthy.
                const std::vector<u8> pong = WebSocket::EncodeFrame(
                    WebSocket::Opcode::Pong, f.payload, !c.isServerSide);
                SendRaw(c, pong.data(), pong.size());
                break;
            }
            case WebSocket::Opcode::Pong:
                break;   // unsolicited pongs are legal and mean nothing to us
            case WebSocket::Opcode::Close:
                CloseConnection(c);
                return;

            case WebSocket::Opcode::Continuation: {
                if (!c.fragmented) {
                    ENJIN_LOG_WARN(Network, "WebSocketTransport: continuation with nothing to continue");
                    CloseConnection(c);
                    return;
                }
                if (c.fragment.size() + f.payload.size() > WebSocket::kMaxMessagePayload) {
                    ENJIN_LOG_WARN(Network, "WebSocketTransport: reassembled message over cap");
                    CloseConnection(c);
                    return;
                }
                c.fragment.insert(c.fragment.end(), f.payload.begin(), f.payload.end());
                if (f.fin) {
                    m_Inbox.emplace_back(c.address, std::move(c.fragment));
                    c.fragment.clear();
                    c.fragmented = false;
                }
                break;
            }

            case WebSocket::Opcode::Text:
            case WebSocket::Opcode::Binary: {
                if (f.fin) {
                    m_Inbox.emplace_back(c.address, std::move(f.payload));
                } else {
                    c.fragment = std::move(f.payload);
                    c.fragmentOpcode = f.opcode;
                    c.fragmented = true;
                }
                break;
            }
        }
    }

    if (offset > 0) {
        c.inBuffer.erase(c.inBuffer.begin(), c.inBuffer.begin() + static_cast<std::ptrdiff_t>(offset));
    }
}

void WebSocketTransport::ServiceConnection(Connection& c) {
    ReadIntoBuffer(c);
    if (c.dead) return;
    if (!c.handshakeDone) {
        AdvanceHandshake(c);
        if (c.dead || !c.handshakeDone) return;
    }
    DrainFrames(c);
}

// ---------------------------------------------------------------------------
// INetworkTransport
// ---------------------------------------------------------------------------

bool WebSocketTransport::SendTo(const NetworkAddress& addr, const u8* data, u32 size) {
    if (!m_Open) return false;

    Connection* c = FindConnection(addr);
    if (!c) {
        // Server mode never dials out: an address with no connection is a peer
        // that has gone, and inventing a connection to it would turn a
        // disconnect into a hang.
        if (m_Listening) return false;
        c = OpenClientConnection(addr);
        if (!c) return false;
    }

    if (!c->handshakeDone) {
        // Queue rather than fail. NetworkSystem::JoinGame sends its connection
        // request immediately after Bind, and failing it here would make every
        // join depend on the handshake having already completed -- which it
        // cannot have, since it is a round trip away.
        c->pendingOut.emplace_back(data, data + size);
        return true;
    }
    return SendFrame(*c, data, size);
}

i32 WebSocketTransport::ReceiveFrom(NetworkAddress& sender, u8* buffer, u32 bufferSize) {
    if (!m_Open) return -1;

    // Pump only when the inbox is empty, so a caller draining several messages
    // in one frame does not re-run accept and recv for each one.
    if (m_Inbox.empty()) {
        AcceptPending();
        for (usize i = 0; i < m_Connections.size(); i++) {
            if (!m_Connections[i].dead) ServiceConnection(m_Connections[i]);
        }
        ReapDead();
    }

    if (m_Inbox.empty()) return 0;

    auto& front = m_Inbox.front();
    if (front.second.size() > bufferSize) {
        // Dropped rather than truncated. Half a message is not a smaller
        // message; it deserializes into nonsense, and silently handing that up
        // is worse than losing it.
        ENJIN_LOG_WARN(Network, "WebSocketTransport: message of %zu bytes exceeds the %u-byte "
                                "receive buffer, dropped", front.second.size(), bufferSize);
        m_Inbox.pop_front();
        return 0;
    }

    sender = front.first;
    const usize n = front.second.size();
    std::memcpy(buffer, front.second.data(), n);
    m_Inbox.pop_front();
    return static_cast<i32>(n);
}

} // namespace Networking
} // namespace Enjin
