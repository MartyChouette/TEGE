#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Math/Vector.h"
#include <string>
#include <vector>

namespace Enjin {
namespace ECS { class World; }
namespace Editor {

class CollaborativeEditingSystem;
struct EditOperation;

// ============================================================================
// Merge Diff Entry — one per entity that diverged during offline editing
// ============================================================================

struct MergeDiffEntry {
    u64 entityId = 0;
    std::string entityName;

    enum class Status : u8 {
        Unchanged,      // Same on both sides
        Modified,       // Both sides changed it differently
        AddedLocal,     // Created locally while offline
        AddedRemote,    // Created by the remote peer while we were offline
        DeletedLocal,   // Deleted locally while offline
        DeletedRemote,  // Deleted by remote while we were offline
        Conflicted      // Both sides made incompatible changes (e.g. both deleted + recreated)
    };
    Status status = Status::Unchanged;

    // Snapshots for side-by-side comparison
    std::string localSnapshot;    // JSON of local entity state
    std::string remoteSnapshot;   // JSON of remote entity state

    enum class Resolution : u8 {
        Undecided,
        KeepLocal,
        KeepRemote,
        AutoMerged    // CRDT handled it automatically
    };
    Resolution resolution = Resolution::Undecided;
};

// ============================================================================
// CollabMergeUI — merge dialog shown after reconnect when projects diverged
// ============================================================================

class ENJIN_API CollabMergeUI {
public:
    CollabMergeUI() = default;
    ~CollabMergeUI() = default;

    void Initialize(ECS::World* world, CollaborativeEditingSystem* collab);

    // Compute the diff between local and remote operations since disconnect.
    // localOps: operations performed locally while offline
    // remoteOps: operations the remote peer performed while we were offline
    void ComputeDiff(const std::vector<EditOperation>& localOps,
                     const std::vector<EditOperation>& remoteOps);

    // Draw the merge dialog (ImGui modal). Returns true when all resolutions
    // are decided and the user clicks "Apply".
    bool DrawMergeDialog();

    // True if there are unresolved merge entries
    bool HasPendingMerge() const { return m_HasPending; }

    // Apply the user's chosen resolutions to the ECS world
    void ApplyResolutions();

    // Get the diff entries (for inspection/testing)
    const std::vector<MergeDiffEntry>& GetEntries() const { return m_Entries; }

    // Clear all state
    void Clear();

private:
    std::vector<MergeDiffEntry> m_Entries;
    ECS::World* m_World = nullptr;
    CollaborativeEditingSystem* m_Collab = nullptr;
    bool m_HasPending = false;
};

} // namespace Editor
} // namespace Enjin
