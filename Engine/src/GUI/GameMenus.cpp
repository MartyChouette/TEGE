#include <nlohmann/json.hpp>
#include "Enjin/GUI/GameMenus.h"
#include "Enjin/GUI/Localization.h"
#include "Enjin/Platform/Input.h"
#include "Enjin/Input/TouchActionBridge.h"
#include "Enjin/Renderer/PostProcessing.h"

#include <algorithm>
#include <array>


namespace {

inline ImVec4 TC(const Enjin::Math::Vector3& c, float a = 1.0f) {
    return ImVec4(c.x, c.y, c.z, a);
}

// Lighten/darken a theme colour for the states a theme does not name outright
// (a tab's unselected body, a scrollbar grab). Keeps everything derived from the
// palette rather than reintroducing a second hardcoded one.
inline ImVec4 TCMix(const Enjin::Math::Vector3& a, const Enjin::Math::Vector3& b,
                    float t, float alpha = 1.0f) {
    return ImVec4(a.x + (b.x - a.x) * t,
                  a.y + (b.y - a.y) * t,
                  a.z + (b.z - a.z) * t, alpha);
}

// Every ImGui colour the options/how-to-play panels actually use, derived from
// the game's theme. Returns the number pushed so the caller can pop exactly
// that many -- a miscount corrupts ImGui's colour stack for the whole frame.
int PushMenuTheme(const Enjin::GUI::UITheme& t) {
    using namespace Enjin;
    ImGui::PushStyleColor(ImGuiCol_WindowBg,            TC(t.background, 0.97f));
    ImGui::PushStyleColor(ImGuiCol_ChildBg,             TC(t.surface, 0.55f));
    ImGui::PushStyleColor(ImGuiCol_PopupBg,             TC(t.surface, 0.98f));
    ImGui::PushStyleColor(ImGuiCol_Border,              TC(t.primary, 0.45f));
    ImGui::PushStyleColor(ImGuiCol_Text,                TC(t.textPrimary));
    ImGui::PushStyleColor(ImGuiCol_TextDisabled,        TC(t.textDisabled));
    ImGui::PushStyleColor(ImGuiCol_Separator,           TC(t.primary, 0.35f));
    ImGui::PushStyleColor(ImGuiCol_Button,              TC(t.buttonDefault));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,       TC(t.buttonHovered));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,        TC(t.buttonPressed));
    ImGui::PushStyleColor(ImGuiCol_FrameBg,             TC(t.inputBg));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered,      TC(t.buttonHovered, 0.85f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive,       TC(t.buttonPressed));
    ImGui::PushStyleColor(ImGuiCol_SliderGrab,          TC(t.sliderThumb));
    ImGui::PushStyleColor(ImGuiCol_SliderGrabActive,    TC(t.primary));
    ImGui::PushStyleColor(ImGuiCol_CheckMark,           TC(t.primary));
    ImGui::PushStyleColor(ImGuiCol_Header,              TC(t.primary, 0.35f));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered,       TC(t.primary, 0.50f));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive,        TC(t.primary, 0.65f));
    ImGui::PushStyleColor(ImGuiCol_Tab,                 TCMix(t.background, t.surface, 0.6f));
    ImGui::PushStyleColor(ImGuiCol_TabHovered,          TC(t.primary, 0.55f));
    ImGui::PushStyleColor(ImGuiCol_TabSelected,         TC(t.primary, 0.80f));
    ImGui::PushStyleColor(ImGuiCol_TabDimmed,           TCMix(t.background, t.surface, 0.35f));
    ImGui::PushStyleColor(ImGuiCol_TabDimmedSelected,   TC(t.primary, 0.55f));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarBg,         TC(t.background, 0.60f));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab,       TC(t.sliderTrack));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered,TC(t.primary, 0.70f));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabActive, TC(t.primary));
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram,       TC(t.primary));
    return 29;
}

} // namespace

