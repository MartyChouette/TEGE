#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Window.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Entity.h"
#include "Enjin/Renderer/Vulkan/VulkanRenderer.h"
#include "Enjin/Renderer/Camera.h"
#include "Enjin/Renderer/CameraController.h"
#include "Enjin/GUI/ImGuiLayer.h"
#include "Enjin/Editor/PlayMode.h"
#include "Enjin/Effects/Weather.h"
#include "Enjin/Effects/Water.h"
#include "Enjin/Effects/RetroEffects.h"
#include <string>
#include <functional>
#include <memory>

namespace Enjin {

// Forward declarations
namespace Renderer {
    class PostProcessing;
}

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
    PostProcessing = 1 << 6,
    Effects = 1 << 7,
    GameView = 1 << 8,
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

// Gizmo operation mode
enum class GizmoOperation {
    Translate,
    Rotate,
    Scale
};

// Gizmo space (local vs world)
enum class GizmoSpace {
    Local,
    World
};

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
    void SetCamera(Renderer::Camera* camera) { m_Camera = camera; InitializePlayMode(); }
    void SetCameraController(Renderer::CameraController* controller) { m_CameraController = controller; InitializePlayMode(); }

    // Play mode controls
    void Play() { m_PlayMode.Play(); }
    void Pause() { m_PlayMode.Pause(); }
    void Stop() { m_PlayMode.Stop(); }
    bool IsPlaying() const { return m_PlayMode.IsPlaying(); }
    bool IsPaused() const { return m_PlayMode.IsPaused(); }
    PlayMode& GetPlayMode() { return m_PlayMode; }

    // Set post-processing for the settings panel
    void SetPostProcessing(Renderer::PostProcessing* postProcessing) { m_PostProcessing = postProcessing; }

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
    void InitializePlayMode();
    void DrawMenuBar();
    void DrawHierarchyPanel();
    void DrawInspectorPanel();
    void DrawViewportPanel();
    void DrawConsolePanel();
    void DrawAssetBrowserPanel();
    void DrawSettingsPanel();
    void DrawPostProcessingPanel();
    void DrawEffectsPanel();
    void DrawGameViewPanel();
    void DrawStatsOverlay();
    void DrawSplashScreen();

    void DrawEntityNode(ECS::Entity entity, const std::string& name);
    void DrawTransformComponent(ECS::Entity entity);
    void DrawMeshComponent(ECS::Entity entity);
    void DrawMaterialComponent(ECS::Entity entity);
    void DrawLightComponent(ECS::Entity entity);
    void DrawCameraComponent(ECS::Entity entity);
    void DrawNotesComponent(ECS::Entity entity);

    // Controller components
    void DrawPlatformer2DController(ECS::Entity entity);
    void DrawTopDown2DController(ECS::Entity entity);
    void DrawTopDown3DController(ECS::Entity entity);
    void DrawThirdPersonController(ECS::Entity entity);
    void DrawFirstPersonController(ECS::Entity entity);

    // Gameplay components
    void DrawHealthComponent(ECS::Entity entity);
    void DrawRigidbodyComponent(ECS::Entity entity);
    void DrawBoxColliderComponent(ECS::Entity entity);
    void DrawAudioSourceComponent(ECS::Entity entity);

    // 2D components
    void DrawSprite2DComponent(ECS::Entity entity);
    void DrawAnimatedSprite2DComponent(ECS::Entity entity);
    void DrawTilemapComponent(ECS::Entity entity);
    void DrawStateMachineComponent(ECS::Entity entity);
    void DrawDialogueComponent(ECS::Entity entity);

    // Scene management
    void SaveScene(const std::string& path);
    void OpenScene(const std::string& path);

    // Entity operations
    void DuplicateEntity(ECS::Entity entity);
    void DeleteSelectedEntity();

    Window* m_Window = nullptr;
    Renderer::VulkanRenderer* m_Renderer = nullptr;
    ECS::World* m_World = nullptr;
    Renderer::Camera* m_Camera = nullptr;
    Renderer::CameraController* m_CameraController = nullptr;
    Renderer::PostProcessing* m_PostProcessing = nullptr;

    std::unique_ptr<GUI::ImGuiLayer> m_ImGuiLayer;

    EditorPanel m_VisiblePanels = EditorPanel::All;
    ECS::Entity m_SelectedEntity = ECS::INVALID_ENTITY;

    EntitySelectedCallback m_OnEntitySelected;

    // Panel state
    bool m_ShowDemoWindow = false;
    bool m_ShowStatsOverlay = true;

    // Scene state
    std::string m_CurrentScenePath;

    // Console log buffer
    std::vector<std::string> m_ConsoleLog;
    static constexpr usize MAX_CONSOLE_LINES = 1000;

    // Helper methods
    void ImportModel(const std::string& path);
    void HandleViewportPicking();
    void DrawGizmos();
    void DrawGrid();
    void FocusOnEntity(ECS::Entity entity);  // Center camera on entity

    // Gizmo state
    GizmoOperation m_GizmoOperation = GizmoOperation::Translate;
    GizmoSpace m_GizmoSpace = GizmoSpace::Local;
    bool m_UseSnap = false;
    f32 m_TranslateSnap = 0.5f;
    f32 m_RotateSnap = 15.0f;
    f32 m_ScaleSnap = 0.1f;

    // Frame time tracking for stats overlay
    static constexpr usize FRAME_TIME_HISTORY_SIZE = 120;  // ~2 seconds at 60fps
    f32 m_FrameTimeHistory[FRAME_TIME_HISTORY_SIZE] = {};
    usize m_FrameTimeIndex = 0;
    f32 m_FrameTimeMin = 0.0f;
    f32 m_FrameTimeMax = 0.0f;
    f32 m_FrameTimeAvg = 0.0f;
    f32 m_LastDeltaTime = 0.0f;

    // Grid settings
    bool m_ShowGrid = true;
    f32 m_GridSize = 20.0f;
    i32 m_GridLines = 20;

    // Play mode
    PlayMode m_PlayMode;

    // Splash screen
    bool m_ShowSplash = true;
    f32 m_SplashTimer = 0.0f;
    f32 m_SplashDuration = 3.0f;   // Show for 3 seconds
    f32 m_SplashFadeStart = 2.0f;  // Start fading at 2 seconds
    f32 m_EditorFadeIn = 0.0f;     // Editor fade-in progress (0 to 1)

    // Docking layout
    bool m_DockingInitialized = false;

    // Game View dimensions (for future render-to-texture implementation)
    u32 m_GameViewWidth = 640;
    u32 m_GameViewHeight = 360;

    // Effects systems (global, rendered in game view)
    Effects::WeatherSystem m_WeatherSystem;
    Effects::Water3D m_Water3D;
    Effects::RetroEffects m_RetroEffects;

    // Draw camera frustum gizmo in editor view
    void DrawCameraFrustum(ECS::Entity cameraEntity);
};

} // namespace Editor
} // namespace Enjin
