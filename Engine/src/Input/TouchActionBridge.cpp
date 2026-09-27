#include <cmath>
#include <algorithm>
#include "Enjin/Input/TouchActionBridge.h"
#include "Enjin/Input/InputAction.h"
#include "Enjin/Input/InputProjectSettings.h"
#include "Enjin/Platform/Input.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/ActionTrigger.h"
#include "Enjin/ECS/Components/Controllers/CharacterController.h"
#include "Enjin/GUI/UISystem.h"
#include <imgui.h>
#include <cctype>
#include <cstring>
#include <string>
#include <vector>

namespace Enjin {
namespace InputSystem {

namespace {
    InputActionMap* s_TouchMap = nullptr;
    TouchPreset s_ActivePreset = TouchPreset::Generic;
    const InputProjectSettings* s_ProjectSettings = nullptr;
    GUI::UISystem* s_UISystem = nullptr;
    u64 s_LastFingerprint = 0;
    std::vector<Input::TouchButtonDef> s_ScriptButtons;
    u32 s_PlayerHand = 0;   // 0 game default, 1 right, 2 left
    u32 s_PlayerSize = 0;   // 0 game default, 1..4 small..huge
    bool s_HasFingerprint = false;

    // The actions each controller type consumes, in hint order. This ONE table
    // drives both the touch scheme and the controls hint, so a scene only ever
    // shows controls it actually has.
    const GameAction kPlatformer2D[] = { GameAction::MoveLeft, GameAction::MoveRight, GameAction::Jump, GameAction::Sprint };
    const GameAction kTopDown2D[]    = { GameAction::MoveForward, GameAction::MoveBack, GameAction::MoveLeft, GameAction::MoveRight,
                                         GameAction::Interact };
    const GameAction kTopDown3D[]    = { GameAction::MoveForward, GameAction::MoveBack, GameAction::MoveLeft, GameAction::MoveRight,
                                         GameAction::Jump, GameAction::Interact, GameAction::Sprint };
    const GameAction kFirstPerson[]  = { GameAction::MoveForward, GameAction::MoveBack, GameAction::MoveLeft, GameAction::MoveRight,
                                         GameAction::Attack, GameAction::Jump, GameAction::Sprint, GameAction::Interact };
    const GameAction kThirdPerson[]  = { GameAction::MoveForward, GameAction::MoveBack, GameAction::MoveLeft, GameAction::MoveRight,
                                         GameAction::Jump, GameAction::Interact, GameAction::Sprint };
    // Generic is the NO-CONTROLLER case: the scene has no FirstPerson,
    // ThirdPerson, TopDown or Platformer controller, so nothing in it consumes
    // a movement action. It used to advertise all four move directions plus
    // Sprint anyway, which meant a scene that is only a camera told the player
    // to move and sprint - FoliageDemo does exactly that.
    //
    // Empty, and no look either: with no controller nothing reads the mouse,
    // so offering look told the player to do something that did nothing.
    // Anything the scene can do arrives from its own ActionTriggers or from
    // script (Touch_AddActionButton), both of which are already in the scheme
    // fingerprint and appear in the hint on their own.
    //
    // A game that drives movement from script with no controller component
    // should set its touch layout in Project Settings > Input & Touch rather
    // than relying on this guess.
    // A zero-length array is not legal C++, so the COUNT in the table below is
    // what makes this empty. The entry itself is never read.
    const GameAction kGeneric[]      = { GameAction::MoveForward };

    struct PresetDef { const GameAction* actions; int count; bool look; };
    const PresetDef kPresets[static_cast<int>(TouchPreset::Count)] = {
        { kPlatformer2D, 4, false },
        { kTopDown2D,    5, false },
        { kTopDown3D,    7, true  },
        { kFirstPerson,  8, true  },
        { kThirdPerson,  7, true  },
        { kGeneric,      0, false },   // no controller: nothing reads the mouse (IN-36)
    };

    const PresetDef& Preset(TouchPreset p) {
        int i = static_cast<int>(p);
        if (i < 0 || i >= static_cast<int>(TouchPreset::Count)) i = static_cast<int>(TouchPreset::Generic);
        return kPresets[i];
    }

