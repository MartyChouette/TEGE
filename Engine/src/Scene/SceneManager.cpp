#include "Enjin/Scene/SceneManager.h"
#include "Enjin/Renderer/SceneRenderSettings.h"
#include "Enjin/Build/AssetReader.h"
#include "Enjin/Logging/Log.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>
#include <algorithm>

namespace Enjin {
namespace Scene {

SceneManager::SceneManager() {
    // Initialize 32 collision group slots, index 0 = "Default"
    m_CollisionGroupNames.resize(32);
    m_CollisionGroupNames[0] = "Default";
}

// --- Project / Manifest ---

void SceneManager::NewProject(const std::string& projectName) {
    m_ProjectName = projectName;
    m_Scenes.clear();
    m_ManifestPath.clear();
    m_ProjectRoot.clear();
    m_CurrentSceneName.clear();
    m_DefaultRenderSettings = Renderer::SceneRenderSettings{};
    m_GameFrameSettings = GameFrameSettings{};
    m_ProjectMode = ProjectMode::Mode3D;
    m_PhysicsBackendType = Physics::PhysicsBackendType::Auto;
    m_CollisionGroupNames.clear();
    m_CollisionGroupNames.resize(32);
    m_CollisionGroupNames[0] = "Default";
}

bool SceneManager::LoadProject(const std::string& manifestPath) {
    try {
        std::ifstream file(manifestPath);
        if (!file.is_open()) {
            ENJIN_LOG_ERROR(Asset, "Failed to open project file: %s", manifestPath.c_str());
            return false;
        }

        nlohmann::json root;
        file >> root;
        file.close();

        m_ManifestPath = manifestPath;
        m_ProjectRoot = std::filesystem::path(manifestPath).parent_path().string();
        m_ProjectName = root.value("projectName", "Untitled Project");

        m_Scenes.clear();
        if (root.contains("scenes") && root["scenes"].is_array()) {
            for (const auto& sceneJson : root["scenes"]) {
                SceneEntry entry;
                entry.name = sceneJson.value("name", "Unnamed Scene");
                entry.path = sceneJson.value("path", "");
                entry.buildIndex = sceneJson.value("buildIndex", -1);
                entry.isStartScene = sceneJson.value("isStartScene", false);
                m_Scenes.push_back(entry);
            }
        }

        // Load collision group names
        m_CollisionGroupNames.clear();
        m_CollisionGroupNames.resize(32);
        m_CollisionGroupNames[0] = "Default";
        if (root.contains("collisionGroups") && root["collisionGroups"].is_array()) {
            const auto& groups = root["collisionGroups"];
            for (usize i = 0; i < groups.size() && i < 32; ++i) {
                m_CollisionGroupNames[i] = groups[i].get<std::string>();
            }
        }

        // Load project mode
        if (root.contains("projectMode")) {
            int mode = root["projectMode"].get<int>();
            if (mode >= 0 && mode <= 2) m_ProjectMode = static_cast<ProjectMode>(mode);
        }

        // Load physics backend type
        m_PhysicsBackendType = Physics::PhysicsBackendType::Auto;
        if (root.contains("physicsBackend")) {
            int val = root["physicsBackend"].get<int>();
            if (val >= 0 && val <= 2) m_PhysicsBackendType = static_cast<Physics::PhysicsBackendType>(val);
        }

        // Load project-level render defaults
        if (root.contains("defaultRenderSettings")) {
            m_DefaultRenderSettings = Renderer::DeserializeRenderSettings(root["defaultRenderSettings"]);
        } else {
            m_DefaultRenderSettings = Renderer::SceneRenderSettings{};
        }

        // Load game frame settings
        m_GameFrameSettings = GameFrameSettings{};
        if (root.contains("frameSettings") && root["frameSettings"].is_object()) {
            const auto& fs = root["frameSettings"];
            if (fs.contains("targetFrameRate")) {
                u32 val = fs["targetFrameRate"].get<u32>();
                if (val == 0 || val == 30 || val == 60 || val == 120 || val == 144 || val == 240) {
                    m_GameFrameSettings.targetFrameRate = static_cast<FrameRateLimit>(val);
                }
            }
            if (fs.contains("vSync")) m_GameFrameSettings.vSync = fs["vSync"].get<bool>();
            if (fs.contains("backgroundBehavior")) {
                u32 val = fs["backgroundBehavior"].get<u32>();
                if (val <= 2) m_GameFrameSettings.backgroundBehavior = static_cast<BackgroundBehavior>(val);
            }
        }

        ENJIN_LOG_INFO(Asset, "Loaded project '%s' with %zu scenes from %s",
            m_ProjectName.c_str(), m_Scenes.size(), manifestPath.c_str());
        return true;

    } catch (const std::exception& e) {
        ENJIN_LOG_ERROR(Asset, "Error loading project: %s", e.what());
        return false;
    }
}

bool SceneManager::SaveProject(const std::string& manifestPath) const {
    try {
        nlohmann::json root;
        root["projectName"] = m_ProjectName;
        root["version"] = "1.0";

        nlohmann::json scenesArray = nlohmann::json::array();
        for (const auto& scene : m_Scenes) {
            nlohmann::json sceneJson;
            sceneJson["name"] = scene.name;
            sceneJson["path"] = scene.path;
            sceneJson["buildIndex"] = scene.buildIndex;
            sceneJson["isStartScene"] = scene.isStartScene;
            scenesArray.push_back(sceneJson);
        }
        root["scenes"] = scenesArray;

        // Save collision group names
        nlohmann::json groupsArray = nlohmann::json::array();
        for (const auto& name : m_CollisionGroupNames) {
            groupsArray.push_back(name);
        }
        root["collisionGroups"] = groupsArray;

        // Save project mode
        root["projectMode"] = static_cast<int>(m_ProjectMode);

        // Save physics backend type
        root["physicsBackend"] = static_cast<int>(m_PhysicsBackendType);

        // Save project-level render defaults
        root["defaultRenderSettings"] = Renderer::SerializeRenderSettings(m_DefaultRenderSettings);

        // Save game frame settings
        nlohmann::json frameSettingsJson;
        frameSettingsJson["targetFrameRate"] = static_cast<u32>(m_GameFrameSettings.targetFrameRate);
        frameSettingsJson["vSync"] = m_GameFrameSettings.vSync;
        frameSettingsJson["backgroundBehavior"] = static_cast<u32>(m_GameFrameSettings.backgroundBehavior);
        root["frameSettings"] = frameSettingsJson;

        std::ofstream file(manifestPath);
        if (!file.is_open()) {
            ENJIN_LOG_ERROR(Asset, "Failed to write project file: %s", manifestPath.c_str());
            return false;
        }
        file << root.dump(2);
        file.close();

        ENJIN_LOG_INFO(Asset, "Saved project '%s' to %s", m_ProjectName.c_str(), manifestPath.c_str());
        return true;

    } catch (const std::exception& e) {
        ENJIN_LOG_ERROR(Asset, "Error saving project: %s", e.what());
        return false;
    }
}

bool SceneManager::SaveProject() const {
    if (m_ManifestPath.empty()) {
        ENJIN_LOG_ERROR(Asset, "No project path set, use SaveProject(path)");
        return false;
    }
    return SaveProject(m_ManifestPath);
}

// --- Scene List Management ---

void SceneManager::AddScene(const SceneEntry& entry) {
    m_Scenes.push_back(entry);
}

void SceneManager::AddScene(const std::string& name, const std::string& path) {
    SceneEntry entry;
    entry.name = name;
    entry.path = path;
    entry.buildIndex = static_cast<i32>(m_Scenes.size());
    m_Scenes.push_back(entry);
}

void SceneManager::RemoveScene(usize index) {
    if (index < m_Scenes.size()) {
        m_Scenes.erase(m_Scenes.begin() + index);
    }
}

const SceneEntry* SceneManager::GetScene(usize index) const {
    if (index < m_Scenes.size()) return &m_Scenes[index];
    return nullptr;
}

const SceneEntry* SceneManager::GetSceneByName(const std::string& name) const {
    for (const auto& scene : m_Scenes) {
        if (scene.name == name) return &scene;
    }
    return nullptr;
}

i32 SceneManager::GetSceneIndex(const std::string& name) const {
    for (usize i = 0; i < m_Scenes.size(); ++i) {
        if (m_Scenes[i].name == name) return static_cast<i32>(i);
    }
    return -1;
}

void SceneManager::SetStartScene(usize index) {
    for (usize i = 0; i < m_Scenes.size(); ++i) {
        m_Scenes[i].isStartScene = (i == index);
        if (i == index) {
            m_Scenes[i].buildIndex = 0;
        }
    }
}

void SceneManager::MoveScene(usize fromIdx, usize toIdx) {
    if (fromIdx >= m_Scenes.size() || toIdx >= m_Scenes.size()) return;
    if (fromIdx == toIdx) return;

    SceneEntry entry = m_Scenes[fromIdx];
    m_Scenes.erase(m_Scenes.begin() + fromIdx);
    m_Scenes.insert(m_Scenes.begin() + toIdx, entry);
}

void SceneManager::AutoAssignBuildIndices() {
    for (usize i = 0; i < m_Scenes.size(); ++i) {
        m_Scenes[i].buildIndex = static_cast<i32>(i);
    }
    if (!m_Scenes.empty()) {
        m_Scenes[0].isStartScene = true;
    }
}

// --- Runtime Scene Loading ---

bool SceneManager::LoadScene(const std::string& name) {
    if (!m_World) {
        ENJIN_LOG_ERROR(Asset, "No world set for scene loading");
        return false;
    }

    const SceneEntry* entry = GetSceneByName(name);
    if (!entry) {
        ENJIN_LOG_ERROR(Asset, "Scene not found: %s", name.c_str());
        return false;
    }

    // Notify unload
    if (!m_CurrentSceneName.empty() && m_OnSceneUnloaded) {
        m_OnSceneUnloaded(m_CurrentSceneName);
    }

    SceneSerializer serializer(m_World);
    DeserializationResult result;

    if (m_AssetReader) {
        // Load from .enjpak via AssetReader (Player runtime)
        auto data = m_AssetReader->ReadFile(entry->path);
        if (data.empty()) {
            ENJIN_LOG_ERROR(Asset, "Failed to read scene from pack: %s", entry->path.c_str());
            return false;
        }
        std::string sceneStr(data.begin(), data.end());
        result = serializer.LoadFromString(sceneStr, true);
    } else {
        // Load from filesystem (Editor)
        std::string fullPath = ResolvePath(entry->path);
        result = serializer.Load(fullPath, true);
    }

    if (result.success) {
        m_CurrentSceneName = name;
        if (m_OnSceneLoaded) {
            m_OnSceneLoaded(name);
        }
        ENJIN_LOG_INFO(Asset, "Loaded scene: %s (%zu entities)", name.c_str(), result.entities.size());
        return true;
    }

    ENJIN_LOG_ERROR(Asset, "Failed to load scene '%s': %s", name.c_str(), result.error.c_str());
    return false;
}

bool SceneManager::LoadSceneByIndex(i32 buildIndex) {
    for (const auto& scene : m_Scenes) {
        if (scene.buildIndex == buildIndex) {
            return LoadScene(scene.name);
        }
    }
    ENJIN_LOG_ERROR(Asset, "No scene with build index %d", buildIndex);
    return false;
}

bool SceneManager::LoadStartScene() {
    for (const auto& scene : m_Scenes) {
        if (scene.isStartScene) {
            return LoadScene(scene.name);
        }
    }
    // Fallback to build index 0
    return LoadSceneByIndex(0);
}

bool SceneManager::LoadSceneAdditive(const std::string& name) {
    if (!m_World) {
        ENJIN_LOG_ERROR(Asset, "No world set for scene loading");
        return false;
    }

    const SceneEntry* entry = GetSceneByName(name);
    if (!entry) {
        ENJIN_LOG_ERROR(Asset, "Scene not found: %s", name.c_str());
        return false;
    }

    SceneSerializer serializer(m_World);
    DeserializationResult result;

    if (m_AssetReader) {
        // Load from .enjpak via AssetReader (Player runtime)
        auto data = m_AssetReader->ReadFile(entry->path);
        if (data.empty()) {
            ENJIN_LOG_ERROR(Asset, "Failed to read scene additively from pack: %s", entry->path.c_str());
            return false;
        }
        std::string sceneStr(data.begin(), data.end());
        // LoadFromString with clearExisting=false for additive loading
        result = serializer.LoadFromString(sceneStr, false);
    } else {
        // Load from filesystem (Editor)
        std::string fullPath = ResolvePath(entry->path);
        result = serializer.LoadAdditive(fullPath);
    }

    if (result.success) {
        ENJIN_LOG_INFO(Asset, "Loaded scene additive: %s (%zu entities)", name.c_str(), result.entities.size());
        return true;
    }

    ENJIN_LOG_ERROR(Asset, "Failed to load scene additive '%s': %s", name.c_str(), result.error.c_str());
    return false;
}

// --- Transitions ---

void SceneManager::LoadSceneWithTransition(const std::string& name, TransitionType type, f32 duration) {
    if (type == TransitionType::Instant) {
        LoadScene(name);
        return;
    }

    m_PendingSceneName = name;
    m_TransitionType = type;
    m_TransitionDuration = duration;
    m_TransitionTimer = 0.0f;
    m_TransitionAlpha = 0.0f;
    m_TransitionState = TransitionState::FadingOut;
}

void SceneManager::UpdateTransition(f32 deltaTime) {
    if (m_TransitionState == TransitionState::None) return;

    m_TransitionTimer += deltaTime;
    f32 halfDuration = m_TransitionDuration * 0.5f;

    switch (m_TransitionState) {
        case TransitionState::FadingOut:
            m_TransitionAlpha = m_TransitionTimer / halfDuration;
            if (m_TransitionAlpha >= 1.0f) {
                m_TransitionAlpha = 1.0f;
                m_TransitionState = TransitionState::Loading;
                // Load the scene while fully faded
                LoadScene(m_PendingSceneName);
                m_TransitionTimer = 0.0f;
            }
            break;

        case TransitionState::Loading:
            // Scene loaded, start fading in
            m_TransitionState = TransitionState::FadingIn;
            m_TransitionTimer = 0.0f;
            break;

        case TransitionState::FadingIn:
            m_TransitionAlpha = 1.0f - (m_TransitionTimer / halfDuration);
            if (m_TransitionAlpha <= 0.0f) {
                m_TransitionAlpha = 0.0f;
                m_TransitionState = TransitionState::None;
                m_PendingSceneName.clear();
            }
            break;

        default:
            break;
    }
}

// --- Helpers ---

std::string SceneManager::ResolvePath(const std::string& relativePath) const {
    if (m_ProjectRoot.empty()) {
        return relativePath;
    }
    std::filesystem::path full = std::filesystem::path(m_ProjectRoot) / relativePath;
    return full.string();
}

} // namespace Scene
} // namespace Enjin
