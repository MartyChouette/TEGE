#include "Enjin/GUI/UISystem.h"
#include "Enjin/GUI/UICanvas.h"
#include "Enjin/Platform/Input.h"
#include "Enjin/Logging/Log.h"

#include <imgui.h>
#include <algorithm>
#include <cmath>

namespace Enjin::GUI {

// ============================================================================
// HELPERS
// ============================================================================

static ImVec4 ResolveColor(const Math::Vector3& override_, const Math::Vector3& themeDefault, f32 alpha = 1.0f) {
    if (override_.x >= 0.0f) {
        return ImVec4(override_.x, override_.y, override_.z, alpha);
    }
    return ImVec4(themeDefault.x, themeDefault.y, themeDefault.z, alpha);
}

static f32 ResolveFloat(f32 override_, f32 themeDefault) {
    return (override_ >= 0.0f) ? override_ : themeDefault;
}

static void DrawRoundedRect(ImDrawList* dl, const UIRect& rect, ImU32 color, f32 radius) {
    dl->AddRectFilled(
        ImVec2(rect.x, rect.y),
        ImVec2(rect.x + rect.w, rect.y + rect.h),
        color, radius);
}

static void DrawRoundedRectBorder(ImDrawList* dl, const UIRect& rect, ImU32 color, f32 radius, f32 thickness) {
    dl->AddRect(
        ImVec2(rect.x, rect.y),
        ImVec2(rect.x + rect.w, rect.y + rect.h),
        color, radius, 0, thickness);
}

static void DrawCenteredText(ImDrawList* dl, const UIRect& rect, const char* text,
                             ImU32 color, u8 alignH, u8 alignV, f32 fontSize) {
    if (!text || text[0] == '\0') return;

    ImFont* font = ImGui::GetFont();
    ImVec2 textSize = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, text);

    f32 x = rect.x;
    f32 y = rect.y;

    switch (alignH) {
        case 0: x = rect.x + 4.0f; break;                             // left
        case 1: x = rect.x + (rect.w - textSize.x) * 0.5f; break;    // center
        case 2: x = rect.x + rect.w - textSize.x - 4.0f; break;      // right
    }
    switch (alignV) {
        case 0: y = rect.y + 2.0f; break;                             // top
        case 1: y = rect.y + (rect.h - textSize.y) * 0.5f; break;    // center
        case 2: y = rect.y + rect.h - textSize.y - 2.0f; break;      // bottom
    }

    dl->AddText(font, fontSize, ImVec2(x, y), color, text);
}

// ============================================================================
// UPDATE (main entry point)
// ============================================================================

void UISystem::Update(ECS::World* world, f32 vpW, f32 vpH, f32 /*deltaTime*/) {
    if (!world || vpW <= 0 || vpH <= 0) return;

    // Collect all canvas entities and sort by sortOrder
    struct CanvasEntry {
        ECS::Entity entity;
        i32 sortOrder;
    };

    std::vector<CanvasEntry> canvases;
    for (ECS::Entity entity : world->GetEntitiesWithComponent<UICanvasComponent>()) {
        auto* canvas = world->GetComponent<UICanvasComponent>(entity);
        if (!canvas || !canvas->visible) continue;
        canvases.push_back({entity, canvas->sortOrder});
    }

    std::sort(canvases.begin(), canvases.end(),
        [](const CanvasEntry& a, const CanvasEntry& b) { return a.sortOrder < b.sortOrder; });

    for (auto& entry : canvases) {
        auto* canvas = world->GetComponent<UICanvasComponent>(entry.entity);
        if (!canvas) continue;

        ComputeLayout(*canvas, vpW, vpH);
        ProcessInput(*canvas, vpW, vpH);
        RenderCanvas(*canvas);
    }
}

// ============================================================================
// LAYOUT
// ============================================================================

