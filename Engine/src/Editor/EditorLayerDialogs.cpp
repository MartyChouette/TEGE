#include "Enjin/Editor/EditorLayer.h"
#include "Enjin/Editor/InspectorUndo.h"

// Webhook URLs — compiled in, not user-configurable. File is .gitignored.
#if __has_include("Enjin/Editor/WebhookConfig.h")
#include "Enjin/Editor/WebhookConfig.h"
#else
#define ENJIN_DISCORD_BUG_WEBHOOK ""
#define ENJIN_DISCORD_FEEDBACK_WEBHOOK ""
#endif
#include "Enjin/Editor/ScenePicker.h"
#include "Enjin/Core/Version.h"
#include "Enjin/Debug/CrashHandler.h"
#include <GLFW/glfw3.h>
#include <chrono>
#include "Enjin/Logging/Log.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Mesh.h"
#include "Enjin/ECS/Components/Material.h"
#include "Enjin/ECS/Components/Light.h"
#include "Enjin/ECS/Components/Name.h"
#include "Enjin/ECS/Components/Camera.h"
#include "Enjin/ECS/Components/Notes.h"
#include "Enjin/ECS/Components/Controllers/CharacterController.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/ECS/Components/VisualScript.h"
#include "Enjin/AI/BehaviorTree.h"
#include "Enjin/Gameplay/QuestFlow.h"
#include "Enjin/Gameplay/TieredSaveSystem.h"
#include "Enjin/Editor/PlayModeDiff.h"
#include "Enjin/Physics/PhysicsBackendType.h"
#include "Enjin/Physics/PhysicsBackendFactory.h"
#include "Enjin/Physics/PhysicsTypes2D.h"
#include "Enjin/ECS/Components/WeatherZone.h"
#include "Enjin/ECS/Components/WaterVolume.h"
#include "Enjin/ECS/Components/GrassVolume.h"
#include "Enjin/ECS/Components/ShrubVolume.h"
#include "Enjin/ECS/Components/TreeVolume.h"
#include "Enjin/ECS/Components/Terrain.h"
#include "Enjin/ECS/Components/Terrain2D.h"
#include "Enjin/ECS/Components/Vegetation.h"
#include "Enjin/ECS/Components/CameraTrigger.h"
#include "Enjin/ECS/Components/TemperatureZone.h"
#include "Enjin/ECS/Components/GravityZone.h"
#include "Enjin/ECS/Components/PostProcessVolume.h"
#include "Enjin/ECS/Components/FluidVolume.h"
#include "Enjin/ECS/Components/Text.h"
#include "Enjin/ECS/Components/IKComponents.h"
#include "Enjin/ECS/Components/Flower.h"
#ifndef _WIN32
#include <unistd.h>
#endif
#include "Enjin/ECS/Components/LOD.h"
#include "Enjin/ECS/Components/Script.h"
#include "Enjin/ECS/Components/Tween.h"
#include "Enjin/ECS/Components/Hierarchy.h"
#include "Enjin/ECS/Components/Skeleton.h"
#include "Enjin/Renderer/MeshSimplifier.h"
#include "Enjin/Renderer/Skybox.h"
#include "Enjin/ECS/Systems/RenderSystem.h"
#include "Enjin/Renderer/RayTracing/RTShadows.h"
#include "Enjin/Renderer/RayTracing/RTReflections.h"
#include "Enjin/Renderer/RayTracing/RTAmbientOcclusion.h"
#include "Enjin/Renderer/RayTracing/RTGlobalIllumination.h"
#include "Enjin/Renderer/RayTracing/PathTracer.h"
#include "Enjin/Renderer/RayTracing/SVGFDenoiser.h"
#include "Enjin/Renderer/RayTracing/OIDNDenoiser.h"
#include "Enjin/Renderer/RayTracing/RTCompositor.h"
#include "Enjin/Renderer/RayTracing/AccelerationStructureManager.h"
#include "Enjin/Renderer/SHLightProbe.h"
#include "Enjin/Renderer/SDFScene.h"
#include "Enjin/Renderer/OITManager.h"
#include "Enjin/Effects/TreeRenderer.h"
#include "Enjin/Effects/Weather.h"
#include "Enjin/Assets/SceneImporter.h"
#include "Enjin/Assets/FontLibrary.h"
#include "Enjin/Assets/AssetLibrary.h"
#include "Enjin/Assets/AssetMetadata.h"
#include "Enjin/Scene/SceneSerializer.h"
#include "Enjin/Renderer/MeshFactory.h"
#include "Enjin/Renderer/PostProcessing.h"
#include "Enjin/Platform/Input.h"
#include "Enjin/Platform/FileDialog.h"
#include "Enjin/Assets/Prefab.h"
#include "Enjin/Build/BuildPipeline.h"
#include "Enjin/Assets/DataAsset.h"
#include "Enjin/Plugin/PluginRepository.h"
#include "Enjin/Audio/AudioSystem.h"
#include "Enjin/Renderer/NormalMapGenerator.h"
#include "Enjin/Editor/SpriteContourTracer.h"
#include "Enjin/GUI/UICanvas.h"
#include "Enjin/GUI/UITemplates.h"
#include "Enjin/GUI/DialogueImportExport.h"
#include "Enjin/Assets/SWFLoader.h"
#include "Enjin/Effects/CurlNoiseSystem.h"
#include "Enjin/Scripting/ScriptBindings.h"
#include "Enjin/Scene/LevelStreaming.h"
#include "Enjin/Effects/VoronoiMeshFracture.h"
#include "Enjin/Effects/InteractiveWater.h"
#include "Enjin/Math/Math.h"
#include <stb_image.h>
#include <stb_image_write.h>
#include "Enjin/Networking/HTTPClient.h"
#include <imgui.h>
#include <ImGuizmo.h>
#include <backends/imgui_impl_vulkan.h>
#include <vulkan/vulkan.h>
#include <sstream>
#include <fstream>
#include <filesystem>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
// Undefine Windows macros that collide with engine methods
#undef LoadImage
#undef CreateWindow
#undef min
#undef max
#else
#include <spawn.h>
#include <sys/wait.h>
#endif
#include <climits>
#include <cmath>
#include <algorithm>

namespace Enjin {
namespace Editor {

void EditorLayer::DrawStatsOverlay() {
    const float DISTANCE = 10.0f;
    ImGuiIO& io = ImGui::GetIO();

    ImVec2 windowPos = ImVec2(io.DisplaySize.x - DISTANCE, DISTANCE);
    ImVec2 windowPivot = ImVec2(1.0f, 0.0f);
    ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always, windowPivot);
    ImGui::SetNextWindowBgAlpha(0.50f);

    ImGuiWindowFlags flags = ImGuiWindowFlags_AlwaysAutoResize |
                             ImGuiWindowFlags_NoSavedSettings |
                             ImGuiWindowFlags_NoFocusOnAppearing |
                             ImGuiWindowFlags_NoNav |
                             ImGuiWindowFlags_NoMove;

    if (ImGui::Begin("Stats", &m_ShowStatsOverlay, flags)) {
        // FPS and frame time
        f32 currentFrameTime = m_LastDeltaTime * 1000.0f;
        f32 fps = m_LastDeltaTime > 0.0f ? 1.0f / m_LastDeltaTime : 0.0f;

        // Color FPS based on performance (green > 60, yellow > 30, red < 30)
        ImVec4 fpsColor = fps >= 60.0f ? ImVec4(0.2f, 1.0f, 0.2f, 1.0f) :
                          fps >= 30.0f ? ImVec4(1.0f, 1.0f, 0.2f, 1.0f) :
                                         ImVec4(1.0f, 0.3f, 0.3f, 1.0f);

        ImGui::TextColored(fpsColor, "FPS: %.1f", fps);
        ImGui::Text("Frame: %.2f ms", currentFrameTime);

        ImGui::Separator();
        ImGui::Text("Min: %.2f ms (%.0f fps)", m_FrameTimeMin, m_FrameTimeMin > 0 ? 1000.0f / m_FrameTimeMin : 0);
        ImGui::Text("Max: %.2f ms (%.0f fps)", m_FrameTimeMax, m_FrameTimeMax > 0 ? 1000.0f / m_FrameTimeMax : 0);
        ImGui::Text("Avg: %.2f ms (%.0f fps)", m_FrameTimeAvg, m_FrameTimeAvg > 0 ? 1000.0f / m_FrameTimeAvg : 0);

        // Percentiles
        ImGui::TextColored(ImVec4(0.7f, 0.9f, 1.0f, 1.0f), "-- Percentiles --");
        ImGui::Text("P50: %.2f ms (%.0f fps)", m_FrameTimeP50, m_FrameTimeP50 > 0 ? 1000.0f / m_FrameTimeP50 : 0);
        ImGui::Text("P95: %.2f ms (%.0f fps)", m_FrameTimeP95, m_FrameTimeP95 > 0 ? 1000.0f / m_FrameTimeP95 : 0);
        ImVec4 p99Color = m_FrameTimeP99 > 16.7f ? ImVec4(1.0f, 0.3f, 0.3f, 1.0f) : ImVec4(0.8f, 0.8f, 0.8f, 1.0f);
        ImGui::TextColored(p99Color, "P99: %.2f ms (%.0f fps)", m_FrameTimeP99, m_FrameTimeP99 > 0 ? 1000.0f / m_FrameTimeP99 : 0);

        // Frame time graph
        ImGui::Separator();
        ImGui::Text("Frame Time History:");

        // Calculate scale for graph (show up to 2x max to see spikes clearly)
        f32 graphMax = m_FrameTimeMax > 0.0f ? m_FrameTimeMax * 1.5f : 33.3f;
        graphMax = graphMax < 16.7f ? 33.3f : graphMax;  // At least show 30fps line

        // Draw the graph with custom getter to handle circular buffer
        auto getter = [](void* data, int idx) -> float {
            EditorLayer* self = static_cast<EditorLayer*>(data);
            usize actualIdx = (self->m_FrameTimeIndex + static_cast<usize>(idx)) % FRAME_TIME_HISTORY_SIZE;
            return self->m_FrameTimeHistory[actualIdx];
        };

        char overlay[64];
        snprintf(overlay, sizeof(overlay), "%.1f ms", currentFrameTime);
        ImGui::PlotLines("##FrameTime", getter, this, static_cast<int>(FRAME_TIME_HISTORY_SIZE),
                        0, overlay, 0.0f, graphMax, ImVec2(200, 60));

        // Reference lines legend
        ImGui::TextDisabled("-- 60fps (16.7ms) -- 30fps (33.3ms)");

        if (m_World) {
            ImGui::Separator();
            ImGui::Text("Entities: %zu", m_World->GetEntityCount());
        }

        if (m_CameraController) {
            ImGui::Separator();
            ImGui::Text("Yaw: %.1f  Pitch: %.1f", m_CameraController->GetYaw(), m_CameraController->GetPitch());
        }

        // GPU info (cached — device name never changes at runtime)
        if (m_Renderer && m_Renderer->GetContext()) {
            ImGui::Separator();
            if (m_CachedGPUName.empty()) {
                VkPhysicalDeviceProperties props;
                vkGetPhysicalDeviceProperties(m_Renderer->GetContext()->GetPhysicalDevice(), &props);
                m_CachedGPUName = props.deviceName;
            }
            ImGui::Text("GPU: %s", m_CachedGPUName.c_str());
        }

        // Memory stats
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.7f, 0.9f, 1.0f, 1.0f), "-- Memory --");
        f32 processMB = static_cast<f32>(m_PerfMetrics.processMemoryBytes) / (1024.0f * 1024.0f);
        ImGui::Text("Process: %.1f MB", processMB);
        if (m_PerfMetrics.totalPhysicalMemory > 0) {
            f32 availMB = static_cast<f32>(m_PerfMetrics.availablePhysicalMemory) / (1024.0f * 1024.0f);
            f32 totalMB = static_cast<f32>(m_PerfMetrics.totalPhysicalMemory) / (1024.0f * 1024.0f);
            ImGui::Text("System: %.0f / %.0f MB", totalMB - availMB, totalMB);
        }
        if (m_PerfMetrics.gpuTotalBytes > 0) {
            f32 gpuAllocMB = static_cast<f32>(m_PerfMetrics.gpuAllocatedBytes) / (1024.0f * 1024.0f);
            f32 gpuTotalMB = static_cast<f32>(m_PerfMetrics.gpuTotalBytes) / (1024.0f * 1024.0f);
            ImGui::Text("GPU: %.1f / %.0f MB", gpuAllocMB, gpuTotalMB);
        }

        // Render stats
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.7f, 0.9f, 1.0f, 1.0f), "-- Render --");
        ImGui::Text("Draw Calls: %u", m_PerfMetrics.drawCallCount);
        if (m_PerfMetrics.triangleCount > 1000000) {
            ImGui::Text("Triangles: %.2f M", static_cast<f32>(m_PerfMetrics.triangleCount) / 1000000.0f);
        } else if (m_PerfMetrics.triangleCount > 1000) {
            ImGui::Text("Triangles: %.1f K", static_cast<f32>(m_PerfMetrics.triangleCount) / 1000.0f);
        } else {
            ImGui::Text("Triangles: %u", m_PerfMetrics.triangleCount);
        }

        // Descriptor cache hit rate
        u32 totalDescOps = m_PerfMetrics.descriptorCacheHits + m_PerfMetrics.descriptorCacheWrites;
        if (totalDescOps > 0) {
            f32 hitRate = static_cast<f32>(m_PerfMetrics.descriptorCacheHits) / static_cast<f32>(totalDescOps) * 100.0f;
            ImVec4 cacheColor = hitRate > 80.0f ? ImVec4(0.2f, 1.0f, 0.2f, 1.0f) :
                                hitRate > 50.0f ? ImVec4(1.0f, 1.0f, 0.2f, 1.0f) :
                                                   ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
            ImGui::TextColored(cacheColor, "Desc Cache: %.0f%% (%u/%u)", hitRate,
                              m_PerfMetrics.descriptorCacheHits, totalDescOps);
        }

        // CSV export
        ImGui::Separator();
        if (ImGui::SmallButton("Export CSV")) {
            try {
                std::string csvPath = "perf_stats.csv";
                std::ofstream ofs(csvPath);
                if (ofs.is_open()) {
                    ofs << "Metric,Value\n";
                    ofs << "FPS," << (m_LastDeltaTime > 0.0f ? 1.0f / m_LastDeltaTime : 0.0f) << "\n";
                    ofs << "FrameTime_ms," << (m_LastDeltaTime * 1000.0f) << "\n";
                    ofs << "Min_ms," << m_FrameTimeMin << "\n";
                    ofs << "Max_ms," << m_FrameTimeMax << "\n";
                    ofs << "Avg_ms," << m_FrameTimeAvg << "\n";
                    ofs << "P50_ms," << m_FrameTimeP50 << "\n";
                    ofs << "P95_ms," << m_FrameTimeP95 << "\n";
                    ofs << "P99_ms," << m_FrameTimeP99 << "\n";
                    ofs << "DrawCalls," << m_PerfMetrics.drawCallCount << "\n";
                    ofs << "Triangles," << m_PerfMetrics.triangleCount << "\n";
                    ofs << "DescCacheHits," << m_PerfMetrics.descriptorCacheHits << "\n";
                    ofs << "DescCacheWrites," << m_PerfMetrics.descriptorCacheWrites << "\n";
                    ofs << "ProcessMemory_MB," << (static_cast<f32>(m_PerfMetrics.processMemoryBytes) / (1024.0f * 1024.0f)) << "\n";
                    if (m_World) ofs << "Entities," << m_World->GetEntityCount() << "\n";
                    ENJIN_LOG_INFO(Editor, "Performance stats exported to %s", csvPath.c_str());
                }
            } catch (...) {}
        }
        ImGui::SameLine();
        ImGui::TextDisabled("perf_stats.csv");
    }
    ImGui::End();
}


