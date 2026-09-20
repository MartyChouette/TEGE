#include "Enjin/Editor/CollaborativeEditingUI.h"
#include "Enjin/Editor/CollabUndoCommands.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Name.h"
#include "Enjin/ECS/Components/Hierarchy.h"
#include "Enjin/Scene/SceneSerializer.h"
#include "Enjin/Logging/Log.h"
#include "Enjin/Math/Quaternion.h"

#include <imgui.h>
#include <cstring>
#include <algorithm>
#include <cmath>
#include <string>

namespace Enjin {
namespace Editor {

// ============================================================================
// PEER COLOR
// ============================================================================

u32 PeerColor::ToImU32(u8 alpha) const {
    return IM_COL32(
        static_cast<u8>(r * 255.0f),
        static_cast<u8>(g * 255.0f),
        static_cast<u8>(b * 255.0f),
        alpha);
}

PeerColor CollaborativeEditingUI::GetPeerColor(u8 peerId) {
    // 8 distinct, visually separable colors for peer identification
    static const PeerColor s_Colors[] = {
        { 0.3f, 0.7f, 1.0f },  // Blue
        { 1.0f, 0.4f, 0.4f },  // Red
        { 0.4f, 1.0f, 0.4f },  // Green
        { 1.0f, 0.8f, 0.2f },  // Yellow
        { 0.8f, 0.4f, 1.0f },  // Purple
        { 1.0f, 0.6f, 0.2f },  // Orange
        { 0.2f, 1.0f, 0.8f },  // Cyan
        { 1.0f, 0.4f, 0.8f },  // Pink
    };
    return s_Colors[peerId % 8];
}

const char* CollaborativeEditingUI::EditOpTypeName(EditOpType type) {
    switch (type) {
        case EditOpType::CreateEntity:    return "Create";
        case EditOpType::DeleteEntity:    return "Delete";
        case EditOpType::RenameEntity:    return "Rename";
        case EditOpType::SetComponent:    return "SetComp";
        case EditOpType::RemoveComponent: return "RmComp";
        case EditOpType::ModifyTransform: return "Transform";
        case EditOpType::SetParent:       return "Parent";
        case EditOpType::LockEntity:      return "Lock";
        case EditOpType::UnlockEntity:    return "Unlock";
    }
    return "?";
}

// ============================================================================
// INITIALIZE
// ============================================================================

void CollaborativeEditingUI::Initialize(CollaborativeEditingSystem* collab,
                                         ECS::World* world,
                                         SceneLockManager* lockMgr,
                                         UndoRedoManager* undoRedo) {
    m_Collab = collab;
    m_World = world;
    m_LockMgr = lockMgr;
    m_UndoRedo = undoRedo;

    ENJIN_LOG_INFO(Editor, "CollaborativeEditingUI initialized");
}

void CollaborativeEditingUI::WireCallbacks() {
    if (!m_Collab) return;

    m_Collab->SetOnRemoteEdit([this](const EditOperation& op) {
        HandleRemoteOperation(op);
    });

    m_Collab->SetOnSceneSyncRequest([this]() -> std::string {
        return OnSceneSyncRequested();
    });

    m_Collab->SetOnSceneSyncReceived([this](const std::string& json) {
        OnSceneSyncReceived(json);
    });

    ENJIN_LOG_INFO(Editor, "CollaborativeEditingUI callbacks wired");
}

// ============================================================================
// DRAW PANEL
// ============================================================================

// DrawPanel lived here and is gone.
//
// There were two collaboration panels. This one used static buffers, ignored
// the editor's panel-visibility state so its close button did nothing, and
// sized its widgets without reading FontGlobalScale. EditorLayer's panel does
// all three properly AND has the conflict-strategy selector, the peers table,
// the resolve buttons and the operation log that this one never grew. Its own
// comment called itself "an alternative integration path", which is how a
// second implementation describes itself right before it starts rotting.
//
// EditorLayer::DrawCollaborationPanel is the panel. This class keeps what it is
// genuinely better at: applying remote operations, the scene sync, the peer
// overlays, and the conflict modal -- which EditorLayer now calls.

bool CollaborativeEditingUI::WorldToScreen(
    const Math::Matrix4& viewProj, f32 originX, f32 originY,
    f32 viewportW, f32 viewportH,
    const Math::Vector3& worldPos, f32& screenX, f32& screenY) const
{
    Math::Vector4 clip = viewProj * Math::Vector4(worldPos.x, worldPos.y, worldPos.z, 1.0f);
    if (clip.w <= 0.001f) return false;

    f32 ndcX = clip.x / clip.w;
    f32 ndcY = clip.y / clip.w;
    f32 ndcZ = clip.z / clip.w;

    if (ndcZ < 0.0f || ndcZ > 1.0f) return false;

    // Same mapping the viewport's own bone picking uses, origin included --
    // that one is proven correct because it hit-tests against the real mouse
    // position, so any disagreement with it would show up immediately.
    screenX = (ndcX + 1.0f) * 0.5f * viewportW + originX;
    screenY = (ndcY + 1.0f) * 0.5f * viewportH + originY;
    return true;
}

void CollaborativeEditingUI::DrawPeerOverlays(
    const Math::Matrix4& viewProj,
    f32 originX, f32 originY,
    f32 viewportWidth, f32 viewportHeight)
{
    if (!m_Collab || !m_Collab->IsActive() || !m_World) return;

    ImDrawList* drawList = ImGui::GetBackgroundDrawList();
    const auto& peers = m_Collab->GetPeers();

    const u8 localPeer = m_Collab->GetLocalPeerId();

    for (const auto& peer : peers) {
        if (!peer.connected) continue;
        // Leave ourselves out. The comment that used to sit here reasoned its
        // way around not having the local peer id -- compare camera positions,
        // or just draw everyone -- and settled on drawing everyone, so your own
        // selection got a second ring and a label with your name on it.
        // CollaborativeEditingSystem knows which peer it is; it just never said.
        if (peer.peerId == localPeer) continue;

        // If this peer has a selected entity, draw an overlay
        if (peer.cursorEntityId == 0) continue;

        ECS::Entity entity = static_cast<ECS::Entity>(peer.cursorEntityId);
        auto* transform = m_World->GetComponent<ECS::TransformComponent>(entity);
        if (!transform) continue;

        PeerColor color = GetPeerColor(peer.peerId);
        ImU32 ringColor = color.ToImU32(200);
        ImU32 labelBg = color.ToImU32(140);

        // Project entity position to screen
        f32 sx = 0.0f, sy = 0.0f;
        if (!WorldToScreen(viewProj, originX, originY, viewportWidth, viewportHeight,
                           transform->position, sx, sy)) {
            continue;
        }

        // Draw selection ring around the entity
        f32 ringRadius = 24.0f;
        drawList->AddCircle(ImVec2(sx, sy), ringRadius, ringColor, 24, 2.5f);

        // Draw peer name label above the entity
        const char* peerName = peer.name.c_str();
        ImVec2 textSize = ImGui::CalcTextSize(peerName);
        f32 labelX = sx - textSize.x * 0.5f - 4.0f;
        f32 labelY = sy - ringRadius - textSize.y - 8.0f;

        drawList->AddRectFilled(
            ImVec2(labelX, labelY),
            ImVec2(labelX + textSize.x + 8.0f, labelY + textSize.y + 4.0f),
            labelBg, 3.0f);
        drawList->AddText(
            ImVec2(labelX + 4.0f, labelY + 2.0f),
            IM_COL32(255, 255, 255, 240),
            peerName);

        // Draw camera position indicator (small diamond at peer's camera pos)
        f32 camSx = 0.0f, camSy = 0.0f;
        if (WorldToScreen(viewProj, originX, originY, viewportWidth, viewportHeight,
                          peer.cameraPos, camSx, camSy)) {
            f32 d = 6.0f;
            ImVec2 pts[4] = {
                { camSx,     camSy - d },
                { camSx + d, camSy     },
                { camSx,     camSy + d },
                { camSx - d, camSy     }
            };
            drawList->AddConvexPolyFilled(pts, 4, color.ToImU32(120));
            drawList->AddPolyline(pts, 4, ringColor, ImDrawFlags_Closed, 1.5f);

            // Draw a thin line from the camera diamond to the selected entity ring
            drawList->AddLine(ImVec2(camSx, camSy), ImVec2(sx, sy),
                              color.ToImU32(60), 1.0f);
        }
    }
}

// ============================================================================
// HANDLE REMOTE OPERATION
// ============================================================================

void CollaborativeEditingUI::HandleRemoteOperation(const EditOperation& op) {
    if (!m_World) {
        ENJIN_LOG_WARN(Editor, "CollabUI: No world set, ignoring remote op");
        return;
    }

    // Apply to ECS world
    switch (op.type) {
        case EditOpType::CreateEntity:    ApplyCreateEntity(op);    break;
        case EditOpType::DeleteEntity:    ApplyDeleteEntity(op);    break;
        case EditOpType::RenameEntity:    ApplyRenameEntity(op);    break;
        case EditOpType::SetComponent:    ApplySetComponent(op);    break;
        case EditOpType::RemoveComponent: ApplyRemoveComponent(op); break;
        case EditOpType::ModifyTransform: ApplyModifyTransform(op); break;
        case EditOpType::SetParent:       ApplySetParent(op);       break;
        case EditOpType::LockEntity:      ApplyLockEntity(op);      break;
        case EditOpType::UnlockEntity:    ApplyUnlockEntity(op);    break;
    }

    // Push onto undo stack so the local user can undo remote edits.
    // Skip lock/unlock ops — those aren't undoable actions.
    if (m_UndoRedo && op.type != EditOpType::LockEntity && op.type != EditOpType::UnlockEntity) {
        std::string desc = std::string("Remote: ") + op.authorName + " " +
            EditOpTypeName(op.type);
        auto cmd = std::make_unique<RemoteEditCommand>(m_Collab, op, desc);
        m_UndoRedo->Execute(std::move(cmd));
    }
}

// ============================================================================
// APPLY OPERATIONS
// ============================================================================

void CollaborativeEditingUI::ApplyCreateEntity(const EditOperation& op) {
    if (op.dataJson.empty()) {
        ENJIN_LOG_WARN(Editor, "CollabUI: CreateEntity op has empty dataJson");
        return;
    }

    ECS::Entity newEntity = Scene::SceneSerializer::DeserializeEntityFromString(
        m_World, op.dataJson);
    if (newEntity == ECS::INVALID_ENTITY) {
        ENJIN_LOG_ERROR(Editor, "CollabUI: Failed to create entity from remote op");
        return;
    }

    ENJIN_LOG_INFO(Editor, "CollabUI: Created entity %llu from remote peer '%s'",
                   (unsigned long long)newEntity, op.authorName.c_str());
}

void CollaborativeEditingUI::ApplyDeleteEntity(const EditOperation& op) {
    ECS::Entity entity = static_cast<ECS::Entity>(op.entityId);

    if (!m_World->IsValid(entity)) {
        ENJIN_LOG_WARN(Editor, "CollabUI: DeleteEntity op references invalid entity %llu",
                       (unsigned long long)op.entityId);
        return;
    }

    // Remove children first to avoid dangling parent references
    auto* cc = m_World->GetComponent<ECS::ChildrenComponent>(entity);
    if (cc) {
        // Copy children list since DestroyEntity will modify it
        std::vector<ECS::Entity> children = cc->children;
        for (ECS::Entity child : children) {
            if (m_World->IsValid(child)) {
                m_World->DestroyEntity(child);
            }
        }
    }

    m_World->DestroyEntity(entity);
    ENJIN_LOG_INFO(Editor, "CollabUI: Deleted entity %llu from remote peer '%s'",
                   (unsigned long long)op.entityId, op.authorName.c_str());
}

void CollaborativeEditingUI::ApplyRenameEntity(const EditOperation& op) {
    ECS::Entity entity = static_cast<ECS::Entity>(op.entityId);

    if (!m_World->IsValid(entity)) {
        ENJIN_LOG_WARN(Editor, "CollabUI: RenameEntity op references invalid entity %llu",
                       (unsigned long long)op.entityId);
        return;
    }

    m_World->SetEntityName(entity, op.dataJson);


    ENJIN_LOG_INFO(Editor, "CollabUI: Renamed entity %llu to '%s' from remote peer '%s'",
                   (unsigned long long)op.entityId, op.dataJson.c_str(),
                   op.authorName.c_str());
}

void CollaborativeEditingUI::ApplySetComponent(const EditOperation& op) {
    ECS::Entity entity = static_cast<ECS::Entity>(op.entityId);

    if (!m_World->IsValid(entity)) {
        ENJIN_LOG_WARN(Editor, "CollabUI: SetComponent op references invalid entity %llu",
                       (unsigned long long)op.entityId);
        return;
    }

    if (op.componentKey.empty() || op.dataJson.empty()) {
        ENJIN_LOG_WARN(Editor, "CollabUI: SetComponent op has empty key or data");
        return;
    }

    bool ok = Scene::SceneSerializer::DeserializeOneComponent(
        m_World, entity, op.componentKey, op.dataJson);

    if (!ok) {
        ENJIN_LOG_ERROR(Editor, "CollabUI: Failed to apply SetComponent '%s' on entity %llu",
                        op.componentKey.c_str(), (unsigned long long)op.entityId);
    } else {
        ENJIN_LOG_INFO(Editor, "CollabUI: Set component '%s' on entity %llu from peer '%s'",
                       op.componentKey.c_str(), (unsigned long long)op.entityId,
                       op.authorName.c_str());
    }
}

void CollaborativeEditingUI::ApplyRemoveComponent(const EditOperation& op) {
    ECS::Entity entity = static_cast<ECS::Entity>(op.entityId);

    if (!m_World->IsValid(entity)) {
        ENJIN_LOG_WARN(Editor, "CollabUI: RemoveComponent op references invalid entity %llu",
                       (unsigned long long)op.entityId);
        return;
    }

    // Removal by scene-JSON key. Three keys are handled directly because they are
    // not plain serializer entries -- "parent" is a hierarchy edge, not a component
    // -- and everything else goes through the registry.
    //
    // The comment that used to sit here described the state before that fallback
    // existed: "for now, we log the removal... for unknown types, we log a
    // warning". It stayed after the fallback was written, so the code and its own
    // description disagreed about whether the feature worked.
    if (op.componentKey == "transform") {
        m_World->RemoveComponent<ECS::TransformComponent>(entity);
    } else if (op.componentKey == "name") {
        m_World->RemoveComponent<ECS::NameComponent>(entity);
    } else if (op.componentKey == "parent") {
        ECS::RemoveParent(m_World, entity);
    } else {
        // Generic type-erased removal by scene-JSON key — handles all ~140 component types the
        // serializer knows (not just transform/name/parent). Previously this only warned, so a
        // peer removing any other component silently desynced.
        if (!Scene::SceneSerializer::RemoveOneComponent(m_World, entity, op.componentKey)) {
            ENJIN_LOG_WARN(Editor,
                "CollabUI: RemoveComponent '%s' on entity %llu - unknown key, ignored",
                op.componentKey.c_str(), (unsigned long long)op.entityId);
        }
    }

    ENJIN_LOG_INFO(Editor, "CollabUI: Removed component '%s' from entity %llu by peer '%s'",
                   op.componentKey.c_str(), (unsigned long long)op.entityId,
                   op.authorName.c_str());
}

void CollaborativeEditingUI::ApplyModifyTransform(const EditOperation& op) {
    ECS::Entity entity = static_cast<ECS::Entity>(op.entityId);

    if (!m_World->IsValid(entity)) {
        ENJIN_LOG_WARN(Editor, "CollabUI: ModifyTransform op references invalid entity %llu",
                       (unsigned long long)op.entityId);
        return;
    }

    auto* transform = m_World->GetComponent<ECS::TransformComponent>(entity);
    if (!transform) {
        // Entity exists but has no transform — add one
        m_World->AddComponent<ECS::TransformComponent>(entity);
        transform = m_World->GetComponent<ECS::TransformComponent>(entity);
        if (!transform) return;
    }

    transform->position = op.position;
    transform->rotation = Math::Quaternion::FromEuler(op.rotation);
    transform->scale = op.scale;
}

void CollaborativeEditingUI::ApplySetParent(const EditOperation& op) {
    ECS::Entity entity = static_cast<ECS::Entity>(op.entityId);

    if (!m_World->IsValid(entity)) {
        ENJIN_LOG_WARN(Editor, "CollabUI: SetParent op references invalid entity %llu",
                       (unsigned long long)op.entityId);
        return;
    }

    // Parse the parent entity ID from the data JSON
    ECS::Entity newParent = ECS::INVALID_ENTITY;
    if (!op.dataJson.empty()) {
        try {
            u64 parentId = std::stoull(op.dataJson);
            newParent = static_cast<ECS::Entity>(parentId);
        } catch (const std::exception&) {
            ENJIN_LOG_WARN(Editor, "CollabUI: SetParent has invalid parent ID: %s",
                           op.dataJson.c_str());
        }
    }

    // Use the hierarchy helper function from Hierarchy.h
    ECS::SetParent(m_World, entity, newParent);

    ENJIN_LOG_INFO(Editor, "CollabUI: Reparented entity %llu to %llu by peer '%s'",
                   (unsigned long long)op.entityId,
                   (unsigned long long)newParent,
                   op.authorName.c_str());
}

void CollaborativeEditingUI::ApplyLockEntity(const EditOperation& op) {
    if (!m_LockMgr) return;

    ECS::Entity entity = static_cast<ECS::Entity>(op.entityId);

    // When a remote peer locks an entity, we record it in the lock manager
    // so CanEditEntity returns false for this entity
    m_LockMgr->Refresh();

    ENJIN_LOG_INFO(Editor, "CollabUI: Entity %llu locked by peer '%s'",
                   (unsigned long long)op.entityId, op.authorName.c_str());
}

void CollaborativeEditingUI::ApplyUnlockEntity(const EditOperation& op) {
    if (!m_LockMgr) return;

    ECS::Entity entity = static_cast<ECS::Entity>(op.entityId);

    // Refresh lock state from disk (the remote peer should have updated
    // the .enjinlock file)
    m_LockMgr->Refresh();

    ENJIN_LOG_INFO(Editor, "CollabUI: Entity %llu unlocked by peer '%s'",
                   (unsigned long long)op.entityId, op.authorName.c_str());
}

// ============================================================================
// SCENE SYNC
// ============================================================================

std::string CollaborativeEditingUI::OnSceneSyncRequested() {
    if (!m_World) return "";

    Scene::SceneSerializer serializer(m_World);
    Scene::SerializationOptions opts;
    opts.prettyPrint = false;  // Compact for network transfer
    opts.includeVertexData = true;
    opts.deterministic = true;

    std::string json = serializer.SaveToString(opts);

    ENJIN_LOG_INFO(Editor, "CollabUI: Scene sync requested, serialized %zu bytes",
                   json.size());
    return json;
}

void CollaborativeEditingUI::OnSceneSyncReceived(const std::string& sceneJson) {
    if (!m_World || sceneJson.empty()) return;

    // Everything holding an entity handle has to let go first: the load below
    // clears the world, and a handle kept across that names a recycled slot.
    if (m_OnBeforeSceneReplaced) m_OnBeforeSceneReplaced();

    Scene::SceneSerializer serializer(m_World);
    auto result = serializer.LoadFromString(sceneJson, true); // clear existing

    if (result.success) {
        ENJIN_LOG_INFO(Editor, "CollabUI: Scene sync received and loaded (%zu entities)",
                       result.entities.size());
    } else {
        ENJIN_LOG_ERROR(Editor, "CollabUI: Scene sync load failed: %s",
                        result.error.c_str());
    }
}

// ============================================================================
// CONFLICT DIALOG
// ============================================================================

void CollaborativeEditingUI::ShowConflictDialog(
    const EditOperation& local, const EditOperation& remote)
{
    m_ConflictDialog.localOp = local;
    m_ConflictDialog.remoteOp = remote;
    m_ConflictDialog.open = true;
    ImGui::OpenPopup("Conflict Resolution");
}

bool CollaborativeEditingUI::HasPendingConflicts() const {
    if (!m_Collab) return false;
    return !m_Collab->GetConflicts().empty() || m_ConflictDialog.open;
}

void CollaborativeEditingUI::DrawConflictModal() {
    if (!m_ConflictDialog.open) return;

    ImGui::SetNextWindowSize(ImVec2(600, 400), ImGuiCond_FirstUseEver);
    if (ImGui::BeginPopupModal("Conflict Resolution", &m_ConflictDialog.open,
                                ImGuiWindowFlags_NoResize)) {
        const auto& local = m_ConflictDialog.localOp;
        const auto& remote = m_ConflictDialog.remoteOp;

        ImGui::Text("Concurrent edits detected on entity #%llu",
                    (unsigned long long)local.entityId);

        // Entity name if available
        if (m_World) {
            auto* nc = m_World->GetComponent<ECS::NameComponent>(
                static_cast<ECS::Entity>(local.entityId));
            if (nc) {
                ImGui::SameLine();
                ImGui::TextDisabled("(%s)", nc->name.c_str());
            }
        }

        ImGui::Separator();

        // Side-by-side comparison
        f32 halfWidth = ImGui::GetContentRegionAvail().x * 0.5f - 8.0f;

        // -- Left column: Local version --
        ImGui::BeginChild("##LocalSide", ImVec2(halfWidth, 260), ImGuiChildFlags_Borders);
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Local Version");
        ImGui::Separator();
        ImGui::Text("Author: %s", local.authorName.empty() ? "You" : local.authorName.c_str());
        ImGui::Text("Type: %s", EditOpTypeName(local.type));
        ImGui::Text("Lamport Clock: %llu", (unsigned long long)local.lamportClock);

        if (local.type == EditOpType::ModifyTransform) {
            ImGui::Spacing();
            ImGui::Text("Position: (%.2f, %.2f, %.2f)",
                        local.position.x, local.position.y, local.position.z);
            ImGui::Text("Rotation: (%.2f, %.2f, %.2f)",
                        local.rotation.x, local.rotation.y, local.rotation.z);
            ImGui::Text("Scale: (%.2f, %.2f, %.2f)",
                        local.scale.x, local.scale.y, local.scale.z);
        } else if (!local.dataJson.empty()) {
            ImGui::Spacing();
            ImGui::TextWrapped("Data: %s",
                local.dataJson.size() > 512
                    ? (local.dataJson.substr(0, 512) + "...").c_str()
                    : local.dataJson.c_str());
        }
        if (!local.componentKey.empty()) {
            ImGui::Text("Component: %s", local.componentKey.c_str());
        }
        ImGui::EndChild();

        ImGui::SameLine();

        // -- Right column: Remote version --
        ImGui::BeginChild("##RemoteSide", ImVec2(halfWidth, 260), ImGuiChildFlags_Borders);
        ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.3f, 1.0f), "Remote Version");
        ImGui::Separator();
        ImGui::Text("Author: %s", remote.authorName.empty() ? "Unknown" : remote.authorName.c_str());
        ImGui::Text("Type: %s", EditOpTypeName(remote.type));
        ImGui::Text("Lamport Clock: %llu", (unsigned long long)remote.lamportClock);