void UISystem::ComputeLayout(UICanvasComponent& canvas, f32 vpW, f32 vpH) {
    f32 scaleX = vpW / canvas.designWidth;
    f32 scaleY = vpH / canvas.designHeight;
    f32 scaleFactor = (canvas.scaleMode == UIScaleMode::ConstantPixelSize)
        ? 1.0f
        : std::min(scaleX, scaleY);

    UIRect rootRect = {0.0f, 0.0f, vpW, vpH};

    // Process root elements
    for (auto& element : canvas.elements) {
        if (element.parentId == 0) {
            ComputeElementRect(element, rootRect, scaleFactor);
        }
    }

    // Process children (iterate until all computed — handles any ordering)
    // Elements reference parents by ID, so we do a simple pass finding children
    // whose parent already has a computed rect
    for (auto& element : canvas.elements) {
        if (element.parentId != 0) {
            const UIElement* parent = canvas.GetElement(element.parentId);
            if (parent) {
                ComputeElementRect(element, parent->computedRect, scaleFactor);
            }
        }
    }
}

void UISystem::ComputeElementRect(UIElement& element, const UIRect& parentRect, f32 scaleFactor) {
    const auto& a = element.anchor;

    f32 resolvedLeft   = parentRect.x + a.anchorMin.x * parentRect.w + a.offsetLeft * scaleFactor;
    f32 resolvedRight  = parentRect.x + a.anchorMax.x * parentRect.w + a.offsetRight * scaleFactor;
    f32 resolvedTop    = parentRect.y + a.anchorMin.y * parentRect.h + a.offsetTop * scaleFactor;
    f32 resolvedBottom = parentRect.y + a.anchorMax.y * parentRect.h + a.offsetBottom * scaleFactor;

    // When anchorMin == anchorMax, offsets define size relative to anchor point
    // offsetLeft/Right become -halfWidth/+halfWidth style
    f32 w = resolvedRight - resolvedLeft;
    f32 h = resolvedBottom - resolvedTop;

    // Ensure non-negative dimensions
    if (w < 0.0f) {
        // Interpret as fixed-size: offsets are (-halfW, -halfW) from anchor point
        f32 cx = parentRect.x + a.anchorMin.x * parentRect.w;
        f32 cy = parentRect.y + a.anchorMin.y * parentRect.h;
        w = std::abs(a.offsetLeft * scaleFactor) + std::abs(a.offsetRight * scaleFactor);
        h = std::abs(a.offsetTop * scaleFactor) + std::abs(a.offsetBottom * scaleFactor);
        element.computedRect.x = cx - w * a.pivot.x;
        element.computedRect.y = cy - h * a.pivot.y;
        element.computedRect.w = w;
        element.computedRect.h = h;
        return;
    }

    element.computedRect.x = resolvedLeft;
    element.computedRect.y = resolvedTop;
    element.computedRect.w = w;
    element.computedRect.h = h;
}

// ============================================================================
// INPUT
// ============================================================================

void UISystem::ProcessInput(UICanvasComponent& canvas, f32 /*vpW*/, f32 /*vpH*/) {
    ImGuiIO& io = ImGui::GetIO();
    f32 mouseX = io.MousePos.x;
    f32 mouseY = io.MousePos.y;
    bool mouseDown = io.MouseDown[0];
    bool mouseClicked = ImGui::IsMouseClicked(0);

    // Iterate elements in reverse order (front-to-back for hit testing)
    for (i32 i = static_cast<i32>(canvas.elements.size()) - 1; i >= 0; --i) {
        auto& element = canvas.elements[static_cast<usize>(i)];
        if (!element.visible || !element.enabled) {
            element.interaction = {};
            continue;
        }

        bool hit = element.computedRect.Contains(mouseX, mouseY);
        element.interaction.hovered = hit;
        element.interaction.pressed = hit && mouseDown;

        if (!hit) continue;

        // Button click
        if (element.type == UIWidgetType::Button && mouseClicked) {
            if (!element.onClickEvent.empty()) {
                UIEventData event;
                event.elementId = element.id;
                event.eventName = element.onClickEvent;
                event.stringValue = element.data.text;
                m_EventBus.Dispatch(event);
            }
        }

        // Checkbox / Toggle click
        if ((element.type == UIWidgetType::Checkbox || element.type == UIWidgetType::Toggle) && mouseClicked) {
            element.data.checked = !element.data.checked;
            if (!element.onValueChangedEvent.empty()) {
                UIEventData event;
                event.elementId = element.id;
                event.eventName = element.onValueChangedEvent;
                event.boolValue = element.data.checked;
                m_EventBus.Dispatch(event);
            }
        }

        // Slider drag
        if (element.type == UIWidgetType::Slider && mouseDown) {
            element.interaction.active = true;
            f32 relX = (mouseX - element.computedRect.x) / element.computedRect.w;
            relX = std::max(0.0f, std::min(1.0f, relX));
            f32 newValue = element.data.sliderMin + relX * (element.data.sliderMax - element.data.sliderMin);
            if (newValue != element.data.sliderValue) {
                element.data.sliderValue = newValue;
                if (!element.onValueChangedEvent.empty()) {
                    UIEventData event;
                    event.elementId = element.id;
                    event.eventName = element.onValueChangedEvent;
                    event.floatValue = newValue;
                    m_EventBus.Dispatch(event);
                }
            }
        }
    }
}

