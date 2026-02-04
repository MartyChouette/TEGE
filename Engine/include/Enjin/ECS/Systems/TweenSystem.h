#pragma once
#include "Enjin/Platform/Platform.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Tween.h"

namespace Enjin {
namespace ECS {

class ENJIN_API TweenSystem {
public:
    TweenSystem() = default;
    ~TweenSystem() = default;

    // Tick all active tweens
    void Update(World* world, f32 deltaTime);

    // Start all tweens marked autoPlay (called on play mode enter)
    void PlayAll(World* world);

private:
    void ApplyTween(World* world, Entity entity, TweenEntry& tween, f32 t);
    void CaptureStartValue(World* world, Entity entity, TweenEntry& tween);
};

} // namespace ECS
} // namespace Enjin
