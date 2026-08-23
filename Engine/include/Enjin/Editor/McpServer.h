#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/ECS/Entity.h"
#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <string>
#include <thread>

namespace Enjin {
namespace ECS { class World; }
namespace Editor {

// MCP (Model Context Protocol) server for the editor: lets AI assistants drive
// a RUNNING editor over localhost HTTP - list entities, read/write any of the
// ~160 registered components (through the same serdes registry the scene
// save/load uses), create/destroy entities, control play mode, and capture the
// game view. Connect with e.g.:
//   claude mcp add --transport http tege http://127.0.0.1:8971/mcp
//
// Threading (adr-0004): the socket thread only parses HTTP and enqueues the
// JSON-RPC body; ALL tool execution happens on the editor main thread when
// EditorLayer calls PumpMainThread() each frame. The socket thread blocks on a
// future with a timeout, so a stalled editor answers with an error instead of
// hanging the client. Localhost-only by design; off by default (Settings >
// System > MCP Server).
class ENJIN_API McpServer {
public:
    McpServer() = default;
    ~McpServer();

    McpServer(const McpServer&) = delete;
    McpServer& operator=(const McpServer&) = delete;

    // Editor-provided hooks. World powers the entity/component tools; the
    // std::function hooks power the tools that need editor subsystems (absent
    // hooks make those tools report a clean error - keeps the dispatcher
    // testable against a bare World).
    void SetWorld(ECS::World* world) { m_World = world; }
    void SetSceneInfoHook(std::function<std::string()> hook) { m_SceneInfo = std::move(hook); }
    void SetPlayControlHook(std::function<std::string(const std::string&)> hook) { m_PlayControl = std::move(hook); }
    void SetCaptureHook(std::function<std::string()> hook) { m_Capture = std::move(hook); }

    // Bind 127.0.0.1 on preferredPort (tries the next few on conflict).
    // Returns the bound port, or 0 on failure.
    u16 Start(u16 preferredPort = 8971);
    void Stop();
    bool IsRunning() const { return m_Running.load(std::memory_order_relaxed); }
    u16 GetPort() const { return m_Port; }

    // Drain pending requests and execute them on the calling (main) thread.
    // Call once per editor frame.
    void PumpMainThread();

    // JSON-RPC dispatcher: takes one request body, returns the response body
    // ("" for notifications). Public so tests can drive the whole tool surface
    // without sockets. Must be called on the thread that owns the World.
    std::string HandleJsonRpc(const std::string& body);

private:
    void Run();

    struct Pending {
        std::string body;
        std::string response;
        bool done = false;
        std::mutex m;
        std::condition_variable cv;
    };

    ECS::World* m_World = nullptr;
    std::function<std::string()> m_SceneInfo;
    std::function<std::string(const std::string&)> m_PlayControl;
    std::function<std::string()> m_Capture;

    std::thread m_Thread;
    std::atomic<bool> m_Running{false};
    u16 m_Port = 0;
    i64 m_ListenSocket = -1;

    std::mutex m_QueueMutex;
    std::queue<Pending*> m_Queue;
};

} // namespace Editor
} // namespace Enjin
