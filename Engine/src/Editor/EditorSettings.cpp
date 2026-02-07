#include "Enjin/Editor/EditorSettings.h"
#include "Enjin/Logging/Log.h"

#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>
#include <algorithm>

using json = nlohmann::json;

namespace Enjin {
namespace Editor {

void EditorSettings::AddRecentProject(const std::string& path) {
    // Remove existing entry if present (will re-add at front)
    recentProjects.erase(
        std::remove(recentProjects.begin(), recentProjects.end(), path),
        recentProjects.end());
    // Insert at front (most recent first)
    recentProjects.insert(recentProjects.begin(), path);
    // Trim to max
    if (static_cast<int>(recentProjects.size()) > MAX_RECENT_PROJECTS) {
        recentProjects.resize(MAX_RECENT_PROJECTS);
    }
}

void EditorSettings::AddRecentComponent(const std::string& name) {
    recentComponents.erase(
        std::remove(recentComponents.begin(), recentComponents.end(), name),
        recentComponents.end());
    recentComponents.insert(recentComponents.begin(), name);
    if (static_cast<int>(recentComponents.size()) > MAX_RECENT_COMPONENTS) {
        recentComponents.resize(MAX_RECENT_COMPONENTS);
    }
}

void EditorSettings::AddRecentVisualScriptNode(const std::string& nodeTypeId) {
    recentVisualScriptNodes.erase(
        std::remove(recentVisualScriptNodes.begin(), recentVisualScriptNodes.end(), nodeTypeId),
        recentVisualScriptNodes.end());
    recentVisualScriptNodes.insert(recentVisualScriptNodes.begin(), nodeTypeId);
    if (static_cast<int>(recentVisualScriptNodes.size()) > MAX_RECENT_VS_NODES) {
        recentVisualScriptNodes.resize(MAX_RECENT_VS_NODES);
    }
}

std::string EditorSettings::GetDefaultPath() {
#ifdef _WIN32
    const char* appdata = std::getenv("APPDATA");
    if (appdata) {
        return std::string(appdata) + "\\enjin\\editor_settings.json";
    }
    return "editor_settings.json";
#elif defined(__APPLE__)
    const char* home = std::getenv("HOME");
    if (home) {
        return std::string(home) + "/Library/Application Support/enjin/editor_settings.json";
    }
    return "editor_settings.json";
#else
    const char* home = std::getenv("HOME");
    if (home) {
        return std::string(home) + "/.config/enjin/editor_settings.json";
    }
    return "editor_settings.json";
#endif
}

bool EditorSettings::Save(const std::string& path) const {
    std::string savePath = path.empty() ? GetDefaultPath() : path;

    // Create directory if needed
    std::filesystem::path dirPath = std::filesystem::path(savePath).parent_path();
    if (!dirPath.empty()) {
        std::error_code ec;
        std::filesystem::create_directories(dirPath, ec);
        if (ec) {
            ENJIN_LOG_WARN(Editor, "Failed to create settings directory: %s", ec.message().c_str());
        }
    }

    try {
        json j;
        j["theme"] = static_cast<u32>(theme);
        j["uiScale"] = uiScale;

        // Visual accessibility
        j["colorblindMode"] = colorblindMode;
        j["colorblindStrength"] = colorblindStrength;
        j["screenBrightness"] = screenBrightness;
        j["screenContrast"] = screenContrast;

        // Motion
        j["reducedMotion"] = reducedMotion;
        j["disableScreenShake"] = disableScreenShake;
        j["disableFOVEffects"] = disableFOVEffects;

        // Subtitles / Cognitive
        j["subtitlesEnabled"] = subtitlesEnabled;
        j["closedCaptionsEnabled"] = closedCaptionsEnabled;
        j["subtitleFontSize"] = subtitleFontSize;
        j["subtitleBgOpacity"] = subtitleBgOpacity;
        j["subtitleSpeakerNames"] = subtitleSpeakerNames;
        j["simplifiedEditor"] = simplifiedEditor;

        // Play Mode
        j["autoFocusMode"] = autoFocusMode;
        j["lockCursorOnPlay"] = lockCursorOnPlay;

        // Performance / Frame Rate
        j["editorFrameRateLimit"] = static_cast<u32>(editorFrameRateLimit);
        j["editorVSync"] = editorVSync;
        j["reduceFrameRateWhenUnfocused"] = reduceFrameRateWhenUnfocused;
        j["unfocusedFrameRate"] = unfocusedFrameRate;
        j["reduceFrameRateWhenIdle"] = reduceFrameRateWhenIdle;
        j["idleTimeoutSeconds"] = idleTimeoutSeconds;
        j["idleFrameRate"] = idleFrameRate;

        // Input
        j["sprintMode"] = sprintMode;
        j["crouchMode"] = crouchMode;
        j["mouseSensitivity"] = mouseSensitivity;
        j["inputPreset"] = inputPreset;
        j["rawMouseInput"] = rawMouseInput;
        j["mouseSmoothing"] = mouseSmoothing;

        // External IDE
        j["externalIDE"] = externalIDE;
        j["customIDEPath"] = customIDEPath;

        // Recent projects
        j["recentProjects"] = json::array();
        for (const auto& rp : recentProjects) {
            j["recentProjects"].push_back(rp);
        }

        // Recent components
        j["recentComponents"] = json::array();
        for (const auto& rc : recentComponents) {
            j["recentComponents"].push_back(rc);
        }

        // Recent visual script nodes
        j["recentVisualScriptNodes"] = json::array();
        for (const auto& rn : recentVisualScriptNodes) {
            j["recentVisualScriptNodes"].push_back(rn);
        }

        std::ofstream file(savePath);
        if (!file.is_open()) {
            ENJIN_LOG_ERROR(Editor, "Failed to open settings file for writing: %s", savePath.c_str());
            return false;
        }

        file << j.dump(2);
        file.close();
        ENJIN_LOG_INFO(Editor, "Saved editor settings to %s", savePath.c_str());
        return true;

    } catch (const std::exception& e) {
        ENJIN_LOG_ERROR(Editor, "Failed to save editor settings: %s", e.what());
        return false;
    }
}

bool EditorSettings::Load(const std::string& path) {
    std::string loadPath = path.empty() ? GetDefaultPath() : path;

    if (!std::filesystem::exists(loadPath)) {
        // No settings file yet, use defaults
        return false;
    }

    try {
        std::ifstream file(loadPath);
        if (!file.is_open()) {
            ENJIN_LOG_WARN(Editor, "Failed to open settings file: %s", loadPath.c_str());
            return false;
        }

        json j;
        file >> j;
        file.close();

        if (j.contains("theme")) theme = static_cast<EditorTheme>(j["theme"].get<u32>());
        if (j.contains("uiScale")) uiScale = j["uiScale"].get<f32>();

        // Visual accessibility
        if (j.contains("colorblindMode")) colorblindMode = j["colorblindMode"].get<u32>();
        if (j.contains("colorblindStrength")) colorblindStrength = j["colorblindStrength"].get<f32>();
        if (j.contains("screenBrightness")) screenBrightness = j["screenBrightness"].get<f32>();
        if (j.contains("screenContrast")) screenContrast = j["screenContrast"].get<f32>();

        // Motion
        if (j.contains("reducedMotion")) reducedMotion = j["reducedMotion"].get<bool>();
        if (j.contains("disableScreenShake")) disableScreenShake = j["disableScreenShake"].get<bool>();
        if (j.contains("disableFOVEffects")) disableFOVEffects = j["disableFOVEffects"].get<bool>();

        // Subtitles / Cognitive
        if (j.contains("subtitlesEnabled")) subtitlesEnabled = j["subtitlesEnabled"].get<bool>();
        if (j.contains("closedCaptionsEnabled")) closedCaptionsEnabled = j["closedCaptionsEnabled"].get<bool>();
        if (j.contains("subtitleFontSize")) subtitleFontSize = j["subtitleFontSize"].get<f32>();
        if (j.contains("subtitleBgOpacity")) subtitleBgOpacity = j["subtitleBgOpacity"].get<f32>();
        if (j.contains("subtitleSpeakerNames")) subtitleSpeakerNames = j["subtitleSpeakerNames"].get<bool>();
        if (j.contains("simplifiedEditor")) simplifiedEditor = j["simplifiedEditor"].get<bool>();

        // Play Mode
        if (j.contains("autoFocusMode")) autoFocusMode = j["autoFocusMode"].get<bool>();
        if (j.contains("lockCursorOnPlay")) lockCursorOnPlay = j["lockCursorOnPlay"].get<bool>();

        // Performance / Frame Rate
        if (j.contains("editorFrameRateLimit")) {
            u32 val = j["editorFrameRateLimit"].get<u32>();
            // Validate the enum value
            if (val == 0 || val == 30 || val == 60 || val == 120 || val == 144 || val == 240) {
                editorFrameRateLimit = static_cast<FrameRateLimit>(val);
            }
        }
        if (j.contains("editorVSync")) editorVSync = j["editorVSync"].get<bool>();
        if (j.contains("reduceFrameRateWhenUnfocused")) reduceFrameRateWhenUnfocused = j["reduceFrameRateWhenUnfocused"].get<bool>();
        if (j.contains("unfocusedFrameRate")) unfocusedFrameRate = j["unfocusedFrameRate"].get<u32>();
        if (j.contains("reduceFrameRateWhenIdle")) reduceFrameRateWhenIdle = j["reduceFrameRateWhenIdle"].get<bool>();
        if (j.contains("idleTimeoutSeconds")) idleTimeoutSeconds = j["idleTimeoutSeconds"].get<f32>();
        if (j.contains("idleFrameRate")) idleFrameRate = j["idleFrameRate"].get<u32>();

        // Input
        if (j.contains("sprintMode")) sprintMode = j["sprintMode"].get<u32>();
        if (j.contains("crouchMode")) crouchMode = j["crouchMode"].get<u32>();
        if (j.contains("mouseSensitivity")) mouseSensitivity = j["mouseSensitivity"].get<f32>();
        if (j.contains("inputPreset")) inputPreset = j["inputPreset"].get<u32>();
        if (j.contains("rawMouseInput")) rawMouseInput = j["rawMouseInput"].get<bool>();
        if (j.contains("mouseSmoothing")) mouseSmoothing = j["mouseSmoothing"].get<f32>();

        // External IDE
        if (j.contains("externalIDE")) externalIDE = j["externalIDE"].get<u32>();
        if (j.contains("customIDEPath")) customIDEPath = j["customIDEPath"].get<std::string>();

        // Recent projects
        if (j.contains("recentProjects") && j["recentProjects"].is_array()) {
            recentProjects.clear();
            for (const auto& rp : j["recentProjects"]) {
                if (rp.is_string()) {
                    recentProjects.push_back(rp.get<std::string>());
                }
            }
        }

        // Recent components
        if (j.contains("recentComponents") && j["recentComponents"].is_array()) {
            recentComponents.clear();
            for (const auto& rc : j["recentComponents"]) {
                if (rc.is_string()) {
                    recentComponents.push_back(rc.get<std::string>());
                }
            }
        }

        // Recent visual script nodes
        if (j.contains("recentVisualScriptNodes") && j["recentVisualScriptNodes"].is_array()) {
            recentVisualScriptNodes.clear();
            for (const auto& rn : j["recentVisualScriptNodes"]) {
                if (rn.is_string()) {
                    recentVisualScriptNodes.push_back(rn.get<std::string>());
                }
            }
        }

        ENJIN_LOG_INFO(Editor, "Loaded editor settings from %s", loadPath.c_str());
        return true;

    } catch (const std::exception& e) {
        ENJIN_LOG_ERROR(Editor, "Failed to load editor settings: %s", e.what());
        return false;
    }
}

} // namespace Editor
} // namespace Enjin
