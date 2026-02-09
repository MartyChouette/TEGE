#include "Enjin/Editor/EditorSettings.h"
#include "Enjin/Logging/Log.h"

#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>
#include <algorithm>

using json = nlohmann::json;

namespace Enjin {
namespace Editor {

// Default accent color configs for each theme
AccentColorConfig AccentColorConfig::DefaultDark() {
    AccentColorConfig c;
    c.button       = {0.22f, 0.27f, 0.24f, 1.00f};
    c.buttonHover  = {0.30f, 0.38f, 0.32f, 1.00f};
    c.buttonActive = {0.35f, 0.45f, 0.38f, 1.00f};
    c.checkMark    = {0.55f, 0.78f, 0.58f, 1.00f};
    c.sliderGrab   = {0.50f, 0.70f, 0.52f, 1.00f};
    c.sliderGrabActive = {0.60f, 0.82f, 0.63f, 1.00f};
    c.resizeGrip   = {0.30f, 0.40f, 0.33f, 0.50f};
    c.textSelected = {0.35f, 0.50f, 0.38f, 0.50f};
    c.dragDropTarget = {0.55f, 0.78f, 0.58f, 0.90f};
    c.tabActive    = {0.25f, 0.33f, 0.28f, 1.00f};
    c.tabHovered   = {0.35f, 0.45f, 0.38f, 1.00f};
    c.useCustom = false;
    return c;
}

AccentColorConfig AccentColorConfig::DefaultLight() {
    AccentColorConfig c;
    c.button       = {0.78f, 0.84f, 0.79f, 1.00f};
    c.buttonHover  = {0.68f, 0.76f, 0.70f, 1.00f};
    c.buttonActive = {0.58f, 0.68f, 0.61f, 1.00f};
    c.checkMark    = {0.20f, 0.50f, 0.30f, 1.00f};
    c.sliderGrab   = {0.28f, 0.55f, 0.35f, 1.00f};
    c.sliderGrabActive = {0.20f, 0.48f, 0.28f, 1.00f};
    c.resizeGrip   = {0.55f, 0.65f, 0.58f, 0.50f};
    c.textSelected = {0.50f, 0.65f, 0.53f, 0.50f};
    c.dragDropTarget = {0.30f, 0.60f, 0.35f, 0.90f};
    c.tabActive    = {0.70f, 0.80f, 0.73f, 1.00f};
    c.tabHovered   = {0.60f, 0.72f, 0.63f, 1.00f};
    c.useCustom = false;
    return c;
}

AccentColorConfig AccentColorConfig::DefaultHighContrastDark() {
    AccentColorConfig c;
    c.button       = {0.15f, 0.18f, 0.16f, 1.00f};
    c.buttonHover  = {0.25f, 0.35f, 0.28f, 1.00f};
    c.buttonActive = {0.35f, 0.48f, 0.38f, 1.00f};
    c.checkMark    = {0.50f, 0.90f, 0.55f, 1.00f};
    c.sliderGrab   = {0.50f, 0.90f, 0.55f, 1.00f};
    c.sliderGrabActive = {0.60f, 0.95f, 0.65f, 1.00f};
    c.resizeGrip   = {0.45f, 0.65f, 0.48f, 0.60f};
    c.textSelected = {0.30f, 0.55f, 0.35f, 0.60f};
    c.dragDropTarget = {0.50f, 0.90f, 0.55f, 1.00f};
    c.tabActive    = {0.20f, 0.30f, 0.23f, 1.00f};
    c.tabHovered   = {0.30f, 0.40f, 0.33f, 1.00f};
    c.useCustom = false;
    return c;
}

AccentColorConfig AccentColorConfig::DefaultHighContrastLight() {
    AccentColorConfig c;
    c.button       = {0.82f, 0.87f, 0.83f, 1.00f};
    c.buttonHover  = {0.68f, 0.76f, 0.70f, 1.00f};
    c.buttonActive = {0.55f, 0.65f, 0.58f, 1.00f};
    c.checkMark    = {0.10f, 0.45f, 0.18f, 1.00f};
    c.sliderGrab   = {0.10f, 0.45f, 0.18f, 1.00f};
    c.sliderGrabActive = {0.08f, 0.38f, 0.15f, 1.00f};
    c.resizeGrip   = {0.28f, 0.42f, 0.32f, 0.60f};
    c.textSelected = {0.38f, 0.55f, 0.42f, 0.50f};
    c.dragDropTarget = {0.15f, 0.55f, 0.25f, 0.90f};
    c.tabActive    = {0.75f, 0.84f, 0.78f, 1.00f};
    c.tabHovered   = {0.65f, 0.74f, 0.67f, 1.00f};
    c.useCustom = false;
    return c;
}

AccentColorConfig AccentColorConfig::DefaultSNES() {
    AccentColorConfig c;
    c.button       = {0.25f, 0.20f, 0.35f, 1.00f};
    c.buttonHover  = {0.35f, 0.28f, 0.50f, 1.00f};
    c.buttonActive = {0.45f, 0.38f, 0.62f, 1.00f};
    c.checkMark    = {0.68f, 0.55f, 0.85f, 1.00f};
    c.sliderGrab   = {0.58f, 0.48f, 0.75f, 1.00f};
    c.sliderGrabActive = {0.70f, 0.58f, 0.88f, 1.00f};
    c.resizeGrip   = {0.40f, 0.32f, 0.55f, 0.50f};
    c.textSelected = {0.45f, 0.35f, 0.60f, 0.50f};
    c.dragDropTarget = {0.68f, 0.55f, 0.85f, 0.90f};
    c.tabActive    = {0.30f, 0.25f, 0.42f, 1.00f};
    c.tabHovered   = {0.40f, 0.32f, 0.55f, 1.00f};
    c.useCustom = false;
    return c;
}

AccentColorConfig AccentColorConfig::DefaultPS2() {
    AccentColorConfig c;
    c.button       = {0.12f, 0.18f, 0.35f, 1.00f};
    c.buttonHover  = {0.18f, 0.28f, 0.52f, 1.00f};
    c.buttonActive = {0.24f, 0.38f, 0.65f, 1.00f};
    c.checkMark    = {0.35f, 0.55f, 0.95f, 1.00f};
    c.sliderGrab   = {0.28f, 0.45f, 0.85f, 1.00f};
    c.sliderGrabActive = {0.38f, 0.58f, 0.98f, 1.00f};
    c.resizeGrip   = {0.20f, 0.32f, 0.60f, 0.50f};
    c.textSelected = {0.20f, 0.35f, 0.65f, 0.50f};
    c.dragDropTarget = {0.35f, 0.55f, 0.95f, 0.90f};
    c.tabActive    = {0.16f, 0.24f, 0.45f, 1.00f};
    c.tabHovered   = {0.22f, 0.32f, 0.58f, 1.00f};
    c.useCustom = false;
    return c;
}

AccentColorConfig AccentColorConfig::DefaultXbox() {
    AccentColorConfig c;
    c.button       = {0.12f, 0.22f, 0.12f, 1.00f};
    c.buttonHover  = {0.18f, 0.38f, 0.18f, 1.00f};
    c.buttonActive = {0.22f, 0.50f, 0.22f, 1.00f};
    c.checkMark    = {0.30f, 0.78f, 0.30f, 1.00f};
    c.sliderGrab   = {0.25f, 0.65f, 0.25f, 1.00f};
    c.sliderGrabActive = {0.35f, 0.82f, 0.35f, 1.00f};
    c.resizeGrip   = {0.20f, 0.42f, 0.20f, 0.50f};
    c.textSelected = {0.18f, 0.45f, 0.18f, 0.50f};
    c.dragDropTarget = {0.30f, 0.78f, 0.30f, 0.90f};
    c.tabActive    = {0.15f, 0.30f, 0.15f, 1.00f};
    c.tabHovered   = {0.20f, 0.42f, 0.20f, 1.00f};
    c.useCustom = false;
    return c;
}

AccentColorConfig AccentColorConfig::DefaultDreamcast() {
    AccentColorConfig c;
    c.button       = {0.28f, 0.20f, 0.13f, 1.00f};
    c.buttonHover  = {0.42f, 0.28f, 0.15f, 1.00f};
    c.buttonActive = {0.55f, 0.35f, 0.18f, 1.00f};
    c.checkMark    = {0.90f, 0.55f, 0.18f, 1.00f};
    c.sliderGrab   = {0.80f, 0.48f, 0.15f, 1.00f};
    c.sliderGrabActive = {0.95f, 0.58f, 0.20f, 1.00f};
    c.resizeGrip   = {0.50f, 0.32f, 0.15f, 0.50f};
    c.textSelected = {0.50f, 0.32f, 0.15f, 0.50f};
    c.dragDropTarget = {0.90f, 0.55f, 0.18f, 0.90f};
    c.tabActive    = {0.35f, 0.24f, 0.14f, 1.00f};
    c.tabHovered   = {0.48f, 0.32f, 0.16f, 1.00f};
    c.useCustom = false;
    return c;
}

AccentColorConfig AccentColorConfig::DefaultSegaSaturn() {
    AccentColorConfig c;
    c.button       = {0.18f, 0.20f, 0.35f, 1.00f};
    c.buttonHover  = {0.28f, 0.32f, 0.52f, 1.00f};
    c.buttonActive = {0.35f, 0.40f, 0.62f, 1.00f};
    c.checkMark    = {0.45f, 0.52f, 0.88f, 1.00f};
    c.sliderGrab   = {0.38f, 0.44f, 0.78f, 1.00f};
    c.sliderGrabActive = {0.50f, 0.56f, 0.92f, 1.00f};
    c.resizeGrip   = {0.30f, 0.35f, 0.58f, 0.50f};
    c.textSelected = {0.28f, 0.32f, 0.55f, 0.50f};
    c.dragDropTarget = {0.45f, 0.52f, 0.88f, 0.90f};
    c.tabActive    = {0.22f, 0.25f, 0.42f, 1.00f};
    c.tabHovered   = {0.32f, 0.36f, 0.55f, 1.00f};
    c.useCustom = false;
    return c;
}

AccentColorConfig AccentColorConfig::DefaultGBA() {
    AccentColorConfig c;
    c.button       = {0.20f, 0.16f, 0.35f, 1.00f};
    c.buttonHover  = {0.30f, 0.24f, 0.52f, 1.00f};
    c.buttonActive = {0.40f, 0.32f, 0.65f, 1.00f};
    c.checkMark    = {0.58f, 0.42f, 0.92f, 1.00f};
    c.sliderGrab   = {0.48f, 0.35f, 0.82f, 1.00f};
    c.sliderGrabActive = {0.62f, 0.48f, 0.95f, 1.00f};
    c.resizeGrip   = {0.35f, 0.26f, 0.58f, 0.50f};
    c.textSelected = {0.35f, 0.26f, 0.58f, 0.50f};
    c.dragDropTarget = {0.58f, 0.42f, 0.92f, 0.90f};
    c.tabActive    = {0.25f, 0.20f, 0.42f, 1.00f};
    c.tabHovered   = {0.35f, 0.28f, 0.58f, 1.00f};
    c.useCustom = false;
    return c;
}

AccentColorConfig AccentColorConfig::DefaultDS() {
    AccentColorConfig c;
    c.button       = {0.20f, 0.22f, 0.30f, 1.00f};
    c.buttonHover  = {0.26f, 0.32f, 0.45f, 1.00f};
    c.buttonActive = {0.32f, 0.42f, 0.58f, 1.00f};
    c.checkMark    = {0.35f, 0.58f, 0.85f, 1.00f};
    c.sliderGrab   = {0.30f, 0.50f, 0.75f, 1.00f};
    c.sliderGrabActive = {0.40f, 0.62f, 0.90f, 1.00f};
    c.resizeGrip   = {0.28f, 0.38f, 0.55f, 0.50f};
    c.textSelected = {0.25f, 0.38f, 0.58f, 0.50f};
    c.dragDropTarget = {0.35f, 0.58f, 0.85f, 0.90f};
    c.tabActive    = {0.22f, 0.28f, 0.40f, 1.00f};
    c.tabHovered   = {0.28f, 0.36f, 0.52f, 1.00f};
    c.useCustom = false;
    return c;
}

AccentColorConfig AccentColorConfig::DefaultForTheme(EditorTheme theme) {
    switch (theme) {
        case EditorTheme::Light:              return DefaultLight();
        case EditorTheme::HighContrastDark:   return DefaultHighContrastDark();
        case EditorTheme::HighContrastLight:  return DefaultHighContrastLight();
        case EditorTheme::SNES:               return DefaultSNES();
        case EditorTheme::PS2:                return DefaultPS2();
        case EditorTheme::Xbox:               return DefaultXbox();
        case EditorTheme::Dreamcast:          return DefaultDreamcast();
        case EditorTheme::SegaSaturn:         return DefaultSegaSaturn();
        case EditorTheme::GBA:               return DefaultGBA();
        case EditorTheme::DS:                return DefaultDS();
        default:                              return DefaultDark();
    }
}

static json AccentColorToJson(const AccentColor& c) {
    return json::array({c.r, c.g, c.b, c.a});
}

static AccentColor AccentColorFromJson(const json& j) {
    if (j.is_array() && j.size() >= 4) {
        return AccentColor(j[0].get<f32>(), j[1].get<f32>(), j[2].get<f32>(), j[3].get<f32>());
    }
    return AccentColor();
}

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