void EditorLayer::DrawSplashScreen() {
    ImGuiIO& io = ImGui::GetIO();
    const f32 t = m_SplashTimer;

    // --- Utility lambdas ---
    auto smoothstep = [](f32 x) -> f32 { x = std::clamp(x, 0.0f, 1.0f); return x * x * (3.0f - 2.0f * x); };
    auto easeOutCubic = [](f32 x) -> f32 { x = std::clamp(x, 0.0f, 1.0f); f32 inv = 1.0f - x; return 1.0f - inv * inv * inv; };
    auto timerRamp = [&](f32 start, f32 end) -> f32 {
        if (t <= start) return 0.0f;
        if (t >= end) return 1.0f;
        return (t - start) / (end - start);
    };
    auto hashFloat = [](u32 seed) -> f32 {
        seed = seed * 2654435761u;
        seed ^= seed >> 16;
        seed *= 0x45d9f3bu;
        seed ^= seed >> 16;
        return static_cast<f32>(seed & 0xFFFFu) / 65535.0f;
    };

    // Global fade: 0-0.3 fade-in, 3.0-4.0 fade-out
    f32 globalAlpha = 1.0f;
    if (t < 0.3f) globalAlpha = smoothstep(t / 0.3f);
    else if (t > m_SplashFadeStart) {
        f32 fadeProgress = (t - m_SplashFadeStart) / (m_SplashDuration - m_SplashFadeStart);
        globalAlpha = 1.0f - smoothstep(fadeProgress);
    }
    f32 ga = globalAlpha;

    // Full-screen overlay
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::SetNextWindowBgAlpha(0.0f);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));

    if (ImGui::Begin("##Splash", nullptr, flags)) {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImFont* font = ImGui::GetFont();
        f32 W = io.DisplaySize.x;
        f32 H = io.DisplaySize.y;
        ImVec2 center(W * 0.5f, H * 0.5f);

        // ========== LAYER 1: Background ==========

        // 1a. Solid dark fill — stays fully opaque even during fade-out
        //     so the 3D scene never shows through
        dl->AddRectFilled(ImVec2(0, 0), ImVec2(W, H),
            IM_COL32(13, 13, 20, 255));

        // 1b. Radial vignette with subtle breathing
        f32 breath = 1.0f + 0.03f * std::sin(t * 0.8f);
        for (int ring = 0; ring < 3; ring++) {
            f32 radius = (0.35f + ring * 0.15f) * std::min(W, H) * breath;
            int vigAlpha = static_cast<int>((20 - ring * 6) * ga);
            if (vigAlpha > 0) {
                dl->AddCircle(center, radius,
                    IM_COL32(100, 130, 180, vigAlpha), 64, 1.5f);
            }
        }

        // 1c. 12 wireframe hexagons — slowly rotating, drifting upward
        for (int h = 0; h < 12; h++) {
            f32 hx = hashFloat(h * 7 + 1) * W;
            f32 hy = hashFloat(h * 7 + 2) * H;
            hy = std::fmod(hy - t * (15.0f + hashFloat(h * 7 + 3) * 10.0f), H + 100.0f);
            if (hy < -50.0f) hy += H + 100.0f;
            f32 hexR = 20.0f + hashFloat(h * 7 + 4) * 30.0f;
            f32 rot = t * (0.2f + hashFloat(h * 7 + 5) * 0.3f);
            int hexAlpha = static_cast<int>(15 * ga);
            if (hexAlpha > 0) {
                ImVec2 pts[6];
                for (int v = 0; v < 6; v++) {
                    f32 angle = rot + v * 3.14159265f / 3.0f;
                    pts[v] = ImVec2(hx + std::cos(angle) * hexR, hy + std::sin(angle) * hexR);
                }
                for (int v = 0; v < 6; v++) {
                    dl->AddLine(pts[v], pts[(v + 1) % 6],
                        IM_COL32(100, 140, 180, hexAlpha), 1.0f);
                }
            }
        }

        // ========== LAYER 2: Floating Light Orbs (24 total) ==========

        auto orbColor = [&](int idx) -> ImVec4 {
            f32 pick = hashFloat(idx * 13 + 100);
            if (pick < 0.4f) return ImVec4(160, 200, 160, 1);
            if (pick < 0.8f) return ImVec4(100, 140, 220, 1);
            return ImVec4(220, 210, 190, 1);
        };

        for (int i = 0; i < 24; i++) {
            bool foreground = (i >= 16);
            f32 baseRadius = foreground ? (5.0f + hashFloat(i * 11 + 50) * 9.0f)
                                        : (3.0f + hashFloat(i * 11 + 50) * 5.0f);
            f32 brightness = foreground ? 1.0f : 0.4f;
            f32 speed = foreground ? (0.6f + hashFloat(i * 11 + 51) * 0.4f)
                                   : (0.3f + hashFloat(i * 11 + 51) * 0.3f);

            f32 phaseX = hashFloat(i * 11 + 52) * 6.28f;
            f32 phaseY = hashFloat(i * 11 + 53) * 6.28f;
            f32 ampX = 0.15f + hashFloat(i * 11 + 54) * 0.25f;
            f32 ampY = 0.1f + hashFloat(i * 11 + 55) * 0.2f;
            f32 freqRatioX = 1.0f + hashFloat(i * 11 + 56) * 2.0f;
            f32 freqRatioY = 1.0f + hashFloat(i * 11 + 57) * 1.5f;

            f32 ox = center.x + std::sin(t * speed * freqRatioX + phaseX) * W * ampX;
            f32 oy = center.y + std::cos(t * speed * freqRatioY + phaseY) * H * ampY;

            if (foreground) {
                f32 converge = smoothstep(timerRamp(0.5f, 1.5f));
                f32 disperse = smoothstep(timerRamp(2.0f, 3.0f));
                f32 pull = converge * (1.0f - disperse);
                ox = ox + (center.x - ox) * pull * 0.6f;
                oy = oy + (center.y - oy) * pull * 0.6f;
            }

            f32 orbAlpha = foreground ? smoothstep(timerRamp(0.3f, 0.8f)) : 1.0f;

            ImVec4 col = orbColor(i);
            f32 r = baseRadius * (0.9f + 0.1f * std::sin(t * 2.0f + i));

            int a3 = static_cast<int>(20 * brightness * orbAlpha * ga);
            int a2 = static_cast<int>(50 * brightness * orbAlpha * ga);
            int a1 = static_cast<int>(180 * brightness * orbAlpha * ga);
            if (a3 > 0) dl->AddCircleFilled(ImVec2(ox, oy), r * 3.0f,
                IM_COL32(static_cast<int>(col.x), static_cast<int>(col.y), static_cast<int>(col.z), a3), 16);
            if (a2 > 0) dl->AddCircleFilled(ImVec2(ox, oy), r * 1.8f,
                IM_COL32(static_cast<int>(col.x), static_cast<int>(col.y), static_cast<int>(col.z), a2), 16);
            if (a1 > 0) dl->AddCircleFilled(ImVec2(ox, oy), r,
                IM_COL32(static_cast<int>(col.x + (255 - col.x) * 0.3f),
                         static_cast<int>(col.y + (255 - col.y) * 0.3f),
                         static_cast<int>(col.z + (255 - col.z) * 0.3f), a1), 16);
        }

        // ========== LAYER 3: Geometric Shapes ==========

        // 3a. 4 diamonds in compass formation
        {
            f32 diamondScale = easeOutCubic(timerRamp(0.4f, 1.0f));
            f32 diamondDrift = smoothstep(timerRamp(1.8f, 3.0f)) * 40.0f;
            f32 diamondR = 12.0f * diamondScale;
            int dAlpha = static_cast<int>(120 * diamondScale * ga);
            if (dAlpha > 0) {
                f32 dist = 120.0f + diamondDrift;
                ImVec2 dPos[4] = {
                    ImVec2(center.x, center.y - dist),
                    ImVec2(center.x + dist, center.y),
                    ImVec2(center.x, center.y + dist),
                    ImVec2(center.x - dist, center.y)
                };
                for (int d = 0; d < 4; d++) {
                    ImVec2 dp[4] = {
                        ImVec2(dPos[d].x, dPos[d].y - diamondR),
                        ImVec2(dPos[d].x + diamondR, dPos[d].y),
                        ImVec2(dPos[d].x, dPos[d].y + diamondR),
                        ImVec2(dPos[d].x - diamondR, dPos[d].y)
                    };
                    dl->AddConvexPolyFilled(dp, 4,
                        IM_COL32(160, 200, 160, dAlpha / 3));
                    dl->AddPolyline(dp, 4, IM_COL32(160, 200, 160, dAlpha), ImDrawFlags_Closed, 1.5f);
                }
            }
        }

        // 3b. 6 small triangles orbiting center
        {
            f32 triAlphaF = smoothstep(timerRamp(0.6f, 1.0f));
            int triAlpha = static_cast<int>(90 * triAlphaF * ga);
            if (triAlpha > 0) {
                f32 orbitR = 180.0f;
                for (int ti = 0; ti < 6; ti++) {
                    f32 orbitAngle = t * 0.4f + ti * 3.14159265f / 3.0f;
                    f32 tx = center.x + std::cos(orbitAngle) * orbitR;
                    f32 ty = center.y + std::sin(orbitAngle) * orbitR;
                    f32 selfRot = t * 1.5f + ti * 1.0f;
                    f32 triR = 8.0f;
                    ImVec2 tp[3];
                    for (int v = 0; v < 3; v++) {
                        f32 a = selfRot + v * 2.0944f;
                        tp[v] = ImVec2(tx + std::cos(a) * triR, ty + std::sin(a) * triR);
                    }
                    dl->AddConvexPolyFilled(tp, 3,
                        IM_COL32(160, 200, 160, triAlpha / 2));
                    dl->AddPolyline(tp, 3, IM_COL32(160, 200, 160, triAlpha), ImDrawFlags_Closed, 1.0f);
                }
            }
        }

        // 3c. 2 pulsing concentric rings
        {
            f32 ringAlphaF = smoothstep(timerRamp(0.5f, 1.0f));
            f32 pulse1 = 200.0f + 5.0f * std::sin(t * 1.2f);
            f32 pulse2 = 250.0f + 5.0f * std::sin(t * 1.2f + 1.5f);
            int ringA = static_cast<int>(40 * ringAlphaF * ga);
            if (ringA > 0) {
                dl->AddCircle(center, pulse1, IM_COL32(100, 140, 200, ringA), 64, 1.0f);
                dl->AddCircle(center, pulse2, IM_COL32(100, 140, 200, ringA / 2), 64, 1.0f);
            }
        }

        // 3d. 4 corner L-bracket lines
        {
            f32 bracketT = easeOutCubic(timerRamp(0.8f, 1.5f));
            int bAlpha = static_cast<int>(80 * bracketT * ga);
            if (bAlpha > 0) {
                f32 margin = 60.0f;
                f32 bLen = 40.0f;
                f32 slideOff = (1.0f - bracketT) * 80.0f;
                u32 bCol = IM_COL32(140, 170, 200, bAlpha);
                dl->AddLine(ImVec2(margin - slideOff, margin - slideOff),
                            ImVec2(margin - slideOff + bLen, margin - slideOff), bCol, 1.5f);
                dl->AddLine(ImVec2(margin - slideOff, margin - slideOff),
                            ImVec2(margin - slideOff, margin - slideOff + bLen), bCol, 1.5f);
                dl->AddLine(ImVec2(W - margin + slideOff, margin - slideOff),
                            ImVec2(W - margin + slideOff - bLen, margin - slideOff), bCol, 1.5f);
                dl->AddLine(ImVec2(W - margin + slideOff, margin - slideOff),
                            ImVec2(W - margin + slideOff, margin - slideOff + bLen), bCol, 1.5f);
                dl->AddLine(ImVec2(margin - slideOff, H - margin + slideOff),
                            ImVec2(margin - slideOff + bLen, H - margin + slideOff), bCol, 1.5f);
                dl->AddLine(ImVec2(margin - slideOff, H - margin + slideOff),
                            ImVec2(margin - slideOff, H - margin + slideOff - bLen), bCol, 1.5f);
                dl->AddLine(ImVec2(W - margin + slideOff, H - margin + slideOff),
                            ImVec2(W - margin + slideOff - bLen, H - margin + slideOff), bCol, 1.5f);
                dl->AddLine(ImVec2(W - margin + slideOff, H - margin + slideOff),
                            ImVec2(W - margin + slideOff, H - margin + slideOff - bLen), bCol, 1.5f);
            }
        }

        // ========== LAYER 3.5: Mantra — "Collaborate Compromise Create" ==========
        // Three words cycle as ghostly watermark text behind the title.
        // Slow crossfade, multi-layer glow, positioned below title area.
        {
            const char* mantraWords[3] = { "Collaborate", "Compromise", "Create" };
            // Slower timing: each word spans ~1.4s with 0.4s overlap for crossfade
            f32 wordStart[3] = { 0.5f, 1.5f, 2.5f };
            f32 wordEnd[3]   = { 1.9f, 2.9f, 3.9f };
            f32 mantraFontSize = 16.0f;

            for (int w = 0; w < 3; w++) {
                f32 wt = timerRamp(wordStart[w], wordEnd[w]);
                if (wt <= 0.0f || wt >= 1.0f) continue;

                // Slow bell-curve: fade in 25%, hold 50%, fade out 25%
                f32 wordAlpha;
                if (wt < 0.25f) wordAlpha = smoothstep(wt / 0.25f);
                else if (wt > 0.75f) wordAlpha = smoothstep((1.0f - wt) / 0.25f);
                else wordAlpha = 1.0f;

                // Gentle upward drift
                f32 driftY = -8.0f * wt;

                // Position: centered, well below the title
                ImVec2 wordSz = font->CalcTextSizeA(mantraFontSize, FLT_MAX, 0.0f, mantraWords[w]);
                f32 wordX = center.x - wordSz.x * 0.5f;
                f32 wordY = center.y + 145.0f + driftY;

                int mAlpha = static_cast<int>(38 * wordAlpha * ga);
                if (mAlpha > 0) {
                    // Multi-layer glow (3 passes at increasing offsets)
                    for (int g = 3; g >= 1; g--) {
                        f32 off = g * 2.0f;
                        int glowA = static_cast<int>((6 + (3 - g) * 3) * wordAlpha * ga);
                        if (glowA > 0) {
                            u32 glowCol = IM_COL32(120, 200, 150, glowA);
                            dl->AddText(nullptr, mantraFontSize, ImVec2(wordX + off, wordY), glowCol, mantraWords[w]);
                            dl->AddText(nullptr, mantraFontSize, ImVec2(wordX - off, wordY), glowCol, mantraWords[w]);
                            dl->AddText(nullptr, mantraFontSize, ImVec2(wordX, wordY + off), glowCol, mantraWords[w]);
                            dl->AddText(nullptr, mantraFontSize, ImVec2(wordX, wordY - off), glowCol, mantraWords[w]);
                        }
                    }
                    // Main text — sage green
                    dl->AddText(nullptr, mantraFontSize, ImVec2(wordX, wordY),
                        IM_COL32(160, 215, 170, mAlpha), mantraWords[w]);
                }
            }
        }

        // ========== LAYER 4: Title "TEGE" ==========

        const char* title = "TEGE";
        f32 splashFontSize = 72.0f;
        ImVec2 titleSz = font->CalcTextSizeA(splashFontSize, FLT_MAX, 0.0f, title);

        {
            f32 revealT = easeOutCubic(timerRamp(1.0f, 1.6f));
            f32 titleScale = 0.9f + 0.1f * revealT;
            f32 scaledFontSize = splashFontSize * titleScale;
            ImVec2 scaledSz = font->CalcTextSizeA(scaledFontSize, FLT_MAX, 0.0f, title);
            ImVec2 titlePos(center.x - scaledSz.x * 0.5f, center.y - scaledSz.y * 0.5f - 10.0f);
            int titleAlpha = static_cast<int>(255 * revealT * ga);

            if (titleAlpha > 0) {
                // Circle halo behind title
                int haloA = static_cast<int>(30 * revealT * ga);
                if (haloA > 0) {
                    dl->AddCircleFilled(ImVec2(center.x, center.y - 10.0f), scaledSz.x * 0.6f,
                        IM_COL32(160, 200, 160, haloA), 32);
                }

                // Multi-layer glow: 3 layers in 4 directions
                for (int layer = 2; layer >= 0; layer--) {
                    f32 off = (layer + 1) * 2.5f;
                    int glowA = static_cast<int>((25 - layer * 7) * revealT * ga);
                    if (glowA > 0) {
                        u32 glowCol = IM_COL32(100, 160, 120, glowA);
                        dl->AddText(nullptr, scaledFontSize, ImVec2(titlePos.x + off, titlePos.y), glowCol, title);
                        dl->AddText(nullptr, scaledFontSize, ImVec2(titlePos.x - off, titlePos.y), glowCol, title);
                        dl->AddText(nullptr, scaledFontSize, ImVec2(titlePos.x, titlePos.y + off), glowCol, title);
                        dl->AddText(nullptr, scaledFontSize, ImVec2(titlePos.x, titlePos.y - off), glowCol, title);
                    }
                }

                // Main title text — sage green
                dl->AddText(nullptr, scaledFontSize, titlePos,
                    IM_COL32(199, 218, 196, titleAlpha), title);

                // Shimmer: white highlight sweep at t=1.8→2.4
                f32 shimmerT = timerRamp(1.8f, 2.4f);
                if (shimmerT > 0.0f && shimmerT < 1.0f) {
                    f32 shimmerX = titlePos.x + scaledSz.x * shimmerT;
                    f32 shimmerW = scaledSz.x * 0.12f;
                    int shimmerA = static_cast<int>(140 * std::sin(shimmerT * 3.14159265f) * ga);
                    if (shimmerA > 0) {
                        // Draw shimmer as a bright vertical band clipped to title region
                        ImVec2 shimmerMin(shimmerX - shimmerW * 0.5f, titlePos.y);
                        ImVec2 shimmerMax(shimmerX + shimmerW * 0.5f, titlePos.y + scaledSz.y);
                        dl->AddRectFilledMultiColor(shimmerMin, shimmerMax,
                            IM_COL32(255, 255, 255, 0),
                            IM_COL32(255, 255, 255, 0),
                            IM_COL32(255, 255, 255, shimmerA),
                            IM_COL32(255, 255, 255, shimmerA));
                        // Re-draw title on top so shimmer is blended behind letters
                        dl->AddText(nullptr, scaledFontSize, titlePos,
                            IM_COL32(220, 235, 218, titleAlpha), title);
                    }
                }
            }
        }

        // ========== LAYER 5: Accent Lines & Flares ==========

        // 5a. Horizontal rules flanking title — grow from center at t=1.2→1.8
        {
            f32 lineGrow = easeOutCubic(timerRamp(1.2f, 1.8f));
            f32 lineHalf = 160.0f * lineGrow;
            int lineA = static_cast<int>(140 * lineGrow * ga);
            if (lineA > 0) {
                f32 lineY1 = center.y - 50.0f;
                f32 lineY2 = center.y + 40.0f;
                // Glow line (wider, dimmer)
                dl->AddLine(ImVec2(center.x - lineHalf, lineY1),
                            ImVec2(center.x + lineHalf, lineY1),
                            IM_COL32(100, 160, 130, lineA / 3), 4.0f);
                dl->AddLine(ImVec2(center.x - lineHalf, lineY2),
                            ImVec2(center.x + lineHalf, lineY2),
                            IM_COL32(100, 160, 130, lineA / 3), 4.0f);
                // Sharp line
                dl->AddLine(ImVec2(center.x - lineHalf, lineY1),
                            ImVec2(center.x + lineHalf, lineY1),
                            IM_COL32(160, 210, 170, lineA), 1.5f);
                dl->AddLine(ImVec2(center.x - lineHalf, lineY2),
                            ImVec2(center.x + lineHalf, lineY2),
                            IM_COL32(160, 210, 170, lineA), 1.5f);

                // 5b. Diamond caps at endpoints
                f32 capR = 4.0f;
                auto drawDiamondCap = [&](f32 cx2, f32 cy2) {
                    ImVec2 dp[4] = {
                        ImVec2(cx2, cy2 - capR), ImVec2(cx2 + capR, cy2),
                        ImVec2(cx2, cy2 + capR), ImVec2(cx2 - capR, cy2)
                    };
                    dl->AddConvexPolyFilled(dp, 4, IM_COL32(200, 230, 200, lineA));
                };
                drawDiamondCap(center.x - lineHalf, lineY1);
                drawDiamondCap(center.x + lineHalf, lineY1);
                drawDiamondCap(center.x - lineHalf, lineY2);
                drawDiamondCap(center.x + lineHalf, lineY2);
            }
        }

        // 5c. Bezier S-curves flanking title area
        {
            f32 bezierAlpha = smoothstep(timerRamp(1.0f, 1.6f));
            int bA = static_cast<int>(60 * bezierAlpha * ga);
            if (bA > 0) {
                // Left curve
                dl->AddBezierCubic(
                    ImVec2(center.x - 200, center.y - 80),
                    ImVec2(center.x - 240, center.y - 20),
                    ImVec2(center.x - 240, center.y + 20),
                    ImVec2(center.x - 200, center.y + 80),
                    IM_COL32(140, 190, 160, bA), 1.5f, 20);
                // Right curve
                dl->AddBezierCubic(
                    ImVec2(center.x + 200, center.y - 80),
                    ImVec2(center.x + 240, center.y - 20),
                    ImVec2(center.x + 240, center.y + 20),
                    ImVec2(center.x + 200, center.y + 80),
                    IM_COL32(140, 190, 160, bA), 1.5f, 20);
            }
        }

        // 5d. 12-sparkle burst from center at t=1.5→2.1
        {
            f32 sparkT = timerRamp(1.5f, 2.1f);
            if (sparkT > 0.0f && sparkT < 1.0f) {
                f32 sparkFade = std::sin(sparkT * 3.14159265f);
                for (int s = 0; s < 12; s++) {
                    f32 angle = s * 3.14159265f / 6.0f + 0.2f;
                    f32 dist = 30.0f + sparkT * 120.0f;
                    f32 sx = center.x + std::cos(angle) * dist;
                    f32 sy = (center.y - 10.0f) + std::sin(angle) * dist;
                    bool isSage = (s % 2 == 0);
                    int sA = static_cast<int>((isSage ? 180 : 220) * sparkFade * ga);
                    u32 sCol = isSage ? IM_COL32(160, 200, 160, sA)
                                      : IM_COL32(240, 240, 230, sA);
                    // Sparkle dot
                    dl->AddCircleFilled(ImVec2(sx, sy), 2.5f, sCol, 8);
                    // Trail line back toward center
                    f32 trailDist = dist - 15.0f;
                    if (trailDist > 0.0f) {
                        f32 tx = center.x + std::cos(angle) * trailDist;
                        f32 ty = (center.y - 10.0f) + std::sin(angle) * trailDist;
                        dl->AddLine(ImVec2(tx, ty), ImVec2(sx, sy),
                            IM_COL32(isSage ? 160 : 240, isSage ? 200 : 240,
                                     isSage ? 160 : 230, sA / 2), 1.0f);
                    }
                }
            }
        }

        // ========== LAYER 6: Info Text ==========

        // "by marty64" — fade in + slide up at t=1.8→2.2
        {
            f32 creditT = easeOutCubic(timerRamp(1.8f, 2.2f));
            int creditA = static_cast<int>(160 * creditT * ga);
            if (creditA > 0) {
                const char* credit = "by marty64";
                ImVec2 creditSz = ImGui::CalcTextSize(credit);
                f32 slideY = center.y + 60.0f + (1.0f - creditT) * 15.0f;
                dl->AddText(ImVec2(center.x - creditSz.x * 0.5f, slideY),
                    IM_COL32(160, 165, 180, creditA), credit);
            }
        }

        // Version string — fade in at t=2.0→2.4
        {
            f32 verT = smoothstep(timerRamp(2.0f, 2.4f));
            int verA = static_cast<int>(140 * verT * ga);
            if (verA > 0) {
                const char* version = "v" ENJIN_VERSION_STRING;
                ImVec2 verSz = ImGui::CalcTextSize(version);
                dl->AddText(ImVec2(center.x - verSz.x * 0.5f, H - 45.0f),
                    IM_COL32(100, 110, 140, verA), version);
            }
        }

        // Spinning arc loader — smooth rotating 120-degree arc
        {
            f32 loaderT = smoothstep(timerRamp(0.5f, 0.8f));
            int loaderA = static_cast<int>(120 * loaderT * ga);
            // Fade out loader when title is fully revealed
            f32 loaderFadeOut = smoothstep(timerRamp(2.2f, 2.6f));
            loaderA = static_cast<int>(loaderA * (1.0f - loaderFadeOut));
            if (loaderA > 0) {
                f32 loaderY = center.y + 100.0f;
                f32 loaderR = 12.0f;
                f32 arcStart = t * 4.0f;  // radians per second rotation
                f32 arcLen = 2.0944f;      // 120 degrees
                int segments = 20;
                for (int s = 0; s < segments; s++) {
                    f32 a1 = arcStart + arcLen * s / segments;
                    f32 a2 = arcStart + arcLen * (s + 1) / segments;
                    // Gradient alpha along arc
                    f32 segAlpha = static_cast<f32>(s) / segments;
                    int sA = static_cast<int>(loaderA * (0.3f + 0.7f * segAlpha));
                    dl->AddLine(
                        ImVec2(center.x + std::cos(a1) * loaderR, loaderY + std::sin(a1) * loaderR),
                        ImVec2(center.x + std::cos(a2) * loaderR, loaderY + std::sin(a2) * loaderR),
                        IM_COL32(160, 200, 160, sA), 2.0f);
                }
            }
        }
    }
    ImGui::End();

    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
}


