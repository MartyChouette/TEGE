#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/ActionTrigger.h"

namespace Enjin {

namespace InputSystem { class InputActionMap; }
namespace Accessibility { class SubtitleSystem; }

namespace ECS {

class EntityEventBus;

// Runs ActionTriggerComponents: reads each trigger's action from the active
// InputActionMap and applies its effect. This is what makes an input action do
// something in a scene without any script.
//
// Update() runs once per frame in every runtime (player, web player, editor
// Play). Reset() puts anything global the triggers changed (currently the time
// scale) back, and is called when play stops or a scene unloads.
class ENJIN_API ActionTriggerSystem {
public:
    // The map every trigger reads. Null disables the system.
    void SetInputActionMap(InputSystem::InputActionMap* map) { m_InputMap = map; }

    // Optional: where ShowSubtitle effects go. Null skips them.
    void SetSubtitleSystem(Accessibility::SubtitleSystem* subtitles) { m_Subtitles = subtitles; }

    // Optional: where EmitEvent effects go. Null skips them.
    void SetEventBus(EntityEventBus* bus) { m_EventBus = bus; }

    void Update(World* world, f32 dt);

    // Restore global state (time scale) and clear per-trigger runtime flags.
    void Reset(World* world);

private:
    void ApplyEffect(World* world, Entity entity, ActionTriggerComponent& trigger, bool on);
    Entity ResolveTarget(World* world, Entity self, const ActionTriggerComponent& trigger);

    InputSystem::InputActionMap* m_InputMap = nullptr;
    Accessibility::SubtitleSystem* m_Subtitles = nullptr;
    EntityEventBus* m_EventBus = nullptr;
    bool m_TimeScaleOwned = false;   // a trigger currently holds the time scale
};

} // namespace ECS
} // namespace Enjin
