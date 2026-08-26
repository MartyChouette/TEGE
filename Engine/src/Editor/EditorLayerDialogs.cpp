#include "Enjin/Editor/EditorLayer.h"
#ifndef _WIN32
// POSIX environment for posix_spawn. Declared at GLOBAL scope: a block-scope
// extern inside namespace Enjin mangles as a namespaced symbol under GCC.
extern char** environ;
#endif
#include "Enjin/Editor/InspectorUndo.h"
#include "Enjin/Assets/AssetPipeline.h"
#include <thread>

// Webhook URLs — compiled in, not user-configurable. File is .gitignored.
#if __has_include("Enjin/Editor/WebhookConfig.h")
#include "Enjin/Editor/WebhookConfig.h"
#else
#define ENJIN_DISCORD_BUG_WEBHOOK ""
#define ENJIN_DISCORD_FEEDBACK_WEBHOOK ""
#endif
#include "Enjin/Editor/ScenePicker.h"
#include "Enjin/Core/Version.h"
#include "Enjin/GUI/EngineSplash.h"
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

// WebGPU runs best on Chromium browsers. The system default may be Firefox
// (spotty WebGPU on Windows), so web previews prefer Chrome, then Edge, then
// fall back to whatever the OS default is.
static void OpenUrlPreferChromium(const std::string& url) {
#ifdef _WIN32
    namespace fs = std::filesystem;
    std::vector<std::string> candidates;
    const char* localAppData = std::getenv("LOCALAPPDATA");
    if (localAppData) {
        candidates.push_back(std::string(localAppData) + "\\Google\\Chrome\\Application\\chrome.exe");
    }
    candidates.push_back("C:\\Program Files\\Google\\Chrome\\Application\\chrome.exe");
    candidates.push_back("C:\\Program Files (x86)\\Google\\Chrome\\Application\\chrome.exe");
    candidates.push_back("C:\\Program Files (x86)\\Microsoft\\Edge\\Application\\msedge.exe");
    candidates.push_back("C:\\Program Files\\Microsoft\\Edge\\Application\\msedge.exe");
    for (const auto& exe : candidates) {
        if (fs::exists(exe)) {
            if (reinterpret_cast<INT_PTR>(ShellExecuteA(nullptr, "open", exe.c_str(),
                    url.c_str(), nullptr, SW_SHOWNORMAL)) > 32) {
                return;
            }
        }
    }
    ShellExecuteA(nullptr, "open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#else
    (void)url;
#endif
}

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
    GUI::DrawEngineSplash(m_SplashTimer, m_SplashDuration, m_SplashFadeStart, "by marty64",
                          m_ImGuiLayer ? m_ImGuiLayer->GetHeadingFont() : nullptr);
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
    // Everything scales with the editor UI scale — the font does (io.FontGlobalScale), so
    // the box, padding and text offsets must too, or the (larger) glyphs overflow a fixed
    // box and the icon collides with the message.
    const f32 s = m_EditorSettings.uiScale > 0.0f ? m_EditorSettings.uiScale : 1.0f;
    const f32 padding  = 16.0f * s;
    const f32 minToastW = 300.0f * s;
    const f32 spacing  = 6.0f * s;
    const f32 padX     = 12.0f * s;   // inner left/right padding
    const f32 gap      = 8.0f * s;    // icon -> message gap
    const f32 startY   = io.DisplaySize.y - padding;

    ImDrawList* fg = ImGui::GetForegroundDrawList();

    // Draw toasts stacked bottom-up (accumulate per-toast height so variable sizes stack cleanly)
    f32 stackedH = 0.0f;
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

        // Type icon
        const char* icon;
        switch (notif.type) {
            case NotificationType::Success: icon = "[OK]"; break;
            case NotificationType::Warning: icon = "[!]"; break;
            case NotificationType::Error:   icon = "[X]"; break;
            default:                        icon = "[i]"; break;
        }

        // Measure at the current (scaled) font so the box fits and nothing overlaps.
        const ImVec2 iconSz = ImGui::CalcTextSize(icon);
        const ImVec2 msgSz  = ImGui::CalcTextSize(notif.message.c_str());
        const f32 toastH = std::max(40.0f * s, std::max(iconSz.y, msgSz.y) + 12.0f * s);
        const f32 contentW = padX + iconSz.x + gap + msgSz.x + padX;
        const f32 toastW = std::min(std::max(minToastW, contentW), io.DisplaySize.x - 2.0f * padding);

        // Right-anchored; slide in from the right.
        const f32 slideOffset = (1.0f - notif.slideIn) * (toastW + padding);
        const f32 yPos = startY - stackedH - toastH;
        const f32 xPos = io.DisplaySize.x - padding - toastW + slideOffset;

        // Background color based on type
        ImU32 bgColor;
        switch (notif.type) {
            case NotificationType::Success: bgColor = IM_COL32(30, 110, 50, static_cast<u8>(220 * fadeAlpha)); break;
            case NotificationType::Warning: bgColor = IM_COL32(140, 110, 20, static_cast<u8>(220 * fadeAlpha)); break;
            case NotificationType::Error:   bgColor = IM_COL32(150, 30, 30, static_cast<u8>(220 * fadeAlpha)); break;
            default:                        bgColor = IM_COL32(40, 60, 90, static_cast<u8>(220 * fadeAlpha)); break;
        }

        fg->AddRectFilled(ImVec2(xPos, yPos), ImVec2(xPos + toastW, yPos + toastH), bgColor, 6.0f * s);

        // Icon + message, vertically centered; message placed AFTER the measured icon.
        const ImU32 textColor = IM_COL32(255, 255, 255, static_cast<u8>(240 * fadeAlpha));
        fg->AddText(ImVec2(xPos + padX, yPos + (toastH - iconSz.y) * 0.5f), textColor, icon);
        fg->AddText(ImVec2(xPos + padX + iconSz.x + gap, yPos + (toastH - msgSz.y) * 0.5f),
                    textColor, notif.message.c_str());

        stackedH += toastH + spacing;
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

    // Pre-scan file to populate scene hierarchy preview
    ScanImportPreview(path);

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

void EditorLayer::ScanImportPreview(const std::string& filepath) {
    m_ImportPreviewNodes.clear();
    m_ImportPreviewScanned = false;

    std::string ext = std::filesystem::path(filepath).extension().string();
    for (auto& c : ext) c = static_cast<char>(std::tolower(c));

    if (ext == ".gltf" || ext == ".glb") {
        Assets::GLTFScene scene;
        if (Assets::GLTFLoader::Load(filepath, scene)) {
            for (usize i = 0; i < scene.nodes.size(); ++i) {
                const auto& node = scene.nodes[i];
                ImportPreviewNode preview;
                preview.name = node.name.empty() ? ("Node " + std::to_string(i)) : node.name;
                preview.meshIndex = node.meshIndex;
                preview.hasMesh = (node.meshIndex >= 0);
                preview.hasSkin = (node.skinIndex >= 0);
                preview.selected = true;

                if (preview.hasMesh && node.meshIndex < static_cast<i32>(scene.meshes.size())) {
                    const auto& mesh = scene.meshes[node.meshIndex];
                    for (const auto& prim : mesh.primitives) {
                        preview.vertexCount += static_cast<u32>(prim.vertices.size());
                        if (!prim.morphTargets.empty()) {
                            preview.morphTargetCount = static_cast<u32>(prim.morphTargets.size());
                        }
                    }
                }

                // Find parent index
                preview.parentIndex = -1;
                for (usize p = 0; p < scene.nodes.size(); ++p) {
                    for (i32 child : scene.nodes[p].children) {
                        if (child == static_cast<i32>(i)) { preview.parentIndex = static_cast<i32>(p); break; }
                    }
                    if (preview.parentIndex >= 0) break;
                }

                m_ImportPreviewNodes.push_back(preview);
            }
            m_ImportPreviewScanned = true;
        }
    } else {
        Assets::AssimpScene scene;
        if (Assets::AssimpLoader::Load(filepath, scene)) {
            for (usize i = 0; i < scene.nodes.size(); ++i) {
                const auto& node = scene.nodes[i];
                ImportPreviewNode preview;
                preview.name = node.name.empty() ? ("Node " + std::to_string(i)) : node.name;
                preview.meshIndex = node.meshIndex;
                preview.hasMesh = (node.meshIndex >= 0 || !node.meshIndices.empty());
                preview.parentIndex = node.parentIndex;
                preview.selected = true;

                if (preview.hasMesh) {
                    for (i32 mi : node.meshIndices) {
                        if (mi >= 0 && mi < static_cast<i32>(scene.meshes.size())) {
                            for (const auto& prim : scene.meshes[mi].primitives) {
                                preview.vertexCount += static_cast<u32>(prim.vertices.size());
                            }
                        }
                    }
                }

                m_ImportPreviewNodes.push_back(preview);
            }
            m_ImportPreviewScanned = true;
            if (scene.hasSkinning) {
                for (auto& n : m_ImportPreviewNodes) {
                    if (n.hasMesh) n.hasSkin = true;
                }
            }
        }
    }
}

void EditorLayer::DrawImportDialog() {
    ImGui::OpenPopup("Import Settings");

    ImGui::SetNextWindowSize(ImVec2(540 * m_EditorSettings.uiScale, 0), ImGuiCond_Always);
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

        // --- Group (multi-drop) batch header ---
        const bool isBatch = m_ImportBatchTotal > 1;
        if (isBatch) {
            ImGui::Separator();
            int doneIdx = m_ImportBatchTotal - static_cast<int>(m_ImportBatchQueue.size());
            ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Group import — %d models (this is #%d of %d)",
                               m_ImportBatchTotal, doneIdx + 1, m_ImportBatchTotal);
            ImGui::Checkbox("Apply these settings to all", &m_ImportBatchApplyToAll);
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("ON: import all %d models now with the settings below.\n"
                                  "OFF: step through them one at a time, choosing settings per model.",
                                  m_ImportBatchTotal);
            }
            if (ImGui::TreeNodeEx("Files in this batch", ImGuiTreeNodeFlags_None)) {
                for (const auto& p : m_ImportBatchQueue) {
                    ImGui::BulletText("%s", std::filesystem::path(p).filename().string().c_str());
                }
                ImGui::TreePop();
            }
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
        ImGui::Checkbox("Normalize size (~1.8m)", &m_ImportDialogOptions.normalizeScale);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("ON: rescale to a sane on-screen size when the file's unit is unreliable.\n"
                              "OFF: import at the file's true unit-converted size — no size magic, you scale it.");
        }
        ImGui::Checkbox("Import Materials", &m_ImportDialogOptions.importMaterials);
        ImGui::Checkbox("Import Animations", &m_ImportDialogOptions.importAnimations);
        if (m_ImportDialogOptions.importAnimations) {
            ImGui::Indent(16.0f);
            ImGui::Checkbox("Auto-play on import", &m_ImportDialogOptions.autoPlayAnimation);
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Start the first animation immediately. OFF = import at the rest\n"
                                  "pose (recommended while skinned animation is being fixed).");
            }
            ImGui::Unindent(16.0f);
        }
        ImGui::Checkbox("Generate Colliders", &m_ImportDialogOptions.generateColliders);
        if (m_ImportDialogOptions.generateColliders) {
            const char* shapeNames[] = { "Box", "Sphere", "Capsule", "Convex Mesh" };
            int shapeIdx = static_cast<int>(m_ImportDialogOptions.colliderShape);
            ImGui::Indent(16.0f);
            ImGui::SetNextItemWidth(160.0f);
            if (ImGui::Combo("Collider Shape", &shapeIdx, shapeNames, 4)) {
                m_ImportDialogOptions.colliderShape = static_cast<Assets::ImportColliderShape>(shapeIdx);
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Box/Sphere/Capsule fit the mesh bounds. Convex Mesh hugs the\n"
                                  "actual geometry (best fit, higher physics cost). Sizes are baked\n"
                                  "in world space from the imported scale.");
            }
            ImGui::Unindent(16.0f);
        }
        ImGui::Checkbox("Generate LODs", &m_ImportDialogOptions.generateLODs);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Generate Level-of-Detail meshes (can be slow for large models)");
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // --- Scene Contents Preview ---
        if (m_ImportPreviewScanned && !m_ImportPreviewNodes.empty()) {
            bool previewOpen = ImGui::TreeNodeEx("Scene Contents", ImGuiTreeNodeFlags_DefaultOpen);
            if (previewOpen) {
                // Select/Deselect all buttons
                if (ImGui::SmallButton("Select All")) {
                    for (auto& n : m_ImportPreviewNodes) n.selected = true;
                }
                ImGui::SameLine();
                if (ImGui::SmallButton("Deselect All")) {
                    for (auto& n : m_ImportPreviewNodes) n.selected = false;
                }
                ImGui::SameLine();
                if (ImGui::SmallButton("Meshes Only")) {
                    for (auto& n : m_ImportPreviewNodes) n.selected = n.hasMesh;
                }

                // Count selected
                u32 selectedCount = 0, totalVerts = 0;
                for (const auto& n : m_ImportPreviewNodes) {
                    if (n.selected) { selectedCount++; totalVerts += n.vertexCount; }
                }
                ImGui::TextDisabled("%u / %zu nodes selected, %u vertices",
                    selectedCount, m_ImportPreviewNodes.size(), totalVerts);

                // Hierarchy tree
                ImGui::BeginChild("ImportPreviewTree", ImVec2(0, 150), true);

                // Recursive draw function for hierarchy
                std::function<void(i32)> drawNode = [&](i32 parentIdx) {
                    for (usize i = 0; i < m_ImportPreviewNodes.size(); ++i) {
                        auto& node = m_ImportPreviewNodes[i];
                        if (node.parentIndex != parentIdx) continue;

                        // Check if has children
                        bool hasChildren = false;
                        for (const auto& n : m_ImportPreviewNodes) {
                            if (n.parentIndex == static_cast<i32>(i)) { hasChildren = true; break; }
                        }

                        ImGui::PushID(static_cast<int>(i));

                        // Checkbox
                        ImGui::Checkbox("##Sel", &node.selected);
                        ImGui::SameLine();

                        // Icon + name
                        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
                        if (!hasChildren) flags |= ImGuiTreeNodeFlags_Leaf;

                        // Color based on type
                        if (node.hasSkin) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.9f, 1.0f, 1.0f));
                        else if (node.hasMesh) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.9f, 0.9f, 1.0f));
                        else ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));

                        bool open = ImGui::TreeNodeEx(node.name.c_str(), flags);
                        ImGui::PopStyleColor();

                        // Info on same line
                        if (node.hasMesh) {
                            ImGui::SameLine(ImGui::GetContentRegionAvail().x - 100);
                            ImGui::TextDisabled("%u verts", node.vertexCount);
                        }
                        if (node.morphTargetCount > 0) {
                            ImGui::SameLine();
                            ImGui::TextColored(ImVec4(0.8f, 0.6f, 1.0f, 1.0f), "%u morphs", node.morphTargetCount);
                        }

                        if (open) {
                            if (hasChildren) drawNode(static_cast<i32>(i));
                            ImGui::TreePop();
                        }

                        ImGui::PopID();
                    }
                };

                drawNode(-1); // Start from root nodes

                ImGui::EndChild();
                ImGui::TreePop();
            }
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
        const char* importLabel = !isBatch ? "Import"
                                : (m_ImportBatchApplyToAll ? "Import All" : "Import This");
        if (ImGui::Button(importLabel, ImVec2(140, 0))) {
            // Build excluded node list from deselected preview checkboxes
            m_ImportDialogOptions.excludedNodeIndices.clear();
            for (usize i = 0; i < m_ImportPreviewNodes.size(); ++i) {
                if (!m_ImportPreviewNodes[i].selected) {
                    m_ImportDialogOptions.excludedNodeIndices.push_back(static_cast<i32>(i));
                }
            }

            if (isBatch && m_ImportBatchApplyToAll) {
                // Apply the same settings to every model. The per-frame processor in
                // Update() imports the queue, spaced into a row, then frames them all.
                m_ImportBatchOptions = m_ImportDialogOptions;
                m_ImportBatchActive = true;
                m_ShowImportDialog = false;
                ImGui::CloseCurrentPopup();
            } else if (isBatch) {
                // Step-through: import THIS model now with its own settings, then reopen
                // the dialog for the next one (fresh options / re-import metadata).
                int doneIdx = m_ImportBatchTotal - static_cast<int>(m_ImportBatchQueue.size());
                const f32 spacing = 3.0f;
                f32 x = (static_cast<f32>(doneIdx) - static_cast<f32>(m_ImportBatchTotal - 1) * 0.5f) * spacing;
                std::string cur = m_ImportBatchQueue.front();
                m_ImportBatchQueue.erase(m_ImportBatchQueue.begin());
                ExecuteImport(cur, m_ImportDialogOptions, Math::Vector3(x, 0.0f, 0.0f), /*showResultDialog=*/false);
                if (m_LastImportResult.rootEntity != ECS::INVALID_ENTITY)
                    m_ImportBatchRoots.push_back(m_LastImportResult.rootEntity);
                ImGui::CloseCurrentPopup();
                if (!m_ImportBatchQueue.empty()) {
                    ImportModel(m_ImportBatchQueue.front());   // reopens the dialog for the next file
                } else {
                    FinishGroupImport();
                    m_ShowImportDialog = false;
                }
            } else {
                // Single import: defer to next frame so the loading overlay can render.
                m_ImportPending = true;
                m_ImportPendingPath = m_ImportDialogPath;
                m_ImportPendingOptions = m_ImportDialogOptions;
                m_ShowImportDialog = false;
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button(isBatch ? "Cancel Batch" : "Cancel", ImVec2(140, 0))) {
            // Cancelling abandons the whole batch.
            m_ImportBatchQueue.clear();
            m_ImportBatchTotal = 0;
            m_ImportBatchActive = false;
            m_ImportBatchRoots.clear();
            m_ShowImportDialog = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

// Frame + select every model imported in a group batch, then clear the batch state.
void EditorLayer::FinishGroupImport() {
    if (!m_World) { m_ImportBatchRoots.clear(); m_ImportBatchTotal = 0; return; }
    if (m_ImportBatchRoots.size() > 1) {
        m_SelectedEntities.clear();
        for (ECS::Entity r : m_ImportBatchRoots) {
            if (m_World->IsValid(r)) m_SelectedEntities.insert(r);
        }
        if (!m_ImportBatchRoots.empty()) m_PrimarySelected = m_ImportBatchRoots.back();
        FocusOnSelection();
    }
    m_ImportBatchRoots.clear();
    m_ImportBatchTotal = 0;
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

void EditorLayer::ImportModelImmediate(const std::string& path, const Math::Vector3& placementOffset) {
    if (!m_World) {
        ENJIN_LOG_ERROR(Editor, "Cannot import model: no world loaded");
        m_ConsoleLog.push_back("[Error] Cannot import model: no world loaded");
        return;
    }
    // Project-first workflow: make sure there's a project so references/assets resolve.
    EnsureProjectForScene(path);

    // Auto-detected options: reuse a saved .enjinasset if this file was imported
    // before, otherwise defaults. sourceApp stays Auto so the importer detects axis
    // from metadata, and scale is now driven by the file's UnitScaleFactor — no dialog
    // needed for the geometry to come in correctly sized.
    Assets::ImportOptions opts;
    if (Assets::AssetMetadata::Exists(path)) {
        Assets::AssetMetadata meta;
        if (meta.Load(path)) opts = meta.importOptions;
    }

    // No modal dialog, no per-file result popup — that is what makes dropping many
    // models at once "just work".
    ExecuteImport(path, opts, placementOffset, /*showResultDialog=*/false);
}

void EditorLayer::BeginGroupImport(const std::vector<std::string>& paths) {
    if (paths.empty() || !m_World) return;
    m_ImportBatchQueue = paths;
    m_ImportBatchTotal = static_cast<int>(paths.size());
    m_ImportBatchApplyToAll = true;
    m_ImportBatchActive = false;
    m_ImportBatchRoots.clear();
    // Open the standard import dialog on the first file. It sets up options, source-app
    // detection and the scene preview; DrawImportDialog shows the batch UI + "apply to
    // all" because m_ImportBatchTotal > 1.
    ImportModel(paths.front());
}

void EditorLayer::ExecuteImport(const std::string& path, const Assets::ImportOptions& options,
                                const Math::Vector3& placementOffset, bool showResultDialog) {
    if (!m_World) return;

    Assets::ImportResult result = Assets::SceneImporter::Import(path, m_World, options);

    if (result.success) {
        // Make imported mesh source references portable: store the source path
        // relative to the project root so the reference survives moving/shipping the
        // project. Assets outside the project keep their absolute path (still works
        // locally). The cache's search root (set each frame in Update) resolves the
        // relative form back to a real file on load.
        {
            std::string projPath = m_SceneManager.GetProjectPath();
            if (!projPath.empty()) {
                std::filesystem::path root = std::filesystem::path(projPath).parent_path();
                std::error_code ec;
                for (ECS::Entity e : result.entities) {
                    if (!m_World->IsValid(e)) continue;
                    auto* mc = m_World->GetComponent<ECS::MeshComponent>(e);
                    if (!mc || !mc->source.Valid()) continue;
                    std::filesystem::path rel =
                        std::filesystem::relative(mc->source.sourcePath, root, ec);
                    bool escapes = ec || rel.empty();
                    if (!escapes) {
                        for (const auto& part : rel) { if (part == "..") { escapes = true; break; } }
                    }
                    if (!escapes) mc->source.sourcePath = rel.generic_string();
                }
            }
        }

        // Copy imported textures into project assets and update the material paths.
        // Mirrors the mesh source portability block above: textures that resolved to
        // an absolute path outside the project are copied to assets/textures/ so they
        // stay valid after the project is moved or shared.
        {
            std::string projPath = m_SceneManager.GetProjectPath();
            if (!projPath.empty()) {
                std::string projRoot = std::filesystem::path(projPath).parent_path().string();
                auto importTex = [&](std::string& texPath) {
                    if (!texPath.empty())
                        texPath = Assets::CopyToProjectAssets(
                            texPath, projRoot, "assets/textures");
                };
                for (ECS::Entity e : result.entities) {
                    if (!m_World->IsValid(e)) continue;
                    auto* mat = m_World->GetComponent<ECS::MaterialComponent>(e);
                    if (!mat) continue;
                    importTex(mat->baseColorTexturePath);
                    importTex(mat->normalTexturePath);
                    importTex(mat->metallicRoughnessTexturePath);
                    importTex(mat->emissiveTexturePath);
                    importTex(mat->specularTexturePath);
                    importTex(mat->heightTexturePath);
                    importTex(mat->matcapTexturePath);
                }
            }
        }

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

        // Place the import where requested (drag-drop spreads multiple models into a
        // row instead of stacking them all at the origin). Applied to the root, which
        // carries the whole hierarchy.
        if (result.rootEntity != ECS::INVALID_ENTITY &&
            (placementOffset.x != 0.0f || placementOffset.y != 0.0f || placementOffset.z != 0.0f)) {
            if (auto* rootXf = m_World->GetComponent<ECS::TransformComponent>(result.rootEntity)) {
                rootXf->position = rootXf->position + placementOffset;
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

        // Track for re-import and undo
        m_LastImportedModelPath = path;
        m_LastImportEntities = result.entities;
    }

    // Store result and (unless suppressed, e.g. batch drag-drop) show the dialog.
    m_LastImportResult = result;
    if (showResultDialog) m_ShowImportResultDialog = true;

    // Screen reader announcement
    if (m_Announcer.enabled) {
        if (result.success) {
            char buf[128];
            snprintf(buf, sizeof(buf), "Import successful: %u meshes, %u materials, %u vertices",
                result.meshCount, result.materialCount, result.totalVertexCount);
            m_Announcer.Announce(buf, Accessibility::AnnouncePriority::Normal);
        } else {
            m_Announcer.Announce("Import failed: " + result.errorMessage, Accessibility::AnnouncePriority::High);
        }
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
    ImGui::Checkbox("\"Made with TEGE\" intro", &m_BuildConfig.engineSplash);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Shows a short engine intro card when the game starts.\nSkippable with any key; fades out into your title screen.");
    }

    // Target platform
    ImGui::Spacing();
    const char* platformLabels[] = { "Desktop", "Web (HTML5)" };
    int platform = static_cast<int>(m_BuildConfig.target);
    if (ImGui::Combo("Platform", &platform, platformLabels, 2)) {
        m_BuildConfig.target = static_cast<Build::BuildTargetPlatform>(platform);
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(
            "Desktop: Native executable (Windows/Linux/macOS)\n"
            "Web (HTML5): WebAssembly + WebGPU — runs in Chrome/Edge browser.\n"
            "Requires Emscripten SDK (emsdk) to be installed.");
    }

    // Web platform limitations panel
    if (m_BuildConfig.target == Build::BuildTargetPlatform::Web) {
        ImGui::Spacing();
        ImVec4 warnColor(1.0f, 0.85f, 0.3f, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_Text, warnColor);
        if (ImGui::TreeNodeEx("WebGPU Limitations", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::PopStyleColor();
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.7f, 1.0f));

            ImGui::BulletText("Shadows: 1 directional cascade (vs 4 on desktop)");
            ImGui::BulletText("Lights: max 4 dir + 4 point + 4 spot (vs 64+32 desktop)");
            ImGui::BulletText("Ray tracing: Not available");
            ImGui::BulletText("DLSS / XeSS: Not available (FXAA + MSAA 4x only)");
            ImGui::BulletText("Particles / Terrain / Water: Not yet implemented");
            ImGui::BulletText("UI Canvas / TextComponent rendering: Not yet implemented");
            ImGui::BulletText("2D sprite textures: Not yet supported (sprites render untextured)");
            ImGui::BulletText("Multi-material (per-submesh): Not yet supported");
            ImGui::BulletText("LOD / Instancing: Not yet supported");

            ImGui::Spacing();
            ImGui::TextWrapped("Supported: PBR lighting, shadows (dir+spot+point), "
                "skeletal animation, physics (Jolt+Box2D), audio, scripting, "
                "full gameplay loop (pickups, hazards, health, trigger zones, win/lose), "
                "built-in HTML game HUD (health bar, coin counter, victory/defeat screen), "
                "AI / dialogue / cutscenes, save persistence (browser storage), "
                "ACES tonemapping, bloom, MSAA 4x, FXAA, fog, procedural sky.");

            ImGui::PopStyleColor();
            ImGui::TreePop();
        } else {
            ImGui::PopStyleColor();
        }
    }

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

    // --- Build / Build & Run buttons ---
    bool canBuild = !m_BuildInProgress && !m_BuildConfig.outputDir.empty();
    bool buildRequested = false;
    bool runAfterBuild = false;
    if (!canBuild) ImGui::BeginDisabled();
    if (ImGui::Button("Build", ImVec2(120, 30))) {
        buildRequested = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Build & Run", ImVec2(120, 30))) {
        buildRequested = true;
        runAfterBuild = true;
    }
    if (!canBuild) ImGui::EndDisabled();

    // Resolve the exe the build will produce (same naming as CopyPlayer)
    auto builtExePath = [this]() -> std::string {
        std::string destName = m_BuildConfig.windowTitle.empty() ? "Game" : m_BuildConfig.windowTitle;
        for (auto& c : destName) {
            if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' ||
                c == '"' || c == '<' || c == '>' || c == '|') {
                c = '_';
            }
        }
#ifdef ENJIN_PLATFORM_WINDOWS
        destName += ".exe";
#endif
        return (std::filesystem::path(m_BuildConfig.outputDir) / destName).string();
    };

    if (buildRequested) {
        StartBuildAsync(runAfterBuild);
    }

    // Progress bar (worker publishes progress under the mutex; UI copies it out)
    if (m_BuildInProgress || m_BuildFinished) {
        ImGui::SameLine();
        if (m_BuildInProgress) {
            {
                std::lock_guard<std::mutex> lock(m_BuildMutex);
                m_BuildProgress = m_BuildWorkerProgress;
                m_BuildProgressPhase = m_BuildWorkerPhase;
            }
            ImGui::ProgressBar(m_BuildProgress, ImVec2(-1, 0),
                               m_BuildProgressPhase.c_str());
        } else if (m_BuildResult.success) {
            ImGui::TextColored(ImVec4(0.2f, 0.9f, 0.2f, 1.0f), "Build succeeded!");
        } else {
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Build failed!");
        }
    }

    // Run + open output folder buttons
    if (m_BuildFinished && m_BuildResult.success) {
        ImGui::SameLine();
        if (ImGui::Button("Run")) {
            std::string exePath = builtExePath();
            if (std::filesystem::exists(exePath)) {
#ifdef ENJIN_PLATFORM_WINDOWS
                ShellExecuteA(nullptr, "open", exePath.c_str(), nullptr,
                              m_BuildConfig.outputDir.c_str(), SW_SHOWNORMAL);
#endif
            } else {
                ShowNotification("Game exe not found — rebuild first", NotificationType::Warning);
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Open Folder")) {
#ifdef ENJIN_PLATFORM_WINDOWS
            ShellExecuteA(nullptr, "open", m_BuildConfig.outputDir.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#elif defined(ENJIN_PLATFORM_MACOS)
            // S20: Use posix_spawn to avoid shell command injection
            {
                const char* argv[] = { "open", m_BuildConfig.outputDir.c_str(), nullptr };
                pid_t pid = 0;
                posix_spawnp(&pid, "open", nullptr, nullptr, const_cast<char**>(argv), environ);
            }
#else
            // S20: Use posix_spawn to avoid shell command injection
            {
                const char* argv[] = { "xdg-open", m_BuildConfig.outputDir.c_str(), nullptr };
                pid_t pid = 0;
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
// Async build: the pipeline is pure file I/O + process spawns, so it runs on
// a worker thread and the editor stays responsive. Everything that touches
// editor subsystems (notifications, dev web server, launching the game)
// happens in PollBuildThread on the main thread.
// ---------------------------------------------------------------------------

void EditorLayer::StartBuildAsync(bool runAfterBuild) {
    if (m_BuildInProgress) return;

    // Auto-save current scene before building so the .enjin file on disk
    // contains all current entities and mesh data (main thread - touches World)
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
    m_BuildProgressPhase.clear();
    m_BuildResult = Build::BuildResult{};
    m_BuildRunAfter = runAfterBuild;
    m_BuildThreadDone.store(false, std::memory_order_release);
    {
        std::lock_guard<std::mutex> lock(m_BuildMutex);
        m_BuildWorkerProgress = 0.0f;
        m_BuildWorkerPhase = "starting...";
    }

    if (m_BuildThread.joinable()) m_BuildThread.join();   // reap a previous build
    Build::BuildConfig config = m_BuildConfig;            // by value - worker owns its copy
    m_BuildThread = std::thread([this, config]() {
        Build::BuildPipeline pipeline;
        pipeline.SetProgressCallback([this](const std::string& phase, float progress) {
            std::lock_guard<std::mutex> lock(m_BuildMutex);
            m_BuildWorkerPhase = phase;
            m_BuildWorkerProgress = progress;
        });
        Build::BuildResult result = pipeline.Execute(config);
        {
            std::lock_guard<std::mutex> lock(m_BuildMutex);
            m_BuildWorkerResult = std::move(result);
        }
        m_BuildThreadDone.store(true, std::memory_order_release);
    });
}

void EditorLayer::PollBuildThread() {
    if (!m_BuildInProgress || !m_BuildThreadDone.load(std::memory_order_acquire)) return;

    if (m_BuildThread.joinable()) m_BuildThread.join();
    {
        std::lock_guard<std::mutex> lock(m_BuildMutex);
        m_BuildResult = m_BuildWorkerResult;
    }
    m_BuildInProgress = false;
    m_BuildFinished = true;
    m_Telemetry.TrackBuildRun();

    if (!m_BuildResult.success) {
        ShowNotification("Build failed", NotificationType::Error);
        return;
    }
    ShowNotification("Build complete!", NotificationType::Success);
    if (!m_BuildRunAfter) return;
    m_BuildRunAfter = false;

    if (m_BuildConfig.target == Build::BuildTargetPlatform::Web) {
        // Web builds have no exe — serve the output on localhost and open the
        // browser (browsers refuse wasm over file://)
        u16 port = m_DevWebServer.Start(m_BuildConfig.outputDir);
        if (port != 0) {
            OpenUrlPreferChromium("http://localhost:" + std::to_string(port) + "/");
            ShowNotification("Running in browser...", NotificationType::Info);
        } else {
            ShowNotification("Build succeeded but the preview server could not start", NotificationType::Warning);
        }
        return;
    }

    // Same exe naming as CopyPlayer
    std::string destName = m_BuildConfig.windowTitle.empty() ? "Game" : m_BuildConfig.windowTitle;
    for (auto& c : destName) {
        if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' ||
            c == '"' || c == '<' || c == '>' || c == '|') {
            c = '_';
        }
    }
#ifdef ENJIN_PLATFORM_WINDOWS
    destName += ".exe";
#endif
    std::string exePath = (std::filesystem::path(m_BuildConfig.outputDir) / destName).string();
    if (std::filesystem::exists(exePath)) {
#ifdef ENJIN_PLATFORM_WINDOWS
        ShellExecuteA(nullptr, "open", exePath.c_str(), nullptr,
                      m_BuildConfig.outputDir.c_str(), SW_SHOWNORMAL);
#endif
        ShowNotification("Launching game...", NotificationType::Info);
    } else {
        ShowNotification("Build succeeded but the game exe was not found", NotificationType::Warning);
    }
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
                // Before SaveScene: a stale layer stack would otherwise be
                // persisted beside the brand-new empty scene.
                ResetLayerSession();
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
    ImGui::Checkbox("Generate Embed Code", &m_HTML5Config.generateEmbedCode);
    ImGui::SameLine();
    ImGui::Checkbox("Create .zip", &m_HTML5Config.zipOutput);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Package output as .zip for upload to web hosting or a game portal");

    // Favicon
    ImGui::Text("Favicon: %s", m_HTML5Config.faviconPath.empty() ? "(none)" : m_HTML5Config.faviconPath.c_str());
    ImGui::SameLine();
    if (ImGui::Button("Browse##Favicon")) {
        std::vector<FileFilter> filters = {
            { "Icons", "*.ico;*.png;*.svg" },
            { "All Files", "*.*" }
        };
        std::string path = FileDialog::OpenFile("Select Favicon", filters);
        if (!path.empty()) m_HTML5Config.faviconPath = path;
    }

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
    static std::string lastZipPath;
    static std::string lastExportError;
    if (ImGui::Button("Export", ImVec2(120, 30))) {
        lastZipPath.clear();
        lastExportError.clear();
        auto result = Build::HTML5Exporter::Export(m_HTML5Config, m_BuildConfig);
        if (result.success) {
            ENJIN_LOG_INFO(Editor, "HTML5 export complete: %zu files", result.files.size());
            lastEmbedCode = result.embedCode;
            lastZipPath = result.zipPath;
        } else {
            ENJIN_LOG_ERROR(Editor, "HTML5 export failed: %s", result.error.c_str());
            lastExportError = result.error;
        }
    }
    ImGui::SameLine();
    // The maker-simple path: serve the export from the editor's built-in
    // localhost server and open the browser — no Python, no terminal.
    // (Browsers refuse WebAssembly over file://, so double-clicking
    // index.html can never work.)
    if (ImGui::Button("Run in Browser", ImVec2(130, 30)) && !m_HTML5Config.outputDir.empty()) {
        u16 port = m_DevWebServer.Start(m_HTML5Config.outputDir);
        if (port != 0) {
            OpenUrlPreferChromium("http://localhost:" + std::to_string(port) + "/");
        } else {
            ShowNotification("Could not start the preview server", NotificationType::Error);
        }
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Serve the exported build on localhost and open it in your browser.\nRe-export, then just reload the browser tab.");
    }
    ImGui::SameLine();
    if (ImGui::Button("Open Output Folder") && !m_HTML5Config.outputDir.empty()) {
#ifdef _WIN32
        ShellExecuteA(nullptr, "explore", m_HTML5Config.outputDir.c_str(), nullptr, nullptr, SW_SHOWDEFAULT);
#endif
    }
    if (m_DevWebServer.IsRunning()) {
        ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.4f, 1.0f),
            "Serving at http://localhost:%u  (reload the tab after re-export)",
            static_cast<u32>(m_DevWebServer.GetPort()));
        ImGui::SameLine();
        if (ImGui::SmallButton("Stop Server")) {
            m_DevWebServer.Stop();
        }
    }

    // Export result feedback
    if (!lastExportError.empty()) {
        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Export failed: %s", lastExportError.c_str());
    }
    if (!lastZipPath.empty()) {
        ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "Zip: %s", lastZipPath.c_str());
        if (ImGui::Button("Copy Zip Path")) {
            ImGui::SetClipboardText(lastZipPath.c_str());
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
        // Consume the crash file NOW that it's in memory. It used to be cleared
        // only when the user dismissed the dialog, so closing the editor with the
        // dialog still up left the file behind and re-fired this warning (and
        // re-spammed the Discord webhook) on every subsequent launch. The dialog
        // and auto-submit below both work from the in-memory copy.
        Debug::ClearPreviousCrashReport();
        if (!m_PreviousCrashReport.empty()) {
            ENJIN_LOG_WARN(Editor, "Previous session crashed — auto-submitting report and showing dialog.");

            // Auto-send to Discord webhook (fire-and-forget, non-blocking)
            {
#if __has_include("Enjin/Editor/WebhookConfig.h")
                constexpr const char* bugHook = ENJIN_DISCORD_BUG_WEBHOOK;
                if (bugHook[0] != '\0') {
                    std::string report = m_PreviousCrashReport;
                    auto* mgr = &m_FeedbackManager;
                    std::thread([mgr, report, bugHook]() {
                        // Format as a Discord webhook message
                        std::string truncated = report.substr(0, 1800); // Discord limit ~2000 chars
                        std::string payload = "{\"content\":\"**Auto Crash Report**\\n```\\n"
                            + truncated + "\\n```\"}";
                        Networking::HTTPClient::Post(bugHook, payload, {{"Content-Type", "application/json"}});
                    }).detach();
                    ENJIN_LOG_INFO(Editor, "Crash report auto-submitted to Discord");
                }
#endif
            }

            // Show the dialog so the user can also review, add context, or submit to GitHub
            m_ShowCrashDialog = true;
        }
        // File already cleared above; the dialog's Clear calls are now no-ops.
    }
}

void EditorLayer::DrawCrashReportDialog() {
    // Modal popup — blocks interaction with windows behind it
    ImGui::OpenPopup("Crash Report — Previous Session");
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(620 * m_EditorSettings.uiScale, 480 * m_EditorSettings.uiScale), ImGuiCond_FirstUseEver);
    if (!ImGui::BeginPopupModal("Crash Report — Previous Session", &m_ShowCrashDialog)) {
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
        ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
}

void EditorLayer::DrawUnsavedChangesDialog() {
    if (!m_ShowUnsavedChangesDialog) return;

    ImGui::OpenPopup("Unsaved Changes");
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    // Auto-resize fits the text, but enforce a comfortable minimum so the
    // exit prompt isn't a cramped strip at high UI scales (half-weight scale —
    // fonts already grow with uiScale).
    const f32 exitScale = 1.0f + (m_EditorSettings.uiScale - 1.0f) * 0.5f;
    ImGui::SetNextWindowSizeConstraints(ImVec2(440.0f * exitScale, 0.0f),
                                        ImVec2(FLT_MAX, FLT_MAX));
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

        const ImVec2 exitBtn(110.0f * exitScale, 34.0f * exitScale);
        if (ImGui::Button("Save", exitBtn)) {
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
                        ResetLayerSession();
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
        if (ImGui::Button("Don't Save", exitBtn)) {
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
                        ResetLayerSession();
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
        if (ImGui::Button("Cancel", exitBtn) || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
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
            // Defer the load to Update() — this modal runs during the Render
            // phase, and clearing/reloading the World here invalidates entity
            // GPU buffers the in-flight frame still references (crash on
            // recovery, 2026-08-07 report: read of 0x17 right after the
            // additive autosave load + skybox create).
            m_PendingRecoveryLoadPath = m_AutoSaveRecoveryPath;
            m_ShowAutoSaveRecoveryDialog = false;
            m_AutoSaveRecoveryPath.clear();
            ImGui::CloseCurrentPopup();
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
    if (!m_ShowQuitFeedbackDialog) return;

    ImGui::OpenPopup("##QuitFeedback");
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    // Scale with the UI at HALF weight (fonts already scale, so full linear scaling
    // made the card comically large at high ui-scale).
    const f32 dlgScale = 1.0f + (m_EditorSettings.uiScale - 1.0f) * 0.5f;
    const f32 contentW = 540.0f * dlgScale;
    const ImVec4 accent(0.36f, 0.62f, 1.0f, 1.0f);

    // Framed, rounded card with an accent border so it reads as a distinct panel
    // rather than a borderless slab. AlwaysAutoResize hugs the content, so there's no
    // awkward empty space below the buttons and no fixed box to fight the ui-scale.
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(26.0f * dlgScale, 22.0f * dlgScale));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 7.0f));
    ImGui::PushStyleColor(ImGuiCol_Border, accent);

    if (ImGui::BeginPopupModal("##QuitFeedback", nullptr,
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize)) {

        // ── Header: larger accented title + a light subtitle ──
        ImGui::SetWindowFontScale(1.55f);
        ImGui::TextColored(accent, "Before you go...");
        ImGui::SetWindowFontScale(1.0f);
        ImGui::TextDisabled("A few seconds of feedback helps shape TEGE. Totally optional.");
        ImGui::Dummy(ImVec2(0, 6));
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0, 8));

        // Helper for a 1-5 rating row — right-aligned pill buttons, accent when picked
        auto RatingRow = [&](const char* label, u8& value) {
            ImGui::AlignTextToFramePadding();
            ImGui::Text("%s", label);
            ImGui::SameLine(contentW - (5.0f * 30.0f + 4.0f * 6.0f) * dlgScale);
            ImGui::PushID(label);
            for (u8 i = 1; i <= 5; i++) {
                ImGui::PushID(static_cast<int>(i));
                bool selected = (value == i);
                if (selected) {
                    ImGui::PushStyleColor(ImGuiCol_Button, accent);
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, accent);
                }
                char num[4];
                snprintf(num, sizeof(num), "%d", i);
                if (ImGui::Button(num, ImVec2(30.0f * dlgScale, 0))) value = i;
                if (selected) ImGui::PopStyleColor(2);
                ImGui::PopID();
                if (i < 5) ImGui::SameLine(0, 6.0f * dlgScale);
            }
            ImGui::PopID();
        };

        ImGui::TextColored(accent, "Rate your experience  (1 = poor, 5 = great)");
        ImGui::Dummy(ImVec2(0, 3));
        RatingRow("Overall Satisfaction", m_QuitSurvey.ratingOverall);
        RatingRow("Stability", m_QuitSurvey.ratingStability);
        RatingRow("Performance", m_QuitSurvey.ratingPerformance);
        RatingRow("Ease of Use", m_QuitSurvey.ratingEaseOfUse);
        RatingRow("Feature Completeness", m_QuitSurvey.ratingFeatureCompleteness);

        ImGui::Dummy(ImVec2(0, 8));
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0, 8));

        // Text fields — static buffers required by ImGui
        static char likeBuf[512] = {};
        static char frustrateBuf[512] = {};
        static char featureBuf[512] = {};

        const f32 inputH = 52.0f * dlgScale;
        ImGui::TextColored(accent, "What did you like?");
        ImGui::InputTextMultiline("##like", likeBuf, sizeof(likeBuf), ImVec2(contentW, inputH));
        ImGui::Dummy(ImVec2(0, 2));
        ImGui::TextColored(accent, "What frustrated you?");
        ImGui::InputTextMultiline("##frustrate", frustrateBuf, sizeof(frustrateBuf), ImVec2(contentW, inputH));
        ImGui::Dummy(ImVec2(0, 2));
        ImGui::TextColored(accent, "What feature do you want most?");
        ImGui::InputTextMultiline("##feature", featureBuf, sizeof(featureBuf), ImVec2(contentW, inputH));

        ImGui::Dummy(ImVec2(0, 14));

        // Buttons — centered against the real window width; primary gets the accent fill
        float buttonWidth = 168.0f * dlgScale;
        float buttonHeight = 34.0f * dlgScale;
        float spacing = 12.0f * dlgScale;
        float totalWidth = buttonWidth * 3 + spacing * 2;
        ImGui::SetCursorPosX((ImGui::GetWindowSize().x - totalWidth) * 0.5f);

        ImGui::PushStyleColor(ImGuiCol_Button, accent);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.46f, 0.70f, 1.0f, 1.0f));
        bool submitQuit = ImGui::Button("Submit & Quit", ImVec2(buttonWidth, buttonHeight));
        ImGui::PopStyleColor(2);
        if (submitQuit) {
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

            // Fire-and-forget: submit to Discord on a detached thread.
            // HTTPClient::Post is synchronous with a 10s timeout — running it on
            // the main thread would freeze the editor if Discord is unreachable.
            {
                constexpr const char* feedbackHook = ENJIN_DISCORD_FEEDBACK_WEBHOOK;
                const std::string webhookUrl = (feedbackHook[0] != '\0')
                    ? feedbackHook : ENJIN_DISCORD_BUG_WEBHOOK;
                if (!webhookUrl.empty()) {
                    // Copy data for the thread (captures by value, not reference)
                    auto survey = m_QuitSurvey;
                    auto* mgr = &m_FeedbackManager;
                    std::thread([mgr, survey, webhookUrl]() {
                        mgr->SubmitQuitSurveyToDiscord(survey, webhookUrl);
                    }).detach();
                }
            }

            likeBuf[0] = frustrateBuf[0] = featureBuf[0] = '\0';
            ImGui::CloseCurrentPopup();
            FinalizeQuit();
        }
        ImGui::SameLine(0, spacing);
        if (ImGui::Button("Skip & Quit", ImVec2(buttonWidth, buttonHeight))) {
            likeBuf[0] = frustrateBuf[0] = featureBuf[0] = '\0';
            ImGui::CloseCurrentPopup();
            FinalizeQuit();
        }
        ImGui::SameLine(0, spacing);
        if (ImGui::Button("Cancel", ImVec2(buttonWidth, buttonHeight))) {
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
    } else {
        // Popup was closed externally or failed to open
        m_ShowQuitFeedbackDialog = false;
        m_QuitSurvey = {};
    }

    ImGui::PopStyleColor(1);   // Border
    ImGui::PopStyleVar(5);     // WindowBorderSize, WindowRounding, WindowPadding, FrameRounding, ItemSpacing
}