void EditorLayer::ShowNotification(const std::string& message, NotificationType type) {
    EditorNotification notif;
    notif.message = message;
    notif.type = type;
    notif.lifetime = (type == NotificationType::Error) ? 5.0f : 3.0f;
    notif.elapsed = 0.0f;
    notif.slideIn = 0.0f;
    m_Notifications.push_back(std::move(notif));
}

void EditorLayer::DrawNotifications(f32 deltaTime) {
    if (m_Notifications.empty()) return;

    ImGuiIO& io = ImGui::GetIO();
    f32 padding = 16.0f;
    f32 toastW = 300.0f;
    f32 toastH = 40.0f;
    f32 spacing = 6.0f;
    f32 startX = io.DisplaySize.x - toastW - padding;
    f32 startY = io.DisplaySize.y - padding;

    ImDrawList* fg = ImGui::GetForegroundDrawList();

    // Update and draw toasts from bottom up
    i32 visibleIdx = 0;
    for (i32 i = static_cast<i32>(m_Notifications.size()) - 1; i >= 0; --i) {
        auto& notif = m_Notifications[i];
        notif.elapsed += deltaTime;

        // Slide-in animation (0 to 1 over 200ms)
        if (notif.slideIn < 1.0f) {
            notif.slideIn += deltaTime * 5.0f;
            if (notif.slideIn > 1.0f) notif.slideIn = 1.0f;
        }

        // Fade out in last 0.5s
        f32 fadeAlpha = 1.0f;
        f32 remaining = notif.lifetime - notif.elapsed;
        if (remaining < 0.5f) {
            fadeAlpha = remaining / 0.5f;
            if (fadeAlpha < 0.0f) fadeAlpha = 0.0f;
        }

        // Slide from right
        f32 slideOffset = (1.0f - notif.slideIn) * (toastW + padding);
        f32 yPos = startY - (toastH + spacing) * (visibleIdx + 1);
        f32 xPos = startX + slideOffset;

        // Background color based on type
        ImU32 bgColor;
        switch (notif.type) {
            case NotificationType::Success: bgColor = IM_COL32(30, 110, 50, static_cast<u8>(220 * fadeAlpha)); break;
            case NotificationType::Warning: bgColor = IM_COL32(140, 110, 20, static_cast<u8>(220 * fadeAlpha)); break;
            case NotificationType::Error:   bgColor = IM_COL32(150, 30, 30, static_cast<u8>(220 * fadeAlpha)); break;
            default:                        bgColor = IM_COL32(40, 60, 90, static_cast<u8>(220 * fadeAlpha)); break;
        }

        ImVec2 p0(xPos, yPos);
        ImVec2 p1(xPos + toastW, yPos + toastH);
        fg->AddRectFilled(p0, p1, bgColor, 6.0f);

        // Type icon
        const char* icon;
        switch (notif.type) {
            case NotificationType::Success: icon = "[OK]"; break;
            case NotificationType::Warning: icon = "[!]"; break;
            case NotificationType::Error:   icon = "[X]"; break;
            default:                        icon = "[i]"; break;
        }

        ImU32 textColor = IM_COL32(255, 255, 255, static_cast<u8>(240 * fadeAlpha));
        fg->AddText(ImVec2(xPos + 10, yPos + 11), textColor, icon);
        fg->AddText(ImVec2(xPos + 40, yPos + 11), textColor, notif.message.c_str());

        visibleIdx++;
    }

    // Remove expired notifications
    m_Notifications.erase(
        std::remove_if(m_Notifications.begin(), m_Notifications.end(),
            [](const EditorNotification& n) { return n.elapsed >= n.lifetime; }),
        m_Notifications.end());
}

// ============================================================================
// Accent Color Picker (with harmony presets)
// ============================================================================

