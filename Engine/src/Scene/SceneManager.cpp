#include "Enjin/Scene/SceneManager.h"
#include "Enjin/Renderer/SceneRenderSettings.h"
#include "Enjin/Build/AssetReader.h"
#include "Enjin/Logging/Log.h"
#include "Enjin/Platform/Paths.h"
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
    m_EnableHRTF = true;
    m_EnableOcclusion = true;
    m_EnableTransmission = true;
    m_WindowIconPath.clear();
    m_WindowTitle = "Enjin Game";
    m_WindowWidth = 1280;
    m_WindowHeight = 720;
    m_Fullscreen = false;
}

bool SceneManager::LoadProject(const std::string& manifestPath) {
    try {
        if (manifestPath.empty()) {
            ENJIN_LOG_ERROR(Asset, "Cannot load project: path is empty");
            return false;
        }

        if (!std::filesystem::is_regular_file(manifestPath)) {
            ENJIN_LOG_ERROR(Asset, "Project path is not a file: %s", manifestPath.c_str());
            return false;
        }

        std::ifstream file(manifestPath);
        if (!file.is_open()) {
            ENJIN_LOG_ERROR(Asset, "Failed to open project file: %s", manifestPath.c_str());
            return false;
        }

        nlohmann::json root;
        file >> root;
        file.close();

        // Only update state after successful parse
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

                // Skip entries with empty paths
                if (entry.path.empty()) {
                    ENJIN_LOG_WARN(Asset, "Scene '%s' has empty path, skipping", entry.name.c_str());
                    continue;
                }

                m_Scenes.push_back(entry);
            }
        }

        // Load the startup flow (authorable boot sequence)
        m_StartupFlow.clear();
        if (root.contains("startupFlow") && root["startupFlow"].is_array()) {
            for (const auto& sj : root["startupFlow"]) {
                StartupFlowStep step;
                std::string t = sj.value("type", "scene");
                step.type = (t == "menu") ? StartupStepType::Menu : StartupStepType::Scene;
                step.scene = sj.value("scene", "");
                std::string a = sj.value("advance", "gameplay");
                if (a == "timer")       step.advance = StartupAdvance::Timer;
                else if (a == "input")  step.advance = StartupAdvance::Input;
                else if (a == "script") step.advance = StartupAdvance::Script;
                else                    step.advance = StartupAdvance::Gameplay;
                step.duration = sj.value("duration", 3.0f);
                m_StartupFlow.push_back(step);
            }
        }

        // Repair hand-edited or legacy manifests (duplicate start flags, etc.)
        u32 fixes = NormalizeSceneList();
        if (fixes > 0) {
            ENJIN_LOG_WARN(Asset, "Project scene list normalized (%u corrections) — will persist on next save", fixes);
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

        // Load audio settings
        if (root.contains("audio") && root["audio"].is_object()) {
            const auto& audio = root["audio"];
            if (audio.contains("enableHRTF")) m_EnableHRTF = audio["enableHRTF"].get<bool>();
            if (audio.contains("enableOcclusion")) m_EnableOcclusion = audio["enableOcclusion"].get<bool>();
            if (audio.contains("enableTransmission")) m_EnableTransmission = audio["enableTransmission"].get<bool>();
        }

        // Load window icon path
        if (root.contains("windowIconPath")) {
            m_WindowIconPath = root["windowIconPath"].get<std::string>();
        }

        // Load build config
        if (root.contains("buildConfig") && root["buildConfig"].is_object()) {
            const auto& bc = root["buildConfig"];
            if (bc.contains("windowTitle")) m_WindowTitle = bc["windowTitle"].get<std::string>();
            if (bc.contains("windowWidth")) m_WindowWidth = bc["windowWidth"].get<u32>();
            if (bc.contains("windowHeight")) m_WindowHeight = bc["windowHeight"].get<u32>();
            if (bc.contains("fullscreen")) m_Fullscreen = bc["fullscreen"].get<bool>();
        }

        ENJIN_LOG_INFO(Asset, "Loaded project '%s' with %zu scenes from %s",
            m_ProjectName.c_str(), m_Scenes.size(), manifestPath.c_str());
        return true;

    } catch (const std::exception& e) {
        ENJIN_LOG_ERROR(Asset, "Error loading project: %s", e.what());
        // Clear state so stale manifest path isn't used for subsequent saves
        m_ManifestPath.clear();
        m_ProjectRoot.clear();
        m_Scenes.clear();
        return false;
    }
}

