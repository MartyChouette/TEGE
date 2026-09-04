#include "Enjin/Input/TouchActionBridge.h"
#include "Enjin/Editor/EditorLayer.h"
#include "Enjin/Editor/EditorWidgets.h"
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
#include "Enjin/Renderer/DDGIProbeSystem.h"
#include "Enjin/Renderer/VolumetricFog.h"
#include "Enjin/Effects/GPUParticleSystem.h"
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

// ── System settings section drawers ──

void EditorLayer::DrawSettingsSection_Camera() {
    if (UI::SectionHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (m_CameraController) {
            // View preset buttons
            ImGui::Text("View Presets:");
            if (ImGui::Button("Persp")) {
                m_CameraController->SetViewPreset(Renderer::ViewPreset::Perspective);
            }
            ImGui::SameLine();
            if (ImGui::Button("Top")) {
                m_CameraController->SetViewPreset(Renderer::ViewPreset::Top);
            }
            ImGui::SameLine();
            if (ImGui::Button("Front")) {
                m_CameraController->SetViewPreset(Renderer::ViewPreset::Front);
            }
            ImGui::SameLine();
            if (ImGui::Button("Right")) {
                m_CameraController->SetViewPreset(Renderer::ViewPreset::Right);
            }

            if (ImGui::Button("Bottom")) {
                m_CameraController->SetViewPreset(Renderer::ViewPreset::Bottom);
            }
            ImGui::SameLine();
            if (ImGui::Button("Back")) {
                m_CameraController->SetViewPreset(Renderer::ViewPreset::Back);
            }
            ImGui::SameLine();
            if (ImGui::Button("Left")) {
                m_CameraController->SetViewPreset(Renderer::ViewPreset::Left);
            }

            ImGui::Separator();

            // Orthographic toggle
            bool isOrtho = m_CameraController->IsOrthographic();
            if (ImGui::Checkbox("Orthographic", &isOrtho)) {
                m_CameraController->SetOrthographic(isOrtho);
            }

            if (isOrtho) {
                f32 orthoSize = m_CameraController->GetOrthoSize();
                if (ImGui::DragFloat("Ortho Size", &orthoSize, 0.5f, 1.0f, 100.0f)) {
                    m_CameraController->SetOrthoSize(orthoSize);
                    m_CameraController->SetOrthographic(true);  // Refresh projection
                }
            }

            ImGui::Separator();

            f32 moveSpeed = m_CameraController->GetMoveSpeed();
            if (ImGui::DragFloat("Move Speed", &moveSpeed, 0.5f, 0.1f, 100.0f)) {
                m_CameraController->SetMoveSpeed(moveSpeed);
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("WASD movement speed in the viewport");
            }

            f32 sensitivity = m_CameraController->GetLookSensitivity();
            if (ImGui::DragFloat("Look Sensitivity", &sensitivity, 0.01f, 0.01f, 1.0f)) {
                m_CameraController->SetLookSensitivity(sensitivity);
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Mouse look sensitivity (Right-click + drag)");
            }

            // Camera mode
            const char* modes[] = { "Fly", "Orbit", "First Person" };
            int currentMode = static_cast<int>(m_CameraController->GetMode());
            if (ImGui::Combo("Mode", &currentMode, modes, 3)) {
                m_CameraController->SetMode(static_cast<Renderer::CameraMode>(currentMode));
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Fly: WASD + RMB look\nOrbit: MMB to orbit around selection\nFirst Person: Ground-level movement");
            }
        }

        if (m_Camera) {
            Math::Vector3 pos = m_Camera->GetPosition();
            ImGui::Text("Position: %.2f, %.2f, %.2f", pos.x, pos.y, pos.z);

            f32 yaw = m_CameraController ? m_CameraController->GetYaw() : 0.0f;
            f32 pitch = m_CameraController ? m_CameraController->GetPitch() : 0.0f;
            ImGui::Text("Yaw: %.1f  Pitch: %.1f", yaw, pitch);
        }

        ImGui::Separator();
        ImGui::Text("Game Camera:");

        // Gather all cameras for the selector
        std::vector<ECS::Entity> settingsCameraEntities;
        if (m_World) {
            const auto& camEnts = m_World->GetEntitiesWithComponent<ECS::CameraComponent>();
            settingsCameraEntities.assign(camEnts.begin(), camEnts.end());
        }

        // Camera selector (shared with game view)
        if (!settingsCameraEntities.empty() && m_Camera && m_CameraController) {
            // Validate selection
            if (m_SelectedGameCamera == ECS::INVALID_ENTITY) {
                m_SelectedGameCamera = settingsCameraEntities[0];
            }

            // Dropdown to pick camera
            if (settingsCameraEntities.size() > 1) {
                std::string currentName = "None";
                if (m_SelectedGameCamera != ECS::INVALID_ENTITY && m_World->HasComponent<ECS::NameComponent>(m_SelectedGameCamera)) {
                    currentName = m_World->GetComponent<ECS::NameComponent>(m_SelectedGameCamera)->name;
                } else if (m_SelectedGameCamera != ECS::INVALID_ENTITY) {
                    currentName = "Camera " + std::to_string(m_SelectedGameCamera);
                }
                ImGui::SetNextItemWidth(-1);
                if (ImGui::BeginCombo("##SettingsCamSelect", currentName.c_str())) {
                    for (ECS::Entity camEnt : settingsCameraEntities) {
                        std::string name;
                        if (m_World->HasComponent<ECS::NameComponent>(camEnt)) {
                            name = m_World->GetComponent<ECS::NameComponent>(camEnt)->name;
                        } else {
                            name = "Camera " + std::to_string(camEnt);
                        }
                        bool selected = (camEnt == m_SelectedGameCamera);
                        if (ImGui::Selectable(name.c_str(), selected)) {
                            m_SelectedGameCamera = camEnt;
                        }
                        if (selected) ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
            }

            ECS::Entity gameCamEntity = m_SelectedGameCamera;
            auto* gameCamComp = m_World->GetComponent<ECS::CameraComponent>(gameCamEntity);
            auto* gameCamTransform = m_World->GetComponent<ECS::TransformComponent>(gameCamEntity);

            if (gameCamComp && gameCamTransform) {
                std::string camName = "Game Camera";
                if (m_World->HasComponent<ECS::NameComponent>(gameCamEntity)) {
                    camName = m_World->GetComponent<ECS::NameComponent>(gameCamEntity)->name;
                }
                ImGui::Text("Selected: %s", camName.c_str());

                // Apply editor view to game camera (ONE-SHOT)
                if (ImGui::Button("Apply Editor View to Game Camera")) {
                    gameCamTransform->position = m_Camera->GetPosition();

                    // Copy the camera's actual world rotation. Both the renderer
                    // camera and the game camera treat forward as -Z, so this is
                    // exact. Rebuilding from controller yaw/pitch (previous code)
                    // negated yaw vs the controller's own forward convention and
                    // went stale after orbit/F-focus — position landed right but
                    // the view faced the wrong way (Marty, 2026-08-07).
                    gameCamTransform->rotation = m_Camera->GetRotation();

                    if (m_CameraController->IsOrthographic()) {
                        gameCamComp->projectionType = ECS::ProjectionType::Orthographic;
                        gameCamComp->orthoSize = m_CameraController->GetOrthoSize();
                    } else {
                        gameCamComp->projectionType = ECS::ProjectionType::Perspective;
                    }

                    ENJIN_LOG_INFO(Editor, "Applied editor view to game camera '%s'", camName.c_str());
                }
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("One-shot: copies current editor position/rotation to the game camera");

                // Apply game camera view to editor (ONE-SHOT)
                if (ImGui::Button("Snap Editor to Game Camera")) {
                    m_Camera->SetPosition(gameCamTransform->position);

                    Math::Vector3 forward = gameCamTransform->rotation.Rotate(Math::Vector3(0.0f, 0.0f, -1.0f));
                    Math::Vector3 up = gameCamTransform->rotation.Rotate(Math::Vector3(0.0f, 1.0f, 0.0f));
                    m_Camera->SetLookAt(gameCamTransform->position,
                                         gameCamTransform->position + forward, up);
                    m_CameraController->SyncFromCamera();

                    if (gameCamComp->projectionType == ECS::ProjectionType::Orthographic) {
                        m_CameraController->SetOrthoSize(gameCamComp->orthoSize);
                        m_CameraController->SetOrthographic(true);
                    } else {
                        m_CameraController->SetOrthographic(false);
                    }

                    ENJIN_LOG_INFO(Editor, "Snapped editor to game camera '%s'", camName.c_str());
                }
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("One-shot: moves editor camera to match the game camera's view");
            }
        } else {
            ImGui::TextDisabled("No game camera in scene");
        }
    }
}

void EditorLayer::DrawSettingsSection_EditorPerformance() {
    if (UI::SectionHeader("Grid", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox("Show Grid", &m_ShowGrid);
        if (m_ShowGrid) {
            ImGui::DragFloat("Grid Size", &m_GridSize, 1.0f, 1.0f, 500.0f);
            ImGui::DragInt("Grid Lines", &m_GridLines, 1, 5, 400);
        }
    }

    if (UI::SectionHeader("Performance")) {
        bool settingsChanged = false;

        // Frame Rate Limit
        const char* fpsOptions[] = { "Uncapped", "30", "60", "120", "144", "240" };
        int fpsValues[] = { 0, 30, 60, 120, 144, 240 };
        int currentIdx = 0;
        u32 currentVal = static_cast<u32>(m_EditorSettings.editorFrameRateLimit);
        for (int i = 0; i < 6; ++i) {
            if (static_cast<u32>(fpsValues[i]) == currentVal) { currentIdx = i; break; }
        }
        if (ImGui::Combo("Frame Rate Limit", &currentIdx, fpsOptions, 6)) {
            m_EditorSettings.editorFrameRateLimit = static_cast<FrameRateLimit>(fpsValues[currentIdx]);
            settingsChanged = true;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Limit the editor's frame rate to reduce GPU usage");
        }

        // VSync (disabled when Uncapped)
        bool isUncapped = m_EditorSettings.editorFrameRateLimit == FrameRateLimit::Uncapped;
        if (isUncapped) {
            ImGui::BeginDisabled();
        }
        if (ImGui::Checkbox("VSync", &m_EditorSettings.editorVSync)) {
            if (m_Renderer) {
                m_Renderer->RequestVSyncChange(m_EditorSettings.editorVSync);
            }
        }
        if (isUncapped) {
            ImGui::EndDisabled();
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
            if (isUncapped) {
                ImGui::SetTooltip("VSync is not available when frame rate is Uncapped");
            } else {
                ImGui::SetTooltip("Synchronize frame rate to monitor refresh rate");
            }
        }

        ImGui::Separator();
        ImGui::Text("Power Saving:");

        // Reduce When Unfocused
        if (ImGui::Checkbox("Reduce When Unfocused", &m_EditorSettings.reduceFrameRateWhenUnfocused)) {
            settingsChanged = true;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Lower frame rate when the editor window is not focused");
        }

        if (m_EditorSettings.reduceFrameRateWhenUnfocused) {
            ImGui::Indent();
            int unfocusedFPS = static_cast<int>(m_EditorSettings.unfocusedFrameRate);
            if (ImGui::SliderInt("Unfocused FPS", &unfocusedFPS, 5, 60)) {
                m_EditorSettings.unfocusedFrameRate = static_cast<u32>(unfocusedFPS);
                settingsChanged = true;
            }
            ImGui::Unindent();
        }

        // Reduce When Idle
        if (ImGui::Checkbox("Reduce When Idle", &m_EditorSettings.reduceFrameRateWhenIdle)) {
            settingsChanged = true;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Lower frame rate after no input for a period of time");
        }

        if (m_EditorSettings.reduceFrameRateWhenIdle) {
            ImGui::Indent();
            if (ImGui::SliderFloat("Idle Timeout (sec)", &m_EditorSettings.idleTimeoutSeconds, 5.0f, 120.0f, "%.0f")) {
                settingsChanged = true;
            }
            int idleFPS = static_cast<int>(m_EditorSettings.idleFrameRate);
            if (ImGui::SliderInt("Idle FPS", &idleFPS, 5, 60)) {
                m_EditorSettings.idleFrameRate = static_cast<u32>(idleFPS);
                settingsChanged = true;
            }
            ImGui::Unindent();
        }

        if (settingsChanged) {
            m_EditorSettings.Save();
        }
    }

    if (UI::SectionHeader("Gizmos", ImGuiTreeNodeFlags_DefaultOpen)) {
        // Gizmo operation
        const char* operations[] = { "Translate (1)", "Rotate (2)", "Scale (3)" };
        int currentOp = static_cast<int>(m_GizmoOperation);
        if (ImGui::Combo("Operation", &currentOp, operations, 3)) {
            m_GizmoOperation = static_cast<GizmoOperation>(currentOp);
        }

        // Gizmo space
        const char* spaces[] = { "Local", "World" };
        int currentSpace = static_cast<int>(m_GizmoSpace);
        if (ImGui::Combo("Space (4)", &currentSpace, spaces, 2)) {
            m_GizmoSpace = static_cast<GizmoSpace>(currentSpace);
        }

        // Snap settings
        ImGui::Checkbox("Enable Snap", &m_UseSnap);
        if (m_UseSnap) {
            ImGui::DragFloat("Translate Snap", &m_TranslateSnap, 0.1f, 0.1f, 10.0f);
            ImGui::DragFloat("Rotate Snap", &m_RotateSnap, 1.0f, 1.0f, 90.0f);
            ImGui::DragFloat("Scale Snap", &m_ScaleSnap, 0.01f, 0.01f, 1.0f);
        }

        ImGui::Separator();
        if (ImGui::Checkbox("Surface Snap", &m_SurfaceSnap)) {
            m_EditorSettings.surfaceSnap = m_SurfaceSnap;
            m_EditorSettings.Save();
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Snap entities to terrain and sphere gravity zone surfaces");
        }
        if (m_SurfaceSnap) {
            if (ImGui::Checkbox("Align to Surface Normal", &m_SurfaceAlignNormal)) {
                m_EditorSettings.surfaceAlignNormal = m_SurfaceAlignNormal;
                m_EditorSettings.Save();
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Rotate entity so Y-axis aligns with the surface normal");
            }
        }
    }

    if (UI::SectionHeader("Gamepad")) {
        // Global dead zone setting
        f32 deadZone = Input::GetGamepadDeadZone();
        if (ImGui::SliderFloat("Dead Zone", &deadZone, 0.01f, 0.5f, "%.2f")) {
            Input::SetGamepadDeadZone(deadZone);
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Analog stick dead zone threshold");

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Show status of each gamepad slot
        for (i32 gp = 0; gp < 4; ++gp) {
            bool connected = Input::IsGamepadConnected(gp);
            ImGui::PushID(gp);

            // Header with connection indicator
            ImVec4 headerCol = connected ? ImVec4(0.2f, 0.6f, 0.2f, 1.0f) : ImVec4(0.4f, 0.4f, 0.4f, 1.0f);
            ImGui::PushStyleColor(ImGuiCol_Text, headerCol);
            char label[64];
            if (connected) {
                snprintf(label, sizeof(label), "Gamepad %d: %s", gp, Input::GetGamepadName(gp));
            } else {
                snprintf(label, sizeof(label), "Gamepad %d: Not Connected", gp);
            }
            bool open = ImGui::TreeNode(label);
            ImGui::PopStyleColor();

            if (open) {
                if (connected) {
                    // Stick visualization using progress bars
                    Math::Vector2 leftStick = Input::GetGamepadLeftStick(gp);
                    Math::Vector2 rightStick = Input::GetGamepadRightStick(gp);

                    ImGui::Text("Left Stick:");
                    ImGui::SameLine(120);
                    ImGui::Text("X: %+.2f  Y: %+.2f", leftStick.x, leftStick.y);

                    ImGui::Text("Right Stick:");
                    ImGui::SameLine(120);
                    ImGui::Text("X: %+.2f  Y: %+.2f", rightStick.x, rightStick.y);

                    // Triggers
                    f32 lt = Input::GetGamepadLeftTrigger(gp);
                    f32 rt = Input::GetGamepadRightTrigger(gp);
                    ImGui::Text("L Trigger:");
                    ImGui::SameLine(120);
                    ImGui::ProgressBar(lt, ImVec2(100, 14), "");
                    ImGui::Text("R Trigger:");
                    ImGui::SameLine(120);
                    ImGui::ProgressBar(rt, ImVec2(100, 14), "");

                    // Button states in a compact grid
                    ImGui::Spacing();
                    ImGui::Text("Buttons:");

                    struct BtnInfo { const char* name; GamepadButton btn; };
                    BtnInfo buttons[] = {
                        {"A", GamepadButton::A}, {"B", GamepadButton::B},
                        {"X", GamepadButton::X}, {"Y", GamepadButton::Y},
                        {"LB", GamepadButton::LeftBumper}, {"RB", GamepadButton::RightBumper},
                        {"Back", GamepadButton::Back}, {"Start", GamepadButton::Start},
                    };

                    for (int b = 0; b < 8; ++b) {
                        bool pressed = Input::IsGamepadButtonDown(buttons[b].btn, gp);
                        if (pressed) {
                            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.7f, 0.3f, 1.0f));
                        }
                        ImGui::SmallButton(buttons[b].name);
                        if (pressed) {
                            ImGui::PopStyleColor();
                        }
                        if (b < 7 && (b % 4) != 3) ImGui::SameLine();
                    }
                } else {
                    ImGui::TextDisabled("Connect a controller to see live input");
                }
                ImGui::TreePop();
            }
            ImGui::PopID();
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::TextDisabled("Gamepad Button Mapping:");
        ImGui::BulletText("Left Stick - Movement");
        ImGui::BulletText("Right Stick - Camera look");
        ImGui::BulletText("A - Jump");
        ImGui::BulletText("B - Crouch");
        ImGui::BulletText("LB / L3 - Sprint");
        ImGui::BulletText("RB - Dash");
        ImGui::BulletText("LT / RT - Move down / up (editor)");
        ImGui::BulletText("D-pad Up/Down - Adjust editor speed");
    }

    if (UI::SectionHeader("Controls")) {
        if (ImGui::TreeNode("Active Controllers")) {
            bool foundAny = false;
            auto getEntName = [this](ECS::Entity entity) -> std::string {
                auto* nameComp = m_World->GetComponent<ECS::NameComponent>(entity);
                return nameComp ? nameComp->name : "Entity " + std::to_string(entity);
            };
            for (ECS::Entity entity : m_World->GetEntitiesWithComponent<ECS::FirstPersonController>()) {
                auto* fps = m_World->GetComponent<ECS::FirstPersonController>(entity);
                if (fps) {
                    foundAny = true;
                    std::string entName = getEntName(entity);
                    ImGui::BulletText("%s [First Person]", entName.c_str());
                    ImGui::Indent();
                    ImGui::Text("Speed: %.1f  Grounded: %s  Pitch: %.1f  Yaw: %.1f",
                        fps->velocity.Length(), fps->isGrounded ? "Y" : "N", fps->pitch, fps->yaw);
                    ImGui::Unindent();
                }
            }
            for (ECS::Entity entity : m_World->GetEntitiesWithComponent<ECS::ThirdPersonController>()) {
                auto* tps = m_World->GetComponent<ECS::ThirdPersonController>(entity);
                if (tps) {
                    foundAny = true;
                    std::string entName = getEntName(entity);
                    ImGui::BulletText("%s [Third Person]", entName.c_str());
                    ImGui::Indent();
                    ImGui::Text("Speed: %.1f  Grounded: %s  Pitch: %.1f  Yaw: %.1f",
                        tps->velocity.Length(), tps->isGrounded ? "Y" : "N", tps->cameraPitch, tps->cameraYaw);
                    ImGui::Unindent();
                }
            }
            for (ECS::Entity entity : m_World->GetEntitiesWithComponent<ECS::TopDown3DController>()) {
                auto* td3 = m_World->GetComponent<ECS::TopDown3DController>(entity);
                if (td3) {
                    foundAny = true;
                    std::string entName = getEntName(entity);
                    ImGui::BulletText("%s [Top-Down 3D]", entName.c_str());
                    ImGui::Indent();
                    ImGui::Text("Speed: %.1f  Grounded: %s", td3->velocity.Length(), td3->isGrounded ? "Y" : "N");
                    ImGui::Unindent();
                }
            }
            for (ECS::Entity entity : m_World->GetEntitiesWithComponent<ECS::VehicleController>()) {
                auto* veh = m_World->GetComponent<ECS::VehicleController>(entity);
                if (veh) {
                    foundAny = true;
                    std::string entName = getEntName(entity);
                    ImGui::BulletText("%s [Vehicle]", entName.c_str());
                    ImGui::Indent();
                    ImGui::Text("Speed: %.1f  Steer: %.1f  Heading: %.1f  Drifting: %s",
                        veh->currentSpeed, veh->currentSteerAngle, veh->heading, veh->isDrifting ? "Y" : "N");
                    ImGui::Unindent();
                }
            }
            for (ECS::Entity entity : m_World->GetEntitiesWithComponent<ECS::Platformer2DController>()) {
                auto* p2d = m_World->GetComponent<ECS::Platformer2DController>(entity);
                if (p2d) {
                    foundAny = true;
                    std::string entName = getEntName(entity);
                    ImGui::BulletText("%s [Platformer 2D]", entName.c_str());
                    ImGui::Indent();
                    ImGui::Text("Speed: %.1f  Grounded: %s  Jumps: %d/%d",
                        p2d->velocity.Length(), p2d->isGrounded ? "Y" : "N", p2d->currentJumps, p2d->maxJumps);
                    ImGui::Unindent();
                }
            }
            for (ECS::Entity entity : m_World->GetEntitiesWithComponent<ECS::TopDown2DController>()) {
                auto* td2 = m_World->GetComponent<ECS::TopDown2DController>(entity);
                if (td2) {
                    foundAny = true;
                    std::string entName = getEntName(entity);
                    ImGui::BulletText("%s [Top-Down 2D]", entName.c_str());
                    ImGui::Indent();
                    ImGui::Text("Speed: %.1f  Facing: %.1f deg", td2->velocity.Length(), td2->facingAngle);
                    ImGui::Unindent();
                }
            }
            if (!foundAny) {
                ImGui::TextDisabled("No controllers in scene");
            }
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Input State")) {
            ImGui::Text("WASD: %s%s%s%s  Arrows: %s%s%s%s",
                Input::IsKeyDown(KeyCode::W) ? "W" : "-",
                Input::IsKeyDown(KeyCode::A) ? "A" : "-",
                Input::IsKeyDown(KeyCode::S) ? "S" : "-",
                Input::IsKeyDown(KeyCode::D) ? "D" : "-",
                Input::IsKeyDown(KeyCode::Up) ? "U" : "-",
                Input::IsKeyDown(KeyCode::Left) ? "L" : "-",
                Input::IsKeyDown(KeyCode::Down) ? "D" : "-",
                Input::IsKeyDown(KeyCode::Right) ? "R" : "-");
            ImGui::Text("Space: %s  Shift: %s  Ctrl: %s",
                Input::IsKeyDown(KeyCode::Space) ? "Y" : "N",
                Input::IsKeyDown(KeyCode::LeftShift) ? "Y" : "N",
                Input::IsKeyDown(KeyCode::LeftControl) ? "Y" : "N");

            Math::Vector2 mpos = Input::GetMousePosition();
            Math::Vector2 mdelta = Input::GetMouseDelta();
            ImGui::Text("Mouse: (%.0f, %.0f)  Delta: (%.1f, %.1f)", mpos.x, mpos.y, mdelta.x, mdelta.y);
            ImGui::Text("Mouse Buttons: L:%s  R:%s  M:%s",
                Input::IsMouseButtonDown(MouseButton::Left) ? "Y" : "N",
                Input::IsMouseButtonDown(MouseButton::Right) ? "Y" : "N",
                Input::IsMouseButtonDown(MouseButton::Middle) ? "Y" : "N");

            for (int gp = 0; gp < 4; ++gp) {
                if (!Input::IsGamepadConnected(gp)) continue;
                ImGui::Separator();
                ImGui::Text("Gamepad %d: %s", gp, Input::GetGamepadName(gp));
                ImGui::Text("  L Stick: (%.2f, %.2f)  R Stick: (%.2f, %.2f)",
                    Input::GetGamepadAxis(GamepadAxis::LeftX, gp), Input::GetGamepadAxis(GamepadAxis::LeftY, gp),
                    Input::GetGamepadAxis(GamepadAxis::RightX, gp), Input::GetGamepadAxis(GamepadAxis::RightY, gp));
                ImGui::Text("  L Trigger: %.2f  R Trigger: %.2f",
                    Input::GetGamepadAxis(GamepadAxis::LeftTrigger, gp), Input::GetGamepadAxis(GamepadAxis::RightTrigger, gp));
            }
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Mouse Settings")) {
            ImGui::Text("Sensitivity: %.2f", m_InputMap.GetMouseSensitivity());
            ImGui::Text("Raw Input: %s", m_EditorSettings.rawMouseInput ? "Enabled" : "Disabled");
            ImGui::Text("Smoothing: %.2f", m_EditorSettings.mouseSmoothing);
            ImGui::Text("Mouse Captured: %s", Input::IsMouseCaptured() ? "Yes" : "No");
            ImGui::TreePop();
        }
    }

    if (UI::SectionHeader("Keyboard Shortcuts")) {
        ImGui::BulletText("RMB + WASD - Fly camera (horizontal plane)");
        ImGui::BulletText("Space / Q - Move up / down");
        ImGui::BulletText("Shift - Sprint");
        ImGui::BulletText("Left Ctrl - Move down (alt)");
        ImGui::BulletText("RMB + Drag - Look around");
        ImGui::BulletText("MMB + Drag - Orbit around selection");
        ImGui::BulletText("Scroll Wheel - Adjust speed / zoom");
        ImGui::BulletText("F - Focus on selected entity");
        ImGui::BulletText("Delete - Delete selected entity");
        ImGui::BulletText("Ctrl+D - Duplicate entity");
        ImGui::BulletText("1/2/3 - Translate/Rotate/Scale gizmo");
        ImGui::BulletText("4 - Toggle Local/World space");
        ImGui::BulletText("Ctrl+S - Save scene");
        ImGui::BulletText("F11 - Toggle focus mode");
    }
}

void EditorLayer::DrawSettingsSection_ExternalIDE() {
    if (UI::SectionHeader("External IDE")) {
        bool ideChanged = false;

        const char* ideNames[] = { "Auto (VS Code)", "VS Code", "Visual Studio", "Rider", "Custom" };
        int currentIDE = static_cast<int>(m_EditorSettings.externalIDE);
        if (ImGui::Combo("IDE", &currentIDE, ideNames, 5)) {
            m_EditorSettings.externalIDE = static_cast<u32>(currentIDE);
            ideChanged = true;
        }

        if (m_EditorSettings.externalIDE == 4) {
            char idePath[512];
            strncpy(idePath, m_EditorSettings.customIDEPath.c_str(), sizeof(idePath) - 1);
            idePath[sizeof(idePath) - 1] = '\0';
            if (ImGui::InputText("IDE Path", idePath, sizeof(idePath))) {
                m_EditorSettings.customIDEPath = idePath;
                ideChanged = true;
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("Browse...")) {
                std::vector<FileFilter> filters = {
#ifdef ENJIN_PLATFORM_WINDOWS
                    { "Executable", "*.exe" },
#endif
                    { "All Files", "*.*" }
                };
                std::string selected = FileDialog::OpenFile("Select IDE Executable", filters);
                if (!selected.empty()) {
                    m_EditorSettings.customIDEPath = selected;
                    ideChanged = true;
                }
            }
        }

        if (ImGui::Button("Test Open")) {
            OpenInExternalIDE("enjin_api/TegeBehavior.as");
        }
        ImGui::SameLine();
        ImGui::TextDisabled("Opens enjin_api/TegeBehavior.as");

        if (ideChanged) {
            m_EditorSettings.Save();
        }
    }
}

void EditorLayer::DrawSettingsSection_BugReporting() {
    if (UI::SectionHeader("Bug Reporting")) {
        bool changed = false;

        ImGui::TextDisabled("Bug reports and feedback are sent to the TEGE Discord automatically.");
        ImGui::TextDisabled("Use Help > Report Bug (Ctrl+Shift+B) to submit a report.");
    }
}

void EditorLayer::DrawSettingsSection_Accessibility() {
    if (UI::SectionHeader("Accessibility")) {
        bool settingsChanged = false;

        // Theme
        const char* themeNames[] = {
            "Dark", "Glass", "Light", "High Contrast Dark", "High Contrast Light",
            "SNES", "PS2", "Xbox", "Dreamcast", "Sega Saturn", "GBA", "DS"
        };
        int currentTheme = static_cast<int>(m_EditorSettings.theme);
        if (ImGui::Combo("Theme", &currentTheme, themeNames, 12)) {
            m_EditorSettings.theme = static_cast<EditorTheme>(currentTheme);
            m_ImGuiLayer->ApplyTheme(m_EditorSettings.theme, &m_EditorSettings.accentColors);
            settingsChanged = true;
        }

        // UI Scale
        if (ImGui::SliderFloat("UI Scale", &m_EditorSettings.uiScale, 0.75f, 2.0f, "%.2f")) {
            m_ImGuiLayer->SetGlobalScale(m_EditorSettings.uiScale);
            settingsChanged = true;
        }

        ImGui::Separator();

        // -- Accent Colors --
        if (ImGui::TreeNode("Accent Colors")) {
            DrawAccentColorPicker();
            ImGui::TreePop();
        }

        // -- Theme Preview --
        if (ImGui::TreeNode("Theme Preview")) {
            DrawThemePreview();
            ImGui::TreePop();
        }

        ImGui::Separator();

        // -- Visual Accessibility --
        if (ImGui::TreeNode("Visual")) {
            const char* cbModes[] = {
                "Off", "Protanopia", "Deuteranopia", "Tritanopia",
                "Protanomaly", "Deuteranomaly", "Tritanomaly", "Achromatopsia"
            };
            int cbMode = static_cast<int>(m_EditorSettings.colorblindMode);
            if (ImGui::Combo("Colorblind Mode", &cbMode, cbModes, 8)) {
                m_EditorSettings.colorblindMode = static_cast<u32>(cbMode);
                settingsChanged = true;
            }

            if (m_EditorSettings.colorblindMode > 0) {
                if (ImGui::SliderFloat("Correction Strength", &m_EditorSettings.colorblindStrength, 0.0f, 1.0f)) {
                    settingsChanged = true;
                }
            }

            if (ImGui::SliderFloat("Screen Brightness", &m_EditorSettings.screenBrightness, -0.5f, 0.5f)) {
                settingsChanged = true;
            }
            if (ImGui::SliderFloat("Screen Contrast", &m_EditorSettings.screenContrast, 0.5f, 2.0f)) {
                settingsChanged = true;
            }

            ImGui::TreePop();
        }

        // -- Motion --
        if (ImGui::TreeNode("Motion")) {
            if (ImGui::Checkbox("Reduced Motion", &m_EditorSettings.reducedMotion)) settingsChanged = true;
            if (ImGui::Checkbox("Disable Screen Shake", &m_EditorSettings.disableScreenShake)) settingsChanged = true;
            if (ImGui::Checkbox("Disable FOV Effects", &m_EditorSettings.disableFOVEffects)) settingsChanged = true;
            if (ImGui::Checkbox("Disable Flashing Lights", &m_EditorSettings.disableFlashingLights)) settingsChanged = true;
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Disables film grain, CRT scanlines, and VHS effects that can trigger photosensitive reactions");
            }
            ImGui::TreePop();
        }

        // -- Subtitles / Cognitive --
        if (ImGui::TreeNode("Cognitive")) {
            if (ImGui::Checkbox("Subtitles", &m_EditorSettings.subtitlesEnabled)) settingsChanged = true;
            if (ImGui::Checkbox("Closed Captions", &m_EditorSettings.closedCaptionsEnabled)) settingsChanged = true;

            if (m_EditorSettings.subtitlesEnabled || m_EditorSettings.closedCaptionsEnabled) {
                if (ImGui::SliderFloat("Subtitle Size", &m_EditorSettings.subtitleFontSize, 16.0f, 48.0f, "%.0f")) settingsChanged = true;
                if (ImGui::SliderFloat("Background Opacity", &m_EditorSettings.subtitleBgOpacity, 0.0f, 1.0f)) settingsChanged = true;
                if (ImGui::Checkbox("Show Speaker Names", &m_EditorSettings.subtitleSpeakerNames)) settingsChanged = true;
            }

            ImGui::Separator();
            if (ImGui::Checkbox("Simplified Editor", &m_EditorSettings.simplifiedEditor)) settingsChanged = true;
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Hides advanced panels and collapses complex inspector sections");
            }

            ImGui::Separator();
            if (ImGui::SliderFloat("Game Font Scale", &m_EditorSettings.gameFontScale, 0.5f, 3.0f, "%.1fx")) {
                settingsChanged = true;
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Scales in-game UI text size during Play mode and in built games");
            }

            ImGui::Separator();
            if (ImGui::Checkbox("Dyslexia-Friendly Mode", &m_EditorSettings.dyslexiaFontEnabled)) {
                settingsChanged = true;
                // Apply dyslexia-friendly spacing immediately
                ImGuiStyle& style = ImGui::GetStyle();
                if (m_EditorSettings.dyslexiaFontEnabled) {
                    style.ItemSpacing.y = std::max(style.ItemSpacing.y, 6.0f);
                    style.FramePadding.y = std::max(style.FramePadding.y, 5.0f);
                } else {
                    // Re-apply theme to reset spacing to defaults
                    m_ImGuiLayer->ApplyTheme(m_EditorSettings.theme, &m_EditorSettings.accentColors);
                    m_ImGuiLayer->SetGlobalScale(m_EditorSettings.uiScale);
                }
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Increases letter, word, and line spacing for improved readability");
            }

            ImGui::TreePop();
        }

        // -- Audio Visual Indicators --
        if (ImGui::TreeNode("Audio Indicators")) {
            auto& aiConfig = m_AudioIndicators.GetConfig();
            if (ImGui::Checkbox("Enable Visual Audio Indicators", &aiConfig.enabled)) {
                settingsChanged = true;
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Show colored dots on screen when audio events occur\n(errors, notifications, playing audio sources)");
            }

            if (aiConfig.enabled) {
                if (ImGui::SliderFloat("Indicator Size", &aiConfig.indicatorSize, 6.0f, 24.0f, "%.0f px")) {
                    settingsChanged = true;
                }
                if (ImGui::Checkbox("Show Labels", &aiConfig.showLabels)) {
                    settingsChanged = true;
                }

                ImGui::Separator();
                if (ImGui::Button("Test Indicator")) {
                    m_AudioIndicators.ShowIndicator("Test Sound", Math::Vector3(0.4f, 0.8f, 0.4f), 2.0f);
                }
                ImGui::SameLine();
                if (ImGui::Button("Test Error")) {
                    m_AudioIndicators.ShowIndicator("Error", Math::Vector3(0.9f, 0.3f, 0.3f), 2.0f);
                }
            }

            ImGui::TreePop();
        }

        // -- Screen Reader / Announcer --
        if (ImGui::TreeNode("Screen Reader")) {
            if (ImGui::Checkbox("Status Announcements", &m_Announcer.enabled)) {
                settingsChanged = true;
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip(
                    "Show a status bar at the bottom of the screen\n"
                    "announcing actions (entity selected, deleted, mode changes).\n"
                    "Groundwork for future OS screen reader API integration.");
            }

            if (ImGui::Checkbox("Log to Console", &m_Announcer.logToConsole)) {
                settingsChanged = true;
            }

            if (ImGui::SliderFloat("Display Duration", &m_Announcer.displayDuration, 1.0f, 10.0f, "%.1f s")) {
                settingsChanged = true;
            }

            if (ImGui::Button("Test Announcement")) {
                m_Announcer.Announce("This is a test accessibility announcement", Accessibility::AnnouncePriority::Normal);
            }

            ImGui::TreePop();
        }

        // -- Alternative Input Devices --
        if (ImGui::TreeNode("Alternative Input")) {
            ImGui::TextWrapped("Support for switch access, eye tracking, and other alternative input devices.");
            ImGui::Separator();

            // Switch Access
            auto switchCfg = m_AlternativeInput.GetSwitchConfig();
            bool switchChanged = false;
            if (ImGui::Checkbox("Switch Access", &switchCfg.enabled)) switchChanged = true;
            if (switchCfg.enabled) {
                if (ImGui::Checkbox("Scanning Mode", &switchCfg.scanningMode)) switchChanged = true;
                if (ImGui::SliderFloat("Scan Speed", &switchCfg.scanSpeed, 0.5f, 5.0f, "%.1f s")) switchChanged = true;
                if (ImGui::SliderInt("Switches", &switchCfg.switchCount, 1, 4)) switchChanged = true;
                if (ImGui::Checkbox("Auto Reverse", &switchCfg.autoScanReverse)) switchChanged = true;
            }
            if (switchChanged) {
                m_AlternativeInput.SetSwitchConfig(switchCfg);
                settingsChanged = true;
            }
            ImGui::Separator();

            // Eye Tracking
            auto eyeCfg = m_AlternativeInput.GetEyeTrackingConfig();
            bool eyeChanged = false;
            if (ImGui::Checkbox("Eye Tracking", &eyeCfg.enabled)) eyeChanged = true;
            if (eyeCfg.enabled) {
                if (ImGui::SliderFloat("Dwell Time", &eyeCfg.dwellTime, 0.3f, 3.0f, "%.1f s")) eyeChanged = true;
                if (ImGui::SliderFloat("Smoothing", &eyeCfg.smoothingFactor, 0.0f, 1.0f)) eyeChanged = true;
                if (ImGui::SliderFloat("Dead Zone", &eyeCfg.deadZone, 0.0f, 20.0f, "%.0f px")) eyeChanged = true;
                if (ImGui::Checkbox("Show Gaze Indicator", &eyeCfg.showGazeIndicator)) eyeChanged = true;
            }
            if (eyeChanged) {
                m_AlternativeInput.SetEyeTrackingConfig(eyeCfg);
                settingsChanged = true;
            }
            ImGui::Separator();

            // Head Tracking
            auto headCfg = m_AlternativeInput.GetHeadTrackingConfig();
            bool headChanged = false;
            if (ImGui::Checkbox("Head Tracking", &headCfg.enabled)) headChanged = true;
            if (headCfg.enabled) {
                if (ImGui::SliderFloat("Sensitivity##Head", &headCfg.sensitivity, 0.1f, 5.0f)) headChanged = true;
                if (ImGui::SliderFloat("Smoothing##Head", &headCfg.smoothing, 0.0f, 1.0f)) headChanged = true;
                if (ImGui::Checkbox("Invert X", &headCfg.invertX)) headChanged = true;
                ImGui::SameLine();
                if (ImGui::Checkbox("Invert Y", &headCfg.invertY)) headChanged = true;
            }
            if (headChanged) {
                m_AlternativeInput.SetHeadTrackingConfig(headCfg);
                settingsChanged = true;
            }
            ImGui::Separator();

            // Sip and Puff
            auto sipCfg = m_AlternativeInput.GetSipAndPuffConfig();
            bool sipChanged = false;
            if (ImGui::Checkbox("Sip and Puff", &sipCfg.enabled)) sipChanged = true;
            if (sipCfg.enabled) {
                if (ImGui::SliderFloat("Sip Threshold", &sipCfg.sipThreshold, 0.1f, 0.9f)) sipChanged = true;
                if (ImGui::SliderFloat("Puff Threshold", &sipCfg.puffThreshold, 0.1f, 0.9f)) sipChanged = true;
            }
            if (sipChanged) {
                m_AlternativeInput.SetSipAndPuffConfig(sipCfg);
                settingsChanged = true;
            }

            ImGui::TextDisabled("Alternative input devices require external driver software.\n"
                                "These settings configure the engine's response to input events.");
            ImGui::TreePop();
        }

        // -- Command Palette --
        if (ImGui::TreeNode("Command Palette")) {
            ImGui::Text("Press Ctrl+P to open the command palette.");
            ImGui::TextDisabled("Provides keyboard-driven access to all editor commands\n"
                                "with fuzzy search. Useful for screen reader workflows.");
            if (ImGui::Button("Open Command Palette")) {
                m_CommandPalette.Open();
            }
            ImGui::Text("Registered commands: %zu", m_CommandPalette.GetFilteredCount());
            ImGui::TreePop();
        }

        // -- Play Mode --
        if (ImGui::TreeNode("Play Mode")) {
            if (ImGui::Checkbox("Auto Focus Mode", &m_EditorSettings.autoFocusMode)) {
                settingsChanged = true;
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Automatically enter fullscreen focus mode when pressing Play");
            }

            if (ImGui::Checkbox("Record Play Sessions", &m_EditorSettings.debugRecordPlay)) {
                settingsChanged = true;
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Record the whole scene during play so you can pause and step or\n"
                                  "scrub backward through time from the toolbar timeline");
            }
            if (m_EditorSettings.debugRecordPlay) {
                if (ImGui::SliderFloat("Recording Buffer (s)", &m_EditorSettings.debugRecordSeconds,
                                       5.0f, 120.0f, "%.0f")) {
                    settingsChanged = true;
                }
            }

            ImGui::TreePop();
        }

        // -- MCP Server --
        if (ImGui::TreeNode("MCP Server")) {
            if (ImGui::Checkbox("Enable MCP Server", &m_EditorSettings.mcpServerEnabled)) {
                settingsChanged = true;
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Let AI assistants drive this editor over localhost HTTP:\n"
                                  "list entities, read/write components, control play mode,\n"
                                  "capture the game view. Localhost only.");
            }
            if (m_EditorSettings.mcpServerEnabled) {
                if (ImGui::InputInt("Port", &m_EditorSettings.mcpServerPort)) {
                    m_EditorSettings.mcpServerPort =
                        std::clamp(m_EditorSettings.mcpServerPort, 1024, 65535);
                    settingsChanged = true;
                }
                if (m_McpServer.IsRunning()) {
                    ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.5f, 1.0f),
                                       "listening on http://127.0.0.1:%u/mcp", m_McpServer.GetPort());
                    ImGui::TextDisabled("connect: claude mcp add --transport http tege http://127.0.0.1:%u/mcp",
                                        m_McpServer.GetPort());
                } else {
                    ImGui::TextDisabled("starting...");
                }
            }
            ImGui::TreePop();
        }

        // -- Workflow --
        if (ImGui::TreeNode("Workflow")) {
            if (ImGui::Checkbox("Enable Drag-and-Drop Import", &m_EditorSettings.enableDragDropImport)) {
                settingsChanged = true;
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Drag model, texture, audio, or scene files onto the editor window to import them");
            }
            ImGui::TreePop();
        }

        // -- Input Accessibility --
        if (ImGui::TreeNode("Input")) {
            const char* holdToggle[] = { "Hold", "Toggle" };

            // These edit the ACTION MAP, which is the same object play mode,
            // the Controls menu and the touch overlay read. There is no second
            // editor-only copy any more.
            int sprintMode = m_InputMap.IsSprintToggle() ? 1 : 0;
            if (ImGui::Combo("Sprint Mode", &sprintMode, holdToggle, 2)) {
                m_InputMap.SetSprintToggle(sprintMode == 1);
            }

            int crouchMode = m_InputMap.IsCrouchToggle() ? 1 : 0;
            if (ImGui::Combo("Crouch Mode", &crouchMode, holdToggle, 2)) {
                m_InputMap.SetCrouchToggle(crouchMode == 1);
            }

            f32 sensitivity = m_InputMap.GetMouseSensitivity();
            if (ImGui::SliderFloat("Mouse Sensitivity", &sensitivity, 0.1f, 3.0f)) {
                m_InputMap.SetMouseSensitivity(sensitivity);
                // The editor fly camera follows the same setting.
                if (m_CameraController) {
                    m_CameraController->SetLookSensitivity(sensitivity * 0.1f);
                }
            }

            bool invertY = m_InputMap.GetInvertY();
            if (ImGui::Checkbox("Invert Look Y", &invertY)) {
                m_InputMap.SetInvertY(invertY);
            }

            if (ImGui::Checkbox("Raw Mouse Input", &m_EditorSettings.rawMouseInput)) {
                settingsChanged = true;
            }

            if (ImGui::SliderFloat("Mouse Smoothing", &m_EditorSettings.mouseSmoothing, 0.0f, 1.0f, "%.2f")) {
                settingsChanged = true;
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("0.0 = no smoothing, 1.0 = heavy smoothing");
            }

            ImGui::Separator();
            ImGui::TextDisabled("Input Presets");
            if (ImGui::Button("Default")) m_InputMap.ResetToDefaults();
            ImGui::SameLine();
            if (ImGui::Button("Left Hand Only")) m_InputMap.ApplyLeftHandOnly();
            ImGui::SameLine();
            if (ImGui::Button("Right Hand Only")) m_InputMap.ApplyRightHandOnly();
            ImGui::SameLine();
            if (ImGui::Button("Gamepad Only")) m_InputMap.ApplyGamepadOnly();

            ImGui::TreePop();
        }

        // -- Motor Accessibility --
        if (ImGui::TreeNode("Motor")) {
            if (ImGui::SliderFloat("Click Threshold", &m_EditorSettings.clickThreshold, 1.0f, 20.0f, "%.0f px")) {
                settingsChanged = true;
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Pixels of mouse movement before a click becomes a drag");
            }

            if (ImGui::SliderFloat("Drag Threshold", &m_EditorSettings.dragThreshold, 1.0f, 30.0f, "%.0f px")) {
                settingsChanged = true;
                ImGui::GetIO().MouseDragThreshold = m_EditorSettings.dragThreshold;
            }

            ImGui::Separator();
            if (ImGui::Checkbox("Dwell Click", &m_EditorSettings.dwellClickEnabled)) {
                settingsChanged = true;
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Automatically click after hovering in one place");
            }
            if (m_EditorSettings.dwellClickEnabled) {
                if (ImGui::SliderFloat("Dwell Delay", &m_EditorSettings.dwellClickDelay, 0.3f, 3.0f, "%.1f s")) {
                    settingsChanged = true;
                }
            }

            ImGui::Separator();
            if (ImGui::Checkbox("Sticky Drag", &m_EditorSettings.stickyDragEnabled)) {
                settingsChanged = true;
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Click once to start dragging, click again to release (no need to hold)");
            }

            ImGui::Separator();
            if (ImGui::SliderFloat("Hold Repeat Delay", &m_EditorSettings.holdRepeatDelay, 0.1f, 1.5f, "%.2f s")) {
                settingsChanged = true;
                ImGui::GetIO().KeyRepeatDelay = m_EditorSettings.holdRepeatDelay;
            }
            if (ImGui::SliderFloat("Hold Repeat Rate", &m_EditorSettings.holdRepeatRate, 0.01f, 0.2f, "%.3f s")) {
                settingsChanged = true;
                ImGui::GetIO().KeyRepeatRate = m_EditorSettings.holdRepeatRate;
            }

            ImGui::TreePop();
        }

        // -- Keyboard Navigation --
        if (ImGui::TreeNode("Keyboard Navigation")) {
            if (ImGui::Checkbox("Enable Keyboard Navigation", &m_EditorSettings.keyboardNavEnabled)) {
                settingsChanged = true;
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip(
                    "Full keyboard-only editor operation:\n"
                    "  Arrow keys: Nudge selected entity\n"
                    "  Ctrl+Arrow: Fine nudge\n"
                    "  PageUp/PageDown: Nudge Y axis\n"
                    "  Ctrl+1-5: Focus panels\n"
                    "  (Hierarchy/Inspector/Viewport/Console/Assets)");
            }

            if (m_EditorSettings.keyboardNavEnabled) {
                if (ImGui::SliderFloat("Nudge Amount", &m_EditorSettings.gizmoNudgeAmount, 0.01f, 10.0f, "%.2f")) {
                    settingsChanged = true;
                }
                if (ImGui::SliderFloat("Fine Nudge", &m_EditorSettings.gizmoNudgeFine, 0.001f, 1.0f, "%.3f")) {
                    settingsChanged = true;
                }
                if (ImGui::SliderFloat("Rotate Nudge", &m_EditorSettings.gizmoRotateNudge, 1.0f, 45.0f, "%.0f deg")) {
                    settingsChanged = true;
                }
            }

            ImGui::TreePop();
        }

        ImGui::Separator();

        // -- Quick Presets --
        ImGui::TextDisabled("Quick Presets");
        if (ImGui::Button("Low Vision")) {
            m_EditorSettings.theme = EditorTheme::HighContrastDark;
            m_EditorSettings.uiScale = 1.5f;
            m_ImGuiLayer->ApplyTheme(m_EditorSettings.theme, &m_EditorSettings.accentColors);
            m_ImGuiLayer->SetGlobalScale(m_EditorSettings.uiScale);
            settingsChanged = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Motor Impaired")) {
            m_InputMap.ApplyGamepadOnly();
            m_InputMap.SetSprintToggle(true);
            m_InputMap.SetCrouchToggle(true);
            m_EditorSettings.clickThreshold = 12.0f;
            m_EditorSettings.dragThreshold = 15.0f;
            m_EditorSettings.dwellClickEnabled = true;
            m_EditorSettings.dwellClickDelay = 1.5f;
            m_EditorSettings.stickyDragEnabled = true;
            m_EditorSettings.keyboardNavEnabled = true;
            settingsChanged = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Photosensitive")) {
            m_EditorSettings.reducedMotion = true;
            m_EditorSettings.disableScreenShake = true;
            m_EditorSettings.disableFOVEffects = true;
            m_EditorSettings.disableFlashingLights = true;
            settingsChanged = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Reset All")) {
            m_EditorSettings = EditorSettings{};
            m_ImGuiLayer->ApplyTheme(m_EditorSettings.theme, &m_EditorSettings.accentColors);
            m_ImGuiLayer->SetGlobalScale(m_EditorSettings.uiScale);
            settingsChanged = true;
        }

        // Auto-save when settings change
        if (settingsChanged) {
            m_EditorSettings.Save();

            // Apply ALL visual accessibility settings to post-processing
            if (m_PostProcessing) {
                auto& ppSettings = m_PostProcessing->GetSettings();
                ppSettings.colorblindMode = m_EditorSettings.colorblindMode;
                ppSettings.colorblindStrength = m_EditorSettings.colorblindStrength;
                ppSettings.brightness = m_EditorSettings.screenBrightness;
                ppSettings.contrast = m_EditorSettings.screenContrast;
                if (m_EditorSettings.disableFlashingLights) {
                    ppSettings.filmGrainEnabled = 0;
                    ppSettings.crtEnabled = 0;
                    ppSettings.vhsEnabled = 0;
                }
            }

            // Re-wire motion settings to PlayMode controller system if active
            auto* ctrlSys = m_PlayMode.GetControllerSystem();
            if (ctrlSys) {
                ctrlSys->SetReducedMotion(m_EditorSettings.reducedMotion);
                ctrlSys->SetDisableScreenShake(m_EditorSettings.disableScreenShake);
                ctrlSys->SetDisableFOVEffects(m_EditorSettings.disableFOVEffects);
            }
            if (m_PlayMode.GetUISystem()) {
                m_PlayMode.GetUISystem()->SetReducedMotion(m_EditorSettings.reducedMotion);
            }

            // Keep runtime accessibility settings in sync for PlayMode/scripting
            SyncRuntimeAccessibility();

            // Apply raw mouse input and smoothing settings
            Input::SetRawMouseInput(m_EditorSettings.rawMouseInput);
            Input::SetMouseSmoothing(m_EditorSettings.mouseSmoothing);
        }
    }
}

