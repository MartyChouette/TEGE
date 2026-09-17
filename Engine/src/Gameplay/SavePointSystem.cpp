#include "Enjin/Gameplay/SavePointSystem.h"
#include "Enjin/Gameplay/TieredSaveSystem.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/ECS/Components/Controllers/CharacterController.h"
#include "Enjin/Input/InputAction.h"
#include "Enjin/Gameplay/SaveIndicator.h"
#include "Enjin/Accessibility/Announcer.h"
#include "Enjin/Logging/Log.h"

#include <cmath>

namespace Enjin {
namespace Gameplay {

using ECS::Entity;
using ECS::INVALID_ENTITY;

namespace {

// Scene-level configuration, or the defaults if no game manager carries it.
// A scene with save points and no SaveSystemComponent should still work: the
// component is how you CHANGE the behaviour, not how you switch it on.
ECS::SaveSystemComponent DefaultConfig() {
    return ECS::SaveSystemComponent{};
}

} // namespace

Entity SavePointSystem::FindPlayer() const {
    if (!m_World) return INVALID_ENTITY;

    // Same order the editor's FindPlayerEntity uses, so "who is the player"
    // does not mean two different things in the editor and at runtime.
    Entity named = m_World->FindEntityByName("Player");
    if (named != INVALID_ENTITY) return named;

    auto first = [](const auto& entities) -> Entity {
        for (Entity e : entities) return e;
        return INVALID_ENTITY;
    };
    Entity found = first(m_World->GetEntitiesWithComponent<ECS::Platformer2DController>());
    if (found != INVALID_ENTITY) return found;
    found = first(m_World->GetEntitiesWithComponent<ECS::TopDown2DController>());
    if (found != INVALID_ENTITY) return found;
    found = first(m_World->GetEntitiesWithComponent<ECS::TopDown3DController>());
    if (found != INVALID_ENTITY) return found;
    found = first(m_World->GetEntitiesWithComponent<ECS::ThirdPersonController>());
    if (found != INVALID_ENTITY) return found;
    return first(m_World->GetEntitiesWithComponent<ECS::FirstPersonController>());
}

void SavePointSystem::Update(f32 deltaTime) {
    // SaveIndicator owns the lifetime of what is on screen -- it takes a
    // duration and expires itself -- so there is no timer here to keep in step
    // with it. These two strings are the last thing issued, for tests.
    m_Prompt.clear();

    if (!m_World || !m_SaveSystem) return;

    // Scene configuration, if a game manager carries it.
    ECS::SaveSystemComponent config = DefaultConfig();
    for (Entity e : m_World->GetEntitiesWithComponent<ECS::SaveSystemComponent>()) {
        if (const auto* c = m_World->GetComponent<ECS::SaveSystemComponent>(e)) config = *c;
        break;
    }
    if (!config.allowInWorldSavePoints) return;

    // isSaving / saveIndicatorTimer are driven by SaveIndicator::Update, on the
    // same clock as what is on screen, so the two cannot disagree.

    const Entity player = FindPlayer();
    if (player == INVALID_ENTITY) return;
    const auto* playerXf = m_World->GetComponent<ECS::TransformComponent>(player);
    if (!playerXf) return;

    const bool interactPressed =
        m_InputMap && m_InputMap->IsActionPressed(InputSystem::GameAction::Interact);

    for (Entity e : m_World->GetEntitiesWithComponent<ECS::SavePointComponent>()) {
        auto* sp = m_World->GetComponent<ECS::SavePointComponent>(e);
        const auto* xf = m_World->GetComponent<ECS::TransformComponent>(e);
        if (!sp || !xf) continue;

        // A point's own radius wins; 0 means "use the scene default", which is
        // what the field comment says and what the default value of 0 implies.
        const f32 radius = (sp->radius > 0.0f) ? sp->radius : config.savePointRadius;

        const Math::Vector3 d = xf->position - playerXf->position;
        const f32 distSq = d.x * d.x + d.y * d.y + d.z * d.z;
        const bool inRange = distSq <= (radius * radius);

        const bool entered = inRange && !sp->playerInRange;
        sp->playerInRange = inRange;

        const bool spent = sp->oneTimeUse && sp->used;
        if (!inRange) {
            if (m_PromptingPoint == e) m_PromptingPoint = INVALID_ENTITY;
            continue;
        }
        if (spent) continue;

        // saveOnEnter is the point's own override; savePointRequiresInput is the
        // scene-wide default for points that do not ask for either.
        const bool autoSave = sp->saveOnEnter || !config.savePointRequiresInput;

        if (!autoSave && config.savePointShowPrompt) {
            m_Prompt = "Press Interact to save";
            // Once, on entering range. A caption re-issued every frame would
            // stack sixty deep a second and never expire.
            if (m_PromptingPoint != e) {
                m_PromptingPoint = e;
                if (m_Indicator) m_Indicator->Show(m_Prompt, 4.0f);
                if (m_Announcer) m_Announcer->Announce(m_Prompt);
            }
        }

        const bool trigger = autoSave ? entered : interactPressed;
        if (!trigger) continue;

        // -1 means next available.
        u32 slot = 0;
        if (sp->slotTarget >= 0) {
            slot = static_cast<u32>(sp->slotTarget);
        } else {
            // First empty slot. Falling back to 0 when every slot is full is
            // deliberate: refusing to save because the player never tidied up
            // their saves is worse than overwriting the first one.
            for (u32 i = 0; i < TieredSaveSystem::MAX_SLOTS; ++i) {
                if (m_SaveSystem->GetSlotInfo(i).isEmpty) { slot = i; break; }
            }
        }

        const bool ok = m_SaveSystem->SaveToSlot(slot, m_World, m_SceneName);
        if (ok) {
            sp->used = true;
            m_Message = sp->saveMessage;
            if (m_Indicator) m_Indicator->Show(m_Message, config.saveIndicatorDuration);
            // Spoken as well as shown: a save is exactly the kind of event a
            // screen-reader user needs and cannot otherwise perceive.
            if (m_Announcer) m_Announcer->Announce(m_Message);
            ENJIN_LOG_INFO(Game, "Save point saved to slot %u (scene '%s')",
                           slot, m_SceneName.c_str());
        } else {
            // Loud. A save point that silently fails to save is the bug this
            // whole system was written to end.
            m_Message = "Save failed";
            if (m_Indicator) m_Indicator->Show(m_Message, config.saveIndicatorDuration);
            if (m_Announcer) {
                m_Announcer->Announce(m_Message, Accessibility::AnnouncePriority::High);
            }
            ENJIN_LOG_ERROR(Game, "Save point FAILED to write slot %u (scene '%s')",
                            slot, m_SceneName.c_str());
        }
    }
}

} // namespace Gameplay
} // namespace Enjin
