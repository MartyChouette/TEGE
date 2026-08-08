#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/Input/InputAction.h"
#include "Enjin/Editor/EditorSettings.h"
#include "Enjin/Accessibility/AccessibilitySettings.h"

#include <imgui.h>

#include <functional>
#include <string>

namespace Enjin::GUI {

enum class MenuScreen : u8 {
    None,
    MainMenu,
    PauseMenu,
    Options,
    Graphics,
    Audio,
    Controls,
    HowToPlay,
    GameOver
};

struct GraphicsSettings {
    u32 resolutionWidth = 1920;
    u32 resolutionHeight = 1080;
    bool fullscreen = false;
    bool vsync = true;
    u32 qualityPreset = 2;
    f32 renderScale = 1.0f;
    f32 fieldOfView = 60.0f;
    bool bloom = true;
    bool fxaa = true;
    bool shadows = true;
    u32 shadowQuality = 2;
};

struct AudioSettings {
    f32 masterVolume = 1.0f;
    f32 musicVolume = 0.8f;
    f32 sfxVolume = 1.0f;
    f32 voiceVolume = 1.0f;
    bool masterMute = false;
    bool musicMute = false;
    bool sfxMute = false;
    bool voiceMute = false;
};

class ENJIN_API GameMenuSystem {
public:
    using MenuCallback = std::function<void(const std::string&)>;

    GameMenuSystem() = default;
    ~GameMenuSystem() = default;

    void SetInputMap(InputSystem::InputActionMap* map);
    void SetEditorSettings(Editor::EditorSettings* settings);

    GraphicsSettings& GetGraphicsSettings();
    AudioSettings& GetAudioSettings();

    // Called when the user leaves the Options screen (Back button)
    using SettingsCallback = std::function<void(const GraphicsSettings&, const AudioSettings&)>;
    void SetSettingsCallback(SettingsCallback cb) { m_SettingsCallback = std::move(cb); }

    // Called when the Options screen OPENS, so the host can fill the menu from
    // live state. Without this the menu applies its own struct defaults on Back
    // (bloom = true stomped the renderer's setting every options visit).
    using SettingsSyncCallback = std::function<void(GraphicsSettings&, AudioSettings&)>;
    void SetSettingsSyncCallback(SettingsSyncCallback cb) { m_SettingsSyncCallback = std::move(cb); }

    // Accessibility tab edits the host's LIVE RuntimeAccessibilitySettings in
    // place (no copy — the host's per-frame apply loops pick most fields up).
    // The changed callback fires on any edit so the host can re-push the fields
    // that are only read at boot (subtitle config, indicators, announcer,
    // UISystem motor toggles, dyslexia font) and persist to accessibility.json.
    void SetAccessibilitySettings(Accessibility::RuntimeAccessibilitySettings* s) { m_Accessibility = s; }
    using AccessibilityChangedCallback = std::function<void()>;
    void SetAccessibilityChangedCallback(AccessibilityChangedCallback cb) { m_AccessibilityChanged = std::move(cb); }

    void ShowScreen(MenuScreen screen);
    void HideAll();
    MenuScreen GetCurrentScreen() const;
    bool IsMenuOpen() const;

    void Render(f32 screenW, f32 screenH);

    void SetCallback(MenuCallback cb);
    const MenuCallback& GetCallback() const { return m_Callback; }
    void SetGameTitle(const std::string& title);

    // Game over screen
    void ShowGameOver(bool won, const std::string& message, bool allowRestart, bool returnToMenu);
    bool IsGameOverScreen() const { return m_CurrentScreen == MenuScreen::GameOver; }

private:
    MenuScreen m_CurrentScreen = MenuScreen::None;
    GraphicsSettings m_Graphics;
    AudioSettings m_Audio;
    InputSystem::InputActionMap* m_InputMap = nullptr;
    Editor::EditorSettings* m_EditorSettings = nullptr;
    Accessibility::RuntimeAccessibilitySettings* m_Accessibility = nullptr;
    MenuCallback m_Callback;
    SettingsCallback m_SettingsCallback;
    SettingsSyncCallback m_SettingsSyncCallback;
    AccessibilityChangedCallback m_AccessibilityChanged;
    std::string m_GameTitle = "My Game";
    i32 m_RebindingAction = -1;
    MenuScreen m_ReturnScreen = MenuScreen::PauseMenu;

    void RenderMainMenu(f32 w, f32 h);
    void RenderPauseMenu(f32 w, f32 h);
    void RenderOptions(f32 w, f32 h);
    void RenderGraphics(f32 w, f32 h);
    void RenderAudio(f32 w, f32 h);
    void RenderAccessibility(f32 w, f32 h);
    void RenderControls(f32 w, f32 h);
    void RenderHowToPlay(f32 w, f32 h);
    void RenderGameOver(f32 w, f32 h);
    bool RenderMenuButton(const char* label, f32 width, bool selected = false);

    // Game over state
    bool m_GameOverWon = false;
    std::string m_GameOverMessage;
    bool m_GameOverAllowRestart = true;
    bool m_GameOverReturnToMenu = true;
};

} // namespace Enjin::GUI