void EditorLayer::DrawSettingsSection_Fonts() {
    if (UI::SectionHeader("Fonts")) {
        static char bodyFontPath[512] = "";
        static char headingFontPath[512] = "";
        static char monoFontPath[512] = "";
        static f32 bodySize = 15.0f;
        static f32 headingSize = 20.0f;
        static f32 monoSize = 14.0f;

        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 80);
        ImGui::InputText("##BodyFont", bodyFontPath, sizeof(bodyFontPath));
        ImGui::SameLine();
        if (ImGui::Button("Browse##BodyFont")) {
            std::string path = FileDialog::OpenFile("Select Font", {{ "Font Files", "*.ttf;*.otf" }});
            if (!path.empty()) {
                strncpy(bodyFontPath, path.c_str(), sizeof(bodyFontPath) - 1);
            }
        }
        ImGui::SameLine();
        ImGui::TextDisabled("Body");
        ImGui::DragFloat("Body Size", &bodySize, 0.5f, 8.0f, 48.0f);

        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 80);
        ImGui::InputText("##HeadingFont", headingFontPath, sizeof(headingFontPath));
        ImGui::SameLine();
        if (ImGui::Button("Browse##HeadingFont")) {
            std::string path = FileDialog::OpenFile("Select Font", {{ "Font Files", "*.ttf;*.otf" }});
            if (!path.empty()) {
                strncpy(headingFontPath, path.c_str(), sizeof(headingFontPath) - 1);
            }
        }
        ImGui::SameLine();
        ImGui::TextDisabled("Heading");
        ImGui::DragFloat("Heading Size", &headingSize, 0.5f, 8.0f, 64.0f);

        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 80);
        ImGui::InputText("##MonoFont", monoFontPath, sizeof(monoFontPath));
        ImGui::SameLine();
        if (ImGui::Button("Browse##MonoFont")) {
            std::string path = FileDialog::OpenFile("Select Font", {{ "Font Files", "*.ttf;*.otf" }});
            if (!path.empty()) {
                strncpy(monoFontPath, path.c_str(), sizeof(monoFontPath) - 1);
            }
        }
        ImGui::SameLine();
        ImGui::TextDisabled("Mono");
        ImGui::DragFloat("Mono Size", &monoSize, 0.5f, 8.0f, 48.0f);

        if (ImGui::Button("Reload Fonts")) {
            GUI::EditorFontConfig fontConfig;
            fontConfig.bodyFontPath = bodyFontPath;
            fontConfig.headingFontPath = headingFontPath;
            fontConfig.monoFontPath = monoFontPath;
            fontConfig.bodyFontSize = bodySize;
            fontConfig.headingFontSize = headingSize;
            fontConfig.monoFontSize = monoSize;
            m_ImGuiLayer->ReloadFonts(fontConfig);
        }

        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "Font Library (%zu fonts)", m_FontLibrary.GetCatalog().size());
        ImGui::TextWrapped("Curated OFL/Apache fonts for games. Place .ttf files in the fonts/ directory.");

        static char fontSearchBuf[128] = "";
        ImGui::InputTextWithHint("##FontSearch", "Search fonts...", fontSearchBuf, sizeof(fontSearchBuf));
        static int selectedFontCategory = -1;
        const char* fontCatNames[] = { "All", "Sans-Serif", "Serif", "Monospace", "Display", "Handwriting", "Pixel", "Fantasy", "Sci-Fi" };
        ImGui::Combo("Category##FontLib", &selectedFontCategory, fontCatNames, 9);

        auto displayList = (strlen(fontSearchBuf) > 0)
            ? m_FontLibrary.Search(fontSearchBuf)
            : (selectedFontCategory > 0
                ? m_FontLibrary.GetByCategory(static_cast<Assets::FontCategory>(selectedFontCategory - 1))
                : [&]() {
                    std::vector<const Assets::FontEntry*> all;
                    for (auto& e : m_FontLibrary.GetCatalog()) all.push_back(&e);
                    return all;
                }());

        if (ImGui::BeginChild("FontList", ImVec2(0, 200), true)) {
            for (auto* font : displayList) {
                bool installed = m_FontLibrary.IsFontInstalled(font->id);
                ImGui::PushID(font->id.c_str());
                if (installed)
                    ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "[OK]");
                else
                    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "[--]");
                ImGui::SameLine();
                ImGui::Text("%s", font->name.c_str());
                ImGui::SameLine();
                ImGui::TextDisabled("(%s, %s)", Assets::FontLibrary::GetCategoryName(font->category),
                    Assets::FontLibrary::GetLicenseName(font->license));
                if (ImGui::IsItemHovered()) {
                    ImGui::BeginTooltip();
                    ImGui::Text("%s", font->name.c_str());
                    ImGui::TextWrapped("%s", font->description.c_str());
                    ImGui::Text("Designer: %s", font->designer.c_str());
                    ImGui::Text("License: %s", Assets::FontLibrary::GetLicenseName(font->license));
                    ImGui::Text("Weights: %u | Italic: %s | Bold: %s",
                        font->weightCount, font->hasItalic ? "Yes" : "No", font->hasBold ? "Yes" : "No");
                    if (installed)
                        ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "Installed: %s", m_FontLibrary.GetFontPath(font->id).c_str());
                    else
                        ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f), "Not installed — download from: %s", font->sourceUrl.c_str());
                    ImGui::EndTooltip();
                }
                if (installed) {
                    ImGui::SameLine();
                    std::string btnLabel = "Use as Body##" + font->id;
                    if (ImGui::SmallButton(btnLabel.c_str())) {
                        std::string path = m_FontLibrary.GetFontPath(font->id);
                        strncpy(bodyFontPath, path.c_str(), sizeof(bodyFontPath) - 1);
                    }
                }
                ImGui::PopID();
            }
        }
        ImGui::EndChild();
    }
}

