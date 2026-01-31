#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/ECS/World.h"
#include "Enjin/Scene/SceneSerializer.h"
#include <string>
#include <vector>
#include <functional>

namespace Enjin {
namespace Scene {

// Scene entry in the project manifest
struct SceneEntry {
    std::string name;       // Display name (e.g., "Main Menu", "Level 1")
    std::string path;       // File path relative to project root (e.g., "scenes/level1.enjin")
    i32 buildIndex = -1;    // Build index (-1 = not in build). 0 = start scene.
    bool isStartScene = false;
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

// Scene Manager - handles multi-scene projects and runtime scene switching
class ENJIN_API SceneManager {
public:
    SceneManager();
    ~SceneManager() = default;

    // Set the world used for scene loading
    void SetWorld(ECS::World* world) { m_World = world; }

    // --- Project / Manifest ---

    // Create a new empty project manifest
    void NewProject(const std::string& projectName);

    // Load project manifest from JSON file
    bool LoadProject(const std::string& manifestPath);

    // Save project manifest to JSON file
    bool SaveProject(const std::string& manifestPath) const;
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

    // Get scenes
    const std::vector<SceneEntry>& GetScenes() const { return m_Scenes; }
    std::vector<SceneEntry>& GetScenes() { return m_Scenes; }
    usize GetSceneCount() const { return m_Scenes.size(); }
    const SceneEntry* GetScene(usize index) const;
    const SceneEntry* GetSceneByName(const std::string& name) const;
    i32 GetSceneIndex(const std::string& name) const;

    // Set which scene is the start scene (build index 0)
    void SetStartScene(usize index);

    // Reorder scenes (move scene at fromIdx to toIdx)
    void MoveScene(usize fromIdx, usize toIdx);

    // Assign build indices based on list order (0, 1, 2...)
    void AutoAssignBuildIndices();

    // --- Runtime Scene Loading ---

    // Load a scene by name (clears current world)
    bool LoadScene(const std::string& name);

    // Load a scene by build index
    bool LoadSceneByIndex(i32 buildIndex);

    // Load the start scene (build index 0)
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

    // --- Callbacks ---
    using SceneLoadedCallback = std::function<void(const std::string& sceneName)>;
    using SceneUnloadedCallback = std::function<void(const std::string& sceneName)>;

    void SetOnSceneLoaded(SceneLoadedCallback cb) { m_OnSceneLoaded = cb; }
    void SetOnSceneUnloaded(SceneUnloadedCallback cb) { m_OnSceneUnloaded = cb; }

private:
    ECS::World* m_World = nullptr;

    // Project data
    std::string m_ProjectName = "Untitled Project";
    std::string m_ManifestPath;    // Path to the .enjinproject file
    std::string m_ProjectRoot;     // Directory containing the manifest
    std::vector<SceneEntry> m_Scenes;

    // Runtime state
    std::string m_CurrentSceneName;

    // Transition state
    TransitionState m_TransitionState = TransitionState::None;
    TransitionType m_TransitionType = TransitionType::Instant;
    f32 m_TransitionAlpha = 0.0f;
    f32 m_TransitionDuration = 0.5f;
    f32 m_TransitionTimer = 0.0f;
    std::string m_PendingSceneName;  // Scene to load during transition

    // Callbacks
    SceneLoadedCallback m_OnSceneLoaded;
    SceneUnloadedCallback m_OnSceneUnloaded;

    // Resolve a scene path (relative to project root)
    std::string ResolvePath(const std::string& relativePath) const;
};

} // namespace Scene
} // namespace Enjin
