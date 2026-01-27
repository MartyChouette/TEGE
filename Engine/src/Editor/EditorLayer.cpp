#include "Enjin/Editor/EditorLayer.h"
#include "Enjin/Logging/Log.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Mesh.h"
#include "Enjin/ECS/Components/Light.h"
#include "Enjin/Renderer/MeshFactory.h"
#include "Enjin/Math/Math.h"
#include <imgui.h>
#include <sstream>

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

    // Disable camera controller when UI wants input
    if (m_CameraController) {
        m_CameraController->SetEnabled(!WantsMouseInput());
    }
}

void EditorLayer::Render(VkCommandBuffer commandBuffer) {
    if (!m_ImGuiLayer) {
        return;
    }

    m_ImGuiLayer->BeginFrame();

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

    // Stats overlay
    if (m_ShowStatsOverlay) {
        DrawStatsOverlay();
    }

    // Demo window (for testing)
    if (m_ShowDemoWindow) {
        ImGui::ShowDemoWindow(&m_ShowDemoWindow);
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
                // TODO: New scene
            }
            if (ImGui::MenuItem("Open Scene...", "Ctrl+O")) {
                // TODO: Open scene
            }
            if (ImGui::MenuItem("Save Scene", "Ctrl+S")) {
                // TODO: Save scene
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Exit", "Alt+F4")) {
                // TODO: Exit application
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
            std::stringstream ss;
            ss << "Entity " << entity;

            // Check if entity has a name component (if we add one later)
            std::string name = ss.str();

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

        // TODO: Add material selection, mesh file loading, etc.
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
        ImGui::Checkbox("Cast Shadows", &light->castShadows);
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

} // namespace Editor
} // namespace Enjin
