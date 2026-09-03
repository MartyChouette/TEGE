#include "Enjin/ECS/Systems/ActionTriggerSystem.h"
#include "Enjin/ECS/Components/Name.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Controllers/CharacterController.h"
#include "Enjin/ECS/EntityEventBus.h"
#include "Enjin/Input/InputAction.h"
#include "Enjin/Accessibility/SubtitleSystem.h"
#include "Enjin/Scripting/ScriptBindings.h"

namespace Enjin {
namespace ECS {

namespace {
    // Bullet time only reads right when the player keeps moving at full speed
    // while the world slows. Every character controller derives from
    // CharacterControllerBase, which carries the opt-out flag.
    void SetControllersIgnoreTimeScale(World* world, bool ignore) {
        auto apply = [&](auto* typeTag) {
            using T = std::remove_pointer_t<decltype(typeTag)>;
            for (Entity e : world->GetEntitiesWithComponent<T>()) {
                if (auto* c = world->GetComponent<T>(e)) c->ignoreGlobalTimeScale = ignore;
            }
        };
        apply(static_cast<FirstPersonController*>(nullptr));
        apply(static_cast<ThirdPersonController*>(nullptr));
        apply(static_cast<TopDown3DController*>(nullptr));
        apply(static_cast<TopDown2DController*>(nullptr));
        apply(static_cast<Platformer2DController*>(nullptr));
        apply(static_cast<SurfaceAlignedController*>(nullptr));
        apply(static_cast<VehicleController*>(nullptr));
    }
}

Entity ActionTriggerSystem::ResolveTarget(World* world, Entity self,
                                          const ActionTriggerComponent& trigger) {
    if (trigger.targetEntity.empty()) return self;
    Entity found = world->FindEntityByName(trigger.targetEntity);
    return found != INVALID_ENTITY ? found : self;
}

void ActionTriggerSystem::ApplyEffect(World* world, Entity entity,
                                      ActionTriggerComponent& trigger, bool on) {
    switch (trigger.effect) {
        case ActionEffect::TimeScale: {
            Scripting::SetTimeScale(on ? trigger.timeScale : 1.0f);
            m_TimeScaleOwned = on;
            if (trigger.keepPlayerSpeed) SetControllersIgnoreTimeScale(world, on);
            break;
        }
        case ActionEffect::ToggleVisibility: {
            Entity target = ResolveTarget(world, entity, trigger);
            if (auto* tf = world->GetComponent<TransformComponent>(target)) {
                tf->visible = !on;   // "on" hides, so the action reads as a toggle
            }
            break;
        }
        case ActionEffect::EmitEvent: {
            if (!m_EventBus || trigger.eventName.empty()) break;
            EntityEvent ev;
            ev.name = trigger.eventName;
            ev.sender = entity;
            ev.target = ResolveTarget(world, entity, trigger);
            ev.ints["on"] = on ? 1 : 0;
            m_EventBus->Send(trigger.eventName, ev);
            break;
        }
        case ActionEffect::ShowSubtitle: {
            if (!m_Subtitles) break;
            const std::string& text = on ? trigger.onText : trigger.offText;
            if (!text.empty()) {
                m_Subtitles->ShowSubtitle(text, "", Math::Vector3(1.0f, 1.0f, 1.0f),
                                          trigger.textDuration);
            }
            break;
        }
        case ActionEffect::None:
        default:
            break;
    }
}

void ActionTriggerSystem::Update(World* world, f32 dt) {
    (void)dt;
    if (!world || !m_InputMap) return;

    for (Entity e : world->GetEntitiesWithComponent<ActionTriggerComponent>()) {
        auto* trigger = world->GetComponent<ActionTriggerComponent>(e);
        if (!trigger) continue;
        if (trigger->action < 0 ||
            trigger->action >= static_cast<i32>(InputSystem::GameAction::Count)) continue;

        auto action = static_cast<InputSystem::GameAction>(trigger->action);
        bool pressed = m_InputMap->IsActionPressed(action);
        bool released = m_InputMap->IsActionReleased(action);
        bool down = m_InputMap->IsActionDown(action);

        switch (trigger->mode) {
            case ActionTriggerMode::OnPress:
                if (pressed) { trigger->active = true; ApplyEffect(world, e, *trigger, true); }
                break;
            case ActionTriggerMode::OnRelease:
                if (released) { trigger->active = true; ApplyEffect(world, e, *trigger, true); }
                break;
            case ActionTriggerMode::WhileHeld:
                if (down != trigger->active) {
                    trigger->active = down;
                    ApplyEffect(world, e, *trigger, down);
                }
                break;
            case ActionTriggerMode::Toggle:
                if (pressed) {
                    trigger->active = !trigger->active;
                    ApplyEffect(world, e, *trigger, trigger->active);
                }
                break;
        }
    }
}

void ActionTriggerSystem::Reset(World* world) {
    // Leaving play with a slowed world would strand the time scale, and the
    // editor keeps running the same process.
    if (m_TimeScaleOwned) {
        Scripting::SetTimeScale(1.0f);
        m_TimeScaleOwned = false;
        if (world) SetControllersIgnoreTimeScale(world, false);
    }
    if (!world) return;
    for (Entity e : world->GetEntitiesWithComponent<ActionTriggerComponent>()) {
        if (auto* t = world->GetComponent<ActionTriggerComponent>(e)) t->active = false;
    }
}

} // namespace ECS
} // namespace Enjin
