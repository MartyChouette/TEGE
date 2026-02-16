#pragma once

#include "Enjin/Networking/NetworkTypes.h"
#include "Enjin/Networking/NetworkTransport.h"
#include "Enjin/Networking/NetworkSerializer.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Entity.h"
#include <unordered_map>
#include <vector>

namespace Enjin {
namespace Networking {

class NetworkSystem {
public:
    NetworkSystem() = default;
    ~NetworkSystem() = default;

    // World reference
    void SetWorld(ECS::World* world) { m_World = world; }
    void SetEnabled(bool enabled) { m_Enabled = enabled; }
    bool IsEnabled() const { return m_Enabled; }

    // ========================================================================
    // CONNECTION API
    // ========================================================================

    // Host a game on the given port. Returns true on success.
    bool HostGame(u16 port, const std::string& playerName);

    // Join a game at the given IP:port. Returns true if connection attempt starts.
    bool JoinGame(const std::string& ip, u16 port, const std::string& playerName);

    // Disconnect from current game
    void Disconnect();

    // ========================================================================
    // UPDATE (call each frame)
    // ========================================================================

    void Update(f32 deltaTime);

    // ========================================================================
    // ENTITY OWNERSHIP
    // ========================================================================

    // Register an entity for network sync. Returns assigned NetworkId.
    NetworkId RegisterNetworkEntity(ECS::Entity entity, PlayerId owner);

    // Unregister a networked entity
    void UnregisterNetworkEntity(NetworkId networkId);

    // Request ownership of an entity (client -> host)
    void RequestOwnership(NetworkId networkId);

    // ========================================================================
    // RPC
    // ========================================================================

    // Register an RPC function
    void RegisterRPC(const std::string& name, RPCCallback callback, bool reliable = false);

    // Call RPC on a specific player
    void CallRPC(const std::string& name, PlayerId target, const u8* data = nullptr, u32 size = 0);

    // Call RPC on all connected peers
    void CallRPCAll(const std::string& name, const u8* data = nullptr, u32 size = 0);

    // ========================================================================
    // LOBBY
    // ========================================================================

    void SetReady(bool ready);
    const std::vector<LobbyPlayer>& GetLobbyPlayers() const { return m_LobbyPlayers; }

    // ========================================================================
    // STATE QUERIES
    // ========================================================================

    NetworkRole GetRole() const { return m_Role; }
    ConnectionState GetConnectionState() const;
    PlayerId GetLocalPlayerId() const { return m_LocalPlayerId; }
    bool IsConnected() const;
    bool IsHost() const { return m_Role == NetworkRole::Host; }

    const NetworkConfig& GetConfig() const { return m_Config; }
    NetworkConfig& GetConfig() { return m_Config; }
    bool LoadConfig(const std::string& path = NetworkConfig::GetDefaultPath());
    bool SaveConfig(const std::string& path = NetworkConfig::GetDefaultPath()) const;

    // Stats
    f32 GetPing() const;
    f32 GetPacketLoss() const;
    f32 GetUploadKBps() const { return m_UploadKBps; }
    f32 GetDownloadKBps() const { return m_DownloadKBps; }
    u32 GetConnectedPlayerCount() const;

    // Security
    bool IsAuthenticationEnabled() const { return m_AuthEnabled; }
    void SetAuthenticationEnabled(bool enabled) { m_AuthEnabled = enabled; }

private:
    // ========================================================================
    // INTERNAL
    // ========================================================================

    // Packet processing
    void ProcessIncomingPackets();
    void HandlePacket(const NetworkAddress& sender, const u8* data, u32 size);
    bool RateLimitPacket(const NetworkAddress& sender, u32 size);
    void CleanupRateLimiters();
    bool IsBanned(const NetworkAddress& sender);
    void RegisterViolation(const NetworkAddress& sender, const char* reason);
    void BanSender(const NetworkAddress& sender, const char* reason);
    void DisconnectSender(const NetworkAddress& sender, const char* reason);

