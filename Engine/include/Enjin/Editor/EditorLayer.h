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
#include "Enjin/Debug/Profiler.h"
#include "Enjin/Input/InputAction.h"
#include "Enjin/GUI/GameMenus.h"
#include "Enjin/GUI/UISystem.h"
#include "Enjin/Accessibility/SubtitleSystem.h"
#include "Enjin/Accessibility/AudioVisualIndicator.h"
#include "Enjin/Accessibility/Announcer.h"
#include "Enjin/Editor/ProceduralGraph.h"
#include "Enjin/Editor/CommandPalette.h"
#include "Enjin/Accessibility/AlternativeInput.h"
#include "Enjin/Effects/Weather.h"
#include "Enjin/Effects/Water.h"
#include "Enjin/Effects/Wind.h"
#include "Enjin/Effects/RetroEffects.h"
#include "Enjin/Effects/WorldTime.h"
#include "Enjin/Effects/SeasonalWeather.h"
#include "Enjin/Effects/ParticleSystem.h"
#include "Enjin/Effects/FluidSimulation.h"
#include "Enjin/Effects/CurlNoiseSystem.h"
#include "Enjin/Scene/SceneManager.h"
#include "Enjin/Renderer/SceneRenderSettings.h"
#include "Enjin/Editor/PerformanceStats.h"
#include "Enjin/Editor/TerrainBrush.h"
#include "Enjin/Editor/ScenePicker.h"
#include "Enjin/Editor/UndoRedo.h"
#include "Enjin/Build/BuildReport.h"
#include "Enjin/Assets/AssetMetadata.h"
#include "Enjin/Assets/ThumbnailGenerator.h"
#include "Enjin/Assets/TextureCompressor.h"
#include "Enjin/Renderer/Texture.h"
#include "Enjin/GUI/DialogueTree.h"
#include "Enjin/Editor/AnimationGraphEditor.h"
#include "Enjin/Editor/VisualScriptEditor.h"
#include "Enjin/Editor/SpriteSheetImporter.h"
#include "Enjin/Editor/PixelEditor.h"
#include "Enjin/Editor/SpriteColliderGenerator.h"
#include "Enjin/Editor/BehaviorTreeEditor.h"
#include "Enjin/Editor/QuestFlowEditor.h"
#include "Enjin/Editor/DocGenerator.h"
#include "Enjin/Editor/SceneLock.h"
#include "Enjin/Editor/CollaborativeEditing.h"
#include "Enjin/Editor/FlashTimeline.h"
#include "Enjin/Editor/VectorDrawingEditor.h"
#include "Enjin/Editor/FeedbackSystem.h"
#include "Enjin/Scripting/AS3Transpiler.h"
#include "Enjin/Networking/NewgroundsAPI.h"
#include "Enjin/Build/HTML5Exporter.h"
#include "Enjin/Plugin/PluginRepository.h"
#include "Enjin/Procedural/ProceduralAlgorithms.h"
#include <string>
#include <functional>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <vulkan/vulkan.h>

namespace Enjin {

// Forward declarations
namespace Renderer {
    class PostProcessing;
}
namespace ECS {
    class RenderSystem;
    struct TerrainComponent;
    struct Terrain2DComponent;
    struct TransformComponent;
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
    EditorSettings = 1 << 5,
    PostProcessing = 1 << 6,
    RetroEffects = 1 << 7,
    GameView = 1 << 8,
    SceneList = 1 << 9,
    Rendering = 1 << 10,
    Profiler = 1 << 11,
    ProjectSettings = 1 << 12,
    ParticleEditor = 1 << 13,
    AnimGraph = 1 << 14,
    Dialogue = 1 << 15,
    VisualScript = 1 << 16,
    SpriteSheetImport = 1 << 17,
    PixelEditorPanel = 1 << 18,
    BehaviorTree = 1 << 19,
    QuestFlow = 1 << 20,
    UserManual = 1 << 21,
    DataAssets = 1 << 22,
    PluginBrowser = 1 << 23,
    ProceduralGen = 1 << 24,
    GitIntegration = 1 << 25,
    NetworkPanel = 1 << 26,
    Collaboration = 1 << 27,
    FlashTimeline = 1 << 28,
    VectorDrawing = 1 << 29,
    FeedbackPanel = 1 << 30,
    SaveDebug = 1u << 31,
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

// Git file status entry
struct GitFileStatus {
    std::string path;
    enum class Status : u8 { Modified, Added, Deleted, Renamed, Untracked, Staged } status;
    bool staged = false;
};

// Git log entry
struct GitLogEntry {
    std::string hash, message, author, date;
};

// UI editor drag handle mode
enum class UIEditDragMode : u8 {
    None = 0,
    Move,
    ResizeLeft,
    ResizeRight,
    ResizeTop,
    ResizeBottom,
    ResizeTL,
    ResizeTR,
    ResizeBL,
    ResizeBR
};

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