void EditorLayer::DrawDeleteConfirmModal() {
    ImGui::OpenPopup("Delete Entities?");
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("Delete Entities?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        usize count = m_PendingDeleteEntities.size();
        if (count == 1) {
            auto* nc = m_World ? m_World->GetComponent<ECS::NameComponent>(m_PendingDeleteEntities[0]) : nullptr;
            std::string name = nc ? nc->name : "Entity " + std::to_string(m_PendingDeleteEntities[0]);
            ImGui::Text("Delete \"%s\"?", name.c_str());
        } else {
            ImGui::Text("Delete %zu entities?", count);
        }
        ImGui::TextDisabled("This can be undone with Ctrl+Z.");
        ImGui::Spacing();
        if (ImGui::Button("Delete", ImVec2(120, 0))) {
            DeleteSelectedEntities();
            m_ShowDeleteConfirm = false;
            m_PendingDeleteEntities.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            m_ShowDeleteConfirm = false;
            m_PendingDeleteEntities.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    } else {
        m_ShowDeleteConfirm = false;
        m_PendingDeleteEntities.clear();
    }
}

void EditorLayer::ImportModel(const std::string& path) {
    if (!m_World) {
        ENJIN_LOG_ERROR(Editor, "Cannot import model: no world loaded");
        m_ConsoleLog.push_back("[Error] Cannot import model: no world loaded");
        return;
    }

    // Auto-create a project if none is loaded (project-first workflow)
    EnsureProjectForScene(path);

    // Pre-fill from .enjinasset if re-importing
    if (Assets::AssetMetadata::Exists(path)) {
        Assets::AssetMetadata meta;
        if (meta.Load(path)) {
            m_ImportDialogOptions = meta.importOptions;
        }
    } else {
        m_ImportDialogOptions = Assets::ImportOptions{};
    }

    m_ImportDialogPath = path;
    m_ShowImportDialog = true;

    // Cache file info once on dialog open (avoid per-frame filesystem calls)
    std::filesystem::path filePath(path);
    m_ImportDialogFilename = filePath.filename().string();
    m_ImportDialogExtension = filePath.extension().string();
    m_ImportDialogFileSize = 0;
    try {
        if (std::filesystem::exists(filePath)) {
            m_ImportDialogFileSize = std::filesystem::file_size(filePath);
        }
    } catch (...) {}
    m_ImportDialogIsReimport = Assets::AssetMetadata::Exists(path);

    // Auto-detect source app from file metadata
    m_ImportDialogDetectedApp = Assets::SourceApp::Auto;
    m_ImportDialogScaleFromPreset = false;
    std::string ext = m_ImportDialogExtension;
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    if (ext == ".gltf" || ext == ".glb") {
        // Quick parse: load glTF to get generator string
        Assets::GLTFScene tempScene;
        if (Assets::GLTFLoader::Load(path, tempScene)) {
            if (!tempScene.generator.empty()) {
                std::string g = tempScene.generator;
                std::transform(g.begin(), g.end(), g.begin(), ::tolower);
                if (g.find("blender") != std::string::npos) m_ImportDialogDetectedApp = Assets::SourceApp::Blender;
                else if (g.find("maya") != std::string::npos) m_ImportDialogDetectedApp = Assets::SourceApp::Maya;
                else if (g.find("3ds max") != std::string::npos || g.find("3dsmax") != std::string::npos) m_ImportDialogDetectedApp = Assets::SourceApp::Max3ds;
                else if (g.find("houdini") != std::string::npos) m_ImportDialogDetectedApp = Assets::SourceApp::Houdini;
                else if (g.find("cinema") != std::string::npos || g.find("c4d") != std::string::npos) m_ImportDialogDetectedApp = Assets::SourceApp::Cinema4D;
                else if (g.find("zbrush") != std::string::npos) m_ImportDialogDetectedApp = Assets::SourceApp::ZBrush;
                else if (g.find("substance") != std::string::npos) m_ImportDialogDetectedApp = Assets::SourceApp::SubstancePainter;
                else if (g.find("unreal") != std::string::npos) m_ImportDialogDetectedApp = Assets::SourceApp::Unreal;
                else if (g.find("unity") != std::string::npos) m_ImportDialogDetectedApp = Assets::SourceApp::Unity;
                else if (g.find("sketchup") != std::string::npos) m_ImportDialogDetectedApp = Assets::SourceApp::SketchUp;
            }
        }
    }
}

void EditorLayer::DrawImportDialog() {
    ImGui::OpenPopup("Import Settings");

    ImGui::SetNextWindowSize(ImVec2(480 * m_EditorSettings.uiScale, 0), ImGuiCond_Always);
    if (ImGui::BeginPopupModal("Import Settings", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        // File info (cached values — no filesystem calls per frame)
        ImGui::Text("File: %s", m_ImportDialogFilename.c_str());
        ImGui::Text("Format: %s", m_ImportDialogExtension.c_str());

        if (m_ImportDialogFileSize > 0) {
            if (m_ImportDialogFileSize < 1024) {
                ImGui::Text("Size: %llu bytes", (unsigned long long)m_ImportDialogFileSize);
            } else if (m_ImportDialogFileSize < 1024 * 1024) {
                ImGui::Text("Size: %.1f KB", m_ImportDialogFileSize / 1024.0f);
            } else {
                ImGui::Text("Size: %.1f MB", m_ImportDialogFileSize / (1024.0f * 1024.0f));
            }
        }

        // Re-import indicator
        if (m_ImportDialogIsReimport) {
            ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "(Re-import)");
        }

        ImGui::Separator();
        ImGui::Spacing();

        // --- Source Application ---
        ImGui::Text("Source Application");

        // Build combo label with auto-detect hint
        const char* sourceAppNames[] = {
            "Auto", "Blender", "Maya", "3ds Max", "Houdini", "Cinema 4D",
            "ZBrush", "Substance Painter", "Unreal Engine", "Unity", "SketchUp", "Custom"
        };
        int currentApp = static_cast<int>(m_ImportDialogOptions.sourceApp);

        // Show detected hint
        if (m_ImportDialogDetectedApp != Assets::SourceApp::Auto) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.5f, 0.8f, 0.5f, 1.0f), "(Detected: %s)",
                               Assets::GetSourceAppName(m_ImportDialogDetectedApp));
        }

        if (ImGui::Combo("##SourceApp", &currentApp, sourceAppNames, IM_ARRAYSIZE(sourceAppNames))) {
            Assets::SourceApp newApp = static_cast<Assets::SourceApp>(currentApp);
            m_ImportDialogOptions.sourceApp = newApp;
            // Auto-fill scale from preset when switching source app
            if (newApp != Assets::SourceApp::Auto && newApp != Assets::SourceApp::Custom) {
                Assets::SourceAppPreset preset = Assets::GetSourceAppPreset(newApp);
                m_ImportDialogOptions.scale = preset.scale;
                m_ImportDialogScaleFromPreset = true;
            }
        }

        // Show read-only preset values when a specific source app is selected
        {
            Assets::SourceApp selectedApp = m_ImportDialogOptions.sourceApp;
            if (selectedApp != Assets::SourceApp::Auto && selectedApp != Assets::SourceApp::Custom) {
                Assets::SourceAppPreset preset = Assets::GetSourceAppPreset(selectedApp);
                ImGui::Indent(16.0f);
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
                ImGui::Text("Preset scale: %.4f", preset.scale);
                ImGui::Text("Z-up to Y-up: %s", preset.zUpToYUp ? "Yes" : "No");
                ImGui::Text("Left to right hand: %s", preset.leftToRight ? "Yes" : "No");
                ImGui::PopStyleColor();
                ImGui::Unindent(16.0f);
            }
        }

        ImGui::Checkbox("Apply Axis Conversion", &m_ImportDialogOptions.convertAxes);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Automatically convert coordinate system (Z-up to Y-up, handedness) based on source app");
        }

        if (m_ImportDialogOptions.convertAxes) {
            ImGui::Indent(16.0f);
            ImGui::Checkbox("Flip X", &m_ImportDialogOptions.flipX);
            ImGui::SameLine();
            ImGui::Checkbox("Flip Y", &m_ImportDialogOptions.flipY);
            ImGui::SameLine();
            ImGui::Checkbox("Flip Z", &m_ImportDialogOptions.flipZ);
            ImGui::Unindent(16.0f);
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // --- Import Options ---
        ImGui::Text("Import Options");
        ImGui::DragFloat("Scale", &m_ImportDialogOptions.scale, 0.01f, 0.001f, 100.0f, "%.3f");
        ImGui::Checkbox("Import Materials", &m_ImportDialogOptions.importMaterials);
        ImGui::Checkbox("Import Animations", &m_ImportDialogOptions.importAnimations);
        ImGui::Checkbox("Generate Colliders", &m_ImportDialogOptions.generateColliders);
        ImGui::Checkbox("Generate LODs", &m_ImportDialogOptions.generateLODs);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Generate Level-of-Detail meshes (can be slow for large models)");
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // --- Texture Search Paths ---
        ImGui::Text("Texture Search Paths");
        auto& searchPaths = m_ImportDialogOptions.textureSearchPaths;
        i32 removeIdx = -1;
        for (i32 i = 0; i < static_cast<i32>(searchPaths.size()); ++i) {
            ImGui::PushID(i);
            ImGui::Text("%s", searchPaths[i].c_str());
            ImGui::SameLine();
            if (ImGui::SmallButton("X")) {
                removeIdx = i;
            }
            ImGui::PopID();
        }
        if (removeIdx >= 0) {
            searchPaths.erase(searchPaths.begin() + removeIdx);
        }
        if (ImGui::SmallButton("+ Add Path...")) {
            // Use nfd or a simple text input — for now, use current model's parent dir as a starting point
            // We'll add a simple text input approach
            searchPaths.push_back("");
        }
        // Editable text input for the last empty entry
        if (!searchPaths.empty() && searchPaths.back().empty()) {
            static char pathBuf[512] = {};
            ImGui::SetNextItemWidth(-1);
            if (ImGui::InputText("##NewTexPath", pathBuf, sizeof(pathBuf),
                                 ImGuiInputTextFlags_EnterReturnsTrue)) {
                if (pathBuf[0] != '\0') {
                    searchPaths.back() = pathBuf;
                    pathBuf[0] = '\0';
                } else {
                    searchPaths.pop_back();
                }
            }
            if (ImGui::IsItemDeactivated() && pathBuf[0] == '\0') {
                searchPaths.pop_back();
            }
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // --- Texture Compression ---
        ImGui::Text("Texture Compression");
        {
            const char* formatNames[] = {
                "None (RGBA8)", "BC1 (DXT1 - RGB)", "BC3 (DXT5 - RGBA)",
                "BC4 (R only)", "BC5 (RG - Normal maps)", "BC7 (High quality RGBA)",
                "ASTC 4x4", "ASTC 6x6", "ASTC 8x8"
            };
            int currentFmt = static_cast<int>(m_ImportDialogOptions.textureCompression.format);
            if (ImGui::Combo("Format##TexCompress", &currentFmt, formatNames, IM_ARRAYSIZE(formatNames))) {
                m_ImportDialogOptions.textureCompression.format = static_cast<Assets::CompressedFormat>(currentFmt);
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("BC1: RGB 4bpp | BC3: RGBA 8bpp | BC5: Normal maps | BC7: Best quality RGBA 8bpp");
            }

            if (m_ImportDialogOptions.textureCompression.format != Assets::CompressedFormat::None) {
                const char* qualityNames[] = { "Fast", "Normal", "High" };
                int currentQuality = static_cast<int>(m_ImportDialogOptions.textureCompression.quality);
                ImGui::Combo("Quality##TexCompress", &currentQuality, qualityNames, IM_ARRAYSIZE(qualityNames));
                m_ImportDialogOptions.textureCompression.quality = static_cast<Assets::CompressionQuality>(currentQuality);

                ImGui::Checkbox("Generate Mipmaps##TexCompress", &m_ImportDialogOptions.textureCompression.generateMipmaps);
                ImGui::Checkbox("sRGB Color Space##TexCompress", &m_ImportDialogOptions.textureCompression.sRGB);

                // Show compression ratio hint
                f32 ratio = Assets::TextureCompressor::CompressionRatio(m_ImportDialogOptions.textureCompression.format);
                u32 bpp = Assets::TextureCompressor::BitsPerPixel(m_ImportDialogOptions.textureCompression.format);
                ImGui::TextDisabled("%s: %u bpp (%.1fx compression)",
                    Assets::TextureCompressor::FormatName(m_ImportDialogOptions.textureCompression.format),
                    bpp, ratio);
            }
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Buttons
        if (ImGui::Button("Import", ImVec2(120, 0))) {
            // Defer import to next frame so a loading overlay can render first
            m_ImportPending = true;
            m_ImportPendingPath = m_ImportDialogPath;
            m_ImportPendingOptions = m_ImportDialogOptions;
            m_ShowImportDialog = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            m_ShowImportDialog = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void EditorLayer::DrawImportLoadingOverlay() {
    // Draw a full-viewport overlay so the user sees feedback while the import blocks
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);
    ImGui::SetNextWindowBgAlpha(0.7f);
    ImGui::Begin("##ImportLoading", nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
        ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBringToFrontOnFocus);

    ImVec2 center(viewport->Pos.x + viewport->Size.x * 0.5f,
                  viewport->Pos.y + viewport->Size.y * 0.5f);
    std::string filename = std::filesystem::path(m_ImportPendingPath).filename().string();
    std::string text = "Importing " + filename + "...";
    ImVec2 textSize = ImGui::CalcTextSize(text.c_str());
    ImGui::SetCursorScreenPos(ImVec2(center.x - textSize.x * 0.5f, center.y - textSize.y * 0.5f));
    ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "%s", text.c_str());

    ImGui::End();
}

void EditorLayer::ExecuteImport(const std::string& path, const Assets::ImportOptions& options) {
    if (!m_World) return;

    Assets::ImportResult result = Assets::SceneImporter::Import(path, m_World, options);

    if (result.success) {
        // Detailed summary log
        std::stringstream ss;
        ss << "[Info] Imported " << result.entities.size() << " entities from "
           << std::filesystem::path(path).filename().string();
        m_ConsoleLog.push_back(ss.str());

        ss.str("");
        ss << "[Info]   Meshes: " << result.meshCount
           << ", Materials: " << result.materialCount
           << ", Animations: " << result.animationCount;
        m_ConsoleLog.push_back(ss.str());

        ss.str("");
        ss << "[Info]   Vertices: " << result.totalVertexCount
           << ", Indices: " << result.totalIndexCount;
        m_ConsoleLog.push_back(ss.str());

        if (!result.texturePathsResolved.empty()) {
            ss.str("");
            ss << "[Info]   Textures resolved: " << result.texturePathsResolved.size();
            m_ConsoleLog.push_back(ss.str());
        }
        if (!result.texturePathsMissing.empty()) {
            ss.str("");
            ss << "[Warn]   Textures missing: " << result.texturePathsMissing.size();
            m_ConsoleLog.push_back(ss.str());
            for (const auto& missing : result.texturePathsMissing) {
                m_ConsoleLog.push_back("  [Warn]     " + missing);
            }
        }
        for (const auto& warning : result.warnings) {
            m_ConsoleLog.push_back("[Warn]   " + warning);
        }

        ENJIN_LOG_INFO(Editor, "Imported %zu entities (%u meshes, %u materials, %u anims, %u verts, %u indices) from %s",
            result.entities.size(), result.meshCount, result.materialCount, result.animationCount,
            result.totalVertexCount, result.totalIndexCount, path.c_str());

        // If the scene has no lights, add a directional light so imported models are visible
        {
            auto lightEntities = m_World->GetEntitiesWithComponent<ECS::LightComponent>();
            if (lightEntities.empty()) {
                ECS::Entity light = m_World->CreateEntity();
                m_World->AddComponent<ECS::NameComponent>(light, "Directional Light");
                auto& xform = m_World->AddComponent<ECS::TransformComponent>(light);
                xform.rotation = Math::Quaternion::FromEuler(Math::Vector3(-45.0f, -45.0f, 0.0f));
                auto& lc = m_World->AddComponent<ECS::LightComponent>(light);
                lc.type = ECS::LightType::Directional;
                lc.color = Math::Vector3(1.0f, 1.0f, 1.0f);
                lc.intensity = 1.5f;
                m_ConsoleLog.push_back("[Info] Added directional light (scene had no lights)");

                // Raise ambient so model is visible from all angles
                if (m_RenderSystem) {
                    m_RenderSystem->SetAmbientIntensity(1.0f);
                    m_RenderSystem->SetAmbientColor(Math::Vector3(0.3f, 0.3f, 0.35f));
                }
            }
        }

        // Log diagnostic info about the imported model
        for (ECS::Entity e : result.entities) {
            auto* mesh = m_World->GetComponent<ECS::MeshComponent>(e);
            auto* xf = m_World->GetComponent<ECS::TransformComponent>(e);
            if (mesh && xf) {
                std::stringstream ds;
                ds << "[Info]   Entity mesh: " << mesh->vertices.size() << " verts, "
                   << mesh->indices.size() << " indices, valid=" << (mesh->IsValid() ? "yes" : "NO")
                   << ", pos=(" << xf->position.x << "," << xf->position.y << "," << xf->position.z << ")"
                   << ", scale=(" << xf->scale.x << "," << xf->scale.y << "," << xf->scale.z << ")";
                m_ConsoleLog.push_back(ds.str());
            }
        }

        // Select the root entity and focus camera on the first child with a mesh
        // (the root is just a transform node for scale, it has no geometry)
        if (result.rootEntity != ECS::INVALID_ENTITY) {
            SelectEntity(result.rootEntity);
            ECS::Entity focusTarget = result.rootEntity;
            for (auto e : result.entities) {
                if (e != result.rootEntity && m_World->HasComponent<ECS::MeshComponent>(e)) {
                    focusTarget = e;
                    break;
                }
            }
            FocusOnEntity(focusTarget);
        }

        // Save .enjinasset metadata
        Assets::AssetMetadata meta;
        meta.PopulateFromResult(result, path, options);
        meta.Save(path);

        // Track for re-import
        m_LastImportedModelPath = path;

        ShowNotification("Imported: " + std::filesystem::path(path).filename().string() +
            " (" + std::to_string(result.meshCount) + " meshes)", NotificationType::Success);
    } else {
        std::stringstream ss;
        ss << "[Error] Failed to import: " << result.errorMessage;
        m_ConsoleLog.push_back(ss.str());
        ENJIN_LOG_ERROR(Editor, "Failed to import %s: %s", path.c_str(), result.errorMessage.c_str());
        ShowNotification("Import failed: " + std::filesystem::path(path).filename().string(), NotificationType::Error);
    }
}


void EditorLayer::DrawBuildDialog() {
    f32 s = m_EditorSettings.uiScale;
    ImGui::SetNextWindowSize(ImVec2(550 * s, 500 * s), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Build Game", &m_ShowBuildDialog)) {
        ImGui::End();
        return;
    }

    bool hasProject = !m_SceneManager.GetProjectPath().empty();

    // Auto-create project from saved scene if possible (try once, not every frame)
    static bool triedAutoCreate = false;
    if (!hasProject && !m_CurrentScenePath.empty() && !triedAutoCreate) {
        triedAutoCreate = true;
        EnsureProjectForScene(m_CurrentScenePath);
        hasProject = !m_SceneManager.GetProjectPath().empty();
    }
    if (hasProject) triedAutoCreate = false; // Reset for next time dialog opens

    if (!hasProject) {
        ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f),
            "Save your scene first (Ctrl+S), then Build will be available.");
        if (ImGui::Button("Save Scene Now")) {
            std::vector<FileFilter> filters = {{ "Enjin Scene", "*.enjin" }, { "All Files", "*.*" }};
            std::string path = FileDialog::SaveFile("Save Scene", filters, "", "scene.enjin");
            if (!path.empty()) {
                SaveScene(path);
            }
        }
        ImGui::End();
        return;
    }

    // --- Config section ---
    ImGui::SeparatorText("Build Configuration");

    // Output directory
    static char outputDir[512] = {};
    if (outputDir[0] == '\0' && !m_BuildConfig.outputDir.empty()) {
        std::strncpy(outputDir, m_BuildConfig.outputDir.c_str(), sizeof(outputDir) - 1);
    }
    ImGui::InputText("Output Dir", outputDir, sizeof(outputDir));
    ImGui::SameLine();
    if (ImGui::Button("Browse##OutputDir")) {
        std::vector<FileFilter> filters = {{ "All Files", "*.*" }};
        std::string path = FileDialog::SaveFile("Select Output Directory", filters, "", "Build");
        if (!path.empty()) {
            // Use parent dir if user selected a file
            auto p = std::filesystem::path(path);
            if (p.has_extension()) p = p.parent_path();
            std::strncpy(outputDir, p.string().c_str(), sizeof(outputDir) - 1);
            outputDir[sizeof(outputDir) - 1] = '\0';
        }
    }
    m_BuildConfig.outputDir = outputDir;

    // Window title
    static char windowTitle[256] = {};
    if (windowTitle[0] == '\0' && !m_BuildConfig.windowTitle.empty()) {
        std::strncpy(windowTitle, m_BuildConfig.windowTitle.c_str(), sizeof(windowTitle) - 1);
    }
    ImGui::InputText("Window Title", windowTitle, sizeof(windowTitle));
    m_BuildConfig.windowTitle = windowTitle;

    // Resolution
    int w = static_cast<int>(m_BuildConfig.windowWidth);
    int h = static_cast<int>(m_BuildConfig.windowHeight);
    ImGui::InputInt("Width", &w);
    ImGui::InputInt("Height", &h);
    if (w > 0) m_BuildConfig.windowWidth = static_cast<u32>(w);
    if (h > 0) m_BuildConfig.windowHeight = static_cast<u32>(h);

    ImGui::Checkbox("Fullscreen", &m_BuildConfig.fullscreen);

    // Packaging mode
    ImGui::Spacing();
    const char* packagingLabels[] = { "Packed", "Packed (Moddable)", "Loose Files" };
    int packMode = static_cast<int>(m_BuildConfig.packagingMode);
    if (ImGui::Combo("Packaging", &packMode, packagingLabels, 3)) {
        m_BuildConfig.packagingMode = static_cast<Build::PackagingMode>(packMode);
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(
            "Packed: Assets bundled into .enjpak with XOR obfuscation (most secure)\n"
            "Packed (Moddable): Assets bundled into .enjpak without obfuscation (modders can read/replace)\n"
            "Loose Files: Assets copied as-is to the output directory (easiest to mod, largest output)");
    }

    // Build key (only shown for Packed mode — not needed for PackedOpen or LooseFiles)
    if (m_BuildConfig.packagingMode == Build::PackagingMode::Packed) {
        static char buildKey[256] = {};
        ImGui::InputText("Pack Key (optional)", buildKey, sizeof(buildKey),
                         ImGuiInputTextFlags_Password);
        m_BuildConfig.buildKey = buildKey;
    } else {
        m_BuildConfig.buildKey = "";
    }

    ImGui::Spacing();

    // Project info
    ImGui::Text("Project: %s", m_SceneManager.GetProjectName().c_str());
    ImGui::Text("Scenes: %zu", m_SceneManager.GetSceneCount());

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // --- Build button ---
    bool canBuild = !m_BuildInProgress && !m_BuildConfig.outputDir.empty();
    if (!canBuild) ImGui::BeginDisabled();
    if (ImGui::Button("Build", ImVec2(120, 30))) {
        // Auto-save current scene before building so the .enjin file
        // on disk contains all current entities and mesh data
        if (!m_CurrentScenePath.empty()) {
            SaveScene(m_CurrentScenePath);
            ENJIN_LOG_INFO(Editor, "Auto-saved scene before build: %s", m_CurrentScenePath.c_str());
        }
        ENJIN_LOG_INFO(Editor, "Build: projectPath='%s' scenePath='%s'",
            m_SceneManager.GetProjectPath().c_str(), m_CurrentScenePath.c_str());
        m_BuildConfig.projectPath = m_SceneManager.GetProjectPath();
        m_BuildInProgress = true;
        m_BuildFinished = false;
        m_BuildProgress = 0.0f;
        m_BuildResult = Build::BuildResult{};

        Build::BuildPipeline pipeline;
        pipeline.SetProgressCallback([this](const std::string& phase, float progress) {
            m_BuildProgressPhase = phase;
            m_BuildProgress = progress;
        });

        m_BuildResult = pipeline.Execute(m_BuildConfig);
        m_BuildInProgress = false;
        m_BuildFinished = true;
        m_Telemetry.TrackBuildRun();
        if (m_BuildResult.success) {
            ShowNotification("Build complete!", NotificationType::Success);
        } else {
            ShowNotification("Build failed", NotificationType::Error);
        }
    }
    if (!canBuild) ImGui::EndDisabled();

    // Progress bar
    if (m_BuildInProgress || m_BuildFinished) {
        ImGui::SameLine();
        if (m_BuildInProgress) {
            ImGui::ProgressBar(m_BuildProgress, ImVec2(-1, 0),
                               m_BuildProgressPhase.c_str());
        } else if (m_BuildResult.success) {
            ImGui::TextColored(ImVec4(0.2f, 0.9f, 0.2f, 1.0f), "Build succeeded!");
        } else {
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Build failed!");
        }
    }

    // Open output folder button
    if (m_BuildFinished && m_BuildResult.success) {
        ImGui::SameLine();
        if (ImGui::Button("Open Folder")) {
#ifdef ENJIN_PLATFORM_WINDOWS
            ShellExecuteA(nullptr, "open", m_BuildConfig.outputDir.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#elif defined(ENJIN_PLATFORM_MACOS)
            // S20: Use posix_spawn to avoid shell command injection
            {
                const char* argv[] = { "open", m_BuildConfig.outputDir.c_str(), nullptr };
                pid_t pid = 0;
                extern char** environ;
                posix_spawnp(&pid, "open", nullptr, nullptr, const_cast<char**>(argv), environ);
            }
#else
            // S20: Use posix_spawn to avoid shell command injection
            {
                const char* argv[] = { "xdg-open", m_BuildConfig.outputDir.c_str(), nullptr };
                pid_t pid = 0;
                extern char** environ;
                posix_spawnp(&pid, "xdg-open", nullptr, nullptr, const_cast<char**>(argv), environ);
            }
#endif
        }
    }

    // --- Build log ---
    if (m_BuildFinished && !m_BuildResult.messages.empty()) {
        ImGui::Spacing();
        ImGui::SeparatorText("Build Log");

        ImGui::BeginChild("BuildLog", ImVec2(0, 0), ImGuiChildFlags_Borders);
        for (const auto& msg : m_BuildResult.messages) {
            ImVec4 color;
            const char* prefix;
            switch (msg.severity) {
                case Build::MessageSeverity::Error:
                    color = ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
                    prefix = "[ERROR] ";
                    break;
                case Build::MessageSeverity::Warning:
                    color = ImVec4(1.0f, 0.85f, 0.2f, 1.0f);
                    prefix = "[WARN]  ";
                    break;
                default:
                    color = ImVec4(0.8f, 0.8f, 0.8f, 1.0f);
                    prefix = "[INFO]  ";
                    break;
            }
            ImGui::TextColored(color, "%s%s", prefix, msg.text.c_str());
        }

        // Build stats
        if (m_BuildResult.success) {
            ImGui::Separator();
            ImGui::Text("Files packed: %u", m_BuildResult.filesPacked);
            ImGui::Text("Original size: %llu bytes", static_cast<unsigned long long>(m_BuildResult.totalSizeBytes));
            ImGui::Text("Packed size: %llu bytes", static_cast<unsigned long long>(m_BuildResult.packedSizeBytes));
        }

        // Auto-scroll to bottom
        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
            ImGui::SetScrollHereY(1.0f);
        }
        ImGui::EndChild();
    }

    ImGui::End();
}