// ── Project settings section drawers ──

void EditorLayer::DrawSettingsSection_ProjectMode() {
    if (UI::SectionHeader("Project", ImGuiTreeNodeFlags_DefaultOpen)) {
        const char* modeNames[] = { "2D", "3D", "Mixed (2.5D)" };
        int currentMode = static_cast<int>(m_SceneManager.GetProjectMode());
        if (ImGui::Combo("Project Mode", &currentMode, modeNames, 3)) {
            m_SceneManager.SetProjectMode(static_cast<Scene::ProjectMode>(currentMode));
            if (!m_SceneManager.GetProjectPath().empty() && !m_SceneManager.SaveProject()) {
                ShowNotification("Failed to save project settings", NotificationType::Error);
            }
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip(
                "2D: Hides 3D-only components (Mesh, 3D controllers, vegetation)\n"
                "3D: Hides 2D-only components (Sprite, Tilemap, 2D controllers)\n"
                "Mixed: Shows all components for 2.5D workflows");
        }

        const char* desc[] = {
            "2D mode: 3D components hidden in Add Component",
            "3D mode: 2D components hidden in Add Component",
            "Mixed mode: All components visible"
        };
        ImGui::TextColored(ImVec4(0.6f, 0.8f, 0.6f, 1.0f), "%s", desc[currentMode]);
    }
}

