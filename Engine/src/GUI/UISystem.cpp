#include "Enjin/GUI/UISystem.h"
#include "Enjin/GUI/UICanvas.h"
#include "Enjin/Platform/Input.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Controllers/CharacterController.h"
#include "Enjin/Renderer/Camera.h"
#include "Enjin/Scene/SceneSerializer.h"
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

    // Shrink-to-fit: accessibility font scaling can push text past its widget
    // rect (buttons/toggles keep their authored box) — clamp the effective size
    // so letters stay inside the frame instead of spilling over its edges.
    if (rect.h > 4.0f && fontSize > rect.h - 4.0f) {
        fontSize = rect.h - 4.0f;
    }
    if (rect.w > 8.0f) {
        ImVec2 fullSize = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, text);
        if (fullSize.x > rect.w - 8.0f) {
            fontSize *= (rect.w - 8.0f) / fullSize.x;
        }
    }
    if (fontSize < 6.0f) fontSize = 6.0f;

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

void UISystem::SyncDataBindings(ECS::World* world) {
    // Resolve bind sources once per frame, then push into every bound element.
    // Player = any controller type with health (cross-compatible by policy).
    f32 healthPct = -1.0f;
    {
        auto tryController = [&](auto componentTag) {
            using T = decltype(componentTag);
            if (healthPct >= 0.0f) return;
            for (ECS::Entity e : world->GetEntitiesWithComponent<T>()) {
                if (auto* hp = world->GetComponent<ECS::HealthComponent>(e)) {
                    healthPct = hp->GetHealthPercent();
                    return;
                }
            }
        };
        tryController(ECS::ThirdPersonController{});
        tryController(ECS::FirstPersonController{});
        tryController(ECS::TopDown3DController{});
        tryController(ECS::Platformer2DController{});
        tryController(ECS::TopDown2DController{});
    }

    i32 coinsRemaining = -1;
    {
        i32 remaining = 0;
        bool anyCoin = false;
        for (ECS::Entity e : world->GetEntitiesWithComponent<ECS::PickupComponent>()) {
            auto* pk = world->GetComponent<ECS::PickupComponent>(e);
            if (pk && pk->type == ECS::PickupComponent::PickupType::Coin) {
                anyCoin = true;
                if (!pk->isCollected) ++remaining;
            }
        }
        if (anyCoin) coinsRemaining = remaining;
    }

    for (auto& entry : m_CachedCanvases) {
        auto* canvas = world->GetComponent<UICanvasComponent>(entry.entity);
        if (!canvas) continue;
        for (auto& element : canvas->elements) {
            if (element.data.bindField.empty()) continue;

            if (element.data.bindField == "health" && healthPct >= 0.0f) {
                element.data.progressValue = healthPct;
                char buf[32];
                snprintf(buf, sizeof(buf), " %d%%", static_cast<int>(healthPct * 100.0f));
                element.data.boundText = element.data.text + buf;
            } else if (element.data.bindField == "coins" && coinsRemaining >= 0) {
                // Total: authored bindMaxValue, else latch the highest count seen
                // (collected coins are destroyed, so remaining only shrinks).
                if (element.data.bindMaxValue < static_cast<f32>(coinsRemaining)) {
                    element.data.bindMaxValue = static_cast<f32>(coinsRemaining);
                }
                i32 total = static_cast<i32>(element.data.bindMaxValue);
                i32 got = total - coinsRemaining;
                if (got < 0) got = 0;
                element.data.progressValue = total > 0 ? static_cast<f32>(got) / total : 0.0f;
                char buf[48];
                snprintf(buf, sizeof(buf), " %d / %d", got, total);
                element.data.boundText = element.data.text + buf;
            }
        }
    }
}

void UISystem::Update(ECS::World* world, f32 vpW, f32 vpH, f32 deltaTime,
                      f32 originX, f32 originY, const Renderer::Camera* camera) {
    if (!world || vpW <= 0 || vpH <= 0) return;

    // Self-healing UI unification: any legacy hudWidget component that reaches
    // this frame (old templates, scripts adding the retired component) is
    // converted to a UICanvas before layout — scene loads already migrate.
    if (!world->GetEntitiesWithComponent<ECS::HUDWidgetComponent>().empty()) {
        Scene::SceneSerializer::MigrateHUDWidgetsToCanvases(world);
    }

    // Collect all canvas entities and sort by sortOrder (reuses member vector)
    m_CachedCanvases.clear();
    for (ECS::Entity entity : world->GetEntitiesWithComponent<UICanvasComponent>()) {
        auto* canvas = world->GetComponent<UICanvasComponent>(entity);
        if (!canvas || !canvas->visible) continue;
        if (!m_HUDEnabled && canvas->sortOrder >= HUD_SORT_ORDER) continue;  // HUD tier gated off
        m_CachedCanvases.push_back({entity, canvas->sortOrder});
    }

    std::sort(m_CachedCanvases.begin(), m_CachedCanvases.end(),
        [](const CanvasEntry& a, const CanvasEntry& b) { return a.sortOrder < b.sortOrder; });

    // Update cursor blink timer (skip animation when reduced motion is on)
    if (m_ReducedMotion) {
        m_CursorVisible = true;  // Always visible — no blinking
    } else {
        m_CursorBlinkTimer += deltaTime;
        if (m_CursorBlinkTimer >= 1.0f) m_CursorBlinkTimer -= 1.0f;
        m_CursorVisible = m_CursorBlinkTimer < 0.5f;
    }

    // Update tooltip hover timer (instant when reduced motion)
    if (m_TooltipHoverElementId != 0) {
        m_TooltipHoverTimer += m_ReducedMotion ? 999.0f : deltaTime;
    }

    // Refresh data-bound elements (health bars, coin counters) from gameplay
    // state before layout/render -- one authored HUD source on every platform.
    SyncDataBindings(world);

    // Clip all canvas drawing to the viewport rect: elements (and dropdown /
    // tooltip overlays) must never bleed past the game view's borders.
    ImDrawList* clipDL = ImGui::GetForegroundDrawList();
    clipDL->PushClipRect(ImVec2(originX, originY),
                         ImVec2(originX + vpW, originY + vpH), false);

    for (auto& entry : m_CachedCanvases) {
        auto* canvas = world->GetComponent<UICanvasComponent>(entry.entity);
        if (!canvas) continue;

        ComputeLayout(*canvas, vpW, vpH, originX, originY);
        ApplyWorldSpaceAnchors(*canvas, world, entry.entity, camera,
                               originX, originY, vpW, vpH);
        ProcessInput(*canvas, vpW, vpH);
        ProcessFocusNavigation(*canvas, deltaTime);
        RenderCanvas(*canvas);
    }

    clipDL->PopClipRect();
}

void UISystem::ApplyWorldSpaceAnchors(UICanvasComponent& canvas, ECS::World* world,
                                      ECS::Entity canvasEntity, const Renderer::Camera* camera,
                                      f32 originX, f32 originY, f32 vpW, f32 vpH) {
    for (auto& element : canvas.elements) {
        element.worldCulled = false;
        if (!element.data.worldSpace) continue;
        if (!camera || !world) { element.worldCulled = true; continue; }

        // Anchor to the source entity (default: the canvas's own entity)
        ECS::Entity src = element.data.worldSourceEntity != 0
                              ? static_cast<ECS::Entity>(element.data.worldSourceEntity)
                              : canvasEntity;
        auto* tf = world->GetComponent<ECS::TransformComponent>(src);
        if (!tf) { element.worldCulled = true; continue; }

        Math::Vector3 wp = tf->position + element.data.worldOffset;
        Math::Vector3 d = wp - camera->GetPosition();
        f32 dist = d.Length();
        if (element.data.maxRenderDistance > 0.0f && dist > element.data.maxRenderDistance) {
            element.worldCulled = true;
            continue;
        }

        Math::Vector4 clip = camera->GetViewProjectionMatrix()
                           * Math::Vector4(wp.x, wp.y, wp.z, 1.0f);
        if (clip.w <= 0.001f) { element.worldCulled = true; continue; }
        f32 ndcX = clip.x / clip.w;
        f32 ndcY = clip.y / clip.w;
        f32 ndcZ = clip.z / clip.w;
        if (ndcZ < 0.0f || ndcZ > 1.0f) { element.worldCulled = true; continue; }
        f32 fx = (ndcX + 1.0f) * 0.5f;
        f32 fy = (ndcY + 1.0f) * 0.5f;
        if (fx < -0.2f || fx > 1.2f || fy < -0.2f || fy > 1.2f) {
            element.worldCulled = true;
            continue;
        }

        // Recenter the laid-out rect on the projected point (size unchanged)
        f32 cx = originX + fx * vpW;
        f32 cy = originY + fy * vpH;
        element.computedRect.x = cx - element.computedRect.w * 0.5f;
        element.computedRect.y = cy - element.computedRect.h * 0.5f;
    }
}

// ============================================================================
// LAYOUT
// ============================================================================

