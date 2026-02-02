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
        t->anchor.offsetLeft = -300.0f; t->anchor.offsetRight = -300.0f;
        t->anchor.offsetTop = -30.0f;   t->anchor.offsetBottom = -30.0f;
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
        b->anchor.offsetTop = -22.0f; b->anchor.offsetBottom = -22.0f;
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
        p->anchor.offsetLeft = -180.0f; p->anchor.offsetRight = -180.0f;
        p->anchor.offsetTop = -140.0f;  p->anchor.offsetBottom = -140.0f;
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
        t->anchor.offsetLeft = -100.0f; t->anchor.offsetRight = -100.0f;
        t->anchor.offsetTop = -16.0f;   t->anchor.offsetBottom = -16.0f;
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
        b->anchor.offsetTop = -20.0f; b->anchor.offsetBottom = -20.0f;
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
        p->anchor.offsetLeft = -250.0f; p->anchor.offsetRight = -250.0f;
        p->anchor.offsetTop = -200.0f;  p->anchor.offsetBottom = -200.0f;
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
        t->anchor.offsetLeft = -100.0f; t->anchor.offsetRight = -100.0f;
        t->anchor.offsetTop = -16.0f;   t->anchor.offsetBottom = -16.0f;
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
        l->anchor.offsetTop = -10.0f; l->anchor.offsetBottom = -10.0f;
        l->data.text = "Master Volume";
        l->data.textAlignH = 0;
    }
    u32 volSlider = canvas.AddElement(UIWidgetType::Slider, "VolumeSlider", panel);
    {
        auto* s = canvas.GetElement(volSlider);
        s->anchor.anchorMin = Math::Vector2(0.38f, 0.18f);
        s->anchor.anchorMax = Math::Vector2(0.92f, 0.18f);
        s->anchor.offsetLeft = 0; s->anchor.offsetRight = 0;
        s->anchor.offsetTop = -12.0f; s->anchor.offsetBottom = -12.0f;
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
        l->anchor.offsetTop = -10.0f; l->anchor.offsetBottom = -10.0f;
        l->data.text = "SFX Volume";
        l->data.textAlignH = 0;
    }
    u32 sfxSlider = canvas.AddElement(UIWidgetType::Slider, "SFXSlider", panel);
    {
        auto* s = canvas.GetElement(sfxSlider);
        s->anchor.anchorMin = Math::Vector2(0.38f, 0.30f);
        s->anchor.anchorMax = Math::Vector2(0.92f, 0.30f);
        s->anchor.offsetLeft = 0; s->anchor.offsetRight = 0;
        s->anchor.offsetTop = -12.0f; s->anchor.offsetBottom = -12.0f;
        s->data.sliderValue = 1.0f;
        s->onValueChangedEvent = "options_sfx_volume";
    }

    // Fullscreen checkbox
    u32 fullscreenCheck = canvas.AddElement(UIWidgetType::Checkbox, "Fullscreen", panel);
    {
        auto* c = canvas.GetElement(fullscreenCheck);
        c->anchor.anchorMin = Math::Vector2(0.05f, 0.45f);
        c->anchor.anchorMax = Math::Vector2(0.5f, 0.45f);
        c->anchor.offsetLeft = 0; c->anchor.offsetRight = 0;
        c->anchor.offsetTop = -12.0f; c->anchor.offsetBottom = -12.0f;
        c->data.text = "Fullscreen";
        c->data.checked = false;
        c->onValueChangedEvent = "options_fullscreen";
    }

    // VSync checkbox
    u32 vsyncCheck = canvas.AddElement(UIWidgetType::Checkbox, "VSync", panel);
    {
        auto* c = canvas.GetElement(vsyncCheck);
        c->anchor.anchorMin = Math::Vector2(0.05f, 0.55f);
        c->anchor.anchorMax = Math::Vector2(0.5f, 0.55f);
        c->anchor.offsetLeft = 0; c->anchor.offsetRight = 0;
        c->anchor.offsetTop = -12.0f; c->anchor.offsetBottom = -12.0f;
        c->data.text = "VSync";
        c->data.checked = true;
        c->onValueChangedEvent = "options_vsync";
    }

    // Shadows toggle
    u32 shadowsToggle = canvas.AddElement(UIWidgetType::Toggle, "Shadows", panel);
    {
        auto* t = canvas.GetElement(shadowsToggle);
        t->anchor.anchorMin = Math::Vector2(0.05f, 0.65f);
        t->anchor.anchorMax = Math::Vector2(0.5f, 0.65f);
        t->anchor.offsetLeft = 0; t->anchor.offsetRight = 0;
        t->anchor.offsetTop = -12.0f; t->anchor.offsetBottom = -12.0f;
        t->data.text = "Shadows";
        t->data.checked = true;
        t->onValueChangedEvent = "options_shadows";
    }

    // Back button
    u32 backBtn = canvas.AddElement(UIWidgetType::Button, "Back", panel);
    {
        auto* b = canvas.GetElement(backBtn);
        b->anchor.anchorMin = Math::Vector2(0.3f, 0.85f);
        b->anchor.anchorMax = Math::Vector2(0.7f, 0.85f);
        b->anchor.offsetLeft = 0.0f; b->anchor.offsetRight = 0.0f;
        b->anchor.offsetTop = -20.0f; b->anchor.offsetBottom = -20.0f;
        b->data.text = "Back";
        b->onClickEvent = "options_back";
    }

    // Suppress unused variable warnings
    (void)volLabel; (void)sfxLabel; (void)fullscreenCheck;
    (void)vsyncCheck; (void)shadowsToggle;

    return canvas;
}

} // namespace UITemplates

} // namespace Enjin::GUI
