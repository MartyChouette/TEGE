#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Window.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Entity.h"
#include "Enjin/Renderer/Vulkan/VulkanRenderer.h"
#include "Enjin/Renderer/Vulkan/VulkanBuffer.h"
#include "Enjin/Renderer/Camera.h"
#include "Enjin/Renderer/CameraController.h"
#include "Enjin/Renderer/RenderTarget.h"
#include "Enjin/GUI/ImGuiLayer.h"
#include "Enjin/Editor/PlayMode.h"
#include "Enjin/Editor/EditorSettings.h"
#include "Enjin/Input/InputAction.h"
#include "Enjin/GUI/GameMenus.h"
#include "Enjin/Effects/Weather.h"
#include "Enjin/Effects/Water.h"
#include "Enjin/Effects/Wind.h"
#include "Enjin/Effects/RetroEffects.h"
#include "Enjin/Effects/WorldTime.h"
#include "Enjin/Effects/SeasonalWeather.h"
#include "Enjin/Scene/SceneManager.h"
#include "Enjin/Editor/PerformanceStats.h"
#include "Enjin/Editor/TerrainBrush.h"
#include "Enjin/Editor/UndoRedo.h"
#include "Enjin/Build/BuildReport.h"
#include <string>
#include <functional>
#include <memory>

namespace Enjin {

// Forward declarations
namespace Renderer {
    class PostProcessing;
}
namespace ECS {
    class RenderSystem;
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
    SceneList = 1 << 9,
    Skybox = 1 << 10,
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
    void RenderOffscreen(VkCommandBuffer commandBuffer);  // Call BEFORE main render pass
    void Render(VkCommandBuffer commandBuffer);            // Call DURING main render pass

    // Set the world to edit
    void SetWorld(ECS::World* world) { m_World = world; m_SceneManager.SetWorld(world); }
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

    // Focus mode (fullscreen game view with captured mouse)
    bool IsFocusMode() const { return m_FocusMode; }

    // Set render system for offscreen game camera rendering
    void SetRenderSystem(ECS::RenderSystem* renderSystem) { m_RenderSystem = renderSystem; }

    // Access post-processing (owned by editor, created during Initialize)
    Renderer::PostProcessing* GetPostProcessing() { return m_PostProcessing.get(); }

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
    void DrawSceneListPanel();
    void DrawSkyboxPanel();
    void DrawStatsOverlay();
    void DrawSplashScreen();
    void DrawBuildDialog();

    void DrawEntityNode(ECS::Entity entity, const std::string& name);
    void DrawTransformComponent(ECS::Entity entity);
    void DrawMeshComponent(ECS::Entity entity);
    void DrawMaterialComponent(ECS::Entity entity);
    void DrawLightComponent(ECS::Entity entity);
    void DrawCameraComponent(ECS::Entity entity);
    void DrawNotesComponent(ECS::Entity entity);
    void DrawTextComponent(ECS::Entity entity);
    void DrawWeatherZoneComponent(ECS::Entity entity);
    void DrawWaterVolumeComponent(ECS::Entity entity);
    void DrawGrassVolumeComponent(ECS::Entity entity);
    void DrawShrubVolumeComponent(ECS::Entity entity);
    void DrawTreeVolumeComponent(ECS::Entity entity);
    void DrawVegetationComponent(ECS::Entity entity);
    void DrawCameraTriggerComponent(ECS::Entity entity);
    void DrawTemperatureZoneComponent(ECS::Entity entity);
    void DrawGravityZoneComponent(ECS::Entity entity);

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
    void DrawSphereColliderComponent(ECS::Entity entity);
    void DrawCapsuleColliderComponent(ECS::Entity entity);
    void DrawTriggerZoneComponent(ECS::Entity entity);
    void DrawDamageComponent(ECS::Entity entity);
    void DrawInteractableComponent(ECS::Entity entity);
    void DrawPickupComponent(ECS::Entity entity);
    void DrawInventoryComponent(ECS::Entity entity);
    void DrawTimerComponent(ECS::Entity entity);
    void DrawAudioSourceComponent(ECS::Entity entity);
    void DrawAudioListenerComponent(ECS::Entity entity);

