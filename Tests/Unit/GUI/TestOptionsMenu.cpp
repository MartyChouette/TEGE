// The options menu template, and the scroll displacement it relies on.
//
// The menu is the boilerplate every game gets, so what is worth asserting is
// that it is built from its row list rather than from hand-placed anchors:
// that adding and removing rows works, that the panel sizes itself, and that
// the defaults it displays are the runtime's real defaults rather than
// plausible-looking numbers.
#include "EnjinTest.h"
#include "Enjin/GUI/UITemplates.h"
#include "Enjin/GUI/UISystem.h"
#include "Enjin/Accessibility/AccessibilitySettings.h"

#include <set>

using namespace Enjin;
using namespace Enjin::GUI;

namespace {

const UIElement* FindByEvent(const UICanvasComponent& c, const std::string& event) {
    for (const UIElement& e : c.elements) {
        if (e.onValueChangedEvent == event || e.onClickEvent == event) return &e;
    }
    return nullptr;
}

usize CountOfType(const UICanvasComponent& c, UIWidgetType t) {
    usize n = 0;
    for (const UIElement& e : c.elements) if (e.type == t) ++n;
    return n;
}

} // namespace

// ---------------------------------------------------------------------------
// The row list drives the menu
// ---------------------------------------------------------------------------

ENJIN_TEST(OptionsMenu, EveryRowInTheSpecBecomesAControl) {
    UITemplates::OptionsMenuSpec spec;
    spec.rows = {
        UITemplates::Options::Heading("Section"),
        UITemplates::Options::Slider("Loudness", "test_loudness", 0.5f),
        UITemplates::Options::Checkbox("Flag", "test_flag", true),
        UITemplates::Options::Dropdown("Pick", "test_pick", {"A", "B", "C"}, 1),
        UITemplates::Options::Button("Go", "test_go"),
    };

    UICanvasComponent c = UITemplates::CreateOptionsMenu(spec);

    ENJIN_ASSERT_TRUE(FindByEvent(c, "test_loudness") != nullptr);
    ENJIN_ASSERT_TRUE(FindByEvent(c, "test_flag") != nullptr);
    ENJIN_ASSERT_TRUE(FindByEvent(c, "test_pick") != nullptr);
    ENJIN_ASSERT_TRUE(FindByEvent(c, "test_go") != nullptr);

    ENJIN_EXPECT_EQ((int)FindByEvent(c, "test_loudness")->type, (int)UIWidgetType::Slider);
    ENJIN_EXPECT_EQ((int)FindByEvent(c, "test_flag")->type,     (int)UIWidgetType::Checkbox);
    ENJIN_EXPECT_EQ((int)FindByEvent(c, "test_pick")->type,     (int)UIWidgetType::Dropdown);
    ENJIN_EXPECT_EQ((int)FindByEvent(c, "test_go")->type,       (int)UIWidgetType::Button);
}

// The point of the row list: an option is added by adding an entry, and
// nothing else has to move. The old menu hard-coded every control's Y as an
// anchor fraction, so inserting one meant re-deriving all the others.
ENJIN_TEST(OptionsMenu, AddingARowDoesNotDisturbTheRest) {
    UITemplates::OptionsMenuSpec spec;
    spec.rows = {
        UITemplates::Options::Checkbox("First",  "test_first"),
        UITemplates::Options::Checkbox("Second", "test_second"),
    };
    UICanvasComponent before = UITemplates::CreateOptionsMenu(spec);

    spec.rows.insert(spec.rows.begin() + 1, UITemplates::Options::Slider("Middle", "test_middle", 0.25f));
    UICanvasComponent after = UITemplates::CreateOptionsMenu(spec);

    // The new control exists and the originals survive unchanged in kind.
    ENJIN_ASSERT_TRUE(FindByEvent(after, "test_middle") != nullptr);
    ENJIN_ASSERT_TRUE(FindByEvent(after, "test_first") != nullptr);
    ENJIN_ASSERT_TRUE(FindByEvent(after, "test_second") != nullptr);
    ENJIN_EXPECT_EQ((int)FindByEvent(after, "test_first")->type,
                    (int)FindByEvent(before, "test_first")->type);
}

