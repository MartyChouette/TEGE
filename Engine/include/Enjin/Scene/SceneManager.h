#pragma once

#include "Enjin/Input/InputProjectSettings.h"

#include "Enjin/Platform/Platform.h"
#include "Enjin/ECS/World.h"
#include "Enjin/Scene/SceneSerializer.h"
#include "Enjin/Renderer/SceneRenderSettings.h"
#include "Enjin/Physics/PhysicsBackendType.h"
#include <string>
#include <vector>
#include <functional>

namespace Enjin {
namespace Build { class AssetReader; }
namespace Scene {

// Scene entry in the project manifest
struct SceneEntry {
    std::string name;       // Display name (e.g., "Main Menu", "Level 1")
    std::string path;       // File path relative to project root (e.g., "scenes/level1.enjin")
    i32 buildIndex = -1;    // Build/load order only (-1 = not in build)
    bool isStartScene = false;  // The single authority for which scene starts the game
};

// Scene transition type
enum class TransitionType : u8 {
    Instant,    // Immediate scene swap
    FadeBlack,  // Fade to black, load, fade in
    FadeWhite,  // Fade to white, load, fade in
    CrossFade   // Overlap old/new scenes
};

// Scene transition state
enum class TransitionState : u8 {
    None,
    FadingOut,
    Loading,
    FadingIn
};

// Project mode controls 2D/3D component filtering
enum class ProjectMode : u8 {
    Mode2D = 0,
    Mode3D = 1,
    Mixed = 2    // 2.5D / intentional mixing
};

// Frame rate limit options (mirrors Editor::FrameRateLimit for consistency)
enum class FrameRateLimit : u32 {
    Uncapped = 0,
    FPS30 = 30,
    FPS60 = 60,
    FPS120 = 120,
    FPS144 = 144,
    FPS240 = 240
};

// What to do when the game window loses focus
enum class BackgroundBehavior : u32 {
    RunNormally = 0,
    ReduceTo30 = 1,
    Pause = 2
};

// Game/Project frame rate settings (applied to exported builds)
// NOTE: During play mode in the editor, Game View FPS is controlled via the
// Game View panel dropdown. These settings only affect exported game builds.
struct GameFrameSettings {
    FrameRateLimit targetFrameRate = FrameRateLimit::Uncapped;
    bool vSync = false;
    BackgroundBehavior backgroundBehavior = BackgroundBehavior::ReduceTo30;
    // Fixed physics timestep (ADR-0005). Default OFF so existing projects keep
    // their tuned variable-step behavior; new projects should enable it.
    bool fixedTimestep = false;
    u32 physicsTicksPerSecond = 60;
};

// Startup flow: an authorable boot sequence the exported game runs instead of
// the fixed "engine splash -> start scene -> title menu". Each step is a Scene
// (played until its advance condition) or the built-in Menu. A splash is a Scene
// with Timer, a cutscene a Scene with Input/Script, gameplay a Scene with
// Gameplay (terminal). An empty flow = the classic default. The player runs this;
// the SceneManager just authors and serializes it into the project.
enum class StartupStepType : u8 { Scene = 0, Menu = 1 };
enum class StartupAdvance  : u8 { Gameplay = 0, Timer = 1, Input = 2, Script = 3 };
struct StartupFlowStep {
    StartupStepType type = StartupStepType::Scene;
    std::string scene;                                  // scene path (Scene steps)
    StartupAdvance advance = StartupAdvance::Gameplay;
    f32 duration = 3.0f;                                // seconds, for Timer advance
};

// Scene Manager - handles multi-scene projects and runtime scene switching
class ENJIN_API SceneManager {
public:
    SceneManager();
    ~SceneManager() = default;

    // Set the world used for scene loading
    void SetWorld(ECS::World* world) { m_World = world; }

    // Set an AssetReader for loading scenes from .enjpak files (Player runtime)
    void SetAssetReader(Build::AssetReader* reader) { m_AssetReader = reader; }

    // --- Project / Manifest ---

    // Create a new empty project manifest
    void NewProject(const std::string& projectName);

    // Load project manifest from JSON file
    bool LoadProject(const std::string& manifestPath);

    // Save project manifest to JSON file
    bool SaveProject(const std::string& manifestPath);  // Save and remember path
    bool SaveProject() const;  // Save to last loaded path

    // Get project info
    const std::string& GetProjectName() const { return m_ProjectName; }
    const std::string& GetProjectPath() const { return m_ManifestPath; }
    void SetProjectName(const std::string& name) { m_ProjectName = name; }

    // --- Scene List Management ---

    // Add a scene to the project
    void AddScene(const SceneEntry& entry);
    void AddScene(const std::string& name, const std::string& path);