namespace Enjin::GUI {

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void GameMenuSystem::SetInputMap(InputSystem::InputActionMap* map) {
    m_InputMap = map;
}

void GameMenuSystem::SetEditorSettings(Editor::EditorSettings* settings) {
    m_EditorSettings = settings;
}

GraphicsSettings& GameMenuSystem::GetGraphicsSettings() {
    return m_Graphics;
}

AudioSettings& GameMenuSystem::GetAudioSettings() {
    return m_Audio;
}

void GameMenuSystem::ShowScreen(MenuScreen screen) {
    // Track where Options/HowToPlay should return to
    if (screen == MenuScreen::Options || screen == MenuScreen::HowToPlay) {
        // None too: opened over an authored title canvas, Back has to close
        // the screen and leave that canvas showing, not bring up the built-in
        // title from some earlier visit
        if (m_CurrentScreen == MenuScreen::MainMenu || m_CurrentScreen == MenuScreen::PauseMenu ||
            m_CurrentScreen == MenuScreen::None) {
            m_ReturnScreen = m_CurrentScreen;
        }
    }
    // Pull live settings into the menu on open so Back applies what the user
    // actually sees, not the struct defaults.
    if ((screen == MenuScreen::Options || screen == MenuScreen::Graphics) &&
        m_CurrentScreen != screen && m_SettingsSyncCallback) {
        m_SettingsSyncCallback(m_Graphics, m_Audio);
    }
    // Read the slots when a screen that shows them opens, not every frame:
    // GetAllSlots() hits the backend once per slot and a menu does not need
    // that at frame rate. The main menu needs it too, because Continue has to
    // know whether it has anything to continue.
    if (screen == MenuScreen::LoadGame || screen == MenuScreen::MainMenu) {
        RefreshSlots();
    }

    m_CurrentScreen = screen;
}

std::string GameSettingsToJson(const GraphicsSettings& g, const AudioSettings& a) {
    nlohmann::json j;
    j["resolutionWidth"] = g.resolutionWidth;
    j["resolutionHeight"] = g.resolutionHeight;
    j["fullscreen"] = g.fullscreen;
    j["vsync"] = g.vsync;
    j["hdr"] = g.hdr;
    j["qualityPreset"] = g.qualityPreset;
    j["renderScale"] = g.renderScale;
    j["fieldOfView"] = g.fieldOfView;
    j["bloom"] = g.bloom;
    j["fxaa"] = g.fxaa;
    j["shadows"] = g.shadows;
    j["shadowQuality"] = g.shadowQuality;
    j["masterVolume"] = a.masterVolume;
    j["musicVolume"] = a.musicVolume;
    j["sfxVolume"] = a.sfxVolume;
    j["voiceVolume"] = a.voiceVolume;
    j["masterMute"] = a.masterMute;
    j["musicMute"] = a.musicMute;
    j["sfxMute"] = a.sfxMute;
    j["voiceMute"] = a.voiceMute;
    return j.dump(2);
}

bool GameSettingsFromJson(const std::string& json, GraphicsSettings& g, AudioSettings& a) {
    try {
        const nlohmann::json j = nlohmann::json::parse(json);
        if (!j.is_object()) return false;
        g.resolutionWidth = j.value("resolutionWidth", g.resolutionWidth);
        g.resolutionHeight = j.value("resolutionHeight", g.resolutionHeight);
        g.fullscreen = j.value("fullscreen", g.fullscreen);
        g.vsync = j.value("vsync", g.vsync);
        g.hdr = j.value("hdr", g.hdr);
        g.qualityPreset = std::min(j.value("qualityPreset", g.qualityPreset), 3u);
        g.renderScale = std::clamp(j.value("renderScale", g.renderScale), 0.5f, 1.0f);
        g.fieldOfView = std::clamp(j.value("fieldOfView", g.fieldOfView), 40.0f, 120.0f);
        g.bloom = j.value("bloom", g.bloom);
        g.fxaa = j.value("fxaa", g.fxaa);
        g.shadows = j.value("shadows", g.shadows);
        g.shadowQuality = std::min(j.value("shadowQuality", g.shadowQuality), 3u);
        a.masterVolume = std::clamp(j.value("masterVolume", a.masterVolume), 0.0f, 1.0f);
        a.musicVolume = std::clamp(j.value("musicVolume", a.musicVolume), 0.0f, 1.0f);
        a.sfxVolume = std::clamp(j.value("sfxVolume", a.sfxVolume), 0.0f, 1.0f);
        a.voiceVolume = std::clamp(j.value("voiceVolume", a.voiceVolume), 0.0f, 1.0f);
        a.masterMute = j.value("masterMute", a.masterMute);
        a.musicMute = j.value("musicMute", a.musicMute);
        a.sfxMute = j.value("sfxMute", a.sfxMute);
        a.voiceMute = j.value("voiceMute", a.voiceMute);
        return true;
    } catch (...) {
        return false;
    }
}

void GameMenuSystem::HideAll() {
    m_CurrentScreen = MenuScreen::None;
}

void GameMenuSystem::OpenControls() {
    // This menu steps aside; the runtime's ControlsScreen brings it back to
    // Options when its Back is pressed
    m_CurrentScreen = MenuScreen::None;
    if (m_OpenControls) m_OpenControls();
}

MenuScreen GameMenuSystem::GetCurrentScreen() const {
    return m_CurrentScreen;
}

bool GameMenuSystem::IsMenuOpen() const {
    return m_CurrentScreen != MenuScreen::None;
}

void GameMenuSystem::SetCallback(MenuCallback cb) {
    m_Callback = std::move(cb);
}

void GameMenuSystem::SetGameTitle(const std::string& title) {
    m_GameTitle = title;
}

// Activate the options-preview split for this frame: left of the divider
// renders without the given effect. Cleared automatically next frame unless a
// hovered control requests it again (see Render).
void GameMenuSystem::RequestPreview(u32 effect) {
// The full PostProcessing class exists only on the Vulkan backend; the web
// renderer has its own post chain and no preview split yet.
#if !ENJIN_RENDERER_WEBGPU
    if (!m_PostProcessing) return;
    auto& s = m_PostProcessing->GetSettings();
    s.previewSplitEffect = effect;
    s.previewSplitDivider = 0.5f;
    m_PreviewRequested = true;
#else
    (void)effect;
#endif
}

void GameMenuSystem::Render(f32 screenW, f32 screenH) {
    // Preview split is a per-frame request: if no control asked for it since the
    // last frame (menu closed, tab changed, cursor moved off), switch it off.
#if !ENJIN_RENDERER_WEBGPU
    if (m_PostProcessing && !m_PreviewRequested)
        m_PostProcessing->GetSettings().previewSplitEffect = 0;
#endif
    m_PreviewRequested = false;

    switch (m_CurrentScreen) {
        case MenuScreen::MainMenu:   RenderMainMenu(screenW, screenH);  break;
        case MenuScreen::PauseMenu:  RenderPauseMenu(screenW, screenH); break;
        case MenuScreen::Options:    RenderOptions(screenW, screenH);   break;
        case MenuScreen::Graphics:   RenderGraphics(screenW, screenH);  break;
        case MenuScreen::Audio:      RenderAudio(screenW, screenH);     break;
        case MenuScreen::Controls:   OpenControls();                    break;
        case MenuScreen::HowToPlay:  RenderHowToPlay(screenW, screenH); break;
        case MenuScreen::GameOver:   RenderGameOver(screenW, screenH);  break;
        case MenuScreen::LoadGame:   RenderLoadGame(screenW, screenH);  break;
        case MenuScreen::None:
        default:
            break;
    }
}

// ---------------------------------------------------------------------------
// Main Menu
// ---------------------------------------------------------------------------

void GameMenuSystem::RenderMainMenu(f32 w, f32 h) {
    // Solid full-screen opaque background cover card over the game scene
    ImDrawList* draw = TargetDrawList();
    draw->AddRectFilledMultiColor(ImVec2(0, 0), ImVec2(w, h),
        IM_COL32(12, 14, 24, 255),   // top-left
        IM_COL32(18, 22, 34, 255),   // top-right
        IM_COL32(10, 12, 20, 255),   // bottom-right
        IM_COL32(8, 10, 18, 255));   // bottom-left

    const f32 buttonW = 280.0f;
    const f32 cardW = 380.0f;
    const f32 cardH = 440.0f;
    const f32 cardX = (w - cardW) * 0.5f;
    const f32 cardY = (h - cardH) * 0.5f;

    // Main Menu Card container frame
    draw->AddRectFilled(ImVec2(cardX, cardY), ImVec2(cardX + cardW, cardY + cardH),
        IM_COL32(20, 24, 38, 245), 14.0f);
    draw->AddRect(ImVec2(cardX, cardY), ImVec2(cardX + cardW, cardY + cardH),
        IM_COL32(65, 80, 120, 200), 14.0f, 0, 1.5f);

    ImGui::SetNextWindowPos(ImVec2(cardX, cardY + 24.0f));
    ImGui::SetNextWindowSize(ImVec2(cardW, cardH - 24.0f));
    ImGui::Begin("##MainMenu", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoSavedSettings);

    // Title
    ImGui::PushFont(nullptr); // use default; engine may push a large font externally
    {
        ImVec2 titleSize = ImGui::CalcTextSize(m_GameTitle.c_str());
        ImGui::SetCursorPosX((cardW - titleSize.x) * 0.5f);
        ImGui::TextColored(TC(Theme().textPrimary), "%s", m_GameTitle.c_str());
    }
    ImGui::PopFont();

    ImGui::Dummy(ImVec2(0, 30));

    // Center buttons
    auto CenterButton = [&](const char* label, const char* action) {
        ImGui::SetCursorPosX((cardW - buttonW) * 0.5f);
        // The callback id is already a stable name for this button, so it
        // doubles as the localization key -- no second naming scheme to
        // agree on. The English literal is the fallback, so an
        // untranslated project renders exactly as it did.
        const std::string text =
            LocalizationManager::Get().GetString(std::string("menu.") + action, label);
        if (RenderMenuButton(text.c_str(), buttonW)) {
            if (m_Callback) m_Callback(action);
        }
        ImGui::Dummy(ImVec2(0, 6));
    };

    // Continue and Load only mean something if there IS a save.
    //
    // Continue used to call ResumeGame(), which sets m_GameStarted = true and
    // consults nothing. On a cold boot it was New Game wearing a different
    // label -- the button a returning player reaches for first, doing the one
    // thing that loses their progress. It now resumes the most recent readable
    // slot, and when there is none it is DISABLED rather than lying.
    auto CenterButtonEnabled = [&](const char* label, const char* action, bool enabled) {
        ImGui::SetCursorPosX((cardW - buttonW) * 0.5f);
        const std::string text =
            LocalizationManager::Get().GetString(std::string("menu.") + action, label);
        if (RenderMenuButton(text.c_str(), buttonW, false, enabled)) {
            if (m_Callback) m_Callback(action);
        }
        ImGui::Dummy(ImVec2(0, 6));
    };

    const bool haveSaves = !m_CachedSlots.empty();

    CenterButton("New Game",  "new_game");
    CenterButtonEnabled("Continue", "continue", m_ResumeSlot >= 0);
    CenterButtonEnabled("Load Game", "load_game", haveSaves);
    CenterButton("Options",   "options");
    CenterButton("How to Play", "how_to_play");
    if (m_QuitAvailable) CenterButton("Quit", "quit");

    // Handle options / how-to-play navigation internally as well
    // The callback can decide whether to navigate or the caller can call ShowScreen
    ImGui::End();
}

// ---------------------------------------------------------------------------
// Load Game (ENG-001, S7)
// ---------------------------------------------------------------------------

void GameMenuSystem::RefreshSlots() {
    m_CachedSlots.clear();
    m_ResumeSlot = -1;
    if (!m_SaveSlotProvider) return;

    m_CachedSlots = m_SaveSlotProvider();

    // Drop the slots that hold nothing. A corrupt slot is KEPT: it is a real
    // save that could not be read, and hiding it is how a player ends up
    // overwriting something recoverable without being told it existed.
    m_CachedSlots.erase(
        std::remove_if(m_CachedSlots.begin(), m_CachedSlots.end(),
                       [](const Gameplay::SaveSlotInfo& s) { return s.isEmpty && !s.isCorrupt; }),
        m_CachedSlots.end());

    // Most recent readable slot. Timestamps are written by GetTimestamp() in a
    // sortable form, so string order is time order.
    const Gameplay::SaveSlotInfo* best = nullptr;
    for (const auto& s : m_CachedSlots) {
        if (s.isCorrupt || s.isEmpty) continue;
        if (!best || s.timestamp > best->timestamp) best = &s;
    }
    if (best) m_ResumeSlot = static_cast<i32>(best->slotIndex);
}

void GameMenuSystem::RenderLoadGame(f32 w, f32 h) {
    const f32 cardW = 520.0f;
    const f32 buttonW = cardW - 60.0f;

    ImGui::SetNextWindowPos(ImVec2(w * 0.5f, h * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(cardW, 0), ImGuiCond_Always);
    ImGui::Begin("##loadgame", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoSavedSettings);

    {
        const std::string title = LocalizationManager::Get().GetString("menu.load_game", "Load Game");
        ImVec2 sz = ImGui::CalcTextSize(title.c_str());
        ImGui::SetCursorPosX((cardW - sz.x) * 0.5f);
        ImGui::TextColored(TC(Theme().textPrimary), "%s", title.c_str());
    }
    ImGui::Dummy(ImVec2(0, 18));

    if (m_CachedSlots.empty()) {
        // Say WHY it is empty. An empty panel reads as a screen that failed to
        // draw, and "no saves yet" is a different thing from "something broke".
        const std::string none = LocalizationManager::Get().GetString(
            "menu.no_saves", "No saved games yet.");
        ImVec2 sz = ImGui::CalcTextSize(none.c_str());
        ImGui::SetCursorPosX((cardW - sz.x) * 0.5f);
        ImGui::TextColored(TC(Theme().textSecondary), "%s", none.c_str());
        ImGui::Dummy(ImVec2(0, 18));
    }

    for (const auto& slot : m_CachedSlots) {
        ImGui::PushID(static_cast<int>(slot.slotIndex));

        // One line the player can actually read: what it is called, where they
        // were, when, and how long they have played.
        const i32 mins = static_cast<i32>(slot.playTime / 60.0f);
        char label[256];
        if (slot.isCorrupt) {
            std::snprintf(label, sizeof(label), "%s  -  unreadable",
                          slot.displayName.c_str());
        } else {
            std::snprintf(label, sizeof(label), "%s  -  %s  -  %s  -  %dh %02dm",
                          slot.displayName.c_str(),
                          slot.sceneName.empty() ? "?" : slot.sceneName.c_str(),
                          slot.timestamp.c_str(), mins / 60, mins % 60);
        }

        ImGui::SetCursorPosX((cardW - buttonW) * 0.5f);
        // A corrupt slot is shown and NOT clickable. Offering it would fail,
        // and hiding it would let the player overwrite it without knowing.
        if (RenderMenuButton(label, buttonW, false, !slot.isCorrupt)) {
            if (m_Callback) m_Callback("load_slot:" + std::to_string(slot.slotIndex));
        }
        ImGui::Dummy(ImVec2(0, 6));
        ImGui::PopID();
    }

    ImGui::Dummy(ImVec2(0, 12));
    ImGui::SetCursorPosX((cardW - buttonW) * 0.5f);
    const std::string back = LocalizationManager::Get().GetString("menu.back", "Back");
    if (RenderMenuButton(back.c_str(), buttonW)) {
        ShowScreen(MenuScreen::MainMenu);
    }

    ImGui::End();
}

// ---------------------------------------------------------------------------
// Pause Menu
// ---------------------------------------------------------------------------

void GameMenuSystem::RenderPauseMenu(f32 w, f32 h) {
    // Dim overlay behind pause menu (background draw list = behind HUD)
    ImDrawList* draw = TargetDrawList();
    draw->AddRectFilledMultiColor(ImVec2(0, 0), ImVec2(w, h),
        IM_COL32(8, 10, 18, 100),   // top-left (lighter)
        IM_COL32(8, 10, 18, 100),   // top-right
        IM_COL32(12, 12, 20, 180),  // bottom-right (darker)
        IM_COL32(12, 12, 20, 180)); // bottom-left

    const f32 buttonW = 260.0f;
    const f32 panelH = 380.0f;  // Tall enough for 5 buttons + title + spacing

    ImGui::SetNextWindowPos(ImVec2((w - buttonW - 60.0f) * 0.5f, (h - panelH) * 0.5f));
    ImGui::SetNextWindowSize(ImVec2(buttonW + 60.0f, panelH));
    ImGui::Begin("##PauseMenu", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoSavedSettings);

    // Title
    {
        const char* title = "PAUSED";
        ImVec2 titleSize = ImGui::CalcTextSize(title);
        ImGui::SetCursorPosX((buttonW + 60.0f - titleSize.x) * 0.5f);
        ImGui::TextColored(TC(Theme().textPrimary, 0.95f), "%s", title);
    }

    ImGui::Dummy(ImVec2(0, 20));

    auto CenterButton = [&](const char* label, const char* action) {
        ImGui::SetCursorPosX((buttonW + 60.0f - buttonW) * 0.5f);
        // The callback id is already a stable name for this button, so it
        // doubles as the localization key -- no second naming scheme to
        // agree on. The English literal is the fallback, so an
        // untranslated project renders exactly as it did.
        const std::string text =
            LocalizationManager::Get().GetString(std::string("menu.") + action, label);
        if (RenderMenuButton(text.c_str(), buttonW)) {
            if (m_Callback) m_Callback(action);
        }
        ImGui::Dummy(ImVec2(0, 6));
    };

    CenterButton("Resume",        "resume");
    CenterButton("Restart",       "restart");
    CenterButton("Options",       "options");
    CenterButton("How to Play",   "how_to_play");
    CenterButton("Quit to Menu",  "quit_to_menu");

    ImGui::End();
}

// ---------------------------------------------------------------------------
// Options (tab-based hub)
// ---------------------------------------------------------------------------

void GameMenuSystem::RenderOptions(f32 w, f32 h) {
    ImDrawList* draw = TargetDrawList();
    if (m_ReturnScreen == MenuScreen::MainMenu || m_CurrentScreen == MenuScreen::MainMenu) {
        draw->AddRectFilledMultiColor(ImVec2(0, 0), ImVec2(w, h),
            IM_COL32(12, 14, 24, 255),
            IM_COL32(18, 22, 34, 255),
            IM_COL32(10, 12, 20, 255),
            IM_COL32(8, 10, 18, 255));
    } else {
        draw->AddRectFilledMultiColor(ImVec2(0, 0), ImVec2(w, h),
            IM_COL32(8, 10, 18, 100),   // top-left (lighter)
            IM_COL32(8, 10, 18, 100),   // top-right
            IM_COL32(12, 12, 20, 180),  // bottom-right (darker)
            IM_COL32(12, 12, 20, 180)); // bottom-left
    }

    const f32 panelW = 640.0f;
    const f32 panelH = 520.0f;

    // Default size + centered position; the panel is user-resizable (drag any
    // edge) so scaled-up fonts and long binding lists get room to breathe.
    ImGui::SetNextWindowPos(ImVec2((w - panelW) * 0.5f, (h - panelH) * 0.5f), ImGuiCond_Appearing);
    ImGui::SetNextWindowSize(ImVec2(panelW, panelH), ImGuiCond_Appearing);
    ImGui::SetNextWindowSizeConstraints(ImVec2(460.0f, 360.0f), ImVec2(w, h));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 12.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
    const int themeColors = PushMenuTheme(Theme());
    ImGui::Begin("##Options", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoSavedSettings);

    ImGui::TextColored(TC(Theme().textPrimary), "Options");
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0, 8));