void EditorLayer::DrawSettingsSection_WindowIcon() {
    if (UI::SectionHeader("Window Icon")) {
        std::string iconPath = m_SceneManager.GetWindowIconPath();
        char iconBuf[256];
        strncpy(iconBuf, iconPath.c_str(), sizeof(iconBuf) - 1);
        iconBuf[sizeof(iconBuf) - 1] = '\0';
        if (ImGui::InputText("Icon Path", iconBuf, sizeof(iconBuf))) {
            m_SceneManager.SetWindowIconPath(iconBuf);
            m_SceneManager.SaveProject();
        }
        ImGui::SameLine();
        if (ImGui::Button("Browse...##icon")) {
            std::vector<FileFilter> filters = {{ "Image Files", "*.png;*.jpg;*.bmp" }};
            std::string path = FileDialog::OpenFile("Select Window Icon", filters);
            if (!path.empty()) {
                m_SceneManager.SetWindowIconPath(path);
                m_SceneManager.SaveProject();
            }
        }
        if (!m_SceneManager.GetWindowIconPath().empty()) {
            ImGui::TextDisabled("Icon: %s", m_SceneManager.GetWindowIconPath().c_str());
            if (ImGui::Button("Apply Icon")) {
                if (m_Window) {
                    m_Window->SetIcon(m_SceneManager.GetWindowIconPath().c_str());
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Clear")) {
                m_SceneManager.SetWindowIconPath("");
                m_SceneManager.SaveProject();
            }
        } else {
            ImGui::TextDisabled("Using default icon (icon.png next to executable)");
        }
    }
}

void EditorLayer::DrawSettingsSection_Physics() {
    if (UI::SectionHeader("Physics", ImGuiTreeNodeFlags_DefaultOpen)) {
        // Physics Backend selection
        {
            const char* backendNames[] = { "Auto", "Jolt (3D)", "Box2D (2D)" };
            int currentBackend = static_cast<int>(m_SceneManager.GetPhysicsBackendType());
            if (currentBackend > 2) currentBackend = 0;
            if (ImGui::Combo("Physics Backend", &currentBackend, backendNames, 3)) {
                m_SceneManager.SetPhysicsBackendType(static_cast<Physics::PhysicsBackendType>(currentBackend));
                if (!m_SceneManager.GetProjectPath().empty() && !m_SceneManager.SaveProject()) {
                    ShowNotification("Failed to save project settings", NotificationType::Error);
                }
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip(
                    "Auto: Selects best backend for project mode\n"
                    "  3D/Mixed -> Jolt Physics\n"
                    "  2D -> Box2D\n"
                    "Jolt: Force Jolt Physics (3D)\n"
                    "Box2D: Force Box2D v3 (2D)");
            }

            // Show resolved backend name
            auto mode = m_SceneManager.GetProjectMode();
            const char* resolved = Physics::ResolveBackendName(
                static_cast<Physics::PhysicsBackendType>(currentBackend), mode);
            ImGui::TextDisabled("Resolves to: %s", resolved);

            // Availability indicators
            ImGui::TextDisabled("Available: Jolt %s | Box2D %s",
                Physics::IsJoltAvailable() ? "[YES]" : "[NO]",
                Physics::IsBox2DAvailable() ? "[YES]" : "[NO]");

            ImGui::Spacing();
        }

        Physics::IPhysicsBackend* physics = m_PlayMode.GetPhysics();
        if (physics) {
            Math::Vector3 gravity = physics->GetGravity();

            f32 grav[3] = { gravity.x, gravity.y, gravity.z };
            if (ImGui::DragFloat3("Global Gravity", grav, 0.1f, -100.0f, 100.0f)) {
                physics->SetGravity(Math::Vector3(grav[0], grav[1], grav[2]));
            }

            // Quick presets
            ImGui::Text("Presets:");
            ImGui::SameLine();
            if (ImGui::SmallButton("Earth")) { physics->SetGravity(Math::Vector3(0.0f, -9.81f, 0.0f)); }
            ImGui::SameLine();
            if (ImGui::SmallButton("Moon")) { physics->SetGravity(Math::Vector3(0.0f, -1.62f, 0.0f)); }
            ImGui::SameLine();
            if (ImGui::SmallButton("Mars")) { physics->SetGravity(Math::Vector3(0.0f, -3.72f, 0.0f)); }
            ImGui::SameLine();
            if (ImGui::SmallButton("Zero")) { physics->SetGravity(Math::Vector3(0.0f, 0.0f, 0.0f)); }

            f32 strength = physics->GetGravity().Length();
            ImGui::TextDisabled("Strength: %.2f m/s^2", strength);

            ImGui::Spacing();
            ImGui::TextDisabled("Use Gravity Zone components for regional overrides");
        } else {
            ImGui::TextDisabled("No physics backend active");
        }
    }
}

void EditorLayer::DrawSettingsSection_FrameRate() {
    if (UI::SectionHeader("Frame Rate")) {
        Scene::GameFrameSettings frameSettings = m_SceneManager.GetGameFrameSettings();
        bool changed = false;

        // Target Frame Rate
        const char* fpsOptions[] = { "Uncapped", "30", "60", "120", "144", "240" };
        int fpsValues[] = { 0, 30, 60, 120, 144, 240 };
        int currentIdx = 0;
        u32 currentVal = static_cast<u32>(frameSettings.targetFrameRate);
        for (int i = 0; i < 6; ++i) {
            if (static_cast<u32>(fpsValues[i]) == currentVal) { currentIdx = i; break; }
        }
        if (ImGui::Combo("Target Frame Rate", &currentIdx, fpsOptions, 6)) {
            frameSettings.targetFrameRate = static_cast<Scene::FrameRateLimit>(fpsValues[currentIdx]);
            changed = true;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Target frame rate for play mode and exported builds");
        }

        if (ImGui::Checkbox("VSync##Game", &frameSettings.vSync)) {
            changed = true;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Synchronize to the monitor refresh rate.\n"
                              "With an Uncapped frame rate, VSync becomes the cap.");
        }

        // Fixed physics timestep (ADR-0005)
        ImGui::Separator();
        if (ImGui::Checkbox("Fixed Physics Timestep", &frameSettings.fixedTimestep)) {
            changed = true;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Physics steps at a constant tick regardless of frame rate\n"
                              "(accumulator + interpolated rendering). Frame-rate-independent\n"
                              "gameplay and the foundation for deterministic replays.\n"
                              "Off = classic per-frame stepping (existing projects' behavior).");
        }
        if (frameSettings.fixedTimestep) {
            int ticks = static_cast<int>(frameSettings.physicsTicksPerSecond);
            ImGui::SetNextItemWidth(120.0f);
            if (ImGui::DragInt("Physics Ticks/Second", &ticks, 1, 15, 240)) {
                frameSettings.physicsTicksPerSecond = static_cast<u32>(ticks < 15 ? 15 : (ticks > 240 ? 240 : ticks));
                changed = true;
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("60 is right for almost everything. Raise for fast\n"
                                  "precision gameplay, lower only for heavy scenes on weak targets.");
            }
        }

        ImGui::Separator();
        ImGui::Text("When Game Loses Focus:");

        // Background behavior radio buttons
        int bgBehavior = static_cast<int>(frameSettings.backgroundBehavior);
        if (ImGui::RadioButton("Run Normally", bgBehavior == 0)) {
            frameSettings.backgroundBehavior = Scene::BackgroundBehavior::RunNormally;
            changed = true;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Continue running at full speed when window is not focused");
        }
        if (ImGui::RadioButton("Reduce to 30 FPS", bgBehavior == 1)) {
            frameSettings.backgroundBehavior = Scene::BackgroundBehavior::ReduceTo30;
            changed = true;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Lower frame rate to 30 FPS when window is not focused");
        }
        if (ImGui::RadioButton("Pause", bgBehavior == 2)) {
            frameSettings.backgroundBehavior = Scene::BackgroundBehavior::Pause;
            changed = true;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Pause the game when window is not focused");
        }

        if (changed) {
            m_SceneManager.SetGameFrameSettings(frameSettings);
            if (!m_SceneManager.GetProjectPath().empty() && !m_SceneManager.SaveProject()) {
                ShowNotification("Failed to save project settings", NotificationType::Error);
            }
        }

        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.8f, 1.0f), "These settings apply to Play Mode and exported builds");
    }
}

void EditorLayer::DrawSettingsSection_Audio() {
    if (UI::SectionHeader("Audio")) {
#ifdef ENJIN_AUDIO_STEAM_AUDIO
        auto* audio = m_PlayMode.GetSimpleAudio();
        if (audio) {
            bool hrtfEnabled = m_SceneManager.GetEnableHRTF();
            if (ImGui::Checkbox("HRTF Binaural Audio (Steam Audio)", &hrtfEnabled)) {
                m_SceneManager.SetEnableHRTF(hrtfEnabled);
                audio->SetHRTFEnabled(hrtfEnabled);
                m_SceneManager.SaveProject();
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip(
                    "Enables physics-based HRTF binaural rendering for 3D sounds.\n"
                    "Best experienced with headphones.");
            }

            if (audio->IsHRTFAvailable()) {
                ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.4f, 1.0f), "Status: Available");
            } else {
                ImGui::TextColored(ImVec4(0.9f, 0.6f, 0.3f, 1.0f), "Status: Init failed");
            }

            // Occlusion checkbox
            bool occlusionEnabled = m_SceneManager.GetEnableOcclusion();
            if (ImGui::Checkbox("Sound Occlusion", &occlusionEnabled)) {
                m_SceneManager.SetEnableOcclusion(occlusionEnabled);
                audio->SetOcclusionEnabled(occlusionEnabled);
                m_SceneManager.SaveProject();
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Sounds are blocked/attenuated by collider geometry");
            }

            // Transmission checkbox (requires occlusion)
            if (!occlusionEnabled) ImGui::BeginDisabled();
            bool transmissionEnabled = m_SceneManager.GetEnableTransmission();
            if (ImGui::Checkbox("Sound Transmission", &transmissionEnabled)) {
                m_SceneManager.SetEnableTransmission(transmissionEnabled);
                audio->SetTransmissionEnabled(transmissionEnabled);
                m_SceneManager.SaveProject();
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip(
                    "Frequency-dependent sound passing through walls.\n"
                    "Requires occlusion to be enabled.");
            }
            if (!occlusionEnabled) ImGui::EndDisabled();
        } else {
            ImGui::TextDisabled("Steam Audio available (start Play Mode to configure)");
        }