    // Button cluster geometry by slot order (grows up/left from bottom-right).
    struct Slot { f32 radiusFrac, col, row; };
    const Slot kSlots[Input::kMaxTouchButtons] = {
        { 0.085f, 0, 0 }, { 0.075f, 1, 0 }, { 0.070f, 0, 1 },
        { 0.070f, 1, 1 }, { 0.065f, 2, 0 }, { 0.065f, 0, 2 },
        // Slots 7-10 continue the same cluster outward, staying clear of the
        // move stick on the left half and of the bottom-left controls hint.
        { 0.065f, 2, 1 }, { 0.060f, 1, 2 }, { 0.060f, 2, 2 }, { 0.060f, 0, 3 },
    };

    bool IsCustom(int action) {
        return s_TouchMap && s_TouchMap->IsProjectAction(action);
    }

    // Once the game has read any gameplay action, lists follow what it reads
    // (IN-37). Before that (the first frame of a scene, a headless test) the
    // preset stands in for it.
    bool FilterByUse() { return s_TouchMap && s_TouchMap->AnyGameplayActionUsed(); }
    bool Shows(int action) { return !FilterByUse() || s_TouchMap->IsActionUsed(action); }
    bool InPreset(TouchPreset p, int action) {
        const PresetDef& d = Preset(p);
        for (int i = 0; i < d.count; ++i) if (static_cast<int>(d.actions[i]) == action) return true;
        return false;
    }

    void CopyLabel(char* dst, const char* src) {
        int i = 0;
        for (; i < 7 && src && src[i]; ++i) dst[i] = src[i];
        dst[i] = 0;
    }
}

int TouchActionKey(int action) {
    if (!s_TouchMap || !s_TouchMap->IsValidAction(action))
        return Input::kTouchNoBinding;
    const ActionConfig& cfg = s_TouchMap->GetActionConfig(static_cast<GameAction>(action));
    for (const auto& b : cfg.bindings) {
        if (b.type == BindingType::Key) return b.code;
        // Core encodes a mouse button as a negative key: button i -> -(i)-1.
        if (b.type == BindingType::MouseButton) return -(b.code) - 1;
    }
    // No key/mouse binding (e.g. gamepad-only): let the touch button fall back
    // to its static key code rather than press nothing.
    return Input::kTouchNoBinding;
}

const char* TouchActionLabel(int action) {
    if (!s_TouchMap || !s_TouchMap->IsValidAction(action))
        return nullptr;
    // A custom action's NAME is the useful glyph ("SLO-MO"), not its key.
    // An unnamed one is nothing to show.
    if (IsCustom(action))
        return s_TouchMap->IsActionListed(action) ? s_TouchMap->GetActionName(action) : nullptr;
    // An engine action shows its touch label (JMP, RUN, USE, FIRE). It used to
    // show the key it emulated, so phones read "Space" and "L.Shift" (IN-28).
    // Touch now drives the action directly, so the key says nothing about
    // what the button does.
    const ActionInfo& info = GetActionInfo(static_cast<GameAction>(action));
    if (info.touchLabel && info.touchLabel[0]) return info.touchLabel;
    return s_TouchMap->GetActionName(action);
}

void SetTouchActionMap(InputActionMap* map) {
    s_TouchMap = map;
    // Function pointers match Input::ActionKeyResolver / ActionLabelResolver.
    Input::SetActionKeyResolver(map ? &TouchActionKey : nullptr);
    Input::SetActionLabelResolver(map ? &TouchActionLabel : nullptr);
    if (!map) ResetTouchPresetTracking();
}

// ---- Presets -----------------------------------------------------------------

int PresetActionCount(TouchPreset preset) { return Preset(preset).count; }

int PresetAction(TouchPreset preset, int index) {
    const PresetDef& d = Preset(preset);
    if (index < 0 || index >= d.count) return -1;
    return static_cast<int>(d.actions[index]);
}

bool PresetHasLook(TouchPreset preset) { return Preset(preset).look; }

namespace {
    // Add one action button in the next free cluster slot.
    void AddActionButton(Input::TouchScheme& s, int action, const char* label,
                         f32 radiusFrac, f32 col, f32 row, int fallbackKey) {
        if (s.buttonCount >= Input::kMaxTouchButtons) return;
        Input::TouchButtonDef b;
        b.radiusFrac = radiusFrac;
        b.colFromRight = col;
        b.rowFromBottom = row;
        b.action = action;
        b.keyCode = fallbackKey;
        CopyLabel(b.label, label ? label : "");
        s.buttons[s.buttonCount++] = b;
    }

