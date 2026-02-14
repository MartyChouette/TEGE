#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/ECS/Entity.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/Math/Matrix.h"
#include "Enjin/Editor/CollaborativeEditing.h"
#include "Enjin/Editor/SceneLock.h"
#include <string>
#include <vector>

namespace Enjin {

// Forward declarations
namespace ECS { class World; }
namespace Scene { class SceneSerializer; }

namespace Editor {

// Peer color assignment for viewport overlays (up to 8 distinct colors)
struct PeerColor {
    f32 r = 1.0f, g = 1.0f, b = 1.0f;
    u32 ToImU32(u8 alpha = 255) const;
};

// Pending conflict that requires user interaction via modal dialog
struct PendingConflictDialog {
    EditOperation localOp;
    EditOperation remoteOp;
    bool open = false;
};

// ==========================================================================
// CollaborativeEditingUI
// ==========================================================================
// Provides the ImGui panel, viewport overlays, and remote operation handling
// for the CollaborativeEditingSystem. Designed to be composed into EditorLayer
// with minimal coupling.
class ENJIN_API CollaborativeEditingUI {
public:
    CollaborativeEditingUI() = default;
    ~CollaborativeEditingUI() = default;

    // Initialize with required subsystem pointers. Must be called before any
    // other method. The SceneSerializer is used for full-scene sync on join.
    void Initialize(CollaborativeEditingSystem* collab, ECS::World* world,
                    SceneLockManager* lockMgr);

    // Draw the collaboration panel (host/join, peer list, conflict queue).
    // Called from EditorLayer::OnImGuiRender when the Collaboration panel bit
    // is set.
    void DrawPanel();

    // Draw peer cursors and camera frustums in the viewport.
    // viewProj is the local editor camera's view-projection matrix.
    void DrawPeerOverlays(const Math::Matrix4& viewProj,
                          f32 viewportWidth, f32 viewportHeight);

    // Apply a remote EditOperation to the local ECS world. This is registered
    // as the CollaborativeEditingSystem::SetOnRemoteEdit callback.
    void HandleRemoteOperation(const EditOperation& op);

    // Host: serialize the full scene for a joining peer.
    // Registered as CollaborativeEditingSystem::SetOnSceneSyncRequest.
    std::string OnSceneSyncRequested();

    // Client: load the full scene received from the host.
    // Registered as CollaborativeEditingSystem::SetOnSceneSyncReceived.
    void OnSceneSyncReceived(const std::string& sceneJson);

    // Show a modal conflict dialog with side-by-side comparison.
    void ShowConflictDialog(const EditOperation& local, const EditOperation& remote);

    // Returns true if there are conflicts awaiting user resolution.
    bool HasPendingConflicts() const;

    // Check whether the local user is allowed to edit a given entity.
    // Returns false if the entity is locked by another peer.
    bool CanEditEntity(ECS::Entity entity) const;

    // Wires up all callbacks on the CollaborativeEditingSystem. Call once
    // after Initialize.
    void WireCallbacks();

private:
    // Helpers
    static PeerColor GetPeerColor(u8 peerId);
    static const char* EditOpTypeName(EditOpType type);

    // World-to-screen projection helper
    bool WorldToScreen(const Math::Matrix4& viewProj, f32 viewportW, f32 viewportH,
                       const Math::Vector3& worldPos, f32& screenX, f32& screenY) const;

    // Apply individual operation types to the ECS world
    void ApplyCreateEntity(const EditOperation& op);
    void ApplyDeleteEntity(const EditOperation& op);
    void ApplyRenameEntity(const EditOperation& op);
    void ApplySetComponent(const EditOperation& op);
    void ApplyRemoveComponent(const EditOperation& op);
    void ApplyModifyTransform(const EditOperation& op);
    void ApplySetParent(const EditOperation& op);
    void ApplyLockEntity(const EditOperation& op);
    void ApplyUnlockEntity(const EditOperation& op);

    // Draw the conflict resolution modal popup
    void DrawConflictModal();

    // State
    CollaborativeEditingSystem* m_Collab = nullptr;
    ECS::World* m_World = nullptr;
    SceneLockManager* m_LockMgr = nullptr;

    // Conflict dialog state
    PendingConflictDialog m_ConflictDialog;

    // Reusable buffers to avoid per-frame allocations
    std::vector<ECS::Entity> m_TempEntities;
};

} // namespace Editor
} // namespace Enjin
