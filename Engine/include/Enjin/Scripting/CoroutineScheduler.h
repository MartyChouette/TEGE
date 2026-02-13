#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include <vector>
#include <functional>

class asIScriptContext;
class asIScriptEngine;

namespace Enjin {
namespace Scripting {

// Yield instruction types
enum class YieldType : u8 {
    WaitForSeconds,
    WaitForFrames,
    WaitUntil,
    WaitForEndOfFrame
};

// A suspended coroutine
struct Coroutine {
    asIScriptContext* context = nullptr;
    YieldType yieldType = YieldType::WaitForSeconds;
    f32 waitTimeRemaining = 0.0f;
    u32 waitFramesRemaining = 0;
    std::function<bool()> waitPredicate;
    bool finished = false;
    u64 entityId = 0; // Owning entity (for cleanup)
    u32 id = 0;       // Unique coroutine ID
};

// Manages suspended AngelScript contexts (coroutines).
// Scripts yield via:
//   yield WaitForSeconds(2.0f)   -> resume after 2 seconds
//   yield WaitForFrames(5)       -> resume after 5 frames
//   yield WaitUntil(predicate)   -> resume when condition is true
//   yield WaitForEndOfFrame()    -> resume at end of current frame
class ENJIN_API CoroutineScheduler {
public:
    CoroutineScheduler();
    ~CoroutineScheduler();

    void SetEngine(asIScriptEngine* engine) { m_Engine = engine; }

    // Start a new coroutine from a suspended context
    u32 StartCoroutine(asIScriptContext* ctx, u64 entityId);

    // Stop a coroutine by ID
    void StopCoroutine(u32 id);

    // Stop all coroutines belonging to an entity
    void StopAllForEntity(u64 entityId);

    // Update all active coroutines — call once per frame
    void Update(f32 deltaTime);

    // End-of-frame processing — resumes WaitForEndOfFrame coroutines
    void EndOfFrame();

    // Clear all coroutines
    void Clear();

    // Get active coroutine count
    usize GetActiveCount() const;

    // Called from script to set yield instruction on current context
    static void YieldSeconds(f32 seconds);
    static void YieldFrames(u32 frames);
    static void YieldEndOfFrame();

private:
    void ResumeCoroutine(Coroutine& co);
    void CleanupFinished();

    asIScriptEngine* m_Engine = nullptr;
    std::vector<Coroutine> m_Coroutines;
    u32 m_NextId = 1;
    bool m_InEndOfFrame = false;

    // Thread-local current scheduler for yield functions
    static CoroutineScheduler* s_Current;
    static asIScriptContext* s_CurrentYieldContext;
    // T-M13: Direct pointer to current coroutine (avoids O(N) scan in yield functions)
    static Coroutine* s_CurrentCoroutine;

    friend void YieldSeconds(f32);
    friend void YieldFrames(u32);
    friend void YieldEndOfFrame();
};

} // namespace Scripting
} // namespace Enjin