#else
        ImGui::TextDisabled("Steam Audio: Not compiled (ENJIN_AUDIO_STEAM_AUDIO=OFF)");
#endif
    }
}

void EditorLayer::DrawSettingsSection_CollisionGroups() {
    if (UI::SectionHeader("Collision Groups")) {
        auto& groupNames = m_SceneManager.GetCollisionGroupNames();

        // Determine visible count: all named groups + 2 blank slots
        int visibleCount = 1;
        for (int i = 1; i < 32; ++i) {
            if (!groupNames[i].empty()) visibleCount = i + 1;
        }
        visibleCount = std::min(visibleCount + 2, 32);

        for (int i = 0; i < visibleCount; ++i) {
            ImGui::PushID(i);
            char label[32];
            snprintf(label, sizeof(label), "Group %d", i);

            if (i == 0) {
                // Group 0 "Default" is read-only
                ImGui::TextDisabled("%s", label);
                ImGui::SameLine();
                ImGui::TextDisabled("Default");
            } else {
                ImGui::Text("%s", label);
                ImGui::SameLine();
                char buf[64];
                strncpy(buf, groupNames[i].c_str(), sizeof(buf) - 1);
                buf[sizeof(buf) - 1] = '\0';
                ImGui::SetNextItemWidth(150.0f);
                if (ImGui::InputText("##name", buf, sizeof(buf))) {
                    groupNames[i] = buf;
                }
            }
            ImGui::PopID();
        }

        ImGui::TextDisabled("Named groups appear as checkboxes on collider components.");
    }
}

void EditorLayer::DrawSettingsSection_Environment() {
    if (UI::SectionHeader("Environment")) {
        // === WORLD TIME / DAY-NIGHT CYCLE ===
        if (ImGui::TreeNode("World Time / Day-Night")) {
            ImGui::Checkbox("Enable World Time", &m_WorldTimeEnabled);

            if (m_WorldTimeEnabled) {
                auto& state = const_cast<Effects::WorldTimeState&>(m_WorldTime.GetState());
                auto& calConfig = m_WorldTime.GetCalendarConfig();
                auto& dayConfig = m_WorldTime.GetDaylightConfig();

                ImGui::Checkbox("Paused", &calConfig.paused);

                f32 timeOfDay = state.timeOfDay;
                if (ImGui::SliderFloat("Time of Day", &timeOfDay, 0.0f, 23.99f, "%.2f h")) {
                    m_WorldTime.SetTime(timeOfDay, state.day, state.month, state.year);
                }

                int day = static_cast<int>(state.day);
                int month = static_cast<int>(state.month);
                int year = static_cast<int>(state.year);
                bool changed = false;
                changed |= ImGui::DragInt("Day", &day, 1, 1, static_cast<int>(calConfig.daysPerMonth));
                changed |= ImGui::DragInt("Month", &month, 1, 1, static_cast<int>(calConfig.monthsPerYear));
                changed |= ImGui::DragInt("Year", &year, 1, 1, 9999);
                if (changed) {
                    m_WorldTime.SetTime(state.timeOfDay, static_cast<u32>(day),
                                       static_cast<u32>(month), static_cast<u32>(year));
                }

                ImGui::DragFloat("Seconds/Game Hour", &calConfig.secondsPerGameHour, 1.0f, 1.0f, 600.0f);

                const char* seasonNames[] = { "Spring", "Summer", "Fall", "Winter" };
                ImGui::Text("Season: %s (%.0f%%)", seasonNames[static_cast<int>(state.season)],
                           m_WorldTime.GetSeasonProgress() * 100.0f);
                ImGui::Text("Daylight: %.1f hours  %s", state.daylightHours, state.isNight ? "[Night]" : "[Day]");
                ImGui::Text("Sun Elevation: %.2f", state.sunElevation);

                ImGui::Separator();
                ImGui::Text("Daylight Config");
                ImGui::DragFloat("Spring Daylight", &dayConfig.springDaylight, 0.1f, 6.0f, 20.0f);
                ImGui::DragFloat("Summer Daylight", &dayConfig.summerDaylight, 0.1f, 6.0f, 22.0f);
                ImGui::DragFloat("Fall Daylight", &dayConfig.fallDaylight, 0.1f, 6.0f, 18.0f);
                ImGui::DragFloat("Winter Daylight", &dayConfig.winterDaylight, 0.1f, 4.0f, 16.0f);

                ImGui::Separator();
                ImGui::Checkbox("Seasonal Weather", &m_SeasonalWeatherEnabled);
                if (m_SeasonalWeatherEnabled) {
                    auto& sConfig = m_SeasonalWeather.GetConfig();
                    ImGui::Text("Temperature: %.1f C", m_SeasonalWeather.GetCurrentTemperature());
                    ImGui::DragFloat("Weather Interval (s)", &sConfig.weatherChangeInterval, 10.0f, 10.0f, 3600.0f);

                    if (ImGui::TreeNode("Temperature Ranges")) {
                        ImGui::DragFloatRange2("Spring", &sConfig.spring.minTemp, &sConfig.spring.maxTemp, 0.5f, -30.0f, 50.0f);
                        ImGui::DragFloatRange2("Summer", &sConfig.summer.minTemp, &sConfig.summer.maxTemp, 0.5f, -30.0f, 50.0f);
                        ImGui::DragFloatRange2("Fall", &sConfig.fall.minTemp, &sConfig.fall.maxTemp, 0.5f, -30.0f, 50.0f);
                        ImGui::DragFloatRange2("Winter", &sConfig.winter.minTemp, &sConfig.winter.maxTemp, 0.5f, -30.0f, 50.0f);
                        ImGui::TreePop();
                    }
                }
            }
            ImGui::TreePop();
        }

        // === WORLD CURVATURE ===
        if (ImGui::TreeNode("World Curvature")) {
            ImGui::Checkbox("Enable Curvature", &m_WorldCurvatureEnabled);
            if (m_WorldCurvatureEnabled) {
                ImGui::DragFloat("Curvature Strength", &m_WorldCurvature, 0.00001f, 0.0f, 0.01f, "%.5f");
                ImGui::TextDisabled("Bends distant geometry downward. Try 0.0001-0.001.");
            }
            ImGui::TreePop();
        }

        // === WEATHER & WATER (Entity-Based Zones) ===
        if (ImGui::TreeNode("Weather & Water")) {
            ImGui::TextWrapped("Weather and Water are entity-based game objects with bounding boxes.");
            ImGui::Spacing();
            ImGui::TextWrapped("Create via Entity > Effects menu, or add components to existing entities.");
            ImGui::Spacing();

            // List weather zones
            const char* weatherTypeNames[] = { "Clear", "Cloudy", "Rain", "Heavy Rain", "Snow", "Fog", "Storm" };
            u32 weatherZoneCount = 0;
            u32 waterVolumeCount = 0;
            if (m_World) {
                if (ImGui::TreeNode("Weather Zones")) {
                    for (ECS::Entity entity : m_World->GetEntitiesWithComponent<ECS::WeatherZoneComponent>()) {
                        weatherZoneCount++;
                        auto* zone = m_World->GetComponent<ECS::WeatherZoneComponent>(entity);
                        auto* name = m_World->GetComponent<ECS::NameComponent>(entity);
                        const char* label = name ? name->name.c_str() : "Unnamed";
                        const char* typeName = (zone->weatherType < 7) ? weatherTypeNames[zone->weatherType] : "Unknown";

                        ImGui::BulletText("%s [%s] (priority: %d)", label, typeName, zone->priority);
                        if (ImGui::IsItemClicked()) {
                            SelectEntity(entity);
                        }
                    }
                    if (weatherZoneCount == 0) {
                        ImGui::TextDisabled("No weather zones in scene");
                    }
                    ImGui::TreePop();
                }

                if (ImGui::TreeNode("Water Volumes")) {
                    for (ECS::Entity entity : m_World->GetEntitiesWithComponent<ECS::WaterVolumeComponent>()) {
                        waterVolumeCount++;
                        auto* volume = m_World->GetComponent<ECS::WaterVolumeComponent>(entity);
                        auto* name = m_World->GetComponent<ECS::NameComponent>(entity);
                        auto* transform = m_World->GetComponent<ECS::TransformComponent>(entity);
                        const char* label = name ? name->name.c_str() : "Unnamed";
                        f32 surfaceY = transform ? transform->position.y : 0.0f;

                        ImGui::BulletText("%s [Y=%.1f] (priority: %d)", label, surfaceY, volume->priority);
                        if (ImGui::IsItemClicked()) {
                            SelectEntity(entity);
                        }
                    }
                    if (waterVolumeCount == 0) {
                        ImGui::TextDisabled("No water volumes in scene");
                    }
                    ImGui::TreePop();
                }
            }

            ImGui::Spacing();
            ImGui::Text("Active Particles: %u / 8000", m_WeatherSystem.GetActiveParticleCount());
            if (m_WeatherSystem.IsLightningActive()) {
                ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.5f, 1.0f), "LIGHTNING ACTIVE!");
            }
            ImGui::TreePop();
        }

        // === WIND ===
        if (ImGui::TreeNode("Wind")) {
            ImGui::TextWrapped("Global wind affects weather particles, vegetation sway, and grass.");
            ImGui::Spacing();

            Effects::WindParams params = m_WindSystem.GetGlobalParams();
            bool changed = false;

            float dir[3] = { params.direction.x, params.direction.y, params.direction.z };
            if (ImGui::DragFloat3("Direction", dir, 0.01f, -1.0f, 1.0f)) {
                params.direction = Math::Vector3(dir[0], dir[1], dir[2]);
                // Normalize if non-zero
                f32 len = params.direction.Length();
                if (len > 0.001f) params.direction = params.direction * (1.0f / len);
                changed = true;
            }
            if (ImGui::DragFloat("Wind Strength", &params.strength, 0.05f, 0.0f, 10.0f)) changed = true;
            if (ImGui::DragFloat("Gust Strength", &params.gustStrength, 0.05f, 0.0f, 5.0f)) changed = true;
            if (ImGui::DragFloat("Gust Frequency", &params.gustFrequency, 0.01f, 0.0f, 2.0f, "%.2f Hz")) changed = true;
            if (ImGui::DragFloat("Turbulence", &params.turbulence, 0.01f, 0.0f, 2.0f)) changed = true;

            if (changed) {
                m_WindSystem.SetGlobalWind(params);
            }

            if (m_WindSystem.HasZoneOverride()) {
                ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.3f, 1.0f), "Zone override active");
            }
            ImGui::TreePop();
        }
    }
}

// ── Wrapper functions ──

// --- Unified Settings Window (3 tabs: System / Project / Scene) ---

void EditorLayer::OpenSettings(int tab) {
    m_SettingsActiveTab = tab;
    // Enable the EditorSettings panel bit to show the unified window
    SetPanelVisibility(EditorPanel::EditorSettings, true);
}

void EditorLayer::MigrateEditorSettingsToProject() {
    bool needsSave = false;

    // Migrate window icon path (empty = not set)
    if (!m_EditorSettings.windowIconPath.empty() && m_SceneManager.GetWindowIconPath().empty()) {
        m_SceneManager.SetWindowIconPath(m_EditorSettings.windowIconPath);
        m_EditorSettings.windowIconPath.clear();
        needsSave = true;
    }

    // Migrate audio settings (only if user disabled something — defaults are all true)
    if (!m_EditorSettings.enableHRTF && m_SceneManager.GetEnableHRTF()) {
        m_SceneManager.SetEnableHRTF(false);
        needsSave = true;
    }
    if (!m_EditorSettings.enableOcclusion && m_SceneManager.GetEnableOcclusion()) {
        m_SceneManager.SetEnableOcclusion(false);
        needsSave = true;
    }
    if (!m_EditorSettings.enableTransmission && m_SceneManager.GetEnableTransmission()) {
        m_SceneManager.SetEnableTransmission(false);
        needsSave = true;
    }

    if (needsSave) {
        m_SceneManager.SaveProject();
        m_EditorSettings.Save();
        ENJIN_LOG_INFO(Editor, "Migrated audio/icon settings from editor_settings.json to .enjinproject");
    }

    // Sync project build config into the runtime m_BuildConfig (so build dialog picks them up)
    if (!m_SceneManager.GetWindowTitle().empty()) {
        m_BuildConfig.windowTitle = m_SceneManager.GetWindowTitle();
    }
    m_BuildConfig.windowWidth = m_SceneManager.GetWindowWidth();
    m_BuildConfig.windowHeight = m_SceneManager.GetWindowHeight();
    m_BuildConfig.fullscreen = m_SceneManager.GetFullscreen();

    m_NetworkConfig.LoadFromFile();
}