    // Returns true when splash screen or project hub covers the entire screen,
    // so the caller can skip 3D scene rendering and camera input.
    bool IsShowingFullscreenOverlay() const { return m_ShowSplash || m_ShowProjectHub; }

    // Play mode controls
    void Play() { m_PlayMode.Play(); }
    void Pause() { m_PlayMode.Pause(); }
    void Stop() { m_PlayMode.Stop(); }
    bool IsPlaying() const { return m_PlayMode.IsPlaying(); }
    bool IsPaused() const { return m_PlayMode.IsPaused(); }
    PlayMode& GetPlayMode() { return m_PlayMode; }

    // Settings access (for frame rate limiting)
    const EditorSettings& GetEditorSettings() const { return m_EditorSettings; }
    EditorSettings& GetEditorSettings() { return m_EditorSettings; }
    Scene::SceneManager& GetSceneManager() { return m_SceneManager; }
    const Scene::SceneManager& GetSceneManager() const { return m_SceneManager; }

    // Focus mode (fullscreen game view with captured mouse)
    bool IsFocusMode() const { return m_FocusMode; }

    // Set render system for offscreen game camera rendering
    void SetRenderSystem(ECS::RenderSystem* renderSystem);

    // Access post-processing (owned by editor, created during Initialize)
    Renderer::PostProcessing* GetPostProcessing() { return m_PostProcessing.get(); }

    // Panel visibility
    void SetPanelVisibility(EditorPanel panel, bool visible);
    bool IsPanelVisible(EditorPanel panel) const;

    // Multi-select entity management
    void SelectEntity(ECS::Entity entity, bool addToSelection = false);
    void DeselectEntity(ECS::Entity entity);
    void ClearSelection();
    void SelectRange(ECS::Entity from, ECS::Entity to);
    bool IsSelected(ECS::Entity entity) const;
    const std::unordered_set<ECS::Entity>& GetSelectedEntities() const { return m_SelectedEntities; }
    void SelectEntitiesInRect(ImVec2 min, ImVec2 max);

    // Backward-compatible single-entity API (returns/sets primary)
    ECS::Entity GetSelectedEntity() const { return m_PrimarySelected; }
    void SetSelectedEntity(ECS::Entity entity);

    // Callbacks
    using EntitySelectedCallback = std::function<void(ECS::Entity)>;
    void SetEntitySelectedCallback(EntitySelectedCallback callback) { m_OnEntitySelected = callback; }

    // Check if UI wants input (for disabling camera when interacting with UI)
    bool WantsKeyboardInput() const;
    bool WantsMouseInput() const;

private:
    void InitializePlayMode();
    void StartPlayMode();  // Starts play mode and applies game VSync settings
    void DrawMenuBar();
    void DrawHierarchyPanel();
    void DrawInspectorPanel();
    void DrawViewportPanel();
    void DrawConsolePanel();
    void DrawAssetBrowserPanel();
    void DrawEditorSettingsPanel();
    void DrawPostProcessingPanel();
    void DrawRetroEffectsPanel();
    void DrawGameViewPanel();
    void DrawSceneListPanel();
    void DrawRenderingPanel();
    void DrawBuildConfigSection();
    void DrawProjectSettingsPanel();
    void DrawParticleEditorPanel();
    void DrawAnimGraphPanel();
    void DrawDialoguePanel();
    void DrawVisualScriptPanel();
    void DrawSpriteSheetImporterPanel();
    void DrawPixelEditorPanel();
    void DrawBehaviorTreePanel();
    void DrawQuestFlowPanel();
    void DrawUserManualPanel();
    void DrawDataAssetPanel();
    void DrawPluginBrowserPanel();
    void DrawProceduralGenPanel();
    void DrawGitIntegrationPanel();
    void DrawNetworkPanel();
    static std::string RunGitCommand(const std::string& args, const std::string& workingDir);
    void DetectGitRepo();
    void RefreshGitStatus();
    void GitStageFile(const std::string& path);
    void GitUnstageFile(const std::string& path);
    void GitStageAll();
    void GitUnstageAll();
    void GitCommit();
    void GitPush();
    void GitPull();
    void GitFetch();
    void GitSwitchBranch(const std::string& branch);
    void DrawStatsOverlay();
    void DrawSplashScreen();
    void DrawBuildDialog();

