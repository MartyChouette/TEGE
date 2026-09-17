#include "Enjin/Gameplay/SaveIndicator.h"
#include "Enjin/ECS/Components/Gameplay.h"

#include <imgui.h>

#include <algorithm>

namespace Enjin {
namespace Gameplay {

namespace {

// The scene's save configuration, or the defaults when no game manager carries
// one. A scene with no SaveSystemComponent still gets feedback: the component
// is how you CHANGE this, not how you switch it on.
const ECS::SaveSystemComponent* FindConfig(ECS::World* world) {
    if (!world) return nullptr;
    for (ECS::Entity e : world->GetEntitiesWithComponent<ECS::SaveSystemComponent>()) {
        if (const auto* c = world->GetComponent<ECS::SaveSystemComponent>(e)) return c;
    }
    return nullptr;
}

ECS::SaveSystemComponent* FindLiveConfig(ECS::World* world) {
    if (!world) return nullptr;
    for (ECS::Entity e : world->GetEntitiesWithComponent<ECS::SaveSystemComponent>()) {
        if (auto* c = world->GetComponent<ECS::SaveSystemComponent>(e)) return c;
    }
    return nullptr;
}

} // namespace

void SaveIndicator::Show(const std::string& text, f32 duration) {
    const ECS::SaveSystemComponent* cfg = FindConfig(m_World);
    if (cfg && !cfg->showSaveIndicator) return;   // the project asked for none

    if (duration <= 0.0f) {
        duration = cfg ? cfg->saveIndicatorDuration
                       : ECS::SaveSystemComponent{}.saveIndicatorDuration;
    }
    m_Text = text;
    m_Duration = duration;
    m_Remaining = duration;

    // The component's own runtime pair, kept on the same clock as this. They
    // were declared for exactly this and nothing had ever written them.
    if (auto* live = FindLiveConfig(m_World)) {
        live->isSaving = true;
        live->saveIndicatorTimer = duration;
    }
}

void SaveIndicator::Update(f32 deltaTime) {
    if (m_Remaining > 0.0f) {
        m_Remaining -= deltaTime;
        if (m_Remaining <= 0.0f) {
            m_Remaining = 0.0f;
            m_Text.clear();
        }
    }

    // Driven from here rather than separately, so a HUD reading the component
    // and a player reading the screen cannot see different things.
    if (auto* live = FindLiveConfig(m_World)) {
        if (live->saveIndicatorTimer > 0.0f) {
            live->saveIndicatorTimer -= deltaTime;
            if (live->saveIndicatorTimer <= 0.0f) {
                live->saveIndicatorTimer = 0.0f;
                live->isSaving = false;
            }
        }
    }
}

void SaveIndicator::RenderOverlay(f32 originX, f32 originY,
                                  u32 viewportWidth, u32 viewportHeight) {
    if (m_Text.empty() || m_Remaining <= 0.0f) return;
    if (viewportWidth == 0 || viewportHeight == 0) return;

    ImDrawList* dl = ImGui::GetForegroundDrawList();
    const f32 w = static_cast<f32>(viewportWidth);
    const f32 h = static_cast<f32>(viewportHeight);
    const f32 padX = 12.0f, padY = 7.0f;

    // Fades over its last second rather than vanishing, so it reads as
    // something that happened rather than a flicker.
    const f32 a = (m_Remaining < 1.0f) ? std::max(0.0f, m_Remaining) : 1.0f;
    const int alpha = static_cast<int>(a * 255.0f);

    const ImVec2 size = ImGui::CalcTextSize(m_Text.c_str());
    const f32 lineH = ImGui::GetTextLineHeight();
    // Upper third: clear of the subtitle line at the bottom and of the
    // controls hint in the bottom-left corner.
    const ImVec2 boxMin(originX + (w - size.x) * 0.5f - padX, originY + h * 0.28f);
    const ImVec2 boxMax(boxMin.x + size.x + padX * 2.0f, boxMin.y + lineH + padY * 2.0f);

    dl->AddRectFilled(boxMin, boxMax, IM_COL32(0, 0, 0, std::min(150, alpha)), 6.0f);
    dl->AddText(ImVec2(boxMin.x + padX, boxMin.y + padY),
                IM_COL32(255, 255, 255, std::clamp(alpha, 0, 255)), m_Text.c_str());
}

} // namespace Gameplay
} // namespace Enjin