void EditorLayer::DrawSettingsWindow() {
    bool open = IsPanelVisible(EditorPanel::EditorSettings);
    ImGui::SetNextWindowSizeConstraints(ImVec2(340, 200), ImVec2(FLT_MAX, FLT_MAX));
    if (!ImGui::Begin("Settings", &open)) {
        ImGui::End();
        if (!open) SetPanelVisibility(EditorPanel::EditorSettings, false);
        return;
    }
    if (!open) SetPanelVisibility(EditorPanel::EditorSettings, false);

    ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x * 0.55f);

    if (ImGui::BeginTabBar("SettingsTabs")) {
        // --- System tab ---
        ImGuiTabItemFlags systemFlags = (m_SettingsActiveTab == 0) ? ImGuiTabItemFlags_SetSelected : 0;
        if (ImGui::BeginTabItem("System", nullptr, systemFlags)) {
            if (m_SettingsActiveTab == 0) m_SettingsActiveTab = -1;  // consume one-shot selection
            ImGui::PushID("System");
            DrawSettingsSection_Camera();
            DrawSettingsSection_EditorPerformance();
            DrawSettingsSection_ExternalIDE();
            DrawSettingsSection_BugReporting();
            DrawSettingsSection_Accessibility();
            DrawSettingsSection_Fonts();
            ImGui::PopID();
            ImGui::EndTabItem();
        }

        // --- Project tab ---
        ImGuiTabItemFlags projectFlags = (m_SettingsActiveTab == 1) ? ImGuiTabItemFlags_SetSelected : 0;
        if (ImGui::BeginTabItem("Project", nullptr, projectFlags)) {
            if (m_SettingsActiveTab == 1) m_SettingsActiveTab = -1;
            ImGui::PushID("Project");
            DrawSettingsSection_ProjectMode();
            DrawSettingsSection_WindowIcon();
            DrawSettingsSection_Physics();
            DrawSettingsSection_FrameRate();
            DrawSettingsSection_Audio();
            DrawSettingsSection_CollisionGroups();
            DrawSettingsSection_BuildScenes();
            DrawSettingsSection_StartupFlow();
            DrawSettingsSection_InputTouch();
            DrawSettingsSection_AccessibilityDefaults();
            DrawSettingsSection_BuildConfig();
            DrawSettingsSection_Networking();
            ImGui::PopID();
            ImGui::EndTabItem();
        }

        // --- Scene tab ---
        ImGuiTabItemFlags sceneFlags = (m_SettingsActiveTab == 2) ? ImGuiTabItemFlags_SetSelected : 0;
        if (ImGui::BeginTabItem("Scene", nullptr, sceneFlags)) {
            if (m_SettingsActiveTab == 2) m_SettingsActiveTab = -1;
            ImGui::PushID("Scene");

            // "Use Project Defaults" toggle at the top
            if (ImGui::Checkbox("Use Project Defaults", &m_CurrentSceneUsesProjectDefaults)) {
                if (m_CurrentSceneUsesProjectDefaults) {
                    m_SceneManager.GetDefaultRenderSettings().ApplyToRuntime(
                        m_RenderSystem, m_PostProcessing ? &m_PostProcessing->GetSettings() : nullptr);
                }
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("When enabled, this scene uses the project's default render settings.\nDisable to customize settings per-scene.");
            }
            ImGui::Separator();

            // --- Setting Conflicts ---
            DrawSettingsConflictWarnings();

            // --- Art Style Preset ---
            DrawSettingsSection_ArtStylePreset();
            ImGui::Separator();

            // --- Content Warnings (accessibility) ---
            // Saved with the scene; the player shows them as a dismissable
            // overlay before gameplay starts.
            ImGui::SeparatorText("Content Warnings");
            ImGui::PushTextWrapPos(); ImGui::TextDisabled("Shown to players before this scene starts (dismissed with any key)"); ImGui::PopTextWrapPos();
            {
                u32 cwFlags = static_cast<u32>(m_SceneContentFlags.flags);
                struct CWEntry { const char* label; u32 bit; };
                static const CWEntry cwEntries[] = {
                    { "Flashing Lights", 1u << 0 }, { "Rapid Motion", 1u << 1 },
                    { "Violence",        1u << 2 }, { "Heights",      1u << 3 },
                    { "Loud Sounds",     1u << 4 }, { "Spiders",      1u << 5 },
                    { "Gore",            1u << 6 }, { "Drowning",     1u << 7 },
                };
                for (int cwI = 0; cwI < 8; ++cwI) {
                    bool on = (cwFlags & cwEntries[cwI].bit) != 0;
                    if ((cwI % 2) == 1) ImGui::SameLine(220.0f);
                    if (ImGui::Checkbox(cwEntries[cwI].label, &on)) {
                        if (on) cwFlags |= cwEntries[cwI].bit;
                        else    cwFlags &= ~cwEntries[cwI].bit;
                        m_SceneContentFlags.flags = static_cast<Accessibility::ContentWarningType>(cwFlags);
                    }
                }

                for (usize cwI = 0; cwI < m_SceneContentFlags.customWarnings.size(); ++cwI) {
                    ImGui::PushID(static_cast<int>(cwI));
                    char cwBuf[256] = {};
                    strncpy(cwBuf, m_SceneContentFlags.customWarnings[cwI].c_str(), sizeof(cwBuf) - 1);
                    ImGui::SetNextItemWidth(260.0f);
                    if (ImGui::InputText("##cwCustom", cwBuf, sizeof(cwBuf))) {
                        m_SceneContentFlags.customWarnings[cwI] = cwBuf;
                    }
                    ImGui::SameLine();
                    if (ImGui::SmallButton("X")) {
                        m_SceneContentFlags.customWarnings.erase(
                            m_SceneContentFlags.customWarnings.begin() + static_cast<std::ptrdiff_t>(cwI));
                        ImGui::PopID();
                        break;
                    }
                    ImGui::PopID();
                }
                if (ImGui::SmallButton("+ Add Custom Warning")) {
                    m_SceneContentFlags.customWarnings.push_back("");
                }
            }
            ImGui::Separator();

            // --- Lighting & Shadows ---
            // Realistic lighting, outdoor scenes, architectural visualization, cinematic mood
            ImGui::SeparatorText("Lighting & Shadows");
            ImGui::PushTextWrapPos(); ImGui::TextDisabled("Realistic lighting, outdoor scenes, cinematic mood"); ImGui::PopTextWrapPos();
            DrawSettingsSection_Skybox();
            DrawSettingsSection_AmbientLighting();
            DrawSettingsSection_ShadingModel();
            DrawSettingsSection_Shadows();
            DrawSettingsSection_LightProbes();

            // --- Ray Tracing ---
            // Photorealistic reflections, soft shadows, global illumination (RTX / RDNA2+ GPU)
            ImGui::SeparatorText("Ray Tracing & Path Tracing");
            ImGui::PushTextWrapPos(); ImGui::TextDisabled("Photorealistic reflections, global illumination, path tracing (RTX/RDNA2+)"); ImGui::PopTextWrapPos();
            ImGui::PushTextWrapPos(); ImGui::TextColored(ImVec4(0.82f, 0.67f, 0.2f, 1.0f), "Experimental in 0.9.7 — expect glitches while it stabilizes"); ImGui::PopTextWrapPos();
            DrawSettingsSection_RayTracing();

            // --- Advanced Rendering (Glacier-inspired) ---
            // Software GI, volumetric fog, GPU particles — scales to all platforms
            ImGui::SeparatorText("Advanced Rendering");
            ImGui::PushTextWrapPos(); ImGui::TextDisabled("Software GI, volumetric fog, GPU particles — scales to all platforms"); ImGui::PopTextWrapPos();

            // DDGI (Software-Traced Global Illumination)
            if (m_RenderSystem && m_RenderSystem->m_DDGISystem) {
                auto& ddgi = *m_RenderSystem->m_DDGISystem;
                auto& cfg = const_cast<Renderer::DDGIConfig&>(ddgi.GetConfig());
                bool ddgiEnabled = ddgi.IsEnabled();
                if (ImGui::Checkbox("Software DDGI (Global Illumination)", &ddgiEnabled)) {
                    ddgi.SetEnabled(ddgiEnabled);
                }
                if (!ddgi.HasGeometryInputs()) {
                    ImGui::SameLine();
                    ImGui::TextDisabled("(no geometry yet)");
                    if (ImGui::IsItemHovered()) {
                        ImGui::SetTooltip("DDGI voxelizes the scene's static 3D meshes into an SDF, traces probes, "
                                          "and the PBR shader samples them directly. It activates once the scene has "
                                          "pool-eligible static geometry — an empty or 2D scene shows this note.");
                    }
                }
                if (ddgiEnabled && ImGui::TreeNode("DDGI Settings")) {
                    ImGui::SliderInt("Probe Grid X", &cfg.probeCountX, 2, 32);
                    ImGui::SliderInt("Probe Grid Y", &cfg.probeCountY, 2, 16);
                    ImGui::SliderInt("Probe Grid Z", &cfg.probeCountZ, 2, 32);
                    ImGui::SliderFloat("Grid Spacing", &cfg.gridSpacing, 0.5f, 16.0f);
                    ImGui::SliderInt("Rays Per Probe", reinterpret_cast<i32*>(&cfg.raysPerProbe), 16, 256);
                    ImGui::SliderFloat("Max Trace Distance", &cfg.maxTraceDistance, 5.0f, 100.0f);
                    ImGui::SliderFloat("Hysteresis", &cfg.hysteresis, 0.8f, 0.99f, "%.3f");
                    int voxRes = cfg.voxelResolution;
                    if (ImGui::Combo("Voxel Resolution", &voxRes, "32\00064\000128\000256\0")) {
                        static const i32 resolutions[] = { 32, 64, 128, 256 };
                        cfg.voxelResolution = resolutions[voxRes];
                    }
                    ImGui::Text("Total probes: %u | Updated/frame: %u",
                                ddgi.GetTotalProbes(), ddgi.GetProbesUpdatedThisFrame());
                    ImGui::TreePop();
                }
            }

            // Volumetric Fog
            if (m_RenderSystem && m_RenderSystem->m_VolumetricFog) {
                auto& fog = *m_RenderSystem->m_VolumetricFog;
                auto& cfg = fog.GetConfig();
                bool fogEnabled = fog.IsEnabled();
                if (ImGui::Checkbox("Volumetric Fog", &fogEnabled)) {
                    fog.SetEnabled(fogEnabled);
                }
                if (fogEnabled && ImGui::TreeNode("Volumetric Fog Settings")) {
                    ImGui::ColorEdit3("Fog Color", &cfg.fogAlbedo.x);
                    ImGui::SliderFloat("Density", &cfg.fogDensity, 0.0f, 0.5f, "%.4f");
                    ImGui::SliderFloat("Height Falloff", &cfg.fogHeightFalloff, 0.0f, 1.0f);
                    ImGui::SliderFloat("Base Height", &cfg.fogBaseHeight, -50.0f, 50.0f);
                    ImGui::SliderFloat("Anisotropy (G)", &cfg.fogAnisotropy, -0.9f, 0.9f);
                    ImGui::SliderFloat("Temporal Blend", &cfg.temporalBlend, 0.0f, 0.98f, "%.3f");
                    ImGui::SliderFloat("Noise Scale", &cfg.noiseScale, 0.0f, 1.0f);
                    ImGui::SliderFloat("Noise Strength", &cfg.noiseStrength, 0.0f, 1.0f);
                    ImGui::SliderFloat("Wind X", &cfg.windSpeedX, -5.0f, 5.0f);
                    ImGui::SliderFloat("Wind Z", &cfg.windSpeedZ, -5.0f, 5.0f);
                    ImGui::TreePop();
                }
            }

            // GPU Particles
            if (m_RenderSystem && m_RenderSystem->m_GPUParticleSystem) {
                auto& gpu = *m_RenderSystem->m_GPUParticleSystem;
                auto& cfg = gpu.GetConfig();
                if (ImGui::TreeNode("GPU Particle System")) {
                    ImGui::TextDisabled("Dormant: no spawn source or render path is wired yet.");
                    if (ImGui::IsItemHovered()) {
                        ImGui::SetTooltip("The compute simulation is complete and correct, but nothing spawns GPU "
                                          "particles and there is no draw path, so the system stays idle (no wasted "
                                          "dispatch). These settings apply once a spawn source and renderer land. "
                                          "The CPU ParticleEmitterComponent path works today.");
                    }
                    ImGui::Text("Max particles: %u | Alive: %u", gpu.GetMaxParticles(), gpu.GetAliveCount());
                    ImGui::SliderFloat("Spawn Rate", &cfg.spawnRate, 0.0f, 10000.0f);
                    ImGui::SliderFloat("Max Lifetime", &cfg.maxLifetime, 0.1f, 30.0f);
                    ImGui::DragFloat3("Gravity", &cfg.gravity.x, 0.1f);
                    ImGui::SliderFloat("Damping", &cfg.damping, 0.0f, 1.0f);
                    ImGui::SliderFloat("Turbulence", &cfg.turbulenceStrength, 0.0f, 10.0f);
                    ImGui::ColorEdit4("Start Color", &cfg.startColor.x);
                    ImGui::ColorEdit4("End Color", &cfg.endColor.x);
                    ImGui::SliderFloat("Start Size", &cfg.startSize, 0.01f, 2.0f);
                    ImGui::SliderFloat("End Size", &cfg.endSize, 0.01f, 5.0f);
                    ImGui::TreePop();
                }
            }

            // --- Post-Processing & Cinematic ---
            // Film looks, color grading, bloom, depth effects — works on all hardware
            ImGui::SeparatorText("Post-Processing & Cinematic");
            ImGui::PushTextWrapPos(); ImGui::TextDisabled("Film looks, color grading, bloom, depth effects — all hardware"); ImGui::PopTextWrapPos();
            DrawSettingsSection_PostProcessing();
            DrawSettingsSection_CelShading();

            // --- Stylized & Retro ---
            // PS1/PS2/N64/Dreamcast aesthetics, pixel art, lo-fi, CRT nostalgia
            ImGui::SeparatorText("Stylized & Retro");
            ImGui::PushTextWrapPos(); ImGui::TextDisabled("PS1/PS2/N64/Dreamcast aesthetics, lo-fi visuals, CRT nostalgia"); ImGui::PopTextWrapPos();
            DrawSettingsSection_RetroEffects();
            DrawSettingsSection_DreamcastEffects();

            // --- Display & Environment ---
            // World settings, weather, time of day, rendering modes
            ImGui::SeparatorText("Display & Environment");
            ImGui::PushTextWrapPos(); ImGui::TextDisabled("World settings, weather, time of day, rendering modes"); ImGui::PopTextWrapPos();
            DrawSettingsSection_DisplayOptions();
            DrawSettingsSection_Environment();
            ImGui::PopID();
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::PopItemWidth();
    ImGui::End();
}

namespace {
    // Curated pick-lists for the project's control defaults. Real GLFW key
    // codes and gamepad button ordinals, so what the panel stores is what the
    // runtime compares against.
    struct KeyChoice { const char* name; Enjin::i32 code; };

    const std::vector<KeyChoice>& KeyChoices() {
        static const std::vector<KeyChoice> choices = {
            {"(none)", -1},
            {"A", 65}, {"B", 66}, {"C", 67}, {"D", 68}, {"E", 69}, {"F", 70},
            {"G", 71}, {"H", 72}, {"I", 73}, {"J", 74}, {"K", 75}, {"L", 76},
            {"M", 77}, {"N", 78}, {"O", 79}, {"P", 80}, {"Q", 81}, {"R", 82},
            {"S", 83}, {"T", 84}, {"U", 85}, {"V", 86}, {"W", 87}, {"X", 88},
            {"Y", 89}, {"Z", 90},
            {"0", 48}, {"1", 49}, {"2", 50}, {"3", 51}, {"4", 52},
            {"5", 53}, {"6", 54}, {"7", 55}, {"8", 56}, {"9", 57},
            {"Space", 32}, {"Tab", 258}, {"Enter", 257}, {"Escape", 256},
            {"Backspace", 259},
            {"Left Shift", 340}, {"Left Ctrl", 341}, {"Left Alt", 342},
            {"Up", 265}, {"Down", 264}, {"Left", 263}, {"Right", 262},
            {"F1", 290}, {"F2", 291}, {"F3", 292}, {"F4", 293},
            {"F5", 294}, {"F6", 295}, {"F7", 296}, {"F8", 297},
            {"F9", 298}, {"F10", 299}, {"F11", 300}, {"F12", 301},
        };
        return choices;
    }

    const std::vector<KeyChoice>& PadChoices() {
        static std::vector<KeyChoice> v = {
            {"(none)", -1}, {"A", 0}, {"B", 1}, {"X", 2}, {"Y", 3},
            {"Left Bumper", 4}, {"Right Bumper", 5}, {"Back", 6}, {"Start", 7},
            {"Left Stick", 9}, {"Right Stick", 10},
            {"D-Pad Up", 11}, {"D-Pad Right", 12}, {"D-Pad Down", 13}, {"D-Pad Left", 14},
        };
        return v;
    }

    // A combo over one of the pick-lists. Returns true when the value changed.
    bool ChoiceCombo(const char* label, const std::vector<KeyChoice>& choices, Enjin::i32& code) {
        int cur = 0;
        for (Enjin::usize i = 0; i < choices.size(); ++i) if (choices[i].code == code) cur = static_cast<int>(i);
        std::vector<const char*> names;
        names.reserve(choices.size());
        for (const auto& c : choices) names.push_back(c.name);
        if (ImGui::Combo(label, &cur, names.data(), static_cast<int>(names.size()))) {
            code = choices[static_cast<Enjin::usize>(cur)].code;
            return true;
        }
        return false;
    }
}

void EditorLayer::DrawSettingsSection_AccessibilityDefaults() {
    if (!UI::SectionHeader("Accessibility Defaults")) return;

    if (m_SceneManager.GetProjectPath().empty()) {
        ImGui::TextDisabled("No project loaded.");
        return;
    }

    ImGui::TextWrapped("The accessibility settings your game STARTS with. Players can still "
                       "change them in the game's own Accessibility menu. Set them up the way "
                       "you want using the editor's Accessibility settings, then save them here.");
    ImGui::Spacing();

    const bool hasDefaults = !m_SceneManager.GetAccessibilityDefaultsJson().empty();
    if (hasDefaults) {
        ImGui::TextColored(ImVec4(0.6f, 0.9f, 0.6f, 1.0f),
                           "This project ships its own accessibility defaults.");
    } else {
        ImGui::TextDisabled("Using engine defaults (nothing accessible turned on).");
    }
    ImGui::Spacing();

    if (ImGui::Button("Save current settings as game defaults")) {
        m_SceneManager.SetAccessibilityDefaultsJson(m_RuntimeAccessibility.ToJson());
        m_SceneManager.SaveProject();
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Copies the editor's current accessibility settings into the project, "
                          "so an exported game starts with them.");
    }
    if (hasDefaults) {
        ImGui::SameLine();
        if (ImGui::Button("Clear")) {
            m_SceneManager.SetAccessibilityDefaultsJson(std::string());
            m_SceneManager.SaveProject();
        }
    }
}