    void DrawEmptyState(const char* icon, const char* heading, const char* body, const char* ctaLabel = nullptr, std::function<void()> ctaAction = nullptr);
    void DrawEntityNode(ECS::Entity entity, const std::string& name);
    void DrawTransformComponent(ECS::Entity entity);
    void DrawMeshComponent(ECS::Entity entity);
    void DrawLODComponent(ECS::Entity entity);
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
    void DrawFluidVolumeComponent(ECS::Entity entity);

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
    void DrawCollisionFilteringUI(u32& categoryBits, u32& collisionMask);
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
    void DrawDialogueBoxComponent(ECS::Entity entity);

    // Terrain components
    void DrawTerrainComponent(ECS::Entity entity);
    void DrawTerrain2DComponent(ECS::Entity entity);

    // Other components
    void DrawTagComponent(ECS::Entity entity);
    void DrawSpawnPointComponent(ECS::Entity entity);
    void DrawLayerComponent(ECS::Entity entity);
    void DrawSaveDataComponent(ECS::Entity entity);
    void DrawSaveLoadMenuComponent(ECS::Entity entity);
    void DrawSkeletonComponent(ECS::Entity entity);

    // Flower components
    void DrawJellyMeshComponent(ECS::Entity entity);
    void DrawTetherComponent(ECS::Entity entity);
    void DrawGrabbableComponent(ECS::Entity entity);
    void DrawFlowerStemComponent(ECS::Entity entity);
    void DrawFlowerParticleConfigComponent(ECS::Entity entity);

    // Scripting
    void DrawScriptComponent(ECS::Entity entity);
    void DrawBehaviorTreeComponent(ECS::Entity entity);
    void DrawQuestFlowComponent(ECS::Entity entity);

    // Vehicle / Possession / Planet
    void DrawVehicleController(ECS::Entity entity);
    void DrawSurfaceAlignedController(ECS::Entity entity);
    void DrawPossessableComponent(ECS::Entity entity);

    // New gameplay components
    void DrawDamageResistanceComponent(ECS::Entity entity);
    void DrawResourceComponent(ECS::Entity entity);
    void DrawFootstepComponent(ECS::Entity entity);
    void DrawPoolableComponent(ECS::Entity entity);
    void DrawQuestStateComponent(ECS::Entity entity);
    void DrawHUDWidgetComponent(ECS::Entity entity);
    void DrawUICanvasComponent(ECS::Entity entity);
    void DrawCinematicCameraComponent(ECS::Entity entity);
    void DrawTweenComponent(ECS::Entity entity);

    // Joint & Ragdoll components
    void DrawDistanceJointComponent(ECS::Entity entity);
    void DrawHingeJointComponent(ECS::Entity entity);
    void DrawBallSocketJointComponent(ECS::Entity entity);
    void DrawSpringJointComponent(ECS::Entity entity);
    void DrawFixedJointComponent(ECS::Entity entity);
    void DrawSliderJointComponent(ECS::Entity entity);
    void DrawRagdollComponent(ECS::Entity entity);

    // Puzzle components
    void DrawLockComponent(ECS::Entity entity);
    void DrawPushableComponent(ECS::Entity entity);
    void DrawSwitchComponent(ECS::Entity entity);
    void DrawGoalZoneComponent(ECS::Entity entity);
    void DrawConveyorComponent(ECS::Entity entity);
    void DrawTeleporterComponent(ECS::Entity entity);
    void DrawDestructibleComponent(ECS::Entity entity);
    void DrawCurlNoiseFieldComponent(ECS::Entity entity);
    void DrawFractureConfigComponent(ECS::Entity entity);
    void DrawMovingPlatformComponent(ECS::Entity entity);
    void DrawPerFrameColliderComponent(ECS::Entity entity);
    void DrawPolygonCollider2DComponent(ECS::Entity entity);

    // Networking components
    void DrawNetworkIdentityComponent(ECS::Entity entity);
    void DrawNetworkTransformComponent(ECS::Entity entity);

    // Runtime dialogue overlay (rendered during play mode)
    void UpdateDialogue(f32 deltaTime);
    void DrawDialogueOverlay();
    ECS::Entity m_ActiveDialogueEntity = ECS::INVALID_ENTITY;
    GUI::DialogueTreeEditor m_DialogueTreeEditor;

    // Dialogue editor panel state
    ECS::Entity m_DialogueEditorEntity = ECS::INVALID_ENTITY;

    // Animation/State Machine graph editor
    AnimationGraphEditor m_AnimGraphEditor;

    // Visual Script graph editor
    VisualScriptEditor m_VisualScriptEditor;

    // Behavior Tree graph editor
    BehaviorTreeEditor m_BehaviorTreeEditor;

    // Quest Flow graph editor
    QuestFlowEditor m_QuestFlowEditor;

    // Scene management
    void SaveScene(const std::string& path);
    void OpenScene(const std::string& path);

    // Entity operations
    void DuplicateEntity(ECS::Entity entity);
    void DeleteSelectedEntities();
    void DuplicateSelectedEntities();
    void FocusOnSelection();