// ---------------------------------------------------------------------------
// New Project Dialog (standalone, not the Project Hub wizard)
// ---------------------------------------------------------------------------

void EditorLayer::DrawNewProjectDialog() {
    f32 s = m_EditorSettings.uiScale;
    ImGui::SetNextWindowSize(ImVec2(480 * s, 280 * s), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("New Project", &m_ShowNewProjectDialog)) {
        ImGui::End();
        return;
    }

    ImGui::InputText("Project Name", m_NewProjDlgName, sizeof(m_NewProjDlgName));

    ImGui::InputText("Location", m_NewProjDlgLocation, sizeof(m_NewProjDlgLocation));
    ImGui::SameLine();
    if (ImGui::Button("Browse##NewProjLoc")) {
        std::string folder = FileDialog::OpenFolder("Select Project Location",
                                                     m_NewProjDlgLocation);
        if (!folder.empty()) {
            std::strncpy(m_NewProjDlgLocation, folder.c_str(),
                         sizeof(m_NewProjDlgLocation) - 1);
            m_NewProjDlgLocation[sizeof(m_NewProjDlgLocation) - 1] = '\0';
        }
    }

    ImGui::InputText("Scene Name", m_NewProjDlgScene, sizeof(m_NewProjDlgScene));

    const char* templateNames[] = { "Empty 3D", "Empty 2D", "Empty Mixed" };
    ImGui::Combo("Template", &m_NewProjDlgTemplate, templateNames, 3);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    bool canCreate = (std::strlen(m_NewProjDlgName) > 0 &&
                      std::strlen(m_NewProjDlgLocation) > 0 &&
                      std::strlen(m_NewProjDlgScene) > 0);
    if (!canCreate) ImGui::BeginDisabled();
    if (ImGui::Button("Create Project", ImVec2(140 * s, 30 * s))) {
        namespace fs = std::filesystem;
        std::string projName(m_NewProjDlgName);
        std::string location(m_NewProjDlgLocation);
        std::string sceneName(m_NewProjDlgScene);
        fs::path projRoot = fs::path(location) / projName;

        // Create directory structure
        std::error_code ec;
        fs::create_directories(projRoot / "scenes", ec);
        fs::create_directories(projRoot / "assets", ec);
        fs::create_directories(projRoot / "scripts", ec);

        if (!ec) {
            // Determine project mode from template
            Scene::ProjectMode mode = Scene::ProjectMode::Mode3D;
            if (m_NewProjDlgTemplate == 1) mode = Scene::ProjectMode::Mode2D;
            else if (m_NewProjDlgTemplate == 2) mode = Scene::ProjectMode::Mixed;

            // Initialize project via SceneManager
            m_SceneManager.NewProject(projName);
            std::string relativeScenePath = "scenes/" + sceneName + ".enjin";
            m_SceneManager.AddScene(sceneName, relativeScenePath);
            m_SceneManager.SetStartScene(0);
            m_SceneManager.SetProjectMode(mode);

            // Save manifest
            fs::path manifestPath = projRoot / (projName + ".enjinproject");
            if (m_SceneManager.SaveProject(manifestPath.string())) {
                // Clear current world and save empty scene
                if (m_World) {
                    m_World->Clear();
                    if (m_RenderSystem) m_RenderSystem->OnSceneClear();
                    ClearSelection();
                }
                fs::path sceneFilePath = projRoot / relativeScenePath;
                SaveScene(sceneFilePath.string());
                m_CurrentScenePath = sceneFilePath.string();
                ClearDirty();
                UpdateWindowTitle();

                // Track project and persist settings
                m_EditorSettings.AddRecentProject(manifestPath.string());
                m_EditorSettings.lastProjectDir = location;
                m_EditorSettings.Save();

                ShowNotification("Created project '" + projName + "'",
                                NotificationType::Success);
                ENJIN_LOG_INFO(Editor, "Created project '%s' at %s",
                               projName.c_str(), projRoot.string().c_str());
                m_ShowNewProjectDialog = false;
            } else {
                ShowNotification("Failed to save project manifest",
                                NotificationType::Error);
            }
        } else {
            ShowNotification("Failed to create project directories",
                            NotificationType::Error);
        }
    }
    if (!canCreate) ImGui::EndDisabled();

    ImGui::End();
}

// ---------------------------------------------------------------------------
// Terrain Brush Implementation
// ---------------------------------------------------------------------------

