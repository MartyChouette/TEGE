#pragma once
#include "Enjin/Platform/Platform.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Tween.h"
#include <string>

namespace Enjin {
namespace Scripting { class ScriptEngine; }
namespace ECS {

class ENJIN_API TweenSystem {
public:
    TweenSystem() = default;
    ~TweenSystem() = default;

    // Set script engine for OnComplete callbacks (optional — callbacks silently skipped if null)
    void SetScriptEngine(Scripting::ScriptEngine* engine) { m_ScriptEngine = engine; }

    // Tick all active tweens
    void Update(World* world, f32 deltaTime);

    // Start all tweens marked autoPlay (called on play mode enter)
    void PlayAll(World* world);

private:
    void ApplyTween(World* world, Entity entity, TweenEntry& tween, f32 t);
    void CaptureStartValue(World* world, Entity entity, TweenEntry& tween);
    void InvokeCallback(World* world, Entity entity, const std::string& methodName);

    Scripting::ScriptEngine* m_ScriptEngine = nullptr;
};

} // namespace ECS
} // namespace Enjin