    // AI components
    void DrawAIControllerComponent(ECS::Entity entity);
    void DrawFollowTargetComponent(ECS::Entity entity);
    void DrawLookAtTargetComponent(ECS::Entity entity);
    void DrawWaypointComponent(ECS::Entity entity);

    // Visual components
    void DrawBillboardComponent(ECS::Entity entity);
    void DrawParticleEmitterComponent(ECS::Entity entity);

    // 2D components
    void DrawSprite2DComponent(ECS::Entity entity);
    void DrawAnimatedSprite2DComponent(ECS::Entity entity);
    void DrawTilemapComponent(ECS::Entity entity);
    void DrawCamera2DBoundsComponent(ECS::Entity entity);
    void DrawStateMachineComponent(ECS::Entity entity);
    void DrawDialogueComponent(ECS::Entity entity);

    // Terrain components
    void DrawTerrainComponent(ECS::Entity entity);
    void DrawTerrain2DComponent(ECS::Entity entity);

    // Other components
    void DrawTagComponent(ECS::Entity entity);
    void DrawSpawnPointComponent(ECS::Entity entity);

    // Flower components
    void DrawJellyMeshComponent(ECS::Entity entity);
    void DrawTetherComponent(ECS::Entity entity);
    void DrawGrabbableComponent(ECS::Entity entity);
    void DrawFlowerStemComponent(ECS::Entity entity);

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
    ECS::RenderSystem* m_RenderSystem = nullptr;

    std::unique_ptr<GUI::ImGuiLayer> m_ImGuiLayer;

    EditorPanel m_VisiblePanels = EditorPanel::All;
    ECS::Entity m_SelectedEntity = ECS::INVALID_ENTITY;

    EntitySelectedCallback m_OnEntitySelected;

    // Panel state
    bool m_ShowDemoWindow = false;
    bool m_ShowStatsOverlay = true;
    bool m_ShowAboutDialog = false;

    // Scene state
    std::string m_CurrentScenePath;

    // Console log buffer
    std::vector<std::string> m_ConsoleLog;
    static constexpr usize MAX_CONSOLE_LINES = 1000;

    // Helper methods
    void ImportModel(const std::string& path);
    void HandleViewportPicking();
    bool SceneHasMouseLookController() const;
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
    f32 m_GridSize = 200.0f;
    i32 m_GridLines = 200;

    // Grid 3D mesh (rendered with depth testing)
    std::unique_ptr<Renderer::VulkanBuffer> m_GridVertexBuffer;
    u32 m_GridVertexCount = 0;
    u32 m_GridRegularCount = 0;  // Vertices for regular lines
    u32 m_GridAxisXStart = 0;    // First vertex of X axis line
    u32 m_GridAxisZStart = 0;    // First vertex of Z axis line
    f32 m_BuiltGridSize = 0.0f;
    i32 m_BuiltGridLines = 0;
    void BuildGridMesh();

    // Play mode
    PlayMode m_PlayMode;

    // Focus mode (fullscreen game view, hides all editor panels)
    bool m_FocusMode = false;

    // Camera zone override (driven by CameraTriggerComponent)
    ECS::Entity m_CameraZoneOverride = ECS::INVALID_ENTITY;

    // Track whether effect pipelines have been updated for the render target's render pass
    bool m_EffectPipelinesUpdated = false;

    // Splash screen
    bool m_ShowSplash = true;
    f32 m_SplashTimer = 0.0f;
    f32 m_SplashDuration = 3.0f;   // Show for 3 seconds
    f32 m_SplashFadeStart = 2.0f;  // Start fading at 2 seconds
    f32 m_EditorFadeIn = 0.0f;     // Editor fade-in progress (0 to 1)

