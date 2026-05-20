#include "Enjin/Editor/CollabMergeUI.h"
#include "Enjin/Editor/CollaborativeEditing.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Name.h"
#include "Enjin/Scene/SceneSerializer.h"
#include "Enjin/Logging/Log.h"

#include <imgui.h>
#include <unordered_map>
#include <unordered_set>

namespace Enjin {
namespace Editor {

void CollabMergeUI::Initialize(ECS::World* world, CollaborativeEditingSystem* collab) {
    m_World = world;
    m_Collab = collab;
}

void CollabMergeUI::ComputeDiff(const std::vector<EditOperation>& localOps,
                                 const std::vector<EditOperation>& remoteOps) {
    m_Entries.clear();
    m_HasPending = false;

    // Collect entity IDs touched by each side
    std::unordered_set<u64> localEntities;
    std::unordered_set<u64> remoteEntities;
    std::unordered_set<u64> localCreated, localDeleted;
    std::unordered_set<u64> remoteCreated, remoteDeleted;

    for (const auto& op : localOps) {
        localEntities.insert(op.entityId);
        if (op.type == EditOpType::CreateEntity) localCreated.insert(op.entityId);
        if (op.type == EditOpType::DeleteEntity) localDeleted.insert(op.entityId);
    }
    for (const auto& op : remoteOps) {
        remoteEntities.insert(op.entityId);
        if (op.type == EditOpType::CreateEntity) remoteCreated.insert(op.entityId);
        if (op.type == EditOpType::DeleteEntity) remoteDeleted.insert(op.entityId);
    }

    // Merge all touched entity IDs
    std::unordered_set<u64> allEntities = localEntities;
    allEntities.insert(remoteEntities.begin(), remoteEntities.end());

    for (u64 eid : allEntities) {
        MergeDiffEntry entry;
        entry.entityId = eid;

        bool touchedLocal = localEntities.count(eid) > 0;
        bool touchedRemote = remoteEntities.count(eid) > 0;
        bool createdLocal = localCreated.count(eid) > 0;
        bool createdRemote = remoteCreated.count(eid) > 0;
        bool deletedLocal = localDeleted.count(eid) > 0;
        bool deletedRemote = remoteDeleted.count(eid) > 0;

        // Determine status
        if (createdLocal && !touchedRemote) {
            entry.status = MergeDiffEntry::Status::AddedLocal;
            entry.resolution = MergeDiffEntry::Resolution::KeepLocal;  // Auto-keep
        } else if (createdRemote && !touchedLocal) {
            entry.status = MergeDiffEntry::Status::AddedRemote;
            entry.resolution = MergeDiffEntry::Resolution::KeepRemote;  // Auto-keep
        } else if (deletedLocal && !touchedRemote) {
            entry.status = MergeDiffEntry::Status::DeletedLocal;
            entry.resolution = MergeDiffEntry::Resolution::KeepLocal;  // Auto-keep
        } else if (deletedRemote && !touchedLocal) {
            entry.status = MergeDiffEntry::Status::DeletedRemote;
            entry.resolution = MergeDiffEntry::Resolution::KeepRemote;  // Auto-keep
        } else if ((deletedLocal && createdRemote) || (deletedRemote && createdLocal)) {
            entry.status = MergeDiffEntry::Status::Conflicted;
            // User must decide
        } else if (touchedLocal && touchedRemote) {
            // Both sides modified — CRDT auto-merge handles this
            entry.status = MergeDiffEntry::Status::Modified;
            entry.resolution = MergeDiffEntry::Resolution::AutoMerged;
        } else if (touchedLocal || touchedRemote) {
            // Only one side changed — no conflict
            entry.status = MergeDiffEntry::Status::Modified;
            entry.resolution = touchedLocal
                ? MergeDiffEntry::Resolution::KeepLocal
                : MergeDiffEntry::Resolution::KeepRemote;
        }

        // Get entity name from the world if it still exists
        ECS::Entity entity = static_cast<ECS::Entity>(eid);
        if (m_World && m_World->IsValid(entity)) {
            auto* nc = m_World->GetComponent<ECS::NameComponent>(entity);
            if (nc) entry.entityName = nc->name;
        }
        if (entry.entityName.empty()) {
            entry.entityName = "Entity " + std::to_string(eid);
        }

        // Only add entries that actually changed
        if (entry.status != MergeDiffEntry::Status::Unchanged) {
            m_Entries.push_back(std::move(entry));
        }
    }

    // Check if any need manual resolution
    for (const auto& e : m_Entries) {
        if (e.resolution == MergeDiffEntry::Resolution::Undecided) {
            m_HasPending = true;
            break;
        }
    }

    ENJIN_LOG_INFO(Editor, "CollabMerge: %zu entities diverged (%s pending manual resolution)",
        m_Entries.size(), m_HasPending ? "some" : "none");
}

bool CollabMergeUI::DrawMergeDialog() {
    if (m_Entries.empty()) return false;

    bool applyClicked = false;

    ImGui::SetNextWindowSize(ImVec2(700, 500), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Reconnect Merge", nullptr, ImGuiWindowFlags_NoCollapse)) {
        ImGui::TextWrapped("Your project diverged while offline. Review the changes below and resolve any conflicts.");
        ImGui::Separator();

        // Summary counts
        int autoCount = 0, pendingCount = 0;
        for (const auto& e : m_Entries) {
            if (e.resolution == MergeDiffEntry::Resolution::Undecided) ++pendingCount;
            else ++autoCount;
        }
        ImGui::Text("Auto-resolved: %d  |  Needs review: %d", autoCount, pendingCount);
        ImGui::Separator();

        // Entity list
        if (ImGui::BeginTable("MergeTable", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY)) {
            ImGui::TableSetupColumn("Entity", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, 120);
            ImGui::TableSetupColumn("Resolution", ImGuiTableColumnFlags_WidthFixed, 140);
            ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthFixed, 140);
            ImGui::TableHeadersRow();

            for (usize i = 0; i < m_Entries.size(); ++i) {
                auto& entry = m_Entries[i];
                ImGui::TableNextRow();

                // Entity name
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%s", entry.entityName.c_str());

                // Status with color
                ImGui::TableSetColumnIndex(1);
                ImVec4 statusColor;
                const char* statusText;
                switch (entry.status) {
                    case MergeDiffEntry::Status::AddedLocal:    statusText = "Added (local)";   statusColor = ImVec4(0.3f, 0.8f, 0.3f, 1); break;
                    case MergeDiffEntry::Status::AddedRemote:   statusText = "Added (remote)";  statusColor = ImVec4(0.3f, 0.6f, 0.9f, 1); break;
                    case MergeDiffEntry::Status::DeletedLocal:  statusText = "Deleted (local)"; statusColor = ImVec4(0.9f, 0.5f, 0.3f, 1); break;
                    case MergeDiffEntry::Status::DeletedRemote: statusText = "Deleted (remote)";statusColor = ImVec4(0.9f, 0.5f, 0.3f, 1); break;
                    case MergeDiffEntry::Status::Modified:      statusText = "Modified";        statusColor = ImVec4(0.9f, 0.8f, 0.3f, 1); break;
                    case MergeDiffEntry::Status::Conflicted:    statusText = "CONFLICT";        statusColor = ImVec4(0.9f, 0.2f, 0.2f, 1); break;
                    default:                                     statusText = "Unchanged";       statusColor = ImVec4(0.5f, 0.5f, 0.5f, 1); break;
                }
                ImGui::TextColored(statusColor, "%s", statusText);

                // Resolution
                ImGui::TableSetColumnIndex(2);
                switch (entry.resolution) {
                    case MergeDiffEntry::Resolution::KeepLocal:  ImGui::Text("Keep Local"); break;
                    case MergeDiffEntry::Resolution::KeepRemote: ImGui::Text("Keep Remote"); break;
                    case MergeDiffEntry::Resolution::AutoMerged: ImGui::TextColored(ImVec4(0.3f, 0.8f, 0.3f, 1), "Auto-merged"); break;
                    default: ImGui::TextColored(ImVec4(0.9f, 0.2f, 0.2f, 1), "Undecided"); break;
                }

                // Action buttons for undecided entries
                ImGui::TableSetColumnIndex(3);
                if (entry.resolution == MergeDiffEntry::Resolution::Undecided ||
                    entry.status == MergeDiffEntry::Status::Conflicted) {
                    ImGui::PushID(static_cast<int>(i));
                    if (ImGui::SmallButton("Local")) {
                        entry.resolution = MergeDiffEntry::Resolution::KeepLocal;
                    }
                    ImGui::SameLine();
                    if (ImGui::SmallButton("Remote")) {
                        entry.resolution = MergeDiffEntry::Resolution::KeepRemote;
                    }
                    ImGui::PopID();
                }
            }
            ImGui::EndTable();
        }

        ImGui::Separator();

        // Check if all resolved
        bool allResolved = true;
        for (const auto& e : m_Entries) {
            if (e.resolution == MergeDiffEntry::Resolution::Undecided) {
                allResolved = false;
                break;
            }
        }

        if (!allResolved) {
            ImGui::BeginDisabled();
        }
        if (ImGui::Button("Apply Resolutions", ImVec2(200, 30))) {
            applyClicked = true;
        }
        if (!allResolved) {
            ImGui::EndDisabled();
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.9f, 0.5f, 0.2f, 1), "Resolve all conflicts first");
        }
    }
    ImGui::End();

    if (applyClicked) {
        ApplyResolutions();
    }
    return applyClicked;
}