        if (remote.type == EditOpType::ModifyTransform) {
            ImGui::Spacing();
            ImGui::Text("Position: (%.2f, %.2f, %.2f)",
                        remote.position.x, remote.position.y, remote.position.z);
            ImGui::Text("Rotation: (%.2f, %.2f, %.2f)",
                        remote.rotation.x, remote.rotation.y, remote.rotation.z);
            ImGui::Text("Scale: (%.2f, %.2f, %.2f)",
                        remote.scale.x, remote.scale.y, remote.scale.z);
        } else if (!remote.dataJson.empty()) {
            ImGui::Spacing();
            ImGui::TextWrapped("Data: %s",
                remote.dataJson.size() > 512
                    ? (remote.dataJson.substr(0, 512) + "...").c_str()
                    : remote.dataJson.c_str());
        }
        if (!remote.componentKey.empty()) {
            ImGui::Text("Component: %s", remote.componentKey.c_str());
        }
        ImGui::EndChild();

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Resolution buttons
        f32 buttonWidth = 140.0f;
        f32 totalWidth = buttonWidth * 3.0f + ImGui::GetStyle().ItemSpacing.x * 2.0f;
        ImGui::SetCursorPosX(
            (ImGui::GetContentRegionAvail().x - totalWidth) * 0.5f +
            ImGui::GetCursorPosX());

