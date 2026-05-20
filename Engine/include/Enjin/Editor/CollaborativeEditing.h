#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/ECS/Entity.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/Math/Quaternion.h"
#include "Enjin/Editor/VectorClock.h"
#include "Enjin/Editor/CRDTState.h"
#include <string>
#include <vector>
#include <deque>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <chrono>

namespace Enjin {

// Forward declarations
namespace ECS { class World; }
namespace Networking { class NetworkSystem; }

namespace Editor {

// ============================================================================
// OPERATION TYPES
// ============================================================================

enum class EditOpType : u8 {
    CreateEntity = 1,
    DeleteEntity,
    RenameEntity,
    SetComponent,       // Full component JSON replacement
    RemoveComponent,
    ModifyTransform,    // Optimized path for frequent transform changes
    SetParent,          // Reparenting
    LockEntity,
    UnlockEntity
};

// ============================================================================
// EDIT OPERATION (the unit of collaboration)
// ============================================================================

struct EditOperation {
    EditOpType type = EditOpType::CreateEntity;
    u64 entityId = 0;               // Target entity (ECS::Entity)
    std::string componentKey;        // For Set/RemoveComponent (e.g., "transform", "mesh")
    std::string dataJson;            // JSON payload (component data, entity name, etc.)
    std::string previousJson;        // Previous state for undo/conflict resolution

    // Transform-specific (optimized, avoids JSON for high-frequency updates)
    Math::Vector3 position;
    Math::Vector3 rotation;          // Euler angles
    Math::Vector3 scale = Math::Vector3(1.0f, 1.0f, 1.0f);

    // Metadata
    u64 sequenceId = 0;             // Globally ordered operation ID
    u64 lamportClock = 0;           // Legacy Lamport clock (kept for backward compat)
    u8 authorId = 0;                // PlayerId / siteId who authored this operation
    std::string authorName;          // Human-readable author name
    f64 timestamp = 0.0;            // Unix timestamp

    // CRDT vector clock — replaces lamportClock for causality ordering.
    // Both are serialized; lamportClock is set from vclock.MaxComponent() for
    // backward compat with older peers.
    VectorClock vclock;
};

// ============================================================================
// COLLAB SESSION
// ============================================================================

enum class CollabSessionState : u8 {
    Disconnected = 0,
    Hosting,
    Joining,
    Connected,
    Syncing          // Full state sync in progress
};

// S-C2: Permission levels for collaborative editing peers
enum class CollabPermission : u8 {
    Viewer = 0,     // Can observe but not modify
    Editor = 1,     // Can modify entities/components
    Owner = 2       // Full control (can delete entities, kick peers)
};

struct CollabPeer {
    u8 peerId = 0;
    std::string name;
    std::string machine;
    u32 cursorEntityId = 0;         // Entity the peer is currently inspecting
    Math::Vector3 cameraPos;
    f32 lastHeartbeat = 0.0f;
    u64 lastAckedSeq = 0;           // Last operation sequence they've acknowledged
    bool connected = false;
    CollabPermission permission = CollabPermission::Editor; // Default for joining peers
};

// ============================================================================
// CONFLICT RESOLUTION
// ============================================================================

enum class ConflictStrategy : u8 {
    LastWriterWins = 0,  // Higher lamport clock wins
    HostAuthority,       // Host's version always wins
    Merge,               // Per-field merge (only for transforms)
    Reject               // Reject the remote operation
};

struct ConflictInfo {
    EditOperation localOp;
    EditOperation remoteOp;
    ConflictStrategy resolution = ConflictStrategy::LastWriterWins;
    bool resolved = false;
};

// ============================================================================
// COLLABORATIVE EDITING SYSTEM
// ============================================================================

// Provides real-time collaborative scene editing over the existing NetworkSystem.
// Uses an OT-like approach:
// - Each edit generates an EditOperation with a Lamport clock
// - Operations are broadcast to all peers via reliable network messages
// - Concurrent edits on the same entity/component are detected and resolved
// - Transform operations use an optimized binary path (not JSON)
// - Full state sync on join (host sends complete scene JSON)
class ENJIN_API CollaborativeEditingSystem {
public:
    CollaborativeEditingSystem() = default;
    ~CollaborativeEditingSystem() = default;

    // ========================================================================
    // LIFECYCLE
    // ========================================================================

    void Initialize(ECS::World* world, Networking::NetworkSystem* network);
    void Shutdown();
    void Update(f32 deltaTime);

    // ========================================================================
    // SESSION MANAGEMENT
    // ========================================================================

    // Host a collaborative editing session on the given port
    bool HostSession(u16 port, const std::string& userName);

    // Join an existing collaborative editing session
    bool JoinSession(const std::string& hostIP, u16 port, const std::string& userName);

    // Disconnect from the current session
    void LeaveSession();

    CollabSessionState GetState() const { return m_State; }
    bool IsActive() const { return m_State == CollabSessionState::Hosting || m_State == CollabSessionState::Connected; }
    bool IsHost() const { return m_State == CollabSessionState::Hosting; }

    // ========================================================================
    // LOCAL EDIT RECORDING
    // ========================================================================

    // Call these from the editor when local edits happen.
    // They record the operation and broadcast to peers.

    void OnEntityCreated(ECS::Entity entity, const std::string& entityJson);
    void OnEntityDeleted(ECS::Entity entity, const std::string& entityJson);
    void OnEntityRenamed(ECS::Entity entity, const std::string& oldName, const std::string& newName);
    void OnComponentChanged(ECS::Entity entity, const std::string& componentKey,
                            const std::string& newJson, const std::string& oldJson);
    void OnComponentRemoved(ECS::Entity entity, const std::string& componentKey,
                            const std::string& oldJson);
    void OnTransformChanged(ECS::Entity entity, const Math::Vector3& pos,
                            const Math::Vector3& rot, const Math::Vector3& scl);
    void OnEntityReparented(ECS::Entity entity, ECS::Entity newParent);

