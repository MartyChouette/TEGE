#pragma once

#include <memory>
#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/ECS/Entity.h"
#include <string>
#include <vector>
#include <functional>

namespace Enjin {
namespace Editor {

// Message types for editor-runtime cross-process communication.
// The protocol is delta-based: only changes are transmitted, not full scene state.
// This ensures O(changes) communication cost, not O(scene size).
enum class EditorMessageType : u16 {
    // Scene manipulation (editor → runtime)
    EntityCreate         = 0x0100,
    EntityDestroy        = 0x0101,
    ComponentAdd         = 0x0102,
    ComponentRemove      = 0x0103,
    ComponentModify      = 0x0104,  // Property change (key + JSON value)
    EntityReparent       = 0x0105,

    // Play mode control (editor → runtime)
    PlayModePlay         = 0x0200,
    PlayModePause        = 0x0201,
    PlayModeStop         = 0x0202,
    PlayModeStep         = 0x0203,  // Single frame advance

    // Viewport (editor → runtime)
    ViewportCameraSync   = 0x0300,  // Camera position/rotation/FOV
    ViewportResize       = 0x0301,

    // Asset management (editor → runtime)
    AssetReload          = 0x0400,  // Hot-reload a specific asset
    SceneLoad            = 0x0401,  // Full scene load
    SceneLoadAdditive    = 0x0402,

    // Runtime → editor responses
    FrameReady           = 0x0500,  // Rendered frame available for viewport
    EntityCreated        = 0x0501,  // Response with assigned entity ID
    PlayStateChanged     = 0x0502,
    LogMessage           = 0x0503,
    RuntimeCrashed       = 0x05FF,  // Runtime process exited unexpectedly

    // Heartbeat (bidirectional)
    Ping                 = 0x0600,
    Pong                 = 0x0601,
};

// Serialized message header (fixed size, precedes variable-length payload)
struct EditorMessageHeader {
    u16 type;         // EditorMessageType
    u16 flags;        // Reserved
    u32 payloadSize;  // Bytes following this header
    u64 sequenceId;   // Monotonic message counter (for ordering/dedup)
    u64 timestamp;    // Microseconds since epoch
};
static_assert(sizeof(EditorMessageHeader) == 24, "Header must be 24 bytes");

// Transport-agnostic message (header + payload bytes)
struct EditorMessage {
    EditorMessageHeader header;
    std::vector<u8> payload;

    // Convenience: serialize a string payload
    static EditorMessage MakeString(EditorMessageType type, const std::string& data, u64 seqId);
    // Convenience: serialize a JSON payload
    static EditorMessage MakeJson(EditorMessageType type, const std::string& json, u64 seqId);
    // Parse payload as string
    std::string PayloadAsString() const;
};

// Transport interface — abstracts shared memory, WebSocket, pipe, etc.
class ENJIN_API IEditorTransport {
public:
    virtual ~IEditorTransport() = default;

    virtual bool Connect(const std::string& endpoint) = 0;
    virtual void Disconnect() = 0;
    virtual bool IsConnected() const = 0;

    virtual bool Send(const EditorMessage& msg) = 0;
    virtual bool Receive(EditorMessage& outMsg) = 0; // Non-blocking, returns false if no message
    virtual bool HasPendingMessages() const = 0;
};

// Same-machine editor-to-runtime transport over shared memory.
//
// Both ends call Connect with the same endpoint string; whichever gets there
// first creates and sizes the segment, and that side unlinks it on Disconnect.
// Declared here because the implementation was only ever defined in its own
// .cpp, so nothing outside that file could reach it.
ENJIN_API std::unique_ptr<IEditorTransport> CreateSharedMemoryTransport();

// Callback-based message handler
using MessageHandler = std::function<void(const EditorMessage&)>;

// Message dispatcher — routes incoming messages to registered handlers
class ENJIN_API EditorMessageDispatcher {
public:
    void RegisterHandler(EditorMessageType type, MessageHandler handler);
    void DispatchAll(IEditorTransport& transport);

private:
    std::unordered_map<u16, std::vector<MessageHandler>> m_Handlers;
};

} // namespace Editor
} // namespace Enjin
