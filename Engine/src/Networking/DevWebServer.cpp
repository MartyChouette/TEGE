#include "Enjin/Networking/DevWebServer.h"
#include "Enjin/Platform/Paths.h"
#include "Enjin/Logging/Log.h"

#include <cstring>
#include <fstream>
#include <sstream>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
using SocketT = SOCKET;
static constexpr SocketT kInvalidSocket = INVALID_SOCKET;
static void CloseSock(SocketT s) { closesocket(s); }
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
using SocketT = int;
static constexpr SocketT kInvalidSocket = -1;
static void CloseSock(SocketT s) { close(s); }
#endif

namespace Enjin {
namespace Networking {

static const char* MimeTypeForPath(const std::string& path) {
    auto ends = [&](const char* ext) {
        size_t n = std::strlen(ext);
        return path.size() >= n &&
               path.compare(path.size() - n, n, ext) == 0;
    };
    if (ends(".html") || ends(".htm")) return "text/html; charset=utf-8";
    if (ends(".js"))    return "text/javascript; charset=utf-8";
    // application/wasm is REQUIRED: browsers only stream-compile with it,
    // and some refuse instantiation outright on a wrong type
    if (ends(".wasm"))  return "application/wasm";
    if (ends(".css"))   return "text/css; charset=utf-8";
    if (ends(".json"))  return "application/json";
    if (ends(".png"))   return "image/png";
    if (ends(".jpg") || ends(".jpeg")) return "image/jpeg";
    if (ends(".svg"))   return "image/svg+xml";
    if (ends(".ico"))   return "image/x-icon";
    if (ends(".ttf"))   return "font/ttf";
    if (ends(".mp3"))   return "audio/mpeg";
    if (ends(".ogg"))   return "audio/ogg";
    if (ends(".wav"))   return "audio/wav";
    return "application/octet-stream"; // .enjpak, .enjin, .bin, everything else
}

// Minimal %XX decoding (spaces in filenames); rejects embedded NUL
static std::string UrlDecode(const std::string& in) {
    std::string out;
    out.reserve(in.size());
    for (size_t i = 0; i < in.size(); ++i) {
        if (in[i] == '%' && i + 2 < in.size()) {
            auto hex = [](char c) -> int {
                if (c >= '0' && c <= '9') return c - '0';
                if (c >= 'a' && c <= 'f') return c - 'a' + 10;
                if (c >= 'A' && c <= 'F') return c - 'A' + 10;
                return -1;
            };
            int hi = hex(in[i + 1]), lo = hex(in[i + 2]);
            if (hi >= 0 && lo >= 0) {
                char c = static_cast<char>((hi << 4) | lo);
                if (c == '\0') return "";
                out.push_back(c);
                i += 2;
                continue;
            }
        }
        out.push_back(in[i]);
    }
    return out;
}

DevWebServer::~DevWebServer() {
    Stop();
}

u16 DevWebServer::Start(const std::string& rootDir, u16 preferredPort) {
    Stop();
    if (rootDir.empty()) return 0;

#ifdef _WIN32
    static bool s_WsaReady = [] {
        WSADATA d;
        return WSAStartup(MAKEWORD(2, 2), &d) == 0;
    }();
    if (!s_WsaReady) return 0;
#endif

    SocketT listenSock = kInvalidSocket;
    u16 boundPort = 0;
    for (u16 port = preferredPort; port < preferredPort + 20; ++port) {
        listenSock = socket(AF_INET, SOCK_STREAM, 0);
        if (listenSock == kInvalidSocket) return 0;

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        // Localhost ONLY — this is a preview server, never a LAN host
        inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

        if (bind(listenSock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0 &&
            listen(listenSock, 8) == 0) {
            boundPort = port;
            break;
        }
        CloseSock(listenSock);
        listenSock = kInvalidSocket;
    }
    if (boundPort == 0) {
        ENJIN_LOG_WARN(Network, "DevWebServer: no free port in %u-%u",
                       preferredPort, preferredPort + 19);
        return 0;
    }

    m_ListenSocket = static_cast<i64>(listenSock);
    m_Port = boundPort;
    m_Root = rootDir;
    m_Running.store(true, std::memory_order_relaxed);
    m_Thread = std::thread(&DevWebServer::Run, this);

    ENJIN_LOG_INFO(Network, "DevWebServer: serving %s at http://localhost:%u/",
                   rootDir.c_str(), boundPort);
    return boundPort;
}

void DevWebServer::Stop() {
    if (!m_Running.exchange(false, std::memory_order_relaxed)) {
        if (m_Thread.joinable()) m_Thread.join();
        return;
    }
    // Closing the listen socket unblocks accept() in the server thread
    if (m_ListenSocket >= 0) {
        CloseSock(static_cast<SocketT>(m_ListenSocket));
        m_ListenSocket = -1;
    }
    if (m_Thread.joinable()) m_Thread.join();
    m_Port = 0;
}

void DevWebServer::Run() {
    SocketT listenSock = static_cast<SocketT>(m_ListenSocket);

    while (m_Running.load(std::memory_order_relaxed)) {
        SocketT client = accept(listenSock, nullptr, nullptr);
        if (client == kInvalidSocket) {
            // Stop() closed the listen socket (or a transient error) — exit if stopping
            if (!m_Running.load(std::memory_order_relaxed)) break;
            continue;
        }

        // Read the request head (single recv is enough for a GET line + headers
        // from a local browser; anything longer is not a request we serve)
        char buf[4096];
        int n = recv(client, buf, sizeof(buf) - 1, 0);
        if (n <= 0) { CloseSock(client); continue; }
        buf[n] = '\0';

        std::string request(buf);
        std::string body;
        std::string status = "404 Not Found";
        const char* mime = "text/plain";
        bool ok = false;

        if (request.rfind("GET ", 0) == 0) {
            size_t pathEnd = request.find(' ', 4);
            std::string urlPath = (pathEnd != std::string::npos)
                ? request.substr(4, pathEnd - 4) : "/";
            size_t query = urlPath.find('?');
            if (query != std::string::npos) urlPath.resize(query);
            urlPath = UrlDecode(urlPath);
            if (urlPath.empty() || urlPath == "/") urlPath = "/index.html";
            while (!urlPath.empty() && urlPath.front() == '/') urlPath.erase(0, 1);

            std::string resolved = Platform::ResolveWithinRoot(m_Root, urlPath);
            if (!resolved.empty()) {
                std::ifstream f(resolved, std::ios::binary);
                if (f.is_open()) {
                    std::ostringstream ss;
                    ss << f.rdbuf();
                    body = ss.str();
                    status = "200 OK";
                    mime = MimeTypeForPath(resolved);
                    ok = true;
                }
            }
        }
        if (!ok) body = "404";

        std::ostringstream resp;
        resp << "HTTP/1.1 " << status << "\r\n"
             << "Content-Type: " << mime << "\r\n"
             << "Content-Length: " << body.size() << "\r\n"
             << "Cache-Control: no-cache\r\n"
             << "Connection: close\r\n\r\n";
        std::string head = resp.str();
        send(client, head.c_str(), static_cast<int>(head.size()), 0);
        if (!body.empty()) {
            size_t off = 0;
            while (off < body.size()) {
                int sent = send(client, body.data() + off,
                                static_cast<int>(body.size() - off), 0);
                if (sent <= 0) break;
                off += static_cast<size_t>(sent);
            }
        }
        CloseSock(client);
    }
}

} // namespace Networking
} // namespace Enjin