void UISystem::ComputeLayout(UICanvasComponent& canvas, f32 vpW, f32 vpH,
                             f32 originX, f32 originY) {
    if (canvas.designWidth <= 0.0f || canvas.designHeight <= 0.0f) return; // UI-H1: prevent div-by-zero
    f32 scaleX = vpW / canvas.designWidth;
    f32 scaleY = vpH / canvas.designHeight;
    f32 scaleFactor = (canvas.scaleMode == UIScaleMode::ConstantPixelSize)
        ? 1.0f
        : std::min(scaleX, scaleY);

    // Root at the viewport origin: computed rects are in WINDOW coordinates,
    // which is what both the ImGui draw lists and io.MousePos hit-tests use.
    UIRect rootRect = {originX, originY, vpW, vpH};

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
    //
    // Inverted edges (right < left / bottom < top) are the single most common
    // authoring mistake: writing the intuitive +100/-100 instead of the
    // Unity-convention -100/+100 for a centered 200-wide element. The old
    // "legacy center-on-anchor" fallback reinterpreted the offsets through the
    // pivot, which flung top-anchored elements off-screen. Swapping the edges
    // instead yields exactly the rect the author meant, and correctly-authored
    // content (non-negative sizes) is untouched.
    if (resolvedRight < resolvedLeft) { f32 t = resolvedLeft; resolvedLeft = resolvedRight; resolvedRight = t; }
    if (resolvedBottom < resolvedTop) { f32 t = resolvedTop; resolvedTop = resolvedBottom; resolvedBottom = t; }

    element.computedRect.x = resolvedLeft;
    element.computedRect.y = resolvedTop;
    element.computedRect.w = resolvedRight - resolvedLeft;
    element.computedRect.h = resolvedBottom - resolvedTop;
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
    bool mouseReleased = ImGui::IsMouseReleased(0);

    // Sticky drag: continue adjusting slider even when mouse leaves element bounds
    if (m_StickyDragEnabled && m_StickyDragActive && m_StickyDragElementId != 0) {
        if (mouseReleased) {
            // Release sticky drag
            m_StickyDragActive = false;
            m_StickyDragElementId = 0;
        } else if (mouseDown) {
            // Continue adjusting the slider
            UIElement* dragElement = canvas.GetElement(m_StickyDragElementId);
            if (dragElement && dragElement->type == UIWidgetType::Slider) {
                dragElement->interaction.active = true;
                f32 relX = (dragElement->computedRect.w > 0.0f)
                ? (mouseX - dragElement->computedRect.x) / dragElement->computedRect.w
                : 0.0f;
                relX = std::max(0.0f, std::min(1.0f, relX));
                f32 newValue = dragElement->data.sliderMin + relX * (dragElement->data.sliderMax - dragElement->data.sliderMin);
                if (newValue != dragElement->data.sliderValue) {
                    dragElement->data.sliderValue = newValue;
                    if (!dragElement->onValueChangedEvent.empty()) {
                        UIEventData event;
                        event.elementId = dragElement->id;
                        event.eventName = dragElement->onValueChangedEvent;
                        event.floatValue = newValue;
                        m_EventBus.Dispatch(event);
                    }
                }
            }
            return; // Skip normal input while sticky-dragging
        }
    }

    // Track which element the mouse is hovering for dwell-click
    u32 hoveredFocusableId = 0;

    // Iterate elements in reverse order (front-to-back for hit testing)
    for (i32 i = static_cast<i32>(canvas.elements.size()) - 1; i >= 0; --i) {
        auto& element = canvas.elements[static_cast<usize>(i)];
        if (!element.visible || !element.enabled || element.worldCulled) {
            element.interaction = {};
            continue;
        }

        bool hit = element.computedRect.Contains(mouseX, mouseY);
        element.interaction.hovered = hit;
        element.interaction.pressed = hit && mouseDown;

        if (!hit) continue;

        // Track hovered focusable element for dwell-click
        if (IsElementFocusable(element) && hoveredFocusableId == 0) {
            hoveredFocusableId = element.id;
        }

        // Set focus on mouse click for focusable elements
        if (mouseClicked && IsElementFocusable(element)) {
            SetFocus(canvas, element.id);
        }

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

            // Start sticky drag if enabled
            if (m_StickyDragEnabled && mouseClicked) {
                m_StickyDragActive = true;
                m_StickyDragElementId = element.id;
            }

            f32 relX = (element.computedRect.w > 0.0f)
                ? (mouseX - element.computedRect.x) / element.computedRect.w
                : 0.0f;
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

        // Dropdown click — toggle open/close
        if (element.type == UIWidgetType::Dropdown && mouseClicked) {
            if (element.data.dropdownOpen) {
                // Close
                element.data.dropdownOpen = false;
                m_OpenDropdownId = 0;
            } else {
                // Close any other open dropdown first
                if (m_OpenDropdownId != 0 && m_OpenDropdownId != element.id) {
                    for (auto& other : canvas.elements) {
                        if (other.id == m_OpenDropdownId) {
                            other.data.dropdownOpen = false;
                            break;
                        }
                    }
                }
                element.data.dropdownOpen = true;
                m_OpenDropdownId = element.id;
            }
        }

        // RadioGroup click — select option based on Y position
        if (element.type == UIWidgetType::RadioGroup && mouseClicked && !element.data.options.empty()) {
            f32 itemH = element.computedRect.h / static_cast<f32>(element.data.options.size());
            if (itemH <= 0.0f) itemH = 1.0f;
            i32 clickedIdx = static_cast<i32>((mouseY - element.computedRect.y) / itemH);
            clickedIdx = std::max(0, std::min(clickedIdx, static_cast<i32>(element.data.options.size()) - 1));
            if (clickedIdx != element.data.selectedOption) {
                element.data.selectedOption = clickedIdx;
                if (!element.onValueChangedEvent.empty()) {
                    UIEventData event;
                    event.elementId = element.id;
                    event.eventName = element.onValueChangedEvent;
                    event.intValue = clickedIdx;
                    event.stringValue = element.data.options[clickedIdx];
                    m_EventBus.Dispatch(event);
                }
            }
        }

        // ListView click — select item based on Y position among children
        if (element.type == UIWidgetType::ListView && mouseClicked && !element.childIds.empty()) {
            f32 fontSize = ResolveFloat(element.style.fontSize, canvas.theme.fontSizeBody) * m_FontScale;
            if (fontSize < 1.0f) fontSize = 1.0f;
            f32 itemH = fontSize + 8.0f;
            f32 scrollOff = element.data.scrollOffset;
            i32 clickedIdx = static_cast<i32>((mouseY - element.computedRect.y + scrollOff) / itemH);
            clickedIdx = std::max(0, std::min(clickedIdx, static_cast<i32>(element.childIds.size()) - 1));
            if (clickedIdx != element.data.listSelectedIndex) {
                element.data.listSelectedIndex = clickedIdx;
                if (!element.onValueChangedEvent.empty()) {
                    UIEventData event;
                    event.elementId = element.id;
                    event.eventName = element.onValueChangedEvent;
                    event.intValue = clickedIdx;
                    m_EventBus.Dispatch(event);
                }
            }
        }

        // TabGroup click — select tab from tab bar
        if (element.type == UIWidgetType::TabGroup && mouseClicked && !element.childIds.empty()) {
            f32 tabBarH = 30.0f;
            if (mouseY < element.computedRect.y + tabBarH) {
                f32 tabW = element.computedRect.w / static_cast<f32>(element.childIds.size());
                if (tabW <= 0.0f) tabW = 1.0f;
                i32 clickedTab = static_cast<i32>((mouseX - element.computedRect.x) / tabW);
                clickedTab = std::max(0, std::min(clickedTab, static_cast<i32>(element.childIds.size()) - 1));
                if (clickedTab != element.data.activeTabIndex) {
                    element.data.activeTabIndex = clickedTab;
                    if (!element.onValueChangedEvent.empty()) {
                        UIEventData event;
                        event.elementId = element.id;
                        event.eventName = element.onValueChangedEvent;
                        event.intValue = clickedTab;
                        m_EventBus.Dispatch(event);
                    }
                }
            }
        }

        // Tooltip hover tracking
        if (element.type == UIWidgetType::Tooltip || !element.data.tooltipText.empty()) {
            // Handled below after loop
        }
    }

    // Dropdown list item selection (when open, check clicks in the overlay list area)
    if (m_OpenDropdownId != 0 && mouseClicked) {
        bool clickedInList = false;
        for (auto& element : canvas.elements) {
            if (element.id != m_OpenDropdownId) continue;
            if (!element.data.dropdownOpen) break;

            f32 fontSize = ResolveFloat(element.style.fontSize, canvas.theme.fontSizeBody) * m_FontScale;
            if (fontSize < 1.0f) fontSize = 1.0f;
            f32 itemH = fontSize + 8.0f;
            f32 listH = itemH * static_cast<f32>(element.data.options.size());
            f32 maxListH = itemH * 8.0f;
            if (listH > maxListH) listH = maxListH;

            UIRect listRect;
            listRect.x = element.computedRect.x;
            listRect.y = element.computedRect.y + element.computedRect.h + 2.0f;
            listRect.w = element.computedRect.w;
            listRect.h = listH;

            if (listRect.Contains(mouseX, mouseY)) {
                i32 clickedIdx = static_cast<i32>((mouseY - listRect.y) / itemH);
                clickedIdx = std::max(0, std::min(clickedIdx, static_cast<i32>(element.data.options.size()) - 1));
                element.data.selectedOption = clickedIdx;
                element.data.dropdownOpen = false;
                m_OpenDropdownId = 0;
                clickedInList = true;

                if (!element.onValueChangedEvent.empty()) {
                    UIEventData event;
                    event.elementId = element.id;
                    event.eventName = element.onValueChangedEvent;
                    event.intValue = clickedIdx;
                    event.stringValue = element.data.options[clickedIdx];
                    m_EventBus.Dispatch(event);
                }
            } else if (!element.computedRect.Contains(mouseX, mouseY)) {
                // Click outside dropdown and list — close it
                element.data.dropdownOpen = false;
                m_OpenDropdownId = 0;
            }
            break;
        }
    }

    // ScrollArea mouse wheel handling
    for (auto& element : canvas.elements) {
        if (element.type == UIWidgetType::ScrollArea && element.visible && element.enabled) {
            if (element.computedRect.Contains(mouseX, mouseY) && io.MouseWheel != 0.0f) {
                element.data.scrollOffset -= io.MouseWheel * 30.0f;
                if (element.data.scrollOffset < 0.0f) element.data.scrollOffset = 0.0f;
                // Content height computed at render time, clamping done there
            }
        }
        // ListView mouse wheel
        if (element.type == UIWidgetType::ListView && element.visible && element.enabled) {
            if (element.computedRect.Contains(mouseX, mouseY) && io.MouseWheel != 0.0f) {
                element.data.scrollOffset -= io.MouseWheel * 30.0f;
                if (element.data.scrollOffset < 0.0f) element.data.scrollOffset = 0.0f;
            }
        }
    }

    // Tooltip hover tracking (check all elements for tooltipText)
    {
        u32 newTooltipHover = 0;
        for (const auto& element : canvas.elements) {
            if (!element.visible || !element.enabled || element.worldCulled) continue;
            if (element.data.tooltipText.empty()) continue;
            if (element.computedRect.Contains(mouseX, mouseY)) {
                newTooltipHover = element.id;
                break;
            }
        }
        if (newTooltipHover != m_TooltipHoverElementId) {
            m_TooltipHoverElementId = newTooltipHover;
            m_TooltipHoverTimer = 0.0f;
            m_TooltipMouseX = mouseX;
            m_TooltipMouseY = mouseY;
        }
    }

    // TextInput keyboard handling for focused text input
    if (canvas.focusedElementId != 0) {
        UIElement* focused = canvas.GetElement(canvas.focusedElementId);
        if (focused && focused->type == UIWidgetType::TextInput && focused->enabled) {
            auto& text = focused->data.inputText;
            auto& cursor = focused->data.cursorPos;
            auto& selStart = focused->data.selectionStart;

            // Clamp cursor
            cursor = std::max(0, std::min(cursor, static_cast<i32>(text.size())));

            // Character input
            for (i32 c = 0; c < io.InputQueueCharacters.Size; ++c) {
                ImWchar ch = io.InputQueueCharacters[c];
                if (ch >= 32 && ch < 127) {
                    // Delete selection if any
                    if (selStart >= 0 && selStart != cursor) {
                        i32 sMin = std::min(selStart, cursor);
                        i32 sMax = std::max(selStart, cursor);
                        text.erase(sMin, sMax - sMin);
                        cursor = sMin;
                        selStart = -1;
                    }
                    text.insert(text.begin() + cursor, static_cast<char>(ch));
                    cursor++;
                    m_CursorBlinkTimer = 0.0f;
                }
            }

            // Backspace
            if (ImGui::IsKeyPressed(ImGuiKey_Backspace)) {
                if (selStart >= 0 && selStart != cursor) {
                    i32 sMin = std::min(selStart, cursor);
                    i32 sMax = std::max(selStart, cursor);
                    text.erase(sMin, sMax - sMin);
                    cursor = sMin;
                    selStart = -1;
                } else if (cursor > 0) {
                    text.erase(cursor - 1, 1);
                    cursor--;
                }
                m_CursorBlinkTimer = 0.0f;
            }

            // Delete
            if (ImGui::IsKeyPressed(ImGuiKey_Delete)) {
                if (selStart >= 0 && selStart != cursor) {
                    i32 sMin = std::min(selStart, cursor);
                    i32 sMax = std::max(selStart, cursor);
                    text.erase(sMin, sMax - sMin);
                    cursor = sMin;
                    selStart = -1;
                } else if (cursor < static_cast<i32>(text.size())) {
                    text.erase(cursor, 1);
                }
                m_CursorBlinkTimer = 0.0f;
            }

            // Left arrow
            if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) {
                if (io.KeyShift) {
                    if (selStart < 0) selStart = cursor;
                } else {
                    selStart = -1;
                }
                if (cursor > 0) cursor--;
                m_CursorBlinkTimer = 0.0f;
            }

            // Right arrow
            if (ImGui::IsKeyPressed(ImGuiKey_RightArrow)) {
                if (io.KeyShift) {
                    if (selStart < 0) selStart = cursor;
                } else {
                    selStart = -1;
                }
                if (cursor < static_cast<i32>(text.size())) cursor++;
                m_CursorBlinkTimer = 0.0f;
            }

            // Home
            if (ImGui::IsKeyPressed(ImGuiKey_Home)) {
                if (io.KeyShift && selStart < 0) selStart = cursor;
                else if (!io.KeyShift) selStart = -1;
                cursor = 0;
                m_CursorBlinkTimer = 0.0f;
            }

            // End
            if (ImGui::IsKeyPressed(ImGuiKey_End)) {
                if (io.KeyShift && selStart < 0) selStart = cursor;
                else if (!io.KeyShift) selStart = -1;
                cursor = static_cast<i32>(text.size());
                m_CursorBlinkTimer = 0.0f;
            }

            // Select All (Ctrl+A)
            if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_A)) {
                selStart = 0;
                cursor = static_cast<i32>(text.size());
            }

            // Submit (Enter)
            if (ImGui::IsKeyPressed(ImGuiKey_Enter)) {
                if (!focused->onSubmitEvent.empty()) {
                    UIEventData event;
                    event.elementId = focused->id;
                    event.eventName = focused->onSubmitEvent;
                    event.stringValue = text;
                    m_EventBus.Dispatch(event);
                }
            }
        }
    }

    // Dwell-click processing (Task #40)
    if (m_DwellClickEnabled) {
        if (hoveredFocusableId != 0 && hoveredFocusableId == m_DwellHoverElementId) {
            m_DwellHoverTimer += io.DeltaTime;
            if (m_DwellHoverTimer >= m_DwellClickTime) {
                // Auto-activate after dwell
                SetFocus(canvas, hoveredFocusableId);
                ActivateFocusedElement(canvas);
                m_DwellHoverTimer = 0.0f;
                m_DwellHoverElementId = 0;
            }
        } else {
            m_DwellHoverElementId = hoveredFocusableId;
            m_DwellHoverTimer = 0.0f;
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
    u32 focusedId = canvas.focusedElementId;

    // Render root elements, then their children recursively
    // Note: ScrollArea, Grid, TabGroup, Modal handle their own children
    for (const auto& element : canvas.elements) {
        if (element.parentId == 0 && element.visible && !element.worldCulled) {
            RenderElement(element, canvas.theme, focusedId, canvas);

            // Container widgets render their own children
            if (element.type == UIWidgetType::ScrollArea ||
                element.type == UIWidgetType::Grid ||
                element.type == UIWidgetType::TabGroup ||
                element.type == UIWidgetType::Modal) {
                continue;
            }

            // Render children
            for (u32 childId : element.childIds) {
                const UIElement* child = canvas.GetElement(childId);
                if (child && child->visible && !child->worldCulled) {
                    RenderElement(*child, canvas.theme, focusedId, canvas);

                    // Container children handle their own sub-children
                    if (child->type == UIWidgetType::ScrollArea ||
                        child->type == UIWidgetType::Grid ||
                        child->type == UIWidgetType::TabGroup ||
                        child->type == UIWidgetType::Modal) {
                        continue;
                    }

                    // One level of nesting for children's children
                    for (u32 grandchildId : child->childIds) {
                        const UIElement* grandchild = canvas.GetElement(grandchildId);
                        if (grandchild && grandchild->visible && !grandchild->worldCulled) {
                            RenderElement(*grandchild, canvas.theme, focusedId, canvas);
                        }
                    }
                }
            }
        }
    }

    // Render dropdown overlays on top of everything (if any open)
    if (m_OpenDropdownId != 0) {
        const UIElement* dropdown = canvas.GetElement(m_OpenDropdownId);
        if (dropdown && dropdown->visible && dropdown->data.dropdownOpen) {
            ImDrawList* dl = ImGui::GetForegroundDrawList();
            const UITheme& theme = canvas.theme;
            f32 radius = ResolveFloat(dropdown->style.borderRadius, theme.borderRadius);
            f32 fontSize = ResolveFloat(dropdown->style.fontSize, theme.fontSizeBody) * m_FontScale;
            if (fontSize < 1.0f) fontSize = 1.0f;
            f32 itemH = fontSize + 8.0f;
            f32 listH = itemH * static_cast<f32>(dropdown->data.options.size());
            f32 maxListH = itemH * 8.0f;
            if (listH > maxListH) listH = maxListH;

            UIRect listRect;
            listRect.x = dropdown->computedRect.x;
            listRect.y = dropdown->computedRect.y + dropdown->computedRect.h + 2.0f;
            listRect.w = dropdown->computedRect.w;
            listRect.h = listH;

            // Background
            ImVec4 bgColor = ImVec4(theme.surface.x, theme.surface.y, theme.surface.z, 0.98f);
            DrawRoundedRect(dl, listRect, ImGui::ColorConvertFloat4ToU32(bgColor), radius);

            // Border
            ImVec4 borderColor = ResolveColor(dropdown->style.borderColor, theme.inputBorder, 0.8f);
            DrawRoundedRectBorder(dl, listRect, ImGui::ColorConvertFloat4ToU32(borderColor), radius, 1.0f);

            // Items
            ImGuiIO& io = ImGui::GetIO();
            f32 mouseX = io.MousePos.x;
            f32 mouseY = io.MousePos.y;
            ImVec4 textColor = ResolveColor(dropdown->style.textColor, theme.textPrimary, 1.0f);
            ImU32 textCol = ImGui::ColorConvertFloat4ToU32(textColor);

            for (i32 i = 0; i < static_cast<i32>(dropdown->data.options.size()); ++i) {
                UIRect itemRect;
                itemRect.x = listRect.x;
                itemRect.y = listRect.y + static_cast<f32>(i) * itemH;
                itemRect.w = listRect.w;
                itemRect.h = itemH;

                if (itemRect.y + itemRect.h > listRect.y + listRect.h) break;

                bool hovered = itemRect.Contains(mouseX, mouseY);
                bool selected = (i == dropdown->data.selectedOption);

                if (selected) {
                    ImVec4 selColor = ImVec4(theme.primary.x, theme.primary.y, theme.primary.z, 0.3f);
                    DrawRoundedRect(dl, itemRect, ImGui::ColorConvertFloat4ToU32(selColor), 0.0f);
                } else if (hovered) {
                    ImVec4 hoverColor = ImVec4(theme.primary.x, theme.primary.y, theme.primary.z, 0.15f);
                    DrawRoundedRect(dl, itemRect, ImGui::ColorConvertFloat4ToU32(hoverColor), 0.0f);
                }

                UIRect textRect = itemRect;
                textRect.x += 8.0f;
                textRect.w -= 16.0f;
                DrawCenteredText(dl, textRect, dropdown->data.options[i].c_str(),
                    textCol, 0, 1, fontSize);
            }
        }
    }

    // Render tooltips last (on top of everything)
    if (m_TooltipHoverElementId != 0) {
        const UIElement* tooltipOwner = canvas.GetElement(m_TooltipHoverElementId);
        if (tooltipOwner && !tooltipOwner->data.tooltipText.empty() &&
            m_TooltipHoverTimer >= tooltipOwner->data.tooltipDelay) {
            RenderTooltip(*tooltipOwner, canvas.theme);
        }
    }
}

void UISystem::RenderElement(const UIElement& element, const UITheme& theme, u32 focusedId,
                             const UICanvasComponent& canvas) {
    bool isFocused = (element.id == focusedId && focusedId != 0);

    switch (element.type) {
        case UIWidgetType::Panel:       RenderPanel(element, theme); break;
        case UIWidgetType::Button:      RenderButton(element, theme, isFocused); break;
        case UIWidgetType::Label:       RenderLabel(element, theme); break;
        case UIWidgetType::Image:       RenderImage(element, theme); break;
        case UIWidgetType::ProgressBar: RenderProgressBar(element, theme); break;
        case UIWidgetType::Slider:      RenderSlider(element, theme); break;
        case UIWidgetType::Checkbox:    RenderCheckbox(element, theme); break;
        case UIWidgetType::Toggle:      RenderToggle(element, theme); break;
        case UIWidgetType::Dropdown:    RenderDropdown(element, theme, isFocused); break;
        case UIWidgetType::TextInput:   RenderTextInput(element, theme, isFocused); break;
        case UIWidgetType::RadioGroup:  RenderRadioGroup(element, theme, isFocused); break;
        case UIWidgetType::ScrollArea:  RenderScrollArea(element, theme, canvas); break;
        case UIWidgetType::Grid:        RenderGrid(element, theme, canvas, focusedId); break;
        case UIWidgetType::TabGroup:    RenderTabGroup(element, theme, canvas, focusedId); break;
        case UIWidgetType::Tooltip:     break; // Tooltips are rendered in RenderCanvas overlay pass
        case UIWidgetType::Modal:       RenderModal(element, theme, canvas, focusedId); break;
        case UIWidgetType::ListView:    RenderListView(element, theme, canvas, isFocused); break;
        default:                        RenderPlaceholder(element, theme); break;
    }

    // Draw focus indicator on top of widget
    if (isFocused) {
        if (m_SwitchAccessEnabled) {
            RenderScanIndicator(element, theme);
        } else {
            RenderFocusIndicator(element, theme);
        }
    }

    // Dwell-click progress ring
    if (m_DwellClickEnabled && element.id == m_DwellHoverElementId && m_DwellHoverTimer > 0.0f) {
        f32 progress = (m_DwellClickTime > 0.0f) ? m_DwellHoverTimer / m_DwellClickTime : 1.0f;
        RenderDwellProgress(element, progress);
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

void UISystem::RenderButton(const UIElement& element, const UITheme& theme, bool focused) {
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
            } else if (element.interaction.hovered || focused) {
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
        } else if (element.interaction.hovered || focused) {
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
    f32 fontSize = ResolveFloat(element.style.fontSize, theme.fontSizeBody) * m_FontScale;
    ImVec4 textColor = ResolveColor(element.style.textColor, theme.textPrimary, 1.0f);
    if (!element.enabled) textColor = ResolveColor(element.style.textColor, theme.textDisabled, 1.0f);

    DrawCenteredText(dl, element.computedRect, element.data.text.c_str(),
        ImGui::ColorConvertFloat4ToU32(textColor),
        element.data.textAlignH, element.data.textAlignV, fontSize);
}

void UISystem::RenderLabel(const UIElement& element, const UITheme& theme) {
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    f32 fontSize = ResolveFloat(element.style.fontSize, theme.fontSizeBody) * m_FontScale;
    ImVec4 textColor = ResolveColor(element.style.textColor, theme.textPrimary, 1.0f);

    const std::string& labelText = element.data.boundText.empty()
        ? element.data.text : element.data.boundText;

    // Fast path: no per-char colors, use single-call rendering
    if (element.data.charColors.empty()) {
        DrawCenteredText(dl, element.computedRect, labelText.c_str(),
            ImGui::ColorConvertFloat4ToU32(textColor),
            element.data.textAlignH, element.data.textAlignV, fontSize);
        return;
    }

    // Per-character color path (used by games needing inline color variation)
    ImFont* font = ImGui::GetFont();
    const std::string& text = element.data.text;
    if (text.empty()) return;

    ImVec2 totalSize = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, text.c_str());

    f32 x = element.computedRect.x;
    f32 y = element.computedRect.y;
    switch (element.data.textAlignH) {
        case 0: x = element.computedRect.x + 4.0f; break;
        case 1: x = element.computedRect.x + (element.computedRect.w - totalSize.x) * 0.5f; break;
        case 2: x = element.computedRect.x + element.computedRect.w - totalSize.x - 4.0f; break;
    }
    switch (element.data.textAlignV) {
        case 0: y = element.computedRect.y + 2.0f; break;
        case 1: y = element.computedRect.y + (element.computedRect.h - totalSize.y) * 0.5f; break;
        case 2: y = element.computedRect.y + element.computedRect.h - totalSize.y - 2.0f; break;
    }

    ImU32 defaultColor = ImGui::ColorConvertFloat4ToU32(textColor);
    for (usize i = 0; i < text.size(); ++i) {
        char ch[2] = { text[i], '\0' };
        ImU32 color = (i < element.data.charColors.size()) ? element.data.charColors[i] : defaultColor;
        dl->AddText(font, fontSize, ImVec2(x, y), color, ch);
        ImVec2 charSize = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, ch);
        x += charSize.x;
    }
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
        f32 fontSize = ResolveFloat(element.style.fontSize, theme.fontSizeBody) * m_FontScale;
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
        f32 fontSize = ResolveFloat(element.style.fontSize, theme.fontSizeBody) * m_FontScale;
        ImVec4 textColor = ResolveColor(element.style.textColor, theme.textPrimary, 1.0f);
        UIRect labelRect = element.computedRect;
        labelRect.x = toggleRect.x + toggleW + 6.0f;
        labelRect.w = element.computedRect.w - toggleW - 6.0f;
        DrawCenteredText(dl, labelRect, element.data.text.c_str(),
            ImGui::ColorConvertFloat4ToU32(textColor), 0, 1, fontSize);
    }
}