bool SceneManager::SaveProject(const std::string& manifestPath) {
    m_ManifestPath = manifestPath;  // Remember the path so GetProjectPath() works

    // Never persist an inconsistent scene list
    NormalizeSceneList();

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

        // Save the startup flow (only when authored, so default projects stay clean)
        if (!m_StartupFlow.empty()) {
            nlohmann::json flowArr = nlohmann::json::array();
            for (const auto& step : m_StartupFlow) {
                nlohmann::json sj;
                sj["type"] = (step.type == StartupStepType::Menu) ? "menu" : "scene";
                sj["scene"] = step.scene;
                const char* adv = "gameplay";
                switch (step.advance) {
                    case StartupAdvance::Timer:  adv = "timer";  break;
                    case StartupAdvance::Input:  adv = "input";  break;
                    case StartupAdvance::Script: adv = "script"; break;
                    default:                     adv = "gameplay"; break;
                }
                sj["advance"] = adv;
                sj["duration"] = step.duration;
                flowArr.push_back(sj);
            }
            root["startupFlow"] = flowArr;
        }

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

        // Save audio settings
        nlohmann::json audioJson;
        audioJson["enableHRTF"] = m_EnableHRTF;
        audioJson["enableOcclusion"] = m_EnableOcclusion;
        audioJson["enableTransmission"] = m_EnableTransmission;
        root["audio"] = audioJson;

        // Save window icon path
        if (!m_WindowIconPath.empty()) {
            root["windowIconPath"] = m_WindowIconPath;
        }

        // Save build config
        nlohmann::json buildConfigJson;
        buildConfigJson["windowTitle"] = m_WindowTitle;
        buildConfigJson["windowWidth"] = m_WindowWidth;
        buildConfigJson["windowHeight"] = m_WindowHeight;
        buildConfigJson["fullscreen"] = m_Fullscreen;
        root["buildConfig"] = buildConfigJson;

        // Atomic file save: write to temp file, then rename
        std::string tmpPath = manifestPath + ".tmp";
        {
            std::ofstream file(tmpPath);
            if (!file.is_open()) {
                ENJIN_LOG_ERROR(Asset, "Failed to open temp file for writing: %s", tmpPath.c_str());
                return false;
            }
            file << root.dump(2);
            file.close();

            if (!file.good()) {
                std::error_code ec;
                std::filesystem::remove(tmpPath, ec);
                ENJIN_LOG_ERROR(Asset, "Write failed for temp file: %s", tmpPath.c_str());
                return false;
            }
        }

        {
            std::error_code ec;
            std::filesystem::rename(tmpPath, manifestPath, ec);
            if (ec) {
                std::filesystem::remove(tmpPath, ec);
                ENJIN_LOG_ERROR(Asset, "Failed to rename temp file to project file: %s", ec.message().c_str());
                return false;
            }
        }

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
    // Use const_cast since path is already set — we just re-save to the same path
    return const_cast<SceneManager*>(this)->SaveProject(m_ManifestPath);
}

// --- Scene List Management ---

void SceneManager::AddScene(const SceneEntry& entry) {
    m_Scenes.push_back(entry);
}

void SceneManager::AddScene(const std::string& name, const std::string& path) {
    SceneEntry entry;
    entry.name = name;
    entry.path = path;
    // list size would collide after a remove-then-add; pick a genuinely free index
    entry.buildIndex = NextFreeBuildIndex();
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
    }
    // isStartScene is the authority; don't steal build index 0 from another
    // scene. The start scene does have to be in the build, though.
    if (index < m_Scenes.size() && m_Scenes[index].buildIndex < 0) {
        m_Scenes[index].buildIndex = NextFreeBuildIndex();
    }
}

void SceneManager::MoveScene(usize fromIdx, usize toIdx) {
    if (fromIdx >= m_Scenes.size() || toIdx >= m_Scenes.size()) return;
    if (fromIdx == toIdx) return;

    // Pure list reorder: build order is governed by buildIndex, and
    // AutoAssignBuildIndices maps list order onto indices when asked
    SceneEntry entry = m_Scenes[fromIdx];
    m_Scenes.erase(m_Scenes.begin() + fromIdx);
    m_Scenes.insert(m_Scenes.begin() + toIdx, entry);
}

void SceneManager::AutoAssignBuildIndices() {
    bool anyStart = false;
    for (usize i = 0; i < m_Scenes.size(); ++i) {
        m_Scenes[i].buildIndex = static_cast<i32>(i);
        anyStart = anyStart || m_Scenes[i].isStartScene;
    }
    // Only elect a start scene when none exists — unconditionally flagging
    // scene 0 used to create a second start flag
    if (!m_Scenes.empty() && !anyStart) {
        m_Scenes[0].isStartScene = true;
        ENJIN_LOG_INFO(Asset, "No start scene marked; '%s' elected during auto-assign", m_Scenes[0].name.c_str());
    }
}

