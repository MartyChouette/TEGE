#include "Enjin/Platform/Desktop.h"
#include "Enjin/Editor/EditorLayer.h"
#ifndef _WIN32
// POSIX environment for posix_spawn. Declared at GLOBAL scope: a block-scope
// extern inside namespace Enjin mangles as a namespaced symbol under GCC.
extern char** environ;
#endif
#include "Enjin/Editor/InspectorUndo.h"
#include "Enjin/Editor/ScenePicker.h"
#include "Enjin/Core/Version.h"
#include "Enjin/Scripting/ScriptEngine.h"
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
#include "Enjin/ECS/Components/ParallaxMachine.h"
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

// Open a folder in the platform file explorer
static void OpenInExplorer(const std::string& folderPath) {
    if (folderPath.empty() || !std::filesystem::exists(folderPath)) return;
    Platform::OpenInDesktop(folderPath);
}

// Duplicate an entire project folder: copies to ProjectName_Copy, renames the .enjinproject file inside
static std::string DuplicateProject(const std::string& projectFilePath) {
    namespace fs = std::filesystem;
    fs::path srcFile(projectFilePath);
    fs::path srcDir = srcFile.parent_path();
    fs::path parentDir = srcDir.parent_path();
    std::string baseName = srcDir.filename().string();

    // Find a unique destination name (ProjectName_Copy, ProjectName_Copy_2, etc.)
    std::string destName = baseName + "_Copy";
    fs::path destDir = parentDir / destName;
    int suffix = 2;
    while (fs::exists(destDir)) {
        destName = baseName + "_Copy_" + std::to_string(suffix++);
        destDir = parentDir / destName;
    }

    // Copy the entire folder
    std::error_code ec;
    fs::copy(srcDir, destDir, fs::copy_options::recursive, ec);
    if (ec) {
        ENJIN_LOG_ERROR(Build, "Failed to duplicate project: %s", ec.message().c_str());
        return "";
    }

    // Rename the .enjinproject file inside the copy to match the new folder name
    fs::path oldProjFile = destDir / srcFile.filename();
    fs::path newProjFile = destDir / (destName + ".enjinproject");
    if (fs::exists(oldProjFile) && oldProjFile != newProjFile) {
        fs::rename(oldProjFile, newProjFile, ec);
        if (ec) {
            ENJIN_LOG_WARN(Build, "Duplicated project folder but could not rename project file: %s", ec.message().c_str());
            // Return the old file path as fallback
            return oldProjFile.string();
        }
    }

    return newProjFile.string();
}

void EditorLayer::DrawProjectHub() {
    try {
    DrawProjectHubInner();
    } catch (const std::exception& e) {
        ENJIN_LOG_ERROR(Editor, "Project Hub exception: %s", e.what());
    } catch (...) {
        ENJIN_LOG_ERROR(Editor, "Project Hub unknown exception");
    }
}

