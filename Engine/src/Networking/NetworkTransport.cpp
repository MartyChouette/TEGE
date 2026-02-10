#include "Enjin/Networking/NetworkTransport.h"
#include "Enjin/Logging/Log.h"

#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <WinSock2.h>
    #include <WS2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    using SocketType = SOCKET;
    static constexpr SocketType INVALID_SOCK = INVALID_SOCKET;
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <errno.h>
    using SocketType = int;
    static constexpr SocketType INVALID_SOCK = -1;
#endif

namespace Enjin {
namespace Networking {

// ============================================================================
// NetworkAddress helpers
// ============================================================================

u32 NetworkAddress::ParseIP(const std::string& ipStr) {
    struct in_addr addr;
#ifdef _WIN32
    addr.s_addr = inet_addr(ipStr.c_str());
#else
    inet_pton(AF_INET, ipStr.c_str(), &addr);
#endif
    return addr.s_addr;
}

std::string NetworkAddress::IPToString(u32 ip) {
    struct in_addr addr;
    addr.s_addr = ip;
#ifdef _WIN32
    return std::string(inet_ntoa(addr));
#else
    char buf[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &addr, buf, sizeof(buf));
    return std::string(buf);
#endif
}

// ============================================================================
// NetworkTransport
// ============================================================================

NetworkTransport::~NetworkTransport() {
    Close();
}

bool NetworkTransport::Bind(u16 port) {
    if (m_Open) Close();

#ifdef _WIN32
    if (!m_WsaInitialized) {
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            ENJIN_LOG_ERROR(Network, "NetworkTransport: WSAStartup failed");
            return false;
        }
        m_WsaInitialized = true;
    }
#endif

    SocketType sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCK) {
        ENJIN_LOG_ERROR(Network, "NetworkTransport: Failed to create socket");
        return false;
    }

    // Set non-blocking
#ifdef _WIN32
    u_long mode = 1;
    if (ioctlsocket(sock, FIONBIO, &mode) != 0) {
        ENJIN_LOG_ERROR(Network, "NetworkTransport: Failed to set non-blocking");
        closesocket(sock);
        return false;
    }
#else
    int flags = fcntl(sock, F_GETFL, 0);
    if (fcntl(sock, F_SETFL, flags | O_NONBLOCK) < 0) {
        ENJIN_LOG_ERROR(Network, "NetworkTransport: Failed to set non-blocking");
        close(sock);
        return false;
    }
#endif

    // Bind
    struct sockaddr_in addr {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sock, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) != 0) {
        ENJIN_LOG_ERROR(Network, "NetworkTransport: Failed to bind to port %u", port);
#ifdef _WIN32
        closesocket(sock);
#else
        close(sock);
#endif
        return false;
    }

    // Get actual bound port (if 0 was passed)
    struct sockaddr_in boundAddr {};
    int addrLen = sizeof(boundAddr);
#ifdef _WIN32
    getsockname(sock, reinterpret_cast<struct sockaddr*>(&boundAddr), &addrLen);
#else
    socklen_t sockLen = sizeof(boundAddr);
    getsockname(sock, reinterpret_cast<struct sockaddr*>(&boundAddr), &sockLen);
#endif
    m_BoundPort = ntohs(boundAddr.sin_port);

    m_Socket = static_cast<u64>(sock);
    m_Open = true;

    ENJIN_LOG_INFO(Network, "NetworkTransport: Bound to port %u", m_BoundPort);
    return true;
}

bool NetworkTransport::SendTo(const NetworkAddress& addr, const u8* data, u32 size) {
    if (!m_Open) return false;

    struct sockaddr_in destAddr {};
    destAddr.sin_family = AF_INET;
    destAddr.sin_port = htons(addr.port);
    destAddr.sin_addr.s_addr = addr.ip;

    SocketType sock = static_cast<SocketType>(m_Socket);
    i32 sent = sendto(sock, reinterpret_cast<const char*>(data), static_cast<int>(size), 0,
                      reinterpret_cast<struct sockaddr*>(&destAddr), sizeof(destAddr));
    return sent > 0;
}

i32 NetworkTransport::ReceiveFrom(NetworkAddress& sender, u8* buffer, u32 bufferSize) {
    if (!m_Open) return -1;

    struct sockaddr_in fromAddr {};
#ifdef _WIN32
    int fromLen = sizeof(fromAddr);
#else
    socklen_t fromLen = sizeof(fromAddr);
#endif

    SocketType sock = static_cast<SocketType>(m_Socket);
    i32 received = recvfrom(sock, reinterpret_cast<char*>(buffer), static_cast<int>(bufferSize), 0,
                            reinterpret_cast<struct sockaddr*>(&fromAddr), &fromLen);

    if (received > 0) {
        sender.ip = fromAddr.sin_addr.s_addr;
        sender.port = ntohs(fromAddr.sin_port);
        return received;
    }

#ifdef _WIN32
    int err = WSAGetLastError();
    if (err == WSAEWOULDBLOCK) return 0;
#else
    if (errno == EAGAIN || errno == EWOULDBLOCK) return 0;
#endif

    return -1;
}

void NetworkTransport::Close() {
    if (!m_Open) return;

    SocketType sock = static_cast<SocketType>(m_Socket);
#ifdef _WIN32
    closesocket(sock);
#else
    close(sock);
#endif

    m_Open = false;
    m_Socket = 0;
    m_BoundPort = 0;

#ifdef _WIN32
    if (m_WsaInitialized) {
        WSACleanup();
        m_WsaInitialized = false;
    }
#endif

    ENJIN_LOG_INFO(Network, "NetworkTransport: Socket closed");
}

} // namespace Networking
} // namespace Enjin
