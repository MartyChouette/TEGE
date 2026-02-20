#include "Enjin/Editor/EditorLayer.h"
#include "Enjin/Editor/InspectorUndo.h"
#include "Enjin/Editor/ScenePicker.h"
#include "Enjin/Core/Version.h"
#include <GLFW/glfw3.h>
#include <chrono>
#include "Enjin/Logging/Log.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Mesh.h"
#include "Enjin/ECS/Components/Material.h"
#include "Enjin/ECS/Components/Light.h"
#include "Enjin/ECS/Components/Name.h"
#include "Enjin/ECS/Components/Camera.h"
#include "Enjin/ECS/Components/Notes.h"
#include "Enjin/ECS/Components/Controllers/CharacterController.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/ECS/Components/VisualScript.h"
#include "Enjin/AI/BehaviorTree.h"
#include "Enjin/Gameplay/QuestFlow.h"
#include "Enjin/Gameplay/TieredSaveSystem.h"
#include "Enjin/Editor/PlayModeDiff.h"
#include "Enjin/Physics/PhysicsBackendType.h"
#include "Enjin/Physics/PhysicsBackendFactory.h"
#include "Enjin/Physics/PhysicsTypes2D.h"
#include "Enjin/ECS/Components/WeatherZone.h"
#include "Enjin/ECS/Components/WaterVolume.h"
#include "Enjin/ECS/Components/GrassVolume.h"
#include "Enjin/ECS/Components/ShrubVolume.h"
#include "Enjin/ECS/Components/TreeVolume.h"
#include "Enjin/ECS/Components/Terrain.h"
#include "Enjin/ECS/Components/Terrain2D.h"
#include "Enjin/ECS/Components/Vegetation.h"
#include "Enjin/ECS/Components/CameraTrigger.h"
#include "Enjin/ECS/Components/TemperatureZone.h"
#include "Enjin/ECS/Components/GravityZone.h"
#include "Enjin/ECS/Components/PostProcessVolume.h"
#include "Enjin/ECS/Components/FluidVolume.h"
#include "Enjin/ECS/Components/Text.h"
#include "Enjin/ECS/Components/IKComponents.h"
#include "Enjin/ECS/Components/Flower.h"
#ifndef _WIN32
#include <unistd.h>
#endif
#include "Enjin/ECS/Components/LOD.h"
#include "Enjin/ECS/Components/Script.h"
#include "Enjin/ECS/Components/Tween.h"
#include "Enjin/ECS/Components/Hierarchy.h"
#include "Enjin/ECS/Components/Skeleton.h"
#include "Enjin/Renderer/MeshSimplifier.h"
#include "Enjin/Renderer/Skybox.h"
#include "Enjin/ECS/Systems/RenderSystem.h"
#include "Enjin/Renderer/RayTracing/RTShadows.h"
#include "Enjin/Renderer/RayTracing/RTReflections.h"
#include "Enjin/Renderer/RayTracing/RTAmbientOcclusion.h"
#include "Enjin/Renderer/RayTracing/RTGlobalIllumination.h"
#include "Enjin/Renderer/RayTracing/PathTracer.h"
#include "Enjin/Renderer/RayTracing/SVGFDenoiser.h"
#include "Enjin/Renderer/RayTracing/OIDNDenoiser.h"
#include "Enjin/Renderer/RayTracing/RTCompositor.h"
#include "Enjin/Renderer/RayTracing/AccelerationStructureManager.h"
#include "Enjin/Renderer/SHLightProbe.h"
#include "Enjin/Renderer/SDFScene.h"
#include "Enjin/Renderer/OITManager.h"
#include "Enjin/Effects/TreeRenderer.h"
#include "Enjin/Effects/Weather.h"
#include "Enjin/Assets/SceneImporter.h"
#include "Enjin/Assets/FontLibrary.h"
#include "Enjin/Assets/AssetLibrary.h"
#include "Enjin/Assets/AssetMetadata.h"
#include "Enjin/Scene/SceneSerializer.h"
#include "Enjin/Renderer/MeshFactory.h"
#include "Enjin/Renderer/PostProcessing.h"
#include "Enjin/Platform/Input.h"
#include "Enjin/Platform/FileDialog.h"
#include "Enjin/Assets/Prefab.h"
#include "Enjin/Build/BuildPipeline.h"
#include "Enjin/Assets/DataAsset.h"
#include "Enjin/Plugin/PluginRepository.h"
#include "Enjin/Audio/AudioSystem.h"
#include "Enjin/Renderer/NormalMapGenerator.h"
#include "Enjin/Editor/SpriteContourTracer.h"
#include "Enjin/GUI/UICanvas.h"
#include "Enjin/GUI/UITemplates.h"
#include "Enjin/GUI/DialogueImportExport.h"
#include "Enjin/Assets/SWFLoader.h"
#include "Enjin/Effects/CurlNoiseSystem.h"
#include "Enjin/Scripting/ScriptBindings.h"
#include "Enjin/Scene/LevelStreaming.h"
#include "Enjin/Effects/VoronoiMeshFracture.h"
#include "Enjin/Effects/InteractiveWater.h"
#include "Enjin/Math/Math.h"
#include <stb_image.h>
#include <imgui.h>
#include <ImGuizmo.h>
#include <backends/imgui_impl_vulkan.h>
#include <vulkan/vulkan.h>
#include <sstream>
#include <fstream>
#include <filesystem>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
// Undefine Windows macros that collide with engine methods
#undef LoadImage
#undef CreateWindow
#undef min
#undef max
#else
#include <spawn.h>
#include <sys/wait.h>
#endif
#include <climits>
#include <cmath>
#include <algorithm>

