#include "Enjin/GUI/GameMenus.h"

#include <algorithm>
#include <array>

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
        if (m_CurrentScreen == MenuScreen::MainMenu || m_CurrentScreen == MenuScreen::PauseMenu) {
            m_ReturnScreen = m_CurrentScreen;
        }
    }
    m_CurrentScreen = screen;
    m_RebindingAction = -1;
}

void GameMenuSystem::HideAll() {
    m_CurrentScreen = MenuScreen::None;
    m_RebindingAction = -1;
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

void GameMenuSystem::Render(f32 screenW, f32 screenH) {
    switch (m_CurrentScreen) {
        case MenuScreen::MainMenu:   RenderMainMenu(screenW, screenH);  break;
        case MenuScreen::PauseMenu:  RenderPauseMenu(screenW, screenH); break;
        case MenuScreen::Options:    RenderOptions(screenW, screenH);   break;
        case MenuScreen::Graphics:   RenderGraphics(screenW, screenH);  break;
        case MenuScreen::Audio:      RenderAudio(screenW, screenH);     break;
        case MenuScreen::Controls:   RenderControls(screenW, screenH);  break;
        case MenuScreen::HowToPlay:  RenderHowToPlay(screenW, screenH); break;
        case MenuScreen::None:
        default:
            break;
    }
}

void GameMenuSystem::RenderInViewport(f32 vpX, f32 vpY, f32 vpW, f32 vpH) {
    if (m_CurrentScreen != MenuScreen::PauseMenu) {
        // Non-pause screens still render fullscreen (options, etc.)
        Render(ImGui::GetIO().DisplaySize.x, ImGui::GetIO().DisplaySize.y);
        return;
    }

    // Dark overlay only covers the game view area (behind editor panels)
    ImDrawList* bgDraw = ImGui::GetBackgroundDrawList();
    bgDraw->AddRectFilled(ImVec2(vpX, vpY), ImVec2(vpX + vpW, vpY + vpH), IM_COL32(0, 0, 0, 160));

    // Center pause menu within the game view viewport
    const f32 buttonW = 260.0f;
    const f32 panelH = 260.0f;

    ImGui::SetNextWindowPos(ImVec2(vpX + (vpW - buttonW - 60.0f) * 0.5f, vpY + (vpH - panelH) * 0.5f));
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
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 0.95f), "%s", title);
    }

    ImGui::Dummy(ImVec2(0, 20));

    auto CenterButton = [&](const char* label, const char* action) {
        ImGui::SetCursorPosX((buttonW + 60.0f - buttonW) * 0.5f);
        if (RenderMenuButton(label, buttonW)) {
            if (m_Callback) m_Callback(action);
        }
        ImGui::Dummy(ImVec2(0, 6));
    };

    CenterButton("Resume",        "resume");
    CenterButton("Options",       "options");
    CenterButton("How to Play",   "how_to_play");
    CenterButton("Quit to Menu",  "quit_to_menu");

    ImGui::End();
}

// ---------------------------------------------------------------------------
// Main Menu
// ---------------------------------------------------------------------------

void GameMenuSystem::RenderMainMenu(f32 w, f32 h) {
    // Full-screen dark background
    ImDrawList* draw = ImGui::GetForegroundDrawList();
    draw->AddRectFilled(ImVec2(0, 0), ImVec2(w, h), IM_COL32(12, 12, 18, 240));

    const f32 buttonW = 280.0f;
    const f32 panelH = 340.0f;
    const f32 startX = (w - buttonW) * 0.5f;
    const f32 startY = (h - panelH) * 0.5f;

    ImGui::SetNextWindowPos(ImVec2(startX - 40.0f, startY - 60.0f));
    ImGui::SetNextWindowSize(ImVec2(buttonW + 80.0f, panelH + 100.0f));
    ImGui::Begin("##MainMenu", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoSavedSettings);

    // Title
    ImGui::PushFont(nullptr); // use default; engine may push a large font externally
    {
        ImVec2 titleSize = ImGui::CalcTextSize(m_GameTitle.c_str());
        ImGui::SetCursorPosX((buttonW + 80.0f - titleSize.x) * 0.5f);
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "%s", m_GameTitle.c_str());
    }
    ImGui::PopFont();

    ImGui::Dummy(ImVec2(0, 30));

    // Center buttons
    auto CenterButton = [&](const char* label, const char* action) {
        ImGui::SetCursorPosX((buttonW + 80.0f - buttonW) * 0.5f);
        if (RenderMenuButton(label, buttonW)) {
            if (m_Callback) m_Callback(action);
        }
        ImGui::Dummy(ImVec2(0, 6));
    };

    CenterButton("New Game",  "new_game");
    CenterButton("Continue",  "continue");
    CenterButton("Options",   "options");
    CenterButton("How to Play", "how_to_play");
    CenterButton("Quit",      "quit");

    // Handle options / how-to-play navigation internally as well
    // The callback can decide whether to navigate or the caller can call ShowScreen
    ImGui::End();
}