ENJIN_TEST(OptionsMenu, RemovingARowRemovesItsControl) {
    UITemplates::OptionsMenuSpec spec = UITemplates::DefaultOptionsMenuSpec();
    ENJIN_ASSERT_TRUE(FindByEvent(UITemplates::CreateOptionsMenu(spec), "options_contrast") != nullptr);

    for (usize i = 0; i < spec.rows.size(); ++i) {
        if (spec.rows[i].event == "options_contrast") { spec.rows.erase(spec.rows.begin() + (long)i); break; }
    }

    UICanvasComponent c = UITemplates::CreateOptionsMenu(spec);
    ENJIN_EXPECT_TRUE(FindByEvent(c, "options_contrast") == nullptr);
    ENJIN_EXPECT_TRUE(FindByEvent(c, "options_brightness") != nullptr);   // neighbour intact
}

ENJIN_TEST(OptionsMenu, AnEmptySpecStillBuildsAUsableMenu) {
    UITemplates::OptionsMenuSpec spec;
    spec.rows.clear();
    UICanvasComponent c = UITemplates::CreateOptionsMenu(spec);

    // No rows, but the way out is still there -- a menu you cannot leave is
    // worse than a menu with nothing in it.
    ENJIN_EXPECT_TRUE(FindByEvent(c, "options_back") != nullptr);
}

ENJIN_TEST(OptionsMenu, BackButtonCanBeTurnedOff) {
    UITemplates::OptionsMenuSpec spec;
    spec.backEvent.clear();
    spec.rows = { UITemplates::Options::Checkbox("Flag", "test_flag") };

    UICanvasComponent c = UITemplates::CreateOptionsMenu(spec);
    ENJIN_EXPECT_TRUE(FindByEvent(c, "options_back") == nullptr);
}

// A short menu should not be a mostly empty box, and a long one must not run
// off the screen. The panel is measured from its rows.
ENJIN_TEST(OptionsMenu, PanelHeightFollowsTheRowCountAndThenStops) {
    UISystem ui;

    auto panelHeight = [&ui](usize rowCount) {
        UITemplates::OptionsMenuSpec spec;
        for (usize i = 0; i < rowCount; ++i) {
            spec.rows.push_back(UITemplates::Options::Checkbox("Row", "test_row"));
        }
        UICanvasComponent c = UITemplates::CreateOptionsMenu(spec);
        ui.ComputeLayoutForCanvas(c, 1920.0f, 1080.0f);
        for (const UIElement& e : c.elements) {
            if (e.name == "OptionsPanel") return e.computedRect.h;
        }
        return 0.0f;
    };

    const f32 few  = panelHeight(3);
    const f32 more = panelHeight(12);
    const f32 many = panelHeight(200);

    ENJIN_EXPECT_TRUE(few < more);          // grows with content
    ENJIN_EXPECT_TRUE(many <= 1080.0f);     // and stops, rather than growing forever
    ENJIN_EXPECT_TRUE(many >= more);
}

ENJIN_TEST(OptionsMenu, LongMenusGetAScrollingList) {
    UICanvasComponent c = UITemplates::CreateOptionsMenu();
    ENJIN_EXPECT_EQ(CountOfType(c, UIWidgetType::ScrollArea), (usize)1);
}

// ---------------------------------------------------------------------------
// What the default menu actually offers
// ---------------------------------------------------------------------------

// The nine colorblind modes were nine unlabelled stops on a slider on web. A
// player cannot pick "Deuteranopia" off a slider.
ENJIN_TEST(OptionsMenu, ColorblindModeIsANamedDropdownNotASlider) {
    UICanvasComponent c = UITemplates::CreateOptionsMenu();
    const UIElement* cb = FindByEvent(c, "options_colorblind");

    ENJIN_ASSERT_TRUE(cb != nullptr);
    ENJIN_ASSERT_EQ((int)cb->type, (int)UIWidgetType::Dropdown);
    ENJIN_EXPECT_EQ(cb->data.options.size(), (usize)9);
    ENJIN_EXPECT_TRUE(cb->data.options[0] == "Off");
    ENJIN_EXPECT_EQ(cb->data.selectedOption, 0);
}

