#include "Enjin/Scripting/ScriptBindings.h"
#include "Enjin/Scripting/ASCallConv.h"
#include "Enjin/Scripting/ScriptEvents.h"
#include "Enjin/Logging/Log.h"
#include "Enjin/Networking/NetworkSystem.h"
#include <angelscript.h>
#include <cassert>
#include <string>
#include <unordered_set>

using namespace Enjin;

#define AS_CHECK(expr) \
    do { int _r = (expr); if (_r < 0) { ENJIN_LOG_ERROR(Script, "AS registration failed (code %d) at %s:%d", _r, __FILE__, __LINE__); } } while(0)

static Networking::NetworkSystem* s_BindingsNetworking = nullptr;
extern Enjin::Scripting::ScriptEventBus* s_BindingsEventBus;

namespace Enjin {
namespace Scripting {

void SetBindingsNetworking(Networking::NetworkSystem* net) {
    s_BindingsNetworking = net;
}

} // namespace Scripting
} // namespace Enjin

// ============================================================================
// Connection
// ============================================================================

static bool Net_HostGame(int port, const std::string& playerName) {
    if (!s_BindingsNetworking) return false;
    if (port < 1 || port > 65535) return false; // SC-H1: validate port range
    return s_BindingsNetworking->HostGame(static_cast<u16>(port), playerName);
}

static bool Net_JoinGame(const std::string& ip, int port, const std::string& playerName) {
    if (!s_BindingsNetworking) return false;
    if (port < 1 || port > 65535 || ip.empty()) return false; // SC-H1: validate port + IP
    return s_BindingsNetworking->JoinGame(ip, static_cast<u16>(port), playerName);
}

static void Net_Disconnect() {
    if (s_BindingsNetworking) s_BindingsNetworking->Disconnect();
}

// ============================================================================
// State queries
// ============================================================================

static bool Net_IsConnected() {
    return s_BindingsNetworking ? s_BindingsNetworking->IsConnected() : false;
}

static bool Net_IsHost() {
    return s_BindingsNetworking ? s_BindingsNetworking->IsHost() : false;
}

static int Net_GetRole() {
    if (!s_BindingsNetworking) return 0; // None
    return static_cast<int>(s_BindingsNetworking->GetRole());
}

static int Net_GetLocalPlayerId() {
    if (!s_BindingsNetworking) return 0xFF; // INVALID_PLAYER
    return static_cast<int>(s_BindingsNetworking->GetLocalPlayerId());
}

static int Net_GetPlayerCount() {
    if (!s_BindingsNetworking) return 0;
    return static_cast<int>(s_BindingsNetworking->GetConnectedPlayerCount());
}

static float Net_GetPing() {
    return s_BindingsNetworking ? s_BindingsNetworking->GetPing() : 0.0f;
}

static float Net_GetPacketLoss() {
    return s_BindingsNetworking ? s_BindingsNetworking->GetPacketLoss() : 0.0f;
}

// ============================================================================
// Lobby
// ============================================================================

static void Net_SetReady(bool ready) {
    if (s_BindingsNetworking) s_BindingsNetworking->SetReady(ready);
}

static int Net_GetLobbyPlayerCount() {
    if (!s_BindingsNetworking) return 0;
    return static_cast<int>(s_BindingsNetworking->GetLobbyPlayers().size());
}

static std::string Net_GetLobbyPlayerName(int index) {
    if (!s_BindingsNetworking) return "";
    const auto& players = s_BindingsNetworking->GetLobbyPlayers();
    if (index < 0 || index >= static_cast<int>(players.size())) return "";
    return players[index].name;
}

static bool Net_GetLobbyPlayerReady(int index) {
    if (!s_BindingsNetworking) return false;
    const auto& players = s_BindingsNetworking->GetLobbyPlayers();
    if (index < 0 || index >= static_cast<int>(players.size())) return false;
    return players[index].ready;
}

// ============================================================================
// Entity ownership
// ============================================================================

static int Net_RegisterEntity(u64 entityId) {
    if (!s_BindingsNetworking) return 0;
    auto localPlayer = s_BindingsNetworking->GetLocalPlayerId();
    return static_cast<int>(s_BindingsNetworking->RegisterNetworkEntity(
        static_cast<ECS::Entity>(entityId), localPlayer));
}

static void Net_UnregisterEntity(int networkId) {
    if (s_BindingsNetworking)
        s_BindingsNetworking->UnregisterNetworkEntity(static_cast<Networking::NetworkId>(networkId));
}

static void Net_RequestOwnership(int networkId) {
    if (s_BindingsNetworking)
        s_BindingsNetworking->RequestOwnership(static_cast<Networking::NetworkId>(networkId));
}

// ============================================================================
// RPC
// ============================================================================

static constexpr usize kMaxRPCDataSize = 65536; // SC-H2: 64KB cap

static void Net_CallRPC(const std::string& name, int targetPlayerId, const std::string& data) {
    if (!s_BindingsNetworking) return;
    if (data.size() > kMaxRPCDataSize) return; // SC-H2: reject oversized payloads
    s_BindingsNetworking->CallRPC(name, static_cast<Networking::PlayerId>(targetPlayerId),
        reinterpret_cast<const u8*>(data.data()), static_cast<u32>(data.size()));
}

