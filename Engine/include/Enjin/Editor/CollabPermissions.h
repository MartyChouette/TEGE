#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include <unordered_map>
#include <vector>
#include <string>

namespace Enjin {
namespace Editor {

// Permission levels for collaborative editing peers
enum class CollabPermission : u8 {
    Viewer = 0,     // Can observe but not modify
    Editor = 1,     // Can modify entities/components
    Owner = 2       // Full control (can delete entities, kick peers)
};

// ============================================================================
// CollabPermissionManager
// ============================================================================
// Centralized permission checking for collaborative editing. Enforces
// Viewer/Editor/Owner roles at all edit paths and supports optional
// per-entity ownership (entity X can only be edited by peer Y).

class ENJIN_API CollabPermissionManager {
public:
    // --- Role-based checks ---

    // Can this peer create new entities?
    bool CanCreateEntity(u8 peerId) const {
        return GetPermission(peerId) >= CollabPermission::Editor;
    }

    // Can this peer edit the given entity?
    bool CanEditEntity(u8 peerId, u64 entityId) const {
        auto perm = GetPermission(peerId);
        if (perm < CollabPermission::Editor) return false;
        // Check entity-level ownership if set
        auto it = m_EntityOwners.find(entityId);
        if (it != m_EntityOwners.end() && it->second != peerId) {
            // Entity is owned by another peer — only Owner role can override
            return perm >= CollabPermission::Owner;
        }
        return true;
    }

    // Can this peer delete entities?
    bool CanDeleteEntity(u8 peerId) const {
        return GetPermission(peerId) >= CollabPermission::Owner;
    }

    // Can this peer modify a specific component on an entity?
    bool CanModifyComponent(u8 peerId, u64 entityId, const std::string& /*componentKey*/) const {
        return CanEditEntity(peerId, entityId);
    }

    // Can this peer change another peer's permissions?
    bool CanChangePermissions(u8 peerId) const {
        return GetPermission(peerId) >= CollabPermission::Owner;
    }

    // --- Permission management ---

    void SetPeerPermission(u8 peerId, CollabPermission perm) {
        m_PeerPermissions[peerId] = perm;
    }

    CollabPermission GetPermission(u8 peerId) const {
        auto it = m_PeerPermissions.find(peerId);
        return it != m_PeerPermissions.end() ? it->second : CollabPermission::Editor;
    }

    // --- Entity ownership (optional fine-grained control) ---

    void SetEntityOwner(u64 entityId, u8 peerId) {
        m_EntityOwners[entityId] = peerId;
    }

    void ClearEntityOwner(u64 entityId) {
        m_EntityOwners.erase(entityId);
    }

    bool HasEntityOwner(u64 entityId) const {
        return m_EntityOwners.count(entityId) > 0;
    }

    u8 GetEntityOwner(u64 entityId) const {
        auto it = m_EntityOwners.find(entityId);
        return it != m_EntityOwners.end() ? it->second : 0xFF;
    }

    // --- Serialization (for sync on join) ---

    std::vector<u8> Serialize() const {
        std::vector<u8> out;
        // Peer permissions: u8 count + (u8 peerId, u8 permission) pairs
        out.push_back(static_cast<u8>(m_PeerPermissions.size()));
        for (auto it = m_PeerPermissions.begin(); it != m_PeerPermissions.end(); ++it) {
            out.push_back(it->first);
            out.push_back(static_cast<u8>(it->second));
        }
        // Entity owners: u16 count + (u64 entityId, u8 ownerId) pairs
        u16 ownerCount = static_cast<u16>(m_EntityOwners.size());
        out.push_back(static_cast<u8>(ownerCount & 0xFF));
        out.push_back(static_cast<u8>((ownerCount >> 8) & 0xFF));
        for (auto it = m_EntityOwners.begin(); it != m_EntityOwners.end(); ++it) {
            u64 eid = it->first;
            for (int b = 0; b < 8; ++b) out.push_back(static_cast<u8>((eid >> (b * 8)) & 0xFF));
            out.push_back(it->second);
        }
        return out;
    }

    bool Deserialize(const u8* data, u32 size) {
        if (size < 1) return false;
        u32 pos = 0;

        // Peer permissions
        u8 permCount = data[pos++];
        for (u8 i = 0; i < permCount && pos + 1 < size; ++i) {
            u8 pid = data[pos++];
            u8 perm = data[pos++];
            if (perm <= static_cast<u8>(CollabPermission::Owner)) {
                m_PeerPermissions[pid] = static_cast<CollabPermission>(perm);
            }
        }

        // Entity owners
        if (pos + 2 > size) return true;  // No entity owners section
        u16 ownerCount = static_cast<u16>(data[pos]) | (static_cast<u16>(data[pos + 1]) << 8);
        pos += 2;
        for (u16 i = 0; i < ownerCount && pos + 9 <= size; ++i) {
            u64 eid = 0;
            for (int b = 0; b < 8; ++b) eid |= static_cast<u64>(data[pos++]) << (b * 8);
            u8 ownerId = data[pos++];
            m_EntityOwners[eid] = ownerId;
        }
        return true;
    }

    // --- Utility ---

    void Clear() {
        m_PeerPermissions.clear();
        m_EntityOwners.clear();
    }

    usize PeerCount() const { return m_PeerPermissions.size(); }

private:
    std::unordered_map<u8, CollabPermission> m_PeerPermissions;
    std::unordered_map<u64, u8> m_EntityOwners;
};

} // namespace Editor
} // namespace Enjin
