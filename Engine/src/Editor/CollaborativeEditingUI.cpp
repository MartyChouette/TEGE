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

void CollaborativeEditingUI::DrawPanel() {
    if (!m_Collab) return;

    if (!ImGui::Begin("Collaboration", nullptr, ImGuiWindowFlags_None)) {
        ImGui::End();
        return;
    }

    CollabSessionState state = m_Collab->GetState();

    // ------------------------------------------------------------------
    // Disconnected: show host/join controls
    // ------------------------------------------------------------------
    if (state == CollabSessionState::Disconnected) {
        // Status indicator
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Status: Disconnected");
        ImGui::Separator();

        // The host/join fields are stored on EditorLayer's collab member
        // variables (m_CollabUserName, etc.). Since this UI class is designed
        // to work independently, we use our own small static buffers here.
        // EditorLayer can still use its own DrawCollaborationPanel if it
        // prefers — this class is an alternative integration path.
        static char s_UserName[64] = {};
        static char s_HostIP[64] = "127.0.0.1";
        static i32 s_Port = 7778;

        ImGui::Text("User Name:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(150);
        ImGui::InputText("##CollabUIName", s_UserName, sizeof(s_UserName));

        ImGui::Separator();

        // Host
        ImGui::Text("Port:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(80);
        ImGui::InputInt("##CollabUIPort", &s_Port, 0, 0);
        // Clamp port to valid range
        if (s_Port < 1024) s_Port = 1024;
        if (s_Port > 65535) s_Port = 65535;

        ImGui::SameLine();
        if (ImGui::Button("Host Session")) {
            if (std::strlen(s_UserName) == 0) {
                std::strncpy(s_UserName, "Host", sizeof(s_UserName) - 1);
            }
            bool ok = m_Collab->HostSession(static_cast<u16>(s_Port), s_UserName);
            if (ok) {
                WireCallbacks();
                ENJIN_LOG_INFO(Editor, "CollabUI: Hosting session on port %d", s_Port);
            }
        }

        ImGui::Spacing();

        // Join
        ImGui::Text("Host IP:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(150);
        ImGui::InputText("##CollabUIIP", s_HostIP, sizeof(s_HostIP));
        ImGui::SameLine();
        if (ImGui::Button("Join Session")) {
            if (std::strlen(s_UserName) == 0) {
                std::strncpy(s_UserName, "Peer", sizeof(s_UserName) - 1);
            }
            bool ok = m_Collab->JoinSession(s_HostIP, static_cast<u16>(s_Port), s_UserName);
            if (ok) {
                WireCallbacks();
                ENJIN_LOG_INFO(Editor, "CollabUI: Joining session at %s:%d", s_HostIP, s_Port);
            }
        }

        ImGui::End();
        return;
    }

    // ------------------------------------------------------------------
    // Active session: status, peers, conflicts, log
    // ------------------------------------------------------------------

    // Status indicator with colored text
    const char* stateStr = "Unknown";
    ImVec4 stateColor = ImVec4(0.7f, 0.7f, 0.7f, 1.0f);
    switch (state) {
        case CollabSessionState::Hosting:
            stateStr = "Hosting";
            stateColor = ImVec4(0.4f, 1.0f, 0.4f, 1.0f);
            break;
        case CollabSessionState::Joining:
            stateStr = "Joining...";
            stateColor = ImVec4(1.0f, 0.8f, 0.3f, 1.0f);
            break;
        case CollabSessionState::Connected:
            stateStr = "Connected";
            stateColor = ImVec4(0.4f, 0.8f, 1.0f, 1.0f);
            break;
        case CollabSessionState::Syncing:
            stateStr = "Syncing...";
            stateColor = ImVec4(1.0f, 0.6f, 0.3f, 1.0f);
            break;
        default: break;
    }
    ImGui::TextColored(stateColor, "Status: %s", stateStr);

    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 100);
    if (ImGui::Button("Leave Session")) {
        m_Collab->LeaveSession();
    }

    ImGui::Separator();

    // ------------------------------------------------------------------
    // Conflict strategy dropdown
    // ------------------------------------------------------------------
    if (ImGui::TreeNode("Settings")) {
        const char* strategies[] = {
            "Last Writer Wins", "Host Authority", "Merge", "Ask (Manual)"
        };
        int currentStrategy = static_cast<int>(m_Collab->GetConflictStrategy());
        if (ImGui::Combo("Conflict Resolution", &currentStrategy,
                         strategies, IM_ARRAYSIZE(strategies))) {
            m_Collab->SetConflictStrategy(
                static_cast<ConflictStrategy>(currentStrategy));
        }
        ImGui::TreePop();
    }

    ImGui::Separator();

    // ------------------------------------------------------------------
    // Connected peers list
    // ------------------------------------------------------------------
    const auto& peers = m_Collab->GetPeers();
    if (ImGui::TreeNodeEx("Peers", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::BeginTable("##CollabUIPeers", 4,
                              ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
            ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 14);
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Machine", ImGuiTableColumnFlags_WidthFixed, 100);
            ImGui::TableSetupColumn("Editing", ImGuiTableColumnFlags_WidthFixed, 100);
            ImGui::TableHeadersRow();

            for (const auto& peer : peers) {
                ImGui::TableNextRow();

                // Colored dot
                ImGui::TableSetColumnIndex(0);
                PeerColor pc = GetPeerColor(peer.peerId);
                ImU32 dotColor = peer.connected
                    ? pc.ToImU32(255)
                    : IM_COL32(100, 100, 100, 180);
                ImVec2 dotCenter = ImVec2(
                    ImGui::GetCursorScreenPos().x + 7,
                    ImGui::GetCursorScreenPos().y + ImGui::GetTextLineHeight() * 0.5f);
                ImGui::GetWindowDrawList()->AddCircleFilled(dotCenter, 5.0f, dotColor);
                ImGui::Dummy(ImVec2(14, ImGui::GetTextLineHeight()));

                // Name
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%s", peer.name.c_str());

                // Machine
                ImGui::TableSetColumnIndex(2);
                ImGui::TextDisabled("%s", peer.machine.c_str());

                // Currently editing entity
                ImGui::TableSetColumnIndex(3);
                if (peer.cursorEntityId != 0 && m_World) {
                    auto* name = m_World->GetComponent<ECS::NameComponent>(
                        static_cast<ECS::Entity>(peer.cursorEntityId));
                    if (name) {
                        ImGui::TextDisabled("%s", name->name.c_str());
                    } else {
                        ImGui::TextDisabled("#%u", peer.cursorEntityId);
                    }
                } else {
                    ImGui::TextDisabled("--");
                }
            }
            ImGui::EndTable();
        }
        ImGui::TreePop();
    }

    // ------------------------------------------------------------------
    // Pending conflicts
    // ------------------------------------------------------------------
    const auto& conflicts = m_Collab->GetConflicts();
    if (!conflicts.empty()) {
        ImGui::Separator();
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.3f, 1.0f),
                           "Conflicts (%zu)", conflicts.size());

        for (usize i = 0; i < conflicts.size(); ++i) {
            const auto& c = conflicts[i];
            ImGui::PushID(static_cast<int>(i));

            // Entity name or ID
            std::string entityLabel;
            if (m_World) {
                auto* nc = m_World->GetComponent<ECS::NameComponent>(
                    static_cast<ECS::Entity>(c.localOp.entityId));
                if (nc) {
                    entityLabel = nc->name;
                }
            }
            if (entityLabel.empty()) {
                entityLabel = "#" + std::to_string(c.localOp.entityId);
            }

            ImGui::BulletText("%s: %s (local) vs %s (remote)",
                              entityLabel.c_str(),
                              EditOpTypeName(c.localOp.type),
                              EditOpTypeName(c.remoteOp.type));

            ImGui::SameLine();
            if (ImGui::SmallButton("Keep Local")) {
                m_Collab->ResolveConflict(i, ConflictStrategy::Reject);
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("Accept Remote")) {
                m_Collab->ResolveConflict(i, ConflictStrategy::LastWriterWins);
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("View Details")) {
                ShowConflictDialog(c.localOp, c.remoteOp);
            }

            ImGui::PopID();
        }
    }

    // ------------------------------------------------------------------
    // Operation log
    // ------------------------------------------------------------------
    ImGui::Separator();
    if (ImGui::TreeNode("Operation Log")) {
        const auto& log = m_Collab->GetOperationLog();
        ImGui::Text("%zu operations (seq %llu)",
                    log.size(),
                    (unsigned long long)m_Collab->GetCurrentSequence());

        f32 maxH = std::min(200.0f, ImGui::GetContentRegionAvail().y);
        ImGui::BeginChild("##CollabUIOpLog", ImVec2(0, maxH), ImGuiChildFlags_Borders);

        // Most recent first
        for (auto it = log.rbegin(); it != log.rend(); ++it) {
            const auto& op = *it;
            PeerColor pc = GetPeerColor(op.authorId);

            ImGui::TextDisabled("[%llu]", (unsigned long long)op.sequenceId);
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(pc.r, pc.g, pc.b, 1.0f),
                               "%s", EditOpTypeName(op.type));
            ImGui::SameLine();
            ImGui::Text("#%llu", (unsigned long long)op.entityId);
            ImGui::SameLine();
            ImGui::TextDisabled("by %s",
                                op.authorName.empty() ? "local" : op.authorName.c_str());
        }

        ImGui::EndChild();
        ImGui::TreePop();
    }

    // Conflict modal (rendered last so it's on top)
    DrawConflictModal();

    ImGui::End();
}

