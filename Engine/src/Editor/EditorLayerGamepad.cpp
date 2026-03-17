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

void EditorLayer::UpdateGamepadEditor(f32 deltaTime) {
    if (!m_GamepadEditorEnabled || !Input::IsGamepadConnected(0)) return;

    // Don't process editor gamepad when game is playing and consuming input
    if (m_PlayMode.IsPlaying() && !m_PlayMode.IsPaused()) return;

    // Radial menu triggers (hold to open, release to select)
    // RB = Tools, LB = File, Start = Play, Y = Create
    auto checkRadialTrigger = [&](GamepadButton btn, RadialMenuType type) {
        if (Input::IsGamepadButtonPressed(btn, 0)) {
            m_RadialMenuActive = type;
            m_RadialMenuOpenTime = 0.0f;
            m_RadialMenuHovered = -1;
            auto extent = m_Renderer->GetSwapchainExtent();
            m_RadialMenuCenter = Math::Vector2(extent.width * 0.5f, extent.height * 0.5f);
        }
        if (m_RadialMenuActive == type && Input::IsGamepadButtonReleased(btn, 0)) {
            if (m_RadialMenuHovered >= 0) {
                // Execute selected action
                DrawRadialMenu(type); // Ensure action is resolved
            }
            m_RadialMenuActive = RadialMenuType::None;
        }
    };

    checkRadialTrigger(GamepadButton::RightBumper, RadialMenuType::Tools);
    checkRadialTrigger(GamepadButton::LeftBumper, RadialMenuType::File);
    checkRadialTrigger(GamepadButton::Start, RadialMenuType::Play);
    checkRadialTrigger(GamepadButton::Y, RadialMenuType::Create);

    if (m_RadialMenuActive != RadialMenuType::None) {
        m_RadialMenuOpenTime += deltaTime;
        // Update stick angle for sector selection
        auto stick = Input::GetGamepadRightStick(0);
        f32 mag = std::sqrt(stick.x * stick.x + stick.y * stick.y);
        if (mag > 0.3f) {
            m_RadialMenuAngle = std::atan2(stick.y, stick.x);
        } else {
            m_RadialMenuHovered = -1; // Stick centered = no selection
        }
    }

    // Quick actions (no radial menu needed)
    // A = Select/confirm, B = Cancel/deselect, X = Delete
    if (m_RadialMenuActive == RadialMenuType::None) {
        if (Input::IsGamepadButtonPressed(GamepadButton::B, 0)) {
            if (!m_SelectedEntities.empty()) ClearSelection();
        }
        if (Input::IsGamepadButtonPressed(GamepadButton::X, 0)) {
            ExecuteGamepadAction(GamepadAction::Delete);
        }

        // DPad: Navigate hierarchy
        if (Input::IsGamepadButtonPressed(GamepadButton::DPadUp, 0) ||
            Input::IsGamepadButtonPressed(GamepadButton::DPadDown, 0)) {
            // Cycle through entities in hierarchy
            auto entities = m_World ? m_World->GetEntitiesWithComponent<ECS::NameComponent>() : std::vector<ECS::Entity>{};
            if (!entities.empty()) {
                i32 currentIdx = -1;
                for (i32 i = 0; i < static_cast<i32>(entities.size()); ++i) {
                    if (entities[i] == m_PrimarySelected) { currentIdx = i; break; }
                }
                bool down = Input::IsGamepadButtonPressed(GamepadButton::DPadDown, 0);
                i32 newIdx = currentIdx + (down ? 1 : -1);
                if (newIdx < 0) newIdx = static_cast<i32>(entities.size()) - 1;
                if (newIdx >= static_cast<i32>(entities.size())) newIdx = 0;
                SelectEntity(entities[newIdx]);
            }
        }

        // Left stick: Viewport camera navigation
        HandleGamepadViewportNavigation(deltaTime);
    }
}

