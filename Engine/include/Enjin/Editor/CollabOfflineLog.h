#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Editor/VectorClock.h"
#include <string>
#include <vector>
#include <fstream>

namespace Enjin {
namespace Editor {

// Forward declare — defined in CollaborativeEditing.h
struct EditOperation;

// ============================================================================
// CollabOfflineLog
// ============================================================================
// Append-only binary log of edit operations stored alongside the scene as a
// .enjincollab sidecar file. Used for:
//   1. Continuing edits when disconnected from peers
//   2. Catch-up after reconnect (send ops the other peer missed)
//   3. Crash recovery (replay ops from disk)
//
// File format:
//   Header: "ECLB" magic (4 bytes) + version u8 + siteId u8
//   Entries: sequential serialized EditOperations (same binary format as wire)

class ENJIN_API CollabOfflineLog {
public:
    CollabOfflineLog() = default;
    ~CollabOfflineLog();

    // Set the scene path — the log file will be scenePath + ".enjincollab"
    void SetPath(const std::string& scenePath);
    const std::string& GetPath() const { return m_LogPath; }

    // Set the local site ID (written to file header)
    void SetSiteId(u8 siteId) { m_SiteId = siteId; }

    // Append an operation to the log (writes to disk immediately)
    void AppendOperation(const EditOperation& op);

    // Load all operations from the log file
    std::vector<EditOperation> LoadAll() const;

    // Get operations that a peer with the given clock hasn't seen
    std::vector<EditOperation> GetOpsSince(const VectorClock& peerClock) const;

    // Remove operations that all peers have acknowledged (compaction).
    // Rewrites the file with only the remaining ops.
    void Compact(const VectorClock& allAckedClock);

    // Get the latest vector clock from the log
    VectorClock GetLatestClock() const { return m_LatestClock; }

    // Get number of logged operations (in memory)
    usize Count() const { return m_EntryCount; }

    // Clear the log file and in-memory state
    void Clear();

    // True if the log file exists on disk
    bool Exists() const;

private:
    // Write the file header (magic + version + siteId)
    void WriteHeader(std::ofstream& out) const;

    // Validate the file header. Returns false if invalid/corrupt.
    bool ValidateHeader(std::ifstream& in) const;

    std::string m_LogPath;
    u8 m_SiteId = 0;
    VectorClock m_LatestClock;
    usize m_EntryCount = 0;

    static constexpr u8 LOG_VERSION = 1;
    static constexpr char MAGIC[4] = {'E', 'C', 'L', 'B'};
};

} // namespace Editor
} // namespace Enjin
