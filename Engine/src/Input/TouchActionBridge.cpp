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
    bool s_HasFingerprint = false;

    // The actions each controller type consumes, in hint order. This ONE table
    // drives both the touch scheme and the controls hint, so a scene only ever
    // shows controls it actually has.
    const GameAction kPlatformer2D[] = { GameAction::MoveLeft, GameAction::MoveRight, GameAction::Jump };
    const GameAction kTopDown2D[]    = { GameAction::MoveForward, GameAction::MoveBack, GameAction::MoveLeft, GameAction::MoveRight,
                                         GameAction::Interact };
    const GameAction kTopDown3D[]    = { GameAction::MoveForward, GameAction::MoveBack, GameAction::MoveLeft, GameAction::MoveRight,
                                         GameAction::Jump, GameAction::Interact, GameAction::Sprint };
    const GameAction kFirstPerson[]  = { GameAction::MoveForward, GameAction::MoveBack, GameAction::MoveLeft, GameAction::MoveRight,
                                         GameAction::Attack, GameAction::Jump, GameAction::Sprint, GameAction::Interact };
    const GameAction kThirdPerson[]  = { GameAction::MoveForward, GameAction::MoveBack, GameAction::MoveLeft, GameAction::MoveRight,
                                         GameAction::Jump, GameAction::Interact, GameAction::Sprint };
    const GameAction kGeneric[]      = { GameAction::MoveForward, GameAction::MoveBack, GameAction::MoveLeft, GameAction::MoveRight,
                                         GameAction::Sprint };

    struct PresetDef { const GameAction* actions; int count; bool look; };
    const PresetDef kPresets[static_cast<int>(TouchPreset::Count)] = {
        { kPlatformer2D, 3, false },
        { kTopDown2D,    5, false },
        { kTopDown3D,    7, true  },
        { kFirstPerson,  8, true  },
        { kThirdPerson,  7, true  },
        { kGeneric,      5, true  },
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
    };

    bool IsCustom(int action) {
        return action >= static_cast<int>(GameAction::Custom0) &&
               action <  static_cast<int>(GameAction::Custom0) + static_cast<int>(kCustomActionCount);
    }

    void CopyLabel(char* dst, const char* src) {
        int i = 0;
        for (; i < 7 && src && src[i]; ++i) dst[i] = src[i];
        dst[i] = 0;
    }
}

int TouchActionKey(int action) {
    if (!s_TouchMap || action < 0 || action >= static_cast<int>(GameAction::Count))
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
    if (!s_TouchMap || action < 0 || action >= static_cast<int>(GameAction::Count))
        return nullptr;
    // A custom action's NAME is the useful glyph ("SLO-MO"), not its key.
    if (IsCustom(action) && s_TouchMap->IsActionListed(action))
        return s_TouchMap->GetActionName(action);
    // Keep the label honest: when the key resolver has nothing (gamepad-only
    // action), the button presses its static fallback key, so don't show the
    // gamepad glyph GetBindingDisplayName would fall through to.
    if (TouchActionKey(action) == Input::kTouchNoBinding) return nullptr;
    return s_TouchMap->GetBindingDisplayName(action);
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
        if (world) {
            for (ECS::Entity e : world->GetEntitiesWithComponent<ECS::ActionTriggerComponent>()) {
                auto* t = world->GetComponent<ECS::ActionTriggerComponent>(e);
                if (!t || !t->touchButton || t->action < 0) continue;
                if (t->action >= static_cast<int>(GameAction::Count)) continue;
                const char* label = s_TouchMap ? s_TouchMap->GetActionName(t->action)
                                               : GetActionInfo(static_cast<GameAction>(t->action)).touchLabel;
                AddActionButton(s, t->action, label, t->touchSize, t->touchCol, t->touchRow, 0);
            }
        }

        // Project overrides last: a hand-authored layout replaces the buttons
        // entirely, and the accessibility settings always apply.
        if (s_ProjectSettings) {
            const InputProjectSettings& p = *s_ProjectSettings;
            if (p.customTouchLayout) {
                s.buttonCount = 0;
                for (const auto& b : p.touchButtons) {
                    if (b.action < 0 || b.action >= static_cast<int>(GameAction::Count)) continue;
                    const char* label = s_TouchMap ? s_TouchMap->GetActionName(b.action)
                                                   : GetActionInfo(static_cast<GameAction>(b.action)).touchLabel;
                    AddActionButton(s, b.action, label, b.size, b.col, b.row, 0);
                }
                s.moveStick = p.touchStick;
            }
            if (p.touchLook == TouchLookMode::AlwaysOn)  s.lookRegion = true;
            if (p.touchLook == TouchLookMode::AlwaysOff) s.lookRegion = false;
            s.leftHanded = p.touchLeftHanded;
            s.buttonScale = p.touchButtonScale;
        }

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

void DrawControlsHint(f32 x0, f32 y0, f32 w, f32 h) {
    if (!s_ControlsHintEnabled) return;   // the game draws its own
    if (!s_TouchMap) return;
    if (Input::GetTouchOverlay().active) return;   // touch buttons carry their own labels
    if (w <= 0.0f || h <= 0.0f) return;

    struct Seg { std::string key; std::string verb; };
    std::vector<Seg> segs;
    std::string moveKeys;
    const PresetDef& d = Preset(s_ActivePreset);
    for (int i = 0; i < d.count; ++i) {
        int a = static_cast<int>(d.actions[i]);
        if (!s_TouchMap->IsActionListed(a)) continue;
        const ActionInfo& info = GetActionInfo(d.actions[i]);
        const char* bind = s_TouchMap->GetBindingDisplayName(a);
        if (!bind || !bind[0] || std::strcmp(bind, "None") == 0) continue;
        if (info.touch == TouchHint::Stick) {
            if (!moveKeys.empty()) moveKeys += "/";
            moveKeys += bind;
            continue;
        }
        segs.push_back({ bind, info.hintVerb });
    }
    // Movement first, then look, then the rest in preset order.
    if (d.look) segs.insert(segs.begin(), { s_ActivePreset == TouchPreset::FirstPerson ? "Mouse" : "Hold RMB", "look" });
    if (!moveKeys.empty()) segs.insert(segs.begin(), { moveKeys, "move" });
    // Named custom actions with a binding (game-specific, e.g. "B slo-mo").
    for (u32 c = 0; c < kCustomActionCount; ++c) {
        int a = static_cast<int>(GameAction::Custom0) + static_cast<int>(c);
        if (!s_TouchMap->IsActionListed(a)) continue;
        const char* bind = s_TouchMap->GetBindingDisplayName(a);
        if (!bind || !bind[0] || std::strcmp(bind, "None") == 0) continue;
        std::string verb = s_TouchMap->GetActionName(a);
        for (auto& ch : verb) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        segs.push_back({ bind, verb });
    }
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