void EditorLayer::DrawProjectHubInner() {
    ImGuiIO& io = ImGui::GetIO();

    // Process deferred project removal from list (set by "Remove from List" in previous frame).
    // Must run BEFORE any UI code that iterates recentProjects.
    if (!m_HubPendingRemovePath.empty()) {
        m_EditorSettings.RemoveRecentProject(m_HubPendingRemovePath);
        m_EditorSettings.Save();
        m_HubPendingRemovePath.clear();
    }

    // Process deferred project deletion (set by Delete button in previous frame).
    // Must run BEFORE any UI code that iterates recentProjects.
    if (!m_HubPendingDeletePath.empty()) {
        std::string pathToDelete = m_HubPendingDeletePath;
        m_HubPendingDeletePath.clear();
        m_EditorSettings.RemoveRecentProject(pathToDelete);
        m_EditorSettings.Save();
        try {
            std::filesystem::path delDir = std::filesystem::path(pathToDelete).parent_path();
            if (!delDir.empty() && std::filesystem::exists(delDir)) {
                std::error_code ec;
                std::filesystem::remove_all(delDir, ec);
            }
        } catch (...) {}
    }

    // Process deferred project open (set by click in previous frame)
    if (!m_HubPendingOpenPath.empty()) {
        std::string pathToOpen = m_HubPendingOpenPath;
        m_HubPendingOpenPath.clear();
        OpenProjectFromPath(pathToOpen);
        if (!m_ShowProjectHub) return; // Successfully opened, hub is now hidden
    }

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
            // Wizard/Demos pages: draw centered title + sidebar. The brand uses
            // the heading typeface (Playfair Display) so it reads classier.
            ImFont* font = (m_ImGuiLayer && m_ImGuiLayer->GetHeadingFont())
                ? m_ImGuiLayer->GetHeadingFont() : ImGui::GetFont();
            f32 cx = io.DisplaySize.x * 0.5f;
            const char* title = "TEGE";
            f32 titleFontSize = 64.0f;
            ImVec2 titleSz = font->CalcTextSizeA(titleFontSize, FLT_MAX, 0.0f, title);
            ImVec2 titlePos(cx - titleSz.x * 0.5f, 28.0f);
            drawList->AddText(font, titleFontSize, titlePos,
                IM_COL32(199, 218, 196, 255), title);

            const char* subtitle = "Game Engine";
            f32 subFontSize = 24.0f;
            ImVec2 subSz = font->CalcTextSizeA(subFontSize, FLT_MAX, 0.0f, subtitle);
            drawList->AddText(font, subFontSize,
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

        // ===== Project context menu (shared popup for sidebar and landing page) =====
        if (m_HubOpenContextMenu) {
            ImGui::OpenPopup("##HubProjectContextMenu");
            m_HubOpenContextMenu = false;
        }
        if (ImGui::BeginPopup("##HubProjectContextMenu")) {
            std::filesystem::path ctxPath(m_HubContextProjectPath);
            std::string ctxName = ctxPath.stem().string();
            bool ctxExists = std::filesystem::exists(m_HubContextProjectPath);

            // Header: project name (non-interactive)
            ImGui::TextDisabled("%s", ctxName.c_str());
            ImGui::Separator();

            // Open
            if (ImGui::MenuItem(">>  Open", nullptr, false, ctxExists)) {
                OpenProjectFromPath(m_HubContextProjectPath);
            }

            // Duplicate
            if (ImGui::MenuItem("++  Duplicate", nullptr, false, ctxExists)) {
                std::string newPath = DuplicateProject(m_HubContextProjectPath);
                if (!newPath.empty()) {
                    m_EditorSettings.AddRecentProject(newPath);
                    m_EditorSettings.Save();
                    ENJIN_LOG_INFO(Build, "Duplicated project to: %s", newPath.c_str());
                }
            }

            ImGui::Separator();

            // Open in Explorer
            if (ImGui::MenuItem("[]  Open in Explorer", nullptr, false, ctxExists)) {
                OpenInExplorer(ctxPath.parent_path().string());
            }

            // Copy Path
            if (ImGui::MenuItem("=   Copy Path")) {
                ImGui::SetClipboardText(m_HubContextProjectPath.c_str());
            }

            ImGui::Separator();

            // Remove from List (deferred — can't modify vector while iterating)
            if (ImGui::MenuItem("x   Remove from List")) {
                m_HubPendingRemovePath = m_HubContextProjectPath;
            }

            // Delete
            if (ImGui::MenuItem("X   Delete Project...", nullptr, false, ctxExists)) {
                m_HubShowDeleteConfirm = true;
                m_HubDeleteProjectPath = m_HubContextProjectPath;
                m_HubDeleteProjectName = ctxName;
            }

            ImGui::EndPopup();
        }

        // ===== Delete confirmation modal =====
        if (m_HubShowDeleteConfirm) {
            ImGui::OpenPopup("Delete Project?");
            m_HubShowDeleteConfirm = false;
        }
        if (ImGui::BeginPopupModal("Delete Project?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Delete project '%s'?", m_HubDeleteProjectName.c_str());
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "This will permanently delete the project folder.");
            ImGui::Text("This cannot be undone.");
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                m_HubDeleteProjectPath.clear();
                m_HubDeleteProjectName.clear();
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.15f, 0.15f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.85f, 0.2f, 0.2f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.95f, 0.25f, 0.25f, 1.0f));
            if (ImGui::Button("Delete", ImVec2(120, 0))) {
                // Store path for deferred deletion — actual work happens
                // at the START of next DrawProjectHub call, after the UI
                // loop is done with the recentProjects vector.
                m_HubPendingDeletePath = m_HubDeleteProjectPath;
                m_HubDeleteProjectPath.clear();
                m_HubDeleteProjectName.clear();
                ImGui::CloseCurrentPopup();
            }
            ImGui::PopStyleColor(3);

            ImGui::EndPopup();
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

    // Invisible window over sidebar to block click-through to template grid behind.
    // NoInputs is NOT used — we want mouse clicks blocked. NoNav prevents keyboard capture.
    if (m_ShowProjectHub) {
        ImGui::SetNextWindowPos(sbMin);
        ImGui::SetNextWindowSize(ImVec2(sidebarW, area.y - contentY));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
        ImGui::Begin("##SidebarBlocker", nullptr,
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNav |
            ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoFocusOnAppearing);
        ImGui::End();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();
    }

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
    bool projectOpened = false; // Flag to stop drawing after a project is opened
    auto drawProjectGroup = [&](const char* groupLabel, ImU32 labelCol,
                                const std::vector<int>& indices, bool canOpen) {
        if (indices.empty() || projectOpened) return;
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
            // Bounds check — vector may have been modified by deferred operations
            if (i < 0 || i >= static_cast<int>(m_EditorSettings.recentProjects.size())) continue;
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

            // Click to open — defer to next frame to avoid vector invalidation
            if (hovered && canOpen && ImGui::IsMouseClicked(0) && !projectOpened) {
                m_HubPendingOpenPath = m_EditorSettings.recentProjects[i];
                projectOpened = true;
                return; // Exit the lambda
            }

            // Right-click context menu
            if (hovered && ImGui::IsMouseClicked(1) && i < static_cast<int>(m_EditorSettings.recentProjects.size())) {
                m_HubContextProjectPath = m_EditorSettings.recentProjects[i];
                m_HubOpenContextMenu = true;
            }

            // Tooltip: full path on hover
            if (hovered && i < static_cast<int>(m_EditorSettings.recentProjects.size())) {
                ImGui::SetTooltip("%s", m_EditorSettings.recentProjects[i].c_str());
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
    // Brand mark uses the heading typeface (Playfair Display) for a classier look.
    ImFont* brandFont = (m_ImGuiLayer && m_ImGuiLayer->GetHeadingFont())
        ? m_ImGuiLayer->GetHeadingFont() : font;

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
    ImVec2 brandSz = brandFont->CalcTextSizeA(brandFontSize, FLT_MAX, 0.0f, brandText);
    f32 brandY = (headerH - brandSz.y) * 0.5f;
    dl->AddText(brandFont, brandFontSize, ImVec2(24.0f, brandY), IM_COL32(199, 218, 196, 255), brandText);

    f32 subFontSize = 24.0f;
    const char* subText = "Game Engine";
    ImVec2 subSz = brandFont->CalcTextSizeA(subFontSize, FLT_MAX, 0.0f, subText);
    dl->AddText(brandFont, subFontSize,
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

        // Card accents come from the active THEME (CheckMark = accent), not
        // hardcoded blues — the hub is the first screen and must match the
        // editor's identity
        const ImVec4 hubAccent = ImGui::GetStyleColorVec4(ImGuiCol_CheckMark);
        const ImU32 hubAccentStrong = ImGui::GetColorU32(ImVec4(hubAccent.x, hubAccent.y, hubAccent.z, 0.85f));
        const ImU32 hubAccentSoft   = ImGui::GetColorU32(ImVec4(hubAccent.x, hubAccent.y, hubAccent.z, 0.55f));

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
            ImU32 dashCol = phHovered ? hubAccentSoft : IM_COL32(60, 65, 85, 150);
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
                phHovered ? hubAccentStrong : IM_COL32(80, 90, 115, 180), plusText);

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
                bool anyHovered = (io.MousePos.x >= cMin.x && io.MousePos.x <= cMax.x &&
                                   io.MousePos.y >= cMin.y && io.MousePos.y <= cMax.y);
                bool hovered = anyHovered && !missing;

                // Soft drop shadow lifts the card off the page
                cdl->AddRectFilled(ImVec2(cMin.x + 1.0f, cMin.y + 4.0f),
                                   ImVec2(cMax.x + 1.0f, cMax.y + 5.0f),
                                   IM_COL32(0, 0, 0, hovered ? 110 : 70), 9.0f);

                // Card background
                ImU32 cardBg = hovered ? IM_COL32(32, 38, 55, 255) : IM_COL32(22, 25, 35, 255);
                if (missing) cardBg = IM_COL32(20, 20, 26, 200);
                cdl->AddRectFilled(cMin, cMax, cardBg, 8.0f);

                // Border: theme accent on hover
                if (hovered)
                    cdl->AddRect(cMin, cMax, hubAccentStrong, 8.0f, 0, 2.0f);
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

                // Right-click context menu (works on both ready and missing cards)
                if (anyHovered && ImGui::IsMouseClicked(1)) {
                    m_HubContextProjectPath = proj->fullPath;
                    m_HubOpenContextMenu = true;
                }

                // Tooltip: full path on hover
                if (anyHovered) {
                    ImGui::SetTooltip("%s", proj->fullPath.c_str());
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
    f32 formW = (std::min)(800.0f, contentW - 60.0f);
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
    ImGui::PushItemWidth(formW - 120.0f);
    ImGui::InputText("##ProjPath", m_NewProjectPath, sizeof(m_NewProjectPath));
    ImGui::PopItemWidth();
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.2f, 0.26f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.28f, 0.35f, 1.0f));
    if (ImGui::Button("Browse...", ImVec2(100, 0))) {
        // A missing dialog helper and a cancelled dialog both come back empty,
        // so without this check the button just looks broken.
        if (!FileDialog::IsAvailable()) {
            ShowNotification("No file dialog available — install zenity or kdialog, "
                             "or type the path into the field",
                             NotificationType::Warning);
        }
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

    struct HubTemplateInfo {
        const char* id;
        const char* name;
        const char* description;
        ImVec4 accentColor;
        u32 categoryFlags;
        Editor::MaturityTier maturity;
    };

    static const HubTemplateInfo s_BuiltinTemplates[] = {
        // -- Flagship (a complete, winnable game in one scene) --
        { "coinrush",     "Coin Rush",          "3D collectathon\nCollect coins, dodge spikes, reach the portal", ImVec4(1.0f, 0.8f, 0.2f, 1.0f), kTMPL_3D, Editor::MaturityTier::Stable },
        // -- Foundations --
        { "blank",        "Blank",              "Empty scene\nStart from scratch",                       ImVec4(0.5f, 0.5f, 0.5f, 1.0f), kTMPL_ALL, Editor::MaturityTier::Stable },
        // -- The three authoring tiers: same tiny game, three ways to build it --
        { "componentsonly", "Components Only",  "No code at all\nA winnable game from Inspector components alone", ImVec4(0.35f, 0.75f, 0.55f, 1.0f), kTMPL_3D, Editor::MaturityTier::Stable },
        { "scriptonly",     "Script Only",      "One AngelScript file\nAll game logic lives in scripts/GameScript.as", ImVec4(0.85f, 0.65f, 0.25f, 1.0f), kTMPL_3D, Editor::MaturityTier::Stable },
        { "stresstest",   "Stress Test",        "Perf benchmark\n64 falling bodies + 16 point lights",   ImVec4(0.9f, 0.45f, 0.2f, 1.0f), kTMPL_3D, Editor::MaturityTier::Stable },
        { "platformer",   "2D Platformer",      "4-zone adventure\nMeadow + cave + tower + sky boss",    ImVec4(0.3f, 0.8f, 0.3f, 1.0f), kTMPL_2D, Editor::MaturityTier::Stable },
        { "thirdperson",  "3D Third Person",    "Over-the-shoulder\nShadows + obstacles + point light",  ImVec4(0.8f, 0.3f, 0.3f, 1.0f), kTMPL_3D, Editor::MaturityTier::Stable },
        // -- Genre showcases --
        { "narrative",    "Dialogue & Narrative","NPC conversations\nQuests + dialogue box + branching",  ImVec4(0.7f, 0.6f, 0.85f, 1.0f), kTMPL_3D, Editor::MaturityTier::Beta },
        { "pointclick",   "Point & Click",      "Adventure game\nClick hotspots + inventory + dialogue", ImVec4(1.0f, 0.55f, 0.2f, 1.0f), kTMPL_2D, Editor::MaturityTier::Beta },
        { "idleclicker",  "Idle/Clicker",       "Incremental game\nUI canvas + meta saves + tweens",    ImVec4(0.4f, 0.8f, 0.4f, 1.0f), kTMPL_2D, Editor::MaturityTier::Beta },
        { "planetgravity","Planet Gravity",     "Spherical gravity\nWalk on a planet surface",           ImVec4(0.3f, 0.6f, 0.95f, 1.0f), kTMPL_3D, Editor::MaturityTier::Beta },
        { "isometric",   "3D Isometric",    "45-degree CRPG\nPerspective + player",               ImVec4(0.9f, 0.6f, 0.2f, 1.0f), kTMPL_3D, Editor::MaturityTier::Beta },
        { "flower",      "Flower Garden", "Procedural flower\nPluckable petals + score",             ImVec4(0.9f, 0.4f, 0.6f, 1.0f), kTMPL_3D, Editor::MaturityTier::Stable },
        // -- Accessibility showcase --
        { "accessibility","Accessibility Demo","Live settings demo\nColorblind + font + screen reader",    ImVec4(0.3f, 0.75f, 0.9f, 1.0f), kTMPL_ALL, Editor::MaturityTier::Beta },
        // -- The website's browser demo: third person + live accessibility --
        { "webdemo",      "Web Demo",           "Site browser demo\nThird person + live accessibility menu", ImVec4(0.25f, 0.85f, 0.75f, 1.0f), kTMPL_3D, Editor::MaturityTier::Stable },
    };
    // Cut firstperson and teamsports: both are the 3D character template with a
    // different camera or ruleset, and planetgravity and isometric already cover
    // that ground more distinctly.
    constexpr int s_BuiltinCount = 15;
} // anonymous namespace

// Draw a procedural mini-preview for template cards (first 4 templates get custom art)
// Pick a symbol character for template cards based on template ID
static const char* GetTemplateSymbol(const char* templateId) {
    if (!templateId) return "?";
    std::string id(templateId);
    // 3D templates
    if (id == "blank")         return "[ ]";
    if (id == "coinrush")      return "$";
    if (id == "componentsonly") return "ECS";
    if (id == "scriptonly")    return "AS";
    if (id == "thirdperson")   return "III";
    if (id == "firstperson")   return "FPS";
    if (id == "narrative")     return "...";
    if (id == "accessibility") return "A11";
    if (id == "webdemo")       return "WWW";
    if (id == "planetgravity") return "@";
    if (id == "isometric")     return "ISO";
    if (id == "teamsports")    return "VS";
    if (id == "flower")        return "*";
    if (id == "stresstest")    return "STR";
    // 2D templates
    if (id == "platformer")    return "2D>";
    if (id == "pointclick")    return "PTR";
    if (id == "idleclicker")   return "$$$";
    if (id == "custom")        return "USR";
    return "?";
}

static void DrawTemplateThumbnail(ImDrawList* dl, const char* templateId, ImVec2 tMin, ImVec2 tMax,
                                   const ImVec4& accent, bool hovered) {
    u8 r = static_cast<u8>(accent.x * 255);
    u8 g = static_cast<u8>(accent.y * 255);
    u8 b = static_cast<u8>(accent.z * 255);

    // Gradient fill: lighter at top, darker at bottom for depth
    u8 aTop    = hovered ? 240 : 200;
    u8 aBottom = hovered ? 180 : 140;
    // Top color (slightly brighter)
    u8 rTop = static_cast<u8>((std::min)(255, static_cast<int>(r) + 30));
    u8 gTop = static_cast<u8>((std::min)(255, static_cast<int>(g) + 30));
    u8 bTop = static_cast<u8>((std::min)(255, static_cast<int>(b) + 30));
    // Bottom color (darker)
    u8 rBot = static_cast<u8>(static_cast<int>(r) * 7 / 10);
    u8 gBot = static_cast<u8>(static_cast<int>(g) * 7 / 10);
    u8 bBot = static_cast<u8>(static_cast<int>(b) * 7 / 10);

    dl->AddRectFilledMultiColor(tMin, tMax,
        IM_COL32(rTop, gTop, bTop, aTop), IM_COL32(rTop, gTop, bTop, aTop),
        IM_COL32(rBot, gBot, bBot, aBottom), IM_COL32(rBot, gBot, bBot, aBottom));

    // Dark bottom shadow edge for depth
    f32 shadowH = 16.0f;
    dl->AddRectFilledMultiColor(
        ImVec2(tMin.x, tMax.y - shadowH), tMax,
        IM_COL32(0, 0, 0, 0), IM_COL32(0, 0, 0, 0),
        IM_COL32(10, 12, 20, 180), IM_COL32(10, 12, 20, 180));

    // Subtle top highlight edge
    dl->AddRectFilledMultiColor(
        tMin, ImVec2(tMax.x, tMin.y + 6.0f),
        IM_COL32(255, 255, 255, 30), IM_COL32(255, 255, 255, 30),
        IM_COL32(255, 255, 255, 0), IM_COL32(255, 255, 255, 0));

    // Center symbol/icon text
    const char* symbol = GetTemplateSymbol(templateId);
    ImFont* font = ImGui::GetFont();
    f32 symbolFontSize = 38.0f;
    ImVec2 symSize = font->CalcTextSizeA(symbolFontSize, FLT_MAX, 0.0f, symbol);
    f32 cx = tMin.x + (tMax.x - tMin.x - symSize.x) * 0.5f;
    f32 cy = tMin.y + (tMax.y - tMin.y - symSize.y) * 0.5f;

    // Shadow behind symbol
    dl->AddText(font, symbolFontSize, ImVec2(cx + 2.0f, cy + 2.0f),
        IM_COL32(0, 0, 0, hovered ? 120 : 80), symbol);
    // Symbol itself (white, slightly transparent)
    dl->AddText(font, symbolFontSize, ImVec2(cx, cy),
        IM_COL32(255, 255, 255, hovered ? 220 : 160), symbol);
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
    const char* filterLabels[] = { "All", "2D", "3D" };
    u32 filterValues[] = { TMPL_ALL, TMPL_2D, TMPL_3D };

    f32 chipFontSize = 26.0f;
    f32 chipPad = 12.0f;
    f32 chipH = 56.0f;
    f32 chipPadX = 28.0f;
    ImVec2 chipTextSizes[3];
    f32 totalChipW = 0.0f;
    for (int f = 0; f < 3; ++f) {
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
        for (int f = 0; f < 3; ++f) {
            chipTextSizes[f] = font->CalcTextSizeA(chipFontSize, FLT_MAX, 0.0f, filterLabels[f]);
            totalChipW += chipTextSizes[f].x + chipPadX * 2.0f + chipPad;
        }
        totalChipW -= chipPad;
    }

    f32 chipX = sidebarW + (contentW - totalChipW) * 0.5f;
    for (int f = 0; f < 3; ++f) {
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
    const char* statusLabels[] = { "All Status", "Stable", "Beta" };
    i32 statusValues[] = { -1, 0, 1 };
    ImU32 statusColors[] = {
        IM_COL32(160, 165, 185, 200),  // All — neutral
        IM_COL32(80, 140, 220, 255),   // Stable — blue
        IM_COL32(80, 180, 80, 255),    // Beta — green
    };

    f32 sChipFontSize = 18.0f;
    f32 sChipPad = 10.0f;
    f32 sChipH = 36.0f;
    f32 sChipPadX = 20.0f;
    ImVec2 sChipTextSizes[3];
    f32 sTotalChipW = 0.0f;
    for (int f = 0; f < 3; ++f) {
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
        for (int f = 0; f < 3; ++f) {
            sChipTextSizes[f] = font->CalcTextSizeA(sChipFontSize, FLT_MAX, 0.0f, statusLabels[f]);
            sTotalChipW += sChipTextSizes[f].x + sChipPadX * 2.0f + sChipPad;
        }
        sTotalChipW -= sChipPad;
    }

    f32 sChipX = sidebarW + (contentW - sTotalChipW) * 0.5f;
    for (int f = 0; f < 3; ++f) {
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

        // Card background with gradient and hover/selection glow
        ImVec4 accent = s_BuiltinTemplates[i].accentColor;
        if (isStable) {
            // Card body: subtle vertical gradient (slightly lighter top to darker bottom)
            ImU32 bgTop = selected ? IM_COL32(40, 50, 78, 255) :
                          (hovered ? IM_COL32(45, 50, 68, 255) : IM_COL32(30, 33, 42, 255));
            ImU32 bgBot = selected ? IM_COL32(28, 35, 55, 255) :
                          (hovered ? IM_COL32(32, 36, 48, 255) : IM_COL32(20, 22, 28, 255));
            gridDl->AddRectFilledMultiColor(cardPos, cardEnd, bgTop, bgTop, bgBot, bgBot);
            // Re-round the corners (AddRectFilledMultiColor doesn't support rounding)
            // Draw rounded rect on top to clip corners
            gridDl->AddRectFilled(cardPos, cardEnd, IM_COL32(0, 0, 0, 0), 8.0f);

            // Hover glow: accent-tinted border glow effect
            if (hovered) {
                u8 gr = static_cast<u8>(accent.x * 255);
                u8 gg = static_cast<u8>(accent.y * 255);
                u8 gb = static_cast<u8>(accent.z * 255);
                gridDl->AddRect(ImVec2(cardPos.x - 1, cardPos.y - 1),
                    ImVec2(cardEnd.x + 1, cardEnd.y + 1),
                    IM_COL32(gr, gg, gb, 60), 10.0f, 0, 3.0f);
            }
        } else {
            gridDl->AddRectFilled(cardPos, cardEnd, IM_COL32(20, 22, 28, 255), 8.0f);
        }

        if (isStable) {
            ImU32 accentCol = IM_COL32(
                (int)(accent.x * 255), (int)(accent.y * 255),
                (int)(accent.z * 255), (hovered || selected) ? 255 : 180);
            // Procedural thumbnail (top 200px) with gradient + symbol
            ImVec2 thumbMin(cardPos.x + 1, cardPos.y + 1);
            ImVec2 thumbMax(cardEnd.x - 1, cardPos.y + 200.0f);
            DrawTemplateThumbnail(gridDl, s_BuiltinTemplates[i].id, thumbMin, thumbMax, accent, hovered);
            // Accent bar along top
            gridDl->AddRectFilled(cardPos, ImVec2(cardEnd.x, cardPos.y + 3.0f), accentCol, 8.0f, ImDrawFlags_RoundCornersTop);

            ImU32 borderCol = selected ? IM_COL32(140, 165, 230, 255) :
                             (hovered  ? accentCol : IM_COL32(55, 60, 75, 130));
            gridDl->AddRect(cardPos, cardEnd, borderCol, 8.0f, 0, selected ? 2.5f : (hovered ? 2.0f : 1.0f));
        } else {
            ImU32 mutedAccent = IM_COL32(
                (int)(accent.x * 80), (int)(accent.y * 80),
                (int)(accent.z * 80), 100);
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

        // Maturity tier badge (top-left corner) — polished pill shape
        {
            const char* tierLabel = Editor::TemplateMarketplace::GetMaturityName(s_BuiltinTemplates[i].maturity);
            ImU32 tierCol, tierBorderCol;
            switch (s_BuiltinTemplates[i].maturity) {
                case Editor::MaturityTier::Stable:
                    tierCol = IM_COL32(60, 120, 200, 220);
                    tierBorderCol = IM_COL32(100, 160, 240, 180);
                    break;
                case Editor::MaturityTier::Beta:
                    tierCol = IM_COL32(60, 160, 60, 220);
                    tierBorderCol = IM_COL32(100, 200, 100, 180);
                    break;
                case Editor::MaturityTier::Preview:
                    tierCol = IM_COL32(190, 150, 40, 220);
                    tierBorderCol = IM_COL32(230, 190, 70, 180);
                    break;
                case Editor::MaturityTier::Experimental:
                    tierCol = IM_COL32(190, 55, 55, 220);
                    tierBorderCol = IM_COL32(230, 90, 90, 180);
                    break;
                default:
                    tierCol = IM_COL32(100, 100, 100, 220);
                    tierBorderCol = IM_COL32(140, 140, 140, 180);
                    break;
            }
            if (!isStable) {
                tierCol = (tierCol & 0x00FFFFFF) | (100 << 24);
                tierBorderCol = (tierBorderCol & 0x00FFFFFF) | (60 << 24);
            }
            ImVec2 tierSize = ImGui::CalcTextSize(tierLabel);
            f32 pillPadX = 10.0f;
            f32 pillPadY = 4.0f;
            f32 pillH = tierSize.y + pillPadY * 2.0f;
            f32 pillW = tierSize.x + pillPadX * 2.0f;
            ImVec2 pillMin(cardPos.x + 8.0f, cardPos.y + 10.0f);
            ImVec2 pillMax(pillMin.x + pillW, pillMin.y + pillH);
            f32 pillRound = pillH * 0.5f;  // full round = pill shape
            gridDl->AddRectFilled(pillMin, pillMax, tierCol, pillRound);
            gridDl->AddRect(pillMin, pillMax, tierBorderCol, pillRound, 0, 1.0f);
            ImVec2 tierPos(pillMin.x + pillPadX, pillMin.y + pillPadY);
            gridDl->AddText(tierPos, isStable ? IM_COL32(255, 255, 255, 245) : IM_COL32(180, 180, 180, 140), tierLabel);
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

        // Card body gradient
        ImU32 bgTop = selected ? IM_COL32(35, 55, 65, 255) :
                      (hovered ? IM_COL32(42, 48, 65, 255) : IM_COL32(30, 33, 42, 255));
        ImU32 bgBot = selected ? IM_COL32(25, 38, 48, 255) :
                      (hovered ? IM_COL32(30, 34, 48, 255) : IM_COL32(20, 22, 28, 255));
        gridDl->AddRectFilledMultiColor(cardPos, cardEnd, bgTop, bgTop, bgBot, bgBot);
        gridDl->AddRectFilled(cardPos, cardEnd, IM_COL32(0, 0, 0, 0), 8.0f);

        // Hover glow
        if (hovered) {
            gridDl->AddRect(ImVec2(cardPos.x - 1, cardPos.y - 1),
                ImVec2(cardEnd.x + 1, cardEnd.y + 1),
                IM_COL32(0, 200, 180, 60), 10.0f, 0, 3.0f);
        }

        ImU32 accentCol = (hovered || selected) ? IM_COL32(0, 200, 180, 255) : IM_COL32(0, 200, 180, 150);
        // Custom template thumbnail with gradient + symbol
        ImVec2 cThumbMin(cardPos.x + 1, cardPos.y + 1);
        ImVec2 cThumbMax(cardEnd.x - 1, cardPos.y + 200.0f);
        ImVec4 cAccent(0.4f, 0.6f, 0.8f, 1.0f);
        DrawTemplateThumbnail(gridDl, "custom", cThumbMin, cThumbMax, cAccent, hovered);
        gridDl->AddRectFilled(cardPos, ImVec2(cardEnd.x, cardPos.y + 3.0f), accentCol, 8.0f, ImDrawFlags_RoundCornersTop);

        ImU32 borderCol = selected ? IM_COL32(0, 220, 200, 255) :
                         (hovered  ? accentCol : IM_COL32(55, 60, 75, 130));
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
        { "Coin Rush",       "Collect coins, dodge spikes,\nand reach the portal.",                   "demos/coinrush_demo.enjin",     ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "coinrush" },
        { "Dialogue & Narrative", "NPC conversations with\nquests and branching dialogue.",          "demos/narrative_demo.enjin",    ImVec4(0.7f, 0.6f, 0.85f, 1.0f), "narrative" },
        { "Point & Click",   "Adventure game with hotspots,\ninventory, and dialogue.",              "demos/pointclick_demo.enjin",   ImVec4(1.0f, 0.55f, 0.2f, 1.0f), "pointclick" },
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
void EditorLayer::TickTemplateExport() {
    if (s_ExportTemplateIndex < 0 || s_ExportTemplatesDir.empty()) return;

    // CreateProjectOnDisk defers the template build and the scene save to the
    // next Update, so only start the next one once the previous has landed.
    if (!m_PendingTemplateId.empty() || !m_PendingSceneLoadPath.empty()) return;

    if (s_ExportTemplateIndex >= s_BuiltinCount) {
        ENJIN_LOG_INFO(Editor, "Exported %d templates to %s",
                       s_BuiltinCount, s_ExportTemplatesDir.c_str());
        s_ExportTemplateIndex = -1;
        if (m_Window) m_Window->Close();
        return;
    }

    const char* id = s_BuiltinTemplates[s_ExportTemplateIndex].id;
    ++s_ExportTemplateIndex;
    ENJIN_LOG_INFO(Editor, "Exporting template %d/%d: %s",
                   s_ExportTemplateIndex, s_BuiltinCount, id);
    if (!CreateProjectOnDisk(s_ExportTemplatesDir, id, "Main", id)) {
        ENJIN_LOG_ERROR(Editor, "Template export failed: %s", id);
    }
}

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

    // Ship the enjin_api script headers (TegeBehavior.as etc.) with the
    // project — #include resolution looks in <project>/scripts/enjin_api,
    // and the auto-injected TegeBehavior base resolves from there too.
    {
        fs::path engineApi = Scripting::ScriptEngine::FindApiDirectory("");
        if (!engineApi.empty()) {
            fs::copy(engineApi, projRoot / "scripts" / "enjin_api",
                     fs::copy_options::recursive | fs::copy_options::skip_existing, ec);
            if (ec) {
                ENJIN_LOG_WARN(Editor, "Could not copy enjin_api into project: %s", ec.message().c_str());
                ec.clear();
            }
        } else {
            ENJIN_LOG_WARN(Editor, "enjin_api directory not found — project scripts will lack API headers");
        }
    }

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
    if (templateId == "platformer") {
        m_SceneManager.SetProjectMode(Scene::ProjectMode::Mode2D);
    } else if (templateId == "blank") {
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
    ResetLayerSession();
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
    if (templateId == "platformer") {
        m_Layout.leftWidth = 0.15f;
        m_Layout.rightWidth = 0.20f;
        m_Layout.bottomHeight = 0.18f;
        m_Layout.gameViewW = 700.0f;
        m_Layout.gameViewH = 450.0f;
        m_ShowStatsOverlay = false;
    }
    else if (templateId == "isometric") {
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
    else if (templateId == "thirdperson" || templateId == "teamsports" || templateId == "coinrush" ||
             templateId == "webdemo") {
        m_Layout.leftWidth = 0.16f;
        m_Layout.rightWidth = 0.23f;
        m_Layout.inspectorSplit = 0.65f;
        m_Layout.gameViewW = 680.0f;
        m_Layout.gameViewH = 440.0f;
    }
    else if (templateId == "firstperson" || templateId == "componentsonly") {
        m_Layout.leftWidth = 0.13f;
        m_Layout.rightWidth = 0.20f;
        m_Layout.bottomHeight = 0.18f;
        m_Layout.gameViewW = 800.0f;
        m_Layout.gameViewH = 500.0f;
    }
    else if (templateId == "scriptonly") {
        // Script tier: give the console room — script output and compile
        // errors are the feedback loop here
        m_Layout.leftWidth = 0.13f;
        m_Layout.rightWidth = 0.20f;
        m_Layout.bottomHeight = 0.26f;
        m_Layout.gameViewW = 760.0f;
        m_Layout.gameViewH = 470.0f;
    }
    else if (templateId == "narrative") {
        m_Layout.leftWidth = 0.14f;
        m_Layout.rightWidth = 0.24f;
        m_Layout.bottomHeight = 0.15f;
        m_Layout.inspectorSplit = 0.7f;
        m_Layout.gameViewW = 750.0f;
        m_Layout.gameViewH = 480.0f;
    }
    else if (templateId == "flower") {
        m_Layout.leftWidth = 0.14f;
        m_Layout.rightWidth = 0.23f;
        m_Layout.inspectorSplit = 0.65f;
        m_Layout.gameViewW = 720.0f;
        m_Layout.gameViewH = 480.0f;
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
    if (templateId == "platformer" || templateId == "pointclick" || templateId == "idleclicker") {
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
        m_World->AddComponent<ECS::NameComponent>(light, "Global Directional Light");
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
        // 2D Platformer — Starter Template
        // Minimal: player, ground, platforms, 1 coin, 1 enemy
        // ═══════════════════════════════════════════════════

        // --- Player ---
        ECS::Entity player = createPlayer2D("Player");
        {
            auto& ctrl = m_World->AddComponent<ECS::Platformer2DController>(player);
            ctrl.moveSpeed = 5.0f;
            ctrl.jumpForce = 10.0f;
            ctrl.maxJumps = 2;
            ctrl.coyoteTime = 0.15f;
            ctrl.collisionRadius = 0.4f;
            ctrl.collisionHeight = 1.6f;
            auto& hp = m_World->AddComponent<ECS::HealthComponent>(player);
            hp.maxHealth = 100.0f;
            hp.currentHealth = 100.0f;
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

        // --- Ground ---
        {
            ECS::Entity ground = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(ground, "Ground");
            auto& gt = m_World->AddComponent<ECS::TransformComponent>(ground);
            gt.position = Math::Vector3(0.0f, -1.0f, 0.0f);
            gt.scale = Math::Vector3(30.0f, 1.0f, 1.0f);
            auto& gmat = m_World->AddComponent<ECS::MaterialComponent>(ground);
            gmat.baseColor = Math::Vector3(0.35f, 0.55f, 0.3f);
            m_World->AddComponent<ECS::MeshComponent>(ground, Renderer::MeshFactory::CreateQuad(1.0f, 1.0f));
            addBoxCollider2D(ground, 15.0f, 0.5f);
        }

        // --- Platforms ---
        {
            const Math::Vector3 platPos[] = {{-4.0f, 1.5f, 0.0f}, {1.0f, 3.5f, 0.0f}, {6.0f, 5.5f, 0.0f}};
            const Math::Vector2 platSize[] = {{2.5f, 0.15f}, {3.0f, 0.15f}, {2.0f, 0.15f}};
            for (int i = 0; i < 3; ++i) {
                ECS::Entity plat = m_World->CreateEntity();
                m_World->AddComponent<ECS::NameComponent>(plat, "Platform " + std::to_string(i + 1));
                auto& pt = m_World->AddComponent<ECS::TransformComponent>(plat);
                pt.position = platPos[i];
                pt.scale = Math::Vector3(platSize[i].x * 2.0f, platSize[i].y * 2.0f, 1.0f);
                auto& pmat = m_World->AddComponent<ECS::MaterialComponent>(plat);
                pmat.baseColor = Math::Vector3(0.45f, 0.35f, 0.25f);
                m_World->AddComponent<ECS::MeshComponent>(plat, Renderer::MeshFactory::CreateQuad(1.0f, 1.0f));
                addBoxCollider2D(plat, platSize[i].x, platSize[i].y);
            }
        }

        // --- Coin ---
        {
            ECS::Entity coin = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(coin, "Coin");
            auto& ct = m_World->AddComponent<ECS::TransformComponent>(coin);
            ct.position = Math::Vector3(1.0f, 4.5f, 0.0f);
            ct.scale = Math::Vector3(0.5f, 0.5f, 1.0f);
            auto& cmat = m_World->AddComponent<ECS::MaterialComponent>(coin);
            cmat.baseColor = Math::Vector3(1.0f, 0.85f, 0.2f);
            cmat.emissiveColor = Math::Vector3(1.0f, 0.85f, 0.2f);
            cmat.emissiveStrength = 0.3f;
            m_World->AddComponent<ECS::MeshComponent>(coin, Renderer::MeshFactory::CreateQuad(1.0f, 1.0f));
            auto& pick = m_World->AddComponent<ECS::PickupComponent>(coin);
            pick.type = ECS::PickupComponent::PickupType::Coin;
            pick.value = 1.0f;
            auto& col = m_World->AddComponent<Physics::Body2DComponent>(coin);
            col.shapeType = Physics::Shape2DType::Box;
            col.box.halfExtents = Math::Vector2(0.25f, 0.25f);
            col.isKinematic = true;
            col.isSensor = true;
            col.gravityScale = 0.0f;
            auto& tw = m_World->AddComponent<ECS::TweenComponent>(coin);
            tw.autoPlay = true;
            ECS::TweenEntry bob;
            bob.property = ECS::TweenProperty::Position;
            bob.easing = ECS::EasingType::EaseInOutSine;
            bob.mode = ECS::TweenMode::PingPong;
            bob.startValue = Math::Vector3(1.0f, 4.5f, 0.0f);
            bob.endValue = Math::Vector3(1.0f, 4.9f, 0.0f);
            bob.duration = 1.0f;
            bob.useCurrentAsStart = false;
            tw.tweens.push_back(bob);
        }

        // --- Enemy (patrol) ---
        {
            ECS::Entity enemy = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(enemy, "Enemy");
            auto& et = m_World->AddComponent<ECS::TransformComponent>(enemy);
            et.position = Math::Vector3(5.0f, 0.2f, 0.0f);
            et.scale = Math::Vector3(0.8f, 0.8f, 1.0f);
            auto& emat = m_World->AddComponent<ECS::MaterialComponent>(enemy);
            emat.baseColor = Math::Vector3(0.8f, 0.2f, 0.2f);
            m_World->AddComponent<ECS::MeshComponent>(enemy, Renderer::MeshFactory::CreateQuad(1.0f, 1.0f));
            auto& dmg = m_World->AddComponent<ECS::DamageComponent>(enemy);
            dmg.damage = 10.0f;
            dmg.damageOnce = true;
            auto& ai = m_World->AddComponent<ECS::AIControllerComponent>(enemy);
            ai.patrolPoints = {Math::Vector3(3.0f, 0.2f, 0.0f), Math::Vector3(8.0f, 0.2f, 0.0f)};
            ai.moveSpeed = 2.0f;
            ai.is2D = true;
            m_World->AddComponent<ECS::TagComponent>(enemy).tags.push_back("enemy");
            auto& col = m_World->AddComponent<Physics::Body2DComponent>(enemy);
            col.shapeType = Physics::Shape2DType::Box;
            col.box.halfExtents = Math::Vector2(0.4f, 0.4f);
            col.isKinematic = true;
            col.isSensor = true;
            col.gravityScale = 0.0f;
        }

        // --- Directional Light ---
        {
            ECS::Entity sun = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(sun, "Global Directional Light");
            auto& lt = m_World->AddComponent<ECS::TransformComponent>(sun);
            lt.position = Math::Vector3(0.0f, 10.0f, 5.0f);
            lt.rotation = Math::Quaternion(Math::Vector3(1, 0, 0), Math::Radians(-45.0f));
            auto& lc = m_World->AddComponent<ECS::LightComponent>(sun);
            lc.type = ECS::LightType::Directional;
            lc.intensity = 1.0f;
            lc.castShadows = false;
        }

        // --- Guide ---
        {
            ECS::Entity guide = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(guide, "-- Guide --");
            m_World->AddComponent<ECS::TransformComponent>(guide);
            m_World->AddComponent<ECS::NotesComponent>(guide).notes =
                "2D PLATFORMER STARTER\n"
                "======================\n\n"
                "Entities:\n"
                "  Player - Capsule with Platformer2DController (WASD + Space)\n"
                "  Ground - Static platform spanning the scene\n"
                "  Platforms - 3 stepping stones at increasing heights\n"
                "  Coin - PickupComponent + TweenComponent bob animation\n"
                "  Enemy - AIControllerComponent with patrol + DamageComponent\n\n"
                "Key systems:\n"
                "  - Platformer2DController handles jump, double-jump, coyote time\n"
                "  - Box2D physics for ground/platform collision\n"
                "  - PickupComponent collected via AABB overlap\n"
                "  - AIControllerComponent patrols between 2 points\n"
                "  - Camera2DBoundsComponent follows player with smoothing\n\n"
                "Try:\n"
                "  - Add more platforms to extend the level\n"
                "  - Duplicate coins and place them on platforms\n"
                "  - Add a moving platform with TweenComponent\n"
                "  - Enable wall jumping: Platformer2DController.enableWallJump";
        }

        // --- Render settings ---
        m_RenderSystem->SetShadowsEnabled(false);
        m_RenderSystem->SetAmbientIntensity(0.15f);
        if (m_PostProcessing) {
            auto& pp = m_PostProcessing->GetSettings();
            pp.fxaaEnabled = 1;
        }

    // ═══════════════════════════════════════════════════
    // PLATFORMER DEMO — Full 4-Zone Adventure (was the old starter)
    // Preserved behind "platformer_demo" templateId for the Demos tab.
    // ═══════════════════════════════════════════════════
    }
    else if (templateId == "thirdperson" || templateId == "webdemo") {
        // "webdemo" = this same third-person scene + the live accessibility
        // menu — the scene exported as the website's WebGPU browser demo.
        createGround();
        ECS::Entity player = createPlayer3D("Player");
        // Player needs HealthComponent for pickup/damage interactions
        auto& health = m_World->AddComponent<ECS::HealthComponent>(player);
        health.maxHealth = 100.0f;
        health.currentHealth = 100.0f;
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

        // Global directional light (neutral base illumination; not "sun")
        {
            ECS::Entity dir = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(dir, "Global Directional Light");
            auto& st = m_World->AddComponent<ECS::TransformComponent>(dir);
            st.position = Math::Vector3(0.0f, 10.0f, 0.0f);
            st.rotation = Math::Quaternion(Math::Vector3(1, 0, 0), Math::Radians(-45.0f));
            auto& slc = m_World->AddComponent<ECS::LightComponent>(dir);
            slc.type = ECS::LightType::Directional;
            slc.color = Math::Vector3(0.85f, 0.87f, 0.92f);  // neutral, slightly cool
            slc.intensity = 0.1f;                            // very dim so the colored lights dominate
            slc.castShadows = true;
        }

        // Two colored lights crossed on the player at ~(0,1.3,0): a deep-blue spot
        // from the front-right and a magenta-pink point from the back-left.
        const Math::Vector3 lookTarget(0.0f, 1.3f, 0.0f);

        // Deep-blue spot: brighter, closer, and intentionally NOT a mirror of the
        // magenta (different distance/height/angle) for an asymmetric cross.
        {
            ECS::Entity spot = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(spot, "Blue Spot");
            auto& t = m_World->AddComponent<ECS::TransformComponent>(spot);
            t.position = Math::Vector3(3.0f, 3.2f, 2.2f);  // closer to the character
            t.rotation = Math::Quaternion::FromToRotation(
                Math::Vector3(0.0f, 0.0f, -1.0f), (lookTarget - t.position).Normalized());
            auto& lc = m_World->AddComponent<ECS::LightComponent>(spot);
            lc.type = ECS::LightType::Spot;
            lc.color = Math::Vector3(0.0f, 0.05f, 1.0f);   // deeper, more saturated blue
            lc.intensity = 13.0f;                          // brighter
            lc.range = 20.0f;
            lc.innerConeAngle = 14.0f;
            lc.outerConeAngle = 26.0f;                     // tighter, punchier beam up close
            lc.castShadows = true;
        }

        // Magenta-pink point, farther and lower than the blue spot so the two
        // lights cross on the character without mirroring each other.
        {
            ECS::Entity mp = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(mp, "Magenta Point");
            auto& t = m_World->AddComponent<ECS::TransformComponent>(mp);
            t.position = Math::Vector3(-4.5f, 2.2f, -3.5f);
            auto& lc = m_World->AddComponent<ECS::LightComponent>(mp);
            lc.type = ECS::LightType::Point;
            lc.color = Math::Vector3(1.0f, 0.10f, 0.60f);   // magenta pink
            lc.intensity = 4.5f;
            lc.range = 14.0f;
            // Accent light only: no shadows. A shadow-casting point light projects
            // a confusing teal square (the region missing the magenta contribution
            // reads as its complement) and point-light shadow cubemaps are costly.
            // The blue spot + global directional carry the character's shadowing.
            lc.castShadows = false;
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

        // Collectible coin with bob — needs a sphere collider + sensor rigidbody
        // so the CharacterVirtual can detect overlap via CheckHazardOverlaps.
        {
            ECS::Entity coin = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(coin, "Coin");
            auto& ct = m_World->AddComponent<ECS::TransformComponent>(coin);
            ct.position = Math::Vector3(3.0f, 1.0f, -2.0f);  // Near obstacles, at reachable height
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
            // Sphere collider defines pickup radius for AABB overlap detection.
            // No RigidbodyComponent — pickups should NOT be solid physics bodies.
            // The GameplayLoop::CheckPickupOverlaps3D handles collection via distance check.
            auto& scol = m_World->AddComponent<ECS::SphereColliderComponent>(coin);
            scol.radius = 0.5f;
            auto& tw = m_World->AddComponent<ECS::TweenComponent>(coin);
            tw.autoPlay = true;
            ECS::TweenEntry bob;
            bob.property = ECS::TweenProperty::Position;
            bob.easing = ECS::EasingType::EaseInOutSine;
            bob.mode = ECS::TweenMode::PingPong;
            bob.startValue = Math::Vector3(3.0f, 1.0f, -2.0f);
            bob.endValue = Math::Vector3(3.0f, 1.5f, -2.0f);
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

        // Guide entity
        {
            ECS::Entity guide = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(guide, "-- Guide --");
            m_World->AddComponent<ECS::TransformComponent>(guide);
            m_World->AddComponent<ECS::NotesComponent>(guide).notes =
                "3D THIRD PERSON STARTER\n"
                "========================\n\n"
                "Entities:\n"
                "  Player - Capsule with ThirdPersonController (WASD + mouse)\n"
                "  Obstacles - Static cubes with BoxColliderComponent\n"
                "  Ramp - Rotated cube showing angled surfaces\n"
                "  Coin - PickupComponent + TweenComponent for bob\n"
                "  Sun / Point Light - Two-light setup for depth\n\n"
                "Key systems:\n"
                "  - ThirdPersonController handles camera orbit + movement\n"
                "  - Jolt physics provides ground detection + collision\n"
                "  - PickupComponent collected via distance check (no rigidbody needed)\n"
                "  - TweenComponent animates position with PingPong mode\n\n"
                "Try:\n"
                "  - Add more coins by duplicating the Coin entity\n"
                "  - Change player speed via ThirdPersonController.moveSpeed\n"
                "  - Add enemies with AIControllerComponent + DamageComponent";
        }

        if (templateId == "webdemo") {
            // Color swatch row along the back edge — makes the colorblind
            // filter changes visibly obvious while walking around
            const Math::Vector3 swatchColors[6] = {
                {0.85f, 0.10f, 0.10f}, {0.90f, 0.50f, 0.10f}, {0.90f, 0.85f, 0.10f},
                {0.10f, 0.70f, 0.20f}, {0.15f, 0.35f, 0.90f}, {0.60f, 0.20f, 0.80f} };
            const char* swatchNames[6] = { "Swatch Red", "Swatch Orange", "Swatch Yellow",
                                           "Swatch Green", "Swatch Blue", "Swatch Purple" };
            for (int i = 0; i < 6; ++i) {
                ECS::Entity cube = m_World->CreateEntity();
                m_World->AddComponent<ECS::NameComponent>(cube, swatchNames[i]);
                auto& t = m_World->AddComponent<ECS::TransformComponent>(cube);
                t.position = Math::Vector3(-4.0f + i * 1.6f, 0.55f, -7.0f);
                auto& mat = m_World->AddComponent<ECS::MaterialComponent>(cube);
                mat.baseColor = swatchColors[i];
                mat.roughness = 0.6f;
                m_World->AddComponent<ECS::MeshComponent>(cube, Renderer::MeshFactory::CreateCube(1.0f));
            }

            // The live accessibility menu (canvas + controller + script)
            CreateAccessibilityMenu();

            // Cute pulsing "Tab" pill, bottom center — authored UI, animated
            // by the demo script (breathing glow, hides while the menu is open)
            {
                ECS::Entity hintEntity = m_World->CreateEntity();
                m_World->AddComponent<ECS::NameComponent>(hintEntity, "Tab Hint");
                m_World->AddComponent<ECS::TransformComponent>(hintEntity);
                auto& canvas = m_World->AddComponent<GUI::UICanvasComponent>(hintEntity);
                canvas.canvasName = "TabHint";
                canvas.designWidth = 1920.0f;
                canvas.designHeight = 1080.0f;

                u32 pillId = canvas.AddElement(GUI::UIWidgetType::Panel, "Tab Pill");
                if (auto* pill = canvas.GetElement(pillId)) {
                    pill->anchor.anchorMin = Math::Vector2(0.5f, 1.0f);
                    pill->anchor.anchorMax = Math::Vector2(0.5f, 1.0f);
                    pill->anchor.offsetLeft = -215.0f;
                    pill->anchor.offsetRight = 215.0f;
                    pill->anchor.offsetTop = -74.0f;
                    pill->anchor.offsetBottom = -26.0f;
                    pill->style.bgColor = Math::Vector3(0.10f, 0.42f, 0.46f);
                    pill->style.bgAlpha = 0.8f;
                    pill->style.borderRadius = 24.0f;   // full pill
                    pill->style.borderWidth = 1.5f;
                    pill->style.borderColor = Math::Vector3(0.45f, 0.85f, 0.9f);
                    pill->focusable = false;
                }
                u32 hintLabelId = canvas.AddElement(GUI::UIWidgetType::Label, "Tab Label", pillId);
                if (auto* lbl = canvas.GetElement(hintLabelId)) {
                    lbl->anchor.anchorMin = Math::Vector2(0.0f, 0.0f);
                    lbl->anchor.anchorMax = Math::Vector2(1.0f, 1.0f);
                    lbl->anchor.offsetLeft = 0.0f; lbl->anchor.offsetRight = 0.0f;
                    lbl->anchor.offsetTop = 0.0f; lbl->anchor.offsetBottom = 0.0f;
                    lbl->data.text = "press  [ Tab ]  ~  accessibility settings";
                    lbl->data.textAlignH = 1;
                    lbl->data.textAlignV = 1;
                    lbl->style.fontSize = 20.0f;
                    lbl->style.textColor = Math::Vector3(0.92f, 0.98f, 1.0f);
                    lbl->focusable = false;
                }
            }

            // Demo input entity: Tab toggles play mode <-> the sliding menu
            {
                ECS::Entity input = m_World->CreateEntity();
                m_World->AddComponent<ECS::NameComponent>(input, "Web Demo Input");
                m_World->AddComponent<ECS::TransformComponent>(input);
                auto& sc = m_World->AddComponent<ECS::ScriptComponent>(input);
                ECS::ScriptAttachment att;
                att.scriptPath = "scripts/WebDemoInput.as";
                att.className = "WebDemoInput";
                sc.scripts.push_back(std::move(att));
            }

            // Write the demo input script next to the project
            {
                namespace fs = std::filesystem;
                fs::path projRoot;
                if (!m_SceneManager.GetProjectPath().empty()) {
                    projRoot = fs::path(m_SceneManager.GetProjectPath()).parent_path();
                }
                std::error_code ec;
                fs::create_directories(projRoot / "scripts", ec);
                std::ofstream sf(projRoot / "scripts" / "WebDemoInput.as");
                if (sf.is_open()) {
                    sf <<
"// WebDemoInput.as — Tab switches between playing and the accessibility menu.\n"
"// Play mode: cursor captured, WASD + mouse look. Menu mode: cursor free,\n"
"// the settings panel slides in from the right, everything is clickable.\n"
"class WebDemoInput : TegeBehavior {\n"
"    uint64 menuCanvas = 0;\n"
"    uint64 hintCanvas = 0;\n"
"    uint64 player = 0;\n"
"    float slide = 1.0f;        // 0 = menu on screen, 1 = parked off the right edge\n"
"    float slideTarget = 1.0f;\n"
"    float pulseT = 0.0f;\n"
"    bool menuOpen = false;\n"
"\n"
"    void OnStart() {\n"
"        menuCanvas = Scene_FindEntity(\"Accessibility Menu\");\n"
"        hintCanvas = Scene_FindEntity(\"Tab Hint\");\n"
"        player = Scene_FindEntity(\"Player\");\n"
"        // Park the menu off screen before the first frame renders\n"
"        if (menuCanvas != 0) UI_SetElementOffsets(menuCanvas, 1, 900.0f, 900.0f, 0.0f, 0.0f);\n"
"        Subtitle_Show(\"WASD to move, mouse to look. Press Tab for accessibility settings.\", \"\", 5.0f);\n"
"    }\n"
"\n"
"    void OnUpdate(float dt) {\n"
"        if (Input_GetKeyDown(Key::Tab)) {\n"
"            menuOpen = !menuOpen;\n"
"            slideTarget = menuOpen ? 0.0f : 1.0f;\n"
"            Input_SetMouseCaptured(!menuOpen);\n"
"            // Menu mode = the character stops listening entirely: no WASD\n"
"            // drift, no camera spin while the cursor crosses the panel.\n"
"            if (player != 0) Controller_SetEnabled(player, !menuOpen);\n"
"            if (menuOpen) {\n"
"                Announcer_Announce(\"Accessibility settings open\");\n"
"            } else {\n"
"                Subtitle_Show(\"Back to play mode\", \"\", 1.5f);\n"
"            }\n"
"        }\n"
"\n"
"        // Ease the panel toward its target (springy slide from the right)\n"
"        float step = dt * 7.0f;\n"
"        if (step > 1.0f) step = 1.0f;\n"
"        slide += (slideTarget - slide) * step;\n"
"        if (slide < 0.001f) slide = 0.0f;\n"
"        if (slide > 0.999f) slide = 1.0f;\n"
"        if (menuCanvas != 0) {\n"
"            float off = slide * 900.0f;\n"
"            UI_SetElementOffsets(menuCanvas, 1, off, off, 0.0f, 0.0f);\n"
"        }\n"
"\n"
"        // Breathing glow on the Tab pill; hide it while the menu is open\n"
"        pulseT += dt;\n"
"        if (pulseT > 2.4f) pulseT -= 2.4f;\n"
"        if (hintCanvas != 0) {\n"
"            UI_SetElementVisible(hintCanvas, 1, !menuOpen);\n"
"            UI_SetElementVisible(hintCanvas, 2, !menuOpen);\n"
"            float half = 1.2f;\n"
"            float tri = pulseT < half ? pulseT / half : (2.4f - pulseT) / half;\n"
"            UI_SetBgColor(hintCanvas, 1, 0.10f, 0.42f, 0.46f, 0.55f + 0.30f * tri);\n"
"        }\n"
"    }\n"
"}\n";
                    sf.close();
                }
            }

            // Web Demo guide replaces the third-person notes intent
            ECS::Entity guide = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(guide, "-- Web Demo Guide --");
            m_World->AddComponent<ECS::TransformComponent>(guide);
            m_World->AddComponent<ECS::NotesComponent>(guide).notes =
                "WEB DEMO — the website's browser demo\n"
                "======================================\n\n"
                "Third-person playable scene + the LIVE accessibility menu:\n"
                "  WASD + mouse to move, collect the coin, climb the ramp.\n"
                "  TAB toggles menu mode: the cursor appears and the settings\n"
                "  panel slides in from the right (WebDemoInput.as drives it —\n"
                "  pure script + UICanvas, no engine magic).\n"
                "  Every control applies immediately; in browser exports the\n"
                "  screen reader speaks via Web Speech.\n\n"
                "Build & Run with target Web opens it in your browser.";
        }

    }
    else if (templateId == "coinrush") {
        // FLAGSHIP: a complete, winnable 3D loop. Collect coins for score, dodge
        // the spinning spikes (they drain health), reach the glowing portal to win.
        // Wires pickups + hazards + health + trigger-zone victory + HUD + shadows.
        createGround();

        ECS::Entity player = createPlayer3D("Player");
        {
            auto& hp = m_World->AddComponent<ECS::HealthComponent>(player);
            hp.maxHealth = 100.0f;
            hp.currentHealth = 100.0f;
            hp.invulnerabilityTime = 1.0f;   // i-frames so a spike touch drains, not instakills
        }
        {
            auto& ctrl = m_World->AddComponent<ECS::ThirdPersonController>(player);
            ctrl.moveSpeed = 6.0f;
            ctrl.cameraDistance = 6.0f;
            ctrl.cameraHeight = 2.5f;
        }
        SetupCameraForController(player, "ThirdPerson");

        // Coins in a ring around the arena — collectible score, emissive + bob.
        {
            const int coinCount = 8;
            const f32 radius = 9.0f;
            for (int i = 0; i < coinCount; ++i) {
                f32 ang = (static_cast<f32>(i) / coinCount) * 6.2831853f;
                Math::Vector3 cpos(std::cos(ang) * radius, 1.0f, std::sin(ang) * radius);
                ECS::Entity coin = m_World->CreateEntity();
                m_World->AddComponent<ECS::NameComponent>(coin, "Coin " + std::to_string(i + 1));
                auto& ct = m_World->AddComponent<ECS::TransformComponent>(coin);
                ct.position = cpos;
                ct.scale = Math::Vector3(0.4f, 0.4f, 0.4f);
                auto& cmat = m_World->AddComponent<ECS::MaterialComponent>(coin);
                cmat.baseColor = Math::Vector3(1.0f, 0.85f, 0.0f);
                cmat.emissiveColor = Math::Vector3(1.0f, 0.8f, 0.0f);
                cmat.emissiveStrength = 0.6f;
                m_World->AddComponent<ECS::MeshComponent>(coin, Renderer::MeshFactory::CreateSphere(0.2f));
                auto& pk = m_World->AddComponent<ECS::PickupComponent>(coin);
                pk.type = ECS::PickupComponent::PickupType::Coin;
                pk.value = 1.0f;
                pk.pickupRange = 1.0f;
                addSphereCollider3D(coin, 0.4f);
                auto& tw = m_World->AddComponent<ECS::TweenComponent>(coin);
                tw.autoPlay = true;
                ECS::TweenEntry bob;
                bob.property = ECS::TweenProperty::Position;
                bob.easing = ECS::EasingType::EaseInOutSine;
                bob.mode = ECS::TweenMode::PingPong;
                bob.startValue = cpos;
                bob.endValue = cpos + Math::Vector3(0.0f, 0.5f, 0.0f);
                bob.duration = 1.2f;
                tw.tweens.push_back(bob);
            }
        }

        // Spinning spikes — pure hazards (DamageComponent, no HealthComponent).
        // CheckHazardOverlaps3D applies contact damage on overlap.
        {
            const Math::Vector3 hazPos[] = { {3.5f, 0.5f, 0.0f}, {-3.0f, 0.5f, 3.5f}, {0.0f, 0.5f, -4.0f} };
            for (int i = 0; i < 3; ++i) {
                ECS::Entity spike = m_World->CreateEntity();
                m_World->AddComponent<ECS::NameComponent>(spike, "Spike " + std::to_string(i + 1));
                auto& st = m_World->AddComponent<ECS::TransformComponent>(spike);
                st.position = hazPos[i];
                st.scale = Math::Vector3(1.0f, 1.0f, 1.0f);
                auto& smat = m_World->AddComponent<ECS::MaterialComponent>(spike);
                smat.baseColor = Math::Vector3(0.85f, 0.1f, 0.1f);
                smat.emissiveColor = Math::Vector3(0.6f, 0.0f, 0.0f);
                smat.emissiveStrength = 0.35f;
                m_World->AddComponent<ECS::MeshComponent>(spike, Renderer::MeshFactory::CreateCube(1.0f));
                addBoxCollider3D(spike, 1.0f, 1.0f, 1.0f);
                // Trigger, not a wall — the player must walk INTO the spike to be hurt.
                // A non-trigger collider becomes a solid static Jolt body that holds
                // the character just outside the damage radius (no damage ever lands).
                m_World->GetComponent<ECS::BoxColliderComponent>(spike)->isTrigger = true;
                auto& dmg = m_World->AddComponent<ECS::DamageComponent>(spike);
                dmg.damage = 25.0f;
                dmg.damageOnce = false;    // keeps hurting on each i-frame window
                dmg.destroyOnHit = false;  // static hazard, not a one-shot projectile
                // Slow spin so it reads as dangerous.
                auto& tw = m_World->AddComponent<ECS::TweenComponent>(spike);
                tw.autoPlay = true;
                ECS::TweenEntry spin;
                spin.property = ECS::TweenProperty::Rotation;
                spin.easing = ECS::EasingType::Linear;
                spin.mode = ECS::TweenMode::Loop;
                spin.startValue = Math::Vector3(0.0f, 0.0f, 0.0f);
                spin.endValue = Math::Vector3(0.0f, 360.0f, 0.0f);
                spin.duration = 3.0f;
                tw.tweens.push_back(spin);
            }
        }

        // Goal portal — a glowing pad with a TriggerZone. Reaching it = victory.
        ECS::Entity goal = m_World->CreateEntity();
        {
            m_World->AddComponent<ECS::NameComponent>(goal, "Goal Portal");
            auto& gt = m_World->AddComponent<ECS::TransformComponent>(goal);
            gt.position = Math::Vector3(0.0f, 0.6f, 14.0f);
            gt.scale = Math::Vector3(2.0f, 1.2f, 2.0f);
            auto& gmat = m_World->AddComponent<ECS::MaterialComponent>(goal);
            gmat.baseColor = Math::Vector3(0.2f, 1.0f, 0.5f);
            gmat.emissiveColor = Math::Vector3(0.1f, 1.0f, 0.4f);
            gmat.emissiveStrength = 1.0f;
            m_World->AddComponent<ECS::MeshComponent>(goal, Renderer::MeshFactory::CreateCube(1.0f));
            auto& tz = m_World->AddComponent<ECS::TriggerZoneComponent>(goal);
            tz.shape = ECS::TriggerZoneComponent::Shape::Box;
            tz.boxSize = Math::Vector3(2.5f, 3.0f, 2.5f);
            tz.triggerOnce = true;
        }

        // Game-over controller: victory when the player reaches the goal trigger.
        {
            ECS::Entity go = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(go, "Game Rules");
            m_World->AddComponent<ECS::TransformComponent>(go);
            auto& goc = m_World->AddComponent<ECS::GameOverComponent>(go);
            goc.victoryOnAllEnemiesDefeated = false;
            goc.victoryTriggerEntity = goal;
            goc.victoryRequiresAllCoins = true;   // portal only opens once all 8 coins are collected
            goc.victoryMessage = "All coins collected — you escaped!";
            goc.defeatMessage = "The spikes got you!";
        }

        // HUD: live health bar + objective label (ImGui overlay; editor + desktop).
        {
            ECS::Entity hb = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(hb, "HUD Health");
            m_World->AddComponent<ECS::TransformComponent>(hb);
            auto& w = m_World->AddComponent<ECS::HUDWidgetComponent>(hb);
            w.type = ECS::HUDWidgetComponent::WidgetType::HealthBar;
            w.sourceEntity = player;
            w.anchorX = 0.04f; w.anchorY = 0.05f;
            w.width = 0.25f; w.height = 0.03f;
            w.fillColor = Math::Vector3(0.9f, 0.2f, 0.2f);
        }
        {
            ECS::Entity ob = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(ob, "HUD Objective");
            m_World->AddComponent<ECS::TransformComponent>(ob);
            auto& w = m_World->AddComponent<ECS::HUDWidgetComponent>(ob);
            w.type = ECS::HUDWidgetComponent::WidgetType::Label;
            w.anchorX = 0.04f; w.anchorY = 0.10f;
            w.text = "Collect all coins, then reach the green portal. Dodge the red spikes!";
            w.textColor = Math::Vector3(1.0f, 1.0f, 1.0f);
            w.fontSize = 18.0f;
        }
        {
            // Live coin counter (Label bound to "coins"; maxValue = total to collect).
            ECS::Entity cc = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(cc, "HUD Coins");
            m_World->AddComponent<ECS::TransformComponent>(cc);
            auto& w = m_World->AddComponent<ECS::HUDWidgetComponent>(cc);
            w.type = ECS::HUDWidgetComponent::WidgetType::Label;
            w.bindField = "coins";
            w.maxValue = 8.0f;   // must match the coin count spawned above
            w.anchorX = 0.04f; w.anchorY = 0.14f;
            w.text = "Coins:";
            w.textColor = Math::Vector3(1.0f, 0.9f, 0.3f);
            w.fontSize = 18.0f;
        }

        createLight();

        // Warm key light so the arena has depth (our fixed directional shadows).
        {
            ECS::Entity spot = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(spot, "Key Point Light");
            auto& t = m_World->AddComponent<ECS::TransformComponent>(spot);
            t.position = Math::Vector3(4.0f, 6.0f, 4.0f);
            auto& lc = m_World->AddComponent<ECS::LightComponent>(spot);
            lc.type = ECS::LightType::Point;
            lc.color = Math::Vector3(1.0f, 0.9f, 0.75f);
            lc.intensity = 2.5f;
            lc.range = 25.0f;
        }

        {
            Renderer::SkyboxConfig sky;
            sky.type = Renderer::SkyboxType::Procedural;
            sky.topColor = Math::Vector3(0.1f, 0.3f, 0.8f);
            sky.horizonColor = Math::Vector3(0.5f, 0.7f, 1.0f);
            sky.bottomColor = Math::Vector3(0.8f, 0.85f, 0.9f);
            sky.sunDirection = Math::Vector3(0.0f, 1.0f, 0.0f);
            m_RenderSystem->SetSkybox(sky);
        }
        m_RenderSystem->SetShadowsEnabled(true);
        m_RenderSystem->SetAmbientIntensity(0.15f);
        if (m_PostProcessing) {
            auto& pp = m_PostProcessing->GetSettings();
            pp.fxaaEnabled = 1;
            pp.bloomEnabled = 1;
            pp.bloomThreshold = 0.85f;
            pp.bloomIntensity = 0.4f;
        }

        // Guide
        {
            ECS::Entity guide = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(guide, "-- Guide --");
            m_World->AddComponent<ECS::TransformComponent>(guide);
            m_World->AddComponent<ECS::NotesComponent>(guide).notes =
                "COIN RUSH - FLAGSHIP 3D LOOP\n"
                "============================\n\n"
                "A complete, winnable game in one scene:\n"
                "  OBJECTIVE- Collect all 8 coins, THEN reach the green portal to WIN\n"
                "  COINS    - 8 collectibles (PickupComponent); the portal stays\n"
                "             locked until every one is collected\n"
                "  SPIKES   - Red hazards (DamageComponent) drain your health\n"
                "  HEALTH   - 100 HP with 1s i-frames; 0 HP = defeat\n"
                "  HUD      - Live health bar + coin counter + objective\n\n"
                "Systems wired together:\n"
                "  - ThirdPersonController + Jolt physics (move/collide)\n"
                "  - CheckPickupOverlaps3D collects coins on contact\n"
                "  - CheckHazardOverlaps3D applies spike damage (i-frames)\n"
                "  - GameOverComponent.victoryRequiresAllCoins gates the portal\n"
                "  - UpdateTriggerZones + GameOverComponent = victory on the portal\n"
                "  - HUDSystem draws the health bar + live coin counter\n\n"
                "Reskin it:\n"
                "  - Move/add coins by duplicating a Coin entity\n"
                "  - Change difficulty via HealthComponent.maxHealth or Spike damage\n"
                "  - Move the Goal Portal to change the route\n"
                "  - Swap meshes/materials to theme the arena";
        }

    }
    else if (templateId == "componentsonly") {
        // TIER 1 DEMO: a complete, winnable game with ZERO code — every
        // behavior is a component configured in the Inspector.
        createGround();
        ECS::Entity player = createPlayer3D("Player");
        auto& ctrl = m_World->AddComponent<ECS::FirstPersonController>(player);
        ctrl.moveSpeed = 5.0f;
        ctrl.mouseSensitivity = 0.15f;
        auto& health = m_World->AddComponent<ECS::HealthComponent>(player);
        health.maxHealth = 100.0f;
        health.currentHealth = 100.0f;
        SetupCameraForController(player, "FirstPerson");

        // Three pickups (PickupComponent does the collecting, no code)
        const Math::Vector3 coinPos[] = { {3,0.6f,-4}, {-3,0.6f,-6}, {0,0.6f,-9} };
        for (int i = 0; i < 3; ++i) {
            ECS::Entity coin = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(coin, ("Pickup " + std::to_string(i + 1)).c_str());
            auto& t = m_World->AddComponent<ECS::TransformComponent>(coin);
            t.position = coinPos[i];
            t.scale = Math::Vector3(0.5f, 0.5f, 0.5f);
            auto& mat = m_World->AddComponent<ECS::MaterialComponent>(coin);
            mat.baseColor = Math::Vector3(1.0f, 0.85f, 0.2f);
            m_World->AddComponent<ECS::MeshComponent>(coin, Renderer::MeshFactory::CreateCube(1.0f));
            auto& pickup = m_World->AddComponent<ECS::PickupComponent>(coin);
            pickup.type = ECS::PickupComponent::PickupType::Coin;
            pickup.value = 1.0f;
            pickup.pickupRange = 1.0f;
        }

        // One hazard (DamageComponent + kinematic sensor rule)
        {
            ECS::Entity spike = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(spike, "Hazard");
            auto& t = m_World->AddComponent<ECS::TransformComponent>(spike);
            t.position = Math::Vector3(0.0f, 0.4f, -5.0f);
            t.scale = Math::Vector3(1.2f, 0.8f, 1.2f);
            auto& mat = m_World->AddComponent<ECS::MaterialComponent>(spike);
            mat.baseColor = Math::Vector3(0.9f, 0.2f, 0.15f);
            m_World->AddComponent<ECS::MeshComponent>(spike, Renderer::MeshFactory::CreateCube(1.0f));
            auto& dmg = m_World->AddComponent<ECS::DamageComponent>(spike);
            dmg.damage = 25.0f;
            auto& col = m_World->AddComponent<ECS::BoxColliderComponent>(spike);
            col.size = Math::Vector3(1.2f, 0.8f, 1.2f);
        }

        // Win portal: trigger zone + game-over gated on all pickups
        {
            ECS::Entity portal = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(portal, "Win Portal");
            auto& t = m_World->AddComponent<ECS::TransformComponent>(portal);
            t.position = Math::Vector3(0.0f, 1.0f, -12.0f);
            t.scale = Math::Vector3(2.0f, 2.0f, 0.5f);
            auto& mat = m_World->AddComponent<ECS::MaterialComponent>(portal);
            mat.baseColor = Math::Vector3(0.2f, 0.9f, 0.4f);
            m_World->AddComponent<ECS::MeshComponent>(portal, Renderer::MeshFactory::CreateCube(1.0f));
            auto& tz = m_World->AddComponent<ECS::TriggerZoneComponent>(portal);
            tz.shape = ECS::TriggerZoneComponent::Shape::Box;
            tz.boxSize = Math::Vector3(2.0f, 2.0f, 1.5f);
            tz.triggerOnce = true;

            ECS::Entity rules = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(rules, "Game Rules");
            m_World->AddComponent<ECS::TransformComponent>(rules);
            auto& goc = m_World->AddComponent<ECS::GameOverComponent>(rules);
            goc.victoryTriggerEntity = portal;
            goc.victoryRequiresAllCoins = true;
            goc.victoryMessage = "All pickups collected - you win!";
            goc.defeatMessage = "The hazard got you!";
        }

        // HUD: health bar + coin counter, all component-driven
        {
            ECS::Entity hb = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(hb, "HUD Health");
            m_World->AddComponent<ECS::TransformComponent>(hb);
            auto& w = m_World->AddComponent<ECS::HUDWidgetComponent>(hb);
            w.type = ECS::HUDWidgetComponent::WidgetType::HealthBar;
            w.bindField = "health";
            w.anchorX = 0.04f; w.anchorY = 0.06f;
        }
        {
            ECS::Entity cc = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(cc, "HUD Pickups");
            m_World->AddComponent<ECS::TransformComponent>(cc);
            auto& w = m_World->AddComponent<ECS::HUDWidgetComponent>(cc);
            w.type = ECS::HUDWidgetComponent::WidgetType::Label;
            w.bindField = "coins";
            w.maxValue = 3.0f;
            w.anchorX = 0.04f; w.anchorY = 0.12f;
            w.text = "Pickups:";
            w.textColor = Math::Vector3(1.0f, 0.85f, 0.2f);
        }

        createLight();

        {
            ECS::Entity guide = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(guide, "-- Guide --");
            m_World->AddComponent<ECS::TransformComponent>(guide);
            m_World->AddComponent<ECS::NotesComponent>(guide).notes =
                "COMPONENTS ONLY - TIER 1\n"
                "========================\n\n"
                "A winnable game with ZERO code. Everything is a component you\n"
                "can read in the Inspector:\n"
                "  Player     - FirstPersonController + HealthComponent\n"
                "  Pickup 1-3 - PickupComponent (collected on touch)\n"
                "  Hazard     - DamageComponent (hurts on touch)\n"
                "  Win Portal - TriggerZone + GameOverComponent, victory gated\n"
                "               on collecting every pickup\n"
                "  HUD        - HUDWidgetComponents bound to health/coins\n\n"
                "Press Play: collect all 3 gold cubes, avoid the red one, walk\n"
                "into the green portal to win.\n\n"
                "See also the 'Script Only' template - the same idea built as\n"
                "one AngelScript file - and Examples/ in the engine repo for\n"
                "the C++-only version.";
        }

    }
    else if (templateId == "scriptonly") {
        // TIER 2 DEMO: Identical scene layout & gameplay to "componentsonly",
        // but every behavior is split up into bite-sized AngelScript files
        // attached directly to each individual game object.
        createGround();
        createLight();

        namespace fs = std::filesystem;
        fs::path projRoot;
        if (!m_SceneManager.GetProjectPath().empty()) {
            projRoot = fs::path(m_SceneManager.GetProjectPath()).parent_path();
        }
        std::error_code ec;
        fs::create_directories(projRoot / "scripts", ec);

        // 1. Write PlayerController.as
        {
            std::ofstream sf(projRoot / "scripts" / "PlayerController.as");
            if (sf.is_open()) {
                sf <<
"// PlayerController.as — Bite-sized behavior attached to Player\n"
"class PlayerController : TegeBehavior {\n"
"    [Property] float moveSpeed = 5.0f;\n"
"    [Property] float maxHealth = 100.0f;\n"
"\n"
"    float currentHealth = 100.0f;\n"
"    int pickupsCollected = 0;\n"
"\n"
"    void OnStart() {\n"
"        currentHealth = maxHealth;\n"
"        UpdateHUD();\n"
"    }\n"
"\n"
"    void OnUpdate(float dt) {\n"
"        Vector3 move = Vector3(0, 0, 0);\n"
"        if (Input_GetKey(Key::W)) move.z -= 1.0f;\n"
"        if (Input_GetKey(Key::S)) move.z += 1.0f;\n"
"        if (Input_GetKey(Key::A)) move.x -= 1.0f;\n"
"        if (Input_GetKey(Key::D)) move.x += 1.0f;\n"
"\n"
"        if (move.Length() > 0.001f) {\n"
"            move = move.Normalized() * (moveSpeed * dt);\n"
"            SetPosition(GetPosition() + move);\n"
"        }\n"
"    }\n"
"\n"
"    void AddPickup() {\n"
"        pickupsCollected++;\n"
"        UpdateHUD();\n"
"        Debug_Log(\"Collected pickup! Total: \" + pickupsCollected);\n"
"    }\n"
"\n"
"    void TakeDamage(float damage) {\n"
"        currentHealth -= damage;\n"
"        if (currentHealth < 0.0f) currentHealth = 0.0f;\n"
"        UpdateHUD();\n"
"        Debug_Log(\"Player took \" + damage + \" damage. HP: \" + currentHealth);\n"
"        if (currentHealth <= 0.0f) {\n"
"            uint64 hud = Scene_FindEntity(\"HUD_Text\");\n"
"            if (hud != 0) HUD_SetText(hud, \"GAME OVER! Health Depleted.\");\n"
"        }\n"
"    }\n"
"\n"
"    void UpdateHUD() {\n"
"        uint64 hud = Scene_FindEntity(\"HUD_Text\");\n"
"        if (hud != 0) {\n"
"            HUD_SetText(hud, \"HP: \" + int(currentHealth) + \"/100  |  Pickups: \" + pickupsCollected + \"/3\");\n"
"        }\n"
"    }\n"
"}\n";
                sf.close();
            }
        }

        // 2. Write PickupItem.as
        {
            std::ofstream sf(projRoot / "scripts" / "PickupItem.as");
            if (sf.is_open()) {
                sf <<
"// PickupItem.as — Bite-sized behavior attached to each Pickup object\n"
"class PickupItem : TegeBehavior {\n"
"    [Property] float spinSpeed = 90.0f;\n"
"    [Property] float pickupRadius = 1.2f;\n"
"\n"
"    void OnUpdate(float dt) {\n"
"        Vector3 rot = GetRotation();\n"
"        rot.y += spinSpeed * dt;\n"
"        SetRotation(rot);\n"
"\n"
"        uint64 player = Scene_FindEntity(\"Player\");\n"
"        if (player != 0) {\n"
"            Vector3 diff = GetPosition() - Entity_GetPosition(player);\n"
"            if (diff.Length() <= pickupRadius) {\n"
"                PlayerController@ controller = cast<PlayerController>(Entity_GetBehavior(player, \"PlayerController\"));\n"
"                if (controller !is null) {\n"
"                    controller.AddPickup();\n"
"                }\n"
"                Scene_DestroyEntity(GetEntity());\n"
"            }\n"
"        }\n"
"    }\n"
"}\n";
                sf.close();
            }
        }

        // 3. Write HazardSpike.as
        {
            std::ofstream sf(projRoot / "scripts" / "HazardSpike.as");
            if (sf.is_open()) {
                sf <<
"// HazardSpike.as — Bite-sized behavior attached to the Hazard object\n"
"class HazardSpike : TegeBehavior {\n"
"    [Property] float damageAmount = 25.0f;\n"
"    [Property] float hazardRadius = 1.2f;\n"
"    [Property] float cooldownTime = 1.0f;\n"
"\n"
"    private float _timer = 0.0f;\n"
"\n"
"    void OnUpdate(float dt) {\n"
"        if (_timer > 0.0f) {\n"
"            _timer -= dt;\n"
"            return;\n"
"        }\n"
"\n"
"        uint64 player = Scene_FindEntity(\"Player\");\n"
"        if (player != 0) {\n"
"            Vector3 diff = GetPosition() - Entity_GetPosition(player);\n"
"            if (diff.Length() <= hazardRadius) {\n"
"                PlayerController@ controller = cast<PlayerController>(Entity_GetBehavior(player, \"PlayerController\"));\n"
"                if (controller !is null) {\n"
"                    controller.TakeDamage(damageAmount);\n"
"                    _timer = cooldownTime;\n"
"                }\n"
"            }\n"
"        }\n"
"    }\n"
"}\n";
                sf.close();
            }
        }

        // 4. Write WinPortal.as
        {
            std::ofstream sf(projRoot / "scripts" / "WinPortal.as");
            if (sf.is_open()) {
                sf <<
"// WinPortal.as — Bite-sized behavior attached to the Win Portal object\n"
"class WinPortal : TegeBehavior {\n"
"    [Property] float portalRadius = 1.8f;\n"
"    [Property] int requiredPickups = 3;\n"
"\n"
"    void OnUpdate(float dt) {\n"
"        Vector3 rot = GetRotation();\n"
"        rot.z += 45.0f * dt;\n"
"        SetRotation(rot);\n"
"\n"
"        uint64 player = Scene_FindEntity(\"Player\");\n"
"        if (player != 0) {\n"
"            Vector3 diff = GetPosition() - Entity_GetPosition(player);\n"
"            if (diff.Length() <= portalRadius) {\n"
"                PlayerController@ controller = cast<PlayerController>(Entity_GetBehavior(player, \"PlayerController\"));\n"
"                if (controller !is null) {\n"
"                    uint64 hud = Scene_FindEntity(\"HUD_Text\");\n"
"                    if (controller.pickupsCollected >= requiredPickups) {\n"
"                        if (hud != 0) HUD_SetText(hud, \"YOU WIN! All \" + requiredPickups + \" Pickups Collected!\");\n"
"                        Debug_Log(\"VICTORY! Reached portal with all pickups.\");\n"
"                    } else {\n"
"                        if (hud != 0) HUD_SetText(hud, \"Need \" + requiredPickups + \" Pickups to Win! (\" + controller.pickupsCollected + \"/\" + requiredPickups + \")\");\n"
"                    }\n"
"                }\n"
"            }\n"
"        }\n"
"    }\n"
"}\n";
                sf.close();
            }
        }

        // Create Player entity and attach PlayerController.as
        ECS::Entity player = createPlayer3D("Player");
        SetupCameraForController(player, "FirstPerson");
        {
            auto& sc = m_World->AddComponent<ECS::ScriptComponent>(player);
            ECS::ScriptAttachment att;
            att.scriptPath = "scripts/PlayerController.as";
            att.className = "PlayerController";
            sc.scripts.push_back(std::move(att));
        }

        // Three Pickups (identical positions & visuals to componentsonly)
        const Math::Vector3 coinPos[] = { {3,0.6f,-4}, {-3,0.6f,-6}, {0,0.6f,-9} };
        for (int i = 0; i < 3; ++i) {
            ECS::Entity coin = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(coin, ("Pickup " + std::to_string(i + 1)).c_str());
            auto& t = m_World->AddComponent<ECS::TransformComponent>(coin);
            t.position = coinPos[i];
            t.scale = Math::Vector3(0.5f, 0.5f, 0.5f);
            auto& mat = m_World->AddComponent<ECS::MaterialComponent>(coin);
            mat.baseColor = Math::Vector3(1.0f, 0.85f, 0.2f);
            m_World->AddComponent<ECS::MeshComponent>(coin, Renderer::MeshFactory::CreateCube(1.0f));

            auto& sc = m_World->AddComponent<ECS::ScriptComponent>(coin);
            ECS::ScriptAttachment att;
            att.scriptPath = "scripts/PickupItem.as";
            att.className = "PickupItem";
            sc.scripts.push_back(std::move(att));
        }

        // One Hazard (identical position & visual to componentsonly)
        {
            ECS::Entity spike = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(spike, "Hazard");
            auto& t = m_World->AddComponent<ECS::TransformComponent>(spike);
            t.position = Math::Vector3(0.0f, 0.4f, -5.0f);
            t.scale = Math::Vector3(1.2f, 0.8f, 1.2f);
            auto& mat = m_World->AddComponent<ECS::MaterialComponent>(spike);
            mat.baseColor = Math::Vector3(0.9f, 0.2f, 0.15f);
            m_World->AddComponent<ECS::MeshComponent>(spike, Renderer::MeshFactory::CreateCube(1.0f));

            auto& sc = m_World->AddComponent<ECS::ScriptComponent>(spike);
            ECS::ScriptAttachment att;
            att.scriptPath = "scripts/HazardSpike.as";
            att.className = "HazardSpike";
            sc.scripts.push_back(std::move(att));
        }

        // Win Portal (identical position & visual to componentsonly)
        {
            ECS::Entity portal = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(portal, "Win Portal");
            auto& t = m_World->AddComponent<ECS::TransformComponent>(portal);
            t.position = Math::Vector3(0.0f, 1.0f, -12.0f);
            t.scale = Math::Vector3(2.0f, 2.0f, 0.5f);
            auto& mat = m_World->AddComponent<ECS::MaterialComponent>(portal);
            mat.baseColor = Math::Vector3(0.2f, 0.9f, 0.4f);
            m_World->AddComponent<ECS::MeshComponent>(portal, Renderer::MeshFactory::CreateCube(1.0f));

            auto& sc = m_World->AddComponent<ECS::ScriptComponent>(portal);
            ECS::ScriptAttachment att;
            att.scriptPath = "scripts/WinPortal.as";
            att.className = "WinPortal";
            sc.scripts.push_back(std::move(att));
        }

        // HUD Text entity
        {
            ECS::Entity hud = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(hud, "HUD_Text");
            m_World->AddComponent<ECS::TransformComponent>(hud);
            auto& w = m_World->AddComponent<ECS::HUDWidgetComponent>(hud);
            w.type = ECS::HUDWidgetComponent::WidgetType::Label;
            w.bindField = "custom";
            w.anchorX = 0.04f; w.anchorY = 0.06f;
            w.text = "HP: 100/100  |  Pickups: 0/3";
            w.fontSize = 20.0f;
        }

        // Guide Entity
        {
            ECS::Entity guide = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(guide, "-- Guide --");
            m_World->AddComponent<ECS::TransformComponent>(guide);
            m_World->AddComponent<ECS::NotesComponent>(guide).notes =
                "SCRIPT ONLY - TIER 2\n"
                "====================\n\n"
                "The EXACT SAME game as 'Components Only', but with logic split\n"
                "into bite-sized AngelScript files on each game object:\n\n"
                "  Player     -> scripts/PlayerController.as (WASD movement, 100 HP, HUD)\n"
                "  Pickup 1-3 -> scripts/PickupItem.as       (spins cube, collects on touch)\n"
                "  Hazard     -> scripts/HazardSpike.as      (detects touch, deals 25 damage)\n"
                "  Win Portal -> scripts/WinPortal.as        (rotates portal, checks 3 pickups to win)\n\n"
                "Compare this 1:1 with 'Components Only' to see how AngelScript\n"
                "behaviors replace built-in components on a per-object basis.\n\n"
                "Press Play, collect all 3 gold cubes, avoid the red hazard, and\n"
                "walk into the green portal to win.\n\n"
                "Edit any script and press Play again — scripts hot-reload instantly.";
        }

    }
    else if (templateId == "firstperson") {
        createGround();
        ECS::Entity player = createPlayer3D("Player");
        auto* playerT = m_World->GetComponent<ECS::TransformComponent>(player);
        if (playerT) playerT->position.y = 1.7f;
        auto& ctrl = m_World->AddComponent<ECS::FirstPersonController>(player);
        ctrl.moveSpeed = 5.0f;
        ctrl.mouseSensitivity = 0.15f;
        // Health so the player can interact with damage-dealing entities
        auto& health = m_World->AddComponent<ECS::HealthComponent>(player);
        health.maxHealth = 100.0f;
        health.currentHealth = 100.0f;
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

        // Guide entity
        {
            ECS::Entity guide = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(guide, "-- Guide --");
            m_World->AddComponent<ECS::TransformComponent>(guide);
            m_World->AddComponent<ECS::NotesComponent>(guide).notes =
                "3D FIRST PERSON STARTER\n"
                "========================\n\n"
                "Entities:\n"
                "  Player - Capsule with FirstPersonController (WASD + mouse look)\n"
                "  Walls - Static cubes forming an L-shaped corridor\n"
                "  Door - SwitchComponent (interact to toggle open/close)\n"
                "  Corridor Light - Warm point light for atmosphere\n"
                "  Flashlight - Spot light with FollowTargetComponent on player\n\n"
                "Key systems:\n"
                "  - FirstPersonController handles mouselook + WASD movement\n"
                "  - SwitchComponent enables interact prompts (press E)\n"
                "  - FollowTargetComponent makes the flashlight track the player\n"
                "  - Vignette post-processing adds atmosphere\n\n"
                "Try:\n"
                "  - Add AudioSourceComponent for ambient sounds\n"
                "  - Add more rooms and doors\n"
                "  - Add PickupComponent items (keys, notes)\n"
                "  - Increase vignette for horror atmosphere";
        }

    }
    else if (templateId == "narrative") {
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

        // Guide entity
        {
            ECS::Entity guide = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(guide, "-- Guide --");
            m_World->AddComponent<ECS::TransformComponent>(guide);
            m_World->AddComponent<ECS::NotesComponent>(guide).notes =
                "NARRATIVE & DIALOGUE STARTER\n"
                "=============================\n\n"
                "Entities:\n"
                "  Player - ThirdPersonController for exploration\n"
                "  NPCs (Quest Giver / Merchant / Storyteller) - DialogueComponent\n"
                "  Quest State - QuestFlowComponent for tracking progress\n"
                "  Dialogue Box - DialogueBoxComponent (UI at screen bottom)\n"
                "  Old Book / Chest - InteractableComponent with examine text\n"
                "  Fireflies - ParticleEmitterComponent for atmosphere\n\n"
                "Key systems:\n"
                "  - DialogueComponent stores multi-line NPC dialogue\n"
                "  - InteractableComponent shows 'Talk' / 'Read' prompts\n"
                "  - QuestFlowComponent tracks quest state transitions\n"
                "  - DialogueBoxComponent renders text at screen bottom\n\n"
                "Try:\n"
                "  - Add DialogueTree branching (choices for the player)\n"
                "  - Add more NPCs with different quest objectives\n"
                "  - Use CinematicCameraComponent for quest intro cutscenes\n"
                "  - Add WorldTimeSystem for day/night NPC schedules";
        }

    }
    else if (templateId == "accessibility") {
        // ACCESSIBILITY DEMO — every option is LIVE. UICanvas controls fire
        // events, scripts/AccessibilityDemo.as applies them through the
        // accessibility script bindings the same frame. This scene is also
        // exported as the website's browser demo, where the screen reader
        // actually speaks via the Web Speech API.
        createGround();
        createLight();

        // In-game camera
        {
            ECS::Entity cam = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(cam, "Camera");
            auto& ct = m_World->AddComponent<ECS::TransformComponent>(cam);
            ct.position = Math::Vector3(-2.5f, 2.4f, 5.5f);
            auto& cc = m_World->AddComponent<ECS::CameraComponent>(cam);
            cc.projectionType = ECS::ProjectionType::Perspective;
            cc.fieldOfView = 60.0f;
            cc.isActive = true;
            cc.priority = 10;
            m_SelectedGameCamera = cam;
        }

        // Color swatch row — makes the colorblind filter change visibly obvious
        {
            const Math::Vector3 swatchColors[6] = {
                {0.85f, 0.10f, 0.10f}, {0.90f, 0.50f, 0.10f}, {0.90f, 0.85f, 0.10f},
                {0.10f, 0.70f, 0.20f}, {0.15f, 0.35f, 0.90f}, {0.60f, 0.20f, 0.80f} };
            const char* swatchNames[6] = { "Swatch Red", "Swatch Orange", "Swatch Yellow",
                                           "Swatch Green", "Swatch Blue", "Swatch Purple" };
            for (int i = 0; i < 6; ++i) {
                ECS::Entity cube = m_World->CreateEntity();
                m_World->AddComponent<ECS::NameComponent>(cube, swatchNames[i]);
                auto& t = m_World->AddComponent<ECS::TransformComponent>(cube);
                t.position = Math::Vector3(-7.0f + i * 1.6f, 0.55f, -3.0f);
                auto& mat = m_World->AddComponent<ECS::MaterialComponent>(cube);
                mat.baseColor = swatchColors[i];
                mat.roughness = 0.6f;
                m_World->AddComponent<ECS::MeshComponent>(cube, Renderer::MeshFactory::CreateCube(1.0f));
            }
        }

        // Spinning cube — demonstrates Reduced Motion (script stops the spin)
        {
            ECS::Entity spin = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(spin, "Spinning Cube");
            auto& t = m_World->AddComponent<ECS::TransformComponent>(spin);
            t.position = Math::Vector3(-4.5f, 1.8f, -6.0f);
            t.scale = Math::Vector3(1.2f, 1.2f, 1.2f);
            auto& mat = m_World->AddComponent<ECS::MaterialComponent>(spin);
            mat.baseColor = Math::Vector3(0.9f, 0.75f, 0.2f);
            mat.metallic = 0.3f;
            m_World->AddComponent<ECS::MeshComponent>(spin, Renderer::MeshFactory::CreateCube(1.0f));
            auto& sc = m_World->AddComponent<ECS::ScriptComponent>(spin);
            ECS::ScriptAttachment att;
            att.scriptPath = "scripts/SpinningCube.as";
            att.className = "SpinningCube";
            sc.scripts.push_back(std::move(att));
        }

        // Screen-reader-labeled interactables
        {
            ECS::Entity exampleObj = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(exampleObj, "Interactive Object");
            auto& ot = m_World->AddComponent<ECS::TransformComponent>(exampleObj);
            ot.position = Math::Vector3(-7.0f, 0.5f, -6.0f);
            auto& omat = m_World->AddComponent<ECS::MaterialComponent>(exampleObj);
            omat.baseColor = Math::Vector3(0.4f, 0.6f, 0.8f);
            m_World->AddComponent<ECS::MeshComponent>(exampleObj, Renderer::MeshFactory::CreateSphere(0.5f));
            auto& interact = m_World->AddComponent<ECS::InteractableComponent>(exampleObj);
            interact.promptText = "Press E to interact";
        }

        // Menu UI + controller entity + live-settings script — shared with
        // the Web Demo template
        CreateAccessibilityMenu();


        // Write the demo scripts next to the project (same pattern as scriptonly)
        {
            namespace fs = std::filesystem;
            fs::path projRoot;
            if (!m_SceneManager.GetProjectPath().empty()) {
                projRoot = fs::path(m_SceneManager.GetProjectPath()).parent_path();
            }
            std::error_code ec;
            fs::create_directories(projRoot / "scripts", ec);


            {
                std::ofstream sf(projRoot / "scripts" / "SpinningCube.as");
                if (sf.is_open()) {
                    sf <<
"// SpinningCube.as — spins unless Reduced Motion is enabled.\n"
"class SpinningCube : TegeBehavior {\n"
"    [Property] float spinSpeed = 60.0f;\n"
"\n"
"    void OnUpdate(float dt) {\n"
"        if (Accessibility_GetReducedMotion()) return;\n"
"        Vector3 rot = GetRotation();\n"
"        rot.y += spinSpeed * dt;\n"
"        SetRotation(rot);\n"
"    }\n"
"}\n";
                    sf.close();
                }
            }
        }

        // Guide notes
        {
            ECS::Entity hint = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(hint, "Accessibility Guide");
            m_World->AddComponent<ECS::TransformComponent>(hint);
            auto& notes = m_World->AddComponent<ECS::NotesComponent>(hint);
            notes.notes = "Accessibility Demo template — every control is LIVE.\n"
                "Press Play: sliders and toggles apply immediately via AccessibilityDemo.as.\n"
                "Navigate with Tab/Shift+Tab, arrows, or gamepad DPad; Enter/Space activates.\n"
                "Colorblind Mode slider: 0=Off through 7=Achromatopsia (watch the swatch cubes).\n"
                "Reduced Motion stops the spinning cube (SpinningCube.as checks the setting).\n"
                "Screen Reader announcements appear in the status bar; in browser exports\n"
                "they are spoken aloud via the Web Speech API.\n"
                "This scene doubles as the website's browser demo.";
        }

        // Sky + render settings
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
    }
    else if (templateId == "pointclick") {
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
                panel->anchor.offsetLeft = 20.0f; panel->anchor.offsetTop = -80.0f;
                panel->anchor.offsetRight = 420.0f; panel->anchor.offsetBottom = -20.0f;
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

    }
    else if (templateId == "idleclicker") {
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
                curr->anchor.offsetLeft = -100.0f; curr->anchor.offsetTop = 30.0f;
                curr->anchor.offsetRight = 100.0f; curr->anchor.offsetBottom = 80.0f;
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
                cp->anchor.offsetLeft = -100.0f; cp->anchor.offsetTop = 80.0f;
                cp->anchor.offsetRight = 100.0f; cp->anchor.offsetBottom = 120.0f;
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
                upg->anchor.offsetLeft = -120.0f; upg->anchor.offsetTop = -120.0f;
                upg->anchor.offsetRight = 120.0f; upg->anchor.offsetBottom = -70.0f;
                upg->data.text = "Upgrade (Cost: 10)";
                upg->tabOrder = 1;
            }
            // Auto-click toggle (below upgrade)
            u32 actId = canvas.AddElement(GUI::UIWidgetType::Toggle, "AutoClickToggle");
            if (auto* act = canvas.GetElement(actId)) {
                act->anchor.anchorMin = Math::Vector2(0.5f, 1.0f);
                act->anchor.anchorMax = Math::Vector2(0.5f, 1.0f);
                act->anchor.offsetLeft = -120.0f; act->anchor.offsetTop = -60.0f;
                act->anchor.offsetRight = 120.0f; act->anchor.offsetBottom = -20.0f;
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

    }
    else if (templateId == "planetgravity") {
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
            saCtrl.jumpForce = 20.0f;
            saCtrl.cameraDistance = 12.0f;
            saCtrl.cameraHeight = 5.0f;
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
            // No FollowTarget or LookAtTarget -- the SurfaceAlignedController
            // manages the camera directly via UpdateGameCameraTransform()
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
                auto& bc = m_World->AddComponent<ECS::BoxColliderComponent>(prop);
                bc.size = Math::Vector3(0.5f, 0.5f, 0.5f);
            }
        }

        // Second moon (smaller, different color)
        {
            ECS::Entity moon = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(moon, "Small Moon");
            auto& mt = m_World->AddComponent<ECS::TransformComponent>(moon);
            mt.position = Math::Vector3(0.0f, 12.0f, 0.0f);
            mt.scale = Math::Vector3(2.0f, 2.0f, 2.0f);
            auto& mmat = m_World->AddComponent<ECS::MaterialComponent>(moon);
            mmat.baseColor = Math::Vector3(0.6f, 0.55f, 0.5f);
            mmat.roughness = 0.9f;
            m_World->AddComponent<ECS::MeshComponent>(moon, Renderer::MeshFactory::CreateSphere(1.0f));
            auto& gz = m_World->AddComponent<ECS::GravityZoneComponent>(moon);
            gz.mode = ECS::GravityZoneMode::Point;
            gz.shape = ECS::GravityZoneShape::Sphere;
            gz.halfExtents = Math::Vector3(30.0f, 30.0f, 30.0f);
            gz.gravityStrength = 10.0f;
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

        // Guide entity
        {
            ECS::Entity guide = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(guide, "-- Guide --");
            m_World->AddComponent<ECS::TransformComponent>(guide);
            m_World->AddComponent<ECS::NotesComponent>(guide).notes =
                "3D ISOMETRIC STARTER\n"
                "=====================\n\n"
                "Entities:\n"
                "  Player - TopDown3DController at 45-degree camera angle\n"
                "  Buildings - Static cubes representing houses/shops\n"
                "  Villager / Merchant - NPCs with DialogueComponent\n"
                "  Treasure Chest - PickupComponent collectible\n"
                "  Lantern - Point light for warm ambient glow\n\n"
                "Key systems:\n"
                "  - TopDown3DController with cameraAngle=45 gives isometric view\n"
                "  - DialogueComponent + InteractableComponent for NPC interaction\n"
                "  - PickupComponent for collectibles\n"
                "  - Adjust cameraDistance to zoom in/out\n\n"
                "Try:\n"
                "  - Add more buildings to form streets\n"
                "  - Add AIControllerComponent for NPC patrol routes\n"
                "  - Add InventoryComponent + shop system\n"
                "  - Add WorldTimeSystem for day/night cycle";
        }

    }
    else if (templateId == "teamsports") {
        // ═══════════════════════════════════════════════════
        // Team Sports — Starter (2v2)
        // Minimal: field, goals, ball, 1 player + 1 AI per team
        // ═══════════════════════════════════════════════════

        // Field
        {
            ECS::Entity field = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(field, "Playing Field");
            auto& ft = m_World->AddComponent<ECS::TransformComponent>(field);
            ft.scale = Math::Vector3(30.0f, 0.1f, 18.0f);
            ft.position = Math::Vector3(0.0f, -0.05f, 0.0f);
            auto& fm = m_World->AddComponent<ECS::MaterialComponent>(field);
            fm.baseColor = Math::Vector3(0.2f, 0.55f, 0.15f);
            fm.roughness = 0.95f;
            m_World->AddComponent<ECS::MeshComponent>(field, Renderer::MeshFactory::CreateCube(1.0f));
            auto& col = m_World->AddComponent<ECS::BoxColliderComponent>(field);
            col.size = Math::Vector3(30.0f, 0.1f, 18.0f);
        }
        createLight();

        // Goals
        for (int side = 0; side < 2; ++side) {
            ECS::Entity goal = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(goal, side == 0 ? "Goal Left" : "Goal Right");
            auto& gt = m_World->AddComponent<ECS::TransformComponent>(goal);
            gt.position = Math::Vector3(side == 0 ? -15.0f : 15.0f, 1.5f, 0.0f);
            gt.scale = Math::Vector3(0.3f, 3.0f, 5.0f);
            m_World->AddComponent<ECS::MeshComponent>(goal, Renderer::MeshFactory::CreateCube(1.0f));
            auto& gm = m_World->AddComponent<ECS::MaterialComponent>(goal);
            gm.baseColor = Math::Vector3(1.0f, 1.0f, 1.0f);
            gm.opacity = 0.3f;
            gm.alphaMode = ECS::MaterialComponent::AlphaMode::Blend;
            auto& trigger = m_World->AddComponent<ECS::TriggerZoneComponent>(goal);
            trigger.shape = ECS::TriggerZoneComponent::Shape::Box;
            trigger.boxSize = Math::Vector3(0.3f, 3.0f, 5.0f);
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
            m_World->AddComponent<ECS::TagComponent>(ball).tags.push_back("ball");
        }

        // Team A (blue): 1 player + 1 AI
        {
            // Player-controlled
            ECS::Entity p1 = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(p1, "Team A - Player");
            auto& pt = m_World->AddComponent<ECS::TransformComponent>(p1);
            pt.position = Math::Vector3(-5.0f, 0.5f, 0.0f);
            m_World->AddComponent<ECS::MeshComponent>(p1, Renderer::MeshFactory::CreateCapsule(0.3f, 0.9f));
            m_World->AddComponent<ECS::MaterialComponent>(p1).baseColor = Math::Vector3(0.15f, 0.3f, 0.85f);
            m_World->AddComponent<ECS::TagComponent>(p1).tags.push_back("team_a");
            auto& pcol = m_World->AddComponent<ECS::CapsuleColliderComponent>(p1);
            pcol.radius = 0.3f; pcol.height = 0.9f;
            auto& ctrl = m_World->AddComponent<ECS::TopDown3DController>(p1);
            ctrl.moveSpeed = 7.0f;
            SetupCameraForController(p1, "TopDown3D");

            // AI teammate
            ECS::Entity a1 = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(a1, "Team A - AI");
            auto& at = m_World->AddComponent<ECS::TransformComponent>(a1);
            at.position = Math::Vector3(-10.0f, 0.5f, 0.0f);
            m_World->AddComponent<ECS::MeshComponent>(a1, Renderer::MeshFactory::CreateCapsule(0.3f, 0.9f));
            m_World->AddComponent<ECS::MaterialComponent>(a1).baseColor = Math::Vector3(0.15f, 0.3f, 0.85f);
            m_World->AddComponent<ECS::TagComponent>(a1).tags.push_back("team_a");
            auto& acol = m_World->AddComponent<ECS::CapsuleColliderComponent>(a1);
            acol.radius = 0.3f; acol.height = 0.9f;
            auto& aai = m_World->AddComponent<ECS::AIControllerComponent>(a1);
            aai.currentState = ECS::AIControllerComponent::AIState::Patrol;
            aai.moveSpeed = 5.0f;
        }

        // Team B (red): 2 AI
        {
            Math::Vector3 bPos[] = {Math::Vector3(5.0f, 0.5f, -2.0f), Math::Vector3(10.0f, 0.5f, 2.0f)};
            const char* bNames[] = {"Team B - Forward", "Team B - Goalie"};
            for (int i = 0; i < 2; ++i) {
                ECS::Entity p = m_World->CreateEntity();
                m_World->AddComponent<ECS::NameComponent>(p, bNames[i]);
                m_World->AddComponent<ECS::TransformComponent>(p).position = bPos[i];
                m_World->AddComponent<ECS::MeshComponent>(p, Renderer::MeshFactory::CreateCapsule(0.3f, 0.9f));
                m_World->AddComponent<ECS::MaterialComponent>(p).baseColor = Math::Vector3(0.85f, 0.15f, 0.15f);
                m_World->AddComponent<ECS::TagComponent>(p).tags.push_back("team_b");
                auto& pcol = m_World->AddComponent<ECS::CapsuleColliderComponent>(p);
                pcol.radius = 0.3f; pcol.height = 0.9f;
                auto& ai = m_World->AddComponent<ECS::AIControllerComponent>(p);
                ai.currentState = ECS::AIControllerComponent::AIState::Patrol;
                ai.moveSpeed = 5.0f;
            }
        }

        // Scoreboard
        {
            ECS::Entity score = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(score, "Scoreboard");
            auto& st = m_World->AddComponent<ECS::TransformComponent>(score);
            st.position = Math::Vector3(0.0f, 6.0f, -10.0f);
            auto& text = m_World->AddComponent<ECS::TextComponent>(score);
            text.text = "Blue  0 - 0  Red";
            text.fontSize = 48.0f;
            text.textColor = Math::Vector3(1.0f, 1.0f, 1.0f);
        }

        // Boundary walls
        for (int i = 0; i < 2; ++i) {
            ECS::Entity wall = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(wall, i == 0 ? "Wall North" : "Wall South");
            auto& wt = m_World->AddComponent<ECS::TransformComponent>(wall);
            wt.position = Math::Vector3(0.0f, 1.0f, i == 0 ? -9.0f : 9.0f);
            wt.scale = Math::Vector3(30.0f, 2.0f, 0.2f);
            m_World->AddComponent<ECS::MeshComponent>(wall, Renderer::MeshFactory::CreateCube(1.0f));
            auto& wm = m_World->AddComponent<ECS::MaterialComponent>(wall);
            wm.baseColor = Math::Vector3(0.3f, 0.3f, 0.3f);
            wm.opacity = 0.2f;
            wm.alphaMode = ECS::MaterialComponent::AlphaMode::Blend;
            auto& col = m_World->AddComponent<ECS::BoxColliderComponent>(wall);
            col.size = Math::Vector3(30.0f, 2.0f, 0.2f);
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

        m_RenderSystem->SetShadowsEnabled(true);
        m_RenderSystem->SetAmbientIntensity(0.15f);
        if (m_PostProcessing) {
            auto& pp = m_PostProcessing->GetSettings();
            pp.fxaaEnabled = 1;
        }

        // Guide
        {
            ECS::Entity guide = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(guide, "-- Guide --");
            m_World->AddComponent<ECS::TransformComponent>(guide);
            m_World->AddComponent<ECS::NotesComponent>(guide).notes =
                "TEAM SPORTS STARTER (2v2)\n"
                "=========================\n\n"
                "Entities:\n"
                "  Team A - Player (blue, TopDown3DController)\n"
                "  Team A - AI (blue, AIControllerComponent patrol)\n"
                "  Team B - Forward / Goalie (red, both AI)\n"
                "  Ball - RigidbodyComponent with drag for realistic rolling\n"
                "  Goals - TriggerZoneComponent for scoring detection\n"
                "  Walls - Boundary colliders to keep ball in play\n\n"
                "Key systems:\n"
                "  - TopDown3DController for player movement (WASD)\n"
                "  - Jolt physics drives ball rolling + player collision\n"
                "  - TriggerZoneComponent detects ball entering goals\n"
                "  - AIControllerComponent patrols AI players\n\n"
                "Try:\n"
                "  - Add more players per team (duplicate + reposition)\n"
                "  - Add TimerComponent for match countdown\n"
                "  - Add collision group names for team filtering\n"
                "  - Add a center circle marking (transparent material)";
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
    else if (templateId == "stresstest") {
        // === STRESS TEST TEMPLATE ===
        // Pushes the systems that define the engine's perf envelope in one scene:
        // a grid of point lights (clustered forward lighting), a pile of dynamic
        // Jolt rigid bodies, and the draw-call + shadow load of hundreds of lit
        // meshes. Useful for profiling and for eyeballing parity (clustered
        // lighting + fog now run in Player builds too).
        createGround();
        createLight();  // directional sun

        // --- Physics + draw-call load: 8x8 dynamic bodies that cascade down ---
        const int kBodyGrid = 8;  // 64 dynamic rigid bodies
        int bodyIndex = 0;
        for (int gx = 0; gx < kBodyGrid; ++gx) {
            for (int gz = 0; gz < kBodyGrid; ++gz, ++bodyIndex) {
                ECS::Entity e = m_World->CreateEntity();
                m_World->AddComponent<ECS::NameComponent>(e, "Body");
                auto& t = m_World->AddComponent<ECS::TransformComponent>(e);
                t.position = Math::Vector3(
                    (gx - kBodyGrid * 0.5f) * 1.5f,
                    3.0f + (bodyIndex % 5) * 1.2f,   // staggered heights -> cascade
                    (gz - kBodyGrid * 0.5f) * 1.5f);
                auto& mat = m_World->AddComponent<ECS::MaterialComponent>(e);
                mat.baseColor = Math::Vector3(0.3f + 0.6f * ((gx % 4) / 3.0f), 0.45f, 0.75f);
                mat.roughness = 0.6f;
                mat.castShadows = true;
                auto& rb = m_World->AddComponent<ECS::RigidbodyComponent>(e);
                rb.bodyType = ECS::RigidbodyComponent::BodyType::Dynamic;
                rb.mass = 1.0f;
                if ((gx + gz) % 2 == 0) {
                    m_World->AddComponent<ECS::MeshComponent>(e, Renderer::MeshFactory::CreateCube(1.0f));
                    addBoxCollider3D(e, 1.0f, 1.0f, 1.0f);
                } else {
                    m_World->AddComponent<ECS::MeshComponent>(e, Renderer::MeshFactory::CreateSphere(0.5f, 16, 12));
                    addSphereCollider3D(e, 0.5f);
                }
            }
        }

        // --- Clustered-lighting load: 4x4 colored point lights overhead ---
        // Shadows off (16 shadow-casting point lights would thrash the cubemap
        // shadow path); this is a clustered-lighting stress test, not a shadow one.
        const int kLightGrid = 4;
        for (int lx = 0; lx < kLightGrid; ++lx) {
            for (int lz = 0; lz < kLightGrid; ++lz) {
                ECS::Entity e = m_World->CreateEntity();
                m_World->AddComponent<ECS::NameComponent>(e, "PointLight");
                auto& t = m_World->AddComponent<ECS::TransformComponent>(e);
                t.position = Math::Vector3(
                    (lx - kLightGrid * 0.5f) * 3.0f,
                    6.0f,
                    (lz - kLightGrid * 0.5f) * 3.0f);
                auto& lc = m_World->AddComponent<ECS::LightComponent>(e);
                lc.type = ECS::LightType::Point;
                lc.color = Math::Vector3((lx % 3) * 0.5f, (lz % 3) * 0.5f, 1.0f - (lx % 2) * 0.5f);
                lc.intensity = 4.0f;
                lc.range = 8.0f;
                lc.castShadows = false;  // many shadow-casting point lights are too heavy
            }
        }

        m_RenderSystem->SetShadowsEnabled(true);
        m_RenderSystem->SetAmbientIntensity(0.08f);
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
            "vampsurvivor", "roguelike", "soulslike", "couchcoop", "justtwo", "shadowtest", "stresstest",
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


// --------------------------------------------------
// Live accessibility settings menu: UICanvas controls + controller entity +
// scripts/AccessibilityDemo.as. Shared by the Accessibility Demo template and
// the Web Demo template (third-person scene + this menu = the site demo).
// --------------------------------------------------
void EditorLayer::CreateAccessibilityMenu() {
    if (!m_World) return;

    // Controller entity running the live settings script
    {
        ECS::Entity ctrl = m_World->CreateEntity();
        m_World->AddComponent<ECS::NameComponent>(ctrl, "A11y Controller");
        m_World->AddComponent<ECS::TransformComponent>(ctrl);
        auto& sc = m_World->AddComponent<ECS::ScriptComponent>(ctrl);
        ECS::ScriptAttachment att;
        att.scriptPath = "scripts/AccessibilityDemo.as";
        att.className = "AccessibilityDemo";
        sc.scripts.push_back(std::move(att));
    }

    // Live settings UICanvas — right side so the scene stays visible
    {
        ECS::Entity uiEntity = m_World->CreateEntity();
        m_World->AddComponent<ECS::NameComponent>(uiEntity, "Accessibility Menu");
        m_World->AddComponent<ECS::TransformComponent>(uiEntity);
        auto& canvas = m_World->AddComponent<GUI::UICanvasComponent>(uiEntity);
        canvas.canvasName = "AccessibilityDemo";
        canvas.designWidth = 1920.0f;
        canvas.designHeight = 1080.0f;

        u32 panelId = canvas.AddElement(GUI::UIWidgetType::Panel, "Settings Panel");
        if (auto* panel = canvas.GetElement(panelId)) {
            panel->anchor.anchorMin = Math::Vector2(0.55f, 0.04f);
            panel->anchor.anchorMax = Math::Vector2(0.98f, 0.97f);
            panel->anchor.offsetLeft = 0; panel->anchor.offsetRight = 0;
            panel->anchor.offsetTop = 0; panel->anchor.offsetBottom = 0;
            panel->style.bgColor = Math::Vector3(0.12f, 0.13f, 0.16f);
            panel->style.bgAlpha = 0.93f;
            panel->style.borderRadius = 12.0f;
            panel->style.borderWidth = 1.0f;
            panel->style.borderColor = Math::Vector3(0.3f, 0.35f, 0.45f);
            panel->focusable = false;
        }

        // NOTE: AddElement pre-fills type-based offsets for CENTERED anchors
        // (labels -100/-100, buttons -80/-80...). Every element here anchors
        // by fractions, so the horizontal offsets MUST be zeroed or each row
        // shifts left by its widget-type default (the "alignment all off"
        // bug, Marty 2026-08-08).
        u32 titleId = canvas.AddElement(GUI::UIWidgetType::Label, "Title", panelId);
        if (auto* title = canvas.GetElement(titleId)) {
            title->anchor.anchorMin = Math::Vector2(0.0f, 0.0f);
            title->anchor.anchorMax = Math::Vector2(1.0f, 0.0f);
            title->anchor.offsetLeft = 0.0f; title->anchor.offsetRight = 0.0f;
            title->anchor.offsetTop = 16.0f; title->anchor.offsetBottom = 56.0f;
            title->data.text = "Accessibility Demo";
            title->data.textAlignH = 1;
            title->style.fontSize = 28.0f;
            title->style.textColor = Math::Vector3(0.9f, 0.92f, 0.96f);
            title->focusable = false;
        }

        i32 tabOrder = 1;
        auto addRowLabel = [&](const char* text, f32 top) {
            u32 id = canvas.AddElement(GUI::UIWidgetType::Label, text, panelId);
            if (auto* lbl = canvas.GetElement(id)) {
                lbl->anchor.anchorMin = Math::Vector2(0.05f, 0.0f);
                lbl->anchor.anchorMax = Math::Vector2(0.55f, 0.0f);
                lbl->anchor.offsetLeft = 0.0f; lbl->anchor.offsetRight = 0.0f;
                lbl->anchor.offsetTop = top; lbl->anchor.offsetBottom = top + 30.0f;
                lbl->data.text = text;
                lbl->data.textAlignH = 0;
                lbl->style.textColor = Math::Vector3(0.8f, 0.82f, 0.88f);
                lbl->focusable = false;
            }
        };
        auto addToggle = [&](const char* label, f32 top, const char* ev, bool checked) {
            addRowLabel(label, top);
            u32 id = canvas.AddElement(GUI::UIWidgetType::Toggle, label, panelId);
            if (auto* tog = canvas.GetElement(id)) {
                tog->anchor.anchorMin = Math::Vector2(0.72f, 0.0f);
                tog->anchor.anchorMax = Math::Vector2(0.95f, 0.0f);
                tog->anchor.offsetLeft = 0.0f; tog->anchor.offsetRight = 0.0f;
                tog->anchor.offsetTop = top; tog->anchor.offsetBottom = top + 30.0f;
                tog->data.checked = checked;
                tog->tabOrder = tabOrder++;
                tog->onValueChangedEvent = ev;
            }
        };
        auto addSlider = [&](const char* label, f32 top, const char* ev,
                             f32 mn, f32 mx, f32 val) {
            addRowLabel(label, top);
            u32 id = canvas.AddElement(GUI::UIWidgetType::Slider, label, panelId);
            if (auto* sl = canvas.GetElement(id)) {
                sl->anchor.anchorMin = Math::Vector2(0.55f, 0.0f);
                sl->anchor.anchorMax = Math::Vector2(0.95f, 0.0f);
                sl->anchor.offsetLeft = 0.0f; sl->anchor.offsetRight = 0.0f;
                sl->anchor.offsetTop = top; sl->anchor.offsetBottom = top + 30.0f;
                sl->data.sliderMin = mn;
                sl->data.sliderMax = mx;
                sl->data.sliderValue = val;
                sl->tabOrder = tabOrder++;
                sl->onValueChangedEvent = ev;
            }
        };
        auto addButton = [&](const char* text, f32 top, const char* ev) {
            u32 id = canvas.AddElement(GUI::UIWidgetType::Button, text, panelId);
            if (auto* btn = canvas.GetElement(id)) {
                btn->anchor.anchorMin = Math::Vector2(0.15f, 0.0f);
                btn->anchor.anchorMax = Math::Vector2(0.85f, 0.0f);
                btn->anchor.offsetLeft = 0.0f; btn->anchor.offsetRight = 0.0f;
                btn->anchor.offsetTop = top; btn->anchor.offsetBottom = top + 42.0f;
                btn->data.text = text;
                btn->tabOrder = tabOrder++;
                btn->onClickEvent = ev;
                btn->style.bgColor = Math::Vector3(0.2f, 0.5f, 0.8f);
                btn->style.bgAlpha = 1.0f;
                btn->style.borderRadius = 6.0f;
            }
        };

        addSlider("Colorblind Mode",   80.0f, "a11y_cb_mode",       0.0f, 8.0f, 0.0f);
        addSlider("Filter Strength",  132.0f, "a11y_cb_strength",   0.0f, 1.0f, 1.0f);
        addSlider("Font Scale",       184.0f, "a11y_font_scale",    0.75f, 2.5f, 1.0f);
        addToggle("Dyslexia Text",    236.0f, "a11y_dyslexia",      false);
        addToggle("Screen Reader",    288.0f, "a11y_reader",        false);
        addButton("Speak Test Announcement", 340.0f, "a11y_reader_test");
        addToggle("Subtitles",        400.0f, "a11y_subtitles",     true);
        addSlider("Subtitle Size",    452.0f, "a11y_subtitle_size", 16.0f, 48.0f, 24.0f);
        addToggle("Reduced Motion",   504.0f, "a11y_reduced_motion", false);
        addSlider("Brightness",       556.0f, "a11y_brightness",    -0.5f, 0.5f, 0.0f);
        addSlider("Contrast",         608.0f, "a11y_contrast",      0.5f, 2.0f, 1.0f);
        addButton("Save Settings",    668.0f, "a11y_save");

        canvas.theme.focusBorderWidth = 3.0f;
        canvas.theme.inputFocused = Math::Vector3(0.3f, 0.7f, 1.0f);
    }

    // Write the live-settings script next to the project
    {
        namespace fs = std::filesystem;
        fs::path projRoot;
        if (!m_SceneManager.GetProjectPath().empty()) {
            projRoot = fs::path(m_SceneManager.GetProjectPath()).parent_path();
        }
        std::error_code ec;
        fs::create_directories(projRoot / "scripts", ec);
    {
        std::ofstream sf(projRoot / "scripts" / "AccessibilityDemo.as");
        if (sf.is_open()) {
            sf <<
"// AccessibilityDemo.as — applies UI control changes to the engine LIVE.\n"
"// Works identically in the editor, exported desktop games, and the browser\n"
"// (where Announcer_Announce is spoken aloud via the Web Speech API).\n"
"class AccessibilityDemo : TegeBehavior {\n"
"    array<string> modeNames = {\"Off\", \"Protanopia\", \"Deuteranopia\", \"Tritanopia\",\n"
"                               \"Protanomaly\", \"Deuteranomaly\", \"Tritanomaly\",\n"
"                               \"Achromatopsia\", \"Achromatomaly\"};\n"
"    int lastMode = 0;\n"
"\n"
"    void OnStart() {\n"
"        Events_Listen(\"a11y_cb_mode\", EventCallback(this.OnColorblindMode));\n"
"        Events_Listen(\"a11y_cb_strength\", EventCallback(this.OnColorblindStrength));\n"
"        Events_Listen(\"a11y_font_scale\", EventCallback(this.OnFontScale));\n"
"        Events_Listen(\"a11y_dyslexia\", EventCallback(this.OnDyslexia));\n"
"        Events_Listen(\"a11y_reader\", EventCallback(this.OnScreenReader));\n"
"        Events_Listen(\"a11y_reader_test\", EventCallback(this.OnReaderTest));\n"
"        Events_Listen(\"a11y_subtitles\", EventCallback(this.OnSubtitles));\n"
"        Events_Listen(\"a11y_subtitle_size\", EventCallback(this.OnSubtitleSize));\n"
"        Events_Listen(\"a11y_reduced_motion\", EventCallback(this.OnReducedMotion));\n"
"        Events_Listen(\"a11y_brightness\", EventCallback(this.OnBrightness));\n"
"        Events_Listen(\"a11y_contrast\", EventCallback(this.OnContrast));\n"
"        Events_Listen(\"a11y_save\", EventCallback(this.OnSave));\n"
"\n"
"        // Sync every control to the ACTUAL loaded settings (accessibility.json\n"
"        // etc.) — without this the toggles draw their design-time defaults and\n"
"        // lie about the real state (e.g. Subtitles shown ON while disabled).\n"
"        uint64 menu = Scene_FindEntity(\"Accessibility Menu\");\n"
"        if (menu != 0) {\n"
"            UI_SetSliderValue(menu, 4, float(Colorblind_GetMode()));\n"
"            UI_SetSliderValue(menu, 6, Colorblind_GetStrength());\n"
"            UI_SetSliderValue(menu, 8, Accessibility_GetFontScale());\n"
"            UI_SetChecked(menu, 10, Accessibility_GetDyslexiaFont());\n"
"            UI_SetChecked(menu, 12, Announcer_IsEnabled());\n"
"            UI_SetChecked(menu, 15, Subtitle_IsEnabled());\n"
"            UI_SetSliderValue(menu, 17, Subtitle_GetFontSize());\n"
"            UI_SetChecked(menu, 19, Accessibility_GetReducedMotion());\n"
"            UI_SetSliderValue(menu, 21, Accessibility_GetBrightness());\n"
"            UI_SetSliderValue(menu, 23, Accessibility_GetContrast());\n"
"            lastMode = Colorblind_GetMode();\n"
"        }\n"
"        Subtitle_Show(\"Accessibility demo ready. Navigate with Tab, arrows, or gamepad.\", \"\", 4.0f);\n"
"    }\n"
"\n"
"    void OnColorblindMode(const string &in ev) {\n"
"        int mode = int(Events_CurrentFloat(\"value\") + 0.5f);\n"
"        if (mode < 0) mode = 0;\n"
"        if (mode > 8) mode = 8;\n"
"        Colorblind_SetMode(mode);\n"
"        if (mode != lastMode) {\n"
"            lastMode = mode;\n"
"            Subtitle_Show(\"Colorblind mode: \" + modeNames[mode], \"\", 2.0f);\n"
"            Announcer_Announce(\"Colorblind mode \" + modeNames[mode]);\n"
"        }\n"
"    }\n"
"\n"
"    void OnColorblindStrength(const string &in ev) {\n"
"        Colorblind_SetStrength(Events_CurrentFloat(\"value\"));\n"
"    }\n"
"\n"
"    void OnFontScale(const string &in ev) {\n"
"        Accessibility_SetFontScale(Events_CurrentFloat(\"value\"));\n"
"    }\n"
"\n"
"    void OnDyslexia(const string &in ev) {\n"
"        bool on = Events_CurrentInt(\"checked\") != 0;\n"
"        Accessibility_SetDyslexiaFont(on);\n"
"        Subtitle_Show(on ? \"Dyslexia-friendly text on\" : \"Dyslexia-friendly text off\", \"\", 2.0f);\n"
"    }\n"
"\n"
"    void OnScreenReader(const string &in ev) {\n"
"        bool on = Events_CurrentInt(\"checked\") != 0;\n"
"        Announcer_SetEnabled(on);\n"
"        if (on) Announcer_Announce(\"Screen reader enabled\");\n"
"        Subtitle_Show(on ? \"Screen reader on\" : \"Screen reader off\", \"\", 2.0f);\n"
"    }\n"
"\n"
"    void OnReaderTest(const string &in ev) {\n"
"        Announcer_AnnounceHighPriority(\"This is a test announcement. In the browser this is spoken aloud.\");\n"
"    }\n"
"\n"
"    void OnSubtitles(const string &in ev) {\n"
"        bool on = Events_CurrentInt(\"checked\") != 0;\n"
"        Subtitle_SetEnabled(on);\n"
"        if (on) Subtitle_Show(\"Subtitles enabled\", \"\", 2.0f);\n"
"    }\n"
"\n"
"    void OnSubtitleSize(const string &in ev) {\n"
"        Subtitle_SetFontSize(Events_CurrentFloat(\"value\"));\n"
"        Subtitle_Show(\"Subtitle size \" + int(Events_CurrentFloat(\"value\")), \"\", 1.5f);\n"
"    }\n"
"\n"
"    void OnReducedMotion(const string &in ev) {\n"
"        bool on = Events_CurrentInt(\"checked\") != 0;\n"
"        Accessibility_SetReducedMotion(on);\n"
"        Subtitle_Show(on ? \"Reduced motion on\" : \"Reduced motion off\", \"\", 2.0f);\n"
"    }\n"
"\n"
"    void OnBrightness(const string &in ev) {\n"
"        Accessibility_SetBrightness(Events_CurrentFloat(\"value\"));\n"
"    }\n"
"\n"
"    void OnContrast(const string &in ev) {\n"
"        Accessibility_SetContrast(Events_CurrentFloat(\"value\"));\n"
"    }\n"
"\n"
"    void OnSave(const string &in ev) {\n"
"        Accessibility_SaveSettings();\n"
"        Subtitle_Show(\"Settings saved\", \"\", 2.0f);\n"
"        Announcer_Announce(\"Settings saved\");\n"
"    }\n"
"}\n";
            sf.close();
        }
    }
    }
}

} // namespace Editor
} // namespace Enjin