namespace Enjin {
namespace Editor {

static const char* GetComponentIcon(const char* componentName) {
    // Map component names to bracket icons
    struct IconMap { const char* name; const char* icon; };
    static const IconMap icons[] = {
        {"Transform", "[+] "},
        {"Mesh", "[M] "},
        {"Material", "[*] "},
        {"Light", "[L] "},
        {"Camera", "[C] "},
        {"Sprite 2D", "[S] "},
        {"Tilemap", "[T] "},
        {"Particle Emitter", "[P] "},
        {"Audio Source", "[A] "},
        {"Rigidbody", "[R] "},
        {"Box Collider", "[B] "},
        {"Sphere Collider", "[O] "},
        {"Capsule Collider", "[I] "},
        {"Dialogue", "[D] "},
        {"Dialogue Box", "[D] "},
        {"Visual Script", "[V] "},
        {"UI Canvas", "[U] "},
        {"Terrain", "[~] "},
        {"Animator", "[>] "},
        {"Health", "[H] "},
        {"AI Controller", "[AI]"},
        {"Behavior Tree", "[BT]"},
        {"LOD", "[#] "},
        {"Skybox", "[=] "},
    };
    for (const auto& m : icons) {
        if (std::strcmp(componentName, m.name) == 0) return m.icon;
    }
    return "";
}

static const char* GetEntityIcon(ECS::World* world, ECS::Entity entity) {
    if (world->HasComponent<ECS::CameraComponent>(entity))      return "[C] ";
    if (world->HasComponent<ECS::LightComponent>(entity))       return "[L] ";
    if (world->HasComponent<ECS::MeshComponent>(entity))        return "[M] ";
    if (world->HasComponent<ECS::Sprite2DComponent>(entity))    return "[S] ";
    if (world->HasComponent<ECS::TilemapComponent>(entity))     return "[T] ";
    if (world->HasComponent<ECS::ParticleEmitterComponent>(entity)) return "[P] ";
    if (world->HasComponent<ECS::AudioSourceComponent>(entity)) return "[A] ";
    if (world->HasComponent<ECS::RigidbodyComponent>(entity))   return "[R] ";
    if (world->HasComponent<ECS::DialogueComponent>(entity))    return "[D] ";
    if (world->HasComponent<ECS::VisualScriptComponent>(entity)) return "[V] ";
    if (world->HasComponent<GUI::UICanvasComponent>(entity))    return "[U] ";
    if (world->HasComponent<ECS::TerrainComponent>(entity))     return "[~] ";
    if (world->HasComponent<ECS::WeatherZoneComponent>(entity)) return "[W] ";
    if (world->HasComponent<ECS::WaterVolumeComponent>(entity))  return "[~] ";
    if (world->HasComponent<ECS::GrassVolumeComponent>(entity)) return "[G] ";
    if (world->HasComponent<ECS::AIControllerComponent>(entity)) return "[AI]";
    if (world->HasComponent<ECS::BehaviorTreeComponent>(entity)) return "[BT]";
    if (world->HasComponent<ECS::NetworkIdentityComponent>(entity)) return "[N] ";
    return "";
}

void EditorLayer::DrawHierarchyPanel() {
    ImGuiWindowFlags flags = 0;
    if (m_FocusMode || !m_PlayMode.IsStopped()) {
        flags |= ImGuiWindowFlags_NoInputs;
    }
    ImGui::Begin("Hierarchy", nullptr, flags);

    // Focus ring for keyboard navigation
    if (m_ShowFocusRing && m_FocusedPanel == FocusedPanel::Hierarchy) {
        ImVec2 wMin = ImGui::GetWindowPos();
        ImVec2 wMax = ImVec2(wMin.x + ImGui::GetWindowWidth(), wMin.y + ImGui::GetWindowHeight());
        ImGui::GetWindowDrawList()->AddRect(wMin, wMax, IM_COL32(100, 200, 255, 200), 0.0f, 0, 2.0f);
        // Auto-focus this window when keyboard-selected
        ImGui::SetWindowFocus();
    }

    if (m_World) {
        // Search/filter bar
        ImGui::SetNextItemWidth(-1);
        ImGui::InputTextWithHint("##HierarchySearch", "Search entities...", m_HierarchySearchBuf, sizeof(m_HierarchySearchBuf));
        ImGui::Separator();

        bool hasFilter = m_HierarchySearchBuf[0] != '\0';
        std::string filterLower;
        if (hasFilter) {
            filterLower = m_HierarchySearchBuf;
            std::transform(filterLower.begin(), filterLower.end(), filterLower.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        }

        // Only show root entities (no parent) at top level; children are drawn recursively
        const auto& entities = m_World->GetAllEntities();

        if (entities.empty()) {
            DrawEmptyState("( )", "No Entities", "Right-click to create your first entity",
                "Create Entity", [this]() {
                    ECS::Entity entity = m_World->CreateEntity();
                    m_World->AddComponent<ECS::TransformComponent>(entity);
                    SelectEntity(entity);
                });
        }

        if (hasFilter) {
            // Flat filtered list — show all matching entities regardless of hierarchy
            for (ECS::Entity entity : entities) {
                auto* nameComp = m_World->GetComponent<ECS::NameComponent>(entity);
                std::string name = nameComp ? nameComp->name : "Entity " + std::to_string(entity);
                std::string nameLower = name;
                std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(),
                    [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                if (nameLower.find(filterLower) == std::string::npos) continue;
                DrawEntityNode(entity, name);
            }
        } else {
        for (ECS::Entity entity : entities) {
            // Skip entities that have a parent — they'll be drawn under their parent
            if (ECS::HasParent(m_World, entity)) continue;

            std::string name;
            auto* nameComp = m_World->GetComponent<ECS::NameComponent>(entity);
            if (nameComp) {
                name = nameComp->name;
            } else {
                name = "Entity " + std::to_string(entity);
            }

            DrawEntityNode(entity, name);
        }
        } // end else (no filter)

        // Drop target for the empty area — unparent entities dropped here
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENTITY_REPARENT")) {
                ECS::Entity droppedEntity = *(const ECS::Entity*)payload->Data;
                ECS::Entity oldParent = ECS::GetParent(m_World, droppedEntity);
                auto cmd = std::make_unique<ReparentEntityCommand>(m_World, droppedEntity, oldParent, ECS::INVALID_ENTITY);
                m_UndoRedo.Execute(std::move(cmd));
            }
            ImGui::EndDragDropTarget();
        }

        // Right-click context menu
        if (ImGui::BeginPopupContextWindow("HierarchyContextMenu", ImGuiPopupFlags_NoOpenOverItems)) {
            if (ImGui::MenuItem("Create Empty Entity")) {
                ECS::Entity entity = m_World->CreateEntity();
                m_World->AddComponent<ECS::TransformComponent>(entity);
                SelectEntity(entity);
                if (m_CollabSystem.IsActive()) {
                    m_CollabSystem.OnEntityCreated(entity,
                        Scene::SceneSerializer::SerializeEntityToString(m_World, entity));
                }
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Load Prefab...")) {
                std::vector<FileFilter> filters = {{ "Prefab Files", "*.enjprefab;*.json" }};
                std::string path = FileDialog::OpenFile("Load Prefab", filters);
                if (!path.empty()) {
                    auto prefab = Assets::PrefabManager::Get().LoadPrefab(path);
                    if (prefab) {
                        ECS::Entity root = Assets::PrefabManager::Get().Instantiate(
                            m_World, *prefab);
                        if (root != ECS::INVALID_ENTITY) {
                            SelectEntity(root);
                        }
                    }
                }
            }
            ImGui::EndPopup();
        }
    } else {
        DrawEmptyState("[ ]", "No World Loaded", "Open or create a scene to begin");
    }

    ImGui::End();
}

void EditorLayer::DrawEmptyState(const char* icon, const char* heading, const char* body,
                                  const char* ctaLabel, std::function<void()> ctaAction) {
    ImVec2 avail = ImGui::GetContentRegionAvail();
    f32 centerY = avail.y * 0.35f;

    // Icon (large, 40% opacity)
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + centerY);
    ImFont* headingFont = m_ImGuiLayer ? m_ImGuiLayer->GetHeadingFont() : nullptr;
    if (headingFont) ImGui::PushFont(headingFont);
    ImVec2 iconSize = ImGui::CalcTextSize(icon);
    ImGui::SetCursorPosX((avail.x - iconSize.x) * 0.5f);
    ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 0.4f), "%s", icon);
    if (headingFont) ImGui::PopFont();

    // Heading
    ImVec2 headingSize = ImGui::CalcTextSize(heading);
    ImGui::SetCursorPosX((avail.x - headingSize.x) * 0.5f);
    ImGui::Text("%s", heading);

    // Body text
    ImVec2 bodySize = ImGui::CalcTextSize(body);
    ImGui::SetCursorPosX((avail.x - bodySize.x) * 0.5f);
    ImGui::TextDisabled("%s", body);

    // Optional CTA button
    if (ctaLabel && ctaAction) {
        ImGui::Spacing();
        ImVec2 btnSize = ImGui::CalcTextSize(ctaLabel);
        btnSize.x += 24.0f;
        btnSize.y += 10.0f;
        ImGui::SetCursorPosX((avail.x - btnSize.x) * 0.5f);
        if (ImGui::Button(ctaLabel, btnSize)) {
            ctaAction();
        }
    }
}