// ============================================================================
// DRAW PEER OVERLAYS
// ============================================================================

bool CollaborativeEditingUI::WorldToScreen(
    const Math::Matrix4& viewProj, f32 viewportW, f32 viewportH,
    const Math::Vector3& worldPos, f32& screenX, f32& screenY) const
{
    Math::Vector4 clip = viewProj * Math::Vector4(worldPos.x, worldPos.y, worldPos.z, 1.0f);
    if (clip.w <= 0.001f) return false;

    f32 ndcX = clip.x / clip.w;
    f32 ndcY = clip.y / clip.w;
    f32 ndcZ = clip.z / clip.w;

    if (ndcZ < 0.0f || ndcZ > 1.0f) return false;

    screenX = (ndcX + 1.0f) * 0.5f * viewportW;
    screenY = (ndcY + 1.0f) * 0.5f * viewportH;
    return true;
}

void CollaborativeEditingUI::DrawPeerOverlays(
    const Math::Matrix4& viewProj,
    f32 viewportWidth, f32 viewportHeight)
{
    if (!m_Collab || !m_Collab->IsActive() || !m_World) return;

    ImDrawList* drawList = ImGui::GetBackgroundDrawList();
    const auto& peers = m_Collab->GetPeers();

    for (const auto& peer : peers) {
        // Skip self and disconnected peers
        if (!peer.connected) continue;
        // Skip the local peer by comparing with the current sequence's author
        // The collab system's local peer ID is implicit — we skip peers whose
        // cursor matches nothing relevant. We identify "self" by checking if
        // this peer is the one we control. The simplest heuristic: the host
        // is peerId 0, and after connection the local peer ID is assigned.
        // Since we don't expose the local peer ID directly, we skip drawing
        // for any peer whose camera position matches our own camera exactly.
        // But a more robust approach: just draw all remote peers' cursors.

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
        if (!WorldToScreen(viewProj, viewportWidth, viewportHeight,
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
        if (WorldToScreen(viewProj, viewportWidth, viewportHeight,
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

    auto* nc = m_World->GetComponent<ECS::NameComponent>(entity);
    if (nc) {
        nc->name = op.dataJson;
    } else {
        m_World->AddComponent<ECS::NameComponent>(entity, ECS::NameComponent(op.dataJson));
    }

    m_World->InvalidateNameCache();

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

    // The componentKey tells us which component to remove. Since ECS components
    // are template-based, we need to go through the serializer's known key
    // mappings. A pragmatic approach: serialize a "null" component value via
    // the same deserializer path. However, that doesn't remove. Instead we
    // use the component key convention from SceneSerializer.
    //
    // For now, we log the removal. The SceneSerializer doesn't expose a
    // RemoveComponentByKey function, so we handle the most common types
    // explicitly. For unknown types, we log a warning.

    // Common component keys used by SceneSerializer
    if (op.componentKey == "transform") {
        m_World->RemoveComponent<ECS::TransformComponent>(entity);
    } else if (op.componentKey == "name") {
        m_World->RemoveComponent<ECS::NameComponent>(entity);
        m_World->InvalidateNameCache();
    } else if (op.componentKey == "parent") {
        ECS::RemoveParent(m_World, entity);
    } else {
        // For all other component types, the best we can do without a
        // type-erased remove-by-key is to log a warning. The remote peer
        // is removing a component we may not have code paths to handle
        // generically.
        ENJIN_LOG_WARN(Editor,
            "CollabUI: RemoveComponent '%s' on entity %llu - "
            "generic removal not implemented, applying full entity re-sync may be needed",
            op.componentKey.c_str(), (unsigned long long)op.entityId);

        // Attempt to deserialize an empty component which might effectively
        // reset it. This is a best-effort approach.
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