void EditorLayer::HandleGamepadViewportNavigation(f32 deltaTime) {
    if (!m_Camera) return;

    auto leftStick = Input::GetGamepadLeftStick(0);
    f32 lt = Input::GetGamepadLeftTrigger(0);
    f32 rt = Input::GetGamepadRightTrigger(0);

    // Left stick: Move camera (forward/back + strafe)
    f32 moveSpeed = 10.0f * deltaTime;
    if (Input::IsGamepadButtonDown(GamepadButton::LeftStick, 0)) moveSpeed *= 3.0f; // Sprint

    if (std::abs(leftStick.x) > 0.01f || std::abs(leftStick.y) > 0.01f) {
        auto forward = m_Camera->GetForward();
        auto right = m_Camera->GetRight();
        auto movement = right * (leftStick.x * moveSpeed) - forward * (leftStick.y * moveSpeed);
        m_Camera->SetPosition(m_Camera->GetPosition() + movement);
    }

    // Triggers: Move up/down
    f32 vertical = (rt - lt) * moveSpeed;
    if (std::abs(vertical) > 0.01f) {
        auto pos = m_Camera->GetPosition();
        pos.y += vertical;
        m_Camera->SetPosition(pos);
    }
}

void EditorLayer::DrawRadialMenu(RadialMenuType type) {
    if (type == RadialMenuType::None) return;

    // Define menu items per type
    struct RadialItem {
        const char* label;
        const char* icon;
        GamepadAction action;
    };

    std::vector<RadialItem> items;
    switch (type) {
        case RadialMenuType::Tools:
            items = {
                {"Translate", "[T]", GamepadAction::Translate},
                {"Rotate", "[R]", GamepadAction::Rotate},
                {"Scale", "[S]", GamepadAction::Scale},
                {"Space", "[W/L]", GamepadAction::ToggleSpace},
                {"Focus", "[F]", GamepadAction::FocusSelection},
                {"Grid", "[G]", GamepadAction::ToggleGrid}
            };
            break;
        case RadialMenuType::File:
            items = {
                {"Save", "[Sv]", GamepadAction::Save},
                {"Undo", "[Un]", GamepadAction::Undo},
                {"Redo", "[Re]", GamepadAction::Redo},
                {"Duplicate", "[Dp]", GamepadAction::Duplicate},
                {"Delete", "[Del]", GamepadAction::Delete},
                {"Palette", "[Cmd]", GamepadAction::CommandPalette}
            };
            break;
        case RadialMenuType::Play:
            items = {
                {"Play/Stop", "[>]", GamepadAction::PlayToggle},
                {"Pause", "[||]", GamepadAction::Pause},
                {"Stop", "[X]", GamepadAction::Stop}
            };
            break;
        case RadialMenuType::Create:
            items = {
                {"Empty", "[E]", GamepadAction::CreateEmpty},
                {"Cube", "[C]", GamepadAction::CreateCube},
                {"Light", "[L]", GamepadAction::CreateLight},
                {"Camera", "[Cam]", GamepadAction::CreateCamera},
                {"Sprite", "[S]", GamepadAction::CreateSprite}
            };
            break;
        default: return;
    }

    if (items.empty()) return;

    // Draw the radial menu using ImGui overlay
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    ImVec2 center(m_RadialMenuCenter.x, m_RadialMenuCenter.y);
    f32 innerRadius = 50.0f;
    f32 outerRadius = 140.0f;
    f32 animScale = std::min(1.0f, m_RadialMenuOpenTime * 5.0f); // Quick pop-in
    outerRadius *= animScale;
    innerRadius *= animScale;

    i32 sectorCount = static_cast<i32>(items.size());
    f32 sectorAngle = 6.2831853f / sectorCount;

    // Background circle
    dl->AddCircleFilled(center, outerRadius + 4, IM_COL32(0, 0, 0, 160), 48);
    dl->AddCircle(center, outerRadius + 4, IM_COL32(200, 200, 200, 120), 48, 2.0f);
    dl->AddCircleFilled(center, innerRadius - 2, IM_COL32(30, 30, 30, 200), 32);

    // Determine hovered sector from stick angle
    auto stick = Input::GetGamepadRightStick(0);
    f32 stickMag = std::sqrt(stick.x * stick.x + stick.y * stick.y);
    m_RadialMenuHovered = -1;
    if (stickMag > 0.3f) {
        f32 angle = std::atan2(stick.y, stick.x);
        if (angle < 0) angle += 6.2831853f;
        // Offset so first sector is centered at top (sector 0 starts at -PI/2)
        f32 offsetAngle = angle + 1.5707963f; // +PI/2 to rotate menu so top = first item
        if (offsetAngle > 6.2831853f) offsetAngle -= 6.2831853f;
        m_RadialMenuHovered = static_cast<i32>(offsetAngle / sectorAngle) % sectorCount;
    }

    // Draw sectors
    for (i32 i = 0; i < sectorCount; ++i) {
        f32 startAngle = -1.5707963f + i * sectorAngle; // Start from top
        f32 endAngle = startAngle + sectorAngle;
        f32 midAngle = startAngle + sectorAngle * 0.5f;
        bool hovered = (i == m_RadialMenuHovered);

        // Sector fill
        ImU32 sectorColor = hovered ? IM_COL32(80, 140, 220, 180) : IM_COL32(50, 55, 65, 150);
        constexpr i32 arcSegs = 16;
        ImVec2 pts[arcSegs * 2 + 2];
        for (i32 s = 0; s <= arcSegs; ++s) {
            f32 a = startAngle + (endAngle - startAngle) * s / arcSegs;
            pts[s] = ImVec2(center.x + std::cos(a) * innerRadius, center.y + std::sin(a) * innerRadius);
            pts[arcSegs * 2 + 1 - s] = ImVec2(center.x + std::cos(a) * outerRadius, center.y + std::sin(a) * outerRadius);
        }
        dl->AddConvexPolyFilled(pts, arcSegs * 2 + 2, sectorColor);

        // Sector border lines
        ImVec2 innerPt(center.x + std::cos(startAngle) * innerRadius, center.y + std::sin(startAngle) * innerRadius);
        ImVec2 outerPt(center.x + std::cos(startAngle) * outerRadius, center.y + std::sin(startAngle) * outerRadius);
        dl->AddLine(innerPt, outerPt, IM_COL32(100, 100, 100, 120), 1.0f);

        // Label text centered in sector
        f32 labelDist = (innerRadius + outerRadius) * 0.5f;
        ImVec2 labelPos(center.x + std::cos(midAngle) * labelDist, center.y + std::sin(midAngle) * labelDist);
        ImU32 textColor = hovered ? IM_COL32(255, 255, 255, 255) : IM_COL32(200, 200, 200, 220);
        auto textSize = ImGui::CalcTextSize(items[i].label);
        dl->AddText(ImVec2(labelPos.x - textSize.x * 0.5f, labelPos.y - textSize.y * 0.5f), textColor, items[i].label);
    }

    // Center label showing hovered action
    if (m_RadialMenuHovered >= 0 && m_RadialMenuHovered < sectorCount) {
        auto textSize = ImGui::CalcTextSize(items[m_RadialMenuHovered].icon);
        dl->AddText(ImVec2(center.x - textSize.x * 0.5f, center.y - textSize.y * 0.5f),
                    IM_COL32(255, 255, 255, 255), items[m_RadialMenuHovered].icon);
    }

    // Execute on release (handled in UpdateGamepadEditor via release detection)
    // But also execute if A is pressed while menu is open
    if (Input::IsGamepadButtonPressed(GamepadButton::A, 0) && m_RadialMenuHovered >= 0) {
        ExecuteGamepadAction(items[m_RadialMenuHovered].action);
        m_RadialMenuActive = RadialMenuType::None;
    }
}