void CollabMergeUI::ApplyResolutions() {
    if (!m_World) return;

    for (const auto& entry : m_Entries) {
        if (entry.resolution == MergeDiffEntry::Resolution::AutoMerged) continue;

        ECS::Entity entity = static_cast<ECS::Entity>(entry.entityId);

        switch (entry.resolution) {
            case MergeDiffEntry::Resolution::KeepLocal:
                // Local state is already in the world — nothing to do for most cases.
                // For "DeletedRemote" entries where remote deleted but we want to keep:
                // the entity is already in our world (we never deleted it), so no action.
                // For "AddedRemote" entries where we choose local (reject remote add):
                if (entry.status == MergeDiffEntry::Status::AddedRemote) {
                    if (m_World->IsValid(entity)) {
                        m_World->DestroyEntity(entity);
                    }
                }
                break;

            case MergeDiffEntry::Resolution::KeepRemote:
                // Apply the remote state. For "DeletedLocal" where remote didn't delete:
                // we need to recreate from remote snapshot (if we have it).
                // For "Modified": remote state was already applied by CRDT.
                // For "AddedLocal" entries where we choose remote (reject our add):
                if (entry.status == MergeDiffEntry::Status::AddedLocal) {
                    if (m_World->IsValid(entity)) {
                        m_World->DestroyEntity(entity);
                    }
                }
                // For "DeletedLocal" — entity needs to be recreated from remote snapshot.
                // This would require the remote snapshot JSON which the CRDT or sync
                // mechanism should provide. For now, log a warning.
                if (entry.status == MergeDiffEntry::Status::DeletedLocal) {
                    ENJIN_LOG_WARN(Editor, "CollabMerge: KeepRemote for deleted entity %llu "
                        "requires scene sync from peer", (unsigned long long)entry.entityId);
                }
                break;

            default:
                break;
        }
    }

    ENJIN_LOG_INFO(Editor, "CollabMerge: Applied resolutions for %zu entities", m_Entries.size());
    Clear();
}

void CollabMergeUI::Clear() {
    m_Entries.clear();
    m_HasPending = false;
}

} // namespace Editor
} // namespace Enjin