// ============================================================================
// NINE-SLICE
// ============================================================================

const NineSliceConfig& UISystem::ResolveNineSlice(const UIElement& element, const UITheme& theme) const {
    if (element.style.nineSlice.IsActive()) return element.style.nineSlice;
    if (element.type == UIWidgetType::Panel) return theme.panelNineSlice;
    if (element.type == UIWidgetType::Button) return theme.buttonNineSlice;
    static NineSliceConfig empty;
    return empty;
}

void UISystem::DrawNineSlice(ImDrawList* dl, const UIRect& rect, void* texId,
                              u32 texW, u32 texH, const NineSliceConfig& config, u32 tint) {
    if (!texId || texW == 0 || texH == 0) return;

    const f32 tw = static_cast<f32>(texW);
    const f32 th = static_cast<f32>(texH);

    // UV coordinates (normalized)
    const f32 u0 = 0.0f;
    const f32 u1 = config.borderLeft / tw;
    const f32 u2 = 1.0f - config.borderRight / tw;
    const f32 u3 = 1.0f;
    const f32 v0 = 0.0f;
    const f32 v1 = config.borderTop / th;
    const f32 v2 = 1.0f - config.borderBottom / th;
    const f32 v3 = 1.0f;

    // Screen positions
    const f32 x0 = rect.x;
    const f32 x1 = rect.x + config.borderLeft;
    const f32 x2 = rect.x + rect.w - config.borderRight;
    const f32 x3 = rect.x + rect.w;
    const f32 y0 = rect.y;
    const f32 y1 = rect.y + config.borderTop;
    const f32 y2 = rect.y + rect.h - config.borderBottom;
    const f32 y3 = rect.y + rect.h;

    auto texID = static_cast<ImTextureID>(reinterpret_cast<uintptr_t>(texId));

    // Top-left corner
    dl->AddImage(texID, ImVec2(x0, y0), ImVec2(x1, y1), ImVec2(u0, v0), ImVec2(u1, v1), tint);
    // Top edge
    dl->AddImage(texID, ImVec2(x1, y0), ImVec2(x2, y1), ImVec2(u1, v0), ImVec2(u2, v1), tint);
    // Top-right corner
    dl->AddImage(texID, ImVec2(x2, y0), ImVec2(x3, y1), ImVec2(u2, v0), ImVec2(u3, v1), tint);
    // Left edge
    dl->AddImage(texID, ImVec2(x0, y1), ImVec2(x1, y2), ImVec2(u0, v1), ImVec2(u1, v2), tint);
    // Center
    dl->AddImage(texID, ImVec2(x1, y1), ImVec2(x2, y2), ImVec2(u1, v1), ImVec2(u2, v2), tint);
    // Right edge
    dl->AddImage(texID, ImVec2(x2, y1), ImVec2(x3, y2), ImVec2(u2, v1), ImVec2(u3, v2), tint);
    // Bottom-left corner
    dl->AddImage(texID, ImVec2(x0, y2), ImVec2(x1, y3), ImVec2(u0, v2), ImVec2(u1, v3), tint);
    // Bottom edge
    dl->AddImage(texID, ImVec2(x1, y2), ImVec2(x2, y3), ImVec2(u1, v2), ImVec2(u2, v3), tint);
    // Bottom-right corner
    dl->AddImage(texID, ImVec2(x2, y2), ImVec2(x3, y3), ImVec2(u2, v2), ImVec2(u3, v3), tint);
}