    if (ImGui::BeginTabBar("##OptionsTabs")) {
        if (ImGui::BeginTabItem("Graphics")) {
            RenderGraphics(w, h);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Audio")) {
            RenderAudio(w, h);
            ImGui::EndTabItem();
        }
        if (m_Accessibility && ImGui::BeginTabItem("Accessibility")) {
            RenderAccessibility(w, h);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    ImGui::Dummy(ImVec2(0, 10));
    // Controls is the one UICanvas screen every runtime shares (IN-16), not a
    // tab here: this page's ImGui copy rebound keys only and disagreed with
    // the web's about which actions exist
    if (m_OpenControls && ImGui::Button("Controls", ImVec2(120, 32))) {
        if (m_SettingsCallback) m_SettingsCallback(m_Graphics, m_Audio);
        OpenControls();
    }
    if (m_OpenControls) ImGui::SameLine();
    if (ImGui::Button("Back", ImVec2(100, 32))) {
        // Notify the host application to apply changed settings
        if (m_SettingsCallback) m_SettingsCallback(m_Graphics, m_Audio);
        ShowScreen(m_ReturnScreen);
    }

    ImGui::End();
    ImGui::PopStyleColor(themeColors);
    ImGui::PopStyleVar(2);
}

// ---------------------------------------------------------------------------
// Graphics Settings
// ---------------------------------------------------------------------------

void GameMenuSystem::RenderGraphics(f32 w, f32 h) {
    (void)w;
    (void)h;

    ImGui::Dummy(ImVec2(0, 4));
    ImGui::PushItemWidth(300.0f);  // Fixed widget width — labels get remaining space

    // Quality tier first: it is the one setting that decides whether the machine
    // can run the game at all, and it caps render COST without touching how the
    // game looks. Hidden entirely unless the project opted in and allows it.
    if (m_RenderQuality && m_ActiveQualityTier &&
        m_RenderQuality->enabled && m_RenderQuality->playerCanChange) {
        static const char* kTierNames[] = { "Low", "Medium", "High", "Ultra", "Custom" };
        i32 tier = static_cast<i32>(*m_ActiveQualityTier);
        if (ImGui::Combo("Quality", &tier, kTierNames, 5)) {
            *m_ActiveQualityTier = static_cast<Renderer::QualityTier>(tier);
            if (m_QualityChanged) m_QualityChanged();
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Lower tiers spend less on lighting and reflections.\n"
                              "The look of the game does not change.");
        }
        ImGui::Separator();
    }

    // Resolution
    struct Resolution { u32 w; u32 h; const char* label; };
    static const std::array<Resolution, 6> resolutions = {{
        { 1280,  720, "1280 x 720  (720p)"  },
        { 1600,  900, "1600 x 900"           },
        { 1920, 1080, "1920 x 1080 (1080p)"  },
        { 2560, 1440, "2560 x 1440 (1440p)"  },
        { 3440, 1440, "3440 x 1440 (UW 1440p)" },
        { 3840, 2160, "3840 x 2160 (4K)"     },
    }};

    // Find current resolution index
    i32 currentRes = 2; // default to 1080p
    for (i32 i = 0; i < static_cast<i32>(resolutions.size()); ++i) {
        if (resolutions[i].w == m_Graphics.resolutionWidth &&
            resolutions[i].h == m_Graphics.resolutionHeight) {
            currentRes = i;
            break;
        }
    }

    if (ImGui::Combo("Resolution", &currentRes,
        [](void* data, int idx) -> const char* {
            auto* res = static_cast<const std::array<Resolution, 6>*>(data);
            return (*res)[idx].label;
        },
        (void*)&resolutions, static_cast<i32>(resolutions.size()))) {
        m_Graphics.resolutionWidth = resolutions[currentRes].w;
        m_Graphics.resolutionHeight = resolutions[currentRes].h;
    }

#if !ENJIN_PLATFORM_WEB
    ImGui::Checkbox("Fullscreen", &m_Graphics.fullscreen);
    ImGui::Checkbox("VSync", &m_Graphics.vsync);
    // Only on a display that has an HDR format; an SDR screen never sees it
    if (m_HDRAvailable) {
        ImGui::Checkbox("HDR", &m_Graphics.hdr);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("High dynamic range output for HDR displays.");
        }
    }
#else
    // In a browser there is no vsync to choose: presentation is the browser's,
    // on its own cadence through requestAnimationFrame, and nothing here can
    // change it. Offering the toggle offers a control that does nothing.
    //
    // Fullscreen is still worth offering on a desktop browser, but not on a
    // phone -- there the browser grants it on its own terms and the game is
    // already filling the screen.
    if (!Input::IsCoarsePointerDevice()) {
        ImGui::Checkbox("Fullscreen", &m_Graphics.fullscreen);
    }
#endif

