#include "Enjin/Editor/CollaborativeEditing.h"
#include "Enjin/Networking/NetworkSystem.h"
#include "Enjin/ECS/World.h"
#include "Enjin/Logging/Log.h"
#include <cstring>
#include <algorithm>
#include <cmath>

namespace Enjin {
namespace Editor {

// RPC names for collaborative editing
static const char* RPC_EDIT_OP = "collab_edit_op";
static const char* RPC_SYNC_REQUEST = "collab_sync_req";
static const char* RPC_SYNC_RESPONSE = "collab_sync_resp";
static const char* RPC_PEER_CURSOR = "collab_cursor";

// ============================================================================
// LIFECYCLE
// ============================================================================

void CollaborativeEditingSystem::Initialize(ECS::World* world, Networking::NetworkSystem* network) {
    m_World = world;
    m_Network = network;

    if (m_Network) {
        // Register RPCs for collab messages
        m_Network->RegisterRPC(RPC_EDIT_OP,
            [this](Networking::PlayerId sender, const u8* data, u32 size) {
                HandleEditOp(sender, data, size);
            }, true); // Reliable

        m_Network->RegisterRPC(RPC_SYNC_REQUEST,
            [this](Networking::PlayerId sender, const u8* data, u32 size) {
                HandleSyncRequest(sender, data, size);
            }, true);

        m_Network->RegisterRPC(RPC_SYNC_RESPONSE,
            [this](Networking::PlayerId sender, const u8* data, u32 size) {
                HandleSyncResponse(sender, data, size);
            }, true);

        m_Network->RegisterRPC(RPC_PEER_CURSOR,
            [this](Networking::PlayerId sender, const u8* data, u32 size) {
                HandlePeerCursor(sender, data, size);
            }, false); // Unreliable (frequent)
    }

    ENJIN_LOG_INFO(Editor, "CollaborativeEditingSystem initialized");
}

void CollaborativeEditingSystem::Shutdown() {
    LeaveSession();
    m_OperationLog.clear();
    m_Peers.clear();
    m_PendingRemoteOps.clear();
    m_RecentLocalEdits.clear();
    m_UnresolvedConflicts.clear();
    m_PendingTransforms.clear();
    ENJIN_LOG_INFO(Editor, "CollaborativeEditingSystem shut down");
}

void CollaborativeEditingSystem::Update(f32 deltaTime) {
    if (!IsActive()) return;

    m_Time += deltaTime;

    // Flush batched transform operations
    m_TransformFlushTimer += deltaTime;
    if (m_TransformFlushTimer >= m_TransformFlushInterval) {
        FlushPendingTransforms();
        m_TransformFlushTimer = 0.0f;
    }

    // Broadcast cursor/camera position periodically
    m_CursorBroadcastTimer += deltaTime;
    if (m_CursorBroadcastTimer >= m_CursorBroadcastInterval && m_Network && m_Network->IsConnected()) {
        m_CursorBroadcastTimer = 0.0f;

        // Pack cursor data: entityId (4) + cameraPos (12) = 16 bytes
        u8 cursorData[16];
        std::memcpy(cursorData, &m_LocalCursorEntity, 4);
        std::memcpy(cursorData + 4, &m_LocalCameraPos.x, 4);
        std::memcpy(cursorData + 8, &m_LocalCameraPos.y, 4);
        std::memcpy(cursorData + 12, &m_LocalCameraPos.z, 4);
        m_Network->CallRPCAll(RPC_PEER_CURSOR, cursorData, 16);
    }

    // Clean up stale peer entries
    for (auto& peer : m_Peers) {
        if (peer.connected && (m_Time - peer.lastHeartbeat) > 15.0f) {
            peer.connected = false;
            ENJIN_LOG_WARN(Editor, "Collab peer '%s' timed out", peer.name.c_str());
        }
    }

    // S-L2: Remove peers that haven't sent a message in >30 seconds to prevent unbounded growth
    m_Peers.erase(
        std::remove_if(m_Peers.begin(), m_Peers.end(),
            [this](const CollabPeer& peer) {
                return peer.peerId != m_LocalPeerId && !peer.connected &&
                       (m_Time - peer.lastHeartbeat) > 30.0f;
            }),
        m_Peers.end());

    // Trim old local edit records (keep last 5 seconds for conflict detection)
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count() / 1000.0;
    for (auto it = m_RecentLocalEdits.begin(); it != m_RecentLocalEdits.end(); ) {
        if (now - it->second.timestamp > 5.0) {
            it = m_RecentLocalEdits.erase(it);
        } else {
            ++it;
        }
    }
}

// ============================================================================
// SESSION MANAGEMENT
// ============================================================================

bool CollaborativeEditingSystem::HostSession(u16 port, const std::string& userName) {
    if (!m_Network) return false;

    if (!m_Network->HostGame(port, userName)) {
        ENJIN_LOG_ERROR(Editor, "Failed to host collaborative session on port %u", port);
        return false;
    }

    m_UserName = userName;
    m_LocalPeerId = 0; // Host is always peer 0
    m_CRDTDoc.SetLocalSiteId(0);
    m_Permissions.Clear();
    m_Permissions.SetPeerPermission(0, CollabPermission::Owner);
    m_State = CollabSessionState::Hosting;
    m_LamportClock = 0;
    m_LocalSequence = 0;
    m_OperationLog.clear();
    m_Peers.clear();

    // Add self as peer (host is always Owner)
    CollabPeer self;
    self.peerId = m_LocalPeerId;
    self.name = userName;
    self.connected = true;
    self.lastHeartbeat = m_Time;
    self.permission = CollabPermission::Owner;
    m_Peers.push_back(self);

    ENJIN_LOG_INFO(Editor, "Hosting collaborative session on port %u as '%s'", port, userName.c_str());
    return true;
}

bool CollaborativeEditingSystem::JoinSession(const std::string& hostIP, u16 port, const std::string& userName) {
    if (!m_Network) return false;

    if (!m_Network->JoinGame(hostIP, port, userName)) {
        ENJIN_LOG_ERROR(Editor, "Failed to join collaborative session at %s:%u", hostIP.c_str(), port);
        return false;
    }

    m_UserName = userName;
    m_State = CollabSessionState::Joining;
    m_LamportClock = 0;
    m_LocalSequence = 0;
    m_OperationLog.clear();
    m_Peers.clear();

    ENJIN_LOG_INFO(Editor, "Joining collaborative session at %s:%u as '%s'", hostIP.c_str(), port, userName.c_str());

    // Request full scene sync from host
    if (m_Network->IsConnected()) {
        m_State = CollabSessionState::Syncing;
        u8 dummy = 0;
        m_Network->CallRPC(RPC_SYNC_REQUEST, 0, &dummy, 1); // target host (peerId 0)
    }

    return true;
}

void CollaborativeEditingSystem::LeaveSession() {
    if (m_State == CollabSessionState::Disconnected) return;

    if (m_Network) {
        m_Network->Disconnect();
    }

    m_State = CollabSessionState::Disconnected;
    m_Peers.clear();
    m_PendingTransforms.clear();
    ENJIN_LOG_INFO(Editor, "Left collaborative session");
}

void CollaborativeEditingSystem::GoOffline() {
    if (m_State == CollabSessionState::Disconnected ||
        m_State == CollabSessionState::Offline) return;

    m_State = CollabSessionState::Offline;
    m_OfflineLog.SetSiteId(m_LocalPeerId);

    // Flush any pending transforms before going offline
    FlushPendingTransforms();

    ENJIN_LOG_INFO(Editor, "Collab: Going offline — operations will be logged to disk");
}

void CollaborativeEditingSystem::AttemptReconnect() {
    if (m_State != CollabSessionState::Offline) return;

    // For now, reconnect is manual — the user re-hosts or re-joins.
    // When they do, the offline log is used to compute catch-up ops.
    // The merge UI is triggered by the caller (CollaborativeEditingUI)
    // when it detects that the offline log has entries.
    m_State = CollabSessionState::Merging;
    ENJIN_LOG_INFO(Editor, "Collab: Attempting reconnect — %zu offline ops to reconcile",
        m_OfflineLog.Count());
}

// ============================================================================
// LOCAL EDIT RECORDING
// ============================================================================

void CollaborativeEditingSystem::OnEntityCreated(ECS::Entity entity, const std::string& entityJson) {
    if (!IsActive()) return;

    EditOperation op = MakeOperation(EditOpType::CreateEntity, entity);
    op.dataJson = entityJson;
    BroadcastOperation(op);
}

void CollaborativeEditingSystem::OnEntityDeleted(ECS::Entity entity, const std::string& entityJson) {
    if (!IsActive()) return;

    EditOperation op = MakeOperation(EditOpType::DeleteEntity, entity);
    op.previousJson = entityJson;
    BroadcastOperation(op);
}

void CollaborativeEditingSystem::OnEntityRenamed(ECS::Entity entity, const std::string& oldName, const std::string& newName) {
    if (!IsActive()) return;

    EditOperation op = MakeOperation(EditOpType::RenameEntity, entity);
    op.dataJson = newName;
    op.previousJson = oldName;
    BroadcastOperation(op);
}

void CollaborativeEditingSystem::OnComponentChanged(ECS::Entity entity, const std::string& componentKey,
                                                     const std::string& newJson, const std::string& oldJson) {
    if (!IsActive()) return;

    EditOperation op = MakeOperation(EditOpType::SetComponent, entity);
    op.componentKey = componentKey;
    op.dataJson = newJson;
    op.previousJson = oldJson;
    BroadcastOperation(op);
}

void CollaborativeEditingSystem::OnComponentRemoved(ECS::Entity entity, const std::string& componentKey,
                                                     const std::string& oldJson) {
    if (!IsActive()) return;

    EditOperation op = MakeOperation(EditOpType::RemoveComponent, entity);
    op.componentKey = componentKey;
    op.previousJson = oldJson;
    BroadcastOperation(op);
}

void CollaborativeEditingSystem::OnTransformChanged(ECS::Entity entity, const Math::Vector3& pos,
                                                     const Math::Vector3& rot, const Math::Vector3& scl) {
    if (!IsActive()) return;

    // Batch transform changes (they're high-frequency during gizmo drag)
    u64 eid = static_cast<u64>(entity);
    auto& pending = m_PendingTransforms[eid];
    pending.position = pos;
    pending.rotation = rot;
    pending.scale = scl;
    pending.lastModified = m_Time;
}

void CollaborativeEditingSystem::OnEntityReparented(ECS::Entity entity, ECS::Entity newParent) {
    if (!IsActive()) return;

    EditOperation op = MakeOperation(EditOpType::SetParent, entity);
    op.dataJson = std::to_string(static_cast<u64>(newParent));
    BroadcastOperation(op);
}

// ============================================================================
// PEER AWARENESS
// ============================================================================

void CollaborativeEditingSystem::SetLocalCursorEntity(ECS::Entity entity) {
    m_LocalCursorEntity = static_cast<u32>(entity);
}

void CollaborativeEditingSystem::SetLocalCameraPosition(const Math::Vector3& pos) {
    m_LocalCameraPos = pos;
}

bool CollaborativeEditingSystem::IsEntityBeingEditedByOther(ECS::Entity entity) const {
    u32 eid = static_cast<u32>(entity);
    for (const auto& peer : m_Peers) {
        if (peer.peerId != m_LocalPeerId && peer.connected && peer.cursorEntityId == eid) {
            return true;
        }
    }
    return false;
}

const CollabPeer* CollaborativeEditingSystem::GetEntityEditor(ECS::Entity entity) const {
    u32 eid = static_cast<u32>(entity);
    for (const auto& peer : m_Peers) {
        if (peer.peerId != m_LocalPeerId && peer.connected && peer.cursorEntityId == eid) {
            return &peer;
        }
    }
    return nullptr;
}

// ============================================================================
// CONFLICT HANDLING
// ============================================================================

void CollaborativeEditingSystem::ResolveConflict(usize index, ConflictStrategy resolution) {
    if (index >= m_UnresolvedConflicts.size()) return;

    const ConflictInfo& conflict = m_UnresolvedConflicts[index];

    // Apply the resolution. Copy what we need first: every branch below can touch
    // m_UnresolvedConflicts, and the erase at the end invalidates the reference.
    const EditOperation localOp = conflict.localOp;
    const EditOperation remoteOp = conflict.remoteOp;
    m_UnresolvedConflicts.erase(m_UnresolvedConflicts.begin() + index);

    switch (resolution) {
        case ConflictStrategy::LastWriterWins:
        case ConflictStrategy::HostAuthority:
            // The remote operation wins -- apply it.
            if (m_OnRemoteEdit) m_OnRemoteEdit(remoteOp);
            break;

        case ConflictStrategy::Merge: {
            // Merge was silently a no-op: it matched neither arm of the old if, so
            // the "Merge" button in the conflict list did exactly what "Keep Mine"
            // did, while saying something else.
            const EditOperation merged = ResolveConflictInternal(localOp, remoteOp);
            if (m_OnRemoteEdit) m_OnRemoteEdit(merged);
            // Tell the peer what we landed on, or only this machine has the merge.
            ReassertLocalOperation(merged);
            break;
        }

        case ConflictStrategy::Reject:
            // Keep local state -- and say so on the wire. The peer applied its own
            // edit when it made it, so staying quiet leaves the two scenes different
            // and both sides convinced they are in sync.
            ReassertLocalOperation(localOp);
            break;
    }
}

// ============================================================================
// INTERNAL — OPERATION CREATION
// ============================================================================

EditOperation CollaborativeEditingSystem::MakeOperation(EditOpType type, ECS::Entity entity) {
    EditOperation op;
    op.type = type;
    op.entityId = static_cast<u64>(entity);
    op.sequenceId = ++m_LocalSequence;
    op.lamportClock = ++m_LamportClock;
    op.authorId = m_LocalPeerId;
    op.authorName = m_UserName;
    op.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count() / 1000.0;
    // CRDT: record local op stamps the vector clock and updates CRDT state
    m_CRDTDoc.RecordLocalOp(op);
    op.lamportClock = op.vclock.MaxComponent();  // Backward compat
    return op;
}

// ============================================================================
// BROADCASTING
// ============================================================================

void CollaborativeEditingSystem::BroadcastOperation(const EditOperation& op) {
    // Log the operation
    m_OperationLog.push_back(op);
    while (m_OperationLog.size() > MAX_LOG_SIZE) {
        m_OperationLog.pop_front();
    }

    // Track for conflict detection
    m_RecentLocalEdits[op.entityId] = op;

    if (m_State == CollabSessionState::Offline) {
        // Offline: persist to disk instead of sending over network
        m_OfflineLog.AppendOperation(op);
        return;
    }

    // Broadcast to all peers via RPC
    if (m_Network && m_Network->IsConnected()) {
        auto data = SerializeOperation(op);
        m_Network->CallRPCAll(RPC_EDIT_OP, data.data(), static_cast<u32>(data.size()));
    }
}

void CollaborativeEditingSystem::FlushPendingTransforms() {
    for (auto& [entityId, pending] : m_PendingTransforms) {
        EditOperation op = MakeOperation(EditOpType::ModifyTransform, static_cast<ECS::Entity>(entityId));
        op.position = pending.position;
        op.rotation = pending.rotation;
        op.scale = pending.scale;
        BroadcastOperation(op);
    }
    m_PendingTransforms.clear();
}

// ============================================================================
// REMOTE OPERATION PROCESSING
// ============================================================================

void CollaborativeEditingSystem::ProcessRemoteOperation(const EditOperation& op) {
    // Update Lamport clock (legacy)
    m_LamportClock = std::max(m_LamportClock, op.lamportClock) + 1;

    // Log the operation
    m_OperationLog.push_back(op);
    while (m_OperationLog.size() > MAX_LOG_SIZE) {
        m_OperationLog.pop_front();
    }

    // CRDT merge. This always runs, whatever the strategy: the document is how two
    // machines agree on a value, and skipping the merge would make this peer's view
    // of the shared document wrong rather than merely different.
    bool shouldApply = m_CRDTDoc.ApplyRemoteOp(op);
    EditOperation toApply = op;

    // Does this collide with an edit WE made inside the conflict window?
    //
    // DetectConflict had no caller at all. The Conflict Strategy combo (in the
    // Collaboration panel AND in the collab UI window) wrote m_ConflictStrategy and
    // nothing ever read it, m_UnresolvedConflicts was never appended to so the
    // Conflicts list was permanently empty in three places, and ResolveConflict was
    // therefore unreachable. Every session ran last-writer-wins regardless of what
    // the combo said.
    const EditOperation* localOp = nullptr;
    auto recent = m_RecentLocalEdits.find(op.entityId);
    if (recent != m_RecentLocalEdits.end() && DetectConflict(recent->second, op)) {
        localOp = &recent->second;
    }

    if (localOp) {
        switch (DecideConflict(m_ConflictStrategy, IsHost(), *localOp, op)) {
            case ConflictOutcome::ApplyRemote:
                break;   // the CRDT's verdict stands

            case ConflictOutcome::ForceApplyRemote:
                shouldApply = true;
                break;

            case ConflictOutcome::ApplyMerged:
                toApply = MergeOperations(*localOp, op);
                shouldApply = true;
                break;

            case ConflictOutcome::KeepLocalAndReassert: {
                ENJIN_LOG_INFO(Editor,
                    "Collab: kept the local edit to entity %llu over '%s' and re-sent it",
                    (unsigned long long)op.entityId, op.authorName.c_str());
                // Copy first: reassert writes m_RecentLocalEdits, which can rehash
                // and leave localOp dangling.
                const EditOperation mine = *localOp;
                ReassertLocalOperation(mine);
                return;
            }

            case ConflictOutcome::HoldForReview: {
                ConflictInfo info;
                info.localOp = *localOp;
                info.remoteOp = op;
                info.resolved = false;
                m_UnresolvedConflicts.push_back(info);
                ENJIN_LOG_WARN(Editor,
                    "Collab: held a conflicting edit to entity %llu from '%s' for review",
                    (unsigned long long)op.entityId, op.authorName.c_str());
                return;
            }
        }
    }

    if (shouldApply && m_OnRemoteEdit) {
        m_OnRemoteEdit(toApply);
    }
}

ConflictOutcome DecideConflict(ConflictStrategy strategy, bool isHost,
                               const EditOperation& local, const EditOperation& remote) {
    (void)local;
    switch (strategy) {
        case ConflictStrategy::LastWriterWins:
            // The CRDT already decided, by vector clock.
            return ConflictOutcome::ApplyRemote;

        case ConflictStrategy::HostAuthority:
            // The host's version of the scene is the one that stands, so the host
            // keeps its own edit and re-sends it. A client defers to an edit that
            // came from the host whatever the clocks say -- that is the whole point
            // of the setting, and respecting the clock there would just be
            // last-writer-wins under another name.
            if (isHost) return ConflictOutcome::KeepLocalAndReassert;
            if (remote.authorId == kHostPeerId) return ConflictOutcome::ForceApplyRemote;
            // Two clients conflicting with each other: neither is authoritative and
            // the host is not involved, so there is nothing to arbitrate.
            return ConflictOutcome::ApplyRemote;

        case ConflictStrategy::Merge:
            return ConflictOutcome::ApplyMerged;

        case ConflictStrategy::Reject:
            return ConflictOutcome::HoldForReview;
    }
    // Unreachable for a valid enum. Falling back to the CRDT's own verdict is the
    // one answer that cannot make the document inconsistent.
    return ConflictOutcome::ApplyRemote;
}

void CollaborativeEditingSystem::ReassertLocalOperation(const EditOperation& winner) {
    EditOperation op = MakeOperation(winner.type, static_cast<ECS::Entity>(winner.entityId));
    op.componentKey = winner.componentKey;
    op.dataJson     = winner.dataJson;
    op.previousJson = winner.previousJson;
    op.position     = winner.position;
    op.rotation     = winner.rotation;
    op.scale        = winner.scale;
    BroadcastOperation(op);
}

bool CollaborativeEditingSystem::DetectConflict(const EditOperation& local, const EditOperation& remote) {
    // Conflict: same entity, same component, within a short time window
    if (local.entityId != remote.entityId) return false;

    // Different operation types on the same entity is usually fine
    // (e.g., one renames, another changes transform)
    if (local.type != remote.type) return false;

    // Same component key means a real conflict
    if (local.type == EditOpType::SetComponent && remote.type == EditOpType::SetComponent) {
        return local.componentKey == remote.componentKey;
    }

    // Transform conflicts
    if (local.type == EditOpType::ModifyTransform && remote.type == EditOpType::ModifyTransform) {
        return true;
    }

    // Delete conflicts with anything on the same entity
    if (local.type == EditOpType::DeleteEntity || remote.type == EditOpType::DeleteEntity) {
        return true;
    }

    return false;
}

EditOperation CollaborativeEditingSystem::ResolveConflictInternal(const EditOperation& local, const EditOperation& remote) {
    return MergeOperations(local, remote);
}

EditOperation MergeOperations(const EditOperation& local, const EditOperation& remote) {
    // For transforms: average positions, take the later rotation and scale.
    if (local.type == EditOpType::ModifyTransform && remote.type == EditOpType::ModifyTransform) {
        EditOperation merged = remote; // Start with remote as base

        merged.position.x = (local.position.x + remote.position.x) * 0.5f;
        merged.position.y = (local.position.y + remote.position.y) * 0.5f;
        merged.position.z = (local.position.z + remote.position.z) * 0.5f;

        if (local.lamportClock > remote.lamportClock) {
            merged.rotation = local.rotation;
            merged.scale = local.scale;
        }

        return merged;
    }

    // Everything else has no fields to blend: two JSON payloads for the same
    // component do not average into a third valid one. Last-writer-wins is the
    // honest answer, and the tooltip says so rather than implying a merge happened.
    return remote.lamportClock > local.lamportClock ? remote : local;
}

// ============================================================================
// SERIALIZATION
// ============================================================================

std::vector<u8> CollaborativeEditingSystem::SerializeOperation(const EditOperation& op) const {
    std::vector<u8> data;
    data.reserve(256);

    // Header: type (1) + entityId (8) + sequenceId (8) + lamportClock (8) + authorId (1) + timestamp (8)
    data.push_back(static_cast<u8>(op.type));

    auto writeU64 = [&data](u64 v) {
        for (int i = 0; i < 8; ++i) data.push_back(static_cast<u8>((v >> (i * 8)) & 0xFF));
    };
    auto writeF32 = [&data](f32 v) {
        u8 bytes[4];
        std::memcpy(bytes, &v, 4);
        data.insert(data.end(), bytes, bytes + 4);
    };
    auto writeString = [&data](const std::string& s) {
        u32 len = static_cast<u32>(s.size());
        for (int i = 0; i < 4; ++i) data.push_back(static_cast<u8>((len >> (i * 8)) & 0xFF));
        data.insert(data.end(), s.begin(), s.end());
    };

    writeU64(op.entityId);
    writeU64(op.sequenceId);
    writeU64(op.lamportClock);
    data.push_back(op.authorId);

    f64 ts = op.timestamp;
    u8 tsBytes[8];
    std::memcpy(tsBytes, &ts, 8);
    data.insert(data.end(), tsBytes, tsBytes + 8);

    // Type-specific data
    switch (op.type) {
        case EditOpType::ModifyTransform:
            writeF32(op.position.x); writeF32(op.position.y); writeF32(op.position.z);
            writeF32(op.rotation.x); writeF32(op.rotation.y); writeF32(op.rotation.z);
            writeF32(op.scale.x); writeF32(op.scale.y); writeF32(op.scale.z);
            break;

        case EditOpType::CreateEntity:
        case EditOpType::DeleteEntity:
        case EditOpType::RenameEntity:
        case EditOpType::SetComponent:
        case EditOpType::RemoveComponent:
        case EditOpType::SetParent:
            writeString(op.componentKey);
            writeString(op.dataJson);
            writeString(op.previousJson);
            writeString(op.authorName);
            break;

        default:
            writeString(op.dataJson);
            break;
    }

    return data;
}

EditOperation CollaborativeEditingSystem::DeserializeOperation(const u8* data, u32 size) const {
    EditOperation op;
    if (size < 35) return op; // Minimum header size

    u32 pos = 0;
    auto readU8 = [&]() -> u8 { return pos < size ? data[pos++] : 0; };
    auto readU64 = [&]() -> u64 {
        u64 v = 0;
        for (int i = 0; i < 8 && pos < size; ++i) v |= static_cast<u64>(data[pos++]) << (i * 8);
        return v;
    };
    auto readF32 = [&]() -> f32 {
        f32 v = 0;
        if (pos + 4 <= size) { std::memcpy(&v, data + pos, 4); pos += 4; }
        return v;
    };
    auto readString = [&]() -> std::string {
        u32 len = 0;
        for (int i = 0; i < 4 && pos < size; ++i) len |= static_cast<u32>(data[pos++]) << (i * 8);
        if (len > size - pos) len = size - pos;
        // S-M2: Cap string length to 64KB to prevent unbounded allocation from malicious data
        constexpr u32 MAX_STRING_LEN = 64 * 1024;
        if (len > MAX_STRING_LEN) {
            ENJIN_LOG_WARN(Editor, "Collab: readString length %u exceeds 64KB cap, truncating", len);
            len = MAX_STRING_LEN;
        }
        std::string s(reinterpret_cast<const char*>(data + pos), len);
        pos += len;
        return s;
    };

    u8 rawType = readU8();
    // S-C2: Validate EditOpType enum range before applying
    if (rawType < static_cast<u8>(EditOpType::CreateEntity) ||
        rawType > static_cast<u8>(EditOpType::UnlockEntity)) {
        ENJIN_LOG_WARN(Editor, "Collab: Invalid EditOpType %u from remote, rejecting", rawType);
        return op;
    }
    op.type = static_cast<EditOpType>(rawType);
    op.entityId = readU64();
    op.sequenceId = readU64();
    op.lamportClock = readU64();
    op.authorId = readU8();

    f64 ts = 0;
    if (pos + 8 <= size) { std::memcpy(&ts, data + pos, 8); pos += 8; }
    op.timestamp = ts;

    switch (op.type) {
        case EditOpType::ModifyTransform:
            op.position.x = readF32(); op.position.y = readF32(); op.position.z = readF32();
            op.rotation.x = readF32(); op.rotation.y = readF32(); op.rotation.z = readF32();
            op.scale.x = readF32(); op.scale.y = readF32(); op.scale.z = readF32();

            // S-H4: Validate all transform floats are finite (reject NaN/Inf from remote)
            if (!std::isfinite(op.position.x) || !std::isfinite(op.position.y) || !std::isfinite(op.position.z) ||
                !std::isfinite(op.rotation.x) || !std::isfinite(op.rotation.y) || !std::isfinite(op.rotation.z) ||
                !std::isfinite(op.scale.x) || !std::isfinite(op.scale.y) || !std::isfinite(op.scale.z)) {
                ENJIN_LOG_WARN(Editor, "Collab: NaN/Inf in remote transform data, zeroing");
                op.position = Math::Vector3(0, 0, 0);
                op.rotation = Math::Vector3(0, 0, 0);
                op.scale = Math::Vector3(1, 1, 1);
            }
            break;

        case EditOpType::CreateEntity:
        case EditOpType::DeleteEntity:
        case EditOpType::RenameEntity:
        case EditOpType::SetComponent:
        case EditOpType::RemoveComponent:
        case EditOpType::SetParent:
            op.componentKey = readString();
            op.dataJson = readString();
            op.previousJson = readString();
            op.authorName = readString();
            break;

        default:
            op.dataJson = readString();
            break;
    }

    return op;
}

// ============================================================================
// NETWORK MESSAGE HANDLERS
// ============================================================================

void CollaborativeEditingSystem::HandleEditOp(u8 senderId, const u8* data, u32 size) {
    EditOperation op = DeserializeOperation(data, size);

    // Permission enforcement via CollabPermissionManager
    if (IsHost()) {
        if (!m_Permissions.CanEditEntity(senderId, op.entityId)) {
            ENJIN_LOG_WARN(Editor, "Collab: Peer %u lacks permission to edit entity %llu, rejecting",
                senderId, (unsigned long long)op.entityId);
            return;
        }
        if (op.type == EditOpType::DeleteEntity && !m_Permissions.CanDeleteEntity(senderId)) {
            ENJIN_LOG_WARN(Editor, "Collab: Peer %u lacks permission to delete entities, rejecting", senderId);
            return;
        }
        if (op.type == EditOpType::CreateEntity && !m_Permissions.CanCreateEntity(senderId)) {
            ENJIN_LOG_WARN(Editor, "Collab: Peer %u lacks permission to create entities, rejecting", senderId);
            return;
        }
    }

    if (m_State == CollabSessionState::Syncing) {
        // Buffer operations during sync
        // S-M6: Cap pending remote ops to prevent unbounded growth
        if (m_PendingRemoteOps.size() >= 10000) {
            ENJIN_LOG_WARN(Editor, "Collab: Pending remote ops buffer full (10000), dropping oldest");
            m_PendingRemoteOps.erase(m_PendingRemoteOps.begin());
        }
        m_PendingRemoteOps.push_back(op);
        return;
    }

    // Update peer heartbeat
    for (auto& peer : m_Peers) {
        if (peer.peerId == senderId) {
            peer.lastHeartbeat = m_Time;
            break;
        }
    }

    ProcessRemoteOperation(op);
}

void CollaborativeEditingSystem::HandleSyncRequest(u8 senderId, const u8* /*data*/, u32 /*size*/) {
    // Host receives this — send full scene JSON to the requesting peer
    if (!IsHost()) return;

    ENJIN_LOG_INFO(Editor, "Collab: Peer %u requesting full scene sync", senderId);

    std::string sceneJson;
    if (m_OnSceneSyncRequest) {
        sceneJson = m_OnSceneSyncRequest();
    }

    // S-H10: Cap sync response size to 64MB to prevent unbounded allocation
    static constexpr size_t MAX_SYNC_SIZE = 64 * 1024 * 1024;
    if (!sceneJson.empty()) {
        if (sceneJson.size() > MAX_SYNC_SIZE) {
            ENJIN_LOG_ERROR(Editor, "Collab: Scene JSON too large for sync (%zu bytes, max 64MB)", sceneJson.size());
        } else {
            auto payload = reinterpret_cast<const u8*>(sceneJson.data());
            m_Network->CallRPC(RPC_SYNC_RESPONSE, senderId, payload, static_cast<u32>(sceneJson.size()));
        }
    }

    // Add the new peer
    CollabPeer peer;
    peer.peerId = senderId;
    peer.name = "Peer " + std::to_string(senderId);
    peer.connected = true;
    peer.lastHeartbeat = m_Time;

    // Check if peer already exists
    bool found = false;
    for (auto& p : m_Peers) {
        if (p.peerId == senderId) { p = peer; found = true; break; }
    }
    if (!found) m_Peers.push_back(peer);
}

void CollaborativeEditingSystem::HandleSyncResponse(u8 /*senderId*/, const u8* data, u32 size) {
    // Client receives full scene JSON from host
    if (IsHost()) return;

    std::string sceneJson(reinterpret_cast<const char*>(data), size);
    ENJIN_LOG_INFO(Editor, "Collab: Received full scene sync (%u bytes)", size);

    if (m_OnSceneSyncReceived) {
        m_OnSceneSyncReceived(sceneJson);
    }

    // Apply buffered operations that arrived during sync
    m_State = CollabSessionState::Connected;
    m_LocalPeerId = m_Network->GetLocalPlayerId();
    m_CRDTDoc.SetLocalSiteId(m_LocalPeerId);
    // Default permission for joining peers — host can promote/demote later
    m_Permissions.SetPeerPermission(m_LocalPeerId, CollabPermission::Editor);

    for (const auto& op : m_PendingRemoteOps) {
        ProcessRemoteOperation(op);
    }

    // S-M10: Log count BEFORE clearing so it prints the actual count
    ENJIN_LOG_INFO(Editor, "Collab: Sync complete, applied %zu buffered operations",
                   m_PendingRemoteOps.size());
    m_PendingRemoteOps.clear();
}

void CollaborativeEditingSystem::HandlePeerCursor(u8 senderId, const u8* data, u32 size) {
    if (size < 16) return;

    u32 cursorEntity = 0;
    Math::Vector3 cameraPos;
    std::memcpy(&cursorEntity, data, 4);
    std::memcpy(&cameraPos.x, data + 4, 4);
    std::memcpy(&cameraPos.y, data + 8, 4);
    std::memcpy(&cameraPos.z, data + 12, 4);

    // Update peer state
    for (auto& peer : m_Peers) {
        if (peer.peerId == senderId) {
            peer.cursorEntityId = cursorEntity;
            peer.cameraPos = cameraPos;
            peer.lastHeartbeat = m_Time;
            peer.connected = true;
            return;
        }
    }

    // New peer (cursor arrived before sync)
    CollabPeer peer;
    peer.peerId = senderId;
    peer.name = "Peer " + std::to_string(senderId);
    peer.cursorEntityId = cursorEntity;
    peer.cameraPos = cameraPos;
    peer.connected = true;
    peer.lastHeartbeat = m_Time;
    m_Peers.push_back(peer);
}

} // namespace Editor
} // namespace Enjin