    // Undo-aware component removal helper
    template<typename T>
    void RemoveComponentWithUndo(ECS::Entity entity, const std::string& componentKey,
                                  const std::string& componentName);

    Window* m_Window = nullptr;
    Renderer::VulkanRenderer* m_Renderer = nullptr;
    ECS::World* m_World = nullptr;
    Renderer::Camera* m_Camera = nullptr;
    Renderer::CameraController* m_CameraController = nullptr;
    ECS::RenderSystem* m_RenderSystem = nullptr;

    std::unique_ptr<GUI::ImGuiLayer> m_ImGuiLayer;

    EditorPanel m_VisiblePanels = EditorPanel::All;

    // Multi-select state
    std::unordered_set<ECS::Entity> m_SelectedEntities;
    ECS::Entity m_PrimarySelected = ECS::INVALID_ENTITY;

    // Marquee (rubber-band) drag state
    bool m_MarqueeDragging = false;
    ImVec2 m_MarqueeStart = {0, 0};
    ImVec2 m_MarqueeEnd = {0, 0};

    EntitySelectedCallback m_OnEntitySelected;

    // Panel state
    bool m_ShowDemoWindow = false;
    bool m_ShowStatsOverlay = true;
    bool m_ShowAboutDialog = false;

    // User Manual panel state
    struct ManualSection {
        std::string title;
        std::string content;      // Raw markdown text
        int level = 0;            // Header level (1 = ##, 2 = ###, etc.)
        bool showChildrenInline = false; // True when content is empty — show child sections inline
    };
    std::vector<ManualSection> m_ManualSections;
    char m_ManualSearchBuf[256] = {};
    int m_ManualSelectedSection = -1;
    bool m_ManualLoaded = false;
    void LoadUserManual();
    void ExportManualAsHTML(const std::string& outputPath);

    // Scene state
    std::string m_CurrentScenePath;

    // Console log buffer
    std::vector<std::string> m_ConsoleLog;
    static constexpr usize MAX_CONSOLE_LINES = 1000;

    // Helper methods
    void ImportModel(const std::string& path);
    void OnFileDrop(int count, const char** paths);
    void HandleViewportPicking();
    bool SceneHasMouseLookController() const;
    void DrawGizmos();
    void DrawGrid();
    void FocusOnEntity(ECS::Entity entity);  // Center camera on entity
    void DrawMarqueeRect();                   // Draw rubber-band selection rectangle
    void DrawMultiSelectInspector();          // Inspector view when multiple entities selected

    // Gizmo state
    GizmoOperation m_GizmoOperation = GizmoOperation::Translate;
    GizmoSpace m_GizmoSpace = GizmoSpace::Local;
    bool m_UseSnap = false;
    f32 m_TranslateSnap = 0.5f;
    f32 m_RotateSnap = 15.0f;
    f32 m_ScaleSnap = 0.1f;
    bool m_SurfaceSnap = false;         // Project entities onto terrain/sphere surfaces
    bool m_SurfaceAlignNormal = true;   // Align entity Y-axis to surface normal

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
    bool m_BuiltGridIs2D = false;
    void BuildGridMesh();

    // Play mode
    PlayMode m_PlayMode;

    // Focus mode (fullscreen game view, hides all editor panels)
    bool m_FocusMode = false;

    // Deferred play mode stop (set during Render, executed at start of next Update
    // to avoid dangling pointers from mid-frame World::Clear)
    bool m_PendingPlayStop = false;

    // Camera zone override (driven by CameraTriggerComponent)
    ECS::Entity m_CameraZoneOverride = ECS::INVALID_ENTITY;

    // Cached player entity for per-frame zone detection (avoid GetAllEntities scan)
    ECS::Entity m_CachedPlayerEntity = ECS::INVALID_ENTITY;

    // Track whether effect pipelines have been updated for the render target's render pass
    bool m_EffectPipelinesUpdated = false;

    // Splash screen
    bool m_ShowSplash = true;
    f32 m_SplashTimer = 0.0f;
    f32 m_SplashDuration = 4.0f;   // Show for 4 seconds
    f32 m_SplashFadeStart = 3.0f;  // Start fading at 3 seconds
    f32 m_EditorFadeIn = 0.0f;     // Editor fade-in progress (0 to 1)

    // Project Hub (shown after splash)
    enum class HubPage : u8 { Landing = 0, WizardSetup, WizardTemplate, Demos };
    bool m_ShowProjectHub = true;
    HubPage m_HubPage = HubPage::Landing;

    // New Project wizard state
    char m_NewProjectName[128] = "MyGame";
    char m_NewProjectPath[512] = "";       // Filled with default (Documents/EnjinProjects)
    char m_NewSceneName[128] = "Main";
    i32 m_SelectedTemplate = -1;
    u32 m_TemplateFilter = 0;             // 0 = All, bitmask for category filtering