void EditorLayer::FinalizeQuit() {
    m_ShowQuitFeedbackDialog = false;
    m_QuitSurvey = {};
    // Defer close to next frame — calling Close() during ImGui rendering
    // destroys resources mid-frame and crashes on some drivers.
    m_PendingQuit = true;
}

// ============================================================================
// Import Result Dialog — shows after every import with stats, warnings, undo
// ============================================================================

void EditorLayer::DrawImportResultDialog() {
    ImGui::OpenPopup("Import Result");
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(600, 0), ImGuiCond_Appearing);

    if (ImGui::BeginPopupModal("Import Result", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        const auto& r = m_LastImportResult;

        // Header — success or failure
        if (r.success) {
            ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.3f, 1.0f), "Import Successful");
        } else {
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Import Failed");
            if (!r.errorMessage.empty()) {
                ImGui::TextWrapped("%s", r.errorMessage.c_str());
            }
        }

        ImGui::Separator();

        if (r.success) {
            // Stats table
            ImGui::Text("Entities: %zu", r.entities.size());
            ImGui::Text("Meshes: %u    Materials: %u    Animations: %u", r.meshCount, r.materialCount, r.animationCount);
            ImGui::Text("Vertices: %u    Indices: %u", r.totalVertexCount, r.totalIndexCount);

            // Textures
            if (!r.texturePathsResolved.empty() || !r.texturePathsMissing.empty()) {
                ImGui::Separator();
                ImGui::Text("Textures: %zu resolved", r.texturePathsResolved.size());
                if (!r.texturePathsMissing.empty()) {
                    ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f), "%zu missing:", r.texturePathsMissing.size());
                    ImGui::Indent(8.0f);
                    for (const auto& tp : r.texturePathsMissing) {
                        // Show just the filename for long paths (e.g. Mixamo server paths)
                        std::string display = tp;
                        auto lastSlash = display.find_last_of("/\\");
                        if (lastSlash != std::string::npos && display.size() > 60) {
                            display = display.substr(lastSlash + 1);
                        }
                        ImGui::BulletText("%s", display.c_str());
                        if (display != tp && ImGui::IsItemHovered()) {
                            ImGui::SetTooltip("%s", tp.c_str());
                        }
                    }
                    ImGui::Unindent(8.0f);
                }
            }

            // Warnings
            if (!r.warnings.empty()) {
                ImGui::Separator();
                if (ImGui::TreeNodeEx("Warnings", ImGuiTreeNodeFlags_DefaultOpen)) {
                    for (const auto& w : r.warnings) {
                        // Color-code: "Fixed"/"Normalized" = yellow, "Note:" = gray, others = orange
                        ImVec4 color(1.0f, 0.7f, 0.3f, 1.0f); // orange default
                        if (w.find("Note:") == 0) color = ImVec4(0.6f, 0.6f, 0.6f, 1.0f);
                        else if (w.find("Fixed") != std::string::npos || w.find("Normalized") != std::string::npos)
                            color = ImVec4(1.0f, 1.0f, 0.4f, 1.0f);
                        ImGui::TextColored(color, "%s", w.c_str());
                    }
                    ImGui::TreePop();
                }
            }
        }

        ImGui::Separator();
        ImGui::Spacing();

        // Action buttons
        f32 btnW = 120.0f;
        if (ImGui::Button("OK", ImVec2(btnW, 0))) {
            m_ShowImportResultDialog = false;
            ImGui::CloseCurrentPopup();
        }

        if (r.success && !m_LastImportEntities.empty()) {
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.2f, 0.2f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.3f, 0.3f, 1.0f));
            if (ImGui::Button("Undo Import", ImVec2(btnW, 0))) {
                UndoLastImport();
                m_ShowImportResultDialog = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::PopStyleColor(2);
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Remove all %zu entities created by this import", m_LastImportEntities.size());
            }
        }

        if (ImGui::IsKeyPressed(ImGuiKey_Escape) || ImGui::IsKeyPressed(ImGuiKey_Enter)) {
            m_ShowImportResultDialog = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    } else {
        m_ShowImportResultDialog = false;
    }
}

// ============================================================================
// Import Undo — destroy all entities from the last import
// ============================================================================

void EditorLayer::UndoLastImport() {
    if (m_LastImportEntities.empty() || !m_World) return;

    u32 destroyed = 0;
    for (ECS::Entity entity : m_LastImportEntities) {
        if (m_World->IsValid(entity)) {
            m_World->DestroyEntity(entity);
            destroyed++;
        }
    }

    ENJIN_LOG_INFO(Editor, "Undo import: destroyed %u entities", destroyed);
    ShowNotification("Undid import (" + std::to_string(destroyed) + " entities removed)", NotificationType::Info);

    m_LastImportEntities.clear();

    // Deselect if any selected entity was part of the import
    m_SelectedEntities.clear();
    m_PrimarySelected = ECS::INVALID_ENTITY;
}

} // namespace Editor
} // namespace Enjin
