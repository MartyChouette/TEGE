#include "Enjin/Gameplay/InteractionSystem.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/CameraZones.h"
#include "Enjin/ECS/EntityEventBus.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/ECS/Components/Hierarchy.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Systems/RenderSystem.h"
#include "Enjin/Input/InputAction.h"
#include "Enjin/Accessibility/Announcer.h"
#include <imgui.h>
#include <algorithm>
#include <cmath>

namespace Enjin {
namespace Gameplay {

void InteractionSystem::Update(f32 deltaTime) {
    const bool pressed = m_InputMap && m_InputMap->IsActionPressed(InputSystem::GameAction::Interact);
    Update(deltaTime, pressed);
}

void InteractionSystem::Update(f32 deltaTime, bool interactPressed) {
    (void)deltaTime;
    if (!m_World) return;

    // Who is reaching, and where they are looking
    const ECS::Entity player = ECS::FindCameraZonePlayer(m_World);
    const ECS::Entity camera = ECS::ResolveGameCamera(m_World);
    Math::Vector3 camPos, camFwd(0.0f, 0.0f, -1.0f), reachFrom;
    bool haveCamera = false;
    if (camera != ECS::INVALID_ENTITY && m_World->GetComponent<ECS::TransformComponent>(camera)) {
        Math::Quaternion rot;
        ECS::GetWorldTransform(m_World, camera, camPos, rot);
        camFwd = rot.Rotate(Math::Vector3(0.0f, 0.0f, -1.0f));
        haveCamera = true;
    }
    if (player != ECS::INVALID_ENTITY && m_World->GetComponent<ECS::TransformComponent>(player)) {
        Math::Quaternion rot;
        ECS::GetWorldTransform(m_World, player, reachFrom, rot);
    } else if (haveCamera) {
        reachFrom = camPos;
    } else {
        Reset();
        return;
    }

    // The nearest one in reach (and in view, where it asks for that)
    ECS::Entity best = ECS::INVALID_ENTITY;
    f32 bestDist = INFINITY;
    for (ECS::Entity e : m_World->GetEntitiesWithComponent<ECS::InteractableComponent>()) {
        if (e == player) continue;
        const auto* ic = m_World->GetComponent<ECS::InteractableComponent>(e);
        if (!ic || !ic->isEnabled || (ic->singleUse && ic->hasBeenUsed)) continue;
        if (!m_World->GetComponent<ECS::TransformComponent>(e)) continue;
        Math::Vector3 pos;
        Math::Quaternion rot;
        ECS::GetWorldTransform(m_World, e, pos, rot);
        const f32 dist = (pos - reachFrom).Length();
        if (dist > ic->interactionRange || dist >= bestDist) continue;
        if (ic->requiresLookAt) {
            if (!haveCamera) continue;
            Math::Vector3 toIt = pos - camPos;
            const f32 len = toIt.Length();
            if (len > 1e-4f) {
                const f32 cosAngle = std::clamp(toIt.Dot(camFwd) / len, -1.0f, 1.0f);
                if (std::acos(cosAngle) * 180.0f / 3.14159265f > ic->lookAtAngle) continue;
            }
        }
        best = e;
        bestDist = dist;
    }

    // Focus, prompt and highlight follow the choice
    if (best != m_Focused) {
        m_Focused = best;
        m_Prompt.clear();
        if (const auto* ic = m_World->GetComponent<ECS::InteractableComponent>(best)) {
            m_Prompt = m_InputMap ? m_InputMap->ResolvePromptText(ic->promptText) : ic->promptText;
            // Once, on focusing: a line re-announced every frame would never stop
            if (m_Announcer && !m_Prompt.empty()) m_Announcer->Announce(m_Prompt);
        }
    }
    if (m_RenderSystem) {
        const auto* ic = m_World->GetComponent<ECS::InteractableComponent>(m_Focused);
        if (ic && ic->highlightOnHover) m_RenderSystem->SetInteractionFocus(m_Focused, ic->highlightColor);
        else m_RenderSystem->SetInteractionFocus(ECS::INVALID_ENTITY, Math::Vector3(0.0f));
    }

    if (!interactPressed || m_Focused == ECS::INVALID_ENTITY) return;
    auto* ic = m_World->GetComponent<ECS::InteractableComponent>(m_Focused);
    if (!ic) return;
    if (ic->singleUse) ic->hasBeenUsed = true;
    if (m_EventBus && !ic->interactEvent.empty()) {
        ECS::EntityEvent ev;
        ev.name = ic->interactEvent;
        ev.sender = m_Focused;
        if (ic->onInteractNotify != ECS::INVALID_ENTITY && m_World->IsValid(ic->onInteractNotify)) {
            ev.target = ic->onInteractNotify;
        }
        if (player != ECS::INVALID_ENTITY) ev.entities["interactor"] = player;
        // ic is not touched after this: a listener may add components
        m_EventBus->Send(ev.name, ev);
    }
}

void InteractionSystem::Reset() {
    m_Focused = ECS::INVALID_ENTITY;
    m_Prompt.clear();
    if (m_RenderSystem) m_RenderSystem->SetInteractionFocus(ECS::INVALID_ENTITY, Math::Vector3(0.0f));
}

void InteractionSystem::RenderOverlay(f32 originX, f32 originY, u32 viewportWidth, u32 viewportHeight) {
    if (m_Focused == ECS::INVALID_ENTITY || m_Prompt.empty()) return;
    if (viewportWidth == 0 || viewportHeight == 0) return;

    ImDrawList* dl = ImGui::GetForegroundDrawList();
    const f32 w = static_cast<f32>(viewportWidth);
    const f32 h = static_cast<f32>(viewportHeight);
    const f32 padX = 12.0f, padY = 7.0f;
    const ImVec2 size = ImGui::CalcTextSize(m_Prompt.c_str());
    const f32 lineH = ImGui::GetTextLineHeight();
    // Below the middle, clear of the subtitle line at the bottom and of the
    // save indicator in the upper third
    const ImVec2 boxMin(originX + (w - size.x) * 0.5f - padX, originY + h * 0.68f);
    const ImVec2 boxMax(boxMin.x + size.x + padX * 2.0f, boxMin.y + lineH + padY * 2.0f);
    dl->AddRectFilled(boxMin, boxMax, IM_COL32(0, 0, 0, 150), 6.0f);
    dl->AddText(ImVec2(boxMin.x + padX, boxMin.y + padY), IM_COL32(255, 255, 255, 255), m_Prompt.c_str());
}

} // namespace Gameplay
} // namespace Enjin