// ============================================================================
// DROPDOWN
// ============================================================================

void UISystem::RenderDropdown(const UIElement& element, const UITheme& theme, bool focused) {
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    f32 radius = ResolveFloat(element.style.borderRadius, theme.borderRadius);
    f32 fontSize = ResolveFloat(element.style.fontSize, theme.fontSizeBody) * m_FontScale;

    // Background
    Math::Vector3 bgDefault = theme.inputBg;
    if (element.style.HasBgColor()) bgDefault = element.style.bgColor;
    Math::Vector3 bgColor = bgDefault;
    if (element.interaction.hovered || focused || element.data.dropdownOpen) {
        bgColor = Math::Vector3(bgDefault.x + 0.04f, bgDefault.y + 0.04f, bgDefault.z + 0.04f);
    }
    DrawRoundedRect(dl, element.computedRect,
        ImGui::ColorConvertFloat4ToU32(ImVec4(bgColor.x, bgColor.y, bgColor.z, 1.0f)), radius);

    // Border
    f32 borderW = ResolveFloat(element.style.borderWidth, theme.borderWidth);
    Math::Vector3 borderCol = (focused || element.data.dropdownOpen) ? theme.inputFocused : theme.inputBorder;
    ImVec4 borderColor = ImVec4(borderCol.x, borderCol.y, borderCol.z, 0.8f);
    DrawRoundedRectBorder(dl, element.computedRect, ImGui::ColorConvertFloat4ToU32(borderColor), radius, borderW);

    // Selected text
    ImVec4 textColor = ResolveColor(element.style.textColor, theme.textPrimary, 1.0f);
    ImU32 textCol = ImGui::ColorConvertFloat4ToU32(textColor);

    std::string displayText;
    if (element.data.selectedOption >= 0 &&
        element.data.selectedOption < static_cast<i32>(element.data.options.size())) {
        displayText = element.data.options[element.data.selectedOption];
    } else if (!element.data.placeholder.empty()) {
        displayText = element.data.placeholder;
        textCol = ImGui::ColorConvertFloat4ToU32(
            ResolveColor(element.style.textColor, theme.textSecondary, 0.6f));
    }

    UIRect textRect = element.computedRect;
    textRect.x += 8.0f;
    textRect.w -= 32.0f; // Leave space for arrow
    DrawCenteredText(dl, textRect, displayText.c_str(), textCol, 0, 1, fontSize);

    // Down arrow
    f32 arrowSize = fontSize * 0.4f;
    f32 arrowX = element.computedRect.x + element.computedRect.w - 16.0f;
    f32 arrowY = element.computedRect.y + element.computedRect.h * 0.5f;
    ImU32 arrowCol = ImGui::ColorConvertFloat4ToU32(
        ResolveColor(element.style.textColor, theme.textSecondary, 0.8f));

    if (element.data.dropdownOpen) {
        // Up arrow when open
        dl->AddTriangleFilled(
            ImVec2(arrowX - arrowSize, arrowY + arrowSize * 0.5f),
            ImVec2(arrowX + arrowSize, arrowY + arrowSize * 0.5f),
            ImVec2(arrowX, arrowY - arrowSize * 0.5f),
            arrowCol);
    } else {
        // Down arrow when closed
        dl->AddTriangleFilled(
            ImVec2(arrowX - arrowSize, arrowY - arrowSize * 0.5f),
            ImVec2(arrowX + arrowSize, arrowY - arrowSize * 0.5f),
            ImVec2(arrowX, arrowY + arrowSize * 0.5f),
            arrowCol);
    }

    // The dropdown list overlay is rendered in RenderCanvas after all elements
}

