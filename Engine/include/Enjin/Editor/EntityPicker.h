#pragma once
#include "Enjin/Editor/InspectorUndo.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Name.h"
#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

namespace Enjin {
namespace Editor {
namespace InspectorUndo {

// Choose an entity for a component field that points at one.
//
// Fields like FollowTarget's and LookAtTarget's target were shown as a
// read-only number, so the only way to aim them was editing the scene file:
// the component existed and could not be used from the editor. This is a
// searchable list of the scene's named entities, and it also takes an entity
// dragged in from the Hierarchy. The setter runs through undo; capture the
// world and entity in it rather than a component pointer, which a later add
// can move.
inline bool EntityPicker(UndoRedoManager& undo, const char* label, ECS::World* world,
                         ECS::Entity self, ECS::Entity current,
                         std::function<void(ECS::Entity)> setter) {
    if (!world) return false;

    auto nameOf = [world](ECS::Entity e) -> std::string {
        if (e == ECS::INVALID_ENTITY) return "(none)";
        if (!world->IsValid(e)) return "(missing)";
        if (auto* n = world->GetComponent<ECS::NameComponent>(e); n && !n->name.empty()) return n->name;
        char buf[32];
        std::snprintf(buf, sizeof(buf), "Entity %llu", static_cast<unsigned long long>(ECS::EntityIndex(e)));
        return buf;
    };

    ECS::Entity chosen = current;
    bool picked = false;

    const std::string preview = nameOf(current);
    if (ImGui::BeginCombo(label, preview.c_str())) {
        static char filter[64] = {};
        if (ImGui::IsWindowAppearing()) {
            filter[0] = '\0';
            ImGui::SetKeyboardFocusHere();
        }
        ImGui::InputTextWithHint("##filter", "Search", filter, sizeof(filter));
        std::string needle = filter;
        std::transform(needle.begin(), needle.end(), needle.begin(), ::tolower);

        if (ImGui::Selectable("(none)", current == ECS::INVALID_ENTITY)) { chosen = ECS::INVALID_ENTITY; picked = true; }

        std::vector<std::pair<std::string, ECS::Entity>> rows;
        for (ECS::Entity e : world->GetEntitiesWithComponent<ECS::NameComponent>()) {
            if (e == self || !world->IsValid(e)) continue;
            std::string n = nameOf(e);
            std::string lower = n;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
            if (!needle.empty() && lower.find(needle) == std::string::npos) continue;
            rows.emplace_back(std::move(n), e);
        }
        std::sort(rows.begin(), rows.end());
        for (const auto& [n, e] : rows) {
            ImGui::PushID(static_cast<int>(ECS::EntityIndex(e)));
            if (ImGui::Selectable(n.c_str(), e == current)) { chosen = e; picked = true; }
            ImGui::PopID();
        }
        ImGui::EndCombo();
    }
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("ENTITY_REPARENT")) {
            if (p->DataSize >= static_cast<int>(sizeof(ECS::Entity))) {
                ECS::Entity dropped = *static_cast<const ECS::Entity*>(p->Data);
                if (dropped != self) { chosen = dropped; picked = true; }
            }
        }
        ImGui::EndDragDropTarget();
    }
    ImGui::SetItemTooltip("Pick from the list, or drag an entity here from the Hierarchy.");

    if (!picked || chosen == current) return false;
    undo.Execute(std::make_unique<PropertyEditCommand<ECS::Entity>>(
        label, current, chosen, [setter](const ECS::Entity& e) { setter(e); }));
    return true;
}

} // namespace InspectorUndo
} // namespace Editor
} // namespace Enjin
