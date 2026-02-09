#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Editor/UndoRedo.h"
#include <imgui.h>
#include <string>
#include <functional>
#include <unordered_map>
#include <cstring>

namespace Enjin {
namespace Editor {

// ============================================================================
// PropertyEditCommand<T> — generic undo command for inspector property edits
// ============================================================================

template<typename T>
class PropertyEditCommand : public ICommand {
public:
    using Setter = std::function<void(const T&)>;

    PropertyEditCommand(const char* desc, T oldVal, T newVal, Setter setter)
        : m_Desc(desc), m_OldValue(std::move(oldVal)), m_NewValue(std::move(newVal)),
          m_Setter(std::move(setter)) {}

    void Execute() override { m_Setter(m_NewValue); }
    void Undo() override { m_Setter(m_OldValue); }
    const char* GetDescription() const override { return m_Desc; }

    bool CanMergeWith(const ICommand* other) const override {
        auto* o = dynamic_cast<const PropertyEditCommand<T>*>(other);
        return o && std::strcmp(o->m_Desc, m_Desc) == 0;
    }
    void MergeWith(const ICommand* other) override {
        auto* o = dynamic_cast<const PropertyEditCommand<T>*>(other);
        if (o) m_NewValue = o->m_NewValue;
    }

private:
    const char* m_Desc;
    T m_OldValue;
    T m_NewValue;
    Setter m_Setter;
};

// ============================================================================
// InspectorUndo namespace — drop-in replacements for ImGui widgets
// ============================================================================
//
// Continuous widgets (DragFloat, SliderFloat, ColorEdit3):
//   Snapshot old value on IsItemActivated(), push command on IsItemDeactivatedAfterEdit().
//   This produces ONE undo entry per drag gesture.
//
// Discrete widgets (Checkbox, Combo):
//   Snapshot old value before widget, push command immediately on change.
//
// Text inputs:
//   Snapshot on IsItemActivated(), push on IsItemDeactivatedAfterEdit().

namespace InspectorUndo {

// Internal: per-widget cached "drag start" values, keyed by ImGuiID
namespace Detail {
    inline std::unordered_map<ImGuiID, f32>& FloatCache() {
        static std::unordered_map<ImGuiID, f32> s;
        return s;
    }
    inline std::unordered_map<ImGuiID, f32[3]>& Float3Cache() {
        static std::unordered_map<ImGuiID, f32[3]> s;
        return s;
    }
    inline std::unordered_map<ImGuiID, i32>& IntCache() {
        static std::unordered_map<ImGuiID, i32> s;
        return s;
    }
    inline std::unordered_map<ImGuiID, std::string>& StringCache() {
        static std::unordered_map<ImGuiID, std::string> s;
        return s;
    }
} // namespace Detail

// ---- DragFloat ----
inline bool DragFloat(UndoRedoManager& undo, const char* label, f32* v,
                      f32 speed = 1.0f, f32 min = 0.0f, f32 max = 0.0f,
                      const char* format = "%.3f", ImGuiSliderFlags flags = 0)
{
    bool changed = ImGui::DragFloat(label, v, speed, min, max, format, flags);
    ImGuiID id = ImGui::GetItemID();
    if (ImGui::IsItemActivated()) {
        Detail::FloatCache()[id] = *v;
    }
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        auto it = Detail::FloatCache().find(id);
        if (it != Detail::FloatCache().end()) {
            f32 oldVal = it->second;
            f32 newVal = *v;
            Detail::FloatCache().erase(it);
            if (oldVal != newVal) {
                f32* ptr = v;
                undo.Execute(std::make_unique<PropertyEditCommand<f32>>(
                    label, oldVal, newVal,
                    [ptr](const f32& val) { *ptr = val; }
                ));
            }
        }
    }
    return changed;
}

// ---- DragFloat3 (operates on f32[3], with setter lambda for Vector3 assignment) ----
// This version takes a setter callback for the common pattern of Vector3 conversion.
inline bool DragFloat3(UndoRedoManager& undo, const char* label, f32 v[3],
                       std::function<void(f32, f32, f32)> setter,
                       f32 speed = 0.1f, f32 min = 0.0f, f32 max = 0.0f,
                       const char* format = "%.3f", ImGuiSliderFlags flags = 0)
{
    bool changed = ImGui::DragFloat3(label, v, speed, min, max, format, flags);
    ImGuiID id = ImGui::GetItemID();
    if (ImGui::IsItemActivated()) {
        auto& cache = Detail::Float3Cache()[id];
        cache[0] = v[0]; cache[1] = v[1]; cache[2] = v[2];
    }
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        auto it = Detail::Float3Cache().find(id);
        if (it != Detail::Float3Cache().end()) {
            f32 o0 = it->second[0], o1 = it->second[1], o2 = it->second[2];
            f32 n0 = v[0], n1 = v[1], n2 = v[2];
            Detail::Float3Cache().erase(it);
            if (o0 != n0 || o1 != n1 || o2 != n2) {
                struct V3 { f32 x, y, z; };
                undo.Execute(std::make_unique<PropertyEditCommand<V3>>(
                    label, V3{o0, o1, o2}, V3{n0, n1, n2},
                    [setter](const V3& val) { setter(val.x, val.y, val.z); }
                ));
            }
        }
    }
    return changed;
}

// ---- DragFloat (pointer-based, no setter needed — writes directly to *v) ----
// For simple cases like &material->opacity where we just need pointer write-back
inline bool DragFloatP(UndoRedoManager& undo, const char* label, f32* v,
                       f32 speed = 1.0f, f32 min = 0.0f, f32 max = 0.0f,
                       const char* format = "%.3f", ImGuiSliderFlags flags = 0)
{
    return DragFloat(undo, label, v, speed, min, max, format, flags);
}

// ---- SliderFloat ----
inline bool SliderFloat(UndoRedoManager& undo, const char* label, f32* v,
                        f32 min, f32 max,
                        const char* format = "%.3f", ImGuiSliderFlags flags = 0)
{
    bool changed = ImGui::SliderFloat(label, v, min, max, format, flags);
    ImGuiID id = ImGui::GetItemID();
    if (ImGui::IsItemActivated()) {
        Detail::FloatCache()[id] = *v;
    }
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        auto it = Detail::FloatCache().find(id);
        if (it != Detail::FloatCache().end()) {
            f32 oldVal = it->second;
            f32 newVal = *v;
            Detail::FloatCache().erase(it);
            if (oldVal != newVal) {
                f32* ptr = v;
                undo.Execute(std::make_unique<PropertyEditCommand<f32>>(
                    label, oldVal, newVal,
                    [ptr](const f32& val) { *ptr = val; }
                ));
            }
        }
    }
    return changed;
}