// Every setting the runtime can apply should be reachable from the menu.
// Before this the exported web player applied twenty-odd settings and offered
// four of them.
ENJIN_TEST(OptionsMenu, DefaultMenuCoversTheWholeAccessibilitySet) {
    UICanvasComponent c = UITemplates::CreateOptionsMenu();

    const char* required[] = {
        "options_colorblind", "options_colorblind_strength", "options_brightness",
        "options_contrast", "options_font_scale",
        "options_dyslexia", "options_letter_spacing", "options_word_spacing",
        "options_line_spacing", "options_subtitles", "options_subtitle_speakers",
        "options_closed_captions", "options_subtitle_directions", "options_subtitle_size",
        "options_subtitle_bg_opacity",
        "options_reduced_motion", "options_disable_screen_shake",
        "options_disable_fov_effects", "options_disable_flashing",
        "options_dwell_click", "options_dwell_time", "options_switch_access",
        "options_scan_speed", "options_gaze", "options_gaze_dwell_time",
        "options_gaze_smoothing", "options_gaze_dead_zone", "options_gaze_indicator",
        "options_sticky_drag",
        "options_screen_reader", "options_audio_indicators",
    };

    for (const char* event : required) {
        ENJIN_EXPECT_TRUE(FindByEvent(c, event) != nullptr);
    }
}

// A duplicated event name means two controls silently drive one setting, and
// copy-paste is how a row list grows.
ENJIN_TEST(OptionsMenu, NoTwoRowsShareAnEvent) {
    UITemplates::OptionsMenuSpec spec = UITemplates::DefaultOptionsMenuSpec();
    std::set<std::string> seen;
    for (const UITemplates::OptionRow& row : spec.rows) {
        if (row.event.empty()) continue;
        ENJIN_EXPECT_TRUE(seen.insert(row.event).second);
    }
}

// The menu must open showing what the engine really defaults to. A control
// that opens on an invented value writes that invention back the moment the
// player touches anything near it.
ENJIN_TEST(OptionsMenu, SliderDefaultsMatchTheRuntimeSettings) {
    UICanvasComponent c = UITemplates::CreateOptionsMenu();
    const Accessibility::RuntimeAccessibilitySettings truth;

    struct { const char* event; f32 expected; } checks[] = {
        {"options_colorblind_strength", truth.colorblindStrength},
        {"options_brightness",          truth.screenBrightness},
        {"options_contrast",            truth.screenContrast},
        {"options_font_scale",          truth.fontScale},
        {"options_letter_spacing",      truth.letterSpacing},
        {"options_word_spacing",        truth.wordSpacing},
        {"options_line_spacing",        truth.lineSpacing},
        {"options_subtitle_size",       truth.subtitleFontSize},
        {"options_subtitle_bg_opacity", truth.subtitleBgOpacity},
        {"options_dwell_time",          truth.dwellClickTime},
        {"options_scan_speed",          truth.switchScanSpeed},
        {"options_gaze_dwell_time",     truth.eyeDwellTime},
        {"options_gaze_smoothing",      truth.eyeSmoothing},
        {"options_gaze_dead_zone",      truth.eyeDeadZone},
    };

    for (const auto& check : checks) {
        const UIElement* e = FindByEvent(c, check.event);
        ENJIN_ASSERT_TRUE(e != nullptr);
        ENJIN_EXPECT_FLOAT_NEAR(e->data.sliderValue, check.expected, 0.0001f);
        // A default outside its own range would clamp on the first interaction.
        ENJIN_EXPECT_TRUE(e->data.sliderValue >= e->data.sliderMin);
        ENJIN_EXPECT_TRUE(e->data.sliderValue <= e->data.sliderMax);
    }
}