i32 SceneManager::NextFreeBuildIndex() const {
    i32 candidate = 0;
    for (;;) {
        bool taken = false;
        for (const auto& scene : m_Scenes) {
            if (scene.buildIndex == candidate) {
                taken = true;
                break;
            }
        }
        if (!taken) {
            return candidate;
        }
        ++candidate;
    }
}

u32 SceneManager::NormalizeSceneList() {
    u32 corrections = 0;

    // Pass 1: exactly one start flag — first in list wins (deterministic)
    bool seenStart = false;
    for (auto& scene : m_Scenes) {
        if (scene.isStartScene) {
            if (seenStart) {
                scene.isStartScene = false;
                ++corrections;
                ENJIN_LOG_WARN(Asset, "Duplicate start scene flag cleared on '%s'", scene.name.c_str());
            } else {
                seenStart = true;
            }
        }
    }

    // Pass 2: repair buildIndex collisions — first holder keeps the index,
    // later duplicates move to the smallest unused one (-1 entries are fine)
    for (usize i = 0; i < m_Scenes.size(); ++i) {
        if (m_Scenes[i].buildIndex < 0) continue;
        bool duplicate = false;
        for (usize j = 0; j < i; ++j) {
            if (m_Scenes[j].buildIndex == m_Scenes[i].buildIndex) {
                duplicate = true;
                break;
            }
        }
        if (duplicate) {
            i32 reassigned = NextFreeBuildIndex();
            ENJIN_LOG_WARN(Asset, "Build index collision: scene '%s' reassigned from %d to %d",
                m_Scenes[i].name.c_str(), m_Scenes[i].buildIndex, reassigned);
            m_Scenes[i].buildIndex = reassigned;
            ++corrections;
        }
    }

    // Pass 3: elect a start scene if none — smallest buildIndex >= 0 wins
    // (ties by list order); if nothing is in the build, the first entry
    if (!m_Scenes.empty() && !seenStart) {
        SceneEntry* pick = nullptr;
        for (auto& scene : m_Scenes) {
            if (scene.buildIndex < 0) continue;
            if (!pick || scene.buildIndex < pick->buildIndex) {
                pick = &scene;
            }
        }
        if (!pick) {
            pick = &m_Scenes.front();
        }
        pick->isStartScene = true;
        ++corrections;
        ENJIN_LOG_WARN(Asset, "No start scene marked; '%s' elected", pick->name.c_str());
    }

    // Pass 4: the start scene must be in the build
    for (auto& scene : m_Scenes) {
        if (scene.isStartScene && scene.buildIndex < 0) {
            scene.buildIndex = NextFreeBuildIndex();
            ++corrections;
            ENJIN_LOG_WARN(Asset, "Start scene '%s' was not in build; assigned index %d",
                scene.name.c_str(), scene.buildIndex);
        }
    }

    return corrections;
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
        if (fullPath.empty()) {
            ENJIN_LOG_ERROR(Asset, "Failed to resolve scene path: %s", entry->path.c_str());
            return false;
        }
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
    // Normally unreachable: NormalizeSceneList guarantees a start scene on
    // load/save. Only hand-built scene lists land here.
    ENJIN_LOG_WARN(Asset, "No scene marked as start scene; falling back to build index 0");
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
        if (fullPath.empty()) {
            ENJIN_LOG_ERROR(Asset, "Failed to resolve scene path: %s", entry->path.c_str());
            return false;
        }
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
    if (halfDuration <= 0.0f) halfDuration = 0.001f;

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
    if (relativePath.empty()) {
        ENJIN_LOG_ERROR(Asset, "ResolvePath: empty path");
        return "";
    }

    if (m_ProjectRoot.empty()) {
        ENJIN_LOG_ERROR(Asset, "ResolvePath: no project loaded, cannot resolve '%s'", relativePath.c_str());
        return "";
    }

    // SN-C5: Validate resolved path stays within project root
    std::string resolved = Platform::ResolveWithinRoot(m_ProjectRoot, relativePath);
    if (resolved.empty()) {
        ENJIN_LOG_ERROR(Asset, "ResolvePath: path traversal rejected: %s", relativePath.c_str());
        return "";
    }
    return resolved;
}

} // namespace Scene
} // namespace Enjin
