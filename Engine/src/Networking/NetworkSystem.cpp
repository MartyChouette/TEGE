#include "Enjin/Platform/Platform.h"
#include "Enjin/Networking/NetworkSystem.h"
#include "Enjin/Networking/NetworkSecurity.h"
#include "Enjin/Networking/TransportFactory.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/ECS/Components/Name.h"
#include "Enjin/Physics/PhysicsTypes2D.h"
#include "Enjin/Logging/Log.h"
#include "Enjin/Debug/Profiler.h"
#include <cmath>
#include <algorithm>

// Transport is created lazily on first HostGame/JoinGame via EnsureTransport().
// By default it creates a UDPTransport. Call SetTransport() before connecting
// to inject a WebSocket or custom transport instead.

namespace Enjin {
namespace Networking {

// ============================================================================
// CONNECTION API
// ============================================================================

bool NetworkSystem::HostGame(u16 port, const std::string& playerName) {
    if (m_Role != NetworkRole::None) {
        ENJIN_LOG_WARN(Network, "NetworkSystem: Already connected, disconnect first");
        return false;
    }

    LoadConfig();

    // Create default transport if none was injected via SetTransport()
    if (!m_Transport) {
        m_Transport = CreateTransport(TransportType::Auto);
        if (!m_Transport) {
            ENJIN_LOG_ERROR(Network, "NetworkSystem: No transport available for this platform");
            return false;
        }
    }

    if (!m_Transport->Bind(port)) {
        ENJIN_LOG_ERROR(Network, "NetworkSystem: Failed to bind as host on port %u", port);
        return false;
    }

    m_Role = NetworkRole::Host;
    m_LocalPlayerId = 0;
    m_LocalPlayerName = playerName;
    m_Time = 0.0f;
    m_Tick = 0;

    // Generate session key for HMAC authentication
    if (m_AuthEnabled) {
        GenerateSessionKey();
    }

    // Add self to lobby
    LobbyPlayer self;
    self.id = 0;
    self.name = playerName;
    self.ready = false;
    self.isHost = true;
    m_LobbyPlayers.clear();
    m_LobbyPlayers.push_back(self);

    // Reserve connection storage upfront so push_back never reallocates
    // (pointers returned by FindConnectionByAddress remain valid)
    m_Connections.reserve(MAX_PLAYERS + 1);

    ENJIN_LOG_INFO(Network, "NetworkSystem: Hosting on port %u as '%s'", port, playerName.c_str());
    return true;
}

bool NetworkSystem::JoinGame(const std::string& ip, u16 port, const std::string& playerName) {
    if (m_Role != NetworkRole::None) {
        ENJIN_LOG_WARN(Network, "NetworkSystem: Already connected, disconnect first");
        return false;
    }

    LoadConfig();

    if (!m_Transport) {
        m_Transport = CreateTransport(TransportType::Auto);
        if (!m_Transport) {
            ENJIN_LOG_ERROR(Network, "NetworkSystem: No transport available for this platform");
            return false;
        }
    }

    // Bind to any available port
    if (!m_Transport->Bind(0)) {
        ENJIN_LOG_ERROR(Network, "NetworkSystem: Failed to bind client socket");
        return false;
    }

    m_Role = NetworkRole::Client;
    m_LocalPlayerId = INVALID_PLAYER;
    m_LocalPlayerName = playerName;
    m_Time = 0.0f;
    m_Tick = 0;

    // Create connection to server
    ConnectionInfo serverConn;
    serverConn.address.ip = NetworkAddress::ParseIP(ip);
    serverConn.address.port = port;
    serverConn.state = ConnectionState::Connecting;
    serverConn.lastSendTime = 0.0f;
    serverConn.lastRecvTime = 0.0f;
    m_Connections.clear();
    m_Connections.reserve(MAX_PLAYERS + 1);
    m_Connections.push_back(serverConn);

    // Send connection request
    std::vector<u8> payload;
    WriteString(payload, playerName);
    SendPacket(serverConn.address, MessageType::ConnectionRequest, payload);

    ENJIN_LOG_INFO(Network, "NetworkSystem: Connecting to %s:%u as '%s'", ip.c_str(), port, playerName.c_str());
    return true;
}

void NetworkSystem::Disconnect() {
    if (m_Role == NetworkRole::None) return;

    // Send disconnect to all peers (S-M11: avoid static vector destruction order issues)
    const std::vector<u8> empty;
    SendToAll(MessageType::Disconnect, empty);

    // If host, notify all clients they're disconnected
    if (m_Role == NetworkRole::Host) {
        for (auto& conn : m_Connections) {
            conn.state = ConnectionState::Disconnected;
        }
    }

    if (m_Transport) m_Transport->Close();
    m_Role = NetworkRole::None;
    m_LocalPlayerId = INVALID_PLAYER;
    m_Connections.clear();
    m_LobbyPlayers.clear();
    m_NetworkToEntity.clear();
    m_EntityToNetwork.clear();
    m_InterpBuffers.clear();
    m_ReliableOutbox.clear();
    m_UnknownRateLimiters.clear();
    m_ViolationStates.clear();
    m_SyncTimer = 0.0f;
    m_HeartbeatTimer = 0.0f;
    m_BytesSentThisSecond = 0;
    m_BytesReceivedThisSecond = 0;
    m_UploadKBps = 0.0f;
    m_DownloadKBps = 0.0f;

    // Clear session key
    m_SessionKey = {};
    m_SessionKeyGenerated = false;

    ENJIN_LOG_INFO(Network, "NetworkSystem: Disconnected");
}

// ============================================================================
// UPDATE
// ============================================================================

void NetworkSystem::Update(f32 deltaTime) {
    if (!m_Enabled || m_Role == NetworkRole::None) return;

    ENJIN_PROFILE_SCOPE("Networking");

    m_Time += deltaTime;

    ProcessIncomingPackets();
    UpdateHeartbeats(deltaTime);
    CheckTimeouts(deltaTime);
    UpdateReliableMessages(deltaTime);
    ExpireReassemblies();

    // Entity sync at configured rate
    m_SyncTimer += deltaTime;
    if (m_SyncTimer >= m_Config.syncRate) {
        SendEntitySnapshots();
        m_SyncTimer -= m_Config.syncRate;
        m_Tick++;
    }

    InterpolateRemoteEntities(deltaTime);
    UpdateBandwidthCounters(deltaTime);
}

// ============================================================================
// ENTITY OWNERSHIP
// ============================================================================

NetworkId NetworkSystem::RegisterNetworkEntity(ECS::Entity entity, PlayerId owner) {
    // Check if already registered
    auto it = m_EntityToNetwork.find(entity);
    if (it != m_EntityToNetwork.end()) return it->second;

    NetworkId id = m_NextNetworkId++;
    // S-H3: Skip 0 on wraparound (0 is reserved as INVALID_NETWORK_ID)
    if (m_NextNetworkId == 0) m_NextNetworkId = 1;
    m_NetworkToEntity[id] = entity;
    m_EntityToNetwork[entity] = id;

    // Set component data if present
    if (m_World) {
        auto* netId = m_World->GetComponent<ECS::NetworkIdentityComponent>(entity);
        if (netId) {
            netId->networkId = id;
            netId->ownerId = owner;
            netId->isLocallyOwned = (owner == m_LocalPlayerId);
            ApplyPhysicsAuthority(entity, netId->isLocallyOwned);
        }
    }

    // Broadcast spawn to all peers
    if (m_Role == NetworkRole::Host) {
        std::vector<u8> payload;
        WriteU32(payload, id);
        WriteU8(payload, owner);
        if (m_World) {
            auto* transform = m_World->GetComponent<ECS::TransformComponent>(entity);
            if (transform) {
                WriteVector3(payload, transform->position);
                WriteQuaternion(payload, transform->rotation);
                WriteVector3(payload, transform->scale);
            } else {
                WriteVector3(payload, Math::Vector3(0, 0, 0));
                WriteQuaternion(payload, Math::Quaternion(0, 0, 0, 1));
                WriteVector3(payload, Math::Vector3(1, 1, 1));
            }
        }
        SendToAll(MessageType::EntitySpawn, payload);
    }

    ENJIN_LOG_INFO(Network, "NetworkSystem: Registered entity %llu as NetworkId %u (owner %u)",
                   (unsigned long long)entity, id, owner);
    return id;
}

void NetworkSystem::UnregisterNetworkEntity(NetworkId networkId) {
    auto it = m_NetworkToEntity.find(networkId);
    if (it == m_NetworkToEntity.end()) return;

    ECS::Entity entity = it->second;
    m_EntityToNetwork.erase(entity);
    m_NetworkToEntity.erase(it);
    m_InterpBuffers.erase(networkId);

    // Broadcast destroy
    if (m_Role == NetworkRole::Host) {
        std::vector<u8> payload;
        WriteU32(payload, networkId);
        SendToAll(MessageType::EntityDestroy, payload);
    }

    ENJIN_LOG_INFO(Network, "NetworkSystem: Unregistered NetworkId %u", networkId);
}

void NetworkSystem::RequestOwnership(NetworkId networkId) {
    if (m_Role != NetworkRole::Client || m_Connections.empty()) return;

    std::vector<u8> payload;
    WriteU32(payload, networkId);
    SendReliable(m_Connections[0].address, MessageType::OwnershipRequest, payload);
}

// ============================================================================
// RPC
// ============================================================================

void NetworkSystem::RegisterRPC(const std::string& name, RPCCallback callback, bool reliable) {
    u32 nameHash = FNV1aHash(name);

    // NET-1: Detect hash collisions — reject if hash already registered with a different name
    auto it = m_RPCRegistry.find(nameHash);
    if (it != m_RPCRegistry.end() && it->second.name != name) {
        ENJIN_LOG_ERROR(Network, "RPC hash collision: '%s' and '%s' produce the same hash 0x%08X — rejecting",
                        name.c_str(), it->second.name.c_str(), nameHash);
        return;
    }

    RPCRegistration reg;
    reg.name = name;
    reg.nameHash = nameHash;
    reg.callback = callback;
    reg.reliable = reliable;
    m_RPCRegistry[nameHash] = reg;
}

void NetworkSystem::CallRPC(const std::string& name, PlayerId target, const u8* data, u32 size) {
    u32 nameHash = FNV1aHash(name);
    auto it = m_RPCRegistry.find(nameHash);
    const bool reliable = ResolveRPCReliability(name, it != m_RPCRegistry.end() ? &it->second : nullptr);

    // The size field is u32 because a scene sync is routinely larger than 64 KB,
    // and the reliable layer fragments anything over one datagram now. The
    // ceiling is what one reassembly is allowed to hold.
    if (size > RELIABLE_MAX_MESSAGE_BYTES) {
        ENJIN_LOG_ERROR(Network, "RPC payload too large: %u bytes (max %u)", size, RELIABLE_MAX_MESSAGE_BYTES);
        return;
    }

    std::vector<u8> payload;
    WriteU32(payload, nameHash);
    WriteU8(payload, target);
    WriteU32(payload, size);
    if (data && size > 0) {
        payload.insert(payload.end(), data, data + size);
    }

    ConnectionInfo* conn = FindConnectionByPlayerId(target);
    if (conn) {
        if (reliable) {
            SendReliable(conn->address, MessageType::RPCCall, payload);
        } else {
            SendPacket(conn->address, MessageType::RPCCall, payload);
        }
    }
}

// Reliability is looked up in the SENDER's own registry, which means a peer
// that calls an RPC it never registered sends it UNRELIABLY whatever the other
// end declared -- a different delivery guarantee for the same message, decided
// by which side is talking. A caller does not need a handler, so this is legal;
// it is just never what anyone means. Say so once per name.
bool NetworkSystem::ResolveRPCReliability(const std::string& name, const RPCRegistration* reg) {
    if (reg) return reg->reliable;
    if (m_WarnedUnregisteredRPCs.insert(name).second) {
        ENJIN_LOG_WARN(Network, "RPC '%s' is not registered here, so it is being sent UNRELIABLY. "
                                "Register it on both ends to get the reliability it was declared with.",
                       name.c_str());
    }
    return false;
}

void NetworkSystem::CallRPCAll(const std::string& name, const u8* data, u32 size) {
    if (size > RELIABLE_MAX_MESSAGE_BYTES) {
        ENJIN_LOG_ERROR(Network, "RPC broadcast payload too large: %u bytes (max %u)",
                        size, RELIABLE_MAX_MESSAGE_BYTES);
        return;
    }

    u32 nameHash = FNV1aHash(name);
    auto it = m_RPCRegistry.find(nameHash);
    const bool reliable = ResolveRPCReliability(name, it != m_RPCRegistry.end() ? &it->second : nullptr);

    std::vector<u8> payload;
    WriteU32(payload, nameHash);
    WriteU8(payload, INVALID_PLAYER);  // broadcast
    WriteU32(payload, size);
    if (data && size > 0) {
        payload.insert(payload.end(), data, data + size);
    }

    if (reliable) {
        for (auto& conn : m_Connections) {
            if (conn.state == ConnectionState::Connected) {
                SendReliable(conn.address, MessageType::RPCCall, payload);
            }
        }
    } else {
        SendToAll(MessageType::RPCCall, payload);
    }
}

// ============================================================================
// LOBBY
// ============================================================================

void NetworkSystem::SetReady(bool ready) {
    // Update local player in lobby
    for (auto& lp : m_LobbyPlayers) {
        if (lp.id == m_LocalPlayerId) {
            lp.ready = ready;
            break;
        }
    }

    if (m_Role == NetworkRole::Host) {
        BroadcastLobbyState();
    } else if (m_Role == NetworkRole::Client && !m_Connections.empty()) {
        std::vector<u8> payload;
        WriteU8(payload, ready ? 1 : 0);
        SendPacket(m_Connections[0].address, MessageType::PlayerReady, payload);
    }
}

// ============================================================================
// STATE QUERIES
// ============================================================================

ConnectionState NetworkSystem::GetConnectionState() const {
    if (m_Role == NetworkRole::Host) return ConnectionState::Connected;
    if (m_Role == NetworkRole::Client && !m_Connections.empty()) {
        return m_Connections[0].state;
    }
    return ConnectionState::Disconnected;
}

bool NetworkSystem::IsConnected() const {
    if (m_Role == NetworkRole::Host) return true;
    if (m_Role == NetworkRole::Client && !m_Connections.empty()) {
        return m_Connections[0].state == ConnectionState::Connected;
    }
    return false;
}

f32 NetworkSystem::GetPing() const {
    if (m_Role == NetworkRole::Client && !m_Connections.empty()) {
        return m_Connections[0].rtt * 1000.0f;  // Convert to ms
    }
    // Host: average ping to all clients
    if (m_Role == NetworkRole::Host && !m_Connections.empty()) {
        f32 total = 0.0f;
        u32 count = 0;
        for (const auto& conn : m_Connections) {
            if (conn.state == ConnectionState::Connected) {
                total += conn.rtt;
                count++;
            }
        }
        return count > 0 ? (total / count) * 1000.0f : 0.0f;
    }
    return 0.0f;
}

f32 NetworkSystem::GetPacketLoss() const {
    if (m_Role == NetworkRole::Client && !m_Connections.empty()) {
        return m_Connections[0].packetLossRate * 100.0f;
    }
    return 0.0f;
}

u32 NetworkSystem::GetConnectedPlayerCount() const {
    if (m_Role == NetworkRole::None) return 0;
    u32 count = 1;  // Self
    for (const auto& conn : m_Connections) {
        if (conn.state == ConnectionState::Connected) count++;
    }
    return count;
}

bool NetworkSystem::LoadConfig(const std::string& path) {
    return m_Config.LoadFromFile(path);
}

bool NetworkSystem::SaveConfig(const std::string& path) const {
    return m_Config.SaveToFile(path);
}

// ============================================================================
// PACKET PROCESSING
// ============================================================================

namespace {
static constexpr f32 kRateLimitUnknownTtl = 10.0f;
static constexpr size_t kRateLimitUnknownCap = 256;
static constexpr f32 kViolationCleanupTtl = 60.0f;

static u32 GetMinPayloadSize(MessageType type) {
    switch (type) {
        case MessageType::ConnectionRequest: return 1;
        case MessageType::ConnectionAccept: return 1;
        case MessageType::ConnectionReject: return 1;
        case MessageType::Heartbeat: return 0;
        case MessageType::HeartbeatAck: return 0;
        case MessageType::PlayerReady: return 1;
        case MessageType::LobbyState: return 1;
        case MessageType::EntitySnapshot: return 2;
        case MessageType::EntitySpawn: return 45;
        case MessageType::EntityDestroy: return 4;
        case MessageType::OwnershipRequest: return 4;
        case MessageType::OwnershipGrant: return 5;
        // [u32 nameHash][u8 target][u32 size]
        case MessageType::RPCCall: return 9;
        case MessageType::SessionKeyExchange: return SESSION_KEY_SIZE;
        // [u16 outer sequence][u32 message id][u8 inner type], then the inner payload
        case MessageType::ReliableMessage: return 7;
        default: return 0;
    }
}
}

bool NetworkSystem::IsBanned(const NetworkAddress& sender) {
    auto it = m_ViolationStates.find(sender);
    if (it == m_ViolationStates.end()) return false;
    return it->second.bannedUntil > m_Time;
}

void NetworkSystem::RegisterViolation(const NetworkAddress& sender, const char* reason) {
    if (m_Config.maxViolations == 0 || m_Config.violationWindowSeconds <= 0.0f) return;

    ViolationState& state = m_ViolationStates[sender];
    if (state.windowStart <= 0.0f || (m_Time - state.windowStart) > m_Config.violationWindowSeconds) {
        state.windowStart = m_Time;
        state.count = 0;
    }

    state.count++;
    state.lastSeen = m_Time;

    if (state.count >= m_Config.maxViolations) {
        BanSender(sender, reason);
    }
}

void NetworkSystem::BanSender(const NetworkAddress& sender, const char* reason) {
    ViolationState& state = m_ViolationStates[sender];
    state.bannedUntil = m_Time + std::max(0.0f, m_Config.banSeconds);
    state.lastSeen = m_Time;
    state.count = 0;

    ENJIN_LOG_WARN(Network, "NetworkSystem: Banning sender (reason: %s)", reason ? reason : "unknown");
    if (m_Config.kickOnViolation) {
        DisconnectSender(sender, reason);
    }
}

void NetworkSystem::DisconnectSender(const NetworkAddress& sender, const char* reason) {
    if (m_Role == NetworkRole::Host) {
        ConnectionInfo* conn = FindConnectionByAddress(sender);
        if (conn) {
            const std::vector<u8> empty;
            SendPacket(sender, MessageType::Disconnect, empty);
            HandleDisconnect(sender, conn->playerId);
        }
    } else if (m_Role == NetworkRole::Client) {
        ENJIN_LOG_WARN(Network, "NetworkSystem: Disconnecting due to protocol violation (%s)", reason ? reason : "unknown");
        Disconnect();
    }
}

bool NetworkSystem::RateLimitPacket(const NetworkAddress& sender, u32 size) {
    if (IsBanned(sender)) return false;
    if (m_Config.maxPacketsPerSecond <= 0.0f && m_Config.maxBytesPerSecond <= 0.0f) {
        return true;
    }

    const f32 now = m_Time;

    auto configure = [now](RateLimiter& limiter, f32 maxPerSecond, f32 burst) {
        if (maxPerSecond <= 0.0f) return;
        const f32 maxTokens = std::max(1.0f, burst);
        const f32 refillRate = std::max(0.0f, maxPerSecond);
        if (limiter.maxTokens != maxTokens || limiter.refillRate != refillRate) {
            limiter.Configure(maxTokens, refillRate, now, maxTokens);
        }
    };

    ConnectionInfo* conn = FindConnectionByAddress(sender);
    if (conn) {
        configure(conn->packetLimiter, m_Config.maxPacketsPerSecond, m_Config.burstPackets);
        configure(conn->byteLimiter, m_Config.maxBytesPerSecond, m_Config.burstBytes);

        bool ok = true;
        if (m_Config.maxPacketsPerSecond > 0.0f) {
            ok = conn->packetLimiter.Consume(1.0f, now) && ok;
        }
        if (m_Config.maxBytesPerSecond > 0.0f) {
            ok = conn->byteLimiter.Consume(static_cast<f32>(size), now) && ok;
        }
        if (!ok) {
            RegisterViolation(sender, "rate limit exceeded");
        }
        return ok;
    }

    // H4 fix: reject unknown senders when map is already at capacity to prevent
    // memory growth between cleanup ticks (e.g., IP spoofing flood)
    if (m_UnknownRateLimiters.find(sender) == m_UnknownRateLimiters.end() &&
        m_UnknownRateLimiters.size() >= kRateLimitUnknownCap) {
        ENJIN_LOG_WARN(Network, "NetworkSystem: Unknown sender rate limiter map at capacity (%zu), dropping packet", kRateLimitUnknownCap);
        return false;
    }

    RateLimitState& state = m_UnknownRateLimiters[sender];
    state.lastSeen = now;
    configure(state.packets, m_Config.maxPacketsPerSecond, m_Config.burstPackets);
    configure(state.bytes, m_Config.maxBytesPerSecond, m_Config.burstBytes);

    bool ok = true;
    if (m_Config.maxPacketsPerSecond > 0.0f) {
        ok = state.packets.Consume(1.0f, now) && ok;
    }
    if (m_Config.maxBytesPerSecond > 0.0f) {
        ok = state.bytes.Consume(static_cast<f32>(size), now) && ok;
    }

    if (!ok) {
        RegisterViolation(sender, "rate limit exceeded");
    }

    return ok;
}

void NetworkSystem::CleanupRateLimiters() {
    if (!m_UnknownRateLimiters.empty()) {
        const f32 now = m_Time;
        for (auto it = m_UnknownRateLimiters.begin(); it != m_UnknownRateLimiters.end(); ) {
            if (now - it->second.lastSeen > kRateLimitUnknownTtl) {
                it = m_UnknownRateLimiters.erase(it);
            } else {
                ++it;
            }
        }

        if (m_UnknownRateLimiters.size() > kRateLimitUnknownCap) {
            std::vector<std::pair<NetworkAddress, f32>> ordered;
            ordered.reserve(m_UnknownRateLimiters.size());
            for (const auto& entry : m_UnknownRateLimiters) {
                ordered.push_back({entry.first, entry.second.lastSeen});
            }

            std::sort(ordered.begin(), ordered.end(),
                [](const auto& a, const auto& b) { return a.second < b.second; });

            const size_t toRemove = ordered.size() - kRateLimitUnknownCap;
            for (size_t i = 0; i < toRemove; ++i) {
                m_UnknownRateLimiters.erase(ordered[i].first);
            }
        }
    }

    if (m_ViolationStates.empty()) return;
    const f32 now = m_Time;
    for (auto it = m_ViolationStates.begin(); it != m_ViolationStates.end(); ) {
        const bool expiredBan = it->second.bannedUntil > 0.0f && now > it->second.bannedUntil;
        const bool stale = (now - it->second.lastSeen) > kViolationCleanupTtl;
        if (expiredBan && stale) {
            it = m_ViolationStates.erase(it);
        } else {
            ++it;
        }
    }
}

void NetworkSystem::ProcessIncomingPackets() {
    u8 buffer[MAX_PACKET_SIZE];
    NetworkAddress sender;

    for (u32 i = 0; i < 256; i++) {  // Process up to 256 packets per frame
        i32 received = m_Transport ? m_Transport->ReceiveFrom(sender, buffer, MAX_PACKET_SIZE) : -1;
        if (received <= 0) break;

        m_BytesReceivedThisSecond += static_cast<u32>(received);

        if (static_cast<u32>(received) < PACKET_HEADER_SIZE) continue;

        HandlePacket(sender, buffer, static_cast<u32>(received));
    }
}

void NetworkSystem::HandlePacket(const NetworkAddress& sender, const u8* data, u32 size) {
    if (!RateLimitPacket(sender, size)) {
        return;
    }

    // ========================================================================
    // HMAC VERIFICATION — verify packet integrity before any processing
    // ========================================================================
    ConnectionInfo* preConn = FindConnectionByAddress(sender);

    if (m_AuthEnabled && m_SessionKeyGenerated) {
        // Authenticated packets have a 4-byte auth sequence + 32-byte HMAC appended.
        // Layout: [PacketHeader | Payload | AuthSequence(4) | HMAC(32)]
        // The four handshake messages are NOT authenticated: a client has no
        // session key until SessionKeyExchange arrives, so it can neither verify
        // nor strip the trailer on anything that precedes it. We peek at the
        // message type byte to decide.
        if (size >= PACKET_HEADER_SIZE) {
            u8 msgTypeByte = data[0];
            // Must match the exemption list in SendPacket exactly. These four
            // are the handshake, and a client has no key while they are in
            // flight.
            bool isUnauthenticatedMsg =
                (msgTypeByte == static_cast<u8>(MessageType::ConnectionRequest) ||
                 msgTypeByte == static_cast<u8>(MessageType::ConnectionAccept) ||
                 msgTypeByte == static_cast<u8>(MessageType::ConnectionReject) ||
                 msgTypeByte == static_cast<u8>(MessageType::SessionKeyExchange));

            if (!isUnauthenticatedMsg) {
                u32 authOverhead = 4 + HMAC_TAG_SIZE;  // auth sequence (4) + HMAC tag (32)
                if (size < PACKET_HEADER_SIZE + authOverhead) {
                    ENJIN_LOG_WARN(Network, "NetworkSystem: Packet too small for auth tag, dropping");
                    RegisterViolation(sender, "auth tag too small");
                    return;
                }

                // Extract auth sequence and HMAC tag from the end
                u32 authSequence = 0;
                u32 hmacOffset = size - HMAC_TAG_SIZE;
                u32 seqOffset = hmacOffset - 4;
                const u8* hmacTag = data + hmacOffset;

                // Verify HMAC over [data .. data + seqOffset + 4) (everything except the HMAC)
                u32 signedLen = seqOffset + 4;
                if (!HMACSHA256::Verify(m_SessionKey.data(), SESSION_KEY_SIZE,
                                        data, signedLen, hmacTag)) {
                    ENJIN_LOG_WARN(Network, "NetworkSystem: HMAC verification failed, dropping packet");
                    RegisterViolation(sender, "HMAC failed");
                    return;
                }

                // Read auth sequence
                authSequence = (static_cast<u32>(data[seqOffset]) << 24) |
                               (static_cast<u32>(data[seqOffset + 1]) << 16) |
                               (static_cast<u32>(data[seqOffset + 2]) << 8) |
                               (static_cast<u32>(data[seqOffset + 3]));

                // Replay protection — check sliding window
                if (preConn) {
                    if (!preConn->replayWindow.Accept(authSequence)) {
                        ENJIN_LOG_WARN(Network, "NetworkSystem: Replay detected (seq=%u), dropping packet", authSequence);
                        return;
                    }
                }

                // Strip the auth overhead for downstream processing
                size = seqOffset;
            }
        }
    }

    u32 offset = 0;
    PacketHeader header = ReadPacketHeader(data, offset, size);

    // S11: Validate payload size matches actual remaining bytes
    u32 actualPayload = (size > offset) ? size - offset : 0;
    if (header.payloadSize != actualPayload) {
        ENJIN_LOG_WARN(Network, "NetworkSystem: Payload size mismatch (header=%u, actual=%u), dropping packet",
                       header.payloadSize, actualPayload);
        RegisterViolation(sender, "payload size mismatch");
        return;
    }

    // Update connection tracking
    ConnectionInfo* conn = FindConnectionByAddress(sender);
    if (conn) {
        conn->lastRecvTime = m_Time;
        conn->packetsReceived++;

        // Update remote sequence tracking (modular arithmetic for wraparound)
        if (static_cast<i16>(header.sequence - conn->remoteSequence) > 0) {
            // Shift ack bitfield
            u16 diff = header.sequence - conn->remoteSequence;
            if (diff <= 32) {
                conn->remoteAckBitfield = (conn->remoteAckBitfield << diff) | 1;
            } else {
                conn->remoteAckBitfield = 1;
            }
            conn->remoteSequence = header.sequence;
        } else {
            // Old or duplicate packet — still record in ack bitfield
            u16 diff = conn->remoteSequence - header.sequence;
            if (diff <= 32) {
                conn->remoteAckBitfield |= (1u << diff);
            }
        }

        // Process acks from remote
        ProcessAck(*conn, header.ackSequence, header.ackBitfield);
    }

    const u8* payload = data + offset;
    u32 payloadSize = (size > offset) ? size - offset : 0;

    // S10: Validate message type is within valid range before processing
    if (header.type == 0 || header.type > static_cast<u8>(MessageType::SessionKeyExchange)) {
        ENJIN_LOG_WARN(Network, "NetworkSystem: Invalid message type %u, dropping packet", header.type);
        RegisterViolation(sender, "invalid message type");
        return;
    }

    MessageType type = static_cast<MessageType>(header.type);

    const u32 minPayload = GetMinPayloadSize(type);
    if (payloadSize < minPayload) {
        ENJIN_LOG_WARN(Network, "NetworkSystem: Payload too small for message %u (min=%u, got=%u), dropping",
                       header.type, minPayload, payloadSize);
        RegisterViolation(sender, "payload too small");
        return;
    }

    DispatchMessage(type, sender, header.senderId, payload, payloadSize);
}

// Routes one decoded message. Split out of HandlePacket so a ReliableMessage
// can unwrap its payload and run it through the same table: before this existed
// the switch had no ReliableMessage case at all, so every message sent through
// SendReliable fell into `default:` and was discarded.
void NetworkSystem::DispatchMessage(MessageType type, const NetworkAddress& sender,
                                    PlayerId senderId, const u8* payload, u32 payloadSize) {
    switch (type) {
        case MessageType::ConnectionRequest:
            HandleConnectionRequest(sender, payload, payloadSize);
            break;
        case MessageType::ConnectionAccept:
            HandleConnectionAccept(payload, payloadSize);
            break;
        case MessageType::ConnectionReject:
            HandleConnectionReject(payload, payloadSize);
            break;
        case MessageType::Disconnect:
            HandleDisconnect(sender, senderId);
            break;
        case MessageType::Heartbeat:
            HandleHeartbeat(sender, senderId);
            break;
        case MessageType::HeartbeatAck:
            HandleHeartbeatAck(sender, senderId);
            break;
        case MessageType::PlayerReady:
            HandlePlayerReady(senderId, payload, payloadSize);
            break;
        case MessageType::LobbyState:
            HandleLobbyState(payload, payloadSize);
            break;
        case MessageType::EntitySnapshot:
            HandleEntitySnapshot(senderId, payload, payloadSize);
            break;
        case MessageType::EntitySpawn:
            HandleEntitySpawn(senderId, payload, payloadSize);
            break;
        case MessageType::EntityDestroy:
            HandleEntityDestroy(senderId, payload, payloadSize);
            break;
        case MessageType::OwnershipRequest:
            HandleOwnershipRequest(senderId, payload, payloadSize);
            break;
        case MessageType::OwnershipGrant:
            HandleOwnershipGrant(payload, payloadSize);
            break;
        case MessageType::RPCCall:
            HandleRPCCall(senderId, payload, payloadSize);
            break;
        case MessageType::SessionKeyExchange:
            HandleSessionKeyExchange(sender, payload, payloadSize);
            break;
        case MessageType::ReliableMessage:
            HandleReliableMessage(sender, senderId, payload, payloadSize);
            break;
        default:
            break;
    }
}

// ============================================================================
// MESSAGE HANDLERS
// ============================================================================

void NetworkSystem::HandleConnectionRequest(const NetworkAddress& sender, const u8* payload, u32 size) {
    if (m_Role != NetworkRole::Host) return;
    if (size < 1) return;

    u32 offset = 0;
    std::string playerName = ReadString(payload, offset, size);

    // Check if already connected
    ConnectionInfo* existing = FindConnectionByAddress(sender);
    if (existing && existing->state == ConnectionState::Connected) return;

    // Check max players
    if (m_Connections.size() >= m_Config.maxPlayers - 1) {
        std::vector<u8> rejectPayload;
        WriteString(rejectPayload, "Server full");
        SendPacket(sender, MessageType::ConnectionReject, rejectPayload);
        return;
    }

    // M2 fix: Recycle player IDs instead of monotonic increment
    // Scan for the lowest unused ID (1-253, 0=host, 255=INVALID_PLAYER)
    PlayerId newId = INVALID_PLAYER;
    for (PlayerId candidate = 1; candidate < 254; candidate++) {
        bool inUse = false;
        for (const auto& c : m_Connections) {
            if (c.playerId == candidate) { inUse = true; break; }
        }
        if (!inUse) { newId = candidate; break; }
    }
    if (newId == INVALID_PLAYER) {
        std::vector<u8> rejectPayload;
        WriteString(rejectPayload, "Player ID space exhausted");
        SendPacket(sender, MessageType::ConnectionReject, rejectPayload);
        return;
    }
    ConnectionInfo conn;
    conn.address = sender;
    conn.state = ConnectionState::Connected;
    conn.playerId = newId;
    conn.playerName = playerName;
    conn.lastRecvTime = m_Time;
    m_Connections.push_back(conn);

    // Send accept
    std::vector<u8> acceptPayload;
    WriteU8(acceptPayload, newId);
    SendPacket(sender, MessageType::ConnectionAccept, acceptPayload);

    // Send session key to the new client for HMAC authentication
    if (m_AuthEnabled && m_SessionKeyGenerated) {
        SendSessionKey(sender);
    }

    // Update lobby
    LobbyPlayer lp;
    lp.id = newId;
    lp.name = playerName;
    lp.ready = false;
    lp.isHost = false;
    m_LobbyPlayers.push_back(lp);

    // Broadcast player joined + full lobby state
    std::vector<u8> joinPayload;
    WriteU8(joinPayload, newId);
    WriteString(joinPayload, playerName);
    SendToAll(MessageType::PlayerJoined, joinPayload, newId);
    BroadcastLobbyState();

    // Send existing networked entities to the new client
    for (const auto& [netId, entity] : m_NetworkToEntity) {
        if (!m_World) continue;
        auto* netComp = m_World->GetComponent<ECS::NetworkIdentityComponent>(entity);
        if (!netComp) continue;

        std::vector<u8> spawnPayload;
        WriteU32(spawnPayload, netId);
        WriteU8(spawnPayload, netComp->ownerId);
        auto* transform = m_World->GetComponent<ECS::TransformComponent>(entity);
        if (transform) {
            WriteVector3(spawnPayload, transform->position);
            WriteQuaternion(spawnPayload, transform->rotation);
            WriteVector3(spawnPayload, transform->scale);
        } else {
            WriteVector3(spawnPayload, Math::Vector3(0, 0, 0));
            WriteQuaternion(spawnPayload, Math::Quaternion(0, 0, 0, 1));
            WriteVector3(spawnPayload, Math::Vector3(1, 1, 1));
        }
        SendPacket(sender, MessageType::EntitySpawn, spawnPayload);
    }

    ENJIN_LOG_INFO(Network, "NetworkSystem: Player '%s' joined (id=%u)", playerName.c_str(), newId);
}

void NetworkSystem::HandleConnectionAccept(const u8* payload, u32 size) {
    if (m_Role != NetworkRole::Client) return;
    if (size < 1) return;
    // C2 fix: only accept from host we connected to (already validated by connection lookup)
    // If we're already connected, ignore duplicate accepts
    if (!m_Connections.empty() && m_Connections[0].state == ConnectionState::Connected) return;

    u32 offset = 0;
    PlayerId assignedId = ReadU8(payload, offset, size);

    m_LocalPlayerId = assignedId;
    if (!m_Connections.empty()) {
        m_Connections[0].state = ConnectionState::Connected;
        m_Connections[0].playerId = 0;  // Host is always 0
    }

    ENJIN_LOG_INFO(Network, "NetworkSystem: Connected to host, assigned player id %u", assignedId);
}

void NetworkSystem::HandleConnectionReject(const u8* payload, u32 size) {
    if (m_Role != NetworkRole::Client) return;
    if (size < 1) return;

    u32 offset = 0;
    std::string reason = ReadString(payload, offset, size);

    ENJIN_LOG_WARN(Network, "NetworkSystem: Connection rejected: %s", reason.c_str());
    Disconnect();
}

void NetworkSystem::HandleDisconnect(const NetworkAddress& sender, PlayerId senderId) {
    if (m_Role == NetworkRole::Host) {
        // Remove client connection
        for (auto it = m_Connections.begin(); it != m_Connections.end(); ++it) {
            if (it->address == sender) {
                PlayerId removedId = it->playerId;
                m_Connections.erase(it);

                // Remove from lobby
                m_LobbyPlayers.erase(
                    std::remove_if(m_LobbyPlayers.begin(), m_LobbyPlayers.end(),
                        [removedId](const LobbyPlayer& lp) { return lp.id == removedId; }),
                    m_LobbyPlayers.end());

                // Notify others
                std::vector<u8> payload;
                WriteU8(payload, removedId);
                SendToAll(MessageType::PlayerLeft, payload);
                BroadcastLobbyState();

                ENJIN_LOG_INFO(Network, "NetworkSystem: Player %u disconnected", removedId);
                break;
            }
        }
    } else if (m_Role == NetworkRole::Client) {
        ENJIN_LOG_INFO(Network, "NetworkSystem: Host disconnected");
        Disconnect();
    }
}

void NetworkSystem::HandleHeartbeat(const NetworkAddress& sender, PlayerId senderId) {
    // Send ack back (S-M11: avoid static vector destruction order issues)
    const std::vector<u8> empty;
    SendPacket(sender, MessageType::HeartbeatAck, empty);
}

void NetworkSystem::HandleHeartbeatAck(const NetworkAddress& sender, PlayerId senderId) {
    // M1 fix: RTT is computed in ProcessAck via per-packet send timestamps (H1 fix).
    // HeartbeatAck is now handled explicitly instead of falling through to default.
    // Connection tracking (lastRecvTime, sequence) is already updated in HandlePacket.
}

void NetworkSystem::HandlePlayerReady(PlayerId senderId, const u8* payload, u32 size) {
    if (m_Role != NetworkRole::Host) return;
    if (size < 1) return;

    u32 offset = 0;
    bool ready = ReadU8(payload, offset, size) != 0;

    for (auto& lp : m_LobbyPlayers) {
        if (lp.id == senderId) {
            lp.ready = ready;
            break;
        }
    }
    BroadcastLobbyState();
}

void NetworkSystem::HandleLobbyState(const u8* payload, u32 size) {
    if (m_Role != NetworkRole::Client) return;
    if (size < 1) return;

    u32 offset = 0;
    u8 playerCount = ReadU8(payload, offset, size);

    m_LobbyPlayers.clear();
    for (u8 i = 0; i < playerCount && offset < size; i++) {
        LobbyPlayer lp;
        lp.id = ReadU8(payload, offset, size);
        lp.name = ReadString(payload, offset, size);
        lp.ready = ReadU8(payload, offset, size) != 0;
        lp.isHost = ReadU8(payload, offset, size) != 0;
        m_LobbyPlayers.push_back(lp);
    }
}

// The host is always PlayerId 0 (StartHost assigns m_LocalPlayerId = 0).
static constexpr PlayerId kHostPlayerId = 0;

bool NetworkSystem::IsSenderAuthoritativeFor(PlayerId senderId, ECS::Entity entity) const {
    if (senderId == INVALID_PLAYER) return false;

    if (!IsHost()) {
        // We are a client: only the host drives entity state. A packet from a
        // peer client is authentic (it passed HMAC) but never authoritative.
        return senderId == kHostPlayerId;
    }

    // We are the host. A client may only act on entities it owns. Our own
    // loopback traffic is always allowed.
    if (senderId == m_LocalPlayerId) return true;
    if (entity == ECS::INVALID_ENTITY) return false;
    if (!m_World) return false;

    auto* netId = m_World->GetComponent<ECS::NetworkIdentityComponent>(entity);
    return netId && netId->ownerId == senderId;
}

void NetworkSystem::HandleEntitySnapshot(PlayerId senderId, const u8* payload, u32 size) {
    if (!m_World) return;
    if (size < 2) return;

    u32 offset = 0;
    u16 count = ReadU16(payload, offset, size);

    // NET-5: Cap entity count to prevent excessive processing from malicious packets
    if (count > 256) {
        ENJIN_LOG_WARN(Network, "NetworkSystem: Entity snapshot count %u exceeds cap (256), dropping", count);
        return;
    }

    for (u16 i = 0; i < count && offset < size; i++) {
        if (size - offset < 9) return;

        EntitySnapshot snap;
        snap.networkId = ReadU32(payload, offset, size);
        snap.fieldMask = ReadU8(payload, offset, size);
        snap.tick = ReadU32(payload, offset, size);

        u32 required = 0;
        if (snap.fieldMask & SnapPosition) required += 12;
        if (snap.fieldMask & SnapRotation) required += 16;
        if (snap.fieldMask & SnapScale) required += 12;
        if (snap.fieldMask & SnapVelocity) required += 12;
        if (size - offset < required) return;

        if (snap.fieldMask & SnapPosition) snap.position = ReadVector3(payload, offset, size);
        if (snap.fieldMask & SnapRotation) snap.rotation = ReadQuaternion(payload, offset, size);
        if (snap.fieldMask & SnapScale) snap.scale = ReadVector3(payload, offset, size);
        if (snap.fieldMask & SnapVelocity) snap.velocity = ReadVector3(payload, offset, size);

        // N4: Validate all floats are finite (reject NaN/Inf injection)
        auto isFiniteVec3 = [](const Math::Vector3& v) {
            return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
        };
        auto isFiniteQuat = [](const Math::Quaternion& q) {
            return std::isfinite(q.x) && std::isfinite(q.y) && std::isfinite(q.z) && std::isfinite(q.w);
        };
        if (!isFiniteVec3(snap.position) || !isFiniteQuat(snap.rotation) ||
            !isFiniteVec3(snap.scale) || !isFiniteVec3(snap.velocity)) {
            continue; // Discard malformed snapshot
        }

        // Find entity
        auto it = m_NetworkToEntity.find(snap.networkId);
        if (it == m_NetworkToEntity.end()) continue;

        ECS::Entity entity = it->second;
        auto* netId = m_World->GetComponent<ECS::NetworkIdentityComponent>(entity);
        if (!netId || netId->isLocallyOwned) continue;  // Don't overwrite own entities

        // isLocallyOwned only protects OUR entities. Without this, any admitted
        // peer could teleport an entity belonging to a third player.
        if (!IsSenderAuthoritativeFor(senderId, entity)) {
            ENJIN_LOG_WARN(Network, "NetworkSystem: player %u snapshot for entity it does not own (NetworkId=%u), dropped",
                           (unsigned)senderId, snap.networkId);
            continue;
        }

        // Push into interpolation buffer
        InterpolationState state;
        state.position = snap.position;
        state.rotation = snap.rotation;
        state.scale = snap.scale;
        state.timestamp = m_Time;
        m_InterpBuffers[snap.networkId].Push(state);

        // Update network velocity on NetworkTransformComponent
        auto* netTrans = m_World->GetComponent<ECS::NetworkTransformComponent>(entity);
        if (netTrans) {
            netTrans->networkVelocity = snap.velocity;
        }

        // Ensure this remote body is network-driven (kinematic). Done here, per snapshot, so it
        // takes effect even if the Rigidbody was added after the entity spawned.
        ApplyPhysicsAuthority(entity, /*isLocallyOwned=*/false);
    }
}

void NetworkSystem::HandleEntitySpawn(PlayerId senderId, const u8* payload, u32 size) {
    if (!m_World || m_Role != NetworkRole::Client) return;
    if (size < 45) return;

    // Client-only handler, so the host rule is the whole gate: a peer client
    // must not be able to conjure entities into our world.
    if (!IsSenderAuthoritativeFor(senderId, ECS::INVALID_ENTITY)) {
        ENJIN_LOG_WARN(Network, "NetworkSystem: entity spawn from non-host player %u, dropped", (unsigned)senderId);
        return;
    }

    u32 offset = 0;
    NetworkId netId = ReadU32(payload, offset, size);
    PlayerId ownerId = ReadU8(payload, offset, size);
    Math::Vector3 position = ReadVector3(payload, offset, size);
    Math::Quaternion rotation = ReadQuaternion(payload, offset, size);
    Math::Vector3 scale = ReadVector3(payload, offset, size);

    // Check if already exists
    if (m_NetworkToEntity.count(netId)) return;

    // Create entity
    ECS::Entity entity = m_World->CreateEntity();
    m_World->AddComponent<ECS::NameComponent>(entity, ECS::NameComponent{"NetworkEntity_" + std::to_string(netId)});

    ECS::TransformComponent transform;
    transform.position = position;
    transform.rotation = rotation;
    transform.scale = scale;
    transform.teleportedThisFrame = true;  // Spawned entity — zero velocity to prevent TAA ghosting
    m_World->AddComponent<ECS::TransformComponent>(entity, transform);

    ECS::NetworkIdentityComponent netComp;
    netComp.networkId = netId;
    netComp.ownerId = ownerId;
    netComp.isLocallyOwned = (ownerId == m_LocalPlayerId);
    m_World->AddComponent<ECS::NetworkIdentityComponent>(entity, netComp);

    ECS::NetworkTransformComponent netTrans;
    netTrans.lastSyncedPosition = position;
    netTrans.lastSyncedRotation = rotation;
    netTrans.lastSyncedScale = scale;
    m_World->AddComponent<ECS::NetworkTransformComponent>(entity, netTrans);

    m_NetworkToEntity[netId] = entity;
    m_EntityToNetwork[entity] = netId;

    if (netId >= m_NextNetworkId) m_NextNetworkId = netId + 1;

    ENJIN_LOG_INFO(Network, "NetworkSystem: Spawned remote entity NetworkId=%u owner=%u", netId, ownerId);
}

void NetworkSystem::HandleEntityDestroy(PlayerId senderId, const u8* payload, u32 size) {
    if (!m_World) return;
    if (size < 4) return;

    u32 offset = 0;
    NetworkId netId = ReadU32(payload, offset, size);

    auto it = m_NetworkToEntity.find(netId);
    if (it == m_NetworkToEntity.end()) return;

    ECS::Entity entity = it->second;

    // This handler used to check only m_World and the payload size, so any
    // admitted peer could delete any networked entity on every other peer.
    if (!IsSenderAuthoritativeFor(senderId, entity)) {
        ENJIN_LOG_WARN(Network, "NetworkSystem: player %u tried to destroy entity it does not own (NetworkId=%u), dropped",
                       (unsigned)senderId, netId);
        return;
    }

    m_EntityToNetwork.erase(entity);
    m_NetworkToEntity.erase(it);
    m_InterpBuffers.erase(netId);

    m_World->DestroyEntity(entity);
    ENJIN_LOG_INFO(Network, "NetworkSystem: Destroyed remote entity NetworkId=%u", netId);
}

void NetworkSystem::HandleOwnershipRequest(PlayerId senderId, const u8* payload, u32 size) {
    if (m_Role != NetworkRole::Host) return;
    if (size < 4) return;

    // NET-3: Rate-limit ownership requests per player (max 1 per 500ms)
    ConnectionInfo* senderConn = FindConnectionByPlayerId(senderId);
    if (senderConn) {
        f32 elapsed = m_Time - senderConn->lastOwnershipRequestTime;
        if (elapsed < 0.5f) {
            ENJIN_LOG_WARN(Network, "NetworkSystem: Ownership request rate-limited for player %u (%.0fms since last)",
                           senderId, elapsed * 1000.0f);
            return;
        }
        senderConn->lastOwnershipRequestTime = m_Time;
    }

    u32 offset = 0;
    NetworkId netId = ReadU32(payload, offset, size);

    auto it = m_NetworkToEntity.find(netId);
    if (it == m_NetworkToEntity.end()) return;

    ECS::Entity entity = it->second;
    auto* netComp = m_World->GetComponent<ECS::NetworkIdentityComponent>(entity);
    if (!netComp) return;

    // Grant ownership
    PlayerId oldOwner = netComp->ownerId;
    netComp->ownerId = senderId;
    netComp->isLocallyOwned = (senderId == m_LocalPlayerId);
    ApplyPhysicsAuthority(entity, netComp->isLocallyOwned);

    // Notify requester
    {
        std::vector<u8> grantPayload;
        WriteU32(grantPayload, netId);
        WriteU8(grantPayload, senderId);
        ConnectionInfo* conn = FindConnectionByPlayerId(senderId);
        if (conn) SendPacket(conn->address, MessageType::OwnershipGrant, grantPayload);
    }

    // Notify old owner (revoke)
    if (oldOwner != senderId && oldOwner != INVALID_PLAYER) {
        std::vector<u8> revokePayload;
        WriteU32(revokePayload, netId);
        ConnectionInfo* oldConn = FindConnectionByPlayerId(oldOwner);
        if (oldConn) SendPacket(oldConn->address, MessageType::OwnershipRevoke, revokePayload);
    }
}

void NetworkSystem::HandleOwnershipGrant(const u8* payload, u32 size) {
    if (!m_World) return;
    if (size < 5) return;

    u32 offset = 0;
    NetworkId netId = ReadU32(payload, offset, size);
    PlayerId newOwner = ReadU8(payload, offset, size);

    auto it = m_NetworkToEntity.find(netId);
    if (it == m_NetworkToEntity.end()) return;

    auto* netComp = m_World->GetComponent<ECS::NetworkIdentityComponent>(it->second);
    if (netComp) {
        netComp->ownerId = newOwner;
        netComp->isLocallyOwned = (newOwner == m_LocalPlayerId);
        ApplyPhysicsAuthority(it->second, netComp->isLocallyOwned);
    }
}

void NetworkSystem::ApplyPhysicsAuthority(ECS::Entity entity, bool isLocallyOwned) {
    if (!m_World) return;
    const bool networkDriven = !isLocallyOwned;
    // Remote-owned -> network-driven (kinematic); owned -> simulate + replicate locally.
    if (auto* rb = m_World->GetComponent<ECS::RigidbodyComponent>(entity)) {
        rb->networkControlled = networkDriven;        // 3D (Jolt)
    }
    if (auto* body2d = m_World->GetComponent<Physics::Body2DComponent>(entity)) {
        body2d->networkControlled = networkDriven;    // 2D (Box2D)
    }
}

void NetworkSystem::HandleReliableMessage(const NetworkAddress& sender, PlayerId senderId,
                                          const u8* payload, u32 size) {
    // Wrapper: [u16 outerSequence][u32 messageId][u16 fragIndex][u16 fragCount][u8 innerType][chunk]
    //
    // The outer sequence is only there so the sender can match an ack; the ack
    // itself rides the next packet header back, handled in ProcessAck. The
    // message id is the one field a retransmit does NOT change, so it is both
    // the de-duplication key and the reassembly key.
    u32 offset = 0;
    ReadU16(payload, offset, size);              // outer sequence, already acked by header
    const u32 messageId = ReadU32(payload, offset, size);
    const u16 fragIndex = ReadU16(payload, offset, size);
    const u16 fragCount = ReadU16(payload, offset, size);

    if (fragCount == 0 || fragIndex >= fragCount || fragCount > RELIABLE_MAX_FRAGMENTS) {
        ENJIN_LOG_WARN(Network, "NetworkSystem: Reliable fragment %u of %u is out of range, dropping",
                       fragIndex, fragCount);
        RegisterViolation(sender, "bad reliable fragment header");
        return;
    }

    // Deliver each fragment once. The ack can be lost as easily as the chunk,
    // and the retransmit that follows is indistinguishable from a first
    // delivery at every other layer.
    if (ConnectionInfo* conn = FindConnectionByAddress(sender)) {
        // Owe an ack either way: a duplicate means our last one did not arrive.
        conn->ackPending = true;
        if (!conn->MarkReliableDelivered(messageId, fragIndex)) return;
    }

    // Only the FIRST fragment carries the inner type byte; the rest are payload.
    const u8* chunk = payload + offset;
    u32 chunkSize = (size > offset) ? size - offset : 0;

    if (fragCount == 1) {
        if (chunkSize < 1) {
            RegisterViolation(sender, "empty reliable message");
            return;
        }
        DispatchReliablePayload(sender, senderId, chunk[0], chunk + 1, chunkSize - 1);
        return;
    }

    auto& perSender = m_Reassembly[sender];
    auto& entry = perSender[messageId];
    if (entry.fragCount == 0) {
        entry.fragCount = fragCount;
        entry.chunks.resize(fragCount);
    } else if (entry.fragCount != fragCount) {
        // The same message id claiming a different length is not a thing a
        // sender does; drop the whole reassembly rather than trust either.
        ENJIN_LOG_WARN(Network, "NetworkSystem: Reliable message %u changed fragment count, dropping", messageId);
        RegisterViolation(sender, "inconsistent fragment count");
        perSender.erase(messageId);
        return;
    }

    entry.lastActivity = m_Time;
    if (entry.chunks[fragIndex].empty() && chunkSize > 0) {
        entry.totalBytes += chunkSize;
        if (entry.totalBytes > RELIABLE_MAX_MESSAGE_BYTES) {
            ENJIN_LOG_ERROR(Network, "NetworkSystem: Reassembled message exceeds %u bytes, dropping",
                            RELIABLE_MAX_MESSAGE_BYTES);
            RegisterViolation(sender, "oversized reassembly");
            perSender.erase(messageId);
            return;
        }
        entry.chunks[fragIndex].assign(chunk, chunk + chunkSize);
        entry.received++;
    }

    if (entry.received < entry.fragCount) return;

    // Complete. Concatenate in index order; fragment 0 leads with the type byte.
    std::vector<u8> whole;
    whole.reserve(entry.totalBytes);
    for (const auto& c : entry.chunks) {
        whole.insert(whole.end(), c.begin(), c.end());
    }
    perSender.erase(messageId);
    if (perSender.empty()) m_Reassembly.erase(sender);

    if (whole.empty()) {
        RegisterViolation(sender, "empty reassembled message");
        return;
    }
    DispatchReliablePayload(sender, senderId, whole[0], whole.data() + 1, static_cast<u32>(whole.size() - 1));
}

// Shared tail of both the single-datagram and the reassembled paths: validate
// the inner type, then run it through the same table an arriving packet uses.
void NetworkSystem::DispatchReliablePayload(const NetworkAddress& sender, PlayerId senderId,
                                            u8 innerTypeByte, const u8* innerPayload, u32 innerSize) {
    if (innerTypeByte == 0 || innerTypeByte > static_cast<u8>(MessageType::SessionKeyExchange)) {
        ENJIN_LOG_WARN(Network, "NetworkSystem: Reliable message carried invalid type %u, dropping", innerTypeByte);
        RegisterViolation(sender, "invalid reliable inner type");
        return;
    }

    const MessageType innerType = static_cast<MessageType>(innerTypeByte);

    // A reliable message may not carry another one. Nothing sends that, and
    // honouring it would let one packet recurse.
    if (innerType == MessageType::ReliableMessage) {
        ENJIN_LOG_WARN(Network, "NetworkSystem: Reliable message nested inside a reliable message, dropping");
        RegisterViolation(sender, "nested reliable message");
        return;
    }

    const u32 minPayload = GetMinPayloadSize(innerType);
    if (innerSize < minPayload) {
        ENJIN_LOG_WARN(Network, "NetworkSystem: Reliable payload too small for message %u (min=%u, got=%u), dropping",
                       innerTypeByte, minPayload, innerSize);
        RegisterViolation(sender, "reliable payload too small");
        return;
    }

    DispatchMessage(innerType, sender, senderId, innerPayload, innerSize);
}

// A sender that disappears mid-message leaves its chunks behind. Without this
// they are held for the life of the process.
void NetworkSystem::ExpireReassemblies() {
    for (auto sit = m_Reassembly.begin(); sit != m_Reassembly.end();) {
        auto& perSender = sit->second;
        for (auto it = perSender.begin(); it != perSender.end();) {
            if (m_Time - it->second.lastActivity > RELIABLE_REASSEMBLY_TIMEOUT) {
                ENJIN_LOG_WARN(Network, "NetworkSystem: Abandoning reliable message %u, %u of %u fragments arrived",
                               it->first, it->second.received, it->second.fragCount);
                it = perSender.erase(it);
            } else {
                ++it;
            }
        }
        if (perSender.empty()) sit = m_Reassembly.erase(sit);
        else ++sit;
    }
}

void NetworkSystem::HandleRPCCall(PlayerId senderId, const u8* payload, u32 size) {
    if (size < 9) return;
    u32 offset = 0;
    u32 nameHash = ReadU32(payload, offset, size);
    PlayerId targetId = ReadU8(payload, offset, size);
    u32 dataSize = ReadU32(payload, offset, size);

    // Check if this is for us
    if (targetId != INVALID_PLAYER && targetId != m_LocalPlayerId) {
        // Forward (host only)
        if (m_Role == NetworkRole::Host) {
            // M6 fix: validate dataSize consistency before forwarding to other clients
            if (dataSize > size - offset) {
                ENJIN_LOG_WARN(Network, "RPC forward rejected: data size mismatch (declared %u, remaining %u)", dataSize, size - offset);
                return;
            }
            ConnectionInfo* conn = FindConnectionByPlayerId(targetId);
            if (conn) {
                std::vector<u8> fwdPayload(payload, payload + size);
                // Forward with the reliability this RPC was declared with. A
                // plain SendPacket here also meant a forwarded message larger
                // than one datagram was unsendable.
                auto reg = m_RPCRegistry.find(nameHash);
                if (reg != m_RPCRegistry.end() && reg->second.reliable) {
                    SendReliable(conn->address, MessageType::RPCCall, fwdPayload);
                } else {
                    SendPacket(conn->address, MessageType::RPCCall, fwdPayload);
                }
            }
        }
        return;
    }

    // N9: Drop RPC if declared data size exceeds remaining bytes
    if (dataSize > size - offset) {
        ENJIN_LOG_WARN(Network, "RPC data size mismatch (declared %u, remaining %u)", dataSize, size - offset);
        return;
    }

    auto it = m_RPCRegistry.find(nameHash);
    if (it != m_RPCRegistry.end()) {
        const u8* rpcData = payload + offset;
        it->second.callback(senderId, rpcData, static_cast<u32>(dataSize));
    }

    // If host and broadcast target, forward to all other clients
    if (m_Role == NetworkRole::Host && targetId == INVALID_PLAYER) {
        std::vector<u8> fwdPayload(payload, payload + size);
        auto reg = m_RPCRegistry.find(nameHash);
        if (reg != m_RPCRegistry.end() && reg->second.reliable) {
            for (auto& conn : m_Connections) {
                if (conn.state == ConnectionState::Connected && conn.playerId != senderId) {
                    SendReliable(conn.address, MessageType::RPCCall, fwdPayload);
                }
            }
        } else {
            SendToAll(MessageType::RPCCall, fwdPayload, senderId);
        }
    }
}

// ============================================================================
// SENDING
// ============================================================================

void NetworkSystem::SendPacket(const NetworkAddress& addr, MessageType type, const std::vector<u8>& payload) {
    // S-C3: Reject payload too large for u16 size field
    if (payload.size() > 65535) {
        ENJIN_LOG_ERROR(Network, "Packet payload too large: %zu bytes (max 65535)", payload.size());
        return;
    }

    ConnectionInfo* conn = FindConnectionByAddress(addr);

    PacketHeader header;
    header.type = static_cast<u8>(type);
    header.senderId = m_LocalPlayerId;
    header.payloadSize = static_cast<u16>(payload.size());

    if (conn) {
        header.sequence = conn->localSequence++;
        header.ackSequence = conn->remoteSequence;
        header.ackBitfield = conn->remoteAckBitfield;
        conn->packetsSent++;
        conn->lastSendTime = m_Time;
        // H1 fix: record per-packet send timestamp for accurate RTT measurement
        conn->RecordSendTime(header.sequence, m_Time);
    }

    m_SendBuffer.clear();
    m_SendBuffer.reserve(PACKET_HEADER_SIZE + payload.size() + 4 + HMAC_TAG_SIZE);
    WritePacketHeader(m_SendBuffer, header);
    m_SendBuffer.insert(m_SendBuffer.end(), payload.begin(), payload.end());

    // Append HMAC authentication if enabled.
    //
    // The whole HANDSHAKE is exempt, not just ConnectionRequest. A client has
    // no session key until SessionKeyExchange arrives, so it can neither verify
    // an authenticated packet nor strip its 36-byte trailer (4-byte sequence +
    // 32-byte HMAC) before the payload-size check. Authenticating the reply
    // therefore made every client drop the host's ConnectionAccept with
    // "Payload size mismatch (header=25, actual=61)" -- exactly 36 bytes over --
    // and no client could ever reach the Connected state.
    //
    // ConnectionRequest alone was exempt, and the comment above the receive-side
    // check even claimed SessionKeyExchange was too. It was not.
    const bool isHandshake = (type == MessageType::ConnectionRequest ||
                              type == MessageType::ConnectionAccept ||
                              type == MessageType::ConnectionReject ||
                              type == MessageType::SessionKeyExchange);
    if (m_AuthEnabled && m_SessionKeyGenerated && !isHandshake) {
        AuthenticateOutgoing(m_SendBuffer, conn);
    }

    if (m_Transport) m_Transport->SendTo(addr, m_SendBuffer.data(), static_cast<u32>(m_SendBuffer.size()));
    m_BytesSentThisSecond += static_cast<u32>(m_SendBuffer.size());
}

void NetworkSystem::SendToAll(MessageType type, const std::vector<u8>& payload, PlayerId exclude) {
    for (auto& conn : m_Connections) {
        if (conn.state == ConnectionState::Connected && conn.playerId != exclude) {
            SendPacket(conn.address, type, payload);
        }
    }
}

bool NetworkSystem::SendReliable(const NetworkAddress& addr, MessageType type, const std::vector<u8>& payload) {
    // Build the inner packet data
    std::vector<u8> innerPacket;
    WriteU8(innerPacket, static_cast<u8>(type));
    innerPacket.insert(innerPacket.end(), payload.begin(), payload.end());

    if (innerPacket.size() > RELIABLE_MAX_MESSAGE_BYTES) {
        ENJIN_LOG_ERROR(Network, "Reliable message too large: %zu bytes (max %u)",
                        innerPacket.size(), RELIABLE_MAX_MESSAGE_BYTES);
        return false;
    }

    // Split into datagram-sized chunks. One chunk is the common case and still
    // goes through this path, so there is only one wire format to get right.
    const usize chunkSize = RELIABLE_CHUNK_PAYLOAD;
    const usize fragTotal = (innerPacket.size() + chunkSize - 1) / chunkSize;
    const u16 fragCount = static_cast<u16>(fragTotal == 0 ? 1 : fragTotal);

    if (fragCount > RELIABLE_MAX_FRAGMENTS) {
        ENJIN_LOG_ERROR(Network, "Reliable message needs %u fragments (max %u)",
                        fragCount, RELIABLE_MAX_FRAGMENTS);
        return false;
    }

    // Reserve the whole message or none of it. A partial send is worse than a
    // refused one: the receiver waits for chunks that were never queued, and
    // the sender reports success.
    if (m_ReliableOutbox.size() + fragCount > RELIABLE_OUTBOX_CAP) {
        ENJIN_LOG_WARN(Network, "Reliable outbox full (%zu of %u), dropping a %u-fragment message",
                       m_ReliableOutbox.size(), RELIABLE_OUTBOX_CAP, fragCount);
        return false;
    }

    const u32 messageId = m_NextReliableMessageId++;
    if (m_NextReliableMessageId == 0) m_NextReliableMessageId = 1;  // 0 means "no id"

    ConnectionInfo* conn = FindConnectionByAddress(addr);

    (void)conn;
    for (u16 frag = 0; frag < fragCount; frag++) {
        const usize begin = static_cast<usize>(frag) * chunkSize;
        const usize end = std::min(begin + chunkSize, innerPacket.size());

        ReliableMessage rm;
        rm.messageId = messageId;
        rm.fragIndex = frag;
        rm.fragCount = fragCount;
        rm.sent = false;
        rm.lastSendTime = m_Time;
        rm.firstSendTime = m_Time;
        rm.retryCount = 0;
        rm.data.assign(innerPacket.begin() + begin, innerPacket.begin() + end);
        rm.target = addr;
        m_ReliableOutbox.push_back(rm);
    }

    // Put what the budget allows on the wire now; the rest goes out over the
    // next frames from Update.
    FlushPendingReliable();
    return true;
}

// The outgoing budget mirrors the receiver's own limits, because both ends read
// the same config. Without it a 200 KB scene sync arrives as 171 datagrams in
// one frame, the receiver's token bucket rejects all but the burst allowance,
// and the sender is registered as a violator for sending the thing it was asked
// to send.
// Share of the configured rate that bulk reliable traffic may use. The rest is
// left for the protocol's own packets.
static constexpr f32 kReliableSendShare = 0.6f;

void NetworkSystem::FlushPendingReliable() {
    const f32 now = m_Time;

    auto configure = [now](RateLimiter& limiter, f32 maxPerSecond, f32 burst) {
        if (maxPerSecond <= 0.0f) return;
        const f32 maxTokens = std::max(1.0f, burst);
        const f32 refillRate = std::max(0.0f, maxPerSecond);
        if (limiter.maxTokens != maxTokens || limiter.refillRate != refillRate) {
            limiter.Configure(maxTokens, refillRate, now, maxTokens);
        }
    };
    // Headroom. The receiver's bucket is also drained by heartbeats, acks and
    // entity snapshots, so pacing reliable fragments at exactly the configured
    // rate overshoots it. The overshoot is small per second and cumulative over
    // a long transfer: a 171-fragment message lost a handful of chunks, each
    // retried until its retry count ran out, and the message then never
    // completed. Measured: at 100% every size up to 120 fragments arrived and
    // 171 did not.
    configure(m_ReliableSendPackets, m_Config.maxPacketsPerSecond * kReliableSendShare,
              m_Config.burstPackets * kReliableSendShare);
    configure(m_ReliableSendBytes, m_Config.maxBytesPerSecond * kReliableSendShare,
              m_Config.burstBytes * kReliableSendShare);

    for (auto& rm : m_ReliableOutbox) {
        if (rm.sent) continue;

        const f32 wireSize = static_cast<f32>(PACKET_HEADER_SIZE + 11 + rm.data.size());
        if (m_Config.maxPacketsPerSecond > 0.0f && !m_ReliableSendPackets.Consume(1.0f, now)) break;
        if (m_Config.maxBytesPerSecond > 0.0f && !m_ReliableSendBytes.Consume(wireSize, now)) break;

        ConnectionInfo* conn = FindConnectionByAddress(rm.target);
        rm.sequence = conn ? conn->localSequence : 0;

        std::vector<u8> wrappedPayload;
        WriteU16(wrappedPayload, rm.sequence);
        WriteU32(wrappedPayload, rm.messageId);
        WriteU16(wrappedPayload, rm.fragIndex);
        WriteU16(wrappedPayload, rm.fragCount);
        wrappedPayload.insert(wrappedPayload.end(), rm.data.begin(), rm.data.end());
        SendPacket(rm.target, MessageType::ReliableMessage, wrappedPayload);

        rm.sent = true;
        rm.lastSendTime = m_Time;
    }
}

void NetworkSystem::UpdateHeartbeats(f32 dt) {
    // Answer reliable traffic in the SAME frame it arrived. The payload does
    // not matter; every packet header carries this connection's ack sequence
    // and bitfield, which is what retires the sender's outbox.
    {
        const std::vector<u8> empty;
        for (auto& conn : m_Connections) {
            if (conn.ackPending && conn.state == ConnectionState::Connected) {
                conn.ackPending = false;
                SendPacket(conn.address, MessageType::Heartbeat, empty);
            }
        }
    }

    m_HeartbeatTimer += dt;
    if (m_HeartbeatTimer < HEARTBEAT_INTERVAL) return;
    m_HeartbeatTimer -= HEARTBEAT_INTERVAL;

    // S-M11: avoid static vector destruction order issues
    const std::vector<u8> empty;
    for (auto& conn : m_Connections) {
        if (conn.state == ConnectionState::Connected) {
            SendPacket(conn.address, MessageType::Heartbeat, empty);
        }
    }
}

void NetworkSystem::CheckTimeouts(f32 dt) {
    for (auto it = m_Connections.begin(); it != m_Connections.end();) {
        if (it->state == ConnectionState::Connected) {
            f32 timeSinceRecv = m_Time - it->lastRecvTime;
            if (timeSinceRecv > CONNECTION_TIMEOUT) {
                PlayerId removedId = it->playerId;
                ENJIN_LOG_WARN(Network, "NetworkSystem: Player %u timed out (%.1fs)",
                               removedId, timeSinceRecv);

                // Remove from lobby
                m_LobbyPlayers.erase(
                    std::remove_if(m_LobbyPlayers.begin(), m_LobbyPlayers.end(),
                        [removedId](const LobbyPlayer& lp) { return lp.id == removedId; }),
                    m_LobbyPlayers.end());

                it = m_Connections.erase(it);

                if (m_Role == NetworkRole::Host) {
                    std::vector<u8> payload;
                    WriteU8(payload, removedId);
                    SendToAll(MessageType::PlayerLeft, payload);
                    BroadcastLobbyState();
                } else {
                    // Client lost connection to host
                    Disconnect();
                    return;
                }
                continue;
            }
        } else if (it->state == ConnectionState::Connecting) {
            // Client retry connection request
            f32 timeSinceRecv = m_Time - it->lastRecvTime;
            if (timeSinceRecv > CONNECTION_TIMEOUT) {
                ENJIN_LOG_WARN(Network, "NetworkSystem: Connection attempt timed out");
                Disconnect();
                return;
            }
            // Retry every 2 seconds
            if (m_Time - it->lastSendTime > 2.0f) {
                std::vector<u8> payload;
                WriteString(payload, m_LocalPlayerName);
                SendPacket(it->address, MessageType::ConnectionRequest, payload);
            }
        }
        ++it;
    }
}

void NetworkSystem::UpdateReliableMessages(f32 dt) {
    (void)dt;

    // A retransmit is re-QUEUED rather than sent here, so it goes out through
    // the same budget a first send does. Retransmitting 171 fragments the
    // instant their retry timer expires is the same burst the pacing exists to
    // avoid, and it would arrive at a receiver that is already rate-limiting.
    for (auto it = m_ReliableOutbox.begin(); it != m_ReliableOutbox.end();) {
        if (!it->sent) { ++it; continue; }   // Still waiting on the send budget

        const f32 elapsed = m_Time - it->lastSendTime;
        if (elapsed >= RELIABLE_RETRY_INTERVAL) {
            if (it->retryCount >= RELIABLE_MAX_RETRIES) {
                // Abandon the WHOLE message, not just this chunk. Dropping one
                // fragment leaves the receiver holding an incomplete reassembly
                // it can never finish, while the sender carries on as though it
                // had delivered -- silence on both sides. The receiver's own
                // reassembly timeout eventually reclaims the buffers.
                const u32 lostId = it->messageId;
                const NetworkAddress lostTarget = it->target;
                const u16 lostFrag = it->fragIndex;
                const u16 lostCount = it->fragCount;
                if (lostCount > 1) {
                    ENJIN_LOG_ERROR(Network,
                        "NetworkSystem: Reliable message %u abandoned after %d retries on fragment %u of %u",
                        lostId, RELIABLE_MAX_RETRIES, lostFrag, lostCount);
                } else {
                    ENJIN_LOG_WARN(Network, "NetworkSystem: Reliable message dropped after %d retries",
                                   RELIABLE_MAX_RETRIES);
                }
                it = std::remove_if(m_ReliableOutbox.begin(), m_ReliableOutbox.end(),
                        [&](const ReliableMessage& rm) {
                            return rm.messageId == lostId && rm.target == lostTarget;
                        });
                m_ReliableOutbox.erase(it, m_ReliableOutbox.end());
                it = m_ReliableOutbox.begin();
                continue;
            }
            it->sent = false;
            it->retryCount++;
        }
        ++it;
    }

    FlushPendingReliable();
}


void NetworkSystem::SendEntitySnapshots() {
    if (!m_World || m_Connections.empty()) return;

    auto entities = m_World->GetEntitiesWithComponent<ECS::NetworkIdentityComponent>();
    if (entities.empty()) return;

    std::vector<u8> payload;
    u16 count = 0;

    // Reserve space for count
    WriteU16(payload, 0);  // Placeholder

    for (auto entity : entities) {
        auto* netId = m_World->GetComponent<ECS::NetworkIdentityComponent>(entity);
        if (!netId || !netId->syncTransform) continue;
        if (!netId->isLocallyOwned) continue;  // Only send entities we own

        // Check sync interval
        netId->syncTimer += m_Config.syncRate;
        if (netId->syncTimer < netId->syncInterval) continue;
        netId->syncTimer = 0.0f;

        auto* transform = m_World->GetComponent<ECS::TransformComponent>(entity);
        if (!transform) continue;

        auto* netTrans = m_World->GetComponent<ECS::NetworkTransformComponent>(entity);
        auto* rb = m_World->GetComponent<ECS::RigidbodyComponent>(entity);

        // Determine which fields changed (M3 fix: delta compress rotation/scale too)
        u8 fieldMask = 0;
        if (!netTrans ||
            (transform->position - netTrans->lastSyncedPosition).Length() > 0.001f) {
            fieldMask |= SnapPosition;
        }
        if (!netTrans ||
            std::abs(transform->rotation.x - netTrans->lastSyncedRotation.x) > 0.0001f ||
            std::abs(transform->rotation.y - netTrans->lastSyncedRotation.y) > 0.0001f ||
            std::abs(transform->rotation.z - netTrans->lastSyncedRotation.z) > 0.0001f ||
            std::abs(transform->rotation.w - netTrans->lastSyncedRotation.w) > 0.0001f) {
            fieldMask |= SnapRotation;
        }
        if (!netTrans ||
            std::abs(transform->scale.x - netTrans->lastSyncedScale.x) > 0.001f ||
            std::abs(transform->scale.y - netTrans->lastSyncedScale.y) > 0.001f ||
            std::abs(transform->scale.z - netTrans->lastSyncedScale.z) > 0.001f) {
            fieldMask |= SnapScale;
        }
        // Physics bodies: replicate linear velocity so remotes can extrapolate between snapshots
        // (and so a moving body doesn't stall on the receiver between position updates).
        if (rb && rb->velocity.Length() > 0.001f) {
            fieldMask |= SnapVelocity;
        }

        if (fieldMask == 0) continue;

        WriteU32(payload, netId->networkId);
        WriteU8(payload, fieldMask);
        WriteU32(payload, m_Tick);

        if (fieldMask & SnapPosition) WriteVector3(payload, transform->position);
        if (fieldMask & SnapRotation) WriteQuaternion(payload, transform->rotation);
        if (fieldMask & SnapScale) WriteVector3(payload, transform->scale);
        if (fieldMask & SnapVelocity) WriteVector3(payload, rb ? rb->velocity : Math::Vector3(0, 0, 0));

        // Update last synced
        if (netTrans) {
            netTrans->lastSyncedPosition = transform->position;
            netTrans->lastSyncedRotation = transform->rotation;
            netTrans->lastSyncedScale = transform->scale;
        }

        count++;

        // Don't exceed packet size
        if (payload.size() > MAX_PACKET_SIZE - 100) break;
    }

    if (count == 0) return;

    // Patch count at start
    payload[0] = static_cast<u8>((count >> 8) & 0xFF);
    payload[1] = static_cast<u8>(count & 0xFF);

    SendToAll(MessageType::EntitySnapshot, payload);
}

void NetworkSystem::InterpolateRemoteEntities(f32 dt) {
    if (!m_World) return;

    f32 renderTime = m_Time - m_Config.interpDelay;

    // L5 fix: periodically clean up stale network entity references (every ~5 seconds)
    static u32 cleanupCounter = 0;
    if (++cleanupCounter >= 100) {  // ~5s at 20Hz sync rate
        cleanupCounter = 0;
        for (auto it = m_NetworkToEntity.begin(); it != m_NetworkToEntity.end(); ) {
            if (m_World->GetComponent<ECS::TransformComponent>(it->second) == nullptr) {
                m_EntityToNetwork.erase(it->second);
                m_InterpBuffers.erase(it->first);
                it = m_NetworkToEntity.erase(it);
            } else {
                ++it;
            }
        }
    }

    auto entities = m_World->GetEntitiesWithComponent<ECS::NetworkIdentityComponent>();
    for (auto entity : entities) {
        auto* netId = m_World->GetComponent<ECS::NetworkIdentityComponent>(entity);
        if (!netId || netId->isLocallyOwned) continue;

        auto it = m_InterpBuffers.find(netId->networkId);
        if (it == m_InterpBuffers.end()) continue;

        auto* transform = m_World->GetComponent<ECS::TransformComponent>(entity);
        if (!transform) continue;

        // If renderTime has run past the newest snapshot (late/lost packets), DON'T freeze at the
        // last position — dead-reckon forward using the last replicated velocity. Capped so a peer
        // that vanished doesn't drift off forever; the next real snapshot snaps it back.
        InterpolationState newest;
        f32 aheadTime = 0.0f;
        if (it->second.GetNewest(newest, renderTime, aheadTime) && aheadTime > 0.0f) {
            constexpr f32 MAX_EXTRAP = 0.25f;   // 250 ms of dead reckoning before we hold position
            f32 extra = std::min(aheadTime, MAX_EXTRAP);
            auto* netTrans = m_World->GetComponent<ECS::NetworkTransformComponent>(entity);
            Math::Vector3 vel = netTrans ? netTrans->networkVelocity : Math::Vector3(0, 0, 0);
            transform->position = newest.position + vel * extra;
            transform->rotation = newest.rotation;   // no angular velocity replicated yet
            transform->scale = newest.scale;
            continue;
        }

        InterpolationState from, to;
        f32 t;
        if (it->second.GetInterpolationPair(renderTime, from, to, t)) {
            // Detect teleport: if the interpolation endpoints are far apart,
            // the entity was teleported or respawned.  Flag it so the render
            // system zeroes the motion vector and TAA doesn't ghost.
            constexpr f32 TELEPORT_THRESHOLD_SQ = 5.0f * 5.0f;  // 5 world units
            f32 dx = to.position.x - from.position.x;
            f32 dy = to.position.y - from.position.y;
            f32 dz = to.position.z - from.position.z;
            f32 distSq = dx * dx + dy * dy + dz * dz;
            if (distSq > TELEPORT_THRESHOLD_SQ) {
                transform->teleportedThisFrame = true;
            }

            // Lerp position
            transform->position = Math::Vector3(
                from.position.x + (to.position.x - from.position.x) * t,
                from.position.y + (to.position.y - from.position.y) * t,
                from.position.z + (to.position.z - from.position.z) * t
            );

            // Slerp rotation
            transform->rotation = Math::Quaternion::Slerp(from.rotation, to.rotation, t);

            // Lerp scale
            transform->scale = Math::Vector3(
                from.scale.x + (to.scale.x - from.scale.x) * t,
                from.scale.y + (to.scale.y - from.scale.y) * t,
                from.scale.z + (to.scale.z - from.scale.z) * t
            );
        }
    }
}

void NetworkSystem::UpdateBandwidthCounters(f32 dt) {
    m_BandwidthTimer += dt;
    if (m_BandwidthTimer >= 1.0f) {
        CleanupRateLimiters();
        m_UploadKBps = static_cast<f32>(m_BytesSentThisSecond) / 1024.0f;
        m_DownloadKBps = static_cast<f32>(m_BytesReceivedThisSecond) / 1024.0f;
        m_BytesSentThisSecond = 0;
        m_BytesReceivedThisSecond = 0;
        m_BandwidthTimer -= 1.0f;
    }
}

// ============================================================================
// ACK PROCESSING
// ============================================================================

void NetworkSystem::ProcessAck(ConnectionInfo& conn, u16 ackSeq, u32 ackBits) {
    // Remove reliable messages that have been acked
    m_ReliableOutbox.erase(
        std::remove_if(m_ReliableOutbox.begin(), m_ReliableOutbox.end(),
            [&](const ReliableMessage& rm) {
                if (rm.target != conn.address) return false;
                if (rm.sequence == ackSeq) return true;
                u16 diff = ackSeq - rm.sequence;
                if (diff > 0 && diff <= 32 && (ackBits & (1u << diff))) return true;
                return false;
            }),
        m_ReliableOutbox.end());

    // H1 fix: accurate RTT from per-packet send timestamps
    f32 sendTime = conn.GetSendTime(ackSeq);
    if (sendTime > 0.0f) {
        f32 estimatedRtt = m_Time - sendTime;
        if (estimatedRtt > 0.0f && estimatedRtt < 2.0f) {
            conn.rtt = conn.rtt * 0.9f + estimatedRtt * 0.1f;  // Exponential smoothing
        }
    }

    // H3 fix: compute packet loss from ack bitfield (32-packet sliding window)
    if (conn.packetsSent > 1) {
        u32 windowSize = std::min(conn.packetsSent, 32u);
        u32 ackedInWindow = 0;
        for (u32 i = 1; i <= windowSize; i++) {
            if (ackBits & (1u << i)) ackedInWindow++;
        }
        f32 windowLoss = 1.0f - static_cast<f32>(ackedInWindow) / static_cast<f32>(windowSize);
        conn.packetLossRate = conn.packetLossRate * 0.95f + windowLoss * 0.05f;
        conn.packetsLost = static_cast<u32>(conn.packetLossRate * conn.packetsSent);
    }
}

// ============================================================================
// LOBBY
// ============================================================================

void NetworkSystem::BroadcastLobbyState() {
    std::vector<u8> payload;
    // N7: Cap to 255 to prevent u8 truncation
    u8 count = static_cast<u8>(std::min<size_t>(m_LobbyPlayers.size(), 255));
    WriteU8(payload, count);
    for (u8 i = 0; i < count; i++) {
        const auto& lp = m_LobbyPlayers[i];
        WriteU8(payload, lp.id);
        WriteString(payload, lp.name);
        WriteU8(payload, lp.ready ? 1 : 0);
        WriteU8(payload, lp.isHost ? 1 : 0);
    }
    SendToAll(MessageType::LobbyState, payload);
}

// ============================================================================
// CONNECTION LOOKUP
// ============================================================================

ConnectionInfo* NetworkSystem::FindConnectionByAddress(const NetworkAddress& addr) {
    for (auto& conn : m_Connections) {
        if (conn.address == addr) return &conn;
    }
    return nullptr;
}

ConnectionInfo* NetworkSystem::FindConnectionByPlayerId(PlayerId id) {
    for (auto& conn : m_Connections) {
        if (conn.playerId == id) return &conn;
    }
    return nullptr;
}

// ============================================================================
// AUTHENTICATION & REPLAY PROTECTION
// ============================================================================

void NetworkSystem::GenerateSessionKey() {
    m_SessionKey = Networking::GenerateSessionKey();

    // C2 fix: verify key is not all zeros (indicates CSPRNG failure)
    bool allZero = true;
    for (u32 i = 0; i < SESSION_KEY_SIZE; i++) {
        if (m_SessionKey[i] != 0) { allZero = false; break; }
    }
    if (allZero) {
        ENJIN_LOG_ERROR(Network, "NetworkSystem: CSPRNG produced zero key — authentication disabled");
        m_AuthEnabled = false;
        return;
    }

    m_SessionKeyGenerated = true;
    ENJIN_LOG_INFO(Network, "NetworkSystem: Session key generated for HMAC authentication");
}

void NetworkSystem::SendSessionKey(const NetworkAddress& addr) {
    // C1 mitigation: The session key is sent to the client over UDP.
    // This is NOT encrypted (proper fix requires ECDH/X25519 key exchange).
    // Current mitigations:
    //   - Sender IP validation in HandleSessionKeyExchange (prevents LAN MITM injection)
    //   - One-shot acceptance (prevents key replacement attacks)
    //   - LAN-only scope (session key only used for HMAC packet authentication)
    // TODO: Implement X25519 Diffie-Hellman key exchange when a crypto library is added.

    // C2 companion: refuse to send a zero/weak key
    bool allZero = true;
    for (u32 i = 0; i < SESSION_KEY_SIZE; i++) {
        if (m_SessionKey[i] != 0) { allZero = false; break; }
    }
    if (allZero) {
        ENJIN_LOG_ERROR(Network, "NetworkSystem: Refusing to send zero session key (CSPRNG failure)");
        return;
    }

    std::vector<u8> payload;
    payload.reserve(SESSION_KEY_SIZE);
    for (u32 i = 0; i < SESSION_KEY_SIZE; i++) {
        WriteU8(payload, m_SessionKey[i]);
    }
    SendPacket(addr, MessageType::SessionKeyExchange, payload);
}

void NetworkSystem::HandleSessionKeyExchange(const NetworkAddress& sender, const u8* payload, u32 size) {
    if (m_Role != NetworkRole::Client) return;
    if (size < SESSION_KEY_SIZE) {
        ENJIN_LOG_WARN(Network, "NetworkSystem: SessionKeyExchange payload too small (%u bytes)", size);
        return;
    }

    // C2 fix: reject session key from any address other than the host we connected to.
    // This prevents LAN-level MITM from injecting a fake session key before the real one.
    if (!m_Connections.empty() && sender != m_Connections[0].address) {
        ENJIN_LOG_WARN(Network, "NetworkSystem: SessionKeyExchange from unexpected sender, dropping");
        return;
    }

    // C2 fix: reject if we already have a session key (prevent key replacement attacks)
    if (m_SessionKeyGenerated) {
        ENJIN_LOG_WARN(Network, "NetworkSystem: SessionKeyExchange received but key already set, ignoring");
        return;
    }

    // Read the session key
    // C1 NOTE: Session key is transmitted in plaintext over UDP. Proper fix requires
    // ECDH/X25519 key exchange so the shared secret never crosses the wire.
    // Current mitigations: sender IP validation + one-shot acceptance + LAN-only scope.
    u32 offset = 0;
    for (u32 i = 0; i < SESSION_KEY_SIZE; i++) {
        m_SessionKey[i] = ReadU8(payload, offset, size);
    }

    // C2 companion: reject a zero session key (indicates CSPRNG failure on host)
    bool allZero = true;
    for (u32 i = 0; i < SESSION_KEY_SIZE; i++) {
        if (m_SessionKey[i] != 0) { allZero = false; break; }
    }
    if (allZero) {
        ENJIN_LOG_ERROR(Network, "NetworkSystem: Received zero session key (host CSPRNG failure?), rejecting");
        std::memset(m_SessionKey.data(), 0, SESSION_KEY_SIZE);
        return;
    }

    m_SessionKeyGenerated = true;

    // Mark connection as authenticated
    ConnectionInfo* conn = FindConnectionByAddress(sender);
    if (conn) {
        conn->authenticated = true;
        conn->replayWindow.Reset();
    }

    ENJIN_LOG_INFO(Network, "NetworkSystem: Received session key from host, authentication enabled");
}

bool NetworkSystem::AuthenticateOutgoing(std::vector<u8>& packet, ConnectionInfo* conn) {
    // Append auth sequence (4 bytes, big-endian) + HMAC tag (32 bytes)
    u32 seq = 0;
    if (conn) {
        seq = conn->authSendSequence++;
    }

    // Append sequence number (big-endian)
    packet.push_back(static_cast<u8>((seq >> 24) & 0xFF));
    packet.push_back(static_cast<u8>((seq >> 16) & 0xFF));
    packet.push_back(static_cast<u8>((seq >> 8) & 0xFF));
    packet.push_back(static_cast<u8>(seq & 0xFF));

    // Compute HMAC-SHA256 over everything up to this point (header + payload + sequence)
    u8 hmacTag[HMAC_TAG_SIZE];
    HMACSHA256::Compute(m_SessionKey.data(), SESSION_KEY_SIZE,
                        packet.data(), static_cast<u32>(packet.size()),
                        hmacTag);

    // Append HMAC tag
    packet.insert(packet.end(), hmacTag, hmacTag + HMAC_TAG_SIZE);

    return true;
}

// H5 fix: VerifyIncoming removed — was dead code. HMAC verification is inline in HandlePacket().

} // namespace Networking
} // namespace Enjin
