#pragma once

#include "Enjin/Networking/NetworkTypes.h"
#include "Enjin/Networking/INetworkTransport.h"
#include "Enjin/Networking/LanDiscovery.h"
#include "Enjin/Networking/NetworkSerializer.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Entity.h"
#include <unordered_map>
#include <vector>
#include <memory>
#include <set>

namespace Enjin {
namespace Networking {

class ENJIN_API NetworkSystem {
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

    // ========================================================================
    // LAN DISCOVERY (adr-0007 Track B step 1)
    //
    // A host announces itself on the subnet automatically; a client can browse
    // for hosts instead of being told an IP address. Both are CONVENIENCES:
    // HostGame and JoinGame(ip, port) work exactly as before with discovery
    // switched off, and nothing in the connect path routes through it. That is
    // adr-0007's standing rule -- nothing we build may become required.
    // ========================================================================

    // Games on one subnet only list their own kind. Set before hosting.
    void SetDiscoveryGameId(const std::string& gameId) { m_DiscoveryGameId = gameId; }
    const std::string& GetDiscoveryGameId() const { return m_DiscoveryGameId; }

    // A host announces by default. Turn this off for a private game.
    void SetAnnounceOnLan(bool enabled) { m_AnnounceOnLan = enabled; }
    bool GetAnnounceOnLan() const { return m_AnnounceOnLan; }

    // CLIENT: start/stop looking for hosts. Safe to call while not connected,
    // which is the whole point -- this is what a join screen does.
    bool StartBrowsingLan();
    void StopBrowsingLan();
    bool IsBrowsingLan() const { return m_Discovery.IsListening(); }
    const std::vector<DiscoveredSession>& GetDiscoveredSessions() const {
        return m_Discovery.GetSessions();
    }

    // Convenience: join a session the browser found.
    bool JoinDiscovered(const DiscoveredSession& session, const std::string& playerName);

    // Test seam: discovery binds its own socket, so a loopback test has to be
    // able to replace that one too, not just the game transport.
    void SetDiscoveryTransportForTest(std::unique_ptr<INetworkTransport> t) {
        m_Discovery.SetTransport(std::move(t));
    }

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

    // Routes one decoded message to its handler. Called for a packet as it
    // arrives, and again for each message unwrapped from a ReliableMessage.
    void DispatchMessage(MessageType type, const NetworkAddress& sender, PlayerId senderId,
                         const u8* payload, u32 payloadSize);

    // Message handlers
    void HandleReliableMessage(const NetworkAddress& sender, PlayerId senderId, const u8* payload, u32 size);

    // Reliability comes from the sender's own registry; warns once per name when
    // the sender has no registration for it.
    bool ResolveRPCReliability(const std::string& name, const RPCRegistration* reg);

    // Shared tail of the single-datagram and reassembled reliable paths.
    void DispatchReliablePayload(const NetworkAddress& sender, PlayerId senderId,
                                 u8 innerTypeByte, const u8* innerPayload, u32 innerSize);

    // Sends queued reliable fragments within the outgoing budget. Fragmenting a
    // large message produces hundreds of datagrams at once, and blasting them
    // trips the RECEIVER's rate limiter -- which is a DoS guard, so the sender
    // paces instead.
    void FlushPendingReliable();

    // Drops reassemblies that stopped arriving, so a half-sent message cannot
    // hold its buffers for the life of the process.
    void ExpireReassemblies();
    void HandleConnectionRequest(const NetworkAddress& sender, const u8* payload, u32 size);
    void HandleConnectionAccept(const u8* payload, u32 size);
    void HandleConnectionReject(const u8* payload, u32 size);
    void HandleDisconnect(const NetworkAddress& sender, PlayerId senderId);
    void HandleHeartbeat(const NetworkAddress& sender, PlayerId senderId);
    void HandleHeartbeatAck(const NetworkAddress& sender, PlayerId senderId);
    void HandlePlayerReady(PlayerId senderId, const u8* payload, u32 size);
    void HandleLobbyState(const u8* payload, u32 size);
    // These three take senderId because the packet being authentic is not the
    // same as the sender being ALLOWED to send it. HMAC and the replay window
    // prove a packet came from an admitted peer; only IsSenderAuthoritativeFor
    // decides whether that peer may move or delete this particular entity.
    void HandleEntitySnapshot(PlayerId senderId, const u8* payload, u32 size);
    void HandleEntitySpawn(PlayerId senderId, const u8* payload, u32 size);
    void HandleEntityDestroy(PlayerId senderId, const u8* payload, u32 size);

    // Authority gate for entity-mutating messages.
    // On a client, only the host may drive entity state -- another client's
    // packet is never authoritative, whatever it claims. On the host, a client
    // may only touch entities it actually owns. `entity` may be INVALID_ENTITY
    // for messages that are not about one entity yet (spawn), which checks the
    // host rule only.
    bool IsSenderAuthoritativeFor(PlayerId senderId, ECS::Entity entity) const;
    void HandleOwnershipRequest(PlayerId senderId, const u8* payload, u32 size);
    void HandleOwnershipGrant(const u8* payload, u32 size);
    void HandleRPCCall(PlayerId senderId, const u8* payload, u32 size);

    // Physics authority: a networked Rigidbody we do NOT own is switched to network-driven
    // (kinematic) so the local sim doesn't fight the snapshot stream; owned bodies simulate
    // normally. No-op on entities without a Rigidbody (so single-player is untouched).
    void ApplyPhysicsAuthority(ECS::Entity entity, bool isLocallyOwned);

    // Sending
    void SendPacket(const NetworkAddress& addr, MessageType type, const std::vector<u8>& payload);
    void SendToAll(MessageType type, const std::vector<u8>& payload, PlayerId exclude = INVALID_PLAYER);
    bool SendReliable(const NetworkAddress& addr, MessageType type, const std::vector<u8>& payload);

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
    std::unique_ptr<INetworkTransport> m_Transport;

    // Discovery owns its OWN transport: it binds a different port and a host
    // announces from an ephemeral one, so it cannot share the game socket.
    LanDiscovery m_Discovery;
    std::string m_DiscoveryGameId = "enjin";
    bool m_AnnounceOnLan = true;

    // Inject a custom transport (e.g. WebSocket). If not set, UDP is used by default.
    public: void SetTransport(std::unique_ptr<INetworkTransport> transport) { m_Transport = std::move(transport); }
    private:

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
    RateLimiter m_ReliableSendPackets;   // Mirrors the receiver's packet budget
    RateLimiter m_ReliableSendBytes;     // and its byte budget
    // Partially-arrived fragmented messages, per sender, keyed by message id.
    std::unordered_map<NetworkAddress, std::unordered_map<u32, ReliableReassembly>, NetworkAddressHash> m_Reassembly;
    u32 m_NextReliableMessageId = 1;   // 0 means "no id", so ids start at 1
    std::set<std::string> m_WarnedUnregisteredRPCs;

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
