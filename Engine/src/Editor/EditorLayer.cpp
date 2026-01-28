#include "Enjin/Editor/EditorLayer.h"
#include "Enjin/Editor/ScenePicker.h"
#include "Enjin/Logging/Log.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Mesh.h"
#include "Enjin/ECS/Components/Material.h"
#include "Enjin/ECS/Components/Light.h"
#include "Enjin/ECS/Components/Name.h"
#include "Enjin/Assets/SceneImporter.h"
#include "Enjin/Scene/SceneSerializer.h"
#include "Enjin/Renderer/MeshFactory.h"
#include "Enjin/Platform/Input.h"
#include "Enjin/Math/Math.h"
#include <imgui.h>
#include <ImGuizmo.h>
#include <sstream>
#include <filesystem>

namespace Enjin {
namespace Editor {

EditorLayer::EditorLayer() {
}

EditorLayer::~EditorLayer() {
    Shutdown();
}

bool EditorLayer::Initialize(Window* window, Renderer::VulkanRenderer* renderer) {
    m_Window = window;
    m_Renderer = renderer;

    m_ImGuiLayer = std::make_unique<GUI::ImGuiLayer>();
    if (!m_ImGuiLayer->Initialize(window, renderer)) {
        ENJIN_LOG_ERROR(Editor, "Failed to initialize ImGui layer");
        return false;
    }

    ENJIN_LOG_INFO(Editor, "EditorLayer initialized");
    return true;
}

void EditorLayer::Shutdown() {
    if (m_ImGuiLayer) {
        m_ImGuiLayer->Shutdown();
        m_ImGuiLayer.reset();
    }
}

void EditorLayer::Update(f32 deltaTime) {
    (void)deltaTime;

    // Camera controller handles its own input - only fully disable when typing in a text field
    // The camera controller checks for right-mouse before looking, so it's OK to leave it enabled
    if (m_CameraController) {
        // Only disable when user is typing in a text field or using gizmo
        bool usingGizmo = ImGuizmo::IsUsing();
        m_CameraController->SetEnabled(!WantsKeyboardInput() && !usingGizmo);
    }

    // Gizmo mode shortcuts (only when not typing)
    if (!WantsKeyboardInput()) {
        if (Input::IsKeyPressed(KeyCode::W)) {
            m_GizmoOperation = GizmoOperation::Translate;
        }
        if (Input::IsKeyPressed(KeyCode::E)) {
            m_GizmoOperation = GizmoOperation::Rotate;
        }
        if (Input::IsKeyPressed(KeyCode::R)) {
            m_GizmoOperation = GizmoOperation::Scale;
        }
        if (Input::IsKeyPressed(KeyCode::Q)) {
            // Toggle between local and world space
            m_GizmoSpace = (m_GizmoSpace == GizmoSpace::World) ? GizmoSpace::Local : GizmoSpace::World;
        }
    }

    // Handle viewport picking (left-click to select, but not when using gizmo)
    if (!ImGuizmo::IsOver()) {
        HandleViewportPicking();
    }
}

void EditorLayer::Render(VkCommandBuffer commandBuffer) {
    if (!m_ImGuiLayer) {
        return;
    }

    m_ImGuiLayer->BeginFrame();

    // Initialize ImGuizmo for this frame
    ImGuizmo::BeginFrame();

    // Menu bar
    DrawMenuBar();

    // Panels
    if (HasPanel(m_VisiblePanels, EditorPanel::Hierarchy)) {
        DrawHierarchyPanel();
    }
    if (HasPanel(m_VisiblePanels, EditorPanel::Inspector)) {
        DrawInspectorPanel();
    }
    if (HasPanel(m_VisiblePanels, EditorPanel::Console)) {
        DrawConsolePanel();
    }
    if (HasPanel(m_VisiblePanels, EditorPanel::AssetBrowser)) {
        DrawAssetBrowserPanel();
    }
    if (HasPanel(m_VisiblePanels, EditorPanel::Settings)) {
        DrawSettingsPanel();
    }

    // Draw gizmos for selected entity
    DrawGizmos();

    // Stats overlay
    if (m_ShowStatsOverlay) {
        DrawStatsOverlay();
    }

    // Demo window (for testing)
    if (m_ShowDemoWindow) {
        ImGui::ShowDemoWindow(&m_ShowDemoWindow);
    }

    // Import dialog
    if (m_ShowImportDialog) {
        DrawImportDialog();
    }

    // Scene dialogs
    if (m_ShowOpenSceneDialog) {
        DrawOpenSceneDialog();
    }
    if (m_ShowSaveSceneDialog) {
        DrawSaveSceneDialog();
    }

    m_ImGuiLayer->EndFrame(commandBuffer);
}

void EditorLayer::SetPanelVisibility(EditorPanel panel, bool visible) {
    if (visible) {
        m_VisiblePanels = m_VisiblePanels | panel;
    } else {
        m_VisiblePanels = static_cast<EditorPanel>(
            static_cast<u32>(m_VisiblePanels) & ~static_cast<u32>(panel));
    }
}

bool EditorLayer::IsPanelVisible(EditorPanel panel) const {
    return HasPanel(m_VisiblePanels, panel);
}

bool EditorLayer::WantsKeyboardInput() const {
    return ImGui::GetIO().WantCaptureKeyboard;
}

bool EditorLayer::WantsMouseInput() const {
    return ImGui::GetIO().WantCaptureMouse;
}

void EditorLayer::DrawMenuBar() {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New Scene", "Ctrl+N")) {
                if (m_World) {
                    m_World->Clear();
                    m_SelectedEntity = ECS::INVALID_ENTITY;
                    m_CurrentScenePath.clear();
                    ENJIN_LOG_INFO(Editor, "Created new scene");
                }
            }
            if (ImGui::MenuItem("Open Scene...", "Ctrl+O")) {
                m_ShowOpenSceneDialog = true;
            }
            if (ImGui::MenuItem("Save Scene", "Ctrl+S")) {
                if (!m_CurrentScenePath.empty()) {
                    SaveScene(m_CurrentScenePath);
                } else {
                    m_ShowSaveSceneDialog = true;
                }
            }
            if (ImGui::MenuItem("Save Scene As...", "Ctrl+Shift+S")) {
                m_ShowSaveSceneDialog = true;
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Import Model...", "Ctrl+I")) {
                m_ShowImportDialog = true;
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Exit", "Alt+F4")) {
                // Application exit handled by main loop
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Edit")) {
            if (ImGui::MenuItem("Undo", "Ctrl+Z")) {}
            if (ImGui::MenuItem("Redo", "Ctrl+Y")) {}
            ImGui::Separator();
            if (ImGui::MenuItem("Cut", "Ctrl+X")) {}
            if (ImGui::MenuItem("Copy", "Ctrl+C")) {}
            if (ImGui::MenuItem("Paste", "Ctrl+V")) {}
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View")) {
            bool hierarchy = IsPanelVisible(EditorPanel::Hierarchy);
            bool inspector = IsPanelVisible(EditorPanel::Inspector);
            bool console = IsPanelVisible(EditorPanel::Console);
            bool assets = IsPanelVisible(EditorPanel::AssetBrowser);
            bool settings = IsPanelVisible(EditorPanel::Settings);

            if (ImGui::MenuItem("Hierarchy", nullptr, &hierarchy)) {
                SetPanelVisibility(EditorPanel::Hierarchy, hierarchy);
            }
            if (ImGui::MenuItem("Inspector", nullptr, &inspector)) {
                SetPanelVisibility(EditorPanel::Inspector, inspector);
            }
            if (ImGui::MenuItem("Console", nullptr, &console)) {
                SetPanelVisibility(EditorPanel::Console, console);
            }
            if (ImGui::MenuItem("Asset Browser", nullptr, &assets)) {
                SetPanelVisibility(EditorPanel::AssetBrowser, assets);
            }
            if (ImGui::MenuItem("Settings", nullptr, &settings)) {
                SetPanelVisibility(EditorPanel::Settings, settings);
            }
            ImGui::Separator();
            ImGui::MenuItem("Stats Overlay", nullptr, &m_ShowStatsOverlay);
            ImGui::MenuItem("ImGui Demo", nullptr, &m_ShowDemoWindow);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Entity")) {
            if (ImGui::MenuItem("Create Empty")) {
                if (m_World) {
                    ECS::Entity entity = m_World->CreateEntity();
                    m_World->AddComponent<ECS::TransformComponent>(entity);
                    m_SelectedEntity = entity;
                }
            }
            if (ImGui::BeginMenu("3D Object")) {
                if (ImGui::MenuItem("Cube")) {
                    if (m_World) {
                        ECS::Entity entity = m_World->CreateEntity();
                        m_World->AddComponent<ECS::TransformComponent>(entity);
                        m_World->AddComponent<ECS::MeshComponent>(entity, Renderer::MeshFactory::CreateCube(1.0f));
                        m_SelectedEntity = entity;
                    }
                }
                if (ImGui::MenuItem("Sphere")) {
                    if (m_World) {
                        ECS::Entity entity = m_World->CreateEntity();
                        m_World->AddComponent<ECS::TransformComponent>(entity);
                        m_World->AddComponent<ECS::MeshComponent>(entity, Renderer::MeshFactory::CreateSphere(0.5f));
                        m_SelectedEntity = entity;
                    }
                }
                if (ImGui::MenuItem("Plane")) {
                    if (m_World) {
                        ECS::Entity entity = m_World->CreateEntity();
                        m_World->AddComponent<ECS::TransformComponent>(entity);
                        m_World->AddComponent<ECS::MeshComponent>(entity, Renderer::MeshFactory::CreatePlane(10.0f, 10.0f));
                        m_SelectedEntity = entity;
                    }
                }
                if (ImGui::MenuItem("Cylinder")) {
                    if (m_World) {
                        ECS::Entity entity = m_World->CreateEntity();
                        m_World->AddComponent<ECS::TransformComponent>(entity);
                        m_World->AddComponent<ECS::MeshComponent>(entity, Renderer::MeshFactory::CreateCylinder(0.5f, 1.0f));
                        m_SelectedEntity = entity;
                    }
                }
                if (ImGui::MenuItem("Cone")) {
                    if (m_World) {
                        ECS::Entity entity = m_World->CreateEntity();
                        m_World->AddComponent<ECS::TransformComponent>(entity);
                        m_World->AddComponent<ECS::MeshComponent>(entity, Renderer::MeshFactory::CreateCone(0.5f, 1.0f));
                        m_SelectedEntity = entity;
                    }
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Light")) {
                if (ImGui::MenuItem("Directional Light")) {
                    if (m_World) {
                        ECS::Entity entity = m_World->CreateEntity();
                        auto& transform = m_World->AddComponent<ECS::TransformComponent>(entity);
                        transform.rotation = Math::Quaternion(Math::Vector3(1, 0, 0), Math::Radians(-45.0f));
                        auto& light = m_World->AddComponent<ECS::LightComponent>(entity);
                        light.type = ECS::LightType::Directional;
                        light.intensity = 1.0f;
                        m_SelectedEntity = entity;
                    }
                }
                if (ImGui::MenuItem("Point Light")) {
                    if (m_World) {
                        ECS::Entity entity = m_World->CreateEntity();
                        auto& transform = m_World->AddComponent<ECS::TransformComponent>(entity);
                        transform.position = Math::Vector3(0, 2, 0);
                        auto& light = m_World->AddComponent<ECS::LightComponent>(entity);
                        light.type = ECS::LightType::Point;
                        m_SelectedEntity = entity;
                    }
                }
                if (ImGui::MenuItem("Spot Light")) {
                    if (m_World) {
                        ECS::Entity entity = m_World->CreateEntity();
                        auto& transform = m_World->AddComponent<ECS::TransformComponent>(entity);
                        transform.position = Math::Vector3(0, 3, 0);
                        auto& light = m_World->AddComponent<ECS::LightComponent>(entity);
                        light.type = ECS::LightType::Spot;
                        m_SelectedEntity = entity;
                    }
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Help")) {
            if (ImGui::MenuItem("About Enjin")) {
                // TODO: About dialog
            }
            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }
}

void EditorLayer::DrawHierarchyPanel() {
    ImGui::Begin("Hierarchy");

    if (m_World) {
        // Get all entities
        const auto& entities = m_World->GetAllEntities();

        for (ECS::Entity entity : entities) {
            std::string name;

            // Use name component if available
            if (m_World->HasComponent<ECS::NameComponent>(entity)) {
                name = m_World->GetComponent<ECS::NameComponent>(entity)->name;
            } else {
                std::stringstream ss;
                ss << "Entity " << entity;
                name = ss.str();
            }

            DrawEntityNode(entity, name);
        }

        // Right-click context menu
        if (ImGui::BeginPopupContextWindow("HierarchyContextMenu", ImGuiPopupFlags_NoOpenOverItems)) {
            if (ImGui::MenuItem("Create Empty Entity")) {
                ECS::Entity entity = m_World->CreateEntity();
                m_World->AddComponent<ECS::TransformComponent>(entity);
                m_SelectedEntity = entity;
            }
            ImGui::EndPopup();
        }
    } else {
        ImGui::TextDisabled("No world loaded");
    }

    ImGui::End();
}

void EditorLayer::DrawEntityNode(ECS::Entity entity, const std::string& name) {
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow |
                               ImGuiTreeNodeFlags_SpanAvailWidth |
                               ImGuiTreeNodeFlags_Leaf; // No children for now

    if (entity == m_SelectedEntity) {
        flags |= ImGuiTreeNodeFlags_Selected;
    }

    bool opened = ImGui::TreeNodeEx((void*)(uintptr_t)entity, flags, "%s", name.c_str());

    if (ImGui::IsItemClicked()) {
        m_SelectedEntity = entity;
        if (m_OnEntitySelected) {
            m_OnEntitySelected(entity);
        }
    }

    // Context menu
    if (ImGui::BeginPopupContextItem()) {
        if (ImGui::MenuItem("Delete")) {
            m_World->DestroyEntity(entity);
            if (m_SelectedEntity == entity) {
                m_SelectedEntity = ECS::INVALID_ENTITY;
            }
        }
        if (ImGui::MenuItem("Duplicate")) {
            // TODO: Duplicate entity
        }
        ImGui::EndPopup();
    }

    if (opened) {
        ImGui::TreePop();
    }
}

void EditorLayer::DrawInspectorPanel() {
    ImGui::Begin("Inspector");

    if (m_SelectedEntity != ECS::INVALID_ENTITY && m_World) {
        ImGui::Text("Entity %llu", (unsigned long long)m_SelectedEntity);
        ImGui::Separator();

        // Transform component
        if (m_World->HasComponent<ECS::TransformComponent>(m_SelectedEntity)) {
            DrawTransformComponent(m_SelectedEntity);
        }

        // Mesh component
        if (m_World->HasComponent<ECS::MeshComponent>(m_SelectedEntity)) {
            DrawMeshComponent(m_SelectedEntity);
        }

        // Material component
        if (m_World->HasComponent<ECS::MaterialComponent>(m_SelectedEntity)) {
            DrawMaterialComponent(m_SelectedEntity);
        }

        // Light component
        if (m_World->HasComponent<ECS::LightComponent>(m_SelectedEntity)) {
            DrawLightComponent(m_SelectedEntity);
        }

        ImGui::Separator();

        // Add component button
        if (ImGui::Button("Add Component")) {
            ImGui::OpenPopup("AddComponentPopup");
        }

        if (ImGui::BeginPopup("AddComponentPopup")) {
            if (!m_World->HasComponent<ECS::MeshComponent>(m_SelectedEntity)) {
                if (ImGui::MenuItem("Mesh")) {
                    m_World->AddComponent<ECS::MeshComponent>(m_SelectedEntity);
                }
            }
            if (!m_World->HasComponent<ECS::MaterialComponent>(m_SelectedEntity)) {
                if (ImGui::MenuItem("Material")) {
                    m_World->AddComponent<ECS::MaterialComponent>(m_SelectedEntity);
                }
            }
            if (!m_World->HasComponent<ECS::LightComponent>(m_SelectedEntity)) {
                if (ImGui::MenuItem("Light")) {
                    m_World->AddComponent<ECS::LightComponent>(m_SelectedEntity);
                }
            }
            ImGui::EndPopup();
        }
    } else {
        ImGui::TextDisabled("No entity selected");
    }

    ImGui::End();
}

void EditorLayer::DrawTransformComponent(ECS::Entity entity) {
    if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
        ECS::TransformComponent* transform = m_World->GetComponent<ECS::TransformComponent>(entity);
        if (!transform) return;

        // Position
        f32 pos[3] = { transform->position.x, transform->position.y, transform->position.z };
        if (ImGui::DragFloat3("Position", pos, 0.1f)) {
            transform->position = Math::Vector3(pos[0], pos[1], pos[2]);
        }

        // Rotation (as euler angles for simplicity)
        // TODO: Convert quaternion to euler and back
        f32 rot[3] = { 0, 0, 0 }; // Simplified
        if (ImGui::DragFloat3("Rotation", rot, 1.0f)) {
            // transform->rotation = Math::Quaternion::FromEuler(...)
        }

        // Scale
        f32 scale[3] = { transform->scale.x, transform->scale.y, transform->scale.z };
        if (ImGui::DragFloat3("Scale", scale, 0.1f, 0.001f, 1000.0f)) {
            transform->scale = Math::Vector3(scale[0], scale[1], scale[2]);
        }
    }
}

void EditorLayer::DrawMeshComponent(ECS::Entity entity) {
    if (ImGui::CollapsingHeader("Mesh", ImGuiTreeNodeFlags_DefaultOpen)) {
        ECS::MeshComponent* mesh = m_World->GetComponent<ECS::MeshComponent>(entity);
        if (!mesh) return;

        ImGui::Text("Vertices: %zu", mesh->vertices.size());
        ImGui::Text("Indices: %zu", mesh->indices.size());
    }
}

void EditorLayer::DrawMaterialComponent(ECS::Entity entity) {
    if (ImGui::CollapsingHeader("Material", ImGuiTreeNodeFlags_DefaultOpen)) {
        ECS::MaterialComponent* material = m_World->GetComponent<ECS::MaterialComponent>(entity);
        if (!material) return;

        // Base color
        f32 baseColor[3] = { material->baseColor.x, material->baseColor.y, material->baseColor.z };
        if (ImGui::ColorEdit3("Base Color", baseColor)) {
            material->baseColor = Math::Vector3(baseColor[0], baseColor[1], baseColor[2]);
        }

        // Opacity
        ImGui::DragFloat("Opacity", &material->opacity, 0.01f, 0.0f, 1.0f);

        // PBR properties
        ImGui::DragFloat("Metallic", &material->metallic, 0.01f, 0.0f, 1.0f);
        ImGui::DragFloat("Roughness", &material->roughness, 0.01f, 0.0f, 1.0f);

        // Emission
        f32 emissive[3] = { material->emissiveColor.x, material->emissiveColor.y, material->emissiveColor.z };
        if (ImGui::ColorEdit3("Emissive Color", emissive)) {
            material->emissiveColor = Math::Vector3(emissive[0], emissive[1], emissive[2]);
        }
        ImGui::DragFloat("Emissive Strength", &material->emissiveStrength, 0.1f, 0.0f, 100.0f);

        // Rendering options
        ImGui::Checkbox("Double Sided", &material->doubleSided);
        ImGui::Checkbox("Cast Shadows", &material->castShadows);
        ImGui::Checkbox("Receive Shadows", &material->receiveShadows);

        // Alpha mode
        const char* alphaModes[] = { "Opaque", "Mask", "Blend" };
        int currentMode = static_cast<int>(material->alphaMode);
        if (ImGui::Combo("Alpha Mode", &currentMode, alphaModes, 3)) {
            material->alphaMode = static_cast<ECS::MaterialComponent::AlphaMode>(currentMode);
        }

        if (material->alphaMode == ECS::MaterialComponent::AlphaMode::Mask) {
            ImGui::DragFloat("Alpha Cutoff", &material->alphaCutoff, 0.01f, 0.0f, 1.0f);
        }

        // Texture indices (read-only info for now)
        if (ImGui::TreeNode("Textures")) {
            ImGui::Text("Base Color: %d", material->baseColorTexture);
            ImGui::Text("Normal: %d", material->normalTexture);
            ImGui::Text("Metallic/Roughness: %d", material->metallicRoughnessTexture);
            ImGui::Text("Emissive: %d", material->emissiveTexture);
            ImGui::TreePop();
        }
    }
}

void EditorLayer::DrawLightComponent(ECS::Entity entity) {
    if (ImGui::CollapsingHeader("Light", ImGuiTreeNodeFlags_DefaultOpen)) {
        ECS::LightComponent* light = m_World->GetComponent<ECS::LightComponent>(entity);
        if (!light) return;

        // Light type
        const char* lightTypes[] = { "Directional", "Point", "Spot" };
        int currentType = static_cast<int>(light->type);
        if (ImGui::Combo("Type", &currentType, lightTypes, 3)) {
            light->type = static_cast<ECS::LightType>(currentType);
        }

        // Color
        f32 color[3] = { light->color.x, light->color.y, light->color.z };
        if (ImGui::ColorEdit3("Color", color)) {
            light->color = Math::Vector3(color[0], color[1], color[2]);
        }

        // Intensity
        ImGui::DragFloat("Intensity", &light->intensity, 0.1f, 0.0f, 100.0f);

        // Point/Spot specific
        if (light->type == ECS::LightType::Point || light->type == ECS::LightType::Spot) {
            ImGui::DragFloat("Range", &light->range, 0.5f, 0.1f, 1000.0f);

            if (ImGui::TreeNode("Attenuation")) {
                ImGui::DragFloat("Constant", &light->constantAttenuation, 0.01f, 0.0f, 10.0f);
                ImGui::DragFloat("Linear", &light->linearAttenuation, 0.001f, 0.0f, 1.0f);
                ImGui::DragFloat("Quadratic", &light->quadraticAttenuation, 0.001f, 0.0f, 1.0f);
                ImGui::TreePop();
            }
        }

        // Spot specific
        if (light->type == ECS::LightType::Spot) {
            ImGui::DragFloat("Inner Cone", &light->innerConeAngle, 0.5f, 0.0f, light->outerConeAngle);
            ImGui::DragFloat("Outer Cone", &light->outerConeAngle, 0.5f, light->innerConeAngle, 90.0f);
        }

        // Shadows
        if (ImGui::TreeNode("Shadows")) {
            ImGui::Checkbox("Cast Shadows", &light->castShadows);
            if (light->castShadows) {
                const char* resolutions[] = { "512", "1024", "2048", "4096" };
                int currentRes = 0;
                if (light->shadowMapResolution == 512) currentRes = 0;
                else if (light->shadowMapResolution == 1024) currentRes = 1;
                else if (light->shadowMapResolution == 2048) currentRes = 2;
                else if (light->shadowMapResolution == 4096) currentRes = 3;

                if (ImGui::Combo("Shadow Resolution", &currentRes, resolutions, 4)) {
                    u32 resValues[] = { 512, 1024, 2048, 4096 };
                    light->shadowMapResolution = resValues[currentRes];
                }
            }
            ImGui::TreePop();
        }
    }
}

void EditorLayer::DrawConsolePanel() {
    ImGui::Begin("Console");

    // Console output
    ImGui::BeginChild("ConsoleOutput", ImVec2(0, -ImGui::GetFrameHeightWithSpacing()), true);
    for (const auto& line : m_ConsoleLog) {
        ImGui::TextUnformatted(line.c_str());
    }
    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
        ImGui::SetScrollHereY(1.0f);
    }
    ImGui::EndChild();

    // Input line
    static char inputBuf[256] = "";
    if (ImGui::InputText("##ConsoleInput", inputBuf, sizeof(inputBuf),
                         ImGuiInputTextFlags_EnterReturnsTrue)) {
        if (inputBuf[0] != '\0') {
            m_ConsoleLog.push_back(std::string("> ") + inputBuf);
            // TODO: Execute console command
            inputBuf[0] = '\0';
        }
        ImGui::SetKeyboardFocusHere(-1);
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear")) {
        m_ConsoleLog.clear();
    }

    ImGui::End();
}

void EditorLayer::DrawAssetBrowserPanel() {
    ImGui::Begin("Asset Browser");

    ImGui::TextDisabled("Asset browser not yet implemented");
    // TODO: Show directory tree, file icons, drag & drop support

    ImGui::End();
}

void EditorLayer::DrawSettingsPanel() {
    ImGui::Begin("Settings");

    if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (m_CameraController) {
            f32 moveSpeed = m_CameraController->GetMoveSpeed();
            if (ImGui::DragFloat("Move Speed", &moveSpeed, 0.5f, 0.1f, 100.0f)) {
                m_CameraController->SetMoveSpeed(moveSpeed);
            }

            f32 sensitivity = m_CameraController->GetLookSensitivity();
            if (ImGui::DragFloat("Look Sensitivity", &sensitivity, 0.01f, 0.01f, 1.0f)) {
                m_CameraController->SetLookSensitivity(sensitivity);
            }

            // Camera mode
            const char* modes[] = { "Fly", "Orbit", "First Person" };
            int currentMode = static_cast<int>(m_CameraController->GetMode());
            if (ImGui::Combo("Mode", &currentMode, modes, 3)) {
                m_CameraController->SetMode(static_cast<Renderer::CameraMode>(currentMode));
            }
        }

        if (m_Camera) {
            Math::Vector3 pos = m_Camera->GetPosition();
            ImGui::Text("Position: %.2f, %.2f, %.2f", pos.x, pos.y, pos.z);
        }
    }