    // ========================================================================
    // PEER AWARENESS
    // ========================================================================

    const std::vector<CollabPeer>& GetPeers() const { return m_Peers; }
    void SetLocalCursorEntity(ECS::Entity entity);
    void SetLocalCameraPosition(const Math::Vector3& pos);

    // Entity currently being edited by another peer (for conflict avoidance UI)
    bool IsEntityBeingEditedByOther(ECS::Entity entity) const;
    const CollabPeer* GetEntityEditor(ECS::Entity entity) const;

    // ========================================================================
    // CONFLICT HANDLING
    // ========================================================================

    ConflictStrategy GetConflictStrategy() const { return m_ConflictStrategy; }
    void SetConflictStrategy(ConflictStrategy strategy) { m_ConflictStrategy = strategy; }

    // Get unresolved conflicts (for UI display)
    const std::vector<ConflictInfo>& GetConflicts() const { return m_UnresolvedConflicts; }
    void ResolveConflict(usize index, ConflictStrategy resolution);

    // ========================================================================
    // OPERATION LOG
    // ========================================================================

    const std::deque<EditOperation>& GetOperationLog() const { return m_OperationLog; }
    usize GetOperationCount() const { return m_OperationLog.size(); }
    u64 GetCurrentSequence() const { return m_LocalSequence; }

    // ========================================================================
    // CALLBACKS
    // ========================================================================

    using RemoteEditCallback = std::function<void(const EditOperation& op)>;

    // Called when a remote operation should be applied to the local scene
    void SetOnRemoteEdit(RemoteEditCallback cb) { m_OnRemoteEdit = std::move(cb); }

    // Called when full scene sync is needed (joining peer receives complete scene)
    using SceneSyncCallback = std::function<std::string()>; // Returns full scene JSON
    void SetOnSceneSyncRequest(SceneSyncCallback cb) { m_OnSceneSyncRequest = std::move(cb); }

    using SceneLoadCallback = std::function<void(const std::string& json)>;
    void SetOnSceneSyncReceived(SceneLoadCallback cb) { m_OnSceneSyncReceived = std::move(cb); }

private:
    // ========================================================================
    // INTERNAL
    // ========================================================================

    // Create an operation with proper metadata
    EditOperation MakeOperation(EditOpType type, ECS::Entity entity);

    // Broadcast an operation to all peers
    void BroadcastOperation(const EditOperation& op);

    // Process a received operation from a remote peer
    void ProcessRemoteOperation(const EditOperation& op);

    // Detect and handle conflicts
    bool DetectConflict(const EditOperation& local, const EditOperation& remote);
    EditOperation ResolveConflictInternal(const EditOperation& local, const EditOperation& remote);

    // Serialization
    std::vector<u8> SerializeOperation(const EditOperation& op) const;
    EditOperation DeserializeOperation(const u8* data, u32 size) const;

    // Network message handlers (registered as RPCs)
    void HandleEditOp(u8 senderId, const u8* data, u32 size);
    void HandleSyncRequest(u8 senderId, const u8* data, u32 size);
    void HandleSyncResponse(u8 senderId, const u8* data, u32 size);
    void HandlePeerCursor(u8 senderId, const u8* data, u32 size);

    // Transform batching (coalesce rapid transform changes)
    void FlushPendingTransforms();

    // ========================================================================
    // STATE
    // ========================================================================

    ECS::World* m_World = nullptr;
    Networking::NetworkSystem* m_Network = nullptr;
    CollabSessionState m_State = CollabSessionState::Disconnected;
    ConflictStrategy m_ConflictStrategy = ConflictStrategy::LastWriterWins;

    // Identity
    std::string m_UserName;
    u8 m_LocalPeerId = 0;

    // Causality tracking
    u64 m_LamportClock = 0;        // Legacy, kept for backward compat
    u64 m_LocalSequence = 0;

    // CRDT document — source of truth for conflict-free merging
    CRDTDocument m_CRDTDoc;

    // Peers
    std::vector<CollabPeer> m_Peers;

    // Operation log (bounded, oldest trimmed when > MAX_LOG_SIZE)
    // S-M1: Capped to prevent unbounded growth
    std::deque<EditOperation> m_OperationLog;
    static constexpr usize MAX_LOG_SIZE = 10000;

    // Pending operations from remote peers (buffered during sync)
    std::vector<EditOperation> m_PendingRemoteOps;

    // Recent local edits per entity (for conflict detection window)
    std::unordered_map<u64, EditOperation> m_RecentLocalEdits;

    // Unresolved conflicts
    std::vector<ConflictInfo> m_UnresolvedConflicts;

    // Transform batching
    struct PendingTransform {
        Math::Vector3 position;
        Math::Vector3 rotation;
        Math::Vector3 scale;
        f32 lastModified = 0.0f;
    };
    std::unordered_map<u64, PendingTransform> m_PendingTransforms;
    f32 m_TransformFlushInterval = 0.1f;  // Batch transforms for 100ms
    f32 m_TransformFlushTimer = 0.0f;

    // Cursor broadcast
    f32 m_CursorBroadcastTimer = 0.0f;
    f32 m_CursorBroadcastInterval = 0.5f;
    u32 m_LocalCursorEntity = 0;
    Math::Vector3 m_LocalCameraPos;

    // Callbacks
    RemoteEditCallback m_OnRemoteEdit;
    SceneSyncCallback m_OnSceneSyncRequest;
    SceneLoadCallback m_OnSceneSyncReceived;

    // Time
    f32 m_Time = 0.0f;
};

} // namespace Editor
} // namespace Enjin