    // Template category flags
    static constexpr u32 TMPL_ALL   = 0;
    static constexpr u32 TMPL_2D    = 1 << 0;
    static constexpr u32 TMPL_3D    = 1 << 1;
    static constexpr u32 TMPL_MULTI = 1 << 2;

    // Template search
    char m_TemplateSearchBuffer[64] = "";

    // Git init option for new projects
    bool m_GitInitOnCreate = true;

    // Template hover preview state
    i32 m_HoverTemplateIdx = -1;
    f32 m_HoverTimer = 0.0f;
    i32 m_HoverFrameIdx = 0;

    // Demos tab state
    std::vector<bool> m_DemoAvailability;
    bool m_DemosCacheValid = false;

    // Custom templates
    std::vector<std::string> m_CustomTemplateNames;
    std::vector<std::string> m_CustomTemplatePaths;

    // Project Hub methods
    void DrawProjectHub();
    void DrawHubRecentSidebar(ImDrawList* dl, const ImVec2& area, f32 contentY, f32 sidebarW);
    void DrawHubLandingPage(ImDrawList* dl, const ImVec2& area, f32 contentY, f32 sidebarW);
    void DrawHubWizardSetup(ImDrawList* dl, const ImVec2& area, f32 contentY, f32 sidebarW);
    void DrawHubWizardTemplate(ImDrawList* dl, const ImVec2& area, f32 contentY, f32 sidebarW);
    void DrawHubDemosTab(ImDrawList* dl, const ImVec2& area, f32 contentY, f32 sidebarW);
    void DrawTemplateHoverPreview(ImDrawList* dl, i32 templateIdx, const ImVec2& cardPos, const ImVec2& cardEnd);
    void ApplyTemplate(const std::string& templateId);
    void SaveCustomTemplate(const std::string& name);
    void LoadCustomTemplates();
    bool CreateProjectOnDisk(const std::string& projectDir, const std::string& projectName,
                             const std::string& sceneName, const std::string& templateId);

    // Docking layout
    bool m_DockingInitialized = false;

    // Per-template layout configuration
    struct LayoutConfig {
        f32 leftWidth   = 0.18f;   // Hierarchy panel width ratio
        f32 rightWidth  = 0.22f;   // Inspector panel width ratio
        f32 bottomHeight = 0.22f;  // Console/Assets height ratio
        f32 inspectorSplit = 0.6f; // Inspector vs Settings vertical split
        f32 gameViewX = -1.0f;     // Game View X (-1 = auto: leftWidth + 20px)
        f32 gameViewY = -1.0f;     // Game View Y (-1 = auto: menuBarH + 20px)
        f32 gameViewW = 500.0f;    // Game View width
        f32 gameViewH = 400.0f;    // Game View height
        EditorPanel panels = EditorPanel::All;  // Which panels to show
    };
    LayoutConfig m_Layout;
    bool m_ForceLayout = false;  // When true, override positions for one frame

    // Game View render targets (offscreen rendering for game camera)
    std::unique_ptr<Renderer::RenderTarget> m_GameViewRenderTarget;  // Final output (displayed in ImGui)
    std::unique_ptr<Renderer::RenderTarget> m_SceneRenderTarget;     // Scene pre-post-processing
    u32 m_GameViewWidth = 640;
    u32 m_GameViewHeight = 360;

    // Game View frame rate limiting (doesn't affect editor, only game view render)
    i32 m_GameViewFPSIndex = 0;  // 0=Unlimited, 1=24, 2=30, 3=60, 4=120, 5=144, 6=240
    bool m_GameViewVSync = false;  // Simulated VSync for game view (caps to ~60fps)
    f64 m_GameViewLastRenderTime = 0.0;  // For frame rate limiting

    // Render profiling (logs avg render time every 120 frames during play)
    f32 m_RenderProfileAccum = 0.0f;
    u32 m_RenderProfileFrames = 0;

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

    // Particle system (CPU simulation for ParticleEmitterComponent)
    Effects::ParticleSystem m_ParticleSystem;

    // Fluid simulation (Stable Fluids solver for FluidVolumeComponent)
    Effects::FluidSimulation m_FluidSimulation;

    // Curl noise flow field system
    std::unique_ptr<Effects::CurlNoiseSystem> m_CurlNoiseSystem;

    // World time and seasonal weather
    Effects::WorldTimeSystem m_WorldTime;
    Effects::SeasonalWeatherSystem m_SeasonalWeather;
    bool m_WorldTimeEnabled = false;
    bool m_SeasonalWeatherEnabled = false;

    // World curvature
    f32 m_WorldCurvature = 0.0f;
    bool m_WorldCurvatureEnabled = false;