    if (ImGui::CollapsingHeader("Gizmos", ImGuiTreeNodeFlags_DefaultOpen)) {
        // Gizmo operation
        const char* operations[] = { "Translate (W)", "Rotate (E)", "Scale (R)" };
        int currentOp = static_cast<int>(m_GizmoOperation);
        if (ImGui::Combo("Operation", &currentOp, operations, 3)) {
            m_GizmoOperation = static_cast<GizmoOperation>(currentOp);
        }

        // Gizmo space
        const char* spaces[] = { "Local", "World" };
        int currentSpace = static_cast<int>(m_GizmoSpace);
        if (ImGui::Combo("Space (Q)", &currentSpace, spaces, 2)) {
            m_GizmoSpace = static_cast<GizmoSpace>(currentSpace);
        }

        // Snap settings
        ImGui::Checkbox("Enable Snap", &m_UseSnap);
        if (m_UseSnap) {
            ImGui::DragFloat("Translate Snap", &m_TranslateSnap, 0.1f, 0.1f, 10.0f);
            ImGui::DragFloat("Rotate Snap", &m_RotateSnap, 1.0f, 1.0f, 90.0f);
            ImGui::DragFloat("Scale Snap", &m_ScaleSnap, 0.01f, 0.01f, 1.0f);
        }
    }