// ============================================================================
// RENDERING
// ============================================================================

void UISystem::RenderCanvas(const UICanvasComponent& canvas) {
    // Render root elements, then their children recursively
    for (const auto& element : canvas.elements) {
        if (element.parentId == 0 && element.visible) {
            RenderElement(element, canvas.theme);

            // Render children
            for (u32 childId : element.childIds) {
                const UIElement* child = canvas.GetElement(childId);
                if (child && child->visible) {
                    RenderElement(*child, canvas.theme);
                    // One level of nesting for children's children
                    for (u32 grandchildId : child->childIds) {
                        const UIElement* grandchild = canvas.GetElement(grandchildId);
                        if (grandchild && grandchild->visible) {
                            RenderElement(*grandchild, canvas.theme);
                        }
                    }
                }
            }
        }
    }
}

void UISystem::RenderElement(const UIElement& element, const UITheme& theme) {
    switch (element.type) {
        case UIWidgetType::Panel:       RenderPanel(element, theme); break;
        case UIWidgetType::Button:      RenderButton(element, theme); break;
        case UIWidgetType::Label:       RenderLabel(element, theme); break;
        case UIWidgetType::Image:       RenderImage(element, theme); break;
        case UIWidgetType::ProgressBar: RenderProgressBar(element, theme); break;
        case UIWidgetType::Slider:      RenderSlider(element, theme); break;
        case UIWidgetType::Checkbox:    RenderCheckbox(element, theme); break;
        case UIWidgetType::Toggle:      RenderToggle(element, theme); break;
        default:                        RenderPlaceholder(element, theme); break;
    }
}

void UISystem::RenderPanel(const UIElement& element, const UITheme& theme) {
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    f32 radius = ResolveFloat(element.style.borderRadius, theme.borderRadius);
    f32 alpha  = ResolveFloat(element.style.bgAlpha, theme.bgAlpha);

    // Try nine-slice rendering first
    const auto& ns = ResolveNineSlice(element, theme);
    if (ns.IsActive() && m_TextureResolver) {
        u32 texW = 0, texH = 0;
        void* texId = m_TextureResolver(ns.texturePath, texW, texH);
        if (texId) {
            u8 a8 = static_cast<u8>(alpha * 255.0f);
            ImU32 tint = IM_COL32(255, 255, 255, a8);
            DrawNineSlice(dl, element.computedRect, texId, texW, texH, ns, tint);

            // Border still applies on top of nine-slice
            f32 borderW = ResolveFloat(element.style.borderWidth, theme.borderWidth);
            if (borderW > 0.0f) {
                ImVec4 borderColor = ResolveColor(element.style.borderColor, theme.inputBorder, alpha);
                DrawRoundedRectBorder(dl, element.computedRect, ImGui::ColorConvertFloat4ToU32(borderColor), radius, borderW);
            }
            return;
        }
    }

    // Flat-color fallback
    ImVec4 bgColor = ResolveColor(element.style.bgColor, theme.surface, alpha);
    DrawRoundedRect(dl, element.computedRect, ImGui::ColorConvertFloat4ToU32(bgColor), radius);

    f32 borderW = ResolveFloat(element.style.borderWidth, theme.borderWidth);
    if (borderW > 0.0f) {
        ImVec4 borderColor = ResolveColor(element.style.borderColor, theme.inputBorder, alpha);
        DrawRoundedRectBorder(dl, element.computedRect, ImGui::ColorConvertFloat4ToU32(borderColor), radius, borderW);
    }
}