// ============================================================================
// TEXT INPUT
// ============================================================================

void UISystem::RenderTextInput(const UIElement& element, const UITheme& theme, bool focused) {
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    f32 radius = ResolveFloat(element.style.borderRadius, theme.borderRadius);
    f32 fontSize = ResolveFloat(element.style.fontSize, theme.fontSizeBody) * m_FontScale;

    // Background
    Math::Vector3 bgDefault = theme.inputBg;
    if (element.style.HasBgColor()) bgDefault = element.style.bgColor;
    DrawRoundedRect(dl, element.computedRect,
        ImGui::ColorConvertFloat4ToU32(ImVec4(bgDefault.x, bgDefault.y, bgDefault.z, 1.0f)), radius);

    // Border (highlight when focused)
    f32 borderW = ResolveFloat(element.style.borderWidth, theme.borderWidth);
    Math::Vector3 borderCol = focused ? theme.inputFocused : theme.inputBorder;
    ImVec4 borderColor = ImVec4(borderCol.x, borderCol.y, borderCol.z, focused ? 1.0f : 0.6f);
    DrawRoundedRectBorder(dl, element.computedRect, ImGui::ColorConvertFloat4ToU32(borderColor), radius, borderW);

    // Content area with padding
    f32 padX = 8.0f;
    UIRect contentRect = element.computedRect;
    contentRect.x += padX;
    contentRect.w -= padX * 2.0f;

    ImFont* font = ImGui::GetFont();
    const auto& text = element.data.inputText;
    i32 cursorPos = element.data.cursorPos;
    i32 selStart = element.data.selectionStart;

    // Show placeholder if empty and not focused
    if (text.empty() && !focused && !element.data.placeholder.empty()) {
        ImVec4 placeholderColor = ResolveColor(Math::Vector3(-1, -1, -1), theme.textSecondary, 0.5f);
        DrawCenteredText(dl, contentRect, element.data.placeholder.c_str(),
            ImGui::ColorConvertFloat4ToU32(placeholderColor), 0, 1, fontSize);
        return;
    }

    // Calculate text metrics
    ImVec2 textSize = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, text.c_str());
    f32 textY = contentRect.y + (contentRect.h - textSize.y) * 0.5f;

    // Draw selection highlight
    if (focused && selStart >= 0 && selStart != cursorPos) {
        i32 sMin = std::min(selStart, cursorPos);
        i32 sMax = std::max(selStart, cursorPos);
        std::string presel(text.begin(), text.begin() + sMin);
        std::string sel(text.begin() + sMin, text.begin() + sMax);
        ImVec2 preSelSize = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, presel.c_str());
        ImVec2 selSize = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, sel.c_str());

        UIRect selRect;
        selRect.x = contentRect.x + preSelSize.x;
        selRect.y = textY;
        selRect.w = selSize.x;
        selRect.h = textSize.y;
        ImVec4 selColor = ImVec4(theme.primary.x, theme.primary.y, theme.primary.z, 0.35f);
        DrawRoundedRect(dl, selRect, ImGui::ColorConvertFloat4ToU32(selColor), 0.0f);
    }

    // Draw text
    ImVec4 textColor = ResolveColor(element.style.textColor, theme.textPrimary, 1.0f);
    dl->AddText(font, fontSize, ImVec2(contentRect.x, textY),
        ImGui::ColorConvertFloat4ToU32(textColor), text.c_str());

    // Draw cursor
    if (focused && m_CursorVisible) {
        i32 clampedCursor = std::max(0, std::min(cursorPos, static_cast<i32>(text.size())));
        std::string preCursor(text.begin(), text.begin() + clampedCursor);
        ImVec2 preCursorSize = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, preCursor.c_str());

        f32 cursorX = contentRect.x + preCursorSize.x;
        ImU32 cursorCol = ImGui::ColorConvertFloat4ToU32(
            ImVec4(theme.textPrimary.x, theme.textPrimary.y, theme.textPrimary.z, 0.9f));
        dl->AddLine(
            ImVec2(cursorX, textY),
            ImVec2(cursorX, textY + textSize.y),
            cursorCol, 1.5f);
    }
}

