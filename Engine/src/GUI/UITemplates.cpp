#include "Enjin/GUI/UITemplates.h"

#include <algorithm>
#include <string>

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

// ============================================================================
// OPTIONS MENU
// ============================================================================
//
// Built from a row list rather than element by element. The previous version
// placed every control by hand at a literal anchor fraction, so adding an
// option meant re-deriving the Y of everything below it and the panel had a
// fixed height that the content had to be trimmed to fit. Rows now stack
// themselves, the panel sizes itself to what it holds, and a list longer than
// the screen scrolls.

namespace Options {

static OptionRow MakeRow(OptionRow::Kind kind, const std::string& label,
                         const std::string& event) {
    OptionRow row;
    row.kind = kind;
    row.label = label;
    row.event = event;
    return row;
}

OptionRow Heading(const std::string& label) {
    return MakeRow(OptionRow::Kind::Heading, label, "");
}

OptionRow Slider(const std::string& label, const std::string& event,
                 f32 value, f32 minValue, f32 maxValue) {
    OptionRow row = MakeRow(OptionRow::Kind::Slider, label, event);
    row.value = value;
    row.minValue = minValue;
    row.maxValue = maxValue;
    return row;
}

OptionRow Checkbox(const std::string& label, const std::string& event, bool checked) {
    OptionRow row = MakeRow(OptionRow::Kind::Checkbox, label, event);
    row.checked = checked;
    return row;
}

OptionRow Dropdown(const std::string& label, const std::string& event,
                   std::vector<std::string> options, i32 selected) {
    OptionRow row = MakeRow(OptionRow::Kind::Dropdown, label, event);
    row.options = std::move(options);
    row.selected = selected;
    return row;
}

OptionRow Button(const std::string& label, const std::string& event) {
    return MakeRow(OptionRow::Kind::Button, label, event);
}

OptionRow Spacer() {
    return MakeRow(OptionRow::Kind::Spacer, "", "");
}

} // namespace Options

namespace {

// Design-space metrics. The canvas design resolution is 1920x1080 and the
// whole panel scales with the screen, so these are authored once, here.
constexpr f32 kPanelWidth  = 620.0f;
constexpr f32 kPanelMaxH   = 760.0f;
constexpr f32 kTitleH      = 56.0f;
constexpr f32 kFooterH     = 58.0f;
constexpr f32 kPad         = 14.0f;
constexpr f32 kRowSpacing  = 6.0f;
constexpr f32 kRowH        = 32.0f;
constexpr f32 kHeadingH    = 38.0f;
constexpr f32 kSpacerH     = 10.0f;
constexpr f32 kLabelRight  = 0.46f;   // label occupies the left of a row
constexpr f32 kCtrlLeft    = 0.49f;   // control takes the rest

f32 RowHeight(const OptionRow& row) {
    switch (row.kind) {
        case OptionRow::Kind::Heading: return kHeadingH;
        case OptionRow::Kind::Spacer:  return kSpacerH;
        default:                       return kRowH;
    }
}

// A row box: full width of the list, fixed height. The VStack assigns the Y;
// these anchors are only what give the row its height for it to stack by.
void SetRowBox(UIElement* e, f32 height) {
    e->anchor.anchorMin  = Math::Vector2(0.0f, 0.0f);
    e->anchor.anchorMax  = Math::Vector2(1.0f, 0.0f);
    e->anchor.offsetLeft = 0.0f;  e->anchor.offsetRight  = 0.0f;
    e->anchor.offsetTop  = 0.0f;  e->anchor.offsetBottom = height;
}

// A horizontal band inside a row, vertically centred.
void SetBand(UIElement* e, f32 fracLeft, f32 fracRight, f32 height) {
    e->anchor.anchorMin  = Math::Vector2(fracLeft, 0.5f);
    e->anchor.anchorMax  = Math::Vector2(fracRight, 0.5f);
    e->anchor.offsetLeft = 0.0f;  e->anchor.offsetRight  = 0.0f;
    e->anchor.offsetTop  = -height * 0.5f;
    e->anchor.offsetBottom = height * 0.5f;
}

std::string RowName(const OptionRow& row, usize index) {
    if (!row.name.empty()) return row.name;
    if (!row.label.empty()) return row.label;
    return "Row" + std::to_string(index);
}

} // namespace