static void Net_CallRPCAll(const std::string& name, const std::string& data) {
    if (!s_BindingsNetworking) return;
    if (data.size() > kMaxRPCDataSize) return; // SC-H2
    s_BindingsNetworking->CallRPCAll(name,
        reinterpret_cast<const u8*>(data.data()), static_cast<u32>(data.size()));
}

static std::unordered_set<std::string> s_RegisteredRPCHandlers; // SC-H3: track registrations

static void Net_RegisterRPCHandler(const std::string& name) {
    if (!s_BindingsNetworking) return;
    if (s_RegisteredRPCHandlers.count(name)) return; // SC-H3: reject duplicates
    if (s_RegisteredRPCHandlers.size() >= 64) return; // SC-H3: cap total registrations
    s_RegisteredRPCHandlers.insert(name);
    // Register a handler that fires a script event "__rpc_<name>"
    std::string eventName = "__rpc_" + name;
    s_BindingsNetworking->RegisterRPC(name, [eventName](Networking::PlayerId sender, const u8* data, u32 size) {
        if (s_BindingsEventBus) {
            std::string payload(reinterpret_cast<const char*>(data), size);
            Scripting::EventData eventData;
            eventData.SetString("data", payload);
            eventData.SetInt("sender", static_cast<i32>(sender));
            s_BindingsEventBus->Send(eventName, eventData);
        }
    });
}

// ============================================================================
// Registration
// ============================================================================

namespace Enjin {
namespace Scripting {

void RegisterNetworkBindings(asIScriptEngine* engine) {
    // ---- Enums ----
    AS_CHECK(engine->RegisterEnum("NetworkRole"));
    AS_CHECK(engine->RegisterEnumValue("NetworkRole", "None", 0));
    AS_CHECK(engine->RegisterEnumValue("NetworkRole", "Host", 1));
    AS_CHECK(engine->RegisterEnumValue("NetworkRole", "Client", 2));

    // ---- Connection ----
    AS_CHECK(engine->RegisterGlobalFunction(
        "bool Net_HostGame(int, const string &in)",
        ENJIN_AS_FN(Net_HostGame), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "bool Net_JoinGame(const string &in, int, const string &in)",
        ENJIN_AS_FN(Net_JoinGame), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "void Net_Disconnect()",
        ENJIN_AS_FN(Net_Disconnect), ENJIN_AS_CALL_CDECL));

    // ---- State ----
    AS_CHECK(engine->RegisterGlobalFunction(
        "bool Net_IsConnected()",
        ENJIN_AS_FN(Net_IsConnected), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "bool Net_IsHost()",
        ENJIN_AS_FN(Net_IsHost), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "int Net_GetRole()",
        ENJIN_AS_FN(Net_GetRole), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "int Net_GetLocalPlayerId()",
        ENJIN_AS_FN(Net_GetLocalPlayerId), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "int Net_GetPlayerCount()",
        ENJIN_AS_FN(Net_GetPlayerCount), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "float Net_GetPing()",
        ENJIN_AS_FN(Net_GetPing), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "float Net_GetPacketLoss()",
        ENJIN_AS_FN(Net_GetPacketLoss), ENJIN_AS_CALL_CDECL));

    // ---- Lobby ----
    AS_CHECK(engine->RegisterGlobalFunction(
        "void Net_SetReady(bool)",
        ENJIN_AS_FN(Net_SetReady), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "int Net_GetLobbyPlayerCount()",
        ENJIN_AS_FN(Net_GetLobbyPlayerCount), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "string Net_GetLobbyPlayerName(int)",
        ENJIN_AS_FN(Net_GetLobbyPlayerName), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "bool Net_GetLobbyPlayerReady(int)",
        ENJIN_AS_FN(Net_GetLobbyPlayerReady), ENJIN_AS_CALL_CDECL));

    // ---- Entity ownership ----
    AS_CHECK(engine->RegisterGlobalFunction(
        "int Net_RegisterEntity(uint64)",
        ENJIN_AS_FN(Net_RegisterEntity), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "void Net_UnregisterEntity(int)",
        ENJIN_AS_FN(Net_UnregisterEntity), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "void Net_RequestOwnership(int)",
        ENJIN_AS_FN(Net_RequestOwnership), ENJIN_AS_CALL_CDECL));

    // ---- RPC ----
    AS_CHECK(engine->RegisterGlobalFunction(
        "void Net_CallRPC(const string &in, int, const string &in)",
        ENJIN_AS_FN(Net_CallRPC), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "void Net_CallRPCAll(const string &in, const string &in)",
        ENJIN_AS_FN(Net_CallRPCAll), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "void Net_RegisterRPCHandler(const string &in)",
        ENJIN_AS_FN(Net_RegisterRPCHandler), ENJIN_AS_CALL_CDECL));
}

} // namespace Scripting
} // namespace Enjin