    // Remove a scene by index
    void RemoveScene(usize index);

    // Startup flow (authorable boot sequence; empty = classic default)
    const std::vector<StartupFlowStep>& GetStartupFlow() const { return m_StartupFlow; }
    std::vector<StartupFlowStep>& GetStartupFlow() { return m_StartupFlow; }

    // Project input settings: custom action names/bindings and the touch
    // layout + touch accessibility defaults. Authored in the editor's
    // Project Settings > Input & Touch tab and shipped with the game.
    const InputSystem::InputProjectSettings& GetInputSettings() const { return m_InputSettings; }
    InputSystem::InputProjectSettings& GetInputSettings() { return m_InputSettings; }

    // Get scenes
    const std::vector<SceneEntry>& GetScenes() const { return m_Scenes; }
    std::vector<SceneEntry>& GetScenes() { return m_Scenes; }
    usize GetSceneCount() const { return m_Scenes.size(); }
    const SceneEntry* GetScene(usize index) const;
    const SceneEntry* GetSceneByName(const std::string& name) const;
    i32 GetSceneIndex(const std::string& name) const;

    // Mark the scene at 'index' as the start scene (clears the flag everywhere
    // else). If that scene was excluded from the build, it gets a free build index.
    void SetStartScene(usize index);

    // Reorder scenes (move scene at fromIdx to toIdx)
    void MoveScene(usize fromIdx, usize toIdx);

    // Assign build indices based on list order (0, 1, 2...)
    void AutoAssignBuildIndices();

    // Repair scene-list invariants: exactly one start scene (when the list is
    // non-empty), no duplicate build indices, start scene included in the build.
    // Runs automatically on LoadProject/SaveProject; each correction logs a
    // warning. Returns the number of corrections (0 = list was consistent).
    u32 NormalizeSceneList();

    // --- Runtime Scene Loading ---

    // Load a scene by name (clears current world)
    bool LoadScene(const std::string& name);

    // ── Deferred scene requests (script-safe scene switching) ──
    // Scripts must never load a scene synchronously: LoadScene clears the
    // world WHILE scripts are iterating it. Bindings call Request* instead,
    // and each runtime consumes the request at its safe point (top of frame,
    // no scripts on the stack). Restart carries no name - the runtime decides
    // what "current" means (the player's loaded scene; the editor's session).
    enum class SceneRequest : u8 { None = 0, Load, Restart };
    void RequestSceneChange(const std::string& name) {
        m_PendingRequest = SceneRequest::Load;
        m_PendingRequestName = name;
    }
    void RequestRestart() { m_PendingRequest = SceneRequest::Restart; }
    SceneRequest TakeSceneRequest(std::string& outName) {
        SceneRequest r = m_PendingRequest;
        outName = m_PendingRequestName;
        m_PendingRequest = SceneRequest::None;
        m_PendingRequestName.clear();
        return r;
    }

    // Load a scene by build index
    bool LoadSceneByIndex(i32 buildIndex);

    // Load the scene marked isStartScene (falls back to build index 0)
    bool LoadStartScene();

    // Load a scene additively (keeps existing entities)
    bool LoadSceneAdditive(const std::string& name);

    // Get the currently loaded scene name
    const std::string& GetCurrentSceneName() const { return m_CurrentSceneName; }

    // --- Transitions ---

    // Load a scene with a transition effect
    void LoadSceneWithTransition(const std::string& name, TransitionType type = TransitionType::FadeBlack, f32 duration = 0.5f);

    // Update transition (call every frame)
    void UpdateTransition(f32 deltaTime);

    // Get transition state for rendering
    TransitionState GetTransitionState() const { return m_TransitionState; }
    f32 GetTransitionAlpha() const { return m_TransitionAlpha; }
    bool IsTransitioning() const { return m_TransitionState != TransitionState::None; }

    // --- Collision group names (up to 32 groups, one per bit) ---
    const std::vector<std::string>& GetCollisionGroupNames() const { return m_CollisionGroupNames; }
    std::vector<std::string>& GetCollisionGroupNames() { return m_CollisionGroupNames; }

    // --- Project mode (2D/3D/Mixed) ---
    void SetProjectMode(ProjectMode mode) { m_ProjectMode = mode; }
    ProjectMode GetProjectMode() const { return m_ProjectMode; }

    // --- Physics backend selection ---
    void SetPhysicsBackendType(Physics::PhysicsBackendType type) { m_PhysicsBackendType = type; }
    Physics::PhysicsBackendType GetPhysicsBackendType() const { return m_PhysicsBackendType; }