    // Per-scene render settings
    bool m_CurrentSceneUsesProjectDefaults = true;
    Renderer::SceneRenderSettings m_PrePlayRenderSettings;

    // Terrain editing
    TerrainBrush m_TerrainBrush;
    bool m_TerrainEditMode = false;
    ECS::Entity m_TerrainEditTarget = 0;
    bool m_BrushActive = false;
    Math::Vector3 m_BrushHitPoint;
    bool m_BrushHitValid = false;
    i32 m_Dragging2DPoint = -1;  // Index of control point being dragged, -1 = none

    void HandleTerrainBrush(f32 deltaTime);
    bool RaycastTerrain(const Ray& ray, ECS::TerrainComponent* terrain,
                        const ECS::TransformComponent* transform, Math::Vector3& hitPoint);
    void ApplyBrush(ECS::TerrainComponent* terrain, const ECS::TransformComponent* transform,
                    const Math::Vector3& worldHit, f32 deltaTime);
    void ApplyBrush2D(ECS::Terrain2DComponent* terrain2d, const ECS::TransformComponent* transform,
                      const Math::Vector3& worldHit);

    // Draw camera frustum gizmo in editor view
    void DrawCameraFrustum(ECS::Entity cameraEntity);

    // Auto-create and configure a game camera when a character controller is added
    void SetupCameraForController(ECS::Entity controllerEntity, const std::string& controllerType);

    // Creative Intelligence — Smart Suggestions & Quick Setup
    void DrawSmartSuggestions(ECS::Entity entity);
    void DrawQuickSetup(ECS::Entity entity);
    bool EntityHasAnyController(ECS::Entity entity) const;
    ECS::Entity FindPlayerEntity() const;

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
    char m_AssetSearchBuf[256] = {};
    bool m_AssetGridView = true;          // true = grid with thumbnails, false = list
    f32 m_AssetThumbnailSize = 80.0f;     // Grid thumbnail size

    // Cached directory listing (avoid re-scan every frame)
    struct AssetEntry {
        std::string name;
        std::string fullPath;
        std::string extension;
        bool isDirectory = false;
        u64 fileSize = 0;
    };
    std::vector<AssetEntry> m_AssetBrowserCache;
    std::string m_AssetBrowserCachedPath;  // Path the cache was built for
    bool m_AssetBrowserCacheDirty = true;
    void RefreshAssetBrowserCache();

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

    // Runtime UI system
    GUI::UISystem m_UISystem;

    // Subtitle system (accessibility)
    Accessibility::SubtitleSystem m_SubtitleSystem;

    // Audio visual indicator system (accessibility)
    Accessibility::AudioVisualIndicatorSystem m_AudioIndicators;

    // Accessibility announcer (screen reader groundwork)
    Accessibility::AccessibilityAnnouncer m_Announcer;

    // Procedural generation graph editor
    ProceduralGraphEditor m_ProcGraphEditor;
    ProcGraphData m_ProcGraphData;

    // Command palette (Ctrl+P)
    CommandPalette m_CommandPalette;
    bool m_CommandsRegistered = false;
    void RegisterPaletteCommands();

    // Alternative input devices
    Accessibility::AlternativeInputManager m_AlternativeInput;

    // Import dialog state
    bool m_ShowImportDialog = false;
    std::string m_ImportDialogPath;
    Assets::ImportOptions m_ImportDialogOptions;
    std::string m_LastImportedModelPath;
    std::string m_ImportDialogFilename;   // Cached on dialog open
    std::string m_ImportDialogExtension;  // Cached on dialog open
    u64 m_ImportDialogFileSize = 0;       // Cached on dialog open
    bool m_ImportDialogIsReimport = false; // Cached on dialog open
    Assets::SourceApp m_ImportDialogDetectedApp = Assets::SourceApp::Auto;  // Auto-detected source app
    bool m_ImportDialogScaleFromPreset = false;  // True if scale was auto-filled from preset

    // Deferred import (renders one "Loading..." frame before the blocking import)
    bool m_ImportPending = false;
    std::string m_ImportPendingPath;
    Assets::ImportOptions m_ImportPendingOptions;

    void DrawImportDialog();
    void DrawImportLoadingOverlay();
    void ExecuteImport(const std::string& path, const Assets::ImportOptions& options);

    // Build dialog state
    bool m_ShowBuildDialog = false;
    Build::BuildConfig m_BuildConfig;
    Build::BuildResult m_BuildResult;
    bool m_BuildInProgress = false;
    bool m_BuildFinished = false;
    float m_BuildProgress = 0.0f;
    std::string m_BuildProgressPhase;