    // Field of View
    ImGui::SliderFloat("Field of View", &m_Graphics.fieldOfView, 40.0f, 120.0f, "%.0f");

    // Quality preset. Not beside the project's quality tier: that is the
    // Quality control above, and two quality menus on one tab disagreed about
    // what "High" meant (IN-18).
    const bool tierShown = m_RenderQuality && m_ActiveQualityTier &&
                           m_RenderQuality->enabled && m_RenderQuality->playerCanChange;
    static const char* qualityLabels[] = { "Low", "Medium", "High", "Ultra" };
    i32 quality = static_cast<i32>(m_Graphics.qualityPreset);
    if (quality < 0) quality = 0;
    if (quality > 3) quality = 3;
    if (!tierShown && ImGui::Combo("Quality Preset", &quality, qualityLabels, 4)) {
        m_Graphics.qualityPreset = static_cast<u32>(quality);
        // Apply preset defaults
        switch (m_Graphics.qualityPreset) {
            case 0: // Low
                m_Graphics.renderScale = 0.5f;
                m_Graphics.bloom = false;
                m_Graphics.fxaa = false;
                m_Graphics.shadows = false;
                m_Graphics.shadowQuality = 0;
                break;
            case 1: // Medium
                m_Graphics.renderScale = 0.75f;
                m_Graphics.bloom = false;
                m_Graphics.fxaa = true;
                m_Graphics.shadows = true;
                m_Graphics.shadowQuality = 1;
                break;
            case 2: // High
                m_Graphics.renderScale = 1.0f;
                m_Graphics.bloom = true;
                m_Graphics.fxaa = true;
                m_Graphics.shadows = true;
                m_Graphics.shadowQuality = 2;
                break;
            case 3: // Ultra
                m_Graphics.renderScale = 1.0f;
                m_Graphics.bloom = true;
                m_Graphics.fxaa = true;
                m_Graphics.shadows = true;
                m_Graphics.shadowQuality = 3;
                break;
        }
    }