        // Accent colors
        if (accentColors.useCustom) {
            json ac;
            ac["useCustom"] = true;
            ac["button"] = AccentColorToJson(accentColors.button);
            ac["buttonHover"] = AccentColorToJson(accentColors.buttonHover);
            ac["buttonActive"] = AccentColorToJson(accentColors.buttonActive);
            ac["checkMark"] = AccentColorToJson(accentColors.checkMark);
            ac["sliderGrab"] = AccentColorToJson(accentColors.sliderGrab);
            ac["sliderGrabActive"] = AccentColorToJson(accentColors.sliderGrabActive);
            ac["resizeGrip"] = AccentColorToJson(accentColors.resizeGrip);
            ac["textSelected"] = AccentColorToJson(accentColors.textSelected);
            ac["dragDropTarget"] = AccentColorToJson(accentColors.dragDropTarget);
            ac["tabActive"] = AccentColorToJson(accentColors.tabActive);
            ac["tabHovered"] = AccentColorToJson(accentColors.tabHovered);
            j["accentColors"] = ac;
        }

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

        // Surface Snap
        j["surfaceSnap"] = surfaceSnap;
        j["surfaceAlignNormal"] = surfaceAlignNormal;

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

        // Accent colors
        if (j.contains("accentColors") && j["accentColors"].is_object()) {
            const auto& ac = j["accentColors"];
            if (ac.contains("useCustom")) accentColors.useCustom = ac["useCustom"].get<bool>();
            if (ac.contains("button")) accentColors.button = AccentColorFromJson(ac["button"]);
            if (ac.contains("buttonHover")) accentColors.buttonHover = AccentColorFromJson(ac["buttonHover"]);
            if (ac.contains("buttonActive")) accentColors.buttonActive = AccentColorFromJson(ac["buttonActive"]);
            if (ac.contains("checkMark")) accentColors.checkMark = AccentColorFromJson(ac["checkMark"]);
            if (ac.contains("sliderGrab")) accentColors.sliderGrab = AccentColorFromJson(ac["sliderGrab"]);
            if (ac.contains("sliderGrabActive")) accentColors.sliderGrabActive = AccentColorFromJson(ac["sliderGrabActive"]);
            if (ac.contains("resizeGrip")) accentColors.resizeGrip = AccentColorFromJson(ac["resizeGrip"]);
            if (ac.contains("textSelected")) accentColors.textSelected = AccentColorFromJson(ac["textSelected"]);
            if (ac.contains("dragDropTarget")) accentColors.dragDropTarget = AccentColorFromJson(ac["dragDropTarget"]);
            if (ac.contains("tabActive")) accentColors.tabActive = AccentColorFromJson(ac["tabActive"]);
            if (ac.contains("tabHovered")) accentColors.tabHovered = AccentColorFromJson(ac["tabHovered"]);
        }

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

        // Surface Snap
        if (j.contains("surfaceSnap")) surfaceSnap = j["surfaceSnap"].get<bool>();
        if (j.contains("surfaceAlignNormal")) surfaceAlignNormal = j["surfaceAlignNormal"].get<bool>();

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