    // ImGui texture descriptor cache for sprite/tilemap previews
    std::unordered_map<std::string, VkDescriptorSet> m_ImGuiTextureCache;
    VkDescriptorSet GetImGuiTexture(const std::string& path);
    void CleanupImGuiTextureCache();

    // Thumbnail generator for non-image assets (3D models, scenes, etc.)
    Assets::ThumbnailGenerator m_ThumbnailGenerator;
    std::vector<std::shared_ptr<Renderer::Texture>> m_ThumbnailTextures; // Keep GPU textures alive
    VkDescriptorSet GetAssetThumbnail(const std::string& path);

    // Sprite frame picker state
    f32 m_SpriteFramePickerW = 32.0f;
    f32 m_SpriteFramePickerH = 32.0f;

    // Auto-slice state
    f32 m_AutoSliceWidth = 32.0f;
    f32 m_AutoSliceHeight = 32.0f;
    f32 m_AutoSliceDuration = 0.1f;
    i32 m_AutoSliceCount = 0;

    // Script creation popup state
    bool m_ShowCreateScriptPopup = false;
    char m_NewScriptNameBuf[128] = "";
    std::string m_NewScriptNameError;
    void OpenInExternalIDE(const std::string& filePath);

    // Tilemap editor state
    bool m_TilemapEditMode = false;
    i32 m_TileBrushIndex = 0;
    void HandleTilemapBrush();

    // Tilemap brush undo state
    bool m_TilemapBrushActive = false;
    std::vector<TilemapTileChange> m_TilemapPaintChanges;
    std::unordered_map<u64, usize> m_TilemapPaintCellIndex; // (y*65536+x) -> index into m_TilemapPaintChanges

    // Terrain sculpt undo state
    std::vector<f32> m_TerrainUndoHeightmapSnapshot;
    std::vector<f32> m_TerrainUndoSplatmapSnapshot;

    // UI editor (viewport WYSIWYG) state
    bool m_UIEditMode = false;
    u32 m_UIEditSelectedElementId = 0;
    ECS::Entity m_UIEditCanvasEntity = ECS::INVALID_ENTITY;
    UIEditDragMode m_UIEditDragMode = UIEditDragMode::None;
    ImVec2 m_UIEditDragStart = {0, 0};
    GUI::UIAnchor m_UIEditDragStartAnchor;

    // Pixel editor
    PixelEditor m_PixelEditor;

    // Data Asset editor state
    char m_DataAssetSchemaSearchBuf[128] = {};
    char m_DataAssetSearchBuf[128] = {};
    std::string m_SelectedSchemaName;
    std::string m_SelectedAssetName;
    bool m_EditingSchema = false;
    std::string m_NewSchemaName;
    std::string m_NewAssetName;

    // Documentation generator
    DocGenerator m_DocGenerator;

    // Plugin repository
    Plugin::PluginRepository m_PluginRepository;
    char m_PluginSearchBuf[128] = {};
    std::string m_PluginCategoryFilter;
    bool m_PluginShowInstalledOnly = false;

    // Sprite sheet importer
    SpriteSheetImporter m_SpriteSheetImporter;
    u32 m_SpriteSheetGridW = 32;
    u32 m_SpriteSheetGridH = 32;
    u32 m_SpriteSheetPadding = 0;
    SpriteSheetImportResult m_SpriteSheetResult;
    bool m_SpriteSheetUseAutoDetect = false;

    // Procedural generation panel state
    i32 m_ProceduralAlgorithm = 0;      // Algorithm dropdown index
    u32 m_ProceduralSeed = 0;           // 0 = random
    Procedural::CellularAutomata::Params m_CAParams;
    Procedural::RandomWalker::Params m_RWParams;
    Procedural::BSPGenerator::Params m_BSPParams;
    Procedural::DiamondSquare::Params m_DSParams;
    Procedural::LSystemGenerator::Params m_LSParams;
    Procedural::WaveFunctionCollapse::Params m_WFCParams;
    Procedural::VoronoiGenerator::Params m_VoronoiParams;
    Procedural::GrammarGenerator::Params m_GrammarParams;
    Procedural::PrefabAssembler::Params m_PAParams;
    std::vector<std::vector<u8>> m_ProceduralPreview;    // 2D grid preview
    std::vector<std::vector<f32>> m_ProceduralHeightmap; // Heightmap preview
    u32 m_ProceduralPreviewW = 0, m_ProceduralPreviewH = 0;
    bool m_ProceduralPreviewDirty = true;

    // Component search popup state
    char m_ComponentSearchBuf[256] = {};
    int m_ComponentSearchSelectedIndex = -1;
    bool m_ShowAllComponents = false;

    // Network panel state
    char m_NetworkIP[64] = "127.0.0.1";
    i32 m_NetworkPort = 7777;
    char m_NetworkPlayerName[64] = "Player";