void UISystem::RenderButton(const UIElement& element, const UITheme& theme) {
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    f32 radius = ResolveFloat(element.style.borderRadius, theme.borderRadius);

    // Try nine-slice rendering first
    const auto& ns = ResolveNineSlice(element, theme);
    bool usedNineSlice = false;
    if (ns.IsActive() && m_TextureResolver) {
        u32 texW = 0, texH = 0;
        void* texId = m_TextureResolver(ns.texturePath, texW, texH);
        if (texId) {
            // Tint based on interaction state
            ImU32 tint = IM_COL32(255, 255, 255, 255);
            if (!element.enabled) {
                tint = IM_COL32(128, 128, 128, 255);
            } else if (element.interaction.pressed) {
                tint = IM_COL32(180, 180, 180, 255);
            } else if (element.interaction.hovered) {
                tint = IM_COL32(230, 230, 230, 255);
            }
            DrawNineSlice(dl, element.computedRect, texId, texW, texH, ns, tint);
            usedNineSlice = true;
        }
    }

    if (!usedNineSlice) {
        // Flat-color fallback
        Math::Vector3 bgDefault = theme.buttonDefault;
        if (element.style.HasBgColor()) bgDefault = element.style.bgColor;

        Math::Vector3 bgColor = bgDefault;
        if (!element.enabled) {
            bgColor = theme.buttonDisabled;
        } else if (element.interaction.pressed) {
            bgColor = theme.buttonPressed;
        } else if (element.interaction.hovered) {
            bgColor = theme.buttonHovered;
        }

        DrawRoundedRect(dl, element.computedRect,
            ImGui::ColorConvertFloat4ToU32(ImVec4(bgColor.x, bgColor.y, bgColor.z, 1.0f)), radius);
    }

    f32 borderW = ResolveFloat(element.style.borderWidth, theme.borderWidth);
    if (borderW > 0.0f) {
        ImVec4 borderColor = ResolveColor(element.style.borderColor, theme.primary, 0.5f);
        DrawRoundedRectBorder(dl, element.computedRect, ImGui::ColorConvertFloat4ToU32(borderColor), radius, borderW);
    }

    // Button text
    f32 fontSize = ResolveFloat(element.style.fontSize, theme.fontSizeBody);
    ImVec4 textColor = ResolveColor(element.style.textColor, theme.textPrimary, 1.0f);
    if (!element.enabled) textColor = ResolveColor(element.style.textColor, theme.textDisabled, 1.0f);

    DrawCenteredText(dl, element.computedRect, element.data.text.c_str(),
        ImGui::ColorConvertFloat4ToU32(textColor),
        element.data.textAlignH, element.data.textAlignV, fontSize);
}

void UISystem::RenderLabel(const UIElement& element, const UITheme& theme) {
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    f32 fontSize = ResolveFloat(element.style.fontSize, theme.fontSizeBody);
    ImVec4 textColor = ResolveColor(element.style.textColor, theme.textPrimary, 1.0f);

    DrawCenteredText(dl, element.computedRect, element.data.text.c_str(),
        ImGui::ColorConvertFloat4ToU32(textColor),
        element.data.textAlignH, element.data.textAlignV, fontSize);
}

