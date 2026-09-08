#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/GUI/UICanvas.h"
#include "Enjin/Input/InputAction.h"

#include <string>
#include <vector>

namespace Enjin::GUI {

namespace UITemplates {

    // Create a main menu canvas with title, and New Game / Continue / Options / Quit buttons
    ENJIN_API UICanvasComponent CreateMainMenu(const std::string& gameTitle = "My Game");

    // Create a pause menu overlay with Resume / Options / Quit buttons
    ENJIN_API UICanvasComponent CreatePauseMenu();

    // --- Options menu ------------------------------------------------------
    //
    // The options menu is described as a LIST OF ROWS and built from that list,
    // rather than written out element by element with hand-computed anchor
    // fractions. Adding an option is one entry; removing one is a deletion.
    // Nothing else moves, because the rows lay themselves out and the panel
    // sizes itself to what it holds.
    //
    // A game that wants its own set starts from DefaultOptionsMenuSpec(),
    // edits the vector, and passes it back:
    //
    //     auto spec = UITemplates::DefaultOptionsMenuSpec();
    //     spec.rows.push_back(Options::Heading("Gameplay"));
    //     spec.rows.push_back(Options::Slider("Difficulty", "options_difficulty", 0.5f));
    //     auto canvas = UITemplates::CreateOptionsMenu(spec);

    struct OptionRow {
        enum class Kind : u8 {
            Heading,     // section title, no control
            Slider,      // label + slider, dispatches floatValue
            Checkbox,    // self-labelled box, dispatches boolValue
            Dropdown,    // label + named choices, dispatches intValue + stringValue
            Button,      // dispatches onClickEvent
            Spacer       // blank gap
        };

        Kind kind = Kind::Checkbox;
        std::string label;    // shown to the player
        std::string event;    // UI event dispatched on change / click
        std::string name;     // element name; empty = derived from the label

        f32  value    = 0.0f;   // Slider: initial value, in slider units
        f32  minValue = 0.0f;   // Slider range. Defaults are a 0..1 fraction, so
        f32  maxValue = 1.0f;   // a runtime that wants degrees can say 40..120.
        bool checked  = false;  // Checkbox initial state

        std::vector<std::string> options;  // Dropdown choices
        i32 selected = 0;                  // Dropdown initial index
    };

    // Row constructors, so a row is one line at the call site.
    namespace Options {
        ENJIN_API OptionRow Heading(const std::string& label);
        ENJIN_API OptionRow Slider(const std::string& label, const std::string& event,
                                   f32 value, f32 minValue = 0.0f, f32 maxValue = 1.0f);
        ENJIN_API OptionRow Checkbox(const std::string& label, const std::string& event,
                                     bool checked = false);
        ENJIN_API OptionRow Dropdown(const std::string& label, const std::string& event,
                                     std::vector<std::string> options, i32 selected = 0);
        ENJIN_API OptionRow Button(const std::string& label, const std::string& event);
        ENJIN_API OptionRow Spacer();
    }

    struct OptionsMenuSpec {
        std::string title = "Options";
        std::string backEvent = "options_back";   // empty = no Back button
        std::string backLabel = "Back";
        std::vector<OptionRow> rows;
    };

    // Every option the three runtimes actually listen for, including the full
    // accessibility set. This is the boilerplate a game gets for free.
    ENJIN_API OptionsMenuSpec DefaultOptionsMenuSpec();

    // Build a canvas from a spec. Rows stack, the panel sizes itself to them,
    // and the list scrolls when it outgrows the screen.
    ENJIN_API UICanvasComponent CreateOptionsMenu(const OptionsMenuSpec& spec);

    // The default spec, built.
    ENJIN_API UICanvasComponent CreateOptionsMenu();

    // Write a live value into a built menu, addressing the control by the event
    // it dispatches. A canvas is built once and shown many times while the
    // values live in the runtime, so without these a returning player sees the
    // factory defaults instead of their own settings. Return false when no
    // control of that kind dispatches that event.
    ENJIN_API bool SetOptionValue(UICanvasComponent& canvas, const std::string& event, f32 value);
    ENJIN_API bool SetOptionChecked(UICanvasComponent& canvas, const std::string& event, bool checked);
    ENJIN_API bool SetOptionSelected(UICanvasComponent& canvas, const std::string& event, i32 selected);

    // ---------------------------------------------------------------------
    // Controls / key bindings screen, built from the live InputActionMap.
    //
    // Generated from the action map rather than hand-written, so it cannot list
    // an action the game does not have or miss one it does. That is the same
    // reason the hint bar and the touch overlay read kActionInfo: one authored
    // list, several presentations, no chance of drift.
    //
    // Built as UICanvas rows rather than as an ImGui panel because this screen
    // has to work under a thumb. UISystem hit-tests touch against interactive
    // elements, so the rows are tappable on a phone for free; an ImGui controls
    // panel is only usable with a mouse.
    //
    // Events dispatched:
    //   controls_back, controls_reset
    //   controls_sensitivity (float), controls_invert_y (bool)
    //   controls_sprint_mode (int), controls_crouch_mode (int)
    //   controls_rebind_<index> (button) -- <index> is the action ordinal
    //
    // rebindingIndex >= 0 marks that action as awaiting a key press, so the
    // caller re-builds the canvas to show "press a key" and again once bound.
    ENJIN_API UICanvasComponent CreateControlsMenu(const InputSystem::InputActionMap& map,
                                                   i32 rebindingIndex = -1);

    // The event name a rebind row dispatches, and the inverse. Kept here so the
    // producer and the consumer cannot disagree about the format.
    ENJIN_API std::string ControlsRebindEvent(i32 actionIndex);
    ENJIN_API i32 ControlsRebindIndexFromEvent(const std::string& event);

    // Create a victory/defeat screen: dark overlay, colored message, optional
    // "Play Again" button that dispatches the "gameover_restart" UI event.
    // Spawned automatically by GameplayLoop when a GameOverComponent triggers ---
    // ONE game-over UI source rendered identically on desktop, editor play, and web.
    ENJIN_API UICanvasComponent CreateGameOverScreen(bool won, const std::string& message,
                                                     bool allowRestart = true);

} // namespace UITemplates

} // namespace Enjin::GUI