ENJIN_TEST(OptionsMenu, CheckboxDefaultsMatchTheRuntimeSettings) {
    UICanvasComponent c = UITemplates::CreateOptionsMenu();
    const Accessibility::RuntimeAccessibilitySettings truth;

    struct { const char* event; bool expected; } checks[] = {
        {"options_dyslexia",            truth.dyslexiaFriendly},
        {"options_subtitles",           truth.subtitlesEnabled},
        {"options_subtitle_speakers",   truth.subtitleSpeakerNames},
        {"options_closed_captions",     truth.closedCaptionsEnabled},
        {"options_subtitle_directions", truth.subtitleDirectionIndicators},
        {"options_reduced_motion",      truth.reducedMotion},
        {"options_disable_screen_shake",truth.disableScreenShake},
        {"options_disable_fov_effects", truth.disableFOVEffects},
        {"options_disable_flashing",    truth.disableFlashingLights},
        {"options_dwell_click",         truth.dwellClickEnabled},
        {"options_switch_access",       truth.switchAccessEnabled},
        {"options_gaze",                truth.eyeTrackingEnabled},
        {"options_gaze_indicator",      truth.eyeShowGazeIndicator},
        {"options_sticky_drag",         truth.stickyDragEnabled},
        {"options_screen_reader",       truth.screenReaderEnabled},
        {"options_audio_indicators",    truth.audioIndicatorsEnabled},
    };

    for (const auto& check : checks) {
        const UIElement* e = FindByEvent(c, check.event);
        ENJIN_ASSERT_TRUE(e != nullptr);
        ENJIN_EXPECT_EQ(e->data.checked, check.expected);
    }
}

// ---------------------------------------------------------------------------
// Reading live values back in
// ---------------------------------------------------------------------------

ENJIN_TEST(OptionsMenu, SettersWriteLiveValuesIntoABuiltMenu) {
    UICanvasComponent c = UITemplates::CreateOptionsMenu();

    ENJIN_EXPECT_TRUE(UITemplates::SetOptionValue(c, "options_contrast", 1.75f));
    ENJIN_EXPECT_TRUE(UITemplates::SetOptionChecked(c, "options_subtitles", true));
    ENJIN_EXPECT_TRUE(UITemplates::SetOptionSelected(c, "options_colorblind", 3));

    ENJIN_EXPECT_FLOAT_NEAR(FindByEvent(c, "options_contrast")->data.sliderValue, 1.75f, 0.0001f);
    ENJIN_EXPECT_TRUE(FindByEvent(c, "options_subtitles")->data.checked);
    ENJIN_EXPECT_EQ(FindByEvent(c, "options_colorblind")->data.selectedOption, 3);
}

ENJIN_TEST(OptionsMenu, SettersClampRatherThanStoreOutOfRangeValues) {
    UICanvasComponent c = UITemplates::CreateOptionsMenu();

    UITemplates::SetOptionValue(c, "options_contrast", 99.0f);      // range is 0.5..2.0
    UITemplates::SetOptionSelected(c, "options_colorblind", 999);   // 9 modes

    ENJIN_EXPECT_FLOAT_NEAR(FindByEvent(c, "options_contrast")->data.sliderValue, 2.0f, 0.0001f);
    ENJIN_EXPECT_EQ(FindByEvent(c, "options_colorblind")->data.selectedOption, 8);
}

ENJIN_TEST(OptionsMenu, SettersRejectAMismatchedControlKind) {
    UICanvasComponent c = UITemplates::CreateOptionsMenu();

    // Right event, wrong kind of control: say so rather than silently doing
    // nothing, so a renamed row is caught at the call site.
    ENJIN_EXPECT_FALSE(UITemplates::SetOptionValue(c, "options_subtitles", 0.5f));
    ENJIN_EXPECT_FALSE(UITemplates::SetOptionChecked(c, "options_contrast", true));
    ENJIN_EXPECT_FALSE(UITemplates::SetOptionSelected(c, "options_contrast", 1));
    ENJIN_EXPECT_FALSE(UITemplates::SetOptionValue(c, "no_such_event", 1.0f));
}