// ---------------------------------------------------------------------------
// Pause Menu
// ---------------------------------------------------------------------------

void GameMenuSystem::RenderPauseMenu(f32 w, f32 h) {
    // Semi-transparent dark overlay
    ImDrawList* draw = ImGui::GetForegroundDrawList();
    draw->AddRectFilled(ImVec2(0, 0), ImVec2(w, h), IM_COL32(0, 0, 0, 160));

    const f32 buttonW = 260.0f;
    const f32 panelH = 260.0f;

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
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 0.95f), "%s", title);
    }

    ImGui::Dummy(ImVec2(0, 20));

    auto CenterButton = [&](const char* label, const char* action) {
        ImGui::SetCursorPosX((buttonW + 60.0f - buttonW) * 0.5f);
        if (RenderMenuButton(label, buttonW)) {
            if (m_Callback) m_Callback(action);
        }
        ImGui::Dummy(ImVec2(0, 6));
    };

    CenterButton("Resume",        "resume");
    CenterButton("Options",       "options");
    CenterButton("How to Play",   "how_to_play");
    CenterButton("Quit to Menu",  "quit_to_menu");

    ImGui::End();
}

// ---------------------------------------------------------------------------
// Options (tab-based hub)
// ---------------------------------------------------------------------------

void GameMenuSystem::RenderOptions(f32 w, f32 h) {
    ImDrawList* draw = ImGui::GetForegroundDrawList();
    draw->AddRectFilled(ImVec2(0, 0), ImVec2(w, h), IM_COL32(10, 10, 14, 230));

    const f32 panelW = 560.0f;
    const f32 panelH = 480.0f;

    ImGui::SetNextWindowPos(ImVec2((w - panelW) * 0.5f, (h - panelH) * 0.5f));
    ImGui::SetNextWindowSize(ImVec2(panelW, panelH));
    ImGui::Begin("##Options", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);

    ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "Options");
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
        if (ImGui::BeginTabItem("Controls")) {
            RenderControls(w, h);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    ImGui::Dummy(ImVec2(0, 10));
    if (ImGui::Button("Back", ImVec2(100, 32))) {
        ShowScreen(m_ReturnScreen);
    }

    ImGui::End();
}

// ---------------------------------------------------------------------------
// Graphics Settings
// ---------------------------------------------------------------------------

void GameMenuSystem::RenderGraphics(f32 w, f32 h) {
    (void)w;
    (void)h;

    ImGui::Dummy(ImVec2(0, 4));

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

    ImGui::Checkbox("Fullscreen", &m_Graphics.fullscreen);
    ImGui::Checkbox("VSync", &m_Graphics.vsync);

    // Quality preset
    static const char* qualityLabels[] = { "Low", "Medium", "High", "Ultra" };
    i32 quality = static_cast<i32>(m_Graphics.qualityPreset);
    if (quality < 0) quality = 0;
    if (quality > 3) quality = 3;
    if (ImGui::Combo("Quality Preset", &quality, qualityLabels, 4)) {
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

    ImGui::SliderFloat("Render Scale", &m_Graphics.renderScale, 0.25f, 2.0f, "%.2f");

    ImGui::Separator();
    ImGui::Text("Post-Processing");
    ImGui::Checkbox("Bloom", &m_Graphics.bloom);
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
// Controls
// ---------------------------------------------------------------------------

void GameMenuSystem::RenderControls(f32 w, f32 h) {
    (void)w;
    (void)h;

    ImGui::Dummy(ImVec2(0, 4));

    if (!m_InputMap) {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.3f, 1.0f), "No InputActionMap assigned.");
        return;
    }

    // Mouse sensitivity
    f32 sensitivity = m_InputMap->GetMouseSensitivity();
    if (ImGui::SliderFloat("Mouse Sensitivity", &sensitivity, 0.05f, 5.0f, "%.2f")) {
        m_InputMap->SetMouseSensitivity(sensitivity);
    }

    // Sprint / Crouch mode
    static const char* toggleModes[] = { "Hold", "Toggle" };

    i32 sprintMode = m_InputMap->IsSprintToggle() ? 1 : 0;
    if (ImGui::Combo("Sprint Mode", &sprintMode, toggleModes, 2)) {
        m_InputMap->SetSprintToggle(sprintMode == 1);
    }

    i32 crouchMode = m_InputMap->IsCrouchToggle() ? 1 : 0;
    if (ImGui::Combo("Crouch Mode", &crouchMode, toggleModes, 2)) {
        m_InputMap->SetCrouchToggle(crouchMode == 1);
    }

    ImGui::Separator();
    ImGui::Text("Key Bindings");
    ImGui::Dummy(ImVec2(0, 4));

    // Rebinding prompt
    if (m_RebindingAction >= 0) {
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.3f, 1.0f), "Press any key to rebind...");
        ImGui::SameLine();
        if (ImGui::SmallButton("Cancel")) {
            m_RebindingAction = -1;
        }

        // Poll for key press
        i32 pressedKey = m_InputMap->PollNextKeyPress();
        if (pressedKey >= 0) {
            m_InputMap->RebindAction(m_RebindingAction, pressedKey);
            m_RebindingAction = -1;
        }
    }

    // Action list
    if (ImGui::BeginChild("##BindingsList", ImVec2(0, -50), true)) {
        i32 actionCount = m_InputMap->GetActionCount();
        for (i32 i = 0; i < actionCount; ++i) {
            const char* actionName = m_InputMap->GetActionName(i);
            const char* bindingName = m_InputMap->GetBindingDisplayName(i);

            ImGui::PushID(i);

            // Action label
            ImGui::Text("%-24s", actionName);
            ImGui::SameLine(250);

            // Binding button - click to rebind
            bool isRebinding = (m_RebindingAction == i);
            if (isRebinding) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.6f, 0.1f, 1.0f));
            }

            const char* displayText = isRebinding ? "..." : bindingName;
            if (ImGui::Button(displayText, ImVec2(140, 0))) {
                m_RebindingAction = i;
            }

            if (isRebinding) {
                ImGui::PopStyleColor();
            }

            // Show gamepad binding if available
            const char* gamepadBinding = m_InputMap->GetGamepadBindingDisplayName(i);
            if (gamepadBinding && gamepadBinding[0] != '\0') {
                ImGui::SameLine();
                ImGui::TextDisabled("[%s]", gamepadBinding);
            }

            ImGui::PopID();
        }
    }
    ImGui::EndChild();

    // Preset buttons
    if (ImGui::Button("Reset Defaults", ImVec2(130, 28))) {
        m_InputMap->ResetToDefaults();
        m_RebindingAction = -1;
    }
    ImGui::SameLine();
    if (ImGui::Button("Left Hand", ImVec2(100, 28))) {
        m_InputMap->ApplyLeftHandOnly();
        m_RebindingAction = -1;
    }
    ImGui::SameLine();
    if (ImGui::Button("Right Hand", ImVec2(100, 28))) {
        m_InputMap->ApplyRightHandOnly();
        m_RebindingAction = -1;
    }
}