void EditorLayer::DrawSettingsSection_InputTouch() {
    if (!UI::SectionHeader("Input & Touch")) return;

    if (m_SceneManager.GetProjectPath().empty()) {
        ImGui::TextDisabled("No project loaded.");
        return;
    }

    auto& settings = m_SceneManager.GetInputSettings();
    bool changed = false;

    ImGui::TextWrapped("Controls your game adds on top of the built-in ones. Name an action here, "
                       "then wire it up in a scene with an Action Trigger component. Named actions "
                       "show in the player's Controls menu, in the on-screen hint, and as touch "
                       "buttons on mobile.");
    ImGui::Spacing();

    // ---- Custom actions -----------------------------------------------------
    ImGui::TextColored(ImVec4(0.6f, 0.85f, 1.0f, 1.0f), "Your Actions");
    ImGui::Separator();

    if (settings.customActions.empty()) {
        ImGui::TextDisabled("(none yet)");
    }

    int removeIdx = -1;
    for (usize i = 0; i < settings.customActions.size(); ++i) {
        auto& def = settings.customActions[i];
        ImGui::PushID(static_cast<int>(i));

        char nameBuf[64];
        std::snprintf(nameBuf, sizeof(nameBuf), "%s", def.name.c_str());
        ImGui::SetNextItemWidth(160);
        if (ImGui::InputText("##name", nameBuf, sizeof(nameBuf))) { def.name = nameBuf; changed = true; }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(110);
        if (ChoiceCombo("##key", KeyChoices(), def.key)) changed = true;
        ImGui::SameLine();
        ImGui::SetNextItemWidth(130);
        if (ChoiceCombo("##pad", PadChoices(), def.gamepad)) changed = true;
        ImGui::SameLine();
        {
            const char* modeNames[] = { "Hold", "Toggle", "Press", "Release" };
            int m = static_cast<int>(def.mode <= 3 ? def.mode : 2);
            ImGui::SetNextItemWidth(90);
            if (ImGui::Combo("##mode", &m, modeNames, 4)) { def.mode = static_cast<u32>(m); changed = true; }
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("X")) removeIdx = static_cast<int>(i);
        if (def.name.empty()) {
            ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.3f, 1.0f), "  Unnamed actions stay hidden.");
        }
        ImGui::PopID();
    }

    if (removeIdx >= 0) {
        settings.customActions.erase(settings.customActions.begin() + removeIdx);
        changed = true;
    }

    // 8 slots exist; each row occupies one.
    if (settings.customActions.size() < InputSystem::kCustomActionCount) {
        if (ImGui::Button("+ Add Action")) {
            InputSystem::CustomActionDef def;
            // First free slot.
            for (i32 slot = 0; slot < static_cast<i32>(InputSystem::kCustomActionCount); ++slot) {
                bool used = false;
                for (const auto& e : settings.customActions) if (e.slot == slot) used = true;
                if (!used) { def.slot = slot; break; }
            }
            def.name = "New Action";
            settings.customActions.push_back(def);
            changed = true;
        }
    } else {
        ImGui::TextDisabled("All 8 action slots are in use.");
    }

    // ---- Touch --------------------------------------------------------------
    ImGui::Spacing();
    ImGui::TextColored(ImVec4(0.6f, 0.85f, 1.0f, 1.0f), "Touch Controls");
    ImGui::Separator();
    ImGui::TextWrapped("By default the on-screen layout follows the scene: the player controller's "
                       "actions plus any Action Trigger that asks for a button. Preview it with "
                       "View > Simulate Touch Controls while playing.");
    ImGui::Spacing();

    if (ImGui::Checkbox("Left-handed layout", &settings.touchLeftHanded)) changed = true;
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Mirrors the layout: move stick on the right, buttons bottom-left.");

    ImGui::SetNextItemWidth(200);
    if (ImGui::SliderFloat("Button size", &settings.touchButtonScale, 0.5f, 2.0f, "%.2fx")) changed = true;
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Scales every on-screen button, for reach or low vision.");

    {
        const char* lookNames[] = { "Follow the controller", "Always on", "Always off" };
        int look = static_cast<int>(settings.touchLook);
        ImGui::SetNextItemWidth(200);
        if (ImGui::Combo("Camera drag", &look, lookNames, 3)) {
            settings.touchLook = static_cast<InputSystem::TouchLookMode>(look);
            changed = true;
        }
    }

    if (ImGui::Checkbox("Custom button layout", &settings.customTouchLayout)) changed = true;
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Replaces the automatic buttons with the list below.");

    if (settings.customTouchLayout) {
        if (ImGui::Checkbox("Show move stick", &settings.touchStick)) changed = true;

        int removeBtn = -1;
        for (usize i = 0; i < settings.touchButtons.size(); ++i) {
            auto& b = settings.touchButtons[i];
            ImGui::PushID(1000 + static_cast<int>(i));

            std::vector<i32> ids;
            std::vector<const char*> names;
            for (i32 a = 0; a < m_InputMap.GetActionCount(); ++a) {
                if (!m_InputMap.IsActionListed(a)) continue;
                ids.push_back(a);
                names.push_back(m_InputMap.GetActionName(a));
            }
            int cur = -1;
            for (usize k = 0; k < ids.size(); ++k) if (ids[k] == b.action) cur = static_cast<int>(k);
            ImGui::SetNextItemWidth(160);
            if (ImGui::Combo("##act", &cur, names.data(), static_cast<int>(names.size()))) {
                if (cur >= 0 && cur < static_cast<int>(ids.size())) { b.action = ids[cur]; changed = true; }
            }
            ImGui::SameLine(); ImGui::SetNextItemWidth(70);
            if (ImGui::DragFloat("col", &b.col, 0.1f, 0.0f, 4.0f, "%.1f")) changed = true;
            ImGui::SameLine(); ImGui::SetNextItemWidth(70);
            if (ImGui::DragFloat("row", &b.row, 0.1f, 0.0f, 4.0f, "%.1f")) changed = true;
            ImGui::SameLine(); ImGui::SetNextItemWidth(80);
            if (ImGui::DragFloat("size", &b.size, 0.005f, 0.03f, 0.2f, "%.3f")) changed = true;
            ImGui::SameLine();
            if (ImGui::SmallButton("X")) removeBtn = static_cast<int>(i);

            ImGui::PopID();
        }
        if (removeBtn >= 0) {
            settings.touchButtons.erase(settings.touchButtons.begin() + removeBtn);
            changed = true;
        }
        if (settings.touchButtons.size() < static_cast<usize>(Input::kMaxTouchButtons)) {
            if (ImGui::Button("+ Add Touch Button")) {
                InputSystem::TouchButtonLayout b;
                b.row = static_cast<f32>(settings.touchButtons.size());
                settings.touchButtons.push_back(b);
                changed = true;
            }
        } else {
            ImGui::TextDisabled("Touch layouts hold at most %d buttons.", Input::kMaxTouchButtons);
        }
    }

    if (changed) {
        // Take effect in the editor immediately, then persist. Resetting the
        // touch fingerprint forces the overlay to rebuild with the new layout.
        settings.ApplyTo(m_InputMap);   // the editor's ONE map; PlayMode borrows it
        InputSystem::SetTouchProjectSettings(&settings);
        m_SceneManager.SaveProject();
    }
}

void EditorLayer::DrawSettingsSection_StartupFlow() {
    if (!UI::SectionHeader("Startup Flow")) return;

    if (m_SceneManager.GetProjectPath().empty()) {
        ImGui::TextDisabled("No project loaded.");
        return;
    }

    auto& flow = m_SceneManager.GetStartupFlow();
    const auto& scenes = m_SceneManager.GetScenes();

    ImGui::TextWrapped("The boot sequence the exported game runs, top to bottom. "
        "Leave it empty for the default (engine splash, then your start scene behind the "
        "automatic title menu). Or author your own: a splash/intro Scene, the built-in Menu, "
        "then your gameplay Scene. A Scene step plays until its advance condition; a step set "
        "to \"Gameplay\" is where the game lives and never advances.");
    ImGui::Spacing();

    if (flow.empty()) {
        ImGui::TextDisabled("(empty - using the default splash + automatic title menu)");
    }

    bool changed = false;
    int moveFrom = -1, moveTo = -1, removeIdx = -1;
    const char* typeNames[] = { "Scene", "Menu" };
    const char* advNames[]  = { "Gameplay (stay here)", "Timer", "On Input", "Script signal" };

    for (usize i = 0; i < flow.size(); ++i) {
        auto& step = flow[i];
        ImGui::PushID(static_cast<int>(i));
        ImGui::Separator();
        ImGui::Text("%zu.", i + 1); ImGui::SameLine();

        int t = static_cast<int>(step.type);
        ImGui::SetNextItemWidth(80);
        if (ImGui::Combo("##type", &t, typeNames, 2)) { step.type = static_cast<Scene::StartupStepType>(t); changed = true; }

        if (step.type == Scene::StartupStepType::Scene) {
            ImGui::SameLine();
            ImGui::SetNextItemWidth(170);
            std::string cur = step.scene.empty() ? std::string("(pick scene)") : step.scene;
            if (ImGui::BeginCombo("##scene", cur.c_str())) {
                for (const auto& s : scenes) {
                    bool sel = (s.path == step.scene);
                    if (ImGui::Selectable(s.path.c_str(), sel)) { step.scene = s.path; changed = true; }
                }
                ImGui::EndCombo();
            }
            ImGui::SameLine();
            ImGui::SetNextItemWidth(150);
            int a = static_cast<int>(step.advance);
            if (ImGui::Combo("##adv", &a, advNames, 4)) { step.advance = static_cast<Scene::StartupAdvance>(a); changed = true; }
            if (step.advance == Scene::StartupAdvance::Timer) {
                ImGui::SameLine();
                ImGui::SetNextItemWidth(70);
                if (ImGui::DragFloat("s##dur", &step.duration, 0.1f, 0.1f, 60.0f, "%.1f")) changed = true;
            }
        } else {
            ImGui::SameLine();
            ImGui::TextDisabled("built-in title menu (New Game advances)");
        }

        ImGui::SameLine();
        if (ImGui::SmallButton("Up")) { if (i > 0) { moveFrom = static_cast<int>(i); moveTo = static_cast<int>(i) - 1; } }
        ImGui::SameLine();
        if (ImGui::SmallButton("Dn")) { if (i + 1 < flow.size()) { moveFrom = static_cast<int>(i); moveTo = static_cast<int>(i) + 1; } }
        ImGui::SameLine();
        if (ImGui::SmallButton("X")) removeIdx = static_cast<int>(i);

        ImGui::PopID();
    }

    ImGui::Spacing();
    if (ImGui::Button("+ Add Scene Step")) {
        Scene::StartupFlowStep s;
        s.type = Scene::StartupStepType::Scene;
        if (!scenes.empty()) s.scene = scenes.front().path;
        flow.push_back(s); changed = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("+ Add Menu Step")) {
        Scene::StartupFlowStep s; s.type = Scene::StartupStepType::Menu;
        flow.push_back(s); changed = true;
    }

    if (moveFrom >= 0 && moveTo >= 0) { std::swap(flow[static_cast<usize>(moveFrom)], flow[static_cast<usize>(moveTo)]); changed = true; }
    if (removeIdx >= 0) { flow.erase(flow.begin() + removeIdx); changed = true; }

    if (changed) m_SceneManager.SaveProject();
}

void EditorLayer::DrawSettingsSection_BuildScenes() {
    if (!UI::SectionHeader("Build Scenes")) return;

    bool hasProject = !m_SceneManager.GetProjectPath().empty();
    if (!hasProject) {
        ImGui::TextDisabled("No project loaded.");
        return;
    }

    auto& scenes = m_SceneManager.GetScenes();
    namespace fs = std::filesystem;
    fs::path projRoot = fs::path(m_SceneManager.GetProjectPath()).parent_path();

    // --- Scan for .enjin files in the project's scenes/ folder ---
    fs::path scenesDir = projRoot / "scenes";
    std::error_code ec;
    if (fs::exists(scenesDir, ec) && fs::is_directory(scenesDir, ec)) {
        // Check for scene files that are not yet in the project manifest
        for (auto& entry : fs::directory_iterator(scenesDir, ec)) {
            if (!entry.is_regular_file()) continue;
            if (entry.path().extension() != ".enjin") continue;

            std::string relPath = fs::relative(entry.path(), projRoot, ec).generic_string();
            if (ec) continue;

            // Already in the scene list?
            bool found = false;
            for (const auto& s : scenes) {
                // Normalize comparison: both as generic (forward-slash) paths
                std::string existing = fs::path(s.path).generic_string();
                if (existing == relPath) { found = true; break; }
            }
            if (!found) {
                // Auto-add newly discovered scenes (unchecked by default: buildIndex = -1)
                Scene::SceneEntry newEntry;
                newEntry.name = entry.path().stem().string();
                newEntry.path = relPath;
                newEntry.buildIndex = -1;
                newEntry.isStartScene = false;
                m_SceneManager.AddScene(newEntry);
            }
        }
    }

    ImGui::TextDisabled("Check scenes to include in build. First checked scene = start scene.");
    ImGui::TextDisabled("Use arrows to reorder.");
    ImGui::Spacing();

    bool changed = false;
    i32 moveFrom = -1, moveTo = -1;

    for (usize i = 0; i < scenes.size(); ++i) {
        ImGui::PushID(static_cast<int>(i));

        // Included checkbox
        bool included = (scenes[i].buildIndex >= 0);
        if (ImGui::Checkbox("##inc", &included)) {
            scenes[i].buildIndex = included ? static_cast<i32>(i) : -1;
            changed = true;
        }
        ImGui::SameLine();

        // Start scene indicator
        if (scenes[i].isStartScene) {
            ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.4f, 1.0f), "[Start]");
            ImGui::SameLine();
        }

        // Scene name and path
        ImGui::Text("%s", scenes[i].name.c_str());
        ImGui::SameLine();
        ImGui::TextDisabled("(%s)", scenes[i].path.c_str());

        // Reorder buttons
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 60);
        if (i > 0) {
            if (ImGui::SmallButton("Up")) {
                moveFrom = static_cast<i32>(i);
                moveTo = static_cast<i32>(i - 1);
            }
        } else {
            ImGui::Dummy(ImVec2(24, 0));
        }
        ImGui::SameLine();
        if (i < scenes.size() - 1) {
            if (ImGui::SmallButton("Dn")) {
                moveFrom = static_cast<i32>(i);
                moveTo = static_cast<i32>(i + 1);
            }
        } else {
            ImGui::Dummy(ImVec2(24, 0));
        }

        // Remove button
        ImGui::SameLine();
        if (ImGui::SmallButton("X")) {
            m_SceneManager.RemoveScene(i);
            changed = true;
            ImGui::PopID();
            break;  // list invalidated
        }

        ImGui::PopID();
    }

    // Apply move
    if (moveFrom >= 0 && moveTo >= 0) {
        m_SceneManager.MoveScene(static_cast<usize>(moveFrom), static_cast<usize>(moveTo));
        changed = true;
    }

    // "Set as start scene" — first included scene is the start scene
    if (changed) {
        bool startSet = false;
        for (usize i = 0; i < scenes.size(); ++i) {
            if (scenes[i].buildIndex >= 0 && !startSet) {
                scenes[i].isStartScene = true;
                scenes[i].buildIndex = 0;
                startSet = true;
            } else {
                scenes[i].isStartScene = false;
                if (scenes[i].buildIndex >= 0) {
                    scenes[i].buildIndex = static_cast<i32>(i);
                }
            }
        }
        m_SceneManager.SaveProject();
    }

    ImGui::Spacing();
}

void EditorLayer::DrawSettingsSection_BuildConfig() {
    if (UI::SectionHeader("Build Config")) {
        bool changed = false;

        // Window title — synced between SceneManager (persisted) and m_BuildConfig (build-time)
        std::string title = m_SceneManager.GetWindowTitle();
        char titleBuf[256];
        strncpy(titleBuf, title.c_str(), sizeof(titleBuf) - 1);
        titleBuf[sizeof(titleBuf) - 1] = '\0';
        if (ImGui::InputText("Window Title", titleBuf, sizeof(titleBuf))) {
            m_SceneManager.SetWindowTitle(titleBuf);
            m_BuildConfig.windowTitle = titleBuf;
            changed = true;
        }

        int w = static_cast<int>(m_SceneManager.GetWindowWidth());
        int h = static_cast<int>(m_SceneManager.GetWindowHeight());
        if (ImGui::DragInt("Window Width", &w, 1, 320, 3840)) {
            m_SceneManager.SetWindowWidth(static_cast<u32>(w));
            m_BuildConfig.windowWidth = static_cast<u32>(w);
            changed = true;
        }
        if (ImGui::DragInt("Window Height", &h, 1, 240, 2160)) {
            m_SceneManager.SetWindowHeight(static_cast<u32>(h));
            m_BuildConfig.windowHeight = static_cast<u32>(h);
            changed = true;
        }

        bool fullscreen = m_SceneManager.GetFullscreen();
        if (ImGui::Checkbox("Fullscreen", &fullscreen)) {
            m_SceneManager.SetFullscreen(fullscreen);
            m_BuildConfig.fullscreen = fullscreen;
            changed = true;
        }

        char outputBuf[512];
        strncpy(outputBuf, m_BuildConfig.outputDir.c_str(), sizeof(outputBuf) - 1);
        outputBuf[sizeof(outputBuf) - 1] = '\0';
        if (ImGui::InputText("Output Directory", outputBuf, sizeof(outputBuf))) {
            m_BuildConfig.outputDir = outputBuf;
        }

        ImGui::Spacing();
        ImGui::TextDisabled("Use File > Build Game to export with these settings.");

        if (changed) {
            m_SceneManager.SaveProject();
        }
    }
}