bool EditorLayer::RaycastTerrain(const Ray& ray, ECS::TerrainComponent* terrain,
                                  const ECS::TransformComponent* transform,
                                  Math::Vector3& hitPoint) {
    if (!terrain || terrain->heightmap.empty()) return false;

    Math::Vector3 origin = transform ? transform->position : Math::Vector3(0.0f);
    f32 terrainWidth = terrain->gridWidth * terrain->cellSize;
    f32 terrainDepth = terrain->gridHeight * terrain->cellSize;

    // Quick reject: if ray is parallel to XZ plane and above max height, no hit
    if (std::abs(ray.direction.y) < 1e-6f) return false;

    // Find approximate t where ray reaches terrain base plane (Y = origin.y)
    f32 tBase = (origin.y - ray.origin.y) / ray.direction.y;
    // Also check top plane
    f32 tTop = (origin.y + terrain->maxHeight - ray.origin.y) / ray.direction.y;

    f32 tMin = std::min(tBase, tTop);
    f32 tMax = std::max(tBase, tTop);
    if (tMin < 0.0f) tMin = 0.0f;
    if (tMax < 0.0f) return false;

    // March along the ray in small steps
    f32 stepSize = terrain->cellSize * 0.5f;
    f32 t = tMin;

    for (int i = 0; i < 500 && t <= tMax + stepSize; ++i, t += stepSize) {
        Math::Vector3 p = ray.origin + ray.direction * t;

        // Convert to grid-local coordinates
        f32 localX = p.x - origin.x;
        f32 localZ = p.z - origin.z;

        // Bounds check
        if (localX < 0.0f || localZ < 0.0f || localX >= terrainWidth || localZ >= terrainDepth)
            continue;

        // Bilinear interpolation of height
        f32 gx = localX / terrain->cellSize;
        f32 gz = localZ / terrain->cellSize;
        u32 ix = static_cast<u32>(gx);
        u32 iz = static_cast<u32>(gz);
        if (ix >= terrain->gridWidth - 1) ix = terrain->gridWidth - 2;
        if (iz >= terrain->gridHeight - 1) iz = terrain->gridHeight - 2;
        f32 fx = gx - static_cast<f32>(ix);
        f32 fz = gz - static_cast<f32>(iz);

        f32 h00 = terrain->GetHeight(ix, iz);
        f32 h10 = terrain->GetHeight(ix + 1, iz);
        f32 h01 = terrain->GetHeight(ix, iz + 1);
        f32 h11 = terrain->GetHeight(ix + 1, iz + 1);
        f32 terrainH = h00 * (1 - fx) * (1 - fz) + h10 * fx * (1 - fz)
                      + h01 * (1 - fx) * fz + h11 * fx * fz;

        f32 worldTerrainY = origin.y + terrainH;

        if (p.y <= worldTerrainY) {
            // Binary search refinement
            f32 lo = t - stepSize;
            f32 hi = t;
            for (int r = 0; r < 8; ++r) {
                f32 mid = (lo + hi) * 0.5f;
                Math::Vector3 mp = ray.origin + ray.direction * mid;
                f32 mlx = mp.x - origin.x;
                f32 mlz = mp.z - origin.z;
                f32 mgx = mlx / terrain->cellSize;
                f32 mgz = mlz / terrain->cellSize;
                u32 mix = static_cast<u32>(std::max(0.0f, std::min(mgx, static_cast<f32>(terrain->gridWidth - 2))));
                u32 miz = static_cast<u32>(std::max(0.0f, std::min(mgz, static_cast<f32>(terrain->gridHeight - 2))));
                f32 mfx = mgx - static_cast<f32>(mix);
                f32 mfz = mgz - static_cast<f32>(miz);
                mfx = std::max(0.0f, std::min(1.0f, mfx));
                mfz = std::max(0.0f, std::min(1.0f, mfz));

                f32 mh = terrain->GetHeight(mix, miz) * (1 - mfx) * (1 - mfz)
                        + terrain->GetHeight(mix + 1, miz) * mfx * (1 - mfz)
                        + terrain->GetHeight(mix, miz + 1) * (1 - mfx) * mfz
                        + terrain->GetHeight(mix + 1, miz + 1) * mfx * mfz;

                if (mp.y <= origin.y + mh) hi = mid;
                else lo = mid;
            }
            hitPoint = ray.origin + ray.direction * ((lo + hi) * 0.5f);
            return true;
        }
    }

    return false;
}


void EditorLayer::DrawTextureCompressionWindow() {
    if (!m_ShowCompressionSettings) return;

    ImGui::SetNextWindowSize(ImVec2(380 * m_EditorSettings.uiScale, 340 * m_EditorSettings.uiScale), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Compress Texture", &m_ShowCompressionSettings)) {
        // Show target file
        namespace fs = std::filesystem;
        fs::path targetPath(m_CompressionTargetPath);
        ImGui::Text("File: %s", targetPath.filename().string().c_str());
        ImGui::TextDisabled("%s", m_CompressionTargetPath.c_str());
        ImGui::Separator();

        // Format combo
        const char* formatNames[] = {
            "None", "BC1 (DXT1)", "BC3 (DXT5)", "BC4 (R)", "BC5 (RG)",
            "BC7 (RGBA)", "ASTC 4x4", "ASTC 6x6", "ASTC 8x8"
        };
        int formatIdx = static_cast<int>(m_TextureCompSettings.format);
        if (ImGui::Combo("Format", &formatIdx, formatNames, 9)) {
            m_TextureCompSettings.format = static_cast<Assets::CompressedFormat>(formatIdx);
        }

        // Quality combo
        const char* qualityNames[] = { "Fast", "Normal", "High" };
        int qualityIdx = static_cast<int>(m_TextureCompSettings.quality);
        if (ImGui::Combo("Quality", &qualityIdx, qualityNames, 3)) {
            m_TextureCompSettings.quality = static_cast<Assets::CompressionQuality>(qualityIdx);
        }

        ImGui::Checkbox("Generate Mipmaps", &m_TextureCompSettings.generateMipmaps);
        ImGui::Checkbox("sRGB", &m_TextureCompSettings.sRGB);

        // Compression ratio preview
        if (m_TextureCompSettings.format != Assets::CompressedFormat::None) {
            f32 ratio = Assets::TextureCompressor::CompressionRatio(m_TextureCompSettings.format);
            u32 bpp = Assets::TextureCompressor::BitsPerPixel(m_TextureCompSettings.format);
            ImGui::Separator();
            ImGui::Text("Compression Ratio: %.1fx", ratio);
            ImGui::Text("Bits Per Pixel: %u (from 32)", bpp);
        }

        ImGui::Separator();

        // Compress button
        if (m_TextureCompSettings.format != Assets::CompressedFormat::None) {
            if (ImGui::Button("Compress", ImVec2(120, 0))) {
                // Load texture data
                int w = 0, h = 0, ch = 0;
                stbi_uc* pixels = stbi_load(m_CompressionTargetPath.c_str(), &w, &h, &ch, STBI_rgb_alpha);
                if (pixels && w > 0 && h > 0) {
                    auto result = Assets::TextureCompressor::Compress(
                        pixels, static_cast<u32>(w), static_cast<u32>(h), m_TextureCompSettings);
                    stbi_image_free(pixels);

                    if (result.valid) {
                        usize totalCompressed = 0;
                        for (const auto& mip : result.mipLevels) totalCompressed += mip.data.size();
                        usize totalUncompressed = static_cast<usize>(w) * h * 4;
                        m_CompressionLastResult = "Compressed: " + std::to_string(totalUncompressed) +
                            " -> " + std::to_string(totalCompressed) + " bytes (" +
                            std::to_string(result.mipLevels.size()) + " mip levels)";
                    } else {
                        m_CompressionLastResult = "Compression failed";
                    }
                } else {
                    if (pixels) stbi_image_free(pixels);
                    m_CompressionLastResult = "Failed to load texture file";
                }
            }
        }

        ImGui::SameLine();
        if (ImGui::Button("Close", ImVec2(80, 0))) {
            m_ShowCompressionSettings = false;
        }

        // Result message
        if (!m_CompressionLastResult.empty()) {
            ImGui::Spacing();
            ImGui::TextWrapped("%s", m_CompressionLastResult.c_str());
        }
    }
    ImGui::End();
}

// --- Tilemap viewport brush tool (follows terrain brush pattern) ---


void EditorLayer::DrawHTML5ExportDialog() {
    ImGui::SetNextWindowSize(ImVec2(500 * m_EditorSettings.uiScale, 480 * m_EditorSettings.uiScale), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Export HTML5", &m_ShowHTML5ExportDialog)) {
        ImGui::End();
        return;
    }

    static char titleBuf[128] = {};
    static char outputBuf[512] = {};
    static char bgColorBuf[16] = "#000000";
    static char cssBuf[1024] = {};
    static bool initialized = false;
    static std::string lastEmbedCode;

    if (!initialized || ImGui::IsWindowAppearing()) {
        strncpy(titleBuf, m_HTML5Config.title.c_str(), sizeof(titleBuf) - 1);
        titleBuf[sizeof(titleBuf) - 1] = '\0';
        strncpy(outputBuf, m_HTML5Config.outputDir.c_str(), sizeof(outputBuf) - 1);
        outputBuf[sizeof(outputBuf) - 1] = '\0';
        strncpy(bgColorBuf, m_HTML5Config.backgroundColor.c_str(), sizeof(bgColorBuf) - 1);
        bgColorBuf[sizeof(bgColorBuf) - 1] = '\0';
        initialized = true;
    }

    ImGui::Text("HTML5 Web Export");
    ImGui::Separator();

    // Title
    ImGui::SetNextItemWidth(-1);
    if (ImGui::InputText("Title", titleBuf, sizeof(titleBuf))) {
        m_HTML5Config.title = titleBuf;
    }

    // Dimensions
    i32 w = (i32)m_HTML5Config.width, h = (i32)m_HTML5Config.height;
    ImGui::SetNextItemWidth(120);
    if (ImGui::InputInt("Width", &w)) m_HTML5Config.width = std::max(1, w);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(120);
    if (ImGui::InputInt("Height", &h)) m_HTML5Config.height = std::max(1, h);

    // Presets
    ImGui::SameLine();
    if (ImGui::Button("550x400")) { m_HTML5Config.width = 550; m_HTML5Config.height = 400; }
    ImGui::SameLine();
    if (ImGui::Button("800x600")) { m_HTML5Config.width = 800; m_HTML5Config.height = 600; }
    ImGui::SameLine();
    if (ImGui::Button("1280x720")) { m_HTML5Config.width = 1280; m_HTML5Config.height = 720; }

    // Background color
    if (ImGui::InputText("Background", bgColorBuf, sizeof(bgColorBuf))) {
        m_HTML5Config.backgroundColor = bgColorBuf;
    }

    // Options
    ImGui::Checkbox("Show Preloader", &m_HTML5Config.showPreloader);
    ImGui::SameLine();
    ImGui::Checkbox("Fullscreen Button", &m_HTML5Config.showFullscreenButton);
    ImGui::SameLine();
    ImGui::Checkbox("Generate Embed Code", &m_HTML5Config.generateEmbedCode);

    // Splash image
    ImGui::Text("Splash Image: %s", m_HTML5Config.splashImagePath.empty() ? "(none)" : m_HTML5Config.splashImagePath.c_str());
    ImGui::SameLine();
    if (ImGui::Button("Browse##Splash")) {
        std::vector<FileFilter> filters = {
            { "Images", "*.png;*.jpg;*.jpeg;*.svg" },
            { "All Files", "*.*" }
        };
        std::string path = FileDialog::OpenFile("Select Splash Image", filters);
        if (!path.empty()) m_HTML5Config.splashImagePath = path;
    }

    // Output directory
    ImGui::Separator();
    if (ImGui::InputText("Output Dir", outputBuf, sizeof(outputBuf))) {
        m_HTML5Config.outputDir = outputBuf;
    }
    ImGui::SameLine();
    if (ImGui::Button("Browse##Output")) {
        std::string path = FileDialog::OpenFolder("Select Output Folder");
        if (!path.empty()) {
            m_HTML5Config.outputDir = path;
            strncpy(outputBuf, path.c_str(), sizeof(outputBuf) - 1);
            outputBuf[sizeof(outputBuf) - 1] = '\0';
        }
    }

    // Custom CSS
    if (ImGui::TreeNode("Custom CSS")) {
        if (ImGui::InputTextMultiline("##CustomCSS", cssBuf, sizeof(cssBuf), ImVec2(-1, 80))) {
            m_HTML5Config.customCSS = cssBuf;
        }
        ImGui::TreePop();
    }

    // Export button
    ImGui::Separator();
    if (ImGui::Button("Export", ImVec2(120, 30))) {
        auto result = Build::HTML5Exporter::Export(m_HTML5Config, m_BuildConfig);
        if (result.success) {
            ENJIN_LOG_INFO(Editor, "HTML5 export complete: %zu files", result.files.size());
            lastEmbedCode = result.embedCode;
        } else {
            ENJIN_LOG_ERROR(Editor, "HTML5 export failed: %s", result.error.c_str());
        }
    }

    // Show embed code if available
    if (!lastEmbedCode.empty()) {
        ImGui::Separator();
        ImGui::Text("Embed Code:");
        ImGui::InputTextMultiline("##EmbedCode",
                                   const_cast<char*>(lastEmbedCode.c_str()),
                                   lastEmbedCode.size() + 1,
                                   ImVec2(-1, 80),
                                   ImGuiInputTextFlags_ReadOnly);
        if (ImGui::Button("Copy Embed Code")) {
            ImGui::SetClipboardText(lastEmbedCode.c_str());
        }
    }

    ImGui::End();
}

// ═══════════════════════════════════════════════════════════════════════
// Feedback / Bug Reporting System
// ═══════════════════════════════════════════════════════════════════════

void EditorLayer::ResetBugReportForm() {
    m_BugTitleBuf[0] = '\0';
    m_BugDescriptionBuf[0] = '\0';
    m_BugStepsBuf[0] = '\0';
    m_BugExpectedBuf[0] = '\0';
    m_BugActualBuf[0] = '\0';
    m_BugTypeSel = 0;
    m_BugSeveritySel = 1;
    m_BugIncludeScene = false;
    m_BugIncludeLogs = true;
}

void EditorLayer::ResetFeedbackForm() {
    m_FeedbackTitleBuf[0] = '\0';
    m_FeedbackDescBuf[0] = '\0';
    m_FeedbackTypeSel = 0;
    m_FeedbackPrioritySel = 1;
    m_FeedbackSatisfaction = 0;
    m_FeedbackIncludeDiag = false;
    m_FeedbackCategoryBuf[0] = '\0';
}

DiagnosticSnapshot EditorLayer::CaptureDiagnostics(bool includeScene) {
    f32 fps = m_FrameTimeAvg > 0.0f ? 1000.0f / m_FrameTimeAvg : 0.0f;
    u32 entityCount = m_World ? static_cast<u32>(m_World->GetEntityCount()) : 0;
    std::string sceneJson;
    // Scene snapshot is intentionally omitted for now — serializing mid-frame
    // is unsafe and the diagnostics already capture the scene path.
    (void)includeScene;

    // Extract plain message strings for the diagnostic snapshot
    std::vector<std::string> logStrings;
    logStrings.reserve(m_ConsoleLog.size());
    for (const auto& entry : m_ConsoleLog) {
        logStrings.push_back(entry.message);
    }

    return DiagnosticSnapshot::Capture(
        m_PerfMetrics,
        fps,
        m_FrameTimeAvg,
        entityCount,
        m_CurrentScenePath,
        logStrings,
        static_cast<u32>(m_SelectedEntities.size()),
        sceneJson);
}

