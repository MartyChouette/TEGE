#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include <string>
#include <unordered_map>
#include <vector>
#include <chrono>

namespace Enjin {
namespace Debug {

// Per-scope timing data
struct ProfileScope {
    std::string name;
    f64 lastTimeMs = 0.0;
    f64 avgTimeMs = 0.0;
    f64 maxTimeMs = 0.0;
    f64 totalTimeMs = 0.0;
    u64 callCount = 0;
};

// Frame timing data
struct FrameData {
    f64 totalMs = 0.0;
    f64 renderMs = 0.0;
    f64 physicsMs = 0.0;
    f64 scriptingMs = 0.0;
    f64 ecsMs = 0.0;
    f64 audioMs = 0.0;
    u32 drawCalls = 0;
    u32 entityCount = 0;
    u32 triangleCount = 0;
    usize memoryUsedBytes = 0;
    u64 scriptStatements = 0;   // AngelScript statements executed this frame
};

// RAII scope timer
class ScopeTimer {
public:
    ScopeTimer(const std::string& name);
    ~ScopeTimer();

private:
    std::string m_Name;
    std::chrono::high_resolution_clock::time_point m_Start;
};

// Main profiler — singleton
class ENJIN_API Profiler {
public:
    static Profiler& Instance() {
        static Profiler inst;
        return inst;
    }

    // Frame lifecycle
    void BeginFrame();
    void EndFrame();

    // Record a scope timing
    void RecordScope(const std::string& name, f64 durationMs);

    // Set per-frame counters
    void SetDrawCalls(u32 count) { m_CurrentFrame.drawCalls = count; }
    void SetEntityCount(u32 count) { m_CurrentFrame.entityCount = count; }
    void SetTriangleCount(u32 count) { m_CurrentFrame.triangleCount = count; }
    void SetMemoryUsed(usize bytes) { m_CurrentFrame.memoryUsedBytes = bytes; }
    // Frame total from ScriptEngine. The script instruction limit is per CALL,
    // so this is the only number that says what scripts cost the whole frame.
    void SetScriptStatements(u64 count) { m_CurrentFrame.scriptStatements = count; }
    void IncrementDrawCalls(u32 count = 1) { m_CurrentFrame.drawCalls += count; }

    // Query
    const FrameData& GetCurrentFrame() const { return m_CurrentFrame; }
    const std::unordered_map<std::string, ProfileScope>& GetScopes() const { return m_Scopes; }
    f64 GetFPS() const { return m_FPS; }
    f64 GetAverageFrameTimeMs() const;

    // Frame time history (for graphing)
    const std::vector<f32>& GetFrameTimeHistory() const { return m_FrameTimeHistory; }
    static constexpr usize HISTORY_SIZE = 240;

    // ImGui overlay. When `open` is non-null the window gets a close (X) button and
    // *open is set false when the user clicks it — the host syncs its panel flag.
    void DrawProfilerPanel(bool* open = nullptr);

    // Enable/disable
    void SetEnabled(bool enabled) { m_Enabled = enabled; }
    bool IsEnabled() const { return m_Enabled; }

    // Reset all collected data
    void Reset();

private:
    Profiler() = default;

    bool m_Enabled = true;
    FrameData m_CurrentFrame;
    std::unordered_map<std::string, ProfileScope> m_Scopes;

    // Frame time tracking
    std::chrono::high_resolution_clock::time_point m_FrameStart;
    std::vector<f32> m_FrameTimeHistory;
    f64 m_FPS = 0.0;
    f64 m_FPSAccumulator = 0.0;
    u32 m_FPSFrameCount = 0;
    f64 m_LastFPSUpdate = 0.0;
};

// Macro for easy scope profiling
#define ENJIN_PROFILE_SCOPE(name) \
    ::Enjin::Debug::ScopeTimer _profiler_##__LINE__(name)

#define ENJIN_PROFILE_FUNCTION() \
    ::Enjin::Debug::ScopeTimer _profiler_func(__FUNCTION__)

} // namespace Debug
} // namespace Enjin