// ---------------------------------------------------------------------------
// Scrolling: the rect that is drawn is the rect that is clicked
// ---------------------------------------------------------------------------

// A ScrollArea used to displace its children as it DREW them, while hit
// testing read the undisplaced computedRect. Scrolled content therefore
// rendered in one place and was clickable in another: the further you
// scrolled, the further a control's hit box lagged behind it. The offset is
// applied in layout now, so there is one rect and both agree.
ENJIN_TEST(OptionsMenu, ScrollingMovesTheRectThatInputAlsoReads) {
    UISystem ui;
    UICanvasComponent canvas;
    canvas.designWidth = 800.0f;
    canvas.designHeight = 600.0f;

    u32 area = canvas.AddElement(UIWidgetType::ScrollArea, "List");
    {
        auto* e = canvas.GetElement(area);
        e->anchor.anchorMin = Math::Vector2(0.0f, 0.0f);
        e->anchor.anchorMax = Math::Vector2(0.0f, 0.0f);
        e->anchor.offsetLeft = 0.0f;  e->anchor.offsetRight  = 200.0f;
        e->anchor.offsetTop  = 0.0f;  e->anchor.offsetBottom = 100.0f;
    }

    u32 row = canvas.AddElement(UIWidgetType::Button, "Row", area);
    {
        auto* e = canvas.GetElement(row);
        e->anchor.anchorMin = Math::Vector2(0.0f, 0.0f);
        e->anchor.anchorMax = Math::Vector2(0.0f, 0.0f);
        e->anchor.offsetLeft = 0.0f;  e->anchor.offsetRight  = 200.0f;
        e->anchor.offsetTop  = 50.0f; e->anchor.offsetBottom = 80.0f;
    }

    ui.ComputeLayoutForCanvas(canvas, 800.0f, 600.0f);
    const f32 unscrolled = canvas.GetElement(row)->computedRect.y;

    canvas.GetElement(area)->data.scrollOffset = 20.0f;
    ui.ComputeLayoutForCanvas(canvas, 800.0f, 600.0f);

    ENJIN_EXPECT_FLOAT_NEAR(canvas.GetElement(row)->computedRect.y, unscrolled - 20.0f, 0.001f);
}

ENJIN_TEST(OptionsMenu, ScrollOffsetsAccumulateThroughNestedLists) {
    UISystem ui;
    UICanvasComponent canvas;
    canvas.designWidth = 800.0f;
    canvas.designHeight = 600.0f;

    auto box = [&canvas](u32 id, f32 top, f32 bottom) {
        auto* e = canvas.GetElement(id);
        e->anchor.anchorMin = Math::Vector2(0.0f, 0.0f);
        e->anchor.anchorMax = Math::Vector2(0.0f, 0.0f);
        e->anchor.offsetLeft = 0.0f; e->anchor.offsetRight = 200.0f;
        e->anchor.offsetTop = top;   e->anchor.offsetBottom = bottom;
    };

    u32 outer = canvas.AddElement(UIWidgetType::ScrollArea, "Outer");
    box(outer, 0.0f, 300.0f);
    u32 inner = canvas.AddElement(UIWidgetType::ScrollArea, "Inner", outer);
    box(inner, 0.0f, 200.0f);
    u32 leaf = canvas.AddElement(UIWidgetType::Button, "Leaf", inner);
    box(leaf, 40.0f, 70.0f);

    ui.ComputeLayoutForCanvas(canvas, 800.0f, 600.0f);
    const f32 unscrolled = canvas.GetElement(leaf)->computedRect.y;

    canvas.GetElement(outer)->data.scrollOffset = 10.0f;
    canvas.GetElement(inner)->data.scrollOffset = 5.0f;
    ui.ComputeLayoutForCanvas(canvas, 800.0f, 600.0f);

    ENJIN_EXPECT_FLOAT_NEAR(canvas.GetElement(leaf)->computedRect.y, unscrolled - 15.0f, 0.001f);
}

