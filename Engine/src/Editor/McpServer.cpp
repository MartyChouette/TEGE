#include "Enjin/Editor/McpServer.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Name.h"
#include "Enjin/Scene/SceneSerializer.h"
#include "Enjin/Logging/Log.h"

#include <nlohmann/json.hpp>
#include <chrono>
#include <cstring>
#include <sstream>

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

using json = nlohmann::json;

namespace Enjin {
namespace Editor {

// Request bodies are component JSON at most (a mesh with vertices can be MBs);
// cap so a runaway client cannot balloon memory.
static constexpr usize kMaxBodyBytes = 16u * 1024u * 1024u;

// ---------------------------------------------------------------------------
// Tool schemas (MCP tools/list)
// ---------------------------------------------------------------------------
static json ToolList() {
    auto entityProp = json{{"type", "integer"}, {"description", "entity id"}};
    auto keyProp = json{{"type", "string"}, {"description", "component key (see registered_component_keys)"}};
    auto tool = [](const char* name, const char* desc, json props, json required) {
        return json{
            {"name", name},
            {"description", desc},
            {"inputSchema", {{"type", "object"}, {"properties", std::move(props)}, {"required", std::move(required)}}},
        };
    };
    return json::array({
        tool("scene_info", "Project, scene, play state and entity count of the running editor.",
             json::object(), json::array()),
        tool("list_entities", "List entities with their names and component keys.",
             {{"limit", {{"type", "integer"}, {"description", "max entities to return (default 200)"}}}},
             json::array()),
        tool("find_entity", "Find an entity id by its name.",
             {{"name", {{"type", "string"}}}}, json::array({"name"})),
        tool("create_entity", "Create a new entity, optionally named.",
             {{"name", {{"type", "string"}}}}, json::array()),
        tool("destroy_entity", "Destroy an entity (deferred to the next update).",
             {{"entity", entityProp}}, json::array({"entity"})),
        tool("get_component", "Read one component of an entity as JSON.",
             {{"entity", entityProp}, {"key", keyProp}}, json::array({"entity", "key"})),
        tool("set_component", "Write one component from JSON (replaces existing values; adds the component if missing).",
             {{"entity", entityProp}, {"key", keyProp},
              {"value", {{"type", "object"}, {"description", "component fields as scene-JSON"}}}},
             json::array({"entity", "key", "value"})),
        tool("add_component", "Add a default-valued component to an entity.",
             {{"entity", entityProp}, {"key", keyProp}}, json::array({"entity", "key"})),
        tool("remove_component", "Remove one component from an entity.",
             {{"entity", entityProp}, {"key", keyProp}}, json::array({"entity", "key"})),
        tool("registered_component_keys", "All component keys the engine can serialize.",
             json::object(), json::array()),
        tool("play_control", "Control play mode: action = play | pause | resume | stop.",
             {{"action", {{"type", "string"}, {"enum", {"play", "pause", "resume", "stop"}}}}},
             json::array({"action"})),
        tool("capture_view", "Capture the game view to a PNG and return its file path.",
             json::object(), json::array()),
        tool("spawn_prefab", "Instantiate a .enjprefab (project-relative path) at an optional position.",
             {{"path", {{"type", "string"}, {"description", "prefab path relative to the project root"}}},
              {"x", {{"type", "number"}}}, {"y", {{"type", "number"}}}, {"z", {{"type", "number"}}}},
             json::array({"path"})),
        tool("build_game", "Start an async build of the open project; returns immediately. Poll scene_info for progress.",
             {{"target", {{"type", "string"}, {"enum", {"desktop", "web"}},
                          {"description", "build target (default: the Build dialog's current setting)"}}},
              {"run", {{"type", "boolean"}, {"description", "launch (desktop) or serve+open (web) when the build succeeds"}}}},
             json::array()),
        tool("script_list", "List the project's AngelScript files (paths relative to the scripts folder).",
             json::object(), json::array()),
        tool("script_read", "Read one AngelScript source file from the project's scripts folder.",
             {{"path", {{"type", "string"}, {"description", "path relative to scripts/ (e.g. Player.as)"}}}},
             json::array({"path"})),
        tool("script_write", "Write an AngelScript source file (scripts folder, .as only) and compile it immediately; returns the compile diagnostics.",
             {{"path", {{"type", "string"}, {"description", "path relative to scripts/ (e.g. Player.as)"}}},
              {"content", {{"type", "string"}, {"description", "full file content"}}}},
             json::array({"path", "content"})),
        tool("script_errors", "Last script compile error and the runtime exception count for this session.",
             json::object(), json::array()),
    });
}

// Wrap a tool's payload as an MCP tools/call result.
static json ToolText(const std::string& text, bool isError = false) {
    json r{{"content", json::array({json{{"type", "text"}, {"text", text}}})}};
    if (isError) r["isError"] = true;
    return r;
}

// ---------------------------------------------------------------------------
// Tool execution (main thread)
// ---------------------------------------------------------------------------
json McpServerCallTool(McpServer* self, ECS::World* world,
                       const std::function<std::string()>& sceneInfo,
                       const std::function<std::string(const std::string&)>& playControl,
                       const std::function<std::string()>& capture,
                       const std::function<std::string(const std::string&, f32, f32, f32)>& spawnPrefab,
                       const std::function<std::string(const std::string&, bool)>& buildGame,
                       const std::function<std::string(const std::string&, const std::string&, const std::string&)>& scriptTool,
                       const std::string& name, const json& args) {
    (void)self;
    auto needWorld = [&]() -> ECS::World* { return world; };

    if (name == "spawn_prefab") {
        if (!spawnPrefab) return ToolText("prefab spawning not available in this context", true);
        std::string path = args.value("path", "");
        if (path.empty()) return ToolText("error: 'path' is required", true);
        std::string r = spawnPrefab(path,
                                    args.value("x", 0.0f),
                                    args.value("y", 0.0f),
                                    args.value("z", 0.0f));
        return ToolText(r, r.rfind("error", 0) == 0);
    }
    if (name == "build_game") {
        if (!buildGame) return ToolText("building not available in this context", true);
        std::string r = buildGame(args.value("target", ""), args.value("run", false));
        return ToolText(r, r.rfind("error", 0) == 0);
    }
    if (name == "script_list" || name == "script_read" || name == "script_write" || name == "script_errors") {
        if (!scriptTool) return ToolText("script tools not available in this context", true);
        std::string op = name.substr(7);   // strip "script_"
        std::string path = args.value("path", "");
        if ((op == "read" || op == "write") && path.empty())
            return ToolText("error: 'path' is required", true);
        std::string r = scriptTool(op, path, args.value("content", ""));
        return ToolText(r, r.rfind("error", 0) == 0);
    }

    if (name == "scene_info") {
        if (sceneInfo) return ToolText(sceneInfo());
        if (!world) return ToolText("no world attached", true);
        json j{{"entityCount", world->GetAllEntities().size()}};
        return ToolText(j.dump());
    }
    if (name == "registered_component_keys") {
        json j = Scene::SceneSerializer::RegisteredComponentKeys();
        return ToolText(j.dump());
    }
    if (name == "play_control") {
        std::string action = args.value("action", "");
        if (!playControl) return ToolText("play control not available in this context", true);
        return ToolText(playControl(action));
    }
    if (name == "capture_view") {
        if (!capture) return ToolText("capture not available in this context", true);
        std::string path = capture();
        if (path.empty()) return ToolText("capture failed (is a scene open and rendering?)", true);
        return ToolText("captured game view to " + path);
    }

    ECS::World* w = needWorld();
    if (!w) return ToolText("no world attached", true);

    if (name == "list_entities") {
        usize limit = args.value("limit", 200u);
        json list = json::array();
        for (ECS::Entity e : w->GetAllEntities()) {
            if (list.size() >= limit) break;
            if (!w->IsValid(e)) continue;
            json ent{{"id", static_cast<u64>(e)}};
            if (auto* n = w->GetComponent<ECS::NameComponent>(e)) ent["name"] = n->name;
            ent["components"] = Scene::SceneSerializer::ComponentKeysOn(w, e);
            list.push_back(std::move(ent));
        }
        json j{{"entities", std::move(list)}, {"total", w->GetAllEntities().size()}};
        return ToolText(j.dump());
    }
    if (name == "find_entity") {
        std::string ename = args.value("name", "");
        ECS::Entity e = w->FindEntityByName(ename);
        if (e == ECS::INVALID_ENTITY) return ToolText("no entity named '" + ename + "'", true);
        return ToolText(json{{"id", static_cast<u64>(e)}}.dump());
    }
    if (name == "create_entity") {
        ECS::Entity e = w->CreateEntity();
        std::string ename = args.value("name", "");
        if (!ename.empty()) {
            auto& n = w->AddComponent<ECS::NameComponent>(e);
            n.name = ename;
        }
        return ToolText(json{{"id", static_cast<u64>(e)}}.dump());
    }

    // Remaining tools address an existing entity.
    u64 id = args.value("entity", static_cast<u64>(ECS::INVALID_ENTITY));
    ECS::Entity entity = static_cast<ECS::Entity>(id);
    if (!w->IsValid(entity)) return ToolText("invalid entity id", true);

    if (name == "destroy_entity") {
        w->DestroyEntity(entity);
        return ToolText("destroyed (deferred to next update)");
    }

    std::string key = args.value("key", "");
    if (name == "get_component") {
        std::string s = Scene::SceneSerializer::SerializeOneComponent(w, entity, key);
        if (s.empty()) return ToolText("entity has no component '" + key + "'", true);
        return ToolText(s);
    }
    if (name == "set_component") {
        if (!args.contains("value") || !args["value"].is_object())
            return ToolText("'value' must be a JSON object of component fields", true);
        bool ok = Scene::SceneSerializer::DeserializeOneComponent(w, entity, key, args["value"].dump());
        return ok ? ToolText("set " + key)
                  : ToolText("unknown component key '" + key + "'", true);
    }
    if (name == "add_component") {
        bool ok = Scene::SceneSerializer::DeserializeOneComponent(w, entity, key, "{}");
        return ok ? ToolText("added " + key)
                  : ToolText("unknown component key '" + key + "'", true);
    }
    if (name == "remove_component") {
        bool ok = Scene::SceneSerializer::RemoveOneComponent(w, entity, key);
        return ok ? ToolText("removed " + key)
                  : ToolText("unknown component key '" + key + "'", true);
    }
    return ToolText("unknown tool '" + name + "'", true);
}

// ---------------------------------------------------------------------------
// JSON-RPC dispatch (main thread)
// ---------------------------------------------------------------------------
std::string McpServer::HandleJsonRpc(const std::string& body) {
    json req;
    try {
        req = json::parse(body);
    } catch (const std::exception&) {
        return json{{"jsonrpc", "2.0"}, {"id", nullptr},
                    {"error", {{"code", -32700}, {"message", "parse error"}}}}.dump();
    }
    const bool isNotification = !req.contains("id");
    json id = isNotification ? json(nullptr) : req["id"];
    std::string method = req.value("method", "");

    auto result = [&](json r) {
        return json{{"jsonrpc", "2.0"}, {"id", id}, {"result", std::move(r)}}.dump();
    };
    auto error = [&](int code, const std::string& msg) {
        return json{{"jsonrpc", "2.0"}, {"id", id},
                    {"error", {{"code", code}, {"message", msg}}}}.dump();
    };

    if (isNotification) return "";   // notifications/initialized etc: no response body

    if (method == "initialize") {
        return result(json{
            {"protocolVersion", "2024-11-05"},
            {"capabilities", {{"tools", json::object()}}},
            {"serverInfo", {{"name", "TEGE Editor"}, {"version", "0.9.7"}}},
        });
    }
    if (method == "ping") return result(json::object());
    if (method == "tools/list") return result(json{{"tools", ToolList()}});
    if (method == "tools/call") {
        const json& params = req.value("params", json::object());
        std::string name = params.value("name", "");
        json args = params.value("arguments", json::object());
        try {
            json r = McpServerCallTool(this, m_World, m_SceneInfo, m_PlayControl, m_Capture,
                                       m_SpawnPrefab, m_Build, m_ScriptTool, name, args);
            return result(std::move(r));
        } catch (const std::exception& e) {
            return result(ToolText(std::string("tool threw: ") + e.what(), true));
        }
    }
    return error(-32601, "method not found: " + method);
}

// ---------------------------------------------------------------------------
// Main-thread pump
// ---------------------------------------------------------------------------
void McpServer::PumpMainThread() {
    for (;;) {
        Pending* p = nullptr;
        {
            std::lock_guard<std::mutex> lock(m_QueueMutex);
            if (m_Queue.empty()) return;
            p = m_Queue.front();
            m_Queue.pop();
        }
        std::string resp = HandleJsonRpc(p->body);
        {
            std::lock_guard<std::mutex> lock(p->m);
            p->response = std::move(resp);
            p->done = true;
        }
        p->cv.notify_one();
    }
}

// ---------------------------------------------------------------------------
// HTTP transport (socket thread)
// ---------------------------------------------------------------------------
McpServer::~McpServer() { Stop(); }

u16 McpServer::Start(u16 preferredPort) {
    Stop();
#ifdef _WIN32
    static bool s_WsaReady = [] {
        WSADATA d;
        return WSAStartup(MAKEWORD(2, 2), &d) == 0;
    }();
    if (!s_WsaReady) return 0;
#endif
    SocketT listenSock = kInvalidSocket;
    u16 boundPort = 0;
    for (u16 port = preferredPort; port < preferredPort + 10; ++port) {
        listenSock = socket(AF_INET, SOCK_STREAM, 0);
        if (listenSock == kInvalidSocket) return 0;
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);   // localhost ONLY
        if (bind(listenSock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0 &&
            listen(listenSock, 4) == 0) {
            boundPort = port;
            break;
        }
        CloseSock(listenSock);
        listenSock = kInvalidSocket;
    }
    if (boundPort == 0) {
        ENJIN_LOG_WARN(Editor, "McpServer: no free port near %u", preferredPort);
        return 0;
    }
    m_ListenSocket = static_cast<i64>(listenSock);
    m_Port = boundPort;
    m_Running.store(true, std::memory_order_relaxed);
    m_Thread = std::thread(&McpServer::Run, this);
    ENJIN_LOG_INFO(Editor, "McpServer: listening on http://127.0.0.1:%u/mcp", boundPort);
    return boundPort;
}

void McpServer::Stop() {
    if (!m_Running.exchange(false, std::memory_order_relaxed)) {
        if (m_Thread.joinable()) m_Thread.join();
        return;
    }
    if (m_ListenSocket >= 0) {
        CloseSock(static_cast<SocketT>(m_ListenSocket));
        m_ListenSocket = -1;
    }
    if (m_Thread.joinable()) m_Thread.join();
    // Fail any requests still parked in the queue so their sockets close.
    std::lock_guard<std::mutex> lock(m_QueueMutex);
    while (!m_Queue.empty()) {
        Pending* p = m_Queue.front();
        m_Queue.pop();
        {
            std::lock_guard<std::mutex> pl(p->m);
            p->response = "";
            p->done = true;
        }
        p->cv.notify_one();
    }
    m_Port = 0;
}

void McpServer::Run() {
    SocketT listenSock = static_cast<SocketT>(m_ListenSocket);
    while (m_Running.load(std::memory_order_relaxed)) {
        SocketT client = accept(listenSock, nullptr, nullptr);
        if (client == kInvalidSocket) {
            if (!m_Running.load(std::memory_order_relaxed)) break;
            continue;
        }

        // Read headers (loop until the blank line), then the Content-Length body.
        std::string data;
        char buf[8192];
        usize headerEnd = std::string::npos;
        while (headerEnd == std::string::npos) {
            int n = recv(client, buf, sizeof(buf), 0);
            if (n <= 0) break;
            data.append(buf, static_cast<usize>(n));
            headerEnd = data.find("\r\n\r\n");
            if (data.size() > kMaxBodyBytes) break;
        }
        if (headerEnd == std::string::npos) { CloseSock(client); continue; }

        std::string head = data.substr(0, headerEnd);
        usize contentLength = 0;
        {
            // Case-insensitive Content-Length scan
            std::string lower = head;
            for (auto& c : lower) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
            usize p = lower.find("content-length:");
            if (p != std::string::npos)
                contentLength = static_cast<usize>(std::strtoull(lower.c_str() + p + 15, nullptr, 10));
        }
        bool isPost = data.rfind("POST ", 0) == 0;

        std::string body = data.substr(headerEnd + 4);
        while (isPost && body.size() < contentLength && body.size() < kMaxBodyBytes) {
            int n = recv(client, buf, sizeof(buf), 0);
            if (n <= 0) break;
            body.append(buf, static_cast<usize>(n));
        }

        std::string response;
        std::string status = "200 OK";
        if (!isPost) {
            status = "405 Method Not Allowed";
            response = "POST JSON-RPC to this endpoint";
        } else if (contentLength > kMaxBodyBytes) {
            status = "413 Payload Too Large";
            response = "body too large";
        } else {
            // Park the request for the main thread; wait with a timeout so a
            // stalled editor produces an error instead of a hung client.
            Pending pending;
            pending.body = body;
            {
                std::lock_guard<std::mutex> lock(m_QueueMutex);
                m_Queue.push(&pending);
            }
            std::unique_lock<std::mutex> lk(pending.m);
            bool ok = pending.cv.wait_for(lk, std::chrono::seconds(10),
                                          [&] { return pending.done; });
            if (!ok) {
                // Editor never picked it up (blocked/modal). Pull it back off the
                // queue if still parked so PumpMainThread can't touch freed stack.
                {
                    std::lock_guard<std::mutex> qlock(m_QueueMutex);
                    std::queue<Pending*> keep;
                    while (!m_Queue.empty()) {
                        if (m_Queue.front() != &pending) keep.push(m_Queue.front());
                        m_Queue.pop();
                    }
                    m_Queue = std::move(keep);
                }
                status = "504 Gateway Timeout";
                response = R"json({"jsonrpc":"2.0","id":null,"error":{"code":-32000,"message":"editor busy (timeout)"}})json";
            } else {
                response = pending.response;
                if (response.empty()) status = "202 Accepted";   // notification
            }
        }

        std::ostringstream resp;
        resp << "HTTP/1.1 " << status << "\r\n"
             << "Content-Type: application/json\r\n"
             << "Content-Length: " << response.size() << "\r\n"
             << "Connection: close\r\n\r\n"
             << response;
        std::string out = resp.str();
        usize off = 0;
        while (off < out.size()) {
            int sent = send(client, out.data() + off, static_cast<int>(out.size() - off), 0);
            if (sent <= 0) break;
            off += static_cast<usize>(sent);
        }
        CloseSock(client);
    }
}

} // namespace Editor
} // namespace Enjin
