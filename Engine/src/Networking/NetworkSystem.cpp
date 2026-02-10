#include "Enjin/Networking/NetworkSystem.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/ECS/Components/Name.h"
#include "Enjin/Logging/Log.h"
#include "Enjin/Debug/Profiler.h"
#include <cmath>
#include <algorithm>

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

    if (!m_Transport.Bind(port)) {
        ENJIN_LOG_ERROR(Network, "NetworkSystem: Failed to bind as host on port %u", port);
        return false;
    }

    m_Role = NetworkRole::Host;
    m_LocalPlayerId = 0;
    m_LocalPlayerName = playerName;
    m_NextPlayerId = 1;
    m_Time = 0.0f;
    m_Tick = 0;

    // Add self to lobby
    LobbyPlayer self;
    self.id = 0;
    self.name = playerName;
    self.ready = false;
    self.isHost = true;
    m_LobbyPlayers.clear();
    m_LobbyPlayers.push_back(self);

    ENJIN_LOG_INFO(Network, "NetworkSystem: Hosting on port %u as '%s'", port, playerName.c_str());
    return true;
}

bool NetworkSystem::JoinGame(const std::string& ip, u16 port, const std::string& playerName) {
    if (m_Role != NetworkRole::None) {
        ENJIN_LOG_WARN(Network, "NetworkSystem: Already connected, disconnect first");
        return false;
    }

    // Bind to any available port
    if (!m_Transport.Bind(0)) {
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

    // Send disconnect to all peers
    std::vector<u8> empty;
    SendToAll(MessageType::Disconnect, empty);

    // If host, notify all clients they're disconnected
    if (m_Role == NetworkRole::Host) {
        for (auto& conn : m_Connections) {
            conn.state = ConnectionState::Disconnected;
        }
    }

    m_Transport.Close();
    m_Role = NetworkRole::None;
    m_LocalPlayerId = INVALID_PLAYER;
    m_Connections.clear();
    m_LobbyPlayers.clear();
    m_NetworkToEntity.clear();
    m_EntityToNetwork.clear();
    m_InterpBuffers.clear();
    m_ReliableOutbox.clear();
    m_SyncTimer = 0.0f;
    m_HeartbeatTimer = 0.0f;
    m_BytesSentThisSecond = 0;
    m_BytesReceivedThisSecond = 0;
    m_UploadKBps = 0.0f;
    m_DownloadKBps = 0.0f;

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
    m_NetworkToEntity[id] = entity;
    m_EntityToNetwork[entity] = id;

    // Set component data if present
    if (m_World) {
        auto* netId = m_World->GetComponent<ECS::NetworkIdentityComponent>(entity);
        if (netId) {
            netId->networkId = id;
            netId->ownerId = owner;
            netId->isLocallyOwned = (owner == m_LocalPlayerId);
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
    RPCRegistration reg;
    reg.name = name;
    reg.nameHash = FNV1aHash(name);
    reg.callback = callback;
    reg.reliable = reliable;
    m_RPCRegistry[reg.nameHash] = reg;
}

void NetworkSystem::CallRPC(const std::string& name, PlayerId target, const u8* data, u32 size) {
    u32 nameHash = FNV1aHash(name);
    auto it = m_RPCRegistry.find(nameHash);
    bool reliable = (it != m_RPCRegistry.end()) ? it->second.reliable : false;

    std::vector<u8> payload;
    WriteU32(payload, nameHash);
    WriteU8(payload, target);
    WriteU16(payload, static_cast<u16>(size));
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

void NetworkSystem::CallRPCAll(const std::string& name, const u8* data, u32 size) {
    u32 nameHash = FNV1aHash(name);
    auto it = m_RPCRegistry.find(nameHash);
    bool reliable = (it != m_RPCRegistry.end()) ? it->second.reliable : false;

    std::vector<u8> payload;
    WriteU32(payload, nameHash);
    WriteU8(payload, INVALID_PLAYER);  // broadcast
    WriteU16(payload, static_cast<u16>(size));
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

// ============================================================================
// PACKET PROCESSING
// ============================================================================

void NetworkSystem::ProcessIncomingPackets() {
    u8 buffer[MAX_PACKET_SIZE];
    NetworkAddress sender;

    for (u32 i = 0; i < 256; i++) {  // Process up to 256 packets per frame
        i32 received = m_Transport.ReceiveFrom(sender, buffer, MAX_PACKET_SIZE);
        if (received <= 0) break;

        m_BytesReceivedThisSecond += static_cast<u32>(received);

        if (static_cast<u32>(received) < PACKET_HEADER_SIZE) continue;

        HandlePacket(sender, buffer, static_cast<u32>(received));
    }
}

void NetworkSystem::HandlePacket(const NetworkAddress& sender, const u8* data, u32 size) {
    u32 offset = 0;
    PacketHeader header = ReadPacketHeader(data, offset, size);

    // Update connection tracking
    ConnectionInfo* conn = FindConnectionByAddress(sender);
    if (conn) {
        conn->lastRecvTime = m_Time;
        conn->packetsReceived++;

        // Update remote sequence tracking
        if (header.sequence > conn->remoteSequence ||
            (conn->remoteSequence > 0xFF00 && header.sequence < 0x00FF)) {
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

    MessageType type = static_cast<MessageType>(header.type);

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
            HandleDisconnect(sender, header.senderId);
            break;
        case MessageType::Heartbeat:
            HandleHeartbeat(sender, header.senderId);
            break;
        case MessageType::PlayerReady:
            HandlePlayerReady(header.senderId, payload, payloadSize);
            break;
        case MessageType::LobbyState:
            HandleLobbyState(payload, payloadSize);
            break;
        case MessageType::EntitySnapshot:
            HandleEntitySnapshot(payload, payloadSize);
            break;
        case MessageType::EntitySpawn:
            HandleEntitySpawn(payload, payloadSize);
            break;
        case MessageType::EntityDestroy:
            HandleEntityDestroy(payload, payloadSize);
            break;
        case MessageType::OwnershipRequest:
            HandleOwnershipRequest(header.senderId, payload, payloadSize);
            break;
        case MessageType::OwnershipGrant:
            HandleOwnershipGrant(payload, payloadSize);
            break;
        case MessageType::RPCCall:
            HandleRPCCall(header.senderId, payload, payloadSize);
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

    // Accept connection
    PlayerId newId = m_NextPlayerId++;
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
    // Send ack back
    std::vector<u8> empty;
    SendPacket(sender, MessageType::HeartbeatAck, empty);
}

void NetworkSystem::HandlePlayerReady(PlayerId senderId, const u8* payload, u32 size) {
    if (m_Role != NetworkRole::Host) return;

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

void NetworkSystem::HandleEntitySnapshot(const u8* payload, u32 size) {
    if (!m_World) return;

    u32 offset = 0;
    u16 count = ReadU16(payload, offset, size);

    for (u16 i = 0; i < count && offset < size; i++) {
        EntitySnapshot snap;
        snap.networkId = ReadU32(payload, offset, size);
        snap.fieldMask = ReadU8(payload, offset, size);
        snap.tick = ReadU32(payload, offset, size);

        if (snap.fieldMask & SnapPosition) snap.position = ReadVector3(payload, offset, size);
        if (snap.fieldMask & SnapRotation) snap.rotation = ReadQuaternion(payload, offset, size);
        if (snap.fieldMask & SnapScale) snap.scale = ReadVector3(payload, offset, size);
        if (snap.fieldMask & SnapVelocity) snap.velocity = ReadVector3(payload, offset, size);

        // Find entity
        auto it = m_NetworkToEntity.find(snap.networkId);
        if (it == m_NetworkToEntity.end()) continue;

        ECS::Entity entity = it->second;
        auto* netId = m_World->GetComponent<ECS::NetworkIdentityComponent>(entity);
        if (!netId || netId->isLocallyOwned) continue;  // Don't overwrite own entities

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
    }
}

void NetworkSystem::HandleEntitySpawn(const u8* payload, u32 size) {
    if (!m_World || m_Role != NetworkRole::Client) return;

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

void NetworkSystem::HandleEntityDestroy(const u8* payload, u32 size) {
    if (!m_World) return;

    u32 offset = 0;
    NetworkId netId = ReadU32(payload, offset, size);

    auto it = m_NetworkToEntity.find(netId);
    if (it == m_NetworkToEntity.end()) return;

    ECS::Entity entity = it->second;
    m_EntityToNetwork.erase(entity);
    m_NetworkToEntity.erase(it);
    m_InterpBuffers.erase(netId);

    m_World->DestroyEntity(entity);
    ENJIN_LOG_INFO(Network, "NetworkSystem: Destroyed remote entity NetworkId=%u", netId);
}

void NetworkSystem::HandleOwnershipRequest(PlayerId senderId, const u8* payload, u32 size) {
    if (m_Role != NetworkRole::Host) return;

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

    u32 offset = 0;
    NetworkId netId = ReadU32(payload, offset, size);
    PlayerId newOwner = ReadU8(payload, offset, size);

    auto it = m_NetworkToEntity.find(netId);
    if (it == m_NetworkToEntity.end()) return;

    auto* netComp = m_World->GetComponent<ECS::NetworkIdentityComponent>(it->second);
    if (netComp) {
        netComp->ownerId = newOwner;
        netComp->isLocallyOwned = (newOwner == m_LocalPlayerId);
    }
}

void NetworkSystem::HandleRPCCall(PlayerId senderId, const u8* payload, u32 size) {
    u32 offset = 0;
    u32 nameHash = ReadU32(payload, offset, size);
    PlayerId targetId = ReadU8(payload, offset, size);
    u16 dataSize = ReadU16(payload, offset, size);

    // Check if this is for us
    if (targetId != INVALID_PLAYER && targetId != m_LocalPlayerId) {
        // Forward (host only)
        if (m_Role == NetworkRole::Host) {
            ConnectionInfo* conn = FindConnectionByPlayerId(targetId);
            if (conn) {
                std::vector<u8> fwdPayload(payload, payload + size);
                SendPacket(conn->address, MessageType::RPCCall, fwdPayload);
            }
        }
        return;
    }

    auto it = m_RPCRegistry.find(nameHash);
    if (it != m_RPCRegistry.end()) {
        const u8* rpcData = (offset < size) ? payload + offset : nullptr;
        u32 rpcSize = (offset < size) ? std::min(static_cast<u32>(dataSize), size - offset) : 0;
        it->second.callback(senderId, rpcData, rpcSize);
    }

    // If host and broadcast target, forward to all other clients
    if (m_Role == NetworkRole::Host && targetId == INVALID_PLAYER) {
        std::vector<u8> fwdPayload(payload, payload + size);
        SendToAll(MessageType::RPCCall, fwdPayload, senderId);
    }
}

// ============================================================================
// SENDING
// ============================================================================

void NetworkSystem::SendPacket(const NetworkAddress& addr, MessageType type, const std::vector<u8>& payload) {
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
    }

    std::vector<u8> packet;
    packet.reserve(PACKET_HEADER_SIZE + payload.size());
    WritePacketHeader(packet, header);
    packet.insert(packet.end(), payload.begin(), payload.end());

    m_Transport.SendTo(addr, packet.data(), static_cast<u32>(packet.size()));
    m_BytesSentThisSecond += static_cast<u32>(packet.size());
}

void NetworkSystem::SendToAll(MessageType type, const std::vector<u8>& payload, PlayerId exclude) {
    for (auto& conn : m_Connections) {
        if (conn.state == ConnectionState::Connected && conn.playerId != exclude) {
            SendPacket(conn.address, type, payload);
        }
    }
}

void NetworkSystem::SendReliable(const NetworkAddress& addr, MessageType type, const std::vector<u8>& payload) {
    // Build the inner packet data
    std::vector<u8> innerPacket;
    WriteU8(innerPacket, static_cast<u8>(type));
    innerPacket.insert(innerPacket.end(), payload.begin(), payload.end());

    // Track for retransmission
    ReliableMessage rm;
    ConnectionInfo* conn = FindConnectionByAddress(addr);
    rm.sequence = conn ? conn->localSequence : 0;
    rm.lastSendTime = m_Time;
    rm.firstSendTime = m_Time;
    rm.retryCount = 0;
    rm.data = innerPacket;
    rm.target = addr;
    m_ReliableOutbox.push_back(rm);

    // Send wrapped
    std::vector<u8> wrappedPayload;
    WriteU16(wrappedPayload, rm.sequence);
    wrappedPayload.insert(wrappedPayload.end(), innerPacket.begin(), innerPacket.end());
    SendPacket(addr, MessageType::ReliableMessage, wrappedPayload);
}

// ============================================================================
// UPDATE TICKS
// ============================================================================

void NetworkSystem::UpdateHeartbeats(f32 dt) {
    m_HeartbeatTimer += dt;
    if (m_HeartbeatTimer < HEARTBEAT_INTERVAL) return;
    m_HeartbeatTimer -= HEARTBEAT_INTERVAL;

    std::vector<u8> empty;
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
    for (auto it = m_ReliableOutbox.begin(); it != m_ReliableOutbox.end();) {
        f32 elapsed = m_Time - it->lastSendTime;
        if (elapsed >= RELIABLE_RETRY_INTERVAL) {
            if (it->retryCount >= RELIABLE_MAX_RETRIES) {
                ENJIN_LOG_WARN(Network, "NetworkSystem: Reliable message dropped after %d retries",
                               RELIABLE_MAX_RETRIES);
                it = m_ReliableOutbox.erase(it);
                continue;
            }

            // Retransmit
            std::vector<u8> wrappedPayload;
            WriteU16(wrappedPayload, it->sequence);
            wrappedPayload.insert(wrappedPayload.end(), it->data.begin(), it->data.end());
            SendPacket(it->target, MessageType::ReliableMessage, wrappedPayload);
            it->lastSendTime = m_Time;
            it->retryCount++;
        }
        ++it;
    }
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

        // Determine which fields changed
        u8 fieldMask = 0;
        if (!netTrans ||
            (transform->position - netTrans->lastSyncedPosition).Length() > 0.001f) {
            fieldMask |= SnapPosition;
        }
        // Always send rotation and scale for simplicity
        fieldMask |= SnapRotation | SnapScale;

        if (fieldMask == 0) continue;

        WriteU32(payload, netId->networkId);
        WriteU8(payload, fieldMask);
        WriteU32(payload, m_Tick);

        if (fieldMask & SnapPosition) WriteVector3(payload, transform->position);
        if (fieldMask & SnapRotation) WriteQuaternion(payload, transform->rotation);
        if (fieldMask & SnapScale) WriteVector3(payload, transform->scale);
        if (fieldMask & SnapVelocity) WriteVector3(payload, Math::Vector3(0, 0, 0));

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

    auto entities = m_World->GetEntitiesWithComponent<ECS::NetworkIdentityComponent>();
    for (auto entity : entities) {
        auto* netId = m_World->GetComponent<ECS::NetworkIdentityComponent>(entity);
        if (!netId || netId->isLocallyOwned) continue;

        auto it = m_InterpBuffers.find(netId->networkId);
        if (it == m_InterpBuffers.end()) continue;

        auto* transform = m_World->GetComponent<ECS::TransformComponent>(entity);
        if (!transform) continue;

        InterpolationState from, to;
        f32 t;
        if (it->second.GetInterpolationPair(renderTime, from, to, t)) {
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

    // Estimate RTT from ack timing
    // Simple: if ack is for a recent sequence, use send/recv delta
    f32 estimatedRtt = m_Time - conn.lastSendTime;
    if (estimatedRtt > 0.0f && estimatedRtt < 2.0f) {
        conn.rtt = conn.rtt * 0.9f + estimatedRtt * 0.1f;  // Exponential smoothing
    }
}

// ============================================================================
// LOBBY
// ============================================================================

void NetworkSystem::BroadcastLobbyState() {
    std::vector<u8> payload;
    WriteU8(payload, static_cast<u8>(m_LobbyPlayers.size()));
    for (const auto& lp : m_LobbyPlayers) {
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

} // namespace Networking
} // namespace Enjin