    // Template selector (shown after splash)
    bool m_ShowTemplateSelector = true;
    i32 m_SelectedTemplate = -1;   // -1 = none selected (hovering)
    std::vector<std::string> m_CustomTemplateNames;  // Saved custom template names
    std::vector<std::string> m_CustomTemplatePaths;  // Saved custom template file paths
    void DrawTemplateSelector();
    void ApplyTemplate(const std::string& templateId);
    void SaveCustomTemplate(const std::string& name);
    void LoadCustomTemplates();

    // Docking layout
    bool m_DockingInitialized = false;

    // Game View render targets (offscreen rendering for game camera)
    std::unique_ptr<Renderer::RenderTarget> m_GameViewRenderTarget;  // Final output (displayed in ImGui)
    std::unique_ptr<Renderer::RenderTarget> m_SceneRenderTarget;     // Scene pre-post-processing
    u32 m_GameViewWidth = 640;
    u32 m_GameViewHeight = 360;

    // Post-processing (owned by editor, applied to Game View)
    std::unique_ptr<Renderer::PostProcessing> m_PostProcessing;

    // Selected game camera entity (user can pick which camera to use)
    ECS::Entity m_SelectedGameCamera = ECS::INVALID_ENTITY;

    // Scene management
    Scene::SceneManager m_SceneManager;

    // Effects systems (global, rendered in game view)
    Effects::WindSystem m_WindSystem;
    Effects::WeatherSystem m_WeatherSystem;
    Effects::Water3D m_Water3D;
    Effects::RetroEffects m_RetroEffects;

    // World time and seasonal weather
    Effects::WorldTimeSystem m_WorldTime;
    Effects::SeasonalWeatherSystem m_SeasonalWeather;
    bool m_WorldTimeEnabled = false;
    bool m_SeasonalWeatherEnabled = false;

    // World curvature
    f32 m_WorldCurvature = 0.0f;
    bool m_WorldCurvatureEnabled = false;

    // Terrain editing
    TerrainBrush m_TerrainBrush;
    bool m_TerrainEditMode = false;

    // Draw camera frustum gizmo in editor view
    void DrawCameraFrustum(ECS::Entity cameraEntity);

    // Auto-create and configure a game camera when a character controller is added
    void SetupCameraForController(ECS::Entity controllerEntity, const std::string& controllerType);

    // Console command execution
    void ExecuteConsoleCommand(const std::string& command);

    // Clipboard state for cut/copy/paste entities
    std::string m_ClipboardEntityJson;
    bool m_ClipboardIsCut = false;
    ECS::Entity m_ClipboardSourceEntity = ECS::INVALID_ENTITY;

    // Performance stats
    PerformanceMetrics m_PerfMetrics;
    f32 m_PerfUpdateTimer = 0.0f;

    // Asset browser state
    std::string m_AssetBrowserPath;  // Current browsing directory
    std::string m_AssetBrowserSelected; // Currently selected file

    // Undo/Redo manager
    UndoRedoManager m_UndoRedo;
    Math::Matrix4 m_GizmoStartTransform;
    bool m_GizmoDragging = false;

    // Game View mouse interaction during play mode
    bool m_GameViewMouseCaptured = false;
    f32 m_GameViewImageMinX = 0.0f, m_GameViewImageMinY = 0.0f;
    f32 m_GameViewImageMaxX = 0.0f, m_GameViewImageMaxY = 0.0f;
    bool m_GameViewHovered = false;

    // Accessibility settings (persistent)
    EditorSettings m_EditorSettings;

    // Input action map for remappable input
    InputSystem::InputActionMap m_InputMap;

    // In-game pause/system menu
    GUI::GameMenuSystem m_GameMenu;

    // Build dialog state
    bool m_ShowBuildDialog = false;
    Build::BuildConfig m_BuildConfig;
    Build::BuildResult m_BuildResult;
    bool m_BuildInProgress = false;
    bool m_BuildFinished = false;
    float m_BuildProgress = 0.0f;
    std::string m_BuildProgressPhase;
};

} // namespace Editor
} // namespace Enjin
