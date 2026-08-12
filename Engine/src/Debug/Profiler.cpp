#include "Enjin/Debug/Profiler.h"
#include <imgui.h>
#include <algorithm>
#include <numeric>

namespace Enjin {
namespace Debug {

// ============================================================================
// ScopeTimer
// ============================================================================

ScopeTimer::ScopeTimer(const std::string& name)
    : m_Name(name)
    , m_Start(std::chrono::high_resolution_clock::now())
{
}

ScopeTimer::~ScopeTimer() {
    auto end = std::chrono::high_resolution_clock::now();
    f64 durationMs = std::chrono::duration<f64, std::milli>(end - m_Start).count();
    Profiler::Instance().RecordScope(m_Name, durationMs);
}

// ============================================================================
// Profiler
// ============================================================================

void Profiler::BeginFrame() {
    if (!m_Enabled) return;

    m_FrameStart = std::chrono::high_resolution_clock::now();
    m_CurrentFrame = FrameData{};
}

void Profiler::EndFrame() {
    if (!m_Enabled) return;

    auto now = std::chrono::high_resolution_clock::now();
    m_CurrentFrame.totalMs = std::chrono::duration<f64, std::milli>(now - m_FrameStart).count();

    // Map well-known scope names to frame categories
    auto mapScope = [&](const std::string& name) -> f64 {
        auto it = m_Scopes.find(name);
        return it != m_Scopes.end() ? it->second.lastTimeMs : 0.0;
    };
    m_CurrentFrame.renderMs = mapScope("Render");
    m_CurrentFrame.physicsMs = mapScope("Physics");
    m_CurrentFrame.scriptingMs = mapScope("Scripting");
    m_CurrentFrame.ecsMs = mapScope("ECS");
    m_CurrentFrame.audioMs = mapScope("Audio");

    // Update frame time history
    if (m_FrameTimeHistory.size() >= HISTORY_SIZE) {
        m_FrameTimeHistory.erase(m_FrameTimeHistory.begin());
    }
    m_FrameTimeHistory.push_back(static_cast<f32>(m_CurrentFrame.totalMs));

    // Update FPS counter (every 0.5 seconds)
    m_FPSAccumulator += m_CurrentFrame.totalMs;
    m_FPSFrameCount++;
    if (m_FPSAccumulator >= 500.0) {
        m_FPS = static_cast<f64>(m_FPSFrameCount) / (m_FPSAccumulator / 1000.0);
        m_FPSFrameCount = 0;
        m_FPSAccumulator = 0.0;
    }
}

void Profiler::RecordScope(const std::string& name, f64 durationMs) {
    if (!m_Enabled) return;

    auto& scope = m_Scopes[name];
    scope.name = name;
    scope.lastTimeMs = durationMs;
    scope.totalTimeMs += durationMs;
    scope.callCount++;
    scope.avgTimeMs = scope.totalTimeMs / static_cast<f64>(scope.callCount);
    if (durationMs > scope.maxTimeMs) {
        scope.maxTimeMs = durationMs;
    }
}

f64 Profiler::GetAverageFrameTimeMs() const {
    if (m_FrameTimeHistory.empty()) return 0.0;
    f64 sum = std::accumulate(m_FrameTimeHistory.begin(), m_FrameTimeHistory.end(), 0.0f);
    return sum / static_cast<f64>(m_FrameTimeHistory.size());
}

void Profiler::DrawProfilerPanel(bool* open) {
    if (!m_Enabled) return;

    if (ImGui::Begin("Profiler", open)) {
        // FPS and frame time header — flow with fixed spacing so it never overlaps at
        // larger font scales (the old absolute SameLine(150/300) collided).
        ImGui::Text("FPS: %.1f", m_FPS);
        ImGui::SameLine(0, 24);
        ImGui::Text("Frame: %.2f ms", m_CurrentFrame.totalMs);
        ImGui::SameLine(0, 24);
        ImGui::Text("Avg: %.2f ms", GetAverageFrameTimeMs());

        ImGui::Separator();

        // Frame time graph
        if (!m_FrameTimeHistory.empty()) {
            f32 maxTime = *std::max_element(m_FrameTimeHistory.begin(), m_FrameTimeHistory.end());
            maxTime = std::max(maxTime, 1.0f);

            char overlay[64];
            snprintf(overlay, sizeof(overlay), "%.1f ms", m_CurrentFrame.totalMs);
            ImGui::PlotLines("##FrameTime", m_FrameTimeHistory.data(),
                static_cast<int>(m_FrameTimeHistory.size()), 0, overlay,
                0.0f, maxTime * 1.2f, ImVec2(ImGui::GetContentRegionAvail().x, 80));
        }

        ImGui::Separator();

        // System breakdown
        ImGui::Text("System Breakdown:");
        ImGui::Columns(2, "##SystemBreakdown");
        ImGui::SetColumnWidth(0, 120);

        auto drawBar = [&](const char* label, f64 timeMs) {
            ImGui::Text("%s", label);
            ImGui::NextColumn();
            f32 frac = m_CurrentFrame.totalMs > 0.0 ?
                static_cast<f32>(timeMs / m_CurrentFrame.totalMs) : 0.0f;
            char buf[32];
            snprintf(buf, sizeof(buf), "%.2f ms", timeMs);
            ImGui::ProgressBar(frac, ImVec2(-1, 0), buf);
            ImGui::NextColumn();
        };

        drawBar("Render", m_CurrentFrame.renderMs);
        drawBar("Physics", m_CurrentFrame.physicsMs);
        drawBar("Scripting", m_CurrentFrame.scriptingMs);
        drawBar("ECS", m_CurrentFrame.ecsMs);
        drawBar("Audio", m_CurrentFrame.audioMs);

        ImGui::Columns(1);
        ImGui::Separator();

        // Counters
        ImGui::Text("Draw Calls: %u", m_CurrentFrame.drawCalls);
        ImGui::SameLine(0, 24);
        ImGui::Text("Entities: %u", m_CurrentFrame.entityCount);
        ImGui::SameLine(0, 24);
        ImGui::Text("Triangles: %u", m_CurrentFrame.triangleCount);

        if (m_CurrentFrame.memoryUsedBytes > 0) {
            f64 mb = static_cast<f64>(m_CurrentFrame.memoryUsedBytes) / (1024.0 * 1024.0);
            ImGui::Text("Memory: %.1f MB", mb);
        }

        ImGui::Separator();

        // Detailed scope timings — a resizable, auto-sizing table so long scope names
        // and 3-decimal values are never clipped (the old fixed 200/80px columns cut off
        // "RenderSys/Update" and truncated the Max column).
        if (ImGui::TreeNode("Detailed Scopes")) {
            // Sort scopes by last time (descending)
            std::vector<const ProfileScope*> sorted;
            for (const auto& [name, scope] : m_Scopes) {
                sorted.push_back(&scope);
            }
            std::sort(sorted.begin(), sorted.end(),
                [](const ProfileScope* a, const ProfileScope* b) {
                    return a->lastTimeMs > b->lastTimeMs;
                });

            constexpr ImGuiTableFlags tblFlags =
                ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp;
            if (ImGui::BeginTable("##Scopes", 5, tblFlags)) {
                ImGui::TableSetupColumn("Scope", ImGuiTableColumnFlags_WidthStretch, 2.4f);
                ImGui::TableSetupColumn("Last",  ImGuiTableColumnFlags_WidthStretch, 1.0f);
                ImGui::TableSetupColumn("Avg",   ImGuiTableColumnFlags_WidthStretch, 1.0f);
                ImGui::TableSetupColumn("Max",   ImGuiTableColumnFlags_WidthStretch, 1.0f);
                ImGui::TableSetupColumn("Calls", ImGuiTableColumnFlags_WidthStretch, 1.0f);
                ImGui::TableHeadersRow();

                for (const auto* scope : sorted) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn(); ImGui::TextUnformatted(scope->name.c_str());
                    ImGui::TableNextColumn(); ImGui::Text("%.3f", scope->lastTimeMs);
                    ImGui::TableNextColumn(); ImGui::Text("%.3f", scope->avgTimeMs);
                    ImGui::TableNextColumn(); ImGui::Text("%.3f", scope->maxTimeMs);
                    ImGui::TableNextColumn(); ImGui::Text("%llu", static_cast<unsigned long long>(scope->callCount));
                }
                ImGui::EndTable();
            }
            ImGui::TreePop();
        }
    }
    ImGui::End();
}

void Profiler::Reset() {
    m_Scopes.clear();
    m_FrameTimeHistory.clear();
    m_CurrentFrame = FrameData{};
    m_FPS = 0.0;
    m_FPSAccumulator = 0.0;
    m_FPSFrameCount = 0;
}

} // namespace Debug
} // namespace Enjin
