#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/Math/Quaternion.h"
#include "Enjin/Networking/NetworkSecurity.h"
#include <string>
#include <vector>
#include <functional>
#include <unordered_map>

namespace Enjin {
namespace Networking {

// ============================================================================
// CONSTANTS
// ============================================================================

static constexpr u16 DEFAULT_PORT = 7777;
static constexpr u32 MAX_PLAYERS = 16;
static constexpr u32 MAX_PACKET_SIZE = 1400;
static constexpr f32 HEARTBEAT_INTERVAL = 1.0f;
static constexpr f32 CONNECTION_TIMEOUT = 10.0f;
static constexpr f32 DEFAULT_SYNC_RATE = 1.0f / 20.0f;  // 20 Hz
static constexpr u32 RELIABLE_MAX_RETRIES = 10;
static constexpr f32 RELIABLE_RETRY_INTERVAL = 0.2f;

// ============================================================================
// TYPE ALIASES
// ============================================================================

using NetworkId = u32;
using PlayerId = u8;

static constexpr PlayerId INVALID_PLAYER = 0xFF;
static constexpr NetworkId INVALID_NETWORK_ID = 0;

// ============================================================================
// ENUMS
// ============================================================================

enum class ConnectionState : u8 {
    Disconnected = 0,
    Connecting,
    Connected,
    Disconnecting
};

enum class NetworkRole : u8 {
    None = 0,
    Host,
    Client
};

enum class MessageType : u8 {
    // Connection
    ConnectionRequest = 1,
    ConnectionAccept,
    ConnectionReject,
    Disconnect,

    // Keepalive
    Heartbeat,
    HeartbeatAck,

    // Lobby
    PlayerJoined,
    PlayerLeft,
    PlayerReady,
    LobbyState,

    // Entity sync
    EntitySnapshot,
    EntitySpawn,
    EntityDestroy,

    // Ownership
    OwnershipRequest,
    OwnershipGrant,
    OwnershipRevoke,

    // RPC
    RPCCall,
    RPCResponse,

    // Reliability
    Ack,
    ReliableMessage,

    // Authentication
    SessionKeyExchange  // Host sends session key to client during handshake
};

// ============================================================================
// NETWORK ADDRESS
// ============================================================================

struct NetworkAddress {
    u32 ip = 0;      // IPv4 in network byte order
    u16 port = 0;    // In host byte order

    bool operator==(const NetworkAddress& other) const {
        return ip == other.ip && port == other.port;
    }

    bool operator!=(const NetworkAddress& other) const {
        return !(*this == other);
    }

    static u32 ParseIP(const std::string& ipStr);
    static std::string IPToString(u32 ip);
};

struct NetworkAddressHash {
    size_t operator()(const NetworkAddress& addr) const {
        return std::hash<u64>()(static_cast<u64>(addr.ip) << 16 | addr.port);
    }
};

// ============================================================================
// PACKET HEADER (12 bytes)
// ============================================================================

struct PacketHeader {
    u8 type = 0;           // MessageType
    u16 sequence = 0;      // Packet sequence number
    u16 ackSequence = 0;   // Last received sequence from remote
    u32 ackBitfield = 0;   // 32 previous sequences received (bitmask)
    u8 senderId = INVALID_PLAYER;
    u16 payloadSize = 0;   // Payload byte count after header
};

static constexpr u32 PACKET_HEADER_SIZE = 12;

// ============================================================================
// CONNECTION INFO
// ============================================================================

struct ConnectionInfo {
    NetworkAddress address;
    ConnectionState state = ConnectionState::Disconnected;
    PlayerId playerId = INVALID_PLAYER;
    std::string playerName;
    bool ready = false;

    // Timing
    f32 lastRecvTime = 0.0f;
    f32 lastSendTime = 0.0f;
    f32 rtt = 0.0f;             // Round-trip time in seconds

    // Sequence tracking
    u16 localSequence = 0;      // Next outgoing sequence
    u16 remoteSequence = 0;     // Last received sequence from peer
    u32 remoteAckBitfield = 0;  // Which of our packets peer has acked

    // Stats
    u32 packetsSent = 0;
    u32 packetsReceived = 0;
    u32 packetsLost = 0;
    f32 packetLossRate = 0.0f;

    // Replay protection (per-connection sliding window)
    ReplayWindow replayWindow;

