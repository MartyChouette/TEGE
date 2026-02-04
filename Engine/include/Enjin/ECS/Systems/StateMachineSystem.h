#pragma once
#include "Enjin/Platform/Platform.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Gameplay.h"

namespace Enjin {
namespace ECS {

class ENJIN_API StateMachineSystem {
public:
    StateMachineSystem() = default;
    ~StateMachineSystem() = default;

    // Evaluate transitions and advance state timers
    void Update(World* world, f32 deltaTime);
};

} // namespace ECS
} // namespace Enjin