    // 0.5 - 1.0 is the range the engine can actually render: below that no
    // path exists, and the old 2.0 upper bound promised supersampling that was
    // never implemented. 1.0 means native, which is why it is the right end.
    ImGui::SliderFloat("Render Scale", &m_Graphics.renderScale, 0.5f, 1.0f, "%.2f");
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Renders the scene below screen resolution and upscales it.\n"
                          "Lower is faster. 1.00 renders at native resolution.");
    }

    ImGui::Separator();
    ImGui::Text("Post-Processing");
    bool bloomToggled = ImGui::Checkbox("Bloom", &m_Graphics.bloom);
#if !ENJIN_RENDERER_WEBGPU
    if (bloomToggled && m_PostProcessing) {
        // Live-apply so the preview's right side tracks the checkbox immediately
        // (Back still commits everything else as before).
        m_PostProcessing->GetSettings().bloomEnabled = m_Graphics.bloom ? 1u : 0u;
    }
#else
    (void)bloomToggled;   // web applies bloom via the normal Back/apply path
#endif
    if (ImGui::IsItemHovered() || ImGui::IsItemActive()) RequestPreview(1u);
    ImGui::Checkbox("FXAA", &m_Graphics.fxaa);

    ImGui::Separator();
    ImGui::Text("Shadows");
    ImGui::Checkbox("Enable Shadows", &m_Graphics.shadows);

    if (m_Graphics.shadows) {
        static const char* shadowLabels[] = { "Low", "Medium", "High", "Ultra" };
        i32 sq = static_cast<i32>(m_Graphics.shadowQuality);
        if (sq < 0) sq = 0;
        if (sq > 3) sq = 3;
        if (ImGui::Combo("Shadow Quality", &sq, shadowLabels, 4)) {
            m_Graphics.shadowQuality = static_cast<u32>(sq);
        }
    }

    ImGui::PopItemWidth();
}

// ---------------------------------------------------------------------------
// Accessibility Settings — every exported game gets this tab for free.
// Edits the host's live RuntimeAccessibilitySettings; the changed callback
// lets the host re-push boot-time-only consumers and persist.
// ---------------------------------------------------------------------------

