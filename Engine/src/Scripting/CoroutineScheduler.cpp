#include "Enjin/Scripting/CoroutineScheduler.h"
#include "Enjin/Logging/Log.h"
#include <angelscript.h>
#include <algorithm>

namespace Enjin {
namespace Scripting {

CoroutineScheduler* CoroutineScheduler::s_Current = nullptr;
asIScriptContext* CoroutineScheduler::s_CurrentYieldContext = nullptr;
Coroutine* CoroutineScheduler::s_CurrentCoroutine = nullptr;

CoroutineScheduler::CoroutineScheduler() = default;

CoroutineScheduler::~CoroutineScheduler() {
    Clear();
}

// S9 fix: Cap active coroutine count to prevent memory exhaustion
static constexpr u32 kMaxActiveCoroutines = 4096;

u32 CoroutineScheduler::StartCoroutine(asIScriptContext* ctx, u64 entityId) {
    if (!ctx) return 0;

    if (m_Coroutines.size() >= kMaxActiveCoroutines) {
        ENJIN_LOG_WARN(Script, "CoroutineScheduler: coroutine cap (%u) reached, rejecting new coroutine",
                       kMaxActiveCoroutines);
        ctx->Abort();
        ctx->Release();
        return 0;
    }

    // S8 fix: skip ID 0 on wrap-around (0 is used as error return)
    if (m_NextId == 0) m_NextId = 1;

    Coroutine co;
    co.context = ctx;
    co.entityId = entityId;
    co.id = m_NextId++;
    co.finished = false;

    m_Coroutines.push_back(co);
    return co.id;
}

void CoroutineScheduler::StopCoroutine(u32 id) {
    for (auto& co : m_Coroutines) {
        if (co.id == id && !co.finished) {
            co.finished = true;
            if (co.context) {
                co.context->Abort();
                co.context->Release();
                co.context = nullptr;
            }
        }
    }
}

void CoroutineScheduler::StopAllForEntity(u64 entityId) {
    for (auto& co : m_Coroutines) {
        if (co.entityId == entityId && !co.finished) {
            co.finished = true;
            if (co.context) {
                co.context->Abort();
                co.context->Release();
                co.context = nullptr;
            }
        }
    }
}

void CoroutineScheduler::Update(f32 deltaTime) {
    s_Current = this;

    // Use index-based loop with pre-captured count because ResumeCoroutine
    // may cause scripts to call StartCoroutine, appending to m_Coroutines
    // and potentially invalidating range-based iterators.
    usize count = m_Coroutines.size();
    for (usize i = 0; i < count; ++i) {
        auto& co = m_Coroutines[i];
        if (co.finished) continue;

        switch (co.yieldType) {
            case YieldType::WaitForSeconds:
                co.waitTimeRemaining -= deltaTime;
                if (co.waitTimeRemaining <= 0.0f) {
                    ResumeCoroutine(co);
                }
                break;

            case YieldType::WaitForFrames:
                if (co.waitFramesRemaining > 0) {
                    co.waitFramesRemaining--;
                }
                if (co.waitFramesRemaining == 0) {
                    ResumeCoroutine(co);
                }
                break;

            case YieldType::WaitUntil:
                if (co.waitPredicate && co.waitPredicate()) {
                    ResumeCoroutine(co);
                }
                break;

            case YieldType::WaitForEndOfFrame:
                // Handled in EndOfFrame()
                break;
        }
    }

    CleanupFinished();
    s_Current = nullptr;
}

void CoroutineScheduler::EndOfFrame() {
    s_Current = this;
    m_InEndOfFrame = true;

    // Use index-based loop with pre-captured count for the same reason
    // as Update() — ResumeCoroutine may append new coroutines.
    usize count = m_Coroutines.size();
    for (usize i = 0; i < count; ++i) {
        auto& co = m_Coroutines[i];
        if (co.finished) continue;
        if (co.yieldType == YieldType::WaitForEndOfFrame) {
            ResumeCoroutine(co);
        }
    }

    m_InEndOfFrame = false;
    CleanupFinished();
    s_Current = nullptr;
}

void CoroutineScheduler::ResumeCoroutine(Coroutine& co) {
    if (!co.context || co.finished) return;

    s_CurrentYieldContext = co.context;
    s_CurrentCoroutine = &co;  // T-M13: Direct pointer avoids O(N) scan in yield functions
    int r = co.context->Execute();
    s_CurrentCoroutine = nullptr;
    s_CurrentYieldContext = nullptr;

    if (r == asEXECUTION_SUSPENDED) {
        // Coroutine yielded again — yield type was set by the yield function
        // The context remains valid
    } else if (r == asEXECUTION_FINISHED) {
        co.finished = true;
        co.context->Release();
        co.context = nullptr;
    } else if (r == asEXECUTION_EXCEPTION) {
        const char* exceptionStr = co.context->GetExceptionString();
        ENJIN_LOG_ERROR(Script, "Coroutine exception: %s", exceptionStr ? exceptionStr : "unknown");
        co.finished = true;
        co.context->Release();
        co.context = nullptr;
    } else {
        // Aborted or other
        co.finished = true;
        if (co.context) {
            co.context->Release();
            co.context = nullptr;
        }
    }
}

void CoroutineScheduler::CleanupFinished() {
    m_Coroutines.erase(
        std::remove_if(m_Coroutines.begin(), m_Coroutines.end(),
            [](const Coroutine& co) { return co.finished; }),
        m_Coroutines.end()
    );
}

void CoroutineScheduler::Clear() {
    for (auto& co : m_Coroutines) {
        if (co.context) {
            co.context->Abort();
            co.context->Release();
        }
    }
    m_Coroutines.clear();
}

usize CoroutineScheduler::GetActiveCount() const {
    usize count = 0;
    for (const auto& co : m_Coroutines) {
        if (!co.finished) count++;
    }
    return count;
}

// Static yield functions called from scripts
// T-M13: Use s_CurrentCoroutine pointer directly instead of O(N) scan
void CoroutineScheduler::YieldSeconds(f32 seconds) {
    if (!s_Current || !s_CurrentYieldContext || !s_CurrentCoroutine) return;

    s_CurrentCoroutine->yieldType = YieldType::WaitForSeconds;
    s_CurrentCoroutine->waitTimeRemaining = seconds;

    s_CurrentYieldContext->Suspend();
}

void CoroutineScheduler::YieldFrames(u32 frames) {
    if (!s_Current || !s_CurrentYieldContext || !s_CurrentCoroutine) return;

    s_CurrentCoroutine->yieldType = YieldType::WaitForFrames;
    s_CurrentCoroutine->waitFramesRemaining = frames;

    s_CurrentYieldContext->Suspend();
}

void CoroutineScheduler::YieldEndOfFrame() {
    if (!s_Current || !s_CurrentYieldContext || !s_CurrentCoroutine) return;

    s_CurrentCoroutine->yieldType = YieldType::WaitForEndOfFrame;

    s_CurrentYieldContext->Suspend();
}

} // namespace Scripting
} // namespace Enjin