    // Authentication sequence (monotonically increasing per-connection)
    u32 authSendSequence = 0;       // Next outgoing auth sequence
    bool authenticated = false;     // True once session key has been exchanged
};

// ============================================================================
// NETWORK CONFIG
// ============================================================================

struct NetworkConfig {
    u16 port = DEFAULT_PORT;
    u32 maxPlayers = MAX_PLAYERS;
    std::string serverIP = "127.0.0.1";
    f32 syncRate = DEFAULT_SYNC_RATE;

    // Interpolation
    f32 interpDelay = 0.1f;           // 100ms interpolation buffer delay
    f32 predictionCorrectionSpeed = 10.0f;
};

// ============================================================================
// ENTITY SNAPSHOT
// ============================================================================

// Bitmask for which fields changed in a snapshot
enum SnapshotField : u8 {
    SnapPosition = 1 << 0,
    SnapRotation = 1 << 1,
    SnapScale    = 1 << 2,
    SnapVelocity = 1 << 3
};

struct EntitySnapshot {
    NetworkId networkId = INVALID_NETWORK_ID;
    u8 fieldMask = 0;
    Math::Vector3 position;
    Math::Quaternion rotation;
    Math::Vector3 scale = Math::Vector3(1.0f, 1.0f, 1.0f);
    Math::Vector3 velocity;
    u32 tick = 0;
};

// ============================================================================
// RPC
// ============================================================================

using RPCCallback = std::function<void(PlayerId sender, const u8* data, u32 size)>;

struct RPCRegistration {
    std::string name;
    u32 nameHash = 0;
    RPCCallback callback;
    bool reliable = false;
};

// ============================================================================
// LOBBY
// ============================================================================

struct LobbyPlayer {
    PlayerId id = INVALID_PLAYER;
    std::string name;
    bool ready = false;
    bool isHost = false;
};

// ============================================================================
// RELIABLE MESSAGE TRACKING
// ============================================================================

struct ReliableMessage {
    u16 sequence = 0;
    f32 lastSendTime = 0.0f;
    f32 firstSendTime = 0.0f;
    i32 retryCount = 0;
    std::vector<u8> data;
    NetworkAddress target;
};

// ============================================================================
// INTERPOLATION STATE (per remote entity)
// ============================================================================

struct InterpolationState {
    Math::Vector3 position;
    Math::Quaternion rotation;
    Math::Vector3 scale = Math::Vector3(1.0f, 1.0f, 1.0f);
    f32 timestamp = 0.0f;
};

static constexpr u32 INTERP_BUFFER_SIZE = 4;

struct InterpolationBuffer {
    InterpolationState states[INTERP_BUFFER_SIZE];
    u32 writeIndex = 0;
    u32 count = 0;

    void Push(const InterpolationState& state) {
        states[writeIndex] = state;
        writeIndex = (writeIndex + 1) % INTERP_BUFFER_SIZE;
        if (count < INTERP_BUFFER_SIZE) count++;
    }

    // Get the two states to interpolate between for a given timestamp
    bool GetInterpolationPair(f32 renderTime, InterpolationState& from, InterpolationState& to, f32& t) const {
        if (count < 2) return false;

        // Find the two states bracketing renderTime
        for (u32 i = 0; i < count - 1; i++) {
            u32 idxA = (writeIndex - count + i + INTERP_BUFFER_SIZE) % INTERP_BUFFER_SIZE;
            u32 idxB = (idxA + 1) % INTERP_BUFFER_SIZE;
            const auto& a = states[idxA];
            const auto& b = states[idxB];
            if (renderTime >= a.timestamp && renderTime <= b.timestamp) {
                from = a;
                to = b;
                f32 duration = b.timestamp - a.timestamp;
                t = duration > 0.0001f ? (renderTime - a.timestamp) / duration : 0.0f;
                return true;
            }
        }

        // If we're past all states, use the latest
        u32 latestIdx = (writeIndex - 1 + INTERP_BUFFER_SIZE) % INTERP_BUFFER_SIZE;
        from = states[latestIdx];
        to = states[latestIdx];
        t = 1.0f;
        return true;
    }
};

// ============================================================================
// FNV-1a HASH (for RPC name hashing)
// ============================================================================

inline u32 FNV1aHash(const std::string& str) {
    u32 hash = 2166136261u;
    for (char c : str) {
        hash ^= static_cast<u32>(c);
        hash *= 16777619u;
    }
    return hash;
}

} // namespace Networking
} // namespace Enjin