void GameMenuSystem::RenderAccessibility(f32 w, f32 h) {
    (void)w;
    (void)h;
    if (!m_Accessibility) return;
    auto& a = *m_Accessibility;
    bool changed = false;

    ImGui::Dummy(ImVec2(0, 4));
    ImGui::BeginChild("##A11yScroll", ImVec2(0, 330), false);
    ImGui::PushItemWidth(280.0f);

    // --- Vision ---
    ImGui::Text("Vision");
    ImGui::Separator();
    static const char* cbModes[] = {
        "Off",
        "Protanopia (no red)", "Deuteranopia (no green)", "Tritanopia (no blue)",
        "Protanomaly (weak red)", "Deuteranomaly (weak green)", "Tritanomaly (weak blue)",
        "Achromatopsia (no color)", "Achromatomaly (weak color)"
    };
    i32 cbMode = static_cast<i32>(a.colorblindMode);
    if (cbMode < 0 || cbMode > 8) cbMode = 0;
    if (ImGui::Combo("Colorblind Mode", &cbMode, cbModes, 9)) {
        a.colorblindMode = static_cast<Accessibility::ColorblindMode>(cbMode);
        changed = true;
    }
    if (ImGui::IsItemHovered() || ImGui::IsItemActive()) RequestPreview(5u);
    if (a.colorblindMode != Accessibility::ColorblindMode::Off) {
        changed |= ImGui::SliderFloat("Correction Strength", &a.colorblindStrength, 0.0f, 1.0f, "%.2f");
        if (ImGui::IsItemHovered() || ImGui::IsItemActive()) RequestPreview(5u);
    }
    changed |= ImGui::SliderFloat("Brightness", &a.screenBrightness, -0.5f, 0.5f, "%.2f");
    if (ImGui::IsItemHovered() || ImGui::IsItemActive()) RequestPreview(6u);
    changed |= ImGui::SliderFloat("Contrast", &a.screenContrast, 0.5f, 2.0f, "%.2f");
    if (ImGui::IsItemHovered() || ImGui::IsItemActive()) RequestPreview(6u);
    changed |= ImGui::SliderFloat("UI Font Scale", &a.fontScale, 0.5f, 3.0f, "%.2f");

    // --- Text & Reading ---
    ImGui::Dummy(ImVec2(0, 6));
    ImGui::Text("Text & Reading");
    ImGui::Separator();
    changed |= ImGui::Checkbox("Dyslexia-Friendly Font", &a.dyslexiaFriendly);
    // Text spacing: persisted and applied for years, but had no UI until the
    // 2026-08-28 accessibility audit. Helps dyslexic and low-vision readers.
    changed |= ImGui::SliderFloat("Letter Spacing", &a.letterSpacing, 0.0f, 8.0f, "%.1f px");
    changed |= ImGui::SliderFloat("Word Spacing", &a.wordSpacing, 0.0f, 16.0f, "%.1f px");
    changed |= ImGui::SliderFloat("Line Spacing", &a.lineSpacing, 1.0f, 3.0f, "%.2fx");
    changed |= ImGui::Checkbox("Subtitles", &a.subtitlesEnabled);
    if (a.subtitlesEnabled) {
        ImGui::Indent(16.0f);
        changed |= ImGui::Checkbox("Speaker Names", &a.subtitleSpeakerNames);
        changed |= ImGui::Checkbox("Closed Captions (sound effects)", &a.closedCaptionsEnabled);
        changed |= ImGui::Checkbox("Direction Indicators", &a.subtitleDirectionIndicators);
        changed |= ImGui::SliderFloat("Subtitle Size", &a.subtitleFontSize, 16.0f, 48.0f, "%.0f");
        changed |= ImGui::SliderFloat("Background Opacity", &a.subtitleBgOpacity, 0.0f, 1.0f, "%.2f");
        ImGui::Unindent(16.0f);
    }

    // --- Motion ---
    ImGui::Dummy(ImVec2(0, 6));
    ImGui::Text("Motion");
    ImGui::Separator();
    changed |= ImGui::Checkbox("Reduced Motion", &a.reducedMotion);
    changed |= ImGui::Checkbox("Disable Screen Shake", &a.disableScreenShake);
    changed |= ImGui::Checkbox("Disable FOV Effects", &a.disableFOVEffects);
    changed |= ImGui::Checkbox("Disable Flashing Lights", &a.disableFlashingLights);

    // --- Motor ---
    ImGui::Dummy(ImVec2(0, 6));
    ImGui::Text("Motor");
    ImGui::Separator();
    changed |= ImGui::Checkbox("Dwell Click (hover to click)", &a.dwellClickEnabled);
    if (a.dwellClickEnabled) {
        ImGui::Indent(16.0f);
        changed |= ImGui::SliderFloat("Dwell Time", &a.dwellClickTime, 0.3f, 3.0f, "%.1f s");
        ImGui::Unindent(16.0f);
    }
    changed |= ImGui::Checkbox("Switch Access (one-button scanning)", &a.switchAccessEnabled);
    if (a.switchAccessEnabled) {
        ImGui::Indent(16.0f);
        changed |= ImGui::SliderFloat("Scan Speed", &a.switchScanSpeed, 0.5f, 5.0f, "%.1f s");
        ImGui::Unindent(16.0f);
    }
    // Gaze / head pointing. Works with any device that moves the pointer, which
    // is how assistive head pointers and eye-gaze systems present themselves.
    changed |= ImGui::Checkbox("Gaze / Head Pointing (dwell to select)", &a.eyeTrackingEnabled);
    if (a.eyeTrackingEnabled) {
        ImGui::Indent(16.0f);
        changed |= ImGui::SliderFloat("Gaze Dwell Time", &a.eyeDwellTime, 0.3f, 3.0f, "%.1f s");
        changed |= ImGui::SliderFloat("Gaze Smoothing", &a.eyeSmoothing, 0.0f, 0.9f, "%.2f");
        changed |= ImGui::SliderFloat("Gaze Dead Zone", &a.eyeDeadZone, 0.0f, 40.0f, "%.0f px");
        changed |= ImGui::Checkbox("Show Gaze Indicator", &a.eyeShowGazeIndicator);
        ImGui::Unindent(16.0f);
    }
    changed |= ImGui::Checkbox("Sticky Slider Drag", &a.stickyDragEnabled);
    {
        // Auto follows the last device used: a touch shows the controls, a key
        // or a pad hides them (IN-29)
        static const char* const kTouchModes[] = { "Auto", "Always", "Never" };
        int mode = static_cast<int>(a.touchMode <= 2 ? a.touchMode : 0);
        if (ImGui::Combo("Touch Controls", &mode, kTouchModes, 3)) {
            a.touchMode = static_cast<u32>(mode);
            Input::SetTouchMode(static_cast<Input::TouchMode>(mode));
            changed = true;
        }
        // The layout the game chose, or the player's own (IN-26)
        static const char* const kHands[] = { "Game Default", "Right-Handed", "Left-Handed" };
        static const char* const kSizes[] = { "Game Default", "Small", "Normal", "Large", "Huge" };
        int hand = static_cast<int>(a.touchHand <= 2 ? a.touchHand : 0);
        int size = static_cast<int>(a.touchButtonSize <= 4 ? a.touchButtonSize : 0);
        bool layout = ImGui::Combo("Touch Layout", &hand, kHands, 3);
        layout |= ImGui::Combo("Touch Button Size", &size, kSizes, 5);
        if (layout) {
            a.touchHand = static_cast<u32>(hand);
            a.touchButtonSize = static_cast<u32>(size);
            InputSystem::SetTouchPlayerLayout(a.touchHand, a.touchButtonSize);
            changed = true;
        }
    }
    if (m_InputMap) {
        ImGui::Dummy(ImVec2(0, 4));
        ImGui::TextUnformatted("Control Presets");
        // A preset is a layer: the lit button is the one in use, and pressing
        // it again takes it off
        auto presetButton = [&](const char* label, InputSystem::BindingPreset p) {
            const bool on = m_InputMap->GetPreset() == p;
            if (on) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
            if (ImGui::Button(label, ImVec2(130, 0))) { m_InputMap->TogglePreset(p); changed = true; }
            if (on) ImGui::PopStyleColor();
        };
        presetButton("Left Hand Only", InputSystem::BindingPreset::LeftHand);
        ImGui::SameLine();
        presetButton("Right Hand Only", InputSystem::BindingPreset::RightHand);
        ImGui::SameLine();
        presetButton("Gamepad Only", InputSystem::BindingPreset::GamepadOnly);
        if (ImGui::Button("Reset Controls to Default", ImVec2(200, 0))) { m_InputMap->ResetToDefaults(); changed = true; }
    }

    // --- Audio & Communication ---
    ImGui::Dummy(ImVec2(0, 6));
    ImGui::Text("Audio & Communication");
    ImGui::Separator();
    changed |= ImGui::Checkbox("Screen Reader / Announcements", &a.screenReaderEnabled);
    changed |= ImGui::Checkbox("Visual Sound Indicators", &a.audioIndicatorsEnabled);

    ImGui::PopItemWidth();
    ImGui::EndChild();

    if (changed && m_AccessibilityChanged) m_AccessibilityChanged();
}

