#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include <string>

namespace Enjin {
namespace ECS {

// Wire an input action to something happening in the scene, with no script.
//
// Drop this on an entity, pick an action (including a project-defined Custom
// action such as "SLO-MO"), pick what it does. The action is then a first-class
// control everywhere: it appears in the Controls menu, in the on-screen controls
// hint with its live binding, and - because `touchButton` defaults to true - as
// an on-screen touch button on mobile, desktop --touch and the editor's touch
// simulation. Rebinding it updates all of those at once.
//
// This is the components-only path that replaces per-game script glue.

// What makes the trigger fire.
enum class ActionTriggerMode : u32 {
    OnPress = 0,   // the frame the action is pressed
    OnRelease,     // the frame it is released
    WhileHeld,     // on while the action is down, off when it comes up
    Toggle         // each press flips it on/off
};

// What the trigger does when it fires. Each effect does one obvious thing;
// EmitEvent is the general hook for anything else (visual scripts and game
// systems listen for the name).
enum class ActionEffect : u32 {
    None = 0,
    TimeScale,        // slow or speed the world (bullet time)
    ToggleVisibility, // show/hide the target entity
    EmitEvent,        // send a named entity event
    ShowSubtitle      // put a line on screen (also read by the screen reader)
};

struct ENJIN_API ActionTriggerComponent {
    // The action this responds to: a GameAction ordinal, -1 = unset. Custom
    // actions are named in Project Settings > Input & Touch.
    i32 action = -1;
    ActionTriggerMode mode = ActionTriggerMode::Toggle;
    ActionEffect effect = ActionEffect::None;

    // Which entity the effect applies to, by name. Empty = this entity.
    std::string targetEntity;

    // TimeScale: the scale while the trigger is ON (1.0 restores normal time).
    // keepPlayerSpeed leaves character controllers running at full speed, which
    // is what makes bullet time feel like bullet time rather than a pause.
    f32 timeScale = 0.25f;
    bool keepPlayerSpeed = true;

    // EmitEvent: the event name. Sent with sender = this entity.
    std::string eventName;

    // ShowSubtitle: the line shown when it turns on, and optionally off.
    std::string onText;
    std::string offText;
    f32 textDuration = 2.5f;

    // On-screen touch button for this action. The label is the action's name,
    // and the position is a grid slot growing inward/upward from the button
    // cluster corner, same units as the automatic buttons.
    bool touchButton = true;
    f32 touchCol = 0.0f;
    f32 touchRow = 2.0f;
    f32 touchSize = 0.065f;

    // Runtime state (not authored): whether the trigger is currently ON.
    bool active = false;
};

} // namespace ECS
} // namespace Enjin
