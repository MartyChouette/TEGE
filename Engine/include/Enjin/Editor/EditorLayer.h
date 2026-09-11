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
#include "Enjin/Editor/EditorWatch.h"
#include "Enjin/Editor/PlayMode.h"
#include "Enjin/Editor/EditorSettings.h"
#include "Enjin/Editor/GifRecorder.h"
#include "Enjin/Editor/McpServer.h"
#include "Enjin/Debug/Profiler.h"
#include "Enjin/Logging/Log.h"
#include "Enjin/Input/InputAction.h"
#include "Enjin/ECS/Components/BrushSolid.h"
#include "Enjin/GUI/GameMenus.h"
#include "Enjin/GUI/UISystem.h"
#include "Enjin/Accessibility/SubtitleSystem.h"
#include "Enjin/Accessibility/AudioVisualIndicator.h"
#include "Enjin/Accessibility/Announcer.h"
#include "Enjin/Editor/ProceduralGraph.h"
#include "Enjin/Editor/CommandPalette.h"
#include "Enjin/Accessibility/AlternativeInput.h"
#include "Enjin/Accessibility/AccessibilitySettings.h"
#include "Enjin/Accessibility/ContentWarning.h"
#include "Enjin/Networking/DevWebServer.h"
#include "Enjin/Effects/Weather.h"
#include "Enjin/Effects/Water.h"
#include "Enjin/Effects/Wind.h"
#include "Enjin/Effects/RetroEffects.h"
#include "Enjin/Effects/WorldTime.h"
#include "Enjin/Effects/SeasonalWeather.h"
#include "Enjin/Effects/ParticleSystem.h"
#include "Enjin/ECS/Systems/ParallaxSystem.h"
#include "Enjin/Effects/ElementalSystem.h"
#include "Enjin/Effects/FluidSimulation.h"
#include "Enjin/Effects/FluidTerrainCoupling.h"
#include "Enjin/Effects/CurlNoiseSystem.h"
#include "Enjin/Scene/SceneManager.h"
#include "Enjin/Scene/LayerSystem.h"
#include <thread>
#include <atomic>
#include <mutex>
#include "Enjin/ECS/Components/CineComponent.h"
#include "Enjin/Renderer/SceneRenderSettings.h"
#include "Enjin/Editor/PerformanceStats.h"
#include "Enjin/Editor/TelemetrySystem.h"
#include "Enjin/Editor/TerrainBrush.h"
#include "Enjin/Editor/ScenePicker.h"
#include "Enjin/Editor/UndoRedo.h"
#include "Enjin/Editor/CreativeMode.h"
#include "Enjin/Build/BuildReport.h"
#include "Enjin/Assets/AssetMetadata.h"
#include "Enjin/Assets/ThumbnailGenerator.h"
#include "Enjin/Assets/TextureCompressor.h"
#include "Enjin/Assets/FontLibrary.h"
#include "Enjin/Assets/AssetLibrary.h"
#include "Enjin/Renderer/Texture.h"
#include "Enjin/GUI/DialogueTree.h"
#include "Enjin/Editor/AnimationGraphEditor.h"
#include "Enjin/Editor/VisualScriptEditor.h"
#include "Enjin/Editor/SpriteSheetImporter.h"
#include "Enjin/Editor/PixelEditor.h"
#include "Enjin/Editor/SpriteColliderGenerator.h"
#include "Enjin/Editor/BehaviorTreeEditor.h"
#include "Enjin/Editor/QuestFlowEditor.h"
#include "Enjin/Editor/ShaderGraph.h"
#include "Enjin/Editor/AudioEventGraph.h"
#include "Enjin/Editor/ParticleGraph.h"
#include "Enjin/Editor/DocGenerator.h"
#include "Enjin/Assets/SrtImport.h"
#include "Enjin/Editor/SceneLock.h"
#include "Enjin/Editor/CollaborativeEditing.h"
#include "Enjin/Editor/EditorShortcuts.h"
#include "Enjin/Editor/FlashTimeline.h"
#include "Enjin/Editor/SymbolLibrary.h"
#include "Enjin/Editor/VectorDrawingEditor.h"
#include "Enjin/Renderer/LightCookie.h"   // CookieParams for the Cookie Creator
#include "Enjin/Editor/FeedbackSystem.h"
#include "Enjin/Editor/TemplateCreator.h"
#include "Enjin/Editor/TemplateMarketplace.h"
#include "Enjin/Scripting/AS3Transpiler.h"
#include "Enjin/Networking/NetworkTypes.h"
#include "Enjin/Build/HTML5Exporter.h"
#include "Enjin/Plugin/PluginRepository.h"
#include "Enjin/Procedural/ProceduralAlgorithms.h"
#include <string>
#include <functional>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <chrono>
#include <filesystem>
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
enum class EditorPanel : u64 {
    None = 0,
    Hierarchy = 1ull << 0,
    Inspector = 1ull << 1,
    Viewport = 1ull << 2,
    Console = 1ull << 3,
    AssetBrowser = 1ull << 4,
    EditorSettings = 1ull << 5,
    PostProcessing = 1ull << 6,
    RetroEffects = 1ull << 7,
    GameView = 1ull << 8,
    SceneList = 1ull << 9,
    Rendering = 1ull << 10,
    Profiler = 1ull << 11,
    ProjectSettings = 1ull << 12,
    ParticleEditor = 1ull << 13,
    AnimGraph = 1ull << 14,
    Dialogue = 1ull << 15,
    VisualScript = 1ull << 16,
    SpriteSheetImport = 1ull << 17,
    PixelEditorPanel = 1ull << 18,
    BehaviorTree = 1ull << 19,
    QuestFlow = 1ull << 20,
    UserManual = 1ull << 21,
    DataAssets = 1ull << 22,
    PluginBrowser = 1ull << 23,
    ProceduralGen = 1ull << 24,
    GitIntegration = 1ull << 25,
    NetworkPanel = 1ull << 26,
    Collaboration = 1ull << 27,
    FlashTimeline = 1ull << 28,
    VectorDrawing = 1ull << 29,
    FeedbackPanel = 1ull << 30,
    SaveDebug = 1ull << 31,
    CaptionTrack = 1ull << 32,
    SymbolLibraryPanel = 1ull << 33,
    All = 0xFFFFFFFFFFFFFFFFull
};

inline EditorPanel operator|(EditorPanel a, EditorPanel b) {
    return static_cast<EditorPanel>(static_cast<u64>(a) | static_cast<u64>(b));
}

inline EditorPanel operator&(EditorPanel a, EditorPanel b) {
    return static_cast<EditorPanel>(static_cast<u64>(a) & static_cast<u64>(b));
}