// ---------------------------------------------------------------------------
// Audio Settings
// ---------------------------------------------------------------------------

void GameMenuSystem::RenderAudio(f32 w, f32 h) {
    (void)w;
    (void)h;

    ImGui::Dummy(ImVec2(0, 4));

    // Master
    ImGui::Text("Master");
    ImGui::SameLine(200);
    ImGui::Checkbox("##MasterMute", &m_Audio.masterMute);
    ImGui::SameLine();
    ImGui::TextUnformatted("Mute");
    if (m_Audio.masterMute) {
        ImGui::BeginDisabled();
    }
    ImGui::SliderFloat("Master Volume", &m_Audio.masterVolume, 0.0f, 1.0f, "%.2f");
    if (m_Audio.masterMute) {
        ImGui::EndDisabled();
    }

    ImGui::Separator();

    // Music
    ImGui::Text("Music");
    ImGui::SameLine(200);
    ImGui::Checkbox("##MusicMute", &m_Audio.musicMute);
    ImGui::SameLine();
    ImGui::TextUnformatted("Mute");
    {
        bool disabled = m_Audio.masterMute || m_Audio.musicMute;
        if (disabled) ImGui::BeginDisabled();
        ImGui::SliderFloat("Music Volume", &m_Audio.musicVolume, 0.0f, 1.0f, "%.2f");
        if (disabled) ImGui::EndDisabled();
    }

    // SFX
    ImGui::Text("SFX");
    ImGui::SameLine(200);
    ImGui::Checkbox("##SfxMute", &m_Audio.sfxMute);
    ImGui::SameLine();
    ImGui::TextUnformatted("Mute");
    {
        bool disabled = m_Audio.masterMute || m_Audio.sfxMute;
        if (disabled) ImGui::BeginDisabled();
        ImGui::SliderFloat("SFX Volume", &m_Audio.sfxVolume, 0.0f, 1.0f, "%.2f");
        if (disabled) ImGui::EndDisabled();
    }

    // Voice
    ImGui::Text("Voice");
    ImGui::SameLine(200);
    ImGui::Checkbox("##VoiceMute", &m_Audio.voiceMute);
    ImGui::SameLine();
    ImGui::TextUnformatted("Mute");
    {
        bool disabled = m_Audio.masterMute || m_Audio.voiceMute;
        if (disabled) ImGui::BeginDisabled();
        ImGui::SliderFloat("Voice Volume", &m_Audio.voiceVolume, 0.0f, 1.0f, "%.2f");
        if (disabled) ImGui::EndDisabled();
    }
}

// ---------------------------------------------------------------------------
// How to Play
// ---------------------------------------------------------------------------

