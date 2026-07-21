#include "Enjin/Gameplay/HUDSystem.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/ECS/Components/Transform.h"
#include <imgui.h>

namespace Enjin {
namespace Gameplay {

void HUDSystem::Update(ECS::World* world, const Renderer::Camera* gameCamera,
                       f32 viewportX, f32 viewportY, f32 viewportW, f32 viewportH) {
    if (!m_Enabled || !world || viewportW <= 0 || viewportH <= 0) return;

    auto entities = world->GetEntitiesWithComponent<ECS::HUDWidgetComponent>();
    for (auto entity : entities) {
        auto* widget = world->GetComponent<ECS::HUDWidgetComponent>(entity);
        if (!widget || !widget->visible) continue;

        switch (widget->type) {
            case ECS::HUDWidgetComponent::WidgetType::HealthBar: {
                // Try to read from source entity's HealthComponent
                ECS::Entity src = widget->sourceEntity != 0 ? widget->sourceEntity : entity;
                auto* health = world->GetComponent<ECS::HealthComponent>(src);
                f32 pct = health ? health->GetHealthPercent() : widget->currentValue / std::max(widget->maxValue, 0.001f);
                pct = std::max(0.0f, std::min(1.0f, pct));
                DrawHealthBar(widget->anchorX, widget->anchorY, widget->width, widget->height,
                              pct, widget->fillColor, widget->bgColor,
                              viewportX, viewportY, viewportW, viewportH);
                break;
            }
            case ECS::HUDWidgetComponent::WidgetType::ResourceBar: {
                ECS::Entity src = widget->sourceEntity != 0 ? widget->sourceEntity : entity;
                auto* res = world->GetComponent<ECS::ResourceComponent>(src);
                f32 pct = res ? res->GetPercent() : widget->currentValue / std::max(widget->maxValue, 0.001f);
                pct = std::max(0.0f, std::min(1.0f, pct));
                DrawResourceBar(widget->anchorX, widget->anchorY, widget->width, widget->height,
                                pct, widget->fillColor, widget->bgColor, widget->text,
                                viewportX, viewportY, viewportW, viewportH);
                break;
            }
            case ECS::HUDWidgetComponent::WidgetType::Label: {
                // Live coin counter: a Label bound to "coins" shows collected/total
                // by counting PickupType::Coin entities each frame.
                std::string label = widget->text;
                if (widget->bindField == "coins") {
                    // Collected coins are destroyed on pickup, so count what REMAINS
                    // and subtract from the known total (widget->maxValue, set at spawn).
                    int total = static_cast<int>(widget->maxValue);
                    int remaining = 0;
                    for (auto pe : world->GetEntitiesWithComponent<ECS::PickupComponent>()) {
                        auto* pk = world->GetComponent<ECS::PickupComponent>(pe);
                        if (pk && pk->type == ECS::PickupComponent::PickupType::Coin && !pk->isCollected) {
                            ++remaining;
                        }
                    }
                    int got = total - remaining;
                    if (got < 0) got = 0;
                    label += " " + std::to_string(got) + " / " + std::to_string(total);
                }
                DrawLabel(label, widget->anchorX, widget->anchorY,
                          widget->textColor, widget->fontSize,
                          viewportX, viewportY, viewportW, viewportH);
                break;
            }
            case ECS::HUDWidgetComponent::WidgetType::Crosshair:
                DrawCrosshair(viewportX, viewportY, viewportW, viewportH);
                break;
            default:
                break;
        }
    }
}

void HUDSystem::DrawHealthBar(f32 anchorX, f32 anchorY, f32 width, f32 height,
                               f32 fillPct, const Math::Vector3& fillColor, const Math::Vector3& bgColor,
                               f32 vpX, f32 vpY, f32 vpW, f32 vpH) {
    ImVec2 pos(vpX + anchorX * vpW, vpY + anchorY * vpH);
    ImVec2 size(width * vpW, height * vpH);

    auto* drawList = ImGui::GetForegroundDrawList();
    ImU32 bg = IM_COL32((int)(bgColor.x * 255), (int)(bgColor.y * 255), (int)(bgColor.z * 255), 180);
    ImU32 fill = IM_COL32((int)(fillColor.x * 255), (int)(fillColor.y * 255), (int)(fillColor.z * 255), 255);

    drawList->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), bg, 3.0f);
    drawList->AddRectFilled(pos, ImVec2(pos.x + size.x * fillPct, pos.y + size.y), fill, 3.0f);
    drawList->AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y), IM_COL32(255, 255, 255, 100), 3.0f);
}

void HUDSystem::DrawResourceBar(f32 anchorX, f32 anchorY, f32 width, f32 height,
                                 f32 fillPct, const Math::Vector3& fillColor, const Math::Vector3& bgColor,
                                 const std::string& label, f32 vpX, f32 vpY, f32 vpW, f32 vpH) {
    DrawHealthBar(anchorX, anchorY, width, height, fillPct, fillColor, bgColor, vpX, vpY, vpW, vpH);

    if (!label.empty()) {
        ImVec2 pos(vpX + anchorX * vpW + 4.0f, vpY + anchorY * vpH);
        auto* drawList = ImGui::GetForegroundDrawList();
        drawList->AddText(pos, IM_COL32(255, 255, 255, 255), label.c_str());
    }
}

void HUDSystem::DrawLabel(const std::string& text, f32 anchorX, f32 anchorY,
                           const Math::Vector3& textColor, f32 fontSize,
                           f32 vpX, f32 vpY, f32 vpW, f32 vpH) {
    if (text.empty()) return;
    ImVec2 pos(vpX + anchorX * vpW, vpY + anchorY * vpH);
    auto* drawList = ImGui::GetForegroundDrawList();
    ImU32 col = IM_COL32((int)(textColor.x * 255), (int)(textColor.y * 255), (int)(textColor.z * 255), 255);
    // Use default font at scaled size
    drawList->AddText(nullptr, fontSize, pos, col, text.c_str());
}

void HUDSystem::DrawCrosshair(f32 vpX, f32 vpY, f32 vpW, f32 vpH) {
    f32 cx = vpX + vpW * 0.5f;
    f32 cy = vpY + vpH * 0.5f;
    f32 size = 10.0f;
    ImU32 col = IM_COL32(255, 255, 255, 200);
    auto* drawList = ImGui::GetForegroundDrawList();
    drawList->AddLine(ImVec2(cx - size, cy), ImVec2(cx + size, cy), col, 1.5f);
    drawList->AddLine(ImVec2(cx, cy - size), ImVec2(cx, cy + size), col, 1.5f);
}

} // namespace Gameplay
} // namespace Enjin