// ---- SliderInt ----
inline bool SliderInt(UndoRedoManager& undo, const char* label, i32* v,
                      i32 min, i32 max,
                      const char* format = "%d", ImGuiSliderFlags flags = 0)
{
    bool changed = ImGui::SliderInt(label, v, min, max, format, flags);
    ImGuiID id = ImGui::GetItemID();
    if (ImGui::IsItemActivated()) {
        Detail::IntCache()[id] = *v;
    }
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        auto it = Detail::IntCache().find(id);
        if (it != Detail::IntCache().end()) {
            i32 oldVal = it->second;
            i32 newVal = *v;
            Detail::IntCache().erase(it);
            if (oldVal != newVal) {
                i32* ptr = v;
                undo.Execute(std::make_unique<PropertyEditCommand<i32>>(
                    label, oldVal, newVal,
                    [ptr](const i32& val) { *ptr = val; }
                ));
            }
        }
    }
    return changed;
}

// ---- DragInt ----
inline bool DragInt(UndoRedoManager& undo, const char* label, i32* v,
                    f32 speed = 1.0f, i32 min = 0, i32 max = 0,
                    const char* format = "%d", ImGuiSliderFlags flags = 0)
{
    bool changed = ImGui::DragInt(label, v, speed, min, max, format, flags);
    ImGuiID id = ImGui::GetItemID();
    if (ImGui::IsItemActivated()) {
        Detail::IntCache()[id] = *v;
    }
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        auto it = Detail::IntCache().find(id);
        if (it != Detail::IntCache().end()) {
            i32 oldVal = it->second;
            i32 newVal = *v;
            Detail::IntCache().erase(it);
            if (oldVal != newVal) {
                i32* ptr = v;
                undo.Execute(std::make_unique<PropertyEditCommand<i32>>(
                    label, oldVal, newVal,
                    [ptr](const i32& val) { *ptr = val; }
                ));
            }
        }
    }
    return changed;
}