void EditorLayer::DrawEntityNode(ECS::Entity entity, const std::string& name) {
    // Check if this entity has children
    auto childEntities = ECS::GetChildren(m_World, entity);
    bool hasChildren = !childEntities.empty();

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow |
                               ImGuiTreeNodeFlags_SpanAvailWidth;

    if (!hasChildren) {
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    }
    if (IsSelected(entity)) {
        flags |= ImGuiTreeNodeFlags_Selected;
    }

    const char* icon = GetEntityIcon(m_World, entity);
    bool entityLockedByOther = m_SceneLockManager.IsEntityLockedByOther(entity);
    bool entityLockedByMe = m_SceneLockManager.IsEntityLocked(entity) && !entityLockedByOther;
    char labelBuf[256];
    snprintf(labelBuf, sizeof(labelBuf), "%s%s%s", icon,
        entityLockedByOther ? "[X] " : (entityLockedByMe ? "[=] " : ""),
        name.c_str());
    bool opened = ImGui::TreeNodeEx((void*)(uintptr_t)entity, flags, "%s", labelBuf);

    // Dim locked-by-other entities
    if (entityLockedByOther) {
        ImVec2 rMin = ImGui::GetItemRectMin();
        ImVec2 rMax = ImGui::GetItemRectMax();
        ImGui::GetWindowDrawList()->AddRectFilled(rMin, rMax, IM_COL32(255, 80, 80, 30));
    }

    // Capture tree node interaction state before the eye icon overwrites "last item"
    bool nodeClicked = ImGui::IsItemClicked();
    bool nodeHovered = ImGui::IsItemHovered();

    // Drag source — start dragging this entity for reparenting
    // (Must be before eye icon, which overwrites ImGui's "last item")
    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
        ImGui::SetDragDropPayload("ENTITY_REPARENT", &entity, sizeof(ECS::Entity));
        ImGui::Text("%s", name.c_str());
        ImGui::EndDragDropSource();
    }

    // Drop target — drop an entity onto this one to make it a child
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENTITY_REPARENT")) {
            ECS::Entity droppedEntity = *(const ECS::Entity*)payload->Data;
            // Prevent parenting to self or to own descendant
            if (droppedEntity != entity) {
                bool isDescendant = false;
                ECS::Entity check = entity;
                u32 depth = 0;
                while (check != ECS::INVALID_ENTITY && depth < 1000) {
                    if (!m_World->IsValid(check)) break;  // Stale entity — stop traversal
                    if (check == droppedEntity) { isDescendant = true; break; }
                    check = ECS::GetParent(m_World, check);
                    ++depth;
                }
                if (!isDescendant) {
                    ECS::Entity oldParent = ECS::GetParent(m_World, droppedEntity);
                    auto cmd = std::make_unique<ReparentEntityCommand>(m_World, droppedEntity, oldParent, entity);
                    m_UndoRedo.Execute(std::move(cmd));
                }
            }
        }
        ImGui::EndDragDropTarget();
    }

    // Context menu (must be before eye icon overwrites "last item")
    if (ImGui::BeginPopupContextItem()) {
        if (ImGui::MenuItem("Delete", "Del")) {
            if (m_CollabSystem.IsActive()) {
                m_CollabSystem.OnEntityDeleted(entity,
                    Scene::SceneSerializer::SerializeEntityToString(m_World, entity));
            }
            DeselectEntity(entity);
            auto cmd = std::make_unique<FullDeleteEntityCommand>(
                m_World, entity,
                [this](ECS::Entity restored) { SelectEntity(restored); });
            m_UndoRedo.Execute(std::move(cmd));
        }
        if (ImGui::MenuItem("Duplicate", "Ctrl+D")) {
            DuplicateEntity(entity);
        }
        if (ImGui::MenuItem("Focus", "F")) {
            FocusOnEntity(entity);
        }
        // Unparent option if entity has a parent
        if (ECS::HasParent(m_World, entity)) {
            if (ImGui::MenuItem("Unparent")) {
                ECS::Entity oldParent = ECS::GetParent(m_World, entity);
                auto cmd = std::make_unique<ReparentEntityCommand>(m_World, entity, oldParent, ECS::INVALID_ENTITY);
                m_UndoRedo.Execute(std::move(cmd));
            }
        }
        ImGui::Separator();
        // Entity locking
        if (m_SceneLockManager.IsEntityLocked(entity)) {
            if (!m_SceneLockManager.IsEntityLockedByOther(entity)) {
                if (ImGui::MenuItem("Unlock Entity")) {
                    m_SceneLockManager.UnlockEntity(entity);
                }
            } else {
                const auto* lockInfo = m_SceneLockManager.GetEntityLockInfo(entity);
                std::string lockLabel = "Locked by " + (lockInfo ? lockInfo->user : "other");
                ImGui::MenuItem(lockLabel.c_str(), nullptr, false, false);
                // Break lock option for stale locks (>1 hour old)
                if (m_SceneLockManager.IsEntityLockStale(entity)) {
                    if (ImGui::MenuItem("Break Stale Lock")) {
                        m_SceneLockManager.BreakEntityLock(entity);
                        m_Announcer.Announce("Broke stale entity lock", Accessibility::AnnouncePriority::High);
                    }
                    ImGui::SetItemTooltip("This lock is over 1 hour old and may be abandoned");
                }
            }
        } else {
            if (ImGui::MenuItem("Lock Entity")) {
                m_SceneLockManager.LockEntity(entity);
            }
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Save as Prefab...")) {
            std::string defaultName = "prefab.enjprefab";
            if (auto* nc = m_World->GetComponent<ECS::NameComponent>(entity)) {
                defaultName = nc->name + ".enjprefab";
            }
            std::vector<FileFilter> filters = {{ "Prefab Files", "*.enjprefab" }};
            std::string path = FileDialog::SaveFile("Save as Prefab", filters, "", defaultName);
            if (!path.empty()) {
                std::string prefabName = defaultName.substr(0, defaultName.find_last_of('.'));
                auto prefab = Assets::PrefabManager::Get().CreateFromEntity(
                    m_World, entity, prefabName);
                if (prefab) {
                    Assets::PrefabManager::Get().SavePrefab(*prefab, path);
                }
            }
        }
        bool isPrefabInstance = Assets::PrefabUtils::IsPrefabInstance(m_World, entity);
        if (isPrefabInstance) {
            if (ImGui::MenuItem("Unpack Prefab")) {
                Assets::PrefabManager::Get().UnpackInstance(m_World, entity);
            }
        }
        ImGui::EndPopup();
    }

    // Eye icon for visibility toggle (right-aligned on same line)
    {
        auto* transform = m_World->GetComponent<ECS::TransformComponent>(entity);
        if (transform) {
            ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - 18.0f);
            ImGui::PushID(static_cast<int>((uintptr_t)entity ^ 0xEEEE));
            const char* icon = transform->visible ? "O" : "-";
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.5f, 0.5f, 0.5f, 0.3f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.5f, 0.5f, 0.5f, 0.5f));
            if (!transform->visible) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 0.5f));
            }
            if (ImGui::SmallButton(icon)) {
                transform->visible = !transform->visible;
            }
            if (!transform->visible) {
                ImGui::PopStyleColor();
            }
            ImGui::PopStyleColor(3);
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip(transform->visible ? "Hide entity" : "Show entity");
            }
            ImGui::PopID();
        }
    }

    // Click handling (use saved tree node state)
    if (nodeClicked && !ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        bool ctrlHeld = Input::IsKeyDown(KeyCode::LeftControl) || Input::IsKeyDown(KeyCode::RightControl);
        bool shiftHeld = Input::IsKeyDown(KeyCode::LeftShift) || Input::IsKeyDown(KeyCode::RightShift);

        if (ctrlHeld) {
            if (IsSelected(entity)) {
                DeselectEntity(entity);
            } else {
                SelectEntity(entity, true);
            }
        } else if (shiftHeld && m_PrimarySelected != ECS::INVALID_ENTITY) {
            SelectRange(m_PrimarySelected, entity);
        } else {
            SelectEntity(entity);
        }
    }

    // Double-click to focus camera on entity
    if (nodeHovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        FocusOnEntity(entity);
    }

    // Recursively draw children if node is open
    if (hasChildren && opened) {
        for (ECS::Entity child : childEntities) {
            std::string childName;
            auto* childNameComp = m_World->GetComponent<ECS::NameComponent>(child);
            if (childNameComp) {
                childName = childNameComp->name;
            } else {
                childName = "Entity " + std::to_string(child);
            }
            DrawEntityNode(child, childName);
        }
        ImGui::TreePop();
    }
}

} // namespace Editor
} // namespace Enjin
