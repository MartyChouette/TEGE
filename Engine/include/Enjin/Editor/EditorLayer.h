#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Window.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Entity.h"
#include "Enjin/Renderer/Vulkan/VulkanRenderer.h"
#include "Enjin/Renderer/Camera.h"
#include "Enjin/Renderer/CameraController.h"
#include "Enjin/GUI/ImGuiLayer.h"
#include <string>
#include <functional>

namespace Enjin {
namespace Editor {

// Editor panel flags
enum class EditorPanel : u32 {
    None = 0,
    Hierarchy = 1 << 0,
    Inspector = 1 << 1,
    Viewport = 1 << 2,
    Console = 1 << 3,
    AssetBrowser = 1 << 4,
    Settings = 1 << 5,
    All = 0xFFFFFFFF
};

inline EditorPanel operator|(EditorPanel a, EditorPanel b) {
    return static_cast<EditorPanel>(static_cast<u32>(a) | static_cast<u32>(b));
}

inline EditorPanel operator&(EditorPanel a, EditorPanel b) {
    return static_cast<EditorPanel>(static_cast<u32>(a) & static_cast<u32>(b));
}

inline bool HasPanel(EditorPanel flags, EditorPanel panel) {
    return (static_cast<u32>(flags) & static_cast<u32>(panel)) != 0;
}

// Editor layer - manages ImGui editor UI
class ENJIN_API EditorLayer {
public:
    EditorLayer();
    ~EditorLayer();

    bool Initialize(Window* window, Renderer::VulkanRenderer* renderer);
    void Shutdown();

    void Update(f32 deltaTime);
    void Render(VkCommandBuffer commandBuffer);

    // Set the world to edit
    void SetWorld(ECS::World* world) { m_World = world; }
    ECS::World* GetWorld() const { return m_World; }

    // Set the camera for the viewport
    void SetCamera(Renderer::Camera* camera) { m_Camera = camera; }
    void SetCameraController(Renderer::CameraController* controller) { m_CameraController = controller; }

    // Panel visibility
    void SetPanelVisibility(EditorPanel panel, bool visible);
    bool IsPanelVisible(EditorPanel panel) const;

    // Selected entity
    ECS::Entity GetSelectedEntity() const { return m_SelectedEntity; }
    void SetSelectedEntity(ECS::Entity entity) { m_SelectedEntity = entity; }

    // Callbacks
    using EntitySelectedCallback = std::function<void(ECS::Entity)>;
    void SetEntitySelectedCallback(EntitySelectedCallback callback) { m_OnEntitySelected = callback; }

    // Check if UI wants input (for disabling camera when interacting with UI)
    bool WantsKeyboardInput() const;
    bool WantsMouseInput() const;

private:
    void DrawMenuBar();
    void DrawHierarchyPanel();
    void DrawInspectorPanel();
    void DrawViewportPanel();
    void DrawConsolePanel();
    void DrawAssetBrowserPanel();
    void DrawSettingsPanel();
    void DrawStatsOverlay();

    void DrawEntityNode(ECS::Entity entity, const std::string& name);
    void DrawTransformComponent(ECS::Entity entity);
    void DrawMeshComponent(ECS::Entity entity);
    void DrawLightComponent(ECS::Entity entity);

    Window* m_Window = nullptr;
    Renderer::VulkanRenderer* m_Renderer = nullptr;
    ECS::World* m_World = nullptr;
    Renderer::Camera* m_Camera = nullptr;
    Renderer::CameraController* m_CameraController = nullptr;

    std::unique_ptr<GUI::ImGuiLayer> m_ImGuiLayer;

    EditorPanel m_VisiblePanels = EditorPanel::All;
    ECS::Entity m_SelectedEntity = ECS::INVALID_ENTITY;

    EntitySelectedCallback m_OnEntitySelected;

    // Panel state
    bool m_ShowDemoWindow = false;
    bool m_ShowStatsOverlay = true;

    // Console log buffer
    std::vector<std::string> m_ConsoleLog;
    static constexpr usize MAX_CONSOLE_LINES = 1000;
};

} // namespace Editor
} // namespace Enjin
