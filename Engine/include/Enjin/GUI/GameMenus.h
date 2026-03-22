#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/Input/InputAction.h"
#include "Enjin/Editor/EditorSettings.h"

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
    HowToPlay
};

struct GraphicsSettings {
    u32 resolutionWidth = 1920;
    u32 resolutionHeight = 1080;
    bool fullscreen = false;
    bool vsync = true;
    u32 qualityPreset = 2;
    f32 renderScale = 1.0f;
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

    void ShowScreen(MenuScreen screen);
    void HideAll();
    MenuScreen GetCurrentScreen() const;
    bool IsMenuOpen() const;

    void Render(f32 screenW, f32 screenH);

    void SetCallback(MenuCallback cb);
    const MenuCallback& GetCallback() const { return m_Callback; }
    void SetGameTitle(const std::string& title);

private:
    MenuScreen m_CurrentScreen = MenuScreen::None;
    GraphicsSettings m_Graphics;
    AudioSettings m_Audio;
    InputSystem::InputActionMap* m_InputMap = nullptr;
    Editor::EditorSettings* m_EditorSettings = nullptr;
    MenuCallback m_Callback;
    std::string m_GameTitle = "My Game";
    i32 m_RebindingAction = -1;
    MenuScreen m_ReturnScreen = MenuScreen::PauseMenu;

    void RenderMainMenu(f32 w, f32 h);
    void RenderPauseMenu(f32 w, f32 h);
    void RenderOptions(f32 w, f32 h);
    void RenderGraphics(f32 w, f32 h);
    void RenderAudio(f32 w, f32 h);
    void RenderControls(f32 w, f32 h);
    void RenderHowToPlay(f32 w, f32 h);
    bool RenderMenuButton(const char* label, f32 width, bool selected = false);
};

} // namespace Enjin::GUI