void EditorLayer::DrawSettingsSection_Networking() {
    if (UI::SectionHeader("Networking")) {
        bool changed = false;

        // --- Connection ---
        ImGui::SeparatorText("Connection");

        int port = static_cast<int>(m_NetworkConfig.port);
        if (ImGui::DragInt("Port", &port, 1, 1024, 65535)) {
            m_NetworkConfig.port = static_cast<u16>(std::clamp(port, 1024, 65535));
            changed = true;
        }

        int maxPlayers = static_cast<int>(m_NetworkConfig.maxPlayers);
        if (ImGui::DragInt("Max Players", &maxPlayers, 1, 2, 64)) {
            m_NetworkConfig.maxPlayers = static_cast<u32>(std::clamp(maxPlayers, 2, 64));
            changed = true;
        }

        char ipBuf[128];
        strncpy(ipBuf, m_NetworkConfig.serverIP.c_str(), sizeof(ipBuf) - 1);
        ipBuf[sizeof(ipBuf) - 1] = '\0';
        if (ImGui::InputText("Server IP", ipBuf, sizeof(ipBuf))) {
            m_NetworkConfig.serverIP = ipBuf;
            changed = true;
        }

        // --- Sync ---
        ImGui::SeparatorText("Sync");

        if (ImGui::DragFloat("Sync Rate (s)", &m_NetworkConfig.syncRate, 0.001f, 0.01f, 1.0f, "%.3f")) {
            m_NetworkConfig.syncRate = std::clamp(m_NetworkConfig.syncRate, 0.01f, 1.0f);
            changed = true;
        }

        if (ImGui::DragFloat("Interpolation Delay (s)", &m_NetworkConfig.interpDelay, 0.001f, 0.0f, 1.0f, "%.3f")) {
            m_NetworkConfig.interpDelay = std::clamp(m_NetworkConfig.interpDelay, 0.0f, 1.0f);
            changed = true;
        }

        if (ImGui::DragFloat("Prediction Correction Speed", &m_NetworkConfig.predictionCorrectionSpeed, 0.1f, 1.0f, 50.0f, "%.1f")) {
            m_NetworkConfig.predictionCorrectionSpeed = std::clamp(m_NetworkConfig.predictionCorrectionSpeed, 1.0f, 50.0f);
            changed = true;
        }

        // --- Rate Limiting ---
        if (ImGui::TreeNode("Rate Limiting")) {
            if (ImGui::DragFloat("Max Packets/sec", &m_NetworkConfig.maxPacketsPerSecond, 1.0f, 10.0f, 1000.0f, "%.0f")) {
                m_NetworkConfig.maxPacketsPerSecond = std::clamp(m_NetworkConfig.maxPacketsPerSecond, 10.0f, 1000.0f);
                changed = true;
            }

            float maxKB = m_NetworkConfig.maxBytesPerSecond / 1024.0f;
            if (ImGui::DragFloat("Max KB/sec", &maxKB, 1.0f, 1.0f, 1024.0f, "%.0f")) {
                maxKB = std::clamp(maxKB, 1.0f, 1024.0f);
                m_NetworkConfig.maxBytesPerSecond = maxKB * 1024.0f;
                changed = true;
            }

            if (ImGui::DragFloat("Burst Packets", &m_NetworkConfig.burstPackets, 1.0f, 5.0f, 500.0f, "%.0f")) {
                m_NetworkConfig.burstPackets = std::clamp(m_NetworkConfig.burstPackets, 5.0f, 500.0f);
                changed = true;
            }

            float burstKB = m_NetworkConfig.burstBytes / 1024.0f;
            if (ImGui::DragFloat("Burst KB", &burstKB, 1.0f, 1.0f, 512.0f, "%.0f")) {
                burstKB = std::clamp(burstKB, 1.0f, 512.0f);
                m_NetworkConfig.burstBytes = burstKB * 1024.0f;
                changed = true;
            }

            ImGui::TreePop();
        }

        // --- Security ---
        if (ImGui::TreeNode("Security")) {
            int maxViol = static_cast<int>(m_NetworkConfig.maxViolations);
            if (ImGui::DragInt("Max Violations", &maxViol, 1, 1, 100)) {
                m_NetworkConfig.maxViolations = static_cast<u32>(std::clamp(maxViol, 1, 100));
                changed = true;
            }

            if (ImGui::DragFloat("Violation Window (s)", &m_NetworkConfig.violationWindowSeconds, 0.1f, 1.0f, 120.0f, "%.1f")) {
                m_NetworkConfig.violationWindowSeconds = std::clamp(m_NetworkConfig.violationWindowSeconds, 1.0f, 120.0f);
                changed = true;
            }

            if (ImGui::DragFloat("Ban Duration (s)", &m_NetworkConfig.banSeconds, 1.0f, 1.0f, 3600.0f, "%.0f")) {
                m_NetworkConfig.banSeconds = std::clamp(m_NetworkConfig.banSeconds, 1.0f, 3600.0f);
                changed = true;
            }

            if (ImGui::Checkbox("Kick on Violation", &m_NetworkConfig.kickOnViolation)) {
                changed = true;
            }

            ImGui::TreePop();
        }

        if (changed) {
            m_NetworkConfig.SaveToFile();
        }
    }
}

void EditorLayer::DrawAccentColorPicker() {
    bool accentChanged = false;
    auto& ac = m_EditorSettings.accentColors;
    bool settingsChanged = false;

    if (ImGui::Checkbox("Use Custom Accent Colors", &ac.useCustom)) {
        accentChanged = true;
    }

    if (ac.useCustom) {
        // Harmony presets — derive all accent colors from a single primary color
        ImGui::TextDisabled("Quick Presets:");
        struct AccentPreset {
            const char* name;
            f32 r, g, b;
        };
        static const AccentPreset presets[] = {
            {"Default Blue",   0.22f, 0.45f, 0.78f},
            {"Warm Orange",    0.80f, 0.45f, 0.15f},
            {"Forest Green",   0.22f, 0.60f, 0.30f},
            {"Royal Purple",   0.50f, 0.25f, 0.70f},
            {"Crimson Red",    0.75f, 0.18f, 0.22f},
            {"Teal",           0.15f, 0.55f, 0.60f},
        };

        for (i32 i = 0; i < 6; ++i) {
            if (i > 0) ImGui::SameLine();
            auto& p = presets[i];

            // Draw small color swatch button
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(p.r, p.g, p.b, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(p.r * 1.2f, p.g * 1.2f, p.b * 1.2f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(p.r * 0.8f, p.g * 0.8f, p.b * 0.8f, 1.0f));
            if (ImGui::Button(p.name, ImVec2(0, 24))) {
                // Auto-derive all accent colors from primary
                ac.button       = {p.r * 0.7f, p.g * 0.7f, p.b * 0.7f, 1.0f};
                ac.buttonHover  = {p.r, p.g, p.b, 1.0f};
                ac.buttonActive = {p.r * 1.15f, p.g * 1.15f, p.b * 1.15f, 1.0f};
                ac.checkMark    = {p.r * 1.1f, p.g * 1.1f, p.b * 1.1f, 1.0f};
                ac.sliderGrab   = {p.r * 0.9f, p.g * 0.9f, p.b * 0.9f, 1.0f};
                ac.sliderGrabActive = {p.r * 1.2f, p.g * 1.2f, p.b * 1.2f, 1.0f};
                ac.resizeGrip   = {p.r * 0.6f, p.g * 0.6f, p.b * 0.6f, 0.5f};
                ac.textSelected = {p.r * 0.7f, p.g * 0.7f, p.b * 0.7f, 0.5f};
                ac.dragDropTarget = {p.r, p.g, p.b, 0.9f};
                ac.tabActive    = {p.r * 0.65f, p.g * 0.65f, p.b * 0.65f, 1.0f};
                ac.tabHovered   = {p.r * 0.85f, p.g * 0.85f, p.b * 0.85f, 1.0f};
                accentChanged = true;
            }
            ImGui::PopStyleColor(3);
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Individual color editors (advanced)
        if (ImGui::TreeNode("Fine-Tune Colors")) {
            if (ImGui::ColorEdit4("Button##accent", &ac.button.r)) accentChanged = true;
            if (ImGui::ColorEdit4("Button Hover##accent", &ac.buttonHover.r)) accentChanged = true;
            if (ImGui::ColorEdit4("Button Active##accent", &ac.buttonActive.r)) accentChanged = true;
            if (ImGui::ColorEdit4("Check Mark##accent", &ac.checkMark.r)) accentChanged = true;
            if (ImGui::ColorEdit4("Slider Grab##accent", &ac.sliderGrab.r)) accentChanged = true;
            if (ImGui::ColorEdit4("Slider Grab Active##accent", &ac.sliderGrabActive.r)) accentChanged = true;
            if (ImGui::ColorEdit4("Resize Grip##accent", &ac.resizeGrip.r)) accentChanged = true;
            if (ImGui::ColorEdit4("Text Selected##accent", &ac.textSelected.r)) accentChanged = true;
            if (ImGui::ColorEdit4("Drag & Drop Target##accent", &ac.dragDropTarget.r)) accentChanged = true;
            if (ImGui::ColorEdit4("Tab Active##accent", &ac.tabActive.r)) accentChanged = true;
            if (ImGui::ColorEdit4("Tab Hovered##accent", &ac.tabHovered.r)) accentChanged = true;
            ImGui::TreePop();
        }

        ImGui::Spacing();
        if (ImGui::Button("Reset to Theme Defaults")) {
            ac = AccentColorConfig::DefaultForTheme(m_EditorSettings.theme);
            ac.useCustom = true;
            accentChanged = true;
        }
    }

    if (accentChanged) {
        m_ImGuiLayer->ApplyTheme(m_EditorSettings.theme, &m_EditorSettings.accentColors);
        settingsChanged = true;
    }

    if (settingsChanged) {
        m_EditorSettings.Save();
    }
}

// ============================================================================
// Theme Preview
// ============================================================================
void EditorLayer::DrawThemePreview() {
    ImVec2 previewSize(250.0f, 160.0f);
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    // Get current ImGui style for preview
    ImGuiStyle& style = ImGui::GetStyle();

    // Background
    ImU32 windowBg = ImGui::GetColorU32(ImGuiCol_WindowBg);
    ImU32 childBg = ImGui::GetColorU32(ImGuiCol_ChildBg);
    ImU32 border = ImGui::GetColorU32(ImGuiCol_Border);
    ImU32 titleBg = ImGui::GetColorU32(ImGuiCol_TitleBgActive);
    ImU32 headerBg = ImGui::GetColorU32(ImGuiCol_Header);
    ImU32 button = ImGui::GetColorU32(ImGuiCol_Button);
    ImU32 buttonHov = ImGui::GetColorU32(ImGuiCol_ButtonHovered);
    ImU32 text = ImGui::GetColorU32(ImGuiCol_Text);
    ImU32 textDis = ImGui::GetColorU32(ImGuiCol_TextDisabled);
    ImU32 sliderGrab = ImGui::GetColorU32(ImGuiCol_SliderGrab);
    ImU32 frameBg = ImGui::GetColorU32(ImGuiCol_FrameBg);
    ImU32 checkMark = ImGui::GetColorU32(ImGuiCol_CheckMark);

    // Main background
    dl->AddRectFilled(pos, ImVec2(pos.x + previewSize.x, pos.y + previewSize.y), windowBg, 4.0f);
    dl->AddRect(pos, ImVec2(pos.x + previewSize.x, pos.y + previewSize.y), border, 4.0f);

    // Title bar
    dl->AddRectFilled(ImVec2(pos.x + 1, pos.y + 1),
                      ImVec2(pos.x + previewSize.x - 1, pos.y + 22), titleBg, 4.0f, ImDrawFlags_RoundCornersTop);
    dl->AddText(ImVec2(pos.x + 8, pos.y + 4), text, "Preview Panel");

    // Simulated hierarchy tree
    f32 y = pos.y + 28;
    dl->AddRectFilled(ImVec2(pos.x + 6, y), ImVec2(pos.x + previewSize.x * 0.45f, y + 16), headerBg, 2.0f);
    dl->AddText(ImVec2(pos.x + 10, y + 1), text, "Scene Root");
    y += 18;
    dl->AddText(ImVec2(pos.x + 22, y + 1), textDis, "Entity 1");
    y += 16;
    dl->AddText(ImVec2(pos.x + 22, y + 1), textDis, "Entity 2");

    // Simulated inspector section (right half)
    f32 rightX = pos.x + previewSize.x * 0.48f;
    y = pos.y + 28;
    dl->AddRectFilled(ImVec2(rightX, y), ImVec2(pos.x + previewSize.x - 6, y + 16), headerBg, 2.0f);
    dl->AddText(ImVec2(rightX + 4, y + 1), text, "Inspector");
    y += 20;

    // Slider mockup
    dl->AddRectFilled(ImVec2(rightX + 4, y + 2), ImVec2(pos.x + previewSize.x - 10, y + 14), frameBg, 2.0f);
    f32 sliderEnd = rightX + 4 + (pos.x + previewSize.x - 10 - rightX - 4) * 0.6f;
    dl->AddRectFilled(ImVec2(rightX + 4, y + 2), ImVec2(sliderEnd, y + 14), sliderGrab, 2.0f);
    y += 20;

    // Button mockups
    dl->AddRectFilled(ImVec2(rightX + 4, y), ImVec2(rightX + 60, y + 18), button, 3.0f);
    dl->AddText(ImVec2(rightX + 12, y + 2), text, "Apply");
    dl->AddRectFilled(ImVec2(rightX + 64, y), ImVec2(rightX + 124, y + 18), buttonHov, 3.0f);
    dl->AddText(ImVec2(rightX + 72, y + 2), text, "Cancel");
    y += 24;

    // Checkbox mockup
    dl->AddRectFilled(ImVec2(rightX + 4, y), ImVec2(rightX + 16, y + 12), frameBg, 2.0f);
    dl->AddText(ImVec2(rightX + 7, y - 1), checkMark, "x");
    dl->AddText(ImVec2(rightX + 20, y), textDis, "Enabled");

    // Bottom status bar
    f32 barY = pos.y + previewSize.y - 18;
    dl->AddRectFilled(ImVec2(pos.x + 1, barY),
                      ImVec2(pos.x + previewSize.x - 1, pos.y + previewSize.y - 1), childBg, 4.0f, ImDrawFlags_RoundCornersBottom);
    dl->AddText(ImVec2(pos.x + 8, barY + 2), textDis, "Ready");

    // Reserve space
    ImGui::Dummy(previewSize);
}

// ============================================================================
// Keyboard Shortcuts Help Modal
// ============================================================================
void EditorLayer::DrawShortcutsHelpModal() {
    ImGui::SetNextWindowSize(ImVec2(520 * m_EditorSettings.uiScale, 480 * m_EditorSettings.uiScale), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Keyboard Shortcuts", &m_ShowShortcutsHelp)) {
        ImGui::End();
        return;
    }

    ImGui::InputTextWithHint("##shortcutsearch", "Search shortcuts...",
                             m_ShortcutSearchBuf, sizeof(m_ShortcutSearchBuf));
    ImGui::Separator();

    struct ShortcutEntry {
        const char* category;
        const char* shortcut;
        const char* description;
    };

    static const ShortcutEntry shortcuts[] = {
        // General
        {"General", "Ctrl+S", "Save scene"},
        {"General", "Ctrl+Z", "Undo"},
        {"General", "Ctrl+Y", "Redo"},
        {"General", "Ctrl+D", "Duplicate selected"},
        {"General", "Delete", "Delete selected entity"},
        {"General", "F", "Focus on selected entity"},
        {"General", "Ctrl+P", "Command Palette"},
        {"General", "Ctrl+Shift+/", "Show this help"},
        // Viewport
        {"Viewport", "1", "Translate gizmo"},
        {"Viewport", "2", "Rotate gizmo"},
        {"Viewport", "3", "Scale gizmo"},
        {"Viewport", "4", "Toggle local/world space"},
        {"Viewport", "W/A/S/D", "Camera movement (hold RMB)"},
        {"Viewport", "Space / E", "Move camera up"},
        {"Viewport", "Q / Ctrl", "Move camera down"},
        {"Viewport", "Shift", "Sprint (faster camera)"},
        {"Viewport", "RMB + Mouse", "Look around"},
        {"Viewport", "MMB", "Orbit (Orbit mode)"},
        // Selection
        {"Selection", "Ctrl+Click", "Toggle entity selection"},
        {"Selection", "Shift+Click", "Range select"},
        {"Selection", "Drag", "Marquee selection"},
        {"Selection", "Escape", "Clear selection"},
        // Play Mode
        {"Play Mode", "Ctrl+P", "Play / Stop"},
        {"Play Mode", "Ctrl+Shift+P", "Pause / Resume"},
        // Editor
        {"Editor", "F1", "Game Debug"},
        {"Editor", "F2", "Engine Debug"},
        {"Editor", "F10", "Toggle input action map"},
        {"Editor", "Ctrl+Shift+/", "Keyboard shortcuts help"},
        {"Editor", "` (Backtick)", "Toggle drop-down console"},
    };

    std::string filter;
    if (m_ShortcutSearchBuf[0] != '\0') {
        filter = m_ShortcutSearchBuf;
        for (auto& c : filter) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }

    const char* lastCategory = nullptr;
    for (auto& sc : shortcuts) {
        // Filter
        if (!filter.empty()) {
            std::string shortcutLower = sc.shortcut;
            std::string descLower = sc.description;
            std::string catLower = sc.category;
            for (auto& c : shortcutLower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            for (auto& c : descLower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            for (auto& c : catLower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (shortcutLower.find(filter) == std::string::npos &&
                descLower.find(filter) == std::string::npos &&
                catLower.find(filter) == std::string::npos) {
                continue;
            }
        }

        // Category header
        if (!lastCategory || std::strcmp(lastCategory, sc.category) != 0) {
            if (lastCategory) ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "%s", sc.category);
            ImGui::Separator();
            lastCategory = sc.category;
        }

        // Shortcut entry
        ImGui::Text("  %-20s", sc.shortcut);
        ImGui::SameLine(200.0f);
        ImGui::TextDisabled("%s", sc.description);
    }

    ImGui::End();
}


} // namespace Editor
} // namespace Enjin