// Nesting deeper than a scroll area holding a row holding a control is exactly
// what the menu does, and layout has to reach all of it.
ENJIN_TEST(OptionsMenu, DeeplyNestedControlsStillGetLaidOut) {
    UICanvasComponent c = UITemplates::CreateOptionsMenu();
    UISystem ui;
    ui.ComputeLayoutForCanvas(c, 1920.0f, 1080.0f);

    // Overlay > Panel > ScrollArea > Row > Slider is five deep.
    const UIElement* slider = FindByEvent(c, "options_contrast");
    ENJIN_ASSERT_TRUE(slider != nullptr);
    ENJIN_EXPECT_TRUE(slider->computedRect.w > 0.0f);
    ENJIN_EXPECT_TRUE(slider->computedRect.h > 0.0f);

    i32 depth = 0;
    u32 pid = slider->parentId;
    while (pid != 0 && depth < 64) { pid = c.GetElement(pid)->parentId; ++depth; }
    ENJIN_EXPECT_TRUE(depth >= 4);
}

// The layout nobody can see in a unit test, checked as numbers: a control that
// overlaps its own label, or sits outside the panel, is a bug that only shows
// up when somebody opens the menu.
ENJIN_TEST(OptionsMenu, RowsLayOutWithoutOverlappingOrEscapingTheList) {
    UICanvasComponent c = UITemplates::CreateOptionsMenu();
    UISystem ui;
    ui.ComputeLayoutForCanvas(c, 1920.0f, 1080.0f);

    const UIElement* list = nullptr;
    const UIElement* panel = nullptr;
    for (const UIElement& e : c.elements) {
        if (e.name == "OptionList")   list = &e;
        if (e.name == "OptionsPanel") panel = &e;
    }
    ENJIN_ASSERT_TRUE(list != nullptr);
    ENJIN_ASSERT_TRUE(panel != nullptr);

    for (const UIElement& e : c.elements) {
        if (e.parentId == 0 || &e == list || &e == panel) continue;

        // Nothing is zero-sized: a control with no width cannot be clicked.
        ENJIN_EXPECT_TRUE(e.computedRect.w > 0.0f);
        ENJIN_EXPECT_TRUE(e.computedRect.h > 0.0f);

        // Everything in the list stays within the list horizontally. (Vertical
        // overflow is the point -- that is what scrolling is for.)
        const UIElement* p = c.GetElement(e.parentId);
        const bool inList = (p == list) || (p && p->parentId == list->id);
        if (inList) {
            ENJIN_EXPECT_TRUE(e.computedRect.x >= list->computedRect.x - 0.01f);
            ENJIN_EXPECT_TRUE(e.computedRect.x + e.computedRect.w
                              <= list->computedRect.x + list->computedRect.w + 0.01f);
        }
    }

    // A labelled control must not sit on top of its own label.
    const UIElement* label = nullptr;
    for (const UIElement& e : c.elements) {
        if (e.name == "ContrastLabel" || e.name == "Contrast" + std::string("Label")) label = &e;
    }
    const UIElement* slider = FindByEvent(c, "options_contrast");
    ENJIN_ASSERT_TRUE(slider != nullptr);
    if (label) {
        ENJIN_EXPECT_TRUE(label->computedRect.x + label->computedRect.w <= slider->computedRect.x);
    }

    // The way out is inside the panel and clear of the scrolling list, so it
    // never scrolls away and never overlaps the last row.
    const UIElement* back = FindByEvent(c, "options_back");
    ENJIN_ASSERT_TRUE(back != nullptr);
    ENJIN_EXPECT_TRUE(back->computedRect.y >= list->computedRect.y + list->computedRect.h);
    ENJIN_EXPECT_TRUE(back->computedRect.y + back->computedRect.h
                      <= panel->computedRect.y + panel->computedRect.h + 0.01f);
}

ENJIN_TEST_MAIN()