    // Scene & Entity Locking
    SceneLockManager m_SceneLockManager;
    f32 m_LockRefreshTimer = 0.0f;

    // Collaborative Editing
    CollaborativeEditingSystem m_CollabSystem;
    char m_CollabHostIP[64] = "127.0.0.1";
    i32 m_CollabPort = 7778;
    char m_CollabUserName[64] = {};
    void DrawCollaborationPanel();

    // Flash Timeline Editor
    FlashTimelineEditor m_FlashTimelineEditor;
    FlashTimelineData m_FlashTimelineData;
    Scripting::AS3Transpiler m_AS3Transpiler;
    char m_AS3TranspileInput[4096] = {};
    std::string m_AS3TranspileOutput;
    void DrawFlashTimelinePanel();

    // Vector Drawing Editor
    VectorDrawingEditor m_VectorDrawingEditor;
    void DrawVectorDrawingPanel();

    // Feedback / Bug Reporting System
    FeedbackManager m_FeedbackManager;
    bool m_FeedbackLoaded = false;

    enum class FeedbackTab : u8 { BugReports, Feedback, NewBug, NewFeedback };
    FeedbackTab m_FeedbackTab = FeedbackTab::BugReports;

    // Bug report form buffers
    char m_BugTitleBuf[128] = {};
    char m_BugDescriptionBuf[4096] = {};
    char m_BugStepsBuf[2048] = {};
    char m_BugExpectedBuf[1024] = {};
    char m_BugActualBuf[1024] = {};
    i32 m_BugTypeSel = 0;
    i32 m_BugSeveritySel = 1;
    bool m_BugIncludeScene = false;
    bool m_BugIncludeLogs = true;

    // Feedback form buffers
    char m_FeedbackTitleBuf[128] = {};
    char m_FeedbackDescBuf[4096] = {};
    i32 m_FeedbackTypeSel = 0;
    i32 m_FeedbackPrioritySel = 1;
    i32 m_FeedbackSatisfaction = 0;
    bool m_FeedbackIncludeDiag = false;
    char m_FeedbackCategoryBuf[64] = {};

    // Browse / filter state
    char m_FeedbackSearchBuf[256] = {};
    i32 m_BugStatusFilter = -1;
    i32 m_BugSeverityFilter = -1;
    u64 m_SelectedBugReportId = 0;
    u64 m_SelectedFeedbackId = 0;
    char m_FeedbackEndpointBuf[512] = {};

    void DrawFeedbackPanel();
    void DrawSaveDebugPanel();
    void DrawPlayModeDiffDialog();
    void DrawBugReportList();
    void DrawBugReportDetail(BugReport& report);
    void DrawNewBugReportForm();
    void DrawFeedbackList();
    void DrawFeedbackDetail(FeedbackEntry& entry);
    void DrawNewFeedbackForm();
    void ResetBugReportForm();
    void ResetFeedbackForm();
    DiagnosticSnapshot CaptureDiagnostics(bool includeScene);

    // Newgrounds API
    Networking::NewgroundsAPI m_NewgroundsAPI;
    char m_NGAppId[64] = {};
    char m_NGEncryptionKey[128] = {};

    // HTML5 Export
    Build::HTML5ExportConfig m_HTML5Config;
    bool m_ShowHTML5ExportDialog = false;
    void DrawHTML5ExportDialog();

    // Keyboard navigation state
    enum class FocusedPanel : u8 {
        None = 0, Hierarchy, Inspector, Viewport, Console, AssetBrowser
    };
    FocusedPanel m_FocusedPanel = FocusedPanel::None;
    bool m_ShowFocusRing = false;

    // Dwell-click state
    ImVec2 m_DwellPos = {0, 0};
    f32 m_DwellTimer = 0.0f;
    bool m_DwellActive = false;

    // Keyboard gizmo nudge
    void HandleKeyboardGizmoNudge();

    // Git integration state
    bool m_GitAvailable = false;
    std::string m_GitBranch;
    std::string m_GitRepoRoot;
    std::vector<GitFileStatus> m_GitFiles;
    std::vector<GitLogEntry> m_GitLog;
    std::vector<std::string> m_GitBranches;
    char m_GitCommitMsg[1024] = {};
    f32 m_GitRefreshTimer = 0.0f;
    bool m_GitNeedsRefresh = true;
    std::string m_GitLastError;

    void DrawUIEditorOverlay();
    void HandleUIEditorInput();
    void UIEditorScreenToDesign(f32 screenX, f32 screenY, f32& designX, f32& designY);
    void UIEditorDesignToScreen(f32 designX, f32 designY, f32& screenX, f32& screenY);
    UIEditDragMode UIEditorHitTestHandles(f32 localX, f32 localY, const GUI::UIRect& rect);
};

} // namespace Editor
} // namespace Enjin