void UISystem::RenderImage(const UIElement& element, const UITheme& theme) {
    (void)theme;
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    f32 alpha = element.data.imageAlpha;
    auto& tint = element.data.imageTint;

    if (!element.data.imagePath.empty() && m_TextureResolver) {
        u32 texW = 0, texH = 0;
        void* texId = m_TextureResolver(element.data.imagePath, texW, texH);
        if (texId) {
            u8 r = static_cast<u8>(tint.x * 255.0f);
            u8 g = static_cast<u8>(tint.y * 255.0f);
            u8 b = static_cast<u8>(tint.z * 255.0f);
            u8 a = static_cast<u8>(alpha * 255.0f);
            ImU32 tintCol = IM_COL32(r, g, b, a);
            ImVec2 pMin(element.computedRect.x, element.computedRect.y);
            ImVec2 pMax(element.computedRect.x + element.computedRect.w,
                        element.computedRect.y + element.computedRect.h);
            dl->AddImage(reinterpret_cast<ImTextureID>(texId), pMin, pMax,
                         ImVec2(0, 0), ImVec2(1, 1), tintCol);
            return;
        }
    }

    // Fallback: tinted rect with path label
    ImU32 color = ImGui::ColorConvertFloat4ToU32(ImVec4(tint.x * 0.3f, tint.y * 0.3f, tint.z * 0.3f, alpha * 0.5f));
    DrawRoundedRect(dl, element.computedRect, color, 0.0f);
    if (!element.data.imagePath.empty()) {
        ImFont* font = ImGui::GetFont();
        std::string label = "[" + element.data.imagePath + "]";
        ImVec2 textSize = font->CalcTextSizeA(12.0f, FLT_MAX, 0.0f, label.c_str());
        f32 tx = element.computedRect.x + (element.computedRect.w - textSize.x) * 0.5f;
        f32 ty = element.computedRect.y + (element.computedRect.h - textSize.y) * 0.5f;
        dl->AddText(font, 12.0f, ImVec2(tx, ty),
            ImGui::ColorConvertFloat4ToU32(ImVec4(0.7f, 0.7f, 0.7f, alpha)), label.c_str());
    }
}

void UISystem::RenderProgressBar(const UIElement& element, const UITheme& theme) {
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    f32 radius = ResolveFloat(element.style.borderRadius, theme.borderRadius);

    // Track background
    ImVec4 trackColor = ResolveColor(element.style.bgColor, theme.sliderTrack, 1.0f);
    DrawRoundedRect(dl, element.computedRect, ImGui::ColorConvertFloat4ToU32(trackColor), radius);

    // Fill
    f32 fillPct = std::max(0.0f, std::min(1.0f, element.data.progressValue));
    if (fillPct > 0.001f) {
        UIRect fillRect = element.computedRect;
        fillRect.w = element.computedRect.w * fillPct;

        Math::Vector3 fillDefault = theme.sliderFill;
        if (element.data.progressFillColor.x >= 0.0f) fillDefault = element.data.progressFillColor;
        ImVec4 fillColor = ImVec4(fillDefault.x, fillDefault.y, fillDefault.z, 1.0f);

        DrawRoundedRect(dl, fillRect, ImGui::ColorConvertFloat4ToU32(fillColor), radius);
    }

    // Border
    f32 borderW = ResolveFloat(element.style.borderWidth, theme.borderWidth);
    if (borderW > 0.0f) {
        ImVec4 borderColor = ResolveColor(element.style.borderColor, theme.inputBorder, 0.5f);
        DrawRoundedRectBorder(dl, element.computedRect, ImGui::ColorConvertFloat4ToU32(borderColor), radius, borderW);
    }
}

void UISystem::RenderSlider(const UIElement& element, const UITheme& theme) {
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    f32 radius = ResolveFloat(element.style.borderRadius, theme.borderRadius);

    // Track
    UIRect trackRect = element.computedRect;
    f32 trackH = std::max(4.0f, element.computedRect.h * 0.3f);
    trackRect.y = element.computedRect.y + (element.computedRect.h - trackH) * 0.5f;
    trackRect.h = trackH;

    ImVec4 trackColor = ResolveColor(element.style.bgColor, theme.sliderTrack, 1.0f);
    DrawRoundedRect(dl, trackRect, ImGui::ColorConvertFloat4ToU32(trackColor), radius);

    // Fill portion
    f32 range = element.data.sliderMax - element.data.sliderMin;
    f32 pct = (range > 0.0f) ? (element.data.sliderValue - element.data.sliderMin) / range : 0.0f;
    pct = std::max(0.0f, std::min(1.0f, pct));

    UIRect fillRect = trackRect;
    fillRect.w = trackRect.w * pct;
    ImVec4 fillColor = ImVec4(theme.sliderFill.x, theme.sliderFill.y, theme.sliderFill.z, 1.0f);
    DrawRoundedRect(dl, fillRect, ImGui::ColorConvertFloat4ToU32(fillColor), radius);

    // Thumb
    f32 thumbRadius = element.computedRect.h * 0.4f;
    f32 thumbX = element.computedRect.x + pct * element.computedRect.w;
    f32 thumbY = element.computedRect.y + element.computedRect.h * 0.5f;

    ImVec4 thumbColor = ImVec4(theme.sliderThumb.x, theme.sliderThumb.y, theme.sliderThumb.z, 1.0f);
    if (element.interaction.active || element.interaction.pressed) {
        thumbColor = ImVec4(theme.primary.x, theme.primary.y, theme.primary.z, 1.0f);
    }
    dl->AddCircleFilled(ImVec2(thumbX, thumbY), thumbRadius, ImGui::ColorConvertFloat4ToU32(thumbColor));
}