void EditorLayer::DrawPlayModeDiffDialog() {
    auto& diff = m_PlayMode.GetDiff();
    if (!diff.HasChanges()) {
        m_PlayMode.DismissDiff();
        return;
    }

    f32 s = m_EditorSettings.uiScale;
    ImGui::SetNextWindowSize(ImVec2(650 * s, 500 * s), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing,
                            ImVec2(0.5f, 0.5f));

    bool open = true;
    if (ImGui::Begin("Play Mode Changes", &open, ImGuiWindowFlags_NoCollapse)) {
        // Header summary
        ImGui::Text("%u modified, %u created, %u deleted",
                    diff.CountModified(), diff.CountCreated(), diff.CountDeleted());
        ImGui::Separator();

        // Select All / Deselect All
        if (ImGui::SmallButton("Select All")) {
            for (auto& ed : diff.entities) {
                ed.selected = true;
                for (auto& cd : ed.components) {
                    cd.selected = true;
                    for (auto& pd : cd.properties) pd.selected = true;
                }
            }
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Deselect All")) {
            for (auto& ed : diff.entities) {
                ed.selected = false;
                for (auto& cd : ed.components) {
                    cd.selected = false;
                    for (auto& pd : cd.properties) pd.selected = false;
                }
            }
        }
        ImGui::Separator();

        // Entity diff tree
        ImGui::BeginChild("DiffList", ImVec2(0, -35), true);
        for (auto& ed : diff.entities) {
            ImGui::PushID(&ed);

            // Color by action
            ImVec4 color;
            const char* actionStr;
            switch (ed.action) {
                case DiffAction::Created:  color = ImVec4(0.3f, 0.9f, 0.3f, 1); actionStr = "[+]"; break;
                case DiffAction::Deleted:  color = ImVec4(0.9f, 0.3f, 0.3f, 1); actionStr = "[-]"; break;
                case DiffAction::Modified: color = ImVec4(0.9f, 0.8f, 0.3f, 1); actionStr = "[~]"; break;
            }

            ImGui::Checkbox("##sel", &ed.selected);
            ImGui::SameLine();
            ImGui::TextColored(color, "%s", actionStr);
            ImGui::SameLine();

            bool nodeOpen = ImGui::TreeNode("##entity", "%s", ed.entityName.c_str());

            // Prefab badge
            if (ed.isPrefabInstance) {
                ImGui::SameLine();
                ImGui::TextDisabled("[Prefab: %s]", ed.prefabPath.c_str());
            }

            if (nodeOpen) {
                // Component diffs
                for (auto& cd : ed.components) {
                    ImGui::PushID(&cd);
                    ImGui::Checkbox("##csel", &cd.selected);
                    ImGui::SameLine();

                    const char* cActionStr = cd.action == DiffAction::Created ? "[+]" :
                                             cd.action == DiffAction::Deleted ? "[-]" : "[~]";
                    ImGui::TextColored(color, "%s", cActionStr);
                    ImGui::SameLine();

                    bool compOpen = ImGui::TreeNode("##comp", "%s", cd.componentType.c_str());
                    if (compOpen) {
                        // Property diffs
                        for (auto& pd : cd.properties) {
                            ImGui::PushID(&pd);
                            ImGui::Checkbox("##psel", &pd.selected);
                            ImGui::SameLine();
                            ImGui::Text("%s:", pd.name.c_str());
                            ImGui::SameLine();
                            if (!pd.oldValue.empty()) {
                                ImGui::TextColored(ImVec4(0.8f, 0.4f, 0.4f, 1), "%s",
                                                   pd.oldValue.c_str());
                            }
                            if (!pd.oldValue.empty() && !pd.newValue.empty()) {
                                ImGui::SameLine();
                                ImGui::TextDisabled("->");
                                ImGui::SameLine();
                            }
                            if (!pd.newValue.empty()) {
                                ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1), "%s",
                                                   pd.newValue.c_str());
                            }
                            ImGui::PopID();
                        }
                        ImGui::TreePop();
                    }
                    ImGui::PopID();
                }

                // Prefab options
                if (ed.isPrefabInstance && ed.action == DiffAction::Modified) {
                    ImGui::Separator();
                    if (ImGui::SmallButton("Apply to Prefab")) {
                        auto& pm = Assets::PrefabManager::Get();
                        auto prefab = pm.CreateFromEntity(m_World, ed.entity, ed.entityName);
                        if (prefab && pm.SavePrefab(*prefab, ed.prefabPath)) {
                            ENJIN_LOG_INFO(Editor, "Applied changes to prefab: %s", ed.prefabPath.c_str());
                        } else {
                            ENJIN_LOG_ERROR(Editor, "Failed to apply prefab: %s", ed.prefabPath.c_str());
                        }
                    }
                    ImGui::SameLine();
                    if (ImGui::SmallButton("Unpack as New Object")) {
                        ed.isPrefabInstance = false;
                        ed.prefabPath.clear();
                        ENJIN_LOG_INFO(Editor, "Unpacked prefab instance '%s'",
                                       ed.entityName.c_str());
                    }
                }

                ImGui::TreePop();
            }

            ImGui::PopID();
        }
        ImGui::EndChild();

        // Action buttons
        if (ImGui::Button("Apply Selected", ImVec2(120, 0))) {
            ApplySelectedDiffs(m_World, diff, m_PlayMode.GetPlayedSceneJson());
            m_PlayMode.DismissDiff();
        }
        ImGui::SameLine();
        if (ImGui::Button("Discard All", ImVec2(100, 0))) {
            m_PlayMode.DismissDiff();
        }
    }

    if (!open) {
        m_PlayMode.DismissDiff();
    }
    ImGui::End();
}


// ============================================================================
// Console Log Callback (wires Logger output to editor console panel)
// ============================================================================

void EditorLayer::PushConsoleMessage(const std::string& message) {
    // Infer level from prefix tags in manually pushed messages
    LogLevel level = LogLevel::Info;
    if (message.find("[Error]") != std::string::npos || message.find("[FATAL]") != std::string::npos) {
        level = LogLevel::Error;
    } else if (message.find("[Warn]") != std::string::npos) {
        level = LogLevel::Warn;
    }
    m_ConsoleLog.push_back({ message, level, LogCategory::Editor });
    if (m_ConsoleLog.size() > MAX_CONSOLE_LINES) {
        m_ConsoleLog.erase(m_ConsoleLog.begin(),
            m_ConsoleLog.begin() + static_cast<ptrdiff_t>(m_ConsoleLog.size() - MAX_CONSOLE_LINES));
    }
}

void EditorLayer::PushConsoleMessage(LogLevel level, LogCategory category, const std::string& message) {
    m_ConsoleLog.push_back({ message, level, category });
    if (m_ConsoleLog.size() > MAX_CONSOLE_LINES) {
        m_ConsoleLog.erase(m_ConsoleLog.begin(),
            m_ConsoleLog.begin() + static_cast<ptrdiff_t>(m_ConsoleLog.size() - MAX_CONSOLE_LINES));
    }
}

void EditorLayer::CheckForCrashReport() {
    if (Debug::HasPreviousCrashReport()) {
        m_PreviousCrashReport = Debug::ReadPreviousCrashReport();
        // Log quietly instead of popping up a dialog on startup.
        // Report is still available via Help > Last Crash Report.
        if (!m_PreviousCrashReport.empty()) {
            ENJIN_LOG_WARN(Editor, "Previous session crashed. View report via Help > Last Crash Report.");
        }
        Debug::ClearPreviousCrashReport();
    }
}

void EditorLayer::DrawCrashReportDialog() {
    ImGui::SetNextWindowSize(ImVec2(620 * m_EditorSettings.uiScale, 480 * m_EditorSettings.uiScale), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Crash Report — Previous Session", &m_ShowCrashDialog)) {
        ImGui::End();
        return;
    }

    ImGui::TextWrapped("The engine crashed during the previous session. "
                       "The crash report below may help diagnose the issue.");
    ImGui::Spacing();

    // Scrollable text area with monospace font
    ImGui::BeginChild("CrashReportText", ImVec2(0, -ImGui::GetFrameHeightWithSpacing() - 4), true);
    ImGui::TextUnformatted(m_PreviousCrashReport.c_str());
    ImGui::EndChild();

    // Action buttons
    if (ImGui::Button("Copy to Clipboard")) {
        ImGui::SetClipboardText(m_PreviousCrashReport.c_str());
    }
    ImGui::SameLine();

    // Submit crash report to GitHub Issues
    if (m_FeedbackManager.IsGitHubConfigured()) {
        if (ImGui::Button("Submit to GitHub")) {
            if (m_FeedbackManager.SubmitCrashReportToGitHub(m_PreviousCrashReport)) {
                ENJIN_LOG_INFO(Editor, "Crash report submitted to GitHub Issues");
                Debug::ClearPreviousCrashReport();
                m_ShowCrashDialog = false;
                m_PreviousCrashReport.clear();
            } else {
                ENJIN_LOG_ERROR(Editor, "Failed to submit crash report to GitHub");
            }
        }
        ImGui::SameLine();
    } else {
        ImGui::BeginDisabled();
        ImGui::Button("Submit to GitHub");
        ImGui::EndDisabled();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
            ImGui::SetTooltip("Configure GitHub token in Help > Bug Reports & Feedback > Settings");
        }
        ImGui::SameLine();
    }

    if (ImGui::Button("Dismiss")) {
        Debug::ClearPreviousCrashReport();
        m_ShowCrashDialog = false;
        m_PreviousCrashReport.clear();
    }

    ImGui::End();
}

void EditorLayer::DrawUnsavedChangesDialog() {
    ImGui::OpenPopup("Unsaved Changes");
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("Unsaved Changes", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("You have unsaved changes.");
        if (!m_CurrentScenePath.empty()) {
            ImGui::TextDisabled("%s", std::filesystem::path(m_CurrentScenePath).filename().string().c_str());
        }
        ImGui::Spacing();
        ImGui::Text("Do you want to save before continuing?");
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::Button("Save", ImVec2(100, 0))) {
            // Save, then proceed with the pending action
            if (!m_CurrentScenePath.empty()) {
                SaveScene(m_CurrentScenePath);
            } else {
                // No path — open Save As dialog
                std::vector<FileFilter> filters = {
                    { "Enjin Scene", "*.enjin" },
                    { "All Files", "*.*" }
                };
                auto projRoot = std::filesystem::path(m_SceneManager.GetProjectPath()).parent_path().string();
                std::string path = FileDialog::SaveFile("Save Scene", filters, projRoot, "scene.enjin");
                if (!path.empty()) {
                    SaveScene(path);
                } else {
                    // User cancelled save — stay in the dialog
                    ImGui::EndPopup();
                    return;
                }
            }

            // Proceed with the action
            UnsavedAction action = m_UnsavedChangesAction;
            m_ShowUnsavedChangesDialog = false;
            m_UnsavedChangesAction = UnsavedAction::None;
            ImGui::CloseCurrentPopup();

            switch (action) {
                case UnsavedAction::Quit:
                    m_ShowQuitFeedbackDialog = true;
                    break;
                case UnsavedAction::NewScene:
                    if (m_World) {
                        m_World->Clear();
                        if (m_RenderSystem) m_RenderSystem->OnSceneClear();
                        ClearSelection();
                        m_CurrentScenePath.clear();
                        ClearDirty();
                        UpdateWindowTitle();
                        m_HubPage = HubPage::WizardSetup;
                        m_SelectedTemplate = -1;
                        m_TemplateFilter = TMPL_ALL;
                        m_TemplateSearchBuffer[0] = '\0';
                        m_ShowProjectHub = true;
                        m_SceneManager.GetDefaultRenderSettings().ApplyToRuntime(
                            m_RenderSystem, m_PostProcessing ? &m_PostProcessing->GetSettings() : nullptr);
                        m_CurrentSceneUsesProjectDefaults = true;
                    }
                    break;
                case UnsavedAction::OpenScene:
                    if (!m_PendingOpenPath.empty()) {
                        OpenScene(m_PendingOpenPath);
                        m_PendingOpenPath.clear();
                    }
                    break;
                default: break;
            }
        }

        ImGui::SameLine();
        if (ImGui::Button("Don't Save", ImVec2(100, 0))) {
            // Discard changes and proceed
            UnsavedAction action = m_UnsavedChangesAction;
            m_ShowUnsavedChangesDialog = false;
            m_UnsavedChangesAction = UnsavedAction::None;
            ClearDirty();
            ImGui::CloseCurrentPopup();

            switch (action) {
                case UnsavedAction::Quit:
                    m_ShowQuitFeedbackDialog = true;
                    break;
                case UnsavedAction::NewScene:
                    if (m_World) {
                        m_World->Clear();
                        if (m_RenderSystem) m_RenderSystem->OnSceneClear();
                        ClearSelection();
                        m_CurrentScenePath.clear();
                        UpdateWindowTitle();
                        m_HubPage = HubPage::WizardSetup;
                        m_SelectedTemplate = -1;
                        m_TemplateFilter = TMPL_ALL;
                        m_TemplateSearchBuffer[0] = '\0';
                        m_ShowProjectHub = true;
                        m_SceneManager.GetDefaultRenderSettings().ApplyToRuntime(
                            m_RenderSystem, m_PostProcessing ? &m_PostProcessing->GetSettings() : nullptr);
                        m_CurrentSceneUsesProjectDefaults = true;
                    }
                    break;
                case UnsavedAction::OpenScene:
                    if (!m_PendingOpenPath.empty()) {
                        OpenScene(m_PendingOpenPath);
                        m_PendingOpenPath.clear();
                    }
                    break;
                default: break;
            }
        }

        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(100, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            m_ShowUnsavedChangesDialog = false;
            m_UnsavedChangesAction = UnsavedAction::None;
            m_PendingOpenPath.clear();
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    } else {
        // Popup was closed externally
        m_ShowUnsavedChangesDialog = false;
        m_UnsavedChangesAction = UnsavedAction::None;
        m_PendingOpenPath.clear();
    }
}

void EditorLayer::DrawAutoSaveRecoveryDialog() {
    ImGui::OpenPopup("Auto-Save Recovery");
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("Auto-Save Recovery", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("An auto-save file was found that is newer than the scene file.");
        ImGui::TextDisabled("%s", m_AutoSaveRecoveryPath.c_str());
        ImGui::Spacing();
        ImGui::Text("Would you like to recover the auto-saved version?");
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::Button("Recover", ImVec2(120, 0))) {
            // Load the auto-save file instead
            std::string recoveryPath = m_AutoSaveRecoveryPath;
            m_ShowAutoSaveRecoveryDialog = false;
            m_AutoSaveRecoveryPath.clear();
            ImGui::CloseCurrentPopup();

            if (m_World && !recoveryPath.empty()) {
                Scene::SceneSerializer serializer(m_World);
                auto result = serializer.Load(recoveryPath, true);
                if (result.success) {
                    // Apply loaded skybox config
                    if (m_RenderSystem) {
                        m_RenderSystem->SetSkybox(serializer.GetSkyboxConfig());
                    }
                    const auto& loaded = serializer.GetRenderSettings();
                    m_CurrentSceneUsesProjectDefaults = loaded.useProjectDefaults;
                    if (loaded.useProjectDefaults) {
                        m_SceneManager.GetDefaultRenderSettings().ApplyToRuntime(
                            m_RenderSystem, m_PostProcessing ? &m_PostProcessing->GetSettings() : nullptr);
                    } else {
                        loaded.ApplyToRuntime(
                            m_RenderSystem, m_PostProcessing ? &m_PostProcessing->GetSettings() : nullptr);
                    }
                    MarkDirty(); // Recovered scene has unsaved changes
                    ENJIN_LOG_INFO(Editor, "Recovered from auto-save: %s", recoveryPath.c_str());
                    ShowNotification("Recovered auto-saved scene", NotificationType::Success);
                } else {
                    ENJIN_LOG_ERROR(Editor, "Failed to load auto-save: %s", result.error.c_str());
                    ShowNotification("Failed to recover auto-save", NotificationType::Error);
                }
            }
        }

        ImGui::SameLine();
        if (ImGui::Button("Discard", ImVec2(120, 0))) {
            // Delete the auto-save and keep the normal scene
            std::error_code ec;
            std::filesystem::remove(m_AutoSaveRecoveryPath, ec);
            m_ShowAutoSaveRecoveryDialog = false;
            m_AutoSaveRecoveryPath.clear();
            ImGui::CloseCurrentPopup();
            ENJIN_LOG_INFO(Editor, "Discarded auto-save recovery");
        }

        ImGui::EndPopup();
    } else {
        m_ShowAutoSaveRecoveryDialog = false;
        m_AutoSaveRecoveryPath.clear();
    }
}

// ============================================================================
// DISCORD BUG REPORT DIALOG
// ============================================================================

void EditorLayer::CaptureViewportScreenshot() {
    m_DiscordScreenshotPng.clear();

    // Prefer editor viewport render target; fall back to game view
    Renderer::RenderTarget* rt = nullptr;
    if (m_EditorViewportRT && m_EditorViewportRT->IsValid()) {
        rt = m_EditorViewportRT.get();
    } else if (m_GameViewRenderTarget && m_GameViewRenderTarget->IsValid()) {
        rt = m_GameViewRenderTarget.get();
    }
    if (!rt) {
        ENJIN_LOG_WARN(Editor, "DiscordBugReport: No render target available for screenshot");
        return;
    }

    auto pixels = rt->CaptureToPixels();
    if (pixels.empty()) {
        ENJIN_LOG_WARN(Editor, "DiscordBugReport: CaptureToPixels returned empty");
        return;
    }

    // Encode to PNG in memory using stb_image_write
    auto pngWriteFunc = [](void* context, void* data, int size) {
        auto* vec = static_cast<std::vector<u8>*>(context);
        auto* bytes = static_cast<u8*>(data);
        vec->insert(vec->end(), bytes, bytes + size);
    };

    stbi_write_png_to_func(pngWriteFunc, &m_DiscordScreenshotPng,
                           static_cast<int>(rt->GetWidth()),
                           static_cast<int>(rt->GetHeight()),
                           4, pixels.data(),
                           static_cast<int>(rt->GetWidth() * 4));

    if (m_DiscordScreenshotPng.empty()) {
        ENJIN_LOG_WARN(Editor, "DiscordBugReport: PNG encoding failed");
    } else {
        ENJIN_LOG_INFO(Editor, "DiscordBugReport: Captured screenshot %ux%u (%zu bytes PNG)",
                       rt->GetWidth(), rt->GetHeight(), m_DiscordScreenshotPng.size());
    }
}