    // --- Project-level render defaults ---
    void SetDefaultRenderSettings(const Renderer::SceneRenderSettings& s) { m_DefaultRenderSettings = s; }
    const Renderer::SceneRenderSettings& GetDefaultRenderSettings() const { return m_DefaultRenderSettings; }

    // --- Game frame rate settings ---
    void SetGameFrameSettings(const GameFrameSettings& s) { m_GameFrameSettings = s; }
    const GameFrameSettings& GetGameFrameSettings() const { return m_GameFrameSettings; }

    // --- Audio settings (project-level) ---
    void SetEnableHRTF(bool v) { m_EnableHRTF = v; }
    bool GetEnableHRTF() const { return m_EnableHRTF; }
    void SetEnableOcclusion(bool v) { m_EnableOcclusion = v; }
    bool GetEnableOcclusion() const { return m_EnableOcclusion; }
    void SetEnableTransmission(bool v) { m_EnableTransmission = v; }
    bool GetEnableTransmission() const { return m_EnableTransmission; }

    // --- Window icon (project-level) ---
    void SetWindowIconPath(const std::string& path) { m_WindowIconPath = path; }
    const std::string& GetWindowIconPath() const { return m_WindowIconPath; }

    // --- Build config (project-level, persisted in .enjinproject) ---
    void SetWindowTitle(const std::string& t) { m_WindowTitle = t; }
    const std::string& GetWindowTitle() const { return m_WindowTitle; }
    void SetWindowWidth(u32 w) { m_WindowWidth = w; }
    u32 GetWindowWidth() const { return m_WindowWidth; }
    void SetWindowHeight(u32 h) { m_WindowHeight = h; }
    u32 GetWindowHeight() const { return m_WindowHeight; }
    void SetFullscreen(bool f) { m_Fullscreen = f; }
    bool GetFullscreen() const { return m_Fullscreen; }

    // --- Callbacks ---
    using SceneLoadedCallback = std::function<void(const std::string& sceneName)>;
    using SceneUnloadedCallback = std::function<void(const std::string& sceneName)>;

    void SetOnSceneLoaded(SceneLoadedCallback cb) { m_OnSceneLoaded = cb; }
    void SetOnSceneUnloaded(SceneUnloadedCallback cb) { m_OnSceneUnloaded = cb; }

private:
    // Smallest non-negative build index not used by any scene
    i32 NextFreeBuildIndex() const;

    ECS::World* m_World = nullptr;
    Build::AssetReader* m_AssetReader = nullptr;  // Optional: read scenes from .enjpak

    // Project data
    std::string m_ProjectName = "Untitled Project";
    std::string m_ManifestPath;    // Path to the .enjinproject file
    std::string m_ProjectRoot;     // Directory containing the manifest
    std::vector<SceneEntry> m_Scenes;
    std::vector<StartupFlowStep> m_StartupFlow;
    InputSystem::InputProjectSettings m_InputSettings;

    // Collision group names (index = bit number, up to 32)
    std::vector<std::string> m_CollisionGroupNames;

    // Project mode (2D/3D/Mixed)
    ProjectMode m_ProjectMode = ProjectMode::Mode3D;

    // Physics backend selection
    Physics::PhysicsBackendType m_PhysicsBackendType = Physics::PhysicsBackendType::Auto;

    // Project-level render defaults
    Renderer::SceneRenderSettings m_DefaultRenderSettings;

    // Game frame rate settings
    GameFrameSettings m_GameFrameSettings;

    // Audio settings (project-level, migrated from EditorSettings)
    bool m_EnableHRTF = true;
    bool m_EnableOcclusion = true;
    bool m_EnableTransmission = true;

    // Window icon path (project-level, migrated from EditorSettings)
    std::string m_WindowIconPath;

    // Build config (project-level, persisted in .enjinproject)
    std::string m_WindowTitle = "Enjin Game";
    u32 m_WindowWidth = 1280;
    u32 m_WindowHeight = 720;
    bool m_Fullscreen = false;

    // Runtime state
    std::string m_CurrentSceneName;

    // Transition state
    TransitionState m_TransitionState = TransitionState::None;
    TransitionType m_TransitionType = TransitionType::Instant;
    f32 m_TransitionAlpha = 0.0f;
    f32 m_TransitionDuration = 0.5f;
    f32 m_TransitionTimer = 0.0f;
    std::string m_PendingSceneName;  // Scene to load during transition
    SceneRequest m_PendingRequest = SceneRequest::None;
    std::string m_PendingRequestName;

    // Callbacks
    SceneLoadedCallback m_OnSceneLoaded;
    SceneUnloadedCallback m_OnSceneUnloaded;

    // Resolve a scene path (relative to project root)
    std::string ResolvePath(const std::string& relativePath) const;
};

} // namespace Scene
} // namespace Enjin
