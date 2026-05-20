// NetworkAddress helpers — kept in this file for backward compatibility.
// The transport implementation has moved to UDPTransport.cpp.
#include "Enjin/Networking/NetworkTypes.h"

#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <WinSock2.h>
    #include <WS2tcpip.h>
#else
    #include <arpa/inet.h>
#endif

namespace Enjin {
namespace Networking {

u32 NetworkAddress::ParseIP(const std::string& ipStr) {
    struct in_addr addr;
    if (inet_pton(AF_INET, ipStr.c_str(), &addr) != 1) {
        addr.s_addr = 0;
    }
    return addr.s_addr;
}

std::string NetworkAddress::IPToString(u32 ip) {
    struct in_addr addr;
    addr.s_addr = ip;
    char buf[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &addr, buf, sizeof(buf));
    return std::string(buf);
}

} // namespace Networking
} // namespace Enjin
