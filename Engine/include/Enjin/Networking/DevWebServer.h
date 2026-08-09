#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include <atomic>
#include <string>
#include <thread>

namespace Enjin {
namespace Networking {

// Tiny localhost static-file server for previewing web exports without any
// external tooling. Browsers refuse WebAssembly over file:// URLs, so "double
// click index.html" can never work — this is the one-button replacement for
// "install Python and run serve.py".
//
// Localhost-only by design (binds 127.0.0.1): it exists to open YOUR export in
// YOUR browser, not to host games on a network. GET only, paths are confined
// to the served directory via Platform::ResolveWithinRoot, and every response
// carries Cache-Control: no-cache so re-exports show up on plain reload
// (Chrome aggressively caches .enjin/.enjpak otherwise).
class ENJIN_API DevWebServer {
public:
    DevWebServer() = default;
    ~DevWebServer();

    DevWebServer(const DevWebServer&) = delete;
    DevWebServer& operator=(const DevWebServer&) = delete;

    // Serve rootDir on 127.0.0.1. Tries preferredPort, then the next 20 ports.
    // Returns the bound port, or 0 on failure. Restarts if already running.
    u16 Start(const std::string& rootDir, u16 preferredPort = 8765);
    void Stop();

    bool IsRunning() const { return m_Running.load(std::memory_order_relaxed); }
    u16 GetPort() const { return m_Port; }
    const std::string& GetRoot() const { return m_Root; }

private:
    void Run();

    std::thread m_Thread;
    std::atomic<bool> m_Running{false};
    u16 m_Port = 0;
    std::string m_Root;
    // Platform socket handle (SOCKET on Windows, fd elsewhere) — stored wide
    // so this header stays free of socket includes
    i64 m_ListenSocket = -1;
};

} // namespace Networking
} // namespace Enjin