// ---------------------------------------------------------------------------
// How to Play
// ---------------------------------------------------------------------------

void GameMenuSystem::RenderHowToPlay(f32 w, f32 h) {
    ImDrawList* draw = ImGui::GetForegroundDrawList();
    draw->AddRectFilled(ImVec2(0, 0), ImVec2(w, h), IM_COL32(10, 10, 14, 230));

    const f32 panelW = 520.0f;
    const f32 panelH = 460.0f;

    ImGui::SetNextWindowPos(ImVec2((w - panelW) * 0.5f, (h - panelH) * 0.5f));
    ImGui::SetNextWindowSize(ImVec2(panelW, panelH));
    ImGui::Begin("##HowToPlay", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);

    ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "How to Play");
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0, 6));

    if (!m_InputMap) {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.3f, 1.0f), "No InputActionMap assigned.");
    } else {
        if (ImGui::BeginChild("##HowToPlayList", ImVec2(0, -44), true)) {
            // Group actions by category
            static const char* categories[] = { "Movement", "Actions", "Camera", "Other" };
            static const i32 categoryCount = 4;

            for (i32 cat = 0; cat < categoryCount; ++cat) {
                i32 actionsInCategory = 0;

                // First pass: count actions in this category
                i32 actionCount = m_InputMap->GetActionCount();
                for (i32 i = 0; i < actionCount; ++i) {
                    if (m_InputMap->GetActionCategory(i) == cat) {
                        ++actionsInCategory;
                    }
                }

                if (actionsInCategory == 0) continue;

                ImGui::TextColored(ImVec4(0.6f, 0.85f, 1.0f, 1.0f), "%s", categories[cat]);
                ImGui::Separator();

                for (i32 i = 0; i < actionCount; ++i) {
                    if (m_InputMap->GetActionCategory(i) != cat) continue;

                    const char* actionName = m_InputMap->GetActionName(i);
                    const char* bindingName = m_InputMap->GetBindingDisplayName(i);

                    ImGui::Text("  %-22s", actionName);
                    ImGui::SameLine(220);
                    ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.5f, 1.0f), "%s", bindingName);

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
}

// ---------------------------------------------------------------------------
// Styled menu button
// ---------------------------------------------------------------------------

bool GameMenuSystem::RenderMenuButton(const char* label, f32 width, bool selected) {
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12.0f, 10.0f));

    if (selected) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.35f, 0.45f, 0.65f, 0.9f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.40f, 0.50f, 0.70f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.30f, 0.40f, 0.60f, 1.0f));
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.20f, 0.28f, 0.85f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.28f, 0.32f, 0.45f, 0.95f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.22f, 0.26f, 0.38f, 1.0f));
    }

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.95f, 0.97f, 1.0f));

    bool pressed = ImGui::Button(label, ImVec2(width, 0));

    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar(2);

    return pressed;
}

} // namespace Enjin::GUI