// ============================================================================
// RADIO GROUP
// ============================================================================

void UISystem::RenderRadioGroup(const UIElement& element, const UITheme& theme, bool focused) {
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    f32 fontSize = ResolveFloat(element.style.fontSize, theme.fontSizeBody) * m_FontScale;

    if (element.data.options.empty()) return;

    f32 itemH = element.computedRect.h / static_cast<f32>(element.data.options.size());
    if (itemH <= 0.0f) return; // Zero-height element, nothing to render
    f32 radioRadius = std::min(itemH * 0.3f, 8.0f);

    ImVec4 textColor = ResolveColor(element.style.textColor, theme.textPrimary, 1.0f);
    ImU32 textCol = ImGui::ColorConvertFloat4ToU32(textColor);

    for (i32 i = 0; i < static_cast<i32>(element.data.options.size()); ++i) {
        f32 itemY = element.computedRect.y + static_cast<f32>(i) * itemH;
        f32 cx = element.computedRect.x + radioRadius + 6.0f;
        f32 cy = itemY + itemH * 0.5f;

        bool isSelected = (i == element.data.selectedOption);

        // Outer circle
        ImU32 outerCol = isSelected
            ? ImGui::ColorConvertFloat4ToU32(ImVec4(theme.primary.x, theme.primary.y, theme.primary.z, 1.0f))
            : ImGui::ColorConvertFloat4ToU32(ImVec4(theme.inputBorder.x, theme.inputBorder.y, theme.inputBorder.z, 0.8f));
        dl->AddCircle(ImVec2(cx, cy), radioRadius, outerCol, 0, 1.5f);

        // Inner filled circle for selected
        if (isSelected) {
            ImU32 innerCol = ImGui::ColorConvertFloat4ToU32(
                ImVec4(theme.primary.x, theme.primary.y, theme.primary.z, 1.0f));
            dl->AddCircleFilled(ImVec2(cx, cy), radioRadius * 0.55f, innerCol);
        }

        // Hover highlight on the radio circle background
        ImGuiIO& io = ImGui::GetIO();
        UIRect itemRect;
        itemRect.x = element.computedRect.x;
        itemRect.y = itemY;
        itemRect.w = element.computedRect.w;
        itemRect.h = itemH;
        if (itemRect.Contains(io.MousePos.x, io.MousePos.y) && !isSelected) {
            ImU32 hoverCircle = ImGui::ColorConvertFloat4ToU32(
                ImVec4(theme.primary.x, theme.primary.y, theme.primary.z, 0.15f));
            dl->AddCircleFilled(ImVec2(cx, cy), radioRadius + 3.0f, hoverCircle);
            // Re-draw outer circle on top of hover
            dl->AddCircle(ImVec2(cx, cy), radioRadius, outerCol, 0, 1.5f);
        }

        // Label text
        UIRect labelRect;
        labelRect.x = cx + radioRadius + 8.0f;
        labelRect.y = itemY;
        labelRect.w = element.computedRect.w - (cx - element.computedRect.x) - radioRadius - 8.0f;
        labelRect.h = itemH;
        DrawCenteredText(dl, labelRect, element.data.options[i].c_str(), textCol, 0, 1, fontSize);
    }
}

// ============================================================================
// SCROLL AREA
// ============================================================================

void UISystem::RenderScrollArea(const UIElement& element, const UITheme& theme,
                                const UICanvasComponent& canvas) {
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    f32 radius = ResolveFloat(element.style.borderRadius, theme.borderRadius);
    f32 alpha  = ResolveFloat(element.style.bgAlpha, theme.bgAlpha);

    // Background panel
    ImVec4 bgColor = ResolveColor(element.style.bgColor, theme.surface, alpha);
    DrawRoundedRect(dl, element.computedRect, ImGui::ColorConvertFloat4ToU32(bgColor), radius);

    // Border
    f32 borderW = ResolveFloat(element.style.borderWidth, theme.borderWidth);
    if (borderW > 0.0f) {
        ImVec4 borderColor = ResolveColor(element.style.borderColor, theme.inputBorder, 0.5f);
        DrawRoundedRectBorder(dl, element.computedRect, ImGui::ColorConvertFloat4ToU32(borderColor), radius, borderW);
    }

    if (element.childIds.empty()) return;

    // Calculate content height from children
    f32 contentH = 0.0f;
    for (u32 childId : element.childIds) {
        const UIElement* child = canvas.GetElement(childId);
        if (child && child->visible && !child->worldCulled) {
            f32 childBottom = (child->computedRect.y + child->computedRect.h) - element.computedRect.y;
            if (childBottom > contentH) contentH = childBottom;
        }
    }

    // Clamp scroll offset
    f32& scrollOff = const_cast<UIElement&>(element).data.scrollOffset;
    f32 maxScroll = std::max(0.0f, contentH - element.computedRect.h);
    scrollOff = std::max(0.0f, std::min(scrollOff, maxScroll));

    // Set up clip rect
    dl->PushClipRect(
        ImVec2(element.computedRect.x, element.computedRect.y),
        ImVec2(element.computedRect.x + element.computedRect.w,
               element.computedRect.y + element.computedRect.h), true);

    // Render children with scroll offset
    for (u32 childId : element.childIds) {
        const UIElement* child = canvas.GetElement(childId);
        if (!child || !child->visible || child->worldCulled) continue;

        // Create a temporary element with offset rect for rendering
        UIElement shifted = *child;
        shifted.computedRect.y -= scrollOff;

        // Skip if fully outside clip region
        if (shifted.computedRect.y + shifted.computedRect.h < element.computedRect.y ||
            shifted.computedRect.y > element.computedRect.y + element.computedRect.h) {
            continue;
        }

        RenderElement(shifted, theme, 0, canvas);
    }

    dl->PopClipRect();

    // Draw scrollbar if content exceeds bounds
    if (contentH > element.computedRect.h && maxScroll > 0.0f) {
        f32 scrollbarW = 6.0f;
        f32 trackH = element.computedRect.h;
        f32 thumbRatio = element.computedRect.h / contentH;
        f32 thumbH = std::max(20.0f, trackH * thumbRatio);
        f32 scrollRatio = scrollOff / maxScroll;
        f32 thumbY = element.computedRect.y + scrollRatio * (trackH - thumbH);

        // Track
        UIRect trackRect;
        trackRect.x = element.computedRect.x + element.computedRect.w - scrollbarW - 2.0f;
        trackRect.y = element.computedRect.y;
        trackRect.w = scrollbarW;
        trackRect.h = trackH;
        DrawRoundedRect(dl, trackRect,
            ImGui::ColorConvertFloat4ToU32(ImVec4(theme.sliderTrack.x, theme.sliderTrack.y, theme.sliderTrack.z, 0.3f)),
            scrollbarW * 0.5f);

        // Thumb
        UIRect thumbRect;
        thumbRect.x = trackRect.x;
        thumbRect.y = thumbY;
        thumbRect.w = scrollbarW;
        thumbRect.h = thumbH;
        f32 thumbAlpha = element.interaction.hovered ? 0.8f : 0.5f;
        DrawRoundedRect(dl, thumbRect,
            ImGui::ColorConvertFloat4ToU32(ImVec4(theme.sliderThumb.x, theme.sliderThumb.y, theme.sliderThumb.z, thumbAlpha)),
            scrollbarW * 0.5f);
    }
}

// ============================================================================
// GRID
// ============================================================================

void UISystem::RenderGrid(const UIElement& element, const UITheme& theme,
                          const UICanvasComponent& canvas, u32 focusedId) {
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    f32 radius = ResolveFloat(element.style.borderRadius, theme.borderRadius);
    f32 alpha  = ResolveFloat(element.style.bgAlpha, theme.bgAlpha);

    // Optional background
    if (element.style.HasBgColor()) {
        ImVec4 bgColor = ResolveColor(element.style.bgColor, theme.surface, alpha);
        DrawRoundedRect(dl, element.computedRect, ImGui::ColorConvertFloat4ToU32(bgColor), radius);
    }

    if (element.childIds.empty()) return;

    i32 cols = std::max(1, element.data.gridColumns);
    f32 cellW = element.computedRect.w / static_cast<f32>(cols);
    f32 spacing = theme.spacing;

    // Calculate cell height from first child's height, or use a reasonable default
    f32 cellH = 0.0f;
    for (u32 childId : element.childIds) {
        const UIElement* child = canvas.GetElement(childId);
        if (child && child->visible && !child->worldCulled) {
            cellH = child->computedRect.h;
            break;
        }
    }
    if (cellH <= 0.0f) cellH = cellW; // Square cells as fallback

    for (i32 i = 0; i < static_cast<i32>(element.childIds.size()); ++i) {
        const UIElement* child = canvas.GetElement(element.childIds[i]);
        if (!child || !child->visible || child->worldCulled) continue;

        i32 col = i % cols;
        i32 row = i / cols;

        // Override child position into grid cell
        UIElement positioned = *child;
        positioned.computedRect.x = element.computedRect.x + static_cast<f32>(col) * cellW + spacing * 0.5f;
        positioned.computedRect.y = element.computedRect.y + static_cast<f32>(row) * (cellH + spacing);
        positioned.computedRect.w = cellW - spacing;
        // Keep original height

        RenderElement(positioned, theme, focusedId, canvas);
    }
}

// ============================================================================
// TAB GROUP
// ============================================================================