void UISystem::RenderCheckbox(const UIElement& element, const UITheme& theme) {
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    f32 boxSize = std::min(element.computedRect.w, element.computedRect.h);

    UIRect boxRect;
    boxRect.x = element.computedRect.x;
    boxRect.y = element.computedRect.y + (element.computedRect.h - boxSize) * 0.5f;
    boxRect.w = boxSize;
    boxRect.h = boxSize;

    f32 radius = ResolveFloat(element.style.borderRadius, theme.borderRadius);

    // Background
    Math::Vector3 bgColor = element.data.checked ? theme.checkboxChecked : theme.checkboxBg;
    if (element.interaction.hovered && !element.data.checked) {
        bgColor = Math::Vector3(bgColor.x + 0.05f, bgColor.y + 0.05f, bgColor.z + 0.05f);
    }
    DrawRoundedRect(dl, boxRect, ImGui::ColorConvertFloat4ToU32(ImVec4(bgColor.x, bgColor.y, bgColor.z, 1.0f)), radius);

    // Border
    f32 borderW = ResolveFloat(element.style.borderWidth, theme.borderWidth);
    ImVec4 borderColor = ResolveColor(element.style.borderColor, theme.inputBorder, 0.8f);
    DrawRoundedRectBorder(dl, boxRect, ImGui::ColorConvertFloat4ToU32(borderColor), radius, borderW);

    // Checkmark
    if (element.data.checked) {
        ImVec4 checkColor = ImVec4(theme.textPrimary.x, theme.textPrimary.y, theme.textPrimary.z, 1.0f);
        ImU32 cc = ImGui::ColorConvertFloat4ToU32(checkColor);
        f32 pad = boxSize * 0.25f;
        f32 cx = boxRect.x + pad;
        f32 cy = boxRect.y + boxSize * 0.5f;
        dl->AddLine(ImVec2(cx, cy), ImVec2(cx + boxSize * 0.15f, cy + boxSize * 0.2f), cc, 2.0f);
        dl->AddLine(ImVec2(cx + boxSize * 0.15f, cy + boxSize * 0.2f),
                    ImVec2(cx + boxSize * 0.4f, cy - boxSize * 0.2f), cc, 2.0f);
    }

    // Label text next to checkbox
    if (!element.data.text.empty()) {
        f32 fontSize = ResolveFloat(element.style.fontSize, theme.fontSizeBody);
        ImVec4 textColor = ResolveColor(element.style.textColor, theme.textPrimary, 1.0f);
        UIRect labelRect = element.computedRect;
        labelRect.x = boxRect.x + boxSize + 6.0f;
        labelRect.w = element.computedRect.w - boxSize - 6.0f;
        DrawCenteredText(dl, labelRect, element.data.text.c_str(),
            ImGui::ColorConvertFloat4ToU32(textColor), 0, 1, fontSize);
    }
}

