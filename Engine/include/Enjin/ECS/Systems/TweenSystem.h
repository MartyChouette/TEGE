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

    // Enable/disable tween updates
    void SetEnabled(bool enabled) { m_Enabled = enabled; }
    bool IsEnabled() const { return m_Enabled; }

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
    bool m_Enabled = true;

    // Reusable per-frame storage (avoids per-frame allocation)
    struct PendingCallback {
        Entity entity;
        std::string methodName;
    };
    std::vector<PendingCallback> m_PendingCallbacks;

    // Cached method signature to avoid per-callback string concatenation
    std::string m_MethodSignature;
};

} // namespace ECS
} // namespace Enjin
