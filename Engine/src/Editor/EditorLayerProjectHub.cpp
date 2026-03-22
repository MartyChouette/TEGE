#include "Enjin/Editor/EditorLayer.h"
#include "Enjin/Editor/InspectorUndo.h"
#include "Enjin/Editor/ScenePicker.h"
#include "Enjin/Core/Version.h"
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

// --- Text ellipsis helper for draw-list rendering ---
// Truncates text with "..." suffix when it exceeds maxWidth pixels.
// Returns the (possibly truncated) string to render.
static std::string EllipsizeText(const char* text, f32 maxWidth, ImFont* font = nullptr, f32 fontSize = 0.0f) {
    ImVec2 fullSize;
    if (font && fontSize > 0.0f) {
        fullSize = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, text);
    } else {
        fullSize = ImGui::CalcTextSize(text);
    }
    if (fullSize.x <= maxWidth) return text;

    // Binary search for the longest prefix that fits with "..."
    std::string str(text);
    const char* ellipsis = "...";
    ImVec2 ellipsisSize;
    if (font && fontSize > 0.0f) {
        ellipsisSize = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, ellipsis);
    } else {
        ellipsisSize = ImGui::CalcTextSize(ellipsis);
    }
    f32 availWidth = maxWidth - ellipsisSize.x;
    if (availWidth <= 0.0f) return ellipsis;

    // Walk characters until we exceed the available width
    for (usize len = str.size(); len > 0; --len) {
        ImVec2 sz;
        if (font && fontSize > 0.0f) {
            sz = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, str.c_str(), str.c_str() + len);
        } else {
            std::string sub = str.substr(0, len);
            sz = ImGui::CalcTextSize(sub.c_str());
        }
        if (sz.x <= availWidth) {
            return str.substr(0, len) + ellipsis;
        }
    }
    return ellipsis;
}

// Draw centered text within a card, clipping to cardWidth with ellipsis
static void DrawCenteredClippedText(ImDrawList* dl, const char* text, f32 cardX, f32 cardW, f32 y,
                                     ImU32 color, f32 padding = 12.0f, ImFont* font = nullptr, f32 fontSize = 0.0f) {
    f32 maxW = cardW - padding * 2.0f;
    std::string clipped = EllipsizeText(text, maxW, font, fontSize);
    ImVec2 textSize;
    if (font && fontSize > 0.0f) {
        textSize = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, clipped.c_str());
        dl->AddText(font, fontSize, ImVec2(cardX + (cardW - textSize.x) * 0.5f, y), color, clipped.c_str());
    } else {
        textSize = ImGui::CalcTextSize(clipped.c_str());
        dl->AddText(ImVec2(cardX + (cardW - textSize.x) * 0.5f, y), color, clipped.c_str());
    }
}

void EditorLayer::DrawProjectHub() {
    ImGuiIO& io = ImGui::GetIO();

    // Initialize default project path on first use (prefer persisted lastProjectDir)
    if (m_NewProjectPath[0] == '\0') {
        std::string defaultDir;
        if (!m_EditorSettings.lastProjectDir.empty() &&
            std::filesystem::exists(m_EditorSettings.lastProjectDir)) {
            defaultDir = m_EditorSettings.lastProjectDir;
        } else {
#ifdef _WIN32
            const char* userProfile = std::getenv("USERPROFILE");
            defaultDir = userProfile ? (std::string(userProfile) + "\\Documents\\EnjinProjects") : ".";
#else
            const char* home = std::getenv("HOME");
            defaultDir = home ? (std::string(home) + "/Documents/EnjinProjects") : ".";
#endif
        }
        std::strncpy(m_NewProjectPath, defaultDir.c_str(), sizeof(m_NewProjectPath) - 1);
        m_NewProjectPath[sizeof(m_NewProjectPath) - 1] = '\0';
    }

    // Full-screen dark background
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::SetNextWindowBgAlpha(1.0f);

    ImGuiWindowFlags bgFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNav;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.05f, 0.05f, 0.08f, 1.0f));

    if (ImGui::Begin("##ProjectHubBackground", nullptr, bgFlags)) {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImVec2 area = io.DisplaySize;

        if (m_HubPage == HubPage::Landing) {
            // Dashboard mode: full-width, no sidebar, no centered title
            DrawHubLandingPage(drawList, area, 0.0f, 0.0f);
        } else {
            // Wizard/Demos pages: draw centered title + sidebar
            ImFont* font = ImGui::GetFont();
            f32 cx = io.DisplaySize.x * 0.5f;
            const char* title = "TEGE";
            f32 titleFontSize = 64.0f;
            ImVec2 titleSz = font->CalcTextSizeA(titleFontSize, FLT_MAX, 0.0f, title);
            ImVec2 titlePos(cx - titleSz.x * 0.5f, 28.0f);
            drawList->AddText(nullptr, titleFontSize, titlePos,
                IM_COL32(199, 218, 196, 255), title);

            const char* subtitle = "Game Engine";
            f32 subFontSize = 24.0f;
            ImVec2 subSz = font->CalcTextSizeA(subFontSize, FLT_MAX, 0.0f, subtitle);
            drawList->AddText(nullptr, subFontSize,
                ImVec2(cx - subSz.x * 0.5f, titlePos.y + titleSz.y + 2.0f),
                IM_COL32(140, 160, 140, 160), subtitle);

            f32 contentY = titlePos.y + titleSz.y + subSz.y + 24.0f;
            f32 sidebarW = 300.0f;

            DrawHubRecentSidebar(drawList, area, contentY, sidebarW);

            switch (m_HubPage) {
                case HubPage::Landing:        break; // handled above
                case HubPage::WizardSetup:    DrawHubWizardSetup(drawList, area, contentY, sidebarW);    break;
                case HubPage::WizardTemplate: DrawHubWizardTemplate(drawList, area, contentY, sidebarW); break;
                case HubPage::Demos:          DrawHubDemosTab(drawList, area, contentY, sidebarW);       break;
            }
        }
    }
    ImGui::End();

    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
}

// --------------------------------------------------
// Recent sidebar: always-visible left panel
// --------------------------------------------------
void EditorLayer::DrawHubRecentSidebar(ImDrawList* dl, const ImVec2& area, f32 contentY, f32 sidebarW) {
    ImGuiIO& io = ImGui::GetIO();
    ImFont* font = ImGui::GetFont();

    // Darker background panel
    ImVec2 sbMin(0, contentY);
    ImVec2 sbMax(sidebarW, area.y);
    dl->AddRectFilled(sbMin, sbMax, IM_COL32(16, 18, 24, 255));

    // Vertical divider on right edge
    dl->AddLine(ImVec2(sidebarW, contentY), ImVec2(sidebarW, area.y),
        IM_COL32(60, 65, 80, 150), 1.0f);

    // Header
    f32 headerFontSize = 26.0f;
    const char* header = "Recent Projects";
    ImVec2 headerSz = font->CalcTextSizeA(headerFontSize, FLT_MAX, 0.0f, header);
    dl->AddText(nullptr, headerFontSize,
        ImVec2(16.0f, contentY + 10.0f),
        IM_COL32(160, 165, 185, 220), header);

    // Filter to only show .enjinproject files, grouped by status
    std::vector<int> readyIndices;
    std::vector<int> missingIndices;
    for (int i = 0; i < static_cast<int>(m_EditorSettings.recentProjects.size()); ++i) {
        std::filesystem::path p(m_EditorSettings.recentProjects[i]);
        if (p.extension() == ".enjinproject") {
            if (std::filesystem::exists(m_EditorSettings.recentProjects[i]))
                readyIndices.push_back(i);
            else
                missingIndices.push_back(i);
        }
    }

    f32 listStartY = contentY + 10.0f + headerSz.y + 16.0f;

    if (readyIndices.empty() && missingIndices.empty()) {
        const char* emptyMsg = "No recent projects";
        f32 emptyFontSize = 15.0f;
        ImVec2 emptySz = font->CalcTextSizeA(emptyFontSize, FLT_MAX, 0.0f, emptyMsg);
        dl->AddText(nullptr, emptyFontSize,
            ImVec2((sidebarW - emptySz.x) * 0.5f, listStartY + 20.0f),
            IM_COL32(100, 105, 125, 160), emptyMsg);
        return;
    }

    // Project rows drawn directly on parent draw list (no child window)
    f32 rowH = 72.0f;
    f32 rowPad = 4.0f;
    f32 rowW = sidebarW - 24.0f;
    f32 rowX = 8.0f;
    f32 listBottomY = area.y - 10.0f;
    f32 groupHeaderH = 24.0f;
    f32 groupGap = 8.0f;

    // Clip to sidebar area
    dl->PushClipRect(ImVec2(0, listStartY), ImVec2(sidebarW, listBottomY), true);

    // Lambda to draw a group of project rows with a section header
    f32 curY = listStartY;
    auto drawProjectGroup = [&](const char* groupLabel, ImU32 labelCol,
                                const std::vector<int>& indices, bool canOpen) {
        if (indices.empty()) return;
        if (curY >= listBottomY) return;

        // Section header
        f32 groupFontSize = 13.0f;
        dl->AddText(nullptr, groupFontSize,
            ImVec2(rowX + 4.0f, curY + 4.0f), labelCol, groupLabel);
        curY += groupHeaderH;

        int maxShow = (std::min)(static_cast<int>(indices.size()), 12);
        for (int pi = 0; pi < maxShow; ++pi) {
            if (curY + rowH > listBottomY) break;

            int i = indices[pi];
            ImVec2 rPos(rowX, curY);
            ImVec2 rEnd(rPos.x + rowW, rPos.y + rowH);

            bool hovered = (io.MousePos.x >= rPos.x && io.MousePos.x <= rEnd.x &&
                           io.MousePos.y >= rPos.y && io.MousePos.y <= rEnd.y);

            dl->AddRectFilled(rPos, rEnd,
                hovered ? IM_COL32(35, 40, 55, 255) : IM_COL32(22, 24, 32, 255), 6.0f);

            // Accent bar (left edge)
            ImU32 accentCol = canOpen
                ? (hovered ? IM_COL32(80, 140, 220, 255) : IM_COL32(60, 110, 180, 150))
                : IM_COL32(100, 60, 60, 150);
            dl->AddRectFilled(rPos, ImVec2(rPos.x + 3.0f, rEnd.y), accentCol, 6.0f, ImDrawFlags_RoundCornersLeft);

            if (hovered)
                dl->AddRect(rPos, rEnd, accentCol, 6.0f, 0, 1.5f);

            // Display name (truncated for sidebar width)
            std::filesystem::path fsPath(m_EditorSettings.recentProjects[i]);
            std::string displayName = fsPath.stem().string();
            if (displayName.length() > 28) {
                displayName = displayName.substr(0, 25) + "...";
            }
            ImU32 nameCol = canOpen ? IM_COL32(210, 215, 235, 255) : IM_COL32(140, 140, 150, 180);
            dl->AddText(ImVec2(rPos.x + 12.0f, rPos.y + 8.0f), nameCol, displayName.c_str());

            // Path (truncated)
            std::string pathStr = fsPath.parent_path().string();
            if (pathStr.length() > 32) {
                pathStr = "..." + pathStr.substr(pathStr.length() - 29);
            }
            f32 pathFontSize = 12.0f;
            dl->AddText(nullptr, pathFontSize,
                ImVec2(rPos.x + 12.0f, rPos.y + 28.0f),
                IM_COL32(100, 105, 125, 160), pathStr.c_str());

            // Status badge
            const char* statusText = canOpen ? "Ready" : "Missing";
            ImU32 statusCol = canOpen ? IM_COL32(80, 200, 120, 180) : IM_COL32(200, 80, 80, 180);
            f32 statusFontSize = 11.0f;
            ImVec2 statusSize = font->CalcTextSizeA(statusFontSize, FLT_MAX, 0.0f, statusText);
            dl->AddText(nullptr, statusFontSize,
                ImVec2(rEnd.x - statusSize.x - 10.0f, rPos.y + 24.0f),
                statusCol, statusText);

            // Click to open
            if (hovered && canOpen && ImGui::IsMouseClicked(0)) {
                if (m_SceneManager.LoadProject(m_EditorSettings.recentProjects[i])) {
                    MigrateEditorSettingsToProject();
                    m_EditorSettings.lastProjectDir = std::filesystem::path(m_EditorSettings.recentProjects[i]).parent_path().parent_path().string();
                    m_EditorSettings.Save();
                    auto& scenes = m_SceneManager.GetScenes();
                    if (!scenes.empty()) {
                        auto projDir = std::filesystem::path(m_EditorSettings.recentProjects[i]).parent_path();
                        OpenScene((projDir / scenes[0].path).string());
                    }
                }
                m_ShowProjectHub = false;
            }

            curY += rowH + rowPad;
        }

        curY += groupGap;
    };

    // Draw Ready projects first, then Missing
    drawProjectGroup("Ready", IM_COL32(80, 200, 120, 200), readyIndices, true);
    drawProjectGroup("Missing", IM_COL32(200, 80, 80, 200), missingIndices, false);

    dl->PopClipRect();
}

// --------------------------------------------------
// Landing page: full-width dashboard with project cards
// --------------------------------------------------
void EditorLayer::DrawHubLandingPage(ImDrawList* dl, const ImVec2& area, f32 /*contentY*/, f32 /*sidebarW*/) {
    ImGuiIO& io = ImGui::GetIO();
    ImFont* font = ImGui::GetFont();

    // Thumbnail color palette for project cards
    static const ImU32 kProjectPalette[] = {
        IM_COL32(45, 65, 120, 255),   // deep blue
        IM_COL32(35, 95, 90, 255),    // teal
        IM_COL32(75, 45, 115, 255),   // purple
        IM_COL32(130, 60, 55, 255),   // coral
        IM_COL32(120, 95, 35, 255),   // amber
        IM_COL32(40, 85, 55, 255),    // forest
        IM_COL32(115, 50, 75, 255),   // rose
        IM_COL32(55, 60, 75, 255),    // slate
    };

    // Helper: simple header button
    auto drawHeaderBtn = [&](f32 x, f32 y, f32 w, f32 h,
                             const char* label, ImU32 bgNormal, ImU32 bgHover) -> bool {
        ImVec2 bMin(x, y), bMax(x + w, y + h);
        bool hovered = (io.MousePos.x >= bMin.x && io.MousePos.x <= bMax.x &&
                       io.MousePos.y >= bMin.y && io.MousePos.y <= bMax.y);
        dl->AddRectFilled(bMin, bMax, hovered ? bgHover : bgNormal, 8.0f);
        f32 fontSize = 24.0f;
        ImVec2 textSz = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, label);
        dl->AddText(nullptr, fontSize,
            ImVec2(x + (w - textSz.x) * 0.5f, y + (h - textSz.y) * 0.5f),
            hovered ? IM_COL32(240, 242, 255, 255) : IM_COL32(200, 205, 220, 230), label);
        return hovered && ImGui::IsMouseClicked(0);
    };

    // ========== 3a. Header Bar ==========
    f32 headerH = 120.0f;
    dl->AddRectFilled(ImVec2(0, 0), ImVec2(area.x, headerH), IM_COL32(15, 16, 22, 255));
    dl->AddLine(ImVec2(0, headerH), ImVec2(area.x, headerH), IM_COL32(50, 55, 70, 180), 1.0f);

    // Left: TEGE branding
    f32 brandFontSize = 64.0f;
    const char* brandText = "TEGE";
    ImVec2 brandSz = font->CalcTextSizeA(brandFontSize, FLT_MAX, 0.0f, brandText);
    f32 brandY = (headerH - brandSz.y) * 0.5f;
    dl->AddText(nullptr, brandFontSize, ImVec2(24.0f, brandY), IM_COL32(199, 218, 196, 255), brandText);

    f32 subFontSize = 24.0f;
    const char* subText = "Game Engine";
    ImVec2 subSz = font->CalcTextSizeA(subFontSize, FLT_MAX, 0.0f, subText);
    dl->AddText(nullptr, subFontSize,
        ImVec2(24.0f + brandSz.x + 14.0f, brandY + brandSz.y - subSz.y - 2.0f),
        IM_COL32(140, 160, 140, 180), subText);

    // Right: action buttons
    f32 btnH = 60.0f, btnGap = 14.0f;
    f32 btnY = (headerH - btnH) * 0.5f;
    f32 btn3W = 200.0f; // Open Scene
    f32 btn2W = 210.0f; // Open Project
    f32 btn1W = 220.0f; // + New Project
    f32 btnRightEdge = area.x - 32.0f;

    // Open Scene (rightmost)
    f32 btn3X = btnRightEdge - btn3W;
    if (drawHeaderBtn(btn3X, btnY, btn3W, btnH, "Open Scene",
                      IM_COL32(30, 34, 48, 255), IM_COL32(45, 52, 72, 255))) {
        std::vector<FileFilter> filters = {{ "Enjin Scene", "*.enjin" }, { "All Files", "*.*" }};
        std::string path = FileDialog::OpenFile("Open Scene", filters);
        if (!path.empty()) {
            m_ShowProjectHub = false;
            OpenScene(path);
        }
    }

    // Open Project
    f32 btn2X = btn3X - btnGap - btn2W;
    if (drawHeaderBtn(btn2X, btnY, btn2W, btnH, "Open Project",
                      IM_COL32(30, 34, 48, 255), IM_COL32(45, 52, 72, 255))) {
        std::vector<FileFilter> filters = {{ "Enjin Project", "*.enjinproject" }, { "All Files", "*.*" }};
        std::string path = FileDialog::OpenFile("Open Project", filters);
        if (!path.empty()) {
            if (m_SceneManager.LoadProject(path)) {
                MigrateEditorSettingsToProject();
                m_EditorSettings.AddRecentProject(path);
                m_EditorSettings.lastProjectDir = std::filesystem::path(path).parent_path().parent_path().string();
                m_EditorSettings.Save();
                auto& scenes = m_SceneManager.GetScenes();
                if (!scenes.empty()) {
                    auto projDir = std::filesystem::path(path).parent_path();
                    OpenScene((projDir / scenes[0].path).string());
                }
                m_ShowProjectHub = false;
            }
        }
    }

    // + New Project (primary accent)
    f32 btn1X = btn2X - btnGap - btn1W;
    if (drawHeaderBtn(btn1X, btnY, btn1W, btnH, "+ New Project",
                      IM_COL32(50, 75, 140, 255), IM_COL32(65, 92, 170, 255))) {
        m_SelectedTemplate = -1;
        m_TemplateFilter = TMPL_ALL;
        m_TemplateStatusFilter = -1;
        m_TemplateSearchBuffer[0] = '\0';
        m_HubPage = HubPage::WizardSetup;
    }

    // ========== 3b. Project Cards Section (100px to area.y-68px) ==========
    f32 bottomBarH = 68.0f;
    f32 cardsTop = headerH;
    f32 cardsBottom = area.y - bottomBarH;

    // Gather projects from recent list
    struct ProjectEntry {
        std::string name;
        std::string parentPath;
        std::string fullPath;
        bool exists;
    };
    std::vector<ProjectEntry> projects;
    for (auto& rp : m_EditorSettings.recentProjects) {
        std::filesystem::path p(rp);
        if (p.extension() == ".enjinproject") {
            ProjectEntry entry;
            entry.name = p.stem().string();
            entry.parentPath = p.parent_path().string();
            entry.fullPath = rp;
            entry.exists = std::filesystem::exists(rp);
            projects.push_back(entry);
        }
    }

    // Apply filter
    std::vector<ProjectEntry*> filtered;
    for (auto& proj : projects) {
        if (m_HubProjectFilter == 1 && !proj.exists) continue;
        if (m_HubProjectFilter == 2 && proj.exists) continue;
        filtered.push_back(&proj);
    }

    // Section header row
    f32 sectionPad = 32.0f;
    f32 sectionHeaderY = cardsTop + 20.0f;

    // "Your Projects" title
    f32 sectionTitleSize = 36.0f;
    dl->AddText(nullptr, sectionTitleSize,
        ImVec2(sectionPad, sectionHeaderY),
        IM_COL32(200, 205, 225, 240), "Your Projects");

    // Filter pills (right-aligned)
    {
        const char* pillLabels[] = { "All", "Ready", "Missing" };
        f32 pillFontSize = 20.0f;
        f32 pillH = 42.0f, pillPad = 22.0f, pillGap = 8.0f;
        f32 pillX = area.x - sectionPad;

        // Measure and draw right-to-left
        for (int pi = 2; pi >= 0; --pi) {
            ImVec2 textSz = font->CalcTextSizeA(pillFontSize, FLT_MAX, 0.0f, pillLabels[pi]);
            f32 pillW = textSz.x + pillPad * 2.0f;
            pillX -= pillW;

            ImVec2 pMin(pillX, sectionHeaderY + 2.0f);
            ImVec2 pMax(pillX + pillW, sectionHeaderY + 2.0f + pillH);
            bool pillHovered = (io.MousePos.x >= pMin.x && io.MousePos.x <= pMax.x &&
                               io.MousePos.y >= pMin.y && io.MousePos.y <= pMax.y);
            bool active = (m_HubProjectFilter == pi);

            ImU32 pillBg = active ? IM_COL32(50, 75, 140, 255)
                         : (pillHovered ? IM_COL32(40, 45, 60, 255) : IM_COL32(28, 32, 44, 255));
            ImU32 pillText = active ? IM_COL32(230, 235, 250, 255) : IM_COL32(150, 155, 175, 200);

            dl->AddRectFilled(pMin, pMax, pillBg, 12.0f);
            if (!active)
                dl->AddRect(pMin, pMax, IM_COL32(55, 60, 78, 150), 12.0f);
            dl->AddText(nullptr, pillFontSize,
                ImVec2(pillX + pillPad, sectionHeaderY + 2.0f + (pillH - textSz.y) * 0.5f),
                pillText, pillLabels[pi]);

            if (pillHovered && ImGui::IsMouseClicked(0))
                m_HubProjectFilter = pi;

            pillX -= pillGap;
        }
    }

    // Card grid area (scrollable child)
    f32 gridTop = sectionHeaderY + 60.0f;
    f32 gridBottom = cardsBottom;

    ImGui::SetCursorScreenPos(ImVec2(0, gridTop));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
    if (ImGui::BeginChild("##HubProjectCards", ImVec2(area.x, gridBottom - gridTop), false,
                           ImGuiWindowFlags_NoBackground)) {
        ImDrawList* cdl = ImGui::GetWindowDrawList();
        ImVec2 scrollOrigin = ImGui::GetCursorScreenPos();

        f32 cardW = 520.0f, cardH = 380.0f, cardGap = 32.0f;
        i32 cardsPerRow = (std::max)(1, static_cast<i32>(std::floor((area.x - sectionPad * 2.0f) / (cardW + cardGap))));
        f32 gridW = cardsPerRow * cardW + (cardsPerRow - 1) * cardGap;
        f32 gridStartX = (area.x - gridW) * 0.5f;

        if (filtered.empty()) {
            // Empty state
            f32 emptyY = scrollOrigin.y + 40.0f;
            f32 emptyFontSize = 26.0f;
            const char* emptyMsg = "No projects yet. Create one to get started!";
            ImVec2 emptySz = font->CalcTextSizeA(emptyFontSize, FLT_MAX, 0.0f, emptyMsg);
            cdl->AddText(nullptr, emptyFontSize,
                ImVec2((area.x - emptySz.x) * 0.5f, emptyY),
                IM_COL32(130, 135, 155, 200), emptyMsg);

            // Dashed "+" placeholder card
            f32 placeholderX = (area.x - cardW) * 0.5f;
            f32 placeholderY = emptyY + 40.0f;
            ImVec2 pMin(placeholderX, placeholderY);
            ImVec2 pMax(placeholderX + cardW, placeholderY + cardH);
            bool phHovered = (io.MousePos.x >= pMin.x && io.MousePos.x <= pMax.x &&
                             io.MousePos.y >= pMin.y && io.MousePos.y <= pMax.y);

            // Dashed border (simulated with dotted rect segments)
            ImU32 dashCol = phHovered ? IM_COL32(80, 110, 180, 200) : IM_COL32(60, 65, 85, 150);
            f32 dashLen = 8.0f, gapLen = 6.0f;
            // Top and bottom edges
            for (f32 dx = 0; dx < cardW; dx += dashLen + gapLen) {
                f32 endX = (std::min)(dx + dashLen, cardW);
                cdl->AddLine(ImVec2(pMin.x + dx, pMin.y), ImVec2(pMin.x + endX, pMin.y), dashCol, 1.5f);
                cdl->AddLine(ImVec2(pMin.x + dx, pMax.y), ImVec2(pMin.x + endX, pMax.y), dashCol, 1.5f);
            }
            // Left and right edges
            for (f32 dy = 0; dy < cardH; dy += dashLen + gapLen) {
                f32 endY = (std::min)(dy + dashLen, cardH);
                cdl->AddLine(ImVec2(pMin.x, pMin.y + dy), ImVec2(pMin.x, pMin.y + endY), dashCol, 1.5f);
                cdl->AddLine(ImVec2(pMax.x, pMin.y + dy), ImVec2(pMax.x, pMin.y + endY), dashCol, 1.5f);
            }

            // "+" text
            f32 plusSize = 80.0f;
            const char* plusText = "+";
            ImVec2 plusSz = font->CalcTextSizeA(plusSize, FLT_MAX, 0.0f, plusText);
            cdl->AddText(nullptr, plusSize,
                ImVec2(placeholderX + (cardW - plusSz.x) * 0.5f,
                       placeholderY + (cardH - plusSz.y) * 0.5f),
                phHovered ? IM_COL32(120, 150, 220, 255) : IM_COL32(80, 90, 115, 180), plusText);

            if (phHovered && ImGui::IsMouseClicked(0)) {
                m_SelectedTemplate = -1;
                m_TemplateFilter = TMPL_ALL;
                m_TemplateStatusFilter = -1;
                m_TemplateSearchBuffer[0] = '\0';
                m_HubPage = HubPage::WizardSetup;
            }

            // Reserve layout space
            ImGui::Dummy(ImVec2(area.x, placeholderY + cardH - scrollOrigin.y + 20.0f));
        } else {
            // Draw project cards in a grid
            i32 totalCards = static_cast<i32>(filtered.size());
            i32 rows = (totalCards + cardsPerRow - 1) / cardsPerRow;

            for (i32 ci = 0; ci < totalCards; ++ci) {
                i32 row = ci / cardsPerRow;
                i32 col = ci % cardsPerRow;
                f32 cx = gridStartX + col * (cardW + cardGap);
                f32 cy = scrollOrigin.y + row * (cardH + cardGap);

                auto* proj = filtered[ci];
                bool missing = !proj->exists;

                // Hit test
                ImVec2 cMin(cx, cy), cMax(cx + cardW, cy + cardH);
                bool hovered = !missing &&
                    (io.MousePos.x >= cMin.x && io.MousePos.x <= cMax.x &&
                     io.MousePos.y >= cMin.y && io.MousePos.y <= cMax.y);

                // Card background
                ImU32 cardBg = hovered ? IM_COL32(32, 38, 55, 255) : IM_COL32(22, 25, 35, 255);
                if (missing) cardBg = IM_COL32(20, 20, 26, 200);
                cdl->AddRectFilled(cMin, cMax, cardBg, 8.0f);

                // Border
                if (hovered)
                    cdl->AddRect(cMin, cMax, IM_COL32(70, 100, 180, 200), 8.0f, 0, 1.5f);
                else
                    cdl->AddRect(cMin, cMax, IM_COL32(40, 45, 60, missing ? 80u : 150u), 8.0f);

                // Thumbnail area (top 100px)
                f32 thumbH = 240.0f;
                ImVec2 tMin(cx + 1, cy + 1);
                ImVec2 tMax(cx + cardW - 1, cy + thumbH);

                // Deterministic color from project name
                u32 hash = 0;
                for (char c : proj->name) hash = hash * 31 + static_cast<u32>(c);
                ImU32 thumbCol = kProjectPalette[hash % 8];
                if (missing) {
                    // Dim the thumbnail color for missing projects
                    u32 r = (thumbCol >> 0) & 0xFF;
                    u32 g = (thumbCol >> 8) & 0xFF;
                    u32 b = (thumbCol >> 16) & 0xFF;
                    thumbCol = IM_COL32(r / 2, g / 2, b / 2, 180);
                } else if (hovered) {
                    // Brighten slightly on hover
                    u32 r = (std::min)(255u, ((thumbCol >> 0) & 0xFF) + 15u);
                    u32 g = (std::min)(255u, ((thumbCol >> 8) & 0xFF) + 15u);
                    u32 b = (std::min)(255u, ((thumbCol >> 16) & 0xFF) + 15u);
                    thumbCol = IM_COL32(r, g, b, 255);
                }
                cdl->AddRectFilled(tMin, tMax, thumbCol, 8.0f, ImDrawFlags_RoundCornersTop);

                // Project initials (2 uppercase chars)
                std::string initials;
                if (!proj->name.empty()) {
                    initials += static_cast<char>(std::toupper(proj->name[0]));
                    // Find second word or second capital
                    for (usize si = 1; si < proj->name.size(); ++si) {
                        if (proj->name[si - 1] == ' ' || proj->name[si - 1] == '_' || proj->name[si - 1] == '-') {
                            if (si < proj->name.size()) {
                                initials += static_cast<char>(std::toupper(proj->name[si]));
                                break;
                            }
                        } else if (std::isupper(proj->name[si]) && initials.size() == 1) {
                            initials += proj->name[si];
                            break;
                        }
                    }
                    if (initials.size() == 1 && proj->name.size() > 1)
                        initials += static_cast<char>(std::toupper(proj->name[1]));
                }
                f32 initialsFontSize = 80.0f;
                ImVec2 initSz = font->CalcTextSizeA(initialsFontSize, FLT_MAX, 0.0f, initials.c_str());
                ImU32 initCol = missing ? IM_COL32(200, 200, 210, 100) : IM_COL32(255, 255, 255, 220);
                cdl->AddText(nullptr, initialsFontSize,
                    ImVec2(cx + (cardW - initSz.x) * 0.5f, cy + (thumbH - initSz.y) * 0.5f),
                    initCol, initials.c_str());

                // "Missing" badge overlay
                if (missing) {
                    f32 badgeFontSize = 16.0f;
                    const char* badgeText = "Missing";
                    ImVec2 badgeSz = font->CalcTextSizeA(badgeFontSize, FLT_MAX, 0.0f, badgeText);
                    f32 badgePad = 6.0f;
                    f32 badgeX = cx + cardW - badgeSz.x - badgePad * 2.0f - 6.0f;
                    f32 badgeY = cy + 6.0f;
                    cdl->AddRectFilled(ImVec2(badgeX, badgeY),
                        ImVec2(badgeX + badgeSz.x + badgePad * 2.0f, badgeY + badgeSz.y + badgePad),
                        IM_COL32(160, 50, 50, 220), 4.0f);
                    cdl->AddText(nullptr, badgeFontSize,
                        ImVec2(badgeX + badgePad, badgeY + badgePad * 0.5f),
                        IM_COL32(240, 200, 200, 255), badgeText);
                }

                // Info area (bottom 80px)
                f32 infoY = cy + thumbH + 14.0f;
                f32 infoTextPad = 16.0f;

                // Project name
                f32 nameFontSize = 26.0f;
                std::string nameClipped = EllipsizeText(proj->name.c_str(), cardW - infoTextPad * 2.0f, font, nameFontSize);
                ImU32 nameCol = missing ? IM_COL32(140, 140, 150, 160) : IM_COL32(210, 215, 235, 255);
                cdl->AddText(nullptr, nameFontSize,
                    ImVec2(cx + infoTextPad, infoY), nameCol, nameClipped.c_str());

                // Parent path + status dot
                f32 pathFontSize = 19.0f;
                f32 pathY = infoY + nameFontSize + 8.0f;
                f32 dotR = 5.0f;
                f32 dotX = cx + cardW - infoTextPad - dotR;
                f32 dotY = pathY + pathFontSize * 0.5f;

                // Status dot
                ImU32 dotCol = missing ? IM_COL32(200, 70, 70, 220) : IM_COL32(70, 200, 110, 220);
                cdl->AddCircleFilled(ImVec2(dotX, dotY), dotR, dotCol);

                // Path text (clipped to leave room for dot)
                f32 pathMaxW = cardW - infoTextPad * 2.0f - dotR * 2.0f - 8.0f;
                std::string parentDir = std::filesystem::path(proj->parentPath).parent_path().string();
                std::string pathClipped = EllipsizeText(parentDir.c_str(), pathMaxW, font, pathFontSize);
                cdl->AddText(nullptr, pathFontSize,
                    ImVec2(cx + infoTextPad, pathY),
                    IM_COL32(100, 105, 125, missing ? 120u : 180u), pathClipped.c_str());

                // Click to open
                if (hovered && ImGui::IsMouseClicked(0)) {
                    if (m_SceneManager.LoadProject(proj->fullPath)) {
                        MigrateEditorSettingsToProject();
                        m_EditorSettings.AddRecentProject(proj->fullPath);
                        m_EditorSettings.lastProjectDir = std::filesystem::path(proj->fullPath).parent_path().parent_path().string();
                        m_EditorSettings.Save();
                        auto& scenes = m_SceneManager.GetScenes();
                        if (!scenes.empty()) {
                            auto projDir = std::filesystem::path(proj->fullPath).parent_path();
                            OpenScene((projDir / scenes[0].path).string());
                        }
                        m_ShowProjectHub = false;
                    }
                }
            }

            // Reserve layout space for scrolling
            ImGui::Dummy(ImVec2(area.x, rows * (cardH + cardGap) + 20.0f));
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();

    // ========== 3c. Bottom Bar (last 68px) ==========
    f32 barY = area.y - bottomBarH;
    dl->AddLine(ImVec2(0, barY), ImVec2(area.x, barY), IM_COL32(50, 55, 70, 150), 1.0f);

    f32 linkFontSize = 22.0f;
    f32 linkY = barY + (bottomBarH - linkFontSize) * 0.5f;

    // Left: Demos link
    const char* demosText = "Demos";
    ImVec2 demosSz = font->CalcTextSizeA(linkFontSize, FLT_MAX, 0.0f, demosText);
    ImVec2 demosPos(sectionPad, linkY);
    bool demosHovered = (io.MousePos.x >= demosPos.x && io.MousePos.x <= demosPos.x + demosSz.x &&
                        io.MousePos.y >= demosPos.y && io.MousePos.y <= demosPos.y + demosSz.y);
    dl->AddText(nullptr, linkFontSize, demosPos,
        demosHovered ? IM_COL32(180, 185, 210, 255) : IM_COL32(120, 125, 150, 200), demosText);
    if (demosHovered)
        dl->AddLine(ImVec2(demosPos.x, demosPos.y + demosSz.y + 1.0f),
                    ImVec2(demosPos.x + demosSz.x, demosPos.y + demosSz.y + 1.0f),
                    IM_COL32(180, 185, 210, 200));
    if (demosHovered && ImGui::IsMouseClicked(0)) {
        m_DemosCacheValid = false;
        m_HubPage = HubPage::Demos;
    }

    // Template Marketplace link
    f32 marketX = demosPos.x + demosSz.x + 24.0f;
    const char* marketText = "Template Marketplace";
    ImVec2 marketSz = font->CalcTextSizeA(linkFontSize, FLT_MAX, 0.0f, marketText);
    ImVec2 marketPos(marketX, linkY);
    bool marketHovered = (io.MousePos.x >= marketPos.x && io.MousePos.x <= marketPos.x + marketSz.x &&
                         io.MousePos.y >= marketPos.y && io.MousePos.y <= marketPos.y + marketSz.y);
    dl->AddText(nullptr, linkFontSize, marketPos,
        marketHovered ? IM_COL32(180, 185, 210, 255) : IM_COL32(120, 125, 150, 200), marketText);
    if (marketHovered)
        dl->AddLine(ImVec2(marketPos.x, marketPos.y + marketSz.y + 1.0f),
                    ImVec2(marketPos.x + marketSz.x, marketPos.y + marketSz.y + 1.0f),
                    IM_COL32(180, 185, 210, 200));
    if (marketHovered && ImGui::IsMouseClicked(0)) {
        m_TemplateMarketplace.SetOpen(true);
    }

    // Right: Skip to Empty Scene
    const char* skipText = "Skip to Empty Scene";
    ImVec2 skipSz = font->CalcTextSizeA(linkFontSize, FLT_MAX, 0.0f, skipText);
    ImVec2 skipPos(area.x - skipSz.x - sectionPad, linkY);
    bool skipHovered = (io.MousePos.x >= skipPos.x && io.MousePos.x <= skipPos.x + skipSz.x &&
                       io.MousePos.y >= skipPos.y && io.MousePos.y <= skipPos.y + skipSz.y);
    dl->AddText(nullptr, linkFontSize, skipPos,
        skipHovered ? IM_COL32(180, 185, 210, 255) : IM_COL32(120, 125, 150, 200), skipText);
    if (skipHovered)
        dl->AddLine(ImVec2(skipPos.x, skipPos.y + skipSz.y + 1.0f),
                    ImVec2(skipPos.x + skipSz.x, skipPos.y + skipSz.y + 1.0f),
                    IM_COL32(180, 185, 210, 200));
    if (skipHovered && ImGui::IsMouseClicked(0)) {
        m_ShowProjectHub = false;
    }
}

// --------------------------------------------------
// Wizard Step 1: project setup (name, location, scene, git)
// --------------------------------------------------
void EditorLayer::DrawHubWizardSetup(ImDrawList* dl, const ImVec2& area, f32 contentY, f32 sidebarW) {
    ImGuiIO& io = ImGui::GetIO();
    ImFont* font = ImGui::GetFont();

    f32 contentW = area.x - sidebarW;
    f32 formW = (std::min)(600.0f, contentW - 80.0f);
    f32 formX = sidebarW + (contentW - formW) * 0.5f;

    // Step indicator
    f32 stepFontSize = 18.0f;
    const char* stepText = "Step 1 of 2 -- Setup";
    ImVec2 stepSz = font->CalcTextSizeA(stepFontSize, FLT_MAX, 0.0f, stepText);
    f32 stepX = sidebarW + (contentW - stepSz.x) * 0.5f;
    dl->AddText(nullptr, stepFontSize, ImVec2(stepX, contentY + 10.0f),
        IM_COL32(140, 150, 170, 200), stepText);

    f32 y = contentY + 10.0f + stepSz.y + 30.0f;

    // Form fields
    ImGui::SetCursorPos(ImVec2(formX, y));
    ImGui::PushItemWidth(formW - 110.0f);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.14f, 0.15f, 0.19f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.87f, 0.92f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_TextDisabled, ImVec4(0.45f, 0.48f, 0.55f, 1.0f));

    // Project Name
    ImGui::TextColored(ImVec4(0.55f, 0.58f, 0.68f, 1.0f), "Project Name");
    ImGui::SetCursorPosX(formX);
    ImGui::InputText("##ProjName", m_NewProjectName, sizeof(m_NewProjectName));

    // Location + Browse
    ImGui::SetCursorPosX(formX);
    ImGui::TextColored(ImVec4(0.55f, 0.58f, 0.68f, 1.0f), "Location");
    ImGui::SetCursorPosX(formX);
    ImGui::PushItemWidth(formW - 190.0f);
    ImGui::InputText("##ProjPath", m_NewProjectPath, sizeof(m_NewProjectPath));
    ImGui::PopItemWidth();
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.2f, 0.26f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.28f, 0.35f, 1.0f));
    if (ImGui::Button("Browse...", ImVec2(70, 0))) {
        std::string folder = FileDialog::OpenFolder("Select Project Location", m_NewProjectPath);
        if (!folder.empty()) {
            std::strncpy(m_NewProjectPath, folder.c_str(), sizeof(m_NewProjectPath) - 1);
            m_NewProjectPath[sizeof(m_NewProjectPath) - 1] = '\0';
        }
    }
    ImGui::PopStyleColor(2);

    // Scene Name
    ImGui::SetCursorPosX(formX);
    ImGui::TextColored(ImVec4(0.55f, 0.58f, 0.68f, 1.0f), "First Scene Name");
    ImGui::SetCursorPosX(formX);
    ImGui::PushItemWidth(formW - 110.0f);
    ImGui::InputText("##SceneName", m_NewSceneName, sizeof(m_NewSceneName));
    ImGui::PopItemWidth();

    // Preview path
    std::string projNameStr(m_NewProjectName);
    std::string projPathStr(m_NewProjectPath);
    std::string previewPath = projPathStr + "/" + projNameStr + "/";
    ImGui::SetCursorPosX(formX);
    ImGui::TextColored(ImVec4(0.40f, 0.45f, 0.55f, 1.0f), "Project folder: %s", previewPath.c_str());

    // Git init checkbox
    ImGui::SetCursorPosX(formX);
    ImGui::Spacing();
    ImGui::SetCursorPosX(formX);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.14f, 0.15f, 0.19f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_CheckMark, ImVec4(0.4f, 0.75f, 0.4f, 1.0f));
    ImGui::Checkbox("Initialize Git repository", &m_GitInitOnCreate);
    ImGui::PopStyleColor(2);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Creates a .git folder and .gitignore file");
    }

    ImGui::PopItemWidth();
    ImGui::PopStyleColor(3); // FrameBg, Text, TextDisabled

    // --- Bottom bar: Back + Next ---
    f32 bottomY = area.y - 85.0f;
    dl->AddLine(ImVec2(sidebarW + 30.0f, bottomY - 12.0f), ImVec2(area.x - 30.0f, bottomY - 12.0f),
        IM_COL32(60, 65, 80, 150), 1.0f);

    bool canNext = (std::strlen(m_NewProjectName) > 0 && std::strlen(m_NewProjectPath) > 0 &&
                   std::strlen(m_NewSceneName) > 0);

    // "< Back" link
    f32 backFontSize = 20.0f;
    const char* backText = "< Back";
    ImVec2 backSz = font->CalcTextSizeA(backFontSize, FLT_MAX, 0.0f, backText);
    ImVec2 backPos(sidebarW + 40.0f, bottomY + 20.0f);
    bool backHovered = (io.MousePos.x >= backPos.x && io.MousePos.x <= backPos.x + backSz.x &&
                       io.MousePos.y >= backPos.y && io.MousePos.y <= backPos.y + backSz.y);
    dl->AddText(nullptr, backFontSize, backPos,
        backHovered ? IM_COL32(180, 185, 205, 255) : IM_COL32(120, 125, 145, 200), backText);
    if (backHovered && ImGui::IsMouseClicked(0)) {
        m_HubPage = HubPage::Landing;
    }

    // "Next >" button
    f32 nextFontSize = 24.0f;
    f32 nextBtnW = 200.0f, nextBtnH = 56.0f;
    ImVec2 nextPos(area.x - nextBtnW - 40.0f, bottomY);
    ImVec2 nextEnd(nextPos.x + nextBtnW, nextPos.y + nextBtnH);
    bool nextHovered = canNext && (io.MousePos.x >= nextPos.x && io.MousePos.x <= nextEnd.x &&
                                   io.MousePos.y >= nextPos.y && io.MousePos.y <= nextEnd.y);

    ImU32 nextBg = !canNext ? IM_COL32(35, 38, 48, 200) :
                   (nextHovered ? IM_COL32(60, 90, 160, 255) : IM_COL32(50, 70, 130, 255));
    dl->AddRectFilled(nextPos, nextEnd, nextBg, 10.0f);
    if (canNext) {
        dl->AddRect(nextPos, nextEnd, IM_COL32(80, 110, 180, 200), 10.0f);
    }

    const char* nextText = "Next >";
    ImVec2 nextSz = font->CalcTextSizeA(nextFontSize, FLT_MAX, 0.0f, nextText);
    ImU32 nextTextCol = canNext ? IM_COL32(220, 225, 245, 255) : IM_COL32(100, 105, 120, 150);
    dl->AddText(nullptr, nextFontSize,
        ImVec2(nextPos.x + (nextBtnW - nextSz.x) * 0.5f, nextPos.y + (nextBtnH - nextSz.y) * 0.5f),
        nextTextCol, nextText);

    if (nextHovered && ImGui::IsMouseClicked(0)) {
        m_HubPage = HubPage::WizardTemplate;
    }
}

// --------------------------------------------------
// Shared template data for wizard + hover preview
// --------------------------------------------------
namespace {
    constexpr u32 kTMPL_ALL   = 0;
    constexpr u32 kTMPL_2D    = 1 << 0;
    constexpr u32 kTMPL_3D    = 1 << 1;
    constexpr u32 kTMPL_MULTI = 1 << 2;

    struct HubTemplateInfo {
        const char* id;
        const char* name;
        const char* description;
        ImVec4 accentColor;
        u32 categoryFlags;
        Editor::MaturityTier maturity;
    };

    static const HubTemplateInfo s_BuiltinTemplates[] = {
        // -- Foundations (Stable) --
        { "blank",        "Blank",              "Empty scene\nStart from scratch",                       ImVec4(0.5f, 0.5f, 0.5f, 1.0f), kTMPL_ALL, Editor::MaturityTier::Stable },
        { "platformer",   "2D Platformer",      "4-zone adventure\nMeadow + cave + tower + sky boss",    ImVec4(0.3f, 0.8f, 0.3f, 1.0f), kTMPL_2D, Editor::MaturityTier::Stable },
        { "topdown2d",    "2D Top-Down Action", "Dungeon action\nMulti-room + enemies + HUD + particles",  ImVec4(0.3f, 0.6f, 0.9f, 1.0f), kTMPL_2D, Editor::MaturityTier::Stable },
        { "thirdperson",  "3D Third Person",    "Over-the-shoulder\nShadows + obstacles + point light",  ImVec4(0.8f, 0.3f, 0.3f, 1.0f), kTMPL_3D, Editor::MaturityTier::Stable },
        { "firstperson",  "3D First Person",    "Eye-level FPS\nCorridor walls + warm lighting",         ImVec4(0.7f, 0.3f, 0.8f, 1.0f), kTMPL_3D, Editor::MaturityTier::Stable },
        // -- Genre Showcases --
        { "puzzle",       "Sokoban Puzzle",     "Pushable blocks\nGoal plates + switches + grid snap",   ImVec4(0.4f, 0.8f, 0.9f, 1.0f), kTMPL_3D, Editor::MaturityTier::Stable },
        { "survival",     "Survival",           "Survive the cold\nTemperature + weather + stamina",     ImVec4(0.7f, 0.5f, 0.2f, 1.0f), kTMPL_3D, Editor::MaturityTier::Beta },
        { "rpg_village",  "RPG Village",        "Talk to NPCs\nDialogue + pickups + lantern",            ImVec4(0.3f, 0.6f, 0.3f, 1.0f), kTMPL_3D, Editor::MaturityTier::Beta },
        { "horror",       "Horror",             "Dark atmosphere\nFlashlight + fog + proximity door",    ImVec4(0.3f, 0.1f, 0.3f, 1.0f), kTMPL_3D, Editor::MaturityTier::Beta },
        { "racing",       "Vehicle Racing",     "Split-screen race\nVehicle + checkpoints + cinematic",  ImVec4(0.9f, 0.25f, 0.1f, 1.0f), kTMPL_MULTI, Editor::MaturityTier::Beta },
        { "ps1rpg",       "PS1 RPG",            "Retro 3D RPG\nPixelation + dither + save point",       ImVec4(0.2f, 0.2f, 0.8f, 1.0f), kTMPL_3D, Editor::MaturityTier::Beta },
        { "arena",        "Arena Fighter",      "2-player brawl\nSplitscreen + health + stamina",       ImVec4(1.0f, 0.5f, 0.0f, 1.0f), kTMPL_MULTI, Editor::MaturityTier::Beta },
        // -- Systems Deep-Dives --
        { "physics",      "Physics Playground", "Rigidbody sandbox\nGravity zones + conveyors + ramps",  ImVec4(0.3f, 0.75f, 0.7f, 1.0f), kTMPL_3D, Editor::MaturityTier::Stable },
        { "narrative",    "Dialogue & Narrative","NPC conversations\nQuests + dialogue box + branching",  ImVec4(0.7f, 0.6f, 0.85f, 1.0f), kTMPL_3D, Editor::MaturityTier::Beta },
        { "savesystem",   "Save System Demo",   "3-tier persistence\nCheckpoints + meta + save menu",   ImVec4(0.4f, 0.55f, 0.75f, 1.0f), kTMPL_3D, Editor::MaturityTier::Beta },
        { "visualscript", "Visual Scripting",   "Node-based logic\nSwitch triggers + particle events",  ImVec4(0.85f, 0.7f, 0.2f, 1.0f), kTMPL_3D, Editor::MaturityTier::Preview },
        { "uicanvas",     "UI Canvas Demo",     "In-game UI\nButtons + health bar + HUD overlay",       ImVec4(0.8f, 0.3f, 0.7f, 1.0f), kTMPL_3D, Editor::MaturityTier::Beta },
        { "accessibility","Accessibility Menu","Settings menu\nSubtitles + colorblind + focus nav",    ImVec4(0.3f, 0.75f, 0.9f, 1.0f), kTMPL_ALL, Editor::MaturityTier::Beta },
        // -- Retro & Flash --
        { "pointclick",   "Point & Click",      "Adventure game\nClick hotspots + inventory + dialogue", ImVec4(1.0f, 0.55f, 0.2f, 1.0f), kTMPL_2D, Editor::MaturityTier::Beta },
        { "bullethell",   "Bullet Hell",        "Danmaku shmup\nObject pool + particles + health",      ImVec4(0.95f, 0.2f, 0.5f, 1.0f), kTMPL_2D, Editor::MaturityTier::Beta },
        { "idleclicker",  "Idle/Clicker",       "Incremental game\nUI canvas + meta saves + tweens",    ImVec4(0.4f, 0.8f, 0.4f, 1.0f), kTMPL_2D, Editor::MaturityTier::Beta },
        // -- Advanced --
        { "planetgravity","Planet Gravity",     "Spherical gravity\nWalk on a planet surface",           ImVec4(0.3f, 0.6f, 0.95f, 1.0f), kTMPL_3D, Editor::MaturityTier::Beta },
        { "dungeon",      "Dungeon Crawler",    "Grid-based FPS\nSnap turns + enemies + dark corridors", ImVec4(0.15f, 0.5f, 0.15f, 1.0f), kTMPL_3D, Editor::MaturityTier::Preview },
        // -- Restored: Genre Showcases --
        { "isometric",   "3D Isometric",    "45-degree CRPG\nPerspective + player",               ImVec4(0.9f, 0.6f, 0.2f, 1.0f), kTMPL_3D, Editor::MaturityTier::Beta },
        { "visualnovel", "Visual Novel",     "Story-driven\nDialogue + sprites",                   ImVec4(0.9f, 0.7f, 0.9f, 1.0f), kTMPL_ALL, Editor::MaturityTier::Beta },
        { "gamemanager", "Game Manager",    "Singleton pattern\nScore + state machine",            ImVec4(0.6f, 0.6f, 0.8f, 1.0f), kTMPL_ALL, Editor::MaturityTier::Beta },
        { "citybuilder", "City Builder",   "Isometric city sim\n3D or faux-iso mode",             ImVec4(0.2f, 0.7f, 0.7f, 1.0f), kTMPL_3D, Editor::MaturityTier::Beta },
        { "fpsarena",    "FPS Arena",      "First-person shooter\nWeapons + respawn + ammo",       ImVec4(0.9f, 0.2f, 0.2f, 1.0f), kTMPL_3D, Editor::MaturityTier::Beta },
        { "teamsports",  "Team Sports",    "3D soccer/basketball\n2 teams + ball + goals",         ImVec4(0.2f, 0.8f, 0.3f, 1.0f), kTMPL_3D, Editor::MaturityTier::Beta },
        { "towerdefense","Tower Defense",  "Isometric TD\nPaths + turrets + waves",                ImVec4(0.8f, 0.6f, 0.2f, 1.0f), kTMPL_3D, Editor::MaturityTier::Beta },
        { "runner",      "Endless Runner", "Auto-scroll\nLanes + obstacles + score",               ImVec4(0.9f, 0.6f, 0.1f, 1.0f), kTMPL_2D, Editor::MaturityTier::Beta },
        { "flower",      "Flower Garden", "Procedural flower\nPluckable petals + score",             ImVec4(0.9f, 0.4f, 0.6f, 1.0f), kTMPL_3D, Editor::MaturityTier::Stable },
        { "fixedcam",    "Fixed Camera",  "Fixed-angle 3rd person\nClassic RE / God of War",           ImVec4(0.6f, 0.25f, 0.5f, 1.0f), kTMPL_3D, Editor::MaturityTier::Beta },
        { "metroidvania","2D Metroidvania","Interconnected map\nAbilities + locked doors",             ImVec4(0.4f, 0.2f, 0.7f, 1.0f), kTMPL_2D, Editor::MaturityTier::Beta },
        { "soulslike",   "3D Souls-like", "Challenging melee\nBonfires + stamina + fog gates",        ImVec4(0.5f, 0.15f, 0.1f, 1.0f), kTMPL_3D, Editor::MaturityTier::Beta },
        { "vampsurvivor","Survivor-like", "Top-down auto-attack\nWaves + XP + level-up",              ImVec4(0.1f, 0.7f, 0.5f, 1.0f), kTMPL_2D, Editor::MaturityTier::Beta },
        { "roguelike",   "2D Rogue-like", "Grid-based dungeon\nRandom rooms + permadeath",            ImVec4(0.6f, 0.5f, 0.1f, 1.0f), kTMPL_2D, Editor::MaturityTier::Preview },
        // -- Restored: Multiplayer --
        { "couchcoop",   "2P Couch Co-op","Splitscreen co-op\n2 players + shared world",              ImVec4(0.8f, 0.4f, 0.2f, 1.0f), kTMPL_MULTI, Editor::MaturityTier::Beta },
        { "justtwo",     "Just the Two of Us","Co-op puzzles + physics\nIt Takes Two inspired",       ImVec4(0.85f, 0.45f, 0.65f, 1.0f), kTMPL_MULTI, Editor::MaturityTier::Beta },
        // -- Restored: Debug/Test --
        { "shadowtest",  "Shadow Test",   "Shadow debug scene\nGround + objects + light",             ImVec4(0.9f, 0.9f, 0.3f, 1.0f), kTMPL_3D, Editor::MaturityTier::Stable },
        // -- Restored: Retro & Flash --
        { "flash_td",    "Flash TD",      "Classic Flash TD\nPath + towers + waves",                 ImVec4(0.8f, 0.6f, 0.2f, 1.0f), kTMPL_2D, Editor::MaturityTier::Experimental },
        { "flash_dress", "Dress Up",      "Character dress-up\nDrag items + layers + save",          ImVec4(0.9f, 0.6f, 0.9f, 1.0f), kTMPL_2D, Editor::MaturityTier::Experimental },
        { "flash_escape","Escape Room",   "Room escape puzzle\nInventory + clues + combinations",    ImVec4(0.5f, 0.3f, 0.2f, 1.0f), kTMPL_2D, Editor::MaturityTier::Experimental },
        { "flash_rhythm","Rhythm Game",   "Music game\nNotes + timing + combo",                      ImVec4(0.3f, 0.4f, 0.9f, 1.0f), kTMPL_2D, Editor::MaturityTier::Experimental },
        // -- Marketplace-only (mirrored for unified master list) --
        { "hello_sprite",        "Hello Sprite",         "Animated sprite\nBasic movement + atlas",                  ImVec4(0.4f, 0.8f, 0.4f, 1.0f), kTMPL_2D, Editor::MaturityTier::Stable },
        { "neon_runner",         "Neon Runner",          "Synthwave runner\nProcedural obstacles + score",           ImVec4(0.9f, 0.2f, 0.9f, 1.0f), kTMPL_2D, Editor::MaturityTier::Beta },
        { "cozy_farm",           "Cozy Farm",            "Farming sim\nCrops + day-night + dialogue",               ImVec4(0.4f, 0.7f, 0.3f, 1.0f), kTMPL_2D, Editor::MaturityTier::Beta },
        { "networking_lobby",    "Multiplayer Lobby",    "LAN multiplayer\nLobby + entity sync + RPC",              ImVec4(0.1f, 0.6f, 0.7f, 1.0f), kTMPL_3D, Editor::MaturityTier::Preview },
        { "ps1_horror",          "PS1 Horror",           "PS1-era horror\nVertex jitter + CRT + fixed cam",         ImVec4(0.1f, 0.15f, 0.1f, 1.0f), kTMPL_3D, Editor::MaturityTier::Beta },
        { "ray_tracing_showcase","Ray Tracing Showcase", "RT reflections\nSoft shadows + AO + SVGF",               ImVec4(1.0f, 0.85f, 0.4f, 1.0f), kTMPL_3D, Editor::MaturityTier::Experimental },
        { "procedural_world",   "Procedural World",     "Fractal terrain\nErosion + L-system + WFC",               ImVec4(0.3f, 0.8f, 0.5f, 1.0f), kTMPL_3D, Editor::MaturityTier::Preview },
    };
    constexpr int s_BuiltinCount = 51;
} // anonymous namespace

// Draw a procedural mini-preview for template cards (first 4 templates get custom art)
static void DrawTemplateThumbnail(ImDrawList* dl, const char* templateId, ImVec2 tMin, ImVec2 tMax,
                                   const ImVec4& accent, bool hovered) {
    // Simple solid color fill using the template's accent color
    u8 r = static_cast<u8>(accent.x * 255);
    u8 g = static_cast<u8>(accent.y * 255);
    u8 b = static_cast<u8>(accent.z * 255);
    u8 a = hovered ? 220 : 180;
    dl->AddRectFilled(tMin, tMax, IM_COL32(r, g, b, a), 4.0f);

    // Subtle bottom edge fade-out
    dl->AddRectFilledMultiColor(
        ImVec2(tMin.x, tMax.y - 8), tMax,
        IM_COL32(0, 0, 0, 0), IM_COL32(0, 0, 0, 0),
        IM_COL32(20, 22, 30, 200), IM_COL32(20, 22, 30, 200));
}

// --------------------------------------------------
// Wizard Step 2: template selection
// --------------------------------------------------
void EditorLayer::DrawHubWizardTemplate(ImDrawList* dl, const ImVec2& area, f32 contentY, f32 sidebarW) {
    ImGuiIO& io = ImGui::GetIO();
    ImFont* font = ImGui::GetFont();

    f32 contentW = area.x - sidebarW;

    // Step indicator
    f32 stepFontSize = 22.0f;
    const char* stepText = "Step 2 of 2 -- Choose Template";
    ImVec2 stepSz = font->CalcTextSizeA(stepFontSize, FLT_MAX, 0.0f, stepText);
    f32 stepX = sidebarW + (contentW - stepSz.x) * 0.5f;
    dl->AddText(nullptr, stepFontSize, ImVec2(stepX, contentY + 10.0f),
        IM_COL32(140, 150, 170, 200), stepText);

    f32 gridAreaStartY = contentY + 10.0f + stepSz.y + 16.0f;

    // === Template search bar ===
    f32 formW = (std::min)(600.0f, contentW - 80.0f);
    f32 formX = sidebarW + (contentW - formW) * 0.5f;

    ImGui::SetCursorPos(ImVec2(formX, gridAreaStartY));
    ImGui::TextColored(ImVec4(0.55f, 0.58f, 0.68f, 1.0f), "Search Templates");
    ImGui::SetCursorPosX(formX);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.14f, 0.15f, 0.19f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.87f, 0.92f, 1.0f));
    ImGui::PushItemWidth(formW - 110.0f);
    ImGui::InputTextWithHint("##TemplateSearch", "Type to filter...", m_TemplateSearchBuffer, sizeof(m_TemplateSearchBuffer));
    ImGui::PopItemWidth();
    ImGui::PopStyleColor(2);

    // === Category filter chips ===
    f32 filterY = ImGui::GetCursorPosY() + 8.0f;
    const char* filterLabels[] = { "All", "2D", "3D", "Multiplayer" };
    u32 filterValues[] = { TMPL_ALL, TMPL_2D, TMPL_3D, TMPL_MULTI };

    f32 chipFontSize = 26.0f;
    f32 chipPad = 12.0f;
    f32 chipH = 56.0f;
    f32 chipPadX = 28.0f;
    ImVec2 chipTextSizes[4];
    f32 totalChipW = 0.0f;
    for (int f = 0; f < 4; ++f) {
        chipTextSizes[f] = font->CalcTextSizeA(chipFontSize, FLT_MAX, 0.0f, filterLabels[f]);
        totalChipW += chipTextSizes[f].x + chipPadX * 2.0f + chipPad;
    }
    totalChipW -= chipPad;

    // Shrink chips if wider than content area
    f32 chipAvailW = contentW - 40.0f;
    if (totalChipW > chipAvailW) {
        f32 scale = chipAvailW / totalChipW;
        chipFontSize *= scale;
        chipPadX *= scale;
        chipPad *= scale;
        chipH *= scale;
        totalChipW = 0.0f;
        for (int f = 0; f < 4; ++f) {
            chipTextSizes[f] = font->CalcTextSizeA(chipFontSize, FLT_MAX, 0.0f, filterLabels[f]);
            totalChipW += chipTextSizes[f].x + chipPadX * 2.0f + chipPad;
        }
        totalChipW -= chipPad;
    }

    f32 chipX = sidebarW + (contentW - totalChipW) * 0.5f;
    for (int f = 0; f < 4; ++f) {
        f32 chipW = chipTextSizes[f].x + chipPadX * 2.0f;
        ImVec2 cPos(chipX, filterY);
        ImVec2 cEnd(chipX + chipW, filterY + chipH);

        bool isActive = (m_TemplateFilter == filterValues[f]);
        bool hovered = (io.MousePos.x >= cPos.x && io.MousePos.x <= cEnd.x &&
                       io.MousePos.y >= cPos.y && io.MousePos.y <= cEnd.y);

        ImU32 chipBg = isActive ? IM_COL32(60, 80, 140, 255) :
                       (hovered ? IM_COL32(45, 50, 70, 255) : IM_COL32(30, 33, 42, 255));
        dl->AddRectFilled(cPos, cEnd, chipBg, chipH * 0.5f);
        if (isActive) {
            dl->AddRect(cPos, cEnd, IM_COL32(100, 130, 200, 200), chipH * 0.5f);
        }

        dl->AddText(nullptr, chipFontSize,
            ImVec2(cPos.x + (chipW - chipTextSizes[f].x) * 0.5f,
                   cPos.y + (chipH - chipTextSizes[f].y) * 0.5f),
            isActive ? IM_COL32(220, 225, 245, 255) : IM_COL32(150, 155, 175, 200),
            filterLabels[f]);

        if (hovered && ImGui::IsMouseClicked(0)) {
            m_TemplateFilter = filterValues[f];
        }

        chipX += chipW + chipPad;
    }

    // === Status (maturity) filter chips ===
    f32 statusFilterY = filterY + chipH + 10.0f;
    const char* statusLabels[] = { "All Status", "Stable", "Beta", "Preview", "Experimental" };
    i32 statusValues[] = { -1, 0, 1, 2, 3 };
    ImU32 statusColors[] = {
        IM_COL32(160, 165, 185, 200),  // All — neutral
        IM_COL32(80, 140, 220, 255),   // Stable — blue
        IM_COL32(80, 180, 80, 255),    // Beta — green
        IM_COL32(210, 170, 50, 255),   // Preview — amber
        IM_COL32(210, 70, 70, 255),    // Experimental — red
    };

    f32 sChipFontSize = 18.0f;
    f32 sChipPad = 10.0f;
    f32 sChipH = 36.0f;
    f32 sChipPadX = 20.0f;
    ImVec2 sChipTextSizes[5];
    f32 sTotalChipW = 0.0f;
    for (int f = 0; f < 5; ++f) {
        sChipTextSizes[f] = font->CalcTextSizeA(sChipFontSize, FLT_MAX, 0.0f, statusLabels[f]);
        sTotalChipW += sChipTextSizes[f].x + sChipPadX * 2.0f + sChipPad;
    }
    sTotalChipW -= sChipPad;

    // Shrink if wider than content area
    f32 sChipAvailW = contentW - 40.0f;
    if (sTotalChipW > sChipAvailW) {
        f32 scale = sChipAvailW / sTotalChipW;
        sChipFontSize *= scale;
        sChipPadX *= scale;
        sChipPad *= scale;
        sChipH *= scale;
        sTotalChipW = 0.0f;
        for (int f = 0; f < 5; ++f) {
            sChipTextSizes[f] = font->CalcTextSizeA(sChipFontSize, FLT_MAX, 0.0f, statusLabels[f]);
            sTotalChipW += sChipTextSizes[f].x + sChipPadX * 2.0f + sChipPad;
        }
        sTotalChipW -= sChipPad;
    }

    f32 sChipX = sidebarW + (contentW - sTotalChipW) * 0.5f;
    for (int f = 0; f < 5; ++f) {
        f32 sChipW = sChipTextSizes[f].x + sChipPadX * 2.0f;
        ImVec2 scPos(sChipX, statusFilterY);
        ImVec2 scEnd(sChipX + sChipW, statusFilterY + sChipH);

        bool isActive = (m_TemplateStatusFilter == statusValues[f]);
        bool hovered = (io.MousePos.x >= scPos.x && io.MousePos.x <= scEnd.x &&
                       io.MousePos.y >= scPos.y && io.MousePos.y <= scEnd.y);

        ImU32 sBg = isActive ? IM_COL32(60, 80, 140, 255) :
                   (hovered ? IM_COL32(45, 50, 70, 255) : IM_COL32(30, 33, 42, 255));
        dl->AddRectFilled(scPos, scEnd, sBg, sChipH * 0.5f);
        if (isActive) {
            dl->AddRect(scPos, scEnd, statusColors[f], sChipH * 0.5f);
        }

        dl->AddText(nullptr, sChipFontSize,
            ImVec2(scPos.x + (sChipW - sChipTextSizes[f].x) * 0.5f,
                   scPos.y + (sChipH - sChipTextSizes[f].y) * 0.5f),
            isActive ? statusColors[f] : IM_COL32(150, 155, 175, 200),
            statusLabels[f]);

        if (hovered && ImGui::IsMouseClicked(0)) {
            m_TemplateStatusFilter = statusValues[f];
        }

        sChipX += sChipW + sChipPad;
    }

    // === Template grid (scrollable) ===
    f32 gridStartY = statusFilterY + sChipH + 15.0f;
    f32 cardW = 500.0f;
    f32 cardH = 380.0f;
    f32 cardPad = 28.0f;
    f32 maxRowWidth = contentW - 60.0f;
    int cardsPerRow = static_cast<int>((maxRowWidth + cardPad) / (cardW + cardPad));
    if (cardsPerRow < 1) cardsPerRow = 1;

    f32 gridBottomMargin = 100.0f;
    f32 gridHeight = area.y - gridStartY - gridBottomMargin;
    if (gridHeight < 100.0f) gridHeight = 100.0f;
    ImGui::SetCursorPos(ImVec2(sidebarW, gridStartY));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
    ImGui::BeginChild("##TemplateGrid", ImVec2(contentW, gridHeight), false,
                       ImGuiWindowFlags_NoBackground);
    ImDrawList* gridDl = ImGui::GetWindowDrawList();
    ImVec2 gridScreenOrigin = ImGui::GetCursorScreenPos();

    // Build filtered index list
    std::vector<int> filteredIndices;
    for (int i = 0; i < s_BuiltinCount; ++i) {
        if (m_TemplateFilter != TMPL_ALL && !(s_BuiltinTemplates[i].categoryFlags & m_TemplateFilter))
            continue;
        if (m_TemplateStatusFilter >= 0 &&
            s_BuiltinTemplates[i].maturity != static_cast<Editor::MaturityTier>(m_TemplateStatusFilter))
            continue;
        filteredIndices.push_back(i);
    }

    // Apply search filter
    if (m_TemplateSearchBuffer[0] != '\0') {
        std::string searchLower = m_TemplateSearchBuffer;
        std::transform(searchLower.begin(), searchLower.end(), searchLower.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        std::vector<int> searchFiltered;
        for (int idx : filteredIndices) {
            std::string nameLower = s_BuiltinTemplates[idx].name;
            std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (nameLower.find(searchLower) != std::string::npos) {
                searchFiltered.push_back(idx);
            }
        }
        filteredIndices = std::move(searchFiltered);
    }

    int customStart = static_cast<int>(filteredIndices.size());

    std::vector<int> filteredCustomIndices;
    if (m_TemplateFilter == TMPL_ALL) {
        for (int ci = 0; ci < static_cast<int>(m_CustomTemplateNames.size()); ++ci) {
            if (m_TemplateSearchBuffer[0] != '\0') {
                std::string searchLower = m_TemplateSearchBuffer;
                std::transform(searchLower.begin(), searchLower.end(), searchLower.begin(),
                    [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                std::string nameLower = m_CustomTemplateNames[ci];
                std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(),
                    [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                if (nameLower.find(searchLower) != std::string::npos) {
                    filteredCustomIndices.push_back(ci);
                }
            } else {
                filteredCustomIndices.push_back(ci);
            }
        }
    }

    int totalVisible = static_cast<int>(filteredIndices.size()) + static_cast<int>(filteredCustomIndices.size());
    int totalRows = totalVisible > 0 ? ((totalVisible - 1) / cardsPerRow + 1) : 0;
    f32 totalContentH = totalRows * (cardH + cardPad);
    if (totalContentH < 1.0f) totalContentH = 1.0f;

    // Hover preview tracking
    ImVec2 hoveredCardPos(0, 0), hoveredCardEnd(0, 0);
    i32 prevHoverIdx = m_HoverTemplateIdx;
    m_HoverTemplateIdx = -1;

    // Bottom action bar occupies y >= bottomY - 12; clicks there belong to
    // Create / Back / Start Blank buttons, not to template cards underneath.
    f32 bottomBarY = area.y - 85.0f - 12.0f;
    bool mouseInBottomBar = (io.MousePos.y >= bottomBarY);

    // Draw filtered builtin templates
    for (int vi = 0; vi < static_cast<int>(filteredIndices.size()); ++vi) {
        int i = filteredIndices[vi];
        int row = vi / cardsPerRow;
        int col = vi % cardsPerRow;
        int itemsInRow = totalVisible - row * cardsPerRow;
        if (itemsInRow > cardsPerRow) itemsInRow = cardsPerRow;

        f32 rowWidth = itemsInRow * (cardW + cardPad) - cardPad;
        f32 rowStartX = (contentW - rowWidth) * 0.5f;

        ImVec2 cardPos(gridScreenOrigin.x + rowStartX + col * (cardW + cardPad),
                       gridScreenOrigin.y + row * (cardH + cardPad));
        ImVec2 cardEnd(cardPos.x + cardW, cardPos.y + cardH);

        bool isStable = true; // All templates unlocked
        bool hovered = (io.MousePos.x >= cardPos.x && io.MousePos.x <= cardEnd.x &&
                       io.MousePos.y >= cardPos.y && io.MousePos.y <= cardEnd.y);
        bool selected = (m_SelectedTemplate == i);

        // Locked (non-Stable) cards get muted styling
        if (isStable) {
            ImU32 bgCol = selected ? IM_COL32(35, 45, 70, 255) :
                          (hovered ? IM_COL32(40, 45, 60, 255) : IM_COL32(25, 28, 35, 255));
            gridDl->AddRectFilled(cardPos, cardEnd, bgCol, 8.0f);
        } else {
            gridDl->AddRectFilled(cardPos, cardEnd, IM_COL32(20, 22, 28, 255), 8.0f);
        }

        ImVec4 accent = s_BuiltinTemplates[i].accentColor;
        if (isStable) {
            ImU32 accentCol = IM_COL32(
                (int)(accent.x * 255), (int)(accent.y * 255),
                (int)(accent.z * 255), (hovered || selected) ? 255 : 180);
            // Procedural thumbnail (top 200px)
            ImVec2 thumbMin(cardPos.x + 1, cardPos.y + 1);
            ImVec2 thumbMax(cardEnd.x - 1, cardPos.y + 200.0f);
            DrawTemplateThumbnail(gridDl, s_BuiltinTemplates[i].id, thumbMin, thumbMax, accent, hovered);
            // Thin accent bar above thumbnail
            gridDl->AddRectFilled(cardPos, ImVec2(cardEnd.x, cardPos.y + 3.0f), accentCol, 8.0f, ImDrawFlags_RoundCornersTop);

            ImU32 borderCol = selected ? IM_COL32(140, 160, 220, 255) :
                             (hovered  ? accentCol : IM_COL32(60, 65, 80, 150));
            gridDl->AddRect(cardPos, cardEnd, borderCol, 8.0f, 0, selected ? 2.5f : (hovered ? 2.0f : 1.0f));
        } else {
            // Desaturated accent stripe for locked cards
            ImU32 mutedAccent = IM_COL32(
                (int)(accent.x * 80), (int)(accent.y * 80),
                (int)(accent.z * 80), 100);
            // Procedural thumbnail (top 200px, dimmed for locked)
            ImVec2 thumbMin(cardPos.x + 1, cardPos.y + 1);
            ImVec2 thumbMax(cardEnd.x - 1, cardPos.y + 200.0f);
            DrawTemplateThumbnail(gridDl, s_BuiltinTemplates[i].id, thumbMin, thumbMax, accent, false);
            gridDl->AddRectFilled(cardPos, ImVec2(cardEnd.x, cardPos.y + 3.0f), mutedAccent, 8.0f, ImDrawFlags_RoundCornersTop);
            gridDl->AddRect(cardPos, cardEnd, IM_COL32(45, 48, 58, 120), 8.0f, 0, 1.0f);
        }

        if (selected && isStable) {
            const char* check = "\xe2\x9c\x93";
            ImVec2 checkSize = ImGui::CalcTextSize(check);
            gridDl->AddText(ImVec2(cardEnd.x - checkSize.x - 8.0f, cardPos.y + 10.0f),
                IM_COL32(140, 200, 140, 255), check);
        }

        // Template name — dimmed for locked
        ImU32 nameCol = isStable ? IM_COL32(220, 225, 245, 255) : IM_COL32(100, 105, 120, 160);
        DrawCenteredClippedText(gridDl, s_BuiltinTemplates[i].name, cardPos.x, cardW,
            cardPos.y + 210.0f, nameCol, 12.0f, font, 20.0f);

        // Maturity tier badge (top-left corner)
        {
            const char* tierLabel = Editor::TemplateMarketplace::GetMaturityName(s_BuiltinTemplates[i].maturity);
            ImU32 tierCol;
            switch (s_BuiltinTemplates[i].maturity) {
                case Editor::MaturityTier::Stable:       tierCol = IM_COL32(80, 140, 220, 200); break;
                case Editor::MaturityTier::Beta:         tierCol = IM_COL32(80, 180, 80, 200); break;
                case Editor::MaturityTier::Preview:      tierCol = IM_COL32(210, 170, 50, 200); break;
                case Editor::MaturityTier::Experimental: tierCol = IM_COL32(210, 70, 70, 200); break;
                default: tierCol = IM_COL32(120, 120, 120, 200); break;
            }
            // Dim badge alpha for locked cards
            if (!isStable) {
                tierCol = (tierCol & 0x00FFFFFF) | (100 << 24);
            }
            ImVec2 tierSize = ImGui::CalcTextSize(tierLabel);
            ImVec2 tierPos(cardPos.x + 6.0f, cardPos.y + 8.0f);
            gridDl->AddRectFilled(ImVec2(tierPos.x - 3.0f, tierPos.y - 1.0f),
                ImVec2(tierPos.x + tierSize.x + 3.0f, tierPos.y + tierSize.y + 1.0f), tierCol, 3.0f);
            gridDl->AddText(tierPos, isStable ? IM_COL32(255, 255, 255, 240) : IM_COL32(180, 180, 180, 140), tierLabel);
        }

        const char* desc = s_BuiltinTemplates[i].description;
        std::string descStr(desc);
        f32 lineY = cardPos.y + 240.0f;
        std::istringstream iss(descStr);
        std::string line;
        ImU32 descCol = isStable ? IM_COL32(140, 145, 165, 200) : IM_COL32(80, 84, 95, 140);
        while (std::getline(iss, line, '\n')) {
            DrawCenteredClippedText(gridDl, line.c_str(), cardPos.x, cardW,
                lineY, descCol, 12.0f, font, 17.0f);
            lineY += 20.0f;
        }

        if (hovered) {
            m_HoverTemplateIdx = i;
            hoveredCardPos = cardPos;
            hoveredCardEnd = cardEnd;
        }

        // Only Stable templates are selectable
        if (hovered && !mouseInBottomBar && ImGui::IsMouseClicked(0)) {
            if (isStable) {
                m_SelectedTemplate = (m_SelectedTemplate == i) ? -1 : i;
            }
        }
    }

    // Draw custom templates
    for (int fci = 0; fci < static_cast<int>(filteredCustomIndices.size()); ++fci) {
        int ci = filteredCustomIndices[fci];
        int vi = customStart + fci;
        int row = vi / cardsPerRow;
        int col = vi % cardsPerRow;
        int itemsInRow = totalVisible - row * cardsPerRow;
        if (itemsInRow > cardsPerRow) itemsInRow = cardsPerRow;

        f32 rowWidth = itemsInRow * (cardW + cardPad) - cardPad;
        f32 rowStartX = (contentW - rowWidth) * 0.5f;

        ImVec2 cardPos(gridScreenOrigin.x + rowStartX + col * (cardW + cardPad),
                       gridScreenOrigin.y + row * (cardH + cardPad));
        ImVec2 cardEnd(cardPos.x + cardW, cardPos.y + cardH);

        bool hovered = (io.MousePos.x >= cardPos.x && io.MousePos.x <= cardEnd.x &&
                       io.MousePos.y >= cardPos.y && io.MousePos.y <= cardEnd.y);
        bool selected = (m_SelectedTemplate == -(ci + 1));

        ImU32 bgCol = selected ? IM_COL32(35, 50, 60, 255) :
                      (hovered ? IM_COL32(40, 45, 60, 255) : IM_COL32(25, 28, 35, 255));
        gridDl->AddRectFilled(cardPos, cardEnd, bgCol, 8.0f);

        ImU32 accentCol = (hovered || selected) ? IM_COL32(0, 200, 180, 255) : IM_COL32(0, 200, 180, 150);
        // Default thumbnail for custom templates
        ImVec2 cThumbMin(cardPos.x + 1, cardPos.y + 1);
        ImVec2 cThumbMax(cardEnd.x - 1, cardPos.y + 200.0f);
        ImVec4 cAccent(0.4f, 0.6f, 0.8f, 1.0f);
        DrawTemplateThumbnail(gridDl, "custom", cThumbMin, cThumbMax, cAccent, hovered);
        gridDl->AddRectFilled(cardPos, ImVec2(cardEnd.x, cardPos.y + 3.0f), accentCol, 8.0f, ImDrawFlags_RoundCornersTop);

        ImU32 borderCol = selected ? IM_COL32(0, 220, 200, 255) :
                         (hovered  ? accentCol : IM_COL32(60, 65, 80, 150));
        gridDl->AddRect(cardPos, cardEnd, borderCol, 8.0f, 0, selected ? 2.5f : (hovered ? 2.0f : 1.0f));

        if (selected) {
            const char* check = "✓";
            ImVec2 checkSize = ImGui::CalcTextSize(check);
            gridDl->AddText(ImVec2(cardEnd.x - checkSize.x - 8.0f, cardPos.y + 10.0f),
                IM_COL32(140, 200, 140, 255), check);
        }

        DrawCenteredClippedText(gridDl, m_CustomTemplateNames[ci].c_str(), cardPos.x, cardW,
            cardPos.y + 210.0f, IM_COL32(220, 225, 245, 255), 12.0f, font, 20.0f);

        const char* customLabel = "Custom Template";
        DrawCenteredClippedText(gridDl, customLabel, cardPos.x, cardW, cardPos.y + 240.0f,
            IM_COL32(0, 180, 160, 200), 12.0f, font, 17.0f);

        if (hovered && !mouseInBottomBar && ImGui::IsMouseClicked(0)) {
            int newSel = -(ci + 1);
            m_SelectedTemplate = (m_SelectedTemplate == newSel) ? -1 : newSel;
        }
    }

    ImGui::Dummy(ImVec2(contentW, totalContentH));
    ImGui::EndChild();
    ImGui::PopStyleColor(); // ChildBg

    // Hover preview overlay
    if (m_HoverTemplateIdx >= 0) {
        if (m_HoverTemplateIdx != prevHoverIdx) {
            m_HoverTimer = 0.0f;
            m_HoverFrameIdx = 0;
        }
        m_HoverTimer += io.DeltaTime;
        DrawTemplateHoverPreview(dl, m_HoverTemplateIdx, hoveredCardPos, hoveredCardEnd);
    }

    // === Bottom bar: Back | Create | Start Blank ===
    f32 bottomY = area.y - 85.0f;
    dl->AddLine(ImVec2(sidebarW + 30.0f, bottomY - 12.0f), ImVec2(area.x - 30.0f, bottomY - 12.0f),
        IM_COL32(60, 65, 80, 150), 1.0f);

    // Block creation if a non-Stable template is selected
    bool templateLocked = false;
    if (m_SelectedTemplate >= 0 && m_SelectedTemplate < s_BuiltinCount) {
        templateLocked = false; // All templates unlocked
    }
    bool canCreate = (std::strlen(m_NewProjectName) > 0 && std::strlen(m_NewProjectPath) > 0 &&
                     std::strlen(m_NewSceneName) > 0 && !templateLocked);

    // "< Back" link
    f32 backFontSize = 20.0f;
    const char* backText = "< Back";
    ImVec2 backSz = font->CalcTextSizeA(backFontSize, FLT_MAX, 0.0f, backText);
    ImVec2 backPos(sidebarW + 40.0f, bottomY + 20.0f);
    bool backHovered = (io.MousePos.x >= backPos.x && io.MousePos.x <= backPos.x + backSz.x &&
                       io.MousePos.y >= backPos.y && io.MousePos.y <= backPos.y + backSz.y);
    dl->AddText(nullptr, backFontSize, backPos,
        backHovered ? IM_COL32(180, 185, 205, 255) : IM_COL32(120, 125, 145, 200), backText);
    if (backHovered && ImGui::IsMouseClicked(0)) {
        m_HubPage = HubPage::WizardSetup;
    }

    // "Create" button (centered)
    f32 createFontSize = 24.0f;
    f32 createBtnW = 280.0f, createBtnH = 56.0f;
    f32 createCenterX = sidebarW + contentW * 0.5f;
    ImVec2 createPos(createCenterX - createBtnW * 0.5f, bottomY);
    ImVec2 createEnd(createPos.x + createBtnW, createPos.y + createBtnH);
    bool createHovered = canCreate && (io.MousePos.x >= createPos.x && io.MousePos.x <= createEnd.x &&
                                       io.MousePos.y >= createPos.y && io.MousePos.y <= createEnd.y);

    ImU32 createBg = !canCreate ? IM_COL32(35, 38, 48, 200) :
                     (createHovered ? IM_COL32(60, 90, 160, 255) : IM_COL32(50, 70, 130, 255));
    dl->AddRectFilled(createPos, createEnd, createBg, 10.0f);
    if (canCreate) {
        dl->AddRect(createPos, createEnd, IM_COL32(80, 110, 180, 200), 10.0f);
    }

    const char* createText = "Create Project";
    ImVec2 createSz = font->CalcTextSizeA(createFontSize, FLT_MAX, 0.0f, createText);
    ImU32 createTextCol = canCreate ? IM_COL32(220, 225, 245, 255) : IM_COL32(100, 105, 120, 150);
    dl->AddText(nullptr, createFontSize,
        ImVec2(createPos.x + (createBtnW - createSz.x) * 0.5f,
               createPos.y + (createBtnH - createSz.y) * 0.5f),
        createTextCol, createText);

    if (createHovered && ImGui::IsMouseClicked(0)) {
        std::string templateId = "blank";
        if (m_SelectedTemplate >= 0 && m_SelectedTemplate < s_BuiltinCount) {
            templateId = s_BuiltinTemplates[m_SelectedTemplate].id;
        } else if (m_SelectedTemplate < -0) {
            int ci = -(m_SelectedTemplate + 1);
            if (ci >= 0 && ci < static_cast<int>(m_CustomTemplateNames.size())) {
                templateId = "custom:" + std::to_string(ci);
            }
        }

        if (CreateProjectOnDisk(m_NewProjectPath, m_NewProjectName, m_NewSceneName, templateId)) {
            std::strncpy(m_NewProjectName, "MyGame", sizeof(m_NewProjectName));
            std::strncpy(m_NewSceneName, "Main", sizeof(m_NewSceneName));
            m_SelectedTemplate = -1;
            m_TemplateFilter = TMPL_ALL;
            m_TemplateSearchBuffer[0] = '\0';
            m_HubPage = HubPage::Landing;
        }
    }

    // "Start Blank" link — always available if form fields are filled (ignores template selection)
    bool canStartBlank = (std::strlen(m_NewProjectName) > 0 && std::strlen(m_NewProjectPath) > 0 &&
                          std::strlen(m_NewSceneName) > 0);
    f32 blankFontSize = 18.0f;
    const char* blankText = "Start Blank";
    ImVec2 blankSz = font->CalcTextSizeA(blankFontSize, FLT_MAX, 0.0f, blankText);
    ImVec2 blankPos(area.x - blankSz.x - 40.0f, bottomY + (createBtnH - blankSz.y) * 0.5f);
    bool blankHovered = canStartBlank && (io.MousePos.x >= blankPos.x && io.MousePos.x <= blankPos.x + blankSz.x &&
                       io.MousePos.y >= blankPos.y && io.MousePos.y <= blankPos.y + blankSz.y);
    dl->AddText(nullptr, blankFontSize, blankPos,
        blankHovered ? IM_COL32(180, 185, 205, 255) :
        (canStartBlank ? IM_COL32(120, 125, 145, 200) : IM_COL32(70, 75, 85, 120)), blankText);

    if (blankHovered && ImGui::IsMouseClicked(0)) {
        if (CreateProjectOnDisk(m_NewProjectPath, m_NewProjectName, m_NewSceneName, "blank")) {
            std::strncpy(m_NewProjectName, "MyGame", sizeof(m_NewProjectName));
            std::strncpy(m_NewSceneName, "Main", sizeof(m_NewSceneName));
            m_SelectedTemplate = -1;
            m_TemplateFilter = TMPL_ALL;
            m_TemplateSearchBuffer[0] = '\0';
            m_HubPage = HubPage::Landing;
        }
    }
}

// --------------------------------------------------
// Template hover preview overlay
// --------------------------------------------------
void EditorLayer::DrawTemplateHoverPreview(ImDrawList* /*dl*/, i32 templateIdx, const ImVec2& cardPos, const ImVec2& cardEnd) {
    if (templateIdx < 0 || templateIdx >= s_BuiltinCount) return;

    ImGuiIO& io = ImGui::GetIO();
    ImFont* font = ImGui::GetFont();
    ImDrawList* fg = ImGui::GetForegroundDrawList();

    const auto& tmpl = s_BuiltinTemplates[templateIdx];
    ImVec4 accent = tmpl.accentColor;
    ImU32 accentCol = IM_COL32((int)(accent.x * 255), (int)(accent.y * 255), (int)(accent.z * 255), 120);

    // --- Probe for preview frame images ---
    std::string previewDir = "Engine/previews/" + std::string(tmpl.id) + "/";
    int frameCount = 0;
    VkDescriptorSet frameTextures[8] = {};
    for (int f = 0; f < 8; ++f) {
        std::string framePath = previewDir + "frame_" + std::to_string(f) + ".png";
        VkDescriptorSet texId = GetImGuiTexture(framePath);
        if (texId == VK_NULL_HANDLE) break;
        frameTextures[f] = texId;
        frameCount++;
    }

    bool hasFrames = (frameCount > 0);

    // --- Cycle frame index ---
    if (hasFrames) {
        f32 cycleInterval = 0.6f;
        m_HoverFrameIdx = static_cast<i32>(m_HoverTimer / cycleInterval) % frameCount;
    }

    // --- Popup dimensions ---
    f32 popupW = 480.0f;
    f32 imgW = 440.0f, imgH = 280.0f;
    f32 margin = 20.0f;
    f32 nameFontSize = 22.0f;
    f32 descFontSize = 15.0f;
    f32 dotAreaH = hasFrames ? 24.0f : 0.0f;

    // Measure name
    ImVec2 nameSz = font->CalcTextSizeA(nameFontSize, FLT_MAX, 0.0f, tmpl.name);

    // Word-wrap description for width
    f32 descWrapWidth = popupW - margin * 2.0f;
    ImVec2 descSz = font->CalcTextSizeA(descFontSize, FLT_MAX, descWrapWidth, tmpl.description);

    f32 accentBarH = 4.0f;
    f32 imageAreaH = hasFrames ? (margin + imgH + 8.0f) : 0.0f;
    f32 dividerH = 12.0f;
    f32 popupH = accentBarH + imageAreaH + margin + nameSz.y + dividerH + descSz.y + margin + dotAreaH;

    // --- Positioning: right of card, flip if needed ---
    f32 gapX = 12.0f;
    f32 popupX, popupY;
    f32 cardMidX = (cardPos.x + cardEnd.x) * 0.5f;
    if (cardMidX < io.DisplaySize.x * 0.5f) {
        popupX = cardEnd.x + gapX;
    } else {
        popupX = cardPos.x - popupW - gapX;
    }
    popupY = cardPos.y;

    // Clamp vertically
    if (popupY + popupH > io.DisplaySize.y - 10.0f) {
        popupY = io.DisplaySize.y - 10.0f - popupH;
    }
    if (popupY < 10.0f) popupY = 10.0f;

    ImVec2 pMin(popupX, popupY);
    ImVec2 pMax(popupX + popupW, popupY + popupH);

    // --- Draw popup background ---
    fg->AddRectFilled(pMin, pMax, IM_COL32(18, 20, 28, 245), 10.0f);
    fg->AddRect(pMin, pMax, accentCol, 10.0f);

    // Accent bar at top
    fg->AddRectFilled(pMin, ImVec2(pMax.x, pMin.y + accentBarH),
        IM_COL32((int)(accent.x * 255), (int)(accent.y * 255), (int)(accent.z * 255), 200),
        10.0f, ImDrawFlags_RoundCornersTop);

    f32 curY = pMin.y + accentBarH;

    // --- Preview image ---
    if (hasFrames) {
        curY += margin;
        f32 imgX = pMin.x + (popupW - imgW) * 0.5f;
        fg->AddImage((ImTextureID)frameTextures[m_HoverFrameIdx],
            ImVec2(imgX, curY), ImVec2(imgX + imgW, curY + imgH));
        curY += imgH + 8.0f;
    }

    // --- Template name ---
    curY += margin;
    f32 nameX = pMin.x + (popupW - nameSz.x) * 0.5f;
    fg->AddText(nullptr, nameFontSize, ImVec2(nameX, curY),
        IM_COL32(220, 225, 245, 255), tmpl.name);
    curY += nameSz.y;

    // --- Divider ---
    curY += dividerH * 0.5f;
    fg->AddLine(ImVec2(pMin.x + margin, curY), ImVec2(pMax.x - margin, curY),
        IM_COL32(60, 65, 80, 150), 1.0f);
    curY += dividerH * 0.5f;

    // --- Description (word-wrapped) ---
    fg->AddText(font, descFontSize, ImVec2(pMin.x + margin, curY), IM_COL32(160, 165, 185, 220),
        tmpl.description, nullptr, descWrapWidth);
    curY += descSz.y;

    // --- Frame indicator dots ---
    if (hasFrames && frameCount > 1) {
        curY += 8.0f;
        f32 dotR = 4.0f;
        f32 dotGap = 12.0f;
        f32 dotsW = frameCount * dotR * 2.0f + (frameCount - 1) * dotGap;
        f32 dotStartX = pMin.x + (popupW - dotsW) * 0.5f + dotR;
        for (int d = 0; d < frameCount; ++d) {
            f32 dx = dotStartX + d * (dotR * 2.0f + dotGap);
            if (d == m_HoverFrameIdx) {
                fg->AddCircleFilled(ImVec2(dx, curY + dotR), dotR, IM_COL32(199, 218, 196, 255));
            } else {
                fg->AddCircle(ImVec2(dx, curY + dotR), dotR, IM_COL32(120, 130, 145, 180), 0, 1.5f);
            }
        }
    }
}

// --------------------------------------------------
// Demos tab: showcase demo scenes
// --------------------------------------------------
void EditorLayer::DrawHubDemosTab(ImDrawList* dl, const ImVec2& area, f32 contentY, f32 sidebarW) {
    ImGuiIO& io = ImGui::GetIO();
    ImFont* font = ImGui::GetFont();

    f32 contentW = area.x - sidebarW;

    struct DemoInfo {
        const char* name;
        const char* description;
        const char* scenePath;
        ImVec4 accentColor;
        const char* templateId;
    };

    DemoInfo demos[] = {
        { "2D Platformer",   "Side-scrolling platformer with\njumping, enemies, and collectibles.",   "demos/platformer_demo.enjin",   ImVec4(0.3f, 0.8f, 0.3f, 1.0f), "platformer" },
        { "3D Third Person",  "Over-the-shoulder exploration\nwith third-person camera controls.",    "demos/thirdperson_demo.enjin",  ImVec4(0.8f, 0.3f, 0.3f, 1.0f), "thirdperson" },
        { "Flower Garden",   "Interactive flower plucking\nwith physics and scoring.",                "demos/flower_demo.enjin",       ImVec4(0.9f, 0.4f, 0.6f, 1.0f), "flower" },
        { "4P Racing",       "Splitscreen multiplayer\nracing with vehicles.",                        "demos/racing_demo.enjin",       ImVec4(0.9f, 0.3f, 0.1f, 1.0f), "racing" },
        { "Visual Novel",    "Story-driven dialogue\nwith branching choices.",                        "demos/visualnovel_demo.enjin",  ImVec4(0.9f, 0.7f, 0.9f, 1.0f), "visualnovel" },
        { "Top-Down RPG",    "Overhead RPG with NPCs,\nquests, and inventory.",                      "demos/topdown_demo.enjin",      ImVec4(0.3f, 0.6f, 0.9f, 1.0f), "rpg_village" },
    };
    constexpr int demoCount = 6;

    // Cache file availability on tab switch
    if (!m_DemosCacheValid) {
        m_DemoAvailability.resize(demoCount);
        for (int i = 0; i < demoCount; ++i) {
            m_DemoAvailability[i] = true;
        }
        m_DemosCacheValid = true;
    }

    // "< Back" link at top
    f32 backFontSize = 20.0f;
    const char* backText = "< Back";
    ImVec2 backSz = font->CalcTextSizeA(backFontSize, FLT_MAX, 0.0f, backText);
    ImVec2 backPos(sidebarW + 30.0f, contentY + 8.0f);
    bool backHovered = (io.MousePos.x >= backPos.x && io.MousePos.x <= backPos.x + backSz.x &&
                       io.MousePos.y >= backPos.y && io.MousePos.y <= backPos.y + backSz.y);
    dl->AddText(nullptr, backFontSize, backPos,
        backHovered ? IM_COL32(180, 185, 205, 255) : IM_COL32(120, 125, 145, 200), backText);
    if (backHovered && ImGui::IsMouseClicked(0)) {
        m_HubPage = HubPage::Landing;
    }

    // Subtitle (clipped if wider than content area)
    f32 subtitleFontSize = 18.0f;
    const char* subtitle = "Click to explore -- each demo creates a scene from a built-in template.";
    f32 subtitleMaxW = contentW - 60.0f;
    std::string clippedSubtitle = EllipsizeText(subtitle, subtitleMaxW, font, subtitleFontSize);
    ImVec2 subSz = font->CalcTextSizeA(subtitleFontSize, FLT_MAX, 0.0f, clippedSubtitle.c_str());
    f32 subX = sidebarW + (contentW - subSz.x) * 0.5f;
    dl->AddText(nullptr, subtitleFontSize, ImVec2(subX, contentY + 8.0f + backSz.y + 12.0f),
        IM_COL32(120, 130, 145, 200), clippedSubtitle.c_str());

    // Grid layout (offset by sidebar)
    f32 gridStartY = contentY + 8.0f + backSz.y + 12.0f + subSz.y + 24.0f;
    f32 cardW = 440.0f;
    f32 cardH = 320.0f;
    f32 cardPad = 24.0f;
    f32 maxRowWidth = contentW - 60.0f;
    int cardsPerRow = static_cast<int>((maxRowWidth + cardPad) / (cardW + cardPad));
    if (cardsPerRow < 1) cardsPerRow = 1;

    for (int i = 0; i < demoCount; ++i) {
        int row = i / cardsPerRow;
        int col = i % cardsPerRow;
        int itemsInRow = demoCount - row * cardsPerRow;
        if (itemsInRow > cardsPerRow) itemsInRow = cardsPerRow;

        f32 rowWidth = itemsInRow * (cardW + cardPad) - cardPad;
        f32 rowStartX = sidebarW + (contentW - rowWidth) * 0.5f;

        ImVec2 cPos(rowStartX + col * (cardW + cardPad), gridStartY + row * (cardH + cardPad));
        ImVec2 cEnd(cPos.x + cardW, cPos.y + cardH);

        bool available = (i < static_cast<int>(m_DemoAvailability.size())) && m_DemoAvailability[i];
        bool hovered = (io.MousePos.x >= cPos.x && io.MousePos.x <= cEnd.x &&
                       io.MousePos.y >= cPos.y && io.MousePos.y <= cEnd.y);

        ImVec4 accent = demos[i].accentColor;
        ImU32 accentCol = IM_COL32(
            (int)(accent.x * 255), (int)(accent.y * 255),
            (int)(accent.z * 255), available ? ((hovered) ? 255 : 180) : 80);

        // Card background — dimmed if not available
        ImU32 bgCol = !available ? IM_COL32(20, 22, 28, 255) :
                      (hovered ? IM_COL32(40, 45, 60, 255) : IM_COL32(25, 28, 35, 255));
        dl->AddRectFilled(cPos, cEnd, bgCol, 8.0f);

        // Procedural thumbnail for demo cards
        ImVec2 dThumbMin(cPos.x + 1, cPos.y + 1);
        ImVec2 dThumbMax(cEnd.x - 1, cPos.y + 160.0f);
        DrawTemplateThumbnail(dl, demos[i].templateId, dThumbMin, dThumbMax, accent, hovered && available);
        dl->AddRectFilled(cPos, ImVec2(cEnd.x, cPos.y + 3.0f), accentCol, 8.0f, ImDrawFlags_RoundCornersTop);

        // Border
        ImU32 borderCol = !available ? IM_COL32(45, 48, 58, 150) :
                          (hovered ? accentCol : IM_COL32(60, 65, 80, 150));
        dl->AddRect(cPos, cEnd, borderCol, 8.0f, 0, hovered ? 2.0f : 1.0f);

        // Name (clipped with ellipsis if wider than card)
        ImU32 nameCol = available ? IM_COL32(220, 225, 245, 255) : IM_COL32(120, 125, 140, 180);
        DrawCenteredClippedText(dl, demos[i].name, cPos.x, cardW,
            cPos.y + 172.0f, nameCol, 12.0f, font, 16.0f);

        // Description (centered, multi-line, clipped per line)
        f32 lineY = cPos.y + 198.0f;
        ImU32 descCol = available ? IM_COL32(140, 145, 165, 200) : IM_COL32(90, 95, 110, 140);
        std::string descStr(demos[i].description);
        std::istringstream iss(descStr);
        std::string line;
        while (std::getline(iss, line, '\n')) {
            DrawCenteredClippedText(dl, line.c_str(), cPos.x, cardW,
                lineY, descCol, 12.0f, font, 14.0f);
            lineY += 20.0f;
        }

        // "Coming Soon" badge overlay for unavailable demos
        if (!available) {
            const char* badge = "Coming Soon";
            f32 badgeFontSize = 16.0f;
            ImVec2 badgeSz = font->CalcTextSizeA(badgeFontSize, FLT_MAX, 0.0f, badge);
            f32 badgePadX = 14.0f, badgePadY = 6.0f;
            f32 badgeW = badgeSz.x + badgePadX * 2.0f;
            f32 badgeH = badgeSz.y + badgePadY * 2.0f;
            ImVec2 badgePos(cPos.x + (cardW - badgeW) * 0.5f, cEnd.y - badgeH - 16.0f);
            ImVec2 badgeEnd(badgePos.x + badgeW, badgePos.y + badgeH);

            dl->AddRectFilled(badgePos, badgeEnd, IM_COL32(60, 65, 80, 200), badgeH * 0.5f);
            dl->AddRect(badgePos, badgeEnd, IM_COL32(100, 105, 120, 150), badgeH * 0.5f);
            dl->AddText(nullptr, badgeFontSize,
                ImVec2(badgePos.x + badgePadX, badgePos.y + badgePadY),
                IM_COL32(160, 165, 180, 220), badge);
        }

        // Click to open available demos
        if (available && hovered && ImGui::IsMouseClicked(0)) {
            if (std::filesystem::exists(demos[i].scenePath)) {
                OpenScene(demos[i].scenePath);
            } else {
                ApplyTemplate(demos[i].templateId);
            }
            m_ShowProjectHub = false;
        }
    }
}

// --------------------------------------------------
// Create project folder structure on disk
// --------------------------------------------------
bool EditorLayer::CreateProjectOnDisk(const std::string& projectDir, const std::string& projectName,
                                      const std::string& sceneName, const std::string& templateId) {
    namespace fs = std::filesystem;

    // Build full project path
    fs::path projRoot = fs::path(projectDir) / projectName;

    // Create directory structure
    std::error_code ec;
    fs::create_directories(projRoot / "scenes", ec);
    if (ec) {
        ENJIN_LOG_ERROR(Editor, "Failed to create project directory: %s", ec.message().c_str());
        return false;
    }
    fs::create_directories(projRoot / "scripts", ec);
    fs::create_directories(projRoot / "assets", ec);
    fs::create_directories(projRoot / "templates", ec);

    // Initialize git repository if requested
    if (m_GitInitOnCreate) {
        // Run git init
#ifdef _WIN32
        std::string gitInitCmd = "git init \"" + projRoot.string() + "\"";
        STARTUPINFOA si{}; si.cb = sizeof(si); si.dwFlags = STARTF_USESHOWWINDOW; si.wShowWindow = SW_HIDE;
        PROCESS_INFORMATION pi{};
        std::string cmdCopy = gitInitCmd;
        if (CreateProcessA(nullptr, cmdCopy.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
            WaitForSingleObject(pi.hProcess, 10000);
            CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
        }
#else
        // S23: Use posix_spawn to avoid shell command injection
        {
            const char* argv[] = { "git", "init", projRoot.string().c_str(), nullptr };
            pid_t pid = 0;
            extern char** environ;
            if (posix_spawnp(&pid, "git", nullptr, nullptr, const_cast<char**>(argv), environ) == 0) {
                int status = 0;
                waitpid(pid, &status, 0);
            }
        }
#endif

        // Write .gitignore
        fs::path gitignorePath = projRoot / ".gitignore";
        std::ofstream gitignore(gitignorePath);
        if (gitignore.is_open()) {
            gitignore << "# Build output\n";
            gitignore << "build/\n";
            gitignore << "*.enjpak\n";
            gitignore << "\n";
            gitignore << "# IDE/Editor\n";
            gitignore << ".vscode/\n";
            gitignore << ".vs/\n";
            gitignore << ".idea/\n";
            gitignore << "*.user\n";
            gitignore << "\n";
            gitignore << "# Logs and temp\n";
            gitignore << "*.log\n";
            gitignore << "__pycache__/\n";
            gitignore << "*.tmp\n";
            gitignore.close();
            ENJIN_LOG_INFO(Editor, "Initialized git repository with .gitignore");
        }
    }

    // Initialize project manifest via SceneManager
    m_SceneManager.NewProject(projectName);
    std::string relativeScenePath = "scenes/" + std::string(sceneName) + ".enjin";
    m_SceneManager.AddScene(sceneName, relativeScenePath);
    m_SceneManager.SetStartScene(0);

    // Auto-set project mode from template category
    if (templateId == "platformer" || templateId == "topdown2d" || templateId == "runner" ||
        templateId == "metroidvania" || templateId == "vampsurvivor" || templateId == "roguelike" ||
        templateId == "hello_sprite" || templateId == "neon_runner" || templateId == "cozy_farm") {
        m_SceneManager.SetProjectMode(Scene::ProjectMode::Mode2D);
    } else if (templateId == "blank" || templateId == "visualnovel" || templateId == "gamemanager") {
        m_SceneManager.SetProjectMode(Scene::ProjectMode::Mixed);
    } else {
        m_SceneManager.SetProjectMode(Scene::ProjectMode::Mode3D);
    }

    // Save manifest
    fs::path manifestPath = projRoot / (projectName + ".enjinproject");
    if (!m_SceneManager.SaveProject(manifestPath.string())) {
        ENJIN_LOG_ERROR(Editor, "Failed to save project manifest");
        return false;
    }

    // Defer template application + scene save to Update() — World::Clear() must not
    // run during Render because it invalidates GPU resources still referenced by
    // in-flight Vulkan command buffers.
    m_PendingTemplateId = templateId;
    m_PendingSceneLoadPath = (projRoot / relativeScenePath).string();

    // Track as recent project and persist last project directory
    m_EditorSettings.AddRecentProject(manifestPath.string());
    m_EditorSettings.lastProjectDir = projectDir;
    m_EditorSettings.Save();

    // Dismiss hub
    m_ShowProjectHub = false;
    ENJIN_LOG_INFO(Editor, "Created project '%s' at %s", projectName.c_str(), projRoot.string().c_str());

    return true;
}

void EditorLayer::ApplyTemplate(const std::string& templateId) {
    if (!m_World) return;

    // Clear existing scene and undo history
    m_World->Clear();
    if (m_RenderSystem) m_RenderSystem->OnSceneClear();
    ClearSelection();
    m_UndoRedo.Clear();

    // --- Configure editor layout per template ---
    // Reset to defaults first
    m_Layout = LayoutConfig{};
    // All templates use minimal panel set: Hierarchy, Inspector, Viewport, Console, AssetBrowser, GameView
    EditorPanel corePanels = EditorPanel::Hierarchy | EditorPanel::Inspector |
                             EditorPanel::Viewport | EditorPanel::Console | EditorPanel::AssetBrowser |
                             EditorPanel::GameView;
    m_Layout.panels = corePanels;

    // 2D templates
    if (templateId == "platformer" || templateId == "topdown2d") {
        m_Layout.leftWidth = 0.15f;
        m_Layout.rightWidth = 0.20f;
        m_Layout.bottomHeight = 0.18f;
        m_Layout.gameViewW = 700.0f;
        m_Layout.gameViewH = 450.0f;
        m_ShowStatsOverlay = false;
    }
    else if (templateId == "isometric" || templateId == "citybuilder" || templateId == "towerdefense") {
        m_Layout.leftWidth = 0.16f;
        m_Layout.rightWidth = 0.22f;
        m_Layout.gameViewW = 650.0f;
        m_Layout.gameViewH = 420.0f;
    }
    else if (templateId == "planetgravity") {
        m_Layout.leftWidth = 0.16f;
        m_Layout.rightWidth = 0.23f;
        m_Layout.inspectorSplit = 0.65f;
        m_Layout.gameViewW = 680.0f;
        m_Layout.gameViewH = 440.0f;
    }
    else if (templateId == "shadowtest") {
        m_Layout.leftWidth = 0.16f;
        m_Layout.rightWidth = 0.22f;
        m_Layout.gameViewW = 650.0f;
        m_Layout.gameViewH = 420.0f;
    }
    else if (templateId == "thirdperson" || templateId == "arena" || templateId == "teamsports") {
        m_Layout.leftWidth = 0.16f;
        m_Layout.rightWidth = 0.23f;
        m_Layout.inspectorSplit = 0.65f;
        m_Layout.gameViewW = 680.0f;
        m_Layout.gameViewH = 440.0f;
    }
    else if (templateId == "firstperson" || templateId == "fpsarena") {
        m_Layout.leftWidth = 0.13f;
        m_Layout.rightWidth = 0.20f;
        m_Layout.bottomHeight = 0.18f;
        m_Layout.gameViewW = 800.0f;
        m_Layout.gameViewH = 500.0f;
    }
    else if (templateId == "visualnovel" || templateId == "narrative") {
        m_Layout.leftWidth = 0.14f;
        m_Layout.rightWidth = 0.24f;
        m_Layout.bottomHeight = 0.15f;
        m_Layout.inspectorSplit = 0.7f;
        m_Layout.gameViewW = 750.0f;
        m_Layout.gameViewH = 480.0f;
    }
    else if (templateId == "rpg_village") {
        m_Layout.leftWidth = 0.17f;
        m_Layout.rightWidth = 0.22f;
        m_Layout.inspectorSplit = 0.55f;
        m_Layout.gameViewW = 640.0f;
        m_Layout.gameViewH = 400.0f;
    }
    else if (templateId == "survival") {
        m_Layout.leftWidth = 0.15f;
        m_Layout.rightWidth = 0.22f;
        m_Layout.gameViewW = 720.0f;
        m_Layout.gameViewH = 450.0f;
    }
    else if (templateId == "gamemanager") {
        m_Layout.leftWidth = 0.18f;
        m_Layout.rightWidth = 0.22f;
        m_Layout.bottomHeight = 0.28f;
        m_Layout.gameViewW = 500.0f;
        m_Layout.gameViewH = 350.0f;
    }
    // 3D wide (splitscreen templates)
    else if (templateId == "racing") {
        m_Layout.leftWidth = 0.12f;
        m_Layout.rightWidth = 0.18f;
        m_Layout.bottomHeight = 0.16f;
        m_Layout.gameViewW = 900.0f;
        m_Layout.gameViewH = 550.0f;
    }
    else if (templateId == "ps1rpg") {
        m_Layout.leftWidth = 0.15f;
        m_Layout.rightWidth = 0.22f;
        m_Layout.gameViewW = 640.0f;
        m_Layout.gameViewH = 420.0f;
    }
    else if (templateId == "horror") {
        m_Layout.leftWidth = 0.14f;
        m_Layout.rightWidth = 0.21f;
        m_Layout.bottomHeight = 0.18f;
        m_Layout.gameViewW = 780.0f;
        m_Layout.gameViewH = 500.0f;
    }
    else if (templateId == "puzzle") {
        m_Layout.leftWidth = 0.15f;
        m_Layout.rightWidth = 0.22f;
        m_Layout.bottomHeight = 0.18f;
        m_Layout.gameViewW = 700.0f;
        m_Layout.gameViewH = 450.0f;
    }
    else if (templateId == "runner") {
        m_Layout.leftWidth = 0.13f;
        m_Layout.rightWidth = 0.19f;
        m_Layout.bottomHeight = 0.16f;
        m_Layout.gameViewW = 850.0f;
        m_Layout.gameViewH = 400.0f;
    }
    else if (templateId == "flower") {
        m_Layout.leftWidth = 0.14f;
        m_Layout.rightWidth = 0.23f;
        m_Layout.inspectorSplit = 0.65f;
        m_Layout.gameViewW = 720.0f;
        m_Layout.gameViewH = 480.0f;
    }
    else if (templateId == "fixedcam") {
        m_Layout.leftWidth = 0.15f;
        m_Layout.rightWidth = 0.22f;
        m_Layout.gameViewW = 750.0f;
        m_Layout.gameViewH = 480.0f;
    }
    else if (templateId == "dungeon") {
        m_Layout.leftWidth = 0.14f;
        m_Layout.rightWidth = 0.21f;
        m_Layout.bottomHeight = 0.20f;
        m_Layout.gameViewW = 800.0f;
        m_Layout.gameViewH = 520.0f;
    }
    else if (templateId == "metroidvania") {
        m_Layout.leftWidth = 0.14f;
        m_Layout.rightWidth = 0.20f;
        m_Layout.bottomHeight = 0.18f;
        m_Layout.gameViewW = 750.0f;
        m_Layout.gameViewH = 450.0f;
    }
    else if (templateId == "soulslike") {
        m_Layout.leftWidth = 0.15f;
        m_Layout.rightWidth = 0.22f;
        m_Layout.gameViewW = 800.0f;
        m_Layout.gameViewH = 500.0f;
    }
    else if (templateId == "vampsurvivor") {
        m_Layout.leftWidth = 0.14f;
        m_Layout.rightWidth = 0.20f;
        m_Layout.bottomHeight = 0.16f;
        m_Layout.gameViewW = 700.0f;
        m_Layout.gameViewH = 700.0f;
    }
    else if (templateId == "roguelike") {
        m_Layout.leftWidth = 0.14f;
        m_Layout.rightWidth = 0.20f;
        m_Layout.bottomHeight = 0.18f;
        m_Layout.gameViewW = 650.0f;
        m_Layout.gameViewH = 650.0f;
    }
    else if (templateId == "couchcoop" || templateId == "justtwo") {
        m_Layout.leftWidth = 0.14f;
        m_Layout.rightWidth = 0.20f;
        m_Layout.gameViewW = 850.0f;
        m_Layout.gameViewH = 480.0f;
    }
    else if (templateId.substr(0, 6) == "flash_") {
        m_Layout.leftWidth = 0.14f;
        m_Layout.rightWidth = 0.20f;
        m_Layout.bottomHeight = 0.30f;
        m_Layout.gameViewW = 550.0f;
        m_Layout.gameViewH = 400.0f;
    }
    // 3D standard (all other non-blank templates)
    else if (templateId != "blank") {
        m_Layout.leftWidth = 0.16f;
        m_Layout.rightWidth = 0.22f;
        m_Layout.gameViewW = 700.0f;
        m_Layout.gameViewH = 450.0f;
    }

        m_VisiblePanels = m_Layout.panels;
    m_ForceLayout = true;

    // Configure editor camera for 2D templates (orthographic, looking at XY plane)
    if (templateId == "platformer" || templateId == "topdown2d" || templateId == "runner" ||
        templateId == "metroidvania" || templateId == "vampsurvivor" || templateId == "roguelike" ||
        templateId == "pointclick" || templateId == "bullethell" || templateId == "idleclicker" ||
        templateId.substr(0, 6) == "flash_") {
        if (m_CameraController) {
            m_CameraController->SetOrbitDistance(15.0f);
            // Front preset: camera at +Z looking along -Z, matching the game camera orientation
            m_CameraController->SetViewPreset(Renderer::ViewPreset::Front);
            m_CameraController->SetOrthoSize(10.0f);
        }
        m_ShowColliderWireframes = true;
    }

        // Handle custom templates
    if (templateId.substr(0, 7) == "custom:") {
        try {
            int idx = std::stoi(templateId.substr(7));
            if (idx >= 0 && idx < static_cast<int>(m_CustomTemplatePaths.size())) {
                OpenScene(m_CustomTemplatePaths[idx]);
            }
        } catch (const std::exception&) {
            ENJIN_LOG_WARN(Editor, "Invalid custom template ID: %s", templateId.c_str());
        }
        return;
    }

    if (templateId == "blank") {
        // Directional light so the scene isn't completely dark
        {
            ECS::Entity sun = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(sun, "Directional Light");
            auto& t = m_World->AddComponent<ECS::TransformComponent>(sun);
            t.position = Math::Vector3(0.0f, 10.0f, 0.0f);
            t.rotation = Math::Quaternion(Math::Vector3(1, 0, 0), Math::Radians(-45.0f));
            auto& lc = m_World->AddComponent<ECS::LightComponent>(sun);
            lc.type = ECS::LightType::Directional;
            lc.color = Math::Vector3(1.0f, 0.98f, 0.95f);
            lc.intensity = 1.0f;
            lc.castShadows = true;
        }

        // Procedural skybox
        {
            Renderer::SkyboxConfig sky;
            sky.type = Renderer::SkyboxType::Procedural;
            sky.topColor = Math::Vector3(0.1f, 0.3f, 0.8f);
            sky.horizonColor = Math::Vector3(0.5f, 0.7f, 1.0f);
            sky.bottomColor = Math::Vector3(0.8f, 0.85f, 0.9f);
            sky.sunDirection = Math::Vector3(0.0f, 1.0f, 0.0f);
            m_RenderSystem->SetSkybox(sky);
        }

        // Render settings
        m_RenderSystem->SetShadowsEnabled(true);
        m_RenderSystem->SetAmbientIntensity(0.15f);
        if (m_PostProcessing) {
            auto& pp = m_PostProcessing->GetSettings();
            pp.fxaaEnabled = 1;
        }

        return;
    }

    // --- Collider helpers: auto-sized to match mesh primitive ---
    // 3D colliders — match the mesh shape
    // 3D collider helpers — size is in WORLD space (not affected by transform scale).
    auto addBoxCollider3D = [&](ECS::Entity e, f32 w, f32 h, f32 d) {
        auto& col = m_World->AddComponent<ECS::BoxColliderComponent>(e);
        col.size = Math::Vector3(w, h, d);
    };
    auto addSphereCollider3D = [&](ECS::Entity e, f32 radius) {
        auto& col = m_World->AddComponent<ECS::SphereColliderComponent>(e);
        col.radius = radius;
    };
    auto addCapsuleCollider3D = [&](ECS::Entity e, f32 radius, f32 height) {
        auto& col = m_World->AddComponent<ECS::CapsuleColliderComponent>(e);
        col.radius = radius;
        col.height = height;
    };

    // 2D colliders — Box2D Body2DComponent
    auto addBoxCollider2D = [&](ECS::Entity e, f32 halfW, f32 halfH, bool isStatic = true) {
        auto& col = m_World->AddComponent<Physics::Body2DComponent>(e);
        col.shapeType = Physics::Shape2DType::Box;
        col.box.halfExtents = Math::Vector2(halfW, halfH);
        col.isStatic = isStatic;
    };
    auto addCircleCollider2D = [&](ECS::Entity e, f32 radius, bool isStatic = false) {
        auto& col = m_World->AddComponent<Physics::Body2DComponent>(e);
        col.shapeType = Physics::Shape2DType::Circle;
        col.circle.radius = radius;
        col.isStatic = isStatic;
    };
    // Capsule2D → Box approximation (Box2D has no capsule shape)
    auto addCapsuleCollider2D = [&](ECS::Entity e, f32 radius, f32 height, bool isStatic = false) {
        auto& col = m_World->AddComponent<Physics::Body2DComponent>(e);
        col.shapeType = Physics::Shape2DType::Box;
        col.box.halfExtents = Math::Vector2(radius, height * 0.5f);
        col.isStatic = isStatic;
    };

    // --- Common setup: ground plane (3D) ---
    auto createGround = [&]() -> ECS::Entity {
        ECS::Entity ground = m_World->CreateEntity();
        m_World->AddComponent<ECS::NameComponent>(ground, "Ground");
        auto& gt = m_World->AddComponent<ECS::TransformComponent>(ground);
        gt.position = Math::Vector3(0.0f, 0.0f, 0.0f);
        gt.scale = Math::Vector3(50.0f, 0.1f, 50.0f);
        auto& gmat = m_World->AddComponent<ECS::MaterialComponent>(ground);
        gmat.baseColor = Math::Vector3(0.35f, 0.55f, 0.3f);
        gmat.roughness = 0.9f;
        m_World->AddComponent<ECS::MeshComponent>(ground, Renderer::MeshFactory::CreateCube(1.0f));
        addBoxCollider3D(ground, 50.0f, 0.1f, 50.0f);
        auto& grb = m_World->AddComponent<ECS::RigidbodyComponent>(ground);
        grb.bodyType = ECS::RigidbodyComponent::BodyType::Static;
        return ground;
    };

    // --- Common: directional light ---
    auto createLight = [&]() -> ECS::Entity {
        ECS::Entity light = m_World->CreateEntity();
        m_World->AddComponent<ECS::NameComponent>(light, "Sun");
        auto& lt = m_World->AddComponent<ECS::TransformComponent>(light);
        lt.position = Math::Vector3(0.0f, 15.0f, 10.0f);
        lt.rotation = Math::Quaternion(Math::Vector3(1, 0, 0), Math::Radians(-45.0f));
        auto& lc = m_World->AddComponent<ECS::LightComponent>(light);
        lc.type = ECS::LightType::Directional;
        lc.intensity = 1.0f;
        lc.castShadows = true;
        return light;
    };

    // --- Common: 3D player with capsule mesh + matching capsule collider ---
    // No RigidbodyComponent — controllers (ThirdPerson/FirstPerson) handle their
    // own gravity, ground detection, and movement. A dynamic rigidbody would fight
    // with the controller for position control.
    auto createPlayer3D = [&](const std::string& name) -> ECS::Entity {
        ECS::Entity player = m_World->CreateEntity();
        m_World->AddComponent<ECS::NameComponent>(player, name);
        auto& pt = m_World->AddComponent<ECS::TransformComponent>(player);
        pt.position = Math::Vector3(0.0f, 1.0f, 0.0f);
        auto& pmat = m_World->AddComponent<ECS::MaterialComponent>(player);
        pmat.baseColor = Math::Vector3(0.2f, 0.4f, 0.9f);
        m_World->AddComponent<ECS::MeshComponent>(player, Renderer::MeshFactory::CreateCapsule(0.3f, 1.0f));
        addCapsuleCollider3D(player, 0.3f, 1.0f);
        return player;
    };

    // --- 2D player with capsule2D mesh + kinematic body for collision callbacks ---
    // NOTE: No Sprite2DComponent — entities without textures must render via the
    // 3D mesh pipeline. SpriteBatchRenderer ignores MeshComponent geometry and
    // requires a texture to be visible. Only add Sprite2DComponent when a texture is set.
    // The player has a kinematic Body2DComponent so Box2D can detect overlaps with
    // sensor-based pickups, hazards, and enemies. Kinematic bodies act as "visitors"
    // to sensors in Box2D v3 (two sensors never trigger events with each other).
    // SyncECSToBox2D pushes the controller position to Box2D, and SyncBox2DToECS
    // skips kinematic bodies (doesn't overwrite the transform).
    auto createPlayer2D = [&](const std::string& name) -> ECS::Entity {
        ECS::Entity player = m_World->CreateEntity();
        m_World->AddComponent<ECS::NameComponent>(player, name);
        auto& pt = m_World->AddComponent<ECS::TransformComponent>(player);
        pt.position = Math::Vector3(0.0f, 1.0f, 0.0f);
        auto& pmat = m_World->AddComponent<ECS::MaterialComponent>(player);
        pmat.baseColor = Math::Vector3(0.2f, 0.4f, 0.9f);
        m_World->AddComponent<ECS::MeshComponent>(player, Renderer::MeshFactory::CreateCapsule2D(0.8f, 1.6f));
        // Kinematic body — enables pickup/damage/enemy collision callbacks
        auto& body = m_World->AddComponent<Physics::Body2DComponent>(player);
        body.shapeType = Physics::Shape2DType::Box;
        body.box.halfExtents = Math::Vector2(0.4f, 0.8f);  // Match capsule dimensions
        body.isKinematic = true;
        body.gravityScale = 0.0f;
        body.fixedRotation = true;
        return player;
    };

    createLight();

    if (templateId == "platformer") {
        // ═══════════════════════════════════════════════════
        // 2D Platformer — 4-Zone Side-Scrolling Adventure
        // Meadow → Cave → Tower → Sky
        // ═══════════════════════════════════════════════════

        // --- Player ---
        ECS::Entity player = createPlayer2D("Player");
        {
            auto& ctrl = m_World->AddComponent<ECS::Platformer2DController>(player);
            ctrl.moveSpeed = 5.0f;
            ctrl.jumpForce = 10.0f;
            ctrl.enableWallJump = true;
            ctrl.coyoteTime = 0.15f;
            ctrl.maxJumps = 2;
            ctrl.collisionRadius = 0.4f;   // Match visual capsule (0.8 radius mesh)
            ctrl.collisionHeight = 1.6f;   // Match visual capsule height
            auto& hp = m_World->AddComponent<ECS::HealthComponent>(player);
            hp.maxHealth = 100.0f;
            hp.currentHealth = 100.0f;
            hp.regenRate = 1.0f;
            hp.regenDelay = 3.0f;
            hp.invulnerabilityTime = 0.5f;
            auto& inv = m_World->AddComponent<ECS::InventoryComponent>(player);
            inv.maxSlots = 10;
            m_World->AddComponent<ECS::TagComponent>(player).tags.push_back("player");
        }

        // --- Camera ---
        {
            SetupCameraForController(player, "Platformer2D");
            ECS::Entity cam = ECS::CameraManager::GetActiveCamera(m_World);
            if (cam != ECS::INVALID_ENTITY) {
                auto& follow = m_World->AddComponent<ECS::Camera2DBoundsComponent>(cam);
                follow.followTarget = player;
                follow.followSmoothing = 5.0f;
                follow.lookAheadDistance = 2.0f;
                follow.deadZoneSize = Math::Vector2(1.0f, 0.5f);
            }
        }

        // ═══════════════════════════════════════════════════
        // Zone 1 — Meadow (x: -10 to 10)
        // ═══════════════════════════════════════════════════

        // Meadow ground
        {
            ECS::Entity ground = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(ground, "Ground Meadow");
            auto& t = m_World->AddComponent<ECS::TransformComponent>(ground);
            t.position = Math::Vector3(0.0f, -1.0f, -0.1f);
            t.scale = Math::Vector3(20.0f, 1.0f, 1.0f);
            auto& mat = m_World->AddComponent<ECS::MaterialComponent>(ground);
            mat.baseColor = Math::Vector3(0.35f, 0.55f, 0.3f);
            mat.roughness = 0.9f;
            m_World->AddComponent<ECS::MeshComponent>(ground, Renderer::MeshFactory::CreateQuad(1.0f, 1.0f));
            addBoxCollider2D(ground, 10.0f, 0.5f);
        }

        // Wall-jump wall (far left)
        {
            ECS::Entity wall = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(wall, "Wall Jump Wall");
            auto& t = m_World->AddComponent<ECS::TransformComponent>(wall);
            t.position = Math::Vector3(-10.0f, 3.0f, 0.0f);
            t.scale = Math::Vector3(0.4f, 7.0f, 1.0f);
            auto& mat = m_World->AddComponent<ECS::MaterialComponent>(wall);
            mat.baseColor = Math::Vector3(0.45f, 0.4f, 0.5f);
            m_World->AddComponent<ECS::MeshComponent>(wall, Renderer::MeshFactory::CreateQuad(1.0f, 1.0f));
            addBoxCollider2D(wall, 0.2f, 3.5f);
        }

        // 3 stepping-stone platforms ascending right
        {
            const Math::Vector3 pos[] = {{-4.0f, 1.5f, 0.0f}, {1.0f, 3.0f, 0.0f}, {6.0f, 5.0f, 0.0f}};
            const Math::Vector3 col[] = {{0.5f, 0.4f, 0.3f}, {0.45f, 0.38f, 0.28f}, {0.55f, 0.42f, 0.32f}};
            for (int i = 0; i < 3; ++i) {
                ECS::Entity plat = m_World->CreateEntity();
                m_World->AddComponent<ECS::NameComponent>(plat, "Meadow Platform " + std::to_string(i + 1));
                auto& t = m_World->AddComponent<ECS::TransformComponent>(plat);
                t.position = pos[i];
                t.scale = Math::Vector3(3.0f, 0.4f, 1.0f);
                auto& mat = m_World->AddComponent<ECS::MaterialComponent>(plat);
                mat.baseColor = col[i];
                m_World->AddComponent<ECS::MeshComponent>(plat, Renderer::MeshFactory::CreateQuad(1.0f, 1.0f));
                addBoxCollider2D(plat, 1.5f, 0.2f);
            }
        }

        // Horizontal moving platform (meadow)
        {
            ECS::Entity mp = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(mp, "Moving Platform Meadow");
            auto& t = m_World->AddComponent<ECS::TransformComponent>(mp);
            t.position = Math::Vector3(-3.0f, 5.5f, 0.0f);
            t.scale = Math::Vector3(2.5f, 0.3f, 1.0f);
            auto& mat = m_World->AddComponent<ECS::MaterialComponent>(mp);
            mat.baseColor = Math::Vector3(0.3f, 0.55f, 0.3f);
            m_World->AddComponent<ECS::MeshComponent>(mp, Renderer::MeshFactory::CreateQuad(1.0f, 1.0f));
            addBoxCollider2D(mp, 1.25f, 0.15f);
            auto& tw = m_World->AddComponent<ECS::TweenComponent>(mp);
            tw.autoPlay = true;
            ECS::TweenEntry move;
            move.property = ECS::TweenProperty::Position;
            move.easing = ECS::EasingType::EaseInOutSine;
            move.mode = ECS::TweenMode::PingPong;
            move.startValue = Math::Vector3(-3.0f, 5.5f, 0.0f);
            move.endValue = Math::Vector3(4.0f, 5.5f, 0.0f);
            move.duration = 3.0f;
            move.useCurrentAsStart = false;
            tw.tweens.push_back(move);
        }

        // Slime ×2 (meadow patrol)
        {
            const Math::Vector3 slimePos[] = {{4.0f, 0.0f, 0.0f}, {-5.0f, 0.0f, 0.0f}};
            const std::vector<Math::Vector3> slimePatrols[] = {
                {{2, 0, 0}, {8, 0, 0}},
                {{-8, 0, 0}, {-2, 0, 0}},
            };
            for (int i = 0; i < 2; ++i) {
                ECS::Entity e = m_World->CreateEntity();
                m_World->AddComponent<ECS::NameComponent>(e, "Slime " + std::to_string(i + 1));
                auto& t = m_World->AddComponent<ECS::TransformComponent>(e);
                t.position = slimePos[i];
                t.scale = Math::Vector3(1.0f, 0.6f, 1.0f);
                auto& mat = m_World->AddComponent<ECS::MaterialComponent>(e);
                mat.baseColor = Math::Vector3(0.4f, 0.8f, 0.3f);
                m_World->AddComponent<ECS::MeshComponent>(e, Renderer::MeshFactory::CreateCapsule2D(0.5f, 0.6f));
                auto& hp = m_World->AddComponent<ECS::HealthComponent>(e);
                hp.maxHealth = 20.0f; hp.currentHealth = 20.0f;
                auto& dmg = m_World->AddComponent<ECS::DamageComponent>(e);
                dmg.damage = 10.0f;
                auto& ai = m_World->AddComponent<ECS::AIControllerComponent>(e);
                ai.currentState = ECS::AIControllerComponent::AIState::Patrol;
                ai.patrolPoints = slimePatrols[i];
                ai.moveSpeed = 2.0f;
                ai.is2D = true;
                m_World->AddComponent<ECS::TagComponent>(e).tags.push_back("enemy");
                // Sensor body — AI controls movement, sensor enables damage callbacks
                auto& col = m_World->AddComponent<Physics::Body2DComponent>(e);
                col.shapeType = Physics::Shape2DType::Box;
                col.box.halfExtents = Math::Vector2(0.5f, 0.3f);
                col.isSensor = true;
                col.gravityScale = 0.0f;
            }
        }

        // Meadow coins ×3 (along platforms)
        {
            const Math::Vector3 coinPos[] = {{-4.0f, 2.5f, 0.0f}, {1.0f, 4.0f, 0.0f}, {6.0f, 6.0f, 0.0f}};
            for (int i = 0; i < 3; ++i) {
                ECS::Entity e = m_World->CreateEntity();
                m_World->AddComponent<ECS::NameComponent>(e, "Coin M" + std::to_string(i + 1));
                auto& t = m_World->AddComponent<ECS::TransformComponent>(e);
                t.position = coinPos[i];
                t.scale = Math::Vector3(0.5f, 0.5f, 1.0f);
                auto& mat = m_World->AddComponent<ECS::MaterialComponent>(e);
                mat.baseColor = Math::Vector3(1.0f, 0.85f, 0.2f);
                mat.emissiveColor = Math::Vector3(1.0f, 0.85f, 0.2f);
                mat.emissiveStrength = 0.3f;
                m_World->AddComponent<ECS::MeshComponent>(e, Renderer::MeshFactory::CreateQuad(1.0f, 1.0f));
                auto& pick = m_World->AddComponent<ECS::PickupComponent>(e);
                pick.type = ECS::PickupComponent::PickupType::Coin;
                pick.value = 1.0f;
                // Sensor body for pickup detection
                auto& col = m_World->AddComponent<Physics::Body2DComponent>(e);
                col.shapeType = Physics::Shape2DType::Box;
                col.box.halfExtents = Math::Vector2(0.25f, 0.25f);
                col.isSensor = true;
                col.gravityScale = 0.0f;
                auto& tw = m_World->AddComponent<ECS::TweenComponent>(e);
                tw.autoPlay = true;
                ECS::TweenEntry bob;
                bob.property = ECS::TweenProperty::Position;
                bob.easing = ECS::EasingType::EaseInOutSine;
                bob.mode = ECS::TweenMode::PingPong;
                bob.startValue = coinPos[i];
                bob.endValue = coinPos[i] + Math::Vector3(0, 0.4f, 0);
                bob.duration = 1.0f;
                bob.useCurrentAsStart = false;
                tw.tweens.push_back(bob);
            }
        }

        // Torch (meadow, Sparks)
        {
            ECS::Entity torch = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(torch, "Torch Meadow");
            auto& t = m_World->AddComponent<ECS::TransformComponent>(torch);
            t.position = Math::Vector3(5.0f, 0.5f, 0.0f);
            t.scale = Math::Vector3(0.3f, 0.8f, 1.0f);
            auto& mat = m_World->AddComponent<ECS::MaterialComponent>(torch);
            mat.baseColor = Math::Vector3(0.4f, 0.25f, 0.1f);
            mat.emissiveColor = Math::Vector3(1.0f, 0.5f, 0.1f);
            mat.emissiveStrength = 0.4f;
            m_World->AddComponent<ECS::MeshComponent>(torch, Renderer::MeshFactory::CreateQuad(1.0f, 1.0f));
            auto& pe = m_World->AddComponent<ECS::ParticleEmitterComponent>(torch);
            ECS::ApplyParticlePreset(pe, "Sparks");
        }

        // ═══════════════════════════════════════════════════
        // Zone 2 — Cave (x: 14 to 34)
        // ═══════════════════════════════════════════════════

        // Cave ground (lower than meadow — player drops in)
        {
            ECS::Entity ground = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(ground, "Ground Cave");
            auto& t = m_World->AddComponent<ECS::TransformComponent>(ground);
            t.position = Math::Vector3(24.0f, -3.0f, -0.1f);
            t.scale = Math::Vector3(20.0f, 1.0f, 1.0f);
            auto& mat = m_World->AddComponent<ECS::MaterialComponent>(ground);
            mat.baseColor = Math::Vector3(0.3f, 0.28f, 0.25f);
            mat.roughness = 0.95f;
            m_World->AddComponent<ECS::MeshComponent>(ground, Renderer::MeshFactory::CreateQuad(1.0f, 1.0f));
            addBoxCollider2D(ground, 10.0f, 0.5f);
        }

        // Cave ceiling
        {
            ECS::Entity ceil = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(ceil, "Cave Ceiling");
            auto& t = m_World->AddComponent<ECS::TransformComponent>(ceil);
            t.position = Math::Vector3(24.0f, 8.0f, 0.0f);
            t.scale = Math::Vector3(20.0f, 0.5f, 1.0f);
            auto& mat = m_World->AddComponent<ECS::MaterialComponent>(ceil);
            mat.baseColor = Math::Vector3(0.25f, 0.22f, 0.2f);
            m_World->AddComponent<ECS::MeshComponent>(ceil, Renderer::MeshFactory::CreateQuad(1.0f, 1.0f));
            addBoxCollider2D(ceil, 10.0f, 0.25f);
        }

        // Spike hazards ×2 (cave)
        {
            const Math::Vector3 spikePos[] = {{18.0f, -2.4f, 0.0f}, {28.0f, -2.4f, 0.0f}};
            for (int i = 0; i < 2; ++i) {
                ECS::Entity spikes = m_World->CreateEntity();
                m_World->AddComponent<ECS::NameComponent>(spikes, "Spikes " + std::to_string(i + 1));
                auto& t = m_World->AddComponent<ECS::TransformComponent>(spikes);
                t.position = spikePos[i];
                t.scale = Math::Vector3(2.0f, 0.4f, 1.0f);
                auto& mat = m_World->AddComponent<ECS::MaterialComponent>(spikes);
                mat.baseColor = Math::Vector3(0.6f, 0.15f, 0.15f);
                m_World->AddComponent<ECS::MeshComponent>(spikes, Renderer::MeshFactory::CreateTriangle(1.0f));
                auto& dmg = m_World->AddComponent<ECS::DamageComponent>(spikes);
                dmg.damage = 25.0f;
                dmg.destroyOnHit = false;
                dmg.damageOnce = true;
                auto& col = m_World->AddComponent<Physics::Body2DComponent>(spikes);
                col.shapeType = Physics::Shape2DType::Polygon;
                // Triangle collider matching the spike mesh visual (pointy top, flat base)
                col.polygon.vertices = {
                    Math::Vector2(-1.0f, -0.2f),  // Bottom-left
                    Math::Vector2( 1.0f, -0.2f),  // Bottom-right
                    Math::Vector2( 0.0f,  0.2f),  // Top point
                };
                col.isStatic = true;
                col.isSensor = true;
            }
        }

        // Lava pit (continuous damage)
        {
            ECS::Entity lava = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(lava, "Lava Pit");
            auto& t = m_World->AddComponent<ECS::TransformComponent>(lava);
            t.position = Math::Vector3(30.0f, -3.5f, 0.0f);
            t.scale = Math::Vector3(4.0f, 0.5f, 1.0f);
            auto& mat = m_World->AddComponent<ECS::MaterialComponent>(lava);
            mat.baseColor = Math::Vector3(0.9f, 0.2f, 0.05f);
            mat.emissiveColor = Math::Vector3(1.0f, 0.3f, 0.05f);
            mat.emissiveStrength = 0.8f;
            m_World->AddComponent<ECS::MeshComponent>(lava, Renderer::MeshFactory::CreateQuad(1.0f, 1.0f));
            auto& dmg = m_World->AddComponent<ECS::DamageComponent>(lava);
            dmg.damage = 15.0f;
            dmg.destroyOnHit = false;
            dmg.damageOnce = false;
            dmg.damageInterval = 0.5f;
            auto& col = m_World->AddComponent<Physics::Body2DComponent>(lava);
            col.shapeType = Physics::Shape2DType::Box;
            col.box.halfExtents = Math::Vector2(2.0f, 0.25f);
            col.isStatic = true;
            col.isSensor = true;
        }

        // Bat ×2 (vertical patrol, cave)
        {
            const Math::Vector3 batPos[] = {{20.0f, 2.0f, 0.0f}, {26.0f, 4.0f, 0.0f}};
            const std::vector<Math::Vector3> batPatrols[] = {
                {{20, 0, 0}, {20, 6, 0}},
                {{26, 1, 0}, {26, 7, 0}},
            };
            for (int i = 0; i < 2; ++i) {
                ECS::Entity e = m_World->CreateEntity();
                m_World->AddComponent<ECS::NameComponent>(e, "Bat " + std::to_string(i + 1));
                auto& t = m_World->AddComponent<ECS::TransformComponent>(e);
                t.position = batPos[i];
                t.scale = Math::Vector3(0.8f, 0.5f, 1.0f);
                auto& mat = m_World->AddComponent<ECS::MaterialComponent>(e);
                mat.baseColor = Math::Vector3(0.5f, 0.2f, 0.6f);
                m_World->AddComponent<ECS::MeshComponent>(e, Renderer::MeshFactory::CreateCapsule2D(0.4f, 0.5f));
                auto& hp = m_World->AddComponent<ECS::HealthComponent>(e);
                hp.maxHealth = 15.0f; hp.currentHealth = 15.0f;
                auto& dmg = m_World->AddComponent<ECS::DamageComponent>(e);
                dmg.damage = 8.0f;
                auto& ai = m_World->AddComponent<ECS::AIControllerComponent>(e);
                ai.currentState = ECS::AIControllerComponent::AIState::Patrol;
                ai.patrolPoints = batPatrols[i];
                ai.moveSpeed = 3.0f;
                ai.detectionRange = 8.0f;
                ai.is2D = true;
                m_World->AddComponent<ECS::TagComponent>(e).tags.push_back("enemy");
                // Sensor body — AI controls movement, sensor enables damage callbacks
                auto& col = m_World->AddComponent<Physics::Body2DComponent>(e);
                col.shapeType = Physics::Shape2DType::Box;
                col.box.halfExtents = Math::Vector2(0.4f, 0.25f);
                col.isSensor = true;
                col.gravityScale = 0.0f;
            }
        }

        // Destructible crates ×3 (cave, drop coins)
        {
            const Math::Vector3 cratePos[] = {{16.0f, -2.0f, 0.0f}, {22.0f, -2.0f, 0.0f}, {32.0f, -2.0f, 0.0f}};
            for (int i = 0; i < 3; ++i) {
                ECS::Entity e = m_World->CreateEntity();
                m_World->AddComponent<ECS::NameComponent>(e, "Crate " + std::to_string(i + 1));
                auto& t = m_World->AddComponent<ECS::TransformComponent>(e);
                t.position = cratePos[i];
                t.scale = Math::Vector3(1.5f, 1.5f, 1.0f);
                auto& mat = m_World->AddComponent<ECS::MaterialComponent>(e);
                mat.baseColor = Math::Vector3(0.55f, 0.4f, 0.25f);
                m_World->AddComponent<ECS::MeshComponent>(e, Renderer::MeshFactory::CreateQuad(1.0f, 1.0f));
                addBoxCollider2D(e, 0.75f, 0.75f);
                auto& dest = m_World->AddComponent<ECS::DestructibleComponent>(e);
                dest.health = 3.0f;
                dest.destroyOnHit = false;
                dest.spawnPickup = true;
                dest.pickupId = "coin";
                dest.pickupCount = 1;
            }
        }

        // Health potion (cave)
        {
            ECS::Entity e = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(e, "Health Potion");
            auto& t = m_World->AddComponent<ECS::TransformComponent>(e);
            t.position = Math::Vector3(22.0f, -1.5f, 0.0f);
            t.scale = Math::Vector3(0.8f, 0.8f, 1.0f);
            auto& mat = m_World->AddComponent<ECS::MaterialComponent>(e);
            mat.baseColor = Math::Vector3(0.2f, 0.9f, 0.3f);
            mat.emissiveColor = Math::Vector3(0.2f, 0.9f, 0.3f);
            mat.emissiveStrength = 0.3f;
            m_World->AddComponent<ECS::MeshComponent>(e, Renderer::MeshFactory::CreateQuad(1.0f, 1.0f));
            auto& pick = m_World->AddComponent<ECS::PickupComponent>(e);
            pick.type = ECS::PickupComponent::PickupType::Health;
            pick.value = 25.0f;
            pick.magnetToPlayer = true;
            pick.magnetRange = 3.0f;
            // Sensor body for pickup detection
            auto& col = m_World->AddComponent<Physics::Body2DComponent>(e);
            col.shapeType = Physics::Shape2DType::Box;
            col.box.halfExtents = Math::Vector2(0.4f, 0.4f);
            col.isSensor = true;
            col.gravityScale = 0.0f;
            auto& tw = m_World->AddComponent<ECS::TweenComponent>(e);
            tw.autoPlay = true;
            ECS::TweenEntry bob;
            bob.property = ECS::TweenProperty::Position;
            bob.easing = ECS::EasingType::EaseInOutSine;
            bob.mode = ECS::TweenMode::PingPong;
            bob.startValue = Math::Vector3(22.0f, -1.5f, 0.0f);
            bob.endValue = Math::Vector3(22.0f, -1.0f, 0.0f);
            bob.duration = 1.2f;
            bob.useCurrentAsStart = false;
            tw.tweens.push_back(bob);
        }

        // Key (cave, guarded by bats)
        {
            ECS::Entity e = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(e, "Cave Key");
            auto& t = m_World->AddComponent<ECS::TransformComponent>(e);
            t.position = Math::Vector3(26.0f, -1.5f, 0.0f);
            t.scale = Math::Vector3(0.6f, 0.6f, 1.0f);
            auto& mat = m_World->AddComponent<ECS::MaterialComponent>(e);
            mat.baseColor = Math::Vector3(1.0f, 0.9f, 0.2f);
            mat.emissiveColor = Math::Vector3(1.0f, 0.9f, 0.2f);
            mat.emissiveStrength = 0.5f;
            m_World->AddComponent<ECS::MeshComponent>(e, Renderer::MeshFactory::CreateQuad(1.0f, 1.0f));
            auto& pick = m_World->AddComponent<ECS::PickupComponent>(e);
            pick.type = ECS::PickupComponent::PickupType::Key;
            pick.value = 1.0f;
            // Sensor body for pickup detection
            auto& col = m_World->AddComponent<Physics::Body2DComponent>(e);
            col.shapeType = Physics::Shape2DType::Box;
            col.box.halfExtents = Math::Vector2(0.3f, 0.3f);
            col.isSensor = true;
            col.gravityScale = 0.0f;
            auto& tw = m_World->AddComponent<ECS::TweenComponent>(e);
            tw.autoPlay = true;
            ECS::TweenEntry bob;
            bob.property = ECS::TweenProperty::Position;
            bob.easing = ECS::EasingType::EaseInOutSine;
            bob.mode = ECS::TweenMode::PingPong;
            bob.startValue = Math::Vector3(26.0f, -1.5f, 0.0f);
            bob.endValue = Math::Vector3(26.0f, -1.0f, 0.0f);
            bob.duration = 1.4f;
            bob.useCurrentAsStart = false;
            tw.tweens.push_back(bob);
        }

        // Vertical moving platform (cave)
        {
            ECS::Entity mp = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(mp, "Moving Platform Cave");
            auto& t = m_World->AddComponent<ECS::TransformComponent>(mp);
            t.position = Math::Vector3(34.0f, -2.0f, 0.0f);
            t.scale = Math::Vector3(2.5f, 0.3f, 1.0f);
            auto& mat = m_World->AddComponent<ECS::MaterialComponent>(mp);
            mat.baseColor = Math::Vector3(0.4f, 0.35f, 0.3f);
            m_World->AddComponent<ECS::MeshComponent>(mp, Renderer::MeshFactory::CreateQuad(1.0f, 1.0f));
            addBoxCollider2D(mp, 1.25f, 0.15f);
            auto& tw = m_World->AddComponent<ECS::TweenComponent>(mp);
            tw.autoPlay = true;
            ECS::TweenEntry move;
            move.property = ECS::TweenProperty::Position;
            move.easing = ECS::EasingType::EaseInOutSine;
            move.mode = ECS::TweenMode::PingPong;
            move.startValue = Math::Vector3(34.0f, -2.0f, 0.0f);
            move.endValue = Math::Vector3(34.0f, 5.0f, 0.0f);
            move.duration = 4.0f;
            move.useCurrentAsStart = false;
            tw.tweens.push_back(move);
        }

        // ═══════════════════════════════════════════════════
        // Zone 3 — Tower (x: 38 to 48)
        // ═══════════════════════════════════════════════════

        // Tower ground
        {
            ECS::Entity ground = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(ground, "Ground Tower");
            auto& t = m_World->AddComponent<ECS::TransformComponent>(ground);
            t.position = Math::Vector3(43.0f, -3.0f, -0.1f);
            t.scale = Math::Vector3(10.0f, 1.0f, 1.0f);
            auto& mat = m_World->AddComponent<ECS::MaterialComponent>(ground);
            mat.baseColor = Math::Vector3(0.4f, 0.38f, 0.35f);
            mat.roughness = 0.95f;
            m_World->AddComponent<ECS::MeshComponent>(ground, Renderer::MeshFactory::CreateQuad(1.0f, 1.0f));
            addBoxCollider2D(ground, 5.0f, 0.5f);
        }

        // Tower walls (left + right)
        {
            const Math::Vector3 wallPos[] = {{38.0f, 7.0f, 0.0f}, {48.0f, 7.0f, 0.0f}};
            const char* wallNames[] = {"Tower Wall Left", "Tower Wall Right"};
            for (int i = 0; i < 2; ++i) {
                ECS::Entity e = m_World->CreateEntity();
                m_World->AddComponent<ECS::NameComponent>(e, wallNames[i]);
                auto& t = m_World->AddComponent<ECS::TransformComponent>(e);
                t.position = wallPos[i];
                t.scale = Math::Vector3(0.5f, 20.0f, 1.0f);
                auto& mat = m_World->AddComponent<ECS::MaterialComponent>(e);
                mat.baseColor = Math::Vector3(0.35f, 0.33f, 0.3f);
                m_World->AddComponent<ECS::MeshComponent>(e, Renderer::MeshFactory::CreateQuad(1.0f, 1.0f));
                addBoxCollider2D(e, 0.25f, 10.0f);
            }
        }

        // Locked door (base of tower)
        {
            ECS::Entity e = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(e, "Locked Door");
            auto& t = m_World->AddComponent<ECS::TransformComponent>(e);
            t.position = Math::Vector3(43.0f, -1.0f, 0.0f);
            t.scale = Math::Vector3(3.0f, 0.5f, 1.0f);
            auto& mat = m_World->AddComponent<ECS::MaterialComponent>(e);
            mat.baseColor = Math::Vector3(0.45f, 0.3f, 0.15f);
            m_World->AddComponent<ECS::MeshComponent>(e, Renderer::MeshFactory::CreateQuad(1.0f, 1.0f));
            addBoxCollider2D(e, 1.5f, 0.25f);
            auto& sw = m_World->AddComponent<ECS::SwitchComponent>(e);
            sw.type = ECS::SwitchComponent::SwitchType::Toggle;
            sw.promptText = "Unlock Door (requires key)";
        }

        // 6 ascending platforms (alternating left/right)
        {
            const Math::Vector3 platPos[] = {
                {40.0f, 0.0f, 0.0f}, {46.0f, 2.5f, 0.0f}, {40.0f, 5.0f, 0.0f},
                {46.0f, 8.0f, 0.0f}, {40.0f, 11.0f, 0.0f}, {46.0f, 14.0f, 0.0f},
            };
            for (int i = 0; i < 6; ++i) {
                ECS::Entity plat = m_World->CreateEntity();
                m_World->AddComponent<ECS::NameComponent>(plat, "Tower Platform " + std::to_string(i + 1));
                auto& t = m_World->AddComponent<ECS::TransformComponent>(plat);
                t.position = platPos[i];
                t.scale = Math::Vector3(3.0f, 0.4f, 1.0f);
                auto& mat = m_World->AddComponent<ECS::MaterialComponent>(plat);
                mat.baseColor = Math::Vector3(0.42f, 0.4f, 0.38f);
                m_World->AddComponent<ECS::MeshComponent>(plat, Renderer::MeshFactory::CreateQuad(1.0f, 1.0f));
                addBoxCollider2D(plat, 1.5f, 0.2f);
            }
        }

        // Vertical moving platform (tower)
        {
            ECS::Entity mp = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(mp, "Moving Platform Tower");
            auto& t = m_World->AddComponent<ECS::TransformComponent>(mp);
            t.position = Math::Vector3(43.0f, 6.0f, 0.0f);
            t.scale = Math::Vector3(2.0f, 0.3f, 1.0f);
            auto& mat = m_World->AddComponent<ECS::MaterialComponent>(mp);
            mat.baseColor = Math::Vector3(0.4f, 0.38f, 0.35f);
            m_World->AddComponent<ECS::MeshComponent>(mp, Renderer::MeshFactory::CreateQuad(1.0f, 1.0f));
            addBoxCollider2D(mp, 1.0f, 0.15f);
            auto& tw = m_World->AddComponent<ECS::TweenComponent>(mp);
            tw.autoPlay = true;
            ECS::TweenEntry move;
            move.property = ECS::TweenProperty::Position;
            move.easing = ECS::EasingType::EaseInOutSine;
            move.mode = ECS::TweenMode::PingPong;
            move.startValue = Math::Vector3(43.0f, 6.0f, 0.0f);
            move.endValue = Math::Vector3(43.0f, 12.0f, 0.0f);
            move.duration = 3.5f;
            move.useCurrentAsStart = false;
            tw.tweens.push_back(move);
        }

        // Spike wall hazard (mid-climb)
        {
            ECS::Entity spikes = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(spikes, "Spike Wall");
            auto& t = m_World->AddComponent<ECS::TransformComponent>(spikes);
            t.position = Math::Vector3(43.0f, 7.0f, 0.0f);
            t.scale = Math::Vector3(4.0f, 0.4f, 1.0f);
            auto& mat = m_World->AddComponent<ECS::MaterialComponent>(spikes);
            mat.baseColor = Math::Vector3(0.6f, 0.15f, 0.15f);
            m_World->AddComponent<ECS::MeshComponent>(spikes, Renderer::MeshFactory::CreateTriangle(1.0f));
            auto& dmg = m_World->AddComponent<ECS::DamageComponent>(spikes);
            dmg.damage = 25.0f;
            dmg.destroyOnHit = false;
            dmg.damageOnce = true;
            auto& col = m_World->AddComponent<Physics::Body2DComponent>(spikes);
            col.shapeType = Physics::Shape2DType::Box;
            col.box.halfExtents = Math::Vector2(2.0f, 0.2f);
            col.isStatic = true;
            col.isSensor = true;
        }

        // Flyer enemy (tower, vertical patrol)
        {
            ECS::Entity e = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(e, "Flyer");
            auto& t = m_World->AddComponent<ECS::TransformComponent>(e);
            t.position = Math::Vector3(43.0f, 10.0f, 0.0f);
            t.scale = Math::Vector3(0.9f, 0.6f, 1.0f);
            auto& mat = m_World->AddComponent<ECS::MaterialComponent>(e);
            mat.baseColor = Math::Vector3(0.9f, 0.5f, 0.1f);
            m_World->AddComponent<ECS::MeshComponent>(e, Renderer::MeshFactory::CreateCapsule2D(0.45f, 0.6f));
            auto& hp = m_World->AddComponent<ECS::HealthComponent>(e);
            hp.maxHealth = 25.0f; hp.currentHealth = 25.0f;
            auto& dmg = m_World->AddComponent<ECS::DamageComponent>(e);
            dmg.damage = 12.0f;
            auto& ai = m_World->AddComponent<ECS::AIControllerComponent>(e);
            ai.currentState = ECS::AIControllerComponent::AIState::Patrol;
            ai.patrolPoints = {{43, 4, 0}, {43, 14, 0}};
            ai.moveSpeed = 2.5f;
            ai.detectionRange = 10.0f;
            ai.is2D = true;
            m_World->AddComponent<ECS::TagComponent>(e).tags.push_back("enemy");
            // Sensor body — AI controls movement, sensor enables damage callbacks
            auto& col = m_World->AddComponent<Physics::Body2DComponent>(e);
            col.shapeType = Physics::Shape2DType::Box;
            col.box.halfExtents = Math::Vector2(0.45f, 0.3f);
            col.isSensor = true;
            col.gravityScale = 0.0f;
        }

        // Torches ×2 (tower walls, Fire)
        {
            const Math::Vector3 torchPos[] = {{39.0f, 3.0f, 0.0f}, {47.0f, 10.0f, 0.0f}};
            for (int i = 0; i < 2; ++i) {
                ECS::Entity e = m_World->CreateEntity();
                m_World->AddComponent<ECS::NameComponent>(e, "Torch Tower " + std::to_string(i + 1));
                auto& t = m_World->AddComponent<ECS::TransformComponent>(e);
                t.position = torchPos[i];
                t.scale = Math::Vector3(0.3f, 0.6f, 1.0f);
                auto& mat = m_World->AddComponent<ECS::MaterialComponent>(e);
                mat.baseColor = Math::Vector3(0.8f, 0.4f, 0.1f);
                mat.emissiveColor = Math::Vector3(1.0f, 0.6f, 0.1f);
                mat.emissiveStrength = 0.8f;
                m_World->AddComponent<ECS::MeshComponent>(e, Renderer::MeshFactory::CreateQuad(1.0f, 1.0f));
                auto& pe = m_World->AddComponent<ECS::ParticleEmitterComponent>(e);
                ECS::ApplyParticlePreset(pe, "Fire");
                pe.emissionRate = 20.0f;
                pe.maxParticles = 128;
            }
        }

        // Tower coins ×3 (along the climb)
        {
            const Math::Vector3 coinPos[] = {{40.0f, 1.0f, 0.0f}, {46.0f, 9.0f, 0.0f}, {40.0f, 12.0f, 0.0f}};
            for (int i = 0; i < 3; ++i) {
                ECS::Entity e = m_World->CreateEntity();
                m_World->AddComponent<ECS::NameComponent>(e, "Coin T" + std::to_string(i + 1));
                auto& t = m_World->AddComponent<ECS::TransformComponent>(e);
                t.position = coinPos[i];
                t.scale = Math::Vector3(0.5f, 0.5f, 1.0f);
                auto& mat = m_World->AddComponent<ECS::MaterialComponent>(e);
                mat.baseColor = Math::Vector3(1.0f, 0.85f, 0.2f);
                mat.emissiveColor = Math::Vector3(1.0f, 0.85f, 0.2f);
                mat.emissiveStrength = 0.3f;
                m_World->AddComponent<ECS::MeshComponent>(e, Renderer::MeshFactory::CreateQuad(1.0f, 1.0f));
                auto& pick = m_World->AddComponent<ECS::PickupComponent>(e);
                pick.type = ECS::PickupComponent::PickupType::Coin;
                pick.value = 1.0f;
                // Sensor body for pickup detection
                auto& col = m_World->AddComponent<Physics::Body2DComponent>(e);
                col.shapeType = Physics::Shape2DType::Box;
                col.box.halfExtents = Math::Vector2(0.25f, 0.25f);
                col.isSensor = true;
                col.gravityScale = 0.0f;
                auto& tw = m_World->AddComponent<ECS::TweenComponent>(e);
                tw.autoPlay = true;
                ECS::TweenEntry bob;
                bob.property = ECS::TweenProperty::Position;
                bob.easing = ECS::EasingType::EaseInOutSine;
                bob.mode = ECS::TweenMode::PingPong;
                bob.startValue = coinPos[i];
                bob.endValue = coinPos[i] + Math::Vector3(0, 0.4f, 0);
                bob.duration = 1.0f;
                bob.useCurrentAsStart = false;
                tw.tweens.push_back(bob);
            }
        }

        // ═══════════════════════════════════════════════════
        // Zone 4 — Sky (x: 48 to 66)
        // ═══════════════════════════════════════════════════

        // Sky platforms (no ground — all floating)
        {
            const Math::Vector3 skyPos[] = {
                {52.0f, 16.0f, 0.0f}, {56.0f, 17.5f, 0.0f},
                {60.0f, 18.0f, 0.0f}, {64.0f, 19.0f, 0.0f},
            };
            for (int i = 0; i < 4; ++i) {
                ECS::Entity plat = m_World->CreateEntity();
                m_World->AddComponent<ECS::NameComponent>(plat, "Sky Platform " + std::to_string(i + 1));
                auto& t = m_World->AddComponent<ECS::TransformComponent>(plat);
                t.position = skyPos[i];
                t.scale = Math::Vector3(3.0f, 0.4f, 1.0f);
                auto& mat = m_World->AddComponent<ECS::MaterialComponent>(plat);
                mat.baseColor = Math::Vector3(0.6f, 0.7f, 0.85f);
                m_World->AddComponent<ECS::MeshComponent>(plat, Renderer::MeshFactory::CreateQuad(1.0f, 1.0f));
                addBoxCollider2D(plat, 1.5f, 0.2f);
            }
        }

        // Sky Boss (final platform)
        {
            ECS::Entity e = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(e, "Sky Boss");
            auto& t = m_World->AddComponent<ECS::TransformComponent>(e);
            t.position = Math::Vector3(64.0f, 20.5f, 0.0f);
            t.scale = Math::Vector3(1.8f, 1.8f, 1.0f);
            auto& mat = m_World->AddComponent<ECS::MaterialComponent>(e);
            mat.baseColor = Math::Vector3(0.6f, 0.1f, 0.1f);
            mat.emissiveColor = Math::Vector3(0.8f, 0.15f, 0.1f);
            mat.emissiveStrength = 0.5f;
            m_World->AddComponent<ECS::MeshComponent>(e, Renderer::MeshFactory::CreateCapsule2D(0.8f, 1.8f));
            auto& hp = m_World->AddComponent<ECS::HealthComponent>(e);
            hp.maxHealth = 100.0f; hp.currentHealth = 100.0f;
            auto& dmg = m_World->AddComponent<ECS::DamageComponent>(e);
            dmg.damage = 20.0f;
            dmg.knockbackForce = 5.0f;
            auto& ai = m_World->AddComponent<ECS::AIControllerComponent>(e);
            ai.currentState = ECS::AIControllerComponent::AIState::Idle;
            ai.moveSpeed = 1.5f;
            ai.detectionRange = 12.0f;
            ai.is2D = true;
            m_World->AddComponent<ECS::TagComponent>(e).tags.push_back("enemy");
            // Sensor body — AI controls movement, sensor enables damage callbacks
            auto& col = m_World->AddComponent<Physics::Body2DComponent>(e);
            col.shapeType = Physics::Shape2DType::Box;
            col.box.halfExtents = Math::Vector2(0.8f, 0.9f);
            col.isSensor = true;
            col.gravityScale = 0.0f;
            auto& pe = m_World->AddComponent<ECS::ParticleEmitterComponent>(e);
            ECS::ApplyParticlePreset(pe, "Sparks");
        }

        // Speed boost (past boss)
        {
            ECS::Entity e = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(e, "Speed Boost");
            auto& t = m_World->AddComponent<ECS::TransformComponent>(e);
            t.position = Math::Vector3(64.0f, 22.0f, 0.0f);
            auto& mat = m_World->AddComponent<ECS::MaterialComponent>(e);
            mat.baseColor = Math::Vector3(0.2f, 0.6f, 1.0f);
            mat.emissiveColor = Math::Vector3(0.2f, 0.6f, 1.0f);
            mat.emissiveStrength = 0.6f;
            m_World->AddComponent<ECS::MeshComponent>(e, Renderer::MeshFactory::CreateCapsule2D(0.3f, 0.6f));
            auto& pick = m_World->AddComponent<ECS::PickupComponent>(e);
            pick.type = ECS::PickupComponent::PickupType::Powerup;
            pick.customId = "speed_boost";
            pick.value = 1.0f;
            // Sensor body for pickup detection
            auto& col = m_World->AddComponent<Physics::Body2DComponent>(e);
            col.shapeType = Physics::Shape2DType::Box;
            col.box.halfExtents = Math::Vector2(0.3f, 0.3f);
            col.isSensor = true;
            col.gravityScale = 0.0f;
        }

        // Teleporter (sky ↔ meadow)
        {
            ECS::Entity padA = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(padA, "Teleporter Sky");
            auto& tA = m_World->AddComponent<ECS::TransformComponent>(padA);
            tA.position = Math::Vector3(52.0f, 17.0f, 0.0f);
            tA.scale = Math::Vector3(1.5f, 1.5f, 1.0f);
            auto& matA = m_World->AddComponent<ECS::MaterialComponent>(padA);
            matA.baseColor = Math::Vector3(0.5f, 0.1f, 0.8f);
            matA.emissiveColor = Math::Vector3(0.6f, 0.2f, 1.0f);
            matA.emissiveStrength = 0.5f;
            m_World->AddComponent<ECS::MeshComponent>(padA, Renderer::MeshFactory::CreateQuad(1.0f, 1.0f));
            auto& telA = m_World->AddComponent<ECS::TeleporterComponent>(padA);
            telA.targetPosition = Math::Vector3(0, 1, 0);
            telA.cooldown = 1.0f;
            auto& peA = m_World->AddComponent<ECS::ParticleEmitterComponent>(padA);
            ECS::ApplyParticlePreset(peA, "Magic");
            peA.emissionRate = 15.0f;
            peA.maxParticles = 128;

            ECS::Entity padB = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(padB, "Teleporter Meadow");
            auto& tB = m_World->AddComponent<ECS::TransformComponent>(padB);
            tB.position = Math::Vector3(0.0f, 1.0f, 0.0f);
            tB.scale = Math::Vector3(1.5f, 1.5f, 1.0f);
            auto& matB = m_World->AddComponent<ECS::MaterialComponent>(padB);
            matB.baseColor = Math::Vector3(0.5f, 0.1f, 0.8f);
            matB.emissiveColor = Math::Vector3(0.6f, 0.2f, 1.0f);
            matB.emissiveStrength = 0.5f;
            m_World->AddComponent<ECS::MeshComponent>(padB, Renderer::MeshFactory::CreateQuad(1.0f, 1.0f));
            auto& telB = m_World->AddComponent<ECS::TeleporterComponent>(padB);
            telB.targetPosition = Math::Vector3(52, 17, 0);
            telB.cooldown = 1.0f;
            auto& peB = m_World->AddComponent<ECS::ParticleEmitterComponent>(padB);
            ECS::ApplyParticlePreset(peB, "Magic");
            peB.emissionRate = 15.0f;
            peB.maxParticles = 128;

            telA.linkedTeleporter = padB;
            telB.linkedTeleporter = padA;
        }

        // Sky coins ×2
        {
            const Math::Vector3 coinPos[] = {{56.0f, 18.5f, 0.0f}, {60.0f, 19.0f, 0.0f}};
            for (int i = 0; i < 2; ++i) {
                ECS::Entity e = m_World->CreateEntity();
                m_World->AddComponent<ECS::NameComponent>(e, "Coin S" + std::to_string(i + 1));
                auto& t = m_World->AddComponent<ECS::TransformComponent>(e);
                t.position = coinPos[i];
                t.scale = Math::Vector3(0.5f, 0.5f, 1.0f);
                auto& mat = m_World->AddComponent<ECS::MaterialComponent>(e);
                mat.baseColor = Math::Vector3(1.0f, 0.85f, 0.2f);
                mat.emissiveColor = Math::Vector3(1.0f, 0.85f, 0.2f);
                mat.emissiveStrength = 0.3f;
                m_World->AddComponent<ECS::MeshComponent>(e, Renderer::MeshFactory::CreateQuad(1.0f, 1.0f));
                auto& pick = m_World->AddComponent<ECS::PickupComponent>(e);
                pick.type = ECS::PickupComponent::PickupType::Coin;
                pick.value = 1.0f;
                // Sensor body for pickup detection
                auto& col = m_World->AddComponent<Physics::Body2DComponent>(e);
                col.shapeType = Physics::Shape2DType::Box;
                col.box.halfExtents = Math::Vector2(0.25f, 0.25f);
                col.isSensor = true;
                col.gravityScale = 0.0f;
                auto& tw = m_World->AddComponent<ECS::TweenComponent>(e);
                tw.autoPlay = true;
                ECS::TweenEntry bob;
                bob.property = ECS::TweenProperty::Position;
                bob.easing = ECS::EasingType::EaseInOutSine;
                bob.mode = ECS::TweenMode::PingPong;
                bob.startValue = coinPos[i];
                bob.endValue = coinPos[i] + Math::Vector3(0, 0.4f, 0);
                bob.duration = 1.0f;
                bob.useCurrentAsStart = false;
                tw.tweens.push_back(bob);
            }
        }

        // ═══════════════════════════════════════════════════
        // HUD
        // ═══════════════════════════════════════════════════
        {
            // Health Bar — reads from player's HealthComponent each frame
            ECS::Entity hud = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(hud, "Health Bar Overlay");
            m_World->AddComponent<ECS::TransformComponent>(hud);
            auto& hw = m_World->AddComponent<ECS::HUDWidgetComponent>(hud);
            hw.type = ECS::HUDWidgetComponent::WidgetType::HealthBar;
            hw.visible = true;
            hw.screenSpace = true;
            hw.anchorX = 0.05f;
            hw.anchorY = 0.05f;
            hw.fillColor = Math::Vector3(0.8f, 0.2f, 0.2f);
            hw.bgColor = Math::Vector3(0.2f, 0.2f, 0.2f);
            hw.sourceEntity = player;  // Link to player's HealthComponent for live updates

            // Coin Counter
            ECS::Entity coinHud = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(coinHud, "Coin Counter");
            m_World->AddComponent<ECS::TransformComponent>(coinHud);
            auto& tc = m_World->AddComponent<ECS::TextComponent>(coinHud);
            tc.text = "Coins: 0";
            tc.fontSize = 24.0f;
            tc.textColor = Math::Vector3(1.0f, 0.85f, 0.2f);
            m_World->AddComponent<ECS::TagComponent>(coinHud).tags.push_back("hud_coins");

            // Key Indicator
            ECS::Entity keyHud = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(keyHud, "Key Indicator");
            m_World->AddComponent<ECS::TransformComponent>(keyHud);
            auto& tk = m_World->AddComponent<ECS::TextComponent>(keyHud);
            tk.text = "Key: --";
            tk.fontSize = 24.0f;
            tk.textColor = Math::Vector3(1.0f, 0.9f, 0.2f);
            m_World->AddComponent<ECS::TagComponent>(keyHud).tags.push_back("hud_key");
        }

        // ═══════════════════════════════════════════════════
        // Notes
        // ═══════════════════════════════════════════════════
        {
            ECS::Entity notes = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(notes, "Template Notes");
            m_World->AddComponent<ECS::TransformComponent>(notes);
            auto& n = m_World->AddComponent<ECS::NotesComponent>(notes);
            n.notes = "2D Platformer — 4-Zone Side-Scrolling Adventure\n\n"
                "Level Layout:\n"
                "- Zone 1 Meadow (x:-10..10): Tutorial area, stepping platforms, wall jump wall\n"
                "- Zone 2 Cave (x:14..34): Hazards, lava pit, bats, destructible crates, key pickup\n"
                "- Zone 3 Tower (x:38..48): Vertical climb, locked door (needs key), ascending platforms\n"
                "- Zone 4 Sky (x:48..66): Floating platforms, boss fight, teleporter back to start\n\n"
                "Player:\n"
                "- Platformer2DController: wall jump, wall slide, coyote time, double jump\n"
                "- 100 HP, 1 HP/s regen (3s delay), 0.5s invulnerability\n"
                "- Inventory (10 slots)\n\n"
                "Enemies (7 total):\n"
                "- Slime x2 (green, 20 HP): Ground patrol in meadow\n"
                "- Bat x2 (purple, 15 HP): Vertical patrol in cave, chase range 8\n"
                "- Flyer (orange, 25 HP): Vertical patrol in tower, chase range 10\n"
                "- Sky Boss (dark red, 100 HP): Idle until triggered, knockback, Sparks aura\n\n"
                "Pickups: Coins x8, Health Potion x1, Key x1, Speed Boost x1\n"
                "Objects: Crates x3, Torches x3, Spikes x3, Lava pit, Locked door, Teleporter, Moving platforms x3\n"
                "HUD: Health bar, coin counter, key indicator\n\n"
                "Controls: WASD/Arrows to move, Space to jump (double tap for double jump)\n"
                "Wall jump: Jump toward wall + press away";
        }

        // --- Render settings ---
        m_RenderSystem->SetShadowsEnabled(false);
        m_RenderSystem->SetAmbientIntensity(0.1f);
        if (m_PostProcessing) {
            auto& pp = m_PostProcessing->GetSettings();
            pp.fxaaEnabled = 1;
            pp.vignetteEnabled = 1;
            pp.vignetteIntensity = 0.2f;
            pp.vignetteSmoothness = 0.6f;
        }

    } else if (templateId == "topdown2d") {
        // ═══════════════════════════════════════════════════
        // 2D Top-Down Action — Multi-room dungeon
        // ═══════════════════════════════════════════════════

        // --- Ground planes (per-room tints) ---
        {
            struct FloorDef { Math::Vector3 pos; Math::Vector3 scl; Math::Vector3 col; const char* name; };
            const FloorDef floors[] = {
                {{0, 0, -0.1f},   {20, 20, 1}, {0.25f, 0.25f, 0.28f}, "Ground Main"},
                {{13, 0, -0.1f},  {6, 4, 1},   {0.28f, 0.25f, 0.25f}, "Ground Corridor East"},
                {{22, 0, -0.1f},  {12, 12, 1}, {0.30f, 0.22f, 0.22f}, "Ground East Wing"},
                {{0, 12, -0.1f},  {4, 4, 1},   {0.25f, 0.22f, 0.28f}, "Ground Corridor North"},
                {{0, 18, -0.1f},  {8, 8, 1},   {0.20f, 0.18f, 0.25f}, "Ground North Alcove"},
            };
            for (auto& f : floors) {
                ECS::Entity e = m_World->CreateEntity();
                m_World->AddComponent<ECS::NameComponent>(e, f.name);
                auto& t = m_World->AddComponent<ECS::TransformComponent>(e);
                t.position = f.pos; t.scale = f.scl;
                auto& mat = m_World->AddComponent<ECS::MaterialComponent>(e);
                mat.baseColor = f.col; mat.roughness = 0.9f;
                m_World->AddComponent<ECS::MeshComponent>(e, Renderer::MeshFactory::CreateQuad(1.0f, 1.0f));
            }
        }

        // --- Wall segments (dungeon perimeter + room dividers) ---
        {
            struct WallDef { Math::Vector3 pos; f32 hw; f32 hh; };
            const WallDef walls[] = {
                // Main room
                {{0, -10, 0},    10.0f, 0.25f},   // South
                {{-10, 0, 0},    0.25f, 10.0f},   // West
                {{-6, 10, 0},    4.0f,  0.25f},   // North-L (gap for north corridor)
                {{6, 10, 0},     4.0f,  0.25f},   // North-R
                {{10, 6, 0},     0.25f, 4.0f},    // East-T (gap for east corridor)
                {{10, -6, 0},    0.25f, 4.0f},    // East-B
                // East corridor
                {{13, 2, 0},     3.0f,  0.25f},
                {{13, -2, 0},    3.0f,  0.25f},
                // East wing
                {{22, 6, 0},     6.0f,  0.25f},
                {{22, -6, 0},    6.0f,  0.25f},
                {{28, 0, 0},     0.25f, 6.25f},
                // North corridor
                {{-2, 12, 0},    0.25f, 2.0f},
                {{2, 12, 0},     0.25f, 2.0f},
                // North alcove
                {{0, 22, 0},     4.0f,  0.25f},
                {{-4, 18, 0},    0.25f, 4.0f},
                {{4, 18, 0},     0.25f, 4.0f},
                {{-3, 14, 0},    1.0f,  0.25f},   // South-L (gap for corridor)
                {{3, 14, 0},     1.0f,  0.25f},   // South-R
            };
            const Math::Vector3 wallColor(0.35f, 0.3f, 0.25f);
            int wIdx = 0;
            for (auto& w : walls) {
                ECS::Entity e = m_World->CreateEntity();
                m_World->AddComponent<ECS::NameComponent>(e, "Wall " + std::to_string(wIdx++));
                auto& t = m_World->AddComponent<ECS::TransformComponent>(e);
                t.position = w.pos;
                t.scale = Math::Vector3(w.hw * 2.0f, w.hh * 2.0f, 1.0f);
                auto& mat = m_World->AddComponent<ECS::MaterialComponent>(e);
                mat.baseColor = wallColor;
                m_World->AddComponent<ECS::MeshComponent>(e, Renderer::MeshFactory::CreateQuad(1.0f, 1.0f));
                addBoxCollider2D(e, w.hw, w.hh);
            }
        }

        // --- Player (enhanced) ---
        ECS::Entity player = createPlayer2D("Player");
        {
            auto& ctrl = m_World->AddComponent<ECS::TopDown2DController>(player);
            ctrl.moveSpeed = 5.0f;
            ctrl.enableDash = true;
            auto& hp = m_World->AddComponent<ECS::HealthComponent>(player);
            hp.maxHealth = 100.0f;
            hp.currentHealth = 100.0f;
            hp.regenRate = 2.0f;
            hp.regenDelay = 3.0f;
            hp.invulnerabilityTime = 0.3f;
            auto& inv = m_World->AddComponent<ECS::InventoryComponent>(player);
            inv.maxSlots = 10;
            m_World->AddComponent<ECS::TagComponent>(player).tags.push_back("player");
            addCapsuleCollider2D(player, 0.4f, 1.0f, false);
        }

        // --- Camera ---
        {
            SetupCameraForController(player, "TopDown2D");
            ECS::Entity cam = ECS::CameraManager::GetActiveCamera(m_World);
            if (cam != ECS::INVALID_ENTITY) {
                auto& follow = m_World->AddComponent<ECS::Camera2DBoundsComponent>(cam);
                follow.followTarget = player;
                follow.followSmoothing = 6.0f;
                follow.deadZoneSize = Math::Vector2(1.5f, 1.5f);
                follow.lookAheadDistance = 1.5f;
            }
        }

        // --- Enemies (4 types, 6 total) ---
        {
            // Scout ×2 (main room — diamond patrol, chase range 8)
            const Math::Vector3 scoutPos[] = {{5, 5, 0}, {-5, -5, 0}};
            const std::vector<Math::Vector3> scoutPatrols[] = {
                {{5, 5, 0}, {5, -5, 0}, {-5, -5, 0}, {-5, 5, 0}},
                {{-5, -5, 0}, {-5, 5, 0}, {5, 5, 0}, {5, -5, 0}},
            };
            for (int i = 0; i < 2; ++i) {
                ECS::Entity e = m_World->CreateEntity();
                m_World->AddComponent<ECS::NameComponent>(e, "Scout " + std::to_string(i + 1));
                auto& t = m_World->AddComponent<ECS::TransformComponent>(e);
                t.position = scoutPos[i];
                auto& mat = m_World->AddComponent<ECS::MaterialComponent>(e);
                mat.baseColor = Math::Vector3(0.9f, 0.2f, 0.2f);
                m_World->AddComponent<ECS::MeshComponent>(e, Renderer::MeshFactory::CreateCapsule2D(0.5f, 1.0f));
                auto& hp = m_World->AddComponent<ECS::HealthComponent>(e);
                hp.maxHealth = 30.0f; hp.currentHealth = 30.0f;
                auto& dmg = m_World->AddComponent<ECS::DamageComponent>(e);
                dmg.damage = 10.0f;
                auto& ai = m_World->AddComponent<ECS::AIControllerComponent>(e);
                ai.currentState = ECS::AIControllerComponent::AIState::Patrol;
                ai.moveSpeed = 3.5f;
                ai.is2D = true;
                ai.detectionRange = 8.0f;
                ai.patrolPoints = scoutPatrols[i];
                m_World->AddComponent<ECS::TagComponent>(e).tags.push_back("enemy");
            }

            // Brute (east wing — short patrol, knockback, chase range 6)
            {
                ECS::Entity e = m_World->CreateEntity();
                m_World->AddComponent<ECS::NameComponent>(e, "Brute");
                auto& t = m_World->AddComponent<ECS::TransformComponent>(e);
                t.position = Math::Vector3(22, 2, 0);
                auto& mat = m_World->AddComponent<ECS::MaterialComponent>(e);
                mat.baseColor = Math::Vector3(0.9f, 0.5f, 0.1f);
                m_World->AddComponent<ECS::MeshComponent>(e, Renderer::MeshFactory::CreateCapsule2D(0.6f, 1.2f));
                auto& hp = m_World->AddComponent<ECS::HealthComponent>(e);
                hp.maxHealth = 80.0f; hp.currentHealth = 80.0f;
                auto& dmg = m_World->AddComponent<ECS::DamageComponent>(e);
                dmg.damage = 25.0f;
                dmg.knockbackForce = 3.0f;
                auto& ai = m_World->AddComponent<ECS::AIControllerComponent>(e);
                ai.currentState = ECS::AIControllerComponent::AIState::Patrol;
                ai.moveSpeed = 1.5f;
                ai.is2D = true;
                ai.detectionRange = 6.0f;
                ai.patrolPoints = {{20, 2, 0}, {24, 2, 0}};
                m_World->AddComponent<ECS::TagComponent>(e).tags.push_back("enemy");
            }

            // Archer (east wing — ranged patrol, chase 12, flees when close)
            {
                ECS::Entity e = m_World->CreateEntity();
                m_World->AddComponent<ECS::NameComponent>(e, "Archer");
                auto& t = m_World->AddComponent<ECS::TransformComponent>(e);
                t.position = Math::Vector3(24, -3, 0);
                auto& mat = m_World->AddComponent<ECS::MaterialComponent>(e);
                mat.baseColor = Math::Vector3(0.6f, 0.2f, 0.8f);
                m_World->AddComponent<ECS::MeshComponent>(e, Renderer::MeshFactory::CreateCapsule2D(0.4f, 0.9f));
                auto& hp = m_World->AddComponent<ECS::HealthComponent>(e);
                hp.maxHealth = 25.0f; hp.currentHealth = 25.0f;
                auto& dmg = m_World->AddComponent<ECS::DamageComponent>(e);
                dmg.damage = 15.0f;
                auto& ai = m_World->AddComponent<ECS::AIControllerComponent>(e);
                ai.currentState = ECS::AIControllerComponent::AIState::Patrol;
                ai.moveSpeed = 2.5f;
                ai.is2D = true;
                ai.detectionRange = 12.0f;
                ai.fleeDistance = 5.0f;
                ai.patrolPoints = {{24, -3, 0}, {20, -3, 0}};
                m_World->AddComponent<ECS::TagComponent>(e).tags.push_back("enemy");
            }

            // Boss Guardian (north alcove — idle until triggered, chase 15)
            {
                ECS::Entity e = m_World->CreateEntity();
                m_World->AddComponent<ECS::NameComponent>(e, "Boss Guardian");
                auto& t = m_World->AddComponent<ECS::TransformComponent>(e);
                t.position = Math::Vector3(0, 18, 0);
                t.scale = Math::Vector3(1.2f, 1.2f, 1.0f);
                auto& mat = m_World->AddComponent<ECS::MaterialComponent>(e);
                mat.baseColor = Math::Vector3(0.5f, 0.1f, 0.1f);
                mat.emissiveColor = Math::Vector3(0.8f, 0.1f, 0.05f);
                mat.emissiveStrength = 0.4f;
                m_World->AddComponent<ECS::MeshComponent>(e, Renderer::MeshFactory::CreateCapsule2D(0.6f, 1.2f));
                auto& hp = m_World->AddComponent<ECS::HealthComponent>(e);
                hp.maxHealth = 150.0f; hp.currentHealth = 150.0f;
                auto& dmg = m_World->AddComponent<ECS::DamageComponent>(e);
                dmg.damage = 30.0f;
                dmg.knockbackForce = 5.0f;
                auto& ai = m_World->AddComponent<ECS::AIControllerComponent>(e);
                ai.currentState = ECS::AIControllerComponent::AIState::Idle;
                ai.moveSpeed = 2.0f;
                ai.is2D = true;
                ai.detectionRange = 15.0f;
                m_World->AddComponent<ECS::TagComponent>(e).tags.push_back("enemy");
                auto& pe = m_World->AddComponent<ECS::ParticleEmitterComponent>(e);
                ECS::ApplyParticlePreset(pe, "Sparks");
                pe.emissionRate = 5.0f;
            }
        }

        // --- Pickups & Items ---
        {
            // Health Potion ×2 (green emissive, bobbing, magnet to player)
            const Math::Vector3 hpPos[] = {{-7, -7, 0}, {7, 7, 0}};
            for (int i = 0; i < 2; ++i) {
                ECS::Entity e = m_World->CreateEntity();
                m_World->AddComponent<ECS::NameComponent>(e, "Health Potion " + std::to_string(i + 1));
                auto& t = m_World->AddComponent<ECS::TransformComponent>(e);
                t.position = hpPos[i];
                t.scale = Math::Vector3(0.8f, 0.8f, 1.0f);
                auto& mat = m_World->AddComponent<ECS::MaterialComponent>(e);
                mat.baseColor = Math::Vector3(0.2f, 0.9f, 0.3f);
                mat.emissiveColor = Math::Vector3(0.2f, 0.9f, 0.3f);
                mat.emissiveStrength = 0.3f;
                m_World->AddComponent<ECS::MeshComponent>(e, Renderer::MeshFactory::CreateQuad(1.0f, 1.0f));
                auto& pick = m_World->AddComponent<ECS::PickupComponent>(e);
                pick.type = ECS::PickupComponent::PickupType::Health;
                pick.value = 25.0f;
                pick.magnetToPlayer = true;
                pick.magnetRange = 3.0f;
                auto& tw = m_World->AddComponent<ECS::TweenComponent>(e);
                tw.autoPlay = true;
                ECS::TweenEntry bob;
                bob.property = ECS::TweenProperty::Position;
                bob.easing = ECS::EasingType::EaseInOutSine;
                bob.mode = ECS::TweenMode::PingPong;
                bob.startValue = hpPos[i];
                bob.endValue = hpPos[i] + Math::Vector3(0, 0.5f, 0);
                bob.duration = 1.2f;
                bob.useCurrentAsStart = false;
                tw.tweens.push_back(bob);
            }

            // Coin ×4 (gold emissive, bobbing, scattered)
            const Math::Vector3 coinPos[] = {{3, -3, 0}, {-3, 6, 0}, {20, 4, 0}, {25, -4, 0}};
            for (int i = 0; i < 4; ++i) {
                ECS::Entity e = m_World->CreateEntity();
                m_World->AddComponent<ECS::NameComponent>(e, "Coin " + std::to_string(i + 1));
                auto& t = m_World->AddComponent<ECS::TransformComponent>(e);
                t.position = coinPos[i];
                t.scale = Math::Vector3(0.5f, 0.5f, 1.0f);
                auto& mat = m_World->AddComponent<ECS::MaterialComponent>(e);
                mat.baseColor = Math::Vector3(1.0f, 0.85f, 0.2f);
                mat.emissiveColor = Math::Vector3(1.0f, 0.85f, 0.2f);
                mat.emissiveStrength = 0.3f;
                m_World->AddComponent<ECS::MeshComponent>(e, Renderer::MeshFactory::CreateQuad(1.0f, 1.0f));
                auto& pick = m_World->AddComponent<ECS::PickupComponent>(e);
                pick.type = ECS::PickupComponent::PickupType::Coin;
                pick.value = 1.0f;
                auto& tw = m_World->AddComponent<ECS::TweenComponent>(e);
                tw.autoPlay = true;
                ECS::TweenEntry bob;
                bob.property = ECS::TweenProperty::Position;
                bob.easing = ECS::EasingType::EaseInOutSine;
                bob.mode = ECS::TweenMode::PingPong;
                bob.startValue = coinPos[i];
                bob.endValue = coinPos[i] + Math::Vector3(0, 0.4f, 0);
                bob.duration = 1.0f;
                bob.useCurrentAsStart = false;
                tw.tweens.push_back(bob);
            }

            // Key (east wing, guarded — yellow emissive)
            {
                ECS::Entity e = m_World->CreateEntity();
                m_World->AddComponent<ECS::NameComponent>(e, "Dungeon Key");
                auto& t = m_World->AddComponent<ECS::TransformComponent>(e);
                t.position = Math::Vector3(26, 0, 0);
                t.scale = Math::Vector3(0.6f, 0.6f, 1.0f);
                auto& mat = m_World->AddComponent<ECS::MaterialComponent>(e);
                mat.baseColor = Math::Vector3(1.0f, 0.9f, 0.2f);
                mat.emissiveColor = Math::Vector3(1.0f, 0.9f, 0.2f);
                mat.emissiveStrength = 0.5f;
                m_World->AddComponent<ECS::MeshComponent>(e, Renderer::MeshFactory::CreateQuad(1.0f, 1.0f));
                auto& pick = m_World->AddComponent<ECS::PickupComponent>(e);
                pick.type = ECS::PickupComponent::PickupType::Key;
                pick.value = 1.0f;
                auto& tw = m_World->AddComponent<ECS::TweenComponent>(e);
                tw.autoPlay = true;
                ECS::TweenEntry bob;
                bob.property = ECS::TweenProperty::Position;
                bob.easing = ECS::EasingType::EaseInOutSine;
                bob.mode = ECS::TweenMode::PingPong;
                bob.startValue = Math::Vector3(26, 0, 0);
                bob.endValue = Math::Vector3(26, 0.5f, 0);
                bob.duration = 1.4f;
                bob.useCurrentAsStart = false;
                tw.tweens.push_back(bob);
            }

            // Speed Boost (north alcove, behind boss — blue emissive)
            {
                ECS::Entity e = m_World->CreateEntity();
                m_World->AddComponent<ECS::NameComponent>(e, "Speed Boost");
                auto& t = m_World->AddComponent<ECS::TransformComponent>(e);
                t.position = Math::Vector3(0, 20, 0);
                auto& mat = m_World->AddComponent<ECS::MaterialComponent>(e);
                mat.baseColor = Math::Vector3(0.2f, 0.6f, 1.0f);
                mat.emissiveColor = Math::Vector3(0.2f, 0.6f, 1.0f);
                mat.emissiveStrength = 0.6f;
                m_World->AddComponent<ECS::MeshComponent>(e, Renderer::MeshFactory::CreateCapsule2D(0.3f, 0.6f));
                auto& pick = m_World->AddComponent<ECS::PickupComponent>(e);
                pick.type = ECS::PickupComponent::PickupType::Powerup;
                pick.customId = "speed_boost";
                pick.value = 1.0f;
            }
        }

        // --- Destructible Crates ×4 (3 HP, drop coin) ---
        {
            const Math::Vector3 cratePos[] = {{-6, 2, 0}, {3, -7, 0}, {8, 4, 0}, {19, -4, 0}};
            for (int i = 0; i < 4; ++i) {
                ECS::Entity e = m_World->CreateEntity();
                m_World->AddComponent<ECS::NameComponent>(e, "Crate " + std::to_string(i + 1));
                auto& t = m_World->AddComponent<ECS::TransformComponent>(e);
                t.position = cratePos[i];
                t.scale = Math::Vector3(1.5f, 1.5f, 1.0f);
                auto& mat = m_World->AddComponent<ECS::MaterialComponent>(e);
                mat.baseColor = Math::Vector3(0.55f, 0.4f, 0.25f);
                m_World->AddComponent<ECS::MeshComponent>(e, Renderer::MeshFactory::CreateQuad(1.0f, 1.0f));
                addBoxCollider2D(e, 0.75f, 0.75f);
                auto& dest = m_World->AddComponent<ECS::DestructibleComponent>(e);
                dest.health = 3.0f;
                dest.destroyOnHit = false;
                dest.spawnPickup = true;
                dest.pickupId = "coin";
                dest.pickupCount = 1;
            }
        }

        // --- Torches ×4 (wall-mounted with fire particles) ---
        {
            const Math::Vector3 torchPos[] = {{-9, 8, 0}, {-9, -8, 0}, {9, 8, 0}, {9, -8, 0}};
            for (int i = 0; i < 4; ++i) {
                ECS::Entity e = m_World->CreateEntity();
                m_World->AddComponent<ECS::NameComponent>(e, "Torch " + std::to_string(i + 1));
                auto& t = m_World->AddComponent<ECS::TransformComponent>(e);
                t.position = torchPos[i];
                t.scale = Math::Vector3(0.3f, 0.6f, 1.0f);
                auto& mat = m_World->AddComponent<ECS::MaterialComponent>(e);
                mat.baseColor = Math::Vector3(0.8f, 0.4f, 0.1f);
                mat.emissiveColor = Math::Vector3(1.0f, 0.6f, 0.1f);
                mat.emissiveStrength = 0.8f;
                m_World->AddComponent<ECS::MeshComponent>(e, Renderer::MeshFactory::CreateQuad(1.0f, 1.0f));
                auto& pe = m_World->AddComponent<ECS::ParticleEmitterComponent>(e);
                ECS::ApplyParticlePreset(pe, "Fire");
                pe.emissionRate = 20.0f;
                pe.maxParticles = 128;
            }
        }

        // --- Locked Door (blocks north alcove corridor) ---
        {
            ECS::Entity e = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(e, "Locked Door");
            auto& t = m_World->AddComponent<ECS::TransformComponent>(e);
            t.position = Math::Vector3(0, 12, 0);
            t.scale = Math::Vector3(3.0f, 0.5f, 1.0f);
            auto& mat = m_World->AddComponent<ECS::MaterialComponent>(e);
            mat.baseColor = Math::Vector3(0.45f, 0.3f, 0.15f);
            m_World->AddComponent<ECS::MeshComponent>(e, Renderer::MeshFactory::CreateQuad(1.0f, 1.0f));
            addBoxCollider2D(e, 1.5f, 0.25f);
            auto& sw = m_World->AddComponent<ECS::SwitchComponent>(e);
            sw.type = ECS::SwitchComponent::SwitchType::Toggle;
            sw.promptText = "Unlock Door (requires key)";
        }

        // --- Teleporter Pad (main room <-> east wing) ---
        {
            // Main room pad
            ECS::Entity padA = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(padA, "Teleporter Main");
            auto& tA = m_World->AddComponent<ECS::TransformComponent>(padA);
            tA.position = Math::Vector3(-7, 7, 0);
            tA.scale = Math::Vector3(1.5f, 1.5f, 1.0f);
            auto& matA = m_World->AddComponent<ECS::MaterialComponent>(padA);
            matA.baseColor = Math::Vector3(0.5f, 0.1f, 0.8f);
            matA.emissiveColor = Math::Vector3(0.6f, 0.2f, 1.0f);
            matA.emissiveStrength = 0.5f;
            m_World->AddComponent<ECS::MeshComponent>(padA, Renderer::MeshFactory::CreateQuad(1.0f, 1.0f));
            auto& telA = m_World->AddComponent<ECS::TeleporterComponent>(padA);
            telA.targetPosition = Math::Vector3(22, 0, 0);
            telA.cooldown = 1.0f;
            auto& peA = m_World->AddComponent<ECS::ParticleEmitterComponent>(padA);
            ECS::ApplyParticlePreset(peA, "Magic");
            peA.emissionRate = 15.0f;
            peA.maxParticles = 128;

            // East wing pad
            ECS::Entity padB = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(padB, "Teleporter East");
            auto& tB = m_World->AddComponent<ECS::TransformComponent>(padB);
            tB.position = Math::Vector3(22, 0, 0);
            tB.scale = Math::Vector3(1.5f, 1.5f, 1.0f);
            auto& matB = m_World->AddComponent<ECS::MaterialComponent>(padB);
            matB.baseColor = Math::Vector3(0.5f, 0.1f, 0.8f);
            matB.emissiveColor = Math::Vector3(0.6f, 0.2f, 1.0f);
            matB.emissiveStrength = 0.5f;
            m_World->AddComponent<ECS::MeshComponent>(padB, Renderer::MeshFactory::CreateQuad(1.0f, 1.0f));
            auto& telB = m_World->AddComponent<ECS::TeleporterComponent>(padB);
            telB.targetPosition = Math::Vector3(-7, 7, 0);
            telB.cooldown = 1.0f;
            auto& peB = m_World->AddComponent<ECS::ParticleEmitterComponent>(padB);
            ECS::ApplyParticlePreset(peB, "Magic");
            peB.emissionRate = 15.0f;
            peB.maxParticles = 128;

            // Link bidirectionally
            telA.linkedTeleporter = padB;
            telB.linkedTeleporter = padA;
        }

        // --- Pressure Plate (east wing) ---
        {
            ECS::Entity e = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(e, "Pressure Plate");
            auto& t = m_World->AddComponent<ECS::TransformComponent>(e);
            t.position = Math::Vector3(19, 0, -0.05f);
            t.scale = Math::Vector3(1.5f, 1.5f, 1.0f);
            auto& mat = m_World->AddComponent<ECS::MaterialComponent>(e);
            mat.baseColor = Math::Vector3(0.5f, 0.5f, 0.4f);
            m_World->AddComponent<ECS::MeshComponent>(e, Renderer::MeshFactory::CreateQuad(1.0f, 1.0f));
            auto& sw = m_World->AddComponent<ECS::SwitchComponent>(e);
            sw.type = ECS::SwitchComponent::SwitchType::PressurePlate;
            auto& trigger = m_World->AddComponent<ECS::TriggerZoneComponent>(e);
            trigger.shape = ECS::TriggerZoneComponent::Shape::Box;
            trigger.boxSize = Math::Vector3(1.5f, 1.5f, 1.0f);
        }

        // --- HUD ---
        {
            // Health Bar
            ECS::Entity hud = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(hud, "Health Bar Overlay");
            m_World->AddComponent<ECS::TransformComponent>(hud);
            auto& hw = m_World->AddComponent<ECS::HUDWidgetComponent>(hud);
            hw.type = ECS::HUDWidgetComponent::WidgetType::HealthBar;
            hw.visible = true;
            hw.screenSpace = true;
            hw.anchorX = 0.05f;
            hw.anchorY = 0.05f;
            hw.fillColor = Math::Vector3(0.8f, 0.2f, 0.2f);
            hw.bgColor = Math::Vector3(0.2f, 0.2f, 0.2f);
            hw.sourceEntity = player;  // Link to player's HealthComponent for live updates

            // Coin Counter
            ECS::Entity coinHud = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(coinHud, "Coin Counter");
            m_World->AddComponent<ECS::TransformComponent>(coinHud);
            auto& tc = m_World->AddComponent<ECS::TextComponent>(coinHud);
            tc.text = "Coins: 0";
            tc.fontSize = 24.0f;
            tc.textColor = Math::Vector3(1.0f, 0.85f, 0.2f);
            m_World->AddComponent<ECS::TagComponent>(coinHud).tags.push_back("hud_coins");

            // Key Indicator
            ECS::Entity keyHud = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(keyHud, "Key Indicator");
            m_World->AddComponent<ECS::TransformComponent>(keyHud);
            auto& tk = m_World->AddComponent<ECS::TextComponent>(keyHud);
            tk.text = "Key: --";
            tk.fontSize = 24.0f;
            tk.textColor = Math::Vector3(1.0f, 0.9f, 0.2f);
            m_World->AddComponent<ECS::TagComponent>(keyHud).tags.push_back("hud_key");
        }

        // --- Notes ---
        {
            ECS::Entity notes = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(notes, "Template Notes");
            m_World->AddComponent<ECS::TransformComponent>(notes);
            auto& n = m_World->AddComponent<ECS::NotesComponent>(notes);
            n.notes = "2D Top-Down Action — Multi-Room Dungeon Template\n\n"
                "Layout:\n"
                "- Main room (20x20): Player start, open arena with wall torches\n"
                "- East wing (12x12): Enemy gauntlet, connected by corridor\n"
                "- North alcove (8x8): Boss room behind locked door\n\n"
                "Enemies (6 total):\n"
                "- Scout x2 (red): Diamond patrol, chase range 8\n"
                "- Brute (orange): Short patrol, knockback attack, chase range 6\n"
                "- Archer (purple): Ranged patrol, chase range 12, flees when close\n"
                "- Boss Guardian (dark red): Idle until triggered, chase range 15\n\n"
                "Pickups:\n"
                "- Health Potions x2 (green), Coins x4 (gold), Key x1 (yellow), Speed Boost x1 (blue)\n\n"
                "Interactive Objects:\n"
                "- Destructible Crates x4: 3 HP, drop coins\n"
                "- Torches x4: Fire particles\n"
                "- Locked Door: Blocks north alcove (key required)\n"
                "- Teleporter: Links main room to east wing\n"
                "- Pressure Plate: Trigger zone in east wing\n\n"
                "HUD: Health bar, coin counter, key indicator\n\n"
                "Controls: WASD to move, Shift to dash";
        }

        // --- Render settings ---
        m_RenderSystem->SetShadowsEnabled(false);
        m_RenderSystem->SetAmbientIntensity(0.3f);
        if (m_PostProcessing) {
            auto& pp = m_PostProcessing->GetSettings();
            pp.fxaaEnabled = 1;
            pp.vignetteEnabled = 1;
            pp.vignetteIntensity = 0.3f;
            pp.vignetteSmoothness = 0.8f;
        }

    } else if (templateId == "thirdperson") {
        createGround();
        ECS::Entity player = createPlayer3D("Player");
        auto& ctrl = m_World->AddComponent<ECS::ThirdPersonController>(player);
        ctrl.moveSpeed = 5.0f;
        ctrl.cameraDistance = 5.0f;
        ctrl.cameraHeight = 2.0f;
        SetupCameraForController(player, "ThirdPerson");

        // Obstacle cubes
        {
            const Math::Vector3 pos[] = { {4,0.5f,3}, {-3,0.75f,-2}, {6,1,0} };
            const Math::Vector3 scl[] = { {1,1,1}, {1.5f,1.5f,1.5f}, {2,2,1} };
            for (int i = 0; i < 3; ++i) {
                ECS::Entity obs = m_World->CreateEntity();
                m_World->AddComponent<ECS::NameComponent>(obs, "Obstacle " + std::to_string(i + 1));
                auto& ot = m_World->AddComponent<ECS::TransformComponent>(obs);
                ot.position = pos[i];
                ot.scale = scl[i];
                auto& omat = m_World->AddComponent<ECS::MaterialComponent>(obs);
                omat.baseColor = Math::Vector3(0.6f, 0.5f, 0.4f);
                m_World->AddComponent<ECS::MeshComponent>(obs, Renderer::MeshFactory::CreateCube(1.0f));
                auto& ocol = m_World->AddComponent<ECS::BoxColliderComponent>(obs);
                ocol.size = scl[i];
                auto& orb = m_World->AddComponent<ECS::RigidbodyComponent>(obs);
                orb.bodyType = ECS::RigidbodyComponent::BodyType::Static;
            }
        }

        // Point light
        {
            ECS::Entity pl = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(pl, "Point Light");
            auto& plt = m_World->AddComponent<ECS::TransformComponent>(pl);
            plt.position = Math::Vector3(3.0f, 4.0f, 2.0f);
            auto& plc = m_World->AddComponent<ECS::LightComponent>(pl);
            plc.type = ECS::LightType::Point;
            plc.intensity = 2.0f;
            plc.range = 15.0f;
            plc.color = Math::Vector3(1.0f, 0.9f, 0.8f);
        }

        // Ramp
        {
            ECS::Entity ramp = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(ramp, "Ramp");
            auto& rt = m_World->AddComponent<ECS::TransformComponent>(ramp);
            rt.position = Math::Vector3(-6.0f, 0.5f, -3.0f);
            rt.scale = Math::Vector3(3.0f, 0.3f, 4.0f);
            rt.rotation = Math::Quaternion(Math::Vector3(0, 0, 1), Math::Radians(20.0f));
            auto& rmat = m_World->AddComponent<ECS::MaterialComponent>(ramp);
            rmat.baseColor = Math::Vector3(0.5f, 0.45f, 0.4f);
            m_World->AddComponent<ECS::MeshComponent>(ramp, Renderer::MeshFactory::CreateCube(1.0f));
            auto& rcol = m_World->AddComponent<ECS::BoxColliderComponent>(ramp);
            rcol.size = Math::Vector3(3.0f, 0.3f, 4.0f);
        }

        // Collectible coin with bob
        {
            ECS::Entity coin = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(coin, "Coin");
            auto& ct = m_World->AddComponent<ECS::TransformComponent>(coin);
            ct.position = Math::Vector3(-6.0f, 2.0f, -3.0f);
            ct.scale = Math::Vector3(0.4f, 0.4f, 0.4f);
            auto& cmat = m_World->AddComponent<ECS::MaterialComponent>(coin);
            cmat.baseColor = Math::Vector3(1.0f, 0.85f, 0.0f);
            cmat.emissiveColor = Math::Vector3(1.0f, 0.85f, 0.0f);
            cmat.emissiveStrength = 0.4f;
            m_World->AddComponent<ECS::MeshComponent>(coin, Renderer::MeshFactory::CreateSphere(0.2f));
            auto& pk = m_World->AddComponent<ECS::PickupComponent>(coin);
            pk.type = ECS::PickupComponent::PickupType::Coin;
            pk.customId = "Coin";
            pk.value = 10.0f;
            auto& tw = m_World->AddComponent<ECS::TweenComponent>(coin);
            ECS::TweenEntry bob;
            bob.property = ECS::TweenProperty::Position;
            bob.easing = ECS::EasingType::EaseInOutSine;
            bob.mode = ECS::TweenMode::PingPong;
            bob.startValue = Math::Vector3(-6.0f, 2.0f, -3.0f);
            bob.endValue = Math::Vector3(-6.0f, 2.5f, -3.0f);
            bob.duration = 1.5f;
            tw.tweens.push_back(bob);
        }

        {
            Renderer::SkyboxConfig skyConfig;
            skyConfig.type = Renderer::SkyboxType::Procedural;
            skyConfig.topColor = Math::Vector3(0.1f, 0.3f, 0.8f);
            skyConfig.horizonColor = Math::Vector3(0.5f, 0.7f, 1.0f);
            skyConfig.bottomColor = Math::Vector3(0.8f, 0.85f, 0.9f);
            skyConfig.sunDirection = Math::Vector3(0.0f, 1.0f, 0.0f);
            m_RenderSystem->SetSkybox(skyConfig);
        }
        m_RenderSystem->SetShadowsEnabled(true);
        m_RenderSystem->SetAmbientIntensity(0.12f);
        m_RenderSystem->SetFogParams(0.01f, 20.0f, 100.0f, 0.3f);
        if (m_PostProcessing) {
            auto& pp = m_PostProcessing->GetSettings();
            pp.fxaaEnabled = 1;
            pp.bloomEnabled = 1;
            pp.bloomThreshold = 0.9f;
            pp.bloomIntensity = 0.3f;
        }

    } else if (templateId == "firstperson") {
        createGround();
        ECS::Entity player = createPlayer3D("Player");
        auto* playerT = m_World->GetComponent<ECS::TransformComponent>(player);
        if (playerT) playerT->position.y = 1.7f;
        auto& ctrl = m_World->AddComponent<ECS::FirstPersonController>(player);
        ctrl.moveSpeed = 5.0f;
        ctrl.mouseSensitivity = 0.15f;
        SetupCameraForController(player, "FirstPerson");

        // L-shaped corridor walls
        {
            const Math::Vector3 wPos[] = { {-3,1.5f,0}, {3,1.5f,0}, {0,1.5f,-5}, {3,1.5f,-8} };
            const Math::Vector3 wScl[] = { {0.3f,3,10}, {0.3f,3,10}, {6,3,0.3f}, {0.3f,3,6} };
            const char* wNames[] = { "Wall Left", "Wall Right", "Wall Back", "Wall Side" };
            for (int i = 0; i < 4; ++i) {
                ECS::Entity wall = m_World->CreateEntity();
                m_World->AddComponent<ECS::NameComponent>(wall, wNames[i]);
                auto& wt = m_World->AddComponent<ECS::TransformComponent>(wall);
                wt.position = wPos[i];
                wt.scale = wScl[i];
                auto& wmat = m_World->AddComponent<ECS::MaterialComponent>(wall);
                wmat.baseColor = Math::Vector3(0.55f, 0.5f, 0.45f);
                m_World->AddComponent<ECS::MeshComponent>(wall, Renderer::MeshFactory::CreateCube(1.0f));
                auto& wcol = m_World->AddComponent<ECS::BoxColliderComponent>(wall);
                wcol.size = wScl[i];
            }
        }

        // Warm point light
        {
            ECS::Entity pl = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(pl, "Corridor Light");
            auto& plt = m_World->AddComponent<ECS::TransformComponent>(pl);
            plt.position = Math::Vector3(0.0f, 2.5f, -2.0f);
            auto& plc = m_World->AddComponent<ECS::LightComponent>(pl);
            plc.type = ECS::LightType::Point;
            plc.intensity = 2.5f;
            plc.range = 12.0f;
            plc.color = Math::Vector3(1.0f, 0.85f, 0.7f);
        }

        // Interactable door
        {
            ECS::Entity door = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(door, "Door");
            auto& dt = m_World->AddComponent<ECS::TransformComponent>(door);
            dt.position = Math::Vector3(0.0f, 1.5f, -4.8f);
            dt.scale = Math::Vector3(1.5f, 3.0f, 0.2f);
            auto& dmat = m_World->AddComponent<ECS::MaterialComponent>(door);
            dmat.baseColor = Math::Vector3(0.4f, 0.3f, 0.2f);
            m_World->AddComponent<ECS::MeshComponent>(door, Renderer::MeshFactory::CreateCube(1.0f));
            auto& dsw = m_World->AddComponent<ECS::SwitchComponent>(door);
            dsw.type = ECS::SwitchComponent::SwitchType::Toggle;
            dsw.promptText = "Open Door";
            auto& dcol = m_World->AddComponent<ECS::BoxColliderComponent>(door);
            dcol.size = Math::Vector3(1.5f, 3.0f, 0.2f);
        }

        // Flashlight (player spot light)
        {
            ECS::Entity flashlight = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(flashlight, "Flashlight");
            auto& flt = m_World->AddComponent<ECS::TransformComponent>(flashlight);
            flt.position = Math::Vector3(0.0f, 1.7f, 0.0f);
            auto& flc = m_World->AddComponent<ECS::LightComponent>(flashlight);
            flc.type = ECS::LightType::Spot;
            flc.intensity = 2.0f;
            flc.range = 15.0f;
            flc.outerConeAngle = 25.0f;
            auto& follow = m_World->AddComponent<ECS::FollowTargetComponent>(flashlight);
            follow.target = player;
            follow.offset = Math::Vector3(0.2f, -0.1f, 0.0f);
        }

        // Ambient sound source note
        {
            ECS::Entity ambNote = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(ambNote, "Ambient Sound Source");
            auto& ant = m_World->AddComponent<ECS::TransformComponent>(ambNote);
            ant.position = Math::Vector3(0.0f, 1.0f, -8.0f);
            auto& ac = m_World->AddComponent<ECS::AudioSourceComponent>(ambNote);
            ac.loop = true;
            ac.volume = 0.4f;
            ac.is3D = true;
            auto& notes = m_World->AddComponent<ECS::NotesComponent>(ambNote);
            notes.notes = "Assign an ambient audio file (e.g. humming, wind).\n"
                "Spatial audio makes it grow louder as you approach.";
        }

        {
            Renderer::SkyboxConfig skyConfig;
            skyConfig.type = Renderer::SkyboxType::Procedural;
            skyConfig.topColor = Math::Vector3(0.1f, 0.3f, 0.8f);
            skyConfig.horizonColor = Math::Vector3(0.5f, 0.7f, 1.0f);
            skyConfig.bottomColor = Math::Vector3(0.8f, 0.85f, 0.9f);
            skyConfig.sunDirection = Math::Vector3(0.0f, 1.0f, 0.0f);
            m_RenderSystem->SetSkybox(skyConfig);
        }
        m_RenderSystem->SetShadowsEnabled(true);
        if (m_PostProcessing) {
            auto& pp = m_PostProcessing->GetSettings();
            pp.fxaaEnabled = 1;
            pp.vignetteEnabled = 1;
            pp.vignetteIntensity = 0.2f;
        }

    } else if (templateId == "puzzle") {
        createGround();
        ECS::Entity player = createPlayer3D("Player");
        auto& ctrl = m_World->AddComponent<ECS::TopDown3DController>(player);
        ctrl.moveSpeed = 4.0f;
        ctrl.cameraAngle = 60.0f;
        ctrl.cameraDistance = 12.0f;
        SetupCameraForController(player, "TopDown3D");

        // 4 pushable crates
        {
            const Math::Vector3 cratePos[] = { {2,0.5f,0}, {-1,0.5f,3}, {4,0.5f,-2}, {-4,0.5f,-1} };
            const Math::Vector3 crateCol[] = { {0.7f,0.4f,0.2f}, {0.6f,0.35f,0.25f}, {0.75f,0.45f,0.15f}, {0.65f,0.4f,0.3f} };
            for (int i = 0; i < 4; ++i) {
                ECS::Entity crate = m_World->CreateEntity();
                m_World->AddComponent<ECS::NameComponent>(crate, "Crate " + std::to_string(i + 1));
                auto& ct = m_World->AddComponent<ECS::TransformComponent>(crate);
                ct.position = cratePos[i];
                auto& cmat = m_World->AddComponent<ECS::MaterialComponent>(crate);
                cmat.baseColor = crateCol[i];
                m_World->AddComponent<ECS::MeshComponent>(crate, Renderer::MeshFactory::CreateCube(1.0f));
                auto& push = m_World->AddComponent<ECS::PushableComponent>(crate);
                push.gridSnap = true;
                push.gridCellSize = 2.0f;
                auto& bcol = m_World->AddComponent<ECS::BoxColliderComponent>(crate);
                bcol.size = Math::Vector3(1, 1, 1);
            }
        }

        // 4 goal plates with pulse tween
        {
            const Math::Vector3 platePos[] = { {6,0.05f,0}, {-5,0.05f,3}, {0,0.05f,-4}, {-6,0.05f,-3} };
            const Math::Vector3 plateCol[] = { {0.2f,0.8f,0.2f}, {0.2f,0.5f,0.9f}, {0.9f,0.8f,0.2f}, {0.8f,0.3f,0.6f} };
            for (int i = 0; i < 4; ++i) {
                ECS::Entity plate = m_World->CreateEntity();
                m_World->AddComponent<ECS::NameComponent>(plate, "Goal Plate " + std::to_string(i + 1));
                auto& pt = m_World->AddComponent<ECS::TransformComponent>(plate);
                pt.position = platePos[i];
                pt.scale = Math::Vector3(1.5f, 0.1f, 1.5f);
                auto& pmat = m_World->AddComponent<ECS::MaterialComponent>(plate);
                pmat.baseColor = plateCol[i];
                pmat.emissiveColor = plateCol[i];
                pmat.emissiveStrength = 0.3f;
                m_World->AddComponent<ECS::MeshComponent>(plate, Renderer::MeshFactory::CreateCube(1.0f));
                auto& goal = m_World->AddComponent<ECS::GoalZoneComponent>(plate);
                goal.type = ECS::GoalZoneComponent::GoalType::PushTarget;
                goal.goalGroup = i;
                // Pulse glow tween
                auto& tw = m_World->AddComponent<ECS::TweenComponent>(plate);
                ECS::TweenEntry pulse;
                pulse.property = ECS::TweenProperty::Scale;
                pulse.easing = ECS::EasingType::EaseInOutSine;
                pulse.mode = ECS::TweenMode::PingPong;
                pulse.startValue = Math::Vector3(1.5f, 0.1f, 1.5f);
                pulse.endValue = Math::Vector3(1.6f, 0.12f, 1.6f);
                pulse.duration = 2.0f;
                tw.tweens.push_back(pulse);
            }
        }

        // Switch door
        {
            ECS::Entity door = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(door, "Door");
            auto& dt = m_World->AddComponent<ECS::TransformComponent>(door);
            dt.position = Math::Vector3(0.0f, 1.5f, -8.0f);
            dt.scale = Math::Vector3(3.0f, 3.0f, 0.3f);
            auto& dmat = m_World->AddComponent<ECS::MaterialComponent>(door);
            dmat.baseColor = Math::Vector3(0.5f, 0.3f, 0.2f);
            m_World->AddComponent<ECS::MeshComponent>(door, Renderer::MeshFactory::CreateCube(1.0f));
            auto& sw = m_World->AddComponent<ECS::SwitchComponent>(door);
            sw.type = ECS::SwitchComponent::SwitchType::Toggle;
            auto& bcol = m_World->AddComponent<ECS::BoxColliderComponent>(door);
            bcol.size = Math::Vector3(3, 3, 0.3f);
        }

        {
            ECS::Entity hint = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(hint, "Puzzle Hint");
            m_World->AddComponent<ECS::TransformComponent>(hint);
            auto& notes = m_World->AddComponent<ECS::NotesComponent>(hint);
            notes.notes = "Push crates onto matching goal plates to solve the puzzle.\nCrates snap to a 2x2 grid. Toggle the switch to open the door.";
        }

        {
            Renderer::SkyboxConfig skyConfig;
            skyConfig.type = Renderer::SkyboxType::Procedural;
            skyConfig.topColor = Math::Vector3(0.1f, 0.3f, 0.8f);
            skyConfig.horizonColor = Math::Vector3(0.5f, 0.7f, 1.0f);
            skyConfig.bottomColor = Math::Vector3(0.8f, 0.85f, 0.9f);
            skyConfig.sunDirection = Math::Vector3(0.0f, 1.0f, 0.0f);
            m_RenderSystem->SetSkybox(skyConfig);
        }
        m_RenderSystem->SetShadowsEnabled(true);
        m_RenderSystem->SetAmbientIntensity(0.15f);
        if (m_PostProcessing) {
            auto& pp = m_PostProcessing->GetSettings();
            pp.fxaaEnabled = 1;
        }

    } else if (templateId == "survival") {
        createGround();
        ECS::Entity player = createPlayer3D("Player");
        auto& ctrl = m_World->AddComponent<ECS::ThirdPersonController>(player);
        ctrl.moveSpeed = 4.0f;
        ctrl.cameraDistance = 6.0f;
        ctrl.cameraHeight = 2.5f;
        m_World->AddComponent<ECS::HealthComponent>(player);
        auto& stamina = m_World->AddComponent<ECS::ResourceComponent>(player);
        stamina.resourceName = "Stamina";
        SetupCameraForController(player, "ThirdPerson");

        // Cold temperature zone
        {
            ECS::Entity coldZone = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(coldZone, "Cold Zone");
            auto& zt = m_World->AddComponent<ECS::TransformComponent>(coldZone);
            zt.position = Math::Vector3(-10.0f, 0.0f, 0.0f);
            auto& tz = m_World->AddComponent<ECS::TemperatureZoneComponent>(coldZone);
            tz.temperature = -20.0f;
            tz.halfExtents = Math::Vector3(8.0f, 5.0f, 8.0f);
        }

        // Snow weather zone
        {
            ECS::Entity snowZone = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(snowZone, "Snow Zone");
            auto& zt = m_World->AddComponent<ECS::TransformComponent>(snowZone);
            zt.position = Math::Vector3(-10.0f, 0.0f, 0.0f);
            auto& wz = m_World->AddComponent<ECS::WeatherZoneComponent>(snowZone);
            wz.weatherType = 4; // Snow
            wz.snowIntensity = 0.8f;
            wz.fogDensity = 0.3f;
        }

        // Campfire
        {
            ECS::Entity fire = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(fire, "Campfire");
            auto& ft = m_World->AddComponent<ECS::TransformComponent>(fire);
            ft.position = Math::Vector3(0.0f, 0.1f, 0.0f);
            ft.scale = Math::Vector3(0.5f, 0.3f, 0.5f);
            auto& fmat = m_World->AddComponent<ECS::MaterialComponent>(fire);
            fmat.baseColor = Math::Vector3(0.3f, 0.2f, 0.1f);
            m_World->AddComponent<ECS::MeshComponent>(fire, Renderer::MeshFactory::CreateCube(1.0f));
            auto& pe = m_World->AddComponent<ECS::ParticleEmitterComponent>(fire);
            ECS::ApplyParticlePreset(pe, "Fire");

            ECS::Entity fireLight = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(fireLight, "Fire Light");
            auto& flt = m_World->AddComponent<ECS::TransformComponent>(fireLight);
            flt.position = Math::Vector3(0.0f, 1.5f, 0.0f);
            auto& flc = m_World->AddComponent<ECS::LightComponent>(fireLight);
            flc.type = ECS::LightType::Point;
            flc.intensity = 3.0f;
            flc.range = 10.0f;
            flc.color = Math::Vector3(1.0f, 0.6f, 0.2f);
        }

        // Damage hazard
        {
            ECS::Entity hazard = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(hazard, "Hazard Zone");
            auto& ht = m_World->AddComponent<ECS::TransformComponent>(hazard);
            ht.position = Math::Vector3(8.0f, 0.05f, 5.0f);
            ht.scale = Math::Vector3(3.0f, 0.1f, 3.0f);
            auto& hmat = m_World->AddComponent<ECS::MaterialComponent>(hazard);
            hmat.baseColor = Math::Vector3(0.8f, 0.2f, 0.1f);
            hmat.emissiveColor = Math::Vector3(0.8f, 0.1f, 0.0f);
            hmat.emissiveStrength = 0.5f;
            m_World->AddComponent<ECS::MeshComponent>(hazard, Renderer::MeshFactory::CreateCube(1.0f));
            auto& trigger = m_World->AddComponent<ECS::TriggerZoneComponent>(hazard);
            trigger.shape = ECS::TriggerZoneComponent::Shape::Box;
            trigger.boxSize = Math::Vector3(3.0f, 1.0f, 3.0f);
        }

        // Shelter (roof + 2 walls)
        {
            ECS::Entity roof = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(roof, "Shelter Roof");
            auto& rt = m_World->AddComponent<ECS::TransformComponent>(roof);
            rt.position = Math::Vector3(5.0f, 2.5f, -5.0f);
            rt.scale = Math::Vector3(4.0f, 0.2f, 3.0f);
            auto& rmat = m_World->AddComponent<ECS::MaterialComponent>(roof);
            rmat.baseColor = Math::Vector3(0.4f, 0.3f, 0.2f);
            m_World->AddComponent<ECS::MeshComponent>(roof, Renderer::MeshFactory::CreateCube(1.0f));
            auto& rcol = m_World->AddComponent<ECS::BoxColliderComponent>(roof);
            rcol.size = rt.scale;

            ECS::Entity wallL = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(wallL, "Shelter Wall Left");
            auto& wlt = m_World->AddComponent<ECS::TransformComponent>(wallL);
            wlt.position = Math::Vector3(3.0f, 1.25f, -5.0f);
            wlt.scale = Math::Vector3(0.2f, 2.5f, 3.0f);
            auto& wlmat = m_World->AddComponent<ECS::MaterialComponent>(wallL);
            wlmat.baseColor = Math::Vector3(0.35f, 0.25f, 0.15f);
            m_World->AddComponent<ECS::MeshComponent>(wallL, Renderer::MeshFactory::CreateCube(1.0f));

            ECS::Entity wallR = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(wallR, "Shelter Wall Right");
            auto& wrt = m_World->AddComponent<ECS::TransformComponent>(wallR);
            wrt.position = Math::Vector3(7.0f, 1.25f, -5.0f);
            wrt.scale = Math::Vector3(0.2f, 2.5f, 3.0f);
            auto& wrmat = m_World->AddComponent<ECS::MaterialComponent>(wallR);
            wrmat.baseColor = Math::Vector3(0.35f, 0.25f, 0.15f);
            m_World->AddComponent<ECS::MeshComponent>(wallR, Renderer::MeshFactory::CreateCube(1.0f));
        }

        // Wood resource pickup
        {
            ECS::Entity wood = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(wood, "Wood Pile");
            auto& wt = m_World->AddComponent<ECS::TransformComponent>(wood);
            wt.position = Math::Vector3(-5.0f, 0.3f, 3.0f);
            wt.scale = Math::Vector3(1.0f, 0.6f, 0.6f);
            auto& wmat = m_World->AddComponent<ECS::MaterialComponent>(wood);
            wmat.baseColor = Math::Vector3(0.55f, 0.35f, 0.15f);
            m_World->AddComponent<ECS::MeshComponent>(wood, Renderer::MeshFactory::CreateCube(1.0f));
            auto& pk = m_World->AddComponent<ECS::PickupComponent>(wood);
            pk.type = ECS::PickupComponent::PickupType::Custom;
            pk.customId = "Wood";
            pk.value = 5.0f;
            m_World->AddComponent<ECS::InteractableComponent>(wood).promptText = "Gather Wood";
        }

        {
            Renderer::SkyboxConfig skyConfig;
            skyConfig.type = Renderer::SkyboxType::Procedural;
            skyConfig.topColor = Math::Vector3(0.3f, 0.35f, 0.4f);
            skyConfig.horizonColor = Math::Vector3(0.5f, 0.5f, 0.55f);
            skyConfig.bottomColor = Math::Vector3(0.4f, 0.4f, 0.4f);
            skyConfig.sunDirection = Math::Vector3(0.2f, 0.5f, 0.3f);
            m_RenderSystem->SetSkybox(skyConfig);
        }
        m_RenderSystem->SetShadowsEnabled(true);
        m_RenderSystem->SetAmbientIntensity(0.1f);
        m_RenderSystem->SetFogParams(0.03f, 10.0f, 80.0f, 0.5f);
        if (m_PostProcessing) {
            auto& pp = m_PostProcessing->GetSettings();
            pp.fxaaEnabled = 1;
            pp.vignetteEnabled = 1;
            pp.vignetteIntensity = 0.25f;
            pp.filmGrainEnabled = 1;
            pp.filmGrainIntensity = 0.08f;
        }

    } else if (templateId == "rpg_village") {
        createGround();
        ECS::Entity player = createPlayer3D("Player");
        auto& ctrl = m_World->AddComponent<ECS::TopDown3DController>(player);
        ctrl.moveSpeed = 5.0f;
        ctrl.cameraAngle = 45.0f;
        ctrl.cameraDistance = 12.0f;
        SetupCameraForController(player, "TopDown3D");

        // NPC with dialogue
        {
            ECS::Entity npc = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(npc, "Villager");
            auto& nt = m_World->AddComponent<ECS::TransformComponent>(npc);
            nt.position = Math::Vector3(4.0f, 1.0f, 2.0f);
            auto& nmat = m_World->AddComponent<ECS::MaterialComponent>(npc);
            nmat.baseColor = Math::Vector3(0.8f, 0.6f, 0.5f);
            m_World->AddComponent<ECS::MeshComponent>(npc, Renderer::MeshFactory::CreateCapsule(0.3f, 1.0f));
            auto& dlg = m_World->AddComponent<ECS::DialogueComponent>(npc);
            dlg.speakerName = "Villager";
            dlg.dialogueLines = { "Welcome to the village!", "The chest near the house has supplies.", "Be careful out there." };
        }

        // Chest
        {
            ECS::Entity chest = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(chest, "Chest");
            auto& ct = m_World->AddComponent<ECS::TransformComponent>(chest);
            ct.position = Math::Vector3(-3.0f, 0.4f, -2.0f);
            ct.scale = Math::Vector3(0.8f, 0.8f, 0.6f);
            auto& cmat = m_World->AddComponent<ECS::MaterialComponent>(chest);
            cmat.baseColor = Math::Vector3(0.6f, 0.4f, 0.15f);
            m_World->AddComponent<ECS::MeshComponent>(chest, Renderer::MeshFactory::CreateCube(1.0f));
            auto& pick = m_World->AddComponent<ECS::PickupComponent>(chest);
            pick.type = ECS::PickupComponent::PickupType::Custom;
            pick.customId = "supplies";
            auto& notes = m_World->AddComponent<ECS::NotesComponent>(chest);
            notes.notes = "Interact with the chest to collect supplies.";
        }

        // House
        {
            ECS::Entity house = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(house, "House");
            auto& ht = m_World->AddComponent<ECS::TransformComponent>(house);
            ht.position = Math::Vector3(-5.0f, 1.5f, -3.0f);
            ht.scale = Math::Vector3(4.0f, 3.0f, 4.0f);
            auto& hmat = m_World->AddComponent<ECS::MaterialComponent>(house);
            hmat.baseColor = Math::Vector3(0.6f, 0.5f, 0.35f);
            m_World->AddComponent<ECS::MeshComponent>(house, Renderer::MeshFactory::CreateCube(1.0f));
            auto& hcol = m_World->AddComponent<ECS::BoxColliderComponent>(house);
            hcol.size = Math::Vector3(4.0f, 3.0f, 4.0f);
        }

        // Fences
        for (int i = 0; i < 2; ++i) {
            ECS::Entity fence = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(fence, "Fence " + std::to_string(i + 1));
            auto& ft = m_World->AddComponent<ECS::TransformComponent>(fence);
            ft.position = Math::Vector3(-5.0f + i * 10.0f, 0.5f, 3.0f);
            ft.scale = Math::Vector3(4.0f, 1.0f, 0.2f);
            auto& fmat = m_World->AddComponent<ECS::MaterialComponent>(fence);
            fmat.baseColor = Math::Vector3(0.45f, 0.35f, 0.2f);
            m_World->AddComponent<ECS::MeshComponent>(fence, Renderer::MeshFactory::CreateCube(1.0f));
            auto& fcol = m_World->AddComponent<ECS::BoxColliderComponent>(fence);
            fcol.size = Math::Vector3(4.0f, 1.0f, 0.2f);
        }

        // Lantern
        {
            ECS::Entity lantern = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(lantern, "Lantern");
            auto& lt = m_World->AddComponent<ECS::TransformComponent>(lantern);
            lt.position = Math::Vector3(3.0f, 3.0f, 1.0f);
            auto& lc = m_World->AddComponent<ECS::LightComponent>(lantern);
            lc.type = ECS::LightType::Point;
            lc.intensity = 2.0f;
            lc.range = 12.0f;
            lc.color = Math::Vector3(1.0f, 0.85f, 0.6f);
        }

        // Fountain with water particles
        {
            ECS::Entity fountain = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(fountain, "Fountain");
            auto& ft = m_World->AddComponent<ECS::TransformComponent>(fountain);
            ft.position = Math::Vector3(0.0f, 0.4f, 0.0f);
            ft.scale = Math::Vector3(1.5f, 0.8f, 1.5f);
            auto& fmat = m_World->AddComponent<ECS::MaterialComponent>(fountain);
            fmat.baseColor = Math::Vector3(0.5f, 0.5f, 0.55f);
            m_World->AddComponent<ECS::MeshComponent>(fountain, Renderer::MeshFactory::CreateCube(1.0f));
            auto& pe = m_World->AddComponent<ECS::ParticleEmitterComponent>(fountain);
            pe.emissionRate = 30.0f;
            pe.startSpeed = 3.0f;
            pe.startSize = 0.1f;
            pe.lifetime = 1.5f;
            pe.startColor = Math::Vector3(0.4f, 0.6f, 0.9f);
            pe.gravity = Math::Vector3(0.0f, -9.8f, 0.0f);
        }

        // 2nd NPC (Quest giver)
        {
            ECS::Entity npc2 = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(npc2, "Elder");
            auto& n2t = m_World->AddComponent<ECS::TransformComponent>(npc2);
            n2t.position = Math::Vector3(-2.0f, 1.0f, 4.0f);
            auto& n2mat = m_World->AddComponent<ECS::MaterialComponent>(npc2);
            n2mat.baseColor = Math::Vector3(0.5f, 0.4f, 0.6f);
            m_World->AddComponent<ECS::MeshComponent>(npc2, Renderer::MeshFactory::CreateCapsule(0.3f, 1.0f));
            auto& dlg2 = m_World->AddComponent<ECS::DialogueComponent>(npc2);
            dlg2.speakerName = "Elder";
            dlg2.dialogueLines = { "Brave adventurer, I need your help.", "Find the ancient amulet in the forest.", "Return it to me for a reward." };
            m_World->AddComponent<ECS::InteractableComponent>(npc2).promptText = "Talk to Elder";
        }

        // Quest item
        {
            ECS::Entity amulet = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(amulet, "Ancient Amulet");
            auto& at = m_World->AddComponent<ECS::TransformComponent>(amulet);
            at.position = Math::Vector3(8.0f, 0.5f, -6.0f);
            at.scale = Math::Vector3(0.4f, 0.4f, 0.2f);
            auto& amat = m_World->AddComponent<ECS::MaterialComponent>(amulet);
            amat.baseColor = Math::Vector3(0.8f, 0.6f, 0.9f);
            amat.emissiveColor = Math::Vector3(0.6f, 0.3f, 0.8f);
            amat.emissiveStrength = 0.5f;
            m_World->AddComponent<ECS::MeshComponent>(amulet, Renderer::MeshFactory::CreateSphere(0.2f));
            auto& pk = m_World->AddComponent<ECS::PickupComponent>(amulet);
            pk.type = ECS::PickupComponent::PickupType::Custom;
            pk.customId = "Ancient Amulet";
            pk.value = 1.0f;
            m_World->AddComponent<ECS::InteractableComponent>(amulet).promptText = "Pick up Amulet";
        }

        {
            Renderer::SkyboxConfig skyConfig;
            skyConfig.type = Renderer::SkyboxType::Procedural;
            skyConfig.topColor = Math::Vector3(0.1f, 0.3f, 0.8f);
            skyConfig.horizonColor = Math::Vector3(0.5f, 0.7f, 1.0f);
            skyConfig.bottomColor = Math::Vector3(0.8f, 0.85f, 0.9f);
            skyConfig.sunDirection = Math::Vector3(0.0f, 1.0f, 0.0f);
            m_RenderSystem->SetSkybox(skyConfig);
        }
        m_RenderSystem->SetShadowsEnabled(true);
        m_RenderSystem->SetAmbientIntensity(0.15f);
        if (m_PostProcessing) {
            auto& pp = m_PostProcessing->GetSettings();
            pp.fxaaEnabled = 1;
        }

    } else if (templateId == "horror") {
        createGround();
        ECS::Entity player = createPlayer3D("Player");
        auto* playerT = m_World->GetComponent<ECS::TransformComponent>(player);
        if (playerT) playerT->position.y = 1.7f;
        auto& ctrl = m_World->AddComponent<ECS::FirstPersonController>(player);
        ctrl.moveSpeed = 3.0f;
        ctrl.mouseSensitivity = 0.12f;
        ctrl.sprintMultiplier = 1.3f;
        SetupCameraForController(player, "FirstPerson");

        // Flashlight (spot light following player)
        {
            ECS::Entity flashlight = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(flashlight, "Flashlight");
            auto& flt = m_World->AddComponent<ECS::TransformComponent>(flashlight);
            flt.position = Math::Vector3(0.0f, 1.7f, 0.0f);
            auto& lc = m_World->AddComponent<ECS::LightComponent>(flashlight);
            lc.type = ECS::LightType::Spot;
            lc.intensity = 3.0f;
            lc.range = 20.0f;
            lc.outerConeAngle = 30.0f;
            lc.castShadows = true;
            auto& follow = m_World->AddComponent<ECS::FollowTargetComponent>(flashlight);
            follow.target = player;
            follow.offset = Math::Vector3(0.3f, -0.2f, 0.0f);
        }

        // Collectible note
        {
            ECS::Entity note = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(note, "Note");
            auto& nt = m_World->AddComponent<ECS::TransformComponent>(note);
            nt.position = Math::Vector3(5.0f, 1.0f, -3.0f);
            nt.scale = Math::Vector3(0.4f, 0.6f, 0.05f);
            auto& nmat = m_World->AddComponent<ECS::MaterialComponent>(note);
            nmat.baseColor = Math::Vector3(0.9f, 0.85f, 0.7f);
            m_World->AddComponent<ECS::MeshComponent>(note, Renderer::MeshFactory::CreateCube(1.0f));
            auto& notes = m_World->AddComponent<ECS::NotesComponent>(note);
            notes.notes = "Day 14: The doors open on their own now. I hear footsteps but no one is there.";
            auto& interact = m_World->AddComponent<ECS::InteractableComponent>(note);
            interact.promptText = "Read Note";
            interact.interactionRange = 2.0f;
        }

        // Door with switch
        {
            ECS::Entity door = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(door, "Door");
            auto& dt = m_World->AddComponent<ECS::TransformComponent>(door);
            dt.position = Math::Vector3(0.0f, 1.5f, -8.0f);
            dt.scale = Math::Vector3(1.5f, 3.0f, 0.3f);
            auto& dmat = m_World->AddComponent<ECS::MaterialComponent>(door);
            dmat.baseColor = Math::Vector3(0.35f, 0.25f, 0.15f);
            m_World->AddComponent<ECS::MeshComponent>(door, Renderer::MeshFactory::CreateCube(1.0f));
            auto& sw = m_World->AddComponent<ECS::SwitchComponent>(door);
            sw.type = ECS::SwitchComponent::SwitchType::Toggle;
            sw.promptText = "Open Door";
            auto& bcol = m_World->AddComponent<ECS::BoxColliderComponent>(door);
            bcol.size = Math::Vector3(1.5f, 3.0f, 0.3f);
        }

        // Dust/smoke particles
        {
            ECS::Entity dust = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(dust, "Dust");
            auto& dt = m_World->AddComponent<ECS::TransformComponent>(dust);
            dt.position = Math::Vector3(0.0f, 2.0f, -4.0f);
            auto& pe = m_World->AddComponent<ECS::ParticleEmitterComponent>(dust);
            ECS::ApplyParticlePreset(pe, "Smoke");
            pe.emissionRate = 3.0f;
            pe.startSpeed = 0.2f;
            pe.startSize = 0.3f;
            pe.lifetime = 4.0f;
        }

        // Second room (through the door)
        {
            // Room walls
            const Math::Vector3 rPos[] = { {-4,1.5f,-12}, {4,1.5f,-12}, {0,1.5f,-16} };
            const Math::Vector3 rScl[] = { {0.3f,3,8}, {0.3f,3,8}, {8,3,0.3f} };
            const char* rNames[] = { "Room 2 Wall Left", "Room 2 Wall Right", "Room 2 Wall Back" };
            for (int i = 0; i < 3; ++i) {
                ECS::Entity wall = m_World->CreateEntity();
                m_World->AddComponent<ECS::NameComponent>(wall, rNames[i]);
                auto& wt = m_World->AddComponent<ECS::TransformComponent>(wall);
                wt.position = rPos[i];
                wt.scale = rScl[i];
                auto& wmat = m_World->AddComponent<ECS::MaterialComponent>(wall);
                wmat.baseColor = Math::Vector3(0.2f, 0.18f, 0.15f);
                m_World->AddComponent<ECS::MeshComponent>(wall, Renderer::MeshFactory::CreateCube(1.0f));
                auto& wcol = m_World->AddComponent<ECS::BoxColliderComponent>(wall);
                wcol.size = rScl[i];
            }

            // Dim light in room 2
            ECS::Entity r2Light = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(r2Light, "Room 2 Light");
            auto& r2lt = m_World->AddComponent<ECS::TransformComponent>(r2Light);
            r2lt.position = Math::Vector3(0.0f, 2.5f, -12.0f);
            auto& r2lc = m_World->AddComponent<ECS::LightComponent>(r2Light);
            r2lc.type = ECS::LightType::Point;
            r2lc.intensity = 0.8f;
            r2lc.range = 8.0f;
            r2lc.color = Math::Vector3(0.4f, 0.3f, 0.5f);
        }

        // Ambient sound entity (creaking)
        {
            ECS::Entity ambSound = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(ambSound, "Ambient Sound");
            auto& ast = m_World->AddComponent<ECS::TransformComponent>(ambSound);
            ast.position = Math::Vector3(0.0f, 2.0f, -10.0f);
            auto& ac = m_World->AddComponent<ECS::AudioSourceComponent>(ambSound);
            ac.loop = true;
            ac.volume = 0.3f;
            ac.is3D = true;
            auto& notes = m_World->AddComponent<ECS::NotesComponent>(ambSound);
            notes.notes = "Assign a creaking/dripping audio file to AudioSourceComponent.\n"
                "Spatial audio will make it louder as the player approaches.";
        }

        {
            Renderer::SkyboxConfig skyConfig;
            skyConfig.type = Renderer::SkyboxType::Procedural;
            skyConfig.topColor = Math::Vector3(0.02f, 0.02f, 0.04f);
            skyConfig.horizonColor = Math::Vector3(0.05f, 0.05f, 0.08f);
            skyConfig.bottomColor = Math::Vector3(0.03f, 0.03f, 0.05f);
            m_RenderSystem->SetSkybox(skyConfig);
        }
        m_RenderSystem->SetShadowsEnabled(true);
        m_RenderSystem->SetAmbientIntensity(0.02f);
        m_RenderSystem->SetFogParams(0.04f, 2.0f, 40.0f, 0.5f);
        m_RenderSystem->SetFogColor(Math::Vector3(0.05f, 0.05f, 0.08f));
        if (m_PostProcessing) {
            auto& pp = m_PostProcessing->GetSettings();
            pp.fxaaEnabled = 1;
            pp.vignetteEnabled = 1;
            pp.vignetteIntensity = 0.4f;
            pp.filmGrainEnabled = 1;
            pp.filmGrainIntensity = 0.1f;
        }

    } else if (templateId == "racing") {
        createGround();

        // Vehicle player
        ECS::Entity vehicle = m_World->CreateEntity();
        m_World->AddComponent<ECS::NameComponent>(vehicle, "Vehicle");
        {
            auto& vt = m_World->AddComponent<ECS::TransformComponent>(vehicle);
            vt.position = Math::Vector3(0.0f, 0.5f, 0.0f);
            auto& vmat = m_World->AddComponent<ECS::MaterialComponent>(vehicle);
            vmat.baseColor = Math::Vector3(0.9f, 0.2f, 0.1f);
            m_World->AddComponent<ECS::MeshComponent>(vehicle, Renderer::MeshFactory::CreateCube(1.0f));
            auto& vc = m_World->AddComponent<ECS::VehicleController>(vehicle);
            vc.maxSpeed = 30.0f;
            vc.acceleration = 15.0f;
            vc.maxSteerAngle = 35.0f;
        }

        // Chase camera
        {
            ECS::Entity cam = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(cam, "Chase Camera");
            auto& ct = m_World->AddComponent<ECS::TransformComponent>(cam);
            ct.position = Math::Vector3(0.0f, 8.0f, -6.0f);
            auto& cc = m_World->AddComponent<ECS::CameraComponent>(cam);
            cc.projectionType = ECS::ProjectionType::Perspective;
            cc.fieldOfView = 65.0f;
            cc.isActive = true;
            cc.priority = 10;
            auto& follow = m_World->AddComponent<ECS::FollowTargetComponent>(cam);
            follow.target = vehicle;
            follow.offset = Math::Vector3(0.0f, 8.0f, -6.0f);
            auto& lookAt = m_World->AddComponent<ECS::LookAtTargetComponent>(cam);
            lookAt.target = vehicle;
            m_SelectedGameCamera = cam;
        }

        // Track barriers
        {
            const Math::Vector3 bPos[] = { {0,0.5f,15}, {0,0.5f,-15}, {15,0.5f,0}, {-15,0.5f,0} };
            const Math::Vector3 bScl[] = { {30,1,0.5f}, {30,1,0.5f}, {0.5f,1,30}, {0.5f,1,30} };
            const char* bNames[] = { "Barrier North", "Barrier South", "Barrier East", "Barrier West" };
            for (int i = 0; i < 4; ++i) {
                ECS::Entity bar = m_World->CreateEntity();
                m_World->AddComponent<ECS::NameComponent>(bar, bNames[i]);
                auto& bt = m_World->AddComponent<ECS::TransformComponent>(bar);
                bt.position = bPos[i];
                bt.scale = bScl[i];
                auto& bmat = m_World->AddComponent<ECS::MaterialComponent>(bar);
                bmat.baseColor = Math::Vector3(0.8f, 0.8f, 0.8f);
                m_World->AddComponent<ECS::MeshComponent>(bar, Renderer::MeshFactory::CreateCube(1.0f));
                auto& bcol = m_World->AddComponent<ECS::BoxColliderComponent>(bar);
                bcol.size = bScl[i];
            }
        }

        // Checkpoints (2)
        {
            const Math::Vector3 cpPos[] = { {10.0f, 0.5f, 0.0f}, {-5.0f, 0.5f, 10.0f} };
            for (int i = 0; i < 2; ++i) {
                ECS::Entity cp = m_World->CreateEntity();
                m_World->AddComponent<ECS::NameComponent>(cp, "Checkpoint " + std::to_string(i + 1));
                auto& ct = m_World->AddComponent<ECS::TransformComponent>(cp);
                ct.position = cpPos[i];
                ct.scale = Math::Vector3(0.3f, 2.0f, 4.0f);
                auto& cmat = m_World->AddComponent<ECS::MaterialComponent>(cp);
                cmat.baseColor = Math::Vector3(1.0f, 0.8f, 0.0f);
                cmat.opacity = 0.5f;
                cmat.alphaMode = ECS::MaterialComponent::AlphaMode::Blend;
                m_World->AddComponent<ECS::MeshComponent>(cp, Renderer::MeshFactory::CreateCube(1.0f));
                auto& goal = m_World->AddComponent<ECS::GoalZoneComponent>(cp);
                goal.type = ECS::GoalZoneComponent::GoalType::Checkpoint;
            }
        }

        // Tire barrier obstacles
        {
            const Math::Vector3 tPos[] = { {5,0.4f,5}, {-8,0.4f,8}, {10,0.4f,-8}, {-3,0.4f,-10} };
            for (int i = 0; i < 4; ++i) {
                ECS::Entity tire = m_World->CreateEntity();
                m_World->AddComponent<ECS::NameComponent>(tire, "Tire Barrier " + std::to_string(i + 1));
                auto& tt = m_World->AddComponent<ECS::TransformComponent>(tire);
                tt.position = tPos[i];
                tt.scale = Math::Vector3(1.5f, 0.8f, 1.5f);
                auto& tmat = m_World->AddComponent<ECS::MaterialComponent>(tire);
                tmat.baseColor = Math::Vector3(0.15f, 0.15f, 0.15f);
                m_World->AddComponent<ECS::MeshComponent>(tire, Renderer::MeshFactory::CreateSphere(0.5f));
                auto& tcol = m_World->AddComponent<ECS::SphereColliderComponent>(tire);
                tcol.radius = 0.75f;
            }
        }

        // Finish line
        {
            ECS::Entity finish = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(finish, "Finish Line");
            auto& ft = m_World->AddComponent<ECS::TransformComponent>(finish);
            ft.position = Math::Vector3(0.0f, 0.5f, 12.0f);
            ft.scale = Math::Vector3(6.0f, 2.0f, 0.3f);
            auto& fmat = m_World->AddComponent<ECS::MaterialComponent>(finish);
            fmat.baseColor = Math::Vector3(0.1f, 0.9f, 0.1f);
            fmat.opacity = 0.5f;
            fmat.alphaMode = ECS::MaterialComponent::AlphaMode::Blend;
            m_World->AddComponent<ECS::MeshComponent>(finish, Renderer::MeshFactory::CreateCube(1.0f));
            auto& goal = m_World->AddComponent<ECS::GoalZoneComponent>(finish);
            goal.type = ECS::GoalZoneComponent::GoalType::LevelExit;
        }

        // Cinematic camera
        {
            ECS::Entity cineCam = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(cineCam, "Cinematic Camera");
            m_World->AddComponent<ECS::TransformComponent>(cineCam);
            auto& cine = m_World->AddComponent<ECS::CinematicCameraComponent>(cineCam);
            cine.waypoints.push_back({Math::Vector3(0,10,10), Math::Vector3(0,0,0), 60.0f, 3.0f, 0.5f});
            cine.waypoints.push_back({Math::Vector3(10,5,-5), Math::Vector3(0,0,0), 50.0f, 2.0f, 0.0f});
            cine.autoPlay = false;
        }

        {
            Renderer::SkyboxConfig skyConfig;
            skyConfig.type = Renderer::SkyboxType::Procedural;
            skyConfig.topColor = Math::Vector3(0.1f, 0.3f, 0.8f);
            skyConfig.horizonColor = Math::Vector3(0.5f, 0.7f, 1.0f);
            skyConfig.bottomColor = Math::Vector3(0.8f, 0.85f, 0.9f);
            skyConfig.sunDirection = Math::Vector3(0.0f, 1.0f, 0.0f);
            m_RenderSystem->SetSkybox(skyConfig);
        }
        m_RenderSystem->SetShadowsEnabled(true);
        m_RenderSystem->SetAmbientIntensity(0.15f);
        if (m_PostProcessing) {
            auto& pp = m_PostProcessing->GetSettings();
            pp.fxaaEnabled = 1;
            pp.bloomEnabled = 1;
            pp.bloomThreshold = 1.0f;
            pp.bloomIntensity = 0.2f;
        }

    } else if (templateId == "ps1rpg") {
        // Ground with retro material
        {
            ECS::Entity ground = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(ground, "Ground");
            auto& gt = m_World->AddComponent<ECS::TransformComponent>(ground);
            gt.scale = Math::Vector3(30.0f, 1.0f, 30.0f);
            gt.position = Math::Vector3(0.0f, -0.5f, 0.0f);
            auto& gmat = m_World->AddComponent<ECS::MaterialComponent>(ground);
            gmat.baseColor = Math::Vector3(0.3f, 0.5f, 0.2f);
            gmat.roughness = 0.9f;
            gmat.flatShading = true;
            gmat.vertexSnapping = true;
            gmat.vertexSnapResolution = 160;
            m_World->AddComponent<ECS::MeshComponent>(ground, Renderer::MeshFactory::CreateCube(1.0f));
            auto& col = m_World->AddComponent<ECS::BoxColliderComponent>(ground);
            col.size = Math::Vector3(30.0f, 1.0f, 30.0f);
        }

        ECS::Entity player = createPlayer3D("Player");
        auto& ctrl = m_World->AddComponent<ECS::TopDown3DController>(player);
        ctrl.moveSpeed = 5.0f;
        ctrl.cameraAngle = 55.0f;
        ctrl.cameraDistance = 12.0f;
        SetupCameraForController(player, "TopDown3D");

        // 2 NPCs with dialogue
        {
            const Math::Vector3 npcPos[] = { {4,1,2}, {-3,1,-4} };
            const char* npcNames[] = { "Elder", "Merchant" };
            const char* greetings[][3] = {
                { "Greetings, adventurer.", "The ancient temple lies to the north.", "Be wary of the guardians." },
                { "Welcome to my shop!", "I have potions and equipment.", "Come back anytime." }
            };
            for (int i = 0; i < 2; ++i) {
                ECS::Entity npc = m_World->CreateEntity();
                m_World->AddComponent<ECS::NameComponent>(npc, npcNames[i]);
                auto& nt = m_World->AddComponent<ECS::TransformComponent>(npc);
                nt.position = npcPos[i];
                auto& nmat = m_World->AddComponent<ECS::MaterialComponent>(npc);
                nmat.baseColor = Math::Vector3(0.8f, 0.6f, 0.5f);
                nmat.flatShading = true;
                nmat.vertexSnapping = true;
                nmat.vertexSnapResolution = 160;
                m_World->AddComponent<ECS::MeshComponent>(npc, Renderer::MeshFactory::CreateCapsule(0.3f, 1.0f));
                auto& dlg = m_World->AddComponent<ECS::DialogueComponent>(npc);
                dlg.speakerName = npcNames[i];
                dlg.dialogueLines = { greetings[i][0], greetings[i][1], greetings[i][2] };
            }
        }

        // Save point
        {
            ECS::Entity savePoint = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(savePoint, "Save Point");
            auto& st = m_World->AddComponent<ECS::TransformComponent>(savePoint);
            st.position = Math::Vector3(0.0f, 0.5f, 5.0f);
            st.scale = Math::Vector3(0.5f, 1.5f, 0.5f);
            auto& smat = m_World->AddComponent<ECS::MaterialComponent>(savePoint);
            smat.baseColor = Math::Vector3(0.3f, 0.5f, 0.9f);
            smat.emissiveColor = Math::Vector3(0.2f, 0.4f, 1.0f);
            smat.emissiveStrength = 0.8f;
            m_World->AddComponent<ECS::MeshComponent>(savePoint, Renderer::MeshFactory::CreateCube(1.0f));
            auto& sd = m_World->AddComponent<ECS::SaveDataComponent>(savePoint);
            sd.tier = ECS::PersistenceTier::SceneState;
            auto& pe = m_World->AddComponent<ECS::ParticleEmitterComponent>(savePoint);
            ECS::ApplyParticlePreset(pe, "Magic");
        }

        // Treasure chest
        {
            ECS::Entity chest = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(chest, "Treasure Chest");
            auto& ct = m_World->AddComponent<ECS::TransformComponent>(chest);
            ct.position = Math::Vector3(6.0f, 0.3f, -2.0f);
            ct.scale = Math::Vector3(0.8f, 0.6f, 0.5f);
            auto& cmat = m_World->AddComponent<ECS::MaterialComponent>(chest);
            cmat.baseColor = Math::Vector3(0.6f, 0.45f, 0.15f);
            cmat.flatShading = true;
            cmat.vertexSnapping = true;
            cmat.vertexSnapResolution = 160;
            m_World->AddComponent<ECS::MeshComponent>(chest, Renderer::MeshFactory::CreateCube(1.0f));
            auto& pk = m_World->AddComponent<ECS::PickupComponent>(chest);
            pk.type = ECS::PickupComponent::PickupType::Health;
            pk.customId = "Potion";
            pk.value = 3.0f;
            m_World->AddComponent<ECS::InteractableComponent>(chest).promptText = "Open Chest";
        }

        // Dungeon entrance
        {
            ECS::Entity entrance = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(entrance, "Dungeon Entrance");
            auto& et = m_World->AddComponent<ECS::TransformComponent>(entrance);
            et.position = Math::Vector3(0.0f, 1.5f, -10.0f);
            et.scale = Math::Vector3(3.0f, 3.0f, 0.5f);
            auto& emat = m_World->AddComponent<ECS::MaterialComponent>(entrance);
            emat.baseColor = Math::Vector3(0.3f, 0.25f, 0.2f);
            emat.flatShading = true;
            emat.vertexSnapping = true;
            emat.vertexSnapResolution = 160;
            m_World->AddComponent<ECS::MeshComponent>(entrance, Renderer::MeshFactory::CreateCube(1.0f));
            auto& bcol = m_World->AddComponent<ECS::BoxColliderComponent>(entrance);
            bcol.size = et.scale;
            auto& sw = m_World->AddComponent<ECS::SwitchComponent>(entrance);
            sw.type = ECS::SwitchComponent::SwitchType::Toggle;
            sw.promptText = "Enter Dungeon";
        }

        // Battle arena trigger zone
        {
            ECS::Entity battleZone = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(battleZone, "Battle Zone");
            auto& bzt = m_World->AddComponent<ECS::TransformComponent>(battleZone);
            bzt.position = Math::Vector3(8.0f, 0.05f, 5.0f);
            bzt.scale = Math::Vector3(4.0f, 0.1f, 4.0f);
            auto& bzmat = m_World->AddComponent<ECS::MaterialComponent>(battleZone);
            bzmat.baseColor = Math::Vector3(0.5f, 0.2f, 0.2f);
            bzmat.opacity = 0.3f;
            bzmat.alphaMode = ECS::MaterialComponent::AlphaMode::Blend;
            m_World->AddComponent<ECS::MeshComponent>(battleZone, Renderer::MeshFactory::CreateCube(1.0f));
            auto& trigger = m_World->AddComponent<ECS::TriggerZoneComponent>(battleZone);
            trigger.shape = ECS::TriggerZoneComponent::Shape::Box;
            trigger.boxSize = Math::Vector3(4.0f, 2.0f, 4.0f);
            auto& notes = m_World->AddComponent<ECS::NotesComponent>(battleZone);
            notes.notes = "Stepping into this zone triggers a random encounter.\n"
                "Wire via VisualScript or AngelScript.";
        }

        {
            Renderer::SkyboxConfig skyConfig;
            skyConfig.type = Renderer::SkyboxType::Procedural;
            skyConfig.topColor = Math::Vector3(0.1f, 0.2f, 0.5f);
            skyConfig.horizonColor = Math::Vector3(0.4f, 0.5f, 0.7f);
            skyConfig.bottomColor = Math::Vector3(0.3f, 0.3f, 0.35f);
            m_RenderSystem->SetSkybox(skyConfig);
        }
        m_RenderSystem->SetShadowsEnabled(true);
        m_RenderSystem->SetAmbientIntensity(0.15f);
        if (m_PostProcessing) {
            auto& pp = m_PostProcessing->GetSettings();
            pp.ditherEnabled = 1;
            pp.colorQuantEnabled = 1;
            pp.colorBitDepth = 5;
            pp.resDownscaleEnabled = 1;
            pp.internalWidth = 320;
            pp.internalHeight = 240;
        }

    } else if (templateId == "arena") {
        createGround();

        // 2 players with health + stamina
        for (int i = 0; i < 2; ++i) {
            ECS::Entity fighter = createPlayer3D("Player " + std::to_string(i + 1));
            auto* ft = m_World->GetComponent<ECS::TransformComponent>(fighter);
            if (ft) ft->position = Math::Vector3(-3.0f + i * 6.0f, 1.0f, 0.0f);
            auto& fctrl = m_World->AddComponent<ECS::ThirdPersonController>(fighter);
            fctrl.moveSpeed = 6.0f;
            fctrl.cameraDistance = 5.0f;
            fctrl.cameraHeight = 2.0f;
            fctrl.gamepadIndex = i;
            m_World->AddComponent<ECS::HealthComponent>(fighter);
            auto& res = m_World->AddComponent<ECS::ResourceComponent>(fighter);
            res.resourceName = "Stamina";

            // Per-player camera with viewport
            ECS::Entity cam = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(cam, "Camera P" + std::to_string(i + 1));
            auto& ct = m_World->AddComponent<ECS::TransformComponent>(cam);
            ct.position = Math::Vector3(-3.0f + i * 6.0f, 5.0f, -5.0f);
            auto& cc = m_World->AddComponent<ECS::CameraComponent>(cam);
            cc.projectionType = ECS::ProjectionType::Perspective;
            cc.fieldOfView = 60.0f;
            cc.isActive = true;
            cc.priority = 10;
            cc.viewportX = i * 0.5f;
            cc.viewportY = 0.0f;
            cc.viewportWidth = 0.5f;
            cc.viewportHeight = 1.0f;
            auto& follow = m_World->AddComponent<ECS::FollowTargetComponent>(cam);
            follow.target = fighter;
            follow.offset = Math::Vector3(0.0f, 5.0f, -5.0f);
            auto& lookAt = m_World->AddComponent<ECS::LookAtTargetComponent>(cam);
            lookAt.target = fighter;
            if (i == 0) m_SelectedGameCamera = cam;
        }

        // Arena walls
        {
            const Math::Vector3 wPos[] = { {0,1,8}, {0,1,-8}, {8,1,0}, {-8,1,0} };
            const Math::Vector3 wScl[] = { {16,2,0.5f}, {16,2,0.5f}, {0.5f,2,16}, {0.5f,2,16} };
            for (int i = 0; i < 4; ++i) {
                ECS::Entity wall = m_World->CreateEntity();
                m_World->AddComponent<ECS::NameComponent>(wall, "Arena Wall " + std::to_string(i + 1));
                auto& wt = m_World->AddComponent<ECS::TransformComponent>(wall);
                wt.position = wPos[i];
                wt.scale = wScl[i];
                auto& wmat = m_World->AddComponent<ECS::MaterialComponent>(wall);
                wmat.baseColor = Math::Vector3(0.5f, 0.45f, 0.4f);
                m_World->AddComponent<ECS::MeshComponent>(wall, Renderer::MeshFactory::CreateCube(1.0f));
                auto& wcol = m_World->AddComponent<ECS::BoxColliderComponent>(wall);
                wcol.size = wScl[i];
            }
        }

        // 2 platform cubes
        {
            for (int i = 0; i < 2; ++i) {
                ECS::Entity plat = m_World->CreateEntity();
                m_World->AddComponent<ECS::NameComponent>(plat, "Platform " + std::to_string(i + 1));
                auto& pt = m_World->AddComponent<ECS::TransformComponent>(plat);
                pt.position = Math::Vector3(-2.0f + i * 5.0f, 0.5f + i * 0.5f, 3.0f - i * 5.0f);
                pt.scale = Math::Vector3(2, 1 + i, 2);
                auto& pmat = m_World->AddComponent<ECS::MaterialComponent>(plat);
                pmat.baseColor = Math::Vector3(0.45f, 0.4f, 0.35f);
                m_World->AddComponent<ECS::MeshComponent>(plat, Renderer::MeshFactory::CreateCube(1.0f));
                auto& pcol = m_World->AddComponent<ECS::BoxColliderComponent>(plat);
                pcol.size = pt.scale;
            }
        }

        // Health pickup in center
        {
            ECS::Entity healthPU = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(healthPU, "Health Pickup");
            auto& hpt = m_World->AddComponent<ECS::TransformComponent>(healthPU);
            hpt.position = Math::Vector3(0.0f, 0.5f, 0.0f);
            hpt.scale = Math::Vector3(0.6f, 0.6f, 0.6f);
            auto& hpmat = m_World->AddComponent<ECS::MaterialComponent>(healthPU);
            hpmat.baseColor = Math::Vector3(0.2f, 0.9f, 0.2f);
            hpmat.emissiveColor = Math::Vector3(0.2f, 0.9f, 0.2f);
            hpmat.emissiveStrength = 0.8f;
            m_World->AddComponent<ECS::MeshComponent>(healthPU, Renderer::MeshFactory::CreateSphere(0.3f));
            auto& pk = m_World->AddComponent<ECS::PickupComponent>(healthPU);
            pk.type = ECS::PickupComponent::PickupType::Health;
            pk.customId = "Health Pack";
            pk.value = 25.0f;
            auto& tw = m_World->AddComponent<ECS::TweenComponent>(healthPU);
            ECS::TweenEntry bob;
            bob.property = ECS::TweenProperty::Position;
            bob.easing = ECS::EasingType::EaseInOutSine;
            bob.mode = ECS::TweenMode::PingPong;
            bob.startValue = Math::Vector3(0.0f, 0.5f, 0.0f);
            bob.endValue = Math::Vector3(0.0f, 1.2f, 0.0f);
            bob.duration = 2.0f;
            tw.tweens.push_back(bob);
        }

        // Destructible pillar
        {
            ECS::Entity pillar = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(pillar, "Destructible Pillar");
            auto& pt = m_World->AddComponent<ECS::TransformComponent>(pillar);
            pt.position = Math::Vector3(0.0f, 1.5f, 4.0f);
            pt.scale = Math::Vector3(1.0f, 3.0f, 1.0f);
            auto& pmat = m_World->AddComponent<ECS::MaterialComponent>(pillar);
            pmat.baseColor = Math::Vector3(0.55f, 0.5f, 0.45f);
            m_World->AddComponent<ECS::MeshComponent>(pillar, Renderer::MeshFactory::CreateCube(1.0f));
            auto& bcol = m_World->AddComponent<ECS::BoxColliderComponent>(pillar);
            bcol.size = pt.scale;
            auto& dest = m_World->AddComponent<ECS::DestructibleComponent>(pillar);
            dest.health = 50.0f;
        }

        // Spotlight
        {
            ECS::Entity spot = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(spot, "Arena Light");
            auto& st = m_World->AddComponent<ECS::TransformComponent>(spot);
            st.position = Math::Vector3(0.0f, 8.0f, 0.0f);
            auto& slc = m_World->AddComponent<ECS::LightComponent>(spot);
            slc.type = ECS::LightType::Point;
            slc.intensity = 3.0f;
            slc.range = 20.0f;
            slc.color = Math::Vector3(1.0f, 0.95f, 0.9f);
        }

        {
            Renderer::SkyboxConfig skyConfig;
            skyConfig.type = Renderer::SkyboxType::Procedural;
            skyConfig.topColor = Math::Vector3(0.15f, 0.1f, 0.2f);
            skyConfig.horizonColor = Math::Vector3(0.3f, 0.2f, 0.35f);
            skyConfig.bottomColor = Math::Vector3(0.1f, 0.08f, 0.12f);
            m_RenderSystem->SetSkybox(skyConfig);
        }
        m_RenderSystem->SetShadowsEnabled(true);
        m_RenderSystem->SetAmbientIntensity(0.1f);
        if (m_PostProcessing) {
            auto& pp = m_PostProcessing->GetSettings();
            pp.fxaaEnabled = 1;
            pp.bloomEnabled = 1;
            pp.bloomThreshold = 1.0f;
            pp.bloomIntensity = 0.3f;
        }

    } else if (templateId == "physics") {
        createGround();

        // Ramp
        {
            ECS::Entity ramp = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(ramp, "Ramp");
            auto& rt = m_World->AddComponent<ECS::TransformComponent>(ramp);
            rt.position = Math::Vector3(-5.0f, 1.0f, 0.0f);
            rt.scale = Math::Vector3(4.0f, 0.3f, 3.0f);
            rt.rotation = Math::Quaternion(Math::Vector3(0, 0, 1), Math::Radians(30.0f));
            auto& rmat = m_World->AddComponent<ECS::MaterialComponent>(ramp);
            rmat.baseColor = Math::Vector3(0.6f, 0.55f, 0.5f);
            m_World->AddComponent<ECS::MeshComponent>(ramp, Renderer::MeshFactory::CreateCube(1.0f));
            auto& rcol = m_World->AddComponent<ECS::BoxColliderComponent>(ramp);
            rcol.size = Math::Vector3(4.0f, 0.3f, 3.0f);
        }

        // 5 rigidbody objects
        {
            for (int i = 0; i < 5; ++i) {
                ECS::Entity obj = m_World->CreateEntity();
                const char* names[] = { "Ball 1", "Ball 2", "Box 1", "Box 2", "Capsule" };
                m_World->AddComponent<ECS::NameComponent>(obj, names[i]);
                auto& ot = m_World->AddComponent<ECS::TransformComponent>(obj);
                ot.position = Math::Vector3(-3.0f + i * 2.0f, 5.0f + i * 0.5f, 0.0f);
                auto& omat = m_World->AddComponent<ECS::MaterialComponent>(obj);
                const Math::Vector3 colors[] = { {0.9f,0.3f,0.2f}, {0.2f,0.7f,0.3f}, {0.3f,0.4f,0.9f}, {0.9f,0.7f,0.1f}, {0.7f,0.3f,0.8f} };
                omat.baseColor = colors[i];
                if (i < 2) {
                    m_World->AddComponent<ECS::MeshComponent>(obj, Renderer::MeshFactory::CreateSphere(0.5f));
                    auto& sc = m_World->AddComponent<ECS::SphereColliderComponent>(obj);
                    sc.radius = 0.5f;
                } else if (i < 4) {
                    m_World->AddComponent<ECS::MeshComponent>(obj, Renderer::MeshFactory::CreateCube(1.0f));
                    auto& bc = m_World->AddComponent<ECS::BoxColliderComponent>(obj);
                    bc.size = Math::Vector3(1, 1, 1);
                } else {
                    m_World->AddComponent<ECS::MeshComponent>(obj, Renderer::MeshFactory::CreateCapsule(0.3f, 1.0f));
                    auto& cc = m_World->AddComponent<ECS::CapsuleColliderComponent>(obj);
                    cc.radius = 0.3f;
                    cc.height = 1.0f;
                }
                auto& rb = m_World->AddComponent<ECS::RigidbodyComponent>(obj);
                rb.mass = 1.0f + i * 0.5f;
                rb.useGravity = true;
            }
        }

        // Gravity zone (inverted point)
        {
            ECS::Entity gz = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(gz, "Gravity Zone");
            auto& gzt = m_World->AddComponent<ECS::TransformComponent>(gz);
            gzt.position = Math::Vector3(8.0f, 3.0f, 0.0f);
            auto& gzc = m_World->AddComponent<ECS::GravityZoneComponent>(gz);
            gzc.mode = ECS::GravityZoneMode::Point;
            gzc.shape = ECS::GravityZoneShape::Sphere;
            gzc.halfExtents = Math::Vector3(6, 6, 6);
            gzc.gravityStrength = 12.0f;
        }

        // Conveyor
        {
            ECS::Entity conv = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(conv, "Conveyor");
            auto& ct = m_World->AddComponent<ECS::TransformComponent>(conv);
            ct.position = Math::Vector3(0.0f, 0.1f, 5.0f);
            ct.scale = Math::Vector3(6.0f, 0.2f, 2.0f);
            auto& cmat = m_World->AddComponent<ECS::MaterialComponent>(conv);
            cmat.baseColor = Math::Vector3(0.3f, 0.3f, 0.35f);
            m_World->AddComponent<ECS::MeshComponent>(conv, Renderer::MeshFactory::CreateCube(1.0f));
            auto& cc = m_World->AddComponent<ECS::ConveyorComponent>(conv);
            cc.direction = Math::Vector3(1, 0, 0);
            cc.speed = 3.0f;
        }

        // Moving platform
        {
            ECS::Entity mp = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(mp, "Moving Platform");
            auto& mt = m_World->AddComponent<ECS::TransformComponent>(mp);
            mt.position = Math::Vector3(-8.0f, 1.0f, 0.0f);
            mt.scale = Math::Vector3(3.0f, 0.3f, 3.0f);
            auto& mmat = m_World->AddComponent<ECS::MaterialComponent>(mp);
            mmat.baseColor = Math::Vector3(0.4f, 0.6f, 0.4f);
            m_World->AddComponent<ECS::MeshComponent>(mp, Renderer::MeshFactory::CreateCube(1.0f));
            auto& mpc = m_World->AddComponent<ECS::MovingPlatformComponent>(mp);
            mpc.waypoints = { Math::Vector3(-8,1,0), Math::Vector3(-8,5,0) };
            mpc.speed = 2.0f;
            mpc.mode = ECS::MovingPlatformComponent::PlatformMode::PingPong;
        }

        // Pendulum (hinge joint demo)
        {
            // Anchor point (static)
            ECS::Entity anchor = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(anchor, "Pendulum Anchor");
            auto& anT = m_World->AddComponent<ECS::TransformComponent>(anchor);
            anT.position = Math::Vector3(4.0f, 6.0f, -5.0f);
            anT.scale = Math::Vector3(0.3f, 0.3f, 0.3f);
            auto& anMat = m_World->AddComponent<ECS::MaterialComponent>(anchor);
            anMat.baseColor = Math::Vector3(0.5f, 0.5f, 0.5f);
            m_World->AddComponent<ECS::MeshComponent>(anchor, Renderer::MeshFactory::CreateCube(1.0f));

            // Pendulum bob
            ECS::Entity bob = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(bob, "Pendulum Bob");
            auto& bt = m_World->AddComponent<ECS::TransformComponent>(bob);
            bt.position = Math::Vector3(6.0f, 4.0f, -5.0f);
            auto& bmat = m_World->AddComponent<ECS::MaterialComponent>(bob);
            bmat.baseColor = Math::Vector3(0.8f, 0.3f, 0.1f);
            m_World->AddComponent<ECS::MeshComponent>(bob, Renderer::MeshFactory::CreateSphere(0.5f));
            auto& sc = m_World->AddComponent<ECS::SphereColliderComponent>(bob);
            sc.radius = 0.5f;
            auto& rb = m_World->AddComponent<ECS::RigidbodyComponent>(bob);
            rb.mass = 2.0f;
            rb.useGravity = true;

            // Hinge joint connecting anchor to bob
            auto& joint = m_World->AddComponent<ECS::HingeJointComponent>(bob);
            joint.entityA = anchor;
            joint.entityB = bob;
            joint.anchorA = Math::Vector3(0, -0.5f, 0);
            joint.axis = Math::Vector3(0, 0, 1);
        }

        // Breakable wall
        {
            ECS::Entity bWall = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(bWall, "Breakable Wall");
            auto& bwt = m_World->AddComponent<ECS::TransformComponent>(bWall);
            bwt.position = Math::Vector3(0.0f, 1.5f, -8.0f);
            bwt.scale = Math::Vector3(4.0f, 3.0f, 0.5f);
            auto& bwmat = m_World->AddComponent<ECS::MaterialComponent>(bWall);
            bwmat.baseColor = Math::Vector3(0.6f, 0.4f, 0.3f);
            m_World->AddComponent<ECS::MeshComponent>(bWall, Renderer::MeshFactory::CreateCube(1.0f));
            auto& bwcol = m_World->AddComponent<ECS::BoxColliderComponent>(bWall);
            bwcol.size = bwt.scale;
            auto& dest = m_World->AddComponent<ECS::DestructibleComponent>(bWall);
            dest.health = 30.0f;
        }

        {
            Renderer::SkyboxConfig skyConfig;
            skyConfig.type = Renderer::SkyboxType::Procedural;
            skyConfig.topColor = Math::Vector3(0.1f, 0.3f, 0.8f);
            skyConfig.horizonColor = Math::Vector3(0.5f, 0.7f, 1.0f);
            skyConfig.bottomColor = Math::Vector3(0.8f, 0.85f, 0.9f);
            skyConfig.sunDirection = Math::Vector3(0.0f, 1.0f, 0.0f);
            m_RenderSystem->SetSkybox(skyConfig);
        }
        m_RenderSystem->SetShadowsEnabled(true);
        m_RenderSystem->SetAmbientIntensity(0.15f);
        if (m_PostProcessing) {
            auto& pp = m_PostProcessing->GetSettings();
            pp.fxaaEnabled = 1;
        }

    } else if (templateId == "narrative") {
        createGround();
        ECS::Entity player = createPlayer3D("Player");
        auto& ctrl = m_World->AddComponent<ECS::ThirdPersonController>(player);
        ctrl.moveSpeed = 4.0f;
        ctrl.cameraDistance = 6.0f;
        ctrl.cameraHeight = 2.0f;
        SetupCameraForController(player, "ThirdPerson");

        // 3 NPCs with dialogue
        {
            const Math::Vector3 npcPos[] = { {3,1,2}, {-4,1,0}, {0,1,-5} };
            const char* npcNames[] = { "Quest Giver", "Merchant", "Storyteller" };
            const char* lines[][3] = {
                { "I need your help!", "Find the lost amulet in the ruins.", "Return it for a reward." },
                { "Looking to buy?", "I have the finest wares.", "Everything is fairly priced." },
                { "Long ago, this land was different...", "The ancient ones built great monuments.", "Their secrets are lost to time." }
            };
            for (int i = 0; i < 3; ++i) {
                ECS::Entity npc = m_World->CreateEntity();
                m_World->AddComponent<ECS::NameComponent>(npc, npcNames[i]);
                auto& nt = m_World->AddComponent<ECS::TransformComponent>(npc);
                nt.position = npcPos[i];
                auto& nmat = m_World->AddComponent<ECS::MaterialComponent>(npc);
                nmat.baseColor = Math::Vector3(0.7f + i * 0.1f, 0.5f, 0.4f);
                m_World->AddComponent<ECS::MeshComponent>(npc, Renderer::MeshFactory::CreateCapsule(0.3f, 1.0f));
                auto& dlg = m_World->AddComponent<ECS::DialogueComponent>(npc);
                dlg.speakerName = npcNames[i];
                dlg.dialogueLines = { lines[i][0], lines[i][1], lines[i][2] };
                auto& notes = m_World->AddComponent<ECS::NotesComponent>(npc);
                notes.notes = std::string("NPC: ") + npcNames[i] + "\nOpen the Dialogue Editor to create branching conversations.";
            }
        }

        // Quest state entity
        {
            ECS::Entity quest = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(quest, "Main Quest");
            m_World->AddComponent<ECS::TransformComponent>(quest);
            auto& qs = m_World->AddComponent<ECS::QuestStateComponent>(quest);
            qs.questId = "find_amulet";
        }

        // Dialogue box entity
        {
            ECS::Entity dbox = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(dbox, "Dialogue Box");
            m_World->AddComponent<ECS::TransformComponent>(dbox);
            m_World->AddComponent<ECS::DialogueBoxComponent>(dbox);
        }

        // Interactable objects
        {
            ECS::Entity book = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(book, "Old Book");
            auto& bt = m_World->AddComponent<ECS::TransformComponent>(book);
            bt.position = Math::Vector3(-2.0f, 0.5f, -3.0f);
            bt.scale = Math::Vector3(0.4f, 0.5f, 0.3f);
            auto& bmat = m_World->AddComponent<ECS::MaterialComponent>(book);
            bmat.baseColor = Math::Vector3(0.5f, 0.3f, 0.2f);
            m_World->AddComponent<ECS::MeshComponent>(book, Renderer::MeshFactory::CreateCube(1.0f));
            auto& bi = m_World->AddComponent<ECS::InteractableComponent>(book);
            bi.promptText = "Read Book";
            auto& bd = m_World->AddComponent<ECS::DialogueComponent>(book);
            bd.dialogueLines = { "The pages describe an ancient ritual...", "Something about a lost amulet." };

            ECS::Entity chest = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(chest, "Mysterious Chest");
            auto& cht = m_World->AddComponent<ECS::TransformComponent>(chest);
            cht.position = Math::Vector3(5.0f, 0.3f, -4.0f);
            cht.scale = Math::Vector3(0.8f, 0.6f, 0.5f);
            auto& chmat = m_World->AddComponent<ECS::MaterialComponent>(chest);
            chmat.baseColor = Math::Vector3(0.55f, 0.4f, 0.2f);
            m_World->AddComponent<ECS::MeshComponent>(chest, Renderer::MeshFactory::CreateCube(1.0f));
            auto& ci = m_World->AddComponent<ECS::InteractableComponent>(chest);
            ci.promptText = "Open Chest";
            auto& pk = m_World->AddComponent<ECS::PickupComponent>(chest);
            pk.type = ECS::PickupComponent::PickupType::Custom;
            pk.customId = "Story Fragment";
            pk.value = 1.0f;
        }

        // Ambient firefly particles
        {
            ECS::Entity fireflies = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(fireflies, "Fireflies");
            auto& ft = m_World->AddComponent<ECS::TransformComponent>(fireflies);
            ft.position = Math::Vector3(0.0f, 1.5f, 0.0f);
            auto& pe = m_World->AddComponent<ECS::ParticleEmitterComponent>(fireflies);
            pe.emissionRate = 8.0f;
            pe.startSpeed = 0.5f;
            pe.startSize = 0.08f;
            pe.lifetime = 4.0f;
            pe.startColor = Math::Vector3(0.8f, 1.0f, 0.3f);
            pe.gravity = Math::Vector3(0.0f, 0.0f, 0.0f);
            pe.shape = ECS::ParticleEmitterComponent::EmitterShape::Sphere;
            pe.shapeRadius = 8.0f;
        }

        // Point light near quest giver
        {
            ECS::Entity ql = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(ql, "Quest Giver Light");
            auto& qlt = m_World->AddComponent<ECS::TransformComponent>(ql);
            qlt.position = Math::Vector3(3.0f, 3.0f, 2.0f);
            auto& qlc = m_World->AddComponent<ECS::LightComponent>(ql);
            qlc.type = ECS::LightType::Point;
            qlc.intensity = 1.5f;
            qlc.range = 8.0f;
            qlc.color = Math::Vector3(1.0f, 0.9f, 0.7f);
        }

        {
            Renderer::SkyboxConfig skyConfig;
            skyConfig.type = Renderer::SkyboxType::Procedural;
            skyConfig.topColor = Math::Vector3(0.08f, 0.15f, 0.4f);
            skyConfig.horizonColor = Math::Vector3(0.4f, 0.35f, 0.5f);
            skyConfig.bottomColor = Math::Vector3(0.2f, 0.2f, 0.25f);
            skyConfig.sunDirection = Math::Vector3(0.3f, 0.3f, 0.0f);
            m_RenderSystem->SetSkybox(skyConfig);
        }
        m_RenderSystem->SetShadowsEnabled(true);
        m_RenderSystem->SetAmbientIntensity(0.1f);
        if (m_PostProcessing) {
            auto& pp = m_PostProcessing->GetSettings();
            pp.fxaaEnabled = 1;
            pp.vignetteEnabled = 1;
            pp.vignetteIntensity = 0.15f;
        }

    } else if (templateId == "savesystem") {
        createGround();
        ECS::Entity player = createPlayer3D("Player");
        auto& ctrl = m_World->AddComponent<ECS::TopDown3DController>(player);
        ctrl.moveSpeed = 5.0f;
        ctrl.cameraAngle = 45.0f;
        ctrl.cameraDistance = 12.0f;
        SetupCameraForController(player, "TopDown3D");

        // 3 collectibles (RunState tier)
        {
            const Math::Vector3 cPos[] = { {3,0.5f,2}, {-2,0.5f,4}, {5,0.5f,-3} };
            for (int i = 0; i < 3; ++i) {
                ECS::Entity col = m_World->CreateEntity();
                m_World->AddComponent<ECS::NameComponent>(col, "Collectible " + std::to_string(i + 1));
                auto& ct = m_World->AddComponent<ECS::TransformComponent>(col);
                ct.position = cPos[i];
                auto& cmat = m_World->AddComponent<ECS::MaterialComponent>(col);
                cmat.baseColor = Math::Vector3(0.2f, 0.8f, 0.3f);
                m_World->AddComponent<ECS::MeshComponent>(col, Renderer::MeshFactory::CreateSphere(0.4f));
                auto& sd = m_World->AddComponent<ECS::SaveDataComponent>(col);
                sd.tier = ECS::PersistenceTier::RunState;
                sd.savePosition = true;
                auto& pick = m_World->AddComponent<ECS::PickupComponent>(col);
                pick.type = ECS::PickupComponent::PickupType::Health;
                pick.value = 10.0f;
            }
        }

        // Checkpoint (SceneState)
        {
            ECS::Entity cp = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(cp, "Checkpoint");
            auto& ct = m_World->AddComponent<ECS::TransformComponent>(cp);
            ct.position = Math::Vector3(0.0f, 0.5f, -5.0f);
            ct.scale = Math::Vector3(0.5f, 1.5f, 0.5f);
            auto& cmat = m_World->AddComponent<ECS::MaterialComponent>(cp);
            cmat.baseColor = Math::Vector3(0.3f, 0.5f, 0.9f);
            cmat.emissiveColor = Math::Vector3(0.2f, 0.4f, 1.0f);
            cmat.emissiveStrength = 0.6f;
            m_World->AddComponent<ECS::MeshComponent>(cp, Renderer::MeshFactory::CreateCube(1.0f));
            auto& sd = m_World->AddComponent<ECS::SaveDataComponent>(cp);
            sd.tier = ECS::PersistenceTier::SceneState;
            auto& pe = m_World->AddComponent<ECS::ParticleEmitterComponent>(cp);
            ECS::ApplyParticlePreset(pe, "Magic");
        }

        // Meta-progression entity
        {
            ECS::Entity meta = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(meta, "Meta Progression");
            m_World->AddComponent<ECS::TransformComponent>(meta);
            auto& sd = m_World->AddComponent<ECS::SaveDataComponent>(meta);
            sd.tier = ECS::PersistenceTier::MetaProgression;
            sd.customData = { {"totalRuns", "0"}, {"bestTime", "999"} };
        }

        // Save/Load menu
        {
            ECS::Entity saveMenu = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(saveMenu, "Save/Load Menu");
            m_World->AddComponent<ECS::TransformComponent>(saveMenu);
            auto& slm = m_World->AddComponent<ECS::SaveLoadMenuComponent>(saveMenu);
            slm.columnsPerRow = 4;
            slm.allowManualSave = true;
            slm.allowManualLoad = true;
        }

        // Danger zone (lava/hazard)
        {
            ECS::Entity danger = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(danger, "Danger Zone");
            auto& dt = m_World->AddComponent<ECS::TransformComponent>(danger);
            dt.position = Math::Vector3(-4.0f, 0.05f, 2.0f);
            dt.scale = Math::Vector3(3.0f, 0.1f, 3.0f);
            auto& dmat = m_World->AddComponent<ECS::MaterialComponent>(danger);
            dmat.baseColor = Math::Vector3(0.9f, 0.3f, 0.1f);
            dmat.emissiveColor = Math::Vector3(0.9f, 0.2f, 0.0f);
            dmat.emissiveStrength = 0.6f;
            m_World->AddComponent<ECS::MeshComponent>(danger, Renderer::MeshFactory::CreateCube(1.0f));
            auto& trigger = m_World->AddComponent<ECS::TriggerZoneComponent>(danger);
            trigger.shape = ECS::TriggerZoneComponent::Shape::Box;
            trigger.boxSize = Math::Vector3(3.0f, 1.0f, 3.0f);
        }

        // Score display
        {
            ECS::Entity score = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(score, "Score Display");
            m_World->AddComponent<ECS::TransformComponent>(score);
            auto& tc = m_World->AddComponent<ECS::TextComponent>(score);
            tc.text = "Collected: 0/3";
            tc.fontSize = 28.0f;
            tc.textColor = Math::Vector3(1.0f, 1.0f, 1.0f);
            m_World->AddComponent<ECS::TagComponent>(score).tags.push_back("score");
        }

        {
            ECS::Entity hint = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(hint, "Save System Guide");
            m_World->AddComponent<ECS::TransformComponent>(hint);
            auto& notes = m_World->AddComponent<ECS::NotesComponent>(hint);
            notes.notes = "3-tier save system demo:\n"
                "- RunState: Collectibles reset each run\n"
                "- SceneState: Checkpoint persists per scene\n"
                "- MetaProgression: Stats survive across all runs\n"
                "Danger Zone deals damage — checkpoint saves progress.\n"
                "Score Display tracks collection progress.";
        }

        {
            Renderer::SkyboxConfig skyConfig;
            skyConfig.type = Renderer::SkyboxType::Procedural;
            skyConfig.topColor = Math::Vector3(0.1f, 0.3f, 0.8f);
            skyConfig.horizonColor = Math::Vector3(0.5f, 0.7f, 1.0f);
            skyConfig.bottomColor = Math::Vector3(0.8f, 0.85f, 0.9f);
            skyConfig.sunDirection = Math::Vector3(0.0f, 1.0f, 0.0f);
            m_RenderSystem->SetSkybox(skyConfig);
        }
        m_RenderSystem->SetShadowsEnabled(true);
        m_RenderSystem->SetAmbientIntensity(0.15f);
        if (m_PostProcessing) {
            auto& pp = m_PostProcessing->GetSettings();
            pp.fxaaEnabled = 1;
        }

    } else if (templateId == "visualscript") {
        createGround();
        ECS::Entity player = createPlayer3D("Player");
        auto& ctrl = m_World->AddComponent<ECS::TopDown3DController>(player);
        ctrl.moveSpeed = 5.0f;
        ctrl.cameraAngle = 45.0f;
        ctrl.cameraDistance = 12.0f;
        SetupCameraForController(player, "TopDown3D");

        // 3 interactive entities with visual scripts
        {
            const Math::Vector3 vsPos[] = { {3,0.5f,0}, {-3,0.5f,3}, {0,0.5f,-4} };
            const Math::Vector3 vsCol[] = { {0.9f,0.6f,0.1f}, {0.1f,0.7f,0.5f}, {0.6f,0.2f,0.8f} };
            const char* vsNames[] = { "Button", "Trigger", "Counter" };
            for (int i = 0; i < 3; ++i) {
                ECS::Entity vs = m_World->CreateEntity();
                m_World->AddComponent<ECS::NameComponent>(vs, vsNames[i]);
                auto& vt = m_World->AddComponent<ECS::TransformComponent>(vs);
                vt.position = vsPos[i];
                auto& vmat = m_World->AddComponent<ECS::MaterialComponent>(vs);
                vmat.baseColor = vsCol[i];
                m_World->AddComponent<ECS::MeshComponent>(vs, Renderer::MeshFactory::CreateCube(1.0f));
                m_World->AddComponent<ECS::VisualScriptComponent>(vs);
                auto& bcol = m_World->AddComponent<ECS::BoxColliderComponent>(vs);
                bcol.size = Math::Vector3(1, 1, 1);
            }
        }

        // Switch
        {
            ECS::Entity sw = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(sw, "Switch");
            auto& st = m_World->AddComponent<ECS::TransformComponent>(sw);
            st.position = Math::Vector3(5.0f, 0.3f, 2.0f);
            st.scale = Math::Vector3(1.0f, 0.5f, 1.0f);
            auto& smat = m_World->AddComponent<ECS::MaterialComponent>(sw);
            smat.baseColor = Math::Vector3(0.7f, 0.15f, 0.15f);
            m_World->AddComponent<ECS::MeshComponent>(sw, Renderer::MeshFactory::CreateCube(1.0f));
            auto& swc = m_World->AddComponent<ECS::SwitchComponent>(sw);
            swc.type = ECS::SwitchComponent::SwitchType::Toggle;
            m_World->AddComponent<ECS::VisualScriptComponent>(sw);
        }

        // Door (tied to switch via visual script)
        {
            ECS::Entity door = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(door, "Door");
            auto& dt = m_World->AddComponent<ECS::TransformComponent>(door);
            dt.position = Math::Vector3(5.0f, 1.5f, -2.0f);
            dt.scale = Math::Vector3(2.0f, 3.0f, 0.3f);
            auto& dmat = m_World->AddComponent<ECS::MaterialComponent>(door);
            dmat.baseColor = Math::Vector3(0.45f, 0.3f, 0.15f);
            m_World->AddComponent<ECS::MeshComponent>(door, Renderer::MeshFactory::CreateCube(1.0f));
            auto& dcol = m_World->AddComponent<ECS::BoxColliderComponent>(door);
            dcol.size = Math::Vector3(2.0f, 3.0f, 0.3f);
            m_World->AddComponent<ECS::VisualScriptComponent>(door);
            m_World->AddComponent<ECS::InteractableComponent>(door).promptText = "Open Door";
        }

        // Moving platform (demonstrates movement via tween)
        {
            ECS::Entity plat = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(plat, "Moving Platform");
            auto& pt = m_World->AddComponent<ECS::TransformComponent>(plat);
            pt.position = Math::Vector3(-5.0f, 0.2f, 0.0f);
            pt.scale = Math::Vector3(3.0f, 0.4f, 3.0f);
            auto& pmat = m_World->AddComponent<ECS::MaterialComponent>(plat);
            pmat.baseColor = Math::Vector3(0.3f, 0.6f, 0.3f);
            m_World->AddComponent<ECS::MeshComponent>(plat, Renderer::MeshFactory::CreateCube(1.0f));
            auto& tw = m_World->AddComponent<ECS::TweenComponent>(plat);
            ECS::TweenEntry move;
            move.property = ECS::TweenProperty::Position;
            move.easing = ECS::EasingType::EaseInOutSine;
            move.mode = ECS::TweenMode::PingPong;
            move.startValue = Math::Vector3(-5.0f, 0.2f, 0.0f);
            move.endValue = Math::Vector3(-5.0f, 0.2f, 6.0f);
            move.duration = 3.0f;
            tw.tweens.push_back(move);
        }

        // Score counter entity
        {
            ECS::Entity score = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(score, "Score Counter");
            m_World->AddComponent<ECS::TransformComponent>(score);
            auto& tc = m_World->AddComponent<ECS::TextComponent>(score);
            tc.text = "Score: 0";
            tc.fontSize = 28.0f;
            tc.textColor = Math::Vector3(1.0f, 1.0f, 0.0f);
            m_World->AddComponent<ECS::TagComponent>(score).tags.push_back("score_display");
            m_World->AddComponent<ECS::VisualScriptComponent>(score);
        }

        // Particle effect
        {
            ECS::Entity fx = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(fx, "Particle Effect");
            auto& ft = m_World->AddComponent<ECS::TransformComponent>(fx);
            ft.position = Math::Vector3(0.0f, 2.0f, 0.0f);
            auto& pe = m_World->AddComponent<ECS::ParticleEmitterComponent>(fx);
            ECS::ApplyParticlePreset(pe, "Magic");
            pe.playOnAwake = false;
        }

        // Particle trigger zone
        {
            ECS::Entity zone = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(zone, "Particle Trigger Zone");
            auto& zt = m_World->AddComponent<ECS::TransformComponent>(zone);
            zt.position = Math::Vector3(0.0f, 0.0f, -4.0f);
            zt.scale = Math::Vector3(3.0f, 1.0f, 3.0f);
            auto& zmat = m_World->AddComponent<ECS::MaterialComponent>(zone);
            zmat.baseColor = Math::Vector3(0.5f, 0.2f, 0.8f);
            zmat.opacity = 0.3f;
            zmat.alphaMode = ECS::MaterialComponent::AlphaMode::Blend;
            m_World->AddComponent<ECS::MeshComponent>(zone, Renderer::MeshFactory::CreateCube(1.0f));
            auto& zcol = m_World->AddComponent<ECS::BoxColliderComponent>(zone);
            zcol.size = Math::Vector3(3.0f, 1.0f, 3.0f);
            zcol.isTrigger = true;
            m_World->AddComponent<ECS::VisualScriptComponent>(zone);
        }

        {
            ECS::Entity hint = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(hint, "Visual Script Guide");
            m_World->AddComponent<ECS::TransformComponent>(hint);
            auto& notes = m_World->AddComponent<ECS::NotesComponent>(hint);
            notes.notes = "Visual Script demo: each entity with VisualScriptComponent can be wired.\n"
                "Switch toggles the Door open/close via events.\n"
                "Moving Platform uses TweenComponent PingPong for back-and-forth motion.\n"
                "Particle Trigger Zone fires particles when player enters.\n"
                "Score Counter tracks interactions — wire OnInteract to increment.";
        }

        {
            Renderer::SkyboxConfig skyConfig;
            skyConfig.type = Renderer::SkyboxType::Procedural;
            skyConfig.topColor = Math::Vector3(0.1f, 0.3f, 0.8f);
            skyConfig.horizonColor = Math::Vector3(0.5f, 0.7f, 1.0f);
            skyConfig.bottomColor = Math::Vector3(0.8f, 0.85f, 0.9f);
            skyConfig.sunDirection = Math::Vector3(0.0f, 1.0f, 0.0f);
            m_RenderSystem->SetSkybox(skyConfig);
        }
        m_RenderSystem->SetShadowsEnabled(true);
        m_RenderSystem->SetAmbientIntensity(0.15f);
        if (m_PostProcessing) {
            auto& pp = m_PostProcessing->GetSettings();
            pp.fxaaEnabled = 1;
        }

    } else if (templateId == "uicanvas") {
        createGround();
        ECS::Entity player = createPlayer3D("Player");
        auto& ctrl = m_World->AddComponent<ECS::ThirdPersonController>(player);
        ctrl.moveSpeed = 5.0f;
        ctrl.cameraDistance = 5.0f;
        ctrl.cameraHeight = 2.0f;
        SetupCameraForController(player, "ThirdPerson");
        m_World->AddComponent<ECS::HealthComponent>(player).maxHealth = 100.0f;
        m_World->GetComponent<ECS::HealthComponent>(player)->currentHealth = 100.0f;

        // UI Canvas entity with populated HUD elements
        {
            ECS::Entity uiEntity = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(uiEntity, "Game UI");
            m_World->AddComponent<ECS::TransformComponent>(uiEntity);
            auto& canvas = m_World->AddComponent<GUI::UICanvasComponent>(uiEntity);
            canvas.canvasName = "GameHUD";
            canvas.designWidth = 1920.0f;
            canvas.designHeight = 1080.0f;

            // Health panel (top-left)
            u32 hpPanelId = canvas.AddElement(GUI::UIWidgetType::Panel, "HealthPanel");
            if (auto* hp = canvas.GetElement(hpPanelId)) {
                hp->anchor.anchorMin = Math::Vector2(0.0f, 0.0f);
                hp->anchor.anchorMax = Math::Vector2(0.0f, 0.0f);
                hp->anchor.offsetLeft = 20.0f; hp->anchor.offsetTop = 20.0f;
                hp->anchor.offsetRight = -320.0f; hp->anchor.offsetBottom = -60.0f;
                hp->style.bgColor = Math::Vector3(0.1f, 0.1f, 0.12f);
                hp->style.bgAlpha = 0.7f;
                hp->focusable = false;
            }
            // Health bar (progress bar inside panel)
            u32 hpBarId = canvas.AddElement(GUI::UIWidgetType::ProgressBar, "HealthBar", hpPanelId);
            if (auto* bar = canvas.GetElement(hpBarId)) {
                bar->anchor.anchorMin = Math::Vector2(0.05f, 0.15f);
                bar->anchor.anchorMax = Math::Vector2(0.95f, 0.85f);
                bar->data.progressValue = 1.0f;
                bar->focusable = false;
            }
            // Score label (top-right)
            u32 scoreId = canvas.AddElement(GUI::UIWidgetType::Label, "ScoreLabel");
            if (auto* sc = canvas.GetElement(scoreId)) {
                sc->anchor.anchorMin = Math::Vector2(1.0f, 0.0f);
                sc->anchor.anchorMax = Math::Vector2(1.0f, 0.0f);
                sc->anchor.offsetLeft = 200.0f; sc->anchor.offsetTop = 20.0f;
                sc->anchor.offsetRight = -20.0f; sc->anchor.offsetBottom = -60.0f;
                sc->data.text = "Score: 0";
                sc->data.textAlignH = 2; // right
                sc->style.textColor = Math::Vector3(1.0f, 1.0f, 1.0f);
                sc->focusable = false;
            }
            // Ammo label (bottom-right)
            u32 ammoId = canvas.AddElement(GUI::UIWidgetType::Label, "AmmoLabel");
            if (auto* ammo = canvas.GetElement(ammoId)) {
                ammo->anchor.anchorMin = Math::Vector2(1.0f, 1.0f);
                ammo->anchor.anchorMax = Math::Vector2(1.0f, 1.0f);
                ammo->anchor.offsetLeft = 200.0f; ammo->anchor.offsetTop = 60.0f;
                ammo->anchor.offsetRight = -20.0f; ammo->anchor.offsetBottom = -20.0f;
                ammo->data.text = "Ammo: 30";
                ammo->data.textAlignH = 2;
                ammo->style.textColor = Math::Vector3(0.8f, 0.8f, 1.0f);
                ammo->focusable = false;
            }
            // Pause button (top-right corner)
            u32 pauseId = canvas.AddElement(GUI::UIWidgetType::Button, "PauseButton");
            if (auto* pause = canvas.GetElement(pauseId)) {
                pause->anchor.anchorMin = Math::Vector2(1.0f, 0.0f);
                pause->anchor.anchorMax = Math::Vector2(1.0f, 0.0f);
                pause->anchor.offsetLeft = 80.0f; pause->anchor.offsetTop = 20.0f;
                pause->anchor.offsetRight = -20.0f; pause->anchor.offsetBottom = -60.0f;
                pause->data.text = "||";
                pause->tabOrder = 1;
            }
        }

        // HUD widget
        {
            ECS::Entity hud = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(hud, "Health Bar Overlay");
            m_World->AddComponent<ECS::TransformComponent>(hud);
            auto& hw = m_World->AddComponent<ECS::HUDWidgetComponent>(hud);
            hw.type = ECS::HUDWidgetComponent::WidgetType::HealthBar;
            hw.visible = true;
            hw.screenSpace = true;
            hw.anchorX = 0.05f;
            hw.anchorY = 0.05f;
            hw.fillColor = Math::Vector3(0.8f, 0.2f, 0.2f);
            hw.bgColor = Math::Vector3(0.2f, 0.2f, 0.2f);
            hw.sourceEntity = player;  // Link to player's HealthComponent for live updates
        }

        // Collectibles
        {
            const Math::Vector3 cPos[] = { {4,0.5f,2}, {-3,0.5f,-4}, {6,0.5f,-1} };
            for (int i = 0; i < 3; ++i) {
                ECS::Entity coin = m_World->CreateEntity();
                m_World->AddComponent<ECS::NameComponent>(coin, "Coin " + std::to_string(i + 1));
                auto& ct = m_World->AddComponent<ECS::TransformComponent>(coin);
                ct.position = cPos[i];
                ct.scale = Math::Vector3(0.5f, 0.5f, 0.5f);
                auto& cmat = m_World->AddComponent<ECS::MaterialComponent>(coin);
                cmat.baseColor = Math::Vector3(1.0f, 0.85f, 0.0f);
                cmat.emissiveColor = Math::Vector3(1.0f, 0.85f, 0.0f);
                cmat.emissiveStrength = 0.5f;
                m_World->AddComponent<ECS::MeshComponent>(coin, Renderer::MeshFactory::CreateSphere(0.3f));
                auto& pk = m_World->AddComponent<ECS::PickupComponent>(coin);
                pk.type = ECS::PickupComponent::PickupType::Coin;
                pk.customId = "Coin";
                pk.value = 10.0f;
                auto& tw = m_World->AddComponent<ECS::TweenComponent>(coin);
                ECS::TweenEntry bob;
                bob.property = ECS::TweenProperty::Position;
                bob.easing = ECS::EasingType::EaseInOutSine;
                bob.mode = ECS::TweenMode::PingPong;
                bob.startValue = cPos[i];
                bob.endValue = cPos[i] + Math::Vector3(0, 0.5f, 0);
                bob.duration = 1.5f;
                tw.tweens.push_back(bob);
            }
        }

        // Enemy with health (to test HUD updates)
        {
            ECS::Entity enemy = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(enemy, "Training Dummy");
            auto& et = m_World->AddComponent<ECS::TransformComponent>(enemy);
            et.position = Math::Vector3(-5.0f, 0.75f, 3.0f);
            auto& emat = m_World->AddComponent<ECS::MaterialComponent>(enemy);
            emat.baseColor = Math::Vector3(0.8f, 0.2f, 0.2f);
            m_World->AddComponent<ECS::MeshComponent>(enemy, Renderer::MeshFactory::CreateCapsule(0.4f, 1.5f));
            auto& hp = m_World->AddComponent<ECS::HealthComponent>(enemy);
            hp.maxHealth = 50.0f;
            hp.currentHealth = 50.0f;
            auto& bcol = m_World->AddComponent<ECS::BoxColliderComponent>(enemy);
            bcol.size = Math::Vector3(0.8f, 1.5f, 0.8f);
        }

        // Point light for atmosphere
        {
            ECS::Entity light = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(light, "Scene Light");
            auto& lt = m_World->AddComponent<ECS::TransformComponent>(light);
            lt.position = Math::Vector3(0.0f, 5.0f, 0.0f);
            auto& lc = m_World->AddComponent<ECS::LightComponent>(light);
            lc.type = ECS::LightType::Point;
            lc.color = Math::Vector3(1.0f, 0.95f, 0.85f);
            lc.intensity = 2.5f;
            lc.range = 20.0f;
        }

        {
            ECS::Entity hint = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(hint, "UI Guide");
            m_World->AddComponent<ECS::TransformComponent>(hint);
            auto& notes = m_World->AddComponent<ECS::NotesComponent>(hint);
            notes.notes = "UI Canvas demo with pre-built HUD elements:\n"
                "- HealthBar (ProgressBar widget) tracks player health\n"
                "- ScoreLabel updates when coins are collected\n"
                "- AmmoLabel shows current ammo count\n"
                "- PauseButton demonstrates interactive UI\n"
                "Select Game UI and open UI Editor to customize layout.\n"
                "HUDWidgetComponent provides a simpler health bar overlay.";
        }

        {
            Renderer::SkyboxConfig skyConfig;
            skyConfig.type = Renderer::SkyboxType::Procedural;
            skyConfig.topColor = Math::Vector3(0.1f, 0.3f, 0.8f);
            skyConfig.horizonColor = Math::Vector3(0.5f, 0.7f, 1.0f);
            skyConfig.bottomColor = Math::Vector3(0.8f, 0.85f, 0.9f);
            skyConfig.sunDirection = Math::Vector3(0.0f, 1.0f, 0.0f);
            m_RenderSystem->SetSkybox(skyConfig);
        }
        m_RenderSystem->SetShadowsEnabled(true);
        m_RenderSystem->SetAmbientIntensity(0.15f);
        if (m_PostProcessing) {
            auto& pp = m_PostProcessing->GetSettings();
            pp.fxaaEnabled = 1;
        }

    } else if (templateId == "accessibility") {
        createGround();

        // Directional light
        {
            ECS::Entity light = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(light, "Directional Light");
            auto& lt = m_World->AddComponent<ECS::TransformComponent>(light);
            lt.position = Math::Vector3(0.0f, 10.0f, 0.0f);
            lt.rotation = Math::Quaternion(Math::Vector3(1, 0, 0), Math::Radians(-45.0f));
            auto& lc = m_World->AddComponent<ECS::LightComponent>(light);
            lc.type = ECS::LightType::Directional;
            lc.intensity = 1.0f;
        }

        // In-game camera
        {
            ECS::Entity cam = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(cam, "Camera");
            auto& ct = m_World->AddComponent<ECS::TransformComponent>(cam);
            ct.position = Math::Vector3(0.0f, 2.0f, 5.0f);
            auto& cc = m_World->AddComponent<ECS::CameraComponent>(cam);
            cc.projectionType = ECS::ProjectionType::Perspective;
            cc.fieldOfView = 60.0f;
            cc.isActive = true;
            cc.priority = 10;
            m_SelectedGameCamera = cam;
        }

        // Accessibility Settings UICanvas
        {
            ECS::Entity uiEntity = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(uiEntity, "Accessibility Menu");
            m_World->AddComponent<ECS::TransformComponent>(uiEntity);
            auto& canvas = m_World->AddComponent<GUI::UICanvasComponent>(uiEntity);
            canvas.canvasName = "AccessibilitySettings";
            canvas.designWidth = 1920.0f;
            canvas.designHeight = 1080.0f;

            // Root panel (dark semi-transparent background, centered)
            u32 panelId = canvas.AddElement(GUI::UIWidgetType::Panel, "Settings Panel");
            if (auto* panel = canvas.GetElement(panelId)) {
                panel->anchor.anchorMin = Math::Vector2(0.25f, 0.1f);
                panel->anchor.anchorMax = Math::Vector2(0.75f, 0.9f);
                panel->anchor.offsetLeft = 0; panel->anchor.offsetRight = 0;
                panel->anchor.offsetTop = 0; panel->anchor.offsetBottom = 0;
                panel->style.bgColor = Math::Vector3(0.12f, 0.13f, 0.16f);
                panel->style.bgAlpha = 0.95f;
                panel->style.borderRadius = 12.0f;
                panel->style.borderWidth = 1.0f;
                panel->style.borderColor = Math::Vector3(0.3f, 0.35f, 0.45f);
                panel->focusable = false;
            }

            // Title label
            u32 titleId = canvas.AddElement(GUI::UIWidgetType::Label, "Title", panelId);
            if (auto* title = canvas.GetElement(titleId)) {
                title->anchor.anchorMin = Math::Vector2(0.0f, 0.0f);
                title->anchor.anchorMax = Math::Vector2(1.0f, 0.0f);
                title->anchor.offsetLeft = 0; title->anchor.offsetRight = 0;
                title->anchor.offsetTop = 20.0f; title->anchor.offsetBottom = -60.0f;
                title->data.text = "Accessibility Settings";
                title->data.textAlignH = 1; // center
                title->style.fontSize = 28.0f;
                title->style.textColor = Math::Vector3(0.9f, 0.92f, 0.96f);
                title->focusable = false;
            }

            // --- Subtitle Toggle ---
            u32 subLabelId = canvas.AddElement(GUI::UIWidgetType::Label, "Subtitle Label", panelId);
            if (auto* lbl = canvas.GetElement(subLabelId)) {
                lbl->anchor.anchorMin = Math::Vector2(0.05f, 0.0f);
                lbl->anchor.anchorMax = Math::Vector2(0.5f, 0.0f);
                lbl->anchor.offsetTop = 90.0f; lbl->anchor.offsetBottom = -120.0f;
                lbl->data.text = "Subtitles";
                lbl->data.textAlignH = 0; // left
                lbl->style.textColor = Math::Vector3(0.8f, 0.82f, 0.88f);
                lbl->focusable = false;
            }
            u32 subToggleId = canvas.AddElement(GUI::UIWidgetType::Toggle, "Subtitle Toggle", panelId);
            if (auto* tog = canvas.GetElement(subToggleId)) {
                tog->anchor.anchorMin = Math::Vector2(0.7f, 0.0f);
                tog->anchor.anchorMax = Math::Vector2(0.95f, 0.0f);
                tog->anchor.offsetTop = 90.0f; tog->anchor.offsetBottom = -120.0f;
                tog->data.checked = true;
                tog->tabOrder = 1;
                tog->onValueChangedEvent = "accessibility_subtitles";
            }

            // --- Subtitle Size Slider ---
            u32 sizeLabelId = canvas.AddElement(GUI::UIWidgetType::Label, "Size Label", panelId);
            if (auto* lbl = canvas.GetElement(sizeLabelId)) {
                lbl->anchor.anchorMin = Math::Vector2(0.05f, 0.0f);
                lbl->anchor.anchorMax = Math::Vector2(0.5f, 0.0f);
                lbl->anchor.offsetTop = 140.0f; lbl->anchor.offsetBottom = -170.0f;
                lbl->data.text = "Subtitle Size";
                lbl->data.textAlignH = 0;
                lbl->style.textColor = Math::Vector3(0.8f, 0.82f, 0.88f);
                lbl->focusable = false;
            }
            u32 sizeSliderId = canvas.AddElement(GUI::UIWidgetType::Slider, "Size Slider", panelId);
            if (auto* sl = canvas.GetElement(sizeSliderId)) {
                sl->anchor.anchorMin = Math::Vector2(0.5f, 0.0f);
                sl->anchor.anchorMax = Math::Vector2(0.95f, 0.0f);
                sl->anchor.offsetTop = 140.0f; sl->anchor.offsetBottom = -170.0f;
                sl->data.sliderMin = 0.5f;
                sl->data.sliderMax = 2.0f;
                sl->data.sliderValue = 1.0f;
                sl->tabOrder = 2;
                sl->onValueChangedEvent = "accessibility_subtitle_size";
            }

            // --- Colorblind Mode (Checkbox for now, dropdown in Phase 2) ---
            u32 cbLabelId = canvas.AddElement(GUI::UIWidgetType::Label, "Colorblind Label", panelId);
            if (auto* lbl = canvas.GetElement(cbLabelId)) {
                lbl->anchor.anchorMin = Math::Vector2(0.05f, 0.0f);
                lbl->anchor.anchorMax = Math::Vector2(0.5f, 0.0f);
                lbl->anchor.offsetTop = 200.0f; lbl->anchor.offsetBottom = -230.0f;
                lbl->data.text = "Colorblind Filter";
                lbl->data.textAlignH = 0;
                lbl->style.textColor = Math::Vector3(0.8f, 0.82f, 0.88f);
                lbl->focusable = false;
            }
            u32 cbToggleId = canvas.AddElement(GUI::UIWidgetType::Toggle, "Colorblind Toggle", panelId);
            if (auto* tog = canvas.GetElement(cbToggleId)) {
                tog->anchor.anchorMin = Math::Vector2(0.7f, 0.0f);
                tog->anchor.anchorMax = Math::Vector2(0.95f, 0.0f);
                tog->anchor.offsetTop = 200.0f; tog->anchor.offsetBottom = -230.0f;
                tog->data.checked = false;
                tog->tabOrder = 3;
                tog->onValueChangedEvent = "accessibility_colorblind";
            }

            // --- Reduced Motion Toggle ---
            u32 motionLabelId = canvas.AddElement(GUI::UIWidgetType::Label, "Motion Label", panelId);
            if (auto* lbl = canvas.GetElement(motionLabelId)) {
                lbl->anchor.anchorMin = Math::Vector2(0.05f, 0.0f);
                lbl->anchor.anchorMax = Math::Vector2(0.5f, 0.0f);
                lbl->anchor.offsetTop = 260.0f; lbl->anchor.offsetBottom = -290.0f;
                lbl->data.text = "Reduced Motion";
                lbl->data.textAlignH = 0;
                lbl->style.textColor = Math::Vector3(0.8f, 0.82f, 0.88f);
                lbl->focusable = false;
            }
            u32 motionToggleId = canvas.AddElement(GUI::UIWidgetType::Toggle, "Motion Toggle", panelId);
            if (auto* tog = canvas.GetElement(motionToggleId)) {
                tog->anchor.anchorMin = Math::Vector2(0.7f, 0.0f);
                tog->anchor.anchorMax = Math::Vector2(0.95f, 0.0f);
                tog->anchor.offsetTop = 260.0f; tog->anchor.offsetBottom = -290.0f;
                tog->data.checked = false;
                tog->tabOrder = 4;
                tog->onValueChangedEvent = "accessibility_reduced_motion";
            }

            // --- Input Sensitivity Slider ---
            u32 sensLabelId = canvas.AddElement(GUI::UIWidgetType::Label, "Sensitivity Label", panelId);
            if (auto* lbl = canvas.GetElement(sensLabelId)) {
                lbl->anchor.anchorMin = Math::Vector2(0.05f, 0.0f);
                lbl->anchor.anchorMax = Math::Vector2(0.5f, 0.0f);
                lbl->anchor.offsetTop = 320.0f; lbl->anchor.offsetBottom = -350.0f;
                lbl->data.text = "Input Sensitivity";
                lbl->data.textAlignH = 0;
                lbl->style.textColor = Math::Vector3(0.8f, 0.82f, 0.88f);
                lbl->focusable = false;
            }
            u32 sensSliderId = canvas.AddElement(GUI::UIWidgetType::Slider, "Sensitivity Slider", panelId);
            if (auto* sl = canvas.GetElement(sensSliderId)) {
                sl->anchor.anchorMin = Math::Vector2(0.5f, 0.0f);
                sl->anchor.anchorMax = Math::Vector2(0.95f, 0.0f);
                sl->anchor.offsetTop = 320.0f; sl->anchor.offsetBottom = -350.0f;
                sl->data.sliderMin = 0.1f;
                sl->data.sliderMax = 3.0f;
                sl->data.sliderValue = 1.0f;
                sl->tabOrder = 5;
                sl->onValueChangedEvent = "accessibility_sensitivity";
            }

            // --- Font Scale Slider ---
            u32 fontLabelId = canvas.AddElement(GUI::UIWidgetType::Label, "Font Scale Label", panelId);
            if (auto* lbl = canvas.GetElement(fontLabelId)) {
                lbl->anchor.anchorMin = Math::Vector2(0.05f, 0.0f);
                lbl->anchor.anchorMax = Math::Vector2(0.5f, 0.0f);
                lbl->anchor.offsetTop = 380.0f; lbl->anchor.offsetBottom = -410.0f;
                lbl->data.text = "Font Scale";
                lbl->data.textAlignH = 0;
                lbl->style.textColor = Math::Vector3(0.8f, 0.82f, 0.88f);
                lbl->focusable = false;
            }
            u32 fontSliderId = canvas.AddElement(GUI::UIWidgetType::Slider, "Font Scale Slider", panelId);
            if (auto* sl = canvas.GetElement(fontSliderId)) {
                sl->anchor.anchorMin = Math::Vector2(0.5f, 0.0f);
                sl->anchor.anchorMax = Math::Vector2(0.95f, 0.0f);
                sl->anchor.offsetTop = 380.0f; sl->anchor.offsetBottom = -410.0f;
                sl->data.sliderMin = 0.75f;
                sl->data.sliderMax = 2.5f;
                sl->data.sliderValue = 1.0f;
                sl->tabOrder = 6;
                sl->onValueChangedEvent = "accessibility_font_scale";
            }

            // --- Apply Button ---
            u32 applyBtnId = canvas.AddElement(GUI::UIWidgetType::Button, "Apply Button", panelId);
            if (auto* btn = canvas.GetElement(applyBtnId)) {
                btn->anchor.anchorMin = Math::Vector2(0.3f, 0.0f);
                btn->anchor.anchorMax = Math::Vector2(0.7f, 0.0f);
                btn->anchor.offsetTop = 440.0f; btn->anchor.offsetBottom = -485.0f;
                btn->data.text = "Apply Settings";
                btn->tabOrder = 7;
                btn->onClickEvent = "accessibility_apply";
                btn->style.bgColor = Math::Vector3(0.2f, 0.5f, 0.8f);
                btn->style.bgAlpha = 1.0f;
                btn->style.borderRadius = 6.0f;
            }

            // Set theme for good focus visibility
            canvas.theme.focusBorderWidth = 3.0f;
            canvas.theme.inputFocused = Math::Vector3(0.3f, 0.7f, 1.0f);
        }

        // Screen reader label examples
        {
            ECS::Entity exampleObj = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(exampleObj, "Interactive Object");
            auto& ot = m_World->AddComponent<ECS::TransformComponent>(exampleObj);
            ot.position = Math::Vector3(3.0f, 0.5f, 0.0f);
            auto& omat = m_World->AddComponent<ECS::MaterialComponent>(exampleObj);
            omat.baseColor = Math::Vector3(0.4f, 0.6f, 0.8f);
            m_World->AddComponent<ECS::MeshComponent>(exampleObj, Renderer::MeshFactory::CreateCube(1.0f));
            auto& interact = m_World->AddComponent<ECS::InteractableComponent>(exampleObj);
            interact.promptText = "Press E to interact";
            auto& bcol = m_World->AddComponent<ECS::BoxColliderComponent>(exampleObj);
            bcol.size = Math::Vector3(1, 1, 1);

            ECS::Entity exampleObj2 = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(exampleObj2, "Labeled Button");
            auto& o2t = m_World->AddComponent<ECS::TransformComponent>(exampleObj2);
            o2t.position = Math::Vector3(-3.0f, 0.5f, 0.0f);
            auto& o2mat = m_World->AddComponent<ECS::MaterialComponent>(exampleObj2);
            o2mat.baseColor = Math::Vector3(0.8f, 0.4f, 0.3f);
            m_World->AddComponent<ECS::MeshComponent>(exampleObj2, Renderer::MeshFactory::CreateSphere(0.5f));
            auto& interact2 = m_World->AddComponent<ECS::InteractableComponent>(exampleObj2);
            interact2.promptText = "Activate button";
        }

        // Guide notes
        {
            ECS::Entity hint = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(hint, "Accessibility Guide");
            m_World->AddComponent<ECS::TransformComponent>(hint);
            auto& notes = m_World->AddComponent<ECS::NotesComponent>(hint);
            notes.notes = "Accessibility Settings Menu template.\n"
                "Navigate with Tab/Shift+Tab or DPad/Arrow keys.\n"
                "Enter/Space activates buttons and toggles.\n"
                "Left/Right adjusts slider values when focused.\n"
                "Font Scale slider controls text size via accessibility_font_scale event.\n"
                "Interactive objects demonstrate screen reader accessible labels.\n"
                "Each widget fires events via onValueChangedEvent for scripting.";
        }

        {
            Renderer::SkyboxConfig skyConfig;
            skyConfig.type = Renderer::SkyboxType::Procedural;
            skyConfig.topColor = Math::Vector3(0.1f, 0.3f, 0.8f);
            skyConfig.horizonColor = Math::Vector3(0.5f, 0.7f, 1.0f);
            skyConfig.bottomColor = Math::Vector3(0.8f, 0.85f, 0.9f);
            skyConfig.sunDirection = Math::Vector3(0.0f, 1.0f, 0.0f);
            m_RenderSystem->SetSkybox(skyConfig);
        }
        m_RenderSystem->SetShadowsEnabled(true);
        m_RenderSystem->SetAmbientIntensity(0.15f);
        if (m_PostProcessing) {
            auto& pp = m_PostProcessing->GetSettings();
            pp.fxaaEnabled = 1;
        }

    } else if (templateId == "pointclick") {
        // Background
        {
            ECS::Entity bg = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(bg, "Background");
            auto& bt = m_World->AddComponent<ECS::TransformComponent>(bg);
            bt.position = Math::Vector3(0.0f, 0.0f, -1.0f);
            bt.scale = Math::Vector3(16.0f, 9.0f, 1.0f);
            auto& bmat = m_World->AddComponent<ECS::MaterialComponent>(bg);
            bmat.baseColor = Math::Vector3(0.4f, 0.5f, 0.6f);
            m_World->AddComponent<ECS::MeshComponent>(bg, Renderer::MeshFactory::CreateQuad(1.0f, 1.0f));
            m_World->AddComponent<ECS::Sprite2DComponent>(bg);
        }

        // Orthographic camera
        {
            ECS::Entity cam = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(cam, "Camera");
            auto& ct = m_World->AddComponent<ECS::TransformComponent>(cam);
            ct.position = Math::Vector3(0.0f, 0.0f, 10.0f);
            auto& cc = m_World->AddComponent<ECS::CameraComponent>(cam);
            cc.projectionType = ECS::ProjectionType::Orthographic;
            cc.orthoSize = 5.4f;
            cc.isActive = true;
            cc.priority = 10;
            m_SelectedGameCamera = cam;
        }

        // 3 hotspot entities
        {
            const Math::Vector3 hsPos[] = { {-4,1,0}, {2,-1,0}, {5,2,0} };
            const char* hsNames[] = { "Door", "Desk", "Window" };
            const char* hsDesc[] = { "A heavy wooden door. It's locked.", "Papers are scattered across the desk.", "The window overlooks a garden." };
            for (int i = 0; i < 3; ++i) {
                ECS::Entity hs = m_World->CreateEntity();
                m_World->AddComponent<ECS::NameComponent>(hs, hsNames[i]);
                auto& ht = m_World->AddComponent<ECS::TransformComponent>(hs);
                ht.position = hsPos[i];
                ht.scale = Math::Vector3(1.5f, 1.5f, 0.1f);
                auto& hmat = m_World->AddComponent<ECS::MaterialComponent>(hs);
                hmat.baseColor = Math::Vector3(0.6f + i * 0.1f, 0.4f, 0.3f);
                m_World->AddComponent<ECS::MeshComponent>(hs, Renderer::MeshFactory::CreateQuad(1.0f, 1.0f));
                m_World->AddComponent<ECS::Sprite2DComponent>(hs);
                auto& interact = m_World->AddComponent<ECS::InteractableComponent>(hs);
                interact.promptText = std::string("Examine ") + hsNames[i];
                auto& dlg = m_World->AddComponent<ECS::DialogueComponent>(hs);
                dlg.dialogueLines = { hsDesc[i] };
            }
        }

        // Inventory key item
        {
            ECS::Entity key = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(key, "Key");
            auto& kt = m_World->AddComponent<ECS::TransformComponent>(key);
            kt.position = Math::Vector3(2.0f, -1.5f, 0.1f);
            kt.scale = Math::Vector3(0.5f, 0.5f, 0.1f);
            auto& kmat = m_World->AddComponent<ECS::MaterialComponent>(key);
            kmat.baseColor = Math::Vector3(1.0f, 0.85f, 0.0f);
            kmat.emissiveColor = Math::Vector3(1.0f, 0.85f, 0.0f);
            kmat.emissiveStrength = 0.5f;
            m_World->AddComponent<ECS::MeshComponent>(key, Renderer::MeshFactory::CreateQuad(1.0f, 1.0f));
            m_World->AddComponent<ECS::Sprite2DComponent>(key);
            auto& pk = m_World->AddComponent<ECS::PickupComponent>(key);
            pk.type = ECS::PickupComponent::PickupType::Key;
            pk.customId = "Brass Key";
            pk.value = 1.0f;
            m_World->AddComponent<ECS::InteractableComponent>(key).promptText = "Pick up Key";
            auto& tw = m_World->AddComponent<ECS::TweenComponent>(key);
            ECS::TweenEntry glow;
            glow.property = ECS::TweenProperty::Scale;
            glow.easing = ECS::EasingType::EaseInOutSine;
            glow.mode = ECS::TweenMode::PingPong;
            glow.startValue = Math::Vector3(0.45f, 0.45f, 0.1f);
            glow.endValue = Math::Vector3(0.55f, 0.55f, 0.1f);
            glow.duration = 1.2f;
            tw.tweens.push_back(glow);
        }

        // Locked cabinet (requires key)
        {
            ECS::Entity cabinet = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(cabinet, "Locked Cabinet");
            auto& ct = m_World->AddComponent<ECS::TransformComponent>(cabinet);
            ct.position = Math::Vector3(-5.0f, 0.0f, 0.0f);
            ct.scale = Math::Vector3(2.0f, 2.5f, 0.1f);
            auto& cmat = m_World->AddComponent<ECS::MaterialComponent>(cabinet);
            cmat.baseColor = Math::Vector3(0.35f, 0.25f, 0.15f);
            m_World->AddComponent<ECS::MeshComponent>(cabinet, Renderer::MeshFactory::CreateQuad(1.0f, 1.0f));
            m_World->AddComponent<ECS::Sprite2DComponent>(cabinet);
            auto& ci = m_World->AddComponent<ECS::InteractableComponent>(cabinet);
            ci.promptText = "Examine Cabinet";
            auto& cd = m_World->AddComponent<ECS::DialogueComponent>(cabinet);
            cd.dialogueLines = { "The cabinet is locked. You need a key." };
        }

        // Dialogue box entity
        {
            ECS::Entity dlgBox = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(dlgBox, "Dialogue Box");
            auto& dt = m_World->AddComponent<ECS::TransformComponent>(dlgBox);
            dt.position = Math::Vector3(0.0f, -3.5f, 1.0f);
            auto& tc = m_World->AddComponent<ECS::TextComponent>(dlgBox);
            tc.text = "Click on objects to examine them.";
            tc.fontSize = 24.0f;
            tc.textColor = Math::Vector3(1.0f, 1.0f, 1.0f);
            m_World->AddComponent<ECS::DialogueBoxComponent>(dlgBox);
        }

        // Ambient point light
        {
            ECS::Entity lamp = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(lamp, "Room Light");
            auto& lt = m_World->AddComponent<ECS::TransformComponent>(lamp);
            lt.position = Math::Vector3(0.0f, 4.0f, 2.0f);
            auto& lc = m_World->AddComponent<ECS::LightComponent>(lamp);
            lc.type = ECS::LightType::Point;
            lc.color = Math::Vector3(1.0f, 0.9f, 0.75f);
            lc.intensity = 1.5f;
            lc.range = 12.0f;
        }

        // Cursor indicator
        {
            ECS::Entity cursor = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(cursor, "Cursor Indicator");
            auto& ct = m_World->AddComponent<ECS::TransformComponent>(cursor);
            ct.position = Math::Vector3(0.0f, 0.0f, 2.0f);
            ct.scale = Math::Vector3(0.3f, 0.3f, 0.1f);
            auto& cmat = m_World->AddComponent<ECS::MaterialComponent>(cursor);
            cmat.baseColor = Math::Vector3(1.0f, 1.0f, 1.0f);
            m_World->AddComponent<ECS::MeshComponent>(cursor, Renderer::MeshFactory::CreateQuad(1.0f, 1.0f));
            m_World->AddComponent<ECS::Sprite2DComponent>(cursor);
            m_World->AddComponent<ECS::TagComponent>(cursor).tags.push_back("cursor");
        }

        // Inventory UI
        {
            ECS::Entity inv = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(inv, "Inventory");
            m_World->AddComponent<ECS::TransformComponent>(inv);
            auto& canvas = m_World->AddComponent<GUI::UICanvasComponent>(inv);
            canvas.canvasName = "Inventory";
            canvas.designWidth = 1920.0f;
            canvas.designHeight = 1080.0f;

            u32 invPanelId = canvas.AddElement(GUI::UIWidgetType::Panel, "InventoryPanel");
            if (auto* panel = canvas.GetElement(invPanelId)) {
                panel->anchor.anchorMin = Math::Vector2(0.0f, 1.0f);
                panel->anchor.anchorMax = Math::Vector2(0.0f, 1.0f);
                panel->anchor.offsetLeft = 20.0f; panel->anchor.offsetTop = 80.0f;
                panel->anchor.offsetRight = -420.0f; panel->anchor.offsetBottom = -20.0f;
                panel->style.bgColor = Math::Vector3(0.1f, 0.1f, 0.12f);
                panel->style.bgAlpha = 0.8f;
                panel->focusable = false;
            }
            u32 invLabelId = canvas.AddElement(GUI::UIWidgetType::Label, "InventoryLabel", invPanelId);
            if (auto* lbl = canvas.GetElement(invLabelId)) {
                lbl->anchor.anchorMin = Math::Vector2(0.05f, 0.1f);
                lbl->anchor.anchorMax = Math::Vector2(0.95f, 0.9f);
                lbl->data.text = "Inventory: (empty)";
                lbl->data.textAlignH = 0; // left
                lbl->style.textColor = Math::Vector3(0.9f, 0.9f, 0.8f);
                lbl->focusable = false;
            }
        }

        {
            ECS::Entity hint = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(hint, "Adventure Guide");
            m_World->AddComponent<ECS::TransformComponent>(hint);
            auto& notes = m_World->AddComponent<ECS::NotesComponent>(hint);
            notes.notes = "Point & click adventure template:\n"
                "- 3 Hotspots: Door, Desk, Window — each with dialogue\n"
                "- Key item (glowing) can be picked up via PickupComponent\n"
                "- Locked Cabinet requires the key to open\n"
                "- Dialogue Box shows examine text at screen bottom\n"
                "- Cursor Indicator follows mouse (wire via script)\n"
                "- Inventory UI panel tracks collected items.";
        }

        m_RenderSystem->SetShadowsEnabled(false);
        m_RenderSystem->SetAmbientIntensity(0.3f);
        if (m_PostProcessing) {
            auto& pp = m_PostProcessing->GetSettings();
            pp.vignetteEnabled = 1;
            pp.vignetteIntensity = 0.2f;
        }

    } else if (templateId == "bullethell") {
        // Player
        ECS::Entity player = createPlayer2D("Player");
        auto& ctrl = m_World->AddComponent<ECS::TopDown2DController>(player);
        ctrl.moveSpeed = 8.0f;
        m_World->AddComponent<ECS::HealthComponent>(player);
        SetupCameraForController(player, "TopDown2D");

        // Enemy spawner
        {
            ECS::Entity spawner = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(spawner, "Enemy Spawner");
            auto& st = m_World->AddComponent<ECS::TransformComponent>(spawner);
            st.position = Math::Vector3(0.0f, 8.0f, 0.0f);
            auto& smat = m_World->AddComponent<ECS::MaterialComponent>(spawner);
            smat.baseColor = Math::Vector3(0.9f, 0.2f, 0.2f);
            m_World->AddComponent<ECS::MeshComponent>(spawner, Renderer::MeshFactory::CreateCapsule2D(1.0f, 1.5f));
            m_World->AddComponent<ECS::Sprite2DComponent>(spawner);
            auto& pe = m_World->AddComponent<ECS::ParticleEmitterComponent>(spawner);
            ECS::ApplyParticlePreset(pe, "Sparks");
            pe.emissionRate = 20.0f;
        }

        // Bullet pool marker
        {
            ECS::Entity pool = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(pool, "Bullet Pool");
            m_World->AddComponent<ECS::TransformComponent>(pool);
            auto& pc = m_World->AddComponent<ECS::PoolableComponent>(pool);
            pc.poolId = "bullets";
            pc.lifetime = 5.0f;
        }

        // 3 enemy types
        {
            const Math::Vector3 ePos[] = { {-4,6,0}, {4,7,0}, {0,8.5f,0} };
            const Math::Vector3 eCol[] = { {0.9f,0.1f,0.1f}, {0.1f,0.1f,0.9f}, {0.7f,0.1f,0.7f} };
            const char* eNames[] = { "Chaser Enemy", "Circle Enemy", "Spread Enemy" };
            const f32 eSpeeds[] = { 3.0f, 2.0f, 1.5f };
            for (int i = 0; i < 3; ++i) {
                ECS::Entity enemy = m_World->CreateEntity();
                m_World->AddComponent<ECS::NameComponent>(enemy, eNames[i]);
                auto& et = m_World->AddComponent<ECS::TransformComponent>(enemy);
                et.position = ePos[i];
                et.scale = Math::Vector3(1.2f, 1.2f, 1.0f);
                auto& emat = m_World->AddComponent<ECS::MaterialComponent>(enemy);
                emat.baseColor = eCol[i];
                emat.emissiveColor = eCol[i];
                emat.emissiveStrength = 0.8f;
                m_World->AddComponent<ECS::MeshComponent>(enemy, Renderer::MeshFactory::CreateCapsule2D(0.5f, 1.0f));
                m_World->AddComponent<ECS::Sprite2DComponent>(enemy);
                auto& hp = m_World->AddComponent<ECS::HealthComponent>(enemy);
                hp.maxHealth = 30.0f + i * 20.0f;
                hp.currentHealth = hp.maxHealth;
                auto& ai = m_World->AddComponent<ECS::AIControllerComponent>(enemy);
                ai.moveSpeed = eSpeeds[i];
                auto& bcol = m_World->AddComponent<ECS::BoxColliderComponent>(enemy);
                bcol.size = Math::Vector3(1.0f, 1.0f, 0.5f);
            }
        }

        // Power-up (shield)
        {
            ECS::Entity powerup = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(powerup, "Shield Power-Up");
            auto& pt = m_World->AddComponent<ECS::TransformComponent>(powerup);
            pt.position = Math::Vector3(0.0f, -5.0f, 0.0f);
            pt.scale = Math::Vector3(0.8f, 0.8f, 1.0f);
            auto& pmat = m_World->AddComponent<ECS::MaterialComponent>(powerup);
            pmat.baseColor = Math::Vector3(0.2f, 0.8f, 1.0f);
            pmat.emissiveColor = Math::Vector3(0.2f, 0.8f, 1.0f);
            pmat.emissiveStrength = 1.0f;
            m_World->AddComponent<ECS::MeshComponent>(powerup, Renderer::MeshFactory::CreateQuad(1.0f, 1.0f));
            m_World->AddComponent<ECS::Sprite2DComponent>(powerup);
            auto& pk = m_World->AddComponent<ECS::PickupComponent>(powerup);
            pk.type = ECS::PickupComponent::PickupType::Powerup;
            pk.customId = "Shield";
            pk.value = 1.0f;
            auto& tw = m_World->AddComponent<ECS::TweenComponent>(powerup);
            ECS::TweenEntry pulse;
            pulse.property = ECS::TweenProperty::Scale;
            pulse.easing = ECS::EasingType::EaseInOutSine;
            pulse.mode = ECS::TweenMode::PingPong;
            pulse.startValue = Math::Vector3(0.7f, 0.7f, 1.0f);
            pulse.endValue = Math::Vector3(0.9f, 0.9f, 1.0f);
            pulse.duration = 1.0f;
            tw.tweens.push_back(pulse);
        }

        // Score display
        {
            ECS::Entity score = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(score, "Score Display");
            m_World->AddComponent<ECS::TransformComponent>(score);
            auto& tc = m_World->AddComponent<ECS::TextComponent>(score);
            tc.text = "SCORE: 0";
            tc.fontSize = 36.0f;
            tc.textColor = Math::Vector3(1.0f, 1.0f, 0.0f);
            m_World->AddComponent<ECS::TagComponent>(score).tags.push_back("score");
        }

        // Background parallax layer
        {
            ECS::Entity bgLayer = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(bgLayer, "Star Field");
            auto& bgt = m_World->AddComponent<ECS::TransformComponent>(bgLayer);
            bgt.position = Math::Vector3(0.0f, 0.0f, -2.0f);
            bgt.scale = Math::Vector3(20.0f, 24.0f, 1.0f);
            auto& bgmat = m_World->AddComponent<ECS::MaterialComponent>(bgLayer);
            bgmat.baseColor = Math::Vector3(0.05f, 0.02f, 0.1f);
            m_World->AddComponent<ECS::MeshComponent>(bgLayer, Renderer::MeshFactory::CreateQuad(1.0f, 1.0f));
            m_World->AddComponent<ECS::Sprite2DComponent>(bgLayer);
        }

        // Boundary walls
        {
            const Math::Vector3 wPos[] = { {0,10,0}, {0,-10,0}, {8,0,0}, {-8,0,0} };
            const Math::Vector3 wScl[] = { {16,0.5f,1}, {16,0.5f,1}, {0.5f,20,1}, {0.5f,20,1} };
            for (int i = 0; i < 4; ++i) {
                ECS::Entity wall = m_World->CreateEntity();
                m_World->AddComponent<ECS::NameComponent>(wall, "Boundary " + std::to_string(i + 1));
                auto& wt = m_World->AddComponent<ECS::TransformComponent>(wall);
                wt.position = wPos[i];
                wt.scale = wScl[i];
                auto& wmat = m_World->AddComponent<ECS::MaterialComponent>(wall);
                wmat.baseColor = Math::Vector3(0.3f, 0.3f, 0.35f);
                m_World->AddComponent<ECS::MeshComponent>(wall, Renderer::MeshFactory::CreateQuad(1.0f, 1.0f));
                auto& wcol = m_World->AddComponent<ECS::BoxColliderComponent>(wall);
                wcol.size = wScl[i];
            }
        }

        {
            ECS::Entity hint = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(hint, "Bullet Hell Guide");
            m_World->AddComponent<ECS::TransformComponent>(hint);
            auto& notes = m_World->AddComponent<ECS::NotesComponent>(hint);
            notes.notes = "Bullet hell template with 3 enemy types:\n"
                "- Chaser: fast, pursues player directly\n"
                "- Circle: fires in circular patterns\n"
                "- Spread: slow but wide projectile spread\n"
                "Use PoolableComponent + ObjectPool for bullet management.\n"
                "Shield power-up pulses and grants temporary invulnerability.\n"
                "Star Field provides dark background parallax layer.";
        }

        // Dark space skybox
        {
            Renderer::SkyboxConfig skyConfig;
            skyConfig.type = Renderer::SkyboxType::SolidColor;
            skyConfig.solidColor = Math::Vector3(0.02f, 0.01f, 0.05f);
            m_RenderSystem->SetSkybox(skyConfig);
        }
        m_RenderSystem->SetShadowsEnabled(false);
        m_RenderSystem->SetAmbientIntensity(0.2f);
        if (m_PostProcessing) {
            auto& pp = m_PostProcessing->GetSettings();
            pp.fxaaEnabled = 1;
            pp.bloomEnabled = 1;
            pp.bloomThreshold = 0.5f;
            pp.bloomIntensity = 0.6f;
        }

    } else if (templateId == "idleclicker") {
        // Orthographic camera
        {
            ECS::Entity cam = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(cam, "Camera");
            auto& ct = m_World->AddComponent<ECS::TransformComponent>(cam);
            ct.position = Math::Vector3(0.0f, 0.0f, 10.0f);
            auto& cc = m_World->AddComponent<ECS::CameraComponent>(cam);
            cc.projectionType = ECS::ProjectionType::Orthographic;
            cc.orthoSize = 5.4f;
            cc.isActive = true;
            cc.priority = 10;
            m_SelectedGameCamera = cam;
        }

        // Main click target
        {
            ECS::Entity clicker = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(clicker, "Click Target");
            auto& ct = m_World->AddComponent<ECS::TransformComponent>(clicker);
            ct.position = Math::Vector3(0.0f, 1.0f, 0.0f);
            ct.scale = Math::Vector3(3.0f, 3.0f, 1.0f);
            auto& cmat = m_World->AddComponent<ECS::MaterialComponent>(clicker);
            cmat.baseColor = Math::Vector3(0.3f, 0.7f, 0.3f);
            m_World->AddComponent<ECS::MeshComponent>(clicker, Renderer::MeshFactory::CreateQuad(1.0f, 1.0f));
            m_World->AddComponent<ECS::Sprite2DComponent>(clicker);
            auto& tw = m_World->AddComponent<ECS::TweenComponent>(clicker);
            tw.autoPlay = false;
            ECS::TweenEntry bounce;
            bounce.property = ECS::TweenProperty::Scale;
            bounce.easing = ECS::EasingType::EaseOutBack;
            bounce.mode = ECS::TweenMode::Once;
            bounce.startValue = Math::Vector3(2.8f, 2.8f, 1.0f);
            bounce.endValue = Math::Vector3(3.0f, 3.0f, 1.0f);
            bounce.duration = 0.3f;
            bounce.useCurrentAsStart = false;
            tw.tweens.push_back(bounce);
        }

        // UI Canvas with idle game HUD elements
        {
            ECS::Entity ui = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(ui, "Game UI");
            m_World->AddComponent<ECS::TransformComponent>(ui);
            auto& canvas = m_World->AddComponent<GUI::UICanvasComponent>(ui);
            canvas.canvasName = "IdleUI";
            canvas.designWidth = 1920.0f;
            canvas.designHeight = 1080.0f;

            // Currency label (top center)
            u32 currId = canvas.AddElement(GUI::UIWidgetType::Label, "CurrencyLabel");
            if (auto* curr = canvas.GetElement(currId)) {
                curr->anchor.anchorMin = Math::Vector2(0.5f, 0.0f);
                curr->anchor.anchorMax = Math::Vector2(0.5f, 0.0f);
                curr->anchor.offsetLeft = 100.0f; curr->anchor.offsetTop = 30.0f;
                curr->anchor.offsetRight = -100.0f; curr->anchor.offsetBottom = -80.0f;
                curr->data.text = "Gold: 0";
                curr->data.textAlignH = 1;
                curr->style.textColor = Math::Vector3(1.0f, 0.85f, 0.0f);
                curr->style.fontSize = 36.0f;
                curr->focusable = false;
            }
            // Click power label
            u32 cpId = canvas.AddElement(GUI::UIWidgetType::Label, "ClickPowerLabel");
            if (auto* cp = canvas.GetElement(cpId)) {
                cp->anchor.anchorMin = Math::Vector2(0.5f, 0.0f);
                cp->anchor.anchorMax = Math::Vector2(0.5f, 0.0f);
                cp->anchor.offsetLeft = 100.0f; cp->anchor.offsetTop = 80.0f;
                cp->anchor.offsetRight = -100.0f; cp->anchor.offsetBottom = -120.0f;
                cp->data.text = "Click Power: 1";
                cp->data.textAlignH = 1;
                cp->style.textColor = Math::Vector3(0.8f, 0.8f, 0.9f);
                cp->focusable = false;
            }
            // Upgrade button (bottom center)
            u32 upgId = canvas.AddElement(GUI::UIWidgetType::Button, "UpgradeButton");
            if (auto* upg = canvas.GetElement(upgId)) {
                upg->anchor.anchorMin = Math::Vector2(0.5f, 1.0f);
                upg->anchor.anchorMax = Math::Vector2(0.5f, 1.0f);
                upg->anchor.offsetLeft = 120.0f; upg->anchor.offsetTop = 120.0f;
                upg->anchor.offsetRight = -120.0f; upg->anchor.offsetBottom = -70.0f;
                upg->data.text = "Upgrade (Cost: 10)";
                upg->tabOrder = 1;
            }
            // Auto-click toggle (below upgrade)
            u32 actId = canvas.AddElement(GUI::UIWidgetType::Toggle, "AutoClickToggle");
            if (auto* act = canvas.GetElement(actId)) {
                act->anchor.anchorMin = Math::Vector2(0.5f, 1.0f);
                act->anchor.anchorMax = Math::Vector2(0.5f, 1.0f);
                act->anchor.offsetLeft = 120.0f; act->anchor.offsetTop = 60.0f;
                act->anchor.offsetRight = -120.0f; act->anchor.offsetBottom = -20.0f;
                act->data.text = "Auto-Click";
                act->tabOrder = 2;
            }
        }

        // Visual upgrade markers (appear at progression milestones)
        {
            const Math::Vector3 mPos[] = { {-4,0,0}, {4,0,0}, {0,-3.5f,0} };
            const Math::Vector3 mCol[] = { {0.6f,0.6f,0.6f}, {0.8f,0.7f,0.2f}, {0.3f,0.8f,1.0f} };
            const char* mNames[] = { "Bronze Trophy", "Gold Trophy", "Diamond Trophy" };
            for (int i = 0; i < 3; ++i) {
                ECS::Entity marker = m_World->CreateEntity();
                m_World->AddComponent<ECS::NameComponent>(marker, mNames[i]);
                auto& mt = m_World->AddComponent<ECS::TransformComponent>(marker);
                mt.position = mPos[i];
                mt.scale = Math::Vector3(1.0f, 1.0f, 1.0f);
                mt.visible = (i == 0); // Only bronze visible initially
                auto& mmat = m_World->AddComponent<ECS::MaterialComponent>(marker);
                mmat.baseColor = mCol[i];
                mmat.emissiveColor = mCol[i];
                mmat.emissiveStrength = 0.3f;
                m_World->AddComponent<ECS::MeshComponent>(marker, Renderer::MeshFactory::CreateQuad(1.0f, 1.0f));
                m_World->AddComponent<ECS::Sprite2DComponent>(marker);
                m_World->AddComponent<ECS::TagComponent>(marker).tags.push_back("trophy_" + std::to_string(i));
            }
        }

        // Particle burst effect (triggered on click)
        {
            ECS::Entity burst = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(burst, "Click Burst");
            auto& bt = m_World->AddComponent<ECS::TransformComponent>(burst);
            bt.position = Math::Vector3(0.0f, 1.0f, 0.5f);
            auto& pe = m_World->AddComponent<ECS::ParticleEmitterComponent>(burst);
            ECS::ApplyParticlePreset(pe, "Sparks");
            pe.playOnAwake = false;
            pe.emissionRate = 50.0f;
            pe.startColor = Math::Vector3(1.0f, 0.85f, 0.0f);
        }

        // Background decorations
        {
            ECS::Entity bgLeft = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(bgLeft, "Decoration Left");
            auto& lt = m_World->AddComponent<ECS::TransformComponent>(bgLeft);
            lt.position = Math::Vector3(-7.0f, 0.0f, -0.5f);
            lt.scale = Math::Vector3(3.0f, 8.0f, 1.0f);
            auto& lmat = m_World->AddComponent<ECS::MaterialComponent>(bgLeft);
            lmat.baseColor = Math::Vector3(0.15f, 0.2f, 0.25f);
            m_World->AddComponent<ECS::MeshComponent>(bgLeft, Renderer::MeshFactory::CreateQuad(1.0f, 1.0f));
            m_World->AddComponent<ECS::Sprite2DComponent>(bgLeft);

            ECS::Entity bgRight = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(bgRight, "Decoration Right");
            auto& rt = m_World->AddComponent<ECS::TransformComponent>(bgRight);
            rt.position = Math::Vector3(7.0f, 0.0f, -0.5f);
            rt.scale = Math::Vector3(3.0f, 8.0f, 1.0f);
            auto& rmat = m_World->AddComponent<ECS::MaterialComponent>(bgRight);
            rmat.baseColor = Math::Vector3(0.15f, 0.2f, 0.25f);
            m_World->AddComponent<ECS::MeshComponent>(bgRight, Renderer::MeshFactory::CreateQuad(1.0f, 1.0f));
            m_World->AddComponent<ECS::Sprite2DComponent>(bgRight);
        }

        // Meta-progression save
        {
            ECS::Entity save = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(save, "Save Data");
            m_World->AddComponent<ECS::TransformComponent>(save);
            auto& sd = m_World->AddComponent<ECS::SaveDataComponent>(save);
            sd.tier = ECS::PersistenceTier::MetaProgression;
            sd.customData = { {"currency", "0"}, {"clickPower", "1"}, {"autoClickRate", "0"}, {"highestTrophy", "0"} };
        }

        {
            ECS::Entity hint = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(hint, "Idle Guide");
            m_World->AddComponent<ECS::TransformComponent>(hint);
            auto& notes = m_World->AddComponent<ECS::NotesComponent>(hint);
            notes.notes = "Idle/clicker template with full UI:\n"
                "- Currency and Click Power labels update in real-time\n"
                "- Upgrade Button increases click power (costs gold)\n"
                "- Auto-Click toggle enables passive income\n"
                "- Trophy markers unlock at milestones (toggle visible)\n"
                "- Click Burst particles fire on each click\n"
                "SaveDataComponent with MetaProgression persists across runs.";
        }

        // Warm gradient background
        {
            Renderer::SkyboxConfig skyConfig;
            skyConfig.type = Renderer::SkyboxType::SolidColor;
            skyConfig.solidColor = Math::Vector3(0.12f, 0.1f, 0.18f);
            m_RenderSystem->SetSkybox(skyConfig);
        }
        m_RenderSystem->SetShadowsEnabled(false);
        m_RenderSystem->SetAmbientIntensity(0.3f);

    } else if (templateId == "planetgravity") {
        // Planet
        {
            ECS::Entity planet = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(planet, "Planet");
            auto& pt = m_World->AddComponent<ECS::TransformComponent>(planet);
            pt.position = Math::Vector3(0.0f, 0.0f, 0.0f);
            pt.scale = Math::Vector3(5.0f, 5.0f, 5.0f);
            auto& pmat = m_World->AddComponent<ECS::MaterialComponent>(planet);
            pmat.baseColor = Math::Vector3(0.3f, 0.5f, 0.25f);
            pmat.roughness = 0.8f;
            m_World->AddComponent<ECS::MeshComponent>(planet, Renderer::MeshFactory::CreateSphere(1.0f));
            auto& gz = m_World->AddComponent<ECS::GravityZoneComponent>(planet);
            gz.mode = ECS::GravityZoneMode::Point;
            gz.shape = ECS::GravityZoneShape::Sphere;
            gz.halfExtents = Math::Vector3(50.0f, 50.0f, 50.0f);
            gz.gravityStrength = 15.0f;
            auto& sc = m_World->AddComponent<ECS::SphereColliderComponent>(planet);
            sc.radius = 5.0f;
        }

        // Player on surface
        ECS::Entity player = createPlayer3D("Player");
        {
            auto* pt = m_World->GetComponent<ECS::TransformComponent>(player);
            if (pt) pt->position = Math::Vector3(0.0f, 6.5f, 0.0f);
            auto& saCtrl = m_World->AddComponent<ECS::SurfaceAlignedController>(player);
            saCtrl.moveSpeed = 6.0f;
            saCtrl.jumpForce = 12.0f;
            saCtrl.cameraDistance = 10.0f;
            saCtrl.cameraHeight = 4.0f;
        }

        // Camera
        {
            ECS::Entity cam = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(cam, "Camera");
            auto& ct = m_World->AddComponent<ECS::TransformComponent>(cam);
            ct.position = Math::Vector3(0.0f, 15.0f, -10.0f);
            auto& cc = m_World->AddComponent<ECS::CameraComponent>(cam);
            cc.projectionType = ECS::ProjectionType::Perspective;
            cc.fieldOfView = 60.0f;
            cc.isActive = true;
            cc.priority = 10;
            auto& follow = m_World->AddComponent<ECS::FollowTargetComponent>(cam);
            follow.target = player;
            follow.offset = Math::Vector3(0.0f, 8.0f, -8.0f);
            auto& lookAt = m_World->AddComponent<ECS::LookAtTargetComponent>(cam);
            lookAt.target = player;
            m_SelectedGameCamera = cam;
        }

        // 3 props on surface
        {
            const f32 angles[] = { 0.0f, 2.1f, 4.2f };
            const char* propNames[] = { "Rock", "Crystal", "Tree" };
            const Math::Vector3 propCol[] = { {0.5f,0.45f,0.4f}, {0.3f,0.7f,0.9f}, {0.25f,0.5f,0.2f} };
            for (int i = 0; i < 3; ++i) {
                ECS::Entity prop = m_World->CreateEntity();
                m_World->AddComponent<ECS::NameComponent>(prop, propNames[i]);
                auto& pt = m_World->AddComponent<ECS::TransformComponent>(prop);
                pt.position = Math::Vector3(
                    5.5f * std::cos(angles[i]),
                    5.5f * std::sin(angles[i]),
                    0.0f
                );
                pt.scale = Math::Vector3(0.5f, 0.5f, 0.5f);
                auto& pmat = m_World->AddComponent<ECS::MaterialComponent>(prop);
                pmat.baseColor = propCol[i];
                m_World->AddComponent<ECS::MeshComponent>(prop, Renderer::MeshFactory::CreateCube(1.0f));
            }
        }

        // Second moon (smaller, different color)
        {
            ECS::Entity moon = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(moon, "Small Moon");
            auto& mt = m_World->AddComponent<ECS::TransformComponent>(moon);
            mt.position = Math::Vector3(15.0f, 8.0f, 0.0f);
            mt.scale = Math::Vector3(2.0f, 2.0f, 2.0f);
            auto& mmat = m_World->AddComponent<ECS::MaterialComponent>(moon);
            mmat.baseColor = Math::Vector3(0.6f, 0.55f, 0.5f);
            mmat.roughness = 0.9f;
            m_World->AddComponent<ECS::MeshComponent>(moon, Renderer::MeshFactory::CreateSphere(1.0f));
            auto& gz = m_World->AddComponent<ECS::GravityZoneComponent>(moon);
            gz.mode = ECS::GravityZoneMode::Point;
            gz.shape = ECS::GravityZoneShape::Sphere;
            gz.halfExtents = Math::Vector3(20.0f, 20.0f, 20.0f);
            gz.gravityStrength = 8.0f;
            auto& sc = m_World->AddComponent<ECS::SphereColliderComponent>(moon);
            sc.radius = 2.0f;
        }

        // Collectible on planet surface
        {
            ECS::Entity gem = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(gem, "Space Gem");
            auto& gt = m_World->AddComponent<ECS::TransformComponent>(gem);
            gt.position = Math::Vector3(0.0f, 5.8f, 2.0f);
            gt.scale = Math::Vector3(0.4f, 0.4f, 0.4f);
            auto& gmat = m_World->AddComponent<ECS::MaterialComponent>(gem);
            gmat.baseColor = Math::Vector3(0.3f, 0.9f, 1.0f);
            gmat.emissiveColor = Math::Vector3(0.3f, 0.9f, 1.0f);
            gmat.emissiveStrength = 1.0f;
            m_World->AddComponent<ECS::MeshComponent>(gem, Renderer::MeshFactory::CreateSphere(0.2f));
            auto& pk = m_World->AddComponent<ECS::PickupComponent>(gem);
            pk.type = ECS::PickupComponent::PickupType::Custom;
            pk.customId = "Space Gem";
            pk.value = 1.0f;
            auto& tw = m_World->AddComponent<ECS::TweenComponent>(gem);
            ECS::TweenEntry spin;
            spin.property = ECS::TweenProperty::Position;
            spin.easing = ECS::EasingType::EaseInOutSine;
            spin.mode = ECS::TweenMode::PingPong;
            spin.startValue = Math::Vector3(0.0f, 5.8f, 2.0f);
            spin.endValue = Math::Vector3(0.0f, 6.3f, 2.0f);
            spin.duration = 1.5f;
            tw.tweens.push_back(spin);
        }

        {
            Renderer::SkyboxConfig skyConfig;
            skyConfig.type = Renderer::SkyboxType::Procedural;
            skyConfig.topColor = Math::Vector3(0.01f, 0.01f, 0.03f);
            skyConfig.horizonColor = Math::Vector3(0.02f, 0.02f, 0.05f);
            skyConfig.bottomColor = Math::Vector3(0.01f, 0.01f, 0.02f);
            m_RenderSystem->SetSkybox(skyConfig);
        }
        m_RenderSystem->SetShadowsEnabled(true);
        m_RenderSystem->SetAmbientIntensity(0.12f);
        if (m_PostProcessing) {
            auto& pp = m_PostProcessing->GetSettings();
            pp.fxaaEnabled = 1;
            pp.bloomEnabled = 1;
            pp.bloomThreshold = 0.7f;
            pp.bloomIntensity = 0.4f;
        }

    } else if (templateId == "dungeon") {
        const f32 CELL = 3.0f;

        // Floor
        {
            ECS::Entity floor = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(floor, "Floor");
            auto& ft = m_World->AddComponent<ECS::TransformComponent>(floor);
            ft.position = Math::Vector3(CELL * 2.5f, 0.0f, CELL * 2.5f);
            ft.scale = Math::Vector3(CELL * 6, 0.1f, CELL * 6);
            auto& fmat = m_World->AddComponent<ECS::MaterialComponent>(floor);
            fmat.baseColor = Math::Vector3(0.25f, 0.22f, 0.2f);
            fmat.roughness = 0.95f;
            m_World->AddComponent<ECS::MeshComponent>(floor, Renderer::MeshFactory::CreateCube(1.0f));
            auto& fcol = m_World->AddComponent<ECS::BoxColliderComponent>(floor);
            fcol.size = ft.scale;
        }

        // L-shaped corridor walls (10 segments)
        {
            struct WallDef { f32 x, z, sx, sz; };
            WallDef walls[] = {
                {0, 0, 0.3f, CELL*4},       // left wall of corridor 1
                {CELL, 0, 0.3f, CELL*4},     // right wall of corridor 1
                {0, CELL*2, CELL*3, 0.3f},   // top wall of turn
                {CELL, CELL, CELL*2, 0.3f},  // inner wall of turn
                {CELL*3, CELL, 0.3f, CELL*4}, // right wall of corridor 2
                {CELL*2, CELL, 0.3f, CELL*4}, // left wall of corridor 2
                {0, -CELL, CELL, 0.3f},       // bottom cap 1
                {CELL*2, CELL*4, CELL*2, 0.3f}, // top cap 2
                {CELL*3, CELL*4, 0.3f, CELL},   // right cap end
                {-0.15f, CELL*2, 0.3f, CELL},   // left extension
            };
            for (int i = 0; i < 10; ++i) {
                ECS::Entity wall = m_World->CreateEntity();
                m_World->AddComponent<ECS::NameComponent>(wall, "Wall " + std::to_string(i + 1));
                auto& wt = m_World->AddComponent<ECS::TransformComponent>(wall);
                wt.position = Math::Vector3(walls[i].x, 1.5f, walls[i].z);
                wt.scale = Math::Vector3(walls[i].sx, 3.0f, walls[i].sz);
                auto& wmat = m_World->AddComponent<ECS::MaterialComponent>(wall);
                wmat.baseColor = Math::Vector3(0.35f, 0.3f, 0.25f);
                m_World->AddComponent<ECS::MeshComponent>(wall, Renderer::MeshFactory::CreateCube(1.0f));
                auto& wcol = m_World->AddComponent<ECS::BoxColliderComponent>(wall);
                wcol.size = wt.scale;
            }
        }

        // Player
        ECS::Entity player = createPlayer3D("Player");
        {
            auto* pt = m_World->GetComponent<ECS::TransformComponent>(player);
            if (pt) pt->position = Math::Vector3(CELL * 0.5f, 1.0f, -CELL * 0.5f);
            auto& fps = m_World->AddComponent<ECS::FirstPersonController>(player);
            fps.moveSpeed = 4.0f;
            fps.gridMovement = true;
            fps.gridCellSize = CELL;
            fps.gridOrigin = Math::Vector3(CELL * 0.5f, 0.0f, CELL * 0.5f);
            fps.dungeonCrawlerMode = true;
            fps.snapTurnAngle = 90.0f;
            SetupCameraForController(player, "FirstPerson");
        }

        // Enemy
        {
            ECS::Entity enemy = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(enemy, "Skeleton");
            auto& et = m_World->AddComponent<ECS::TransformComponent>(enemy);
            et.position = Math::Vector3(CELL * 2.5f, 1.0f, CELL * 2.5f);
            auto& emat = m_World->AddComponent<ECS::MaterialComponent>(enemy);
            emat.baseColor = Math::Vector3(0.8f, 0.75f, 0.65f);
            m_World->AddComponent<ECS::MeshComponent>(enemy, Renderer::MeshFactory::CreateCapsule(0.3f, 1.0f));
            auto& hp = m_World->AddComponent<ECS::HealthComponent>(enemy);
            hp.maxHealth = 30.0f;
            hp.currentHealth = 30.0f;
            auto& ai = m_World->AddComponent<ECS::AIControllerComponent>(enemy);
            ai.currentState = ECS::AIControllerComponent::AIState::Patrol;
            ai.moveSpeed = 2.0f;
        }

        // Second enemy (in corridor 2)
        {
            ECS::Entity enemy2 = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(enemy2, "Zombie");
            auto& e2t = m_World->AddComponent<ECS::TransformComponent>(enemy2);
            e2t.position = Math::Vector3(CELL * 2.5f, 1.0f, CELL * 1.0f);
            auto& e2mat = m_World->AddComponent<ECS::MaterialComponent>(enemy2);
            e2mat.baseColor = Math::Vector3(0.4f, 0.5f, 0.35f);
            m_World->AddComponent<ECS::MeshComponent>(enemy2, Renderer::MeshFactory::CreateCapsule(0.3f, 1.0f));
            auto& e2hp = m_World->AddComponent<ECS::HealthComponent>(enemy2);
            e2hp.maxHealth = 20.0f;
            e2hp.currentHealth = 20.0f;
            auto& e2ai = m_World->AddComponent<ECS::AIControllerComponent>(enemy2);
            e2ai.currentState = ECS::AIControllerComponent::AIState::Idle;
            e2ai.moveSpeed = 1.5f;
        }

        // Locked door (requires key)
        {
            ECS::Entity lockedDoor = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(lockedDoor, "Locked Door");
            auto& ldt = m_World->AddComponent<ECS::TransformComponent>(lockedDoor);
            ldt.position = Math::Vector3(CELL * 0.5f, 1.5f, CELL * 1.0f);
            ldt.scale = Math::Vector3(CELL * 0.9f, 3.0f, 0.3f);
            auto& ldmat = m_World->AddComponent<ECS::MaterialComponent>(lockedDoor);
            ldmat.baseColor = Math::Vector3(0.4f, 0.25f, 0.1f);
            m_World->AddComponent<ECS::MeshComponent>(lockedDoor, Renderer::MeshFactory::CreateCube(1.0f));
            auto& ldsw = m_World->AddComponent<ECS::SwitchComponent>(lockedDoor);
            ldsw.type = ECS::SwitchComponent::SwitchType::Toggle;
            ldsw.promptText = "Unlock Door (requires key)";
            auto& ldcol = m_World->AddComponent<ECS::BoxColliderComponent>(lockedDoor);
            ldcol.size = ldt.scale;
        }

        // Dungeon key
        {
            ECS::Entity dKey = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(dKey, "Dungeon Key");
            auto& dkt = m_World->AddComponent<ECS::TransformComponent>(dKey);
            dkt.position = Math::Vector3(CELL * 2.5f, 0.5f, CELL * 0.5f);
            dkt.scale = Math::Vector3(0.3f, 0.3f, 0.1f);
            auto& dkmat = m_World->AddComponent<ECS::MaterialComponent>(dKey);
            dkmat.baseColor = Math::Vector3(0.9f, 0.8f, 0.2f);
            dkmat.emissiveColor = Math::Vector3(0.9f, 0.8f, 0.2f);
            dkmat.emissiveStrength = 0.6f;
            m_World->AddComponent<ECS::MeshComponent>(dKey, Renderer::MeshFactory::CreateCube(1.0f));
            auto& dkpk = m_World->AddComponent<ECS::PickupComponent>(dKey);
            dkpk.type = ECS::PickupComponent::PickupType::Key;
            dkpk.customId = "Dungeon Key";
            dkpk.value = 1.0f;
            m_World->AddComponent<ECS::InteractableComponent>(dKey).promptText = "Pick up Key";
        }

        // Treasure
        {
            ECS::Entity treasure = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(treasure, "Treasure");
            auto& tt = m_World->AddComponent<ECS::TransformComponent>(treasure);
            tt.position = Math::Vector3(CELL * 2.5f, 0.4f, CELL * 3.5f);
            tt.scale = Math::Vector3(0.6f, 0.6f, 0.4f);
            auto& tmat = m_World->AddComponent<ECS::MaterialComponent>(treasure);
            tmat.baseColor = Math::Vector3(0.7f, 0.55f, 0.1f);
            m_World->AddComponent<ECS::MeshComponent>(treasure, Renderer::MeshFactory::CreateCube(1.0f));
            auto& pick = m_World->AddComponent<ECS::PickupComponent>(treasure);
            pick.type = ECS::PickupComponent::PickupType::Coin;
            pick.value = 100.0f;
        }

        // Corridor lights
        {
            const Math::Vector3 lightPos[] = {
                {CELL*0.5f, 2.5f, CELL*0.5f},
                {CELL*0.5f, 2.5f, CELL*1.5f},
                {CELL*2.5f, 2.5f, CELL*1.5f},
                {CELL*2.5f, 2.5f, CELL*3.0f}
            };
            for (int i = 0; i < 4; ++i) {
                ECS::Entity light = m_World->CreateEntity();
                m_World->AddComponent<ECS::NameComponent>(light, "Torch Light " + std::to_string(i + 1));
                auto& lt = m_World->AddComponent<ECS::TransformComponent>(light);
                lt.position = lightPos[i];
                auto& lc = m_World->AddComponent<ECS::LightComponent>(light);
                lc.type = ECS::LightType::Point;
                lc.intensity = 1.5f;
                lc.range = 6.0f;
                lc.color = Math::Vector3(1.0f, 0.7f, 0.4f);
            }
        }

        {
            Renderer::SkyboxConfig skyConfig;
            skyConfig.type = Renderer::SkyboxType::SolidColor;
            skyConfig.solidColor = Math::Vector3(0.0f, 0.0f, 0.0f);
            m_RenderSystem->SetSkybox(skyConfig);
        }
        m_RenderSystem->SetShadowsEnabled(true);
        m_RenderSystem->SetAmbientIntensity(0.03f);
        if (m_PostProcessing) {
            auto& pp = m_PostProcessing->GetSettings();
            pp.fxaaEnabled = 1;
            pp.vignetteEnabled = 1;
            pp.vignetteIntensity = 0.3f;
        }
    }
    else if (templateId == "isometric") {
        createGround();
        ECS::Entity player = createPlayer3D("Player");
        auto& ctrl = m_World->AddComponent<ECS::TopDown3DController>(player);
        ctrl.moveSpeed = 5.0f;
        ctrl.cameraAngle = 45.0f;
        ctrl.cameraDistance = 15.0f;
        SetupCameraForController(player, "TopDown3D");

        // Buildings (4 varied cubes representing houses)
        {
            const Math::Vector3 bPos[] = { {-6,1.5f,-4}, {-3,1.0f,-6}, {5,2.0f,-3}, {7,1.0f,-7} };
            const Math::Vector3 bScl[] = { {3,3,3}, {2,2,2}, {3,4,3}, {2.5f,2,2.5f} };
            const Math::Vector3 bCol[] = { {0.7f,0.55f,0.4f}, {0.6f,0.45f,0.35f}, {0.75f,0.6f,0.45f}, {0.65f,0.5f,0.38f} };
            const char* bNames[] = { "House A", "House B", "House C", "House D" };
            for (int i = 0; i < 4; ++i) {
                ECS::Entity bld = m_World->CreateEntity();
                m_World->AddComponent<ECS::NameComponent>(bld, bNames[i]);
                auto& bt = m_World->AddComponent<ECS::TransformComponent>(bld);
                bt.position = bPos[i];
                bt.scale = bScl[i];
                auto& bmat = m_World->AddComponent<ECS::MaterialComponent>(bld);
                bmat.baseColor = bCol[i];
                m_World->AddComponent<ECS::MeshComponent>(bld, Renderer::MeshFactory::CreateCube(1.0f));
                auto& bcol = m_World->AddComponent<ECS::BoxColliderComponent>(bld);
                bcol.size = bScl[i];
            }
        }

        // NPCs with dialogue
        {
            ECS::Entity npc1 = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(npc1, "Villager");
            auto& n1t = m_World->AddComponent<ECS::TransformComponent>(npc1);
            n1t.position = Math::Vector3(-2.0f, 0.5f, -2.0f);
            auto& n1mat = m_World->AddComponent<ECS::MaterialComponent>(npc1);
            n1mat.baseColor = Math::Vector3(0.2f, 0.5f, 0.8f);
            m_World->AddComponent<ECS::MeshComponent>(npc1, Renderer::MeshFactory::CreateCapsule(0.3f, 1.0f));
            auto& n1d = m_World->AddComponent<ECS::DialogueComponent>(npc1);
            n1d.dialogueLines = { "Welcome, traveler!", "The market is to the east." };
            m_World->AddComponent<ECS::InteractableComponent>(npc1).promptText = "Talk to Villager";

            ECS::Entity npc2 = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(npc2, "Merchant");
            auto& n2t = m_World->AddComponent<ECS::TransformComponent>(npc2);
            n2t.position = Math::Vector3(3.0f, 0.5f, 1.0f);
            auto& n2mat = m_World->AddComponent<ECS::MaterialComponent>(npc2);
            n2mat.baseColor = Math::Vector3(0.8f, 0.6f, 0.2f);
            m_World->AddComponent<ECS::MeshComponent>(npc2, Renderer::MeshFactory::CreateCapsule(0.3f, 1.0f));
            auto& n2d = m_World->AddComponent<ECS::DialogueComponent>(npc2);
            n2d.dialogueLines = { "Fine wares for sale!", "Come back anytime." };
            m_World->AddComponent<ECS::InteractableComponent>(npc2).promptText = "Talk to Merchant";
        }

        // Treasure chest
        {
            ECS::Entity chest = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(chest, "Treasure Chest");
            auto& chT = m_World->AddComponent<ECS::TransformComponent>(chest);
            chT.position = Math::Vector3(6.0f, 0.3f, 2.0f);
            chT.scale = Math::Vector3(0.8f, 0.6f, 0.6f);
            auto& chMat = m_World->AddComponent<ECS::MaterialComponent>(chest);
            chMat.baseColor = Math::Vector3(0.6f, 0.4f, 0.1f);
            m_World->AddComponent<ECS::MeshComponent>(chest, Renderer::MeshFactory::CreateCube(1.0f));
            auto& pk = m_World->AddComponent<ECS::PickupComponent>(chest);
            pk.type = ECS::PickupComponent::PickupType::Coin;
            pk.customId = "Gold Coins";
            pk.value = 50.0f;
            m_World->AddComponent<ECS::InteractableComponent>(chest).promptText = "Open Chest";
        }

        // Lantern (point light)
        {
            ECS::Entity lantern = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(lantern, "Lantern");
            auto& lnT = m_World->AddComponent<ECS::TransformComponent>(lantern);
            lnT.position = Math::Vector3(0.0f, 3.0f, 0.0f);
            auto& lnL = m_World->AddComponent<ECS::LightComponent>(lantern);
            lnL.type = ECS::LightType::Point;
            lnL.color = Math::Vector3(1.0f, 0.9f, 0.7f);
            lnL.intensity = 2.0f;
            lnL.range = 15.0f;
        }

        // Notes
        {
            ECS::Entity hint = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(hint, "Isometric Guide");
            m_World->AddComponent<ECS::TransformComponent>(hint);
            auto& notes = m_World->AddComponent<ECS::NotesComponent>(hint);
            notes.notes = "Isometric template uses TopDown3DController with 45-degree camera angle.\n"
                "Camera distance 15 gives a classic isometric overview.\n"
                "Buildings use BoxColliderComponent for solid walls.\n"
                "NPCs have DialogueComponent + InteractableComponent for talk prompts.";
        }

        // Skybox
        {
            Renderer::SkyboxConfig skyConfig;
            skyConfig.type = Renderer::SkyboxType::Procedural;
            skyConfig.topColor = Math::Vector3(0.1f, 0.3f, 0.8f);
            skyConfig.horizonColor = Math::Vector3(0.5f, 0.7f, 1.0f);
            skyConfig.bottomColor = Math::Vector3(0.8f, 0.85f, 0.9f);
            skyConfig.sunDirection = Math::Vector3(0.0f, 1.0f, 0.0f);
            m_RenderSystem->SetSkybox(skyConfig);
        }

        // Render settings
        m_RenderSystem->SetShadowsEnabled(true);
        m_RenderSystem->SetAmbientIntensity(0.15f);

        // Post-processing
        if (m_PostProcessing) {
            auto& pp = m_PostProcessing->GetSettings();
            pp.fxaaEnabled = 1;
        }


    } else if (templateId == "visualnovel") {
        // Camera: orthographic, 16:9, looking at -Z
        ECS::Entity cam = m_World->CreateEntity();
        m_World->AddComponent<ECS::NameComponent>(cam, "VN Camera");
        auto& camT = m_World->AddComponent<ECS::TransformComponent>(cam);
        camT.position = Math::Vector3(0.0f, 0.0f, 10.0f);
        auto& camC = m_World->AddComponent<ECS::CameraComponent>(cam);
        camC.projectionType = ECS::ProjectionType::Orthographic;
        camC.orthoSize = 5.4f;
        camC.nearPlane = 0.1f;
        camC.farPlane = 100.0f;
        m_SelectedGameCamera = cam;

        // Background quad (fills screen)
        ECS::Entity bg = m_World->CreateEntity();
        m_World->AddComponent<ECS::NameComponent>(bg, "Background");
        auto& bgT = m_World->AddComponent<ECS::TransformComponent>(bg);
        bgT.position = Math::Vector3(0.0f, 0.0f, -1.0f);
        bgT.scale = Math::Vector3(19.2f, 10.8f, 1.0f);
        auto& bgMat = m_World->AddComponent<ECS::MaterialComponent>(bg);
        bgMat.baseColor = Math::Vector3(0.5f, 0.6f, 0.8f);  // Light blue placeholder
        m_World->AddComponent<ECS::MeshComponent>(bg, Renderer::MeshFactory::CreateQuad(1.0f, 1.0f));

        // Character Left
        ECS::Entity charL = m_World->CreateEntity();
        m_World->AddComponent<ECS::NameComponent>(charL, "Character Left");
        auto& clT = m_World->AddComponent<ECS::TransformComponent>(charL);
        clT.position = Math::Vector3(-3.0f, 0.0f, 0.0f);
        clT.scale = Math::Vector3(3.0f, 5.0f, 1.0f);
        auto& clMat = m_World->AddComponent<ECS::MaterialComponent>(charL);
        clMat.baseColor = Math::Vector3(0.9f, 0.85f, 0.8f);
        m_World->AddComponent<ECS::MeshComponent>(charL, Renderer::MeshFactory::CreateQuad(1.0f, 1.0f));

        // Character Center
        ECS::Entity charC = m_World->CreateEntity();
        m_World->AddComponent<ECS::NameComponent>(charC, "Character Center");
        auto& ccT = m_World->AddComponent<ECS::TransformComponent>(charC);
        ccT.position = Math::Vector3(0.0f, 0.0f, 0.0f);
        ccT.scale = Math::Vector3(3.0f, 5.0f, 1.0f);
        auto& ccMat = m_World->AddComponent<ECS::MaterialComponent>(charC);
        ccMat.baseColor = Math::Vector3(0.9f, 0.85f, 0.8f);
        m_World->AddComponent<ECS::MeshComponent>(charC, Renderer::MeshFactory::CreateQuad(1.0f, 1.0f));

        // Character Right
        ECS::Entity charR = m_World->CreateEntity();
        m_World->AddComponent<ECS::NameComponent>(charR, "Character Right");
        auto& crT = m_World->AddComponent<ECS::TransformComponent>(charR);
        crT.position = Math::Vector3(3.0f, 0.0f, 0.0f);
        crT.scale = Math::Vector3(3.0f, 5.0f, 1.0f);
        auto& crMat = m_World->AddComponent<ECS::MaterialComponent>(charR);
        crMat.baseColor = Math::Vector3(0.9f, 0.85f, 0.8f);
        m_World->AddComponent<ECS::MeshComponent>(charR, Renderer::MeshFactory::CreateQuad(1.0f, 1.0f));

        // Text Box (dark semi-transparent)
        ECS::Entity textBox = m_World->CreateEntity();
        m_World->AddComponent<ECS::NameComponent>(textBox, "Text Box");
        auto& tbT = m_World->AddComponent<ECS::TransformComponent>(textBox);
        tbT.position = Math::Vector3(0.0f, -3.5f, 1.0f);
        tbT.scale = Math::Vector3(16.0f, 3.0f, 1.0f);
        auto& tbMat = m_World->AddComponent<ECS::MaterialComponent>(textBox);
        tbMat.baseColor = Math::Vector3(0.1f, 0.1f, 0.15f);
        tbMat.opacity = 0.8f;
        tbMat.alphaMode = ECS::MaterialComponent::AlphaMode::Blend;
        m_World->AddComponent<ECS::MeshComponent>(textBox, Renderer::MeshFactory::CreateQuad(1.0f, 1.0f));

        // Dialogue Text entity
        ECS::Entity dialogue = m_World->CreateEntity();
        m_World->AddComponent<ECS::NameComponent>(dialogue, "Dialogue");
        auto& dlT = m_World->AddComponent<ECS::TransformComponent>(dialogue);
        dlT.position = Math::Vector3(0.0f, -3.5f, 1.1f);
        auto& textComp = m_World->AddComponent<ECS::TextComponent>(dialogue);
        textComp.text = "Welcome to the visual novel template.";
        textComp.fontSize = 32.0f;
        textComp.textColor = Math::Vector3(1.0f, 1.0f, 1.0f);
        m_World->AddComponent<ECS::DialogueBoxComponent>(dialogue);

        // Directional Light (even illumination)
        ECS::Entity vnLight = m_World->CreateEntity();
        m_World->AddComponent<ECS::NameComponent>(vnLight, "Light");
        auto& vnLT = m_World->AddComponent<ECS::TransformComponent>(vnLight);
        vnLT.position = Math::Vector3(0.0f, 10.0f, 5.0f);
        auto& vnLC = m_World->AddComponent<ECS::LightComponent>(vnLight);
        vnLC.type = ECS::LightType::Directional;
        vnLC.intensity = 1.2f;
        vnLC.color = Math::Vector3(1.0f, 1.0f, 1.0f);

        // Skybox: solid black
        {
            Renderer::SkyboxConfig skyConfig;
            skyConfig.type = Renderer::SkyboxType::SolidColor;
            skyConfig.solidColor = Math::Vector3(0.0f, 0.0f, 0.0f);
            m_RenderSystem->SetSkybox(skyConfig);
        }

        // Render settings: shadows off for 2D VN
        m_RenderSystem->SetShadowsEnabled(false);

        // Post-processing: bloom + vignette
        if (m_PostProcessing) {
            auto& pp = m_PostProcessing->GetSettings();
            pp.bloomEnabled = 1;
            pp.bloomThreshold = 0.8f;
            pp.bloomIntensity = 0.3f;
            pp.vignetteEnabled = 1;
            pp.vignetteIntensity = 0.15f;
        }

        // Name Plate (speaker name display below text box top edge)
        {
            ECS::Entity namePlate = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(namePlate, "Name Plate");
            auto& npT = m_World->AddComponent<ECS::TransformComponent>(namePlate);
            npT.position = Math::Vector3(-5.5f, -2.2f, 1.2f);
            npT.scale = Math::Vector3(4.0f, 0.8f, 1.0f);
            auto& npMat = m_World->AddComponent<ECS::MaterialComponent>(namePlate);
            npMat.baseColor = Math::Vector3(0.15f, 0.12f, 0.2f);
            npMat.opacity = 0.9f;
            npMat.alphaMode = ECS::MaterialComponent::AlphaMode::Blend;
            m_World->AddComponent<ECS::MeshComponent>(namePlate, Renderer::MeshFactory::CreateQuad(1.0f, 1.0f));
        }
        {
            ECS::Entity nameText = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(nameText, "Speaker Name");
            auto& ntT = m_World->AddComponent<ECS::TransformComponent>(nameText);
            ntT.position = Math::Vector3(-5.5f, -2.2f, 1.3f);
            auto& ntText = m_World->AddComponent<ECS::TextComponent>(nameText);
            ntText.text = "Character Name";
            ntText.fontSize = 26.0f;
            ntText.textColor = Math::Vector3(0.9f, 0.8f, 1.0f);
        }

        // Choice Buttons (3 branching choice buttons below dialogue text)
        for (int ci = 0; ci < 3; ++ci) {
            ECS::Entity choiceBtn = m_World->CreateEntity();
            char cname[32]; snprintf(cname, sizeof(cname), "Choice %d", ci + 1);
            m_World->AddComponent<ECS::NameComponent>(choiceBtn, cname);
            auto& cbT = m_World->AddComponent<ECS::TransformComponent>(choiceBtn);
            cbT.position = Math::Vector3(-5.0f + ci * 5.0f, -4.8f, 1.2f);
            cbT.scale = Math::Vector3(4.5f, 0.7f, 1.0f);
            auto& cbMat = m_World->AddComponent<ECS::MaterialComponent>(choiceBtn);
            cbMat.baseColor = Math::Vector3(0.2f, 0.18f, 0.28f);
            cbMat.opacity = 0.85f;
            cbMat.alphaMode = ECS::MaterialComponent::AlphaMode::Blend;
            m_World->AddComponent<ECS::MeshComponent>(choiceBtn, Renderer::MeshFactory::CreateQuad(1.0f, 1.0f));
            auto& interact = m_World->AddComponent<ECS::InteractableComponent>(choiceBtn);
            interact.promptText = cname;
            auto& tag = m_World->AddComponent<ECS::TagComponent>(choiceBtn);
            tag.tags.push_back("choice_button");
        }
        for (int ci = 0; ci < 3; ++ci) {
            ECS::Entity choiceText = m_World->CreateEntity();
            char ctname[48]; snprintf(ctname, sizeof(ctname), "Choice %d Text", ci + 1);
            m_World->AddComponent<ECS::NameComponent>(choiceText, ctname);
            auto& ctT = m_World->AddComponent<ECS::TransformComponent>(choiceText);
            ctT.position = Math::Vector3(-5.0f + ci * 5.0f, -4.8f, 1.3f);
            auto& ct = m_World->AddComponent<ECS::TextComponent>(choiceText);
            char optLabel[32]; snprintf(optLabel, sizeof(optLabel), "Option %d...", ci + 1);
            ct.text = optLabel;
            ct.fontSize = 22.0f;
            ct.textColor = Math::Vector3(0.8f, 0.8f, 0.9f);
        }

        // Main Menu UI canvas
        {
            ECS::Entity uiEntity = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(uiEntity, "Main Menu");
            m_World->AddComponent<ECS::TransformComponent>(uiEntity);
            m_World->AddComponent<GUI::UICanvasComponent>(uiEntity, GUI::UITemplates::CreateMainMenu("Visual Novel"));
        }
    }

    else if (templateId == "gamemanager") {
        createGround();

        // Game Manager entity (singleton-like entity that holds global game state)
        {
            ECS::Entity gm = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(gm, "GameManager");
            auto& gmT = m_World->AddComponent<ECS::TransformComponent>(gm);
            gmT.position = Math::Vector3(0.0f, 0.0f, 0.0f);
            auto& notes = m_World->AddComponent<ECS::NotesComponent>(gm);
            notes.notes = "GAME MANAGER PATTERN\n"
                "========================\n"
                "This entity acts as a global game state manager.\n"
                "Use the StateMachineComponent to track game states:\n"
                "  - MainMenu -> Playing -> Paused -> GameOver\n"
                "\n"
                "The TimerComponent tracks elapsed game time.\n"
                "Add a custom script to manage score, lives, and transitions.\n"
                "\n"
                "Game States:\n"
                "  MainMenu: Show title screen, wait for Start\n"
                "  Playing: Gameplay active, update score\n"
                "  Paused: Freeze gameplay, show pause menu\n"
                "  GameOver: Show results, option to restart\n";

            auto& sm = m_World->AddComponent<ECS::StateMachineComponent>(gm);
            sm.currentState = "MainMenu";

            // Define game states with transitions
            ECS::SMState mainMenuState;
            mainMenuState.name = "MainMenu";
            mainMenuState.onEnter = "OnMainMenuEnter";
            mainMenuState.onUpdate = "OnMainMenuUpdate";
            mainMenuState.onExit = "OnMainMenuExit";
            mainMenuState.editorPosition = Math::Vector2(100, 100);
            {
                ECS::SMTransition toPlaying;
                toPlaying.toState = "Playing";
                ECS::SMTransitionCondition startCond;
                startCond.paramName = "StartGame";
                startCond.type = ECS::SMConditionType::Trigger;
                toPlaying.conditions.push_back(startCond);
                mainMenuState.transitions.push_back(toPlaying);
            }
            sm.states.push_back(mainMenuState);

            ECS::SMState playingState;
            playingState.name = "Playing";
            playingState.onEnter = "OnPlayingEnter";
            playingState.onUpdate = "OnPlayingUpdate";
            playingState.onExit = "OnPlayingExit";
            playingState.editorPosition = Math::Vector2(350, 100);
            {
                ECS::SMTransition toPaused;
                toPaused.toState = "Paused";
                ECS::SMTransitionCondition pauseCond;
                pauseCond.paramName = "Pause";
                pauseCond.type = ECS::SMConditionType::Trigger;
                toPaused.conditions.push_back(pauseCond);
                playingState.transitions.push_back(toPaused);

                ECS::SMTransition toGameOver;
                toGameOver.toState = "GameOver";
                ECS::SMTransitionCondition healthCond;
                healthCond.paramName = "PlayerHealth";
                healthCond.type = ECS::SMConditionType::FloatLess;
                healthCond.threshold = 0.01f;
                toGameOver.conditions.push_back(healthCond);
                playingState.transitions.push_back(toGameOver);
            }
            sm.states.push_back(playingState);

            ECS::SMState pausedState;
            pausedState.name = "Paused";
            pausedState.onEnter = "OnPausedEnter";
            pausedState.onUpdate = "OnPausedUpdate";
            pausedState.onExit = "OnPausedExit";
            pausedState.editorPosition = Math::Vector2(350, 300);
            {
                ECS::SMTransition toResume;
                toResume.toState = "Playing";
                ECS::SMTransitionCondition resumeCond;
                resumeCond.paramName = "Resume";
                resumeCond.type = ECS::SMConditionType::Trigger;
                toResume.conditions.push_back(resumeCond);
                pausedState.transitions.push_back(toResume);

                ECS::SMTransition toMainMenu;
                toMainMenu.toState = "MainMenu";
                ECS::SMTransitionCondition quitCond;
                quitCond.paramName = "QuitToMenu";
                quitCond.type = ECS::SMConditionType::Trigger;
                toMainMenu.conditions.push_back(quitCond);
                pausedState.transitions.push_back(toMainMenu);
            }
            sm.states.push_back(pausedState);

            ECS::SMState gameOverState;
            gameOverState.name = "GameOver";
            gameOverState.onEnter = "OnGameOverEnter";
            gameOverState.onUpdate = "OnGameOverUpdate";
            gameOverState.editorPosition = Math::Vector2(600, 200);
            {
                ECS::SMTransition toRestart;
                toRestart.toState = "Playing";
                ECS::SMTransitionCondition restartCond;
                restartCond.paramName = "Restart";
                restartCond.type = ECS::SMConditionType::Trigger;
                toRestart.conditions.push_back(restartCond);
                gameOverState.transitions.push_back(toRestart);

                ECS::SMTransition toMenu;
                toMenu.toState = "MainMenu";
                ECS::SMTransitionCondition menuCond;
                menuCond.paramName = "QuitToMenu";
                menuCond.type = ECS::SMConditionType::Trigger;
                toMenu.conditions.push_back(menuCond);
                gameOverState.transitions.push_back(toMenu);
            }
            sm.states.push_back(gameOverState);

            // Initialize parameters
            sm.floatParams["PlayerHealth"] = 100.0f;
            sm.boolParams["IsGameActive"] = false;

            auto& timer = m_World->AddComponent<ECS::TimerComponent>(gm);
            timer.duration = 0.0f;  // Counts up
            timer.loop = true;
        }

        // Score Display
        {
            ECS::Entity scoreUI = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(scoreUI, "Score Display");
            auto& st = m_World->AddComponent<ECS::TransformComponent>(scoreUI);
            st.position = Math::Vector3(-7.0f, 4.0f, 5.0f);
            auto& text = m_World->AddComponent<ECS::TextComponent>(scoreUI);
            text.text = "Score: 0";
            text.fontSize = 40.0f;
            text.textColor = Math::Vector3(1.0f, 1.0f, 0.0f);
        }

        // Lives Display
        {
            ECS::Entity livesUI = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(livesUI, "Lives Display");
            auto& lt = m_World->AddComponent<ECS::TransformComponent>(livesUI);
            lt.position = Math::Vector3(5.0f, 4.0f, 5.0f);
            auto& text = m_World->AddComponent<ECS::TextComponent>(livesUI);
            text.text = "Lives: 3";
            text.fontSize = 40.0f;
            text.textColor = Math::Vector3(1.0f, 0.3f, 0.3f);
        }

        // Player with health
        {
            ECS::Entity player = createPlayer3D("Player");
            auto& health = m_World->AddComponent<ECS::HealthComponent>(player);
            health.maxHealth = 100.0f;
            health.currentHealth = 100.0f;
            auto& ctrl = m_World->AddComponent<ECS::ThirdPersonController>(player);
            ctrl.moveSpeed = 5.0f;
            SetupCameraForController(player, "ThirdPerson");
        }

        // Spawn Point
        {
            ECS::Entity spawn = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(spawn, "Player Spawn");
            auto& spT = m_World->AddComponent<ECS::TransformComponent>(spawn);
            spT.position = Math::Vector3(0.0f, 1.0f, 0.0f);
            m_World->AddComponent<ECS::SpawnPointComponent>(spawn);
        }

        // Collectible
        {
            ECS::Entity coin = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(coin, "Coin");
            auto& ct = m_World->AddComponent<ECS::TransformComponent>(coin);
            ct.position = Math::Vector3(3.0f, 1.0f, 3.0f);
            ct.scale = Math::Vector3(0.3f);
            m_World->AddComponent<ECS::MeshComponent>(coin, Renderer::MeshFactory::CreateSphere(0.5f));
            auto& cm = m_World->AddComponent<ECS::MaterialComponent>(coin);
            cm.baseColor = Math::Vector3(1.0f, 0.85f, 0.0f);
            cm.emissiveColor = Math::Vector3(1.0f, 0.7f, 0.0f);
            cm.emissiveStrength = 0.5f;
            auto& pc = m_World->AddComponent<ECS::PickupComponent>(coin);
            pc.type = ECS::PickupComponent::PickupType::Coin;
            pc.value = 100.0f;
        }

        // Enemy Spawner (spawn point for waves of enemies)
        {
            ECS::Entity spawner = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(spawner, "Enemy Spawner");
            auto& spt = m_World->AddComponent<ECS::TransformComponent>(spawner);
            spt.position = Math::Vector3(8.0f, 0.5f, -8.0f);
            spt.scale = Math::Vector3(1.0f);
            m_World->AddComponent<ECS::MeshComponent>(spawner, Renderer::MeshFactory::CreateSphere(0.5f));
            auto& spm = m_World->AddComponent<ECS::MaterialComponent>(spawner);
            spm.baseColor = Math::Vector3(0.7f, 0.1f, 0.1f);
            spm.emissiveColor = Math::Vector3(0.8f, 0.1f, 0.05f);
            spm.emissiveStrength = 0.6f;
            m_World->AddComponent<ECS::SpawnPointComponent>(spawner);
            auto& spTag = m_World->AddComponent<ECS::TagComponent>(spawner);
            spTag.tags.push_back("enemy_spawner");
            auto& spNotes = m_World->AddComponent<ECS::NotesComponent>(spawner);
            spNotes.notes = "Enemy spawn point — spawn enemies here on wave start.\n"
                "Use a script or VS to: read wave number, create N enemy entities,\n"
                "set their AI state to Patrol, increase difficulty each wave.";
        }

        // Sample Enemy (shows what spawned enemies look like)
        {
            ECS::Entity enemy = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(enemy, "Sample Enemy");
            auto& et = m_World->AddComponent<ECS::TransformComponent>(enemy);
            et.position = Math::Vector3(6.0f, 0.5f, -5.0f);
            m_World->AddComponent<ECS::MeshComponent>(enemy, Renderer::MeshFactory::CreateCapsule(0.3f, 0.8f));
            auto& em = m_World->AddComponent<ECS::MaterialComponent>(enemy);
            em.baseColor = Math::Vector3(0.8f, 0.15f, 0.1f);
            auto& eh = m_World->AddComponent<ECS::HealthComponent>(enemy);
            eh.maxHealth = 50.0f; eh.currentHealth = 50.0f;
            auto& eai = m_World->AddComponent<ECS::AIControllerComponent>(enemy);
            eai.currentState = ECS::AIControllerComponent::AIState::Patrol;
            eai.moveSpeed = 3.0f;
            auto& edm = m_World->AddComponent<ECS::DamageComponent>(enemy);
            edm.damage = 10.0f;
        }

        // Wave Counter Text
        {
            ECS::Entity waveUI = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(waveUI, "Wave Counter");
            auto& wt = m_World->AddComponent<ECS::TransformComponent>(waveUI);
            wt.position = Math::Vector3(0.0f, 4.0f, 5.0f);
            auto& wtext = m_World->AddComponent<ECS::TextComponent>(waveUI);
            wtext.text = "Wave: 1";
            wtext.fontSize = 48.0f;
            wtext.textColor = Math::Vector3(1.0f, 0.5f, 0.2f);
            auto& wTag = m_World->AddComponent<ECS::TagComponent>(waveUI);
            wTag.tags.push_back("wave_counter");
        }

        // Skybox: Midday
        {
            Renderer::SkyboxConfig skyConfig;
            skyConfig.type = Renderer::SkyboxType::Procedural;
            skyConfig.topColor = Math::Vector3(0.1f, 0.3f, 0.8f);
            skyConfig.horizonColor = Math::Vector3(0.5f, 0.7f, 1.0f);
            skyConfig.bottomColor = Math::Vector3(0.8f, 0.85f, 0.9f);
            skyConfig.sunDirection = Math::Vector3(0.0f, 1.0f, 0.0f);
            m_RenderSystem->SetSkybox(skyConfig);
        }

        // Render settings: shadows on, moderate ambient
        m_RenderSystem->SetShadowsEnabled(true);
        m_RenderSystem->SetAmbientIntensity(0.12f);

        // Post-processing: FXAA
        if (m_PostProcessing) {
            auto& pp = m_PostProcessing->GetSettings();
            pp.fxaaEnabled = 1;
        }

        // Main Menu UI canvas
        {
            ECS::Entity uiEntity = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(uiEntity, "Main Menu");
            m_World->AddComponent<ECS::TransformComponent>(uiEntity);
            m_World->AddComponent<GUI::UICanvasComponent>(uiEntity, GUI::UITemplates::CreateMainMenu("My Game"));
        }
    }

    else if (templateId == "citybuilder") {
        // Terrain Grid
        {
            ECS::Entity terrain = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(terrain, "City Terrain");
            auto& tt = m_World->AddComponent<ECS::TransformComponent>(terrain);
            tt.position = Math::Vector3(0.0f, 0.0f, 0.0f);
            tt.scale = Math::Vector3(60.0f, 0.2f, 60.0f);
            m_World->AddComponent<ECS::MeshComponent>(terrain, Renderer::MeshFactory::CreateCube(1.0f));
            auto& tm = m_World->AddComponent<ECS::MaterialComponent>(terrain);
            tm.baseColor = Math::Vector3(0.35f, 0.5f, 0.3f);
            tm.roughness = 0.9f;
            auto& col = m_World->AddComponent<ECS::BoxColliderComponent>(terrain);
            col.size = Math::Vector3(60.0f, 0.2f, 60.0f);
        }

        // Sun
        {
            ECS::Entity sun = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(sun, "Sun");
            auto& lt = m_World->AddComponent<ECS::TransformComponent>(sun);
            lt.position = Math::Vector3(0.0f, 20.0f, 10.0f);
            lt.rotation = Math::Quaternion(Math::Vector3(1, 0, 0), Math::Radians(-50.0f));
            auto& lc = m_World->AddComponent<ECS::LightComponent>(sun);
            lc.type = ECS::LightType::Directional;
            lc.intensity = 1.3f;
            lc.color = Math::Vector3(1.0f, 0.97f, 0.9f);
            lc.castShadows = true;
        }

        // Isometric City Camera
        {
            ECS::Entity cam = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(cam, "City Camera");
            auto& camT = m_World->AddComponent<ECS::TransformComponent>(cam);
            // Classic isometric angle: 30 degrees from horizontal, rotated 45 degrees on Y
            camT.position = Math::Vector3(20.0f, 20.0f, 20.0f);
            // Look toward origin with isometric angle
            camT.rotation = Math::Quaternion(Math::Vector3(1, 0, 0), Math::Radians(-35.0f))
                          * Math::Quaternion(Math::Vector3(0, 1, 0), Math::Radians(45.0f));
            auto& camC = m_World->AddComponent<ECS::CameraComponent>(cam);
            camC.projectionType = ECS::ProjectionType::Orthographic;
            camC.orthoSize = 15.0f;  // Adjustable zoom
            camC.nearPlane = 0.1f;
            camC.farPlane = 200.0f;
            m_SelectedGameCamera = cam;

            auto& notes = m_World->AddComponent<ECS::NotesComponent>(cam);
            notes.notes = "CITY CAMERA\n"
                "============\n"
                "Orthographic projection gives the classic isometric look.\n"
                "Adjust orthoSize to zoom in/out.\n"
                "Scroll input -> change orthoSize.\n"
                "WASD or arrow keys -> pan the camera.\n"
                "\n"
                "For faux-iso (2D look):\n"
                "  Enable retro flat shading on all building materials\n"
                "  Reduce orthoSize for tighter zoom\n"
                "  Consider enabling dithering in post-processing\n";
        }

        // Sample Buildings (small placeholder city)
        // Residential
        Math::Vector3 buildingPositions[] = {
            Math::Vector3(-4.0f, 0.0f, -4.0f),
            Math::Vector3(-4.0f, 0.0f, 0.0f),
            Math::Vector3(-4.0f, 0.0f, 4.0f),
            Math::Vector3(0.0f, 0.0f, -4.0f),
            Math::Vector3(4.0f, 0.0f, -4.0f),
            Math::Vector3(4.0f, 0.0f, 0.0f),
        };
        Math::Vector3 buildingScales[] = {
            Math::Vector3(1.5f, 2.0f, 1.5f),
            Math::Vector3(1.5f, 3.0f, 1.5f),
            Math::Vector3(1.5f, 1.5f, 1.5f),
            Math::Vector3(2.0f, 4.0f, 2.0f),
            Math::Vector3(1.8f, 2.5f, 1.8f),
            Math::Vector3(1.5f, 1.0f, 1.5f),
        };
        Math::Vector3 buildingColors[] = {
            Math::Vector3(0.6f, 0.55f, 0.5f),   // Beige house
            Math::Vector3(0.5f, 0.5f, 0.55f),    // Gray apartment
            Math::Vector3(0.55f, 0.45f, 0.4f),   // Brown house
            Math::Vector3(0.4f, 0.45f, 0.5f),    // Blue-gray office
            Math::Vector3(0.5f, 0.4f, 0.35f),    // Brick
            Math::Vector3(0.45f, 0.5f, 0.4f),    // Green shop
        };
        const char* buildingNames[] = {
            "House A", "Apartment", "House B",
            "Office Tower", "Store", "Workshop",
        };

        for (int i = 0; i < 6; ++i) {
            ECS::Entity bld = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(bld, buildingNames[i]);
            auto& bt = m_World->AddComponent<ECS::TransformComponent>(bld);
            bt.position = buildingPositions[i] + Math::Vector3(0.0f, buildingScales[i].y * 0.5f + 0.1f, 0.0f);
            bt.scale = buildingScales[i];
            m_World->AddComponent<ECS::MeshComponent>(bld, Renderer::MeshFactory::CreateCube(1.0f));
            auto& bm = m_World->AddComponent<ECS::MaterialComponent>(bld);
            bm.baseColor = buildingColors[i];
            bm.roughness = 0.8f;
        }

        // Road segments
        for (int i = -3; i <= 3; ++i) {
            ECS::Entity road = m_World->CreateEntity();
            char roadName[32];
            snprintf(roadName, sizeof(roadName), "Road Seg %d", i + 4);
            m_World->AddComponent<ECS::NameComponent>(road, roadName);
            auto& rt = m_World->AddComponent<ECS::TransformComponent>(road);
            rt.position = Math::Vector3(static_cast<f32>(i) * 4.0f, 0.11f, -8.0f);
            rt.scale = Math::Vector3(3.8f, 0.02f, 2.0f);
            m_World->AddComponent<ECS::MeshComponent>(road, Renderer::MeshFactory::CreateCube(1.0f));
            auto& rm = m_World->AddComponent<ECS::MaterialComponent>(road);
            rm.baseColor = Math::Vector3(0.2f, 0.2f, 0.22f);
            rm.roughness = 0.6f;
        }

        // Park / green space
        {
            ECS::Entity park = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(park, "Park");
            auto& pt = m_World->AddComponent<ECS::TransformComponent>(park);
            pt.position = Math::Vector3(0.0f, 0.11f, 4.0f);
            pt.scale = Math::Vector3(6.0f, 0.05f, 4.0f);
            m_World->AddComponent<ECS::MeshComponent>(park, Renderer::MeshFactory::CreateCube(1.0f));
            auto& pm = m_World->AddComponent<ECS::MaterialComponent>(park);
            pm.baseColor = Math::Vector3(0.25f, 0.55f, 0.2f);
            pm.roughness = 0.95f;
        }

        // Tree placeholders in park
        for (int i = 0; i < 4; ++i) {
            ECS::Entity tree = m_World->CreateEntity();
            char treeName[32];
            snprintf(treeName, sizeof(treeName), "Park Tree %d", i + 1);
            m_World->AddComponent<ECS::NameComponent>(tree, treeName);
            auto& tt = m_World->AddComponent<ECS::TransformComponent>(tree);
            f32 tx = -2.0f + static_cast<f32>(i % 2) * 4.0f;
            f32 tz = 3.0f + static_cast<f32>(i / 2) * 2.0f;
            tt.position = Math::Vector3(tx, 1.2f, tz);
            tt.scale = Math::Vector3(0.5f, 2.0f, 0.5f);
            m_World->AddComponent<ECS::MeshComponent>(tree, Renderer::MeshFactory::CreateCapsule(0.5f, 1.0f));
            auto& trm = m_World->AddComponent<ECS::MaterialComponent>(tree);
            trm.baseColor = Math::Vector3(0.2f, 0.6f, 0.15f);
        }

        // Game Notes
        {
            ECS::Entity notes = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(notes, "City Builder Notes");
            auto& nt = m_World->AddComponent<ECS::TransformComponent>(notes);
            nt.position = Math::Vector3(0.0f, 10.0f, 0.0f);
            auto& nc = m_World->AddComponent<ECS::NotesComponent>(notes);
            nc.notes = "CITY BUILDER TEMPLATE\n"
                "========================\n"
                "Camera: Orthographic isometric (45-degree Y, 35-degree X)\n"
                "All buildings are 3D cubes that look classic from this angle.\n"
                "\n"
                "Faux-Isometric Mode:\n"
                "  To get the classic 2D city builder look:\n"
                "  1. Set all materials to flatShading = true\n"
                "  2. Enable vertex snapping for PS1 jitter (optional)\n"
                "  3. Enable dithering in Post-Processing\n"
                "  4. Reduce orthoSize on camera for tighter zoom\n"
                "\n"
                "Grid System:\n"
                "  Buildings snap to a 4x4 grid.\n"
                "  To implement placement:\n"
                "  1. Raycast from mouse to ground plane\n"
                "  2. Snap hit position to nearest grid point\n"
                "  3. Check for collisions with existing buildings\n"
                "  4. Place building entity at snapped position\n"
                "\n"
                "Building Types (to implement):\n"
                "  Residential: generates population\n"
                "  Commercial: generates income, needs population\n"
                "  Industrial: provides jobs, generates pollution\n"
                "  Parks: increases happiness, reduces pollution\n"
                "  Roads: connects zones, required for buildings\n"
                "  Services: fire, police, hospital (radius-based coverage)\n";
        }

        // Skybox: Midday
        {
            Renderer::SkyboxConfig skyConfig;
            skyConfig.type = Renderer::SkyboxType::Procedural;
            skyConfig.topColor = Math::Vector3(0.1f, 0.3f, 0.8f);
            skyConfig.horizonColor = Math::Vector3(0.5f, 0.7f, 1.0f);
            skyConfig.bottomColor = Math::Vector3(0.8f, 0.85f, 0.9f);
            skyConfig.sunDirection = Math::Vector3(0.0f, 1.0f, 0.0f);
            m_RenderSystem->SetSkybox(skyConfig);
        }

        // Population / Income HUD labels
        {
            ECS::Entity popLabel = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(popLabel, "Population Label");
            auto& plt = m_World->AddComponent<ECS::TransformComponent>(popLabel);
            plt.position = Math::Vector3(-7.0f, 4.0f, 5.0f);
            auto& ptext = m_World->AddComponent<ECS::TextComponent>(popLabel);
            ptext.text = "Pop: 24";
            ptext.fontSize = 32.0f;
            ptext.textColor = Math::Vector3(1.0f, 1.0f, 1.0f);
            auto& ptag = m_World->AddComponent<ECS::TagComponent>(popLabel);
            ptag.tags.push_back("hud_population");
        }
        {
            ECS::Entity incLabel = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(incLabel, "Income Label");
            auto& ilt = m_World->AddComponent<ECS::TransformComponent>(incLabel);
            ilt.position = Math::Vector3(-1.0f, 4.0f, 5.0f);
            auto& itext = m_World->AddComponent<ECS::TextComponent>(incLabel);
            itext.text = "$150/min";
            itext.fontSize = 32.0f;
            itext.textColor = Math::Vector3(0.3f, 0.9f, 0.3f);
            auto& itag = m_World->AddComponent<ECS::TagComponent>(incLabel);
            itag.tags.push_back("hud_income");
        }
        {
            ECS::Entity fundsLabel = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(fundsLabel, "Funds Label");
            auto& flt = m_World->AddComponent<ECS::TransformComponent>(fundsLabel);
            flt.position = Math::Vector3(5.0f, 4.0f, 5.0f);
            auto& ftext = m_World->AddComponent<ECS::TextComponent>(fundsLabel);
            ftext.text = "$1,000";
            ftext.fontSize = 36.0f;
            ftext.textColor = Math::Vector3(1.0f, 0.85f, 0.0f);
            auto& ftag = m_World->AddComponent<ECS::TagComponent>(fundsLabel);
            ftag.tags.push_back("hud_funds");
        }

        m_RenderSystem->SetShadowsEnabled(true);
        m_RenderSystem->SetShadowDistance(200.0f);
        if (m_PostProcessing) {
            auto& pp = m_PostProcessing->GetSettings();
            pp.fxaaEnabled = 1;
        }
    }

    else if (templateId == "fpsarena") {
        // Arena floor
        {
            ECS::Entity ground = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(ground, "Arena Floor");
            auto& gt = m_World->AddComponent<ECS::TransformComponent>(ground);
            gt.scale = Math::Vector3(30.0f, 0.2f, 30.0f);
            gt.position = Math::Vector3(0.0f, -0.1f, 0.0f);
            auto& gm = m_World->AddComponent<ECS::MaterialComponent>(ground);
            gm.baseColor = Math::Vector3(0.3f, 0.3f, 0.32f);
            gm.roughness = 0.7f;
            m_World->AddComponent<ECS::MeshComponent>(ground, Renderer::MeshFactory::CreateCube(1.0f));
            auto& col = m_World->AddComponent<ECS::BoxColliderComponent>(ground);
            col.size = Math::Vector3(30.0f, 0.2f, 30.0f);
        }
        createLight();

        // Player (FPS)
        ECS::Entity player;
        {
            player = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(player, "Player");
            auto& pt = m_World->AddComponent<ECS::TransformComponent>(player);
            pt.position = Math::Vector3(0.0f, 1.7f, -10.0f);
            m_World->AddComponent<ECS::MeshComponent>(player, Renderer::MeshFactory::CreateCapsule(0.3f, 1.0f));
            auto& pm = m_World->AddComponent<ECS::MaterialComponent>(player);
            pm.baseColor = Math::Vector3(0.2f, 0.4f, 0.7f);
            auto& health = m_World->AddComponent<ECS::HealthComponent>(player);
            health.maxHealth = 100.0f;
            health.currentHealth = 100.0f;
            auto& inv = m_World->AddComponent<ECS::InventoryComponent>(player);
            (void)inv;
            auto& ctrl = m_World->AddComponent<ECS::FirstPersonController>(player);
            ctrl.moveSpeed = 7.0f;
            ctrl.mouseSensitivity = 0.15f;
            ctrl.sprintMultiplier = 1.5f;
            SetupCameraForController(player, "FirstPerson");
            m_World->AddComponent<ECS::AudioListenerComponent>(player);
        }

        // Weapon HUD
        {
            ECS::Entity hud = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(hud, "Ammo Display");
            auto& ht = m_World->AddComponent<ECS::TransformComponent>(hud);
            ht.position = Math::Vector3(6.0f, -4.0f, 5.0f);
            auto& text = m_World->AddComponent<ECS::TextComponent>(hud);
            text.text = "Ammo: 30 / 90";
            text.fontSize = 32.0f;
            text.textColor = Math::Vector3(1.0f, 1.0f, 1.0f);
        }

        // Health HUD
        {
            ECS::Entity hpHud = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(hpHud, "Health Display");
            auto& ht = m_World->AddComponent<ECS::TransformComponent>(hpHud);
            ht.position = Math::Vector3(-7.0f, -4.0f, 5.0f);
            auto& text = m_World->AddComponent<ECS::TextComponent>(hpHud);
            text.text = "HP: 100";
            text.fontSize = 32.0f;
            text.textColor = Math::Vector3(0.2f, 1.0f, 0.3f);
        }

        // Spawn Points
        Math::Vector3 spawnPositions[] = {
            Math::Vector3(-10.0f, 1.0f, -10.0f), Math::Vector3(10.0f, 1.0f, -10.0f),
            Math::Vector3(-10.0f, 1.0f, 10.0f),  Math::Vector3(10.0f, 1.0f, 10.0f),
        };
        for (int i = 0; i < 4; ++i) {
            ECS::Entity sp = m_World->CreateEntity();
            char name[32]; snprintf(name, sizeof(name), "Spawn Point %d", i + 1);
            m_World->AddComponent<ECS::NameComponent>(sp, name);
            auto& st = m_World->AddComponent<ECS::TransformComponent>(sp);
            st.position = spawnPositions[i];
            m_World->AddComponent<ECS::SpawnPointComponent>(sp);
        }

        // Weapon Pickups
        Math::Vector3 weaponPositions[] = {
            Math::Vector3(-5.0f, 0.5f, 0.0f), Math::Vector3(5.0f, 0.5f, 0.0f),
            Math::Vector3(0.0f, 0.5f, 5.0f),
        };
        const char* weaponNames[] = { "Shotgun Pickup", "Rifle Pickup", "Rocket Pickup" };
        Math::Vector3 weaponColors[] = {
            Math::Vector3(0.8f, 0.5f, 0.2f), Math::Vector3(0.3f, 0.6f, 0.3f), Math::Vector3(0.7f, 0.2f, 0.2f),
        };
        for (int i = 0; i < 3; ++i) {
            ECS::Entity wp = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(wp, weaponNames[i]);
            auto& wt = m_World->AddComponent<ECS::TransformComponent>(wp);
            wt.position = weaponPositions[i];
            wt.scale = Math::Vector3(0.4f, 0.2f, 0.8f);
            m_World->AddComponent<ECS::MeshComponent>(wp, Renderer::MeshFactory::CreateCube(1.0f));
            auto& wm = m_World->AddComponent<ECS::MaterialComponent>(wp);
            wm.baseColor = weaponColors[i];
            wm.emissiveColor = weaponColors[i];
            wm.emissiveStrength = 0.3f;
            auto& pc = m_World->AddComponent<ECS::PickupComponent>(wp);
            pc.type = ECS::PickupComponent::PickupType::Powerup;
            pc.value = 1.0f;
            pc.pickupRange = 1.5f;
        }

        // Health Packs
        for (int i = 0; i < 2; ++i) {
            ECS::Entity hp = m_World->CreateEntity();
            char name[32]; snprintf(name, sizeof(name), "Health Pack %d", i + 1);
            m_World->AddComponent<ECS::NameComponent>(hp, name);
            auto& ht = m_World->AddComponent<ECS::TransformComponent>(hp);
            ht.position = Math::Vector3(i == 0 ? -8.0f : 8.0f, 0.3f, 8.0f);
            ht.scale = Math::Vector3(0.4f);
            m_World->AddComponent<ECS::MeshComponent>(hp, Renderer::MeshFactory::CreateCube(1.0f));
            auto& hm = m_World->AddComponent<ECS::MaterialComponent>(hp);
            hm.baseColor = Math::Vector3(1.0f, 1.0f, 1.0f);
            hm.emissiveColor = Math::Vector3(0.1f, 0.8f, 0.1f);
            hm.emissiveStrength = 0.5f;
            auto& pc = m_World->AddComponent<ECS::PickupComponent>(hp);
            pc.type = ECS::PickupComponent::PickupType::Health;
            pc.value = 50.0f;
        }

        // Ammo Crates
        for (int i = 0; i < 3; ++i) {
            ECS::Entity ammo = m_World->CreateEntity();
            char name[32]; snprintf(name, sizeof(name), "Ammo Crate %d", i + 1);
            m_World->AddComponent<ECS::NameComponent>(ammo, name);
            auto& at = m_World->AddComponent<ECS::TransformComponent>(ammo);
            at.position = Math::Vector3(-4.0f + i * 4.0f, 0.25f, -5.0f);
            at.scale = Math::Vector3(0.3f);
            m_World->AddComponent<ECS::MeshComponent>(ammo, Renderer::MeshFactory::CreateCube(1.0f));
            auto& am = m_World->AddComponent<ECS::MaterialComponent>(ammo);
            am.baseColor = Math::Vector3(0.6f, 0.5f, 0.2f);
            auto& pc = m_World->AddComponent<ECS::PickupComponent>(ammo);
            pc.type = ECS::PickupComponent::PickupType::Ammo;
            pc.value = 30.0f;
        }

        // Cover walls
        Math::Vector3 wallPositions[] = {
            Math::Vector3(-5.0f, 1.0f, -5.0f), Math::Vector3(5.0f, 1.0f, -5.0f),
            Math::Vector3(0.0f, 1.0f, 5.0f), Math::Vector3(-8.0f, 1.0f, 3.0f),
        };
        for (int i = 0; i < 4; ++i) {
            ECS::Entity wall = m_World->CreateEntity();
            char name[32]; snprintf(name, sizeof(name), "Cover Wall %d", i + 1);
            m_World->AddComponent<ECS::NameComponent>(wall, name);
            auto& wt = m_World->AddComponent<ECS::TransformComponent>(wall);
            wt.position = wallPositions[i];
            wt.scale = Math::Vector3(3.0f, 2.0f, 0.3f);
            if (i % 2 == 1) wt.rotation = Math::Quaternion(Math::Vector3(0, 1, 0), Math::Radians(90.0f));
            m_World->AddComponent<ECS::MeshComponent>(wall, Renderer::MeshFactory::CreateCube(1.0f));
            auto& wm = m_World->AddComponent<ECS::MaterialComponent>(wall);
            wm.baseColor = Math::Vector3(0.4f, 0.38f, 0.35f);
            auto& col = m_World->AddComponent<ECS::BoxColliderComponent>(wall);
            col.size = Math::Vector3(3.0f, 2.0f, 0.3f);
        }

        // Enemy bots
        for (int i = 0; i < 3; ++i) {
            ECS::Entity bot = m_World->CreateEntity();
            char name[32]; snprintf(name, sizeof(name), "Enemy Bot %d", i + 1);
            m_World->AddComponent<ECS::NameComponent>(bot, name);
            auto& bt = m_World->AddComponent<ECS::TransformComponent>(bot);
            bt.position = Math::Vector3(-6.0f + i * 6.0f, 0.5f, 8.0f);
            m_World->AddComponent<ECS::MeshComponent>(bot, Renderer::MeshFactory::CreateCapsule(0.3f, 1.0f));
            auto& bm = m_World->AddComponent<ECS::MaterialComponent>(bot);
            bm.baseColor = Math::Vector3(0.8f, 0.15f, 0.1f);
            auto& bh = m_World->AddComponent<ECS::HealthComponent>(bot);
            bh.maxHealth = 80.0f; bh.currentHealth = 80.0f;
            auto& ai = m_World->AddComponent<ECS::AIControllerComponent>(bot);
            ai.currentState = ECS::AIControllerComponent::AIState::Patrol;
            ai.moveSpeed = 4.0f;
            auto& dmg = m_World->AddComponent<ECS::DamageComponent>(bot);
            dmg.damage = 15.0f;
        }
        // Skybox: Midday
        {
            Renderer::SkyboxConfig skyConfig;
            skyConfig.type = Renderer::SkyboxType::Procedural;
            skyConfig.topColor = Math::Vector3(0.1f, 0.3f, 0.8f);
            skyConfig.horizonColor = Math::Vector3(0.5f, 0.7f, 1.0f);
            skyConfig.bottomColor = Math::Vector3(0.8f, 0.85f, 0.9f);
            skyConfig.sunDirection = Math::Vector3(0.0f, 1.0f, 0.0f);
            m_RenderSystem->SetSkybox(skyConfig);
        }
        // Render settings: shadows
        m_RenderSystem->SetShadowsEnabled(true);
        // Post-processing: FXAA, vignette
        if (m_PostProcessing) {
            auto& pp = m_PostProcessing->GetSettings();
            pp.fxaaEnabled = 1;
            pp.vignetteEnabled = 1;
            pp.vignetteIntensity = 0.15f;
        }
        // Crosshair HUD entity (centered on screen)
        {
            ECS::Entity crosshair = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(crosshair, "Crosshair");
            auto& cht = m_World->AddComponent<ECS::TransformComponent>(crosshair);
            cht.position = Math::Vector3(0.0f, 0.0f, 5.0f);
            auto& chText = m_World->AddComponent<ECS::TextComponent>(crosshair);
            chText.text = "+";
            chText.fontSize = 28.0f;
            chText.textColor = Math::Vector3(1.0f, 1.0f, 1.0f);
            auto& chTag = m_World->AddComponent<ECS::TagComponent>(crosshair);
            chTag.tags.push_back("crosshair");
            auto& notes = m_World->AddComponent<ECS::NotesComponent>(crosshair);
            notes.notes = "CROSSHAIR\nReplace with a UICanvas image for a proper crosshair sprite.\n"
                "Or use a small quad with an alpha texture.";
        }

        // Kill Feed text
        {
            ECS::Entity killFeed = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(killFeed, "Kill Feed");
            auto& kft = m_World->AddComponent<ECS::TransformComponent>(killFeed);
            kft.position = Math::Vector3(5.0f, 3.5f, 5.0f);
            auto& kfText = m_World->AddComponent<ECS::TextComponent>(killFeed);
            kfText.text = "";
            kfText.fontSize = 20.0f;
            kfText.textColor = Math::Vector3(0.9f, 0.9f, 0.9f);
            auto& kfTag = m_World->AddComponent<ECS::TagComponent>(killFeed);
            kfTag.tags.push_back("kill_feed");
        }

        // Pause Menu UI canvas
        {
            ECS::Entity uiEntity = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(uiEntity, "Pause Menu");
            m_World->AddComponent<ECS::TransformComponent>(uiEntity);
            m_World->AddComponent<GUI::UICanvasComponent>(uiEntity, GUI::UITemplates::CreatePauseMenu());
        }
        // Collision groups
        {
            auto& groups = m_SceneManager.GetCollisionGroupNames();
            groups[1] = "Players";
            groups[2] = "Projectiles";
            groups[3] = "Environment";
        }
    }

    else if (templateId == "teamsports") {
        // Field
        {
            ECS::Entity field = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(field, "Playing Field");
            auto& ft = m_World->AddComponent<ECS::TransformComponent>(field);
            ft.scale = Math::Vector3(40.0f, 0.1f, 25.0f);
            ft.position = Math::Vector3(0.0f, -0.05f, 0.0f);
            auto& fm = m_World->AddComponent<ECS::MaterialComponent>(field);
            fm.baseColor = Math::Vector3(0.2f, 0.55f, 0.15f);
            fm.roughness = 0.95f;
            m_World->AddComponent<ECS::MeshComponent>(field, Renderer::MeshFactory::CreateCube(1.0f));
            auto& col = m_World->AddComponent<ECS::BoxColliderComponent>(field);
            col.size = Math::Vector3(40.0f, 0.1f, 25.0f);
        }
        createLight();

        // Goals (two trigger zones)
        for (int side = 0; side < 2; ++side) {
            ECS::Entity goal = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(goal, side == 0 ? "Goal Left" : "Goal Right");
            auto& gt = m_World->AddComponent<ECS::TransformComponent>(goal);
            gt.position = Math::Vector3(side == 0 ? -20.0f : 20.0f, 1.5f, 0.0f);
            gt.scale = Math::Vector3(0.3f, 3.0f, 6.0f);
            m_World->AddComponent<ECS::MeshComponent>(goal, Renderer::MeshFactory::CreateCube(1.0f));
            auto& gm = m_World->AddComponent<ECS::MaterialComponent>(goal);
            gm.baseColor = Math::Vector3(1.0f, 1.0f, 1.0f);
            gm.opacity = 0.3f;
            gm.alphaMode = ECS::MaterialComponent::AlphaMode::Blend;
            auto& trigger = m_World->AddComponent<ECS::TriggerZoneComponent>(goal);
            trigger.shape = ECS::TriggerZoneComponent::Shape::Box;
            trigger.boxSize = Math::Vector3(0.3f, 3.0f, 6.0f);
            auto& tag = m_World->AddComponent<ECS::TagComponent>(goal);
            tag.tags.push_back(side == 0 ? "goal_team_b" : "goal_team_a");
        }

        // Ball
        {
            ECS::Entity ball = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(ball, "Ball");
            auto& bt = m_World->AddComponent<ECS::TransformComponent>(ball);
            bt.position = Math::Vector3(0.0f, 0.5f, 0.0f);
            bt.scale = Math::Vector3(0.4f);
            m_World->AddComponent<ECS::MeshComponent>(ball, Renderer::MeshFactory::CreateSphere(0.5f));
            auto& bm = m_World->AddComponent<ECS::MaterialComponent>(ball);
            bm.baseColor = Math::Vector3(1.0f, 1.0f, 1.0f);
            bm.roughness = 0.4f;
            auto& rb = m_World->AddComponent<ECS::RigidbodyComponent>(ball);
            rb.mass = 0.4f;
            rb.drag = 0.3f;
            auto& sc = m_World->AddComponent<ECS::SphereColliderComponent>(ball);
            sc.radius = 0.2f;
            auto& tag = m_World->AddComponent<ECS::TagComponent>(ball);
            tag.tags.push_back("ball");
        }

        // Team A (blue) - 3 players + 1 goalie
        Math::Vector3 teamAPositions[] = {
            Math::Vector3(-15.0f, 0.5f, 0.0f),  // Goalie
            Math::Vector3(-8.0f, 0.5f, -4.0f),
            Math::Vector3(-8.0f, 0.5f, 4.0f),
            Math::Vector3(-3.0f, 0.5f, 0.0f),   // Forward
        };
        for (int i = 0; i < 4; ++i) {
            ECS::Entity p = m_World->CreateEntity();
            char name[32]; snprintf(name, sizeof(name), "Team A - %s", i == 0 ? "Goalie" : (i == 3 ? "Forward" : "Defender"));
            m_World->AddComponent<ECS::NameComponent>(p, name);
            auto& pt = m_World->AddComponent<ECS::TransformComponent>(p);
            pt.position = teamAPositions[i];
            m_World->AddComponent<ECS::MeshComponent>(p, Renderer::MeshFactory::CreateCapsule(0.3f, 0.9f));
            auto& pm = m_World->AddComponent<ECS::MaterialComponent>(p);
            pm.baseColor = Math::Vector3(0.15f, 0.3f, 0.85f);
            auto& tag = m_World->AddComponent<ECS::TagComponent>(p);
            tag.tags.push_back("team_a");
            if (i == 3) {
                // Player-controlled forward
                auto& ctrl = m_World->AddComponent<ECS::TopDown3DController>(p);
                ctrl.moveSpeed = 7.0f;
                SetupCameraForController(p, "TopDown3D");
            } else {
                auto& ai = m_World->AddComponent<ECS::AIControllerComponent>(p);
                ai.currentState = ECS::AIControllerComponent::AIState::Patrol;
                ai.moveSpeed = 5.0f;
            }
        }

        // Team B (red) - 4 AI players
        Math::Vector3 teamBPositions[] = {
            Math::Vector3(15.0f, 0.5f, 0.0f),
            Math::Vector3(8.0f, 0.5f, -4.0f),
            Math::Vector3(8.0f, 0.5f, 4.0f),
            Math::Vector3(3.0f, 0.5f, 0.0f),
        };
        for (int i = 0; i < 4; ++i) {
            ECS::Entity p = m_World->CreateEntity();
            char name[32]; snprintf(name, sizeof(name), "Team B - %s", i == 0 ? "Goalie" : (i == 3 ? "Forward" : "Defender"));
            m_World->AddComponent<ECS::NameComponent>(p, name);
            auto& pt = m_World->AddComponent<ECS::TransformComponent>(p);
            pt.position = teamBPositions[i];
            m_World->AddComponent<ECS::MeshComponent>(p, Renderer::MeshFactory::CreateCapsule(0.3f, 0.9f));
            auto& pm = m_World->AddComponent<ECS::MaterialComponent>(p);
            pm.baseColor = Math::Vector3(0.85f, 0.15f, 0.15f);
            auto& tag = m_World->AddComponent<ECS::TagComponent>(p);
            tag.tags.push_back("team_b");
            auto& ai = m_World->AddComponent<ECS::AIControllerComponent>(p);
            ai.currentState = ECS::AIControllerComponent::AIState::Patrol;
            ai.moveSpeed = 5.0f;
        }

        // Scoreboard
        {
            ECS::Entity score = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(score, "Scoreboard");
            auto& st = m_World->AddComponent<ECS::TransformComponent>(score);
            st.position = Math::Vector3(0.0f, 6.0f, -13.0f);
            auto& text = m_World->AddComponent<ECS::TextComponent>(score);
            text.text = "Team A  0 - 0  Team B";
            text.fontSize = 48.0f;
            text.textColor = Math::Vector3(1.0f, 1.0f, 1.0f);
        }

        // Timer
        {
            ECS::Entity timer = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(timer, "Match Timer");
            auto& tt = m_World->AddComponent<ECS::TransformComponent>(timer);
            tt.position = Math::Vector3(0.0f, 5.0f, -13.0f);
            auto& text = m_World->AddComponent<ECS::TextComponent>(timer);
            text.text = "5:00";
            text.fontSize = 36.0f;
            text.textColor = Math::Vector3(1.0f, 0.9f, 0.3f);
            auto& tc = m_World->AddComponent<ECS::TimerComponent>(timer);
            tc.duration = 300.0f;
        }

        // Center Circle (ground marking)
        {
            ECS::Entity circle = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(circle, "Center Circle");
            auto& cct = m_World->AddComponent<ECS::TransformComponent>(circle);
            cct.position = Math::Vector3(0.0f, 0.01f, 0.0f);
            cct.scale = Math::Vector3(6.0f, 0.01f, 6.0f);
            m_World->AddComponent<ECS::MeshComponent>(circle, Renderer::MeshFactory::CreateCube(1.0f));
            auto& ccm = m_World->AddComponent<ECS::MaterialComponent>(circle);
            ccm.baseColor = Math::Vector3(0.9f, 0.9f, 0.9f);
            ccm.opacity = 0.3f;
            ccm.alphaMode = ECS::MaterialComponent::AlphaMode::Blend;
        }

        // Center line (midfield marking)
        {
            ECS::Entity midline = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(midline, "Center Line");
            auto& mlt = m_World->AddComponent<ECS::TransformComponent>(midline);
            mlt.position = Math::Vector3(0.0f, 0.01f, 0.0f);
            mlt.scale = Math::Vector3(0.15f, 0.01f, 25.0f);
            m_World->AddComponent<ECS::MeshComponent>(midline, Renderer::MeshFactory::CreateCube(1.0f));
            auto& mlm = m_World->AddComponent<ECS::MaterialComponent>(midline);
            mlm.baseColor = Math::Vector3(1.0f, 1.0f, 1.0f);
            mlm.opacity = 0.5f;
            mlm.alphaMode = ECS::MaterialComponent::AlphaMode::Blend;
        }

        // Field boundary walls
        Math::Vector3 boundaryPos[] = {
            Math::Vector3(0.0f, 1.0f, -12.5f), Math::Vector3(0.0f, 1.0f, 12.5f),
        };
        for (int i = 0; i < 2; ++i) {
            ECS::Entity wall = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(wall, i == 0 ? "Wall North" : "Wall South");
            auto& wt = m_World->AddComponent<ECS::TransformComponent>(wall);
            wt.position = boundaryPos[i];
            wt.scale = Math::Vector3(40.0f, 2.0f, 0.2f);
            m_World->AddComponent<ECS::MeshComponent>(wall, Renderer::MeshFactory::CreateCube(1.0f));
            auto& wm = m_World->AddComponent<ECS::MaterialComponent>(wall);
            wm.baseColor = Math::Vector3(0.3f, 0.3f, 0.3f);
            wm.opacity = 0.2f;
            wm.alphaMode = ECS::MaterialComponent::AlphaMode::Blend;
            auto& col = m_World->AddComponent<ECS::BoxColliderComponent>(wall);
            col.size = Math::Vector3(40.0f, 2.0f, 0.2f);
        }

        // Skybox: Midday
        {
            Renderer::SkyboxConfig skyConfig;
            skyConfig.type = Renderer::SkyboxType::Procedural;
            skyConfig.topColor = Math::Vector3(0.1f, 0.3f, 0.8f);
            skyConfig.horizonColor = Math::Vector3(0.5f, 0.7f, 1.0f);
            skyConfig.bottomColor = Math::Vector3(0.8f, 0.85f, 0.9f);
            skyConfig.sunDirection = Math::Vector3(0.0f, 1.0f, 0.0f);
            m_RenderSystem->SetSkybox(skyConfig);
        }

        m_RenderSystem->SetShadowsEnabled(true);
        m_RenderSystem->SetAmbientIntensity(0.15f);
        if (m_PostProcessing) {
            auto& pp = m_PostProcessing->GetSettings();
            pp.fxaaEnabled = 1;
        }

        {
            auto& groups = m_SceneManager.GetCollisionGroupNames();
            groups[1] = "TeamA";
            groups[2] = "TeamB";
            groups[3] = "Ball";
            groups[4] = "Goals";
        }
    }

    else if (templateId == "towerdefense") {
        // Ground grid
        {
            ECS::Entity ground = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(ground, "TD Ground");
            auto& gt = m_World->AddComponent<ECS::TransformComponent>(ground);
            gt.scale = Math::Vector3(30.0f, 0.1f, 20.0f);
            gt.position = Math::Vector3(0.0f, -0.05f, 0.0f);
            auto& gm = m_World->AddComponent<ECS::MaterialComponent>(ground);
            gm.baseColor = Math::Vector3(0.35f, 0.5f, 0.3f);
            gm.roughness = 0.9f;
            m_World->AddComponent<ECS::MeshComponent>(ground, Renderer::MeshFactory::CreateCube(1.0f));
            auto& col = m_World->AddComponent<ECS::BoxColliderComponent>(ground);
            col.size = Math::Vector3(30.0f, 0.1f, 20.0f);
        }

        // Isometric camera
        {
            ECS::Entity cam = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(cam, "TD Camera");
            auto& camT = m_World->AddComponent<ECS::TransformComponent>(cam);
            camT.position = Math::Vector3(0.0f, 25.0f, 15.0f);
            camT.rotation = Math::Quaternion(Math::Vector3(1, 0, 0), Math::Radians(-55.0f));
            auto& camC = m_World->AddComponent<ECS::CameraComponent>(cam);
            camC.projectionType = ECS::ProjectionType::Orthographic;
            camC.orthoSize = 12.0f;
            m_SelectedGameCamera = cam;
        }
        createLight();

        // Enemy path waypoints (L-shaped path)
        Math::Vector3 pathPoints[] = {
            Math::Vector3(-12.0f, 0.1f, -8.0f),
            Math::Vector3(-12.0f, 0.1f, 0.0f),
            Math::Vector3(-5.0f, 0.1f, 0.0f),
            Math::Vector3(-5.0f, 0.1f, 6.0f),
            Math::Vector3(5.0f, 0.1f, 6.0f),
            Math::Vector3(5.0f, 0.1f, 0.0f),
            Math::Vector3(12.0f, 0.1f, 0.0f),
            Math::Vector3(12.0f, 0.1f, -8.0f),
        };
        // Draw path as a visible road
        for (int i = 0; i < 8; ++i) {
            ECS::Entity wp = m_World->CreateEntity();
            char wpName[32]; snprintf(wpName, sizeof(wpName), "Path Point %d", i + 1);
            m_World->AddComponent<ECS::NameComponent>(wp, wpName);
            auto& wt = m_World->AddComponent<ECS::TransformComponent>(wp);
            wt.position = pathPoints[i];
            wt.scale = Math::Vector3(0.3f);
            m_World->AddComponent<ECS::MeshComponent>(wp, Renderer::MeshFactory::CreateSphere(0.5f));
            auto& wm = m_World->AddComponent<ECS::MaterialComponent>(wp);
            wm.baseColor = Math::Vector3(0.9f, 0.4f, 0.1f);
            wm.emissiveColor = Math::Vector3(0.9f, 0.4f, 0.1f);
            wm.emissiveStrength = 0.2f;
            auto& waypoint = m_World->AddComponent<ECS::WaypointComponent>(wp);
            waypoint.index = i;
        }

        // Path road segments between waypoints
        for (int i = 0; i < 7; ++i) {
            Math::Vector3 a = pathPoints[i], b = pathPoints[i + 1];
            Math::Vector3 mid = (a + b) * 0.5f;
            Math::Vector3 diff = b - a;
            f32 len = diff.Length();
            ECS::Entity road = m_World->CreateEntity();
            char rname[32]; snprintf(rname, sizeof(rname), "Path Seg %d", i + 1);
            m_World->AddComponent<ECS::NameComponent>(road, rname);
            auto& rt = m_World->AddComponent<ECS::TransformComponent>(road);
            rt.position = mid;
            bool horizontal = Math::Abs(diff.x) > Math::Abs(diff.z);
            rt.scale = horizontal ? Math::Vector3(len, 0.02f, 2.0f) : Math::Vector3(2.0f, 0.02f, len);
            m_World->AddComponent<ECS::MeshComponent>(road, Renderer::MeshFactory::CreateCube(1.0f));
            auto& rm = m_World->AddComponent<ECS::MaterialComponent>(road);
            rm.baseColor = Math::Vector3(0.45f, 0.4f, 0.3f);
        }

        // Turret Placement Slots (positions adjacent to path)
        Math::Vector3 turretSlots[] = {
            Math::Vector3(-12.0f, 0.1f, 3.0f), Math::Vector3(-8.0f, 0.1f, 0.0f),
            Math::Vector3(-5.0f, 0.1f, 3.0f),  Math::Vector3(0.0f, 0.1f, 6.0f),
            Math::Vector3(5.0f, 0.1f, 3.0f),   Math::Vector3(8.0f, 0.1f, 0.0f),
        };
        for (int i = 0; i < 6; ++i) {
            ECS::Entity slot = m_World->CreateEntity();
            char sname[32]; snprintf(sname, sizeof(sname), "Turret Slot %d", i + 1);
            m_World->AddComponent<ECS::NameComponent>(slot, sname);
            auto& st = m_World->AddComponent<ECS::TransformComponent>(slot);
            st.position = turretSlots[i];
            st.scale = Math::Vector3(1.5f, 0.3f, 1.5f);
            m_World->AddComponent<ECS::MeshComponent>(slot, Renderer::MeshFactory::CreateCube(1.0f));
            auto& sm = m_World->AddComponent<ECS::MaterialComponent>(slot);
            sm.baseColor = Math::Vector3(0.5f, 0.5f, 0.55f);
            sm.roughness = 0.5f;
            auto& interact = m_World->AddComponent<ECS::InteractableComponent>(slot);
            interact.interactionRange = 3.0f;
            interact.promptText = "Build Turret ($50)";
            auto& tag = m_World->AddComponent<ECS::TagComponent>(slot);
            tag.tags.push_back("turret_slot");
        }

        // Sample Turret on Slot 1 (shows what a built turret looks like)
        {
            ECS::Entity turret = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(turret, "Arrow Turret");
            auto& trt = m_World->AddComponent<ECS::TransformComponent>(turret);
            trt.position = Math::Vector3(-12.0f, 0.6f, 3.0f);  // On top of Turret Slot 1
            trt.scale = Math::Vector3(0.6f, 1.0f, 0.6f);
            m_World->AddComponent<ECS::MeshComponent>(turret, Renderer::MeshFactory::CreateCube(1.0f));
            auto& trm = m_World->AddComponent<ECS::MaterialComponent>(turret);
            trm.baseColor = Math::Vector3(0.4f, 0.35f, 0.6f);
            trm.roughness = 0.5f;
            auto& trTag = m_World->AddComponent<ECS::TagComponent>(turret);
            trTag.tags.push_back("turret");
            trTag.tags.push_back("arrow_turret");
            auto& trdmg = m_World->AddComponent<ECS::DamageComponent>(turret);
            trdmg.damage = 15.0f;
        }
        // Turret barrel (visual detail)
        {
            ECS::Entity barrel = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(barrel, "Turret Barrel");
            auto& bt = m_World->AddComponent<ECS::TransformComponent>(barrel);
            bt.position = Math::Vector3(-12.0f, 0.9f, 2.2f);
            bt.scale = Math::Vector3(0.15f, 0.15f, 0.8f);
            m_World->AddComponent<ECS::MeshComponent>(barrel, Renderer::MeshFactory::CreateCube(1.0f));
            auto& bm = m_World->AddComponent<ECS::MaterialComponent>(barrel);
            bm.baseColor = Math::Vector3(0.3f, 0.3f, 0.5f);
            bm.metallic = 0.6f;
        }

        // Sample Enemy on path (shows what enemies look like)
        {
            ECS::Entity enemy = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(enemy, "Sample Creep");
            auto& et = m_World->AddComponent<ECS::TransformComponent>(enemy);
            et.position = Math::Vector3(-12.0f, 0.4f, -4.0f);  // On path between waypoints 1-2
            et.scale = Math::Vector3(0.5f);
            m_World->AddComponent<ECS::MeshComponent>(enemy, Renderer::MeshFactory::CreateSphere(0.5f));
            auto& em = m_World->AddComponent<ECS::MaterialComponent>(enemy);
            em.baseColor = Math::Vector3(0.7f, 0.2f, 0.15f);
            auto& eh = m_World->AddComponent<ECS::HealthComponent>(enemy);
            eh.maxHealth = 30.0f; eh.currentHealth = 30.0f;
            auto& eai = m_World->AddComponent<ECS::AIControllerComponent>(enemy);
            eai.currentState = ECS::AIControllerComponent::AIState::Patrol;
            eai.moveSpeed = 2.5f;
            auto& eTag = m_World->AddComponent<ECS::TagComponent>(enemy);
            eTag.tags.push_back("enemy");
        }

        // Spawn portal (where enemies come from)
        {
            ECS::Entity portal = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(portal, "Enemy Spawn Portal");
            auto& pt = m_World->AddComponent<ECS::TransformComponent>(portal);
            pt.position = Math::Vector3(-12.0f, 1.0f, -8.0f);
            pt.scale = Math::Vector3(1.5f);
            m_World->AddComponent<ECS::MeshComponent>(portal, Renderer::MeshFactory::CreateSphere(0.5f));
            auto& pm = m_World->AddComponent<ECS::MaterialComponent>(portal);
            pm.baseColor = Math::Vector3(0.8f, 0.1f, 0.1f);
            pm.emissiveColor = Math::Vector3(0.8f, 0.1f, 0.1f);
            pm.emissiveStrength = 1.0f;
        }

        // Base (what enemies attack)
        {
            ECS::Entity base = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(base, "Player Base");
            auto& bt = m_World->AddComponent<ECS::TransformComponent>(base);
            bt.position = Math::Vector3(12.0f, 1.0f, -8.0f);
            bt.scale = Math::Vector3(2.0f);
            m_World->AddComponent<ECS::MeshComponent>(base, Renderer::MeshFactory::CreateCube(1.0f));
            auto& bm = m_World->AddComponent<ECS::MaterialComponent>(base);
            bm.baseColor = Math::Vector3(0.2f, 0.4f, 0.9f);
            bm.emissiveColor = Math::Vector3(0.1f, 0.2f, 0.5f);
            bm.emissiveStrength = 0.3f;
            auto& bh = m_World->AddComponent<ECS::HealthComponent>(base);
            bh.maxHealth = 100.0f; bh.currentHealth = 100.0f;
            auto& tag = m_World->AddComponent<ECS::TagComponent>(base);
            tag.tags.push_back("base");
        }

        // Wave notes
        {
            ECS::Entity notes = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(notes, "TD Notes");
            auto& nt = m_World->AddComponent<ECS::TransformComponent>(notes);
            nt.position = Math::Vector3(0.0f, 5.0f, 0.0f);
            auto& nc = m_World->AddComponent<ECS::NotesComponent>(notes);
            nc.notes = "TOWER DEFENSE\n"
                "================\n"
                "Enemy path: follows waypoints 1-8 (orange spheres).\n"
                "Turret slots: click to build turrets (gray platforms).\n"
                "Enemies spawn at red portal, attack blue base.\n"
                "\n"
                "Wave system (to implement):\n"
                "  - Wave timer: spawn N enemies per wave\n"
                "  - Enemy types: fast/slow/armored/flying\n"
                "  - Gold earned per kill, spend on turrets\n"
                "  - Turret types: arrow (single), cannon (AOE), frost (slow)\n";
        }

        // Skybox: Overcast
        {
            Renderer::SkyboxConfig skyConfig;
            skyConfig.type = Renderer::SkyboxType::Procedural;
            skyConfig.topColor = Math::Vector3(0.4f, 0.5f, 0.6f);
            skyConfig.horizonColor = Math::Vector3(0.5f, 0.6f, 0.7f);
            skyConfig.bottomColor = Math::Vector3(0.6f, 0.65f, 0.7f);
            skyConfig.sunDirection = Math::Vector3(0.1f, 0.5f, 0.2f);
            m_RenderSystem->SetSkybox(skyConfig);
        }

        m_RenderSystem->SetShadowsEnabled(true);
        if (m_PostProcessing) {
            auto& pp = m_PostProcessing->GetSettings();
            pp.fxaaEnabled = 1;
        }

        // Spawn portal fire particles
        {
            ECS::Entity portalFx = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(portalFx, "Spawn Portal FX");
            auto& pft = m_World->AddComponent<ECS::TransformComponent>(portalFx);
            pft.position = Math::Vector3(-12.0f, 1.0f, 0.0f);
            auto& pe = m_World->AddComponent<ECS::ParticleEmitterComponent>(portalFx);
            ECS::ApplyParticlePreset(pe, "Fire");
        }

        {
            auto& groups = m_SceneManager.GetCollisionGroupNames();
            groups[1] = "Towers";
            groups[2] = "Enemies";
            groups[3] = "Projectiles";
        }
    }

    else if (templateId == "runner") {
        // Ground (long strip)
        {
            ECS::Entity ground = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(ground, "Ground");
            auto& gt = m_World->AddComponent<ECS::TransformComponent>(ground);
            gt.position = Math::Vector3(0.0f, -0.5f, 50.0f);
            gt.scale = Math::Vector3(6.0f, 1.0f, 200.0f);
            auto& gm = m_World->AddComponent<ECS::MaterialComponent>(ground);
            gm.baseColor = Math::Vector3(0.35f, 0.35f, 0.4f);
            gm.roughness = 0.7f;
            m_World->AddComponent<ECS::MeshComponent>(ground, Renderer::MeshFactory::CreateCube(1.0f));
            auto& col = m_World->AddComponent<ECS::BoxColliderComponent>(ground);
            col.size = Math::Vector3(6.0f, 1.0f, 200.0f);
        }

        // Sun
        {
            ECS::Entity sun = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(sun, "Sun");
            auto& lt = m_World->AddComponent<ECS::TransformComponent>(sun);
            lt.position = Math::Vector3(5.0f, 15.0f, 10.0f);
            auto& lc = m_World->AddComponent<ECS::LightComponent>(sun);
            lc.type = ECS::LightType::Directional;
            lc.intensity = 1.2f;
            lc.castShadows = true;
        }

        // Runner (player - 3 lanes: -2, 0, +2)
        ECS::Entity runnerEntity = m_World->CreateEntity();
        {
            m_World->AddComponent<ECS::NameComponent>(runnerEntity, "Runner");
            auto& rt = m_World->AddComponent<ECS::TransformComponent>(runnerEntity);
            rt.position = Math::Vector3(0.0f, 0.5f, 0.0f);
            m_World->AddComponent<ECS::MeshComponent>(runnerEntity, Renderer::MeshFactory::CreateCapsule(0.3f, 0.8f));
            auto& rm = m_World->AddComponent<ECS::MaterialComponent>(runnerEntity);
            rm.baseColor = Math::Vector3(0.2f, 0.5f, 0.9f);
            auto& health = m_World->AddComponent<ECS::HealthComponent>(runnerEntity);
            health.maxHealth = 3.0f;  // 3 lives
            health.currentHealth = 3.0f;
            auto& tag = m_World->AddComponent<ECS::TagComponent>(runnerEntity);
            tag.tags.push_back("runner");

            auto& notes = m_World->AddComponent<ECS::NotesComponent>(runnerEntity);
            notes.notes = "RUNNER CONTROLS\n"
                "================\n"
                "Move left/right to switch lanes (-2, 0, +2 on X).\n"
                "Jump to go over low obstacles.\n"
                "Slide/crouch to go under high obstacles.\n"
                "Auto-moves forward constantly (translate Z each frame).\n";
        }

        // Chase camera (follows behind runner, looking at them)
        {
            ECS::Entity cam = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(cam, "Runner Camera");
            auto& ct = m_World->AddComponent<ECS::TransformComponent>(cam);
            ct.position = Math::Vector3(0.0f, 3.0f, -5.0f);
            auto& cc = m_World->AddComponent<ECS::CameraComponent>(cam);
            cc.fieldOfView = 65.0f;
            cc.nearPlane = 0.1f;
            cc.farPlane = 300.0f;
            m_SelectedGameCamera = cam;

            // Follow the runner from behind and above
            auto& follow = m_World->AddComponent<ECS::FollowTargetComponent>(cam);
            follow.target = runnerEntity;
            follow.offset = Math::Vector3(0.0f, 2.5f, -5.0f);
            follow.moveSpeed = 8.0f;
            auto& lookAt = m_World->AddComponent<ECS::LookAtTargetComponent>(cam);
            lookAt.target = runnerEntity;
        }

        // Lane markers
        for (int lane = -1; lane <= 1; ++lane) {
            for (int seg = 0; seg < 10; ++seg) {
                ECS::Entity marker = m_World->CreateEntity();
                char name[32]; snprintf(name, sizeof(name), "Lane %d Seg %d", lane + 2, seg);
                m_World->AddComponent<ECS::NameComponent>(marker, name);
                auto& mt = m_World->AddComponent<ECS::TransformComponent>(marker);
                mt.position = Math::Vector3(lane * 2.0f, 0.01f, seg * 10.0f);
                mt.scale = Math::Vector3(0.05f, 0.01f, 8.0f);
                m_World->AddComponent<ECS::MeshComponent>(marker, Renderer::MeshFactory::CreateCube(1.0f));
                auto& mm = m_World->AddComponent<ECS::MaterialComponent>(marker);
                mm.baseColor = Math::Vector3(0.6f, 0.6f, 0.6f);
            }
        }

        // Sample obstacles (low barriers to jump over)
        Math::Vector3 obstaclePositions[] = {
            Math::Vector3(-2.0f, 0.4f, 15.0f), Math::Vector3(0.0f, 0.4f, 25.0f),
            Math::Vector3(2.0f, 0.4f, 35.0f),  Math::Vector3(0.0f, 0.4f, 50.0f),
            Math::Vector3(-2.0f, 0.4f, 60.0f),
        };
        for (int i = 0; i < 5; ++i) {
            ECS::Entity obs = m_World->CreateEntity();
            char name[32]; snprintf(name, sizeof(name), "Obstacle %d", i + 1);
            m_World->AddComponent<ECS::NameComponent>(obs, name);
            auto& ot = m_World->AddComponent<ECS::TransformComponent>(obs);
            ot.position = obstaclePositions[i];
            ot.scale = Math::Vector3(1.5f, 0.8f, 0.4f);
            m_World->AddComponent<ECS::MeshComponent>(obs, Renderer::MeshFactory::CreateCube(1.0f));
            auto& om = m_World->AddComponent<ECS::MaterialComponent>(obs);
            om.baseColor = Math::Vector3(0.8f, 0.2f, 0.15f);
            auto& dmg = m_World->AddComponent<ECS::DamageComponent>(obs);
            dmg.damage = 1.0f;
            auto& col = m_World->AddComponent<ECS::BoxColliderComponent>(obs);
            col.size = Math::Vector3(1.5f, 0.8f, 0.4f);
            auto& tag = m_World->AddComponent<ECS::TagComponent>(obs);
            tag.tags.push_back("obstacle");
        }

        // Coins to collect
        for (int i = 0; i < 8; ++i) {
            ECS::Entity coin = m_World->CreateEntity();
            char name[32]; snprintf(name, sizeof(name), "Coin %d", i + 1);
            m_World->AddComponent<ECS::NameComponent>(coin, name);
            auto& ct = m_World->AddComponent<ECS::TransformComponent>(coin);
            int lane = (i % 3) - 1;
            f32 coinX = lane * 2.0f;
            f32 coinY = 1.0f;
            f32 coinZ = 10.0f + i * 8.0f;
            ct.position = Math::Vector3(coinX, coinY, coinZ);
            ct.scale = Math::Vector3(0.3f);
            m_World->AddComponent<ECS::MeshComponent>(coin, Renderer::MeshFactory::CreateSphere(0.5f));
            auto& cm = m_World->AddComponent<ECS::MaterialComponent>(coin);
            cm.baseColor = Math::Vector3(1.0f, 0.85f, 0.0f);
            cm.emissiveColor = Math::Vector3(0.5f, 0.4f, 0.0f);
            cm.emissiveStrength = 0.4f;
            auto& pc = m_World->AddComponent<ECS::PickupComponent>(coin);
            pc.type = ECS::PickupComponent::PickupType::Coin;
            pc.magnetToPlayer = true;
            pc.magnetRange = 2.0f;

            // Bobbing tween
            auto& tw = m_World->AddComponent<ECS::TweenComponent>(coin);
            tw.autoPlay = true;
            ECS::TweenEntry bob;
            bob.property = ECS::TweenProperty::Position;
            bob.easing = ECS::EasingType::EaseInOutSine;
            bob.mode = ECS::TweenMode::PingPong;
            bob.startValue = Math::Vector3(coinX, coinY, coinZ);
            bob.endValue = Math::Vector3(coinX, coinY + 0.5f, coinZ);
            bob.duration = 1.5f;
            bob.useCurrentAsStart = false;
            tw.tweens.push_back(bob);
        }

        // High obstacles (crouch/slide to go under)
        Math::Vector3 highObsPositions[] = {
            Math::Vector3(0.0f, 1.5f, 20.0f), Math::Vector3(-2.0f, 1.5f, 45.0f),
            Math::Vector3(2.0f, 1.5f, 70.0f),
        };
        for (int i = 0; i < 3; ++i) {
            ECS::Entity hobs = m_World->CreateEntity();
            char hname[32]; snprintf(hname, sizeof(hname), "High Obstacle %d", i + 1);
            m_World->AddComponent<ECS::NameComponent>(hobs, hname);
            auto& ht = m_World->AddComponent<ECS::TransformComponent>(hobs);
            ht.position = highObsPositions[i];
            ht.scale = Math::Vector3(1.5f, 0.3f, 0.4f);
            m_World->AddComponent<ECS::MeshComponent>(hobs, Renderer::MeshFactory::CreateCube(1.0f));
            auto& hm = m_World->AddComponent<ECS::MaterialComponent>(hobs);
            hm.baseColor = Math::Vector3(0.6f, 0.4f, 0.15f);
            auto& hcol = m_World->AddComponent<ECS::BoxColliderComponent>(hobs);
            hcol.size = Math::Vector3(1.5f, 0.3f, 0.4f);
            auto& hdmg = m_World->AddComponent<ECS::DamageComponent>(hobs);
            hdmg.damage = 1.0f;
            auto& htag = m_World->AddComponent<ECS::TagComponent>(hobs);
            htag.tags.push_back("high_obstacle");
        }

        // Speed Boost Power-up
        {
            ECS::Entity boost = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(boost, "Speed Boost");
            auto& bt = m_World->AddComponent<ECS::TransformComponent>(boost);
            bt.position = Math::Vector3(0.0f, 0.5f, 40.0f);
            bt.scale = Math::Vector3(0.4f);
            m_World->AddComponent<ECS::MeshComponent>(boost, Renderer::MeshFactory::CreateSphere(0.5f));
            auto& bm = m_World->AddComponent<ECS::MaterialComponent>(boost);
            bm.baseColor = Math::Vector3(0.0f, 0.8f, 1.0f);
            bm.emissiveColor = Math::Vector3(0.0f, 0.6f, 0.9f);
            bm.emissiveStrength = 0.6f;
            auto& bpc = m_World->AddComponent<ECS::PickupComponent>(boost);
            bpc.type = ECS::PickupComponent::PickupType::Powerup;
            bpc.value = 2.0f;  // 2x speed multiplier
            auto& btag = m_World->AddComponent<ECS::TagComponent>(boost);
            btag.tags.push_back("powerup");
            btag.tags.push_back("speed_boost");
            // Pulsing tween
            auto& btw = m_World->AddComponent<ECS::TweenComponent>(boost);
            btw.autoPlay = true;
            ECS::TweenEntry pulse;
            pulse.property = ECS::TweenProperty::Scale;
            pulse.easing = ECS::EasingType::EaseInOutSine;
            pulse.mode = ECS::TweenMode::PingPong;
            pulse.startValue = Math::Vector3(0.4f);
            pulse.endValue = Math::Vector3(0.5f);
            pulse.duration = 0.8f;
            btw.tweens.push_back(pulse);
        }

        // Shield Power-up
        {
            ECS::Entity shield = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(shield, "Shield");
            auto& st = m_World->AddComponent<ECS::TransformComponent>(shield);
            st.position = Math::Vector3(2.0f, 0.5f, 55.0f);
            st.scale = Math::Vector3(0.35f);
            m_World->AddComponent<ECS::MeshComponent>(shield, Renderer::MeshFactory::CreateSphere(0.5f));
            auto& sm = m_World->AddComponent<ECS::MaterialComponent>(shield);
            sm.baseColor = Math::Vector3(0.2f, 0.9f, 0.3f);
            sm.emissiveColor = Math::Vector3(0.1f, 0.7f, 0.2f);
            sm.emissiveStrength = 0.5f;
            auto& spc = m_World->AddComponent<ECS::PickupComponent>(shield);
            spc.type = ECS::PickupComponent::PickupType::Powerup;
            spc.value = 1.0f;
            auto& stag = m_World->AddComponent<ECS::TagComponent>(shield);
            stag.tags.push_back("powerup");
            stag.tags.push_back("shield");
        }

        // Score display
        {
            ECS::Entity scoreUI = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(scoreUI, "Score");
            auto& st = m_World->AddComponent<ECS::TransformComponent>(scoreUI);
            st.position = Math::Vector3(0.0f, 5.0f, 5.0f);
            auto& text = m_World->AddComponent<ECS::TextComponent>(scoreUI);
            text.text = "Distance: 0m  Coins: 0";
            text.fontSize = 36.0f;
            text.textColor = Math::Vector3(1.0f, 1.0f, 1.0f);
        }

        // Skybox: Midday
        {
            Renderer::SkyboxConfig skyConfig;
            skyConfig.type = Renderer::SkyboxType::Procedural;
            skyConfig.topColor = Math::Vector3(0.1f, 0.3f, 0.8f);
            skyConfig.horizonColor = Math::Vector3(0.5f, 0.7f, 1.0f);
            skyConfig.bottomColor = Math::Vector3(0.8f, 0.85f, 0.9f);
            skyConfig.sunDirection = Math::Vector3(0.0f, 1.0f, 0.0f);
            m_RenderSystem->SetSkybox(skyConfig);
        }

        // Render settings
        m_RenderSystem->SetShadowsEnabled(true);

        // Post-processing
        if (m_PostProcessing) {
            auto& pp = m_PostProcessing->GetSettings();
            pp.fxaaEnabled = 1;
            pp.bloomEnabled = 1;
            pp.bloomThreshold = 1.0f;
            pp.bloomIntensity = 0.2f;
        }
    }

    else if (templateId == "flower") {
        // Flower Garden template - procedural flower with pluckable leaves and petals

        // Set up collision group names for flower parts
        auto& groups = m_SceneManager.GetCollisionGroupNames();
        groups[1] = "Petals";
        groups[2] = "Leaves";

        // Ground plane
        {
            ECS::Entity ground = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(ground, "Ground");
            auto& gt = m_World->AddComponent<ECS::TransformComponent>(ground);
            gt.scale = Math::Vector3(20.0f, 0.2f, 20.0f);
            gt.position = Math::Vector3(0.0f, -0.1f, 0.0f);
            auto& gmat = m_World->AddComponent<ECS::MaterialComponent>(ground);
            gmat.baseColor = Math::Vector3(0.25f, 0.45f, 0.15f);
            gmat.roughness = 0.95f;
            m_World->AddComponent<ECS::MeshComponent>(ground, Renderer::MeshFactory::CreateCube(1.0f));
        }

        // Flower stem (accessible to petal/leaf loops for tethering)
        ECS::Entity stemEntity = m_World->CreateEntity();
        {
            m_World->AddComponent<ECS::NameComponent>(stemEntity, "Flower_Stem");
            auto& st = m_World->AddComponent<ECS::TransformComponent>(stemEntity);
            st.position = Math::Vector3(0.0f, 0.8f, 0.0f);
            st.scale = Math::Vector3(0.08f, 1.6f, 0.08f);
            auto& smat = m_World->AddComponent<ECS::MaterialComponent>(stemEntity);
            smat.baseColor = Math::Vector3(0.2f, 0.5f, 0.15f);
            smat.roughness = 0.8f;
            m_World->AddComponent<ECS::MeshComponent>(stemEntity, Renderer::MeshFactory::CreateCube(1.0f));
            auto& stemFlower = m_World->AddComponent<ECS::FlowerStemComponent>(stemEntity);
            stemFlower.liquidIntensity = 1.0f;
            m_World->AddComponent<ECS::FlowerParticleConfigComponent>(stemEntity);
        }

        // Crown (created before petals so petals can reference it as connectedEntity)
        const float petalHeight = 1.7f;
        ECS::Entity crownEntity = m_World->CreateEntity();
        {
            m_World->AddComponent<ECS::NameComponent>(crownEntity, "Flower_Crown");
            auto& crt = m_World->AddComponent<ECS::TransformComponent>(crownEntity);
            crt.position = Math::Vector3(0.0f, petalHeight, 0.0f);
            crt.scale = Math::Vector3(0.25f, 0.1f, 0.25f);
            auto& crmat = m_World->AddComponent<ECS::MaterialComponent>(crownEntity);
            crmat.baseColor = Math::Vector3(0.95f, 0.8f, 0.2f); // golden yellow
            crmat.roughness = 0.5f;
            m_World->AddComponent<ECS::MeshComponent>(crownEntity, Renderer::MeshFactory::CreateCube(1.0f));
            auto& crPickup = m_World->AddComponent<ECS::PickupComponent>(crownEntity);
            crPickup.type = ECS::PickupComponent::PickupType::Custom;
            crPickup.value = 25.0f;
            auto& crTag = m_World->AddComponent<ECS::TagComponent>(crownEntity);
            crTag.tags.push_back("healthy");

            auto& crJelly = m_World->AddComponent<ECS::JellyMeshComponent>(crownEntity);
            crJelly.springStiffness = 100.0f;
            crJelly.maxStretch = 0.6f;
            auto& crTether = m_World->AddComponent<ECS::TetherComponent>(crownEntity);
            crTether.stemEntity = stemEntity;
            crTether.connectedEntity = stemEntity;  // Crown connects to stem
            crTether.attachLocalPos = Math::Vector3(0.0f, 0.8f, 0.0f);
            crTether.maxDistance = 1.0f;
            crTether.relativeSpeedThreshold = 8.0f;
            crTether.ownSpeedThreshold = 10.0f;
            crTether.absoluteTravelThreshold = 8.0f;
            crTether.relativeTravelThreshold = 8.0f;
            crTether.autoMass = 0.5f;
            crTether.autoSpringK = 1500.0f;
            crTether.autoDamping = 80.0f;
            crTether.autoDrag = 2.0f;
            crTether.driveMaxForce = 600.0f;
            auto& crGrab = m_World->AddComponent<ECS::GrabbableComponent>(crownEntity);
            crGrab.grabRadius = 0.25f;
        }

        // Petals (arranged radially, connected to crown)
        const int petalCount = 10;
        for (int i = 0; i < petalCount; ++i) {
            ECS::Entity petal = m_World->CreateEntity();
            char pname[32]; snprintf(pname, sizeof(pname), "Petal_%d", i + 1);
            m_World->AddComponent<ECS::NameComponent>(petal, pname);
            auto& pt = m_World->AddComponent<ECS::TransformComponent>(petal);
            float angle = (float)i / (float)petalCount * 6.28318f;
            float radius = 0.6f;
            pt.position = Math::Vector3(std::cos(angle) * radius, petalHeight, std::sin(angle) * radius);
            pt.rotation = Math::Quaternion(Math::Vector3(0, 1, 0), -angle)
                        * Math::Quaternion(Math::Vector3(0, 0, 1), Math::Radians(30.0f));
            pt.scale = Math::Vector3(0.5f, 0.08f, 0.3f);
            auto& pmat = m_World->AddComponent<ECS::MaterialComponent>(petal);
            bool withered = (i % 4 == 0);
            if (withered) {
                pmat.baseColor = Math::Vector3(0.6f, 0.45f, 0.2f);
            } else {
                pmat.baseColor = Math::Vector3(0.9f + (float)i * 0.01f, 0.3f, 0.4f + (float)i * 0.03f);
            }
            pmat.roughness = 0.6f;
            m_World->AddComponent<ECS::MeshComponent>(petal, Renderer::MeshFactory::CreateCube(1.0f));
            auto& pickup = m_World->AddComponent<ECS::PickupComponent>(petal);
            pickup.type = ECS::PickupComponent::PickupType::Custom;
            pickup.value = withered ? 5.0f : 10.0f;
            auto& tag = m_World->AddComponent<ECS::TagComponent>(petal);
            tag.tags.push_back(withered ? "withered" : "healthy");

            // Flower interaction components
            auto& jelly = m_World->AddComponent<ECS::JellyMeshComponent>(petal);
            jelly.springStiffness = 80.0f;
            jelly.maxStretch = 0.8f;
            auto& tether = m_World->AddComponent<ECS::TetherComponent>(petal);
            tether.stemEntity = stemEntity;           // For scoring
            tether.connectedEntity = crownEntity;     // Tether connection to crown
            tether.attachLocalPos = Math::Vector3(0.0f, 0.0f, 0.0f);
            tether.maxDistance = 0.75f;
            tether.relativeSpeedThreshold = 6.0f;
            tether.ownSpeedThreshold = 8.0f;
            tether.absoluteTravelThreshold = 5.0f;
            tether.relativeTravelThreshold = 5.0f;
            tether.autoMass = 0.3f;
            tether.autoSpringK = 1200.0f;
            tether.autoDamping = 60.0f;
            tether.autoDrag = 1.5f;
            tether.driveMaxForce = 500.0f;
            auto& grab = m_World->AddComponent<ECS::GrabbableComponent>(petal);
            grab.grabRadius = 0.3f;

            // No physics collider — FlowerSystem uses ray-sphere picking via grabRadius,
            // not physics-backed collision. Adding a collider would cause Jolt to create
            // a body and fight with FlowerSystem's direct position control.
        }

        // Leaves along the stem (connected to stem)
        const int leafCount = 5;
        for (int i = 0; i < leafCount; ++i) {
            ECS::Entity leaf = m_World->CreateEntity();
            char lname[32]; snprintf(lname, sizeof(lname), "Leaf_%d", i + 1);
            m_World->AddComponent<ECS::NameComponent>(leaf, lname);
            auto& lt = m_World->AddComponent<ECS::TransformComponent>(leaf);
            float height = 0.3f + (float)i * 0.3f;
            float side = (i % 2 == 0) ? 1.0f : -1.0f;
            lt.position = Math::Vector3(side * 0.25f, height, 0.0f);
            lt.rotation = Math::Quaternion(Math::Vector3(0, 0, 1), Math::Radians(side * 35.0f));
            lt.scale = Math::Vector3(0.45f, 0.06f, 0.2f);
            auto& lmat = m_World->AddComponent<ECS::MaterialComponent>(leaf);
            bool witheredLeaf = (i % 3 == 0);
            lmat.baseColor = witheredLeaf ? Math::Vector3(0.5f, 0.4f, 0.15f) : Math::Vector3(0.2f, 0.55f, 0.15f);
            lmat.roughness = 0.7f;
            m_World->AddComponent<ECS::MeshComponent>(leaf, Renderer::MeshFactory::CreateCube(1.0f));
            auto& pickup = m_World->AddComponent<ECS::PickupComponent>(leaf);
            pickup.type = ECS::PickupComponent::PickupType::Custom;
            pickup.value = witheredLeaf ? 5.0f : 10.0f;
            auto& tag = m_World->AddComponent<ECS::TagComponent>(leaf);
            tag.tags.push_back(witheredLeaf ? "withered" : "healthy");

            // Flower interaction components
            auto& jelly = m_World->AddComponent<ECS::JellyMeshComponent>(leaf);
            jelly.springStiffness = 60.0f;
            jelly.maxStretch = 0.8f;
            auto& tether = m_World->AddComponent<ECS::TetherComponent>(leaf);
            tether.stemEntity = stemEntity;           // For scoring
            tether.connectedEntity = stemEntity;      // Tether connection to stem
            tether.attachLocalPos = Math::Vector3(0.0f, height - 0.8f, 0.0f);
            tether.maxDistance = 0.75f;
            tether.relativeSpeedThreshold = 6.0f;
            tether.ownSpeedThreshold = 8.0f;
            tether.absoluteTravelThreshold = 5.0f;
            tether.relativeTravelThreshold = 5.0f;
            tether.autoMass = 0.2f;
            tether.autoSpringK = 1000.0f;
            tether.autoDamping = 50.0f;
            tether.autoDrag = 1.5f;
            tether.driveMaxForce = 400.0f;
            auto& grab = m_World->AddComponent<ECS::GrabbableComponent>(leaf);
            grab.grabRadius = 0.25f;

            // No physics collider — FlowerSystem uses ray-sphere picking via grabRadius.
        }

        // Camera
        {
            ECS::Entity cam = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(cam, "Game Camera");
            auto& ct = m_World->AddComponent<ECS::TransformComponent>(cam);
            ct.position = Math::Vector3(2.0f, 2.5f, 2.0f);
            // Look from (2, 2.5, 2) toward flower at origin — Qy(yaw) * Qx(pitch)
            // to apply pitch in local frame (no roll/tilt)
            ct.rotation = Math::Quaternion(Math::Vector3(0, 1, 0), Math::Radians(45.0f))
                        * Math::Quaternion(Math::Vector3(1, 0, 0), Math::Radians(-25.0f));
            auto& cc = m_World->AddComponent<ECS::CameraComponent>(cam);
            cc.fieldOfView = 50.0f;
            cc.nearPlane = 0.1f;
            cc.farPlane = 100.0f;
            m_SelectedGameCamera = cam;
        }

        // Directional light
        {
            ECS::Entity light = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(light, "Sun Light");
            auto& ltr = m_World->AddComponent<ECS::TransformComponent>(light);
            ltr.rotation = Math::Quaternion(Math::Vector3(1, 0, 0), Math::Radians(-50.0f))
                         * Math::Quaternion(Math::Vector3(0, 1, 0), Math::Radians(30.0f));
            auto& lc = m_World->AddComponent<ECS::LightComponent>(light);
            lc.type = ECS::LightType::Directional;
            lc.color = Math::Vector3(1.0f, 0.95f, 0.85f);
            lc.intensity = 1.5f;
            lc.castShadows = true;
        }

        // Score text with score_display tag
        {
            ECS::Entity scoreText = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(scoreText, "Score Display");
            auto& tt = m_World->AddComponent<ECS::TransformComponent>(scoreText);
            tt.position = Math::Vector3(0.0f, 3.0f, 0.0f);
            auto& text = m_World->AddComponent<ECS::TextComponent>(scoreText);
            text.text = "Plucked: 0/16 | Score: 0";
            text.fontSize = 24.0f;
            text.textColor = Math::Vector3(1.0f, 1.0f, 1.0f);
            auto& scoreTag = m_World->AddComponent<ECS::TagComponent>(scoreText);
            scoreTag.tags.push_back("score_display");
        }

        // Skybox: Dawn
        {
            Renderer::SkyboxConfig skyConfig;
            skyConfig.type = Renderer::SkyboxType::Procedural;
            skyConfig.topColor = Math::Vector3(0.15f, 0.15f, 0.5f);
            skyConfig.horizonColor = Math::Vector3(0.8f, 0.5f, 0.3f);
            skyConfig.bottomColor = Math::Vector3(0.6f, 0.4f, 0.3f);
            skyConfig.sunDirection = Math::Vector3(-0.8f, 0.15f, 0.2f);
            m_RenderSystem->SetSkybox(skyConfig);
        }

        // Render settings
        m_RenderSystem->SetShadowsEnabled(true);

        // Ambient Bee particles (small yellow dots floating around the flower)
        {
            ECS::Entity bees = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(bees, "Ambient Bees");
            auto& bt = m_World->AddComponent<ECS::TransformComponent>(bees);
            bt.position = Math::Vector3(0.0f, 1.5f, 0.0f);
            auto& pe = m_World->AddComponent<ECS::ParticleEmitterComponent>(bees);
            pe.maxParticles = 20;
            pe.emissionRate = 3.0f;
            pe.lifetime = 5.0f;
            pe.startSpeed = 0.3f;
            pe.startSize = 0.04f;
            pe.endSize = 0.02f;
            pe.startColor = Math::Vector3(1.0f, 0.9f, 0.2f);
            pe.endColor = Math::Vector3(0.8f, 0.7f, 0.1f);
            pe.gravity = Math::Vector3(0.0f, 0.0f, 0.0f);
            pe.drag = 0.5f;
            pe.shape = ECS::ParticleEmitterComponent::EmitterShape::Sphere;
            pe.shapeRadius = 1.5f;
            pe.playOnAwake = true;
        }

        // Second flower (smaller, to the side)
        {
            ECS::Entity stem2 = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(stem2, "Small Flower Stem");
            auto& st2 = m_World->AddComponent<ECS::TransformComponent>(stem2);
            st2.position = Math::Vector3(2.5f, 0.4f, 1.0f);
            st2.scale = Math::Vector3(0.05f, 0.8f, 0.05f);
            auto& sm2 = m_World->AddComponent<ECS::MaterialComponent>(stem2);
            sm2.baseColor = Math::Vector3(0.15f, 0.45f, 0.1f);
            m_World->AddComponent<ECS::MeshComponent>(stem2, Renderer::MeshFactory::CreateCube(1.0f));
        }
        {
            ECS::Entity bud = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(bud, "Small Flower Bud");
            auto& bdt = m_World->AddComponent<ECS::TransformComponent>(bud);
            bdt.position = Math::Vector3(2.5f, 0.9f, 1.0f);
            bdt.scale = Math::Vector3(0.3f, 0.15f, 0.3f);
            auto& bdm = m_World->AddComponent<ECS::MaterialComponent>(bud);
            bdm.baseColor = Math::Vector3(0.95f, 0.5f, 0.7f);
            m_World->AddComponent<ECS::MeshComponent>(bud, Renderer::MeshFactory::CreateSphere(0.5f));
        }

        // Post-processing
        if (m_PostProcessing) {
            auto& pp = m_PostProcessing->GetSettings();
            pp.fxaaEnabled = 1;
            pp.bloomEnabled = 1;
            pp.bloomThreshold = 0.8f;
            pp.bloomIntensity = 0.25f;
        }
    }

    else if (templateId == "fixedcam") {
        // Fixed-Angle Third-Person Camera template
        // Classic RE / God of War style — player moves in world, camera stays at fixed position+angle
        // Multiple camera zones demonstrate room-based camera switching

        // --- Ground ---
        {
            ECS::Entity ground = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(ground, "Ground");
            auto& gt = m_World->AddComponent<ECS::TransformComponent>(ground);
            gt.scale = Math::Vector3(30.0f, 0.1f, 30.0f);
            auto& gmat = m_World->AddComponent<ECS::MaterialComponent>(ground);
            gmat.baseColor = Math::Vector3(0.25f, 0.22f, 0.2f);
            gmat.roughness = 0.9f;
            m_World->AddComponent<ECS::MeshComponent>(ground, Renderer::MeshFactory::CreateCube(1.0f));
            auto& gcol = m_World->AddComponent<ECS::BoxColliderComponent>(ground);
            gcol.size = Math::Vector3(30.0f, 0.1f, 30.0f);
        }

        // --- Player ---
        ECS::Entity player = m_World->CreateEntity();
        {
            m_World->AddComponent<ECS::NameComponent>(player, "Player");
            auto& pt = m_World->AddComponent<ECS::TransformComponent>(player);
            pt.position = Math::Vector3(0.0f, 1.0f, 0.0f);
            auto& pmat = m_World->AddComponent<ECS::MaterialComponent>(player);
            pmat.baseColor = Math::Vector3(0.3f, 0.35f, 0.8f);
            m_World->AddComponent<ECS::MeshComponent>(player, Renderer::MeshFactory::CreateCapsule(0.3f, 1.0f));

            // Third-person controller with NO mouse-look (fixed camera)
            auto& ctrl = m_World->AddComponent<ECS::ThirdPersonController>(player);
            ctrl.moveSpeed = 4.0f;
            ctrl.rotateToFaceMovement = true;
            ctrl.rotateToFaceCamera = false;
            ctrl.cameraDistance = 8.0f;
            ctrl.cameraHeight = 5.0f;
            ctrl.cameraPitch = 35.0f;
            ctrl.cameraYaw = 0.0f;
            ctrl.cameraSensitivity = 0.0f; // Disable mouse orbit — fixed angle
            ctrl.cameraLerpSpeed = 5.0f;   // Smooth follow
            ctrl.enableCameraCollision = false;
            ctrl.jumpForce = 8.0f;

            auto& hp = m_World->AddComponent<ECS::HealthComponent>(player);
            hp.maxHealth = 100.0f;
            hp.currentHealth = 100.0f;
        }

        // --- Fixed camera ---
        {
            ECS::Entity cam = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(cam, "Fixed Camera");
            auto& ct = m_World->AddComponent<ECS::TransformComponent>(cam);
            ct.position = Math::Vector3(0.0f, 8.0f, 10.0f);
            ct.rotation = Math::Quaternion(Math::Vector3(1, 0, 0), Math::Radians(-35.0f));
            auto& cc = m_World->AddComponent<ECS::CameraComponent>(cam);
            cc.fieldOfView = 55.0f;
            cc.nearPlane = 0.1f;
            cc.farPlane = 100.0f;
            m_SelectedGameCamera = cam;

            // Camera follows player at fixed offset
            auto& follow = m_World->AddComponent<ECS::FollowTargetComponent>(cam);
            follow.target = player;
            follow.offset = Math::Vector3(0.0f, 8.0f, 10.0f);
            follow.smoothTime = 0.5f;
            follow.moveSpeed = 6.0f;

            // Look at player
            auto& lookAt = m_World->AddComponent<ECS::LookAtTargetComponent>(cam);
            lookAt.target = player;
            lookAt.rotationSpeed = 360.0f;
        }

        // --- Room environment: walls forming an L-shaped corridor ---
        auto makeWall = [&](const char* name, Math::Vector3 pos, Math::Vector3 scale, Math::Vector3 color) {
            ECS::Entity wall = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(wall, name);
            auto& wt = m_World->AddComponent<ECS::TransformComponent>(wall);
            wt.position = pos;
            wt.scale = scale;
            auto& wmat = m_World->AddComponent<ECS::MaterialComponent>(wall);
            wmat.baseColor = color;
            wmat.roughness = 0.8f;
            m_World->AddComponent<ECS::MeshComponent>(wall, Renderer::MeshFactory::CreateCube(1.0f));
            auto& wcol = m_World->AddComponent<ECS::BoxColliderComponent>(wall);
            wcol.size = scale;
            return wall;
        };

        Math::Vector3 wallCol(0.35f, 0.30f, 0.28f);
        Math::Vector3 pillarCol(0.4f, 0.35f, 0.3f);

        // Main room walls
        makeWall("Wall_North", Math::Vector3(0.0f, 1.5f, -8.0f), Math::Vector3(16.0f, 3.0f, 0.3f), wallCol);
        makeWall("Wall_South", Math::Vector3(-4.0f, 1.5f, 8.0f), Math::Vector3(8.0f, 3.0f, 0.3f), wallCol);
        makeWall("Wall_West", Math::Vector3(-8.0f, 1.5f, 0.0f), Math::Vector3(0.3f, 3.0f, 16.0f), wallCol);
        makeWall("Wall_East_Upper", Math::Vector3(8.0f, 1.5f, -4.0f), Math::Vector3(0.3f, 3.0f, 8.0f), wallCol);

        // Corridor extension to the east
        makeWall("Corridor_N", Math::Vector3(12.0f, 1.5f, -1.0f), Math::Vector3(8.0f, 3.0f, 0.3f), wallCol);
        makeWall("Corridor_S", Math::Vector3(12.0f, 1.5f, 5.0f), Math::Vector3(8.0f, 3.0f, 0.3f), wallCol);
        makeWall("Corridor_End", Math::Vector3(16.0f, 1.5f, 2.0f), Math::Vector3(0.3f, 3.0f, 6.0f), wallCol);

        // Decorative pillars
        makeWall("Pillar_1", Math::Vector3(-3.0f, 1.5f, -3.0f), Math::Vector3(0.5f, 3.0f, 0.5f), pillarCol);
        makeWall("Pillar_2", Math::Vector3(3.0f, 1.5f, -3.0f), Math::Vector3(0.5f, 3.0f, 0.5f), pillarCol);
        makeWall("Pillar_3", Math::Vector3(-3.0f, 1.5f, 3.0f), Math::Vector3(0.5f, 3.0f, 0.5f), pillarCol);
        makeWall("Pillar_4", Math::Vector3(3.0f, 1.5f, 3.0f), Math::Vector3(0.5f, 3.0f, 0.5f), pillarCol);

        // --- Interactable objects ---
        // Door at corridor entrance
        {
            ECS::Entity door = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(door, "Corridor Door");
            auto& dt = m_World->AddComponent<ECS::TransformComponent>(door);
            dt.position = Math::Vector3(8.0f, 1.2f, 2.0f);
            dt.scale = Math::Vector3(0.3f, 2.4f, 3.0f);
            auto& dmat = m_World->AddComponent<ECS::MaterialComponent>(door);
            dmat.baseColor = Math::Vector3(0.45f, 0.30f, 0.15f);
            dmat.roughness = 0.6f;
            m_World->AddComponent<ECS::MeshComponent>(door, Renderer::MeshFactory::CreateCube(1.0f));
            auto& interact = m_World->AddComponent<ECS::InteractableComponent>(door);
            interact.promptText = "Open Door";
            interact.interactionRange = 2.5f;
            auto& dtag = m_World->AddComponent<ECS::TagComponent>(door);
            dtag.tags.push_back("door");
        }

        // Pickup item
        {
            ECS::Entity key = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(key, "Key Item");
            auto& kt = m_World->AddComponent<ECS::TransformComponent>(key);
            kt.position = Math::Vector3(-5.0f, 0.5f, -5.0f);
            kt.scale = Math::Vector3(0.3f, 0.3f, 0.1f);
            auto& kmat = m_World->AddComponent<ECS::MaterialComponent>(key);
            kmat.baseColor = Math::Vector3(0.9f, 0.8f, 0.2f);
            kmat.metallic = 0.8f;
            kmat.roughness = 0.2f;
            m_World->AddComponent<ECS::MeshComponent>(key, Renderer::MeshFactory::CreateCube(1.0f));
            auto& pickup = m_World->AddComponent<ECS::PickupComponent>(key);
            pickup.type = ECS::PickupComponent::PickupType::Custom;
            pickup.value = 1.0f;
            auto& ktag = m_World->AddComponent<ECS::TagComponent>(key);
            ktag.tags.push_back("key_item");
        }

        // Camera trigger zone — switches camera angle in corridor
        {
            ECS::Entity camZone = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(camZone, "Corridor Camera Zone");
            auto& zt = m_World->AddComponent<ECS::TransformComponent>(camZone);
            zt.position = Math::Vector3(10.0f, 1.5f, 2.0f);
            auto& trigger = m_World->AddComponent<ECS::TriggerZoneComponent>(camZone);
            trigger.shape = ECS::TriggerZoneComponent::Shape::Box;
            trigger.boxSize = Math::Vector3(6.0f, 3.0f, 5.0f);
            trigger.triggerOnce = false;
            auto& ztag = m_World->AddComponent<ECS::TagComponent>(camZone);
            ztag.tags.push_back("camera_zone");
        }

        // --- Lighting ---
        {
            ECS::Entity light = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(light, "Main Light");
            auto& lt = m_World->AddComponent<ECS::TransformComponent>(light);
            lt.rotation = Math::Quaternion(Math::Vector3(1, 0, 0), Math::Radians(-55.0f))
                        * Math::Quaternion(Math::Vector3(0, 1, 0), Math::Radians(25.0f));
            auto& lc = m_World->AddComponent<ECS::LightComponent>(light);
            lc.type = ECS::LightType::Directional;
            lc.color = Math::Vector3(0.9f, 0.85f, 0.75f);
            lc.intensity = 1.2f;
            lc.castShadows = true;
        }

        // Corridor point light
        {
            ECS::Entity cLight = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(cLight, "Corridor Light");
            auto& clt = m_World->AddComponent<ECS::TransformComponent>(cLight);
            clt.position = Math::Vector3(12.0f, 2.5f, 2.0f);
            auto& clc = m_World->AddComponent<ECS::LightComponent>(cLight);
            clc.type = ECS::LightType::Point;
            clc.color = Math::Vector3(1.0f, 0.8f, 0.5f);
            clc.intensity = 2.0f;
            clc.range = 8.0f;
        }

        // --- 2nd Camera Zone (corridor, different angle) ---
        {
            ECS::Entity camZone2 = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(camZone2, "Corridor Camera Zone 2");
            auto& z2t = m_World->AddComponent<ECS::TransformComponent>(camZone2);
            z2t.position = Math::Vector3(14.0f, 1.5f, 2.0f);
            auto& z2trigger = m_World->AddComponent<ECS::TriggerZoneComponent>(camZone2);
            z2trigger.shape = ECS::TriggerZoneComponent::Shape::Box;
            z2trigger.boxSize = Math::Vector3(4.0f, 3.0f, 5.0f);
            z2trigger.triggerOnce = false;
            auto& z2tag = m_World->AddComponent<ECS::TagComponent>(camZone2);
            z2tag.tags.push_back("camera_zone");
            auto& z2notes = m_World->AddComponent<ECS::NotesComponent>(camZone2);
            z2notes.notes = "CAMERA ZONE 2\nWhen player enters, switch to overhead camera angle.\n"
                "Script: detect trigger enter -> lerp camera to new offset.";
        }

        // Corridor end collectible
        {
            ECS::Entity gem = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(gem, "Corridor Gem");
            auto& gt = m_World->AddComponent<ECS::TransformComponent>(gem);
            gt.position = Math::Vector3(15.0f, 0.6f, 2.0f);
            gt.scale = Math::Vector3(0.3f);
            m_World->AddComponent<ECS::MeshComponent>(gem, Renderer::MeshFactory::CreateSphere(0.5f));
            auto& gm = m_World->AddComponent<ECS::MaterialComponent>(gem);
            gm.baseColor = Math::Vector3(0.4f, 0.9f, 0.5f);
            gm.emissiveColor = Math::Vector3(0.2f, 0.7f, 0.3f);
            gm.emissiveStrength = 0.5f;
            auto& gpc = m_World->AddComponent<ECS::PickupComponent>(gem);
            gpc.type = ECS::PickupComponent::PickupType::Custom;
            gpc.value = 50.0f;
            auto& gtw = m_World->AddComponent<ECS::TweenComponent>(gem);
            gtw.autoPlay = true;
            ECS::TweenEntry bob;
            bob.property = ECS::TweenProperty::Position;
            bob.easing = ECS::EasingType::EaseInOutSine;
            bob.mode = ECS::TweenMode::PingPong;
            bob.startValue = Math::Vector3(15.0f, 0.6f, 2.0f);
            bob.endValue = Math::Vector3(15.0f, 1.0f, 2.0f);
            bob.duration = 1.5f;
            gtw.tweens.push_back(bob);
        }

        // --- HUD ---
        {
            ECS::Entity hud = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(hud, "HUD");
            auto& ht = m_World->AddComponent<ECS::TransformComponent>(hud);
            ht.position = Math::Vector3(0.0f, 5.0f, 0.0f);
            auto& htext = m_World->AddComponent<ECS::TextComponent>(hud);
            htext.text = "HP: 100/100 | Items: 0";
            htext.fontSize = 20.0f;
            htext.textColor = Math::Vector3(0.9f, 0.9f, 0.9f);
        }

        // Skybox: Overcast
        {
            Renderer::SkyboxConfig skyConfig;
            skyConfig.type = Renderer::SkyboxType::Procedural;
            skyConfig.topColor = Math::Vector3(0.4f, 0.5f, 0.6f);
            skyConfig.horizonColor = Math::Vector3(0.5f, 0.6f, 0.7f);
            skyConfig.bottomColor = Math::Vector3(0.6f, 0.65f, 0.7f);
            skyConfig.sunDirection = Math::Vector3(0.1f, 0.5f, 0.2f);
            m_RenderSystem->SetSkybox(skyConfig);
        }

        // Render settings
        m_RenderSystem->SetShadowsEnabled(true);
        m_RenderSystem->SetAmbientIntensity(0.1f);

        // Post-processing
        if (m_PostProcessing) {
            auto& pp = m_PostProcessing->GetSettings();
            pp.fxaaEnabled = 1;
            pp.vignetteEnabled = 1;
            pp.vignetteIntensity = 0.2f;
        }
    }

    else if (templateId == "metroidvania") {
        // =====================================================================
        // METROIDVANIA TEMPLATE - Interconnected 2D side-scrolling rooms
        // =====================================================================
        // Room layout (XY plane):
        //   Room A (start)  -> Room B (upper) -> Room C (boss corridor)
        //        |                                     |
        //   Room D (lower)  -> Room E (hub)   -> Room F (ability room)
        //
        // Rooms are connected by openings. Locked doors block progression
        // until the player finds keys or abilities.

        // --- Room geometry constants ---
        const f32 ROOM_W = 16.0f;
        const f32 ROOM_H = 10.0f;
        const f32 WALL_T = 0.5f;   // Wall thickness
        const Math::Vector3 wallColor(0.22f, 0.18f, 0.25f);
        const Math::Vector3 floorColor(0.15f, 0.13f, 0.18f);
        const Math::Vector3 platColor(0.28f, 0.24f, 0.30f);

        // Helper to create a wall/floor block
        auto makeBlock = [&](const std::string& name, Math::Vector3 pos, Math::Vector3 scl,
                             Math::Vector3 color) -> ECS::Entity {
            ECS::Entity e = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(e, name);
            auto& t = m_World->AddComponent<ECS::TransformComponent>(e);
            t.position = pos;
            t.scale = scl;
            auto& mat = m_World->AddComponent<ECS::MaterialComponent>(e);
            mat.baseColor = color;
            mat.roughness = 0.85f;
            m_World->AddComponent<ECS::MeshComponent>(e, Renderer::MeshFactory::CreateQuad(1.0f, 1.0f));
            auto& col = m_World->AddComponent<ECS::BoxColliderComponent>(e);
            col.size = scl;
            return e;
        };

        // Helper to create a room shell (floor, ceiling, left wall, right wall)
        auto makeRoom = [&](const std::string& prefix, f32 ox, f32 oy) {
            // Floor
            makeBlock(prefix + " Floor", Math::Vector3(ox, oy - ROOM_H * 0.5f, 0.0f),
                      Math::Vector3(ROOM_W, WALL_T, 1.0f), floorColor);
            // Ceiling
            makeBlock(prefix + " Ceiling", Math::Vector3(ox, oy + ROOM_H * 0.5f, 0.0f),
                      Math::Vector3(ROOM_W, WALL_T, 1.0f), wallColor);
            // Left wall
            makeBlock(prefix + " Wall L", Math::Vector3(ox - ROOM_W * 0.5f, oy, 0.0f),
                      Math::Vector3(WALL_T, ROOM_H, 1.0f), wallColor);
            // Right wall
            makeBlock(prefix + " Wall R", Math::Vector3(ox + ROOM_W * 0.5f, oy, 0.0f),
                      Math::Vector3(WALL_T, ROOM_H, 1.0f), wallColor);
        };

        // Room origins (center of each room)
        const f32 rAx = 0.0f,   rAy = 0.0f;   // Room A - Start
        const f32 rBx = 18.0f,  rBy = 6.0f;    // Room B - Upper right
        const f32 rCx = 36.0f,  rCy = 6.0f;    // Room C - Boss corridor
        const f32 rDx = 0.0f,   rDy = -12.0f;  // Room D - Lower left
        const f32 rEx = 18.0f,  rEy = -12.0f;  // Room E - Hub
        const f32 rFx = 36.0f,  rFy = -12.0f;  // Room F - Ability room

        // Build room shells
        makeRoom("Room A", rAx, rAy);
        makeRoom("Room B", rBx, rBy);
        makeRoom("Room C", rCx, rCy);
        makeRoom("Room D", rDx, rDy);
        makeRoom("Room E", rEx, rEy);
        makeRoom("Room F", rFx, rFy);

        // --- Platforms inside rooms ---
        // Room A platforms (starting area with wall-jump practice)
        makeBlock("A Platform 1", Math::Vector3(rAx - 4.0f, rAy - 2.0f, 0.0f),
                  Math::Vector3(4.0f, 0.4f, 1.0f), platColor);
        makeBlock("A Platform 2", Math::Vector3(rAx + 3.0f, rAy + 1.0f, 0.0f),
                  Math::Vector3(3.0f, 0.4f, 1.0f), platColor);
        makeBlock("A Platform 3", Math::Vector3(rAx - 2.0f, rAy + 3.0f, 0.0f),
                  Math::Vector3(3.0f, 0.4f, 1.0f), platColor);

        // Room B platforms (vertical challenge)
        makeBlock("B Platform 1", Math::Vector3(rBx - 5.0f, rBy - 3.0f, 0.0f),
                  Math::Vector3(4.0f, 0.4f, 1.0f), platColor);
        makeBlock("B Platform 2", Math::Vector3(rBx + 2.0f, rBy - 1.0f, 0.0f),
                  Math::Vector3(3.0f, 0.4f, 1.0f), platColor);
        makeBlock("B Platform 3", Math::Vector3(rBx - 3.0f, rBy + 2.0f, 0.0f),
                  Math::Vector3(5.0f, 0.4f, 1.0f), platColor);

        // Room D platforms (lower maze)
        makeBlock("D Platform 1", Math::Vector3(rDx + 4.0f, rDy - 2.0f, 0.0f),
                  Math::Vector3(3.5f, 0.4f, 1.0f), platColor);
        makeBlock("D Platform 2", Math::Vector3(rDx - 3.0f, rDy + 1.5f, 0.0f),
                  Math::Vector3(3.0f, 0.4f, 1.0f), platColor);

        // Room E platforms (hub - central area)
        makeBlock("E Platform 1", Math::Vector3(rEx, rEy - 2.0f, 0.0f),
                  Math::Vector3(6.0f, 0.4f, 1.0f), platColor);
        makeBlock("E Platform 2", Math::Vector3(rEx - 5.0f, rEy + 1.0f, 0.0f),
                  Math::Vector3(3.0f, 0.4f, 1.0f), platColor);
        makeBlock("E Platform 3", Math::Vector3(rEx + 5.0f, rEy + 1.0f, 0.0f),
                  Math::Vector3(3.0f, 0.4f, 1.0f), platColor);

        // Room F platforms (ability room)
        makeBlock("F Platform 1", Math::Vector3(rFx - 4.0f, rFy - 1.0f, 0.0f),
                  Math::Vector3(3.0f, 0.4f, 1.0f), platColor);
        makeBlock("F Platform 2", Math::Vector3(rFx + 3.0f, rFy + 2.0f, 0.0f),
                  Math::Vector3(4.0f, 0.4f, 1.0f), platColor);

        // --- Player ---
        ECS::Entity player = createPlayer2D("Player");
        {
            auto* pt = m_World->GetComponent<ECS::TransformComponent>(player);
            if (pt) pt->position = Math::Vector3(rAx, rAy - 2.0f, 0.0f);
            auto& ctrl = m_World->AddComponent<ECS::Platformer2DController>(player);
            ctrl.moveSpeed = 6.0f;
            ctrl.jumpForce = 12.0f;
            ctrl.enableWallJump = true;
            ctrl.enableWallSlide = true;
            ctrl.wallSlideSpeed = 2.0f;
            ctrl.wallJumpForce = 8.0f;
            auto& health = m_World->AddComponent<ECS::HealthComponent>(player);
            health.maxHealth = 100.0f;
            health.currentHealth = 100.0f;
            health.invulnerabilityTime = 1.0f;
            m_World->AddComponent<ECS::InventoryComponent>(player);
            SetupCameraForController(player, "Platformer2D");
        }

        // --- Locked Doors ---
        // Door A->B (requires Red Key)
        {
            ECS::Entity door = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(door, "Door A-B (Red Key)");
            auto& dt = m_World->AddComponent<ECS::TransformComponent>(door);
            dt.position = Math::Vector3(rAx + ROOM_W * 0.5f, rAy + 2.0f, 0.0f);
            dt.scale = Math::Vector3(0.5f, 3.0f, 1.0f);
            auto& dm = m_World->AddComponent<ECS::MaterialComponent>(door);
            dm.baseColor = Math::Vector3(0.7f, 0.15f, 0.1f);
            dm.emissiveColor = Math::Vector3(0.3f, 0.05f, 0.02f);
            dm.emissiveStrength = 0.4f;
            m_World->AddComponent<ECS::MeshComponent>(door, Renderer::MeshFactory::CreateQuad(1.0f, 1.0f));
            auto& col = m_World->AddComponent<ECS::BoxColliderComponent>(door);
            col.size = Math::Vector3(0.5f, 3.0f, 1.0f);
            auto& lock = m_World->AddComponent<ECS::LockComponent>(door);
            lock.requiredKey = "red_key";
            lock.isLocked = true;
            lock.openMode = ECS::LockComponent::OpenMode::OpenOnly;
            lock.lockedPrompt = "Requires Red Key";
            lock.openPosition = Math::Vector3(0.0f, 3.5f, 0.0f);
        }

        // Door D->E (requires Blue Key)
        {
            ECS::Entity door = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(door, "Door D-E (Blue Key)");
            auto& dt = m_World->AddComponent<ECS::TransformComponent>(door);
            dt.position = Math::Vector3(rDx + ROOM_W * 0.5f, rDy, 0.0f);
            dt.scale = Math::Vector3(0.5f, 3.0f, 1.0f);
            auto& dm = m_World->AddComponent<ECS::MaterialComponent>(door);
            dm.baseColor = Math::Vector3(0.1f, 0.2f, 0.7f);
            dm.emissiveColor = Math::Vector3(0.02f, 0.05f, 0.3f);
            dm.emissiveStrength = 0.4f;
            m_World->AddComponent<ECS::MeshComponent>(door, Renderer::MeshFactory::CreateQuad(1.0f, 1.0f));
            auto& col = m_World->AddComponent<ECS::BoxColliderComponent>(door);
            col.size = Math::Vector3(0.5f, 3.0f, 1.0f);
            auto& lock = m_World->AddComponent<ECS::LockComponent>(door);
            lock.requiredKey = "blue_key";
            lock.isLocked = true;
            lock.openMode = ECS::LockComponent::OpenMode::OpenOnly;
            lock.lockedPrompt = "Requires Blue Key";
            lock.openPosition = Math::Vector3(0.0f, 3.5f, 0.0f);
        }

        // Door E->F (requires Boss Key)
        {
            ECS::Entity door = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(door, "Door E-F (Boss Key)");
            auto& dt = m_World->AddComponent<ECS::TransformComponent>(door);
            dt.position = Math::Vector3(rEx + ROOM_W * 0.5f, rEy, 0.0f);
            dt.scale = Math::Vector3(0.5f, 3.0f, 1.0f);
            auto& dm = m_World->AddComponent<ECS::MaterialComponent>(door);
            dm.baseColor = Math::Vector3(0.6f, 0.5f, 0.1f);
            dm.emissiveColor = Math::Vector3(0.3f, 0.25f, 0.02f);
            dm.emissiveStrength = 0.5f;
            m_World->AddComponent<ECS::MeshComponent>(door, Renderer::MeshFactory::CreateQuad(1.0f, 1.0f));
            auto& col = m_World->AddComponent<ECS::BoxColliderComponent>(door);
            col.size = Math::Vector3(0.5f, 3.0f, 1.0f);
            auto& lock = m_World->AddComponent<ECS::LockComponent>(door);
            lock.requiredKey = "boss_key";
            lock.isLocked = true;
            lock.openMode = ECS::LockComponent::OpenMode::OpenOnly;
            lock.lockedPrompt = "Requires Boss Key";
            lock.openPosition = Math::Vector3(0.0f, 3.5f, 0.0f);
        }

        // --- Key Pickups ---
        // Red Key (in Room D)
        {
            ECS::Entity key = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(key, "Red Key");
            auto& kt = m_World->AddComponent<ECS::TransformComponent>(key);
            kt.position = Math::Vector3(rDx - 3.0f, rDy + 2.5f, 0.0f);
            kt.scale = Math::Vector3(0.4f);
            auto& km = m_World->AddComponent<ECS::MaterialComponent>(key);
            km.baseColor = Math::Vector3(0.9f, 0.2f, 0.15f);
            km.emissiveColor = Math::Vector3(0.5f, 0.1f, 0.05f);
            km.emissiveStrength = 0.8f;
            m_World->AddComponent<ECS::MeshComponent>(key, Renderer::MeshFactory::CreateSphere(0.5f));
            auto& pc = m_World->AddComponent<ECS::PickupComponent>(key);
            pc.type = ECS::PickupComponent::PickupType::Key;
            pc.customId = "red_key";
            pc.bobSpeed = 2.0f;
            pc.bobHeight = 0.15f;
        }

        // Blue Key (in Room B, on high platform)
        {
            ECS::Entity key = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(key, "Blue Key");
            auto& kt = m_World->AddComponent<ECS::TransformComponent>(key);
            kt.position = Math::Vector3(rBx - 3.0f, rBy + 3.0f, 0.0f);
            kt.scale = Math::Vector3(0.4f);
            auto& km = m_World->AddComponent<ECS::MaterialComponent>(key);
            km.baseColor = Math::Vector3(0.15f, 0.3f, 0.9f);
            km.emissiveColor = Math::Vector3(0.05f, 0.1f, 0.5f);
            km.emissiveStrength = 0.8f;
            m_World->AddComponent<ECS::MeshComponent>(key, Renderer::MeshFactory::CreateSphere(0.5f));
            auto& pc = m_World->AddComponent<ECS::PickupComponent>(key);
            pc.type = ECS::PickupComponent::PickupType::Key;
            pc.customId = "blue_key";
            pc.bobSpeed = 2.0f;
            pc.bobHeight = 0.15f;
        }

        // Boss Key (in Room C, guarded)
        {
            ECS::Entity key = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(key, "Boss Key");
            auto& kt = m_World->AddComponent<ECS::TransformComponent>(key);
            kt.position = Math::Vector3(rCx + 4.0f, rCy + 2.0f, 0.0f);
            kt.scale = Math::Vector3(0.5f);
            auto& km = m_World->AddComponent<ECS::MaterialComponent>(key);
            km.baseColor = Math::Vector3(0.85f, 0.75f, 0.15f);
            km.emissiveColor = Math::Vector3(0.4f, 0.35f, 0.05f);
            km.emissiveStrength = 1.0f;
            m_World->AddComponent<ECS::MeshComponent>(key, Renderer::MeshFactory::CreateSphere(0.5f));
            auto& pc = m_World->AddComponent<ECS::PickupComponent>(key);
            pc.type = ECS::PickupComponent::PickupType::Key;
            pc.customId = "boss_key";
            pc.bobSpeed = 3.0f;
            pc.bobHeight = 0.2f;
        }

        // --- Ability Gate (blocks passage until player has double-jump ability) ---
        {
            ECS::Entity gate = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(gate, "Ability Gate (Double Jump)");
            auto& gt = m_World->AddComponent<ECS::TransformComponent>(gate);
            gt.position = Math::Vector3(rBx + ROOM_W * 0.5f, rBy + 1.0f, 0.0f);
            gt.scale = Math::Vector3(0.4f, 4.0f, 1.0f);
            auto& gm = m_World->AddComponent<ECS::MaterialComponent>(gate);
            gm.baseColor = Math::Vector3(0.4f, 0.1f, 0.5f);
            gm.emissiveColor = Math::Vector3(0.2f, 0.05f, 0.3f);
            gm.emissiveStrength = 0.6f;
            m_World->AddComponent<ECS::MeshComponent>(gate, Renderer::MeshFactory::CreateQuad(1.0f, 1.0f));
            auto& col = m_World->AddComponent<ECS::BoxColliderComponent>(gate);
            col.size = Math::Vector3(0.4f, 4.0f, 1.0f);
            auto& tag = m_World->AddComponent<ECS::TagComponent>(gate);
            tag.tags.push_back("ability_gate");
            tag.tags.push_back("requires_double_jump");
            auto& interact = m_World->AddComponent<ECS::InteractableComponent>(gate);
            interact.promptText = "Requires Double Jump ability";
            interact.isEnabled = false;
        }

        // Ability Gate (blocks Room A to Room D - requires dash)
        {
            ECS::Entity gate = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(gate, "Ability Gate (Dash)");
            auto& gt = m_World->AddComponent<ECS::TransformComponent>(gate);
            gt.position = Math::Vector3(rAx, rAy - ROOM_H * 0.5f - 1.0f, 0.0f);
            gt.scale = Math::Vector3(3.0f, 0.4f, 1.0f);
            auto& gm = m_World->AddComponent<ECS::MaterialComponent>(gate);
            gm.baseColor = Math::Vector3(0.1f, 0.4f, 0.5f);
            gm.emissiveColor = Math::Vector3(0.05f, 0.2f, 0.3f);
            gm.emissiveStrength = 0.6f;
            m_World->AddComponent<ECS::MeshComponent>(gate, Renderer::MeshFactory::CreateQuad(1.0f, 1.0f));
            auto& col = m_World->AddComponent<ECS::BoxColliderComponent>(gate);
            col.size = Math::Vector3(3.0f, 0.4f, 1.0f);
            auto& tag = m_World->AddComponent<ECS::TagComponent>(gate);
            tag.tags.push_back("ability_gate");
            tag.tags.push_back("requires_dash");
            auto& interact = m_World->AddComponent<ECS::InteractableComponent>(gate);
            interact.promptText = "Requires Dash ability";
            interact.isEnabled = false;
        }

        // --- Save Point (Room E hub) ---
        {
            ECS::Entity save = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(save, "Save Point");
            auto& st = m_World->AddComponent<ECS::TransformComponent>(save);
            st.position = Math::Vector3(rEx, rEy - 1.5f, 0.0f);
            st.scale = Math::Vector3(0.6f);
            auto& sm = m_World->AddComponent<ECS::MaterialComponent>(save);
            sm.baseColor = Math::Vector3(0.9f, 0.9f, 0.4f);
            sm.emissiveColor = Math::Vector3(0.6f, 0.6f, 0.2f);
            sm.emissiveStrength = 1.2f;
            m_World->AddComponent<ECS::MeshComponent>(save, Renderer::MeshFactory::CreateSphere(0.5f));
            auto& tag = m_World->AddComponent<ECS::TagComponent>(save);
            tag.tags.push_back("save_point");
            auto& interact = m_World->AddComponent<ECS::InteractableComponent>(save);
            interact.promptText = "Save Progress";
            interact.interactionRange = 2.0f;
            auto& trigger = m_World->AddComponent<ECS::TriggerZoneComponent>(save);
            trigger.shape = ECS::TriggerZoneComponent::Shape::Sphere;
            trigger.sphereRadius = 2.0f;
        }

        // --- Health Pickups ---
        {
            Math::Vector3 hpPositions[] = {
                Math::Vector3(rAx + 5.0f, rAy - 3.0f, 0.0f),
                Math::Vector3(rBx + 4.0f, rBy - 2.0f, 0.0f),
                Math::Vector3(rDx + 5.0f, rDy + 0.5f, 0.0f),
                Math::Vector3(rEx - 6.0f, rEy - 3.0f, 0.0f)
            };
            for (int i = 0; i < 4; ++i) {
                char name[32];
                snprintf(name, sizeof(name), "Health Pickup %d", i + 1);
                ECS::Entity hp = m_World->CreateEntity();
                m_World->AddComponent<ECS::NameComponent>(hp, name);
                auto& ht = m_World->AddComponent<ECS::TransformComponent>(hp);
                ht.position = hpPositions[i];
                ht.scale = Math::Vector3(0.3f);
                auto& hm = m_World->AddComponent<ECS::MaterialComponent>(hp);
                hm.baseColor = Math::Vector3(0.2f, 0.85f, 0.3f);
                hm.emissiveColor = Math::Vector3(0.1f, 0.4f, 0.1f);
                hm.emissiveStrength = 0.6f;
                m_World->AddComponent<ECS::MeshComponent>(hp, Renderer::MeshFactory::CreateSphere(0.5f));
                auto& pc = m_World->AddComponent<ECS::PickupComponent>(hp);
                pc.type = ECS::PickupComponent::PickupType::Health;
                pc.value = 25.0f;
                pc.bobSpeed = 1.5f;
                pc.bobHeight = 0.1f;
            }
        }

        // --- Enemies ---
        // Patrol enemy in Room B
        {
            ECS::Entity enemy = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(enemy, "Crawler (Room B)");
            auto& et = m_World->AddComponent<ECS::TransformComponent>(enemy);
            et.position = Math::Vector3(rBx, rBy - 3.5f, 0.0f);
            auto& em = m_World->AddComponent<ECS::MaterialComponent>(enemy);
            em.baseColor = Math::Vector3(0.75f, 0.12f, 0.1f);
            m_World->AddComponent<ECS::MeshComponent>(enemy, Renderer::MeshFactory::CreateCapsule2D(0.7f, 1.2f));
            m_World->AddComponent<ECS::Sprite2DComponent>(enemy);
            auto& ai = m_World->AddComponent<ECS::AIControllerComponent>(enemy);
            ai.currentState = ECS::AIControllerComponent::AIState::Patrol;
            ai.moveSpeed = 2.5f;
            ai.patrolPoints.push_back(Math::Vector3(rBx - 5.0f, rBy - 3.5f, 0.0f));
            ai.patrolPoints.push_back(Math::Vector3(rBx + 5.0f, rBy - 3.5f, 0.0f));
            auto& eh = m_World->AddComponent<ECS::HealthComponent>(enemy);
            eh.maxHealth = 30.0f;
            eh.currentHealth = 30.0f;
            auto& dmg = m_World->AddComponent<ECS::DamageComponent>(enemy);
            dmg.damage = 15.0f;
            auto& tag = m_World->AddComponent<ECS::TagComponent>(enemy);
            tag.tags.push_back("enemy");
        }

        // Flying enemy in Room C
        {
            ECS::Entity enemy = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(enemy, "Flyer (Room C)");
            auto& et = m_World->AddComponent<ECS::TransformComponent>(enemy);
            et.position = Math::Vector3(rCx - 2.0f, rCy + 1.0f, 0.0f);
            auto& em = m_World->AddComponent<ECS::MaterialComponent>(enemy);
            em.baseColor = Math::Vector3(0.6f, 0.1f, 0.4f);
            m_World->AddComponent<ECS::MeshComponent>(enemy, Renderer::MeshFactory::CreateCapsule2D(0.6f, 1.0f));
            m_World->AddComponent<ECS::Sprite2DComponent>(enemy);
            auto& ai = m_World->AddComponent<ECS::AIControllerComponent>(enemy);
            ai.currentState = ECS::AIControllerComponent::AIState::Patrol;
            ai.moveSpeed = 3.0f;
            ai.patrolPoints.push_back(Math::Vector3(rCx - 5.0f, rCy + 2.0f, 0.0f));
            ai.patrolPoints.push_back(Math::Vector3(rCx + 5.0f, rCy - 1.0f, 0.0f));
            auto& eh = m_World->AddComponent<ECS::HealthComponent>(enemy);
            eh.maxHealth = 20.0f;
            eh.currentHealth = 20.0f;
            auto& dmg = m_World->AddComponent<ECS::DamageComponent>(enemy);
            dmg.damage = 10.0f;
            auto& tag = m_World->AddComponent<ECS::TagComponent>(enemy);
            tag.tags.push_back("enemy");
            tag.tags.push_back("flying");
        }

        // Heavy enemy in Room D
        {
            ECS::Entity enemy = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(enemy, "Brute (Room D)");
            auto& et = m_World->AddComponent<ECS::TransformComponent>(enemy);
            et.position = Math::Vector3(rDx + 4.0f, rDy - 3.0f, 0.0f);
            et.scale = Math::Vector3(1.2f, 1.2f, 1.0f);
            auto& em = m_World->AddComponent<ECS::MaterialComponent>(enemy);
            em.baseColor = Math::Vector3(0.5f, 0.15f, 0.1f);
            m_World->AddComponent<ECS::MeshComponent>(enemy, Renderer::MeshFactory::CreateCapsule2D(0.9f, 1.6f));
            m_World->AddComponent<ECS::Sprite2DComponent>(enemy);
            auto& ai = m_World->AddComponent<ECS::AIControllerComponent>(enemy);
            ai.currentState = ECS::AIControllerComponent::AIState::Patrol;
            ai.moveSpeed = 1.5f;
            ai.attackDamage = 25.0f;
            ai.patrolPoints.push_back(Math::Vector3(rDx + 2.0f, rDy - 3.0f, 0.0f));
            ai.patrolPoints.push_back(Math::Vector3(rDx + 6.0f, rDy - 3.0f, 0.0f));
            auto& eh = m_World->AddComponent<ECS::HealthComponent>(enemy);
            eh.maxHealth = 60.0f;
            eh.currentHealth = 60.0f;
            auto& dmg = m_World->AddComponent<ECS::DamageComponent>(enemy);
            dmg.damage = 25.0f;
            auto& tag = m_World->AddComponent<ECS::TagComponent>(enemy);
            tag.tags.push_back("enemy");
            tag.tags.push_back("heavy");
        }

        // --- Save Station in Room E (hub) ---
        {
            ECS::Entity saveStation = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(saveStation, "Save Station (Hub)");
            auto& sst = m_World->AddComponent<ECS::TransformComponent>(saveStation);
            sst.position = Math::Vector3(rEx, rEy, 0.0f);
            sst.scale = Math::Vector3(0.6f, 0.6f, 1.0f);
            auto& ssm = m_World->AddComponent<ECS::MaterialComponent>(saveStation);
            ssm.baseColor = Math::Vector3(0.3f, 0.8f, 0.9f);
            ssm.emissiveColor = Math::Vector3(0.2f, 0.6f, 0.8f);
            ssm.emissiveStrength = 0.8f;
            m_World->AddComponent<ECS::MeshComponent>(saveStation, Renderer::MeshFactory::CreateCapsule2D(0.3f, 0.6f));
            m_World->AddComponent<ECS::Sprite2DComponent>(saveStation);
            auto& ssint = m_World->AddComponent<ECS::InteractableComponent>(saveStation);
            ssint.promptText = "Save Progress";
            ssint.interactionRange = 2.0f;
            auto& sstag = m_World->AddComponent<ECS::TagComponent>(saveStation);
            sstag.tags.push_back("save_station");
            auto& ssnotes = m_World->AddComponent<ECS::NotesComponent>(saveStation);
            ssnotes.notes = "Save station — restores health and saves progress.\n"
                "Script: on interact, call SaveGame_ToSlot() and restore player HP.";
        }

        // Spike trap in Room D
        {
            ECS::Entity spikes = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(spikes, "Spike Trap (Room D)");
            auto& spt = m_World->AddComponent<ECS::TransformComponent>(spikes);
            spt.position = Math::Vector3(rDx - 2.0f, rDy - 4.5f, 0.0f);
            spt.scale = Math::Vector3(3.0f, 0.3f, 1.0f);
            auto& spm = m_World->AddComponent<ECS::MaterialComponent>(spikes);
            spm.baseColor = Math::Vector3(0.5f, 0.2f, 0.15f);
            m_World->AddComponent<ECS::MeshComponent>(spikes, Renderer::MeshFactory::CreateQuad(1.0f, 1.0f));
            auto& dmg = m_World->AddComponent<ECS::DamageComponent>(spikes);
            dmg.damage = 20.0f;
            auto& trigger = m_World->AddComponent<ECS::TriggerZoneComponent>(spikes);
            trigger.shape = ECS::TriggerZoneComponent::Shape::Box;
            trigger.boxSize = Math::Vector3(3.0f, 0.3f, 1.0f);
            auto& sptag = m_World->AddComponent<ECS::TagComponent>(spikes);
            sptag.tags.push_back("hazard");
        }

        // --- Lighting (dark atmospheric) ---
        // Dim directional light
        {
            ECS::Entity sun = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(sun, "Ambient Light");
            auto& lt = m_World->AddComponent<ECS::TransformComponent>(sun);
            lt.position = Math::Vector3(0.0f, 20.0f, 0.0f);
            auto& lc = m_World->AddComponent<ECS::LightComponent>(sun);
            lc.type = ECS::LightType::Directional;
            lc.color = Math::Vector3(0.4f, 0.35f, 0.5f);
            lc.intensity = 0.3f;
            lc.castShadows = true;
        }

        // Point lights in each room for atmosphere
        {
            struct RoomLight {
                const char* name;
                Math::Vector3 pos;
                Math::Vector3 color;
                f32 intensity;
            };
            RoomLight lights[] = {
                {"Light Room A", Math::Vector3(rAx, rAy, -1.0f), Math::Vector3(0.6f, 0.5f, 0.4f), 1.2f},
                {"Light Room B", Math::Vector3(rBx, rBy, -1.0f), Math::Vector3(0.4f, 0.4f, 0.7f), 1.0f},
                {"Light Room C", Math::Vector3(rCx, rCy, -1.0f), Math::Vector3(0.7f, 0.3f, 0.3f), 1.0f},
                {"Light Room D", Math::Vector3(rDx, rDy, -1.0f), Math::Vector3(0.3f, 0.5f, 0.4f), 0.8f},
                {"Light Room E", Math::Vector3(rEx, rEy, -1.0f), Math::Vector3(0.5f, 0.5f, 0.6f), 1.5f},
                {"Light Room F", Math::Vector3(rFx, rFy, -1.0f), Math::Vector3(0.6f, 0.3f, 0.6f), 1.0f}
            };
            for (auto& rl : lights) {
                ECS::Entity le = m_World->CreateEntity();
                m_World->AddComponent<ECS::NameComponent>(le, rl.name);
                auto& lt = m_World->AddComponent<ECS::TransformComponent>(le);
                lt.position = rl.pos;
                auto& lc = m_World->AddComponent<ECS::LightComponent>(le);
                lc.type = ECS::LightType::Point;
                lc.color = rl.color;
                lc.intensity = rl.intensity;
                lc.range = ROOM_W * 0.8f;
            }
        }

        // --- Skybox: dark underground ---
        {
            Renderer::SkyboxConfig skyConfig;
            skyConfig.type = Renderer::SkyboxType::Procedural;
            skyConfig.topColor = Math::Vector3(0.03f, 0.02f, 0.06f);
            skyConfig.horizonColor = Math::Vector3(0.06f, 0.04f, 0.09f);
            skyConfig.bottomColor = Math::Vector3(0.01f, 0.01f, 0.03f);
            m_RenderSystem->SetSkybox(skyConfig);
        }

        // Render settings
        m_RenderSystem->SetAmbientIntensity(0.08f);

        // Post-processing
        if (m_PostProcessing) {
            auto& pp = m_PostProcessing->GetSettings();
            pp.fxaaEnabled = 1;
            pp.vignetteEnabled = 1;
            pp.vignetteIntensity = 0.3f;
        }
    }

    else if (templateId == "vampsurvivor") {
        // Large grassy field
        createGround();
        // Bright overhead sunlight
        {
            ECS::Entity sun = createLight();
            auto* lt = m_World->GetComponent<ECS::TransformComponent>(sun);
            if (lt) lt->position = Math::Vector3(0.0f, 20.0f, 0.0f);
            auto* lc = m_World->GetComponent<ECS::LightComponent>(sun);
            if (lc) {
                lc->intensity = 1.6f;
                lc->castShadows = true;
            }
        }

        // Player entity - fast top-down survivor
        ECS::Entity player = createPlayer3D("Player");
        auto& ctrl = m_World->AddComponent<ECS::TopDown3DController>(player);
        ctrl.moveSpeed = 8.0f;
        ctrl.enableClickToMove = false;
        ctrl.rotateToFaceMovement = true;
        ctrl.rotationSpeed = 900.0f;
        SetupCameraForController(player, "TopDown3D");
        // Override camera: high angle looking straight down, far distance
        ctrl.cameraAngle = 80.0f;
        ctrl.cameraDistance = 25.0f;
        ctrl.cameraHeight = 22.0f;
        ctrl.lockCameraToPlayer = true;

        // Player gameplay components
        auto& health = m_World->AddComponent<ECS::HealthComponent>(player);
        health.maxHealth = 200.0f;
        health.currentHealth = 200.0f;
        health.regenRate = 1.0f;
        health.regenDelay = 5.0f;
        health.invulnerabilityTime = 0.5f;
        auto& inv = m_World->AddComponent<ECS::InventoryComponent>(player);
        inv.maxSlots = 10;
        auto& playerTag = m_World->AddComponent<ECS::TagComponent>(player);
        playerTag.tags.push_back("player");
        playerTag.tags.push_back("survivor");

        // --- Enemy swarm spawned in a circle around the player ---
        const int enemyCount = 10;
        const f32 spawnRadius = 18.0f;
        Math::Vector3 enemyColors[] = {
            Math::Vector3(0.7f, 0.15f, 0.1f),
            Math::Vector3(0.6f, 0.1f, 0.3f),
            Math::Vector3(0.5f, 0.2f, 0.5f),
        };
        for (int i = 0; i < enemyCount; ++i) {
            f32 angle = (f32)i / (f32)enemyCount * 6.28318f;
            f32 px = std::cos(angle) * spawnRadius;
            f32 pz = std::sin(angle) * spawnRadius;

            char ename[32];
            snprintf(ename, sizeof(ename), "Enemy_%d", i + 1);
            ECS::Entity enemy = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(enemy, ename);
            auto& et = m_World->AddComponent<ECS::TransformComponent>(enemy);
            et.position = Math::Vector3(px, 0.5f, pz);
            et.scale = Math::Vector3(0.8f);
            m_World->AddComponent<ECS::MeshComponent>(enemy, Renderer::MeshFactory::CreateSphere(0.5f));
            auto& em = m_World->AddComponent<ECS::MaterialComponent>(enemy);
            em.baseColor = enemyColors[i % 3];
            auto& ai = m_World->AddComponent<ECS::AIControllerComponent>(enemy);
            ai.currentState = ECS::AIControllerComponent::AIState::Chase;
            ai.moveSpeed = 2.5f;
            ai.detectionRange = 30.0f;
            ai.attackRange = 1.5f;
            auto& eh = m_World->AddComponent<ECS::HealthComponent>(enemy);
            eh.maxHealth = 15.0f;
            eh.currentHealth = 15.0f;
            auto& dmg = m_World->AddComponent<ECS::DamageComponent>(enemy);
            dmg.damage = 5.0f;
            dmg.destroyOnHit = false;
            dmg.damageInterval = 1.0f;
            auto& etag = m_World->AddComponent<ECS::TagComponent>(enemy);
            etag.tags.push_back("enemy");
            etag.tags.push_back("wave_1");
            m_World->AddComponent<ECS::BehaviorTreeComponent>(enemy);
        }

        // --- XP gem pickups scattered around ---
        const int xpGemCount = 12;
        for (int i = 0; i < xpGemCount; ++i) {
            f32 angle = (f32)i / (f32)xpGemCount * 6.28318f;
            f32 radius = 6.0f + (f32)(i % 3) * 4.0f;
            f32 gx = std::cos(angle) * radius;
            f32 gz = std::sin(angle) * radius;

            char gname[32];
            snprintf(gname, sizeof(gname), "XP_Gem_%d", i + 1);
            ECS::Entity gem = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(gem, gname);
            auto& gt = m_World->AddComponent<ECS::TransformComponent>(gem);
            gt.position = Math::Vector3(gx, 0.3f, gz);
            gt.scale = Math::Vector3(0.25f);
            m_World->AddComponent<ECS::MeshComponent>(gem, Renderer::MeshFactory::CreateSphere(0.5f));
            auto& gmat = m_World->AddComponent<ECS::MaterialComponent>(gem);
            gmat.baseColor = Math::Vector3(0.1f, 0.9f, 0.7f);
            gmat.emissiveColor = Math::Vector3(0.05f, 0.5f, 0.35f);
            gmat.emissiveStrength = 0.8f;
            auto& pc = m_World->AddComponent<ECS::PickupComponent>(gem);
            pc.type = ECS::PickupComponent::PickupType::Custom;
            pc.customId = "xp_gem";
            pc.value = 5.0f;
            pc.pickupRange = 1.5f;
            pc.magnetToPlayer = true;
            pc.magnetRange = 4.0f;
            pc.magnetSpeed = 8.0f;
            auto& gtag = m_World->AddComponent<ECS::TagComponent>(gem);
            gtag.tags.push_back("xp_gem");

            // Bobbing tween
            auto& tw = m_World->AddComponent<ECS::TweenComponent>(gem);
            tw.autoPlay = true;
            ECS::TweenEntry bob;
            bob.property = ECS::TweenProperty::Position;
            bob.easing = ECS::EasingType::EaseInOutSine;
            bob.mode = ECS::TweenMode::PingPong;
            bob.startValue = Math::Vector3(gx, 0.3f, gz);
            bob.endValue = Math::Vector3(gx, 0.8f, gz);
            bob.duration = 1.5f;
            bob.useCurrentAsStart = false;
            tw.tweens.push_back(bob);
        }

        // --- Health pickup orbs ---
        Math::Vector3 healthPositions[] = {
            Math::Vector3(-10.0f, 0.4f, 10.0f),
            Math::Vector3(10.0f, 0.4f, -10.0f),
            Math::Vector3(12.0f, 0.4f, 8.0f),
        };
        for (int i = 0; i < 3; ++i) {
            char hname[32];
            snprintf(hname, sizeof(hname), "Health_Orb_%d", i + 1);
            ECS::Entity orb = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(orb, hname);
            auto& ht = m_World->AddComponent<ECS::TransformComponent>(orb);
            ht.position = healthPositions[i];
            ht.scale = Math::Vector3(0.35f);
            m_World->AddComponent<ECS::MeshComponent>(orb, Renderer::MeshFactory::CreateSphere(0.5f));
            auto& hmat = m_World->AddComponent<ECS::MaterialComponent>(orb);
            hmat.baseColor = Math::Vector3(0.9f, 0.2f, 0.2f);
            hmat.emissiveColor = Math::Vector3(0.5f, 0.1f, 0.1f);
            hmat.emissiveStrength = 0.6f;
            auto& hpc = m_World->AddComponent<ECS::PickupComponent>(orb);
            hpc.type = ECS::PickupComponent::PickupType::Health;
            hpc.value = 50.0f;
            hpc.pickupRange = 1.5f;
            hpc.magnetToPlayer = true;
            hpc.magnetRange = 3.0f;
        }

        // --- HUD text entities ---
        // Timer
        {
            ECS::Entity timer = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(timer, "HUD Timer");
            auto& tt = m_World->AddComponent<ECS::TransformComponent>(timer);
            tt.position = Math::Vector3(0.0f, 8.0f, 0.0f);
            auto& tc = m_World->AddComponent<ECS::TextComponent>(timer);
            tc.text = "Time: 0:00";
            tc.fontSize = 28.0f;
            tc.textColor = Math::Vector3(1.0f, 1.0f, 1.0f);
            tc.bgOpacity = 0.0f;
            tc.horizontalAlign = ECS::TextAlign::Center;
            auto& ttag = m_World->AddComponent<ECS::TagComponent>(timer);
            ttag.tags.push_back("hud_timer");
        }
        // Level / XP display
        {
            ECS::Entity lvl = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(lvl, "HUD Level");
            auto& lt = m_World->AddComponent<ECS::TransformComponent>(lvl);
            lt.position = Math::Vector3(-3.0f, 7.0f, 0.0f);
            auto& tc = m_World->AddComponent<ECS::TextComponent>(lvl);
            tc.text = "Lv 1  XP: 0/10";
            tc.fontSize = 24.0f;
            tc.textColor = Math::Vector3(0.6f, 1.0f, 0.6f);
            tc.bgOpacity = 0.0f;
            auto& ltag = m_World->AddComponent<ECS::TagComponent>(lvl);
            ltag.tags.push_back("hud_level");
        }
        // Wave counter
        {
            ECS::Entity wave = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(wave, "HUD Wave");
            auto& wt = m_World->AddComponent<ECS::TransformComponent>(wave);
            wt.position = Math::Vector3(3.0f, 7.0f, 0.0f);
            auto& tc = m_World->AddComponent<ECS::TextComponent>(wave);
            tc.text = "Wave 1";
            tc.fontSize = 24.0f;
            tc.textColor = Math::Vector3(1.0f, 0.8f, 0.3f);
            tc.bgOpacity = 0.0f;
            auto& wtag = m_World->AddComponent<ECS::TagComponent>(wave);
            wtag.tags.push_back("hud_wave");
        }

        // --- Arena boundary pillars (4 corners) ---
        const f32 arenaHalf = 24.0f;
        Math::Vector3 pillarPositions[] = {
            Math::Vector3(-arenaHalf, 2.5f,  arenaHalf),
            Math::Vector3( arenaHalf, 2.5f,  arenaHalf),
            Math::Vector3( arenaHalf, 2.5f, -arenaHalf),
            Math::Vector3(-arenaHalf, 2.5f, -arenaHalf),
        };
        for (int i = 0; i < 4; ++i) {
            char pname[32];
            snprintf(pname, sizeof(pname), "Boundary_Pillar_%d", i + 1);
            ECS::Entity pillar = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(pillar, pname);
            auto& pt = m_World->AddComponent<ECS::TransformComponent>(pillar);
            pt.position = pillarPositions[i];
            pt.scale = Math::Vector3(0.6f, 5.0f, 0.6f);
            m_World->AddComponent<ECS::MeshComponent>(pillar, Renderer::MeshFactory::CreateCube(1.0f));
            auto& pm = m_World->AddComponent<ECS::MaterialComponent>(pillar);
            pm.baseColor = Math::Vector3(0.5f, 0.5f, 0.55f);
            pm.roughness = 0.4f;
            pm.emissiveColor = Math::Vector3(0.15f, 0.15f, 0.3f);
            pm.emissiveStrength = 0.3f;
            auto& col = m_World->AddComponent<ECS::BoxColliderComponent>(pillar);
            col.size = Math::Vector3(0.6f, 5.0f, 0.6f);
        }

        // --- Arena boundary trigger zone ---
        {
            ECS::Entity boundary = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(boundary, "Arena Boundary");
            auto& bt = m_World->AddComponent<ECS::TransformComponent>(boundary);
            bt.position = Math::Vector3(0.0f, 0.0f, 0.0f);
            bt.scale = Math::Vector3(arenaHalf * 2.0f, 5.0f, arenaHalf * 2.0f);
            auto& trigger = m_World->AddComponent<ECS::TriggerZoneComponent>(boundary);
            trigger.shape = ECS::TriggerZoneComponent::Shape::Box;
            auto& btag = m_World->AddComponent<ECS::TagComponent>(boundary);
            btag.tags.push_back("arena_boundary");
        }

        // Bright midday procedural skybox
        {
            Renderer::SkyboxConfig skyConfig;
            skyConfig.type = Renderer::SkyboxType::Procedural;
            skyConfig.topColor = Math::Vector3(0.2f, 0.5f, 0.95f);
            skyConfig.horizonColor = Math::Vector3(0.6f, 0.8f, 1.0f);
            skyConfig.bottomColor = Math::Vector3(0.35f, 0.55f, 0.3f);
            skyConfig.sunDirection = Math::Vector3(0.0f, 1.0f, 0.2f);
            m_RenderSystem->SetSkybox(skyConfig);
        }

        // Render settings
        m_RenderSystem->SetShadowsEnabled(true);

        // Post-processing
        if (m_PostProcessing) {
            auto& pp = m_PostProcessing->GetSettings();
            pp.fxaaEnabled = 1;
        }

        // Player magic aura
        {
            ECS::Entity aura = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(aura, "Player Aura");
            auto& at = m_World->AddComponent<ECS::TransformComponent>(aura);
            at.position = Math::Vector3(0.0f, 1.0f, 0.0f);
            auto& pe = m_World->AddComponent<ECS::ParticleEmitterComponent>(aura);
            ECS::ApplyParticlePreset(pe, "Magic");
        }

        // Level-up zone (golden pillar of light, triggers level-up UI)
        {
            ECS::Entity lvlUp = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(lvlUp, "Level Up Zone");
            auto& lt = m_World->AddComponent<ECS::TransformComponent>(lvlUp);
            lt.position = Math::Vector3(0.0f, 1.5f, 0.0f);
            lt.scale = Math::Vector3(2.0f, 3.0f, 2.0f);
            m_World->AddComponent<ECS::MeshComponent>(lvlUp, Renderer::MeshFactory::CreateCube(1.0f));
            auto& lm = m_World->AddComponent<ECS::MaterialComponent>(lvlUp);
            lm.baseColor = Math::Vector3(1.0f, 0.9f, 0.3f);
            lm.opacity = 0.15f;
            lm.alphaMode = ECS::MaterialComponent::AlphaMode::Blend;
            lm.emissiveColor = Math::Vector3(1.0f, 0.8f, 0.2f);
            lm.emissiveStrength = 0.3f;
            auto& trigger = m_World->AddComponent<ECS::TriggerZoneComponent>(lvlUp);
            trigger.shape = ECS::TriggerZoneComponent::Shape::Box;
            trigger.boxSize = Math::Vector3(2.0f, 3.0f, 2.0f);
            auto& ltag = m_World->AddComponent<ECS::TagComponent>(lvlUp);
            ltag.tags.push_back("level_up_zone");
            auto& lnotes = m_World->AddComponent<ECS::NotesComponent>(lvlUp);
            lnotes.notes = "Level-up zone — spawns when player has enough XP.\n"
                "Step inside to choose an upgrade (weapon, speed, max HP, etc.).\n"
                "Hide by default, make visible=true when XP threshold is reached.";
            lt.visible = false;  // Initially hidden
        }

        // Wave counter text
        {
            ECS::Entity waveText = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(waveText, "Wave Counter");
            auto& wt = m_World->AddComponent<ECS::TransformComponent>(waveText);
            wt.position = Math::Vector3(0.0f, 5.0f, 5.0f);
            auto& wtext = m_World->AddComponent<ECS::TextComponent>(waveText);
            wtext.text = "Wave: 1";
            wtext.fontSize = 40.0f;
            wtext.textColor = Math::Vector3(1.0f, 0.8f, 0.2f);
            auto& wtag = m_World->AddComponent<ECS::TagComponent>(waveText);
            wtag.tags.push_back("wave_counter");
        }
    }

    else if (templateId == "roguelike") {
        // 2D Roguelike — grid-based dungeon crawl in XY plane
        // Top-down camera looking at XY, Z=0 is the play plane

        const f32 CELL = 1.0f;  // Grid cell size (matches gridCellSize)

        // --- Dungeon layout (10x8 grid) ---
        // 0=wall, 1=floor, 2=door, 3=enemy, 4=stairs, 5=treasure
        const int COLS = 10;
        const int ROWS = 8;
        int layout[ROWS][COLS] = {
            {0,0,0,0,0,0,0,0,0,0},
            {0,1,1,1,0,1,1,1,1,0},
            {0,1,0,1,0,1,0,0,1,0},
            {0,1,0,2,1,1,3,0,1,0},
            {0,1,0,1,0,0,1,0,1,0},
            {0,1,3,1,1,5,1,1,1,0},
            {0,1,0,0,0,1,0,3,4,0},
            {0,0,0,0,0,0,0,0,0,0},
        };

        // Palette
        Math::Vector3 wallColor(0.12f, 0.10f, 0.14f);
        Math::Vector3 floorColor(0.25f, 0.24f, 0.22f);
        Math::Vector3 doorColor(0.50f, 0.35f, 0.15f);
        Math::Vector3 enemyColor(0.75f, 0.15f, 0.10f);
        Math::Vector3 stairsColor(0.55f, 0.55f, 0.60f);
        Math::Vector3 treasureColor(0.85f, 0.75f, 0.20f);
        Math::Vector3 keyColor(0.90f, 0.80f, 0.10f);
        Math::Vector3 potionColor(0.20f, 0.80f, 0.25f);

        int wallIdx = 0;
        int floorIdx = 0;
        int enemyIdx = 0;
        char nameBuf[64];

        // --- Player (grid cell 1,1) ---
        ECS::Entity player = createPlayer2D("Player");
        {
            auto* pt = m_World->GetComponent<ECS::TransformComponent>(player);
            pt->position = Math::Vector3(1.0f * CELL + CELL * 0.5f,
                                         1.0f * CELL + CELL * 0.5f, 0.0f);
            auto& ctrl = m_World->AddComponent<ECS::TopDown2DController>(player);
            ctrl.moveSpeed = 5.0f;
            ctrl.gridMovement = true;
            ctrl.gridCellSize = CELL;
            ctrl.rotateToFaceMovement = false;
            SetupCameraForController(player, "TopDown2D");

            auto& health = m_World->AddComponent<ECS::HealthComponent>(player);
            health.maxHealth = 10.0f;
            health.currentHealth = 10.0f;
            health.invulnerabilityTime = 0.5f;

            m_World->AddComponent<ECS::InventoryComponent>(player);
            auto& notes = m_World->AddComponent<ECS::NotesComponent>(player);
            notes.notes = "TIP: Use ProceduralAlgorithms (CellularAutomata, BSP, WFC) to\n"
                "generate dungeon layouts at runtime instead of hand-crafting tiles.";
        }

        // --- Build dungeon tiles ---
        for (int row = 0; row < ROWS; ++row) {
            for (int col = 0; col < COLS; ++col) {
                f32 cx = static_cast<f32>(col) * CELL + CELL * 0.5f;
                f32 cy = static_cast<f32>(row) * CELL + CELL * 0.5f;

                if (layout[row][col] == 0) {
                    // Wall tile — dark cube with collider
                    snprintf(nameBuf, sizeof(nameBuf), "Wall_%d", wallIdx++);
                    ECS::Entity wall = m_World->CreateEntity();
                    m_World->AddComponent<ECS::NameComponent>(wall, nameBuf);
                    auto& wt = m_World->AddComponent<ECS::TransformComponent>(wall);
                    wt.position = Math::Vector3(cx, cy, 0.0f);
                    wt.scale = Math::Vector3(CELL, CELL, CELL);
                    auto& wmat = m_World->AddComponent<ECS::MaterialComponent>(wall);
                    wmat.baseColor = wallColor;
                    wmat.roughness = 0.95f;
                    m_World->AddComponent<ECS::MeshComponent>(wall,
                        Renderer::MeshFactory::CreateCube(1.0f));
                    auto& wcol = m_World->AddComponent<ECS::BoxColliderComponent>(wall);
                    wcol.size = Math::Vector3(CELL, CELL, CELL);
                } else {
                    // Floor tile — lighter quad slightly behind entities
                    snprintf(nameBuf, sizeof(nameBuf), "Floor_%d", floorIdx++);
                    ECS::Entity floor = m_World->CreateEntity();
                    m_World->AddComponent<ECS::NameComponent>(floor, nameBuf);
                    auto& ft = m_World->AddComponent<ECS::TransformComponent>(floor);
                    ft.position = Math::Vector3(cx, cy, -0.05f);
                    ft.scale = Math::Vector3(CELL, CELL, 1.0f);
                    auto& fmat = m_World->AddComponent<ECS::MaterialComponent>(floor);
                    fmat.baseColor = floorColor;
                    fmat.roughness = 0.9f;
                    m_World->AddComponent<ECS::MeshComponent>(floor,
                        Renderer::MeshFactory::CreateQuad(1.0f, 1.0f));

                    // --- Special cells ---
                    if (layout[row][col] == 2) {
                        // Locked door
                        snprintf(nameBuf, sizeof(nameBuf), "Door_%d_%d", col, row);
                        ECS::Entity door = m_World->CreateEntity();
                        m_World->AddComponent<ECS::NameComponent>(door, nameBuf);
                        auto& dt = m_World->AddComponent<ECS::TransformComponent>(door);
                        dt.position = Math::Vector3(cx, cy, 0.0f);
                        dt.scale = Math::Vector3(CELL * 0.9f, CELL * 0.9f, 0.5f);
                        auto& dmat = m_World->AddComponent<ECS::MaterialComponent>(door);
                        dmat.baseColor = doorColor;
                        dmat.roughness = 0.6f;
                        m_World->AddComponent<ECS::MeshComponent>(door,
                            Renderer::MeshFactory::CreateCube(1.0f));
                        auto& dcol = m_World->AddComponent<ECS::BoxColliderComponent>(door);
                        dcol.size = Math::Vector3(CELL * 0.9f, CELL * 0.9f, 0.5f);
                        auto& dtag = m_World->AddComponent<ECS::TagComponent>(door);
                        dtag.tags.push_back("door");
                        dtag.tags.push_back("locked");
                        auto& lock = m_World->AddComponent<ECS::LockComponent>(door);
                        lock.requiredKey = "dungeon_key";
                        lock.isLocked = true;
                        lock.consumeKey = true;
                        lock.openMode = ECS::LockComponent::OpenMode::OpenOnly;
                        lock.lockedPrompt = "Locked - requires key";
                        lock.unlockedPrompt = "Open";
                        auto& dinteract = m_World->AddComponent<ECS::InteractableComponent>(door);
                        dinteract.promptText = "Locked";
                        dinteract.interactionRange = CELL;
                    }
                    else if (layout[row][col] == 3) {
                        // Enemy — melee creature
                        snprintf(nameBuf, sizeof(nameBuf), "Enemy_%d", enemyIdx++);
                        ECS::Entity enemy = m_World->CreateEntity();
                        m_World->AddComponent<ECS::NameComponent>(enemy, nameBuf);
                        auto& et = m_World->AddComponent<ECS::TransformComponent>(enemy);
                        et.position = Math::Vector3(cx, cy, 0.0f);
                        et.scale = Math::Vector3(0.7f, 0.7f, 0.7f);
                        auto& emat = m_World->AddComponent<ECS::MaterialComponent>(enemy);
                        emat.baseColor = enemyColor;
                        emat.roughness = 0.7f;
                        m_World->AddComponent<ECS::MeshComponent>(enemy,
                            Renderer::MeshFactory::CreateCapsule2D(0.6f, 0.8f));
                        m_World->AddComponent<ECS::Sprite2DComponent>(enemy);
                        auto& ai = m_World->AddComponent<ECS::AIControllerComponent>(enemy);
                        ai.currentState = ECS::AIControllerComponent::AIState::Patrol;
                        ai.moveSpeed = 2.0f;
                        ai.detectionRange = 3.0f;
                        ai.attackRange = CELL;
                        ai.attackDamage = 2.0f;
                        auto& eh = m_World->AddComponent<ECS::HealthComponent>(enemy);
                        eh.maxHealth = 3.0f;
                        eh.currentHealth = 3.0f;
                        auto& edmg = m_World->AddComponent<ECS::DamageComponent>(enemy);
                        edmg.damage = 2.0f;
                        auto& etag = m_World->AddComponent<ECS::TagComponent>(enemy);
                        etag.tags.push_back("enemy");
                        etag.tags.push_back("melee");
                    }
                    else if (layout[row][col] == 4) {
                        // Stairs down — level exit
                        snprintf(nameBuf, sizeof(nameBuf), "Stairs_%d_%d", col, row);
                        ECS::Entity stairs = m_World->CreateEntity();
                        m_World->AddComponent<ECS::NameComponent>(stairs, nameBuf);
                        auto& st = m_World->AddComponent<ECS::TransformComponent>(stairs);
                        st.position = Math::Vector3(cx, cy, 0.01f);
                        st.scale = Math::Vector3(CELL * 0.8f, CELL * 0.8f, 1.0f);
                        auto& smat = m_World->AddComponent<ECS::MaterialComponent>(stairs);
                        smat.baseColor = stairsColor;
                        smat.roughness = 0.5f;
                        m_World->AddComponent<ECS::MeshComponent>(stairs,
                            Renderer::MeshFactory::CreateQuad(1.0f, 1.0f));
                        auto& stag = m_World->AddComponent<ECS::TagComponent>(stairs);
                        stag.tags.push_back("stairs_down");
                        auto& sinteract = m_World->AddComponent<ECS::InteractableComponent>(stairs);
                        sinteract.promptText = "Descend";
                        sinteract.interactionRange = CELL;
                        auto& trigger = m_World->AddComponent<ECS::TriggerZoneComponent>(stairs);
                        trigger.shape = ECS::TriggerZoneComponent::Shape::Box;
                        trigger.boxSize = Math::Vector3(CELL * 0.6f, CELL * 0.6f, 1.0f);
                        trigger.triggerOnce = true;
                    }
                    else if (layout[row][col] == 5) {
                        // Treasure chest
                        snprintf(nameBuf, sizeof(nameBuf), "Treasure_%d_%d", col, row);
                        ECS::Entity chest = m_World->CreateEntity();
                        m_World->AddComponent<ECS::NameComponent>(chest, nameBuf);
                        auto& ct = m_World->AddComponent<ECS::TransformComponent>(chest);
                        ct.position = Math::Vector3(cx, cy, 0.0f);
                        ct.scale = Math::Vector3(0.6f, 0.5f, 0.5f);
                        auto& cmat = m_World->AddComponent<ECS::MaterialComponent>(chest);
                        cmat.baseColor = treasureColor;
                        cmat.roughness = 0.4f;
                        cmat.metallic = 0.6f;
                        m_World->AddComponent<ECS::MeshComponent>(chest,
                            Renderer::MeshFactory::CreateCube(1.0f));
                        auto& ctag = m_World->AddComponent<ECS::TagComponent>(chest);
                        ctag.tags.push_back("treasure");
                        auto& cinteract = m_World->AddComponent<ECS::InteractableComponent>(chest);
                        cinteract.promptText = "Open Chest";
                        cinteract.interactionRange = CELL;
                        cinteract.singleUse = true;
                    }
                }
            }
        }

        // --- Key pickup (placed on floor cell 1,5 area) ---
        {
            ECS::Entity key = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(key, "Dungeon Key");
            auto& kt = m_World->AddComponent<ECS::TransformComponent>(key);
            kt.position = Math::Vector3(1.0f * CELL + CELL * 0.5f,
                                        5.0f * CELL + CELL * 0.5f, 0.0f);
            kt.scale = Math::Vector3(0.3f, 0.3f, 0.3f);
            auto& kmat = m_World->AddComponent<ECS::MaterialComponent>(key);
            kmat.baseColor = keyColor;
            kmat.emissiveColor = Math::Vector3(0.5f, 0.45f, 0.05f);
            kmat.emissiveStrength = 0.6f;
            m_World->AddComponent<ECS::MeshComponent>(key,
                Renderer::MeshFactory::CreateSphere(0.5f));
            auto& kpc = m_World->AddComponent<ECS::PickupComponent>(key);
            kpc.type = ECS::PickupComponent::PickupType::Key;
            kpc.value = 1.0f;
            kpc.pickupRange = CELL * 0.8f;
            kpc.bobSpeed = 3.0f;
            kpc.bobHeight = 0.1f;
            auto& ktag = m_World->AddComponent<ECS::TagComponent>(key);
            ktag.tags.push_back("dungeon_key");

            // Bobbing tween
            f32 keyX = 1.0f * CELL + CELL * 0.5f;
            f32 keyY = 5.0f * CELL + CELL * 0.5f;
            auto& tw = m_World->AddComponent<ECS::TweenComponent>(key);
            tw.autoPlay = true;
            ECS::TweenEntry bob;
            bob.property = ECS::TweenProperty::Position;
            bob.easing = ECS::EasingType::EaseInOutSine;
            bob.mode = ECS::TweenMode::PingPong;
            bob.startValue = Math::Vector3(keyX, keyY, 0.0f);
            bob.endValue = Math::Vector3(keyX, keyY + 0.3f, 0.0f);
            bob.duration = 1.5f;
            bob.useCurrentAsStart = false;
            tw.tweens.push_back(bob);
        }

        // --- Health potion (placed on floor cell 5,1 area) ---
        {
            ECS::Entity potion = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(potion, "Health Potion");
            auto& pt = m_World->AddComponent<ECS::TransformComponent>(potion);
            pt.position = Math::Vector3(5.0f * CELL + CELL * 0.5f,
                                        1.0f * CELL + CELL * 0.5f, 0.0f);
            pt.scale = Math::Vector3(0.25f, 0.25f, 0.25f);
            auto& pmat = m_World->AddComponent<ECS::MaterialComponent>(potion);
            pmat.baseColor = potionColor;
            pmat.emissiveColor = Math::Vector3(0.1f, 0.4f, 0.1f);
            pmat.emissiveStrength = 0.5f;
            m_World->AddComponent<ECS::MeshComponent>(potion,
                Renderer::MeshFactory::CreateSphere(0.5f));
            auto& ppc = m_World->AddComponent<ECS::PickupComponent>(potion);
            ppc.type = ECS::PickupComponent::PickupType::Health;
            ppc.value = 5.0f;
            ppc.pickupRange = CELL * 0.8f;
            ppc.bobSpeed = 2.5f;
            ppc.bobHeight = 0.08f;
        }

        // --- HUD: Floor indicator ---
        {
            ECS::Entity floorHud = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(floorHud, "HUD Floor");
            auto& ht = m_World->AddComponent<ECS::TransformComponent>(floorHud);
            ht.position = Math::Vector3(0.5f, 7.5f, 0.0f);
            auto& htext = m_World->AddComponent<ECS::TextComponent>(floorHud);
            htext.text = "Floor 1";
            htext.fontSize = 22.0f;
            htext.textColor = Math::Vector3(0.9f, 0.85f, 0.6f);
            auto& htag = m_World->AddComponent<ECS::TagComponent>(floorHud);
            htag.tags.push_back("hud_floor");
        }

        // --- HUD: Health display ---
        {
            ECS::Entity hpHud = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(hpHud, "HUD Health");
            auto& ht = m_World->AddComponent<ECS::TransformComponent>(hpHud);
            ht.position = Math::Vector3(8.0f, 7.5f, 0.0f);
            auto& htext = m_World->AddComponent<ECS::TextComponent>(hpHud);
            htext.text = "HP: 10/10";
            htext.fontSize = 22.0f;
            htext.textColor = Math::Vector3(0.9f, 0.25f, 0.2f);
            auto& htag = m_World->AddComponent<ECS::TagComponent>(hpHud);
            htag.tags.push_back("hud_health");
        }

        // --- Trap tile (hidden spike trap) ---
        {
            ECS::Entity trap = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(trap, "Spike Trap");
            auto& tt = m_World->AddComponent<ECS::TransformComponent>(trap);
            tt.position = Math::Vector3(3.0f * CELL, -3.0f * CELL, 0.0f);  // On a floor tile
            tt.scale = Math::Vector3(CELL * 0.9f, CELL * 0.9f, 1.0f);
            auto& tm = m_World->AddComponent<ECS::MaterialComponent>(trap);
            tm.baseColor = Math::Vector3(0.35f, 0.25f, 0.2f);  // Slightly different floor color
            m_World->AddComponent<ECS::MeshComponent>(trap, Renderer::MeshFactory::CreateQuad(1.0f, 1.0f));
            auto& tdmg = m_World->AddComponent<ECS::DamageComponent>(trap);
            tdmg.damage = 3.0f;
            auto& ttrigger = m_World->AddComponent<ECS::TriggerZoneComponent>(trap);
            ttrigger.shape = ECS::TriggerZoneComponent::Shape::Box;
            ttrigger.boxSize = Math::Vector3(CELL * 0.8f, CELL * 0.8f, 1.0f);
            auto& ttag = m_World->AddComponent<ECS::TagComponent>(trap);
            ttag.tags.push_back("trap");
        }

        // --- Potion pickup ---
        {
            ECS::Entity potion = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(potion, "Health Potion");
            auto& pt = m_World->AddComponent<ECS::TransformComponent>(potion);
            pt.position = Math::Vector3(1.0f * CELL, -1.0f * CELL, 0.0f);
            pt.scale = Math::Vector3(0.4f, 0.4f, 1.0f);
            auto& pm = m_World->AddComponent<ECS::MaterialComponent>(potion);
            pm.baseColor = Math::Vector3(0.8f, 0.15f, 0.2f);
            pm.emissiveColor = Math::Vector3(0.6f, 0.1f, 0.15f);
            pm.emissiveStrength = 0.5f;
            m_World->AddComponent<ECS::MeshComponent>(potion, Renderer::MeshFactory::CreateCapsule2D(0.2f, 0.3f));
            m_World->AddComponent<ECS::Sprite2DComponent>(potion);
            auto& ppc = m_World->AddComponent<ECS::PickupComponent>(potion);
            ppc.type = ECS::PickupComponent::PickupType::Health;
            ppc.value = 5.0f;
            auto& ptag = m_World->AddComponent<ECS::TagComponent>(potion);
            ptag.tags.push_back("potion");
        }

        // --- Dim overhead light ---
        {
            ECS::Entity light = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(light, "Dungeon Light");
            auto& lt = m_World->AddComponent<ECS::TransformComponent>(light);
            lt.position = Math::Vector3(5.0f, 4.0f, 5.0f);
            lt.rotation = Math::Quaternion(Math::Vector3(1, 0, 0), Math::Radians(-60.0f));
            auto& lc = m_World->AddComponent<ECS::LightComponent>(light);
            lc.type = ECS::LightType::Directional;
            lc.color = Math::Vector3(0.7f, 0.65f, 0.55f);
            lc.intensity = 0.6f;
        }

        // Skybox: dark underground
        {
            Renderer::SkyboxConfig skyConfig;
            skyConfig.type = Renderer::SkyboxType::Procedural;
            skyConfig.topColor = Math::Vector3(0.02f, 0.02f, 0.05f);
            skyConfig.horizonColor = Math::Vector3(0.06f, 0.05f, 0.08f);
            skyConfig.bottomColor = Math::Vector3(0.01f, 0.01f, 0.03f);
            skyConfig.sunDirection = Math::Vector3(0.0f, -1.0f, 0.0f);
            m_RenderSystem->SetSkybox(skyConfig);
        }

        // Render settings
        m_RenderSystem->SetAmbientIntensity(0.08f);

        // Post-processing
        if (m_PostProcessing) {
            auto& pp = m_PostProcessing->GetSettings();
            pp.fxaaEnabled = 1;
        }
    }

    else if (templateId == "soulslike") {
        // Dark stone ground plane (large arena)
        {
            ECS::Entity ground = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(ground, "Arena Floor");
            auto& gt = m_World->AddComponent<ECS::TransformComponent>(ground);
            gt.position = Math::Vector3(0.0f, -0.05f, 0.0f);
            gt.scale = Math::Vector3(40.0f, 0.1f, 40.0f);
            auto& gm = m_World->AddComponent<ECS::MaterialComponent>(ground);
            gm.baseColor = Math::Vector3(0.10f, 0.09f, 0.08f);
            gm.roughness = 0.85f;
            m_World->AddComponent<ECS::MeshComponent>(ground, Renderer::MeshFactory::CreateCube(1.0f));
            auto& col = m_World->AddComponent<ECS::BoxColliderComponent>(ground);
            col.size = Math::Vector3(40.0f, 0.1f, 40.0f);
        }

        // Dim directional light — oppressive atmosphere
        {
            ECS::Entity sun = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(sun, "Dim Sun");
            auto& lt = m_World->AddComponent<ECS::TransformComponent>(sun);
            lt.position = Math::Vector3(0.0f, 15.0f, 10.0f);
            lt.rotation = Math::Quaternion(Math::Vector3(1, 0, 0), Math::Radians(-40.0f));
            auto& lc = m_World->AddComponent<ECS::LightComponent>(sun);
            lc.type = ECS::LightType::Directional;
            lc.intensity = 0.25f;
            lc.color = Math::Vector3(0.6f, 0.55f, 0.5f);
            lc.castShadows = true;
        }

        // Player — slow, methodical third-person character
        ECS::Entity player;
        {
            player = createPlayer3D("Player");
            auto* pt = m_World->GetComponent<ECS::TransformComponent>(player);
            if (pt) pt->position = Math::Vector3(0.0f, 1.0f, -12.0f);
            auto* pm = m_World->GetComponent<ECS::MaterialComponent>(player);
            if (pm) pm->baseColor = Math::Vector3(0.25f, 0.25f, 0.30f);
            auto& ctrl = m_World->AddComponent<ECS::ThirdPersonController>(player);
            ctrl.moveSpeed = 3.0f;
            ctrl.acceleration = 20.0f;
            ctrl.deceleration = 18.0f;
            ctrl.cameraDistance = 4.5f;
            ctrl.cameraHeight = 2.0f;
            ctrl.jumpForce = 6.0f;
            ctrl.rotateToFaceMovement = true;
            SetupCameraForController(player, "ThirdPerson");

            auto& health = m_World->AddComponent<ECS::HealthComponent>(player);
            health.maxHealth = 100.0f;
            health.currentHealth = 100.0f;
            health.invulnerabilityTime = 0.8f;

            m_World->AddComponent<ECS::InventoryComponent>(player);

            auto& rb = m_World->AddComponent<ECS::RigidbodyComponent>(player);
            rb.mass = 70.0f;
            rb.useGravity = true;
        }

        // Bonfire save point (emissive orange glow + point light)
        {
            ECS::Entity bonfire = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(bonfire, "Bonfire");
            auto& bt = m_World->AddComponent<ECS::TransformComponent>(bonfire);
            bt.position = Math::Vector3(0.0f, 0.3f, -10.0f);
            bt.scale = Math::Vector3(0.5f, 0.8f, 0.5f);
            m_World->AddComponent<ECS::MeshComponent>(bonfire, Renderer::MeshFactory::CreateSphere(0.5f));
            auto& bm = m_World->AddComponent<ECS::MaterialComponent>(bonfire);
            bm.baseColor = Math::Vector3(0.6f, 0.25f, 0.05f);
            bm.emissiveColor = Math::Vector3(1.0f, 0.5f, 0.1f);
            bm.emissiveStrength = 2.0f;
            bm.roughness = 0.4f;
            auto& btag = m_World->AddComponent<ECS::TagComponent>(bonfire);
            btag.tags.push_back("bonfire");
            auto& bint = m_World->AddComponent<ECS::InteractableComponent>(bonfire);
            bint.interactionRange = 2.5f;
            bint.promptText = "Rest at Bonfire";

            // Bonfire point light
            ECS::Entity bfLight = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(bfLight, "Bonfire Light");
            auto& blt = m_World->AddComponent<ECS::TransformComponent>(bfLight);
            blt.position = Math::Vector3(0.0f, 1.2f, -10.0f);
            auto& blc = m_World->AddComponent<ECS::LightComponent>(bfLight);
            blc.type = ECS::LightType::Point;
            blc.intensity = 2.5f;
            blc.color = Math::Vector3(1.0f, 0.6f, 0.15f);
            blc.range = 12.0f;
        }

        // Fog Gate — tall translucent barrier with emissive
        {
            ECS::Entity fogGate = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(fogGate, "Fog Gate");
            auto& ft = m_World->AddComponent<ECS::TransformComponent>(fogGate);
            ft.position = Math::Vector3(0.0f, 2.5f, -3.0f);
            ft.scale = Math::Vector3(6.0f, 5.0f, 0.15f);
            m_World->AddComponent<ECS::MeshComponent>(fogGate, Renderer::MeshFactory::CreateCube(1.0f));
            auto& fm = m_World->AddComponent<ECS::MaterialComponent>(fogGate);
            fm.baseColor = Math::Vector3(0.7f, 0.75f, 0.9f);
            fm.emissiveColor = Math::Vector3(0.4f, 0.45f, 0.7f);
            fm.emissiveStrength = 1.2f;
            fm.opacity = 0.35f;
            fm.roughness = 0.1f;
            auto& ftag = m_World->AddComponent<ECS::TagComponent>(fogGate);
            ftag.tags.push_back("fog_gate");
            auto& fint = m_World->AddComponent<ECS::InteractableComponent>(fogGate);
            fint.interactionRange = 3.0f;
            fint.promptText = "Enter Fog";
        }

        // Enemies — spread around the arena
        struct EnemyDef {
            const char* name;
            Math::Vector3 pos;
            f32 hp;
            f32 dmg;
            f32 speed;
            Math::Vector3 color;
        };
        EnemyDef enemies[] = {
            {"Hollow Soldier",  Math::Vector3( 6.0f, 1.0f,  2.0f), 60.0f, 12.0f, 2.0f, Math::Vector3(0.35f, 0.30f, 0.25f)},
            {"Hollow Knight",   Math::Vector3(-5.0f, 1.0f,  5.0f), 80.0f, 18.0f, 1.8f, Math::Vector3(0.30f, 0.28f, 0.32f)},
            {"Undead Sentinel", Math::Vector3( 8.0f, 1.2f,  8.0f), 120.0f, 25.0f, 1.5f, Math::Vector3(0.20f, 0.20f, 0.22f)},
            {"Hollow Archer",   Math::Vector3(-7.0f, 1.0f, 10.0f), 45.0f, 15.0f, 2.5f, Math::Vector3(0.40f, 0.32f, 0.28f)},
        };
        for (auto& def : enemies) {
            ECS::Entity enemy = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(enemy, def.name);
            auto& et = m_World->AddComponent<ECS::TransformComponent>(enemy);
            et.position = def.pos;
            m_World->AddComponent<ECS::MeshComponent>(enemy, Renderer::MeshFactory::CreateCapsule(0.3f, 1.0f));
            auto& em = m_World->AddComponent<ECS::MaterialComponent>(enemy);
            em.baseColor = def.color;
            em.roughness = 0.7f;
            auto& ai = m_World->AddComponent<ECS::AIControllerComponent>(enemy);
            ai.currentState = ECS::AIControllerComponent::AIState::Patrol;
            ai.moveSpeed = def.speed;
            ai.detectionRange = 8.0f;
            ai.attackRange = 2.0f;
            auto& eh = m_World->AddComponent<ECS::HealthComponent>(enemy);
            eh.maxHealth = def.hp;
            eh.currentHealth = def.hp;
            auto& dmg = m_World->AddComponent<ECS::DamageComponent>(enemy);
            dmg.damage = def.dmg;
            dmg.knockbackForce = 3.0f;
            auto& col = m_World->AddComponent<ECS::BoxColliderComponent>(enemy);
            col.size = Math::Vector3(0.6f, 1.8f, 0.6f);
            m_World->AddComponent<ECS::BehaviorTreeComponent>(enemy);
        }

        // Treasure Chest
        {
            ECS::Entity chest = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(chest, "Treasure Chest");
            auto& ct = m_World->AddComponent<ECS::TransformComponent>(chest);
            ct.position = Math::Vector3(10.0f, 0.3f, 6.0f);
            ct.scale = Math::Vector3(0.8f, 0.6f, 0.5f);
            m_World->AddComponent<ECS::MeshComponent>(chest, Renderer::MeshFactory::CreateCube(1.0f));
            auto& cm = m_World->AddComponent<ECS::MaterialComponent>(chest);
            cm.baseColor = Math::Vector3(0.45f, 0.30f, 0.12f);
            cm.roughness = 0.65f;
            auto& ci = m_World->AddComponent<ECS::InteractableComponent>(chest);
            ci.interactionRange = 2.0f;
            ci.promptText = "Open Chest";
            ci.singleUse = true;
            auto& lock = m_World->AddComponent<ECS::LockComponent>(chest);
            lock.requiredKey = "";
            lock.isLocked = false;
            lock.openMode = ECS::LockComponent::OpenMode::OpenOnly;
            auto& inv = m_World->AddComponent<ECS::InventoryComponent>(chest);
            inv.maxSlots = 3;
            auto& col = m_World->AddComponent<ECS::BoxColliderComponent>(chest);
            col.size = Math::Vector3(0.8f, 0.6f, 0.5f);
        }

        // Stone pillars — arena structure
        Math::Vector3 pillarPositions[] = {
            Math::Vector3(-8.0f, 2.0f,  0.0f),
            Math::Vector3( 8.0f, 2.0f,  0.0f),
            Math::Vector3(-4.0f, 2.0f,  8.0f),
            Math::Vector3( 4.0f, 2.0f,  8.0f),
            Math::Vector3( 0.0f, 2.0f, 12.0f),
        };
        for (int i = 0; i < 5; ++i) {
            ECS::Entity pillar = m_World->CreateEntity();
            char name[32]; snprintf(name, sizeof(name), "Stone Pillar %d", i + 1);
            m_World->AddComponent<ECS::NameComponent>(pillar, name);
            auto& pt = m_World->AddComponent<ECS::TransformComponent>(pillar);
            pt.position = pillarPositions[i];
            pt.scale = Math::Vector3(1.0f, 4.0f, 1.0f);
            m_World->AddComponent<ECS::MeshComponent>(pillar, Renderer::MeshFactory::CreateCube(1.0f));
            auto& pm = m_World->AddComponent<ECS::MaterialComponent>(pillar);
            pm.baseColor = Math::Vector3(0.18f, 0.17f, 0.16f);
            pm.roughness = 0.8f;
            auto& col = m_World->AddComponent<ECS::BoxColliderComponent>(pillar);
            col.size = Math::Vector3(1.0f, 4.0f, 1.0f);
        }

        // Arena walls (perimeter)
        struct WallDef {
            const char* name;
            Math::Vector3 pos;
            Math::Vector3 scale;
        };
        WallDef walls[] = {
            {"North Wall", Math::Vector3( 0.0f, 1.5f, 15.0f), Math::Vector3(30.0f, 3.0f, 0.5f)},
            {"South Wall", Math::Vector3( 0.0f, 1.5f,-15.0f), Math::Vector3(30.0f, 3.0f, 0.5f)},
            {"East Wall",  Math::Vector3(15.0f, 1.5f,  0.0f), Math::Vector3( 0.5f, 3.0f, 30.0f)},
            {"West Wall",  Math::Vector3(-15.0f,1.5f,  0.0f), Math::Vector3( 0.5f, 3.0f, 30.0f)},
        };
        for (auto& w : walls) {
            ECS::Entity wall = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(wall, w.name);
            auto& wt = m_World->AddComponent<ECS::TransformComponent>(wall);
            wt.position = w.pos;
            wt.scale = w.scale;
            m_World->AddComponent<ECS::MeshComponent>(wall, Renderer::MeshFactory::CreateCube(1.0f));
            auto& wm = m_World->AddComponent<ECS::MaterialComponent>(wall);
            wm.baseColor = Math::Vector3(0.12f, 0.11f, 0.10f);
            wm.roughness = 0.9f;
            auto& col = m_World->AddComponent<ECS::BoxColliderComponent>(wall);
            col.size = w.scale;
        }

        // HUD — HP text
        {
            ECS::Entity hpText = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(hpText, "HP Display");
            auto& ht = m_World->AddComponent<ECS::TransformComponent>(hpText);
            ht.position = Math::Vector3(1.0f, 8.5f, 0.0f);
            auto& text = m_World->AddComponent<ECS::TextComponent>(hpText);
            text.text = "HP  |||||||||||||||||||";
            text.fontSize = 22.0f;
            text.textColor = Math::Vector3(0.8f, 0.15f, 0.1f);
            auto& tag = m_World->AddComponent<ECS::TagComponent>(hpText);
            tag.tags.push_back("hud_hp");
        }

        // HUD — Stamina text
        {
            ECS::Entity staminaText = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(staminaText, "Stamina Display");
            auto& st = m_World->AddComponent<ECS::TransformComponent>(staminaText);
            st.position = Math::Vector3(1.0f, 8.0f, 0.0f);
            auto& text = m_World->AddComponent<ECS::TextComponent>(staminaText);
            text.text = "STA |||||||||||||||||||";
            text.fontSize = 20.0f;
            text.textColor = Math::Vector3(0.2f, 0.7f, 0.2f);
            auto& tag = m_World->AddComponent<ECS::TagComponent>(staminaText);
            tag.tags.push_back("hud_stamina");
        }

        // Dark procedural skybox
        {
            Renderer::SkyboxConfig skyConfig;
            skyConfig.type = Renderer::SkyboxType::Procedural;
            skyConfig.topColor = Math::Vector3(0.03f, 0.03f, 0.05f);
            skyConfig.horizonColor = Math::Vector3(0.08f, 0.06f, 0.05f);
            skyConfig.bottomColor = Math::Vector3(0.02f, 0.02f, 0.02f);
            skyConfig.sunDirection = Math::Vector3(0.3f, 0.2f, -0.5f);
            m_RenderSystem->SetSkybox(skyConfig);
        }
        // Render settings: low ambient, fog
        m_RenderSystem->SetAmbientIntensity(0.06f);
        m_RenderSystem->SetFogParams(0.02f, 5.0f, 60.0f, 0.3f);
        m_RenderSystem->SetFogColor(Math::Vector3(0.08f, 0.06f, 0.1f));
        // Post-processing: FXAA, vignette, film grain, chromatic aberration
        if (m_PostProcessing) {
            auto& pp = m_PostProcessing->GetSettings();
            pp.fxaaEnabled = 1;
            pp.vignetteEnabled = 1;
            pp.vignetteIntensity = 0.35f;
            pp.filmGrainEnabled = 1;
            pp.filmGrainIntensity = 0.06f;
            pp.chromaticAberrationEnabled = 1;
            pp.chromaticAberrationIntensity = 0.003f;
        }
        // Bonfire fire particles
        {
            ECS::Entity fireParticle = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(fireParticle, "Bonfire Fire");
            auto& fpt = m_World->AddComponent<ECS::TransformComponent>(fireParticle);
            fpt.position = Math::Vector3(0.0f, 0.5f, -8.0f);
            auto& pe = m_World->AddComponent<ECS::ParticleEmitterComponent>(fireParticle);
            ECS::ApplyParticlePreset(pe, "Fire");
        }

        // Soul Pickup (dropped by enemies, currency for leveling)
        {
            ECS::Entity souls = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(souls, "Soul Fragment");
            auto& st = m_World->AddComponent<ECS::TransformComponent>(souls);
            st.position = Math::Vector3(5.0f, 0.5f, 3.0f);
            st.scale = Math::Vector3(0.25f);
            m_World->AddComponent<ECS::MeshComponent>(souls, Renderer::MeshFactory::CreateSphere(0.5f));
            auto& sm = m_World->AddComponent<ECS::MaterialComponent>(souls);
            sm.baseColor = Math::Vector3(0.3f, 0.8f, 1.0f);
            sm.emissiveColor = Math::Vector3(0.2f, 0.5f, 0.8f);
            sm.emissiveStrength = 0.8f;
            auto& spc = m_World->AddComponent<ECS::PickupComponent>(souls);
            spc.type = ECS::PickupComponent::PickupType::Custom;
            spc.value = 100.0f;
            spc.magnetToPlayer = true;
            spc.magnetRange = 3.0f;
            auto& stag = m_World->AddComponent<ECS::TagComponent>(souls);
            stag.tags.push_back("soul");
            // Gentle bob
            auto& stw = m_World->AddComponent<ECS::TweenComponent>(souls);
            stw.autoPlay = true;
            ECS::TweenEntry bob;
            bob.property = ECS::TweenProperty::Position;
            bob.easing = ECS::EasingType::EaseInOutSine;
            bob.mode = ECS::TweenMode::PingPong;
            bob.startValue = Math::Vector3(5.0f, 0.5f, 3.0f);
            bob.endValue = Math::Vector3(5.0f, 0.8f, 3.0f);
            bob.duration = 2.0f;
            stw.tweens.push_back(bob);
        }

        // Bloodstain (corpse run marker — where you died last)
        {
            ECS::Entity bloodstain = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(bloodstain, "Bloodstain");
            auto& bt = m_World->AddComponent<ECS::TransformComponent>(bloodstain);
            bt.position = Math::Vector3(8.0f, 0.06f, 0.0f);
            bt.scale = Math::Vector3(1.0f, 0.01f, 1.0f);
            m_World->AddComponent<ECS::MeshComponent>(bloodstain, Renderer::MeshFactory::CreateCube(1.0f));
            auto& bm = m_World->AddComponent<ECS::MaterialComponent>(bloodstain);
            bm.baseColor = Math::Vector3(0.6f, 0.1f, 0.05f);
            bm.emissiveColor = Math::Vector3(0.4f, 0.05f, 0.02f);
            bm.emissiveStrength = 0.3f;
            auto& bint = m_World->AddComponent<ECS::InteractableComponent>(bloodstain);
            bint.promptText = "Retrieve Souls (250)";
            bint.interactionRange = 2.0f;
            auto& btag = m_World->AddComponent<ECS::TagComponent>(bloodstain);
            btag.tags.push_back("bloodstain");
            bt.visible = false;  // Hidden until player dies
        }
    }

    else if (templateId == "couchcoop") {
        // --- Ground Plane ---
        createGround();

        // --- Sun Light ---
        createLight();

        // --- Player 1 (Blue, WASD) ---
        ECS::Entity player1 = createPlayer3D("Player 1");
        {
            auto* pt = m_World->GetComponent<ECS::TransformComponent>(player1);
            pt->position = Math::Vector3(-3.0f, 1.0f, 0.0f);
            auto* pmat = m_World->GetComponent<ECS::MaterialComponent>(player1);
            pmat->baseColor = Math::Vector3(0.2f, 0.35f, 0.9f);  // Blue

            auto& ctrl = m_World->AddComponent<ECS::ThirdPersonController>(player1);
            ctrl.moveSpeed = 6.0f;
            ctrl.useWASD = true;
            ctrl.useArrowKeys = false;
            ctrl.useGamepad = false;
            ctrl.gamepadIndex = 0;

            auto& hp = m_World->AddComponent<ECS::HealthComponent>(player1);
            hp.maxHealth = 100.0f;
            hp.currentHealth = 100.0f;
            m_World->AddComponent<ECS::InventoryComponent>(player1);
            auto& rb1 = m_World->AddComponent<ECS::RigidbodyComponent>(player1);
            rb1.mass = 1.0f;

            auto& tag1 = m_World->AddComponent<ECS::TagComponent>(player1);
            tag1.tags.push_back("player");
            tag1.tags.push_back("player1");
        }

        // Camera 1 (left half) via SetupCameraForController, then override viewport
        SetupCameraForController(player1, "ThirdPerson");
        ECS::Entity cam1 = ECS::CameraManager::GetActiveCamera(m_World);
        if (cam1 != ECS::INVALID_ENTITY) {
            auto* nc1 = m_World->GetComponent<ECS::NameComponent>(cam1);
            if (nc1) nc1->name = "Camera P1";
            auto* camC1 = m_World->GetComponent<ECS::CameraComponent>(cam1);
            if (camC1) {
                camC1->viewportX = 0.0f;
                camC1->viewportY = 0.0f;
                camC1->viewportWidth = 0.5f;
                camC1->viewportHeight = 1.0f;
                camC1->fieldOfView = 60.0f;
            }
            // Override follow/lookAt for co-op offset
            if (auto* f = m_World->GetComponent<ECS::FollowTargetComponent>(cam1)) {
                f->target = player1;
                f->offset = Math::Vector3(0.0f, 5.0f, 8.0f);
                f->moveSpeed = 5.0f;
            }
            if (auto* l = m_World->GetComponent<ECS::LookAtTargetComponent>(cam1)) {
                l->target = player1;
            }
            m_SelectedGameCamera = cam1;
        }

        // --- Player 2 (Red, Arrow Keys / Gamepad) ---
        ECS::Entity player2 = createPlayer3D("Player 2");
        {
            auto* pt = m_World->GetComponent<ECS::TransformComponent>(player2);
            pt->position = Math::Vector3(3.0f, 1.0f, 0.0f);
            auto* pmat = m_World->GetComponent<ECS::MaterialComponent>(player2);
            pmat->baseColor = Math::Vector3(0.9f, 0.2f, 0.15f);  // Red

            auto& ctrl = m_World->AddComponent<ECS::ThirdPersonController>(player2);
            ctrl.moveSpeed = 6.0f;
            ctrl.useWASD = false;
            ctrl.useArrowKeys = true;
            ctrl.useGamepad = true;
            ctrl.gamepadIndex = 1;

            auto& hp = m_World->AddComponent<ECS::HealthComponent>(player2);
            hp.maxHealth = 100.0f;
            hp.currentHealth = 100.0f;
            m_World->AddComponent<ECS::InventoryComponent>(player2);
            auto& rb2 = m_World->AddComponent<ECS::RigidbodyComponent>(player2);
            rb2.mass = 1.0f;

            auto& tag2 = m_World->AddComponent<ECS::TagComponent>(player2);
            tag2.tags.push_back("player");
            tag2.tags.push_back("player2");
        }

        // Camera 2 (right half) -- manually created to avoid overriding active camera
        {
            ECS::Entity cam2 = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(cam2, "Camera P2");
            auto& camT2 = m_World->AddComponent<ECS::TransformComponent>(cam2);
            camT2.position = Math::Vector3(3.0f, 6.0f, 8.0f);

            auto& camC2 = m_World->AddComponent<ECS::CameraComponent>(cam2);
            camC2.fieldOfView = 60.0f;
            camC2.nearPlane = 0.1f;
            camC2.farPlane = 500.0f;
            camC2.viewportX = 0.5f;
            camC2.viewportY = 0.0f;
            camC2.viewportWidth = 0.5f;
            camC2.viewportHeight = 1.0f;

            auto& follow2 = m_World->AddComponent<ECS::FollowTargetComponent>(cam2);
            follow2.target = player2;
            follow2.offset = Math::Vector3(0.0f, 5.0f, 8.0f);
            follow2.moveSpeed = 5.0f;
            auto& lookAt2 = m_World->AddComponent<ECS::LookAtTargetComponent>(cam2);
            lookAt2.target = player2;
        }

        // --- Shared Objectives ---

        // Collectible objective (glowing sphere)
        {
            ECS::Entity objective = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(objective, "Objective");
            auto& ot = m_World->AddComponent<ECS::TransformComponent>(objective);
            ot.position = Math::Vector3(0.0f, 1.5f, -15.0f);
            m_World->AddComponent<ECS::MeshComponent>(objective, Renderer::MeshFactory::CreateSphere(0.5f));
            auto& omat = m_World->AddComponent<ECS::MaterialComponent>(objective);
            omat.baseColor = Math::Vector3(1.0f, 0.85f, 0.0f);
            omat.emissiveColor = Math::Vector3(1.0f, 0.85f, 0.0f);
            omat.emissiveStrength = 0.6f;
            auto& pickup = m_World->AddComponent<ECS::PickupComponent>(objective);
            pickup.type = ECS::PickupComponent::PickupType::Custom;
            pickup.customId = "objective";
            pickup.pickupRange = 1.5f;
            auto& otag = m_World->AddComponent<ECS::TagComponent>(objective);
            otag.tags.push_back("objective");
        }

        // Key pickup
        {
            ECS::Entity key = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(key, "Gate Key");
            auto& kt = m_World->AddComponent<ECS::TransformComponent>(key);
            kt.position = Math::Vector3(-10.0f, 0.8f, -8.0f);
            kt.scale = Math::Vector3(0.4f, 0.4f, 0.4f);
            m_World->AddComponent<ECS::MeshComponent>(key, Renderer::MeshFactory::CreateCube(1.0f));
            auto& km = m_World->AddComponent<ECS::MaterialComponent>(key);
            km.baseColor = Math::Vector3(0.9f, 0.75f, 0.1f);
            km.emissiveColor = Math::Vector3(0.9f, 0.75f, 0.1f);
            km.emissiveStrength = 0.4f;
            auto& kp = m_World->AddComponent<ECS::PickupComponent>(key);
            kp.type = ECS::PickupComponent::PickupType::Key;
            kp.customId = "gate_key";
            auto& ktag = m_World->AddComponent<ECS::TagComponent>(key);
            ktag.tags.push_back("key");
        }

        // Locked gate
        {
            ECS::Entity gate = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(gate, "Locked Gate");
            auto& gateT = m_World->AddComponent<ECS::TransformComponent>(gate);
            gateT.position = Math::Vector3(0.0f, 1.5f, -10.0f);
            gateT.scale = Math::Vector3(3.0f, 3.0f, 0.3f);
            m_World->AddComponent<ECS::MeshComponent>(gate, Renderer::MeshFactory::CreateCube(1.0f));
            auto& gm = m_World->AddComponent<ECS::MaterialComponent>(gate);
            gm.baseColor = Math::Vector3(0.4f, 0.25f, 0.15f);
            gm.roughness = 0.8f;
            auto& col = m_World->AddComponent<ECS::BoxColliderComponent>(gate);
            col.size = Math::Vector3(3.0f, 3.0f, 0.3f);
            auto& lock = m_World->AddComponent<ECS::LockComponent>(gate);
            lock.requiredKey = "gate_key";
            lock.isLocked = true;
            lock.consumeKey = true;
            auto& interact = m_World->AddComponent<ECS::InteractableComponent>(gate);
            interact.promptText = "Use Gate Key to open";
            interact.interactionRange = 2.5f;
        }

        // Enemies (patrol around the arena)
        {
            Math::Vector3 enemyPositions[] = {
                Math::Vector3(-8.0f, 0.5f, -5.0f),
                Math::Vector3(8.0f, 0.5f, -5.0f),
                Math::Vector3(0.0f, 0.5f, -18.0f),
            };
            const char* enemyNames[] = { "Enemy Scout", "Enemy Brute", "Enemy Sniper" };
            for (int i = 0; i < 3; ++i) {
                ECS::Entity enemy = m_World->CreateEntity();
                m_World->AddComponent<ECS::NameComponent>(enemy, enemyNames[i]);
                auto& et = m_World->AddComponent<ECS::TransformComponent>(enemy);
                et.position = enemyPositions[i];
                m_World->AddComponent<ECS::MeshComponent>(enemy, Renderer::MeshFactory::CreateCapsule(0.3f, 1.0f));
                auto& em = m_World->AddComponent<ECS::MaterialComponent>(enemy);
                em.baseColor = Math::Vector3(0.6f, 0.1f, 0.5f);
                auto& ai = m_World->AddComponent<ECS::AIControllerComponent>(enemy);
                ai.currentState = ECS::AIControllerComponent::AIState::Patrol;
                ai.moveSpeed = 2.5f;
                auto& eh = m_World->AddComponent<ECS::HealthComponent>(enemy);
                eh.maxHealth = 40.0f + i * 15.0f;
                eh.currentHealth = eh.maxHealth;
                auto& dmg = m_World->AddComponent<ECS::DamageComponent>(enemy);
                dmg.damage = 10.0f + i * 5.0f;
                auto& etag = m_World->AddComponent<ECS::TagComponent>(enemy);
                etag.tags.push_back("enemy");
            }
        }

        // --- Decorative Elements ---

        // Crates for cover
        {
            Math::Vector3 cratePositions[] = {
                Math::Vector3(-5.0f, 0.5f, -3.0f),
                Math::Vector3(5.0f, 0.5f, -3.0f),
                Math::Vector3(-2.0f, 0.5f, -7.0f),
                Math::Vector3(2.0f, 0.5f, -7.0f),
            };
            for (int i = 0; i < 4; ++i) {
                char name[32];
                snprintf(name, sizeof(name), "Crate %d", i + 1);
                ECS::Entity crate = m_World->CreateEntity();
                m_World->AddComponent<ECS::NameComponent>(crate, name);
                auto& ct = m_World->AddComponent<ECS::TransformComponent>(crate);
                ct.position = cratePositions[i];
                m_World->AddComponent<ECS::MeshComponent>(crate, Renderer::MeshFactory::CreateCube(1.0f));
                auto& cm = m_World->AddComponent<ECS::MaterialComponent>(crate);
                cm.baseColor = Math::Vector3(0.55f, 0.4f, 0.2f);
                cm.roughness = 0.85f;
                auto& ccol = m_World->AddComponent<ECS::BoxColliderComponent>(crate);
                ccol.size = Math::Vector3(1.0f, 1.0f, 1.0f);
            }
        }

        // Barriers
        {
            Math::Vector3 barrierPositions[] = {
                Math::Vector3(-12.0f, 0.75f, 0.0f),
                Math::Vector3(12.0f, 0.75f, 0.0f),
            };
            for (int i = 0; i < 2; ++i) {
                char name[32];
                snprintf(name, sizeof(name), "Barrier %d", i + 1);
                ECS::Entity barrier = m_World->CreateEntity();
                m_World->AddComponent<ECS::NameComponent>(barrier, name);
                auto& bt = m_World->AddComponent<ECS::TransformComponent>(barrier);
                bt.position = barrierPositions[i];
                bt.scale = Math::Vector3(0.5f, 1.5f, 4.0f);
                m_World->AddComponent<ECS::MeshComponent>(barrier, Renderer::MeshFactory::CreateCube(1.0f));
                auto& bm = m_World->AddComponent<ECS::MaterialComponent>(barrier);
                bm.baseColor = Math::Vector3(0.45f, 0.45f, 0.5f);
                bm.roughness = 0.7f;
                auto& bcol = m_World->AddComponent<ECS::BoxColliderComponent>(barrier);
                bcol.size = Math::Vector3(0.5f, 1.5f, 4.0f);
            }
        }

        // --- HUD Text ---
        {
            ECS::Entity hud1 = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(hud1, "P1 HUD");
            auto& h1t = m_World->AddComponent<ECS::TransformComponent>(hud1);
            h1t.position = Math::Vector3(-3.0f, 4.5f, 0.0f);
            auto& h1text = m_World->AddComponent<ECS::TextComponent>(hud1);
            h1text.text = "P1 HP: 100";
            h1text.fontSize = 22.0f;
            h1text.textColor = Math::Vector3(0.2f, 0.5f, 1.0f);
            auto& h1tag = m_World->AddComponent<ECS::TagComponent>(hud1);
            h1tag.tags.push_back("hud_p1");
        }
        {
            ECS::Entity hud2 = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(hud2, "P2 HUD");
            auto& h2t = m_World->AddComponent<ECS::TransformComponent>(hud2);
            h2t.position = Math::Vector3(3.0f, 4.5f, 0.0f);
            auto& h2text = m_World->AddComponent<ECS::TextComponent>(hud2);
            h2text.text = "P2 HP: 100";
            h2text.fontSize = 22.0f;
            h2text.textColor = Math::Vector3(1.0f, 0.3f, 0.2f);
            auto& h2tag = m_World->AddComponent<ECS::TagComponent>(hud2);
            h2tag.tags.push_back("hud_p2");
        }

        // --- Procedural Skybox (bright day) ---
        {
            Renderer::SkyboxConfig skyConfig;
            skyConfig.type = Renderer::SkyboxType::Procedural;
            skyConfig.topColor = Math::Vector3(0.3f, 0.5f, 0.95f);
            skyConfig.horizonColor = Math::Vector3(0.7f, 0.8f, 1.0f);
            skyConfig.bottomColor = Math::Vector3(0.4f, 0.55f, 0.35f);
            skyConfig.sunDirection = Math::Vector3(0.3f, 0.8f, 0.5f);
            m_RenderSystem->SetSkybox(skyConfig);
        }

        // --- Setup Notes ---
        {
            ECS::Entity notes = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(notes, "Co-op Setup Notes");
            auto& nt = m_World->AddComponent<ECS::TransformComponent>(notes);
            nt.position = Math::Vector3(0.0f, 0.0f, 0.0f);
            auto& nc = m_World->AddComponent<ECS::NotesComponent>(notes);
            nc.notes = "2-PLAYER COUCH CO-OP (Horizontal Splitscreen)\n"
                "================================================\n"
                "Player 1 (Blue): WASD controls, left screen half\n"
                "Player 2 (Red): Arrow Keys / Gamepad 2, right screen half\n"
                "\n"
                "Objectives:\n"
                "  - Find the Gate Key and unlock the gate\n"
                "  - Collect the Objective item behind the gate\n"
                "  - Defeat enemies together\n"
                "\n"
                "Each player has their own camera with FollowTarget + LookAtTarget.\n"
                "Viewport subdivision: P1 left 50%, P2 right 50%.\n";
        }

        // Shared power-up (both players can pick up)
        {
            ECS::Entity sharedPickup = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(sharedPickup, "Shared Power-up");
            auto& spt = m_World->AddComponent<ECS::TransformComponent>(sharedPickup);
            spt.position = Math::Vector3(0.0f, 0.5f, 5.0f);
            spt.scale = Math::Vector3(0.4f);
            m_World->AddComponent<ECS::MeshComponent>(sharedPickup, Renderer::MeshFactory::CreateSphere(0.5f));
            auto& spm = m_World->AddComponent<ECS::MaterialComponent>(sharedPickup);
            spm.baseColor = Math::Vector3(1.0f, 0.8f, 0.0f);
            spm.emissiveColor = Math::Vector3(0.8f, 0.6f, 0.0f);
            spm.emissiveStrength = 0.5f;
            auto& sppc = m_World->AddComponent<ECS::PickupComponent>(sharedPickup);
            sppc.type = ECS::PickupComponent::PickupType::Powerup;
            sppc.value = 1.0f;
            auto& sptw = m_World->AddComponent<ECS::TweenComponent>(sharedPickup);
            sptw.autoPlay = true;
            ECS::TweenEntry bob;
            bob.property = ECS::TweenProperty::Position;
            bob.easing = ECS::EasingType::EaseInOutSine;
            bob.mode = ECS::TweenMode::PingPong;
            bob.startValue = Math::Vector3(0.0f, 0.5f, 5.0f);
            bob.endValue = Math::Vector3(0.0f, 1.0f, 5.0f);
            bob.duration = 1.2f;
            sptw.tweens.push_back(bob);
        }

        // Treasure chest (cooperative objective — both players must be nearby)
        {
            ECS::Entity chest = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(chest, "Treasure Chest");
            auto& ct = m_World->AddComponent<ECS::TransformComponent>(chest);
            ct.position = Math::Vector3(0.0f, 0.5f, -5.0f);
            ct.scale = Math::Vector3(0.8f, 0.6f, 0.5f);
            m_World->AddComponent<ECS::MeshComponent>(chest, Renderer::MeshFactory::CreateCube(1.0f));
            auto& cm = m_World->AddComponent<ECS::MaterialComponent>(chest);
            cm.baseColor = Math::Vector3(0.5f, 0.35f, 0.15f);
            auto& cint = m_World->AddComponent<ECS::InteractableComponent>(chest);
            cint.promptText = "Open (both players needed)";
            cint.interactionRange = 2.5f;
            auto& ctag = m_World->AddComponent<ECS::TagComponent>(chest);
            ctag.tags.push_back("coop_objective");
        }

        // Render settings
        m_RenderSystem->SetShadowsEnabled(true);
        m_RenderSystem->SetAmbientIntensity(0.12f);

        // Post-processing
        if (m_PostProcessing) {
            auto& pp = m_PostProcessing->GetSettings();
            pp.fxaaEnabled = 1;
        }

        // Collision groups
        {
            auto& groups = m_SceneManager.GetCollisionGroupNames();
            groups[1] = "Players";
            groups[2] = "Enemies";
        }
    }

    else if (templateId == "justtwo") {
        // === JUST THE TWO OF US — Co-op Puzzle Template ===
        // Inspired by "It Takes Two": cooperative physics puzzles,
        // environment minigames, seamless unified/splitscreen camera.

        // --- Ground Plane ---
        {
            ECS::Entity ground = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(ground, "Ground");
            auto& gt = m_World->AddComponent<ECS::TransformComponent>(ground);
            gt.position = Math::Vector3(0.0f, 0.0f, 0.0f);
            gt.scale = Math::Vector3(60.0f, 0.1f, 60.0f);
            m_World->AddComponent<ECS::MeshComponent>(ground, Renderer::MeshFactory::CreateCube(1.0f));
            auto& gmat = m_World->AddComponent<ECS::MaterialComponent>(ground);
            gmat.baseColor = Math::Vector3(0.35f, 0.55f, 0.3f);
            gmat.roughness = 0.9f;
            auto& gcol = m_World->AddComponent<ECS::BoxColliderComponent>(ground);
            gcol.size = Math::Vector3(60.0f, 0.1f, 60.0f);
        }

        // --- Sun Light ---
        createLight();

        // --- Player 1 (Teal, WASD) ---
        ECS::Entity player1 = createPlayer3D("Player A");
        {
            auto* pt = m_World->GetComponent<ECS::TransformComponent>(player1);
            pt->position = Math::Vector3(-2.0f, 1.0f, 8.0f);
            auto* pmat = m_World->GetComponent<ECS::MaterialComponent>(player1);
            pmat->baseColor = Math::Vector3(0.15f, 0.7f, 0.65f);  // Teal

            auto& ctrl = m_World->AddComponent<ECS::ThirdPersonController>(player1);
            ctrl.moveSpeed = 5.0f;
            ctrl.useWASD = true;
            ctrl.useArrowKeys = false;
            ctrl.useGamepad = false;
            ctrl.gamepadIndex = 0;

            auto& rb1 = m_World->AddComponent<ECS::RigidbodyComponent>(player1);
            rb1.mass = 1.0f;
            auto& tag1 = m_World->AddComponent<ECS::TagComponent>(player1);
            tag1.tags.push_back("player");
            tag1.tags.push_back("player1");
        }

        // --- Player 2 (Coral, Arrow Keys / Gamepad) ---
        ECS::Entity player2 = createPlayer3D("Player B");
        {
            auto* pt = m_World->GetComponent<ECS::TransformComponent>(player2);
            pt->position = Math::Vector3(2.0f, 1.0f, 8.0f);
            auto* pmat = m_World->GetComponent<ECS::MaterialComponent>(player2);
            pmat->baseColor = Math::Vector3(0.95f, 0.4f, 0.35f);  // Coral

            auto& ctrl = m_World->AddComponent<ECS::ThirdPersonController>(player2);
            ctrl.moveSpeed = 5.0f;
            ctrl.useWASD = false;
            ctrl.useArrowKeys = true;
            ctrl.useGamepad = true;
            ctrl.gamepadIndex = 1;

            auto& rb2 = m_World->AddComponent<ECS::RigidbodyComponent>(player2);
            rb2.mass = 1.0f;
            auto& tag2 = m_World->AddComponent<ECS::TagComponent>(player2);
            tag2.tags.push_back("player");
            tag2.tags.push_back("player2");
        }

        // Camera 1 (left half) via SetupCameraForController, then override viewport
        SetupCameraForController(player1, "ThirdPerson");
        ECS::Entity cam1 = ECS::CameraManager::GetActiveCamera(m_World);
        if (cam1 != ECS::INVALID_ENTITY) {
            auto* nc1 = m_World->GetComponent<ECS::NameComponent>(cam1);
            if (nc1) nc1->name = "Camera A";
            auto* camC1 = m_World->GetComponent<ECS::CameraComponent>(cam1);
            if (camC1) {
                camC1->viewportX = 0.0f;
                camC1->viewportY = 0.0f;
                camC1->viewportWidth = 0.5f;
                camC1->viewportHeight = 1.0f;
                camC1->fieldOfView = 55.0f;
            }
            if (auto* f = m_World->GetComponent<ECS::FollowTargetComponent>(cam1)) {
                f->target = player1;
                f->offset = Math::Vector3(0.0f, 6.0f, 7.0f);
                f->moveSpeed = 4.0f;
            }
            if (auto* l = m_World->GetComponent<ECS::LookAtTargetComponent>(cam1)) {
                l->target = player1;
            }
            m_SelectedGameCamera = cam1;
        }

        // Camera 2 (right half)
        {
            ECS::Entity cam2 = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(cam2, "Camera B");
            auto& camT2 = m_World->AddComponent<ECS::TransformComponent>(cam2);
            camT2.position = Math::Vector3(2.0f, 7.0f, 15.0f);

            auto& camC2 = m_World->AddComponent<ECS::CameraComponent>(cam2);
            camC2.fieldOfView = 55.0f;
            camC2.nearPlane = 0.1f;
            camC2.farPlane = 500.0f;
            camC2.viewportX = 0.5f;
            camC2.viewportY = 0.0f;
            camC2.viewportWidth = 0.5f;
            camC2.viewportHeight = 1.0f;

            auto& follow2 = m_World->AddComponent<ECS::FollowTargetComponent>(cam2);
            follow2.target = player2;
            follow2.offset = Math::Vector3(0.0f, 6.0f, 7.0f);
            follow2.moveSpeed = 4.0f;
            auto& lookAt2 = m_World->AddComponent<ECS::LookAtTargetComponent>(cam2);
            lookAt2.target = player2;
        }

        // ========================================
        // PUZZLE 1: Dual Pressure Plates + Bridge
        // Both players must stand on their plate to raise the bridge
        // ========================================
        {
            // Pressure plate A (left)
            ECS::Entity plateA = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(plateA, "Pressure Plate A");
            auto& paT = m_World->AddComponent<ECS::TransformComponent>(plateA);
            paT.position = Math::Vector3(-4.0f, 0.08f, 0.0f);
            paT.scale = Math::Vector3(1.5f, 0.05f, 1.5f);
            m_World->AddComponent<ECS::MeshComponent>(plateA, Renderer::MeshFactory::CreateCube(1.0f));
            auto& paM = m_World->AddComponent<ECS::MaterialComponent>(plateA);
            paM.baseColor = Math::Vector3(0.15f, 0.7f, 0.65f);  // Teal (matches P1)
            paM.emissiveColor = Math::Vector3(0.15f, 0.7f, 0.65f);
            paM.emissiveStrength = 0.3f;
            auto& paCol = m_World->AddComponent<ECS::BoxColliderComponent>(plateA);
            paCol.size = Math::Vector3(1.5f, 0.05f, 1.5f);
            auto& paTag = m_World->AddComponent<ECS::TagComponent>(plateA);
            paTag.tags.push_back("pressure_plate");
            paTag.tags.push_back("plate_a");

            // Pressure plate B (right)
            ECS::Entity plateB = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(plateB, "Pressure Plate B");
            auto& pbT = m_World->AddComponent<ECS::TransformComponent>(plateB);
            pbT.position = Math::Vector3(4.0f, 0.08f, 0.0f);
            pbT.scale = Math::Vector3(1.5f, 0.05f, 1.5f);
            m_World->AddComponent<ECS::MeshComponent>(plateB, Renderer::MeshFactory::CreateCube(1.0f));
            auto& pbM = m_World->AddComponent<ECS::MaterialComponent>(plateB);
            pbM.baseColor = Math::Vector3(0.95f, 0.4f, 0.35f);  // Coral (matches P2)
            pbM.emissiveColor = Math::Vector3(0.95f, 0.4f, 0.35f);
            pbM.emissiveStrength = 0.3f;
            auto& pbCol = m_World->AddComponent<ECS::BoxColliderComponent>(plateB);
            pbCol.size = Math::Vector3(1.5f, 0.05f, 1.5f);
            auto& pbTag = m_World->AddComponent<ECS::TagComponent>(plateB);
            pbTag.tags.push_back("pressure_plate");
            pbTag.tags.push_back("plate_b");

            // Bridge (starts lowered / blocking)
            ECS::Entity bridge = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(bridge, "Co-op Bridge");
            auto& brT = m_World->AddComponent<ECS::TransformComponent>(bridge);
            brT.position = Math::Vector3(0.0f, -0.5f, -3.0f);
            brT.scale = Math::Vector3(4.0f, 0.15f, 2.0f);
            m_World->AddComponent<ECS::MeshComponent>(bridge, Renderer::MeshFactory::CreateCube(1.0f));
            auto& brM = m_World->AddComponent<ECS::MaterialComponent>(bridge);
            brM.baseColor = Math::Vector3(0.55f, 0.45f, 0.3f);
            brM.roughness = 0.85f;
            auto& brCol = m_World->AddComponent<ECS::BoxColliderComponent>(bridge);
            brCol.size = Math::Vector3(4.0f, 0.15f, 2.0f);
            auto& brTag = m_World->AddComponent<ECS::TagComponent>(bridge);
            brTag.tags.push_back("bridge");
            brTag.tags.push_back("coop_mechanism");
        }

        // ========================================
        // PUZZLE 2: Physics Seesaw
        // One player stands on one end, the other pushes a boulder
        // ========================================
        {
            // Seesaw plank
            ECS::Entity seesaw = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(seesaw, "Seesaw Plank");
            auto& ssT = m_World->AddComponent<ECS::TransformComponent>(seesaw);
            ssT.position = Math::Vector3(-10.0f, 0.6f, -8.0f);
            ssT.scale = Math::Vector3(6.0f, 0.15f, 1.5f);
            m_World->AddComponent<ECS::MeshComponent>(seesaw, Renderer::MeshFactory::CreateCube(1.0f));
            auto& ssM = m_World->AddComponent<ECS::MaterialComponent>(seesaw);
            ssM.baseColor = Math::Vector3(0.6f, 0.4f, 0.2f);
            auto& ssRb = m_World->AddComponent<ECS::RigidbodyComponent>(seesaw);
            ssRb.mass = 3.0f;
            auto& ssTag = m_World->AddComponent<ECS::TagComponent>(seesaw);
            ssTag.tags.push_back("seesaw");

            // Seesaw fulcrum (pivot)
            ECS::Entity fulcrum = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(fulcrum, "Seesaw Fulcrum");
            auto& fT = m_World->AddComponent<ECS::TransformComponent>(fulcrum);
            fT.position = Math::Vector3(-10.0f, 0.3f, -8.0f);
            fT.scale = Math::Vector3(0.5f, 0.6f, 1.5f);
            m_World->AddComponent<ECS::MeshComponent>(fulcrum, Renderer::MeshFactory::CreateCube(1.0f));
            auto& fM = m_World->AddComponent<ECS::MaterialComponent>(fulcrum);
            fM.baseColor = Math::Vector3(0.5f, 0.5f, 0.55f);
            fM.roughness = 0.6f;
            auto& fCol = m_World->AddComponent<ECS::BoxColliderComponent>(fulcrum);
            fCol.size = Math::Vector3(0.5f, 0.6f, 1.5f);

            // Boulder to push
            ECS::Entity boulder = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(boulder, "Push Boulder");
            auto& bT = m_World->AddComponent<ECS::TransformComponent>(boulder);
            bT.position = Math::Vector3(-13.0f, 0.8f, -8.0f);
            m_World->AddComponent<ECS::MeshComponent>(boulder, Renderer::MeshFactory::CreateSphere(0.7f));
            auto& bM = m_World->AddComponent<ECS::MaterialComponent>(boulder);
            bM.baseColor = Math::Vector3(0.45f, 0.42f, 0.4f);
            bM.roughness = 0.9f;
            auto& bRb = m_World->AddComponent<ECS::RigidbodyComponent>(boulder);
            bRb.mass = 5.0f;
            auto& bCol = m_World->AddComponent<ECS::SphereColliderComponent>(boulder);
            bCol.radius = 0.7f;
            auto& bTag = m_World->AddComponent<ECS::TagComponent>(boulder);
            bTag.tags.push_back("pushable");
        }

        // ========================================
        // PUZZLE 3: Weight Balance Scale
        // Both players must push objects onto a scale to match weight
        // ========================================
        {
            // Scale platform
            ECS::Entity scale = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(scale, "Weight Scale");
            auto& scT = m_World->AddComponent<ECS::TransformComponent>(scale);
            scT.position = Math::Vector3(10.0f, 0.4f, -8.0f);
            scT.scale = Math::Vector3(3.0f, 0.1f, 3.0f);
            m_World->AddComponent<ECS::MeshComponent>(scale, Renderer::MeshFactory::CreateCube(1.0f));
            auto& scM = m_World->AddComponent<ECS::MaterialComponent>(scale);
            scM.baseColor = Math::Vector3(0.7f, 0.65f, 0.5f);
            auto& scCol = m_World->AddComponent<ECS::BoxColliderComponent>(scale);
            scCol.size = Math::Vector3(3.0f, 0.1f, 3.0f);
            auto& scTag = m_World->AddComponent<ECS::TagComponent>(scale);
            scTag.tags.push_back("weight_scale");

            // Pushable crates for weighing
            Math::Vector3 cratePos[] = {
                Math::Vector3(8.0f, 0.4f, -5.0f),
                Math::Vector3(12.0f, 0.4f, -5.0f),
                Math::Vector3(10.0f, 0.4f, -5.0f),
            };
            f32 crateMasses[] = { 2.0f, 3.0f, 5.0f };
            for (int i = 0; i < 3; ++i) {
                char name[32];
                snprintf(name, sizeof(name), "Weight Crate %d", i + 1);
                ECS::Entity crate = m_World->CreateEntity();
                m_World->AddComponent<ECS::NameComponent>(crate, name);
                auto& ct = m_World->AddComponent<ECS::TransformComponent>(crate);
                ct.position = cratePos[i];
                f32 s = 0.4f + crateMasses[i] * 0.08f;
                ct.scale = Math::Vector3(s, s, s);
                m_World->AddComponent<ECS::MeshComponent>(crate, Renderer::MeshFactory::CreateCube(1.0f));
                auto& cm = m_World->AddComponent<ECS::MaterialComponent>(crate);
                cm.baseColor = Math::Vector3(0.5f + i * 0.15f, 0.35f, 0.15f);
                auto& crb = m_World->AddComponent<ECS::RigidbodyComponent>(crate);
                crb.mass = crateMasses[i];
                auto& ccol = m_World->AddComponent<ECS::BoxColliderComponent>(crate);
                ccol.size = Math::Vector3(s, s, s);
                auto& ctag = m_World->AddComponent<ECS::TagComponent>(crate);
                ctag.tags.push_back("pushable");
                ctag.tags.push_back("weight_object");
            }
        }

        // ========================================
        // MINIGAME AREA: Physics Bowling Lane
        // Side activity — roll a ball to knock down pins
        // ========================================
        {
            // Lane floor
            ECS::Entity lane = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(lane, "Bowling Lane");
            auto& lnT = m_World->AddComponent<ECS::TransformComponent>(lane);
            lnT.position = Math::Vector3(20.0f, 0.05f, 0.0f);
            lnT.scale = Math::Vector3(3.0f, 0.05f, 12.0f);
            m_World->AddComponent<ECS::MeshComponent>(lane, Renderer::MeshFactory::CreateCube(1.0f));
            auto& lnM = m_World->AddComponent<ECS::MaterialComponent>(lane);
            lnM.baseColor = Math::Vector3(0.7f, 0.55f, 0.3f);
            lnM.roughness = 0.3f;
            auto& lnCol = m_World->AddComponent<ECS::BoxColliderComponent>(lane);
            lnCol.size = Math::Vector3(3.0f, 0.05f, 12.0f);

            // Bowling ball
            ECS::Entity ball = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(ball, "Bowling Ball");
            auto& blT = m_World->AddComponent<ECS::TransformComponent>(ball);
            blT.position = Math::Vector3(20.0f, 0.5f, 5.0f);
            m_World->AddComponent<ECS::MeshComponent>(ball, Renderer::MeshFactory::CreateSphere(0.35f));
            auto& blM = m_World->AddComponent<ECS::MaterialComponent>(ball);
            blM.baseColor = Math::Vector3(0.1f, 0.1f, 0.15f);
            blM.roughness = 0.2f;
            auto& blRb = m_World->AddComponent<ECS::RigidbodyComponent>(ball);
            blRb.mass = 4.0f;
            auto& blSc = m_World->AddComponent<ECS::SphereColliderComponent>(ball);
            blSc.radius = 0.35f;
            auto& blTag = m_World->AddComponent<ECS::TagComponent>(ball);
            blTag.tags.push_back("bowling_ball");
            blTag.tags.push_back("pushable");

            // Bowling pins (triangle formation)
            Math::Vector3 pinPos[] = {
                Math::Vector3(20.0f, 0.4f, -4.0f),
                Math::Vector3(19.5f, 0.4f, -5.0f), Math::Vector3(20.5f, 0.4f, -5.0f),
                Math::Vector3(19.0f, 0.4f, -6.0f), Math::Vector3(20.0f, 0.4f, -6.0f), Math::Vector3(21.0f, 0.4f, -6.0f),
            };
            for (int i = 0; i < 6; ++i) {
                char name[32];
                snprintf(name, sizeof(name), "Pin %d", i + 1);
                ECS::Entity pin = m_World->CreateEntity();
                m_World->AddComponent<ECS::NameComponent>(pin, name);
                auto& pnT = m_World->AddComponent<ECS::TransformComponent>(pin);
                pnT.position = pinPos[i];
                pnT.scale = Math::Vector3(0.15f, 0.6f, 0.15f);
                m_World->AddComponent<ECS::MeshComponent>(pin, Renderer::MeshFactory::CreateCapsule(0.15f, 0.4f));
                auto& pnM = m_World->AddComponent<ECS::MaterialComponent>(pin);
                pnM.baseColor = Math::Vector3(0.95f, 0.95f, 0.9f);
                auto& pnRb = m_World->AddComponent<ECS::RigidbodyComponent>(pin);
                pnRb.mass = 0.3f;
                auto& pnTag = m_World->AddComponent<ECS::TagComponent>(pin);
                pnTag.tags.push_back("bowling_pin");
            }
        }

        // ========================================
        // MINIGAME AREA: Target Practice
        // Cooperative — one player activates targets, the other aims
        // ========================================
        {
            // Target stand
            ECS::Entity stand = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(stand, "Target Range");
            auto& stT = m_World->AddComponent<ECS::TransformComponent>(stand);
            stT.position = Math::Vector3(-20.0f, 1.5f, -8.0f);
            stT.scale = Math::Vector3(6.0f, 3.0f, 0.2f);
            m_World->AddComponent<ECS::MeshComponent>(stand, Renderer::MeshFactory::CreateCube(1.0f));
            auto& stM = m_World->AddComponent<ECS::MaterialComponent>(stand);
            stM.baseColor = Math::Vector3(0.4f, 0.3f, 0.2f);
            auto& stCol = m_World->AddComponent<ECS::BoxColliderComponent>(stand);
            stCol.size = Math::Vector3(6.0f, 3.0f, 0.2f);

            // 3 targets on the stand
            for (int i = 0; i < 3; ++i) {
                char name[32];
                snprintf(name, sizeof(name), "Target %d", i + 1);
                ECS::Entity target = m_World->CreateEntity();
                m_World->AddComponent<ECS::NameComponent>(target, name);
                auto& tT = m_World->AddComponent<ECS::TransformComponent>(target);
                tT.position = Math::Vector3(-22.0f + i * 2.0f, 1.5f, -7.8f);
                m_World->AddComponent<ECS::MeshComponent>(target, Renderer::MeshFactory::CreateSphere(0.4f));
                auto& tM = m_World->AddComponent<ECS::MaterialComponent>(target);
                tM.baseColor = Math::Vector3(0.9f, 0.2f, 0.15f);
                tM.emissiveColor = Math::Vector3(0.9f, 0.2f, 0.15f);
                tM.emissiveStrength = 0.2f;
                auto& tInt = m_World->AddComponent<ECS::InteractableComponent>(target);
                tInt.promptText = "Hit!";
                tInt.interactionRange = 1.5f;
                auto& tTag = m_World->AddComponent<ECS::TagComponent>(target);
                tTag.tags.push_back("target");
                auto& tHp = m_World->AddComponent<ECS::HealthComponent>(target);
                tHp.maxHealth = 10.0f;
                tHp.currentHealth = 10.0f;
            }

            // Ammo crate (shared resource)
            ECS::Entity ammo = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(ammo, "Ammo Crate");
            auto& amT = m_World->AddComponent<ECS::TransformComponent>(ammo);
            amT.position = Math::Vector3(-20.0f, 0.35f, -4.0f);
            amT.scale = Math::Vector3(0.6f, 0.5f, 0.6f);
            m_World->AddComponent<ECS::MeshComponent>(ammo, Renderer::MeshFactory::CreateCube(1.0f));
            auto& amM = m_World->AddComponent<ECS::MaterialComponent>(ammo);
            amM.baseColor = Math::Vector3(0.25f, 0.35f, 0.2f);
            auto& amPu = m_World->AddComponent<ECS::PickupComponent>(ammo);
            amPu.type = ECS::PickupComponent::PickupType::Custom;
            amPu.customId = "ammo";
            amPu.value = 10.0f;
            auto& amTag = m_World->AddComponent<ECS::TagComponent>(ammo);
            amTag.tags.push_back("pickup");
        }

        // ========================================
        // ENVIRONMENT: Decorative walls + pathways
        // ========================================
        {
            // Central puzzle area walls
            struct WallDef { Math::Vector3 pos; Math::Vector3 scale; };
            WallDef walls[] = {
                { Math::Vector3(-8.0f, 1.0f, -3.0f),  Math::Vector3(0.3f, 2.0f, 8.0f) },   // Left wall
                { Math::Vector3(8.0f, 1.0f, -3.0f),   Math::Vector3(0.3f, 2.0f, 8.0f) },   // Right wall
                { Math::Vector3(0.0f, 1.0f, -12.0f),  Math::Vector3(16.0f, 2.0f, 0.3f) },  // Back wall
                { Math::Vector3(0.0f, 0.6f, 3.0f),    Math::Vector3(6.0f, 0.3f, 0.3f) },   // Low fence
            };
            for (int i = 0; i < 4; ++i) {
                char name[32];
                snprintf(name, sizeof(name), "Wall %d", i + 1);
                ECS::Entity wall = m_World->CreateEntity();
                m_World->AddComponent<ECS::NameComponent>(wall, name);
                auto& wT = m_World->AddComponent<ECS::TransformComponent>(wall);
                wT.position = walls[i].pos;
                wT.scale = walls[i].scale;
                m_World->AddComponent<ECS::MeshComponent>(wall, Renderer::MeshFactory::CreateCube(1.0f));
                auto& wM = m_World->AddComponent<ECS::MaterialComponent>(wall);
                wM.baseColor = Math::Vector3(0.6f, 0.58f, 0.55f);
                wM.roughness = 0.75f;
                auto& wCol = m_World->AddComponent<ECS::BoxColliderComponent>(wall);
                wCol.size = walls[i].scale;
            }
        }

        // ========================================
        // GOAL: Cooperative chest at the end
        // ========================================
        {
            ECS::Entity goal = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(goal, "Victory Chest");
            auto& gT = m_World->AddComponent<ECS::TransformComponent>(goal);
            gT.position = Math::Vector3(0.0f, 0.5f, -10.0f);
            gT.scale = Math::Vector3(1.0f, 0.8f, 0.6f);
            m_World->AddComponent<ECS::MeshComponent>(goal, Renderer::MeshFactory::CreateCube(1.0f));
            auto& gM = m_World->AddComponent<ECS::MaterialComponent>(goal);
            gM.baseColor = Math::Vector3(0.85f, 0.7f, 0.15f);
            gM.emissiveColor = Math::Vector3(0.85f, 0.7f, 0.15f);
            gM.emissiveStrength = 0.5f;
            auto& gInt = m_World->AddComponent<ECS::InteractableComponent>(goal);
            gInt.promptText = "Open together (both players needed)";
            gInt.interactionRange = 2.5f;
            auto& gTag = m_World->AddComponent<ECS::TagComponent>(goal);
            gTag.tags.push_back("coop_objective");
            gTag.tags.push_back("victory");

            // Bobbing tween
            auto& tw = m_World->AddComponent<ECS::TweenComponent>(goal);
            tw.autoPlay = true;
            ECS::TweenEntry bob;
            bob.property = ECS::TweenProperty::Position;
            bob.easing = ECS::EasingType::EaseInOutSine;
            bob.mode = ECS::TweenMode::PingPong;
            bob.startValue = Math::Vector3(0.0f, 0.5f, -10.0f);
            bob.endValue = Math::Vector3(0.0f, 0.9f, -10.0f);
            bob.duration = 1.5f;
            tw.tweens.push_back(bob);
        }

        // --- Procedural Skybox (warm sunset) ---
        {
            Renderer::SkyboxConfig skyConfig;
            skyConfig.type = Renderer::SkyboxType::Procedural;
            skyConfig.topColor = Math::Vector3(0.25f, 0.35f, 0.75f);
            skyConfig.horizonColor = Math::Vector3(0.95f, 0.65f, 0.4f);
            skyConfig.bottomColor = Math::Vector3(0.35f, 0.5f, 0.3f);
            skyConfig.sunDirection = Math::Vector3(0.4f, 0.6f, 0.5f);
            m_RenderSystem->SetSkybox(skyConfig);
        }

        // --- Notes ---
        {
            ECS::Entity notes = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(notes, "Just the Two of Us — Setup Notes");
            auto& nt = m_World->AddComponent<ECS::TransformComponent>(notes);
            nt.position = Math::Vector3(0.0f, 0.0f, 0.0f);
            auto& nc = m_World->AddComponent<ECS::NotesComponent>(notes);
            nc.notes = "JUST THE TWO OF US (Co-op Puzzle Template)\n"
                "=============================================\n"
                "Inspired by 'It Takes Two' — cooperative physics puzzles,\n"
                "environment minigames, and seamless splitscreen.\n"
                "\n"
                "Player A (Teal): WASD controls, left screen half\n"
                "Player B (Coral): Arrow Keys / Gamepad 2, right screen half\n"
                "\n"
                "PUZZLES:\n"
                "  1. Dual Pressure Plates — both players stand on plates to raise bridge\n"
                "  2. Physics Seesaw — one player weighs down, other pushes boulder up\n"
                "  3. Weight Balance — push correct crate combo onto scale\n"
                "\n"
                "MINIGAMES:\n"
                "  - Bowling Lane (east) — roll ball to knock down pins\n"
                "  - Target Practice (west) — one activates targets, other aims\n"
                "\n"
                "GOAL: Both players reach the Victory Chest together.\n"
                "\n"
                "Splitscreen: Each player has their own viewport (left/right halves).\n"
                "Tip: To create a unified camera that splits when players walk apart,\n"
                "use a Visual Script to detect distance and swap between a single\n"
                "shared camera (viewportWidth=1.0) and two split cameras.\n";
        }

        // Render settings
        m_RenderSystem->SetShadowsEnabled(true);
        m_RenderSystem->SetAmbientIntensity(0.15f);

        // Post-processing
        if (m_PostProcessing) {
            auto& pp = m_PostProcessing->GetSettings();
            pp.fxaaEnabled = 1;
            pp.bloomEnabled = 1;
            pp.bloomIntensity = 0.15f;
        }
    }

    else if (templateId == "shadowtest") {
        // === SHADOW TEST TEMPLATE ===
        // Bright white ground so shadows are clearly visible
        {
            ECS::Entity ground = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(ground, "Ground");
            auto& gt = m_World->AddComponent<ECS::TransformComponent>(ground);
            gt.position = Math::Vector3(0.0f, 0.0f, 0.0f);
            gt.scale = Math::Vector3(40.0f, 0.1f, 40.0f);
            auto& gmat = m_World->AddComponent<ECS::MaterialComponent>(ground);
            gmat.baseColor = Math::Vector3(0.9f, 0.9f, 0.9f);
            gmat.roughness = 0.95f;
            gmat.receiveShadows = true;
            gmat.castShadows = false;
            m_World->AddComponent<ECS::MeshComponent>(ground, Renderer::MeshFactory::CreateCube(1.0f));
        }

        // Cube
        {
            ECS::Entity cube = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(cube, "Cube");
            auto& ct = m_World->AddComponent<ECS::TransformComponent>(cube);
            ct.position = Math::Vector3(0.0f, 1.0f, 0.0f);
            auto& cm = m_World->AddComponent<ECS::MaterialComponent>(cube);
            cm.baseColor = Math::Vector3(0.8f, 0.2f, 0.2f);
            cm.castShadows = true;
            m_World->AddComponent<ECS::MeshComponent>(cube, Renderer::MeshFactory::CreateCube(2.0f));
        }

        // Tall pillar
        {
            ECS::Entity pillar = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(pillar, "Pillar");
            auto& pt = m_World->AddComponent<ECS::TransformComponent>(pillar);
            pt.position = Math::Vector3(-4.0f, 2.0f, -2.0f);
            pt.scale = Math::Vector3(0.5f, 4.0f, 0.5f);
            auto& pm = m_World->AddComponent<ECS::MaterialComponent>(pillar);
            pm.baseColor = Math::Vector3(0.2f, 0.6f, 0.2f);
            pm.castShadows = true;
            m_World->AddComponent<ECS::MeshComponent>(pillar, Renderer::MeshFactory::CreateCube(1.0f));
        }

        // Sphere
        {
            ECS::Entity sphere = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(sphere, "Sphere");
            auto& st = m_World->AddComponent<ECS::TransformComponent>(sphere);
            st.position = Math::Vector3(4.0f, 1.0f, 2.0f);
            auto& sm = m_World->AddComponent<ECS::MaterialComponent>(sphere);
            sm.baseColor = Math::Vector3(0.2f, 0.3f, 0.9f);
            sm.castShadows = true;
            m_World->AddComponent<ECS::MeshComponent>(sphere, Renderer::MeshFactory::CreateSphere(1.0f, 24, 16));
        }

        // Capsule (like a character)
        {
            ECS::Entity capsule = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(capsule, "Character");
            auto& ct = m_World->AddComponent<ECS::TransformComponent>(capsule);
            ct.position = Math::Vector3(2.0f, 1.0f, -3.0f);
            auto& cm = m_World->AddComponent<ECS::MaterialComponent>(capsule);
            cm.baseColor = Math::Vector3(0.9f, 0.6f, 0.1f);
            cm.castShadows = true;
            m_World->AddComponent<ECS::MeshComponent>(capsule, Renderer::MeshFactory::CreateCapsule(0.4f, 1.2f));
        }

        // Archway (two pillars + lintel — interesting shadow shapes)
        {
            ECS::Entity pillarL = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(pillarL, "Arch Left");
            auto& plt = m_World->AddComponent<ECS::TransformComponent>(pillarL);
            plt.position = Math::Vector3(-6.0f, 1.5f, -5.0f);
            plt.scale = Math::Vector3(0.4f, 3.0f, 0.4f);
            auto& plm = m_World->AddComponent<ECS::MaterialComponent>(pillarL);
            plm.baseColor = Math::Vector3(0.6f, 0.55f, 0.5f);
            plm.castShadows = true;
            m_World->AddComponent<ECS::MeshComponent>(pillarL, Renderer::MeshFactory::CreateCube(1.0f));
        }
        {
            ECS::Entity pillarR = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(pillarR, "Arch Right");
            auto& prt = m_World->AddComponent<ECS::TransformComponent>(pillarR);
            prt.position = Math::Vector3(-2.0f, 1.5f, -5.0f);
            prt.scale = Math::Vector3(0.4f, 3.0f, 0.4f);
            auto& prm = m_World->AddComponent<ECS::MaterialComponent>(pillarR);
            prm.baseColor = Math::Vector3(0.6f, 0.55f, 0.5f);
            prm.castShadows = true;
            m_World->AddComponent<ECS::MeshComponent>(pillarR, Renderer::MeshFactory::CreateCube(1.0f));
        }
        {
            ECS::Entity lintel = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(lintel, "Arch Lintel");
            auto& llt = m_World->AddComponent<ECS::TransformComponent>(lintel);
            llt.position = Math::Vector3(-4.0f, 3.2f, -5.0f);
            llt.scale = Math::Vector3(4.4f, 0.4f, 0.5f);
            auto& llm = m_World->AddComponent<ECS::MaterialComponent>(lintel);
            llm.baseColor = Math::Vector3(0.55f, 0.5f, 0.45f);
            llm.castShadows = true;
            m_World->AddComponent<ECS::MeshComponent>(lintel, Renderer::MeshFactory::CreateCube(1.0f));
        }

        // Rotated cube (angled shadow)
        {
            ECS::Entity rotCube = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(rotCube, "Rotated Cube");
            auto& rct = m_World->AddComponent<ECS::TransformComponent>(rotCube);
            rct.position = Math::Vector3(6.0f, 1.0f, -4.0f);
            rct.rotation = Math::Quaternion(Math::Vector3(0, 1, 0), Math::Radians(45.0f));
            auto& rcm = m_World->AddComponent<ECS::MaterialComponent>(rotCube);
            rcm.baseColor = Math::Vector3(0.7f, 0.4f, 0.6f);
            rcm.castShadows = true;
            m_World->AddComponent<ECS::MeshComponent>(rotCube, Renderer::MeshFactory::CreateCube(1.5f));
        }

        // Point light (warm, casts different shadow than directional)
        {
            ECS::Entity pointLight = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(pointLight, "Point Light");
            auto& plt = m_World->AddComponent<ECS::TransformComponent>(pointLight);
            plt.position = Math::Vector3(-4.0f, 3.0f, 3.0f);
            auto& plc = m_World->AddComponent<ECS::LightComponent>(pointLight);
            plc.type = ECS::LightType::Point;
            plc.color = Math::Vector3(1.0f, 0.85f, 0.6f);
            plc.intensity = 3.0f;
            plc.range = 12.0f;
        }

        // Spot light (focused cone shadow)
        {
            ECS::Entity spotLight = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(spotLight, "Spot Light");
            auto& slt = m_World->AddComponent<ECS::TransformComponent>(spotLight);
            slt.position = Math::Vector3(5.0f, 4.0f, -2.0f);
            slt.rotation = Math::Quaternion(Math::Vector3(1, 0, 0), Math::Radians(-70.0f));
            auto& slc = m_World->AddComponent<ECS::LightComponent>(spotLight);
            slc.type = ECS::LightType::Spot;
            slc.color = Math::Vector3(0.8f, 0.9f, 1.0f);
            slc.intensity = 5.0f;
            slc.range = 15.0f;
            slc.outerConeAngle = 35.0f;
        }

        // Procedural skybox (daytime)
        {
            Renderer::SkyboxConfig skyConfig;
            skyConfig.type = Renderer::SkyboxType::Procedural;
            skyConfig.topColor = Math::Vector3(0.15f, 0.35f, 0.85f);
            skyConfig.horizonColor = Math::Vector3(0.6f, 0.75f, 1.0f);
            skyConfig.bottomColor = Math::Vector3(0.8f, 0.85f, 0.9f);
            skyConfig.sunDirection = Math::Vector3(0.5f, 0.8f, 0.3f);
            m_RenderSystem->SetSkybox(skyConfig);
        }

        // Render settings: shadows ON, low ambient to make shadows obvious
        m_RenderSystem->SetShadowsEnabled(true);
        m_RenderSystem->SetAmbientIntensity(0.08f);

        if (m_PostProcessing) {
            auto& pp = m_PostProcessing->GetSettings();
            pp.fxaaEnabled = 1;
        }
    }

    else if (templateId == "flash_td") {
        // Tower defense — path + tower slots + spawn
        {
            ECS::Entity bg = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(bg, "TD Background");
            auto& bgt = m_World->AddComponent<ECS::TransformComponent>(bg);
            bgt.position = Math::Vector3(0, 0, 0);
            auto& bgs = m_World->AddComponent<ECS::Sprite2DComponent>(bg);
            bgs.srcWidth = 600; bgs.srcHeight = 400;
            bgs.tint = Math::Vector3(0.3f, 0.5f, 0.2f);  // Green grass
        }
        // Path waypoints
        for (i32 i = 0; i < 5; ++i) {
            ECS::Entity wp = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(wp, "Waypoint " + std::to_string(i + 1));
            auto& wt = m_World->AddComponent<ECS::TransformComponent>(wp);
            f32 x = -5.0f + i * 2.5f;
            f32 y = (i % 2 == 0) ? -1.5f : 1.5f;
            wt.position = Math::Vector3(x, y, 0);
            auto& ws = m_World->AddComponent<ECS::Sprite2DComponent>(wp);
            ws.srcWidth = 32; ws.srcHeight = 32;
            ws.tint = Math::Vector3(0.6f, 0.5f, 0.3f);  // Dirt path
        }
        // Tower slots
        for (i32 i = 0; i < 4; ++i) {
            ECS::Entity tower = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(tower, "Tower Slot " + std::to_string(i + 1));
            auto& tt = m_World->AddComponent<ECS::TransformComponent>(tower);
            tt.position = Math::Vector3(-3.0f + i * 2.0f, 0.0f, 0.0f);
            auto& ts = m_World->AddComponent<ECS::Sprite2DComponent>(tower);
            ts.srcWidth = 48; ts.srcHeight = 48;
            ts.tint = Math::Vector3(0.5f, 0.5f, 0.6f);
            m_World->AddComponent<ECS::BoxColliderComponent>(tower);
        }
        {
            ECS::Entity cam = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(cam, "Camera");
            auto& ct = m_World->AddComponent<ECS::TransformComponent>(cam);
            ct.position = Math::Vector3(0, 0, 5);
            auto& cc = m_World->AddComponent<ECS::CameraComponent>(cam);
            cc.projectionType = ECS::ProjectionType::Orthographic; cc.orthoSize = 8.0f; cc.isActive = true;
        }
        // Enemy entity on path
        {
            ECS::Entity enemy = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(enemy, "Creep 1");
            auto& et = m_World->AddComponent<ECS::TransformComponent>(enemy);
            et.position = Math::Vector3(-5.0f, -1.5f, 0);
            auto& es = m_World->AddComponent<ECS::Sprite2DComponent>(enemy);
            es.srcWidth = 28; es.srcHeight = 28;
            es.tint = Math::Vector3(0.8f, 0.2f, 0.15f);
            es.sortingLayer = 2;
            auto& eh = m_World->AddComponent<ECS::HealthComponent>(enemy);
            eh.maxHealth = 20.0f; eh.currentHealth = 20.0f;
            auto& etag = m_World->AddComponent<ECS::TagComponent>(enemy);
            etag.tags.push_back("enemy");
        }
        // Gold / Wave HUD
        {
            ECS::Entity goldUI = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(goldUI, "Gold Display");
            auto& gt = m_World->AddComponent<ECS::TransformComponent>(goldUI);
            gt.position = Math::Vector3(-4.0f, 3.5f, 0);
            auto& gtext = m_World->AddComponent<ECS::TextComponent>(goldUI);
            gtext.text = "Gold: 100  Wave: 1/5";
            gtext.fontSize = 24.0f;
            gtext.textColor = Math::Vector3(1.0f, 0.85f, 0.0f);
        }
        // Lives HUD
        {
            ECS::Entity livesUI = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(livesUI, "Lives Display");
            auto& lt = m_World->AddComponent<ECS::TransformComponent>(livesUI);
            lt.position = Math::Vector3(3.0f, 3.5f, 0);
            auto& ltext = m_World->AddComponent<ECS::TextComponent>(livesUI);
            ltext.text = "Lives: 10";
            ltext.fontSize = 24.0f;
            ltext.textColor = Math::Vector3(1.0f, 0.3f, 0.3f);
        }
        m_FlashTimelineData = FlashTimelineData{};
        m_FlashTimelineData.name = "TowerDefense";
        m_FlashTimelineData.AddLayer("Background");
        m_FlashTimelineData.AddLayer("Path");
        m_FlashTimelineData.AddLayer("Towers");
        m_FlashTimelineData.AddLayer("Enemies");
        m_FlashTimelineEditor.SetTimeline(&m_FlashTimelineData);
    }

    else if (templateId == "flash_dress") {
        // Dress up — character base + draggable items
        {
            ECS::Entity base = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(base, "Character");
            auto& bt = m_World->AddComponent<ECS::TransformComponent>(base);
            bt.position = Math::Vector3(-1.5f, 0, 0);
            auto& bs = m_World->AddComponent<ECS::Sprite2DComponent>(base);
            bs.srcWidth = 128; bs.srcHeight = 256;
            bs.tint = Math::Vector3(0.9f, 0.8f, 0.7f);
            bs.sortingLayer = 0;
        }
        const char* items[] = { "Hat", "Shirt", "Pants", "Shoes", "Accessory", "Hair" };
        for (i32 i = 0; i < 6; ++i) {
            ECS::Entity item = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(item, items[i]);
            auto& it = m_World->AddComponent<ECS::TransformComponent>(item);
            it.position = Math::Vector3(3.0f, 3.0f - i * 1.2f, 0);
            auto& is = m_World->AddComponent<ECS::Sprite2DComponent>(item);
            is.srcWidth = 48; is.srcHeight = 48;
            is.tint = Math::Vector3(
                0.5f + (i % 3) * 0.2f,
                0.3f + (i % 2) * 0.4f,
                0.7f - (i % 3) * 0.2f
            );
            is.sortingLayer = 1;
            m_World->AddComponent<ECS::BoxColliderComponent>(item);
        }
        // Background (dressing room)
        {
            ECS::Entity bg = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(bg, "Background");
            auto& bgt = m_World->AddComponent<ECS::TransformComponent>(bg);
            bgt.position = Math::Vector3(0, 0, -0.1f);
            auto& bgs = m_World->AddComponent<ECS::Sprite2DComponent>(bg);
            bgs.srcWidth = 500; bgs.srcHeight = 400;
            bgs.tint = Math::Vector3(0.85f, 0.8f, 0.9f);  // Light lavender
            bgs.sortingLayer = -1;
        }
        // Save outfit button
        {
            ECS::Entity saveBtn = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(saveBtn, "Save Button");
            auto& sbt = m_World->AddComponent<ECS::TransformComponent>(saveBtn);
            sbt.position = Math::Vector3(3.0f, -3.5f, 0);
            auto& sbs = m_World->AddComponent<ECS::Sprite2DComponent>(saveBtn);
            sbs.srcWidth = 80; sbs.srcHeight = 32;
            sbs.tint = Math::Vector3(0.3f, 0.7f, 0.3f);
            sbs.sortingLayer = 3;
            m_World->AddComponent<ECS::BoxColliderComponent>(saveBtn);
            auto& sbtag = m_World->AddComponent<ECS::TagComponent>(saveBtn);
            sbtag.tags.push_back("save_button");
        }
        // Clear button
        {
            ECS::Entity clearBtn = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(clearBtn, "Clear Button");
            auto& cbt = m_World->AddComponent<ECS::TransformComponent>(clearBtn);
            cbt.position = Math::Vector3(3.0f, -2.5f, 0);
            auto& cbs = m_World->AddComponent<ECS::Sprite2DComponent>(clearBtn);
            cbs.srcWidth = 80; cbs.srcHeight = 32;
            cbs.tint = Math::Vector3(0.7f, 0.3f, 0.3f);
            cbs.sortingLayer = 3;
            m_World->AddComponent<ECS::BoxColliderComponent>(clearBtn);
            auto& cbtag = m_World->AddComponent<ECS::TagComponent>(clearBtn);
            cbtag.tags.push_back("clear_button");
        }
        {
            ECS::Entity cam = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(cam, "Camera");
            auto& ct = m_World->AddComponent<ECS::TransformComponent>(cam);
            ct.position = Math::Vector3(0, 0, 5);
            auto& cc = m_World->AddComponent<ECS::CameraComponent>(cam);
            cc.projectionType = ECS::ProjectionType::Orthographic; cc.orthoSize = 6.0f; cc.isActive = true;
        }
    }

    else if (templateId == "flash_escape") {
        // Escape room — room background + interactive objects + inventory
        {
            ECS::Entity room = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(room, "Room");
            auto& rt = m_World->AddComponent<ECS::TransformComponent>(room);
            rt.position = Math::Vector3(0, 0, 0);
            auto& rs = m_World->AddComponent<ECS::Sprite2DComponent>(room);
            rs.srcWidth = 550; rs.srcHeight = 400;
            rs.tint = Math::Vector3(0.4f, 0.35f, 0.3f);
        }
        const char* objects[] = { "Door", "Drawer", "Safe", "Painting", "Key" };
        for (i32 i = 0; i < 5; ++i) {
            ECS::Entity obj = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(obj, objects[i]);
            auto& ot = m_World->AddComponent<ECS::TransformComponent>(obj);
            ot.position = Math::Vector3(-3.0f + i * 1.5f, i % 2 == 0 ? 0.0f : 1.5f, 0);
            auto& os = m_World->AddComponent<ECS::Sprite2DComponent>(obj);
            os.srcWidth = 48; os.srcHeight = 48;
            os.tint = Math::Vector3(0.6f + i * 0.05f, 0.5f, 0.3f);
            os.sortingLayer = 1;
            m_World->AddComponent<ECS::BoxColliderComponent>(obj);
            auto& notes = m_World->AddComponent<ECS::NotesComponent>(obj);
            notes.notes = std::string("Interactive object: ") + objects[i];
        }
        // Inventory bar at bottom
        {
            ECS::Entity invBar = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(invBar, "Inventory Bar");
            auto& ibt = m_World->AddComponent<ECS::TransformComponent>(invBar);
            ibt.position = Math::Vector3(0, -3.5f, 0);
            auto& ibs = m_World->AddComponent<ECS::Sprite2DComponent>(invBar);
            ibs.srcWidth = 400; ibs.srcHeight = 48;
            ibs.tint = Math::Vector3(0.2f, 0.2f, 0.25f);
            ibs.sortingLayer = 3;
        }
        // Inventory slots
        for (i32 i = 0; i < 5; ++i) {
            ECS::Entity slot = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(slot, "Inv Slot " + std::to_string(i + 1));
            auto& st = m_World->AddComponent<ECS::TransformComponent>(slot);
            st.position = Math::Vector3(-2.0f + i * 1.0f, -3.5f, 0);
            auto& ss = m_World->AddComponent<ECS::Sprite2DComponent>(slot);
            ss.srcWidth = 40; ss.srcHeight = 40;
            ss.tint = Math::Vector3(0.35f, 0.35f, 0.4f);
            ss.sortingLayer = 4;
            auto& stag = m_World->AddComponent<ECS::TagComponent>(slot);
            stag.tags.push_back("inv_slot");
        }
        // Hint text
        {
            ECS::Entity hint = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(hint, "Hint Text");
            auto& ht = m_World->AddComponent<ECS::TransformComponent>(hint);
            ht.position = Math::Vector3(0, 3.0f, 0);
            auto& htext = m_World->AddComponent<ECS::TextComponent>(hint);
            htext.text = "Find the key to unlock the door...";
            htext.fontSize = 20.0f;
            htext.textColor = Math::Vector3(0.8f, 0.8f, 0.6f);
        }
        {
            ECS::Entity cam = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(cam, "Camera");
            auto& ct = m_World->AddComponent<ECS::TransformComponent>(cam);
            ct.position = Math::Vector3(0, 0, 5);
            auto& cc = m_World->AddComponent<ECS::CameraComponent>(cam);
            cc.projectionType = ECS::ProjectionType::Orthographic; cc.orthoSize = 6.0f; cc.isActive = true;
        }
    }

    else if (templateId == "flash_rhythm") {
        // Rhythm game — note lanes + receptor line + score
        for (i32 lane = 0; lane < 4; ++lane) {
            ECS::Entity receptor = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(receptor,
                "Receptor " + std::to_string(lane + 1));
            auto& rt = m_World->AddComponent<ECS::TransformComponent>(receptor);
            rt.position = Math::Vector3(-1.5f + lane * 1.0f, -3.0f, 0);
            auto& rs = m_World->AddComponent<ECS::Sprite2DComponent>(receptor);
            rs.srcWidth = 40; rs.srcHeight = 40;
            rs.tint = Math::Vector3(0.4f, 0.4f, 0.8f);
            rs.sortingLayer = 1;

            // Sample falling notes
            for (i32 n = 0; n < 3; ++n) {
                ECS::Entity note = m_World->CreateEntity();
                m_World->AddComponent<ECS::NameComponent>(note,
                    "Note L" + std::to_string(lane + 1) + " N" + std::to_string(n + 1));
                auto& nt = m_World->AddComponent<ECS::TransformComponent>(note);
                nt.position = Math::Vector3(-1.5f + lane * 1.0f, 1.0f + n * 2.0f, 0);
                auto& ns = m_World->AddComponent<ECS::Sprite2DComponent>(note);
                ns.srcWidth = 36; ns.srcHeight = 16;
                ns.tint = Math::Vector3(
                    lane == 0 ? 1.0f : 0.3f,
                    lane == 1 ? 1.0f : 0.3f,
                    lane >= 2 ? 1.0f : 0.3f
                );
                ns.sortingLayer = 2;
            }
        }
        {
            ECS::Entity cam = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(cam, "Camera");
            auto& ct = m_World->AddComponent<ECS::TransformComponent>(cam);
            ct.position = Math::Vector3(0, 0, 5);
            auto& cc = m_World->AddComponent<ECS::CameraComponent>(cam);
            cc.projectionType = ECS::ProjectionType::Orthographic; cc.orthoSize = 6.0f; cc.isActive = true;
        }
        // Background
        {
            ECS::Entity bg = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(bg, "Background");
            auto& bgt = m_World->AddComponent<ECS::TransformComponent>(bg);
            bgt.position = Math::Vector3(0, 0, -0.1f);
            auto& bgs = m_World->AddComponent<ECS::Sprite2DComponent>(bg);
            bgs.srcWidth = 500; bgs.srcHeight = 400;
            bgs.tint = Math::Vector3(0.08f, 0.06f, 0.15f);  // Dark purple
            bgs.sortingLayer = -1;
        }
        // Receptor line (judgment line glow)
        {
            ECS::Entity line = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(line, "Judgment Line");
            auto& lt = m_World->AddComponent<ECS::TransformComponent>(line);
            lt.position = Math::Vector3(0, -3.0f, 0);
            auto& ls = m_World->AddComponent<ECS::Sprite2DComponent>(line);
            ls.srcWidth = 300; ls.srcHeight = 4;
            ls.tint = Math::Vector3(0.3f, 0.3f, 0.9f);
            ls.sortingLayer = 0;
        }
        // Score text
        {
            ECS::Entity score = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(score, "Score");
            auto& st = m_World->AddComponent<ECS::TransformComponent>(score);
            st.position = Math::Vector3(3.0f, 4.0f, 0);
            auto& stext = m_World->AddComponent<ECS::TextComponent>(score);
            stext.text = "Score: 0";
            stext.fontSize = 28.0f;
            stext.textColor = Math::Vector3(1.0f, 1.0f, 1.0f);
            auto& stag = m_World->AddComponent<ECS::TagComponent>(score);
            stag.tags.push_back("score");
        }
        // Combo counter
        {
            ECS::Entity combo = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(combo, "Combo Counter");
            auto& ct = m_World->AddComponent<ECS::TransformComponent>(combo);
            ct.position = Math::Vector3(-3.0f, 4.0f, 0);
            auto& ctext = m_World->AddComponent<ECS::TextComponent>(combo);
            ctext.text = "Combo: 0";
            ctext.fontSize = 24.0f;
            ctext.textColor = Math::Vector3(1.0f, 0.8f, 0.2f);
            auto& ctag = m_World->AddComponent<ECS::TagComponent>(combo);
            ctag.tags.push_back("combo");
        }
        // Judgment text (Perfect/Great/Good/Miss)
        {
            ECS::Entity judgment = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(judgment, "Judgment");
            auto& jt = m_World->AddComponent<ECS::TransformComponent>(judgment);
            jt.position = Math::Vector3(0, -1.5f, 0);
            auto& jtext = m_World->AddComponent<ECS::TextComponent>(judgment);
            jtext.text = "";
            jtext.fontSize = 36.0f;
            jtext.textColor = Math::Vector3(0.3f, 1.0f, 0.5f);
            auto& jtag = m_World->AddComponent<ECS::TagComponent>(judgment);
            jtag.tags.push_back("judgment");
        }
        m_FlashTimelineData = FlashTimelineData{};
        m_FlashTimelineData.name = "RhythmGame";
        m_FlashTimelineData.frameRate = 60.0f;
        m_FlashTimelineData.AddLayer("Background");
        m_FlashTimelineData.AddLayer("Receptors");
        m_FlashTimelineData.AddLayer("Notes");
        m_FlashTimelineData.AddLayer("Effects");
        m_FlashTimelineEditor.SetTimeline(&m_FlashTimelineData);
    }

    // --- Marketplace-only templates (basic starter scenes) ---
    else if (templateId == "hello_sprite") {
        // Simple 2D sprite scene
        {
            ECS::Entity cam = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(cam, "Camera");
            auto& ct = m_World->AddComponent<ECS::TransformComponent>(cam);
            ct.position = Math::Vector3(0, 0, 5);
            auto& cc = m_World->AddComponent<ECS::CameraComponent>(cam);
            cc.projectionType = ECS::ProjectionType::Orthographic; cc.orthoSize = 5.0f; cc.isActive = true;
        }
        {
            ECS::Entity player = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(player, "Player Sprite");
            auto& pt = m_World->AddComponent<ECS::TransformComponent>(player);
            pt.position = Math::Vector3(0, 0, 0);
            auto& ps = m_World->AddComponent<ECS::Sprite2DComponent>(player);
            ps.srcWidth = 32; ps.srcHeight = 32;
            ps.tint = Math::Vector3(0.4f, 0.8f, 0.4f);
            ps.sortingLayer = 1;
            auto& ptag = m_World->AddComponent<ECS::TagComponent>(player);
            ptag.tags.push_back("player");
        }
        {
            ECS::Entity bg = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(bg, "Background");
            auto& bgt = m_World->AddComponent<ECS::TransformComponent>(bg);
            bgt.position = Math::Vector3(0, 0, -0.1f);
            auto& bgs = m_World->AddComponent<ECS::Sprite2DComponent>(bg);
            bgs.srcWidth = 400; bgs.srcHeight = 300;
            bgs.tint = Math::Vector3(0.15f, 0.15f, 0.25f);
            bgs.sortingLayer = -1;
        }
    }
    else if (templateId == "neon_runner") {
        // Synthwave runner scene
        {
            ECS::Entity cam = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(cam, "Camera");
            auto& ct = m_World->AddComponent<ECS::TransformComponent>(cam);
            ct.position = Math::Vector3(0, 0, 5);
            auto& cc = m_World->AddComponent<ECS::CameraComponent>(cam);
            cc.projectionType = ECS::ProjectionType::Orthographic; cc.orthoSize = 6.0f; cc.isActive = true;
        }
        {
            ECS::Entity player = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(player, "Runner");
            auto& pt = m_World->AddComponent<ECS::TransformComponent>(player);
            pt.position = Math::Vector3(-3.0f, -2.0f, 0);
            auto& ps = m_World->AddComponent<ECS::Sprite2DComponent>(player);
            ps.srcWidth = 24; ps.srcHeight = 32;
            ps.tint = Math::Vector3(0.9f, 0.2f, 0.9f);
            ps.sortingLayer = 2;
            auto& ptag = m_World->AddComponent<ECS::TagComponent>(player);
            ptag.tags.push_back("player");
        }
        // Ground
        {
            ECS::Entity ground = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(ground, "Ground");
            auto& gt = m_World->AddComponent<ECS::TransformComponent>(ground);
            gt.position = Math::Vector3(0, -3.5f, 0);
            auto& gs = m_World->AddComponent<ECS::Sprite2DComponent>(ground);
            gs.srcWidth = 500; gs.srcHeight = 20;
            gs.tint = Math::Vector3(0.2f, 0.05f, 0.3f);
            gs.sortingLayer = 0;
        }
        // Obstacles
        for (i32 i = 0; i < 3; ++i) {
            ECS::Entity obs = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(obs, "Obstacle " + std::to_string(i + 1));
            auto& ot = m_World->AddComponent<ECS::TransformComponent>(obs);
            ot.position = Math::Vector3(3.0f + i * 4.0f, -2.0f, 0);
            auto& os = m_World->AddComponent<ECS::Sprite2DComponent>(obs);
            os.srcWidth = 20; os.srcHeight = 40;
            os.tint = Math::Vector3(1.0f, 0.3f, 0.5f);
            os.sortingLayer = 1;
            auto& otag = m_World->AddComponent<ECS::TagComponent>(obs);
            otag.tags.push_back("obstacle");
        }
        // Score
        {
            ECS::Entity score = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(score, "Score");
            auto& st = m_World->AddComponent<ECS::TransformComponent>(score);
            st.position = Math::Vector3(3.0f, 4.5f, 0);
            auto& stext = m_World->AddComponent<ECS::TextComponent>(score);
            stext.text = "Score: 0";
            stext.fontSize = 24.0f;
            stext.textColor = Math::Vector3(0.9f, 0.2f, 0.9f);
        }
    }
    else if (templateId == "cozy_farm") {
        // Cozy farming scene
        {
            ECS::Entity cam = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(cam, "Camera");
            auto& ct = m_World->AddComponent<ECS::TransformComponent>(cam);
            ct.position = Math::Vector3(0, 0, 5);
            auto& cc = m_World->AddComponent<ECS::CameraComponent>(cam);
            cc.projectionType = ECS::ProjectionType::Orthographic; cc.orthoSize = 7.0f; cc.isActive = true;
        }
        {
            ECS::Entity farmer = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(farmer, "Farmer");
            auto& ft = m_World->AddComponent<ECS::TransformComponent>(farmer);
            ft.position = Math::Vector3(0, 0, 0);
            auto& fs = m_World->AddComponent<ECS::Sprite2DComponent>(farmer);
            fs.srcWidth = 28; fs.srcHeight = 32;
            fs.tint = Math::Vector3(0.4f, 0.7f, 0.3f);
            fs.sortingLayer = 2;
            auto& ftag = m_World->AddComponent<ECS::TagComponent>(farmer);
            ftag.tags.push_back("player");
        }
        // Farm plots
        for (i32 r = 0; r < 2; ++r) {
            for (i32 c = 0; c < 3; ++c) {
                ECS::Entity plot = m_World->CreateEntity();
                m_World->AddComponent<ECS::NameComponent>(plot,
                    "Plot " + std::to_string(r * 3 + c + 1));
                auto& pt = m_World->AddComponent<ECS::TransformComponent>(plot);
                pt.position = Math::Vector3(-2.0f + c * 2.0f, -2.0f - r * 1.5f, 0);
                auto& ps = m_World->AddComponent<ECS::Sprite2DComponent>(plot);
                ps.srcWidth = 48; ps.srcHeight = 48;
                ps.tint = Math::Vector3(0.35f, 0.25f, 0.12f);
                ps.sortingLayer = 0;
                auto& ptag = m_World->AddComponent<ECS::TagComponent>(plot);
                ptag.tags.push_back("farm-plot");
            }
        }
        // Ground
        {
            ECS::Entity bg = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(bg, "Ground");
            auto& bgt = m_World->AddComponent<ECS::TransformComponent>(bg);
            bgt.position = Math::Vector3(0, 0, -0.1f);
            auto& bgs = m_World->AddComponent<ECS::Sprite2DComponent>(bg);
            bgs.srcWidth = 500; bgs.srcHeight = 400;
            bgs.tint = Math::Vector3(0.3f, 0.55f, 0.2f);
            bgs.sortingLayer = -1;
        }
    }
    else if (templateId == "networking_lobby") {
        // Multiplayer lobby starter — camera + ground + spawn points
        {
            ECS::Entity cam = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(cam, "Camera");
            auto& ct = m_World->AddComponent<ECS::TransformComponent>(cam);
            ct.position = Math::Vector3(0, 5, -8);
            ct.rotation = Math::Quaternion::FromEuler(Math::Vector3(0.35f, 0, 0));
            auto& cc = m_World->AddComponent<ECS::CameraComponent>(cam);
            cc.isActive = true;
        }
        {
            ECS::Entity light = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(light, "Sun");
            auto& lt = m_World->AddComponent<ECS::TransformComponent>(light);
            lt.position = Math::Vector3(0, 8, 0);
            lt.rotation = Math::Quaternion::FromEuler(Math::Vector3(0.8f, 0.3f, 0));
            auto& lc = m_World->AddComponent<ECS::LightComponent>(light);
            lc.type = ECS::LightType::Directional;
            lc.color = Math::Vector3(1, 1, 0.95f); lc.intensity = 1.2f;
        }
        {
            ECS::Entity ground = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(ground, "Ground");
            auto& gt = m_World->AddComponent<ECS::TransformComponent>(ground);
            gt.position = Math::Vector3(0, 0, 0);
            gt.scale = Math::Vector3(20, 0.1f, 20);
            m_World->AddComponent<ECS::MeshComponent>(ground, Renderer::MeshFactory::CreateCube(1.0f));
            auto& gmat = m_World->AddComponent<ECS::MaterialComponent>(ground);
            gmat.baseColor = Math::Vector3(0.3f, 0.4f, 0.5f);
        }
        // Spawn points
        for (i32 i = 0; i < 4; ++i) {
            ECS::Entity spawn = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(spawn, "Spawn " + std::to_string(i + 1));
            auto& st = m_World->AddComponent<ECS::TransformComponent>(spawn);
            f32 angle = static_cast<f32>(i) * 1.5708f;
            st.position = Math::Vector3(std::cos(angle) * 4.0f, 0.5f, std::sin(angle) * 4.0f);
            st.scale = Math::Vector3(0.3f, 1.0f, 0.3f);
            m_World->AddComponent<ECS::MeshComponent>(spawn, Renderer::MeshFactory::CreateCylinder(0.5f, 1.0f));
            auto& smat = m_World->AddComponent<ECS::MaterialComponent>(spawn);
            smat.baseColor = Math::Vector3(0.1f, 0.6f, 0.7f);
            smat.emissiveColor = Math::Vector3(0.1f, 0.6f, 0.7f);
            smat.emissiveStrength = 2.0f;
            auto& stag = m_World->AddComponent<ECS::TagComponent>(spawn);
            stag.tags.push_back("spawn-point");
        }
    }
    else if (templateId == "ps1_horror") {
        // PS1-era horror scene — dark corridor + camera + flickering light
        {
            ECS::Entity cam = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(cam, "Fixed Camera");
            auto& ct = m_World->AddComponent<ECS::TransformComponent>(cam);
            ct.position = Math::Vector3(0, 2.5f, -6);
            ct.rotation = Math::Quaternion::FromEuler(Math::Vector3(0.15f, 0, 0));
            auto& cc = m_World->AddComponent<ECS::CameraComponent>(cam);
            cc.isActive = true;
        }
        {
            ECS::Entity light = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(light, "Ceiling Light");
            auto& lt = m_World->AddComponent<ECS::TransformComponent>(light);
            lt.position = Math::Vector3(0, 3.5f, 0);
            auto& lc = m_World->AddComponent<ECS::LightComponent>(light);
            lc.type = ECS::LightType::Point;
            lc.color = Math::Vector3(0.9f, 0.7f, 0.4f); lc.intensity = 1.5f; lc.range = 10.0f;
        }
        // Corridor floor
        {
            ECS::Entity floor = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(floor, "Floor");
            auto& ft = m_World->AddComponent<ECS::TransformComponent>(floor);
            ft.position = Math::Vector3(0, 0, 0);
            ft.scale = Math::Vector3(4, 0.1f, 12);
            m_World->AddComponent<ECS::MeshComponent>(floor, Renderer::MeshFactory::CreateCube(1.0f));
            auto& fmat = m_World->AddComponent<ECS::MaterialComponent>(floor);
            fmat.baseColor = Math::Vector3(0.15f, 0.12f, 0.1f);
        }
        // Walls
        for (i32 side = -1; side <= 1; side += 2) {
            ECS::Entity wall = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(wall, side < 0 ? "Left Wall" : "Right Wall");
            auto& wt = m_World->AddComponent<ECS::TransformComponent>(wall);
            wt.position = Math::Vector3(side * 2.0f, 1.5f, 0);
            wt.scale = Math::Vector3(0.1f, 3.0f, 12.0f);
            m_World->AddComponent<ECS::MeshComponent>(wall, Renderer::MeshFactory::CreateCube(1.0f));
            auto& wmat = m_World->AddComponent<ECS::MaterialComponent>(wall);
            wmat.baseColor = Math::Vector3(0.12f, 0.1f, 0.08f);
        }
        // Player
        {
            ECS::Entity player = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(player, "Player");
            auto& pt = m_World->AddComponent<ECS::TransformComponent>(player);
            pt.position = Math::Vector3(0, 0.5f, -4);
            m_World->AddComponent<ECS::MeshComponent>(player, Renderer::MeshFactory::CreateCapsule(0.3f, 1.0f));
            auto& pmat = m_World->AddComponent<ECS::MaterialComponent>(player);
            pmat.baseColor = Math::Vector3(0.3f, 0.3f, 0.35f);
            auto& ptag = m_World->AddComponent<ECS::TagComponent>(player);
            ptag.tags.push_back("player");
        }
    }
    else if (templateId == "ray_tracing_showcase") {
        // RT showcase — reflective spheres + ground + directional light
        {
            ECS::Entity cam = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(cam, "Camera");
            auto& ct = m_World->AddComponent<ECS::TransformComponent>(cam);
            ct.position = Math::Vector3(0, 3, -7);
            ct.rotation = Math::Quaternion::FromEuler(Math::Vector3(0.3f, 0, 0));
            auto& cc = m_World->AddComponent<ECS::CameraComponent>(cam);
            cc.isActive = true;
        }
        {
            ECS::Entity light = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(light, "Sun");
            auto& lt = m_World->AddComponent<ECS::TransformComponent>(light);
            lt.position = Math::Vector3(0, 10, 0);
            lt.rotation = Math::Quaternion::FromEuler(Math::Vector3(0.9f, 0.5f, 0));
            auto& lc = m_World->AddComponent<ECS::LightComponent>(light);
            lc.type = ECS::LightType::Directional;
            lc.color = Math::Vector3(1, 0.98f, 0.9f); lc.intensity = 1.5f;
        }
        // Ground plane
        {
            ECS::Entity ground = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(ground, "Ground");
            auto& gt = m_World->AddComponent<ECS::TransformComponent>(ground);
            gt.position = Math::Vector3(0, 0, 0);
            gt.scale = Math::Vector3(30, 0.1f, 30);
            m_World->AddComponent<ECS::MeshComponent>(ground, Renderer::MeshFactory::CreateCube(1.0f));
            auto& gmat = m_World->AddComponent<ECS::MaterialComponent>(ground);
            gmat.baseColor = Math::Vector3(0.8f, 0.8f, 0.8f);
            gmat.metallic = 0.0f; gmat.roughness = 0.3f;
        }
        // Reflective spheres
        Math::Vector3 colors[] = {
            Math::Vector3(1.0f, 0.2f, 0.2f),
            Math::Vector3(0.2f, 0.8f, 0.2f),
            Math::Vector3(0.2f, 0.2f, 1.0f),
            Math::Vector3(1.0f, 0.85f, 0.4f),
        };
        for (i32 i = 0; i < 4; ++i) {
            ECS::Entity sphere = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(sphere, "Sphere " + std::to_string(i + 1));
            auto& st = m_World->AddComponent<ECS::TransformComponent>(sphere);
            f32 x = -3.0f + i * 2.0f;
            st.position = Math::Vector3(x, 1.0f, 0);
            m_World->AddComponent<ECS::MeshComponent>(sphere, Renderer::MeshFactory::CreateSphere(0.5f));
            auto& smat = m_World->AddComponent<ECS::MaterialComponent>(sphere);
            smat.baseColor = colors[i];
            smat.metallic = 0.9f; smat.roughness = 0.1f;
        }
        // Glass sphere (translucent)
        {
            ECS::Entity glass = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(glass, "Glass Sphere");
            auto& gt = m_World->AddComponent<ECS::TransformComponent>(glass);
            gt.position = Math::Vector3(0, 1.0f, -2.5f);
            gt.scale = Math::Vector3(1.5f, 1.5f, 1.5f);
            m_World->AddComponent<ECS::MeshComponent>(glass, Renderer::MeshFactory::CreateSphere(0.5f));
            auto& gmat = m_World->AddComponent<ECS::MaterialComponent>(glass);
            gmat.baseColor = Math::Vector3(0.95f, 0.95f, 1.0f);
            gmat.metallic = 0.0f; gmat.roughness = 0.0f;
            gmat.transmission = 0.95f; gmat.ior = 1.5f;
        }
    }
    else if (templateId == "procedural_world") {
        // Procedural world — camera + directional light + terrain placeholder + trees
        {
            ECS::Entity cam = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(cam, "Camera");
            auto& ct = m_World->AddComponent<ECS::TransformComponent>(cam);
            ct.position = Math::Vector3(0, 8, -12);
            ct.rotation = Math::Quaternion::FromEuler(Math::Vector3(0.4f, 0, 0));
            auto& cc = m_World->AddComponent<ECS::CameraComponent>(cam);
            cc.isActive = true;
        }
        {
            ECS::Entity light = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(light, "Sun");
            auto& lt = m_World->AddComponent<ECS::TransformComponent>(light);
            lt.position = Math::Vector3(0, 15, 0);
            lt.rotation = Math::Quaternion::FromEuler(Math::Vector3(1.0f, 0.4f, 0));
            auto& lc = m_World->AddComponent<ECS::LightComponent>(light);
            lc.type = ECS::LightType::Directional;
            lc.color = Math::Vector3(1, 0.95f, 0.85f); lc.intensity = 1.3f;
        }
        // Terrain base
        {
            ECS::Entity terrain = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(terrain, "Terrain");
            auto& tt = m_World->AddComponent<ECS::TransformComponent>(terrain);
            tt.position = Math::Vector3(0, 0, 0);
            tt.scale = Math::Vector3(40, 0.5f, 40);
            m_World->AddComponent<ECS::MeshComponent>(terrain, Renderer::MeshFactory::CreateCube(1.0f));
            auto& tmat = m_World->AddComponent<ECS::MaterialComponent>(terrain);
            tmat.baseColor = Math::Vector3(0.35f, 0.55f, 0.25f);
            tmat.roughness = 0.9f;
        }
        // Sample trees
        for (i32 i = 0; i < 5; ++i) {
            ECS::Entity tree = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(tree, "Tree " + std::to_string(i + 1));
            auto& trt = m_World->AddComponent<ECS::TransformComponent>(tree);
            f32 x = -8.0f + i * 4.0f;
            f32 z = (i % 2 == 0) ? 2.0f : -2.0f;
            trt.position = Math::Vector3(x, 1.5f, z);
            trt.scale = Math::Vector3(0.5f, 3.0f, 0.5f);
            m_World->AddComponent<ECS::MeshComponent>(tree, Renderer::MeshFactory::CreateCylinder(0.5f, 1.0f));
            auto& trmat = m_World->AddComponent<ECS::MaterialComponent>(tree);
            trmat.baseColor = Math::Vector3(0.4f, 0.25f, 0.1f);
        }
    }

    m_CurrentScenePath.clear();
    ENJIN_LOG_INFO(Editor, "Applied template: %s", templateId.c_str());
}

void EditorLayer::SaveCustomTemplate(const std::string& name) {
    if (!m_World) return;

    // Create templates directory next to the executable
    std::filesystem::path templateDir = "templates";
    std::filesystem::create_directories(templateDir);

    // Sanitize name for filename
    std::string safeName = name;
    for (char& c : safeName) {
        if (c == ' ' || c == '/' || c == '\\' || c == ':' || c == '*' ||
            c == '?' || c == '"' || c == '<' || c == '>' || c == '|') {
            c = '_';
        }
    }

    std::string filepath = (templateDir / (safeName + ".enjin")).string();

    Scene::SceneSerializer serializer(m_World);
    Scene::SerializationOptions opts;
    opts.includeVertexData = true;
    auto result = serializer.Save(filepath, opts);

    if (result.success) {
        // Add to the custom templates list if not already present
        bool found = false;
        for (const auto& n : m_CustomTemplateNames) {
            if (n == name) { found = true; break; }
        }
        if (!found) {
            m_CustomTemplateNames.push_back(name);
            m_CustomTemplatePaths.push_back(filepath);
        }
        ENJIN_LOG_INFO(Editor, "Saved custom template: %s -> %s", name.c_str(), filepath.c_str());
    }
}

void EditorLayer::LoadCustomTemplates() {
    m_CustomTemplateNames.clear();
    m_CustomTemplatePaths.clear();

    std::filesystem::path templateDir = "templates";
    if (!std::filesystem::exists(templateDir)) return;

    for (const auto& entry : std::filesystem::directory_iterator(templateDir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".enjin") {
            std::string name = entry.path().stem().string();
            // Replace underscores with spaces for display
            for (char& c : name) {
                if (c == '_') c = ' ';
            }
            m_CustomTemplateNames.push_back(name);
            m_CustomTemplatePaths.push_back(entry.path().string());
        }
    }

    if (!m_CustomTemplateNames.empty()) {
        ENJIN_LOG_INFO(Editor, "Found %zu custom templates", m_CustomTemplateNames.size());
    }
}

void EditorLayer::DrawTemplateCreatorWindow() {
    ImGui::SetNextWindowSize(ImVec2(520 * m_EditorSettings.uiScale, 600 * m_EditorSettings.uiScale), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Template Creator", &m_ShowTemplateCreator)) {
        ImGui::End();
        return;
    }

    // Rescan custom templates when needed
    if (m_TmplNeedsRescan) {
        m_ScannedTemplates = Editor::TemplateCreator::ScanTemplates("templates");
        m_TmplNeedsRescan = false;
        m_TmplDeleteConfirm = -1;
    }

    // --- Save Section ---
    if (ImGui::CollapsingHeader("Save Current Scene as Template", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::InputText("Name", m_TmplName, sizeof(m_TmplName));
        ImGui::InputTextMultiline("Description", m_TmplDescription, sizeof(m_TmplDescription),
                                  ImVec2(-1, 60));
        ImGui::InputText("Author", m_TmplAuthor, sizeof(m_TmplAuthor));

        const char* categoryNames[] = { "2D", "3D", "Multiplayer", "Tools" };
        ImGui::Combo("Category", &m_TmplCategory, categoryNames, IM_ARRAYSIZE(categoryNames));

        ImGui::ColorEdit4("Accent Color", m_TmplAccentColor,
                          ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaPreview);

        ImGui::InputText("Thumbnail (PNG)", m_TmplThumbnailPath, sizeof(m_TmplThumbnailPath));
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Optional path to a PNG thumbnail image for this template");
        }

        ImGui::Spacing();

        bool canSave = m_World && m_TmplName[0] != '\0';
        if (!canSave) ImGui::BeginDisabled();

        if (ImGui::Button("Save as Template", ImVec2(-1, 32))) {
            const char* categoryNames2[] = { "2D", "3D", "Multiplayer", "Tools" };
            Editor::TemplateMetadata meta;
            meta.name = m_TmplName;
            meta.description = m_TmplDescription;
            meta.author = m_TmplAuthor;
            meta.category = categoryNames2[m_TmplCategory];
            meta.accentColor = Math::Vector4(m_TmplAccentColor[0], m_TmplAccentColor[1],
                                             m_TmplAccentColor[2], m_TmplAccentColor[3]);
            meta.thumbnailPath = m_TmplThumbnailPath;

            Scene::SceneSerializer serializer(m_World);
            if (Editor::TemplateCreator::SaveTemplate("templates", meta, m_World, serializer)) {
                // Auto-capture thumbnail from game view render target
                if (m_GameViewRenderTarget && m_GameViewRenderTarget->IsValid()) {
                    auto pixels = m_GameViewRenderTarget->CaptureToPixels();
                    if (!pixels.empty()) {
                        std::string tmplId = meta.id.empty() ? meta.name : meta.id;
                        // Sanitize id same way TemplateCreator does
                        for (auto& c : tmplId) { if (c == ' ') c = '_'; c = static_cast<char>(std::tolower(c)); }
                        Editor::TemplateCreator::SaveThumbnail("templates", tmplId,
                            pixels.data(), m_GameViewRenderTarget->GetWidth(), m_GameViewRenderTarget->GetHeight());
                    }
                }
                m_ConsoleLog.push_back("[Template] Saved: " + meta.name);
                ShowNotification("Template saved: " + std::string(m_TmplName), NotificationType::Success);
                m_TmplNeedsRescan = true;
            } else {
                m_ConsoleLog.push_back("[Template] ERROR: Failed to save template");
            }
        }

        if (!canSave) ImGui::EndDisabled();
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // --- Existing Templates Section ---
    if (ImGui::CollapsingHeader("Custom Templates", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (m_ScannedTemplates.empty()) {
            ImGui::TextDisabled("No custom templates found in 'templates/' directory.");
        }

        for (i32 i = 0; i < static_cast<i32>(m_ScannedTemplates.size()); ++i) {
            const auto& tmpl = m_ScannedTemplates[i];
            ImGui::PushID(i);

            // Accent color bar
            ImVec4 accent(tmpl.accentColor.x, tmpl.accentColor.y,
                          tmpl.accentColor.z, tmpl.accentColor.w);
            ImGui::PushStyleColor(ImGuiCol_Header, accent);
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered,
                ImVec4(accent.x * 1.2f, accent.y * 1.2f, accent.z * 1.2f, accent.w));

            bool nodeOpen = ImGui::TreeNode("##tmpl", "%s", tmpl.name.c_str());

            ImGui::PopStyleColor(2);

            // Category badge
            if (!tmpl.category.empty()) {
                ImGui::SameLine();
                ImGui::TextDisabled("[%s]", tmpl.category.c_str());
            }

            if (nodeOpen) {
                if (!tmpl.description.empty()) {
                    ImGui::TextWrapped("%s", tmpl.description.c_str());
                }
                if (!tmpl.author.empty()) {
                    ImGui::TextDisabled("Author: %s", tmpl.author.c_str());
                }
                ImGui::TextDisabled("ID: %s", tmpl.id.c_str());

                ImGui::Spacing();

                if (ImGui::Button("Load", ImVec2(80, 0))) {
                    if (m_World) {
                        Editor::TemplateMetadata loadedMeta;
                        std::string templatePath = (std::filesystem::path("templates") / tmpl.id).string();
                        Scene::SceneSerializer serializer(m_World);
                        if (Editor::TemplateCreator::LoadTemplate(templatePath, m_World, serializer, loadedMeta)) {
                            m_ConsoleLog.push_back("[Template] Loaded: " + loadedMeta.name);
                            m_CurrentScenePath.clear();
                        } else {
                            m_ConsoleLog.push_back("[Template] ERROR: Failed to load template");
                        }
                    }
                }

                ImGui::SameLine();

                // Delete with confirmation
                if (m_TmplDeleteConfirm == i) {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.15f, 0.15f, 1.0f));
                    if (ImGui::Button("Confirm Delete", ImVec2(120, 0))) {
                        if (Editor::TemplateCreator::DeleteTemplate("templates", tmpl.id)) {
                            m_ConsoleLog.push_back("[Template] Deleted: " + tmpl.name);
                            m_TmplNeedsRescan = true;
                        }
                        m_TmplDeleteConfirm = -1;
                    }
                    ImGui::PopStyleColor();
                    ImGui::SameLine();
                    if (ImGui::Button("Cancel##del", ImVec2(60, 0))) {
                        m_TmplDeleteConfirm = -1;
                    }
                } else {
                    if (ImGui::Button("Delete", ImVec2(80, 0))) {
                        m_TmplDeleteConfirm = i;
                    }
                }

                ImGui::TreePop();
            }

            ImGui::PopID();
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // --- Builtin Templates (read-only list) ---
    if (ImGui::CollapsingHeader("Built-in Templates (read-only)")) {
        ImGui::TextDisabled("These templates ship with the engine and cannot be modified.");
        ImGui::Spacing();

        // List the builtin template names (same ones used in the Project Hub wizard)
        const char* builtinNames[] = {
            "empty", "platformer", "topdown2d", "thirdperson", "firstperson",
            "isometric", "rpg_village", "narrative", "savesystem", "visualscript",
            "uicanvas", "bullethell", "idleclicker", "pointclick", "ps1rpg",
            "visualnovel", "gamemanager", "citybuilder", "fpsarena", "teamsports",
            "towerdefense", "runner", "flower", "fixedcam", "metroidvania",
            "vampsurvivor", "roguelike", "soulslike", "couchcoop", "justtwo", "shadowtest",
            "flash_td", "flash_dress", "flash_escape", "flash_rhythm"
        };

        for (const char* name : builtinNames) {
            ImGui::BulletText("%s", name);
        }
    }

    if (ImGui::Button("Refresh", ImVec2(-1, 0))) {
        m_TmplNeedsRescan = true;
    }

    ImGui::End();
}

// ============================================================================
// Template Marketplace Window
// ============================================================================
void EditorLayer::DrawTemplateMarketplaceWindow() {
    ImGui::SetNextWindowSize(ImVec2(680 * m_EditorSettings.uiScale, 550 * m_EditorSettings.uiScale), ImGuiCond_FirstUseEver);
    bool open = m_TemplateMarketplace.IsOpen();
    if (!ImGui::Begin("Template Marketplace", &open)) {
        ImGui::End();
        m_TemplateMarketplace.SetOpen(open);
        return;
    }
    m_TemplateMarketplace.SetOpen(open);

    // Search bar
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.4f);
    ImGui::InputTextWithHint("##mktsearch", "Search templates...", m_MarketSearchBuf, sizeof(m_MarketSearchBuf));
    ImGui::SameLine();

    // Category filter
    const char* categories[] = { "All", "Starter", "Genre", "Systems", "Retro", "Advanced" };
    ImGui::SetNextItemWidth(100.0f);
    ImGui::Combo("##mktcat", &m_MarketCategoryFilter, categories, 6);
    ImGui::SameLine();

    // Status (maturity) filter
    const char* maturityOpts[] = { "All Status", "Stable", "Beta", "Preview", "Experimental" };
    ImGui::SetNextItemWidth(110.0f);
    ImGui::Combo("##mktstatus", &m_MarketMaturityFilter, maturityOpts, 5);
    ImGui::SameLine();

    // Sort
    const char* sortOpts[] = { "Name", "Rating", "Downloads" };
    ImGui::SetNextItemWidth(90.0f);
    ImGui::Combo("##mktsort", &m_MarketSortBy, sortOpts, 3);

    ImGui::Separator();

    // Get filtered results
    std::string catFilter = m_MarketCategoryFilter > 0 ? categories[m_MarketCategoryFilter] : "";
    auto results = m_TemplateMarketplace.FilterAndSearch(m_MarketSearchBuf, catFilter);

    // Apply maturity filter
    if (m_MarketMaturityFilter > 0) {
        Editor::MaturityTier filterTier = static_cast<Editor::MaturityTier>(m_MarketMaturityFilter - 1);
        results.erase(std::remove_if(results.begin(), results.end(),
            [filterTier](const Editor::MarketplaceEntry* e) { return e->maturity != filterTier; }),
            results.end());
    }

    // Sort results
    if (m_MarketSortBy == 1) { // Rating
        std::sort(results.begin(), results.end(),
            [](const Editor::MarketplaceEntry* a, const Editor::MarketplaceEntry* b) {
                return a->rating > b->rating;
            });
    } else if (m_MarketSortBy == 2) { // Downloads
        std::sort(results.begin(), results.end(),
            [](const Editor::MarketplaceEntry* a, const Editor::MarketplaceEntry* b) {
                return a->downloadCount > b->downloadCount;
            });
    }

    if (results.empty()) {
        DrawEmptyState("{ }", "No Templates Found", "Try a different search or category filter");
    } else {
        // Results count
        ImGui::TextDisabled("%zu template%s", results.size(), results.size() == 1 ? "" : "s");
        ImGui::Spacing();

        // Group results by maturity tier
        struct TierGroup {
            Editor::MaturityTier tier;
            const char* label;
            ImVec4 color;
            std::vector<const Editor::MarketplaceEntry*> entries;
        };
        TierGroup groups[] = {
            { Editor::MaturityTier::Stable,       "Stable",       ImVec4(0.3f, 0.55f, 0.86f, 1.0f), {} },
            { Editor::MaturityTier::Beta,         "Beta",         ImVec4(0.3f, 0.7f, 0.3f, 1.0f),   {} },
            { Editor::MaturityTier::Preview,      "Preview",      ImVec4(0.82f, 0.67f, 0.2f, 1.0f), {} },
            { Editor::MaturityTier::Experimental, "Experimental", ImVec4(0.82f, 0.27f, 0.27f, 1.0f), {} },
        };
        for (auto* entry : results) {
            for (auto& g : groups) {
                if (entry->maturity == g.tier) { g.entries.push_back(entry); break; }
            }
        }

        // Draw each non-empty group
        for (auto& group : groups) {
            if (group.entries.empty()) continue;

            // Group header with colored label and count
            ImGui::PushStyleColor(ImGuiCol_Text, group.color);
            bool groupOpen = ImGui::TreeNodeEx(group.label, ImGuiTreeNodeFlags_DefaultOpen,
                "%s (%zu)", group.label, group.entries.size());
            ImGui::PopStyleColor();
            if (!groupOpen) continue;

            // Subtle separator under group header
            ImGui::Separator();

            for (auto* entry : group.entries) {
                ImGui::PushID(entry->id.c_str());

                // Accent color bar
                ImVec4 accent(entry->accentColor[0], entry->accentColor[1],
                              entry->accentColor[2], entry->accentColor[3]);
                ImGui::PushStyleColor(ImGuiCol_Header, accent);
                ImGui::PushStyleColor(ImGuiCol_HeaderHovered,
                    ImVec4(accent.x * 1.2f, accent.y * 1.2f, accent.z * 1.2f, accent.w));

                bool nodeOpen = ImGui::TreeNode("##mktentry", "%s", entry->name.c_str());
                ImGui::PopStyleColor(2);

                // Badges on same line
                ImGui::SameLine();
                ImGui::TextDisabled("[%s]", entry->category.c_str());
                ImGui::SameLine();
                ImGui::TextDisabled("[%s]", entry->projectMode.c_str());

                // Rating stars + download count
                ImGui::SameLine(ImGui::GetContentRegionAvail().x - 120.0f + ImGui::GetCursorPosX());
                ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.2f, 1.0f), "%.1f", entry->rating);
                ImGui::SameLine();
                ImGui::TextDisabled("(%u)", entry->downloadCount);

                // Install status indicator
                bool installed = m_TemplateMarketplace.IsInstalled(entry->id);
                if (installed) {
                    ImGui::SameLine();
                    ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.3f, 1.0f), "[OK]");
                }

                if (nodeOpen) {
                    // Description
                    ImGui::TextWrapped("%s", entry->description.c_str());
                    ImGui::Spacing();

                    // Metadata
                    ImGui::TextDisabled("Author: %s  |  Version: %s  |  License: %s",
                        entry->author.c_str(), entry->version.c_str(), entry->license.c_str());
                    ImGui::TextDisabled("Quality: %s  |  Maturity: %s  |  Size: %s",
                        Editor::TemplateMarketplace::GetQualityName(entry->quality),
                        Editor::TemplateMarketplace::GetMaturityName(entry->maturity),
                        entry->fileSizeBytes < 1024 ? (std::to_string(entry->fileSizeBytes) + " B").c_str() :
                        (std::to_string(entry->fileSizeBytes / 1024) + " KB").c_str());

                    // Tags
                    if (!entry->tags.empty()) {
                        ImGui::TextDisabled("Tags:");
                        ImGui::SameLine();
                        for (usize t = 0; t < entry->tags.size(); ++t) {
                            if (t > 0) ImGui::SameLine();
                            ImGui::SmallButton(entry->tags[t].c_str());
                        }
                    }

                    ImGui::Spacing();

                    // Install / Uninstall buttons — all templates unlocked
                    if (installed) {
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
                        ImGui::Button("Installed", ImVec2(90, 0));
                        ImGui::PopStyleColor();
                        ImGui::SameLine();
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.15f, 0.15f, 1.0f));
                        if (ImGui::Button("Remove", ImVec2(70, 0))) {
                            m_TemplateMarketplace.Uninstall(entry->id);
                            ShowNotification("Removed: " + entry->name, NotificationType::Info);
                            m_TmplNeedsRescan = true;
                        }
                        ImGui::PopStyleColor();
                    } else {
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.4f, 0.8f, 1.0f));
                        if (ImGui::Button("Install", ImVec2(90, 0))) {
                            if (m_TemplateMarketplace.Install(entry->id)) {
                                ShowNotification("Installed: " + entry->name, NotificationType::Success);
                                m_TmplNeedsRescan = true;
                            } else {
                                ShowNotification("Failed to install: " + entry->name, NotificationType::Error);
                            }
                        }
                        ImGui::PopStyleColor();
                    }

                    ImGui::TreePop();
                }

                ImGui::PopID();
            }

            ImGui::TreePop();
            ImGui::Spacing();
        }
    }

    ImGui::End();
}

// ============================================================================
// Notification Toast System
// ============================================================================

} // namespace Editor
} // namespace Enjin