void UISystem::RenderToggle(const UIElement& element, const UITheme& theme) {
    ImDrawList* dl = ImGui::GetForegroundDrawList();

    f32 toggleH = std::min(element.computedRect.h, 24.0f);
    f32 toggleW = toggleH * 1.8f;

    UIRect toggleRect;
    toggleRect.x = element.computedRect.x;
    toggleRect.y = element.computedRect.y + (element.computedRect.h - toggleH) * 0.5f;
    toggleRect.w = toggleW;
    toggleRect.h = toggleH;

    f32 radius = toggleH * 0.5f;

    // Background track
    Math::Vector3 bgColor = element.data.checked ? theme.toggleOnBg : theme.toggleOffBg;
    DrawRoundedRect(dl, toggleRect, ImGui::ColorConvertFloat4ToU32(ImVec4(bgColor.x, bgColor.y, bgColor.z, 1.0f)), radius);

    // Knob
    f32 knobRadius = (toggleH - 4.0f) * 0.5f;
    f32 knobX = element.data.checked
        ? toggleRect.x + toggleW - knobRadius - 3.0f
        : toggleRect.x + knobRadius + 3.0f;
    f32 knobY = toggleRect.y + toggleH * 0.5f;

    ImVec4 knobColor = ImVec4(theme.toggleKnob.x, theme.toggleKnob.y, theme.toggleKnob.z, 1.0f);
    dl->AddCircleFilled(ImVec2(knobX, knobY), knobRadius, ImGui::ColorConvertFloat4ToU32(knobColor));

    // Label text next to toggle
    if (!element.data.text.empty()) {
        f32 fontSize = ResolveFloat(element.style.fontSize, theme.fontSizeBody);
        ImVec4 textColor = ResolveColor(element.style.textColor, theme.textPrimary, 1.0f);
        UIRect labelRect = element.computedRect;
        labelRect.x = toggleRect.x + toggleW + 6.0f;
        labelRect.w = element.computedRect.w - toggleW - 6.0f;
        DrawCenteredText(dl, labelRect, element.data.text.c_str(),
            ImGui::ColorConvertFloat4ToU32(textColor), 0, 1, fontSize);
    }
}

void UISystem::RenderPlaceholder(const UIElement& element, const UITheme& theme) {
    // Phase 2+ widgets render as a labeled panel placeholder
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    f32 radius = ResolveFloat(element.style.borderRadius, theme.borderRadius);
    f32 alpha  = ResolveFloat(element.style.bgAlpha, theme.bgAlpha * 0.5f);

    ImVec4 bgColor = ResolveColor(element.style.bgColor, theme.surface, alpha);
    DrawRoundedRect(dl, element.computedRect, ImGui::ColorConvertFloat4ToU32(bgColor), radius);

    // Dashed border
    ImVec4 borderColor = ResolveColor(element.style.borderColor, theme.inputBorder, 0.4f);
    DrawRoundedRectBorder(dl, element.computedRect, ImGui::ColorConvertFloat4ToU32(borderColor), radius, 1.0f);

    // Type label
    const char* typeNames[] = {
        "Panel", "Button", "Label", "Image", "ProgressBar", "Slider", "Checkbox", "Toggle",
        "Dropdown", "TextInput", "RadioGroup", "ScrollArea", "Grid", "TabGroup", "Tooltip", "Modal", "ListView"
    };
    u8 typeIdx = static_cast<u8>(element.type);
    const char* typeName = (typeIdx < 17) ? typeNames[typeIdx] : "Unknown";

    std::string label = std::string("[") + typeName + "] " + element.name;
    f32 fontSize = ResolveFloat(element.style.fontSize, theme.fontSizeSmall);
    ImVec4 textColor = ResolveColor(element.style.textColor, theme.textSecondary, 0.7f);
    DrawCenteredText(dl, element.computedRect, label.c_str(),
        ImGui::ColorConvertFloat4ToU32(textColor), 1, 1, fontSize);
}

// ============================================================================
// EDITOR API (public wrappers for editor viewport preview)
// ============================================================================

void UISystem::ComputeLayoutForCanvas(UICanvasComponent& canvas, f32 vpW, f32 vpH) {
    ComputeLayout(canvas, vpW, vpH);
}

void UISystem::RenderCanvasPreview(const UICanvasComponent& canvas) {
    RenderCanvas(canvas);
}

} // namespace Enjin::GUI