    // The scheme for a preset, plus the scene's ActionTrigger buttons, plus any
    // project override. World may be null (script-driven Touch_UsePreset).
    void BuildScheme(TouchPreset preset, ECS::World* world) {
        const PresetDef& d = Preset(preset);
        Input::TouchScheme s;
        s.moveStick = false;
        s.lookRegion = d.look;
        for (int i = 0; i < 4; ++i) { s.stickKeys[i] = -1; s.stickActions[i] = -1; }

        for (int i = 0; i < d.count; ++i) {
            GameAction a = d.actions[i];
            if (!Shows(static_cast<int>(a))) continue;   // a button for something nothing reads
            const ActionInfo& info = GetActionInfo(a);
            switch (info.touch) {
                case TouchHint::Stick: {
                    s.moveStick = true;
                    int idx = -1;
                    if (a == GameAction::MoveLeft)    idx = 0;
                    if (a == GameAction::MoveRight)   idx = 1;
                    if (a == GameAction::MoveForward) idx = 2;
                    if (a == GameAction::MoveBack)    idx = 3;
                    if (idx >= 0) {
                        s.stickActions[idx] = static_cast<int>(a);
                        s.stickKeys[idx] = info.key1;   // fallback when no map is wired
                    }
                    break;
                }
                case TouchHint::Button: {
                    const Slot& slot = kSlots[s.buttonCount < Input::kMaxTouchButtons ? s.buttonCount : 0];
                    // Static fallback: first default key, else the default mouse
                    // button in Core's negative encoding.
                    int fallback = info.key1 >= 0 ? info.key1
                                 : (info.mouse >= 0 ? -(info.mouse) - 1 : 0);
                    AddActionButton(s, static_cast<int>(a), info.touchLabel,
                                    slot.radiusFrac, slot.col, slot.row, fallback);
                    break;
                }
                case TouchHint::Look:
                case TouchHint::NotShown:
                default:
                    break;
            }
        }

        // Scene-authored buttons: an ActionTriggerComponent asking for one. This
        // is what makes a game-specific control (bullet time, a horn, a torch)
        // appear on mobile from dropping a component in the scene, no script.
        // An action that already has a button gets no second one, and a button
        // whose spot is taken moves to the next free slot of the cluster: every
        // trigger defaulted to the same spot and they stacked (IN-34).
        auto hasAction = [&s](int action) {
            for (int i = 0; i < s.buttonCount; ++i) if (s.buttons[i].action == action) return true;
            return false;
        };
        auto spotTaken = [&s](f32 col, f32 row) {
            for (int i = 0; i < s.buttonCount; ++i) {
                if (std::fabs(s.buttons[i].colFromRight - col) < 0.5f &&
                    std::fabs(s.buttons[i].rowFromBottom - row) < 0.5f) return true;
            }
            return false;
        };
        auto freeSlot = [&](f32& col, f32& row, f32& radius) {
            if (!spotTaken(col, row)) return;
            for (const Slot& slot : kSlots) {
                if (spotTaken(slot.col, slot.row)) continue;
                col = slot.col; row = slot.row; radius = slot.radiusFrac;
                return;
            }
        };
        if (world) {
            for (ECS::Entity e : world->GetEntitiesWithComponent<ECS::ActionTriggerComponent>()) {
                auto* t = world->GetComponent<ECS::ActionTriggerComponent>(e);
                if (!t || !t->touchButton || t->action < 0) continue;
                if (s_TouchMap ? !s_TouchMap->IsValidAction(t->action)
                               : t->action >= static_cast<int>(GameAction::Count)) continue;
                if (hasAction(t->action)) continue;
                const char* label = s_TouchMap ? s_TouchMap->GetActionName(t->action)
                                               : GetActionInfo(static_cast<GameAction>(t->action)).touchLabel;
                f32 col = t->touchCol, row = t->touchRow, radius = t->touchSize;
                freeSlot(col, row, radius);
                AddActionButton(s, t->action, label, radius, col, row, 0);
            }
        }

        // Buttons scripts added, kept across rebuilds
        for (const Input::TouchButtonDef& b : s_ScriptButtons) {
            if (s.buttonCount >= Input::kMaxTouchButtons) break;
            if (b.action >= 0 && hasAction(b.action)) continue;
            s.buttons[s.buttonCount++] = b;
        }

        // Project overrides last: a hand-authored layout replaces the buttons
        // entirely, and the accessibility settings always apply.
        if (s_ProjectSettings) {
            const InputProjectSettings& p = *s_ProjectSettings;
            if (p.customTouchLayout) {
                s.buttonCount = 0;
                for (const auto& b : p.touchButtons) {
                    if (b.action < 0 || (s_TouchMap ? !s_TouchMap->IsValidAction(b.action)
                                                    : b.action >= static_cast<int>(GameAction::Count))) continue;
                    const char* label = s_TouchMap ? s_TouchMap->GetActionName(b.action)
                                                   : GetActionInfo(static_cast<GameAction>(b.action)).touchLabel;
                    AddActionButton(s, b.action, label, b.size, b.col, b.row, 0);
                }
                s.moveStick = p.touchStick;
                // A stick the preset did not fill (a no-controller game) had no
                // actions behind it and pressed nothing (IN-31). It drives the
                // four move actions, which is what a script-driven game reads.
                if (s.moveStick) {
                    const GameAction moves[4] = { GameAction::MoveLeft, GameAction::MoveRight,
                                                  GameAction::MoveForward, GameAction::MoveBack };
                    for (int i = 0; i < 4; ++i) {
                        if (s.stickActions[i] >= 0) continue;
                        s.stickActions[i] = static_cast<int>(moves[i]);
                        s.stickKeys[i] = GetActionInfo(moves[i]).key1;
                    }
                }
            }
            if (p.touchLook == TouchLookMode::AlwaysOn)  s.lookRegion = true;
            if (p.touchLook == TouchLookMode::AlwaysOff) s.lookRegion = false;
            s.leftHanded = p.touchLeftHanded;
            s.buttonScale = p.touchButtonScale;
        }
        // The player's choice last: a project sets the default, the player owns it
        if (s_PlayerHand != 0) s.leftHanded = s_PlayerHand == 2;
        if (s_PlayerSize != 0) s.buttonScale = TouchButtonScaleForSize(s_PlayerSize);

        Input::SetTouchScheme(s);
        s_ActivePreset = preset;
    }

