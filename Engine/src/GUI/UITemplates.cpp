#include "Enjin/GUI/UITemplates.h"

namespace Enjin::GUI {

namespace UITemplates {

UICanvasComponent CreateMainMenu(const std::string& gameTitle) {
    UICanvasComponent canvas;
    canvas.canvasName = "MainMenu";
    canvas.sortOrder = 100;
    canvas.theme = UITheme::Dark();

    // Full-screen dark background panel
    u32 bgPanel = canvas.AddElement(UIWidgetType::Panel, "Background");
    {
        auto* bg = canvas.GetElement(bgPanel);
        bg->anchor.anchorMin = Math::Vector2(0.0f, 0.0f);
        bg->anchor.anchorMax = Math::Vector2(1.0f, 1.0f);
        bg->anchor.offsetLeft = 0; bg->anchor.offsetRight = 0;
        bg->anchor.offsetTop = 0;  bg->anchor.offsetBottom = 0;
        bg->style.bgColor = Math::Vector3(0.05f, 0.05f, 0.08f);
        bg->style.bgAlpha = 0.95f;
        bg->style.borderWidth = 0.0f;
    }

    // Title label
    u32 title = canvas.AddElement(UIWidgetType::Label, "Title", bgPanel);
    {
        auto* t = canvas.GetElement(title);
        t->anchor.anchorMin = Math::Vector2(0.5f, 0.2f);
        t->anchor.anchorMax = Math::Vector2(0.5f, 0.2f);
        // Unity-style anchors: edge = anchor*parent + offset, so a centered
        // 600-wide element is offsetLeft=-300 / offsetRight=+300 (NOT -300/-300,
        // which yields zero width — the classic inverted-offset template bug).
        t->anchor.offsetLeft = -300.0f; t->anchor.offsetRight = 300.0f;
        t->anchor.offsetTop = -30.0f;   t->anchor.offsetBottom = 30.0f;
        t->data.text = gameTitle;
        t->data.textAlignH = 1;
        t->style.fontSize = 42.0f;
    }

    // Menu button panel (centered column)
    u32 menuPanel = canvas.AddElement(UIWidgetType::Panel, "MenuButtons", bgPanel);
    {
        auto* mp = canvas.GetElement(menuPanel);
        mp->anchor.anchorMin = Math::Vector2(0.5f, 0.40f);
        mp->anchor.anchorMax = Math::Vector2(0.5f, 0.75f);
        mp->anchor.offsetLeft = -150.0f; mp->anchor.offsetRight = 150.0f;
        mp->anchor.offsetTop = 0.0f;     mp->anchor.offsetBottom = 0.0f;
        mp->style.bgAlpha = 0.0f;
        mp->style.borderWidth = 0.0f;
    }

    // Buttons: New Game, Continue, Options, Quit
    const char* buttonNames[] = {"New Game", "Continue", "Options", "Quit"};
    const char* buttonEvents[] = {"menu_newgame", "menu_continue", "menu_options", "menu_quit"};

    for (int i = 0; i < 4; ++i) {
        u32 btn = canvas.AddElement(UIWidgetType::Button, buttonNames[i], menuPanel);
        auto* b = canvas.GetElement(btn);
        f32 yPos = 0.05f + static_cast<f32>(i) * 0.22f;
        b->anchor.anchorMin = Math::Vector2(0.1f, yPos);
        b->anchor.anchorMax = Math::Vector2(0.9f, yPos);
        b->anchor.offsetLeft = 0.0f; b->anchor.offsetRight = 0.0f;
        b->anchor.offsetTop = -22.0f; b->anchor.offsetBottom = 22.0f;
        b->data.text = buttonNames[i];
        b->onClickEvent = buttonEvents[i];
    }

    return canvas;
}

UICanvasComponent CreatePauseMenu() {
    UICanvasComponent canvas;
    canvas.canvasName = "PauseMenu";
    canvas.sortOrder = 200;
    canvas.theme = UITheme::Dark();

    // Semi-transparent overlay
    u32 overlay = canvas.AddElement(UIWidgetType::Panel, "Overlay");
    {
        auto* o = canvas.GetElement(overlay);
        o->anchor.anchorMin = Math::Vector2(0.0f, 0.0f);
        o->anchor.anchorMax = Math::Vector2(1.0f, 1.0f);
        o->anchor.offsetLeft = 0; o->anchor.offsetRight = 0;
        o->anchor.offsetTop = 0;  o->anchor.offsetBottom = 0;
        o->style.bgColor = Math::Vector3(0.0f, 0.0f, 0.0f);
        o->style.bgAlpha = 0.60f;
        o->style.borderWidth = 0.0f;
    }

    // Center panel
    u32 panel = canvas.AddElement(UIWidgetType::Panel, "PausePanel", overlay);
    {
        auto* p = canvas.GetElement(panel);
        p->anchor.anchorMin = Math::Vector2(0.5f, 0.5f);
        p->anchor.anchorMax = Math::Vector2(0.5f, 0.5f);
        p->anchor.offsetLeft = -180.0f; p->anchor.offsetRight = 180.0f;
        p->anchor.offsetTop = -140.0f;  p->anchor.offsetBottom = 140.0f;
        p->style.bgColor = Math::Vector3(0.10f, 0.10f, 0.14f);
        p->style.bgAlpha = 0.95f;
        p->style.borderRadius = 8.0f;
    }

    // "Paused" title
    u32 title = canvas.AddElement(UIWidgetType::Label, "Title", panel);
    {
        auto* t = canvas.GetElement(title);
        t->anchor.anchorMin = Math::Vector2(0.5f, 0.10f);
        t->anchor.anchorMax = Math::Vector2(0.5f, 0.10f);
        t->anchor.offsetLeft = -100.0f; t->anchor.offsetRight = 100.0f;
        t->anchor.offsetTop = -16.0f;   t->anchor.offsetBottom = 16.0f;
        t->data.text = "Paused";
        t->data.textAlignH = 1;
        t->style.fontSize = 28.0f;
    }

    // Resume, Options, Quit
    const char* buttonNames[] = {"Resume", "Options", "Quit"};
    const char* buttonEvents[] = {"pause_resume", "pause_options", "pause_quit"};

    for (int i = 0; i < 3; ++i) {
        u32 btn = canvas.AddElement(UIWidgetType::Button, buttonNames[i], panel);
        auto* b = canvas.GetElement(btn);
        f32 yPos = 0.30f + static_cast<f32>(i) * 0.22f;
        b->anchor.anchorMin = Math::Vector2(0.15f, yPos);
        b->anchor.anchorMax = Math::Vector2(0.85f, yPos);
        b->anchor.offsetLeft = 0.0f; b->anchor.offsetRight = 0.0f;
        b->anchor.offsetTop = -20.0f; b->anchor.offsetBottom = 20.0f;
        b->data.text = buttonNames[i];
        b->onClickEvent = buttonEvents[i];
    }

    return canvas;
}

UICanvasComponent CreateOptionsMenu() {
    UICanvasComponent canvas;
    canvas.canvasName = "OptionsMenu";
    canvas.sortOrder = 210;
    canvas.theme = UITheme::Dark();

    // Semi-transparent overlay
    u32 overlay = canvas.AddElement(UIWidgetType::Panel, "Overlay");
    {
        auto* o = canvas.GetElement(overlay);
        o->anchor.anchorMin = Math::Vector2(0.0f, 0.0f);
        o->anchor.anchorMax = Math::Vector2(1.0f, 1.0f);
        o->anchor.offsetLeft = 0; o->anchor.offsetRight = 0;
        o->anchor.offsetTop = 0;  o->anchor.offsetBottom = 0;
        o->style.bgColor = Math::Vector3(0.0f, 0.0f, 0.0f);
        o->style.bgAlpha = 0.60f;
        o->style.borderWidth = 0.0f;
    }

    // Options panel
    u32 panel = canvas.AddElement(UIWidgetType::Panel, "OptionsPanel", overlay);
    {
        auto* p = canvas.GetElement(panel);
        p->anchor.anchorMin = Math::Vector2(0.5f, 0.5f);
        p->anchor.anchorMax = Math::Vector2(0.5f, 0.5f);
        p->anchor.offsetLeft = -250.0f; p->anchor.offsetRight = 250.0f;
        p->anchor.offsetTop = -280.0f;  p->anchor.offsetBottom = 280.0f;
        p->style.bgColor = Math::Vector3(0.10f, 0.10f, 0.14f);
        p->style.bgAlpha = 0.95f;
        p->style.borderRadius = 8.0f;
    }

    // "Options" title
    u32 title = canvas.AddElement(UIWidgetType::Label, "Title", panel);
    {
        auto* t = canvas.GetElement(title);
        t->anchor.anchorMin = Math::Vector2(0.5f, 0.05f);
        t->anchor.anchorMax = Math::Vector2(0.5f, 0.05f);
        t->anchor.offsetLeft = -100.0f; t->anchor.offsetRight = 100.0f;
        t->anchor.offsetTop = -16.0f;   t->anchor.offsetBottom = 16.0f;
        t->data.text = "Options";
        t->data.textAlignH = 1;
        t->style.fontSize = 28.0f;
    }

    // Master Volume label + slider
    u32 volLabel = canvas.AddElement(UIWidgetType::Label, "VolumeLabel", panel);
    {
        auto* l = canvas.GetElement(volLabel);
        l->anchor.anchorMin = Math::Vector2(0.05f, 0.18f);
        l->anchor.anchorMax = Math::Vector2(0.35f, 0.18f);
        l->anchor.offsetLeft = 0; l->anchor.offsetRight = 0;
        l->anchor.offsetTop = -10.0f; l->anchor.offsetBottom = 10.0f;
        l->data.text = "Master Volume";
        l->data.textAlignH = 0;
    }
    u32 volSlider = canvas.AddElement(UIWidgetType::Slider, "VolumeSlider", panel);
    {
        auto* s = canvas.GetElement(volSlider);
        s->anchor.anchorMin = Math::Vector2(0.38f, 0.18f);
        s->anchor.anchorMax = Math::Vector2(0.92f, 0.18f);
        s->anchor.offsetLeft = 0; s->anchor.offsetRight = 0;
        s->anchor.offsetTop = -12.0f; s->anchor.offsetBottom = 12.0f;
        s->data.sliderValue = 0.8f;
        s->onValueChangedEvent = "options_master_volume";
    }

    // SFX Volume label + slider
    u32 sfxLabel = canvas.AddElement(UIWidgetType::Label, "SFXLabel", panel);
    {
        auto* l = canvas.GetElement(sfxLabel);
        l->anchor.anchorMin = Math::Vector2(0.05f, 0.30f);
        l->anchor.anchorMax = Math::Vector2(0.35f, 0.30f);
        l->anchor.offsetLeft = 0; l->anchor.offsetRight = 0;
        l->anchor.offsetTop = -10.0f; l->anchor.offsetBottom = 10.0f;
        l->data.text = "SFX Volume";
        l->data.textAlignH = 0;
    }
    u32 sfxSlider = canvas.AddElement(UIWidgetType::Slider, "SFXSlider", panel);
    {
        auto* s = canvas.GetElement(sfxSlider);
        s->anchor.anchorMin = Math::Vector2(0.38f, 0.30f);
        s->anchor.anchorMax = Math::Vector2(0.92f, 0.30f);
        s->anchor.offsetLeft = 0; s->anchor.offsetRight = 0;
        s->anchor.offsetTop = -12.0f; s->anchor.offsetBottom = 12.0f;
        s->data.sliderValue = 1.0f;
        s->onValueChangedEvent = "options_sfx_volume";
    }

    // Music Volume label + slider (desktop-menu parity; web routes it to the
    // Music channel like SFX)
    u32 musicLabel = canvas.AddElement(UIWidgetType::Label, "MusicLabel", panel);
    {
        auto* l = canvas.GetElement(musicLabel);
        l->anchor.anchorMin = Math::Vector2(0.05f, 0.375f);
        l->anchor.anchorMax = Math::Vector2(0.35f, 0.375f);
        l->anchor.offsetLeft = 0; l->anchor.offsetRight = 0;
        l->anchor.offsetTop = -10.0f; l->anchor.offsetBottom = 10.0f;
        l->data.text = "Music Volume";
        l->data.textAlignH = 0;
    }
    u32 musicSlider = canvas.AddElement(UIWidgetType::Slider, "MusicSlider", panel);
    {
        auto* s = canvas.GetElement(musicSlider);
        s->anchor.anchorMin = Math::Vector2(0.38f, 0.375f);
        s->anchor.anchorMax = Math::Vector2(0.92f, 0.375f);
        s->anchor.offsetLeft = 0; s->anchor.offsetRight = 0;
        s->anchor.offsetTop = -12.0f; s->anchor.offsetBottom = 12.0f;
        s->data.sliderValue = 1.0f;
        s->onValueChangedEvent = "options_music_volume";
    }

    // Fullscreen checkbox
    u32 fullscreenCheck = canvas.AddElement(UIWidgetType::Checkbox, "Fullscreen", panel);
    {
        auto* c = canvas.GetElement(fullscreenCheck);
        c->anchor.anchorMin = Math::Vector2(0.05f, 0.45f);
        c->anchor.anchorMax = Math::Vector2(0.5f, 0.45f);
        c->anchor.offsetLeft = 0; c->anchor.offsetRight = 0;
        c->anchor.offsetTop = -12.0f; c->anchor.offsetBottom = 12.0f;
        c->data.text = "Fullscreen";
        c->data.checked = false;
        c->onValueChangedEvent = "options_fullscreen";
    }

    // Field of View label + slider (desktop-menu parity; 40..120 degrees,
    // slider stores the normalized fraction)
    u32 fovLabel = canvas.AddElement(UIWidgetType::Label, "FOVLabel", panel);
    {
        auto* l = canvas.GetElement(fovLabel);
        l->anchor.anchorMin = Math::Vector2(0.05f, 0.505f);
        l->anchor.anchorMax = Math::Vector2(0.35f, 0.505f);
        l->anchor.offsetLeft = 0; l->anchor.offsetRight = 0;
        l->anchor.offsetTop = -10.0f; l->anchor.offsetBottom = 10.0f;
        l->data.text = "Field of View";
        l->data.textAlignH = 0;
    }
    u32 fovSlider = canvas.AddElement(UIWidgetType::Slider, "FOVSlider", panel);
    {
        auto* s = canvas.GetElement(fovSlider);
        s->anchor.anchorMin = Math::Vector2(0.38f, 0.505f);
        s->anchor.anchorMax = Math::Vector2(0.92f, 0.505f);
        s->anchor.offsetLeft = 0; s->anchor.offsetRight = 0;
        s->anchor.offsetTop = -12.0f; s->anchor.offsetBottom = 12.0f;
        s->data.sliderValue = 0.375f;   // (70 - 40) / 80 degrees
        s->onValueChangedEvent = "options_fov";
    }

    // Render Scale label + slider. The scene renders below screen resolution
    // and the post-process pass resolves it back up, which is the single
    // biggest thing a player on a weak GPU or a dense phone screen can change.
    // Slider fraction 0..1 maps to 0.5..1.0; 1.0 is native.
    u32 scaleLabel = canvas.AddElement(UIWidgetType::Label, "RenderScaleLabel", panel);
    {
        auto* l = canvas.GetElement(scaleLabel);
        l->anchor.anchorMin = Math::Vector2(0.05f, 0.55f);
        l->anchor.anchorMax = Math::Vector2(0.35f, 0.55f);
        l->anchor.offsetLeft = 0; l->anchor.offsetRight = 0;
        l->anchor.offsetTop = -10.0f; l->anchor.offsetBottom = 10.0f;
        l->data.text = "Render Scale";
        l->data.textAlignH = 0;
    }
    u32 scaleSlider = canvas.AddElement(UIWidgetType::Slider, "RenderScaleSlider", panel);
    {
        auto* s = canvas.GetElement(scaleSlider);
        s->anchor.anchorMin = Math::Vector2(0.38f, 0.55f);
        s->anchor.anchorMax = Math::Vector2(0.92f, 0.55f);
        s->anchor.offsetLeft = 0; s->anchor.offsetRight = 0;
        s->anchor.offsetTop = -12.0f; s->anchor.offsetBottom = 12.0f;
        s->data.sliderValue = 1.0f;     // native
        s->onValueChangedEvent = "options_render_scale";
    }

    // Shadows toggle
    u32 shadowsToggle = canvas.AddElement(UIWidgetType::Toggle, "Shadows", panel);
    {
        auto* t = canvas.GetElement(shadowsToggle);
        t->anchor.anchorMin = Math::Vector2(0.05f, 0.65f);
        t->anchor.anchorMax = Math::Vector2(0.5f, 0.65f);
        t->anchor.offsetLeft = 0; t->anchor.offsetRight = 0;
        t->anchor.offsetTop = -12.0f; t->anchor.offsetBottom = 12.0f;
        t->data.text = "Shadows";
        t->data.checked = true;
        t->onValueChangedEvent = "options_shadows";
    }

    // --- Accessibility section -------------------------------------------------
    u32 a11yLabel = canvas.AddElement(UIWidgetType::Label, "AccessLabel", panel);
    {
        auto* l = canvas.GetElement(a11yLabel);
        l->anchor.anchorMin = Math::Vector2(0.05f, 0.71f);
        l->anchor.anchorMax = Math::Vector2(0.5f, 0.71f);
        l->anchor.offsetLeft = 0; l->anchor.offsetRight = 0;
        l->anchor.offsetTop = -12.0f; l->anchor.offsetBottom = 12.0f;
        l->data.text = "Accessibility";
        l->data.textAlignH = 0;
        l->style.fontSize = 18.0f;
    }
    // Reduced Motion (left) + Subtitles (right)
    u32 reducedMotion = canvas.AddElement(UIWidgetType::Checkbox, "ReducedMotion", panel);
    {
        auto* c = canvas.GetElement(reducedMotion);
        c->anchor.anchorMin = Math::Vector2(0.05f, 0.77f);
        c->anchor.anchorMax = Math::Vector2(0.48f, 0.77f);
        c->anchor.offsetLeft = 0; c->anchor.offsetRight = 0;
        c->anchor.offsetTop = -12.0f; c->anchor.offsetBottom = 12.0f;
        c->data.text = "Reduced Motion";
        c->data.checked = false;
        c->onValueChangedEvent = "options_reduced_motion";
    }
    u32 subtitlesChk = canvas.AddElement(UIWidgetType::Checkbox, "Subtitles", panel);
    {
        auto* c = canvas.GetElement(subtitlesChk);
        c->anchor.anchorMin = Math::Vector2(0.52f, 0.77f);
        c->anchor.anchorMax = Math::Vector2(0.95f, 0.77f);
        c->anchor.offsetLeft = 0; c->anchor.offsetRight = 0;
        c->anchor.offsetTop = -12.0f; c->anchor.offsetBottom = 12.0f;
        c->data.text = "Subtitles";
        c->data.checked = false;
        c->onValueChangedEvent = "options_subtitles";
    }
    // Dyslexia Font (left)
    u32 dyslexiaChk = canvas.AddElement(UIWidgetType::Checkbox, "DyslexiaFont", panel);
    {
        auto* c = canvas.GetElement(dyslexiaChk);
        c->anchor.anchorMin = Math::Vector2(0.05f, 0.83f);
        c->anchor.anchorMax = Math::Vector2(0.48f, 0.83f);
        c->anchor.offsetLeft = 0; c->anchor.offsetRight = 0;
        c->anchor.offsetTop = -12.0f; c->anchor.offsetBottom = 12.0f;
        c->data.text = "Dyslexia Font";
        c->data.checked = false;
        c->onValueChangedEvent = "options_dyslexia";
    }
    // Colorblind mode label + slider (0 = Off .. 8 modes)
    u32 cbLabel = canvas.AddElement(UIWidgetType::Label, "ColorblindLabel", panel);
    {
        auto* l = canvas.GetElement(cbLabel);
        l->anchor.anchorMin = Math::Vector2(0.05f, 0.89f);
        l->anchor.anchorMax = Math::Vector2(0.35f, 0.89f);
        l->anchor.offsetLeft = 0; l->anchor.offsetRight = 0;
        l->anchor.offsetTop = -10.0f; l->anchor.offsetBottom = 10.0f;
        l->data.text = "Colorblind";
        l->data.textAlignH = 0;
    }
    u32 cbSlider = canvas.AddElement(UIWidgetType::Slider, "ColorblindSlider", panel);
    {
        auto* s = canvas.GetElement(cbSlider);
        s->anchor.anchorMin = Math::Vector2(0.38f, 0.89f);
        s->anchor.anchorMax = Math::Vector2(0.92f, 0.89f);
        s->anchor.offsetLeft = 0; s->anchor.offsetRight = 0;
        s->anchor.offsetTop = -12.0f; s->anchor.offsetBottom = 12.0f;
        s->data.sliderValue = 0.0f;
        s->onValueChangedEvent = "options_colorblind";
    }

    // Back button
    u32 backBtn = canvas.AddElement(UIWidgetType::Button, "Back", panel);
    {
        auto* b = canvas.GetElement(backBtn);
        b->anchor.anchorMin = Math::Vector2(0.3f, 0.95f);
        b->anchor.anchorMax = Math::Vector2(0.7f, 0.95f);
        b->anchor.offsetLeft = 0.0f; b->anchor.offsetRight = 0.0f;
        b->anchor.offsetTop = -20.0f; b->anchor.offsetBottom = 20.0f;
        b->data.text = "Back";
        b->onClickEvent = "options_back";
    }

    // Suppress unused variable warnings
    (void)volLabel; (void)sfxLabel; (void)fullscreenCheck;
    (void)scaleLabel; (void)scaleSlider; (void)shadowsToggle;
    (void)a11yLabel; (void)reducedMotion; (void)subtitlesChk;
    (void)dyslexiaChk; (void)cbLabel; (void)cbSlider;

    return canvas;
}

UICanvasComponent CreateGameOverScreen(bool won, const std::string& message, bool allowRestart) {
    UICanvasComponent canvas;
    canvas.canvasName = "GameOverScreen";
    canvas.sortOrder = 300;  // Above pause menu (200) and gameplay HUD canvases
    canvas.theme = UITheme::Dark();

    // Full-screen dark overlay
    u32 overlay = canvas.AddElement(UIWidgetType::Panel, "Overlay");
    {
        auto* o = canvas.GetElement(overlay);
        o->anchor.anchorMin = Math::Vector2(0.0f, 0.0f);
        o->anchor.anchorMax = Math::Vector2(1.0f, 1.0f);
        o->anchor.offsetLeft = 0; o->anchor.offsetRight = 0;
        o->anchor.offsetTop = 0;  o->anchor.offsetBottom = 0;
        o->style.bgColor = Math::Vector3(0.0f, 0.0f, 0.0f);
        o->style.bgAlpha = 0.65f;
        o->style.borderWidth = 0.0f;
    }

    // Victory/defeat message (green on win, red on loss)
    u32 title = canvas.AddElement(UIWidgetType::Label, "Message", overlay);
    {
        auto* t = canvas.GetElement(title);
        t->anchor.anchorMin = Math::Vector2(0.5f, 0.40f);
        t->anchor.anchorMax = Math::Vector2(0.5f, 0.40f);
        t->anchor.offsetLeft = -400.0f; t->anchor.offsetRight = 400.0f;
        t->anchor.offsetTop = -30.0f;   t->anchor.offsetBottom = 30.0f;
        t->data.text = message;
        t->data.textAlignH = 1;
        t->style.fontSize = 38.0f;
        t->style.textColor = won ? Math::Vector3(0.45f, 1.0f, 0.45f)
                                 : Math::Vector3(1.0f, 0.45f, 0.45f);
    }

    if (allowRestart) {
        u32 btn = canvas.AddElement(UIWidgetType::Button, "PlayAgain", overlay);
        auto* b = canvas.GetElement(btn);
        b->anchor.anchorMin = Math::Vector2(0.5f, 0.55f);
        b->anchor.anchorMax = Math::Vector2(0.5f, 0.55f);
        b->anchor.offsetLeft = -120.0f; b->anchor.offsetRight = 120.0f;
        b->anchor.offsetTop = -24.0f;   b->anchor.offsetBottom = 24.0f;
        b->data.text = "Play Again";
        b->onClickEvent = "gameover_restart";
    }

    return canvas;
}

} // namespace UITemplates

} // namespace Enjin::GUI