void UISystem::RenderTabGroup(const UIElement& element, const UITheme& theme,
                              const UICanvasComponent& canvas, u32 focusedId) {
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    f32 radius = ResolveFloat(element.style.borderRadius, theme.borderRadius);
    f32 fontSize = ResolveFloat(element.style.fontSize, theme.fontSizeBody) * m_FontScale;

    if (element.childIds.empty()) return;

    f32 tabBarH = 30.0f;
    i32 numTabs = static_cast<i32>(element.childIds.size());
    f32 tabW = (numTabs > 0) ? element.computedRect.w / static_cast<f32>(numTabs) : element.computedRect.w;
    if (tabW <= 0.0f) tabW = 1.0f;

    // Tab bar background
    UIRect tabBarRect;
    tabBarRect.x = element.computedRect.x;
    tabBarRect.y = element.computedRect.y;
    tabBarRect.w = element.computedRect.w;
    tabBarRect.h = tabBarH;
    ImVec4 tabBarBg = ImVec4(theme.surface.x, theme.surface.y, theme.surface.z, 1.0f);
    DrawRoundedRect(dl, tabBarRect, ImGui::ColorConvertFloat4ToU32(tabBarBg), radius);

    ImGuiIO& io = ImGui::GetIO();
    f32 mouseX = io.MousePos.x;
    f32 mouseY = io.MousePos.y;

    // Render tab buttons
    for (i32 i = 0; i < numTabs; ++i) {
        UIRect tabRect;
        tabRect.x = element.computedRect.x + static_cast<f32>(i) * tabW;
        tabRect.y = element.computedRect.y;
        tabRect.w = tabW;
        tabRect.h = tabBarH;

        bool isActive = (i == element.data.activeTabIndex);
        bool isHovered = tabRect.Contains(mouseX, mouseY);

        // Tab background
        if (isActive) {
            ImVec4 activeColor = ImVec4(theme.primary.x, theme.primary.y, theme.primary.z, 0.2f);
            DrawRoundedRect(dl, tabRect, ImGui::ColorConvertFloat4ToU32(activeColor), radius);
            // Active indicator bar at bottom
            UIRect indicator;
            indicator.x = tabRect.x + 2.0f;
            indicator.y = tabRect.y + tabBarH - 3.0f;
            indicator.w = tabRect.w - 4.0f;
            indicator.h = 3.0f;
            ImU32 indicatorCol = ImGui::ColorConvertFloat4ToU32(
                ImVec4(theme.primary.x, theme.primary.y, theme.primary.z, 1.0f));
            DrawRoundedRect(dl, indicator, indicatorCol, 1.5f);
        } else if (isHovered) {
            ImVec4 hoverColor = ImVec4(theme.primary.x, theme.primary.y, theme.primary.z, 0.08f);
            DrawRoundedRect(dl, tabRect, ImGui::ColorConvertFloat4ToU32(hoverColor), radius);
        }

        // Tab label — use child element name
        const UIElement* child = canvas.GetElement(element.childIds[i]);
        std::string tabLabel = child ? child->name : std::string("Tab ") + std::to_string(i + 1);

        ImVec4 textColor = isActive
            ? ResolveColor(element.style.textColor, theme.textPrimary, 1.0f)
            : ResolveColor(element.style.textColor, theme.textSecondary, 0.8f);
        DrawCenteredText(dl, tabRect, tabLabel.c_str(),
            ImGui::ColorConvertFloat4ToU32(textColor), 1, 1, fontSize);
    }

    // Content area — render only the active tab's child
    UIRect contentRect;
    contentRect.x = element.computedRect.x;
    contentRect.y = element.computedRect.y + tabBarH;
    contentRect.w = element.computedRect.w;
    contentRect.h = element.computedRect.h - tabBarH;

    // Content area background
    ImVec4 contentBg = ResolveColor(element.style.bgColor, theme.surface, 0.5f);
    DrawRoundedRect(dl, contentRect, ImGui::ColorConvertFloat4ToU32(contentBg), 0.0f);

    // Border around entire tab group
    f32 borderW = ResolveFloat(element.style.borderWidth, theme.borderWidth);
    if (borderW > 0.0f) {
        ImVec4 borderColor = ResolveColor(element.style.borderColor, theme.inputBorder, 0.4f);
        DrawRoundedRectBorder(dl, element.computedRect, ImGui::ColorConvertFloat4ToU32(borderColor), radius, borderW);
    }

    // Render active tab's child element and its sub-children
    i32 activeIdx = std::max(0, std::min(element.data.activeTabIndex, numTabs - 1));
    u32 activeChildId = element.childIds[activeIdx];
    const UIElement* activeChild = canvas.GetElement(activeChildId);
    if (activeChild && activeChild->visible) {
        RenderElement(*activeChild, theme, focusedId, canvas);
        // Render grandchildren of active tab
        for (u32 grandchildId : activeChild->childIds) {
            const UIElement* grandchild = canvas.GetElement(grandchildId);
            if (grandchild && grandchild->visible && !grandchild->worldCulled) {
                RenderElement(*grandchild, theme, focusedId, canvas);
            }
        }
    }
}

// ============================================================================
// TOOLTIP
// ============================================================================

void UISystem::RenderTooltip(const UIElement& element, const UITheme& theme) {
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    f32 fontSize = ResolveFloat(element.style.fontSize, theme.fontSizeSmall) * m_FontScale;
    f32 radius = ResolveFloat(element.style.borderRadius, theme.borderRadius);

    const std::string& tipText = element.data.tooltipText;
    if (tipText.empty()) return;

    ImFont* font = ImGui::GetFont();
    ImVec2 textSize = font->CalcTextSizeA(fontSize, FLT_MAX, 300.0f, tipText.c_str());

    f32 padX = 8.0f;
    f32 padY = 4.0f;
    f32 tipW = textSize.x + padX * 2.0f;
    f32 tipH = textSize.y + padY * 2.0f;

    // Position near the element, offset below and slightly right
    f32 tipX = m_TooltipMouseX + 12.0f;
    f32 tipY = m_TooltipMouseY + 18.0f;

    // Clamp to viewport
    ImGuiIO& io = ImGui::GetIO();
    if (tipX + tipW > io.DisplaySize.x) tipX = io.DisplaySize.x - tipW - 4.0f;
    if (tipY + tipH > io.DisplaySize.y) tipY = m_TooltipMouseY - tipH - 4.0f;
    if (tipX < 0.0f) tipX = 4.0f;
    if (tipY < 0.0f) tipY = 4.0f;

    UIRect tipRect;
    tipRect.x = tipX;
    tipRect.y = tipY;
    tipRect.w = tipW;
    tipRect.h = tipH;

    // Shadow
    UIRect shadowRect = tipRect;
    shadowRect.x += 2.0f;
    shadowRect.y += 2.0f;
    DrawRoundedRect(dl, shadowRect, IM_COL32(0, 0, 0, 80), radius);

    // Background
    ImVec4 bgColor = ImVec4(theme.surface.x + 0.05f, theme.surface.y + 0.05f, theme.surface.z + 0.05f, 0.95f);
    DrawRoundedRect(dl, tipRect, ImGui::ColorConvertFloat4ToU32(bgColor), radius);

    // Border
    ImVec4 borderColor = ImVec4(theme.inputBorder.x, theme.inputBorder.y, theme.inputBorder.z, 0.6f);
    DrawRoundedRectBorder(dl, tipRect, ImGui::ColorConvertFloat4ToU32(borderColor), radius, 1.0f);

    // Text
    ImVec4 textColor = ResolveColor(element.style.textColor, theme.textPrimary, 1.0f);
    dl->AddText(font, fontSize, ImVec2(tipX + padX, tipY + padY),
        ImGui::ColorConvertFloat4ToU32(textColor), tipText.c_str(), nullptr, 300.0f);
}

// ============================================================================
// MODAL
// ============================================================================

void UISystem::RenderModal(const UIElement& element, const UITheme& theme,
                           const UICanvasComponent& canvas, u32 focusedId) {
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    f32 radius = ResolveFloat(element.style.borderRadius, theme.borderRadius);

    // Full-screen semi-transparent dim overlay
    ImGuiIO& io = ImGui::GetIO();
    dl->AddRectFilled(
        ImVec2(0.0f, 0.0f),
        ImVec2(io.DisplaySize.x, io.DisplaySize.y),
        IM_COL32(0, 0, 0, 140));

    // Modal panel background (centered)
    f32 alpha = ResolveFloat(element.style.bgAlpha, theme.bgAlpha);
    ImVec4 bgColor = ResolveColor(element.style.bgColor, theme.surface, alpha);

    // Shadow
    UIRect shadowRect = element.computedRect;
    shadowRect.x += 4.0f;
    shadowRect.y += 4.0f;
    DrawRoundedRect(dl, shadowRect, IM_COL32(0, 0, 0, 100), radius + 2.0f);

    // Main panel
    DrawRoundedRect(dl, element.computedRect, ImGui::ColorConvertFloat4ToU32(bgColor), radius);

    // Border
    f32 borderW = ResolveFloat(element.style.borderWidth, theme.borderWidth);
    if (borderW > 0.0f) {
        ImVec4 borderColor = ResolveColor(element.style.borderColor, theme.inputBorder, 0.6f);
        DrawRoundedRectBorder(dl, element.computedRect, ImGui::ColorConvertFloat4ToU32(borderColor), radius, borderW);
    }

    // Title text
    if (!element.data.text.empty()) {
        f32 fontSize = ResolveFloat(element.style.fontSize, theme.fontSizeHeading) * m_FontScale;
        ImVec4 textColor = ResolveColor(element.style.textColor, theme.textPrimary, 1.0f);
        UIRect titleRect = element.computedRect;
        titleRect.h = fontSize + 16.0f;
        DrawCenteredText(dl, titleRect, element.data.text.c_str(),
            ImGui::ColorConvertFloat4ToU32(textColor), 1, 1, fontSize);
    }

    // Render children on top of modal panel
    for (u32 childId : element.childIds) {
        const UIElement* child = canvas.GetElement(childId);
        if (child && child->visible && !child->worldCulled) {
            RenderElement(*child, theme, focusedId, canvas);
            // Render grandchildren
            for (u32 grandchildId : child->childIds) {
                const UIElement* grandchild = canvas.GetElement(grandchildId);
                if (grandchild && grandchild->visible && !grandchild->worldCulled) {
                    RenderElement(*grandchild, theme, focusedId, canvas);
                }
            }
        }
    }
}

// ============================================================================
// LIST VIEW
// ============================================================================