OptionsMenuSpec DefaultOptionsMenuSpec() {
    OptionsMenuSpec spec;

    // Every label here is the one the desktop menu already ships, and every
    // default is the field's real default from RuntimeAccessibilitySettings --
    // a menu that opens showing invented values is worse than one that opens
    // showing none.
    spec.rows = {
        Options::Heading("Audio"),
        Options::Slider("Master Volume", "options_master_volume", 0.8f),
        Options::Slider("SFX Volume",    "options_sfx_volume",    1.0f),
        Options::Slider("Music Volume",  "options_music_volume",  1.0f),

        Options::Heading("Display"),
        Options::Checkbox("Fullscreen", "options_fullscreen", false),
        // Field of View and Render Scale carry a 0..1 fraction rather than
        // their real units, because that is what the runtimes already map:
        // 40..120 degrees and 0.5..1.0 scale respectively.
        Options::Slider("Field of View", "options_fov",          0.375f),
        Options::Slider("Render Scale",  "options_render_scale", 1.0f),
        Options::Checkbox("Shadows", "options_shadows", true),

        Options::Heading("Vision"),
        Options::Dropdown("Colorblind Mode", "options_colorblind", {
            "Off",
            "Protanopia (no red)", "Deuteranopia (no green)", "Tritanopia (no blue)",
            "Protanomaly (weak red)", "Deuteranomaly (weak green)", "Tritanomaly (weak blue)",
            "Achromatopsia (no color)", "Achromatomaly (weak color)"
        }, 0),
        Options::Slider("Correction Strength", "options_colorblind_strength", 1.0f, 0.0f, 1.0f),
        Options::Slider("Brightness", "options_brightness", 0.0f, -0.5f, 0.5f),
        Options::Slider("Contrast",   "options_contrast",   1.0f,  0.5f, 2.0f),
        Options::Slider("UI Font Scale", "options_font_scale", 1.0f, 0.5f, 3.0f),

        Options::Heading("Text & Reading"),
        Options::Checkbox("Dyslexia-Friendly Font", "options_dyslexia", false),
        Options::Slider("Letter Spacing", "options_letter_spacing", 0.0f, 0.0f,  8.0f),
        Options::Slider("Word Spacing",   "options_word_spacing",   0.0f, 0.0f, 16.0f),
        Options::Slider("Line Spacing",   "options_line_spacing",   1.0f, 1.0f,  3.0f),
        Options::Checkbox("Subtitles", "options_subtitles", false),
        Options::Checkbox("Speaker Names", "options_subtitle_speakers", true),
        Options::Checkbox("Closed Captions (sound effects)", "options_closed_captions", false),
        Options::Checkbox("Direction Indicators", "options_subtitle_directions", false),
        Options::Slider("Subtitle Size", "options_subtitle_size", 24.0f, 16.0f, 48.0f),
        Options::Slider("Background Opacity", "options_subtitle_bg_opacity", 0.7f, 0.0f, 1.0f),

        Options::Heading("Motion"),
        Options::Checkbox("Reduced Motion", "options_reduced_motion", false),
        Options::Checkbox("Disable Screen Shake", "options_disable_screen_shake", false),
        Options::Checkbox("Disable FOV Effects", "options_disable_fov_effects", false),
        Options::Checkbox("Disable Flashing Lights", "options_disable_flashing", false),

        Options::Heading("Motor"),
        Options::Checkbox("Dwell Click (hover to click)", "options_dwell_click", false),
        Options::Slider("Dwell Time", "options_dwell_time", 1.0f, 0.3f, 3.0f),
        Options::Checkbox("Switch Access (one-button scanning)", "options_switch_access", false),
        Options::Slider("Scan Speed", "options_scan_speed", 1.5f, 0.5f, 5.0f),
        Options::Checkbox("Gaze / Head Pointing (dwell to select)", "options_gaze", false),
        Options::Slider("Gaze Dwell Time", "options_gaze_dwell_time", 1.0f, 0.3f, 3.0f),
        Options::Slider("Gaze Smoothing",  "options_gaze_smoothing",  0.3f, 0.0f, 0.9f),
        Options::Slider("Gaze Dead Zone",  "options_gaze_dead_zone",  5.0f, 0.0f, 40.0f),
        Options::Checkbox("Show Gaze Indicator", "options_gaze_indicator", true),
        Options::Checkbox("Sticky Slider Drag", "options_sticky_drag", false),
        Options::Button("Left Hand Only",  "options_preset_left_hand"),
        Options::Button("Right Hand Only", "options_preset_right_hand"),
        Options::Button("Gamepad Only",    "options_preset_gamepad"),
        Options::Button("Reset Controls to Default", "options_reset_controls"),

        Options::Heading("Audio & Communication"),
        Options::Checkbox("Screen Reader / Announcements", "options_screen_reader", false),
        Options::Checkbox("Visual Sound Indicators", "options_audio_indicators", false),
    };

    return spec;
}