    if (ImGui::CollapsingHeader("Rendering")) {
        // TODO: Rendering settings (MSAA, shadows, etc.)
        ImGui::TextDisabled("Rendering settings not yet implemented");
    }

    ImGui::End();
}

void EditorLayer::DrawStatsOverlay() {
    const float DISTANCE = 10.0f;
    ImGuiIO& io = ImGui::GetIO();

    ImVec2 windowPos = ImVec2(io.DisplaySize.x - DISTANCE, DISTANCE);
    ImVec2 windowPivot = ImVec2(1.0f, 0.0f);
    ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always, windowPivot);
    ImGui::SetNextWindowBgAlpha(0.35f);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration |
                             ImGuiWindowFlags_AlwaysAutoResize |
                             ImGuiWindowFlags_NoSavedSettings |
                             ImGuiWindowFlags_NoFocusOnAppearing |
                             ImGuiWindowFlags_NoNav |
                             ImGuiWindowFlags_NoMove;

    if (ImGui::Begin("Stats", &m_ShowStatsOverlay, flags)) {
        ImGui::Text("FPS: %.1f", io.Framerate);
        ImGui::Text("Frame Time: %.3f ms", 1000.0f / io.Framerate);

        if (m_World) {
            ImGui::Separator();
            ImGui::Text("Entities: %zu", m_World->GetAllEntities().size());
        }

        if (m_CameraController) {
            ImGui::Separator();
            ImGui::Text("Yaw: %.1f", m_CameraController->GetYaw());
            ImGui::Text("Pitch: %.1f", m_CameraController->GetPitch());
        }
    }
    ImGui::End();
}