    // Message handlers
    void HandleConnectionRequest(const NetworkAddress& sender, const u8* payload, u32 size);
    void HandleConnectionAccept(const u8* payload, u32 size);
    void HandleConnectionReject(const u8* payload, u32 size);
    void HandleDisconnect(const NetworkAddress& sender, PlayerId senderId);
    void HandleHeartbeat(const NetworkAddress& sender, PlayerId senderId);
    void HandleHeartbeatAck(const NetworkAddress& sender, PlayerId senderId);
    void HandlePlayerReady(PlayerId senderId, const u8* payload, u32 size);
    void HandleLobbyState(const u8* payload, u32 size);
    void HandleEntitySnapshot(const u8* payload, u32 size);
    void HandleEntitySpawn(const u8* payload, u32 size);
    void HandleEntityDestroy(const u8* payload, u32 size);
    void HandleOwnershipRequest(PlayerId senderId, const u8* payload, u32 size);
    void HandleOwnershipGrant(const u8* payload, u32 size);
    void HandleRPCCall(PlayerId senderId, const u8* payload, u32 size);

    // Sending
    void SendPacket(const NetworkAddress& addr, MessageType type, const std::vector<u8>& payload);
    void SendToAll(MessageType type, const std::vector<u8>& payload, PlayerId exclude = INVALID_PLAYER);
    void SendReliable(const NetworkAddress& addr, MessageType type, const std::vector<u8>& payload);

    // Authentication & Replay protection
    void GenerateSessionKey();
    void SendSessionKey(const NetworkAddress& addr);
    void HandleSessionKeyExchange(const NetworkAddress& sender, const u8* payload, u32 size);
    bool AuthenticateOutgoing(std::vector<u8>& packet, ConnectionInfo* conn);

    // Update ticks
    void UpdateHeartbeats(f32 dt);
    void CheckTimeouts(f32 dt);
    void UpdateReliableMessages(f32 dt);
    void SendEntitySnapshots();
    void InterpolateRemoteEntities(f32 dt);
    void UpdateBandwidthCounters(f32 dt);

    // Ack processing (Gaffer pattern)
    void ProcessAck(ConnectionInfo& conn, u16 ackSeq, u32 ackBits);

    // Lobby broadcast
    void BroadcastLobbyState();

    // Connection lookup
    ConnectionInfo* FindConnectionByAddress(const NetworkAddress& addr);
    ConnectionInfo* FindConnectionByPlayerId(PlayerId id);

    // ========================================================================
    // STATE
    // ========================================================================

    ECS::World* m_World = nullptr;
    bool m_Enabled = false;
    NetworkRole m_Role = NetworkRole::None;
    PlayerId m_LocalPlayerId = INVALID_PLAYER;
    std::string m_LocalPlayerName;
    NetworkConfig m_Config;
    NetworkTransport m_Transport;

    f32 m_Time = 0.0f;  // Monotonic time accumulator
    u32 m_Tick = 0;      // Sync tick counter

    // Connections (host: one per client, client: one for host)
    std::vector<ConnectionInfo> m_Connections;

    // Lobby
    std::vector<LobbyPlayer> m_LobbyPlayers;

    // Entity ownership
    NetworkId m_NextNetworkId = 1;
    std::unordered_map<NetworkId, ECS::Entity> m_NetworkToEntity;
    std::unordered_map<ECS::Entity, NetworkId> m_EntityToNetwork;

    // Interpolation buffers for remote entities
    std::unordered_map<NetworkId, InterpolationBuffer> m_InterpBuffers;

    // Rate limiting for unknown senders
    std::unordered_map<NetworkAddress, RateLimitState, NetworkAddressHash> m_UnknownRateLimiters;

    // Violation tracking / bans
    std::unordered_map<NetworkAddress, ViolationState, NetworkAddressHash> m_ViolationStates;

    // RPC registry
    std::unordered_map<u32, RPCRegistration> m_RPCRegistry;

    // Reliable outbox
    std::vector<ReliableMessage> m_ReliableOutbox;

    // Sync timer
    f32 m_SyncTimer = 0.0f;
    f32 m_HeartbeatTimer = 0.0f;

    // Bandwidth tracking
    u32 m_BytesSentThisSecond = 0;
    u32 m_BytesReceivedThisSecond = 0;
    f32 m_BandwidthTimer = 0.0f;
    f32 m_UploadKBps = 0.0f;
    f32 m_DownloadKBps = 0.0f;

    // Reusable send buffer to avoid per-packet allocation
    std::vector<u8> m_SendBuffer;

    // Authentication
    bool m_AuthEnabled = true;             // HMAC authentication enabled by default
    SessionKey m_SessionKey = {};          // Shared secret for HMAC-SHA256
    bool m_SessionKeyGenerated = false;    // True once host generates key
};

} // namespace Networking
} // namespace Enjin