UICanvasComponent CreateOptionsMenu(const OptionsMenuSpec& spec) {
    UICanvasComponent canvas;
    canvas.canvasName = "OptionsMenu";
    canvas.sortOrder = 210;
    canvas.theme = UITheme::Dark();

    // Measured before anything is placed: a short custom menu gets a short
    // panel instead of a mostly empty box, and a long one stops growing and
    // scrolls instead of running off the screen.
    f32 contentH = 0.0f;
    for (const OptionRow& row : spec.rows) contentH += RowHeight(row) + kRowSpacing;
    if (contentH > 0.0f) contentH -= kRowSpacing;

    const bool hasFooter = !spec.backEvent.empty();
    const f32 chromeH = kTitleH + (hasFooter ? kFooterH : 0.0f) + kPad * 2.0f;
    const f32 panelH  = std::min(kPanelMaxH, chromeH + contentH);
    const f32 listH   = std::max(0.0f, panelH - chromeH);

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
        o->focusable = false;
    }

    // Options panel
    u32 panel = canvas.AddElement(UIWidgetType::Panel, "OptionsPanel", overlay);
    {
        auto* p = canvas.GetElement(panel);
        p->anchor.anchorMin = Math::Vector2(0.5f, 0.5f);
        p->anchor.anchorMax = Math::Vector2(0.5f, 0.5f);
        // Unity-style anchors: edge = anchor*parent + offset, so a centered
        // box is -half / +half. Writing +half / -half yields negative width.
        p->anchor.offsetLeft = -kPanelWidth * 0.5f; p->anchor.offsetRight  = kPanelWidth * 0.5f;
        p->anchor.offsetTop  = -panelH * 0.5f;      p->anchor.offsetBottom = panelH * 0.5f;
        p->style.bgColor = Math::Vector3(0.10f, 0.10f, 0.14f);
        p->style.bgAlpha = 0.95f;
        p->style.borderRadius = 8.0f;
        p->focusable = false;
    }

    // Title
    u32 title = canvas.AddElement(UIWidgetType::Label, "Title", panel);
    {
        auto* t = canvas.GetElement(title);
        t->anchor.anchorMin = Math::Vector2(0.0f, 0.0f);
        t->anchor.anchorMax = Math::Vector2(1.0f, 0.0f);
        t->anchor.offsetLeft = 0.0f; t->anchor.offsetRight = 0.0f;
        t->anchor.offsetTop  = kPad; t->anchor.offsetBottom = kPad + kTitleH;
        t->data.text = spec.title;
        t->data.textAlignH = 1;
        t->style.fontSize = 28.0f;
        t->focusable = false;
    }

    // The scrolling list. Its VStack gives every row its Y, so a row's own
    // anchors only have to say how tall it is.
    u32 list = canvas.AddElement(UIWidgetType::ScrollArea, "OptionList", panel);
    {
        auto* s = canvas.GetElement(list);
        s->anchor.anchorMin = Math::Vector2(0.0f, 0.0f);
        s->anchor.anchorMax = Math::Vector2(1.0f, 0.0f);
        s->anchor.offsetLeft = kPad; s->anchor.offsetRight = -kPad;
        s->anchor.offsetTop  = kPad + kTitleH;
        s->anchor.offsetBottom = kPad + kTitleH + listH;
        s->style.bgAlpha = 0.0f;
        s->style.borderWidth = 0.0f;
        s->layoutMode    = UILayoutMode::VStack;
        s->layoutSpacing = kRowSpacing;
        s->layoutPaddingX = 6.0f;
        s->layoutPaddingY = 2.0f;
        s->layoutAlign   = UILayoutAlign::Stretch;
        s->focusable = false;
    }

    for (usize i = 0; i < spec.rows.size(); ++i) {
        const OptionRow& row = spec.rows[i];
        const std::string name = RowName(row, i);
        const f32 height = RowHeight(row);

        switch (row.kind) {
            case OptionRow::Kind::Spacer: {
                u32 id = canvas.AddElement(UIWidgetType::Panel, name.empty() ? "Spacer" : name, list);
                auto* e = canvas.GetElement(id);
                SetRowBox(e, height);
                e->style.bgAlpha = 0.0f;
                e->style.borderWidth = 0.0f;
                e->focusable = false;
                break;
            }
            case OptionRow::Kind::Heading: {
                u32 id = canvas.AddElement(UIWidgetType::Label, name, list);
                auto* e = canvas.GetElement(id);
                SetRowBox(e, height);
                e->data.text = row.label;
                e->data.textAlignH = 0;
                e->data.textAlignV = 2;
                e->style.fontSize = 19.0f;
                e->focusable = false;
                break;
            }
            case OptionRow::Kind::Checkbox: {
                // A checkbox draws its own label, so it is the whole row.
                u32 id = canvas.AddElement(UIWidgetType::Checkbox, name, list);
                auto* e = canvas.GetElement(id);
                SetRowBox(e, height);
                e->data.text = row.label;
                e->data.checked = row.checked;
                e->onValueChangedEvent = row.event;
                e->accessibleLabel = row.label;
                break;
            }
            case OptionRow::Kind::Button: {
                u32 id = canvas.AddElement(UIWidgetType::Button, name, list);
                auto* e = canvas.GetElement(id);
                SetRowBox(e, height);
                e->data.text = row.label;
                e->onClickEvent = row.event;
                e->accessibleLabel = row.label;
                break;
            }
            case OptionRow::Kind::Slider:
            case OptionRow::Kind::Dropdown: {
                // Label on the left, control on the right, inside a transparent
                // row. This is the nesting that used to be impossible: the row
                // sits four deep and rendering stopped at three.
                u32 rowId = canvas.AddElement(UIWidgetType::Panel, name + "Row", list);
                {
                    auto* e = canvas.GetElement(rowId);
                    SetRowBox(e, height);
                    e->style.bgAlpha = 0.0f;
                    e->style.borderWidth = 0.0f;
                    e->focusable = false;
                }

                u32 labelId = canvas.AddElement(UIWidgetType::Label, name + "Label", rowId);
                {
                    auto* e = canvas.GetElement(labelId);
                    SetBand(e, 0.0f, kLabelRight, height);
                    e->data.text = row.label;
                    e->data.textAlignH = 0;
                    e->focusable = false;
                }

                if (row.kind == OptionRow::Kind::Slider) {
                    u32 id = canvas.AddElement(UIWidgetType::Slider, name, rowId);
                    auto* e = canvas.GetElement(id);
                    SetBand(e, kCtrlLeft, 1.0f, height - 8.0f);
                    e->data.sliderMin = row.minValue;
                    e->data.sliderMax = row.maxValue;
                    e->data.sliderValue = row.value;
                    e->onValueChangedEvent = row.event;
                    e->accessibleLabel = row.label;
                } else {
                    u32 id = canvas.AddElement(UIWidgetType::Dropdown, name, rowId);
                    auto* e = canvas.GetElement(id);
                    SetBand(e, kCtrlLeft, 1.0f, height - 4.0f);
                    e->data.options = row.options;
                    e->data.selectedOption = row.selected;
                    e->onValueChangedEvent = row.event;
                    e->accessibleLabel = row.label;
                }
                break;
            }
        }
    }

    // Back button, pinned below the list rather than scrolling with it, so the
    // way out of the menu is always on screen.
    if (hasFooter) {
        u32 back = canvas.AddElement(UIWidgetType::Button, "Back", panel);
        auto* b = canvas.GetElement(back);
        b->anchor.anchorMin = Math::Vector2(0.5f, 1.0f);
        b->anchor.anchorMax = Math::Vector2(0.5f, 1.0f);
        b->anchor.offsetLeft = -80.0f; b->anchor.offsetRight  = 80.0f;
        b->anchor.offsetTop  = -kPad - 38.0f; b->anchor.offsetBottom = -kPad;
        b->data.text = spec.backLabel;
        b->onClickEvent = spec.backEvent;
        b->accessibleLabel = spec.backLabel;
    }

    return canvas;
}

