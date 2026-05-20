#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Editor/VectorClock.h"
#include "Enjin/Math/Vector.h"
#include <string>
#include <unordered_map>
#include <vector>
#include <memory>

namespace Enjin {

// Forward declare EditOperation — defined in CollaborativeEditing.h
namespace Editor { struct EditOperation; }

namespace Editor {

// ============================================================================
// LWW Register — Last-Writer-Wins Register CRDT
// ============================================================================
// Each field is tracked as (value, vectorClock, siteId).
// Merge rule: the register with the dominating clock wins.
// If concurrent, the higher siteId wins (deterministic tiebreak — same
// result on all nodes regardless of application order).

template<typename T>
struct LWWRegister {
    T value{};
    VectorClock clock;
    u8 siteId = 0;

    // Attempt to merge a remote value. Returns true if remote wins (value updated).
    bool Merge(const T& remoteValue, const VectorClock& remoteClock, u8 remoteSite) {
        i32 cmp = remoteClock.Compare(clock);
        if (cmp > 0) {
            // Remote happened-after local — remote wins
            value = remoteValue;
            clock = remoteClock;
            siteId = remoteSite;
            return true;
        }
        if (cmp == 0) {
            // Concurrent — deterministic tiebreak: higher siteId wins
            if (remoteSite > siteId) {
                value = remoteValue;
                clock = remoteClock;
                siteId = remoteSite;
                return true;
            }
        }
        // Local wins (local happened-after, or concurrent with higher local siteId)
        return false;
    }

    // Set locally (increments clock)
    void SetLocal(const T& newValue, VectorClock& localClock, u8 localSiteId) {
        localClock.Increment(localSiteId);
        value = newValue;
        clock = localClock;
        siteId = localSiteId;
    }
};

// ============================================================================
// Entity CRDT State — per-entity replicated state
// ============================================================================

struct EntityCRDTState {
    // Existence tracking (create/delete)
    LWWRegister<bool> exists;

    // Core fields with individual CRDT registers
    LWWRegister<std::string> name;
    LWWRegister<Math::Vector3> position;
    LWWRegister<Math::Vector3> rotation;  // Euler angles
    LWWRegister<Math::Vector3> scale;
    LWWRegister<u64> parentId;

    // Component-level tracking: componentKey -> full component JSON
    // Granularity is per-component (not per-field within a component).
    // This is pragmatic for v1 — per-field would require schema awareness.
    std::unordered_map<std::string, LWWRegister<std::string>> components;

    EntityCRDTState() {
        exists.value = true;
        scale.value = Math::Vector3(1.0f, 1.0f, 1.0f);
    }
};

// ============================================================================
// CRDT Document — top-level replicated scene state
// ============================================================================

class CRDTDocument {
public:
    CRDTDocument();
    ~CRDTDocument();  // Defined in .cpp where LogEntry is complete

    void SetLocalSiteId(u8 siteId) { m_LocalSiteId = siteId; }
    u8 GetLocalSiteId() const { return m_LocalSiteId; }

    VectorClock& GetLocalClock() { return m_LocalClock; }
    const VectorClock& GetLocalClock() const { return m_LocalClock; }

    // Get or create state for an entity
    EntityCRDTState& GetOrCreate(u64 entityId);
    const EntityCRDTState* Get(u64 entityId) const;

    // Apply a remote operation through the CRDT merge rules.
    // Returns true if the operation changed local state (should be applied to ECS).
    bool ApplyRemoteOp(const EditOperation& op);

    // Record a local operation (stamps the vector clock, updates CRDT state).
    void RecordLocalOp(EditOperation& op);

    // Get all operations that a peer with the given clock hasn't seen.
    // Used for catch-up after reconnect.
    std::vector<EditOperation> GetOpsSince(const VectorClock& peerClock) const;

    // Reset all state (e.g. on scene load)
    void Clear();

    // Number of tracked entities
    usize EntityCount() const { return m_Entities.size(); }

private:
    std::unordered_map<u64, EntityCRDTState> m_Entities;
    VectorClock m_LocalClock;
    u8 m_LocalSiteId = 0;

    // Operation log for catch-up (capped). Stored as opaque entries in the .cpp
    // to avoid circular include with EditOperation.
    static constexpr usize MAX_OP_LOG = 10000;
    struct LogEntry;  // Defined in CRDTState.cpp
    std::vector<std::unique_ptr<LogEntry>> m_OpLog;
    void TrimOpLog();
};

} // namespace Editor
} // namespace Enjin
