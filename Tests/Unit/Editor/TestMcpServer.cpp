// MCP dispatcher tests: drive the JSON-RPC surface directly (no sockets) against
// a bare World - the same path PumpMainThread executes on the editor main thread.

#include "EnjinTest.h"
#include "Enjin/Editor/McpServer.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Name.h"
#include <nlohmann/json.hpp>

using namespace Enjin;
using json = nlohmann::json;

static json Call(Editor::McpServer& s, const std::string& method, json params = json::object()) {
    json req{{"jsonrpc", "2.0"}, {"id", 1}, {"method", method}, {"params", std::move(params)}};
    std::string resp = s.HandleJsonRpc(req.dump());
    return json::parse(resp);
}

static json CallTool(Editor::McpServer& s, const std::string& tool, json args = json::object()) {
    json r = Call(s, "tools/call", json{{"name", tool}, {"arguments", std::move(args)}});
    return r["result"];
}

static json ToolPayload(const json& result) {
    // tools/call wraps payloads as {content:[{type:text,text:...}]}
    return json::parse(result["content"][0]["text"].get<std::string>());
}

ENJIN_TEST(McpServer, InitializeAndToolListRespond) {
    // Arrange
    Editor::McpServer s;

    // Act
    json init = Call(s, "initialize");
    json tools = Call(s, "tools/list");

    // Assert
    ENJIN_EXPECT_TRUE(init["result"]["protocolVersion"] == "2024-11-05");
    ENJIN_EXPECT_TRUE(init["result"]["serverInfo"]["name"] == "TEGE Editor");
    ENJIN_ASSERT_TRUE(tools["result"]["tools"].is_array());
    ENJIN_EXPECT_TRUE(tools["result"]["tools"].size() >= 12);
}

ENJIN_TEST(McpServer, EntityAndComponentCrudRoundTrips) {
    // Arrange: a server bound to a bare world.
    ECS::World world;
    Editor::McpServer s;
    s.SetWorld(&world);

    // Act: create a named entity via the tool surface.
    json created = ToolPayload(CallTool(s, "create_entity", {{"name", "McpCube"}}));
    u64 id = created["id"].get<u64>();

    // Assert: find_entity resolves it.
    json found = ToolPayload(CallTool(s, "find_entity", {{"name", "McpCube"}}));
    ENJIN_EXPECT_TRUE(found["id"].get<u64>() == id);

    // Act: set a transform through set_component, read it back via get_component.
    json setR = CallTool(s, "set_component",
                         {{"entity", id}, {"key", "transform"},
                          {"value", {{"position", {1.5, 2.5, 3.5}}}}});
    ENJIN_EXPECT_TRUE(!setR.contains("isError"));
    json got = ToolPayload(CallTool(s, "get_component", {{"entity", id}, {"key", "transform"}}));
    ENJIN_EXPECT_TRUE(got["position"][0].get<f32>() > 1.4f && got["position"][0].get<f32>() < 1.6f);

    // The world really has the component with the value.
    auto* tf = world.GetComponent<ECS::TransformComponent>(static_cast<ECS::Entity>(id));
    ENJIN_ASSERT_NOT_NULL(tf);
    ENJIN_EXPECT_TRUE(tf->position.y > 2.4f && tf->position.y < 2.6f);

    // list_entities includes it with its component keys.
    json list = ToolPayload(CallTool(s, "list_entities"));
    bool foundInList = false;
    for (const auto& e : list["entities"]) {
        if (e["id"].get<u64>() == id) {
            foundInList = true;
            bool hasTransform = false;
            for (const auto& k : e["components"]) if (k == "transform") hasTransform = true;
            ENJIN_EXPECT_TRUE(hasTransform);
        }
    }
    ENJIN_EXPECT_TRUE(foundInList);

    // remove_component + get_component now errors.
    CallTool(s, "remove_component", {{"entity", id}, {"key", "transform"}});
    json gone = CallTool(s, "get_component", {{"entity", id}, {"key", "transform"}});
    ENJIN_EXPECT_TRUE(gone.value("isError", false));
}

ENJIN_TEST(McpServer, HooklessToolsFailCleanly) {
    // Arrange: no world, no hooks - every tool must answer, never crash.
    Editor::McpServer s;

    // Act + Assert
    json play = CallTool(s, "play_control", {{"action", "play"}});
    ENJIN_EXPECT_TRUE(play.value("isError", false));
    json cap = CallTool(s, "capture_view");
    ENJIN_EXPECT_TRUE(cap.value("isError", false));
    json list = CallTool(s, "list_entities");
    ENJIN_EXPECT_TRUE(list.value("isError", false));

    // Unknown method -> JSON-RPC error, notifications -> no response.
    json bad = Call(s, "bogus/method");
    ENJIN_EXPECT_TRUE(bad.contains("error"));
    std::string none = s.HandleJsonRpc(R"({"jsonrpc":"2.0","method":"notifications/initialized"})");
    ENJIN_EXPECT_TRUE(none.empty());
}

ENJIN_TEST_MAIN()