inline bool HasPanel(EditorPanel flags, EditorPanel panel) {
    return (static_cast<u64>(flags) & static_cast<u64>(panel)) != 0;
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
    // Set by main() before Run() — if non-empty, opens this project directly (skips hub)
    static inline std::string s_LaunchProjectPath;
    // Set by main() from --play: auto-enter play mode shortly after the launch
    // project's scene loads. For automated testing (e.g. validation probes).
    // s_AutoPlayOnLaunch clears once play fires; s_AutoPlayRequested persists
    // so the --golden counter knows to wait for play mode before counting.
    static inline bool s_AutoPlayOnLaunch = false;
    // --creative: open straight into the build surface. Lets a person launch
    // into creative mode without hunting for it, and makes the surface
    // capturable by the headless --golden harness, which is the only way to
    // look at it without a human at the keyboard.
    static inline bool s_StartInCreativeMode = false;
    static inline bool s_AutoPlayRequested = false;
    // Set by main() from --play-cycle <N>: stop/restart play mode every N
    // frames (skinned-mesh play-transition crash probe). 0 = off.
    static inline i32 s_PlayCycleFrames = 0;
    // T4 stress: stop after this many play/stop cycles and exit with a code
    // (0 = healthy). 0 = cycle forever (the original probe behavior).
    static inline i32 s_PlayCycleMax = 0;
    static inline i32 s_PlayCycleExitCode = 0;
    // Set by main() from --compute-skinning: force ADR-0002 compute skinning on
    // at boot. For automated verification probes.
    static inline bool s_ComputeSkinningOnLaunch = false;
    // Set by main() from --golden <basePath>: after s_GoldenCaptureFrame
    // frames, the game view render target is read back and written as
    // <basePath>.png (human diffing) + <basePath>.ppm (dependency-free
    // comparer), then the editor exits. Golden-image regression harness
    // (tools/probes/golden.ps1). Combine with --play to capture play-mode frames.
    // Load the shipped templates and report them, without a window or a GPU.
    // Deleting the generators removed the only way to check the roster from a
    // script, and "do the shipped templates still parse" is exactly the question
    // CI should be able to ask.
    static int ValidateBuiltinTemplates();

    static inline std::string s_GoldenCapturePath;
    // --bake-plate <name>: bake a background plate from the launch scene, then
    // exit. Exists so the bake has a path that is not a mouse click.
    static inline std::string s_BakePlateName;
    // --bake-lightmap: bake the launch scene's lightmap, save, and exit.
    static inline bool s_BakeLightmapOnLaunch = false;
    static inline i32 s_GoldenCaptureFrame = 180;

    EditorLayer();
    ~EditorLayer();

    bool Initialize(Window* window, Renderer::VulkanRenderer* renderer);
    void Shutdown();

    void Update(f32 deltaTime);
    void PrepareRenderTargets();                           // Call BEFORE command buffer recording
    void RenderOffscreen(VkCommandBuffer commandBuffer);  // Call BEFORE main render pass
    void Render(VkCommandBuffer commandBuffer);            // Call DURING main render pass

    // Set the world to edit
    void SetWorld(ECS::World* world) { m_World = world; m_SceneManager.SetWorld(world); m_ParallaxSystem.SetWorld(world); m_LayerSystem.SetWorld(world); }
    ECS::World* GetWorld() const { return m_World; }

    // Set the camera for the viewport
    void SetCamera(Renderer::Camera* camera) { m_Camera = camera; m_ParallaxSystem.SetCamera(camera); InitializePlayMode(); }
    void SetCameraController(Renderer::CameraController* controller) {
        m_CameraController = controller;
        // The fly camera follows the action map's mouse sensitivity, the same
        // value play mode and the Controls menu use.
        if (m_CameraController && m_InputMap.GetMouseSensitivity() != 1.0f) {
            m_CameraController->SetLookSensitivity(m_InputMap.GetMouseSensitivity() * 0.1f);
        }
        InitializePlayMode();
    }

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
    McpServer& GetMcpServer() { return m_McpServer; }

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

    // Unified settings window
    void OpenSettings(int tab);

    // Multi-select entity management
    void SelectEntity(ECS::Entity entity, bool addToSelection = false);
    // Jump straight into WYSIWYG UI editing for a canvas entity (selects the
    // entity so the inspector shows its properties, enables Edit-in-Viewport).
    void OpenUIEditor(ECS::Entity canvasEntity);
    void DeselectEntity(ECS::Entity entity);
    void ClearSelection();
    void SelectRange(ECS::Entity from, ECS::Entity to);
    bool IsSelected(ECS::Entity entity) const;
    const std::unordered_set<ECS::Entity>& GetSelectedEntities() const { return m_SelectedEntities; }
    void SelectEntitiesInRect(ImVec2 min, ImVec2 max);

    // Backward-compatible single-entity API (returns/sets primary)
    ECS::Entity GetSelectedEntity() const { return m_PrimarySelected; }
    void SetSelectedEntity(ECS::Entity entity);

    // Unsaved changes tracking
    bool IsSceneDirty() const { return m_SceneDirty; }
    void MarkDirty();
    void ClearDirty();

    // Callbacks
    using EntitySelectedCallback = std::function<void(ECS::Entity)>;
    void SetEntitySelectedCallback(EntitySelectedCallback callback) { m_OnEntitySelected = callback; }

    // Check if UI wants input (for disabling camera when interacting with UI)
    bool WantsKeyboardInput() const;
    bool WantsMouseInput() const;
    bool IsEditorViewportHovered() const { return m_EditorViewportHovered; }

    // Push a message to the editor console (used by Logger callback)
    void PushConsoleMessage(const std::string& message);
    void PushConsoleMessage(LogLevel level, LogCategory category, const std::string& message);

private:
    // Atlas packer tool (Tools menu): packs a folder of images into one atlas
    // texture + a .atlas.json region map for the material Atlas Region UI.
    void DrawAtlasPackerWindow();
    bool m_ShowAtlasPacker = false;

    // --- Cookie Creator (light gobos) ---
    // A plain bool rather than an EditorPanel bit. The original reason was that
    // the mask was a u32 and full; it is a u64 now, so a bit is available if this
    // ever wants its visibility saved and restored like a real panel.
    bool m_ShowCookieCreator = false;

    // Pre-rendered background baking. The plate is produced from the Game
    // View, so there is no separate preview to hold here -- only the name and
    // whatever the last bake had to say.
    bool m_ShowPlateBaker = false;

    // Lightmap baking.
    bool m_ShowLightmapBaker = false;
    std::string m_LightmapBakeName = "scene";
    std::string m_LightmapBakeStatus;
    // 1024 at 4 texels per unit, because the first thing tried at 512/8 was a
    // 16x20 room and it ran out of atlas at triangle 14 of 36. Baked light is
    // meant to be soft; density buys detail a normal map is better at, and the
    // failure of guessing too high is a bake that refuses to run.
    u32 m_LightmapAtlasSize = 1024;
    f32 m_LightmapTexelsPerUnit = 4.0f;
    u32 m_LightmapSkySamples = 32;
    std::string m_PlateBakeName = "plate";
    std::string m_PlateBakeStatus;
    int m_PlateBakeFrameCounter = 0;
    int m_LightmapBakeFrameCounter = 0;
    Renderer::CookieParams m_CookieDraft;
    std::vector<u8> m_CookiePreview;         // regenerated only when the draft changes
    Renderer::CookieParams m_CookiePreviewOf; // what m_CookiePreview was built from
    bool m_CookiePreviewValid = false;
    std::string m_CookieSaveName = "cookie";
    std::string m_CookieStatus;              // last save result, shown in the window
    char m_AtlasInputDir[512] = {};
    char m_AtlasOutputName[128] = "atlas";
    int m_AtlasSize = 2048;
    std::string m_AtlasStatus;

    // Loaded atlas region map for the material inspector
    std::string m_LoadedAtlasPath;
    std::string m_LoadedAtlasImage;
    std::vector<std::pair<std::string, Math::Vector4>> m_LoadedAtlasRegions;  // name -> x,y,w,h (normalized)

    void InitializePlayMode();
    void StartPlayMode();  // Starts play mode and applies game VSync settings

    // Pause root, spawned from the shipped UICanvas template so the editor
    // shows the menu an exported game shows. Both are safe to call twice.
    void OpenPauseMenu();
    void ClosePauseMenu();
    void DrawMenuBar();
    // --- Creative mode (option B: a hand-drawn surface hosted by ImGui) ---
    // The rail, the options column and the mode toggle, all ImDrawList.
    void DrawCreativeSurface();
    // How wide the surface is drawn at the editor's current UI scale. The
    // dockspace is inset by exactly this, so the two cannot disagree.
    f32 CreativeSurfaceWidthPx() const;
    // Where a screen point lands on the y = 0 build plane. False when the ray
    // runs parallel to the plane or points away from it, rather than returning
    // a placement at infinity.
    bool CreativeGroundPoint(f32 screenX, f32 screenY, f32 viewW, f32 viewH,
                             Math::Vector3& out) const;
    // Grid/snap state, brush and triangle counts, the active tool, and the
    // first-run hint -- drawn over the viewport, inside the ImGui frame.
    void DrawCreativeOverlay(const ImVec2& imgMin, const ImVec2& imgMax);
    // Every creative gesture, dispatched by tool. Must run inside the ImGui
    // frame: it hit-tests the mouse and draws into the foreground draw list.
    void HandleBuildDrag();
    // Turn a finished press-drag-release into a thing: a new brush solid, a cut
    // into the selected one, or a placed component. Undoable in every case.
    void CommitCreativeDrag(const Math::Vector3& start, const Math::Vector3& end);
    // Water and Ladder: a component placed from the drag's footprint rather than
    // brushes built from it. Returns the new entity, or INVALID_ENTITY.
    ECS::Entity PlaceCreativeComponent(BuildTool tool, const Math::Vector3& start,
                                       const Math::Vector3& end);
    // One undo step per placement, snapshotted after the components are on.
    void FinishCreativePlacement(ECS::Entity entity);
    // Terrain: a press-and-hold sculpt rather than a drag that commits on
    // release. Drives the same ApplyBrush the inspector's Edit Mode does, so
    // there is one sculpt implementation and not two.
    void HandleCreativeTerrain(const Math::Vector3& ground, bool onGround,
                               f32 localX, f32 localY, f32 viewW, f32 viewH);
    // Reduce: a click on whatever model is under the cursor, not a drag.
    void HandleCreativeReduce(f32 localX, f32 localY, f32 viewW, f32 viewH);
    // Edit: click to select something you built, then drag one of the eight
    // grips on its footprint to resize it. The only tool where a viewport click
    // selects instead of building.
    void HandleCreativeEdit(f32 localX, f32 localY, f32 viewW, f32 viewH,
                            const Math::Vector3& ground, bool onGround);
    // Path: click corners, pull a span sideways to bow it, Enter to finish. The
    // first creative gesture that is not press-drag-release, so it carries state
    // between frames rather than living inside one drag.
    void HandleCreativePath(f32 viewW, f32 viewH, const Math::Vector3& ground, bool onGround);
    void CommitCreativePath();
    // End an in-flight drag or sculpt without committing it. Needed because a
    // mouse release is not guaranteed to arrive: see the definition.
    void CancelCreativeGesture();

    // Distinct from the older Build palette below (m_ShowCreativePalette and its
    // own nested CreativeTool enum), which places pre-made objects. These are the
    // brush build tools. The two overlap and should be folded together.
    CreativeMode m_Creative;
    bool m_BuildDragging = false;
    Math::Vector3 m_BuildDragStart;
    // Rising-edge detection for entering creative mode, and the countdown that
    // pulls the Scene tab in front when it does.
    bool m_CreativeWasActive = false;
    i32 m_CreativeFocusSceneFrames = 0;
    // The terrain the creative Terrain tool is sculpting. Held across the stroke
    // so a drag that wanders off the heightmap does not re-pick a different
    // terrain halfway through, and so the undo snapshot belongs to one entity.
    ECS::Entity m_CreativeTerrainTarget = ECS::INVALID_ENTITY;
    // The grip being dragged, and the brush list as it was when the drag began.
    // The snapshot is what makes a resize ONE undo entry instead of one per
    // frame the mouse moved.
    i32 m_CreativeGrip = -1;
    std::vector<ECS::BrushSolidComponent::Brush> m_CreativeGripStart;
    // The path being clicked out, one bow per span, and which bow handle is
    // being dragged. Non-empty points mean a path is in progress.
    std::vector<Math::Vector3> m_CreativePathPoints;
    std::vector<f32> m_CreativePathBows;
    i32 m_CreativePathBow = -1;

    void DrawHierarchyPanel();
    void DrawInspectorPanel();
    void DrawViewportPanel();
    void DrawConsolePanel();
    void DrawLayersPanel();  // VWS override-layer management (list/add/enable/lock/active/resolve)
    void DrawAssetBrowserPanel();
    void DrawPostProcessVolumeComponent(ECS::Entity entity);
    void DrawArtStyleComponent(ECS::Entity entity);
    void EvaluatePostProcessVolumes(const Math::Vector3& cameraPosition);
    void DrawGameViewPanel();
    void DrawSceneListPanel();

    // Unified settings window (3 tabs: System / Project / Scene)
    void DrawSettingsWindow();

    // Settings section drawers (extracted from monolithic panel functions)
    // System tab sections
    void DrawSettingsSection_Camera();
    void DrawSettingsSection_EditorPerformance();
    void DrawSettingsSection_ExternalIDE();
    void DrawSettingsSection_BugReporting();
    void DrawSettingsSection_Accessibility();
    void DrawSettingsSection_Fonts();
    // Sync editor settings to RuntimeAccessibilitySettings for PlayMode/scripting
    void SyncRuntimeAccessibility();
    // Project tab sections
    void DrawSettingsSection_ProjectMode();
    void DrawSettingsSection_WindowIcon();
    void DrawSettingsSection_Physics();
    void DrawSettingsSection_FrameRate();
    void DrawSettingsSection_Audio();
    void DrawSettingsSection_CollisionGroups();
    void DrawSettingsSection_BuildScenes();
    void DrawSettingsSection_StartupFlow();
    void DrawSettingsSection_InputTouch();
    void DrawSettingsSection_AccessibilityDefaults();
    void DrawSettingsSection_RenderQuality();
    void DrawSettingsSection_ScenePalette();
    void DrawSettingsSection_BuildConfig();
    void DrawSettingsSection_Networking();
    // Scene tab sections
    void DrawSettingsSection_ArtStylePreset();
    void DrawSettingsSection_Skybox();
    void DrawSettingsSection_Shadows();
    void DrawSettingsSection_AmbientLighting();
    void DrawSettingsSection_ShadingModel();
    void DrawSettingsSection_DreamcastEffects();
    void DrawSettingsSection_CelShading();
    void DrawSettingsSection_DisplayOptions();
    void DrawSettingsSection_RayTracing();
    void DrawSettingsSection_LightProbes();
    void DrawSettingsSection_PostProcessing();
    void DrawSettingsSection_RetroEffects();
    void DrawSettingsSection_Environment();
    void DrawSettingsConflictWarnings();
    void DrawParticleEditorPanel();
    void DrawAnimGraphPanel();
    void DrawDialoguePanel();
    void DrawVisualScriptPanel();
    void DrawSpriteSheetImporterPanel();
    void DrawPixelEditorPanel();
    // Build and preview light cookies (gobos), then apply one to a spot light.
    void DrawCookieCreatorWindow();
    void DrawBackgroundPlateBakerWindow();
    void DrawLightmapBakerWindow();
    // Collects opted-in geometry, unwraps it, traces the light, writes three
    // atlases into the project and points the scene at them.
    bool BakeSceneLightmap(std::string& outStatus);
    // Captures the Game View's colour and depth, writes both to the project,
    // and attaches them to the active camera. Returns false with a reason.
    bool BakeBackgroundPlate(std::string& outStatus);
    // Shared by the creator and the Light inspector section, so a cookie edited
    // in either place looks the same.
    void DrawCookiePreview(const std::vector<u8>& pixels, u32 res, f32 sizePx);
    void DrawBehaviorTreePanel();
    void DrawQuestFlowPanel();
    void DrawUserManualPanel();
    void DrawDataAssetPanel();

    // A list view that came back empty has to say WHICH empty it is. "Nothing
    // matches your filter", "I looked here and there was nothing", and "I have
    // never looked" are three different facts, and rendering them as the same
    // blank turns every bug in the panel into an investigation -- an empty Data
    // Assets panel cost a whole session on 2026-09-10 because a scan of the
    // wrong directory was indistinguishable from a project with no records.
    //
    //   what     what the list holds, lowercase plural ("Dictation records")
    //   source   where they were looked for; shown verbatim, so make it a path
    //            or a name the reader can act on. Empty = nowhere to look yet.
    //   filtered true when a search box is non-empty and hid everything
    static void DrawEmptyListState(const char* what, const std::string& source,
                                   bool filtered = false);

    // Re-scan the project's .enjschema/.enjdata when the tree moves on disk.
    // Registers the watch lazily against the open project, and re-registers when
    // the open project changes. This is what makes the Data Assets panel correct
    // without a Refresh button: opening the project loads the records, and an
    // edit in an external editor shows up within a second.
    void ReloadDataAssetsIfChangedOnDisk();
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
    void DrawDebugWorkstation();   // F2 — Editor/Engine debug (legacy)
    void DrawGameDebugPanel();     // F1 — Game debug (legacy)
    void DrawDebugOverlay();       // F1 — Transparent HUD overlay
    void DrawDebugModeIndicator(); // Status bar indicator for F1/F2 debug groups
    void ToggleGameDebug();        // F1 — Toggle game debug panels
    void ToggleEngineDebug();      // F2 — Toggle engine debug panels
    void DrawSplashScreen();
    void DrawBuildDialog();

    void DrawEmptyState(const char* icon, const char* heading, const char* body, const char* ctaLabel = nullptr, std::function<void()> ctaAction = nullptr);
    void DrawEntityNode(ECS::Entity entity, const std::string& name);
    void DrawTransformComponent(ECS::Entity entity);
    void DrawMeshComponent(ECS::Entity entity);
    void DrawLODComponent(ECS::Entity entity);
    void DrawMaterialComponent(ECS::Entity entity);
    void DrawMaterialSlotsComponent(ECS::Entity entity);
    void DrawLightComponent(ECS::Entity entity);
    void DrawCameraComponent(ECS::Entity entity);
    void DrawNotesComponent(ECS::Entity entity);
    void DrawPreRenderedBackgroundComponent(ECS::Entity entity);
    void DrawHoverHighlightComponent(ECS::Entity entity);
    void DrawTextComponent(ECS::Entity entity);
    void DrawDisplayGraphicComponent(ECS::Entity entity);
    void DrawWeatherZoneComponent(ECS::Entity entity);
    void DrawWaterVolumeComponent(ECS::Entity entity);
    void DrawWater3DComponent(ECS::Entity entity);
    void DrawGaussianSplatComponent(ECS::Entity entity);
    void DrawGrassVolumeComponent(ECS::Entity entity);
    void DrawShrubVolumeComponent(ECS::Entity entity);
    void DrawTreeVolumeComponent(ECS::Entity entity);
    void DrawVegetationComponent(ECS::Entity entity);
    void DrawViewmodelComponent(ECS::Entity entity);
    void DrawCameraTriggerComponent(ECS::Entity entity);
    void DrawTemperatureZoneComponent(ECS::Entity entity);
    void DrawGravityZoneComponent(ECS::Entity entity);
    void DrawReflectionProbeComponent(ECS::Entity entity);
    void DrawReflectivePlaneComponent(ECS::Entity entity);
    void DrawActionTriggerComponent(ECS::Entity entity);
    void DrawBrushSolidComponent(ECS::Entity entity);

    // One undo command for any brush-list change, so the inspector rows and the
    // viewport gizmo cannot drift apart on what Ctrl+Z does. Snapshots the whole
    // vector: a per-brush command would need stable identities the list does not
    // have, since indices shift the moment anything is removed.
    void PushBrushListUndo(ECS::Entity entity, const char* desc,
                           std::vector<ECS::BrushSolidComponent::Brush> before,
                           std::vector<ECS::BrushSolidComponent::Brush> after);

    // Brush gizmo drag state, mirroring m_GizmoDragging for the entity gizmo:
    // one undo entry per gesture, not per manipulated frame.
    // Name at the moment the entity-name box was focused, so a rename is one
    // undo entry per commit rather than one per keystroke.
    std::string m_RenameStartName;

    // Every selected entity's transform at the start of a multi-select gizmo
    // drag, so the gesture undoes as one compound step.
    std::vector<std::pair<ECS::Entity, ECS::TransformComponent>> m_MultiDragStart;

    bool m_BrushGizmoDragging = false;
    std::vector<ECS::BrushSolidComponent::Brush> m_BrushGizmoStart;
    void DrawLadderComponent(ECS::Entity entity);
    void DrawRopeComponent(ECS::Entity entity);
    void DrawDoorComponent(ECS::Entity entity);
    void DrawFluidVolumeComponent(ECS::Entity entity);
    void DrawFluidTerrainCoupling(ECS::Entity entity);
    void DrawElementalSurfaceComponent(ECS::Entity entity);
    void DrawElementalEmitterComponent(ECS::Entity entity);
    void DrawElementalVolumeComponent(ECS::Entity entity);

    // Controller components
    void DrawPlatformer2DController(ECS::Entity entity);
    void DrawTopDown2DController(ECS::Entity entity);
    void DrawTopDown3DController(ECS::Entity entity);
    // Swim tuning lives on CharacterControllerBase, so every controller that
    // can swim draws the SAME section instead of each repeating the fields.
    void DrawSwimSettings(ECS::CharacterControllerBase& ctrl, const char* idSuffix);
    void DrawThirdPersonController(ECS::Entity entity);
    void DrawFirstPersonController(ECS::Entity entity);

    // Gameplay components
    void DrawHealthComponent(ECS::Entity entity);
    void DrawRecordRewindComponent(ECS::Entity entity);
    void DrawSceneRewindComponent(ECS::Entity entity);
    void DrawRewindTimeline();
    void DrawRigidbodyComponent(ECS::Entity entity);
    void DrawBoxColliderComponent(ECS::Entity entity);
    void DrawSphereColliderComponent(ECS::Entity entity);
    void DrawCapsuleColliderComponent(ECS::Entity entity);
    void DrawMeshColliderComponent(ECS::Entity entity);
    void DrawCollisionFilteringUI(u32& categoryBits, u32& collisionMask);
    void DrawTriggerZoneComponent(ECS::Entity entity);
    void DrawDamageComponent(ECS::Entity entity);
    void DrawInteractableComponent(ECS::Entity entity);
    void DrawPickupComponent(ECS::Entity entity);
    void DrawInventoryComponent(ECS::Entity entity);
    void DrawTimerComponent(ECS::Entity entity);
    void DrawGameOverComponent(ECS::Entity entity);
    void DrawAudioSourceComponent(ECS::Entity entity);
    void DrawAudioListenerComponent(ECS::Entity entity);
    void DrawReverbZoneComponent(ECS::Entity entity);
    void DrawAmbientSoundLayerComponent(ECS::Entity entity);
    void DrawMusicZoneComponent(ECS::Entity entity);
    void DrawAudioSnapshotTriggerComponent(ECS::Entity entity);
    void DrawAudioOcclusionComponent(ECS::Entity entity);
    void DrawLipSyncComponent(ECS::Entity entity);
    void DrawAudioReactiveComponent(ECS::Entity entity);
    void DrawAudioThresholdTriggerComponent(ECS::Entity entity);
    void DrawRTPCComponent(ECS::Entity entity);
    void DrawBeatClockComponent(ECS::Entity entity);
    void DrawBeatSyncComponent(ECS::Entity entity);
    void DrawMIDIBindingComponent(ECS::Entity entity);
    void DrawAudioFidelityComponent(ECS::Entity entity);
    void DrawConductorComponent(ECS::Entity entity);
    void DrawAudioCollisionComponent(ECS::Entity entity);
    void DrawMaterialInteractionTableComponent(ECS::Entity entity);
    void DrawSidechainComponent(ECS::Entity entity);
    void DrawAudioMixer();
    void DrawAudioMeterStrip();  // Thin VU meter bar above viewport

    // AI components
    void DrawAIControllerComponent(ECS::Entity entity);
    void DrawFollowTargetComponent(ECS::Entity entity);
    void DrawLookAtTargetComponent(ECS::Entity entity);
    void DrawVirtualCameraComponent(ECS::Entity entity);
    void DrawLensComponent(ECS::Entity entity);
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
    void DrawBoneAttachmentComponent(ECS::Entity entity);
    void DrawStreamingVolumeComponent(ECS::Entity entity);
    void DrawStreamingPortalComponent(ECS::Entity entity);

    // Flower components
    void DrawJellyMeshComponent(ECS::Entity entity);
    void DrawTetherComponent(ECS::Entity entity);
    void DrawGrabbableComponent(ECS::Entity entity);
    void DrawFlowerStemComponent(ECS::Entity entity);
    void DrawFlowerParticleConfigComponent(ECS::Entity entity);

    // Scripting
    void DrawScriptComponent(ECS::Entity entity);
    // Attach an AngelScript asset (dropped from the Asset Browser) to an entity. Shared
    // by the Scene view, Hierarchy, and Inspector drop targets. Ignores non-.as paths.
    void AttachScriptFromAsset(ECS::Entity target, const std::string& assetPath);
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
    void DrawCineComponent(ECS::Entity entity);
    void DrawTweenComponent(ECS::Entity entity);
    void DrawDynamicDifficultyComponent(ECS::Entity entity);

    // Joint & Ragdoll components
    void DrawDistanceJointComponent(ECS::Entity entity);
    void DrawHingeJointComponent(ECS::Entity entity);
    void DrawBallSocketJointComponent(ECS::Entity entity);
    void DrawSpringJointComponent(ECS::Entity entity);
    void DrawFixedJointComponent(ECS::Entity entity);
    void DrawSliderJointComponent(ECS::Entity entity);
    void DrawRagdollComponent(ECS::Entity entity);
    void DrawAnimationRecorderComponent(ECS::Entity entity);

    // Puzzle components
    void DrawLockComponent(ECS::Entity entity);
    void DrawPushableComponent(ECS::Entity entity);
    void DrawSwitchComponent(ECS::Entity entity);
    void DrawGoalZoneComponent(ECS::Entity entity);
    void DrawConveyorComponent(ECS::Entity entity);
    void DrawNavmeshVolumeComponent(ECS::Entity entity);
    void DrawTeleporterComponent(ECS::Entity entity);
    void DrawDestructibleComponent(ECS::Entity entity);
    void DrawCurlNoiseFieldComponent(ECS::Entity entity);
    void DrawFractureConfigComponent(ECS::Entity entity);
    void DrawMovingPlatformComponent(ECS::Entity entity);
    void DrawPerFrameColliderComponent(ECS::Entity entity);
    void DrawPolygonCollider2DComponent(ECS::Entity entity);
    void DrawBody2DComponent(ECS::Entity entity);
    void DrawJoint2DComponent(ECS::Entity entity);

    // Networking components
    void DrawNetworkIdentityComponent(ECS::Entity entity);
    void DrawNetworkTransformComponent(ECS::Entity entity);

    // Runtime dialogue overlay (rendered during play mode)
    void UpdateDialogue(f32 deltaTime);
    // Origin and size of the image it is drawn over, stated by the caller.
    // It used to read io.DisplaySize itself, which is the editor WINDOW, so a
    // dialogue box in the docked editor was centred on the editor rather than
    // on the game. See SubtitleSystem::RenderOverlay for the same fix.
    void DrawDialogueOverlay(f32 originX, f32 originY, f32 viewW, f32 viewH);
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

    // Shader Graph editor
    ShaderGraphEditor m_ShaderGraphEditor;
    ShaderGraphData m_ShaderGraphData;
    // Entity whose stored graph is currently loaded in the editor (auto-load
    // guard so we only pull a selection's saved graph once, and never clobber
    // an in-progress graph — see the restore block in the shader graph window).
    ECS::Entity m_ShaderGraphLoadedEntity = ECS::INVALID_ENTITY;

    // Audio Event Graph editor
    AudioEventGraphEditor m_AudioGraphEditor;
    AudioEventGraphData m_AudioGraphData;

    // Particle Graph editor
    ParticleGraphEditor m_ParticleGraphEditor;
    ParticleGraphData m_ParticleGraphData;

    // Scene management
    void SaveScene(const std::string& path);
    void OpenScene(const std::string& path);
    void OpenSceneImmediate(const std::string& path);

    // Project-first workflow helpers
    void EnsureProjectForScene(const std::string& scenePath);
    void OpenProjectFromPath(const std::string& projectPath);
    void AutoDetectProjectForScene(const std::string& scenePath);
    // If scenePath belongs to a project other than the one currently loaded,
    // return that project's .enjinproject path; otherwise "". Used to guard
    // OpenScene from loading a scene under the wrong project root.
    std::string FindMismatchedProjectForScene(const std::string& scenePath);
    void DrawWrongProjectDialog();

    // Watch the open scene file for out-of-band edits (git pull, another tool,
    // a second editor) so we prompt to reload instead of silently overwriting it.
    void RecordOpenSceneDiskTime();
    void CheckExternalSceneChange(f32 deltaTime);
    void DrawExternalSceneChangeDialog();

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

    // Persistent copy of the game camera handed to RenderSystem as the RT camera
    // override (RT dispatch runs before the game view rebuilds its local camera,
    // so the override must outlive the frame).
    Renderer::Camera m_RTGameCamera;

    // Last image view bound as the post-process source. The PP descriptor set is
    // a single plain set (no update-after-bind), so it must only be rewritten when
    // the source actually changes, never restored after recording a draw with it.
    VkImageView m_LastPPSourceView = VK_NULL_HANDLE;

    std::unique_ptr<GUI::ImGuiLayer> m_ImGuiLayer;

    // Default visible panels: core editing panels without debug/dev panels.
    // Console, Profiler, SaveDebug, NetworkPanel etc. are opt-in via View menu.
    EditorPanel m_VisiblePanels = EditorPanel::Hierarchy | EditorPanel::Inspector |
        EditorPanel::Viewport | EditorPanel::AssetBrowser | EditorPanel::GameView;

    // Multi-select state
    std::unordered_set<ECS::Entity> m_SelectedEntities;
    ECS::Entity m_PrimarySelected = ECS::INVALID_ENTITY;
    // When selection changes from outside the Hierarchy (e.g. viewport pick), the
    // Hierarchy scrolls to reveal the selected row on its next draw.
    bool m_HierarchyScrollToSelected = false;
    // Inspector lock: pin the panel to one entity so it stops following selection.
    bool m_InspectorLocked = false;
    ECS::Entity m_InspectorLockedEntity = ECS::INVALID_ENTITY;

    // Inspector rotation edit cache. Euler angles are re-extracted from the
    // quaternion only when the selection or the quaternion changes externally,
    // so dragging past +/-90 degrees stays smooth instead of gimbal-locking
    // (ToEuler is singular at the poles; caching the edited euler avoids the
    // quaternion->euler->quaternion round-trip that jumps there).
    ECS::Entity m_RotEulerEntity = ECS::INVALID_ENTITY;
    Math::Vector3 m_RotEulerCacheDeg = Math::Vector3(0.0f, 0.0f, 0.0f);

    // Hierarchy: clicking an already-multi-selected entity must not collapse
    // the selection on mouse-down (that killed multi-drag) — the collapse is
    // deferred to mouse-release, and skipped entirely if a drag started.
    ECS::Entity m_HierarchyDeferredCollapse = ECS::INVALID_ENTITY;

    // One-shot script-error surfacing per play session (toast + console)
    bool m_ScriptErrorsChecked = false;

    // Marquee (rubber-band) drag state
    bool m_MarqueeDragging = false;
    ImVec2 m_MarqueeStart = {0, 0};
    ImVec2 m_MarqueeEnd = {0, 0};

    EntitySelectedCallback m_OnEntitySelected;

    // Panel state
    bool m_MSAAImGuiUpdatePending = false;  // Deferred ImGui pipeline update after MSAA change
    bool m_HDRImGuiUpdatePending = false;   // Deferred ImGui pipeline + PP hdrOutputMode update after HDR change
    // Load the scene's LUT image, resolved against the project root.
    // ApplyToRuntime cannot do it: the image belongs to PostProcessing and the
    // settings struct only carries the path.
    void ApplySceneLUT(const Renderer::SceneRenderSettings& settings);

    bool m_PendingWireframe = false;       // Deferred wireframe toggle (pipeline recreation unsafe mid-render)
    bool m_PendingQuit = false;            // Deferred quit (Close() unsafe mid-ImGui-render)
    bool m_PrePlayFullscreen = false;      // Window fullscreen state before play mode changed it
    bool m_PrePlayFullscreenSaved = false;  // Whether we captured pre-play fullscreen state
    bool m_ShowDemoWindow = false;
    bool m_ShowStatsOverlay = false;
    bool m_ShowAboutDialog = false;
    bool m_ShowDebugWorkstation = false;  // perf counters + skinning stress tool; opened from View menu
    bool m_ShowGameDebug = false;          // Legacy — kept for backward compat
    bool m_ShowDebugOverlay = false;       // Debug HUD overlay (shown by F1)
    u8 m_DebugOverlayDetail = 0;           // 0=compact, 1=detailed (shown by F2)

    // F1/F2 debug group toggles
    bool m_GameDebugActive = false;        // F1 — Game debug group (Console + overlay)
    bool m_EngineDebugActive = false;      // F2 — Engine debug group (Profiler + Rendering + etc.)

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
    struct ConsoleEntry {
        std::string message;
        LogLevel level = LogLevel::Info;
        LogCategory category = LogCategory::Editor;

        ConsoleEntry() = default;
        ConsoleEntry(const char* msg) : message(msg) {}          // NOLINT — allow implicit from literals
        ConsoleEntry(const std::string& msg) : message(msg) {}   // NOLINT — allow implicit from string pushes
        ConsoleEntry(std::string&& msg) : message(std::move(msg)) {}
        ConsoleEntry(const std::string& msg, LogLevel lvl, LogCategory cat)
            : message(msg), level(lvl), category(cat) {}
    };
    // MAIN THREAD ONLY. Read across a dozen ImGui calls while the panel draws,
    // so nothing may push into it from elsewhere.
    std::vector<ConsoleEntry> m_ConsoleLog;

    // Cross-thread inbox. EditorLogCallback is installed as the GLOBAL logger
    // callback and Logger::Log fires it from whatever thread called it -- the
    // build thread, the MCP socket thread, the dev web server. Those used to
    // push_back straight into m_ConsoleLog, so exporting a game with the Console
    // panel open could reallocate the vector under the panel's own read of it.
    // They land here instead and the main thread drains them once a frame.
    std::vector<ConsoleEntry> m_PendingConsoleEntries;
    std::mutex m_PendingConsoleMutex;
    // Set once at Initialize. Comparing against it is how PushConsoleMessage
    // tells an editor-thread call from a background one without every one of the
    // ~440 main-thread call sites paying for a lock.
    std::thread::id m_MainThreadId;

    // Move anything the background threads logged into m_ConsoleLog. Main thread.
    void DrainPendingConsoleEntries();
    static constexpr usize MAX_CONSOLE_LINES = 1000;

    // Console filter state
    bool m_ConsoleShowInfo = true;
    bool m_ConsoleShowWarn = true;
    bool m_ConsoleShowError = true;
    int  m_ConsoleFeedTab = 0;  // 0=All, 1=Editor, 2=Runtime

    // Console multi-selection (Shift+Click to toggle, Ctrl+C to copy selected)
    std::unordered_set<int> m_ConsoleSelectedIndices;

    // --- Auditioning a sound while EDITING -----------------------------------
    // The editor had no audio device at all outside play mode. PlayMode owns an
    // AudioEngine but only Initialize()s it in Play() and Shutdown()s it in
    // Stop(), so the Audio Source inspector's Play button could not have made a
    // sound even in principle -- it set a bool, and the only reader of that bool
    // runs in play mode. Worse, in play mode setting it true makes the source
    // look already-playing and SKIPS the play-on-awake start, so the button was
    // not merely inert, it was harmful.
    //
    // This is a second, editor-owned engine, so auditioning a clip never touches
    // the game's mixer, listener or buses. It starts on first use and not before:
    // opening a device costs a real audio thread, and most editor sessions never
    // press play on a sound.
    Audio::AudioEngine m_AuditionAudio;
    bool m_AuditionInitialized = false;
    Audio::SoundHandle m_AuditionSound = 0;
    Audio::AudioClipHandle m_AuditionClip = 0;
    std::string m_AuditionPath;

    // Play `path` (project-relative or absolute) through the editor's own
    // device, stopping whatever it was playing. Returns false and toasts when
    // the clip will not load.
    bool AuditionSound(const std::string& path);
    void AuditionStop();
    bool AuditionIsPlaying() const;
    // Seconds, or -1 when nothing is auditioning / the backend cannot answer.
    f32 AuditionTime() const;
    f32 AuditionLength() const;
    void AuditionSeek(f32 seconds);
    // One row: transport for whichever engine owns this clip right now -- the
    // game's during play, the editor's otherwise. Draws nothing when neither
    // has it. Returns true if it drew.
    bool DrawAudioTransport(const std::string& clipPath, ECS::Entity entity);

    // --- Script error peek ---------------------------------------------------
    // Double-clicking a console line that names a script file and a line opens
    // the file at that line, here in the editor. A script error already carries
    // its own location -- "scripts/Player.as (31, 9): Expected ';'" from the
    // compiler, "... at scripts/Player.as:31:9" from a thrown exception -- and
    // until now reading that meant finding the file yourself and counting to the
    // line. Nothing else in the editor can show a line of script.
    // Reads the file from disk on open; it is a viewer, not an editor.
    bool ParseScriptLocation(const std::string& message, std::string& outPath, int& outLine) const;
    bool PeekScriptAtLine(const std::string& path, int line);
    void DrawScriptPeekWindow();

    bool m_ScriptPeekOpen = false;
    std::string m_ScriptPeekPath;         // resolved absolute path on disk
    std::string m_ScriptPeekLabel;        // what the message called it
    std::vector<std::string> m_ScriptPeekLines;
    int  m_ScriptPeekLine = 0;            // 1-based, 0 = no line highlighted
    bool m_ScriptPeekScrollPending = false;
    // Opening has to raise the window, not just set the flag: a window that is
    // already open sits wherever it was in the z-order, and a second
    // double-click would look like nothing happened.
    bool m_ScriptPeekFocusPending = false;

    // Helper methods
    void ImportModel(const std::string& path);
    void OnFileDrop(int count, const char** paths);
    void HandleViewportPicking();
    bool SceneHasMouseLookController() const;
    void DrawGizmos();
    void DrawGrid();
    void FocusOnEntity(ECS::Entity entity);  // Center camera on entity
    void DrawMarqueeRect();                   // Draw rubber-band selection rectangle
    void DrawSelectionHighlight();            // Projected bounding boxes for selection + descendants
    void DrawCameraGizmos();                  // Always-visible, clickable virtual-camera icons + frustums
    // Screen positions of camera gizmo icons this frame, for click-selection.
    std::vector<std::pair<ECS::Entity, ImVec2>> m_CameraGizmoScreenPos;
    ImDrawList* GetViewportOverlayDrawList(); // Scene window draw list so overlays layer under dialogs
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
    f32 m_FrameTimeP50 = 0.0f;
    f32 m_FrameTimeP95 = 0.0f;
    f32 m_FrameTimeP99 = 0.0f;
    f32 m_LastDeltaTime = 0.0f;
    // Game-view sims run in UpdateGameViewSims (update path, once per editor
    // frame with time-scaled dt) - NOT in RenderOffscreen. This retired the
    // m_GameViewSimAccum workaround for the 30fps-slow-rain bug class: the
    // Game View FPS throttle now only affects rendering, so a sim can no
    // longer couple to render cadence. Weather flags computed by the sim
    // pass and consumed by the render pass live here as members.
    void UpdateGameViewSims(f32 simDt);
    bool m_GameViewWeatherParticles = false;
    bool m_GameViewIsRain = false;

    // Accumulated effects time (drives fire-light flicker) and the per-frame fire
    // light buffer reused to keep the injection allocation-free.
    f32 m_EffectsTime = 0.0f;
    std::vector<Effects::FireLight> m_FireLights;

    // Frame time histogram (8 buckets: <1ms, 1-2, 2-4, 4-8, 8-16, 16-33, 33-66, >66ms)
    u32 m_FrameHistogram[8] = {};
    u32 m_FrameHistogramTotal = 0;

    // Spike log — ring buffer of last 10 frames that exceeded 8ms
    struct SpikeEntry {
        f32 frameTimeMs;
        f32 renderMs;
        f32 fenceMs;
        u32 frameNumber;
    };
    SpikeEntry m_SpikeLog[10] = {};
    u32 m_SpikeLogIndex = 0;
    u32 m_SpikeLogCount = 0;
    u32 m_FrameNumber = 0;

    // Physics debug visualization
    bool m_ShowColliderWireframes = false;

    // SH Light Probe visualization
    bool m_ShowSHProbes = false;
    bool m_ShowSHGridBounds = false;

    // --- Gamepad Editor Navigation ---
    struct RadialMenuItem {
        const char* label;
        const char* icon; // Short bracket-tag icon
        i32 actionId;     // Mapped to an action enum
    };
    enum class RadialMenuType : u8 { None = 0, Tools, File, Play, Create };
    enum class GamepadAction : u8 {
        None = 0,
        // Tools radial (RB)
        Translate, Rotate, Scale, ToggleSpace, FocusSelection, ToggleGrid,
        // File radial (LB)
        Save, Undo, Redo, Duplicate, Delete, CommandPalette,
        // Play radial (Start)
        PlayToggle, Pause, Stop,
        // Create radial (Y)
        CreateEmpty, CreateCube, CreateLight, CreateCamera, CreateSprite
    };
    bool m_GamepadEditorEnabled = true;
    RadialMenuType m_RadialMenuActive = RadialMenuType::None;
    f32 m_RadialMenuAngle = 0.0f;     // Current stick angle
    i32 m_RadialMenuHovered = -1;     // Hovered sector index
    f32 m_RadialMenuOpenTime = 0.0f;  // For open animation
    Math::Vector2 m_RadialMenuCenter; // Screen position

    void UpdateGamepadEditor(f32 deltaTime);
    // One entry per wedge. Drawing and release-to-select both read this table;
    // they used to disagree, which is why the radial menus could be opened and
    // never chosen from.
    struct RadialItem {
        const char* label;
        const char* icon;
        GamepadAction action;
    };
    std::vector<RadialItem> GetRadialItems(RadialMenuType type) const;
    void DrawRadialMenu(RadialMenuType type);
    void ExecuteGamepadAction(GamepadAction action);
    void HandleGamepadViewportNavigation(f32 deltaTime);
    void UpdateGamepadInspector(f32 deltaTime);
    void DrawGamepadInspectorOverlay();

    // Gamepad inspector mode — edit properties with controller
    bool m_GamepadInspectorMode = false;
    i32 m_GamepadInspectorIndex = 0;       // Currently focused property
    f32 m_GamepadInspectorRepeat = 0.0f;   // DPad repeat timer

    // Editable property descriptor for gamepad navigation
    struct GamepadProperty {
        std::string label;
        f32* valuePtr = nullptr;           // Direct pointer to the float value
        f32 step = 0.1f;                   // How much left stick adjusts per second
        f32 minVal = -FLT_MAX;
        f32 maxVal = FLT_MAX;
    };
    std::vector<GamepadProperty> m_GamepadProperties;
    void RebuildGamepadPropertyList();

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
    // Debug-recording timeline: how far back from "now" the paused scrubber sits
    // (seconds; 0 = live edge). Reset on play start and on resume.
    f32 m_DebugScrubOffset = 0.0f;
    // MCP server (Settings > System > MCP Server): AI-assistant control surface.
    McpServer m_McpServer;
    bool m_McpDesiredRunning = false;   // reconcile setting <-> server each frame
    bool m_PendingPlayRestart = false;  // Set alongside m_PendingPlayStop to re-enter play mode
    bool m_PendingPlayStart = false;    // Deferred play mode start (for restart after stop)
    bool m_SkipNextRender = false;  // Skip one frame after Stop to let render caches refresh

    // Camera zone override (driven by CameraTriggerComponent)
    ECS::Entity m_CameraZoneOverride = ECS::INVALID_ENTITY;

    // Cached player entity for per-frame zone detection (avoid GetAllEntities scan)
    ECS::Entity m_CachedPlayerEntity = ECS::INVALID_ENTITY;

    // The render pass the offscreen/effect pipelines were last built against.
    // When an RT resize replaces the pass, pipelines referencing the old pass
    // render undefined (black geometry) — compare each frame and rebuild on change.
    VkRenderPass m_LastEffectRenderPass = VK_NULL_HANDLE;

    // Splash screen
    bool m_ShowSplash = true;
    f32 m_SplashTimer = 0.0f;
    f32 m_SplashDuration = 4.0f;   // Show for 4 seconds
    f32 m_SplashFadeStart = 3.0f;  // Start fading at 3 seconds
    f32 m_EditorFadeIn = 0.0f;     // Editor fade-in progress (0 to 1)

    // Project Hub (shown after splash)
    enum class HubPage : u8 { Landing = 0, WizardSetup, WizardTemplate };
    bool m_ShowProjectHub = true;
    HubPage m_HubPage = HubPage::Landing;

    // New Project wizard state
    char m_NewProjectName[128] = "MyGame";
    char m_NewProjectPath[512] = "";       // Filled with default (Documents/EnjinProjects)
    char m_NewSceneName[128] = "Main";
    i32 m_SelectedTemplate = -1;
    u32 m_TemplateFilter = 0;             // 0 = All, bitmask for category filtering
    i32 m_TemplateStatusFilter = -1;      // -1 = All, 0=Stable, 1=Beta, 2=Preview, 3=Experimental
    i32 m_HubProjectFilter = 0;            // 0=All, 1=Ready, 2=Missing

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


    // Custom templates
    std::vector<std::string> m_CustomTemplateNames;
    std::vector<std::string> m_CustomTemplatePaths;

    // Project context menu / delete confirmation state
    std::string m_HubContextProjectPath;       // Full path of project being acted on
    bool m_HubOpenContextMenu = false;         // Whether to open context menu this frame
    bool m_HubShowDeleteConfirm = false;       // Whether to show delete confirmation popup
    std::string m_HubDeleteProjectPath;        // Path of project pending deletion
    std::string m_HubDeleteProjectName;        // Display name for confirmation dialog
    std::string m_HubPendingDeletePath;        // Deferred: delete this path at start of next frame
    std::string m_HubPendingRemovePath;       // Deferred: remove from recent list at start of next frame
    std::string m_HubPendingOpenPath;          // Deferred: open this project at start of next frame

    // Project Hub methods
    void DrawProjectHub();
    void DrawProjectHubInner();
    void DrawHubRecentSidebar(ImDrawList* dl, const ImVec2& area, f32 contentY, f32 sidebarW);
    void DrawHubLandingPage(ImDrawList* dl, const ImVec2& area, f32 contentY, f32 sidebarW);
    void DrawHubWizardSetup(ImDrawList* dl, const ImVec2& area, f32 contentY, f32 sidebarW);
    void DrawHubWizardTemplate(ImDrawList* dl, const ImVec2& area, f32 contentY, f32 sidebarW);
    void DrawTemplateHoverPreview(ImDrawList* dl, i32 templateIdx, const ImVec2& cardPos, const ImVec2& cardEnd);
    // Panel widths / game-view size / stats overlay authored per template.
    // Called by ApplyTemplate AND by the shipped-template-folder path, which
    // does not go through ApplyTemplate at all.
    void ApplyTemplateLayout(const std::string& templateId);
    // Live accessibility settings menu (UICanvas + controller entity +
    // scripts/AccessibilityDemo.as) — shared by the Accessibility Demo and
    // Web Demo templates
    void CreateAccessibilityMenu();
    void SaveCustomTemplate(const std::string& name);
    void LoadCustomTemplates();
    bool CreateProjectOnDisk(const std::string& projectDir, const std::string& projectName,
                             const std::string& sceneName, const std::string& templateId);

    // Docking layout
    bool m_DockingInitialized = false;

    // Per-template layout configuration
    struct LayoutConfig {
        f32 leftWidth   = 0.18f;   // Hierarchy panel width ratio
        f32 rightWidth  = 0.25f;   // Inspector panel width ratio
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

    // Hidden-viewport skip: the editor renders the scene into BOTH the Scene and Game
    // view targets every frame, which doubles the (draw-call-bound) GPU cost with many
    // meshes. We skip rendering whichever view isn't actually visible. "*ThisFrame" is set
    // by the panel's ImGui::Begin return (true only when the dock tab is showing);
    // RenderOffscreen reads the previous frame's value (it runs before the panels), so a
    // freshly-revealed tab is one frame stale — imperceptible. Default true so the first
    // frame renders both.
    bool m_SceneViewVisibleThisFrame = true;
    bool m_SceneViewVisiblePrev = true;
    bool m_GameViewVisibleThisFrame = true;
    bool m_GameViewVisiblePrev = true;

    // Render profiling (logs avg render time every 120 frames during play)
    f32 m_RenderProfileAccum = 0.0f;
    u32 m_RenderProfileFrames = 0;

    // CPU frame profiler (editor mode) — exponential moving averages
    f32 m_CPUUpdateMs = 0.0f;
    f32 m_CPUShadowMs = 0.0f;
    f32 m_CPURenderMs = 0.0f;
    f32 m_CPUImGuiMs = 0.0f;
    f32 m_CPUPresentMs = 0.0f;

    // Editor viewport render target (offscreen rendering for scene editing camera)
    std::unique_ptr<Renderer::RenderTarget> m_EditorViewportRT;
    u32 m_EditorViewportWidth = 800;
    u32 m_EditorViewportHeight = 600;
    f32 m_EditorViewportImageMinX = 0.0f, m_EditorViewportImageMinY = 0.0f;
    f32 m_EditorViewportImageMaxX = 0.0f, m_EditorViewportImageMaxY = 0.0f;
    bool m_EditorViewportHovered = false;
    bool m_EditorViewportFocused = false;

    // Viewport aspect ratio constraint
    enum class AspectRatio : u8 {
        Free = 0,   // Fill panel
        R16_9,      // 16:9  (1.778)
        R16_10,     // 16:10 (1.600)
        R21_9,      // 21:9  (2.333)
        R4_3,       // 4:3   (1.333)
        R3_2,       // 3:2   (1.500)
        R9_16,      // 9:16  (0.5625) — mobile portrait
        R9_20,      // 9:20  (0.450)  — modern phone portrait
        Count
    };
    static constexpr const char* AspectRatioLabels[] = {
        "Free", "16:9", "16:10", "21:9", "4:3", "3:2", "9:16", "9:20"
    };
    static constexpr f32 AspectRatioValues[] = {
        0.0f, 16.0f/9.0f, 16.0f/10.0f, 21.0f/9.0f, 4.0f/3.0f, 3.0f/2.0f, 9.0f/16.0f, 9.0f/20.0f
    };
    AspectRatio m_SceneViewAspect = AspectRatio::R16_9;
    AspectRatio m_GameViewAspect = AspectRatio::R16_9;

    // Scene view display mode (like Blender's viewport shading)
    enum class SceneViewMode : u8 {
        Wireframe = 0,  // Wireframe only
        Solid,          // Flat shading, no lighting (default)
        Lit,            // Lighting, no shadows
        LitShadows,     // Lighting + shadows
        Full            // Everything: shadows + post-processing
    };
    // Solid by default, the same choice Blender makes and for the same reason:
    // the scene view always shows the geometry at full brightness, so a scene
    // that is simply dark, or a light that is not reaching a surface, cannot be
    // mistaken for the editor failing to render. Switch to Lit in the viewport
    // toolbar to judge the actual lighting.
    SceneViewMode m_SceneViewMode = SceneViewMode::Solid;

    // Compute letterboxed image size from available space and aspect ratio
    static ImVec2 ComputeAspectConstrainedSize(f32 availW, f32 availH, f32 aspect);

    // Post-processing (owned by editor, applied to Game View)
    std::unique_ptr<Renderer::PostProcessing> m_PostProcessing;

    // Selected game camera entity (user can pick which camera to use)
    ECS::Entity m_SelectedGameCamera = ECS::INVALID_ENTITY;

    // Scene management
    Scene::SceneManager m_SceneManager;

    // Asset libraries (font + 2D/3D catalogs)
    Assets::FontLibrary m_FontLibrary;
    Assets::AssetLibrary m_AssetLibrary;

    // Effects systems (global, rendered in game view)
    Effects::WindSystem m_WindSystem;
    Effects::WeatherSystem m_WeatherSystem;
    Effects::Water3D m_Water3D;
    Effects::RetroEffects m_RetroEffects;

    // Was retro enabled last frame? The clear-down is edge-triggered off this.
    //
    // Without it the disabled branch ran every frame and zeroed fourteen
    // post-process and global render fields whoever had set them, so any other
    // system's CRT / VHS / dither / colour-quant / downscale setting was wiped on
    // the next frame by a panel that was not even open.
    bool m_RetroWasEnabled = false;

    // Particle system (CPU simulation for ParticleEmitterComponent)
    Effects::ParticleSystem m_ParticleSystem;

    // Parallax scrolling background system (2D scenes)
    ECS::ParallaxSystem m_ParallaxSystem;

    // Elemental system (unified fire/water/earth/air particle simulation)
    Effects::ElementalSystem m_ElementalSystem;

    // VWS override-layer capture (DAW-for-games). Edits routed through it are
    // folded into the active layer as sparse deltas over a pristine base scene.
    Scene::LayerSystem m_LayerSystem;
    bool m_ShowLayersPanel = false;

    // Drop the layer session (base + stack). Must be called by EVERY path that
    // changes the scene context without going through OpenSceneImmediate (New
    // Scene, Apply Template, New Project) — a stale base would let "Rebuild
    // From Layers" resurrect the previous scene into the new one.
    void ResetLayerSession() {
        m_LayerSystem.SetBaseScene(std::string{});
        m_LayerSystem.Stack().layers.clear();
        m_LayerSystem.SetActiveLayer(-1);
    }

    // Capture one component of every selected entity into the active layer.
    // No-op without an unlocked active layer. For the batch-apply paths (gizmo
    // drag end, multi-select inspector) where one action edits many entities.
    void RecordLayerEditForSelection(const std::string& key) {
        Scene::Layer* active = m_LayerSystem.ActiveLayer();
        if (!active || active->locked) return;
        for (ECS::Entity e : m_SelectedEntities) {
            m_LayerSystem.RecordEdit(e, key);
        }
    }

    // Fold a newly created entity into the active layer as a "created" delta so
    // toggling the layer adds/removes it. No-op without an unlocked active layer,
    // so entities made with no layer active just belong to the base scene.
    void RecordLayerCreate(ECS::Entity e) {
        Scene::Layer* active = m_LayerSystem.ActiveLayer();
        if (!active || active->locked) return;
        m_LayerSystem.RecordCreate(e);
    }

    // Fluid simulation (Stable Fluids solver for FluidVolumeComponent)
    Effects::FluidSimulation m_FluidSimulation;

    // Fluid-terrain coupling (erosion/deposition from fluid to terrain heightmap)
    Effects::FluidTerrainCoupling m_FluidTerrainCoupling;

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

    // Unified settings window state
    int m_SettingsActiveTab = 0;  // 0=System, 1=Project, 2=Scene

    // One-time migration of deprecated EditorSettings fields to .enjinproject
    void MigrateEditorSettingsToProject();

    // Per-scene content warning flags — authored in Settings > Scene, saved
    // with the scene, shown by the player before gameplay starts
    Accessibility::SceneContentFlags m_SceneContentFlags;

    // One-button local preview of web exports ("Run in Browser") — browsers
    // refuse wasm over file://, so the editor serves the export itself
    Networking::DevWebServer m_DevWebServer;

    // True while a game script explicitly released the cursor during play
    // (Web Demo menu mode) — suppresses the editor's click-to-recapture so
    // the game's own toggle stays in charge
    bool m_GameScriptReleasedCursor = false;

    // Per-scene render settings
    bool m_CurrentSceneUsesProjectDefaults = true;
    // Drawn at the top of every per-scene render settings section.
    void DrawProjectDefaultsBanner();
    u32 m_ArtStylePreset = 0;  // Tracks the Art Style Preset dropdown index
    Renderer::SceneRenderSettings m_PrePlayRenderSettings;

    // Terrain editing
    TerrainBrush m_TerrainBrush;
    bool m_TerrainEditMode = false;
    ECS::Entity m_TerrainEditTarget = 0;
    bool m_BrushActive = false;
    Math::Vector3 m_BrushHitPoint;
    bool m_BrushHitValid = false;
    i32 m_Dragging2DPoint = -1;  // Index of control point being dragged, -1 = none

    // Creative mode: a SimCity/MS-Paint-style build palette. Pick a tool, then
    // click-drag on the ground to place that object sized to the drag footprint.
    // Area tools drag out a footprint; point tools click-place one object.
    enum class CreativeTool { None,
        Lake, TreeGrove, GrassPatch, ShrubPatch,        // area (drag to size)
        Block, Ball, PointLight, PhysicsBox, Barrel, SpawnPoint };  // point (click to place)
    bool m_ShowCreativePalette = false;
    CreativeTool m_CreativeTool = CreativeTool::None;
    bool m_CreativePlacing = false;         // mid drag-out
    ECS::Entity m_CreativePlaceEntity = 0;  // the object being dragged out (its own live preview)
    i32 m_BoundaryDragPoint = -1;           // index of the boundary-polygon handle being dragged, -1 = none
    Math::Vector3 m_CreativeDragStart;      // world point where the drag began (ground)
    Math::Vector3 m_CreativeDragEnd;        // current world point under the cursor
    void DrawCreativePalette();
    void HandleCreativePlacement(f32 deltaTime);

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

    // Post-processing enable prompt: PP is opt-in per camera (default off). When the
    // user edits a PP effect while the active camera has PP off, offer to turn it on.
    std::vector<unsigned char> m_PrevPPBytes;   // last frame's PostProcessSettings bytes
    bool m_PPPromptSuppressed = false;          // latched by "Not Now" until PP is on again
    bool m_AskEnablePPPopup = false;            // request to open the modal this frame
    ECS::Entity m_AskEnablePPCamera = ECS::INVALID_ENTITY;

    // Undo/Redo manager
    UndoRedoManager m_UndoRedo;
    // Mesh > Reduce: fraction of triangles to keep. A transient tool setting,
    // deliberately not per-entity -- it is the dial you last used, not scene data.
    f32 m_MeshSimplifyRatio = 0.5f;
    Math::Matrix4 m_GizmoStartTransform;
    bool m_GizmoDragging = false;

    // Generic inspector-edit undo capture: baseline entity JSON from the last
    // quiet frame; while an ImGui item is active (drag/typing) the baseline is
    // frozen, and when activity ends the before/after diff becomes ONE
    // EntityEditCommand. External mutations on quiet frames (animators,
    // scripts) are adopted silently — only widget interaction creates history.
    bool m_PropUndoEditing = false;
    ECS::Entity m_PropUndoBaselineEntity = ECS::INVALID_ENTITY;
    i32 m_PropUndoRefreshTick = 0;   // frames since last baseline refresh (heavy-entity throttle)
    std::string m_PropUndoBaseline;
    u32 m_PropUndoStackAtSessionStart = 0;

    // History panel: lists the undo/redo stacks, click any entry to jump
    bool m_ShowHistoryPanel = false;
    void DrawHistoryPanel();

    // Golden-image capture (--golden): frame countdown + writer
    i32 m_GoldenFrameCounter = 0;
    void WriteGoldenCapture();
    // Replay files: export the last session to <project>/replays/, replay the newest.
    // Playback swaps the live scene for the replay's snapshot; the working scene
    // is preserved here and restored when the replay session stops.
    void ExportReplayToProject();
    void PlayLatestReplay();
    std::string m_PreReplaySceneJson;
    bool m_WasReplaying = false;
    // Free camera during replay playback: the fly cam takes the game view
    // while the replay drives the world (trailer / spectator angle).
    bool m_ReplayFreeCam = false;
    // Readback + write the game view as <basePath>.png/.ppm (no exit).
    bool CaptureGameViewToFile(const std::string& basePath);

    // Floating viewport toolbar screen rect — picking/marquee must not fire
    // through its buttons (set each frame the toolbar draws)
    f32 m_ViewportToolbarMinX = 0.0f, m_ViewportToolbarMinY = 0.0f;
    f32 m_ViewportToolbarMaxX = 0.0f, m_ViewportToolbarMaxY = 0.0f;

    // Game View mouse interaction during play mode
    bool m_GameViewMouseCaptured = false;
    // Game view click-to-capture wants a DOUBLE click, so the time of the last
    // one has to be remembered. Starts far in the past so the first click of a
    // session is never treated as the second half of a pair.
    f32 m_GameViewLastClickTime = -1000.0f;
    static constexpr f32 kGameViewDoubleClickSeconds = 0.4f;
    // True only on frames where the Game View panel actually submitted its
    // image (visible + front tab). The play-mode game-UI overlay draws on the
    // FOREGROUND list at the cached image rect - without this gate it painted
    // the game's HUD over whatever panel was docked in that spot (Marty
    // 2026-08-30: game text all over the Visual Script editor).
    bool m_GameViewImageDrawnThisFrame = false;
    f32 m_GameViewImageMinX = 0.0f, m_GameViewImageMinY = 0.0f;
    f32 m_GameViewImageMaxX = 0.0f, m_GameViewImageMaxY = 0.0f;
    bool m_GameViewHovered = false;

    // Accessibility settings (persistent)
    EditorSettings m_EditorSettings;

    // Runtime accessibility settings (synced from EditorSettings, passed to PlayMode)
    Accessibility::RuntimeAccessibilitySettings m_RuntimeAccessibility;

    // Input action map for remappable input
    InputSystem::InputActionMap m_InputMap;

    // In-game pause/system menu. Owns Main Menu / Options / How to Play only —
    // the pause ROOT is the shipped UICanvas template (m_PauseMenuEntity), the
    // same one both players spawn, so the editor previews the real menu.
    GUI::GameMenuSystem m_GameMenu;

    // Pause root: an entity carrying UITemplates::CreatePauseMenu(), spawned
    // into the play world on pause and destroyed on resume/stop. Non-zero is
    // the editor's "pause menu is up" state (the toolbar pause button pauses
    // WITHOUT it, which is why this is not just IsPaused()).
    ECS::Entity m_PauseMenuEntity = 0;

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
    // The Game View window's draw list, captured while that window is being
    // built. The game UI is appended to it after the panel closes, so it stacks
    // with the editor's windows instead of painting over all of them.
    // Null until the Game View has drawn its image this frame.
    ImDrawList* m_GameViewDrawList = nullptr;

    bool m_ShowImportDialog = false;
    std::string m_ImportDialogPath;
    Assets::ImportOptions m_ImportDialogOptions;
    std::string m_LastImportedModelPath;

    // Group (multi-drop) import: one dialog for a batch of dropped models, with an
    // "apply to all" choice. m_ImportBatchQueue holds the files still to import.
    std::vector<std::string> m_ImportBatchQueue;
    bool m_ImportBatchApplyToAll = true;
    int  m_ImportBatchTotal = 0;      // count when the batch started (for "X of Y" + spread)
    bool m_ImportBatchActive = false; // apply-to-all processor runs across frames in Update
    Assets::ImportOptions m_ImportBatchOptions;
    std::vector<ECS::Entity> m_ImportBatchRoots;  // roots imported so far (to frame at the end)

    // Import result dialog + undo support
    bool m_ShowImportResultDialog = false;
    Assets::ImportResult m_LastImportResult;
    std::vector<ECS::Entity> m_LastImportEntities;  // For undo-import
    void DrawImportResultDialog();
    void UndoLastImport();

    // Import preview — scene hierarchy from the file
    struct ImportPreviewNode {
        std::string name;
        i32 meshIndex = -1;       // -1 = no mesh
        i32 parentIndex = -1;
        bool selected = true;     // User can deselect to skip import
        bool hasMesh = false;
        bool hasSkin = false;
        u32 vertexCount = 0;
        u32 morphTargetCount = 0;
    };
    std::vector<ImportPreviewNode> m_ImportPreviewNodes;
    bool m_ImportPreviewScanned = false;
    void ScanImportPreview(const std::string& filepath);

    // --- Live import preview -------------------------------------------------
    // A decimated point sample of the model in its FILE space, plus its bounds,
    // collected during ScanImportPreview (which already loads the whole file, so
    // this costs nothing extra). The dialog projects these every frame under the
    // current scale / axis conversion / flips / rotation, so the settings can be
    // judged by eye instead of imported, inspected, undone and imported again.
    //
    // Points are in file space with node transforms baked in. The import
    // settings are a rigid transform on top, which is exactly what the preview
    // applies -- so what is drawn is what lands in the scene.
    std::vector<Math::Vector3> m_ImportPreviewPoints;
    Math::Vector3 m_ImportPreviewMin = Math::Vector3(0.0f);
    Math::Vector3 m_ImportPreviewMax = Math::Vector3(0.0f);
    bool m_ImportPreviewHasGeometry = false;
    static constexpr usize kImportPreviewMaxPoints = 4000;

    // Orbit state for the preview viewport (drag to turn, wheel to zoom).
    f32 m_ImportPreviewYaw = 0.7f;
    f32 m_ImportPreviewPitch = 0.25f;
    f32 m_ImportPreviewZoom = 1.0f;
    bool m_ImportPreviewShowRef = true;

    void DrawImportPreviewViewport();
    // The rigid transform the current options describe, so the preview and the
    // importer cannot disagree about what a setting means.
    Math::Quaternion ImportPreviewRotation() const;

    std::string m_ImportDialogFilename;   // Cached on dialog open
    std::string m_ImportDialogExtension;  // Cached on dialog open
    u64 m_ImportDialogFileSize = 0;       // Cached on dialog open
    bool m_ImportDialogIsReimport = false; // Cached on dialog open
    Assets::SourceApp m_ImportDialogDetectedApp = Assets::SourceApp::Auto;  // Auto-detected source app
    bool m_ImportDialogScaleFromPreset = false;  // True if scale was auto-filled from preset

    // Deferred scene open (prevents World::Clear during Render-phase ImGui callbacks)
    std::string m_PendingSceneLoadPath;

    // Deferred "new scene". The menu bar, the unsaved-changes dialog, the
    // project-creation dialog and the command palette all used to call
    // World::Clear() straight from their ImGui callbacks -- i.e. from the
    // Render phase, while the frame's command buffer was mid-recording and
    // every panel drawn after them still walked the world. The hub path for
    // the same action had always deferred; these had not, so one user action
    // had a safe and an unsafe implementation depending on which widget
    // invoked it. All four now queue this and Update() performs the clear.
    enum class NewSceneMode : u8 {
        None,
        ToHub,      // File > New Scene: clear, forget the path, reopen the hub
        SaveAsNew,  // project creation: clear, then save an empty scene to the path below
        ClearOnly,  // command palette: clear and forget the path, leave the UI where it is
    };
    NewSceneMode m_PendingNewScene = NewSceneMode::None;
    std::string  m_PendingNewScenePath;   // SaveAsNew only
    void ApplyPendingNewScene();          // runs from Update(), never from Render

    // Script-requested play restart / scene switch (Scene_Restart /
    // Scene_LoadScene during editor play): stop -> (open scene) -> re-play,
    // via the same deferred plumbing the toolbar and --play-cycle use.
    bool m_RestartPlayPending = false;
    std::string m_RestartPlayScene;
    int m_RestartPlayDelay = 0;

    // Wrong-project guard: set when OpenScene is asked to load a scene that
    // belongs to a different project than the one currently open.
    bool m_ShowWrongProjectDialog = false;
    std::string m_WrongProjectScenePath;
    std::string m_WrongProjectManifest;

    // One throttled disk watch for the whole editor. A panel registers a path and
    // reads back a version; nothing else in the editor owns a stat timer, and
    // nothing asks the user to press Refresh because the disk moved.
    EditorWatch m_Watch;
    EditorWatch::Handle m_DataAssetWatch = 0;
    u64 m_DataAssetWatchSeen = 0;
    std::string m_DataAssetWatchRoot;   // the root m_DataAssetWatch was opened on
    EditorWatch::Handle m_AssetBrowserWatch = 0;
    u64 m_AssetBrowserWatchSeen = 0;
    std::string m_AssetBrowserWatchPath;
    EditorWatch::Handle m_ScriptPeekWatch = 0;
    u64 m_ScriptPeekWatchSeen = 0;
    std::string m_ScriptPeekWatchPath;
    u64 m_PluginSourcesSeen = 0;
    bool m_PluginSourcesWatched = false;
    u64 m_TmplWatchSeen = 0;

    // External scene-change watch: baseline mtime of the open scene file,
    // recorded on load/save; a mismatch means someone edited it out-of-band.
    std::filesystem::file_time_type m_OpenSceneDiskTime{};
    bool m_HasOpenSceneDiskTime = false;
    f32  m_SceneWatchTimer = 0.0f;
    bool m_ShowExternalSceneChangeDialog = false;

    // Deferred auto-save recovery load (same reason — the recovery dialog is a
    // Render-phase ImGui modal; loading the world from inside it crashed)
    std::string m_PendingRecoveryLoadPath;

    // Deferred template application (same reason — must not Clear during Render)
    std::string m_PendingTemplateId;

    // Deferred import (renders one "Loading..." frame before the blocking import)
    bool m_ImportPending = false;
    std::string m_ImportPendingPath;
    Assets::ImportOptions m_ImportPendingOptions;

    void DrawImportDialog();
    void DrawImportLoadingOverlay();
    void ExecuteImport(const std::string& path, const Assets::ImportOptions& options,
                       const Math::Vector3& placementOffset = Math::Vector3(0.0f),
                       bool showResultDialog = true);
    // Drag-and-drop fast path: import a model immediately with auto-detected options
    // (no modal dialog), placed at placementOffset. Used for dropping one or many
    // models at once so they "just work" without clicking through a dialog per file.
    void ImportModelImmediate(const std::string& path, const Math::Vector3& placementOffset);
    // Open the group-import dialog for a batch of dropped models (>1). The dialog lets
    // you set shared options and choose whether to apply them to all or step through.
    void BeginGroupImport(const std::vector<std::string>& paths);
    // Frame + select every model imported in a group batch, then clear batch state.
    void FinishGroupImport();

    // Build dialog state. Builds run on a worker thread (BuildPipeline is pure
    // file I/O + process spawns, no live editor state): the worker writes
    // progress under m_BuildMutex and flips m_BuildThreadDone; PollBuildThread
    // (called every frame from Update) joins and publishes the result on the
    // main thread, where notifications / run-after-build / the dev web server
    // belong. Editor shutdown joins a still-running build.
    bool m_ShowBuildDialog = false;
    Build::BuildConfig m_BuildConfig;
    Networking::NetworkConfig m_NetworkConfig;
    Build::BuildResult m_BuildResult;
    bool m_BuildInProgress = false;
    bool m_BuildFinished = false;
    float m_BuildProgress = 0.0f;
    std::string m_BuildProgressPhase;
    std::thread m_BuildThread;
    std::atomic<bool> m_BuildThreadDone{false};
    std::mutex m_BuildMutex;                  // guards the two worker-written fields below
    float m_BuildWorkerProgress = 0.0f;
    std::string m_BuildWorkerPhase;
    Build::BuildResult m_BuildWorkerResult;   // written once before m_BuildThreadDone flips
    bool m_BuildRunAfter = false;
    void StartBuildAsync(bool runAfterBuild);
    void PollBuildThread();

    // New Project dialog state (standalone, not project hub)
    bool m_ShowNewProjectDialog = false;
    char m_NewProjDlgName[128] = "MyGame";
    char m_NewProjDlgLocation[512] = "";
    char m_NewProjDlgScene[128] = "Main";
    i32 m_NewProjDlgTemplate = 0;  // 0=Empty 3D, 1=Empty 2D, 2=Empty Mixed
    void DrawNewProjectDialog();

    // ImGui texture descriptor cache for sprite/tilemap previews
    std::unordered_map<std::string, VkDescriptorSet> m_ImGuiTextureCache;
    VkDescriptorSet GetImGuiTexture(const std::string& path);
    void CleanupImGuiTextureCache();

    // Thumbnail generator for non-image assets (3D models, scenes, etc.)
    Assets::ThumbnailGenerator m_ThumbnailGenerator;
    std::vector<std::shared_ptr<Renderer::Texture>> m_ThumbnailTextures; // Keep GPU textures alive
    VkDescriptorSet GetAssetThumbnail(const std::string& path);

    // Texture compression settings (asset browser context menu)
    Assets::TextureCompressionSettings m_TextureCompSettings;
    bool m_ShowCompressionSettings = false;
    std::string m_CompressionTargetPath;       // Path of the texture being compressed
    std::string m_CompressionLastResult;       // Status message after compression
    void DrawTextureCompressionWindow();

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
    bool m_OpenCreateScriptPopup = false;  // deferred OpenPopup("Create Script") request
    char m_NewScriptNameBuf[128] = "";
    std::string m_NewScriptNameError;
    int m_NewScriptTemplate = 0;   // 0 Empty, 1 Rotator, 2 Interactable, 3 Spawner
    void OpenInExternalIDE(const std::string& filePath);
    // Open a script in the IDE jumped to a specific line (VS Code `-g`); falls
    // back to opening the file when the IDE has no line-jump form.
    void OpenScriptAtLine(const std::string& filePath, int line);

    // Load an image into the built-in Pixel Editor and bring that panel up.
    // The path may be project-relative (as material texture paths are) or
    // absolute; it is rooted against the open project the same way the material
    // inspector's external-editor button does it. One entry point so the asset
    // browser, the material inspector and anything added later all behave the
    // same. Returns false and toasts if the image will not load.
    bool OpenTextureInPixelEditor(const std::string& path);
    // Set by OpenTextureInPixelEditor so the panel focuses itself on the next
    // draw - ImGui can only focus a window from inside its own Begin/End.
    bool m_FocusPixelEditor = false;

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

    // Play mode: draw the mobile touch overlay over the Game View and let the
    // mouse act as one touch (View > Simulate Touch Controls).
    bool m_SimulateTouch = false;

    // Project Settings > Input & Touch: which custom-action slot is expanded.
    i32 m_InputTouchEditSlot = -1;

    // UI editor (viewport WYSIWYG) state
    bool m_UIEditMode = false;
    ECS::Entity m_UIEditAutoFor = ECS::INVALID_ENTITY;  // auto-enable tracking (once per selected canvas)
    u32 m_UIEditSelectedElementId = 0;
    ECS::Entity m_UIEditCanvasEntity = ECS::INVALID_ENTITY;
    UIEditDragMode m_UIEditDragMode = UIEditDragMode::None;
    // Context-menu open request, carried from HandleUIEditorInput (update
    // phase, outside the ImGui frame) to DrawUIEditorOverlay (render phase).
    // BeginPopup MUST NOT be called from the update phase: with any popup
    // open it dereferences a null CurrentWindow (the delete-a-menu crash).
    bool m_UIEditContextMenuRequest = false;
    f32 m_UIEditContextMenuDesignX = 0.0f;
    f32 m_UIEditContextMenuDesignY = 0.0f;
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

    // The symbol library: reusable drawings and prefabs, with a browser, nested
    // editing, instantiation and update propagation to every instance.
    //
    // All of that existed and NONE of it was reachable. SymbolLibrary was
    // constructed nowhere, DrawBrowserPanel() was called nowhere, and the class
    // was referenced by no file but its own -- a whole subsystem with no way in,
    // which by the golden rule is not a shipped feature.
    SymbolLibrary m_SymbolLibrary;
    bool m_SymbolLibraryInitialized = false;
    Scripting::AS3Transpiler m_AS3Transpiler;
    char m_AS3TranspileInput[4096] = {};
    std::string m_AS3TranspileOutput;
    void DrawFlashTimelinePanel();
    void DrawSymbolLibraryPanel();

    // Vector Drawing Editor
    VectorDrawingEditor m_VectorDrawingEditor;
    void DrawVectorDrawingPanel();

    // Telemetry
    TelemetrySystem m_Telemetry;

    // Feedback / Bug Reporting System
    FeedbackManager m_FeedbackManager;
    bool m_FeedbackLoaded = false;

    enum class FeedbackTab : u8 { BugReports, Feedback, NewBug, NewFeedback, GitHubIssues, GitHubSettings };
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

    // GitHub Issues tab state
    char m_GitHubOwnerBuf[128] = "MartyChouette";
    char m_GitHubRepoBuf[128] = "TEGE";
    char m_GitHubTokenBuf[256] = {};
    i32 m_SelectedGitHubIssue = -1;
    char m_GitHubIssueSearchBuf[256] = {};
    bool m_GitHubFetching = false;

    void DrawFeedbackPanel();
    void DrawGitHubIssuesTab();
    void DrawGitHubSettingsTab();
    void QuickBugReport();  // F5 instant bug report with diagnostics

    // Discord Bug Report Dialog (Help > Report Bug or Ctrl+Shift+B)
    bool m_ShowDiscordBugDialog = false;
    char m_DiscordBugTitleBuf[256] = {};
    char m_DiscordBugDescBuf[4096] = {};
    bool m_DiscordBugIncludeScreenshot = true;

    // MCP input injection (Ink_Ribbon Tier 0): synthetic key holds, clicks,
    // and typed text driven by AI tools. Merged with LIVE hardware each frame
    // (CaptureFrameState + overlay) while any action is active, so a human at
    // the keyboard and the MCP can drive together. Play mode only.
    struct McpInputAction {
        enum class Kind : u8 { Key, Click, Text } kind = Kind::Key;
        i32 code = 0;            // KeyCode (Key) / mouse button (Click)
        f32 x = 0.0f, y = 0.0f;  // click position, window coords
        f32 remainingMs = 0.0f;  // Key hold / Click press time left
        std::string text;        // Text: remaining characters to type
        f32 charTimer = 0.0f;
    };
    std::vector<McpInputAction> m_McpInputQueue;
    bool m_McpInjecting = false;
    void ProcessMcpInput(f32 deltaTime);

    // R1 GIF recorder: records the game view to an animated GIF. Fidelity
    // picks resolution + capture rate; frames stream to disk as they're taken.
    GifRecorder m_GifRecorder;
    int m_GifFidelity = 1;          // 0 = full/20fps, 1 = half/15fps, 2 = quarter/10fps
    f32 m_GifCaptureAccum = 0.0f;   // wall-clock seconds since last captured frame
    void ToggleGifRecording();
    bool m_DiscordBugIncludeLog = true;
    i32 m_DiscordBugSeverity = 2;  // 0=Crash, 1=Major, 2=Minor, 3=Cosmetic
    enum class DiscordSendState : u8 { Idle, Sending, Sent, Failed };
    DiscordSendState m_DiscordSendState = DiscordSendState::Idle;
    std::string m_DiscordSendError;
    std::vector<u8> m_DiscordScreenshotPng;   // Captured PNG bytes
    void DrawDiscordBugReportDialog();
    void SendDiscordBugReport();
    void CaptureViewportScreenshot();          // Grabs editor viewport pixels -> m_DiscordScreenshotPng
    void DrawSaveDebugPanel();

    // --- Caption Track panel ---
    // A timeline of the cues in a CaptionTrack data asset, the coverage figure, and
    // the linter's findings. The faults in a caption track (overlaps, cues past the
    // end of the audio, stretches with nothing in them) are invisible in a table of
    // numbers and obvious against a time axis.
    void DrawCaptionTrackPanel();
    std::string m_CaptionTrackName;        // which loaded track is shown
    f32 m_CaptionClipLength = 0.0f;        // 0 = unknown; two lint rules need it
    f32 m_CaptionPlayhead = 0.0f;          // seconds, scrubbed by clicking the strip
    void DrawPlayModeDiffDialog();

    // The Debug Recorder / Replay settings popup, hung off the transport strip.
    // One function because both states of the strip open the same popup -- the
    // buffer size it controls governs what happens while PLAYING, and it used to
    // be reachable only once playing had stopped.
    void DrawPlaybackToolsPopup();

    // Keyboard shortcuts whose action lives inside a menu item's body.
    //
    // Seven of these -- New, Open, Save As, Import, Cut, Copy, Paste -- were
    // printed as accelerators in the menu bar and handled nowhere, so the menu
    // item worked when clicked and the key it advertised did nothing.
    //
    // Rather than copy each body out (file dialogs, unsaved-changes prompts,
    // clipboard state), the key RAISES the same request the menu item answers:
    //   if (ImGui::MenuItem(...) || TakeShortcut(Action::NewScene)) { ... }
    // so the key and the menu cannot do different things, because they are one
    // code path.
    void RaiseShortcut(ShortcutAction action) {
        m_PendingShortcuts |= (1ull << static_cast<u64>(action));
    }
    bool TakeShortcut(ShortcutAction action) {
        const u64 bit = 1ull << static_cast<u64>(action);
        const bool hit = (m_PendingShortcuts & bit) != 0;
        m_PendingShortcuts &= ~bit;
        return hit;
    }
    u64 m_PendingShortcuts = 0;

    // Writes the current session's replay for a bug report to point at. Silent:
    // a failure here must not derail the report being filed.
    std::string ExportReplayForDiagnostics();
    void DrawBugReportList();
    void DrawBugReportDetail(BugReport& report);
    void DrawNewBugReportForm();
    void DrawFeedbackList();
    void DrawFeedbackDetail(FeedbackEntry& entry);
    void DrawNewFeedbackForm();
    void ResetBugReportForm();
    void ResetFeedbackForm();
    DiagnosticSnapshot CaptureDiagnostics(bool includeScene);

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

    // Audio Mixer tool
    bool m_ShowAudioMixer = false;
    i32 m_MixerChannelTab = -1; // -1 = All, 0-3 = SFX/Music/UI/Voice

    // Template Creator tool
    bool m_ShowTemplateCreator = false;
    char m_TmplName[128] = "My Template";
    char m_TmplDescription[512] = "";
    char m_TmplAuthor[128] = "";
    i32 m_TmplCategory = 0;    // index into category list
    f32 m_TmplAccentColor[4] = { 0.3f, 0.6f, 1.0f, 1.0f };
    char m_TmplThumbnailPath[512] = "";
    std::vector<Editor::TemplateMetadata> m_ScannedTemplates;
    bool m_TmplNeedsRescan = true;
    i32 m_TmplDeleteConfirm = -1; // index of template pending delete confirmation

    void DrawTemplateCreatorWindow();

    // Template Marketplace
    Editor::TemplateMarketplace m_TemplateMarketplace;
    char m_MarketSearchBuf[128] = "";
    i32 m_MarketCategoryFilter = 0;   // 0=All, 1=Starter, 2=Genre, 3=Systems, 4=Retro, 5=Advanced
    i32 m_MarketMaturityFilter = 0;   // 0=All, 1=Stable, 2=Beta, 3=Preview, 4=Experimental
    i32 m_MarketSortBy = 0;           // 0=Name, 1=Rating, 2=Downloads
    std::string m_MarketDetailId;     // ID of entry with detail popup open

    void DrawTemplateMarketplaceWindow();

    // Notification Toast System
    enum class NotificationType : u8 { Info = 0, Success, Warning, Error };
    struct EditorNotification {
        std::string message;
        NotificationType type = NotificationType::Info;
        f32 lifetime = 3.0f;
        f32 elapsed = 0.0f;
        f32 slideIn = 0.0f;  // 0=offscreen, 1=fully visible (animated)
    };
    std::vector<EditorNotification> m_Notifications;
    void ShowNotification(const std::string& message, NotificationType type = NotificationType::Info);
    void DrawNotifications(f32 deltaTime);

    // Export an AngelScript API stub (declarations only) so external code
    // editors can autocomplete the engine API. Writes .tege/tege_api.as and
    // as.predefined into the open project's root. See Tools > Scripting & Logic.
    void ExportScriptApiStub();

    // Import a .srt subtitle file as a CaptionTrack .enjdata in the open
    // project's assets/data. Reachable from Tools > Scripting & Logic and by
    // dropping the file on the window -- a capability only reachable from C++ is
    // not a capability a person authoring a game has.
    void ImportCaptionFile(const std::string& srtPath);
    Assets::SrtImportOptions m_CaptionImportOptions;

    // Generate the reference set (components, script API, visual-script nodes,
    // data assets) into the open project's docs/generated, then reveal it.
    void GenerateProjectDocumentation();

    // Accent Color Picker
    void DrawAccentColorPicker();

    // Theme Preview
    void DrawThemePreview();

    // Quake-style drop-down console
    bool m_ShowDropConsole = false;
    f32 m_DropConsoleAnim = 0.0f;  // 0=hidden, 1=fully visible (for slide animation)
    char m_DropConsoleInput[512] = {};
    std::vector<std::string> m_DropConsoleHistory;
    int m_DropConsoleHistoryPos = -1;
    void DrawDropConsole(f32 deltaTime);

    // Keyboard Shortcuts Help Modal
    bool m_ShowShortcutsHelp = false;
    char m_ShortcutSearchBuf[64] = "";
    void DrawShortcutsHelpModal();

    // Hierarchy search/filter
    char m_HierarchySearchBuf[128] = "";

    // Cached GPU device name (queried once, never changes at runtime)
    std::string m_CachedGPUName;

    // Entity delete confirmation
    bool m_ShowDeleteConfirm = false;
    std::vector<ECS::Entity> m_PendingDeleteEntities;
    void DrawDeleteConfirmModal();

    // Crash report dialog (shown on next launch after a crash)
    bool m_ShowCrashDialog = false;
    std::string m_PreviousCrashReport;
    void CheckForCrashReport();
    void DrawCrashReportDialog();

    // UV Preview panel
    bool m_ShowUVPreview = false;
    void DrawUVPreviewPanel();

    // Bone weight visualization: cached original vertex colors for restoration
    ECS::Entity m_BoneWeightEntity = ECS::INVALID_ENTITY;
    std::vector<Math::Vector4> m_BoneWeightOriginalColors;
    void ApplyBoneWeightColors(ECS::Entity entity, i32 boneIndex);
    void RestoreBoneWeightColors(ECS::Entity entity);

    // Unsaved changes tracking (dirty flag system)
    bool m_SceneDirty = false;
    f32 m_AutoSaveTimer = 0.0f;
    bool m_ShowUnsavedChangesDialog = false;
    bool m_ShowAutoSaveRecoveryDialog = false;
    std::string m_AutoSaveRecoveryPath;
    std::string m_PendingOpenPath;
    enum class UnsavedAction : u8 { None, Quit, NewScene, OpenScene };
    UnsavedAction m_UnsavedChangesAction = UnsavedAction::None;
    void UpdateWindowTitle();
    void AutoSave();
    void DrawUnsavedChangesDialog();
    void DrawAutoSaveRecoveryDialog();

    // Quit feedback survey dialog
    bool m_ShowQuitFeedbackDialog = false;
    QuitSurvey m_QuitSurvey;
    std::chrono::steady_clock::time_point m_SessionStartTime;
    void DrawQuitFeedbackDialog();
    void FinalizeQuit();
};

} // namespace Editor
} // namespace Enjin