void UISystem::RenderListView(const UIElement& element, const UITheme& theme,
                              const UICanvasComponent& canvas, bool focused) {
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    f32 radius = ResolveFloat(element.style.borderRadius, theme.borderRadius);
    f32 fontSize = ResolveFloat(element.style.fontSize, theme.fontSizeBody) * m_FontScale;

    // Background
    ImVec4 bgColor = ResolveColor(element.style.bgColor, theme.inputBg, 1.0f);
    DrawRoundedRect(dl, element.computedRect, ImGui::ColorConvertFloat4ToU32(bgColor), radius);

    // Border
    f32 borderW = ResolveFloat(element.style.borderWidth, theme.borderWidth);
    Math::Vector3 borderCol = focused ? theme.inputFocused : theme.inputBorder;
    ImVec4 borderColor = ImVec4(borderCol.x, borderCol.y, borderCol.z, focused ? 0.8f : 0.5f);
    DrawRoundedRectBorder(dl, element.computedRect, ImGui::ColorConvertFloat4ToU32(borderColor), radius, borderW);

    if (element.childIds.empty()) return;

    f32 itemH = fontSize + 8.0f;
    f32& scrollOff = const_cast<UIElement&>(element).data.scrollOffset;
    f32 contentH = static_cast<f32>(element.childIds.size()) * itemH;
    f32 maxScroll = std::max(0.0f, contentH - element.computedRect.h);
    scrollOff = std::max(0.0f, std::min(scrollOff, maxScroll));

    // Clip children
    dl->PushClipRect(
        ImVec2(element.computedRect.x, element.computedRect.y),
        ImVec2(element.computedRect.x + element.computedRect.w,
               element.computedRect.y + element.computedRect.h), true);

    ImGuiIO& io = ImGui::GetIO();
    f32 mouseX = io.MousePos.x;
    f32 mouseY = io.MousePos.y;

    ImVec4 textColor = ResolveColor(element.style.textColor, theme.textPrimary, 1.0f);
    ImU32 textCol = ImGui::ColorConvertFloat4ToU32(textColor);

    for (i32 i = 0; i < static_cast<i32>(element.childIds.size()); ++i) {
        UIRect itemRect;
        itemRect.x = element.computedRect.x;
        itemRect.y = element.computedRect.y + static_cast<f32>(i) * itemH - scrollOff;
        itemRect.w = element.computedRect.w;
        itemRect.h = itemH;

        // Skip if outside clip region
        if (itemRect.y + itemRect.h < element.computedRect.y ||
            itemRect.y > element.computedRect.y + element.computedRect.h) {
            continue;
        }

        bool isSelected = (i == element.data.listSelectedIndex);
        bool isHovered = itemRect.Contains(mouseX, mouseY);

        // Selection highlight
        if (isSelected) {
            ImVec4 selColor = ImVec4(theme.primary.x, theme.primary.y, theme.primary.z, 0.3f);
            DrawRoundedRect(dl, itemRect, ImGui::ColorConvertFloat4ToU32(selColor), 0.0f);
        } else if (isHovered) {
            ImVec4 hoverColor = ImVec4(theme.primary.x, theme.primary.y, theme.primary.z, 0.1f);
            DrawRoundedRect(dl, itemRect, ImGui::ColorConvertFloat4ToU32(hoverColor), 0.0f);
        }

        // Separator line between items
        if (i > 0) {
            ImU32 sepCol = ImGui::ColorConvertFloat4ToU32(
                ImVec4(theme.inputBorder.x, theme.inputBorder.y, theme.inputBorder.z, 0.2f));
            dl->AddLine(
                ImVec2(itemRect.x + 4.0f, itemRect.y),
                ImVec2(itemRect.x + itemRect.w - 4.0f, itemRect.y),
                sepCol, 1.0f);
        }

        // Render child element content — use child name as fallback text
        const UIElement* child = canvas.GetElement(element.childIds[i]);
        if (child) {
            std::string label = child->data.text.empty() ? child->name : child->data.text;
            UIRect textRect = itemRect;
            textRect.x += 8.0f;
            textRect.w -= 16.0f;
            DrawCenteredText(dl, textRect, label.c_str(), textCol, 0, 1, fontSize);
        }
    }

    dl->PopClipRect();

    // Scrollbar
    if (contentH > element.computedRect.h && maxScroll > 0.0f) {
        f32 scrollbarW = 5.0f;
        f32 trackH = element.computedRect.h;
        f32 thumbRatio = element.computedRect.h / contentH;
        f32 thumbH = std::max(16.0f, trackH * thumbRatio);
        f32 scrollRatio = scrollOff / maxScroll;
        f32 thumbY = element.computedRect.y + scrollRatio * (trackH - thumbH);

        UIRect thumbRect;
        thumbRect.x = element.computedRect.x + element.computedRect.w - scrollbarW - 2.0f;
        thumbRect.y = thumbY;
        thumbRect.w = scrollbarW;
        thumbRect.h = thumbH;
        f32 thumbAlpha = element.interaction.hovered ? 0.7f : 0.4f;
        DrawRoundedRect(dl, thumbRect,
            ImGui::ColorConvertFloat4ToU32(ImVec4(theme.sliderThumb.x, theme.sliderThumb.y, theme.sliderThumb.z, thumbAlpha)),
            scrollbarW * 0.5f);
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
    f32 fontSize = ResolveFloat(element.style.fontSize, theme.fontSizeSmall) * m_FontScale;
    ImVec4 textColor = ResolveColor(element.style.textColor, theme.textSecondary, 0.7f);
    DrawCenteredText(dl, element.computedRect, label.c_str(),
        ImGui::ColorConvertFloat4ToU32(textColor), 1, 1, fontSize);
}

// ============================================================================
// FOCUS MANAGEMENT
// ============================================================================

void UISystem::SetFocus(UICanvasComponent& canvas, u32 elementId) {
    if (canvas.focusedElementId == elementId) return;
    canvas.focusedElementId = elementId;

    // Announce focus change for screen readers (Task #36: rich announcements)
    if (m_AnnouncerCallback && elementId != 0) {
        const UIElement* el = canvas.GetElement(elementId);
        if (el) {
            std::string announcement = el->accessibleLabel.empty() ? el->name : el->accessibleLabel;

            // Append widget type and state context
            switch (el->type) {
                case UIWidgetType::Button:
                    if (!el->data.text.empty()) announcement += ": " + el->data.text;
                    announcement += ", Button";
                    break;
                case UIWidgetType::Slider: {
                    f32 range = el->data.sliderMax - el->data.sliderMin;
                    i32 pct = (range > 0.0f)
                        ? static_cast<i32>(((el->data.sliderValue - el->data.sliderMin) / range) * 100.0f)
                        : 0;
                    announcement += ", Slider at " + std::to_string(pct) + "%";
                    break;
                }
                case UIWidgetType::Checkbox:
                    announcement += el->data.checked ? ", Checkbox checked" : ", Checkbox unchecked";
                    break;
                case UIWidgetType::Toggle:
                    announcement += el->data.checked ? ", Toggle on" : ", Toggle off";
                    break;
                case UIWidgetType::ProgressBar: {
                    i32 pct = static_cast<i32>(el->data.progressValue * 100.0f);
                    announcement += ", ProgressBar at " + std::to_string(pct) + "%";
                    break;
                }
                case UIWidgetType::Label:
                    if (!el->data.text.empty()) announcement += ": " + el->data.text;
                    break;
                case UIWidgetType::Dropdown:
                    if (el->data.selectedOption >= 0 &&
                        el->data.selectedOption < static_cast<i32>(el->data.options.size())) {
                        announcement += ": " + el->data.options[el->data.selectedOption];
                    }
                    announcement += ", Dropdown";
                    break;
                case UIWidgetType::TextInput:
                    if (!el->data.inputText.empty()) announcement += ": " + el->data.inputText;
                    announcement += ", Text field";
                    break;
                case UIWidgetType::RadioGroup:
                    if (el->data.selectedOption >= 0 &&
                        el->data.selectedOption < static_cast<i32>(el->data.options.size())) {
                        announcement += ": " + el->data.options[el->data.selectedOption] + " selected";
                    }
                    announcement += ", Radio group";
                    break;
                case UIWidgetType::ListView:
                    announcement += ", List view";
                    if (el->data.listSelectedIndex >= 0) {
                        announcement += ", item " + std::to_string(el->data.listSelectedIndex + 1) + " selected";
                    }
                    break;
                default:
                    if (!el->data.text.empty()) announcement += ": " + el->data.text;
                    break;
            }

            // Append parent context if available
            if (el->parentId != 0) {
                const UIElement* parent = canvas.GetElement(el->parentId);
                if (parent && !parent->name.empty()) {
                    announcement += ", in " + parent->name;
                }
            }

            m_AnnouncerCallback(announcement);
        }
    }
}

void UISystem::ClearFocus(UICanvasComponent& canvas) {
    canvas.focusedElementId = 0;
}

bool UISystem::IsElementFocusable(const UIElement& element) const {
    return element.focusable && element.visible && element.enabled &&
           IsFocusableType(element.type);
}

std::vector<UIElement*> UISystem::BuildTabOrder(UICanvasComponent& canvas) {
    std::vector<UIElement*> focusable;
    for (auto& el : canvas.elements) {
        if (IsElementFocusable(el)) {
            focusable.push_back(&el);
        }
    }

    // Sort: explicit tabOrder (>0) first in ascending order, then auto (0) in element order
    std::stable_sort(focusable.begin(), focusable.end(),
        [](const UIElement* a, const UIElement* b) {
            if (a->tabOrder > 0 && b->tabOrder > 0) return a->tabOrder < b->tabOrder;
            if (a->tabOrder > 0) return true;   // Explicit before auto
            if (b->tabOrder > 0) return false;
            return false; // Preserve original order for auto
        });

    return focusable;
}

void UISystem::ProcessFocusNavigation(UICanvasComponent& canvas, f32 deltaTime) {
    ImGuiIO& io = ImGui::GetIO();

    // Switch access auto-scan mode: cycle focus automatically, single input to select
    if (m_SwitchAccessEnabled) {
        if (!m_ReducedMotion) {
            m_SwitchPulsePhase += deltaTime * 4.0f;
            if (m_SwitchPulsePhase > 6.2832f) m_SwitchPulsePhase -= 6.2832f;
        } else {
            m_SwitchPulsePhase = 0.0f;  // Static appearance
        }

        m_SwitchScanTimer += deltaTime;
        if (m_SwitchScanTimer >= m_SwitchScanSpeed) {
            m_SwitchScanTimer -= m_SwitchScanSpeed;
            // Advance focus to next element
            auto focusable = BuildTabOrder(canvas);
            if (!focusable.empty()) {
                m_SwitchScanIndex = (m_SwitchScanIndex + 1) % static_cast<i32>(focusable.size());
                canvas.focusedElementId = focusable[m_SwitchScanIndex]->id;

                // Build rich announcement: label + widget type context
                if (m_AnnouncerCallback) {
                    const auto& el = *focusable[m_SwitchScanIndex];
                    std::string announcement = el.accessibleLabel.empty() ? el.name : el.accessibleLabel;

                    // Append widget type context
                    switch (el.type) {
                        case UIWidgetType::Button:
                            announcement += ", Button";
                            break;
                        case UIWidgetType::Slider: {
                            f32 range = el.data.sliderMax - el.data.sliderMin;
                            i32 pct = (range > 0.0f)
                                ? static_cast<i32>(((el.data.sliderValue - el.data.sliderMin) / range) * 100.0f)
                                : 0;
                            announcement += ", Slider at " + std::to_string(pct) + "%";
                            break;
                        }
                        case UIWidgetType::Checkbox:
                            announcement += el.data.checked ? ", Checkbox checked" : ", Checkbox unchecked";
                            break;
                        case UIWidgetType::Toggle:
                            announcement += el.data.checked ? ", Toggle on" : ", Toggle off";
                            break;
                        default:
                            break;
                    }

                    m_AnnouncerCallback(announcement);
                }
            }
        }
        // Check for activation (single input)
        if (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_Space) ||
            ImGui::IsKeyPressed(ImGuiKey_GamepadFaceDown)) {
            ActivateFocusedElement(canvas);
        }
        return; // Skip normal navigation in switch access mode
    }

    // Check for Tab / Shift+Tab
    bool tabPressed = ImGui::IsKeyPressed(ImGuiKey_Tab);
    bool shiftHeld = io.KeyShift;

    // Check for DPad / Arrow keys with repeat
    bool navDown  = ImGui::IsKeyPressed(ImGuiKey_DownArrow)  || ImGui::IsKeyPressed(ImGuiKey_GamepadDpadDown);
    bool navUp    = ImGui::IsKeyPressed(ImGuiKey_UpArrow)    || ImGui::IsKeyPressed(ImGuiKey_GamepadDpadUp);
    bool navRight = ImGui::IsKeyPressed(ImGuiKey_RightArrow) || ImGui::IsKeyPressed(ImGuiKey_GamepadDpadRight);
    bool navLeft  = ImGui::IsKeyPressed(ImGuiKey_LeftArrow)  || ImGui::IsKeyPressed(ImGuiKey_GamepadDpadLeft);

    // Key repeat for held keys
    bool anyNavHeld = ImGui::IsKeyDown(ImGuiKey_DownArrow) || ImGui::IsKeyDown(ImGuiKey_UpArrow) ||
                      ImGui::IsKeyDown(ImGuiKey_GamepadDpadDown) || ImGui::IsKeyDown(ImGuiKey_GamepadDpadUp);
    if (anyNavHeld) {
        m_NavRepeatTimer += deltaTime;
        if (m_NavRepeatTimer > NAV_REPEAT_DELAY) {
            f32 elapsed = m_NavRepeatTimer - NAV_REPEAT_DELAY;
            if (std::fmod(elapsed, NAV_REPEAT_RATE) < deltaTime) {
                // Fire repeat
                if (ImGui::IsKeyDown(ImGuiKey_DownArrow) || ImGui::IsKeyDown(ImGuiKey_GamepadDpadDown)) navDown = true;
                if (ImGui::IsKeyDown(ImGuiKey_UpArrow) || ImGui::IsKeyDown(ImGuiKey_GamepadDpadUp)) navUp = true;
            }
        }
    } else {
        m_NavRepeatTimer = 0.0f;
    }

    // Navigate forward/backward
    bool forward  = tabPressed && !shiftHeld;
    bool backward = (tabPressed && shiftHeld) || navUp;
    if (navDown) forward = true;

    if (forward || backward) {
        auto focusable = BuildTabOrder(canvas);
        if (focusable.empty()) return;

        // Find current index
        i32 currentIdx = -1;
        for (i32 i = 0; i < static_cast<i32>(focusable.size()); ++i) {
            if (focusable[i]->id == canvas.focusedElementId) {
                currentIdx = i;
                break;
            }
        }

        i32 newIdx;
        if (forward) {
            newIdx = (currentIdx + 1) % static_cast<i32>(focusable.size());
        } else {
            newIdx = (currentIdx <= 0)
                ? static_cast<i32>(focusable.size()) - 1
                : currentIdx - 1;
        }

        SetFocus(canvas, focusable[newIdx]->id);
        return; // Don't process activation in same frame as navigation
    }

    // Left/Right for slider adjustment when focused
    if ((navLeft || navRight) && canvas.focusedElementId != 0) {
        UIElement* focused = canvas.GetElement(canvas.focusedElementId);
        if (focused && focused->type == UIWidgetType::Slider) {
            AdjustSliderValue(*focused, navRight ? 1 : -1);
            return;
        }
    }

    // Activation: Enter / Space / Gamepad A
    bool activate = ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_Space) ||
                    ImGui::IsKeyPressed(ImGuiKey_GamepadFaceDown);
    if (activate && canvas.focusedElementId != 0) {
        ActivateFocusedElement(canvas);
    }
}