// ---- ColorEdit3 (operates on f32[3], with setter for Vector3 conversion) ----
inline bool ColorEdit3(UndoRedoManager& undo, const char* label, f32 col[3],
                       std::function<void(f32, f32, f32)> setter,
                       ImGuiColorEditFlags flags = 0)
{
    bool changed = ImGui::ColorEdit3(label, col, flags);
    ImGuiID id = ImGui::GetItemID();
    if (ImGui::IsItemActivated()) {
        auto& cache = Detail::Float3Cache()[id];
        cache[0] = col[0]; cache[1] = col[1]; cache[2] = col[2];
    }
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        auto it = Detail::Float3Cache().find(id);
        if (it != Detail::Float3Cache().end()) {
            f32 o0 = it->second[0], o1 = it->second[1], o2 = it->second[2];
            f32 n0 = col[0], n1 = col[1], n2 = col[2];
            Detail::Float3Cache().erase(it);
            if (o0 != n0 || o1 != n1 || o2 != n2) {
                struct V3 { f32 x, y, z; };
                undo.Execute(std::make_unique<PropertyEditCommand<V3>>(
                    label, V3{o0, o1, o2}, V3{n0, n1, n2},
                    [setter](const V3& val) { setter(val.x, val.y, val.z); }
                ));
            }
        }
    }
    return changed;
}

// ---- Checkbox (discrete — push immediately) ----
inline bool Checkbox(UndoRedoManager& undo, const char* label, bool* v) {
    bool oldVal = *v;
    bool changed = ImGui::Checkbox(label, v);
    if (changed) {
        bool newVal = *v;
        bool* ptr = v;
        undo.Execute(std::make_unique<PropertyEditCommand<bool>>(
            label, oldVal, newVal,
            [ptr](const bool& val) { *ptr = val; }
        ));
    }
    return changed;
}

// ---- Combo (discrete — push immediately) ----
// Basic overload: int* for enum-like usage
inline bool Combo(UndoRedoManager& undo, const char* label, int* current,
                  const char* const* items, int itemsCount)
{
    int oldVal = *current;
    bool changed = ImGui::Combo(label, current, items, itemsCount);
    if (changed) {
        int newVal = *current;
        int* ptr = current;
        undo.Execute(std::make_unique<PropertyEditCommand<int>>(
            label, oldVal, newVal,
            [ptr](const int& val) { *ptr = val; }
        ));
    }
    return changed;
}

// ---- InputText (snapshot on activate, push on deactivated-after-edit) ----
// This version works with char buffers and writes back to a std::string via setter.
inline bool InputText(UndoRedoManager& undo, const char* label, char* buf, size_t bufSize,
                      std::function<void(const std::string&)> setter,
                      ImGuiInputTextFlags flags = 0)
{
    bool changed = ImGui::InputText(label, buf, bufSize, flags);
    ImGuiID id = ImGui::GetItemID();
    if (ImGui::IsItemActivated()) {
        Detail::StringCache()[id] = buf;
    }
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        auto it = Detail::StringCache().find(id);
        if (it != Detail::StringCache().end()) {
            std::string oldVal = it->second;
            std::string newVal = buf;
            Detail::StringCache().erase(it);
            if (oldVal != newVal) {
                undo.Execute(std::make_unique<PropertyEditCommand<std::string>>(
                    label, std::move(oldVal), std::move(newVal),
                    [setter](const std::string& val) { setter(val); }
                ));
            }
        }
    }
    return changed;
}

// ---- InputTextMultiline (same pattern as InputText) ----
inline bool InputTextMultiline(UndoRedoManager& undo, const char* label, char* buf, size_t bufSize,
                               std::function<void(const std::string&)> setter,
                               const ImVec2& size = ImVec2(0, 0),
                               ImGuiInputTextFlags flags = 0)
{
    bool changed = ImGui::InputTextMultiline(label, buf, bufSize, size, flags);
    ImGuiID id = ImGui::GetItemID();
    if (ImGui::IsItemActivated()) {
        Detail::StringCache()[id] = buf;
    }
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        auto it = Detail::StringCache().find(id);
        if (it != Detail::StringCache().end()) {
            std::string oldVal = it->second;
            std::string newVal = buf;
            Detail::StringCache().erase(it);
            if (oldVal != newVal) {
                undo.Execute(std::make_unique<PropertyEditCommand<std::string>>(
                    label, std::move(oldVal), std::move(newVal),
                    [setter](const std::string& val) { setter(val); }
                ));
            }
        }
    }
    return changed;
}

} // namespace InspectorUndo
} // namespace Editor
} // namespace Enjin