void EditorLayer::ExecuteGamepadAction(GamepadAction action) {
    switch (action) {
        // Tools
        case GamepadAction::Translate:    m_GizmoOperation = GizmoOperation::Translate; break;
        case GamepadAction::Rotate:       m_GizmoOperation = GizmoOperation::Rotate; break;
        case GamepadAction::Scale:        m_GizmoOperation = GizmoOperation::Scale; break;
        case GamepadAction::ToggleSpace:
            m_GizmoSpace = (m_GizmoSpace == GizmoSpace::World) ? GizmoSpace::Local : GizmoSpace::World;
            break;
        case GamepadAction::FocusSelection:
            if (!m_SelectedEntities.empty()) FocusOnSelection();
            break;
        case GamepadAction::ToggleGrid:   m_ShowGrid = !m_ShowGrid; break;

        // File
        case GamepadAction::Save:
            if (!m_CurrentScenePath.empty()) SaveScene(m_CurrentScenePath);
            break;
        case GamepadAction::Undo:         m_UndoRedo.Undo(); break;
        case GamepadAction::Redo:         m_UndoRedo.Redo(); break;
        case GamepadAction::Duplicate:
            if (!m_SelectedEntities.empty()) DuplicateSelectedEntities();
            break;
        case GamepadAction::Delete:
            if (!m_SelectedEntities.empty()) DeleteSelectedEntities();
            break;
        case GamepadAction::CommandPalette:
            m_CommandPalette.Toggle();
            break;

        // Play
        case GamepadAction::PlayToggle:
            if (m_PlayMode.IsStopped()) m_PlayMode.Play();
            else m_PendingPlayStop = true;
            break;
        case GamepadAction::Pause:
            if (m_PlayMode.IsPlaying()) m_PlayMode.Pause();
            else if (m_PlayMode.IsPaused()) m_PlayMode.Resume();
            break;
        case GamepadAction::Stop:
            if (!m_PlayMode.IsStopped()) m_PendingPlayStop = true;
            break;

        // Create
        case GamepadAction::CreateEmpty:
            if (m_World) {
                auto e = m_World->CreateEntity();
                m_World->AddComponent<ECS::NameComponent>(e, ECS::NameComponent{"New Entity"});
                m_World->AddComponent<ECS::TransformComponent>(e);
                SelectEntity(e);
            }
            break;
        case GamepadAction::CreateCube:
            if (m_World) {
                auto e = m_World->CreateEntity();
                m_World->AddComponent<ECS::NameComponent>(e, ECS::NameComponent{"Cube"});
                m_World->AddComponent<ECS::TransformComponent>(e);
                m_World->AddComponent<ECS::MeshComponent>(e, Renderer::MeshFactory::CreateCube(1.0f));
                m_World->AddComponent<ECS::MaterialComponent>(e);
                SelectEntity(e);
            }
            break;
        case GamepadAction::CreateLight:
            if (m_World) {
                auto e = m_World->CreateEntity();
                m_World->AddComponent<ECS::NameComponent>(e, ECS::NameComponent{"Light"});
                auto& tf = m_World->AddComponent<ECS::TransformComponent>(e);
                tf.position = Math::Vector3(0, 5, 0);
                m_World->AddComponent<ECS::LightComponent>(e);
                SelectEntity(e);
            }
            break;
        case GamepadAction::CreateCamera:
            if (m_World) {
                auto e = m_World->CreateEntity();
                m_World->AddComponent<ECS::NameComponent>(e, ECS::NameComponent{"Camera"});
                m_World->AddComponent<ECS::TransformComponent>(e);
                m_World->AddComponent<ECS::CameraComponent>(e);
                SelectEntity(e);
            }
            break;
        case GamepadAction::CreateSprite:
            if (m_World) {
                auto e = m_World->CreateEntity();
                m_World->AddComponent<ECS::NameComponent>(e, ECS::NameComponent{"Sprite"});
                m_World->AddComponent<ECS::TransformComponent>(e);
                m_World->AddComponent<ECS::MeshComponent>(e, Renderer::MeshFactory::CreateQuad());
                m_World->AddComponent<ECS::MaterialComponent>(e);
                SelectEntity(e);
            }
            break;
        default: break;
    }
}

} // namespace Editor
} // namespace Enjin