void GameMenuSystem::RenderHowToPlay(f32 w, f32 h) {
    ImDrawList* draw = TargetDrawList();
    if (m_ReturnScreen == MenuScreen::MainMenu || m_CurrentScreen == MenuScreen::MainMenu) {
        draw->AddRectFilledMultiColor(ImVec2(0, 0), ImVec2(w, h),
            IM_COL32(12, 14, 24, 255),
            IM_COL32(18, 22, 34, 255),
            IM_COL32(10, 12, 20, 255),
            IM_COL32(8, 10, 18, 255));
    } else {
        draw->AddRectFilledMultiColor(ImVec2(0, 0), ImVec2(w, h),
            IM_COL32(8, 10, 18, 100),   // top-left (lighter)
            IM_COL32(8, 10, 18, 100),   // top-right
            IM_COL32(12, 12, 20, 180),  // bottom-right (darker)
            IM_COL32(12, 12, 20, 180)); // bottom-left
    }

    const f32 panelW = 520.0f;
    const f32 panelH = 460.0f;

    ImGui::SetNextWindowPos(ImVec2((w - panelW) * 0.5f, (h - panelH) * 0.5f));
    ImGui::SetNextWindowSize(ImVec2(panelW, panelH));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 12.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
    const int themeColors = PushMenuTheme(Theme());
    ImGui::Begin("##HowToPlay", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);

    ImGui::TextColored(TC(Theme().textPrimary), "How to Play");
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0, 6));

    if (!m_InputMap) {
        ImGui::TextColored(TC(Theme().error), "No InputActionMap assigned.");
    } else {
        if (ImGui::BeginChild("##HowToPlayList", ImVec2(0, -44), true)) {
            // Group actions by category
            // Ordinals match InputSystem::ActionCategory.
            static const i32 categoryCount = static_cast<i32>(InputSystem::ActionCategory::Count);
            // The actions this game reads (IN-38): a list of every built-in
            // action told players about Dash and Block in games with neither.
            // Before the game has read anything (How to Play from the title
            // screen) there is nothing to go on, and every action is listed.
            const bool byUse = m_InputMap->AnyGameplayActionUsed();
            auto shown = [&](i32 i) {
                if (!m_InputMap->IsActionListed(i)) return false;
                if (!byUse || m_InputMap->GetActionCategory(i) == static_cast<i32>(InputSystem::ActionCategory::UI)) return true;
                return m_InputMap->IsActionUsed(i);
            };

            for (i32 cat = 0; cat < categoryCount; ++cat) {
                i32 actionsInCategory = 0;

                // First pass: count actions in this category
                i32 actionCount = m_InputMap->GetActionCount();
                for (i32 i = 0; i < actionCount; ++i) {
                    if (m_InputMap->GetActionCategory(i) == cat && shown(i)) {
                        ++actionsInCategory;
                    }
                }

                if (actionsInCategory == 0) continue;

                ImGui::TextColored(TC(Theme().primary), "%s",
                                   InputSystem::GetActionCategoryName(static_cast<InputSystem::ActionCategory>(cat)));
                ImGui::Separator();

                for (i32 i = 0; i < actionCount; ++i) {
                    if (m_InputMap->GetActionCategory(i) != cat) continue;
                    if (!shown(i)) continue;

                    const char* actionName = m_InputMap->GetActionName(i);
                    // The keyboard column names keys and mouse buttons only. Look
                    // has no key (the mouse does it) and zoom is the wheel; they
                    // used to show the pad's stick names here.
                    const char* bindingName = m_InputMap->GetKeyboardBindingDisplayName(i);
                    if (!bindingName || !*bindingName) {
                        const auto act = static_cast<InputSystem::GameAction>(i);
                        if (act == InputSystem::GameAction::LookUp || act == InputSystem::GameAction::LookDown ||
                            act == InputSystem::GameAction::LookLeft || act == InputSystem::GameAction::LookRight)
                            bindingName = "Mouse";
                        else if (act == InputSystem::GameAction::CameraZoomIn) bindingName = "Wheel Up";
                        else if (act == InputSystem::GameAction::CameraZoomOut) bindingName = "Wheel Down";
                        else bindingName = "-";
                    }

                    ImGui::Text("  %-22s", actionName);
                    ImGui::SameLine(220);
                    ImGui::TextColored(TC(Theme().textPrimary), "%s", bindingName);

                    // Show gamepad binding on the same line if available
                    const char* gamepadBinding = m_InputMap->GetGamepadBindingDisplayName(i);
                    if (gamepadBinding && gamepadBinding[0] != '\0') {
                        ImGui::SameLine(380);
                        ImGui::TextDisabled("%s", gamepadBinding);
                    }
                }

                ImGui::Dummy(ImVec2(0, 6));
            }
        }
        ImGui::EndChild();
    }

    ImGui::Dummy(ImVec2(0, 4));
    if (ImGui::Button("Back", ImVec2(100, 32))) {
        ShowScreen(m_ReturnScreen);
    }

    ImGui::End();
    ImGui::PopStyleColor(themeColors);
    ImGui::PopStyleVar(2);
}

// ---------------------------------------------------------------------------
// Styled menu button
// ---------------------------------------------------------------------------

bool GameMenuSystem::RenderMenuButton(const char* label, f32 width, bool selected,
                                      bool enabled) {
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(16.0f, 12.0f));

    // A button that cannot do anything says so by looking like it. The
    // alternative -- rendering it normally and ignoring the click -- is the
    // shape of failure this menu was built to stop: Continue looked available
    // on a cold boot and silently started a new game.
    if (!enabled) ImGui::BeginDisabled();

    const UITheme& t = Theme();
    if (selected) {
        ImGui::PushStyleColor(ImGuiCol_Button, TC(t.primary, 0.85f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, TC(t.buttonHovered));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, TC(t.buttonPressed));
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, TC(t.buttonDefault, 0.92f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, TC(t.buttonHovered));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, TC(t.buttonPressed));
    }

    ImGui::PushStyleColor(ImGuiCol_Text, TC(t.textPrimary));

    bool pressed = ImGui::Button(label, ImVec2(width, 0));

    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar(2);

    if (!enabled) ImGui::EndDisabled();
    return pressed;
}

// ---------------------------------------------------------------------------
// Game Over Screen
// ---------------------------------------------------------------------------

void GameMenuSystem::ShowGameOver(bool won, const std::string& message,
                                   bool allowRestart, bool returnToMenu) {
    m_GameOverWon = won;
    m_GameOverMessage = message;
    m_GameOverAllowRestart = allowRestart;
    m_GameOverReturnToMenu = returnToMenu;
    m_CurrentScreen = MenuScreen::GameOver;
}

void GameMenuSystem::RenderGameOver(f32 w, f32 h) {
    // Full-screen dark overlay (heavier than pause for finality)
    ImDrawList* draw = TargetDrawList();
    ImU32 overlayColor = m_GameOverWon ? IM_COL32(5, 15, 5, 220)
                                       : IM_COL32(18, 5, 5, 220);
    draw->AddRectFilled(ImVec2(0, 0), ImVec2(w, h), overlayColor);

    const f32 panelW = 360.0f;
    const f32 panelH = 260.0f;

    ImGui::SetNextWindowPos(ImVec2((w - panelW) * 0.5f, (h - panelH) * 0.5f));
    ImGui::SetNextWindowSize(ImVec2(panelW, panelH));
    ImGui::Begin("##GameOver", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoSavedSettings);

    // Message
    ImVec4 messageColor = m_GameOverWon ? ImVec4(0.3f, 1.0f, 0.3f, 1.0f)
                                        : ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
    {
        ImVec2 textSize = ImGui::CalcTextSize(m_GameOverMessage.c_str());
        ImGui::SetCursorPosX((panelW - textSize.x) * 0.5f);
        ImGui::TextColored(messageColor, "%s", m_GameOverMessage.c_str());
    }

    ImGui::Dummy(ImVec2(0, 40));

    const f32 buttonW = 240.0f;

    auto CenterButton = [&](const char* label, const char* action) {
        ImGui::SetCursorPosX((panelW - buttonW) * 0.5f);
        // The callback id is already a stable name for this button, so it
        // doubles as the localization key -- no second naming scheme to
        // agree on. The English literal is the fallback, so an
        // untranslated project renders exactly as it did.
        const std::string text =
            LocalizationManager::Get().GetString(std::string("menu.") + action, label);
        if (RenderMenuButton(text.c_str(), buttonW)) {
            if (m_Callback) m_Callback(action);
        }
        ImGui::Dummy(ImVec2(0, 6));
    };

    if (m_GameOverAllowRestart) {
        CenterButton("Restart", "game_over_restart");
    }
    if (m_GameOverReturnToMenu) {
        CenterButton("Main Menu", "game_over_menu");
    }

    ImGui::End();
}

} // namespace Enjin::GUI
