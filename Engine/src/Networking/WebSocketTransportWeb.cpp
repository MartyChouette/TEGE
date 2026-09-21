#include "Enjin/Networking/WebSocketTransportWeb.h"

// PARKED 2026-09-20, on Marty's call: browser network calls are not being
// pursued for now. Kept rather than deleted because it is written, it compiled
// and linked for web, and the remaining work is runtime proof rather than more
// code -- but it is compiled OUT, and nothing defines ENJIN_WEB_NETWORKING.
//
// Deliberately not left merely unreferenced: an object that still needs
// -lwebsocket.js to link is a build failure waiting for whoever next touches
// the web target. Compiled to nothing, it costs nobody anything.
//
// To revive: define ENJIN_WEB_NETWORKING, put -lwebsocket.js back in
// cmake/EmscriptenToolchain.cmake, and return this from TransportFactory on
// web. What was never shown is a browser completing a join against a live
// host; see docs/WEB_TIER.md.
#if defined(__EMSCRIPTEN__) && defined(ENJIN_WEB_NETWORKING)

#include "Enjin/Logging/Log.h"
#include <cstring>
#include <emscripten/websocket.h>

namespace Enjin {
namespace Networking {

namespace {

// Same ceiling the desktop transport uses, for the same reason: a message this
// large is not a game message, and holding it costs a browser tab its memory.
constexpr usize kMaxMessageBytes = 8u * 1024u * 1024u;

std::string UrlFor(const NetworkAddress& addr) {
    // ws:// rather than wss://. A page served over HTTPS will refuse this as
    // mixed content -- correctly -- so a browser reaching a host directly has
    // to be served over plain HTTP. The relay in step 4 is what makes wss://
    // possible, and it changes this string and nothing else.
    return "ws://" + NetworkAddress::IPToString(addr.ip) + ":" +
           std::to_string(addr.port) + "/";
}

EM_BOOL OnOpenThunk(int, const EmscriptenWebSocketOpenEvent* e, void* user) {
    WebSocketTransportWeb::OnOpen(e->socket, user);
    return EM_TRUE;
}

EM_BOOL OnMessageThunk(int, const EmscriptenWebSocketMessageEvent* e, void* user) {
    WebSocketTransportWeb::OnMessage(e->socket, e->data, e->numBytes,
                                           e->isText != 0, user);
    return EM_TRUE;
}

EM_BOOL OnCloseThunk(int, const EmscriptenWebSocketCloseEvent* e, void* user) {
    WebSocketTransportWeb::OnClose(e->socket, user);
    return EM_TRUE;
}

EM_BOOL OnErrorThunk(int, const EmscriptenWebSocketErrorEvent* e, void* user) {
    // An error is always followed by a close event, so the teardown lives there
    // and this only says why -- which is the part a person debugging a failed
    // join actually needs, since the browser's own console message is not
    // visible from inside the wasm module.
    ENJIN_LOG_WARN(Network, "WebSocketTransportWeb: socket error (likely refused, "
                            "wrong port, or ws:// blocked from an https:// page)");
    WebSocketTransportWeb::OnClose(e->socket, user);
    return EM_TRUE;
}

} // namespace

WebSocketTransportWeb::~WebSocketTransportWeb() {
    Close();
}

bool WebSocketTransportWeb::Bind(u16 port) {
    if (!emscripten_websocket_is_supported()) {
        ENJIN_LOG_ERROR(Network, "WebSocketTransportWeb: this browser has no WebSocket support");
        return false;
    }
    if (port != 0) {
        // A browser cannot listen. Failing loudly beats hosting a game that
        // nobody can reach while reporting success.
        ENJIN_LOG_ERROR(Network, "WebSocketTransportWeb: a browser cannot host "
                                 "(Bind(%u) refused); join an existing host instead", port);
        return false;
    }
    m_Open = true;
    return true;
}

void WebSocketTransportWeb::Close() {
    for (Socket& s : m_Sockets) {
        if (s.handle) emscripten_websocket_close(s.handle, 1000, "closing");
        if (s.handle) emscripten_websocket_delete(s.handle);
    }
    m_Sockets.clear();
    m_Inbox.clear();
    m_Open = false;
}

WebSocketTransportWeb::Socket* WebSocketTransportWeb::FindByAddress(const NetworkAddress& addr) {
    for (Socket& s : m_Sockets) if (s.address == addr) return &s;
    return nullptr;
}

WebSocketTransportWeb::Socket* WebSocketTransportWeb::FindByHandle(i32 handle) {
    for (Socket& s : m_Sockets) if (s.handle == handle) return &s;
    return nullptr;
}

void WebSocketTransportWeb::FlushPending(Socket& s) {
    while (!s.pendingOut.empty()) {
        const std::vector<u8>& p = s.pendingOut.front();
        emscripten_websocket_send_binary(s.handle, const_cast<u8*>(p.data()),
                                         static_cast<u32>(p.size()));
        s.pendingOut.pop_front();
    }
}

void WebSocketTransportWeb::OnOpen(i32 handle, void* user) {
    auto* self = static_cast<WebSocketTransportWeb*>(user);
    if (Socket* s = self->FindByHandle(handle)) {
        s->open = true;
        self->FlushPending(*s);
    }
}

void WebSocketTransportWeb::OnMessage(i32 handle, const u8* data, u32 size,
                                      bool isText, void* user) {
    auto* self = static_cast<WebSocketTransportWeb*>(user);
    Socket* s = self->FindByHandle(handle);
    if (!s) return;
    // Text frames are not game traffic. A relay may speak JSON one day, but
    // nothing above this expects it, and handing text up as though it were a
    // packet deserializes into nonsense.
    if (isText || size == 0 || size > kMaxMessageBytes) return;
    self->m_Inbox.emplace_back(s->address, std::vector<u8>(data, data + size));
}

void WebSocketTransportWeb::OnClose(i32 handle, void* user) {
    auto* self = static_cast<WebSocketTransportWeb*>(user);
    for (usize i = 0; i < self->m_Sockets.size(); i++) {
        if (self->m_Sockets[i].handle == handle) {
            emscripten_websocket_delete(handle);
            self->m_Sockets.erase(self->m_Sockets.begin() + static_cast<std::ptrdiff_t>(i));
            return;
        }
    }
}

bool WebSocketTransportWeb::SendTo(const NetworkAddress& addr, const u8* data, u32 size) {
    if (!m_Open) return false;

    Socket* s = FindByAddress(addr);
    if (!s) {
        const std::string url = UrlFor(addr);
        EmscriptenWebSocketCreateAttributes attr;
        emscripten_websocket_init_create_attributes(&attr);
        attr.url = url.c_str();
        attr.createOnMainThread = EM_TRUE;

        const EMSCRIPTEN_WEBSOCKET_T handle = emscripten_websocket_new(&attr);
        if (handle <= 0) {
            ENJIN_LOG_ERROR(Network, "WebSocketTransportWeb: could not create a socket to %s",
                            url.c_str());
            return false;
        }

        Socket created;
        created.handle = handle;
        created.address = addr;
        m_Sockets.push_back(std::move(created));
        s = &m_Sockets.back();

        emscripten_websocket_set_onopen_callback(handle, this, OnOpenThunk);
        emscripten_websocket_set_onmessage_callback(handle, this, OnMessageThunk);
        emscripten_websocket_set_onclose_callback(handle, this, OnCloseThunk);
        emscripten_websocket_set_onerror_callback(handle, this, OnErrorThunk);
        ENJIN_LOG_INFO(Network, "WebSocketTransportWeb: opening %s", url.c_str());
    }

    if (!s->open) {
        // Queue rather than fail: the browser opens asynchronously and
        // NetworkSystem::JoinGame sends its connection request immediately.
        s->pendingOut.emplace_back(data, data + size);
        return true;
    }

    emscripten_websocket_send_binary(s->handle, const_cast<u8*>(data), size);
    return true;
}

i32 WebSocketTransportWeb::ReceiveFrom(NetworkAddress& sender, u8* buffer, u32 bufferSize) {
    if (!m_Open) return -1;
    if (m_Inbox.empty()) return 0;

    auto& front = m_Inbox.front();
    if (front.second.size() > bufferSize) {
        // Dropped, not truncated: half a packet deserializes into nonsense.
        ENJIN_LOG_WARN(Network, "WebSocketTransportWeb: message of %zu bytes exceeds the "
                                "%u-byte buffer, dropped", front.second.size(), bufferSize);
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

#endif // __EMSCRIPTEN__