        if (ImGui::Button("Accept Local", ImVec2(buttonWidth, 0))) {
            // Keep local state, reject remote
            // Find this conflict in the collab system's list and resolve it
            const auto& conflicts = m_Collab->GetConflicts();
            for (usize i = 0; i < conflicts.size(); ++i) {
                if (conflicts[i].localOp.sequenceId == local.sequenceId &&
                    conflicts[i].remoteOp.sequenceId == remote.sequenceId) {
                    m_Collab->ResolveConflict(i, ConflictStrategy::Reject);
                    break;
                }
            }
            m_ConflictDialog.open = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();
        if (ImGui::Button("Accept Remote", ImVec2(buttonWidth, 0))) {
            const auto& conflicts = m_Collab->GetConflicts();
            for (usize i = 0; i < conflicts.size(); ++i) {
                if (conflicts[i].localOp.sequenceId == local.sequenceId &&
                    conflicts[i].remoteOp.sequenceId == remote.sequenceId) {
                    m_Collab->ResolveConflict(i, ConflictStrategy::LastWriterWins);
                    break;
                }
            }
            m_ConflictDialog.open = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();
        if (ImGui::Button("Merge", ImVec2(buttonWidth, 0))) {
            const auto& conflicts = m_Collab->GetConflicts();
            for (usize i = 0; i < conflicts.size(); ++i) {
                if (conflicts[i].localOp.sequenceId == local.sequenceId &&
                    conflicts[i].remoteOp.sequenceId == remote.sequenceId) {
                    m_Collab->ResolveConflict(i, ConflictStrategy::Merge);
                    break;
                }
            }
            m_ConflictDialog.open = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

// ============================================================================
// LOCK ENFORCEMENT
// ============================================================================

bool CollaborativeEditingUI::CanEditEntity(ECS::Entity entity) const {
    // If no collab system or not active, allow all edits
    if (!m_Collab || !m_Collab->IsActive()) return true;

    // Permission check — Viewers cannot edit anything
    const auto& perms = m_Collab->GetPermissions();
    u8 localPeer = m_Collab->GetPeers().empty() ? 0 : m_Collab->GetPeers()[0].peerId;
    // Find our own peer ID
    for (const auto& p : m_Collab->GetPeers()) {
        if (p.name == "local" || p.peerId == 0) { // Host is always 0
            localPeer = p.peerId;
            break;
        }
    }
    if (!perms.CanEditEntity(localPeer, static_cast<u64>(entity))) {
        return false;
    }

    // Check lock manager — entity locked by another user on disk
    if (m_LockMgr && m_LockMgr->IsEntityLockedByOther(entity)) {
        return false;
    }

    return true;
}

} // namespace Editor
} // namespace Enjin