void UISystem::ActivateFocusedElement(UICanvasComponent& canvas) {
    UIElement* element = canvas.GetElement(canvas.focusedElementId);
    if (!element || !element->enabled) return;

    if (element->type == UIWidgetType::Button) {
        if (!element->onClickEvent.empty()) {
            UIEventData event;
            event.elementId = element->id;
            event.eventName = element->onClickEvent;
            event.stringValue = element->data.text;
            m_EventBus.Dispatch(event);
        }
    } else if (element->type == UIWidgetType::Checkbox || element->type == UIWidgetType::Toggle) {
        element->data.checked = !element->data.checked;
        if (!element->onValueChangedEvent.empty()) {
            UIEventData event;
            event.elementId = element->id;
            event.eventName = element->onValueChangedEvent;
            event.boolValue = element->data.checked;
            m_EventBus.Dispatch(event);
        }
    } else if (element->type == UIWidgetType::Dropdown) {
        // Toggle dropdown open/close on activation
        if (element->data.dropdownOpen) {
            element->data.dropdownOpen = false;
            m_OpenDropdownId = 0;
        } else {
            // Close any other open dropdown
            if (m_OpenDropdownId != 0 && m_OpenDropdownId != element->id) {
                UIElement* other = canvas.GetElement(m_OpenDropdownId);
                if (other) other->data.dropdownOpen = false;
            }
            element->data.dropdownOpen = true;
            m_OpenDropdownId = element->id;
        }
    } else if (element->type == UIWidgetType::RadioGroup) {
        // Cycle to next option
        if (!element->data.options.empty()) {
            element->data.selectedOption = (element->data.selectedOption + 1) % static_cast<i32>(element->data.options.size());
            if (!element->onValueChangedEvent.empty()) {
                UIEventData event;
                event.elementId = element->id;
                event.eventName = element->onValueChangedEvent;
                event.intValue = element->data.selectedOption;
                event.stringValue = element->data.options[element->data.selectedOption];
                m_EventBus.Dispatch(event);
            }
        }
    }
}

void UISystem::AdjustSliderValue(UIElement& element, i32 direction) {
    f32 range = element.data.sliderMax - element.data.sliderMin;
    f32 step = range * 0.05f; // 5% per key press
    f32 newValue = element.data.sliderValue + step * static_cast<f32>(direction);
    newValue = std::max(element.data.sliderMin, std::min(element.data.sliderMax, newValue));

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

void UISystem::RenderFocusIndicator(const UIElement& element, const UITheme& theme) {
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    f32 radius = ResolveFloat(element.style.borderRadius, theme.borderRadius) + 2.0f;
    f32 focusWidth = theme.focusBorderWidth;

    // Use element focus color override or theme inputFocused
    Math::Vector3 focusColor = element.style.HasFocusColor()
        ? element.style.focusColor
        : theme.inputFocused;

    // Draw outset border (slightly larger than element rect)
    f32 outset = focusWidth;
    UIRect focusRect;
    focusRect.x = element.computedRect.x - outset;
    focusRect.y = element.computedRect.y - outset;
    focusRect.w = element.computedRect.w + outset * 2.0f;
    focusRect.h = element.computedRect.h + outset * 2.0f;

    ImU32 color = ImGui::ColorConvertFloat4ToU32(
        ImVec4(focusColor.x, focusColor.y, focusColor.z, 0.85f));
    DrawRoundedRectBorder(dl, focusRect, color, radius, focusWidth);
}

// ============================================================================
// SWITCH ACCESS SCAN INDICATOR (Task #34)
// ============================================================================

void UISystem::RenderScanIndicator(const UIElement& element, const UITheme& theme) {
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    f32 radius = ResolveFloat(element.style.borderRadius, theme.borderRadius) + 3.0f;

    // Animated pulse: border thickness oscillates between 2.5 and 4.5
    // (static when reduced motion is enabled)
    f32 pulse = m_ReducedMotion ? 0.5f : (0.5f + 0.5f * std::sin(m_SwitchPulsePhase));
    f32 thickness = 2.5f + pulse * 2.0f;

    // Use a bright scanning color (cyan-blue)
    f32 alpha = 0.7f + pulse * 0.3f;
    ImU32 scanColor = ImGui::ColorConvertFloat4ToU32(
        ImVec4(0.3f, 0.8f, 1.0f, alpha));

    // Outer glow (larger, semi-transparent)
    f32 glowOutset = thickness + 2.0f;
    UIRect glowRect;
    glowRect.x = element.computedRect.x - glowOutset;
    glowRect.y = element.computedRect.y - glowOutset;
    glowRect.w = element.computedRect.w + glowOutset * 2.0f;
    glowRect.h = element.computedRect.h + glowOutset * 2.0f;
    ImU32 glowColor = ImGui::ColorConvertFloat4ToU32(
        ImVec4(0.3f, 0.8f, 1.0f, pulse * 0.25f));
    DrawRoundedRectBorder(dl, glowRect, glowColor, radius + 2.0f, 2.0f);

    // Main border
    f32 outset = thickness;
    UIRect scanRect;
    scanRect.x = element.computedRect.x - outset;
    scanRect.y = element.computedRect.y - outset;
    scanRect.w = element.computedRect.w + outset * 2.0f;
    scanRect.h = element.computedRect.h + outset * 2.0f;
    DrawRoundedRectBorder(dl, scanRect, scanColor, radius, thickness);

    // "SCAN" label indicator at top-right
    const char* scanLabel = "[SCAN]";
    ImVec2 labelSize = ImGui::CalcTextSize(scanLabel);
    f32 labelX = scanRect.x + scanRect.w - labelSize.x - 2.0f;
    f32 labelY = scanRect.y - labelSize.y - 2.0f;
    if (labelY >= 0.0f) {
        dl->AddRectFilled(
            ImVec2(labelX - 3.0f, labelY - 1.0f),
            ImVec2(labelX + labelSize.x + 3.0f, labelY + labelSize.y + 1.0f),
            IM_COL32(20, 60, 100, static_cast<u8>(200 * alpha)), 3.0f);
        dl->AddText(ImVec2(labelX, labelY),
            IM_COL32(100, 200, 255, static_cast<u8>(255 * alpha)), scanLabel);
    }
}

// ============================================================================
// DWELL-CLICK PROGRESS INDICATOR (Task #40)
// ============================================================================

void UISystem::RenderDwellProgress(const UIElement& element, f32 progress) {
    if (progress <= 0.0f || progress > 1.0f) return;

    ImDrawList* dl = ImGui::GetForegroundDrawList();

    // Draw a circular progress indicator at the top-right of the element
    f32 cx = element.computedRect.x + element.computedRect.w - 8.0f;
    f32 cy = element.computedRect.y + 8.0f;
    f32 radius = 8.0f;

    // Background circle
    dl->AddCircleFilled(ImVec2(cx, cy), radius, IM_COL32(0, 0, 0, 150), 16);

    // Progress arc
    f32 startAngle = -1.5708f; // -PI/2 (top)
    f32 endAngle = startAngle + progress * 6.2832f;
    i32 segments = static_cast<i32>(progress * 20.0f) + 1;
    for (i32 i = 0; i < segments; ++i) {
        f32 a0 = startAngle + (static_cast<f32>(i) / static_cast<f32>(segments)) * progress * 6.2832f;
        f32 a1 = startAngle + (static_cast<f32>(i + 1) / static_cast<f32>(segments)) * progress * 6.2832f;
        if (a1 > endAngle) a1 = endAngle;
        dl->AddLine(
            ImVec2(cx + std::cos(a0) * radius, cy + std::sin(a0) * radius),
            ImVec2(cx + std::cos(a1) * radius, cy + std::sin(a1) * radius),
            IM_COL32(50, 200, 255, 240), 2.5f);
    }
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