void EditorLayer::SendDiscordBugReport() {
    m_DiscordSendState = DiscordSendState::Sending;
    m_DiscordSendError.clear();
    m_Telemetry.TrackBugReport();

    // Ensure feedback system is loaded
    if (!m_FeedbackLoaded) {
        m_FeedbackManager.LoadAll();
        m_FeedbackLoaded = true;
    }

    // Create a bug report record
    u64 id = m_FeedbackManager.CreateBugReport();
    auto* report = m_FeedbackManager.GetBugReport(id);
    if (!report) {
        m_DiscordSendState = DiscordSendState::Failed;
        m_DiscordSendError = "Failed to create report record";
        return;
    }

    report->title = m_DiscordBugTitleBuf;
    report->type = ReportType::Bug;
    // Map dropdown index to severity enum
    static const ReportSeverity severityMap[] = {
        ReportSeverity::Critical, ReportSeverity::High,
        ReportSeverity::Medium, ReportSeverity::Low
    };
    report->severity = severityMap[std::clamp(m_DiscordBugSeverity, 0, 3)];
    report->description = m_DiscordBugDescBuf;
    report->diagnostics = CaptureDiagnostics(false);
    if (!m_DiscordBugIncludeLog) {
        report->diagnostics.consoleLogTail.clear();
    }

    // Try to fill GPU name from Vulkan physical device properties
    if (m_Renderer && m_Renderer->GetContext()) {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(m_Renderer->GetContext()->GetPhysicalDevice(), &props);
        report->diagnostics.gpuName = props.deviceName;
    }

    const std::string webhookUrl = ENJIN_DISCORD_BUG_WEBHOOK;

    // If no webhook URL configured, save as local file instead
    if (webhookUrl.empty()) {
        // Save locally as fallback
        std::string dir = FeedbackManager::GetDefaultDirectory();
        std::filesystem::create_directories(dir);

        std::string baseName = "bug_report_" + std::to_string(id);
        std::string txtPath = dir + "/" + baseName + ".txt";

        std::ofstream txtFile(txtPath);
        if (txtFile.is_open()) {
            txtFile << "Bug Report #" << id << "\n";
            txtFile << "Title: " << report->title << "\n";
            txtFile << "Description: " << report->description << "\n\n";
            txtFile << "GPU: " << report->diagnostics.gpuName << "\n";
            txtFile << "Platform: " << report->diagnostics.platform << "\n";
            txtFile << "Engine: " << report->diagnostics.engineVersion << "\n";
            char fpsLine[128];
            snprintf(fpsLine, sizeof(fpsLine), "FPS: %.1f | Frame: %.2fms",
                     report->diagnostics.fps, report->diagnostics.frameTimeMs);
            txtFile << fpsLine << "\n";
            txtFile << "Entities: " << report->diagnostics.entityCount << "\n";
            if (!report->diagnostics.scenePath.empty())
                txtFile << "Scene: " << report->diagnostics.scenePath << "\n";

            if (!report->diagnostics.consoleLogTail.empty()) {
                txtFile << "\nConsole Log:\n";
                for (auto& line : report->diagnostics.consoleLogTail)
                    txtFile << line << "\n";
            }
            txtFile.close();
        }

        // Save screenshot PNG locally
        if (m_DiscordBugIncludeScreenshot && !m_DiscordScreenshotPng.empty()) {
            std::string pngPath = dir + "/" + baseName + ".png";
            std::ofstream pngFile(pngPath, std::ios::binary);
            if (pngFile.is_open()) {
                pngFile.write(reinterpret_cast<const char*>(m_DiscordScreenshotPng.data()),
                              static_cast<std::streamsize>(m_DiscordScreenshotPng.size()));
                pngFile.close();
            }
        }

        m_FeedbackManager.SaveAll();
        m_DiscordSendState = DiscordSendState::Sent;
        m_ConsoleLog.push_back({
            "[Bug Report] Report #" + std::to_string(id) + " saved locally to " + dir +
            " (no Discord webhook configured)"});
        return;
    }

    // Submit to Discord
    std::vector<u8> screenshotData;
    if (m_DiscordBugIncludeScreenshot && !m_DiscordScreenshotPng.empty()) {
        screenshotData = m_DiscordScreenshotPng;
    }

    bool ok = m_FeedbackManager.SubmitBugReportToDiscord(id, webhookUrl, screenshotData);
    if (ok) {
        m_DiscordSendState = DiscordSendState::Sent;
        m_ConsoleLog.push_back({
            "[Bug Report] Report #" + std::to_string(id) + " sent to Discord"});
    } else {
        m_DiscordSendState = DiscordSendState::Failed;
        m_DiscordSendError = "Discord webhook request failed";
        m_ConsoleLog.push_back({
            "[Bug Report] Failed to send report #" + std::to_string(id) + " to Discord"});
    }

    m_FeedbackManager.SaveAll();
}

void EditorLayer::DrawDiscordBugReportDialog() {
    if (!m_ShowDiscordBugDialog) return;

    f32 s = m_EditorSettings.uiScale;
    ImGui::SetNextWindowSize(ImVec2(520 * s, 480 * s), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing,
                            ImVec2(0.5f, 0.5f));

    bool open = true;
    if (!ImGui::Begin("Report Bug", &open, ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        if (!open) {
            m_ShowDiscordBugDialog = false;
            m_DiscordSendState = DiscordSendState::Idle;
        }
        return;
    }
    if (!open) {
        m_ShowDiscordBugDialog = false;
        m_DiscordSendState = DiscordSendState::Idle;
        ImGui::End();
        return;
    }

    bool sending = (m_DiscordSendState == DiscordSendState::Sending);

    // Disable all inputs while sending
    if (sending) ImGui::BeginDisabled();

    // Title (required)
    ImGui::Text("Title *");
    ImGui::SetNextItemWidth(-1);
    ImGui::InputText("##DiscordBugTitle", m_DiscordBugTitleBuf, sizeof(m_DiscordBugTitleBuf));

    ImGui::Spacing();

    // Description (optional, multiline)
    ImGui::Text("Description:");
    ImGui::InputTextMultiline("##DiscordBugDesc", m_DiscordBugDescBuf, sizeof(m_DiscordBugDescBuf),
                               ImVec2(-1, 120 * s));

    // Severity
    ImGui::Spacing();
    static const char* severities[] = { "Crash", "Major", "Minor", "Cosmetic" };
    ImGui::SetNextItemWidth(160 * s);
    ImGui::Combo("Severity", &m_DiscordBugSeverity, severities, 4);

    ImGui::Spacing();

    // Checkboxes
    ImGui::Checkbox("Include Screenshot", &m_DiscordBugIncludeScreenshot);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Captures the current editor viewport as a PNG image");
    }
    ImGui::SameLine();
    ImGui::Checkbox("Include Log", &m_DiscordBugIncludeLog);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Includes the last 50 console log lines");
    }

    // System info preview (auto-included)
    ImGui::Spacing();
    if (ImGui::TreeNode("System Info (auto-included)")) {
        f32 fps = m_FrameTimeAvg > 0.0f ? 1000.0f / m_FrameTimeAvg : 0.0f;
        u32 entityCount = m_World ? static_cast<u32>(m_World->GetEntityCount()) : 0;

        // GPU name
        if (m_Renderer && m_Renderer->GetContext()) {
            VkPhysicalDeviceProperties props;
            vkGetPhysicalDeviceProperties(m_Renderer->GetContext()->GetPhysicalDevice(), &props);
            ImGui::Text("GPU: %s", props.deviceName);

            VkExtent2D extent = m_Renderer->GetSwapchainExtent();
            ImGui::Text("Resolution: %ux%u", extent.width, extent.height);
        }

        ImGui::Text("FPS: %.1f | Frame: %.2fms", fps, m_FrameTimeAvg);
        ImGui::Text("Draw Calls: %u | Triangles: %u",
                     m_PerfMetrics.drawCallCount, m_PerfMetrics.triangleCount);
        ImGui::Text("Entities: %u", entityCount);
        ImGui::Text("RAM: %.1f MB",
                     m_PerfMetrics.processMemoryBytes / (1024.0f * 1024.0f));
        if (!m_CurrentScenePath.empty())
            ImGui::Text("Scene: %s", m_CurrentScenePath.c_str());

        ImGui::TreePop();
    }

    // Webhook destination info
    ImGui::Spacing();
    {
        constexpr const char* bugHook = ENJIN_DISCORD_BUG_WEBHOOK;
        if (bugHook[0] == '\0') {
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f),
                "No Discord webhook compiled in. Report will be saved locally.");
        } else {
            ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1.0f),
                "Will send to Discord");
        }
    }

    if (sending) ImGui::EndDisabled();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Status messages
    if (m_DiscordSendState == DiscordSendState::Sending) {
        ImGui::TextColored(ImVec4(1.0f, 0.9f, 0.3f, 1.0f), "Sending...");
    } else if (m_DiscordSendState == DiscordSendState::Sent) {
        ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.3f, 1.0f), "Sent!");
    } else if (m_DiscordSendState == DiscordSendState::Failed) {
        ImGui::TextColored(ImVec4(0.9f, 0.3f, 0.3f, 1.0f),
            "Failed: %s", m_DiscordSendError.c_str());
    }

    // Buttons
    bool titleEmpty = (m_DiscordBugTitleBuf[0] == '\0');
    bool canSend = !titleEmpty && m_DiscordSendState != DiscordSendState::Sending;

    if (!canSend) {
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
        ImGui::Button("Send");
        ImGui::PopStyleVar();
        if (titleEmpty && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
            ImGui::SetTooltip("Title is required");
        }
    } else {
        if (ImGui::Button("Send")) {
            m_DiscordSendState = DiscordSendState::Sending;
            // Capture screenshot if requested
            if (m_DiscordBugIncludeScreenshot) {
                CaptureViewportScreenshot();
            } else {
                m_DiscordScreenshotPng.clear();
            }
            SendDiscordBugReport();
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
        m_ShowDiscordBugDialog = false;
        m_DiscordSendState = DiscordSendState::Idle;
        m_DiscordBugTitleBuf[0] = '\0';
        m_DiscordBugDescBuf[0] = '\0';
        m_DiscordScreenshotPng.clear();
    }

    // After successful send, auto-close after a brief moment
    if (m_DiscordSendState == DiscordSendState::Sent) {
        ImGui::SameLine();
        if (ImGui::Button("Close")) {
            m_ShowDiscordBugDialog = false;
            m_DiscordSendState = DiscordSendState::Idle;
            m_DiscordBugTitleBuf[0] = '\0';
            m_DiscordBugDescBuf[0] = '\0';
            m_DiscordScreenshotPng.clear();
        }
    }

    ImGui::End();
}

// ── Quit Feedback Survey Dialog ──────────────────────────────────────

void EditorLayer::DrawQuitFeedbackDialog() {
    ImGui::OpenPopup("##QuitFeedback");
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(520, 560));

    if (ImGui::BeginPopupModal("##QuitFeedback", nullptr,
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar)) {

        ImGui::TextColored(ImVec4(1, 1, 1, 1), "Before you go...");
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0, 4));

        // Helper for 1-5 rating row
        auto RatingRow = [](const char* label, u8& value) {
            ImGui::Text("%s", label);
            ImGui::SameLine(220);
            ImGui::PushID(label);
            for (u8 i = 1; i <= 5; i++) {
                ImGui::PushID(static_cast<int>(i));
                bool selected = (value == i);
                if (selected) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.6f, 1.0f, 1.0f));
                char num[4];
                snprintf(num, sizeof(num), "%d", i);
                if (ImGui::SmallButton(num)) value = i;
                if (selected) ImGui::PopStyleColor();
                ImGui::PopID();
                if (i < 5) ImGui::SameLine();
            }
            ImGui::PopID();
        };

        ImGui::Text("Rate your experience (1-5):");
        ImGui::Dummy(ImVec2(0, 2));
        RatingRow("Overall Satisfaction", m_QuitSurvey.ratingOverall);
        RatingRow("Stability", m_QuitSurvey.ratingStability);
        RatingRow("Performance", m_QuitSurvey.ratingPerformance);
        RatingRow("Ease of Use", m_QuitSurvey.ratingEaseOfUse);
        RatingRow("Feature Completeness", m_QuitSurvey.ratingFeatureCompleteness);

        ImGui::Dummy(ImVec2(0, 8));
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0, 4));

        // Text fields — static buffers required by ImGui
        static char likeBuf[512] = {};
        static char frustrateBuf[512] = {};
        static char featureBuf[512] = {};

        ImGui::Text("What did you like?");
        ImGui::InputTextMultiline("##like", likeBuf, sizeof(likeBuf), ImVec2(-1, 40));

        ImGui::Text("What frustrated you?");
        ImGui::InputTextMultiline("##frustrate", frustrateBuf, sizeof(frustrateBuf), ImVec2(-1, 40));

        ImGui::Text("What feature do you want most?");
        ImGui::InputTextMultiline("##feature", featureBuf, sizeof(featureBuf), ImVec2(-1, 40));

        ImGui::Dummy(ImVec2(0, 8));

        // Buttons
        float buttonWidth = 140.0f;
        float spacing = 20.0f;
        float totalWidth = buttonWidth * 3 + spacing * 2;
        ImGui::SetCursorPosX((520 - totalWidth) * 0.5f);

        if (ImGui::Button("Submit & Quit", ImVec2(buttonWidth, 32))) {
            m_QuitSurvey.whatDidYouLike = likeBuf;
            m_QuitSurvey.whatFrustratedYou = frustrateBuf;
            m_QuitSurvey.mostWantedFeature = featureBuf;

            // Compute session duration
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - m_SessionStartTime);
            m_QuitSurvey.sessionDurationMinutes = static_cast<f32>(elapsed.count()) / 60.0f;

            // Set timestamp and engine version
            m_QuitSurvey.timestamp = FeedbackManager::CurrentTimestamp();
            m_QuitSurvey.engineVersion = ENJIN_VERSION_STRING;

            // Fire-and-forget: submit to Discord (never blocks quit)
            {
                constexpr const char* feedbackHook = ENJIN_DISCORD_FEEDBACK_WEBHOOK;
                const std::string webhookUrl = (feedbackHook[0] != '\0')
                    ? feedbackHook : ENJIN_DISCORD_BUG_WEBHOOK;
                if (!webhookUrl.empty()) {
                    m_FeedbackManager.SubmitQuitSurveyToDiscord(m_QuitSurvey, webhookUrl);
                }
            }

            likeBuf[0] = frustrateBuf[0] = featureBuf[0] = '\0';
            ImGui::CloseCurrentPopup();
            FinalizeQuit();
        }
        ImGui::SameLine(0, spacing);
        if (ImGui::Button("Skip & Quit", ImVec2(buttonWidth, 32))) {
            likeBuf[0] = frustrateBuf[0] = featureBuf[0] = '\0';
            ImGui::CloseCurrentPopup();
            FinalizeQuit();
        }
        ImGui::SameLine(0, spacing);
        if (ImGui::Button("Cancel", ImVec2(buttonWidth, 32))) {
            likeBuf[0] = frustrateBuf[0] = featureBuf[0] = '\0';
            m_ShowQuitFeedbackDialog = false;
            m_QuitSurvey = {};
            ImGui::CloseCurrentPopup();
        }

        // ESC = cancel (return to editor)
        if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            likeBuf[0] = frustrateBuf[0] = featureBuf[0] = '\0';
            m_ShowQuitFeedbackDialog = false;
            m_QuitSurvey = {};
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void EditorLayer::FinalizeQuit() {
    m_ShowQuitFeedbackDialog = false;
    m_QuitSurvey = {};
    if (m_Window) m_Window->Close();
}

} // namespace Editor
} // namespace Enjin