    // Cheap fingerprint of everything BuildScheme reads, so the scheme is
    // rebuilt when the scene's triggers or the project settings change, not
    // only when the controller type does.
    u64 SchemeFingerprint(TouchPreset preset, ECS::World* world) {
        u64 h = 1469598103934665603ull;
        auto mix = [&h](u64 v) { h ^= v; h *= 1099511628211ull; };
        mix(static_cast<u64>(preset));
        mix(static_cast<u64>(s_PlayerHand) * 11u + s_PlayerSize);
        // Which of the preset's actions the game reads: the buttons follow it
        if (FilterByUse()) {
            const PresetDef& d = Preset(preset);
            for (int i = 0; i < d.count; ++i) mix(Shows(static_cast<int>(d.actions[i])) ? 3u : 5u);
        }
        if (world) {
            for (ECS::Entity e : world->GetEntitiesWithComponent<ECS::ActionTriggerComponent>()) {
                auto* t = world->GetComponent<ECS::ActionTriggerComponent>(e);
                if (!t || !t->touchButton || t->action < 0) continue;
                mix(static_cast<u64>(t->action));
                mix(static_cast<u64>(t->touchCol * 100.0f));
                mix(static_cast<u64>(t->touchRow * 100.0f));
                mix(static_cast<u64>(t->touchSize * 1000.0f));
            }
        }
        if (s_ProjectSettings) {
            const InputProjectSettings& p = *s_ProjectSettings;
            mix(p.customTouchLayout ? 1u : 2u);
            mix(p.touchStick ? 1u : 2u);
            mix(static_cast<u64>(p.touchLook));
            mix(static_cast<u64>(p.touchButtonScale * 1000.0f));
            mix(p.touchLeftHanded ? 1u : 2u);
            for (const auto& b : p.touchButtons) {
                mix(static_cast<u64>(b.action));
                mix(static_cast<u64>(b.col * 100.0f));
                mix(static_cast<u64>(b.row * 100.0f));
                mix(static_cast<u64>(b.size * 1000.0f));
            }
        }
        return h;
    }
}

void ApplyTouchPreset(TouchPreset preset) { BuildScheme(preset, nullptr); }

TouchPreset GetActiveTouchPreset() { return s_ActivePreset; }

namespace {
    // Core calls this at touchstart, before it decides whether the finger
    // belongs to the stick, a button, or the camera.
    bool UIHitTestForTouch(f32 x, f32 y) {
        return s_UISystem && s_UISystem->HitTestInteractive(x, y);
    }
}

void SetUIHitTestSystem(GUI::UISystem* ui) {
    s_UISystem = ui;
    Input::SetUIHitTestResolver(ui ? &UIHitTestForTouch : nullptr);
}

void SetTouchProjectSettings(const InputProjectSettings* settings) {
    s_ProjectSettings = settings;
    ResetTouchPresetTracking();
}

TouchPreset TouchPresetForWorld(ECS::World* world) {
    if (!world) return TouchPreset::Generic;
    using namespace ECS;
    if (!world->GetEntitiesWithComponent<FirstPersonController>().empty())  return TouchPreset::FirstPerson;
    if (!world->GetEntitiesWithComponent<ThirdPersonController>().empty())  return TouchPreset::ThirdPerson;
    if (!world->GetEntitiesWithComponent<TopDown3DController>().empty())    return TouchPreset::TopDown3D;
    if (!world->GetEntitiesWithComponent<TopDown2DController>().empty())    return TouchPreset::TopDown2D;
    if (!world->GetEntitiesWithComponent<Platformer2DController>().empty()) return TouchPreset::Platformer2D;
    // Vehicles and the surface-aligned walker steer with the move actions and
    // orbit a camera like third person. They fell through to Generic, which
    // has no stick, so a phone could not drive (IN-33). The third-person
    // buttons they do not read drop out on their own: touch lists what the
    // game reads.
    if (!world->GetEntitiesWithComponent<VehicleController>().empty() ||
        !world->GetEntitiesWithComponent<WaterVehicleController>().empty() ||
        !world->GetEntitiesWithComponent<SurfaceAlignedController>().empty()) return TouchPreset::ThirdPerson;
    return TouchPreset::Generic;
}

bool ApplyTouchPresetForWorld(ECS::World* world) {
    TouchPreset p = TouchPresetForWorld(world);
    u64 fp = SchemeFingerprint(p, world);
    if (s_HasFingerprint && fp == s_LastFingerprint) return false;
    s_LastFingerprint = fp;
    s_HasFingerprint = true;
    BuildScheme(p, world);
    return true;
}

void ResetTouchPresetTracking() { s_HasFingerprint = false; }

void AddScriptTouchButton(const Input::TouchButtonDef& button) {
    // The same action or key added again (a scene restarting its script)
    // replaces the old one rather than stacking a copy
    for (auto& b : s_ScriptButtons) {
        const bool same = button.action >= 0 ? b.action == button.action
                                             : (b.action < 0 && b.keyCode == button.keyCode);
        if (same) { b = button; ResetTouchPresetTracking(); return; }
    }
    s_ScriptButtons.push_back(button);
    ResetTouchPresetTracking();
}

f32 TouchButtonScaleForSize(u32 size) {
    switch (size) {
        case 1:  return 0.75f;
        case 2:  return 1.0f;
        case 3:  return 1.35f;
        case 4:  return 1.75f;
        default: return 0.0f;
    }
}

void SetTouchPlayerLayout(u32 hand, u32 size) {
    hand = hand <= 2 ? hand : 0;
    size = size <= 4 ? size : 0;
    if (hand == s_PlayerHand && size == s_PlayerSize) return;
    s_PlayerHand = hand;
    s_PlayerSize = size;
    ResetTouchPresetTracking();
}

void ClearScriptTouchButtons() {
    if (s_ScriptButtons.empty()) return;
    s_ScriptButtons.clear();
    ResetTouchPresetTracking();
}

// ---- Drawing -----------------------------------------------------------------

void DrawTouchOverlay() {
    auto st = Input::GetTouchOverlay();
    if (!st.active) return;
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    const ImU32 ring  = IM_COL32(255, 255, 255, 70);
    const ImU32 fill  = IM_COL32(255, 255, 255, 40);
    const ImU32 nub   = IM_COL32(255, 255, 255, 150);
    const ImU32 label = IM_COL32(255, 255, 255, 190);
    // Floating move stick (drawn only while held, and only if the active
    // scheme has one).
    if (st.showStick && st.stickHeld) {
        dl->AddCircle(ImVec2(st.stickBaseX, st.stickBaseY), st.stickRadius, ring, 32, 3.0f);
        dl->AddCircleFilled(ImVec2(st.stickNubX, st.stickNubY), st.stickRadius * 0.4f, nub);
    }
    // Anchored action buttons from the active scheme.
    for (int i = 0; i < st.buttonCount; ++i) {
        const auto& b = st.buttons[i];
        dl->AddCircleFilled(ImVec2(b.x, b.y), b.r, b.held ? nub : fill);
        dl->AddCircle(ImVec2(b.x, b.y), b.r, ring, 32, 2.5f);
        if (b.label[0]) {
            ImVec2 ts = ImGui::CalcTextSize(b.label);
            dl->AddText(ImVec2(b.x - ts.x * 0.5f, b.y - ts.y * 0.5f), label, b.label);
        }
    }
}

namespace {
// Default ON: a project that never touches this keeps the hint it has always had.
bool s_ControlsHintEnabled = true;
}

void SetControlsHintEnabled(bool enabled) { s_ControlsHintEnabled = enabled; }
bool IsControlsHintEnabled() { return s_ControlsHintEnabled; }

bool AutoTouchSlot(int index, f32& col, f32& row, f32& radiusFrac) {
    if (index < 0 || index >= static_cast<int>(Input::kMaxTouchButtons)) return false;
    col = kSlots[index].col;
    row = kSlots[index].row;
    radiusFrac = kSlots[index].radiusFrac;
    return true;
}

const char* ControlsHintLookKey(TouchPreset preset, bool mouseCaptured) {
    if (!Preset(preset).look) return nullptr;
    return mouseCaptured ? "Mouse" : "Hold RMB";
}

void DrawControlsHint(f32 x0, f32 y0, f32 w, f32 h) {
    if (!s_ControlsHintEnabled) return;   // the game draws its own
    if (!s_TouchMap) return;
    if (Input::GetTouchOverlay().active) return;   // touch buttons carry their own labels
    if (w <= 0.0f || h <= 0.0f) return;

    struct Seg { std::string key; std::string verb; };
    std::vector<Seg> segs;
    auto bindingOf = [](int a) -> const char* {
        const char* bind = s_TouchMap->GetBindingDisplayName(a);
        return (!bind || !bind[0] || std::strcmp(bind, "None") == 0) ? nullptr : bind;
    };

    // Movement as one group. "WASD" and "Arrows" read better than the four
    // keys joined with slashes.
    const int moveIds[4] = { static_cast<int>(GameAction::MoveForward), static_cast<int>(GameAction::MoveLeft),
                             static_cast<int>(GameAction::MoveBack), static_cast<int>(GameAction::MoveRight) };
    std::string moveKeys;
    {
        std::string joined, letters;
        int shown = 0;
        for (int a : moveIds) {
            if (FilterByUse() ? !s_TouchMap->IsActionUsed(a) : !InPreset(s_ActivePreset, a)) continue;
            const char* bind = bindingOf(a);
            if (!bind) continue;
            if (!joined.empty()) joined += "/";
            joined += bind;
            letters += bind;
            ++shown;
        }
        if (shown == 4 && letters == "WASD") moveKeys = "WASD";
        else if (shown == 4 && letters == "UpLeftDownRight") moveKeys = "Arrows";
        else if (shown == 2 && letters == "AD") moveKeys = "A/D";
        else moveKeys = joined;
    }

    // Then every other action the game reads (or, before it has read any, the
    // preset's), in table order. Not menu actions, not look (it has its own
    // label below), not Pause (every game has one; it is in the menus).
    auto consider = [&](int a) {
        if (!s_TouchMap->IsActionListed(a) || !Shows(a)) return;
        if (s_TouchMap->GetActionCategory(a) == static_cast<i32>(ActionCategory::UI)) return;
        if (a == static_cast<int>(GameAction::Pause)) return;
        const ActionInfo& info = GetActionInfo(static_cast<GameAction>(a));
        if (info.touch == TouchHint::Stick || info.touch == TouchHint::Look) return;
        const char* bind = bindingOf(a);
        if (!bind) return;
        std::string verb = s_TouchMap->IsProjectAction(a) ? s_TouchMap->GetActionName(a) : info.hintVerb;
        for (auto& ch : verb) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        segs.push_back({ bind, verb });
    };
    if (FilterByUse()) {
        for (int a = 0; a < s_TouchMap->GetActionCount(); ++a) consider(a);
    } else {
        const PresetDef& d = Preset(s_ActivePreset);
        for (int i = 0; i < d.count; ++i) consider(static_cast<int>(d.actions[i]));
        for (int a = static_cast<int>(kFirstProjectAction); a < s_TouchMap->GetActionCount(); ++a) consider(a);
    }
    // Movement first, then look, then the rest
    if (const char* look = ControlsHintLookKey(s_ActivePreset, Input::IsMouseCaptured())) {
        segs.insert(segs.begin(), { look, "look" });
    }
    if (!moveKeys.empty()) segs.insert(segs.begin(), { moveKeys, "move" });
    bool anyPad = false;
    for (i32 gp = 0; gp < 4; ++gp) if (Input::IsGamepadConnected(gp)) anyPad = true;
    if (anyPad) segs.push_back({ "Gamepad", "connected" });
    if (segs.empty()) return;

    ImDrawList* dl = ImGui::GetForegroundDrawList();
    const char* sep = "  \xC2\xB7  ";   // middle dot
    const f32 padX = 10.0f, padY = 6.0f, margin = 12.0f;
    ImVec2 sepSize = ImGui::CalcTextSize(sep);
    ImVec2 space = ImGui::CalcTextSize(" ");
    f32 total = 0.0f;
    for (size_t i = 0; i < segs.size(); ++i) {
        total += ImGui::CalcTextSize(segs[i].key.c_str()).x + space.x + ImGui::CalcTextSize(segs[i].verb.c_str()).x;
        if (i + 1 < segs.size()) total += sepSize.x;
    }
    f32 lineH = ImGui::GetTextLineHeight();
    ImVec2 boxMin(x0 + margin, y0 + h - margin - lineH - padY * 2.0f);
    ImVec2 boxMax(boxMin.x + total + padX * 2.0f, boxMin.y + lineH + padY * 2.0f);
    dl->AddRectFilled(boxMin, boxMax, IM_COL32(0, 0, 0, 140), 6.0f);
    const ImU32 keyCol = IM_COL32(255, 255, 255, 235);
    const ImU32 verbCol = IM_COL32(200, 200, 200, 200);
    f32 x = boxMin.x + padX, y = boxMin.y + padY;
    for (size_t i = 0; i < segs.size(); ++i) {
        dl->AddText(ImVec2(x, y), keyCol, segs[i].key.c_str());
        x += ImGui::CalcTextSize(segs[i].key.c_str()).x + space.x;
        dl->AddText(ImVec2(x, y), verbCol, segs[i].verb.c_str());
        x += ImGui::CalcTextSize(segs[i].verb.c_str()).x;
        if (i + 1 < segs.size()) {
            dl->AddText(ImVec2(x, y), verbCol, sep);
            x += sepSize.x;
        }
    }
}

} // namespace InputSystem
} // namespace Enjin