void EditorLayer::DrawImportDialog() {
    ImGui::OpenPopup("Import Model");

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if (ImGui::BeginPopupModal("Import Model", &m_ShowImportDialog, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Enter the path to a glTF model (.gltf or .glb):");
        ImGui::Separator();

        ImGui::InputText("Path", m_ImportPath, sizeof(m_ImportPath));

        ImGui::Separator();

        if (ImGui::Button("Import", ImVec2(120, 0))) {
            if (m_ImportPath[0] != '\0') {
                ImportModel(m_ImportPath);
                m_ShowImportDialog = false;
                m_ImportPath[0] = '\0';
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            m_ShowImportDialog = false;
            m_ImportPath[0] = '\0';
        }

        ImGui::EndPopup();
    }
}

void EditorLayer::ImportModel(const std::string& path) {
    if (!m_World) {
        ENJIN_LOG_ERROR(Editor, "Cannot import model: no world loaded");
        m_ConsoleLog.push_back("[Error] Cannot import model: no world loaded");
        return;
    }

    Assets::ImportOptions options;
    options.scale = 1.0f;

    Assets::ImportResult result = Assets::SceneImporter::ImportGLTF(path, m_World, options);

    if (result.success) {
        std::stringstream ss;
        ss << "[Info] Imported " << result.entities.size() << " entities from " << path;
        m_ConsoleLog.push_back(ss.str());
        ENJIN_LOG_INFO(Editor, "Imported %zu entities from %s", result.entities.size(), path.c_str());

        // Select the root entity
        if (result.rootEntity != ECS::INVALID_ENTITY) {
            m_SelectedEntity = result.rootEntity;
        }
    } else {
        std::stringstream ss;
        ss << "[Error] Failed to import: " << result.errorMessage;
        m_ConsoleLog.push_back(ss.str());
        ENJIN_LOG_ERROR(Editor, "Failed to import %s: %s", path.c_str(), result.errorMessage.c_str());
    }
}

void EditorLayer::HandleViewportPicking() {
    // Only pick when left mouse is clicked and not interacting with UI
    if (!Input::IsMouseButtonPressed(MouseButton::Left)) {
        return;
    }

    // Don't pick if ImGui wants the mouse (hovering over a panel)
    if (WantsMouseInput()) {
        return;
    }

    if (!m_World || !m_Camera || !m_Renderer) {
        return;
    }

    // Get mouse position and viewport size
    Math::Vector2 mousePos = Input::GetMousePosition();
    auto extent = m_Renderer->GetSwapchainExtent();

    if (extent.width == 0 || extent.height == 0) {
        return;
    }

    // Pick entity at mouse position
    ECS::Entity picked = ScenePicker::PickEntity(
        m_World, m_Camera,
        mousePos.x, mousePos.y,
        static_cast<f32>(extent.width), static_cast<f32>(extent.height)
    );

    if (picked != ECS::INVALID_ENTITY) {
        m_SelectedEntity = picked;
        if (m_OnEntitySelected) {
            m_OnEntitySelected(picked);
        }
        ENJIN_LOG_DEBUG(Editor, "Selected entity %llu", (unsigned long long)picked);
    } else {
        // Clicked on empty space - deselect
        m_SelectedEntity = ECS::INVALID_ENTITY;
    }
}

void EditorLayer::DrawGizmos() {
    if (m_SelectedEntity == ECS::INVALID_ENTITY || !m_World || !m_Camera || !m_Renderer) {
        return;
    }

    // Check if entity has transform
    auto* transform = m_World->GetComponent<ECS::TransformComponent>(m_SelectedEntity);
    if (!transform) {
        return;
    }

    // Get viewport size
    auto extent = m_Renderer->GetSwapchainExtent();
    if (extent.width == 0 || extent.height == 0) {
        return;
    }

    // Set ImGuizmo to use the full screen as the viewport
    ImGuizmo::SetOrthographic(false);
    ImGuizmo::SetDrawlist(ImGui::GetBackgroundDrawList());
    ImGuizmo::SetRect(0, 0, static_cast<f32>(extent.width), static_cast<f32>(extent.height));

    // Get camera matrices (need to convert to float arrays for ImGuizmo)
    Math::Matrix4 viewMat = m_Camera->GetViewMatrix();
    Math::Matrix4 projMat = m_Camera->GetProjectionMatrix();

    // Flip projection for Vulkan (Y is flipped compared to OpenGL)
    projMat.m[5] *= -1.0f;

    // Build entity transform matrix
    Math::Matrix4 entityMat = Math::Matrix4::Translation(transform->position) *
                               transform->rotation.ToMatrix() *
                               Math::Matrix4::Scale(transform->scale);

    // Determine ImGuizmo operation
    ImGuizmo::OPERATION op = ImGuizmo::TRANSLATE;
    switch (m_GizmoOperation) {
        case GizmoOperation::Translate: op = ImGuizmo::TRANSLATE; break;
        case GizmoOperation::Rotate: op = ImGuizmo::ROTATE; break;
        case GizmoOperation::Scale: op = ImGuizmo::SCALE; break;
    }

    // Determine ImGuizmo mode (local/world)
    ImGuizmo::MODE mode = (m_GizmoSpace == GizmoSpace::Local) ? ImGuizmo::LOCAL : ImGuizmo::WORLD;

    // Snap values
    f32 snapValues[3] = { 0.0f, 0.0f, 0.0f };
    if (m_UseSnap) {
        switch (m_GizmoOperation) {
            case GizmoOperation::Translate:
                snapValues[0] = snapValues[1] = snapValues[2] = m_TranslateSnap;
                break;
            case GizmoOperation::Rotate:
                snapValues[0] = snapValues[1] = snapValues[2] = m_RotateSnap;
                break;
            case GizmoOperation::Scale:
                snapValues[0] = snapValues[1] = snapValues[2] = m_ScaleSnap;
                break;
        }
    }

    // Draw and manipulate gizmo
    if (ImGuizmo::Manipulate(viewMat.m, projMat.m, op, mode, entityMat.m,
                              nullptr, m_UseSnap ? snapValues : nullptr)) {
        // Decompose the modified matrix back to transform components
        f32 translation[3], rotation[3], scale[3];
        ImGuizmo::DecomposeMatrixToComponents(entityMat.m, translation, rotation, scale);

        transform->position = Math::Vector3(translation[0], translation[1], translation[2]);
        transform->scale = Math::Vector3(scale[0], scale[1], scale[2]);

        // Convert euler angles to quaternion
        f32 rx = Math::Radians(rotation[0]);
        f32 ry = Math::Radians(rotation[1]);
        f32 rz = Math::Radians(rotation[2]);

        Math::Quaternion qx(Math::Vector3(1, 0, 0), rx);
        Math::Quaternion qy(Math::Vector3(0, 1, 0), ry);
        Math::Quaternion qz(Math::Vector3(0, 0, 1), rz);
        transform->rotation = qy * qx * qz; // YXZ order
    }
}

void EditorLayer::SaveScene(const std::string& path) {
    if (!m_World) {
        ENJIN_LOG_ERROR(Editor, "Cannot save scene: no world loaded");
        m_ConsoleLog.push_back("[Error] Cannot save scene: no world loaded");
        return;
    }

    Scene::SceneSerializer serializer(m_World);
    auto result = serializer.Save(path);

    if (result.success) {
        m_CurrentScenePath = path;
        usize entityCount = m_World->GetAllEntities().size();
        std::stringstream ss;
        ss << "[Info] Saved scene to " << path << " (" << entityCount << " entities)";
        m_ConsoleLog.push_back(ss.str());
        ENJIN_LOG_INFO(Editor, "Saved scene to %s (%zu entities)", path.c_str(), entityCount);
    } else {
        std::stringstream ss;
        ss << "[Error] Failed to save scene: " << result.error;
        m_ConsoleLog.push_back(ss.str());
        ENJIN_LOG_ERROR(Editor, "Failed to save scene to %s: %s", path.c_str(), result.error.c_str());
    }
}

void EditorLayer::OpenScene(const std::string& path) {
    if (!m_World) {
        ENJIN_LOG_ERROR(Editor, "Cannot open scene: no world loaded");
        m_ConsoleLog.push_back("[Error] Cannot open scene: no world loaded");
        return;
    }

    Scene::SceneSerializer serializer(m_World);
    auto result = serializer.Load(path, true); // Clear existing entities

    if (result.success) {
        m_CurrentScenePath = path;
        m_SelectedEntity = ECS::INVALID_ENTITY;
        usize entityCount = result.entities.size();
        std::stringstream ss;
        ss << "[Info] Loaded scene from " << path << " (" << entityCount << " entities)";
        m_ConsoleLog.push_back(ss.str());
        ENJIN_LOG_INFO(Editor, "Loaded scene from %s (%zu entities)", path.c_str(), entityCount);
    } else {
        std::stringstream ss;
        ss << "[Error] Failed to load scene: " << result.error;
        m_ConsoleLog.push_back(ss.str());
        ENJIN_LOG_ERROR(Editor, "Failed to load scene from %s: %s", path.c_str(), result.error.c_str());
    }
}

void EditorLayer::DrawOpenSceneDialog() {
    ImGui::OpenPopup("Open Scene");

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if (ImGui::BeginPopupModal("Open Scene", &m_ShowOpenSceneDialog, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Enter the path to a scene file (.enjin):");
        ImGui::Separator();

        ImGui::InputText("Path", m_ScenePath, sizeof(m_ScenePath));

        ImGui::Separator();

        if (ImGui::Button("Open", ImVec2(120, 0))) {
            if (m_ScenePath[0] != '\0') {
                OpenScene(m_ScenePath);
                m_ShowOpenSceneDialog = false;
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            m_ShowOpenSceneDialog = false;
        }

        ImGui::EndPopup();
    }
}

void EditorLayer::DrawSaveSceneDialog() {
    ImGui::OpenPopup("Save Scene As");

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if (ImGui::BeginPopupModal("Save Scene As", &m_ShowSaveSceneDialog, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Enter the path to save the scene:");
        ImGui::Separator();

        // Pre-fill with current path if available
        static bool initialized = false;
        if (!initialized && !m_CurrentScenePath.empty()) {
            strncpy(m_ScenePath, m_CurrentScenePath.c_str(), sizeof(m_ScenePath) - 1);
            m_ScenePath[sizeof(m_ScenePath) - 1] = '\0';
            initialized = true;
        }

        ImGui::InputText("Path", m_ScenePath, sizeof(m_ScenePath));

        ImGui::TextDisabled("(Use .enjin extension for scene files)");

        ImGui::Separator();

        if (ImGui::Button("Save", ImVec2(120, 0))) {
            if (m_ScenePath[0] != '\0') {
                std::string path = m_ScenePath;
                // Add .enjin extension if not present
                if (path.find(".enjin") == std::string::npos) {
                    path += ".enjin";
                }
                SaveScene(path);
                m_ShowSaveSceneDialog = false;
                initialized = false;
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            m_ShowSaveSceneDialog = false;
            initialized = false;
        }

        ImGui::EndPopup();
    }
}

} // namespace Editor
} // namespace Enjin