UICanvasComponent CreateOptionsMenu() {
    return CreateOptionsMenu(DefaultOptionsMenuSpec());
}

// --- Reading current values back into a built menu ---------------------------
//
// A canvas is built once and shown many times, while the values it displays
// live in the runtime and change behind it (a script, a loaded settings file,
// another menu). Without this a returning player opens the menu and sees the
// factory defaults rather than the settings the game is actually running, and
// every control lies about its own state.
//
// Elements are addressed by the event they dispatch, because that is the name
// the runtime already knows -- it wired a handler to it.

namespace {

UIElement* FindByEvent(UICanvasComponent& canvas, const std::string& event) {
    if (event.empty()) return nullptr;
    for (UIElement& e : canvas.elements) {
        if (e.onValueChangedEvent == event || e.onClickEvent == event) return &e;
    }
    return nullptr;
}

} // namespace

bool SetOptionValue(UICanvasComponent& canvas, const std::string& event, f32 value) {
    UIElement* e = FindByEvent(canvas, event);
    if (!e || e->type != UIWidgetType::Slider) return false;
    e->data.sliderValue = std::max(e->data.sliderMin, std::min(e->data.sliderMax, value));
    return true;
}

bool SetOptionChecked(UICanvasComponent& canvas, const std::string& event, bool checked) {
    UIElement* e = FindByEvent(canvas, event);
    if (!e || (e->type != UIWidgetType::Checkbox && e->type != UIWidgetType::Toggle)) return false;
    e->data.checked = checked;
    return true;
}

