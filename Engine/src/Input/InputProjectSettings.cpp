#include "Enjin/Input/InputProjectSettings.h"
#include "Enjin/Input/InputAction.h"
#include "Enjin/Logging/Log.h"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace Enjin {
namespace InputSystem {

void InputProjectSettings::ApplyTo(InputActionMap& map) const {
    for (const auto& def : customActions) {
        if (def.slot < 0 || def.slot >= static_cast<i32>(kCustomActionCount)) continue;
        GameAction action = static_cast<GameAction>(
            static_cast<i32>(GameAction::Custom0) + def.slot);
        map.SetCustomActionName(action, def.name);
        if (def.name.empty()) continue;   // unnamed slots stay hidden and unbound

        map.ClearBindings(action);
        if (def.key >= 0) {
            InputBinding b;
            b.type = BindingType::Key;
            b.code = def.key;
            map.AddBinding(action, b);
        }
        if (def.mouse >= 0) {
            InputBinding b;
            b.type = BindingType::MouseButton;
            b.code = def.mouse;
            map.AddBinding(action, b);
        }
        if (def.gamepad >= 0) {
            InputBinding b;
            b.type = BindingType::GamepadButton;
            b.code = def.gamepad;
            map.AddBinding(action, b);
        }
        map.SetActionMode(action, static_cast<ActionMode>(def.mode <= 3 ? def.mode : 2));
    }
}

std::string InputProjectSettings::ToJson() const {
    json j;
    json actions = json::array();
    for (const auto& def : customActions) {
        json a;
        a["slot"] = def.slot;
        a["name"] = def.name;
        a["key"] = def.key;
        a["mouse"] = def.mouse;
        a["gamepad"] = def.gamepad;
        a["mode"] = def.mode;
        actions.push_back(a);
    }
    j["customActions"] = actions;

    json touch;
    touch["customLayout"] = customTouchLayout;
    touch["stick"] = touchStick;
    touch["look"] = static_cast<u32>(touchLook);
    touch["buttonScale"] = touchButtonScale;
    touch["leftHanded"] = touchLeftHanded;
    json buttons = json::array();
    for (const auto& b : touchButtons) {
        json bj;
        bj["action"] = b.action;
        bj["col"] = b.col;
        bj["row"] = b.row;
        bj["size"] = b.size;
        buttons.push_back(bj);
    }
    touch["buttons"] = buttons;
    j["touch"] = touch;

    return j.dump(2);
}

bool InputProjectSettings::FromJson(const std::string& jsonStr) {
    if (jsonStr.empty()) return false;
    try {
        json j = json::parse(jsonStr);
        if (!j.is_object()) return false;

        customActions.clear();
        if (j.contains("customActions") && j["customActions"].is_array()) {
            for (const auto& a : j["customActions"]) {
                if (!a.is_object()) continue;
                CustomActionDef def;
                def.slot = a.value("slot", 0);
                def.name = a.value("name", std::string());
                def.key = a.value("key", -1);
                def.mouse = a.value("mouse", -1);
                def.gamepad = a.value("gamepad", -1);
                def.mode = a.value("mode", 2u);
                if (def.slot < 0 || def.slot >= static_cast<i32>(kCustomActionCount)) continue;
                customActions.push_back(def);
            }
        }

        touchButtons.clear();
        if (j.contains("touch") && j["touch"].is_object()) {
            const auto& t = j["touch"];
            customTouchLayout = t.value("customLayout", false);
            touchStick = t.value("stick", true);
            u32 look = t.value("look", 0u);
            touchLook = static_cast<TouchLookMode>(look <= 2 ? look : 0);
            touchButtonScale = t.value("buttonScale", 1.0f);
            if (touchButtonScale < 0.5f) touchButtonScale = 0.5f;
            if (touchButtonScale > 2.0f) touchButtonScale = 2.0f;
            touchLeftHanded = t.value("leftHanded", false);
            if (t.contains("buttons") && t["buttons"].is_array()) {
                for (const auto& bj : t["buttons"]) {
                    if (!bj.is_object()) continue;
                    TouchButtonLayout b;
                    b.action = bj.value("action", -1);
                    b.col = bj.value("col", 0.0f);
                    b.row = bj.value("row", 0.0f);
                    b.size = bj.value("size", 0.075f);
                    if (b.size < 0.02f || b.size > 0.3f) b.size = 0.075f;
                    touchButtons.push_back(b);
                }
            }
        }
        return true;
    } catch (const std::exception& e) {
        ENJIN_LOG_ERROR(Core, "Failed to parse project input settings: %s", e.what());
        return false;
    } catch (...) {
        ENJIN_LOG_ERROR(Core, "Failed to parse project input settings: unknown error");
        return false;
    }
}

} // namespace InputSystem
} // namespace Enjin