bool SetOptionSelected(UICanvasComponent& canvas, const std::string& event, i32 selected) {
    UIElement* e = FindByEvent(canvas, event);
    if (!e || e->type != UIWidgetType::Dropdown) return false;
    if (e->data.options.empty()) return false;
    e->data.selectedOption = std::max(0, std::min(static_cast<i32>(e->data.options.size()) - 1, selected));
    return true;
}

// ---------------------------------------------------------------------------
// Controls / key bindings, generated from the live action map.
// ---------------------------------------------------------------------------
namespace {

const char* ControlsCategoryLabel(i32 category) {
    switch (static_cast<InputSystem::ActionCategory>(category)) {
        case InputSystem::ActionCategory::Movement: return "Movement";
        case InputSystem::ActionCategory::Actions:  return "Actions";
        case InputSystem::ActionCategory::Camera:   return "Camera";
        case InputSystem::ActionCategory::UI:       return "Interface";
        case InputSystem::ActionCategory::Custom:   return "Game";
        default: return "Other";
    }
}

} // namespace

std::string ControlsRebindEvent(i32 actionIndex) {
    return "controls_rebind_" + std::to_string(actionIndex);
}

i32 ControlsRebindIndexFromEvent(const std::string& event) {
    constexpr const char* kPrefix = "controls_rebind_";
    constexpr usize kPrefixLen = 16;   // strlen(kPrefix)
    if (event.size() <= kPrefixLen || event.compare(0, kPrefixLen, kPrefix) != 0) return -1;
    i32 out = 0;
    for (usize i = kPrefixLen; i < event.size(); ++i) {
        const char c = event[i];
        if (c < '0' || c > '9') return -1;      // a malformed event is not action 0
        out = out * 10 + (c - '0');
    }
    return out;
}

UICanvasComponent CreateControlsMenu(const InputSystem::InputActionMap& map, i32 rebindingIndex) {
    OptionsMenuSpec spec;
    spec.title = "Controls";
    spec.backEvent = "controls_back";

    spec.rows.push_back(Options::Heading("Look"));
    // Sensitivity is authored in 0.05..5.0 on the map; the row carries the real
    // range so the slider reads in the same units the rest of the engine uses.
    spec.rows.push_back(Options::Slider("Mouse Sensitivity", "controls_sensitivity",
                                        map.GetMouseSensitivity(), 0.05f, 5.0f));
    spec.rows.push_back(Options::Checkbox("Invert Look Y", "controls_invert_y", map.GetInvertY()));

    spec.rows.push_back(Options::Heading("Hold or Toggle"));
    spec.rows.push_back(Options::Dropdown("Sprint", "controls_sprint_mode",
                                          {"Hold", "Toggle"}, map.IsSprintToggle() ? 1 : 0));
    spec.rows.push_back(Options::Dropdown("Crouch", "controls_crouch_mode",
                                          {"Hold", "Toggle"}, map.IsCrouchToggle() ? 1 : 0));

    // One row per action, grouped by the category the action table already
    // declares. Headings are emitted lazily so a category with no actions does
    // not leave an empty title behind.
    i32 lastCategory = -1;
    const i32 count = map.GetActionCount();
    for (i32 i = 0; i < count; ++i) {
        const char* name = map.GetActionName(i);
        if (!name || !*name) continue;

        const i32 category = map.GetActionCategory(i);
        if (category != lastCategory) {
            spec.rows.push_back(Options::Heading(ControlsCategoryLabel(category)));
            lastCategory = category;
        }

        const char* binding = map.GetBindingDisplayName(i);
        std::string label = name;
        label += "     ";
        if (i == rebindingIndex) {
            // The row IS the prompt: a separate modal would have to be dismissed
            // on touch, and there is nothing to dismiss it with while every key
            // is being captured.
            label += "< press a key or mouse button - Esc cancels >";
        } else {
            label += (binding && *binding) ? binding : "unbound";
        }
        spec.rows.push_back(Options::Button(label, ControlsRebindEvent(i)));
    }

    spec.rows.push_back(Options::Spacer());
    spec.rows.push_back(Options::Button("Reset All Bindings", "controls_reset"));

    return CreateOptionsMenu(spec);
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
