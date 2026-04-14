#include "Enjin/Editor/EditorLayer.h"
#include "Enjin/Editor/EditorTheme.h"
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
#include "Enjin/Renderer/RayTracing/ReSTIR.h"
#include "Enjin/Renderer/RayTracing/RadianceCache.h"
#include "Enjin/Renderer/RayTracing/SurfelRadianceCache.h"
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

void EditorLayer::DrawConsolePanel() {
    bool panelOpen = true;
    ImGui::Begin("Console", &panelOpen);
    if (!panelOpen) {
        SetPanelVisibility(EditorPanel::Console, false);
    }

    // ── Feed tabs ──
    const char* feedLabels[] = { "All", "Editor", "Runtime" };
    for (int i = 0; i < 3; ++i) {
        if (i > 0) ImGui::SameLine();
        bool selected = (m_ConsoleFeedTab == i);
        if (selected) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
        if (ImGui::SmallButton(feedLabels[i])) m_ConsoleFeedTab = i;
        if (selected) ImGui::PopStyleColor();
    }

    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();

    // ── Level filter toggles (colored) ──
    auto toggleButton = [](const char* label, bool& active, const ImVec4& color) {
        if (active) ImGui::PushStyleColor(ImGuiCol_Button, color);
        else        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
        if (ImGui::SmallButton(label)) active = !active;
        ImGui::PopStyleColor();
    };

    toggleButton("Info",  m_ConsoleShowInfo,  ImVec4(0.20f, 0.40f, 0.60f, 1.0f));
    ImGui::SameLine();
    toggleButton("Warn",  m_ConsoleShowWarn,  ImVec4(0.60f, 0.50f, 0.10f, 1.0f));
    ImGui::SameLine();
    toggleButton("Error", m_ConsoleShowError, ImVec4(0.65f, 0.15f, 0.15f, 1.0f));

    // ── Console output ──
    ImGui::BeginChild("ConsoleOutput", ImVec2(0, -ImGui::GetFrameHeightWithSpacing()), true);

    // Classify categories into editor vs runtime feeds
    // Editor: Editor, Asset, Assets, Build, Core
    // Runtime: Game, Script, Physics, Audio, AI, Animation, Network, Renderer, Player, Procedural
    auto isEditorCategory = [](LogCategory cat) -> bool {
        return cat == LogCategory::Editor || cat == LogCategory::Asset ||
               cat == LogCategory::Assets || cat == LogCategory::Build ||
               cat == LogCategory::Core;
    };

    // Ctrl+C: copy all selected entries to clipboard
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows) &&
        ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_C) &&
        !m_ConsoleSelectedIndices.empty()) {
        std::string combined;
        for (int i = 0; i < static_cast<int>(m_ConsoleLog.size()); ++i) {
            if (m_ConsoleSelectedIndices.count(i)) {
                if (!combined.empty()) combined += '\n';
                combined += m_ConsoleLog[i].message;
            }
        }
        ImGui::SetClipboardText(combined.c_str());
    }

    bool anyVisible = false;
    for (int idx = 0; idx < static_cast<int>(m_ConsoleLog.size()); ++idx) {
        const auto& entry = m_ConsoleLog[idx];
        // Level filter
        if (entry.level <= LogLevel::Info  && !m_ConsoleShowInfo)  continue;
        if (entry.level == LogLevel::Warn  && !m_ConsoleShowWarn)  continue;
        if (entry.level >= LogLevel::Error && !m_ConsoleShowError) continue;

        // Feed filter
        if (m_ConsoleFeedTab == 1 && !isEditorCategory(entry.category)) continue;
        if (m_ConsoleFeedTab == 2 &&  isEditorCategory(entry.category)) continue;

        // Color by level
        ImVec4 color;
        if (entry.level >= LogLevel::Error) {
            color = ImVec4(1.0f, 0.35f, 0.35f, 1.0f);  // Red
        } else if (entry.level == LogLevel::Warn) {
            color = ImVec4(1.0f, 0.85f, 0.30f, 1.0f);  // Yellow
        } else {
            color = ImVec4(0.85f, 0.85f, 0.85f, 1.0f);  // Light gray
        }

        bool isSelected = m_ConsoleSelectedIndices.count(idx) > 0;
        ImGui::PushStyleColor(ImGuiCol_Text, color);
        ImGui::PushID(idx);
        if (ImGui::Selectable(entry.message.c_str(), isSelected)) {
            if (ImGui::GetIO().KeyShift) {
                // Shift+Click: toggle selection
                if (isSelected)
                    m_ConsoleSelectedIndices.erase(idx);
                else
                    m_ConsoleSelectedIndices.insert(idx);
            } else {
                // Plain click: select only this entry
                m_ConsoleSelectedIndices.clear();
                m_ConsoleSelectedIndices.insert(idx);
            }
        }
        ImGui::PopID();
        ImGui::PopStyleColor();
        anyVisible = true;
    }

    if (!anyVisible) {
        DrawEmptyState(">>", "Console Empty", "Messages will appear here");
    }

    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
        ImGui::SetScrollHereY(1.0f);
    }
    ImGui::EndChild();

    // ── Input line ──
    static char inputBuf[256] = "";
    if (ImGui::InputText("##ConsoleInput", inputBuf, sizeof(inputBuf),
                         ImGuiInputTextFlags_EnterReturnsTrue)) {
        if (inputBuf[0] != '\0') {
            m_ConsoleLog.push_back(std::string("> ") + inputBuf);
            ExecuteConsoleCommand(std::string(inputBuf));
            inputBuf[0] = '\0';
        }
        ImGui::SetKeyboardFocusHere(-1);
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear")) {
        m_ConsoleLog.clear();
        m_ConsoleSelectedIndices.clear();
    }

    ImGui::End();
}

void EditorLayer::RefreshAssetBrowserCache() {
    m_AssetBrowserCache.clear();
    m_AssetBrowserCachedPath = m_AssetBrowserPath;
    m_AssetBrowserCacheDirty = false;

    try {
        namespace fs = std::filesystem;
        fs::path browsePath(m_AssetBrowserPath);
        if (!fs::exists(browsePath) || !fs::is_directory(browsePath)) return;

        // Directories first
        for (const auto& entry : fs::directory_iterator(browsePath)) {
            if (entry.is_directory()) {
                AssetEntry ae;
                ae.name = entry.path().filename().string();
                ae.fullPath = entry.path().string();
                ae.isDirectory = true;
                m_AssetBrowserCache.push_back(std::move(ae));
            }
        }

        // Sort directories alphabetically
        std::sort(m_AssetBrowserCache.begin(), m_AssetBrowserCache.end(),
            [](const AssetEntry& a, const AssetEntry& b) { return a.name < b.name; });

        usize dirCount = m_AssetBrowserCache.size();

        // Then files
        for (const auto& entry : fs::directory_iterator(browsePath)) {
            if (!entry.is_regular_file()) continue;
            AssetEntry ae;
            ae.name = entry.path().filename().string();
            ae.fullPath = entry.path().string();
            ae.extension = entry.path().extension().string();
            ae.isDirectory = false;
            std::error_code ec;
            ae.fileSize = static_cast<u64>(fs::file_size(entry.path(), ec));
            m_AssetBrowserCache.push_back(std::move(ae));
        }

        // Sort files alphabetically (after directories)
        std::sort(m_AssetBrowserCache.begin() + dirCount, m_AssetBrowserCache.end(),
            [](const AssetEntry& a, const AssetEntry& b) { return a.name < b.name; });

    } catch (const std::exception& e) {
        ENJIN_LOG_WARN(Editor, "Error scanning directory '%s': %s", m_AssetBrowserPath.c_str(), e.what());
    }
}

void EditorLayer::DrawAssetBrowserPanel() {
    bool panelOpen = true;
    ImGui::Begin("Asset Browser", &panelOpen);
    if (!panelOpen) {
        SetPanelVisibility(EditorPanel::AssetBrowser, false);
    }

    // Initialize browse path to current working directory
    if (m_AssetBrowserPath.empty()) {
        m_AssetBrowserPath = ".";
        m_AssetBrowserCacheDirty = true;
    }

    // Refresh cache if path changed or flagged dirty
    if (m_AssetBrowserCacheDirty || m_AssetBrowserCachedPath != m_AssetBrowserPath) {
        RefreshAssetBrowserCache();
    }

    // --- Toolbar ---
    // Navigation
    if (ImGui::Button("Up")) {
        auto pos = m_AssetBrowserPath.find_last_of("/\\");
        if (pos != std::string::npos && pos > 0) {
            m_AssetBrowserPath = m_AssetBrowserPath.substr(0, pos);
        }
        m_AssetBrowserCacheDirty = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Refresh")) {
        m_AssetBrowserCacheDirty = true;
        m_AssetBrowserSelected.clear();
    }
    ImGui::SameLine();

    // View toggle
    if (ImGui::Button(m_AssetGridView ? "List" : "Grid")) {
        m_AssetGridView = !m_AssetGridView;
    }
    ImGui::SameLine();

    // Import buttons
    if (ImGui::Button("Import...")) {
        std::vector<FileFilter> filters = {
            { "3D Models", "*.gltf;*.glb;*.fbx;*.obj;*.dae;*.3ds" },
            { "All Files", "*.*" }
        };
        auto projRoot = std::filesystem::path(m_SceneManager.GetProjectPath()).parent_path().string();
        std::string path = FileDialog::OpenFile("Import Model", filters, projRoot);
        if (!path.empty()) {
            ImportModel(path);
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Open Scene...")) {
        std::vector<FileFilter> filters = {
            { "Enjin Scene", "*.enjin;*.json" },
            { "All Files", "*.*" }
        };
        auto sceneDir = m_CurrentScenePath.empty()
            ? std::filesystem::path(m_SceneManager.GetProjectPath()).parent_path().string()
            : std::filesystem::path(m_CurrentScenePath).parent_path().string();
        std::string path = FileDialog::OpenFile("Open Scene", filters, sceneDir);
        if (!path.empty()) {
            OpenScene(path);
        }
    }

    // Path display
    ImGui::TextDisabled("%s", m_AssetBrowserPath.c_str());

    // Search bar
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##AssetSearch", "Search...", m_AssetSearchBuf, sizeof(m_AssetSearchBuf));
    std::string searchFilter(m_AssetSearchBuf);
    // Lowercase for case-insensitive matching
    std::string searchLower = searchFilter;
    for (auto& ch : searchLower) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));

    if (m_AssetGridView) {
        // Thumbnail size slider
        ImGui::SetNextItemWidth(100);
        ImGui::SliderFloat("##ThumbSize", &m_AssetThumbnailSize, 48.0f, 160.0f, "%.0f px");
    }

    ImGui::Separator();

    // --- File listing ---
    ImGui::BeginChild("FileList", ImVec2(0, 0), true);

    namespace fs = std::filesystem;
    fs::path browsePath(m_AssetBrowserPath);

    if (!fs::exists(browsePath) || !fs::is_directory(browsePath)) {
        DrawEmptyState("?", "Directory Not Found", "The current path does not exist",
            "Reset to Project Root", [this]() {
                m_AssetBrowserPath = ".";
                m_AssetBrowserCacheDirty = true;
            });
        ImGui::EndChild();
        ImGui::End();
        return;
    }

    // Helper lambdas for file type classification
    auto IsImage = [](const std::string& ext) {
        return ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga" || ext == ".bmp" || ext == ".svg";
    };
    auto IsModel = [](const std::string& ext) {
        return ext == ".gltf" || ext == ".glb" || ext == ".fbx" || ext == ".obj" || ext == ".dae" || ext == ".3ds";
    };
    auto IsScene = [](const std::string& ext) {
        return ext == ".enjin" || ext == ".json";
    };
    auto IsShader = [](const std::string& ext) {
        return ext == ".vert" || ext == ".frag" || ext == ".glsl" || ext == ".spv" || ext == ".comp";
    };
    auto IsScript = [](const std::string& ext) {
        return ext == ".as" || ext == ".angelscript";
    };
    auto IsAudio = [](const std::string& ext) {
        return ext == ".wav" || ext == ".mp3" || ext == ".ogg" || ext == ".flac";
    };
    auto IsPrefab = [](const std::string& ext) {
        return ext == ".enjprefab";
    };

    auto GetTypeColor = [&](const std::string& ext) -> ImVec4 {
        if (IsModel(ext))  return ImVec4(0.4f, 0.8f, 1.0f, 1.0f);   // Cyan
        if (IsScene(ext))  return ImVec4(0.4f, 1.0f, 0.4f, 1.0f);   // Green
        if (IsShader(ext)) return ImVec4(1.0f, 0.8f, 0.4f, 1.0f);   // Gold
        if (IsImage(ext))  return ImVec4(1.0f, 0.6f, 1.0f, 1.0f);   // Pink
        if (IsScript(ext)) return ImVec4(0.6f, 0.8f, 1.0f, 1.0f);   // Light blue
        if (IsAudio(ext))  return ImVec4(0.5f, 1.0f, 0.8f, 1.0f);   // Teal
        if (IsPrefab(ext)) return ImVec4(1.0f, 0.9f, 0.5f, 1.0f);   // Yellow
        return ImVec4(0.7f, 0.7f, 0.7f, 1.0f);                      // Gray
    };

    auto GetTypeLabel = [&](const std::string& ext) -> const char* {
        if (IsModel(ext))  return "3D";
        if (IsScene(ext))  return "SCN";
        if (IsShader(ext)) return "SHD";
        if (IsImage(ext))  return "IMG";
        if (IsScript(ext)) return "AS";
        if (IsAudio(ext))  return "SFX";
        if (IsPrefab(ext)) return "PFB";
        return "";
    };

    auto FormatFileSize = [](u64 bytes) -> std::string {
        char buf[32];
        if (bytes < 1024) snprintf(buf, sizeof(buf), "%llu B", (unsigned long long)bytes);
        else if (bytes < 1024 * 1024) snprintf(buf, sizeof(buf), "%llu KB", (unsigned long long)(bytes / 1024));
        else snprintf(buf, sizeof(buf), "%llu MB", (unsigned long long)(bytes / (1024 * 1024)));
        return buf;
    };

    if (m_AssetGridView) {
        // === Grid view with thumbnails ===
        f32 thumbSize = m_AssetThumbnailSize;
        f32 cellSize = thumbSize + 16.0f; // padding
        f32 panelWidth = ImGui::GetContentRegionAvail().x;
        int columns = std::max(1, static_cast<int>(panelWidth / cellSize));

        int col = 0;
        for (const auto& entry : m_AssetBrowserCache) {
            // Search filter
            if (!searchLower.empty()) {
                std::string nameLower = entry.name;
                for (auto& ch : nameLower) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
                if (nameLower.find(searchLower) == std::string::npos) continue;
            }

            ImGui::PushID(entry.fullPath.c_str());

            if (col > 0) ImGui::SameLine();

            ImGui::BeginGroup();

            bool selected = (m_AssetBrowserSelected == entry.fullPath);

            if (entry.isDirectory) {
                // Directory: colored box with folder label
                ImVec2 cursor = ImGui::GetCursorScreenPos();
                ImDrawList* dl = ImGui::GetWindowDrawList();

                if (selected) {
                    dl->AddRectFilled(cursor, ImVec2(cursor.x + thumbSize, cursor.y + thumbSize),
                        IM_COL32(60, 80, 60, 255), 4.0f);
                } else {
                    dl->AddRectFilled(cursor, ImVec2(cursor.x + thumbSize, cursor.y + thumbSize),
                        IM_COL32(40, 45, 50, 255), 4.0f);
                }

                // Folder icon (simple triangle + rect)
                f32 cx = cursor.x + thumbSize * 0.5f;
                f32 cy = cursor.y + thumbSize * 0.4f;
                f32 fw = thumbSize * 0.4f;
                f32 fh = thumbSize * 0.3f;
                dl->AddRectFilled(ImVec2(cx - fw, cy - fh * 0.3f), ImVec2(cx + fw, cy + fh),
                    IM_COL32(180, 160, 80, 200), 3.0f);
                dl->AddRectFilled(ImVec2(cx - fw, cy - fh * 0.3f), ImVec2(cx - fw * 0.2f, cy - fh * 0.1f),
                    IM_COL32(200, 180, 90, 200), 2.0f);

                if (ImGui::InvisibleButton("##dir", ImVec2(thumbSize, thumbSize))) {
                    m_AssetBrowserSelected = entry.fullPath;
                }
                if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
                    m_AssetBrowserPath = entry.fullPath;
                    m_AssetBrowserCacheDirty = true;
                }
            } else {
                // File: show thumbnail for images, type label for others
                bool isImg = IsImage(entry.extension);
                bool drewThumb = false;

                if (isImg) {
                    VkDescriptorSet texId = GetImGuiTexture(entry.fullPath);
                    if (texId != VK_NULL_HANDLE) {
                        ImVec2 cursor = ImGui::GetCursorScreenPos();
                        if (selected) {
                            ImDrawList* dl = ImGui::GetWindowDrawList();
                            dl->AddRectFilled(
                                ImVec2(cursor.x - 2, cursor.y - 2),
                                ImVec2(cursor.x + thumbSize + 2, cursor.y + thumbSize + 2),
                                IM_COL32(100, 140, 100, 255), 4.0f);
                        }
                        ImGui::Image(static_cast<ImTextureID>(reinterpret_cast<uintptr_t>(texId)),
                                     ImVec2(thumbSize, thumbSize));
                        drewThumb = true;

                        // Compression status badge (bottom-right corner)
                        {
                            ImVec2 imgMin = ImGui::GetItemRectMin();
                            ImVec2 imgMax = ImGui::GetItemRectMax();
                            ImDrawList* dl = ImGui::GetWindowDrawList();
                            const char* badge = "RAW";
                            ImU32 badgeCol = IM_COL32(200, 140, 40, 200);
                            ImU32 badgeBg = IM_COL32(40, 30, 10, 180);
                            ImVec2 badgeSize = ImGui::CalcTextSize(badge);
                            f32 px = 3.0f, py = 1.0f;
                            ImVec2 badgeMin(imgMax.x - badgeSize.x - px * 2, imgMax.y - badgeSize.y - py * 2);
                            dl->AddRectFilled(badgeMin, imgMax, badgeBg, 3.0f);
                            dl->AddText(ImVec2(badgeMin.x + px, badgeMin.y + py), badgeCol, badge);
                        }

                        // Hover tooltip: larger preview
                        if (ImGui::IsItemHovered()) {
                            ImGui::BeginTooltip();
                            f32 previewSize = 256.0f;
                            ImGui::Image(static_cast<ImTextureID>(reinterpret_cast<uintptr_t>(texId)),
                                         ImVec2(previewSize, previewSize));
                            ImGui::Text("%s", entry.name.c_str());
                            ImGui::TextDisabled("%s", FormatFileSize(entry.fileSize).c_str());
                            ImGui::TextDisabled("Right-click to compress");
                            ImGui::EndTooltip();
                        }
                    }
                }

                if (!drewThumb) {
                    // Try generated thumbnail for 3D models and other assets
                    VkDescriptorSet thumbId = GetAssetThumbnail(entry.fullPath);
                    if (thumbId != VK_NULL_HANDLE) {
                        ImVec2 cursor = ImGui::GetCursorScreenPos();
                        if (selected) {
                            ImDrawList* dl = ImGui::GetWindowDrawList();
                            dl->AddRectFilled(
                                ImVec2(cursor.x - 2, cursor.y - 2),
                                ImVec2(cursor.x + thumbSize + 2, cursor.y + thumbSize + 2),
                                IM_COL32(100, 140, 100, 255), 4.0f);
                        }
                        ImGui::Image(static_cast<ImTextureID>(reinterpret_cast<uintptr_t>(thumbId)),
                                     ImVec2(thumbSize, thumbSize));
                        drewThumb = true;

                        if (ImGui::IsItemHovered()) {
                            ImGui::BeginTooltip();
                            f32 previewSize = 256.0f;
                            ImGui::Image(static_cast<ImTextureID>(reinterpret_cast<uintptr_t>(thumbId)),
                                         ImVec2(previewSize, previewSize));
                            ImGui::Text("%s", entry.name.c_str());
                            ImGui::TextDisabled("%s", FormatFileSize(entry.fileSize).c_str());
                            ImGui::EndTooltip();
                        }
                    }
                }

                if (!drewThumb) {
                    // Non-image: colored rectangle with type label
                    ImVec2 cursor = ImGui::GetCursorScreenPos();
                    ImDrawList* dl = ImGui::GetWindowDrawList();
                    ImVec4 typeCol = GetTypeColor(entry.extension);
                    ImU32 bgCol = selected
                        ? IM_COL32((int)(typeCol.x * 80), (int)(typeCol.y * 80), (int)(typeCol.z * 80), 255)
                        : IM_COL32(35, 35, 40, 255);
                    dl->AddRectFilled(cursor, ImVec2(cursor.x + thumbSize, cursor.y + thumbSize),
                        bgCol, 4.0f);
                    dl->AddRect(cursor, ImVec2(cursor.x + thumbSize, cursor.y + thumbSize),
                        IM_COL32((int)(typeCol.x * 200), (int)(typeCol.y * 200), (int)(typeCol.z * 200), 150), 4.0f);

                    const char* label = GetTypeLabel(entry.extension);
                    if (label[0]) {
                        ImVec2 textSize = ImGui::CalcTextSize(label);
                        dl->AddText(
                            ImVec2(cursor.x + (thumbSize - textSize.x) * 0.5f,
                                   cursor.y + (thumbSize - textSize.y) * 0.5f),
                            IM_COL32((int)(typeCol.x * 255), (int)(typeCol.y * 255), (int)(typeCol.z * 255), 220),
                            label);
                    }
                }

                if (!drewThumb) {
                    if (ImGui::InvisibleButton("##file", ImVec2(thumbSize, thumbSize))) {
                        m_AssetBrowserSelected = entry.fullPath;
                    }
                } else {
                    if (ImGui::IsItemClicked()) {
                        m_AssetBrowserSelected = entry.fullPath;
                    }
                }

                // Double-click to import/open
                if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
                    if (IsModel(entry.extension)) {
                        ImportModel(entry.fullPath);
                    } else if (IsScene(entry.extension)) {
                        OpenScene(entry.fullPath);
                    }
                }

                // Hover tooltip for non-image files
                if (!isImg && ImGui::IsItemHovered()) {
                    ImGui::BeginTooltip();
                    ImGui::Text("%s", entry.name.c_str());
                    ImGui::TextDisabled("%s  |  %s", entry.extension.c_str(), FormatFileSize(entry.fileSize).c_str());
                    ImGui::EndTooltip();
                }

                // Drag source for drag-to-viewport / drag-to-inspector
                if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
                    ImGui::SetDragDropPayload("ASSET_PATH", entry.fullPath.c_str(), entry.fullPath.size() + 1);
                    // Show preview tooltip during drag for image files
                    if (IsImage(entry.extension)) {
                        VkDescriptorSet dragTexId = GetImGuiTexture(entry.fullPath);
                        if (dragTexId != VK_NULL_HANDLE) {
                            ImGui::Image(static_cast<ImTextureID>(reinterpret_cast<uintptr_t>(dragTexId)),
                                         ImVec2(64.0f, 64.0f));
                        }
                    }
                    ImGui::Text("%s", entry.name.c_str());
                    ImGui::EndDragDropSource();
                }

                // Right-click context menu
                if (ImGui::BeginPopupContextItem("##AssetCtxGrid")) {
                    if (IsImage(entry.extension)) {
                        if (ImGui::MenuItem("Compress Texture...")) {
                            m_ShowCompressionSettings = true;
                            m_CompressionTargetPath = entry.fullPath;
                            m_CompressionLastResult.clear();
                            // Recommend format based on extension
                            bool hasAlpha = (entry.extension == ".png" || entry.extension == ".tga");
                            m_TextureCompSettings.format = Assets::TextureCompressor::RecommendFormat(
                                4, hasAlpha, false, false);
                        }
                    }
                    if (IsModel(entry.extension)) {
                        if (ImGui::MenuItem("Import Model")) {
                            ImportModel(entry.fullPath);
                        }
                    }
                    if (IsScene(entry.extension)) {
                        if (ImGui::MenuItem("Open Scene")) {
                            OpenScene(entry.fullPath);
                        }
                    }
                    ImGui::EndPopup();
                }
            }

            // Filename label below thumbnail (truncated)
            ImVec2 textSize = ImGui::CalcTextSize(entry.name.c_str());
            if (textSize.x > thumbSize) {
                // Truncate with ellipsis
                std::string truncated = entry.name;
                while (truncated.size() > 3 && ImGui::CalcTextSize(truncated.c_str()).x > thumbSize) {
                    truncated.pop_back();
                }
                if (truncated.size() < entry.name.size()) {
                    // Remove last 2 chars and add ".."
                    if (truncated.size() > 2) truncated = truncated.substr(0, truncated.size() - 2) + "..";
                }
                ImGui::TextUnformatted(truncated.c_str());
            } else {
                ImGui::TextUnformatted(entry.name.c_str());
            }

            ImGui::EndGroup();

            col++;
            if (col >= columns) col = 0;

            ImGui::PopID();
        }
    } else {
        // === List view ===
        for (const auto& entry : m_AssetBrowserCache) {
            // Search filter
            if (!searchLower.empty()) {
                std::string nameLower = entry.name;
                for (auto& ch : nameLower) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
                if (nameLower.find(searchLower) == std::string::npos) continue;
            }

            if (entry.isDirectory) {
                std::string dirLabel = "[DIR] " + entry.name;
                if (ImGui::Selectable(dirLabel.c_str(), m_AssetBrowserSelected == entry.fullPath,
                                       ImGuiSelectableFlags_AllowDoubleClick)) {
                    m_AssetBrowserSelected = entry.fullPath;
                    if (ImGui::IsMouseDoubleClicked(0)) {
                        m_AssetBrowserPath = entry.fullPath;
                        m_AssetBrowserCacheDirty = true;
                    }
                }
            } else {
                ImVec4 typeCol = GetTypeColor(entry.extension);
                ImGui::PushStyleColor(ImGuiCol_Text, typeCol);

                bool selected = (m_AssetBrowserSelected == entry.fullPath);
                if (ImGui::Selectable(entry.name.c_str(), selected, ImGuiSelectableFlags_AllowDoubleClick)) {
                    m_AssetBrowserSelected = entry.fullPath;
                    if (ImGui::IsMouseDoubleClicked(0)) {
                        if (IsModel(entry.extension)) {
                            ImportModel(entry.fullPath);
                        } else if (IsScene(entry.extension)) {
                            OpenScene(entry.fullPath);
                        }
                    }
                }

                ImGui::PopStyleColor();

                // Size on same line
                ImGui::SameLine(ImGui::GetContentRegionAvail().x - 60);
                ImGui::TextDisabled("%s", FormatFileSize(entry.fileSize).c_str());

                // Hover tooltip with preview for images
                if (ImGui::IsItemHovered()) {
                    bool isImg = IsImage(entry.extension);
                    if (isImg) {
                        VkDescriptorSet texId = GetImGuiTexture(entry.fullPath);
                        if (texId != VK_NULL_HANDLE) {
                            ImGui::BeginTooltip();
                            ImGui::Image(static_cast<ImTextureID>(reinterpret_cast<uintptr_t>(texId)),
                                         ImVec2(200.0f, 200.0f));
                            ImGui::EndTooltip();
                        }
                    }
                }

                // Drag source
                if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
                    ImGui::SetDragDropPayload("ASSET_PATH", entry.fullPath.c_str(), entry.fullPath.size() + 1);
                    // Show preview tooltip during drag for image files
                    if (IsImage(entry.extension)) {
                        VkDescriptorSet dragTexId = GetImGuiTexture(entry.fullPath);
                        if (dragTexId != VK_NULL_HANDLE) {
                            ImGui::Image(static_cast<ImTextureID>(reinterpret_cast<uintptr_t>(dragTexId)),
                                         ImVec2(64.0f, 64.0f));
                        }
                    }
                    ImGui::Text("%s", entry.name.c_str());
                    ImGui::EndDragDropSource();
                }

                // Right-click context menu
                if (ImGui::BeginPopupContextItem("##AssetCtxList")) {
                    if (IsImage(entry.extension)) {
                        if (ImGui::MenuItem("Compress Texture...")) {
                            m_ShowCompressionSettings = true;
                            m_CompressionTargetPath = entry.fullPath;
                            m_CompressionLastResult.clear();
                            bool hasAlpha = (entry.extension == ".png" || entry.extension == ".tga");
                            m_TextureCompSettings.format = Assets::TextureCompressor::RecommendFormat(
                                4, hasAlpha, false, false);
                        }
                    }
                    if (IsModel(entry.extension)) {
                        if (ImGui::MenuItem("Import Model")) {
                            ImportModel(entry.fullPath);
                        }
                    }
                    if (IsScene(entry.extension)) {
                        if (ImGui::MenuItem("Open Scene")) {
                            OpenScene(entry.fullPath);
                        }
                    }
                    ImGui::EndPopup();
                }
            }
        }
    }

    ImGui::EndChild();

    // Draw compression settings window if open
    if (m_ShowCompressionSettings) {
        DrawTextureCompressionWindow();
    }

    ImGui::End();
}


void EditorLayer::DrawSceneListPanel() {
    bool panelOpen = true;
    ImGui::Begin("Scene List", &panelOpen, ImGuiWindowFlags_None);
    if (!panelOpen) {
        SetPanelVisibility(EditorPanel::SceneList, false);
    }

    // Project header
    ImGui::Text("Project: %s", m_SceneManager.GetProjectName().c_str());
    if (!m_SceneManager.GetProjectPath().empty()) {
        ImGui::TextDisabled("%s", m_SceneManager.GetProjectPath().c_str());
    } else {
        ImGui::TextDisabled("(No project file)");
    }
    ImGui::Separator();

    // Project name editing
    static char projectNameBuf[256] = {};
    if (projectNameBuf[0] == '\0') {
        std::strncpy(projectNameBuf, m_SceneManager.GetProjectName().c_str(), sizeof(projectNameBuf) - 1);
    }
    if (ImGui::InputText("Project Name", projectNameBuf, sizeof(projectNameBuf), ImGuiInputTextFlags_EnterReturnsTrue)) {
        m_SceneManager.SetProjectName(projectNameBuf);
    }
    ImGui::Separator();

    // Scene lock status
    if (!m_SceneLockManager.GetScenePath().empty()) {
        if (m_SceneLockManager.IsSceneLocked()) {
            bool byOther = m_SceneLockManager.IsSceneLockedByOther();
            if (byOther) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
                ImGui::Text("Scene locked by %s", m_SceneLockManager.GetSceneLockedBy().c_str());
                ImGui::PopStyleColor();
                if (m_SceneLockManager.IsSceneLockStale()) {
                    ImGui::SameLine();
                    ImGui::TextDisabled("(stale)");
                    ImGui::SameLine();
                    if (ImGui::SmallButton("Break Lock")) {
                        m_SceneLockManager.BreakSceneLock();
                        m_Announcer.Announce("Broke stale scene lock", Accessibility::AnnouncePriority::High);
                    }
                }
            } else {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.8f, 1.0f, 1.0f));
                ImGui::Text("Scene locked by you");
                ImGui::PopStyleColor();
                ImGui::SameLine();
                if (ImGui::SmallButton("Unlock Scene")) {
                    m_SceneLockManager.UnlockScene();
                    m_Announcer.Announce("Scene unlocked", Accessibility::AnnouncePriority::Normal);
                }
            }
        } else {
            if (ImGui::SmallButton("Lock Scene")) {
                m_SceneLockManager.LockScene();
                m_Announcer.Announce("Scene locked", Accessibility::AnnouncePriority::Normal);
            }
            ImGui::SetItemTooltip("Lock this scene to prevent edits by other users");
        }

        // Lock stats
        usize myLocks = m_SceneLockManager.GetMyEntityLockCount();
        usize otherLocks = m_SceneLockManager.GetOtherEntityLockCount();
        if (myLocks > 0 || otherLocks > 0) {
            ImGui::TextDisabled("Entity locks: %zu mine, %zu others", myLocks, otherLocks);
        }
        ImGui::Separator();
    }

    // Scene list header with add button
    ImGui::Text("Scenes (%zu)", m_SceneManager.GetSceneCount());
    ImGui::SameLine();
    if (ImGui::SmallButton("+ Add Current Scene")) {
        // Add current scene to the project
        std::string sceneName = "Unnamed Scene";
        if (!m_CurrentScenePath.empty()) {
            sceneName = std::filesystem::path(m_CurrentScenePath).stem().string();
        }
        std::string scenePath = m_CurrentScenePath;
        if (scenePath.empty()) {
            // Prompt to save first
            std::vector<FileFilter> filters = {
                { "Enjin Scene", "*.enjin" },
                { "All Files", "*.*" }
            };
            scenePath = FileDialog::SaveFile("Save Scene to Add", filters, "", "scene.enjin");
            if (!scenePath.empty()) {
                SaveScene(scenePath);
                sceneName = std::filesystem::path(scenePath).stem().string();
            }
        }
        if (!scenePath.empty()) {
            // Use path relative to project root if possible
            std::string relativePath = scenePath;
            std::string projectRoot = m_SceneManager.GetProjectPath().empty() ? "" :
                std::filesystem::path(m_SceneManager.GetProjectPath()).parent_path().string();
            if (!projectRoot.empty()) {
                std::filesystem::path absScene = std::filesystem::absolute(scenePath);
                std::filesystem::path absRoot = std::filesystem::absolute(projectRoot);
                auto rel = std::filesystem::relative(absScene, absRoot);
                if (!rel.empty() && rel.string().find("..") == std::string::npos) {
                    relativePath = rel.string();
                }
            }
            m_SceneManager.AddScene(sceneName, relativePath);
        }
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("+ Add Scene File...")) {
        std::vector<FileFilter> filters = {
            { "Enjin Scene", "*.enjin" },
            { "All Files", "*.*" }
        };
        std::string path = FileDialog::OpenFile("Add Scene to Project", filters);
        if (!path.empty()) {
            std::string name = std::filesystem::path(path).stem().string();
            std::string relativePath = path;
            std::string projectRoot = m_SceneManager.GetProjectPath().empty() ? "" :
                std::filesystem::path(m_SceneManager.GetProjectPath()).parent_path().string();
            if (!projectRoot.empty()) {
                std::filesystem::path absScene = std::filesystem::absolute(path);
                std::filesystem::path absRoot = std::filesystem::absolute(projectRoot);
                auto rel = std::filesystem::relative(absScene, absRoot);
                if (!rel.empty() && rel.string().find("..") == std::string::npos) {
                    relativePath = rel.string();
                }
            }
            m_SceneManager.AddScene(name, relativePath);
        }
    }

    ImGui::Separator();

    // Scene list
    auto& scenes = m_SceneManager.GetScenes();
    i32 removeIndex = -1;
    i32 moveFromIdx = -1;
    i32 moveToIdx = -1;
    i32 setStartIdx = -1;

    for (usize i = 0; i < scenes.size(); ++i) {
        auto& scene = scenes[i];
        ImGui::PushID(static_cast<int>(i));

        // Build index badge
        if (scene.isStartScene) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 1.0f, 0.2f, 1.0f));
            ImGui::Text("[Start]");
            ImGui::PopStyleColor();
        } else {
            ImGui::Text("[%d]", scene.buildIndex);
        }
        ImGui::SameLine();

        // Selectable scene name
        bool isCurrent = (m_SceneManager.GetCurrentSceneName() == scene.name);
        if (isCurrent) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.9f, 0.3f, 1.0f));
        }

        bool selected = false;
        if (ImGui::Selectable(scene.name.c_str(), &selected, ImGuiSelectableFlags_AllowDoubleClick)) {
            if (ImGui::IsMouseDoubleClicked(0)) {
                // Double click to load scene
                m_SceneManager.LoadScene(scene.name);
                ShowNotification("Scene loaded: " + scene.name, NotificationType::Success);
            }
        }
        if (isCurrent) {
            ImGui::PopStyleColor();
        }

        // Tooltip showing file path
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Path: %s\nBuild Index: %d\nDouble-click to load", scene.path.c_str(), scene.buildIndex);
        }

        // Context menu
        if (ImGui::BeginPopupContextItem("SceneContextMenu")) {
            if (ImGui::MenuItem("Load")) {
                m_SceneManager.LoadScene(scene.name);
                ShowNotification("Scene loaded: " + scene.name, NotificationType::Success);
            }
            if (ImGui::MenuItem("Load Additive")) {
                m_SceneManager.LoadSceneAdditive(scene.name);
                ShowNotification("Scene loaded (additive): " + scene.name, NotificationType::Success);
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Set as Start Scene")) {
                setStartIdx = static_cast<i32>(i);
            }
            ImGui::Separator();
            if (i > 0 && ImGui::MenuItem("Move Up")) {
                moveFromIdx = static_cast<i32>(i);
                moveToIdx = static_cast<i32>(i - 1);
            }
            if (i < scenes.size() - 1 && ImGui::MenuItem("Move Down")) {
                moveFromIdx = static_cast<i32>(i);
                moveToIdx = static_cast<i32>(i + 1);
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Remove from Project")) {
                removeIndex = static_cast<i32>(i);
            }
            ImGui::EndPopup();
        }

        ImGui::PopID();
    }

    // Apply deferred operations
    if (setStartIdx >= 0) {
        m_SceneManager.SetStartScene(static_cast<usize>(setStartIdx));
    }
    if (moveFromIdx >= 0 && moveToIdx >= 0) {
        m_SceneManager.MoveScene(static_cast<usize>(moveFromIdx), static_cast<usize>(moveToIdx));
    }
    if (removeIndex >= 0) {
        m_SceneManager.RemoveScene(static_cast<usize>(removeIndex));
    }

    ImGui::Separator();

    // Auto-assign build indices button
    if (ImGui::Button("Auto-Assign Build Indices")) {
        m_SceneManager.AutoAssignBuildIndices();
    }
    ImGui::SameLine();
    if (ImGui::Button("Save Project")) {
        if (m_SceneManager.GetProjectPath().empty()) {
            std::vector<FileFilter> filters = {
                { "Enjin Project", "*.enjinproject" },
                { "All Files", "*.*" }
            };
            std::string path = FileDialog::SaveFile("Save Project", filters, "", "project.enjinproject");
            if (!path.empty()) {
                if (!m_SceneManager.SaveProject(path)) {
                    ShowNotification("Failed to save project", NotificationType::Error);
                }
            }
        } else {
            if (!m_SceneManager.SaveProject()) {
                ShowNotification("Failed to save project", NotificationType::Error);
            }
        }
    }

    ImGui::Separator();

    // Scene transition controls
    ImGui::Text("Scene Transitions");
    static int transType = 0;
    ImGui::Combo("Transition", &transType, "Instant\0Fade Black\0Fade White\0Cross Fade\0");
    static float transDuration = 0.5f;
    ImGui::SliderFloat("Duration", &transDuration, 0.1f, 3.0f, "%.1f s");

    if (m_SceneManager.IsTransitioning()) {
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.3f, 1.0f), "Transitioning... (%.0f%%)",
            m_SceneManager.GetTransitionAlpha() * 100.0f);
    }

    // Quick load buttons for each scene
    if (scenes.size() > 0) {
        ImGui::Text("Quick Load:");
        for (usize i = 0; i < scenes.size(); ++i) {
            if (i > 0) ImGui::SameLine();
            ImGui::PushID(static_cast<int>(i) + 1000);
            if (ImGui::SmallButton(scenes[i].name.c_str())) {
                Scene::TransitionType tt = static_cast<Scene::TransitionType>(transType);
                m_SceneManager.LoadSceneWithTransition(scenes[i].name, tt, transDuration);
            }
            ImGui::PopID();
        }
    }

    ImGui::End();
}


void EditorLayer::ExecuteConsoleCommand(const std::string& command) {
    // Tokenize
    std::istringstream iss(command);
    std::string cmd;
    iss >> cmd;

    // Convert to lowercase for matching
    std::string cmdLower = cmd;
    for (auto& c : cmdLower) c = static_cast<char>(std::tolower(c));

    if (cmdLower == "help") {
        // Check if user asked for help on a specific command
        std::string helpTopic;
        iss >> helpTopic;
        for (auto& c : helpTopic) c = static_cast<char>(std::tolower(c));

        if (helpTopic.empty()) {
            m_ConsoleLog.push_back("=============================================================");
            m_ConsoleLog.push_back("  TEGE Console — Command Reference");
            m_ConsoleLog.push_back("  Type 'help <command>' for detailed usage");
            m_ConsoleLog.push_back("=============================================================");
            m_ConsoleLog.push_back("");
            m_ConsoleLog.push_back("--- GENERAL ---");
            m_ConsoleLog.push_back("  help [cmd]            Show this list or detailed help");
            m_ConsoleLog.push_back("  clear                 Clear console output");
            m_ConsoleLog.push_back("  stats                 Show scene statistics");
            m_ConsoleLog.push_back("  fps                   Show current FPS and frame time");
            m_ConsoleLog.push_back("  version               Show engine version");
            m_ConsoleLog.push_back("");
            m_ConsoleLog.push_back("--- ENTITIES ---");
            m_ConsoleLog.push_back("  list                  List all entities in the scene");
            m_ConsoleLog.push_back("  select <id>           Select entity by ID");
            m_ConsoleLog.push_back("  deselect              Clear selection");
            m_ConsoleLog.push_back("  create <name>         Create an empty entity");
            m_ConsoleLog.push_back("  delete                Delete selected entity(ies)");
            m_ConsoleLog.push_back("  inspect               Show selected entity components");
            m_ConsoleLog.push_back("");
            m_ConsoleLog.push_back("--- TRANSFORM ---");
            m_ConsoleLog.push_back("  pos <x> <y> <z>       Set selected entity position");
            m_ConsoleLog.push_back("  rot <x> <y> <z>       Set selected entity rotation (degrees)");
            m_ConsoleLog.push_back("  scale <x> <y> <z>     Set selected entity scale");
            m_ConsoleLog.push_back("  getpos                Print selected entity position");
            m_ConsoleLog.push_back("");
            m_ConsoleLog.push_back("--- RENDERING ---");
            m_ConsoleLog.push_back("  wireframe             Toggle wireframe mode");
            m_ConsoleLog.push_back("  shadows               Toggle shadows");
            m_ConsoleLog.push_back("  fog <density>         Set fog density (0 = off)");
            m_ConsoleLog.push_back("  ambient <r> <g> <b>   Set ambient color (0.0-1.0)");
            m_ConsoleLog.push_back("  culling               Toggle backface culling");
            m_ConsoleLog.push_back("  hdr                   Toggle HDR rendering");
            m_ConsoleLog.push_back("");
            m_ConsoleLog.push_back("--- RETRO ---");
            m_ConsoleLog.push_back("  flatshading           Toggle PS1-style flat shading");
            m_ConsoleLog.push_back("  vertexsnap            Toggle PS1-style vertex snapping");
            m_ConsoleLog.push_back("  affine                Toggle affine texture mapping");
            m_ConsoleLog.push_back("  gouraud               Toggle Gouraud-only shading");
            m_ConsoleLog.push_back("  stipple               Toggle stipple transparency");
            m_ConsoleLog.push_back("");
            m_ConsoleLog.push_back("--- SCENE ---");
            m_ConsoleLog.push_back("  save <path>           Save scene to file");
            m_ConsoleLog.push_back("  load <path>           Load scene from file");
            m_ConsoleLog.push_back("  play                  Start play mode");
            m_ConsoleLog.push_back("  stop                  Stop play mode");
            m_ConsoleLog.push_back("  pause                 Pause play mode");
            m_ConsoleLog.push_back("");
            m_ConsoleLog.push_back("--- COMPONENTS ---");
            m_ConsoleLog.push_back("  addcomp <type>        Add component to selected entity");
            m_ConsoleLog.push_back("  removecomp <type>     Remove component from selected entity");
            m_ConsoleLog.push_back("  setname <name>        Set entity name");
            m_ConsoleLog.push_back("  setnotes <text>       Set entity notes");
            m_ConsoleLog.push_back("  visible [true/false]  Toggle or set entity visibility");
            m_ConsoleLog.push_back("  components            List all components on selected entity");
            m_ConsoleLog.push_back("");
            m_ConsoleLog.push_back("--- MATERIALS ---");
            m_ConsoleLog.push_back("  setcolor <r> <g> <b>  Set base color (0-1)");
            m_ConsoleLog.push_back("  setemissive <r> <g> <b> <s>  Set emissive color + strength");
            m_ConsoleLog.push_back("  setmetallic <value>   Set metallic (0-1)");
            m_ConsoleLog.push_back("  setroughness <value>  Set roughness (0-1)");
            m_ConsoleLog.push_back("  setopacity <value>    Set opacity (0-1)");
            m_ConsoleLog.push_back("");
            m_ConsoleLog.push_back("--- LIGHTS ---");
            m_ConsoleLog.push_back("  lightcolor <r> <g> <b>  Set light color");
            m_ConsoleLog.push_back("  lightintensity <val>  Set light intensity");
            m_ConsoleLog.push_back("  lighttype <type>      Set light type (dir/point/spot)");
            m_ConsoleLog.push_back("  lightrange <val>      Set light range");
            m_ConsoleLog.push_back("");
            m_ConsoleLog.push_back("--- CAMERA ---");
            m_ConsoleLog.push_back("  fov <degrees>         Set camera field of view");
            m_ConsoleLog.push_back("  near <value>          Set camera near plane");
            m_ConsoleLog.push_back("  far <value>           Set camera far plane");
            m_ConsoleLog.push_back("");
            m_ConsoleLog.push_back("--- QUERY ---");
            m_ConsoleLog.push_back("  find <name>           Find entities by name substring");
            m_ConsoleLog.push_back("  count <component>     Count entities with component type");
            m_ConsoleLog.push_back("  children              List children of selected entity");
            m_ConsoleLog.push_back("  parent                Show parent of selected entity");
            m_ConsoleLog.push_back("");
            m_ConsoleLog.push_back("--- BULK ---");
            m_ConsoleLog.push_back("  selectall             Select all entities");
            m_ConsoleLog.push_back("  hideall               Hide all entities");
            m_ConsoleLog.push_back("  showall               Show all entities");
            m_ConsoleLog.push_back("  deleteall confirm     Delete all entities");
            m_ConsoleLog.push_back("");
            m_ConsoleLog.push_back("--- DEBUG ---");
            m_ConsoleLog.push_back("  colliders             Toggle collider wireframe display");
            m_ConsoleLog.push_back("  grid                  Toggle editor grid");
            m_ConsoleLog.push_back("  rain                  Toggle rain effect");
            m_ConsoleLog.push_back("  snow <intensity>      Set snow intensity (0 = off)");
            m_ConsoleLog.push_back("  shadowres <size>      Set shadow resolution (512-4096)");
            m_ConsoleLog.push_back("  shadowdist <dist>     Set shadow distance");
            m_ConsoleLog.push_back("  ambient_intensity <v> Set ambient intensity");
            m_ConsoleLog.push_back("  curvature <value>     Set world curvature strength");
            m_ConsoleLog.push_back("=============================================================");
        } else if (helpTopic == "pos") {
            m_ConsoleLog.push_back("pos <x> <y> <z>");
            m_ConsoleLog.push_back("  Set the world position of the selected entity.");
            m_ConsoleLog.push_back("  Example: pos 10 5.5 -3");
        } else if (helpTopic == "rot") {
            m_ConsoleLog.push_back("rot <x> <y> <z>");
            m_ConsoleLog.push_back("  Set rotation of selected entity in degrees (Euler angles).");
            m_ConsoleLog.push_back("  Example: rot 0 90 0  (rotate 90 degrees around Y)");
        } else if (helpTopic == "scale") {
            m_ConsoleLog.push_back("scale <x> <y> <z>");
            m_ConsoleLog.push_back("  Set scale of selected entity.");
            m_ConsoleLog.push_back("  Example: scale 2 2 2  (double size)");
        } else if (helpTopic == "select") {
            m_ConsoleLog.push_back("select <entity_id>");
            m_ConsoleLog.push_back("  Select an entity by its numeric ID.");
            m_ConsoleLog.push_back("  Use 'list' to see all entity IDs.");
        } else if (helpTopic == "fog") {
            m_ConsoleLog.push_back("fog <density>");
            m_ConsoleLog.push_back("  Set fog density. 0 = no fog, 0.01-0.1 = typical range.");
            m_ConsoleLog.push_back("  Example: fog 0.05");
        } else if (helpTopic == "ambient") {
            m_ConsoleLog.push_back("ambient <r> <g> <b>");
            m_ConsoleLog.push_back("  Set ambient light color (0.0 to 1.0 per channel).");
            m_ConsoleLog.push_back("  Example: ambient 0.2 0.2 0.3  (slightly blue ambient)");
        } else if (helpTopic == "timescale") {
            m_ConsoleLog.push_back("timescale <factor>");
            m_ConsoleLog.push_back("  Set game time scale. 1.0 = normal, 0.5 = half speed, 2.0 = double.");
            m_ConsoleLog.push_back("  0 = frozen. Negative values not supported.");
        } else if (helpTopic == "snow") {
            m_ConsoleLog.push_back("snow <intensity>");
            m_ConsoleLog.push_back("  Set snow particle intensity. 0 = off, 1.0 = normal, 2.0+ = blizzard.");
        } else {
            m_ConsoleLog.push_back("No detailed help for '" + helpTopic + "'. Type 'help' for full list.");
        }
    } else if (cmdLower == "version") {
        m_ConsoleLog.push_back("TEGE (The Enjin Game Engine) v" ENJIN_VERSION_STRING);
    } else if (cmdLower == "fps") {
        f32 fps = 1.0f / m_LastDeltaTime;
        f32 ms = m_LastDeltaTime * 1000.0f;
        m_ConsoleLog.push_back("FPS: " + std::to_string(static_cast<int>(fps)) + "  (" +
            std::to_string(ms).substr(0, 5) + " ms/frame)");
    } else if (cmdLower == "deselect") {
        ClearSelection();
        m_ConsoleLog.push_back("Selection cleared");
    } else if (cmdLower == "inspect") {
        if (m_SelectedEntities.empty() || !m_World) {
            m_ConsoleLog.push_back("No entity selected");
        } else {
            ECS::Entity e = m_PrimarySelected;
            std::string name = "Entity " + std::to_string(e);
            if (auto* nc = m_World->GetComponent<ECS::NameComponent>(e)) name = nc->name;
            m_ConsoleLog.push_back("Inspecting: [" + std::to_string(e) + "] " + name);
            if (m_World->HasComponent<ECS::TransformComponent>(e)) {
                auto* t = m_World->GetComponent<ECS::TransformComponent>(e);
                m_ConsoleLog.push_back("  Transform: pos(" +
                    std::to_string(t->position.x) + ", " + std::to_string(t->position.y) + ", " + std::to_string(t->position.z) +
                    ") visible=" + (t->visible ? "true" : "false"));
            }
            if (m_World->HasComponent<ECS::MeshComponent>(e)) {
                auto* m = m_World->GetComponent<ECS::MeshComponent>(e);
                m_ConsoleLog.push_back("  Mesh: " + std::to_string(m->vertices.size()) + " verts, " +
                    std::to_string(m->indices.size() / 3) + " tris");
            }
            if (m_World->HasComponent<ECS::MaterialComponent>(e)) m_ConsoleLog.push_back("  Material: yes");
            if (m_World->HasComponent<ECS::LightComponent>(e)) m_ConsoleLog.push_back("  Light: yes");
            if (m_World->HasComponent<ECS::CameraComponent>(e)) m_ConsoleLog.push_back("  Camera: yes");
            if (m_World->HasComponent<ECS::ScriptComponent>(e)) m_ConsoleLog.push_back("  Script: yes");
        }
    } else if (cmdLower == "getpos") {
        if (m_SelectedEntities.empty() || !m_World) {
            m_ConsoleLog.push_back("No entity selected");
        } else {
            auto* t = m_World->GetComponent<ECS::TransformComponent>(m_PrimarySelected);
            if (t) {
                m_ConsoleLog.push_back("Position: " + std::to_string(t->position.x) + ", " +
                    std::to_string(t->position.y) + ", " + std::to_string(t->position.z));
            } else {
                m_ConsoleLog.push_back("Selected entity has no transform");
            }
        }
    } else if (cmdLower == "rot") {
        if (m_SelectedEntities.empty() || !m_World) {
            m_ConsoleLog.push_back("No entity selected");
            return;
        }
        f32 x, y, z;
        if (iss >> x >> y >> z) {
            auto* transform = m_World->GetComponent<ECS::TransformComponent>(m_PrimarySelected);
            if (transform) {
                transform->rotation = Math::Quaternion::FromEuler(
                    Math::Vector3(Math::Radians(x), Math::Radians(y), Math::Radians(z)));
                m_ConsoleLog.push_back("Set rotation to " + std::to_string(x) + ", " + std::to_string(y) + ", " + std::to_string(z) + " degrees");
            } else {
                m_ConsoleLog.push_back("Selected entity has no transform");
            }
        } else {
            m_ConsoleLog.push_back("Usage: rot <x> <y> <z> (degrees)");
        }
    } else if (cmdLower == "scale") {
        if (m_SelectedEntities.empty() || !m_World) {
            m_ConsoleLog.push_back("No entity selected");
            return;
        }
        f32 x, y, z;
        if (iss >> x >> y >> z) {
            auto* transform = m_World->GetComponent<ECS::TransformComponent>(m_PrimarySelected);
            if (transform) {
                transform->scale = Math::Vector3(x, y, z);
                m_ConsoleLog.push_back("Set scale to " + std::to_string(x) + ", " + std::to_string(y) + ", " + std::to_string(z));
            } else {
                m_ConsoleLog.push_back("Selected entity has no transform");
            }
        } else {
            // Single uniform scale
            f32 s;
            std::istringstream retry(command);
            std::string skip; retry >> skip;
            if (retry >> s) {
                auto* transform = m_World->GetComponent<ECS::TransformComponent>(m_PrimarySelected);
                if (transform) {
                    transform->scale = Math::Vector3(s, s, s);
                    m_ConsoleLog.push_back("Set uniform scale to " + std::to_string(s));
                }
            } else {
                m_ConsoleLog.push_back("Usage: scale <x> <y> <z> or scale <uniform>");
            }
        }
    } else if (cmdLower == "fog") {
        f32 density;
        if (iss >> density) {
            if (m_RenderSystem) {
                m_RenderSystem->SetFogParams(density, 10.0f, 100.0f, 0.5f);
                m_ConsoleLog.push_back("Fog density set to " + std::to_string(density));
            }
        } else {
            m_ConsoleLog.push_back("Usage: fog <density> (e.g. fog 0.05)");
        }
    } else if (cmdLower == "ambient") {
        f32 r, g, b;
        if (iss >> r >> g >> b) {
            if (m_RenderSystem) {
                m_RenderSystem->SetAmbientColor(Math::Vector3(r, g, b));
                m_ConsoleLog.push_back("Ambient color set to " + std::to_string(r) + ", " + std::to_string(g) + ", " + std::to_string(b));
            }
        } else {
            m_ConsoleLog.push_back("Usage: ambient <r> <g> <b> (0.0-1.0)");
        }
    } else if (cmdLower == "culling") {
        if (m_RenderSystem) {
            bool enabled = !m_RenderSystem->IsBackfaceCullingEnabled();
            m_RenderSystem->SetBackfaceCullingEnabled(enabled);
            m_ConsoleLog.push_back(std::string("Backface culling ") + (enabled ? "ON" : "OFF"));
        }
    } else if (cmdLower == "hdr") {
        if (m_RenderSystem) {
            bool enabled = !m_RenderSystem->IsHDREnabled();
            m_RenderSystem->SetHDREnabled(enabled);
            m_ConsoleLog.push_back(std::string("HDR ") + (enabled ? "ON" : "OFF"));
        }
    } else if (cmdLower == "flatshading") {
        if (m_RenderSystem) {
            m_RenderSystem->SetGlobalFlatShading(!m_RenderSystem->GetGlobalFlatShading());
            m_ConsoleLog.push_back(std::string("Flat shading ") + (m_RenderSystem->GetGlobalFlatShading() ? "ON" : "OFF"));
        }
    } else if (cmdLower == "vertexsnap") {
        if (m_RenderSystem) {
            m_RenderSystem->SetGlobalVertexSnapping(!m_RenderSystem->GetGlobalVertexSnapping());
            m_ConsoleLog.push_back(std::string("Vertex snapping ") + (m_RenderSystem->GetGlobalVertexSnapping() ? "ON" : "OFF"));
        }
    } else if (cmdLower == "affine") {
        if (m_RenderSystem) {
            m_RenderSystem->SetGlobalAffineTexturing(!m_RenderSystem->GetGlobalAffineTexturing());
            m_ConsoleLog.push_back(std::string("Affine texturing ") + (m_RenderSystem->GetGlobalAffineTexturing() ? "ON" : "OFF"));
        }
    } else if (cmdLower == "gouraud") {
        if (m_RenderSystem) {
            m_RenderSystem->SetGlobalGouraudOnly(!m_RenderSystem->GetGlobalGouraudOnly());
            m_ConsoleLog.push_back(std::string("Gouraud-only ") + (m_RenderSystem->GetGlobalGouraudOnly() ? "ON" : "OFF"));
        }
    } else if (cmdLower == "stipple") {
        if (m_RenderSystem) {
            m_RenderSystem->SetGlobalStippleTransparency(!m_RenderSystem->GetGlobalStippleTransparency());
            m_ConsoleLog.push_back(std::string("Stipple transparency ") + (m_RenderSystem->GetGlobalStippleTransparency() ? "ON" : "OFF"));
        }
    } else if (cmdLower == "play") {
        if (m_PlayMode.IsStopped()) {
            m_PrePlayRenderSettings = Renderer::SceneRenderSettings::CaptureFromRuntime(
                m_RenderSystem, m_PostProcessing ? &m_PostProcessing->GetSettings() : nullptr);
            StartPlayMode();
            m_ConsoleLog.push_back("Play mode started");
        } else {
            m_ConsoleLog.push_back("Already in play mode");
        }
    } else if (cmdLower == "stop") {
        if (!m_PlayMode.IsStopped()) {
            m_PlayMode.Stop();
            ClearSelection();
            m_PrePlayRenderSettings.ApplyToRuntime(
                m_RenderSystem, m_PostProcessing ? &m_PostProcessing->GetSettings() : nullptr);
            m_ConsoleLog.push_back("Play mode stopped");
        } else {
            m_ConsoleLog.push_back("Not in play mode");
        }
    } else if (cmdLower == "pause") {
        if (m_PlayMode.IsPlaying()) {
            m_PlayMode.Pause();
            m_ConsoleLog.push_back("Play mode paused");
        } else if (m_PlayMode.IsPaused()) {
            m_PlayMode.Resume();
            m_ConsoleLog.push_back("Play mode resumed");
        } else {
            m_ConsoleLog.push_back("Not in play mode");
        }
    } else if (cmdLower == "colliders") {
        m_ShowColliderWireframes = !m_ShowColliderWireframes;
        m_ConsoleLog.push_back(std::string("Collider wireframes ") + (m_ShowColliderWireframes ? "ON" : "OFF"));
    } else if (cmdLower == "grid") {
        m_ShowGrid = !m_ShowGrid;
        m_ConsoleLog.push_back(std::string("Grid ") + (m_ShowGrid ? "ON" : "OFF"));
    } else if (cmdLower == "rain") {
        if (m_RenderSystem) {
            bool active = !m_RenderSystem->IsRainActive();
            m_RenderSystem->SetRainActive(active);
            m_ConsoleLog.push_back(std::string("Rain ") + (active ? "ON" : "OFF"));
        }
    } else if (cmdLower == "snow") {
        f32 intensity;
        if (iss >> intensity) {
            if (m_RenderSystem) {
                m_RenderSystem->SetSnowIntensity(intensity);
                m_ConsoleLog.push_back("Snow intensity set to " + std::to_string(intensity));
            }
        } else {
            m_ConsoleLog.push_back("Usage: snow <intensity> (0=off, 1=normal, 2+=blizzard)");
        }
    } else if (cmdLower == "clear") {
        m_ConsoleLog.clear();
    } else if (cmdLower == "list") {
        if (!m_World) {
            m_ConsoleLog.push_back("Error: No world loaded");
            return;
        }
        const auto& entities = m_World->GetAllEntities();
        m_ConsoleLog.push_back("Entities (" + std::to_string(entities.size()) + "):");
        for (ECS::Entity entity : entities) {
            std::string name = "Entity " + std::to_string(entity);
            if (auto* nc = m_World->GetComponent<ECS::NameComponent>(entity)) {
                name = nc->name;
            }
            std::string line = "  [" + std::to_string(entity) + "] " + name;
            if (IsSelected(entity)) line += " (selected)";
            m_ConsoleLog.push_back(line);
        }
    } else if (cmdLower == "select") {
        u64 id = 0;
        if (iss >> id) {
            SelectEntity(static_cast<ECS::Entity>(id));
            m_ConsoleLog.push_back("Selected entity " + std::to_string(id));
        } else {
            m_ConsoleLog.push_back("Usage: select <entity_id>");
        }
    } else if (cmdLower == "create") {
        if (!m_World) {
            m_ConsoleLog.push_back("Error: No world loaded");
            return;
        }
        std::string name;
        std::getline(iss >> std::ws, name);
        if (name.empty()) name = "New Entity";
        ECS::Entity entity = m_World->CreateEntity();
        m_World->AddComponent<ECS::NameComponent>(entity, name);
        m_World->AddComponent<ECS::TransformComponent>(entity);
        SelectEntity(entity);
        m_ConsoleLog.push_back("Created entity [" + std::to_string(entity) + "] '" + name + "'");
    } else if (cmdLower == "delete") {
        if (m_SelectedEntities.empty()) {
            m_ConsoleLog.push_back("No entity selected");
        } else {
            usize count = m_SelectedEntities.size();
            DeleteSelectedEntities();
            m_ConsoleLog.push_back("Deleted " + std::to_string(count) + " entity(ies)");
        }
    } else if (cmdLower == "pos") {
        if (m_SelectedEntities.empty() || !m_World) {
            m_ConsoleLog.push_back("No entity selected");
            return;
        }
        f32 x, y, z;
        if (iss >> x >> y >> z) {
            auto* transform = m_World->GetComponent<ECS::TransformComponent>(m_PrimarySelected);
            if (transform) {
                transform->position = Math::Vector3(x, y, z);
                m_ConsoleLog.push_back("Set position to " + std::to_string(x) + ", " + std::to_string(y) + ", " + std::to_string(z));
            } else {
                m_ConsoleLog.push_back("Selected entity has no transform");
            }
        } else {
            m_ConsoleLog.push_back("Usage: pos <x> <y> <z>");
        }
    } else if (cmdLower == "wireframe") {
        if (m_RenderSystem) {
            bool enabled = !m_RenderSystem->IsWireframeEnabled();
            m_RenderSystem->SetWireframeEnabled(enabled);
            m_ConsoleLog.push_back(std::string("Wireframe ") + (enabled ? "ON" : "OFF"));
        }
    } else if (cmdLower == "shadows") {
        if (m_RenderSystem) {
            bool enabled = !m_RenderSystem->IsShadowsEnabled();
            m_RenderSystem->SetShadowsEnabled(enabled);
            m_ConsoleLog.push_back(std::string("Shadows ") + (enabled ? "ON" : "OFF"));
        }
    } else if (cmdLower == "stats") {
        if (!m_World) {
            m_ConsoleLog.push_back("Error: No world loaded");
            return;
        }
        usize entityCount = m_World->GetEntityCount();
        u32 meshCount = 0, lightCount = 0, cameraCount = 0;
        u32 totalVerts = 0, totalTris = 0;
        for (ECS::Entity entity : m_World->GetEntitiesWithComponent<ECS::MeshComponent>()) {
            meshCount++;
            auto* mesh = m_World->GetComponent<ECS::MeshComponent>(entity);
            totalVerts += static_cast<u32>(mesh->vertices.size());
            totalTris += static_cast<u32>(mesh->indices.size()) / 3;
        }
        lightCount = static_cast<u32>(m_World->GetEntitiesWithComponent<ECS::LightComponent>().size());
        cameraCount = static_cast<u32>(m_World->GetEntitiesWithComponent<ECS::CameraComponent>().size());
        m_ConsoleLog.push_back("Scene Statistics:");
        m_ConsoleLog.push_back("  Entities: " + std::to_string(entityCount));
        m_ConsoleLog.push_back("  Meshes: " + std::to_string(meshCount) + " (" + std::to_string(totalVerts) + " verts, " + std::to_string(totalTris) + " tris)");
        m_ConsoleLog.push_back("  Lights: " + std::to_string(lightCount));
        m_ConsoleLog.push_back("  Cameras: " + std::to_string(cameraCount));
        m_ConsoleLog.push_back("  FPS: " + std::to_string(static_cast<int>(1.0f / m_LastDeltaTime)));
    } else if (cmdLower == "save") {
        std::string path;
        iss >> path;
        if (path.empty()) {
            m_ConsoleLog.push_back("Usage: save <filepath>");
        } else {
            SaveScene(path);
            m_ConsoleLog.push_back("Saved scene to " + path);
        }
    } else if (cmdLower == "load") {
        std::string path;
        iss >> path;
        if (path.empty()) {
            m_ConsoleLog.push_back("Usage: load <filepath>");
        } else {
            OpenScene(path);
            m_ConsoleLog.push_back("Loaded scene from " + path);
        }
    // =====================================================================
    // Component commands
    // =====================================================================
    } else if (cmdLower == "addcomp") {
        if (m_SelectedEntities.empty() || !m_World) {
            m_ConsoleLog.push_back("No entity selected");
            return;
        }
        std::string type;
        iss >> type;
        for (auto& c : type) c = static_cast<char>(std::tolower(c));
        ECS::Entity e = m_PrimarySelected;
        if (type == "mesh") {
            if (m_World->HasComponent<ECS::MeshComponent>(e)) { m_ConsoleLog.push_back("Entity already has MeshComponent"); }
            else { m_World->AddComponent<ECS::MeshComponent>(e); m_ConsoleLog.push_back("Added MeshComponent"); }
        } else if (type == "material") {
            if (m_World->HasComponent<ECS::MaterialComponent>(e)) { m_ConsoleLog.push_back("Entity already has MaterialComponent"); }
            else { m_World->AddComponent<ECS::MaterialComponent>(e); m_ConsoleLog.push_back("Added MaterialComponent"); }
        } else if (type == "light") {
            if (m_World->HasComponent<ECS::LightComponent>(e)) { m_ConsoleLog.push_back("Entity already has LightComponent"); }
            else { m_World->AddComponent<ECS::LightComponent>(e); m_ConsoleLog.push_back("Added LightComponent"); }
        } else if (type == "camera") {
            if (m_World->HasComponent<ECS::CameraComponent>(e)) { m_ConsoleLog.push_back("Entity already has CameraComponent"); }
            else { m_World->AddComponent<ECS::CameraComponent>(e); m_ConsoleLog.push_back("Added CameraComponent"); }
        } else if (type == "script") {
            if (m_World->HasComponent<ECS::ScriptComponent>(e)) { m_ConsoleLog.push_back("Entity already has ScriptComponent"); }
            else { m_World->AddComponent<ECS::ScriptComponent>(e); m_ConsoleLog.push_back("Added ScriptComponent"); }
        } else if (type == "audio") {
            if (m_World->HasComponent<ECS::AudioSourceComponent>(e)) { m_ConsoleLog.push_back("Entity already has AudioSourceComponent"); }
            else { m_World->AddComponent<ECS::AudioSourceComponent>(e); m_ConsoleLog.push_back("Added AudioSourceComponent"); }
        } else if (type == "rigidbody") {
            if (m_World->HasComponent<ECS::RigidbodyComponent>(e)) { m_ConsoleLog.push_back("Entity already has RigidbodyComponent"); }
            else { m_World->AddComponent<ECS::RigidbodyComponent>(e); m_ConsoleLog.push_back("Added RigidbodyComponent"); }
        } else if (type == "name") {
            if (m_World->HasComponent<ECS::NameComponent>(e)) { m_ConsoleLog.push_back("Entity already has NameComponent"); }
            else { m_World->AddComponent<ECS::NameComponent>(e); m_ConsoleLog.push_back("Added NameComponent"); }
        } else if (type == "notes") {
            if (m_World->HasComponent<ECS::NotesComponent>(e)) { m_ConsoleLog.push_back("Entity already has NotesComponent"); }
            else { m_World->AddComponent<ECS::NotesComponent>(e); m_ConsoleLog.push_back("Added NotesComponent"); }
        } else if (type == "sprite") {
            if (m_World->HasComponent<ECS::Sprite2DComponent>(e)) { m_ConsoleLog.push_back("Entity already has Sprite2DComponent"); }
            else { m_World->AddComponent<ECS::Sprite2DComponent>(e); m_ConsoleLog.push_back("Added Sprite2DComponent"); }
        } else if (type == "particle") {
            if (m_World->HasComponent<ECS::ParticleEmitterComponent>(e)) { m_ConsoleLog.push_back("Entity already has ParticleEmitterComponent"); }
            else { m_World->AddComponent<ECS::ParticleEmitterComponent>(e); m_ConsoleLog.push_back("Added ParticleEmitterComponent"); }
        } else if (type == "tween") {
            if (m_World->HasComponent<ECS::TweenComponent>(e)) { m_ConsoleLog.push_back("Entity already has TweenComponent"); }
            else { m_World->AddComponent<ECS::TweenComponent>(e); m_ConsoleLog.push_back("Added TweenComponent"); }
        } else if (type == "lod") {
            if (m_World->HasComponent<ECS::LODComponent>(e)) { m_ConsoleLog.push_back("Entity already has LODComponent"); }
            else { m_World->AddComponent<ECS::LODComponent>(e); m_ConsoleLog.push_back("Added LODComponent"); }
        } else {
            m_ConsoleLog.push_back("Unknown component type: " + type);
            m_ConsoleLog.push_back("  Types: mesh, material, light, camera, script, audio, rigidbody, name, notes, sprite, particle, tween, lod");
        }
    } else if (cmdLower == "removecomp") {
        if (m_SelectedEntities.empty() || !m_World) {
            m_ConsoleLog.push_back("No entity selected");
            return;
        }
        std::string type;
        iss >> type;
        for (auto& c : type) c = static_cast<char>(std::tolower(c));
        ECS::Entity e = m_PrimarySelected;
        if (type == "mesh") {
            if (!m_World->HasComponent<ECS::MeshComponent>(e)) { m_ConsoleLog.push_back("Entity has no MeshComponent"); }
            else { m_World->RemoveComponent<ECS::MeshComponent>(e); m_ConsoleLog.push_back("Removed MeshComponent"); }
        } else if (type == "material") {
            if (!m_World->HasComponent<ECS::MaterialComponent>(e)) { m_ConsoleLog.push_back("Entity has no MaterialComponent"); }
            else { m_World->RemoveComponent<ECS::MaterialComponent>(e); m_ConsoleLog.push_back("Removed MaterialComponent"); }
        } else if (type == "light") {
            if (!m_World->HasComponent<ECS::LightComponent>(e)) { m_ConsoleLog.push_back("Entity has no LightComponent"); }
            else { m_World->RemoveComponent<ECS::LightComponent>(e); m_ConsoleLog.push_back("Removed LightComponent"); }
        } else if (type == "camera") {
            if (!m_World->HasComponent<ECS::CameraComponent>(e)) { m_ConsoleLog.push_back("Entity has no CameraComponent"); }
            else { m_World->RemoveComponent<ECS::CameraComponent>(e); m_ConsoleLog.push_back("Removed CameraComponent"); }
        } else if (type == "script") {
            if (!m_World->HasComponent<ECS::ScriptComponent>(e)) { m_ConsoleLog.push_back("Entity has no ScriptComponent"); }
            else { m_World->RemoveComponent<ECS::ScriptComponent>(e); m_ConsoleLog.push_back("Removed ScriptComponent"); }
        } else if (type == "audio") {
            if (!m_World->HasComponent<ECS::AudioSourceComponent>(e)) { m_ConsoleLog.push_back("Entity has no AudioSourceComponent"); }
            else { m_World->RemoveComponent<ECS::AudioSourceComponent>(e); m_ConsoleLog.push_back("Removed AudioSourceComponent"); }
        } else if (type == "rigidbody") {
            if (!m_World->HasComponent<ECS::RigidbodyComponent>(e)) { m_ConsoleLog.push_back("Entity has no RigidbodyComponent"); }
            else { m_World->RemoveComponent<ECS::RigidbodyComponent>(e); m_ConsoleLog.push_back("Removed RigidbodyComponent"); }
        } else if (type == "name") {
            if (!m_World->HasComponent<ECS::NameComponent>(e)) { m_ConsoleLog.push_back("Entity has no NameComponent"); }
            else { m_World->RemoveComponent<ECS::NameComponent>(e); m_ConsoleLog.push_back("Removed NameComponent"); }
        } else if (type == "notes") {
            if (!m_World->HasComponent<ECS::NotesComponent>(e)) { m_ConsoleLog.push_back("Entity has no NotesComponent"); }
            else { m_World->RemoveComponent<ECS::NotesComponent>(e); m_ConsoleLog.push_back("Removed NotesComponent"); }
        } else if (type == "sprite") {
            if (!m_World->HasComponent<ECS::Sprite2DComponent>(e)) { m_ConsoleLog.push_back("Entity has no Sprite2DComponent"); }
            else { m_World->RemoveComponent<ECS::Sprite2DComponent>(e); m_ConsoleLog.push_back("Removed Sprite2DComponent"); }
        } else if (type == "particle") {
            if (!m_World->HasComponent<ECS::ParticleEmitterComponent>(e)) { m_ConsoleLog.push_back("Entity has no ParticleEmitterComponent"); }
            else { m_World->RemoveComponent<ECS::ParticleEmitterComponent>(e); m_ConsoleLog.push_back("Removed ParticleEmitterComponent"); }
        } else if (type == "tween") {
            if (!m_World->HasComponent<ECS::TweenComponent>(e)) { m_ConsoleLog.push_back("Entity has no TweenComponent"); }
            else { m_World->RemoveComponent<ECS::TweenComponent>(e); m_ConsoleLog.push_back("Removed TweenComponent"); }
        } else if (type == "lod") {
            if (!m_World->HasComponent<ECS::LODComponent>(e)) { m_ConsoleLog.push_back("Entity has no LODComponent"); }
            else { m_World->RemoveComponent<ECS::LODComponent>(e); m_ConsoleLog.push_back("Removed LODComponent"); }
        } else {
            m_ConsoleLog.push_back("Unknown component type: " + type);
            m_ConsoleLog.push_back("  Types: mesh, material, light, camera, script, audio, rigidbody, name, notes, sprite, particle, tween, lod");
        }
    } else if (cmdLower == "setname") {
        if (m_SelectedEntities.empty() || !m_World) {
            m_ConsoleLog.push_back("No entity selected");
            return;
        }
        std::string name;
        std::getline(iss >> std::ws, name);
        if (name.empty()) {
            m_ConsoleLog.push_back("Usage: setname <name>");
            return;
        }
        ECS::Entity e = m_PrimarySelected;
        auto* nc = m_World->GetComponent<ECS::NameComponent>(e);
        if (!nc) {
            m_World->AddComponent<ECS::NameComponent>(e, name);
        } else {
            nc->name = name;
        }
        m_ConsoleLog.push_back("Set name to '" + name + "'");
    } else if (cmdLower == "setnotes") {
        if (m_SelectedEntities.empty() || !m_World) {
            m_ConsoleLog.push_back("No entity selected");
            return;
        }
        std::string text;
        std::getline(iss >> std::ws, text);
        if (text.empty()) {
            m_ConsoleLog.push_back("Usage: setnotes <text>");
            return;
        }
        ECS::Entity e = m_PrimarySelected;
        auto* nc = m_World->GetComponent<ECS::NotesComponent>(e);
        if (!nc) {
            m_World->AddComponent<ECS::NotesComponent>(e, text);
        } else {
            nc->notes = text;
        }
        m_ConsoleLog.push_back("Set notes to '" + text + "'");
    } else if (cmdLower == "visible") {
        if (m_SelectedEntities.empty() || !m_World) {
            m_ConsoleLog.push_back("No entity selected");
            return;
        }
        ECS::Entity e = m_PrimarySelected;
        auto* transform = m_World->GetComponent<ECS::TransformComponent>(e);
        if (!transform) {
            m_ConsoleLog.push_back("Selected entity has no transform");
            return;
        }
        std::string val;
        iss >> val;
        for (auto& c : val) c = static_cast<char>(std::tolower(c));
        if (val == "true") {
            transform->visible = true;
        } else if (val == "false") {
            transform->visible = false;
        } else {
            transform->visible = !transform->visible;
        }
        m_ConsoleLog.push_back(std::string("Visible: ") + (transform->visible ? "true" : "false"));
    } else if (cmdLower == "components") {
        if (m_SelectedEntities.empty() || !m_World) {
            m_ConsoleLog.push_back("No entity selected");
            return;
        }
        ECS::Entity e = m_PrimarySelected;
        std::string name = "Entity " + std::to_string(e);
        if (auto* nc = m_World->GetComponent<ECS::NameComponent>(e)) name = nc->name;
        m_ConsoleLog.push_back("Components on [" + std::to_string(e) + "] " + name + ":");
        if (m_World->HasComponent<ECS::TransformComponent>(e)) m_ConsoleLog.push_back("  TransformComponent");
        if (m_World->HasComponent<ECS::NameComponent>(e)) m_ConsoleLog.push_back("  NameComponent");
        if (m_World->HasComponent<ECS::NotesComponent>(e)) m_ConsoleLog.push_back("  NotesComponent");
        if (m_World->HasComponent<ECS::MeshComponent>(e)) m_ConsoleLog.push_back("  MeshComponent");
        if (m_World->HasComponent<ECS::MaterialComponent>(e)) m_ConsoleLog.push_back("  MaterialComponent");
        if (m_World->HasComponent<ECS::LightComponent>(e)) m_ConsoleLog.push_back("  LightComponent");
        if (m_World->HasComponent<ECS::CameraComponent>(e)) m_ConsoleLog.push_back("  CameraComponent");
        if (m_World->HasComponent<ECS::ScriptComponent>(e)) m_ConsoleLog.push_back("  ScriptComponent");
        if (m_World->HasComponent<ECS::AudioSourceComponent>(e)) m_ConsoleLog.push_back("  AudioSourceComponent");
        if (m_World->HasComponent<ECS::RigidbodyComponent>(e)) m_ConsoleLog.push_back("  RigidbodyComponent");
        if (m_World->HasComponent<ECS::Sprite2DComponent>(e)) m_ConsoleLog.push_back("  Sprite2DComponent");
        if (m_World->HasComponent<ECS::ParticleEmitterComponent>(e)) m_ConsoleLog.push_back("  ParticleEmitterComponent");
        if (m_World->HasComponent<ECS::TweenComponent>(e)) m_ConsoleLog.push_back("  TweenComponent");
        if (m_World->HasComponent<ECS::LODComponent>(e)) m_ConsoleLog.push_back("  LODComponent");
        if (m_World->HasComponent<ECS::ParentComponent>(e)) m_ConsoleLog.push_back("  ParentComponent");
        if (m_World->HasComponent<ECS::ChildrenComponent>(e)) m_ConsoleLog.push_back("  ChildrenComponent");
    // =====================================================================
    // Material commands
    // =====================================================================
    } else if (cmdLower == "setcolor") {
        if (m_SelectedEntities.empty() || !m_World) {
            m_ConsoleLog.push_back("No entity selected");
            return;
        }
        f32 r, g, b;
        if (iss >> r >> g >> b) {
            auto* mat = m_World->GetComponent<ECS::MaterialComponent>(m_PrimarySelected);
            if (!mat) {
                m_ConsoleLog.push_back("Selected entity has no MaterialComponent");
            } else {
                mat->baseColor = Math::Vector3(r, g, b);
                m_ConsoleLog.push_back("Set base color to " + std::to_string(r) + ", " + std::to_string(g) + ", " + std::to_string(b));
            }
        } else {
            m_ConsoleLog.push_back("Usage: setcolor <r> <g> <b> (0.0-1.0)");
        }
    } else if (cmdLower == "setemissive") {
        if (m_SelectedEntities.empty() || !m_World) {
            m_ConsoleLog.push_back("No entity selected");
            return;
        }
        f32 r, g, b, strength;
        if (iss >> r >> g >> b >> strength) {
            auto* mat = m_World->GetComponent<ECS::MaterialComponent>(m_PrimarySelected);
            if (!mat) {
                m_ConsoleLog.push_back("Selected entity has no MaterialComponent");
            } else {
                mat->emissiveColor = Math::Vector3(r, g, b);
                mat->emissiveStrength = strength;
                m_ConsoleLog.push_back("Set emissive to (" + std::to_string(r) + ", " + std::to_string(g) + ", " + std::to_string(b) +
                    ") strength=" + std::to_string(strength));
            }
        } else {
            m_ConsoleLog.push_back("Usage: setemissive <r> <g> <b> <strength>");
        }
    } else if (cmdLower == "setmetallic") {
        if (m_SelectedEntities.empty() || !m_World) {
            m_ConsoleLog.push_back("No entity selected");
            return;
        }
        f32 value;
        if (iss >> value) {
            auto* mat = m_World->GetComponent<ECS::MaterialComponent>(m_PrimarySelected);
            if (!mat) {
                m_ConsoleLog.push_back("Selected entity has no MaterialComponent");
            } else {
                mat->metallic = value;
                m_ConsoleLog.push_back("Set metallic to " + std::to_string(value));
            }
        } else {
            m_ConsoleLog.push_back("Usage: setmetallic <value> (0.0-1.0)");
        }
    } else if (cmdLower == "setroughness") {
        if (m_SelectedEntities.empty() || !m_World) {
            m_ConsoleLog.push_back("No entity selected");
            return;
        }
        f32 value;
        if (iss >> value) {
            auto* mat = m_World->GetComponent<ECS::MaterialComponent>(m_PrimarySelected);
            if (!mat) {
                m_ConsoleLog.push_back("Selected entity has no MaterialComponent");
            } else {
                mat->roughness = value;
                m_ConsoleLog.push_back("Set roughness to " + std::to_string(value));
            }
        } else {
            m_ConsoleLog.push_back("Usage: setroughness <value> (0.0-1.0)");
        }
    } else if (cmdLower == "setopacity") {
        if (m_SelectedEntities.empty() || !m_World) {
            m_ConsoleLog.push_back("No entity selected");
            return;
        }
        f32 value;
        if (iss >> value) {
            auto* mat = m_World->GetComponent<ECS::MaterialComponent>(m_PrimarySelected);
            if (!mat) {
                m_ConsoleLog.push_back("Selected entity has no MaterialComponent");
            } else {
                mat->opacity = value;
                m_ConsoleLog.push_back("Set opacity to " + std::to_string(value));
            }
        } else {
            m_ConsoleLog.push_back("Usage: setopacity <value> (0.0-1.0)");
        }
    // =====================================================================
    // Light commands
    // =====================================================================
    } else if (cmdLower == "lightcolor") {
        if (m_SelectedEntities.empty() || !m_World) {
            m_ConsoleLog.push_back("No entity selected");
            return;
        }
        f32 r, g, b;
        if (iss >> r >> g >> b) {
            auto* light = m_World->GetComponent<ECS::LightComponent>(m_PrimarySelected);
            if (!light) {
                m_ConsoleLog.push_back("Selected entity has no LightComponent");
            } else {
                light->color = Math::Vector3(r, g, b);
                m_ConsoleLog.push_back("Set light color to " + std::to_string(r) + ", " + std::to_string(g) + ", " + std::to_string(b));
            }
        } else {
            m_ConsoleLog.push_back("Usage: lightcolor <r> <g> <b> (0.0-1.0)");
        }
    } else if (cmdLower == "lightintensity") {
        if (m_SelectedEntities.empty() || !m_World) {
            m_ConsoleLog.push_back("No entity selected");
            return;
        }
        f32 value;
        if (iss >> value) {
            auto* light = m_World->GetComponent<ECS::LightComponent>(m_PrimarySelected);
            if (!light) {
                m_ConsoleLog.push_back("Selected entity has no LightComponent");
            } else {
                light->intensity = value;
                m_ConsoleLog.push_back("Set light intensity to " + std::to_string(value));
            }
        } else {
            m_ConsoleLog.push_back("Usage: lightintensity <value>");
        }
    } else if (cmdLower == "lighttype") {
        if (m_SelectedEntities.empty() || !m_World) {
            m_ConsoleLog.push_back("No entity selected");
            return;
        }
        std::string type;
        iss >> type;
        for (auto& c : type) c = static_cast<char>(std::tolower(c));
        auto* light = m_World->GetComponent<ECS::LightComponent>(m_PrimarySelected);
        if (!light) {
            m_ConsoleLog.push_back("Selected entity has no LightComponent");
        } else if (type == "dir" || type == "directional") {
            light->type = ECS::LightType::Directional;
            m_ConsoleLog.push_back("Set light type to Directional");
        } else if (type == "point") {
            light->type = ECS::LightType::Point;
            m_ConsoleLog.push_back("Set light type to Point");
        } else if (type == "spot") {
            light->type = ECS::LightType::Spot;
            m_ConsoleLog.push_back("Set light type to Spot");
        } else {
            m_ConsoleLog.push_back("Usage: lighttype <dir|point|spot>");
        }
    } else if (cmdLower == "lightrange") {
        if (m_SelectedEntities.empty() || !m_World) {
            m_ConsoleLog.push_back("No entity selected");
            return;
        }
        f32 value;
        if (iss >> value) {
            auto* light = m_World->GetComponent<ECS::LightComponent>(m_PrimarySelected);
            if (!light) {
                m_ConsoleLog.push_back("Selected entity has no LightComponent");
            } else {
                light->range = value;
                m_ConsoleLog.push_back("Set light range to " + std::to_string(value));
            }
        } else {
            m_ConsoleLog.push_back("Usage: lightrange <value>");
        }
    // =====================================================================
    // Query commands
    // =====================================================================
    } else if (cmdLower == "find") {
        if (!m_World) {
            m_ConsoleLog.push_back("Error: No world loaded");
            return;
        }
        std::string searchTerm;
        std::getline(iss >> std::ws, searchTerm);
        if (searchTerm.empty()) {
            m_ConsoleLog.push_back("Usage: find <name>");
            return;
        }
        // Convert search term to lowercase for case-insensitive matching
        std::string searchLower = searchTerm;
        for (auto& c : searchLower) c = static_cast<char>(std::tolower(c));
        u32 found = 0;
        for (ECS::Entity entity : m_World->GetAllEntities()) {
            auto* nc = m_World->GetComponent<ECS::NameComponent>(entity);
            if (!nc) continue;
            std::string nameLower = nc->name;
            for (auto& c : nameLower) c = static_cast<char>(std::tolower(c));
            if (nameLower.find(searchLower) != std::string::npos) {
                m_ConsoleLog.push_back("  [" + std::to_string(entity) + "] " + nc->name);
                found++;
            }
        }
        if (found == 0) {
            m_ConsoleLog.push_back("No entities found matching '" + searchTerm + "'");
        } else {
            m_ConsoleLog.push_back("Found " + std::to_string(found) + " matching entity(ies)");
        }
    } else if (cmdLower == "count") {
        if (!m_World) {
            m_ConsoleLog.push_back("Error: No world loaded");
            return;
        }
        std::string type;
        iss >> type;
        for (auto& c : type) c = static_cast<char>(std::tolower(c));
        u32 n = 0;
        if (type == "mesh") {
            n = static_cast<u32>(m_World->GetEntitiesWithComponent<ECS::MeshComponent>().size());
        } else if (type == "material") {
            n = static_cast<u32>(m_World->GetEntitiesWithComponent<ECS::MaterialComponent>().size());
        } else if (type == "light") {
            n = static_cast<u32>(m_World->GetEntitiesWithComponent<ECS::LightComponent>().size());
        } else if (type == "camera") {
            n = static_cast<u32>(m_World->GetEntitiesWithComponent<ECS::CameraComponent>().size());
        } else if (type == "script") {
            n = static_cast<u32>(m_World->GetEntitiesWithComponent<ECS::ScriptComponent>().size());
        } else if (type == "audio") {
            n = static_cast<u32>(m_World->GetEntitiesWithComponent<ECS::AudioSourceComponent>().size());
        } else if (type == "rigidbody") {
            n = static_cast<u32>(m_World->GetEntitiesWithComponent<ECS::RigidbodyComponent>().size());
        } else if (type == "name") {
            n = static_cast<u32>(m_World->GetEntitiesWithComponent<ECS::NameComponent>().size());
        } else if (type == "notes") {
            n = static_cast<u32>(m_World->GetEntitiesWithComponent<ECS::NotesComponent>().size());
        } else if (type == "sprite") {
            n = static_cast<u32>(m_World->GetEntitiesWithComponent<ECS::Sprite2DComponent>().size());
        } else if (type == "particle") {
            n = static_cast<u32>(m_World->GetEntitiesWithComponent<ECS::ParticleEmitterComponent>().size());
        } else if (type == "tween") {
            n = static_cast<u32>(m_World->GetEntitiesWithComponent<ECS::TweenComponent>().size());
        } else if (type == "lod") {
            n = static_cast<u32>(m_World->GetEntitiesWithComponent<ECS::LODComponent>().size());
        } else if (type == "transform") {
            n = static_cast<u32>(m_World->GetEntitiesWithComponent<ECS::TransformComponent>().size());
        } else {
            m_ConsoleLog.push_back("Usage: count <component>");
            m_ConsoleLog.push_back("  Types: mesh, material, light, camera, script, audio, rigidbody, name, notes, sprite, particle, tween, lod, transform");
            return;
        }
        m_ConsoleLog.push_back("Entities with " + type + ": " + std::to_string(n));
    } else if (cmdLower == "children") {
        if (m_SelectedEntities.empty() || !m_World) {
            m_ConsoleLog.push_back("No entity selected");
            return;
        }
        ECS::Entity e = m_PrimarySelected;
        const auto& kids = ECS::GetChildren(m_World, e);
        if (kids.empty()) {
            m_ConsoleLog.push_back("Entity has no children");
        } else {
            m_ConsoleLog.push_back("Children (" + std::to_string(kids.size()) + "):");
            for (ECS::Entity child : kids) {
                std::string name = "Entity " + std::to_string(child);
                if (auto* nc = m_World->GetComponent<ECS::NameComponent>(child)) name = nc->name;
                m_ConsoleLog.push_back("  [" + std::to_string(child) + "] " + name);
            }
        }
    } else if (cmdLower == "parent") {
        if (m_SelectedEntities.empty() || !m_World) {
            m_ConsoleLog.push_back("No entity selected");
            return;
        }
        ECS::Entity e = m_PrimarySelected;
        ECS::Entity p = ECS::GetParent(m_World, e);
        if (p == ECS::INVALID_ENTITY) {
            m_ConsoleLog.push_back("Entity has no parent (root entity)");
        } else {
            std::string name = "Entity " + std::to_string(p);
            if (auto* nc = m_World->GetComponent<ECS::NameComponent>(p)) name = nc->name;
            m_ConsoleLog.push_back("Parent: [" + std::to_string(p) + "] " + name);
        }
    // =====================================================================
    // Camera commands
    // =====================================================================
    } else if (cmdLower == "fov") {
        if (m_SelectedEntities.empty() || !m_World) {
            m_ConsoleLog.push_back("No entity selected");
            return;
        }
        f32 degrees;
        if (iss >> degrees) {
            auto* cam = m_World->GetComponent<ECS::CameraComponent>(m_PrimarySelected);
            if (!cam) {
                m_ConsoleLog.push_back("Selected entity has no CameraComponent");
            } else {
                cam->fieldOfView = degrees;
                m_ConsoleLog.push_back("Set FOV to " + std::to_string(degrees) + " degrees");
            }
        } else {
            m_ConsoleLog.push_back("Usage: fov <degrees>");
        }
    } else if (cmdLower == "near") {
        if (m_SelectedEntities.empty() || !m_World) {
            m_ConsoleLog.push_back("No entity selected");
            return;
        }
        f32 value;
        if (iss >> value) {
            auto* cam = m_World->GetComponent<ECS::CameraComponent>(m_PrimarySelected);
            if (!cam) {
                m_ConsoleLog.push_back("Selected entity has no CameraComponent");
            } else {
                cam->nearPlane = value;
                m_ConsoleLog.push_back("Set near plane to " + std::to_string(value));
            }
        } else {
            m_ConsoleLog.push_back("Usage: near <value>");
        }
    } else if (cmdLower == "far") {
        if (m_SelectedEntities.empty() || !m_World) {
            m_ConsoleLog.push_back("No entity selected");
            return;
        }
        f32 value;
        if (iss >> value) {
            auto* cam = m_World->GetComponent<ECS::CameraComponent>(m_PrimarySelected);
            if (!cam) {
                m_ConsoleLog.push_back("Selected entity has no CameraComponent");
            } else {
                cam->farPlane = value;
                m_ConsoleLog.push_back("Set far plane to " + std::to_string(value));
            }
        } else {
            m_ConsoleLog.push_back("Usage: far <value>");
        }
    // =====================================================================
    // Bulk commands
    // =====================================================================
    } else if (cmdLower == "selectall") {
        if (!m_World) {
            m_ConsoleLog.push_back("Error: No world loaded");
            return;
        }
        ClearSelection();
        const auto& entities = m_World->GetAllEntities();
        for (ECS::Entity entity : entities) {
            SelectEntity(entity, true);
        }
        m_ConsoleLog.push_back("Selected " + std::to_string(entities.size()) + " entities");
    } else if (cmdLower == "hideall") {
        if (!m_World) {
            m_ConsoleLog.push_back("Error: No world loaded");
            return;
        }
        u32 count = 0;
        for (ECS::Entity entity : m_World->GetEntitiesWithComponent<ECS::TransformComponent>()) {
            auto* t = m_World->GetComponent<ECS::TransformComponent>(entity);
            if (t && t->visible) {
                t->visible = false;
                count++;
            }
        }
        m_ConsoleLog.push_back("Hidden " + std::to_string(count) + " entities");
    } else if (cmdLower == "showall") {
        if (!m_World) {
            m_ConsoleLog.push_back("Error: No world loaded");
            return;
        }
        u32 count = 0;
        for (ECS::Entity entity : m_World->GetEntitiesWithComponent<ECS::TransformComponent>()) {
            auto* t = m_World->GetComponent<ECS::TransformComponent>(entity);
            if (t && !t->visible) {
                t->visible = true;
                count++;
            }
        }
        m_ConsoleLog.push_back("Shown " + std::to_string(count) + " entities");
    } else if (cmdLower == "deleteall") {
        if (!m_World) {
            m_ConsoleLog.push_back("Error: No world loaded");
            return;
        }
        std::string confirm;
        iss >> confirm;
        for (auto& c : confirm) c = static_cast<char>(std::tolower(c));
        if (confirm != "confirm") {
            m_ConsoleLog.push_back("WARNING: This will delete ALL entities in the scene.");
            m_ConsoleLog.push_back("Type 'deleteall confirm' to proceed.");
            return;
        }
        const auto& entities = m_World->GetAllEntities();
        usize count = entities.size();
        // Copy entity list since destruction modifies it
        std::vector<ECS::Entity> toDelete(entities.begin(), entities.end());
        ClearSelection();
        for (ECS::Entity entity : toDelete) {
            m_World->DestroyEntity(entity);
        }
        m_ConsoleLog.push_back("Deleted " + std::to_string(count) + " entities");
    // =====================================================================
    // System commands
    // =====================================================================
    } else if (cmdLower == "shadowres") {
        u32 size;
        if (iss >> size) {
            if (size != 512 && size != 1024 && size != 2048 && size != 4096) {
                m_ConsoleLog.push_back("Shadow resolution must be 512, 1024, 2048, or 4096");
            } else if (m_RenderSystem) {
                m_RenderSystem->SetShadowResolution(size);
                m_ConsoleLog.push_back("Shadow resolution set to " + std::to_string(size));
            }
        } else {
            m_ConsoleLog.push_back("Usage: shadowres <512|1024|2048|4096>");
        }
    } else if (cmdLower == "shadowdist") {
        f32 dist;
        if (iss >> dist) {
            if (m_RenderSystem) {
                m_RenderSystem->SetShadowDistance(dist);
                m_ConsoleLog.push_back("Shadow distance set to " + std::to_string(dist));
            }
        } else {
            m_ConsoleLog.push_back("Usage: shadowdist <distance>");
        }
    } else if (cmdLower == "ambient_intensity") {
        f32 value;
        if (iss >> value) {
            if (m_RenderSystem) {
                m_RenderSystem->SetAmbientIntensity(value);
                m_ConsoleLog.push_back("Ambient intensity set to " + std::to_string(value));
            }
        } else {
            m_ConsoleLog.push_back("Usage: ambient_intensity <value>");
        }
    } else if (cmdLower == "curvature") {
        f32 value;
        if (iss >> value) {
            if (m_RenderSystem) {
                m_RenderSystem->SetWorldCurvature(value);
                m_ConsoleLog.push_back("World curvature set to " + std::to_string(value));
            }
        } else {
            m_ConsoleLog.push_back("Usage: curvature <value>");
        }
    } else {
        m_ConsoleLog.push_back("Unknown command: " + cmd + " (type 'help' for commands)");
    }

    // Trim console log
    while (m_ConsoleLog.size() > MAX_CONSOLE_LINES) {
        m_ConsoleLog.erase(m_ConsoleLog.begin());
    }
}


void EditorLayer::DrawParticleEditorPanel() {
    bool panelOpen = true;
    if (!ImGui::Begin("Particle Editor", &panelOpen)) {
        if (!panelOpen) SetPanelVisibility(EditorPanel::ParticleEditor, false);
        ImGui::End();
        return;
    }
    if (!panelOpen) { SetPanelVisibility(EditorPanel::ParticleEditor, false); ImGui::End(); return; }

    if (!m_World) {
        DrawEmptyState("[ ]", "No World Loaded", "Open or create a scene to begin");
        ImGui::End();
        return;
    }

    // Operate on selected entity's ParticleEmitterComponent
    ECS::Entity entity = m_PrimarySelected;
    ECS::ParticleEmitterComponent* emitter = nullptr;
    ECS::TransformComponent* transform = nullptr;

    if (entity != ECS::INVALID_ENTITY) {
        emitter = m_World->GetComponent<ECS::ParticleEmitterComponent>(entity);
        transform = m_World->GetComponent<ECS::TransformComponent>(entity);
    }

    if (!emitter) {
        ImGui::TextDisabled("Select an entity with a Particle Emitter component");
        ImGui::End();
        return;
    }

    // --- Stats ---
    auto* nameComp = m_World->GetComponent<ECS::NameComponent>(entity);
    if (nameComp) {
        ImGui::Text("Entity: %s", nameComp->name.c_str());
    }
    ImGui::Text("Active: %u / %u", emitter->pool.activeCount, emitter->pool.maxParticles);
    ImGui::Text("Total Active: %u  Emitters: %u",
        m_ParticleSystem.GetTotalActiveParticles(),
        m_ParticleSystem.GetTotalEmitterCount());
    ImGui::Separator();

    // --- Playback Controls ---
    if (emitter->isPlaying) {
        if (ImGui::Button("Pause")) {
            emitter->isPlaying = false;
        }
    } else {
        if (ImGui::Button("Play")) {
            emitter->isPlaying = true;
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Restart")) {
        emitter->pool.activeCount = 0;
        emitter->pool.spawnAccumulator = 0.0f;
        emitter->pool.burstTimer = 0.0f;
        emitter->pool.systemAge = 0.0f;
        emitter->isPlaying = true;
    }
    ImGui::Separator();

    // --- Presets ---
    if (ImGui::CollapsingHeader("Presets", ImGuiTreeNodeFlags_DefaultOpen)) {
        const char* presets[] = { "Fire", "Smoke", "Sparks", "Snow", "Rain", "Magic", "Explosion" };
        for (int i = 0; i < 7; ++i) {
            if (i > 0) ImGui::SameLine();
            if (ImGui::Button(presets[i])) {
                ECS::ApplyParticlePreset(*emitter, presets[i]);
            }
        }
        ImGui::Spacing();
        ImGui::TextDisabled("Liquids:");
        const char* liquidPresets[] = { "Water Splash", "Blood/Sap", "Lava", "Fountain", "Drip" };
        for (int i = 0; i < 5; ++i) {
            if (i > 0) ImGui::SameLine();
            if (ImGui::Button(liquidPresets[i])) {
                ECS::ApplyParticlePreset(*emitter, liquidPresets[i]);
            }
        }
    }

    // --- Color Gradient ---
    if (ImGui::CollapsingHeader("Color Gradient")) {
        f32 startCol[3] = { emitter->startColor.x, emitter->startColor.y, emitter->startColor.z };
        if (ImGui::ColorEdit3("Start Color##pe", startCol)) {
            emitter->startColor = Math::Vector3(startCol[0], startCol[1], startCol[2]);
        }
        f32 endCol[3] = { emitter->endColor.x, emitter->endColor.y, emitter->endColor.z };
        if (ImGui::ColorEdit3("End Color##pe", endCol)) {
            emitter->endColor = Math::Vector3(endCol[0], endCol[1], endCol[2]);
        }

        ImGui::DragFloat("Start Alpha##pe", &emitter->startAlpha, 0.01f, 0.0f, 1.0f);
        ImGui::DragFloat("End Alpha##pe", &emitter->endAlpha, 0.01f, 0.0f, 1.0f);

        // Gradient preview bar
        ImVec2 barPos = ImGui::GetCursorScreenPos();
        f32 barWidth = ImGui::GetContentRegionAvail().x;
        f32 barHeight = 20.0f;
        ImDrawList* drawList = ImGui::GetWindowDrawList();

        const int segments = 32;
        for (int i = 0; i < segments; ++i) {
            f32 t0 = static_cast<f32>(i) / segments;
            f32 t1 = static_cast<f32>(i + 1) / segments;
            f32 r0 = emitter->startColor.x + (emitter->endColor.x - emitter->startColor.x) * t0;
            f32 g0 = emitter->startColor.y + (emitter->endColor.y - emitter->startColor.y) * t0;
            f32 b0 = emitter->startColor.z + (emitter->endColor.z - emitter->startColor.z) * t0;
            f32 a0 = emitter->startAlpha + (emitter->endAlpha - emitter->startAlpha) * t0;
            f32 r1 = emitter->startColor.x + (emitter->endColor.x - emitter->startColor.x) * t1;
            f32 g1 = emitter->startColor.y + (emitter->endColor.y - emitter->startColor.y) * t1;
            f32 b1 = emitter->startColor.z + (emitter->endColor.z - emitter->startColor.z) * t1;
            f32 a1 = emitter->startAlpha + (emitter->endAlpha - emitter->startAlpha) * t1;

            f32 x0 = barPos.x + t0 * barWidth;
            f32 x1 = barPos.x + t1 * barWidth;

            ImU32 col0 = IM_COL32(
                static_cast<u8>(r0 * 255), static_cast<u8>(g0 * 255),
                static_cast<u8>(b0 * 255), static_cast<u8>(a0 * 255));
            ImU32 col1 = IM_COL32(
                static_cast<u8>(r1 * 255), static_cast<u8>(g1 * 255),
                static_cast<u8>(b1 * 255), static_cast<u8>(a1 * 255));

            drawList->AddRectFilledMultiColor(
                ImVec2(x0, barPos.y), ImVec2(x1, barPos.y + barHeight),
                col0, col1, col1, col0);
        }
        drawList->AddRect(barPos, ImVec2(barPos.x + barWidth, barPos.y + barHeight),
            IM_COL32(128, 128, 128, 255));
        ImGui::Dummy(ImVec2(barWidth, barHeight + 4));
    }

    // --- Size Over Lifetime ---
    if (ImGui::CollapsingHeader("Size Over Lifetime")) {
        ImGui::DragFloat("Start Size##pe", &emitter->startSize, 0.01f, 0.001f, 10.0f);
        ImGui::DragFloat("Mid Size##pe", &emitter->sizeMid, 0.01f, -1.0f, 10.0f, "%.3f (-1=auto)");
        ImGui::DragFloat("End Size##pe", &emitter->endSize, 0.01f, 0.0f, 10.0f);

        // Curve visualization
        f32 midVal = emitter->sizeMid >= 0.0f ? emitter->sizeMid
            : (emitter->startSize + emitter->endSize) * 0.5f;
        f32 maxSize = std::max({emitter->startSize, midVal, emitter->endSize, 0.01f});

        ImVec2 curvePos = ImGui::GetCursorScreenPos();
        f32 curveW = ImGui::GetContentRegionAvail().x;
        f32 curveH = 50.0f;
        ImDrawList* dl = ImGui::GetWindowDrawList();

        dl->AddRectFilled(curvePos, ImVec2(curvePos.x + curveW, curvePos.y + curveH),
            IM_COL32(30, 30, 30, 255));
        dl->AddRect(curvePos, ImVec2(curvePos.x + curveW, curvePos.y + curveH),
            IM_COL32(80, 80, 80, 255));

        // Draw piecewise linear curve
        f32 y0 = curvePos.y + curveH - (emitter->startSize / maxSize) * curveH;
        f32 y1 = curvePos.y + curveH - (midVal / maxSize) * curveH;
        f32 y2 = curvePos.y + curveH - (emitter->endSize / maxSize) * curveH;
        dl->AddLine(ImVec2(curvePos.x, y0), ImVec2(curvePos.x + curveW * 0.5f, y1),
            IM_COL32(100, 200, 255, 255), 2.0f);
        dl->AddLine(ImVec2(curvePos.x + curveW * 0.5f, y1),
            ImVec2(curvePos.x + curveW, y2), IM_COL32(100, 200, 255, 255), 2.0f);

        ImGui::Dummy(ImVec2(curveW, curveH + 4));
    }

    // --- Speed Over Lifetime ---
    if (ImGui::CollapsingHeader("Speed Over Lifetime")) {
        ImGui::DragFloat("Mid Multiplier##spd", &emitter->speedMultiplierMid, 0.01f, 0.0f, 5.0f);
        ImGui::DragFloat("End Multiplier##spd", &emitter->speedMultiplierEnd, 0.01f, 0.0f, 5.0f);

        f32 maxSpd = std::max({1.0f, emitter->speedMultiplierMid, emitter->speedMultiplierEnd, 0.01f});

        ImVec2 curvePos = ImGui::GetCursorScreenPos();
        f32 curveW = ImGui::GetContentRegionAvail().x;
        f32 curveH = 50.0f;
        ImDrawList* dl = ImGui::GetWindowDrawList();

        dl->AddRectFilled(curvePos, ImVec2(curvePos.x + curveW, curvePos.y + curveH),
            IM_COL32(30, 30, 30, 255));
        dl->AddRect(curvePos, ImVec2(curvePos.x + curveW, curvePos.y + curveH),
            IM_COL32(80, 80, 80, 255));

        f32 y0 = curvePos.y + curveH - (1.0f / maxSpd) * curveH;
        f32 y1 = curvePos.y + curveH - (emitter->speedMultiplierMid / maxSpd) * curveH;
        f32 y2 = curvePos.y + curveH - (emitter->speedMultiplierEnd / maxSpd) * curveH;
        dl->AddLine(ImVec2(curvePos.x, y0), ImVec2(curvePos.x + curveW * 0.5f, y1),
            IM_COL32(255, 200, 100, 255), 2.0f);
        dl->AddLine(ImVec2(curvePos.x + curveW * 0.5f, y1),
            ImVec2(curvePos.x + curveW, y2), IM_COL32(255, 200, 100, 255), 2.0f);

        ImGui::Dummy(ImVec2(curveW, curveH + 4));
    }

    // --- Shape Preview ---
    if (ImGui::CollapsingHeader("Shape Preview")) {
        const char* shapes[] = { "Point", "Sphere", "Hemisphere", "Cone", "Box" };
        int shape = static_cast<int>(emitter->shape);
        if (ImGui::Combo("Shape##pe", &shape, shapes, 5)) {
            emitter->shape = static_cast<ECS::ParticleEmitterComponent::EmitterShape>(shape);
        }
        ImGui::DragFloat("Radius##pe", &emitter->shapeRadius, 0.05f, 0.0f, 50.0f);
        if (emitter->shape == ECS::ParticleEmitterComponent::EmitterShape::Cone) {
            ImGui::DragFloat("Cone Angle##pe", &emitter->coneAngle, 1.0f, 0.0f, 90.0f);
        }

        // 2D wireframe shape preview
        ImVec2 previewPos = ImGui::GetCursorScreenPos();
        f32 previewSize = 80.0f;
        f32 cx = previewPos.x + previewSize;
        f32 cy = previewPos.y + previewSize;
        ImDrawList* dl = ImGui::GetWindowDrawList();

        dl->AddRectFilled(previewPos,
            ImVec2(previewPos.x + previewSize * 2, previewPos.y + previewSize * 2),
            IM_COL32(20, 20, 20, 255));

        ImU32 shapeColor = IM_COL32(100, 200, 100, 180);
        f32 scale = previewSize * 0.7f;

        switch (emitter->shape) {
            case ECS::ParticleEmitterComponent::EmitterShape::Point:
                dl->AddCircleFilled(ImVec2(cx, cy), 3.0f, shapeColor);
                break;
            case ECS::ParticleEmitterComponent::EmitterShape::Sphere:
                dl->AddCircle(ImVec2(cx, cy), scale * 0.5f, shapeColor, 32, 1.5f);
                break;
            case ECS::ParticleEmitterComponent::EmitterShape::Hemisphere: {
                // Draw upper half circle
                for (int i = 0; i < 16; ++i) {
                    f32 a0 = 3.14159265f + (3.14159265f * i / 16.0f);
                    f32 a1 = 3.14159265f + (3.14159265f * (i + 1) / 16.0f);
                    f32 r = scale * 0.5f;
                    dl->AddLine(
                        ImVec2(cx + std::cos(a0) * r, cy + std::sin(a0) * r),
                        ImVec2(cx + std::cos(a1) * r, cy + std::sin(a1) * r),
                        shapeColor, 1.5f);
                }
                dl->AddLine(ImVec2(cx - scale * 0.5f, cy), ImVec2(cx + scale * 0.5f, cy),
                    shapeColor, 1.5f);
                break;
            }
            case ECS::ParticleEmitterComponent::EmitterShape::Cone: {
                f32 angleRad = emitter->coneAngle * (3.14159265f / 180.0f);
                f32 halfW = std::sin(angleRad) * scale * 0.6f;
                f32 h = std::cos(angleRad) * scale * 0.6f;
                dl->AddLine(ImVec2(cx, cy), ImVec2(cx - halfW, cy - h), shapeColor, 1.5f);
                dl->AddLine(ImVec2(cx, cy), ImVec2(cx + halfW, cy - h), shapeColor, 1.5f);
                dl->AddLine(ImVec2(cx - halfW, cy - h), ImVec2(cx + halfW, cy - h), shapeColor, 1.5f);
                break;
            }
            case ECS::ParticleEmitterComponent::EmitterShape::Box: {
                f32 half = scale * 0.4f;
                dl->AddRect(ImVec2(cx - half, cy - half), ImVec2(cx + half, cy + half),
                    shapeColor, 0.0f, 0, 1.5f);
                break;
            }
        }

        ImGui::Dummy(ImVec2(previewSize * 2, previewSize * 2 + 4));
    }

    // --- Additional Settings ---
    if (ImGui::CollapsingHeader("Emission")) {
        ImGui::DragFloat("Rate##pe", &emitter->emissionRate, 0.5f, 0.0f, 1000.0f);
        ImGui::DragInt("Burst Count##pe", &emitter->burstCount, 1, 0, 100);
        if (emitter->burstCount > 0) {
            ImGui::DragFloat("Burst Interval##pe", &emitter->burstInterval, 0.1f, 0.0f, 30.0f);
        }
        int maxP = static_cast<int>(emitter->maxParticles);
        if (ImGui::DragInt("Max Particles##pe", &maxP, 16, 16, 16384)) {
            emitter->maxParticles = static_cast<u32>(std::max(16, maxP));
        }
    }

    if (ImGui::CollapsingHeader("Rotation")) {
        ImGui::DragFloat("Start Rotation##pe", &emitter->startRotation, 0.01f, -6.28f, 6.28f);
        ImGui::DragFloat("Rotation Variance##pe", &emitter->rotationVariance, 0.01f, 0.0f, 6.28f);
        ImGui::DragFloat("Rotation Speed##pe", &emitter->rotationSpeed, 0.01f, -10.0f, 10.0f);
        ImGui::DragFloat("Speed Variance##pe2", &emitter->rotationSpeedVariance, 0.01f, 0.0f, 10.0f);
    }

    if (ImGui::CollapsingHeader("Forces##pe")) {
        f32 grav[3] = { emitter->gravity.x, emitter->gravity.y, emitter->gravity.z };
        if (ImGui::DragFloat3("Gravity##pe", grav, 0.1f)) {
            emitter->gravity = Math::Vector3(grav[0], grav[1], grav[2]);
        }
        ImGui::DragFloat("Drag##pe", &emitter->drag, 0.01f, 0.0f, 10.0f);
    }

    if (ImGui::CollapsingHeader("Rendering##pe")) {
        const char* renderModes[] = { "Billboard", "Velocity Stretch" };
        int currentMode = static_cast<int>(emitter->renderMode);
        if (ImGui::Combo("Render Mode##pe", &currentMode, renderModes, 2)) {
            emitter->renderMode = static_cast<ECS::ParticleEmitterComponent::RenderMode>(currentMode);
        }
        if (emitter->renderMode == ECS::ParticleEmitterComponent::RenderMode::VelocityStretch) {
            ImGui::DragFloat("Stretch Scale##pe", &emitter->velocityStretchScale, 0.01f, 0.0f, 5.0f);
        }
    }

    ImGui::End();
}

void EditorLayer::DrawAnimGraphPanel() {
    bool panelOpen = true;
    if (!ImGui::Begin("Animation Graph", &panelOpen)) {
        if (!panelOpen) SetPanelVisibility(EditorPanel::AnimGraph, false);
        ImGui::End();
        return;
    }
    if (!panelOpen) { SetPanelVisibility(EditorPanel::AnimGraph, false); ImGui::End(); return; }

    if (!m_World) {
        DrawEmptyState("[ ]", "No World Loaded", "Open or create a scene to begin");
        ImGui::End();
        return;
    }

    // Auto-target selected entity if it has a StateMachineComponent
    ECS::Entity target = m_AnimGraphEditor.GetTargetEntity();
    if (m_PrimarySelected != ECS::INVALID_ENTITY && m_PrimarySelected != target) {
        auto* sm = m_World->GetComponent<ECS::StateMachineComponent>(m_PrimarySelected);
        if (sm) {
            m_AnimGraphEditor.SetTarget(m_World, m_PrimarySelected);
        }
    }

    bool isPlaying = m_PlayMode.IsPlaying() || m_PlayMode.IsPaused();
    m_AnimGraphEditor.Render(m_EditorSettings, isPlaying);

    ImGui::End();
}

void EditorLayer::DrawDialoguePanel() {
    bool panelOpen = true;
    if (!ImGui::Begin("Dialogue Editor", &panelOpen)) {
        if (!panelOpen) SetPanelVisibility(EditorPanel::Dialogue, false);
        ImGui::End();
        return;
    }
    if (!panelOpen) { SetPanelVisibility(EditorPanel::Dialogue, false); ImGui::End(); return; }

    if (!m_World) {
        DrawEmptyState("[ ]", "No World Loaded", "Open or create a scene to begin");
        ImGui::End();
        return;
    }

    // Left sidebar: list entities with DialogueComponent
    ImGui::BeginChild("DialogueEntityList", ImVec2(200, 0), true);
    ImGui::Text("Entities");
    ImGui::Separator();

    std::vector<ECS::Entity> dialogueEntities;
    for (ECS::Entity entity : m_World->GetEntitiesWithComponent<ECS::DialogueComponent>()) {
        dialogueEntities.push_back(entity);
    }

    if (dialogueEntities.empty()) {
        DrawEmptyState("...", "No Dialogue Trees", "Add a DialogueComponent to an entity to begin");
    } else {
        for (ECS::Entity entity : dialogueEntities) {
            std::string name = "Entity " + std::to_string(entity);
            auto* nameComp = m_World->GetComponent<ECS::NameComponent>(entity);
            if (nameComp && !nameComp->name.empty()) {
                name = nameComp->name;
            }

            bool isSelected = (m_DialogueEditorEntity == entity);
            if (ImGui::Selectable(name.c_str(), isSelected)) {
                m_DialogueEditorEntity = entity;
                auto* dlg = m_World->GetComponent<ECS::DialogueComponent>(entity);
                if (dlg && dlg->IsTreeMode()) {
                    m_DialogueTreeEditor.SetTree(&dlg->dialogueTree);
                    m_DialogueTreeEditor.SetOpen(true);
                }
            }
        }
    }

    ImGui::EndChild();

    ImGui::SameLine();

    // Right side: dialogue tree editor for selected entity
    ImGui::BeginChild("DialogueTreeArea", ImVec2(0, 0), true);

    // Auto-select primary selected entity if it has DialogueComponent
    if (m_PrimarySelected != ECS::INVALID_ENTITY && m_PrimarySelected != m_DialogueEditorEntity) {
        auto* dlg = m_World->GetComponent<ECS::DialogueComponent>(m_PrimarySelected);
        if (dlg && dlg->IsTreeMode()) {
            m_DialogueEditorEntity = m_PrimarySelected;
            m_DialogueTreeEditor.SetTree(&dlg->dialogueTree);
            m_DialogueTreeEditor.SetOpen(true);
        }
    }

    if (m_DialogueEditorEntity == ECS::INVALID_ENTITY) {
        ImGui::TextDisabled("Select an entity with\nDialogueComponent");
    } else {
        auto* dlg = m_World->GetComponent<ECS::DialogueComponent>(m_DialogueEditorEntity);
        if (!dlg) {
            ImGui::TextDisabled("DialogueComponent removed");
            m_DialogueEditorEntity = ECS::INVALID_ENTITY;
        } else if (!dlg->IsTreeMode()) {
            ImGui::TextDisabled("Entity is using legacy\ndialogue mode");
            ImGui::Separator();
            if (ImGui::Button("Convert to Tree Mode")) {
                // Initialize an empty tree
                dlg->dialogueTree.treeName = "Dialogue";
                dlg->dialogueTree.nextId = 1;
                dlg->dialogueTree.nodes.clear();

                // Add a root node
                u32 rootId = dlg->dialogueTree.AddNode(GUI::DialogueNodeType::Root);
                dlg->dialogueTree.rootNodeId = rootId;
                if (auto* rootNode = dlg->dialogueTree.GetNode(rootId)) {
                    rootNode->editorPosition = Math::Vector2(50, 150);
                }

                // Add an end node
                u32 endId = dlg->dialogueTree.AddNode(GUI::DialogueNodeType::End);
                if (auto* endNode = dlg->dialogueTree.GetNode(endId)) {
                    endNode->editorPosition = Math::Vector2(400, 150);
                }
                if (auto* rootNode = dlg->dialogueTree.GetNode(rootId)) {
                    rootNode->nextNodeId = endId;
                }

                m_DialogueTreeEditor.SetTree(&dlg->dialogueTree);
                m_DialogueTreeEditor.SetOpen(true);
            }
        } else {
            // Show entity name in header
            std::string headerName = "Editing: Entity " + std::to_string(m_DialogueEditorEntity);
            auto* nameComp = m_World->GetComponent<ECS::NameComponent>(m_DialogueEditorEntity);
            if (nameComp && !nameComp->name.empty()) {
                headerName = "Editing: " + nameComp->name;
            }
            ImGui::Text("%s", headerName.c_str());

            // Tree name edit
            char treeBuf[128];
            strncpy(treeBuf, dlg->dialogueTree.treeName.c_str(), sizeof(treeBuf) - 1);
            treeBuf[sizeof(treeBuf) - 1] = '\0';
            if (ImGui::InputText("Tree Name", treeBuf, sizeof(treeBuf))) {
                dlg->dialogueTree.treeName = treeBuf;
            }

            // Import / Export toolbar
            ImGui::SameLine();
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 10);

            if (ImGui::Button("Import")) {
                ImGui::OpenPopup("DialogueImportPopup");
            }
            ImGui::SameLine();
            if (ImGui::Button("Export")) {
                ImGui::OpenPopup("DialogueExportPopup");
            }

            // Import popup
            if (ImGui::BeginPopup("DialogueImportPopup")) {
                if (ImGui::MenuItem("Yarn Spinner (.yarn)")) {
                    std::vector<FileFilter> filters = {
                        { "Yarn Spinner", "*.yarn" },
                        { "All Files", "*.*" }
                    };
                    std::string path = FileDialog::OpenFile("Import Yarn Spinner", filters);
                    if (!path.empty()) {
                        GUI::DialogueTreeData importedTree;
                        auto result = GUI::YarnSpinnerIO::Import(path, importedTree);
                        if (result.success) {
                            dlg->dialogueTree = importedTree;
                            m_DialogueTreeEditor.SetTree(&dlg->dialogueTree);
                            ENJIN_LOG_INFO(Editor, "Imported Yarn: %u nodes, %u links",
                                           result.nodeCount, result.linkCount);
                        } else {
                            ENJIN_LOG_ERROR(Editor, "Yarn import failed: %s",
                                            result.errorMessage.c_str());
                        }
                        for (auto& w : result.warnings) {
                            ENJIN_LOG_WARN(Editor, "Yarn import: %s", w.c_str());
                        }
                    }
                }
                if (ImGui::MenuItem("Twine (.twee / .html)")) {
                    std::vector<FileFilter> filters = {
                        { "Twine Files", "*.twee;*.tw;*.html" },
                        { "All Files", "*.*" }
                    };
                    std::string path = FileDialog::OpenFile("Import Twine", filters);
                    if (!path.empty()) {
                        GUI::DialogueTreeData importedTree;
                        auto result = GUI::TwineIO::Import(path, importedTree);
                        if (result.success) {
                            dlg->dialogueTree = importedTree;
                            m_DialogueTreeEditor.SetTree(&dlg->dialogueTree);
                            ENJIN_LOG_INFO(Editor, "Imported Twine: %u nodes, %u links",
                                           result.nodeCount, result.linkCount);
                        } else {
                            ENJIN_LOG_ERROR(Editor, "Twine import failed: %s",
                                            result.errorMessage.c_str());
                        }
                        for (auto& w : result.warnings) {
                            ENJIN_LOG_WARN(Editor, "Twine import: %s", w.c_str());
                        }
                    }
                }
                ImGui::EndPopup();
            }

            // Export popup
            if (ImGui::BeginPopup("DialogueExportPopup")) {
                if (ImGui::MenuItem("Yarn Spinner (.yarn)")) {
                    std::vector<FileFilter> filters = {
                        { "Yarn Spinner", "*.yarn" },
                        { "All Files", "*.*" }
                    };
                    std::string defaultName = dlg->dialogueTree.treeName + ".yarn";
                    std::string path = FileDialog::SaveFile("Export Yarn Spinner", filters, "", defaultName);
                    if (!path.empty()) {
                        auto result = GUI::YarnSpinnerIO::Export(path, dlg->dialogueTree);
                        if (result.success) {
                            ENJIN_LOG_INFO(Editor, "Exported Yarn: %u nodes to %s",
                                           result.nodeCount, path.c_str());
                        } else {
                            ENJIN_LOG_ERROR(Editor, "Yarn export failed: %s",
                                            result.errorMessage.c_str());
                        }
                    }
                }
                if (ImGui::MenuItem("Twee 3 (.twee)")) {
                    std::vector<FileFilter> filters = {
                        { "Twee 3", "*.twee;*.tw" },
                        { "All Files", "*.*" }
                    };
                    std::string defaultName = dlg->dialogueTree.treeName + ".twee";
                    std::string path = FileDialog::SaveFile("Export Twee", filters, "", defaultName);
                    if (!path.empty()) {
                        auto result = GUI::TwineIO::Export(path, dlg->dialogueTree);
                        if (result.success) {
                            ENJIN_LOG_INFO(Editor, "Exported Twee: %u nodes to %s",
                                           result.nodeCount, path.c_str());
                        } else {
                            ENJIN_LOG_ERROR(Editor, "Twee export failed: %s",
                                            result.errorMessage.c_str());
                        }
                    }
                }
                ImGui::EndPopup();
            }

            ImGui::Separator();

            // Render the dialogue tree editor widget
            m_DialogueTreeEditor.Render();
        }
    }

    ImGui::EndChild();

    ImGui::End();
}

void EditorLayer::DrawVisualScriptPanel() {
    bool panelOpen = true;
    if (!ImGui::Begin("Visual Script Editor", &panelOpen)) {
        if (!panelOpen) SetPanelVisibility(EditorPanel::VisualScript, false);
        ImGui::End();
        return;
    }
    if (!panelOpen) { SetPanelVisibility(EditorPanel::VisualScript, false); ImGui::End(); return; }

    bool isPlaying = IsPlaying();

    // Wire undo manager (safe to call repeatedly)
    m_VisualScriptEditor.SetUndoManager(&m_UndoRedo);

    // Update target when selection changes
    if (m_PrimarySelected != ECS::INVALID_ENTITY &&
        m_World && m_World->HasComponent<ECS::VisualScriptComponent>(m_PrimarySelected)) {
        m_VisualScriptEditor.SetTarget(m_World, m_PrimarySelected);
    }

    // Render the visual script editor
    m_VisualScriptEditor.Render(m_EditorSettings, isPlaying);

    ImGui::End();
}

void EditorLayer::DrawPixelEditorPanel() {
    bool panelOpen = true;
    if (!ImGui::Begin("Pixel Editor", &panelOpen)) {
        if (!panelOpen) SetPanelVisibility(EditorPanel::PixelEditorPanel, false);
        ImGui::End();
        return;
    }
    if (!panelOpen) { SetPanelVisibility(EditorPanel::PixelEditorPanel, false); ImGui::End(); return; }

    // Export as Prefab button (top toolbar)
    if (m_PixelEditor.HasCanvas()) {
        if (ImGui::Button("Export as Prefab")) {
            ImGui::OpenPopup("ExportPrefabPopup");
        }
        ImGui::SameLine();
    }

    // Export popup with path input
    if (ImGui::BeginPopup("ExportPrefabPopup")) {
        static char exportPath[512] = "exported_sprite";
        ImGui::Text("Export Path (without extension):");
        ImGui::InputText("##exportpath", exportPath, sizeof(exportPath));
        if (ImGui::Button("Export")) {
            m_PixelEditor.ExportAsPrefab(exportPath);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    m_PixelEditor.Render(m_EditorSettings);

    ImGui::End();
}

void EditorLayer::DrawSpriteSheetImporterPanel() {
    bool panelOpen = true;
    if (!ImGui::Begin("Sprite Sheet Importer", &panelOpen)) {
        if (!panelOpen) SetPanelVisibility(EditorPanel::SpriteSheetImport, false);
        ImGui::End();
        return;
    }
    if (!panelOpen) { SetPanelVisibility(EditorPanel::SpriteSheetImport, false); ImGui::End(); return; }

    // Load button
    if (ImGui::Button("Load Sprite Sheet...")) {
        // Use native file dialog via nfd or fallback to text input
        // For now, use a simple text input path
        ImGui::OpenPopup("LoadSpriteSheetPath");
    }

    if (ImGui::BeginPopup("LoadSpriteSheetPath")) {
        static char pathBuf[512] = "";
        ImGui::Text("Image Path:");
        ImGui::InputText("##ssipath", pathBuf, sizeof(pathBuf));
        if (ImGui::Button("Load")) {
            if (m_SpriteSheetImporter.LoadImage(pathBuf)) {
                m_SpriteSheetResult = {};
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if (!m_SpriteSheetImporter.IsLoaded()) {
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "No sprite sheet loaded.");
        ImGui::End();
        return;
    }

    ImGui::Text("Image: %s (%ux%u)", m_SpriteSheetImporter.GetPath().c_str(),
                m_SpriteSheetImporter.GetWidth(), m_SpriteSheetImporter.GetHeight());
    ImGui::Separator();

    // Slicing mode
    ImGui::Checkbox("Auto-Detect", &m_SpriteSheetUseAutoDetect);

    if (!m_SpriteSheetUseAutoDetect) {
        // Grid slicing controls
        ImGui::Text("Grid Slicing:");
        bool changed = false;
        changed |= ImGui::InputScalar("Cell Width", ImGuiDataType_U32, &m_SpriteSheetGridW);
        changed |= ImGui::InputScalar("Cell Height", ImGuiDataType_U32, &m_SpriteSheetGridH);
        changed |= ImGui::InputScalar("Padding", ImGuiDataType_U32, &m_SpriteSheetPadding);

        // Clamp to reasonable values
        m_SpriteSheetGridW = std::max(1u, std::min(m_SpriteSheetGridW, m_SpriteSheetImporter.GetWidth()));
        m_SpriteSheetGridH = std::max(1u, std::min(m_SpriteSheetGridH, m_SpriteSheetImporter.GetHeight()));

        if (ImGui::Button("Slice Grid")) {
            m_SpriteSheetResult = m_SpriteSheetImporter.SliceGrid(
                m_SpriteSheetGridW, m_SpriteSheetGridH, m_SpriteSheetPadding);
        }
    } else {
        if (ImGui::Button("Auto-Detect Regions")) {
            m_SpriteSheetResult = m_SpriteSheetImporter.SliceAutoDetect();
        }
    }

    // Show results
    if (!m_SpriteSheetResult.slices.empty()) {
        ImGui::Separator();
        ImGui::Text("Slices: %zu", m_SpriteSheetResult.slices.size());

        // Preview grid of slice thumbnails
        ImGui::BeginChild("SlicePreview", ImVec2(0, 200), true);
        f32 thumbSize = 48.0f;
        f32 availWidth = ImGui::GetContentRegionAvail().x;
        u32 cols = std::max(1u, static_cast<u32>(availWidth / (thumbSize + 8.0f)));

        for (usize i = 0; i < m_SpriteSheetResult.slices.size(); i++) {
            const auto& slice = m_SpriteSheetResult.slices[i];
            if (i > 0 && (i % cols) != 0) ImGui::SameLine();

            ImGui::BeginGroup();
            // Draw a colored rect representing the frame
            ImVec2 cursor = ImGui::GetCursorScreenPos();
            ImGui::GetWindowDrawList()->AddRectFilled(
                cursor,
                ImVec2(cursor.x + thumbSize, cursor.y + thumbSize),
                IM_COL32(60, 80, 120, 255));
            ImGui::GetWindowDrawList()->AddRect(
                cursor,
                ImVec2(cursor.x + thumbSize, cursor.y + thumbSize),
                IM_COL32(150, 180, 220, 255));
            ImGui::Dummy(ImVec2(thumbSize, thumbSize));

            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s (%.0f,%.0f %.0fx%.0f)",
                    slice.name.c_str(), slice.x, slice.y, slice.width, slice.height);
            }
            ImGui::EndGroup();
        }
        ImGui::EndChild();

        // Apply to entity
        if (m_PrimarySelected != ECS::INVALID_ENTITY && m_World) {
            ImGui::Separator();
            static f32 animFps = 12.0f;
            static char animNameBuf[64] = "idle";
            ImGui::InputText("Animation Name", animNameBuf, sizeof(animNameBuf));
            ImGui::DragFloat("FPS", &animFps, 0.5f, 1.0f, 60.0f);

            if (ImGui::Button("Apply to Selected Entity")) {
                f32 frameDuration = (animFps > 0.0f) ? (1.0f / animFps) : 0.1f;

                // Add or update AnimatedSprite2D component
                if (!m_World->HasComponent<ECS::AnimatedSprite2DComponent>(m_PrimarySelected)) {
                    m_World->AddComponent<ECS::AnimatedSprite2DComponent>(m_PrimarySelected);
                }
                auto* animComp = m_World->GetComponent<ECS::AnimatedSprite2DComponent>(m_PrimarySelected);
                if (animComp) {
                    animComp->frames.clear();
                    for (const auto& slice : m_SpriteSheetResult.slices) {
                        ECS::AnimatedSprite2DComponent::Frame frame;
                        frame.srcX = slice.x;
                        frame.srcY = slice.y;
                        frame.duration = frameDuration;
                        animComp->frames.push_back(frame);
                    }
                    animComp->currentFrame = 0;
                    animComp->frameTimer = 0.0f;
                    animComp->playing = true;
                    animComp->loop = true;
                    ENJIN_LOG_INFO(Editor, "Applied animation '%s' with %zu frames to entity",
                                   animNameBuf, animComp->frames.size());
                }

                // Also set Sprite2D texture path if the entity has one
                if (m_World->HasComponent<ECS::Sprite2DComponent>(m_PrimarySelected)) {
                    auto* sprite = m_World->GetComponent<ECS::Sprite2DComponent>(m_PrimarySelected);
                    if (sprite) {
                        sprite->texturePath = m_SpriteSheetResult.texturePath;
                    }
                }
            }
        }
    }

    ImGui::End();
}

// ============================================================================
// Behavior Tree Panel
// ============================================================================

void EditorLayer::DrawBehaviorTreePanel() {
    bool panelOpen = true;
    if (!ImGui::Begin("Behavior Tree", &panelOpen)) {
        if (!panelOpen) SetPanelVisibility(EditorPanel::BehaviorTree, false);
        ImGui::End();
        return;
    }
    if (!panelOpen) { SetPanelVisibility(EditorPanel::BehaviorTree, false); ImGui::End(); return; }

    if (!m_World) {
        DrawEmptyState("[ ]", "No World Loaded", "Open or create a scene to begin");
        ImGui::End();
        return;
    }

    // Auto-target selected entity if it has a BehaviorTreeComponent
    ECS::Entity target = m_BehaviorTreeEditor.GetTargetEntity();
    if (m_PrimarySelected != ECS::INVALID_ENTITY && m_PrimarySelected != target) {
        if (m_World->HasComponent<ECS::BehaviorTreeComponent>(m_PrimarySelected)) {
            m_BehaviorTreeEditor.SetTarget(m_World, m_PrimarySelected);
        }
    }

    bool isPlaying = m_PlayMode.IsPlaying() || m_PlayMode.IsPaused();
    m_BehaviorTreeEditor.Render(m_EditorSettings, isPlaying);

    ImGui::End();
}

// ============================================================================
// Behavior Tree Component Inspector
// ============================================================================


void EditorLayer::DrawQuestFlowPanel() {
    bool panelOpen = true;
    if (!ImGui::Begin("Quest Flow", &panelOpen)) {
        if (!panelOpen) SetPanelVisibility(EditorPanel::QuestFlow, false);
        ImGui::End();
        return;
    }
    if (!panelOpen) { SetPanelVisibility(EditorPanel::QuestFlow, false); ImGui::End(); return; }

    if (!m_World) {
        DrawEmptyState("[ ]", "No World Loaded", "Open or create a scene to begin");
        ImGui::End();
        return;
    }

    // Auto-target selected entity if it has a QuestFlowComponent
    ECS::Entity target = m_QuestFlowEditor.GetTargetEntity();
    if (m_PrimarySelected != ECS::INVALID_ENTITY && m_PrimarySelected != target) {
        if (m_World->HasComponent<ECS::QuestFlowComponent>(m_PrimarySelected)) {
            m_QuestFlowEditor.SetTarget(m_World, m_PrimarySelected);
        }
    }

    bool isPlaying = m_PlayMode.IsPlaying() || m_PlayMode.IsPaused();
    m_QuestFlowEditor.Render(m_EditorSettings, isPlaying);

    ImGui::End();
}

// ============================================================================
// Quest Flow Component Inspector
// ============================================================================


void EditorLayer::LoadUserManual() {
    m_ManualSections.clear();
    m_ManualLoaded = false;

    // Try to find USER_MANUAL.md relative to the executable or in docs/
    std::vector<std::string> searchPaths = {
        "docs/USER_MANUAL.md",
        "../docs/USER_MANUAL.md",
        "../../docs/USER_MANUAL.md",
        "../../../docs/USER_MANUAL.md",
    };

    std::string content;
    for (const auto& path : searchPaths) {
        std::ifstream file(path);
        if (file.is_open()) {
            std::ostringstream ss;
            ss << file.rdbuf();
            content = ss.str();
            break;
        }
    }

    if (content.empty()) {
        ManualSection s;
        s.title = "Manual Not Found";
        s.content = "Could not locate docs/USER_MANUAL.md.\nMake sure the file exists in the docs/ directory relative to the executable.";
        s.level = 0;
        m_ManualSections.push_back(std::move(s));
        m_ManualLoaded = true;
        return;
    }

    // Parse markdown into sections by ## headers
    std::istringstream stream(content);
    std::string line;
    ManualSection current;
    bool hasSection = false;

    while (std::getline(stream, line)) {
        // Detect ## headers (level 2+)
        if (line.size() >= 3 && line[0] == '#' && line[1] == '#') {
            // Save previous section
            if (hasSection) {
                m_ManualSections.push_back(std::move(current));
                current = ManualSection{};
            }
            // Determine level
            int level = 0;
            while (level < (int)line.size() && line[level] == '#') level++;
            current.title = line.substr(level);
            // Trim leading spaces
            while (!current.title.empty() && current.title[0] == ' ') {
                current.title = current.title.substr(1);
            }
            current.level = level - 2; // ## = 0, ### = 1, etc.
            hasSection = true;
        } else if (hasSection) {
            current.content += line + "\n";
        }
    }
    // Push last section
    if (hasSection) {
        m_ManualSections.push_back(std::move(current));
    }

    // Mark level-0 sections with empty content to show children inline
    for (int i = 0; i < (int)m_ManualSections.size(); ++i) {
        auto& sec = m_ManualSections[i];
        if (sec.level == 0) {
            std::string trimmed = sec.content;
            trimmed.erase(0, trimmed.find_first_not_of(" \t\n\r"));
            if (trimmed.empty()) {
                sec.showChildrenInline = true;
            }
        }
    }

    m_ManualLoaded = true;
}

void EditorLayer::ExportManualAsHTML(const std::string& outputPath) {
    // Read the raw markdown file
    std::vector<std::string> searchPaths = {
        "docs/USER_MANUAL.md",
        "../docs/USER_MANUAL.md",
        "../../docs/USER_MANUAL.md",
        "../../../docs/USER_MANUAL.md",
    };

    std::string markdown;
    for (const auto& path : searchPaths) {
        std::ifstream file(path);
        if (file.is_open()) {
            std::ostringstream ss;
            ss << file.rdbuf();
            markdown = ss.str();
            break;
        }
    }

    if (markdown.empty()) {
        m_ConsoleLog.push_back("[Manual] ERROR: Could not find USER_MANUAL.md");
        return;
    }

    // Simple markdown -> HTML conversion
    std::ofstream out(outputPath);
    if (!out.is_open()) {
        m_ConsoleLog.push_back("[Manual] ERROR: Could not write to " + outputPath);
        return;
    }

    out << "<!DOCTYPE html>\n<html lang=\"en\">\n<head>\n";
    out << "<meta charset=\"UTF-8\">\n";
    out << "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n";
    out << "<title>Enjin Engine User Manual</title>\n";
    out << "<style>\n";
    out << "body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; ";
    out << "max-width: 900px; margin: 0 auto; padding: 20px 40px; color: #333; line-height: 1.6; }\n";
    out << "h1, h2, h3, h4 { color: #1a1a1a; }\n";
    out << "h1 { border-bottom: 2px solid #4a7c59; padding-bottom: 8px; }\n";
    out << "h2 { border-bottom: 1px solid #ddd; padding-bottom: 6px; margin-top: 2em; }\n";
    out << "code { background: #f4f4f4; padding: 2px 6px; border-radius: 3px; font-size: 0.9em; }\n";
    out << "pre { background: #1e1e1e; color: #d4d4d4; padding: 16px; border-radius: 6px; overflow-x: auto; }\n";
    out << "pre code { background: none; padding: 0; color: inherit; }\n";
    out << "table { border-collapse: collapse; width: 100%; margin: 1em 0; }\n";
    out << "th, td { border: 1px solid #ddd; padding: 8px 12px; text-align: left; }\n";
    out << "th { background: #f0f0f0; font-weight: 600; }\n";
    out << "tr:nth-child(even) { background: #fafafa; }\n";
    out << "hr { border: none; border-top: 1px solid #ddd; margin: 2em 0; }\n";
    out << "blockquote { border-left: 4px solid #4a7c59; margin: 1em 0; padding: 0.5em 1em; background: #f9f9f9; }\n";
    out << "@media print { body { max-width: none; } pre { white-space: pre-wrap; } }\n";
    out << "</style>\n</head>\n<body>\n";

    std::istringstream stream(markdown);
    std::string line;
    bool inCodeBlock = false;
    bool inTable = false;
    bool inList = false;

    while (std::getline(stream, line)) {
        // Code blocks
        if (line.find("```") != std::string::npos) {
            if (!inCodeBlock) {
                if (inList) { out << "</ul>\n"; inList = false; }
                out << "<pre><code>";
                inCodeBlock = true;
            } else {
                out << "</code></pre>\n";
                inCodeBlock = false;
            }
            continue;
        }

        if (inCodeBlock) {
            // HTML-escape code content
            for (char c : line) {
                if (c == '<') out << "&lt;";
                else if (c == '>') out << "&gt;";
                else if (c == '&') out << "&amp;";
                else out << c;
            }
            out << "\n";
            continue;
        }

        // Horizontal rules
        if (line == "---") {
            if (inList) { out << "</ul>\n"; inList = false; }
            if (inTable) { out << "</table>\n"; inTable = false; }
            out << "<hr>\n";
            continue;
        }

        // Headers
        if (!line.empty() && line[0] == '#') {
            if (inList) { out << "</ul>\n"; inList = false; }
            if (inTable) { out << "</table>\n"; inTable = false; }
            int level = 0;
            while (level < (int)line.size() && line[level] == '#') level++;
            std::string text = line.substr(level);
            while (!text.empty() && text[0] == ' ') text = text.substr(1);
            out << "<h" << level << ">" << text << "</h" << level << ">\n";
            continue;
        }

        // Table rows
        if (!line.empty() && line[0] == '|') {
            // Skip separator rows
            if (line.find("---") != std::string::npos) continue;

            if (!inTable) {
                if (inList) { out << "</ul>\n"; inList = false; }
                out << "<table>\n";
                inTable = true;
                // First table row is header
                out << "<tr>";
                std::istringstream cells(line);
                std::string cell;
                std::getline(cells, cell, '|'); // skip leading empty
                while (std::getline(cells, cell, '|')) {
                    // Trim
                    while (!cell.empty() && cell[0] == ' ') cell = cell.substr(1);
                    while (!cell.empty() && cell.back() == ' ') cell.pop_back();
                    if (!cell.empty()) out << "<th>" << cell << "</th>";
                }
                out << "</tr>\n";
                continue;
            }

            out << "<tr>";
            std::istringstream cells(line);
            std::string cell;
            std::getline(cells, cell, '|'); // skip leading empty
            while (std::getline(cells, cell, '|')) {
                while (!cell.empty() && cell[0] == ' ') cell = cell.substr(1);
                while (!cell.empty() && cell.back() == ' ') cell.pop_back();
                if (!cell.empty()) out << "<td>" << cell << "</td>";
            }
            out << "</tr>\n";
            continue;
        }

        // End table if we hit a non-table line
        if (inTable && (line.empty() || line[0] != '|')) {
            out << "</table>\n";
            inTable = false;
        }

        // Bullet lists
        if (line.size() >= 2 && line[0] == '-' && line[1] == ' ') {
            if (!inList) { out << "<ul>\n"; inList = true; }
            out << "<li>" << line.substr(2) << "</li>\n";
            continue;
        }

        // End list if non-list line
        if (inList && !line.empty() && !(line[0] == '-' && line.size() > 1 && line[1] == ' ')) {
            out << "</ul>\n";
            inList = false;
        }

        // Empty lines
        if (line.empty()) {
            continue;
        }

        // Regular paragraph
        out << "<p>" << line << "</p>\n";
    }

    if (inCodeBlock) out << "</code></pre>\n";
    if (inTable) out << "</table>\n";
    if (inList) out << "</ul>\n";

    out << "</body>\n</html>\n";
    out.close();
}

void EditorLayer::DrawUserManualPanel() {
    bool panelOpen = true;
    if (!ImGui::Begin("User Manual", &panelOpen)) {
        if (!panelOpen) SetPanelVisibility(EditorPanel::UserManual, false);
        ImGui::End();
        return;
    }
    if (!panelOpen) { SetPanelVisibility(EditorPanel::UserManual, false); ImGui::End(); return; }

    // Load on first access
    if (!m_ManualLoaded) {
        LoadUserManual();
    }

    // Toolbar: search bar + export button
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 160.0f);
    bool searchChanged = ImGui::InputTextWithHint("##manual_search", "Search manual...", m_ManualSearchBuf, sizeof(m_ManualSearchBuf));
    ImGui::SameLine();
    if (ImGui::Button("Export HTML", ImVec2(145, 0))) {
        std::string outputPath = "UserManual.html";
        ExportManualAsHTML(outputPath);
        m_ConsoleLog.push_back("[Manual] Exported to " + outputPath);
        // Open in default browser without invoking a shell
        std::string absPath = std::filesystem::absolute(outputPath).string();
#ifdef _WIN32
        ShellExecuteA(nullptr, "open", absPath.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#else
        auto spawnOpen = [](const char* opener, const std::string& path) {
            pid_t pid = fork();
            if (pid == 0) {
                execlp(opener, opener, path.c_str(), (char*)nullptr);
                _exit(1);
            }
        };
#ifdef __APPLE__
        spawnOpen("open", absPath);
#else
        spawnOpen("xdg-open", absPath);
#endif
#endif
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Export the user manual as an HTML file.\nOpen in a browser and use Print > Save as PDF.");
    }
    ImGui::Separator();

    std::string searchLower;
    if (m_ManualSearchBuf[0] != '\0') {
        searchLower = m_ManualSearchBuf;
        for (auto& c : searchLower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }

    // Two-column layout: section list on left, content on right
    float listWidth = 250.0f;
    ImGui::BeginChild("##manual_list", ImVec2(listWidth, 0), true);

    for (int i = 0; i < (int)m_ManualSections.size(); ++i) {
        const auto& section = m_ManualSections[i];

        // Filter by search
        if (!searchLower.empty()) {
            std::string titleLower = section.title;
            for (auto& c : titleLower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            std::string contentLower = section.content;
            for (auto& c : contentLower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

            if (titleLower.find(searchLower) == std::string::npos &&
                contentLower.find(searchLower) == std::string::npos) {
                continue;
            }
        }

        // Indent sub-sections
        float indent = section.level * 12.0f;
        if (indent > 0) ImGui::Indent(indent);

        bool selected = (m_ManualSelectedSection == i);
        if (ImGui::Selectable(section.title.c_str(), selected)) {
            m_ManualSelectedSection = i;
        }

        if (indent > 0) ImGui::Unindent(indent);
    }

    ImGui::EndChild();

    ImGui::SameLine();

    // Content display
    ImGui::BeginChild("##manual_content", ImVec2(0, 0), true);

    if (m_ManualSelectedSection >= 0 && m_ManualSelectedSection < (int)m_ManualSections.size()) {
        const auto& section = m_ManualSections[m_ManualSelectedSection];

        // Helper: render a line with inline **bold**, *italic*, and `code` formatting
        auto RenderMarkdownLine = [](const std::string& text) {
            ImFont* headingFont = (ImGui::GetIO().Fonts->Fonts.Size > 1) ? ImGui::GetIO().Fonts->Fonts[1] : nullptr;
            ImFont* monoFont = (ImGui::GetIO().Fonts->Fonts.Size > 2) ? ImGui::GetIO().Fonts->Fonts[2] : nullptr;

            size_t pos = 0;
            bool firstSegment = true;
            while (pos < text.size()) {
                // Find next special marker
                size_t boldPos = text.find("**", pos);
                size_t italicPos = std::string::npos;
                // Only match single * that isn't **
                for (size_t s = pos; s < text.size(); ++s) {
                    if (text[s] == '*' && (s + 1 >= text.size() || text[s + 1] != '*') &&
                        (s == 0 || text[s - 1] != '*')) {
                        italicPos = s;
                        break;
                    }
                }
                size_t codePos = text.find('`', pos);

                // Find earliest marker
                size_t earliest = std::min({boldPos, italicPos, codePos});
                if (earliest == std::string::npos) {
                    // No more markers — render rest as plain text
                    std::string rest = text.substr(pos);
                    if (!rest.empty()) {
                        if (!firstSegment) ImGui::SameLine(0, 0);
                        ImGui::TextUnformatted(rest.c_str());
                        firstSegment = false;
                    }
                    break;
                }

                // Render plain text before the marker
                if (earliest > pos) {
                    std::string plain = text.substr(pos, earliest - pos);
                    if (!firstSegment) ImGui::SameLine(0, 0);
                    ImGui::TextUnformatted(plain.c_str());
                    firstSegment = false;
                }

                if (earliest == boldPos) {
                    // Bold: **text**
                    size_t end = text.find("**", boldPos + 2);
                    if (end == std::string::npos) { pos = boldPos + 2; continue; }
                    std::string boldText = text.substr(boldPos + 2, end - boldPos - 2);
                    if (!firstSegment) ImGui::SameLine(0, 0);
                    if (headingFont) ImGui::PushFont(headingFont);
                    ImGui::TextUnformatted(boldText.c_str());
                    if (headingFont) ImGui::PopFont();
                    firstSegment = false;
                    pos = end + 2;
                } else if (earliest == codePos) {
                    // Inline code: `text`
                    size_t end = text.find('`', codePos + 1);
                    if (end == std::string::npos) { pos = codePos + 1; continue; }
                    std::string codeText = text.substr(codePos + 1, end - codePos - 1);
                    if (!firstSegment) ImGui::SameLine(0, 0);
                    if (monoFont) ImGui::PushFont(monoFont);
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.7f, 0.4f, 1.0f));
                    ImGui::TextUnformatted(codeText.c_str());
                    ImGui::PopStyleColor();
                    if (monoFont) ImGui::PopFont();
                    firstSegment = false;
                    pos = end + 1;
                } else {
                    // Italic: *text*
                    size_t end = std::string::npos;
                    for (size_t s = italicPos + 1; s < text.size(); ++s) {
                        if (text[s] == '*' && (s + 1 >= text.size() || text[s + 1] != '*') &&
                            text[s - 1] != '*') {
                            end = s;
                            break;
                        }
                    }
                    if (end == std::string::npos) { pos = italicPos + 1; continue; }
                    std::string italicText = text.substr(italicPos + 1, end - italicPos - 1);
                    if (!firstSegment) ImGui::SameLine(0, 0);
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.85f, 1.0f, 1.0f));
                    ImGui::TextUnformatted(italicText.c_str());
                    ImGui::PopStyleColor();
                    firstSegment = false;
                    pos = end + 1;
                }
            }
        };

        // Helper: render a section's content lines with markdown formatting
        auto RenderSectionContent = [&](const std::string& contentStr) {
            std::istringstream contentStream(contentStr);
            std::string line;
            bool inCodeBlock = false;
            int tableId = 0;

            while (std::getline(contentStream, line)) {
                // Code blocks
                if (line.find("```") != std::string::npos) {
                    inCodeBlock = !inCodeBlock;
                    if (inCodeBlock) {
                        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.1f, 0.1f, 0.1f, 1.0f));
                        ImGui::BeginChild(("##code_" + std::to_string(ImGui::GetCursorPosY())).c_str(), ImVec2(-1, 0), false, ImGuiWindowFlags_AlwaysAutoResize);
                    } else {
                        ImGui::EndChild();
                        ImGui::PopStyleColor();
                    }
                    continue;
                }

                if (inCodeBlock) {
                    if (ImGui::GetIO().Fonts->Fonts.Size > 2) ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[2]);
                    ImGui::TextUnformatted(line.c_str());
                    if (ImGui::GetIO().Fonts->Fonts.Size > 2) ImGui::PopFont();
                    continue;
                }

                // Table rows
                if (!line.empty() && line[0] == '|') {
                    // Skip separator rows (|---|---|)
                    if (line.find("---") != std::string::npos) continue;

                    // Parse columns
                    std::vector<std::string> cols;
                    size_t p = 1; // skip leading |
                    while (p < line.size()) {
                        size_t next = line.find('|', p);
                        if (next == std::string::npos) break;
                        std::string cell = line.substr(p, next - p);
                        // Trim whitespace
                        size_t start = cell.find_first_not_of(" \t");
                        size_t end = cell.find_last_not_of(" \t");
                        if (start != std::string::npos)
                            cols.push_back(cell.substr(start, end - start + 1));
                        else
                            cols.push_back("");
                        p = next + 1;
                    }

                    if (!cols.empty()) {
                        std::string tableIdStr = "##manual_table_" + std::to_string(tableId);
                        if (ImGui::BeginTable(tableIdStr.c_str(), (int)cols.size(),
                            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchSame)) {
                            for (const auto& col : cols) {
                                ImGui::TableNextColumn();
                                ImGui::TextWrapped("%s", col.c_str());
                            }
                            // Peek ahead to render additional table rows
                            std::string peekLine;
                            while (contentStream.peek() == '|' && std::getline(contentStream, peekLine)) {
                                if (peekLine.find("---") != std::string::npos) continue;
                                // Parse row columns
                                size_t rp = 1;
                                while (rp < peekLine.size()) {
                                    size_t rnext = peekLine.find('|', rp);
                                    if (rnext == std::string::npos) break;
                                    std::string cell = peekLine.substr(rp, rnext - rp);
                                    size_t cs = cell.find_first_not_of(" \t");
                                    size_t ce = cell.find_last_not_of(" \t");
                                    ImGui::TableNextColumn();
                                    if (cs != std::string::npos)
                                        ImGui::TextWrapped("%s", cell.substr(cs, ce - cs + 1).c_str());
                                    else
                                        ImGui::TextWrapped("");
                                    rp = rnext + 1;
                                }
                            }
                            ImGui::EndTable();
                            ++tableId;
                        }
                    }
                    continue;
                }

                // Sub-headers within content (### ####)
                if (line.size() >= 4 && line[0] == '#' && line[1] == '#' && line[2] == '#') {
                    std::string subTitle = line.substr(3);
                    while (!subTitle.empty() && subTitle[0] == '#') subTitle = subTitle.substr(1);
                    while (!subTitle.empty() && subTitle[0] == ' ') subTitle = subTitle.substr(1);
                    ImGui::Spacing();
                    if (ImGui::GetIO().Fonts->Fonts.Size > 1) ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[1]);
                    ImGui::TextWrapped("%s", subTitle.c_str());
                    if (ImGui::GetIO().Fonts->Fonts.Size > 1) ImGui::PopFont();
                    ImGui::Separator();
                    continue;
                }

                // Bullet points
                if (line.size() >= 2 && line[0] == '-' && line[1] == ' ') {
                    ImGui::Bullet();
                    ImGui::SameLine();
                    RenderMarkdownLine(line.substr(2));
                    continue;
                }

                // Numbered list items
                if (line.size() >= 3 && std::isdigit(static_cast<unsigned char>(line[0]))) {
                    size_t dotPos = line.find(". ");
                    if (dotPos != std::string::npos && dotPos <= 3) {
                        std::string prefix = line.substr(0, dotPos + 2);
                        ImGui::TextUnformatted(prefix.c_str());
                        ImGui::SameLine(0, 0);
                        RenderMarkdownLine(line.substr(dotPos + 2));
                        continue;
                    }
                }

                // Separator
                if (line == "---") {
                    ImGui::Separator();
                    continue;
                }

                // Empty lines = spacing
                if (line.empty()) {
                    ImGui::Spacing();
                    continue;
                }

                // Regular text with inline formatting
                RenderMarkdownLine(line);
            }

            // Close any unclosed code block
            if (inCodeBlock) {
                ImGui::EndChild();
                ImGui::PopStyleColor();
            }
        };

        // Title
        ImGui::PushFont(ImGui::GetIO().Fonts->Fonts.Size > 1 ? ImGui::GetIO().Fonts->Fonts[1] : nullptr);
        ImGui::TextWrapped("%s", section.title.c_str());
        ImGui::PopFont();
        ImGui::Separator();
        ImGui::Spacing();

        // Render section content
        RenderSectionContent(section.content);

        // If section has no content, show child sections inline
        if (section.showChildrenInline) {
            for (int j = m_ManualSelectedSection + 1; j < (int)m_ManualSections.size(); ++j) {
                if (m_ManualSections[j].level <= section.level) break;
                const auto& child = m_ManualSections[j];
                ImGui::Spacing();
                if (ImGui::GetIO().Fonts->Fonts.Size > 1) ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[1]);
                ImGui::TextWrapped("%s", child.title.c_str());
                if (ImGui::GetIO().Fonts->Fonts.Size > 1) ImGui::PopFont();
                ImGui::Separator();
                ImGui::Spacing();
                RenderSectionContent(child.content);
            }
        }

        // If search is active, highlight matches
        if (!searchLower.empty()) {
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::TextColored(ImVec4(0.5f, 0.8f, 0.5f, 1.0f), "Matches found for: %s", m_ManualSearchBuf);
        }
    } else {
        ImGui::TextWrapped("Select a section from the list on the left to view its contents.");
        ImGui::Spacing();
        ImGui::TextWrapped("Use the search bar to filter sections by keyword.");
        ImGui::Spacing();
        ImGui::TextDisabled("%zu sections loaded", m_ManualSections.size());
    }

    ImGui::EndChild();

    ImGui::End();
}

// ============================================================================
// DATA ASSET EDITOR PANEL
// ============================================================================

void EditorLayer::DrawDataAssetPanel() {
    bool panelOpen = true;
    if (!ImGui::Begin("Data Asset Editor", &panelOpen)) {
        if (!panelOpen) SetPanelVisibility(EditorPanel::DataAssets, false);
        ImGui::End();
        return;
    }
    if (!panelOpen) { SetPanelVisibility(EditorPanel::DataAssets, false); ImGui::End(); return; }

    auto& registry = Assets::DataAssetRegistry::Get();

    // Split: left = schema/asset lists, right = editor
    ImGui::Columns(2, "DataAssetColumns", true);
    ImGui::SetColumnWidth(0, 260.0f);

    // --- LEFT PANE: Schema list ---
    ImGui::Text("Schemas");
    ImGui::SameLine();
    if (ImGui::SmallButton("New Schema")) {
        m_NewSchemaName = "NewSchema";
        ImGui::OpenPopup("NewSchemaPopup");
    }

    // New schema popup
    if (ImGui::BeginPopup("NewSchemaPopup")) {
        ImGui::Text("Schema Name:");
        char buf[128];
        strncpy(buf, m_NewSchemaName.c_str(), sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
        if (ImGui::InputText("##schemaname", buf, sizeof(buf))) {
            m_NewSchemaName = buf;
        }
        if (ImGui::Button("Create") && !m_NewSchemaName.empty()) {
            Assets::DataAssetSchema schema;
            schema.name = m_NewSchemaName;
            registry.RegisterSchema(schema);
            m_SelectedSchemaName = m_NewSchemaName;
            m_EditingSchema = true;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    ImGui::InputText("##schemasearch", m_DataAssetSchemaSearchBuf, sizeof(m_DataAssetSchemaSearchBuf));
    ImGui::Separator();

    auto schemas = registry.GetAllSchemas();
    for (const auto* schema : schemas) {
        // Filter
        if (m_DataAssetSchemaSearchBuf[0] != '\0') {
            std::string lower = schema->name;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
            std::string filter = m_DataAssetSchemaSearchBuf;
            std::transform(filter.begin(), filter.end(), filter.begin(), ::tolower);
            if (lower.find(filter) == std::string::npos) continue;
        }

        bool selected = (m_SelectedSchemaName == schema->name);
        if (ImGui::Selectable(schema->name.c_str(), selected)) {
            m_SelectedSchemaName = schema->name;
            m_SelectedAssetName.clear();
            m_EditingSchema = true;
        }
    }

    ImGui::Spacing();
    ImGui::Separator();

    // --- Asset list (filtered by selected schema) ---
    ImGui::Text("Assets");
    ImGui::SameLine();
    if (!m_SelectedSchemaName.empty() && ImGui::SmallButton("New Asset")) {
        m_NewAssetName = "NewAsset";
        ImGui::OpenPopup("NewAssetPopup");
    }

    // New asset popup
    if (ImGui::BeginPopup("NewAssetPopup")) {
        ImGui::Text("Asset Name:");
        char buf[128];
        strncpy(buf, m_NewAssetName.c_str(), sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
        if (ImGui::InputText("##assetname", buf, sizeof(buf))) {
            m_NewAssetName = buf;
        }
        if (ImGui::Button("Create") && !m_NewAssetName.empty()) {
            Assets::DataAsset asset;
            asset.name = m_NewAssetName;
            asset.schemaName = m_SelectedSchemaName;

            // Fill defaults from schema
            const auto* schema = registry.FindSchema(m_SelectedSchemaName);
            if (schema) {
                for (const auto& field : schema->fields) {
                    asset.values[field.name] = field.defaultValue;
                }
            }

            registry.CreateAsset(asset);
            m_SelectedAssetName = m_NewAssetName;
            m_EditingSchema = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    ImGui::InputText("##assetsearch", m_DataAssetSearchBuf, sizeof(m_DataAssetSearchBuf));
    ImGui::Separator();

    if (!m_SelectedSchemaName.empty()) {
        auto assets = registry.GetAssetsBySchema(m_SelectedSchemaName);
        for (const auto* asset : assets) {
            if (m_DataAssetSearchBuf[0] != '\0') {
                std::string lower = asset->name;
                std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
                std::string filter = m_DataAssetSearchBuf;
                std::transform(filter.begin(), filter.end(), filter.begin(), ::tolower);
                if (lower.find(filter) == std::string::npos) continue;
            }

            bool selected = (m_SelectedAssetName == asset->name);
            if (ImGui::Selectable(asset->name.c_str(), selected)) {
                m_SelectedAssetName = asset->name;
                m_EditingSchema = false;
            }
        }
    }

    ImGui::NextColumn();

    // --- RIGHT PANE: Editor ---
    if (m_EditingSchema && !m_SelectedSchemaName.empty()) {
        // Schema editor
        const auto* schema = registry.FindSchema(m_SelectedSchemaName);
        if (schema) {
            ImGui::Text("Schema: %s", schema->name.c_str());
            ImGui::Separator();

            // Description
            static char descBuf[512] = {};
            strncpy(descBuf, schema->description.c_str(), sizeof(descBuf) - 1);
            descBuf[sizeof(descBuf) - 1] = '\0';
            if (ImGui::InputText("Description", descBuf, sizeof(descBuf))) {
                // Update in-place (registry has a copy)
                auto schemas_copy = registry.GetAllSchemas();
                for (auto* s : schemas_copy) {
                    (void)s;
                }
                Assets::DataAssetSchema updated = *schema;
                updated.description = descBuf;
                registry.RegisterSchema(updated);
            }

            ImGui::Spacing();
            ImGui::Text("Fields:");
            ImGui::Separator();

            Assets::DataAssetSchema editSchema = *schema;
            bool modified = false;

            for (size_t i = 0; i < editSchema.fields.size(); i++) {
                ImGui::PushID(static_cast<int>(i));

                auto& field = editSchema.fields[i];

                // Field name
                char nameBuf[128];
                strncpy(nameBuf, field.name.c_str(), sizeof(nameBuf) - 1);
                nameBuf[sizeof(nameBuf) - 1] = '\0';
                ImGui::SetNextItemWidth(120.0f);
                if (ImGui::InputText("##name", nameBuf, sizeof(nameBuf))) {
                    field.name = nameBuf;
                    modified = true;
                }
                ImGui::SameLine();

                // Type dropdown
                const char* typeNames[] = {"String", "Float", "Int", "Bool", "Vector3", "Vector4", "StringArray", "FloatArray"};
                int typeIdx = static_cast<int>(field.type);
                ImGui::SetNextItemWidth(100.0f);
                if (ImGui::Combo("##type", &typeIdx, typeNames, 8)) {
                    field.type = static_cast<Assets::DataFieldType>(typeIdx);
                    // Reset default value for new type
                    switch (field.type) {
                        case Assets::DataFieldType::String:      field.defaultValue = std::string(""); break;
                        case Assets::DataFieldType::Float:       field.defaultValue = 0.0f; break;
                        case Assets::DataFieldType::Int:         field.defaultValue = 0; break;
                        case Assets::DataFieldType::Bool:        field.defaultValue = false; break;
                        case Assets::DataFieldType::Vector3:     field.defaultValue = Math::Vector3(0,0,0); break;
                        case Assets::DataFieldType::Vector4:     field.defaultValue = Math::Vector4(0,0,0,0); break;
                        case Assets::DataFieldType::StringArray: field.defaultValue = std::vector<std::string>{}; break;
                        case Assets::DataFieldType::FloatArray:  field.defaultValue = std::vector<f32>{}; break;
                    }
                    modified = true;
                }
                ImGui::SameLine();

                // Remove button
                if (ImGui::SmallButton("X")) {
                    editSchema.fields.erase(editSchema.fields.begin() + i);
                    modified = true;
                    ImGui::PopID();
                    break;
                }

                ImGui::PopID();
            }

            if (ImGui::Button("+ Add Field")) {
                Assets::DataAssetField newField;
                newField.name = "newField";
                newField.type = Assets::DataFieldType::Float;
                newField.defaultValue = 0.0f;
                editSchema.fields.push_back(newField);
                modified = true;
            }

            if (modified) {
                registry.RegisterSchema(editSchema);
            }

            ImGui::Spacing();
            ImGui::Separator();

            // Save/Load buttons
            if (ImGui::Button("Save Schema (.enjschema)")) {
                std::string path = editSchema.name + ".enjschema";
                registry.SaveSchema(editSchema, path);
            }
            ImGui::SameLine();
            if (ImGui::Button("Delete Schema")) {
                registry.RemoveSchema(m_SelectedSchemaName);
                m_SelectedSchemaName.clear();
            }
        }
    } else if (!m_EditingSchema && !m_SelectedAssetName.empty()) {
        // Asset editor
        auto* asset = registry.FindAssetMut(m_SelectedAssetName);
        if (asset) {
            ImGui::Text("Asset: %s", asset->name.c_str());
            ImGui::TextDisabled("Schema: %s", asset->schemaName.c_str());
            ImGui::Separator();

            const auto* schema = registry.FindSchema(asset->schemaName);
            if (schema) {
                for (const auto& field : schema->fields) {
                    ImGui::PushID(field.name.c_str());

                    auto it = asset->values.find(field.name);
                    if (it == asset->values.end()) {
                        asset->values[field.name] = field.defaultValue;
                        it = asset->values.find(field.name);
                    }

                    switch (field.type) {
                        case Assets::DataFieldType::Float: {
                            f32 val = std::holds_alternative<f32>(it->second) ? std::get<f32>(it->second) : 0.0f;
                            if (ImGui::DragFloat(field.name.c_str(), &val, 0.1f)) {
                                it->second = val;
                            }
                            break;
                        }
                        case Assets::DataFieldType::Int: {
                            i32 val = std::holds_alternative<i32>(it->second) ? std::get<i32>(it->second) : 0;
                            if (ImGui::DragInt(field.name.c_str(), &val)) {
                                it->second = val;
                            }
                            break;
                        }
                        case Assets::DataFieldType::Bool: {
                            bool val = std::holds_alternative<bool>(it->second) ? std::get<bool>(it->second) : false;
                            if (ImGui::Checkbox(field.name.c_str(), &val)) {
                                it->second = val;
                            }
                            break;
                        }
                        case Assets::DataFieldType::String: {
                            std::string val = std::holds_alternative<std::string>(it->second) ? std::get<std::string>(it->second) : "";
                            char buf[512];
                            strncpy(buf, val.c_str(), sizeof(buf) - 1);
                            buf[sizeof(buf) - 1] = '\0';
                            if (ImGui::InputText(field.name.c_str(), buf, sizeof(buf))) {
                                it->second = std::string(buf);
                            }
                            break;
                        }
                        case Assets::DataFieldType::Vector3: {
                            Math::Vector3 val = std::holds_alternative<Math::Vector3>(it->second) ? std::get<Math::Vector3>(it->second) : Math::Vector3(0,0,0);
                            f32 v[3] = {val.x, val.y, val.z};
                            if (ImGui::DragFloat3(field.name.c_str(), v, 0.1f)) {
                                it->second = Math::Vector3(v[0], v[1], v[2]);
                            }
                            break;
                        }
                        case Assets::DataFieldType::Vector4: {
                            Math::Vector4 val = std::holds_alternative<Math::Vector4>(it->second) ? std::get<Math::Vector4>(it->second) : Math::Vector4(0,0,0,0);
                            f32 v[4] = {val.x, val.y, val.z, val.w};
                            if (ImGui::DragFloat4(field.name.c_str(), v, 0.1f)) {
                                it->second = Math::Vector4(v[0], v[1], v[2], v[3]);
                            }
                            break;
                        }
                        default:
                            ImGui::TextDisabled("%s: (array type — edit in file)", field.name.c_str());
                            break;
                    }

                    ImGui::PopID();
                }
            }

            ImGui::Spacing();
            ImGui::Separator();

            if (ImGui::Button("Save Asset (.enjdata)")) {
                std::string path = asset->filePath.empty() ? (asset->name + ".enjdata") : asset->filePath;
                registry.SaveAsset(*asset, path);
                asset->filePath = path;
            }
            ImGui::SameLine();
            if (ImGui::Button("Delete Asset")) {
                registry.RemoveAsset(m_SelectedAssetName);
                m_SelectedAssetName.clear();
            }
        }
    } else {
        ImGui::TextDisabled("Select a schema or asset to edit.");
    }

    ImGui::Columns(1);

    // Scan buttons at bottom
    ImGui::Separator();
    if (ImGui::Button("Scan Schemas...")) {
        registry.ScanSchemaDirectory(".");
    }
    ImGui::SameLine();
    if (ImGui::Button("Scan Assets...")) {
        registry.ScanAssetDirectory(".");
    }

    ImGui::End();
}

// ============================================================================
// PLUGIN BROWSER PANEL
// ============================================================================


void EditorLayer::DrawPluginBrowserPanel() {
    bool panelOpen = true;
    if (!ImGui::Begin("Plugin Browser", &panelOpen)) {
        if (!panelOpen) SetPanelVisibility(EditorPanel::PluginBrowser, false);
        ImGui::End();
        return;
    }
    if (!panelOpen) { SetPanelVisibility(EditorPanel::PluginBrowser, false); ImGui::End(); return; }

    // Search bar
    ImGui::SetNextItemWidth(200.0f);
    ImGui::InputText("Search", m_PluginSearchBuf, sizeof(m_PluginSearchBuf));
    ImGui::SameLine();

    // Category filter
    auto categories = m_PluginRepository.GetCategories();
    if (ImGui::BeginCombo("Category", m_PluginCategoryFilter.empty() ? "All" : m_PluginCategoryFilter.c_str())) {
        if (ImGui::Selectable("All", m_PluginCategoryFilter.empty())) {
            m_PluginCategoryFilter.clear();
        }
        for (const auto& cat : categories) {
            if (ImGui::Selectable(cat.c_str(), m_PluginCategoryFilter == cat)) {
                m_PluginCategoryFilter = cat;
            }
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    ImGui::Checkbox("Installed Only", &m_PluginShowInstalledOnly);
    ImGui::SameLine();
    if (ImGui::Button("Refresh")) {
        m_PluginRepository.RefreshCatalog();
    }

    ImGui::Separator();

    // Get filtered catalog
    const auto& catalog = m_PluginRepository.GetCatalog();
    std::string searchLower = m_PluginSearchBuf;
    std::transform(searchLower.begin(), searchLower.end(), searchLower.begin(), ::tolower);

    if (ImGui::BeginTable("PluginCatalog", 6,
        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable |
        ImGuiTableFlags_ScrollY)) {

        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Version", ImGuiTableColumnFlags_WidthFixed, 70.0f);
        ImGui::TableSetupColumn("Author", ImGuiTableColumnFlags_WidthFixed, 100.0f);
        ImGui::TableSetupColumn("Category", ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, 100.0f);
        ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthFixed, 90.0f);
        ImGui::TableHeadersRow();

        for (const auto& entry : catalog) {
            // Category filter
            if (!m_PluginCategoryFilter.empty() && entry.category != m_PluginCategoryFilter) continue;

            // Search filter
            if (!searchLower.empty()) {
                std::string text = entry.name + " " + entry.description + " " + entry.author;
                for (const auto& tag : entry.tags) text += " " + tag;
                std::transform(text.begin(), text.end(), text.begin(), ::tolower);
                if (text.find(searchLower) == std::string::npos) continue;
            }

            bool installed = m_PluginRepository.IsInstalled(entry.name, nullptr);
            if (m_PluginShowInstalledOnly && !installed) continue;

            bool hasUpdate = m_PluginRepository.HasUpdate(entry.name, nullptr);

            ImGui::TableNextRow();

            // Name
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%s", entry.name.c_str());
            if (!entry.description.empty() && ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", entry.description.c_str());
            }

            // Version
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%s", entry.version.c_str());

            // Author
            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%s", entry.author.c_str());

            // Category
            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%s", entry.category.c_str());

            // Status
            ImGui::TableSetColumnIndex(4);
            if (installed && hasUpdate) {
                ImGui::TextColored(ImVec4(0.9f, 0.8f, 0.2f, 1.0f), "Update Available");
            } else if (installed) {
                ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "Installed");
            } else {
                ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Not Installed");
            }

            // Action
            ImGui::TableSetColumnIndex(5);
            ImGui::PushID(entry.name.c_str());
            if (installed && hasUpdate) {
                if (ImGui::SmallButton("Update")) {
                    m_PluginRepository.Install(entry.name, "plugins");
                }
            } else if (installed) {
                if (ImGui::SmallButton("Uninstall")) {
                    m_PluginRepository.Uninstall(entry.name, "plugins");
                }
            } else {
                if (ImGui::SmallButton("Install")) {
                    m_PluginRepository.Install(entry.name, "plugins");
                }
            }
            ImGui::PopID();
        }

        ImGui::EndTable();
    }

    if (catalog.empty()) {
        DrawEmptyState("{+}", "No Plugins Found", "Add a repository source and click Refresh");
    }

    // Repository sources management
    ImGui::Separator();
    if (ImGui::CollapsingHeader("Repository Sources")) {
        const auto& sources = m_PluginRepository.GetSources();
        for (size_t i = 0; i < sources.size(); i++) {
            ImGui::PushID(static_cast<int>(i));
            ImGui::Text("%s", sources[i].name.c_str());
            ImGui::SameLine();
            ImGui::TextDisabled("(%s)", sources[i].path.c_str());
            ImGui::SameLine();
            if (ImGui::SmallButton("Remove")) {
                m_PluginRepository.RemoveSource(sources[i].name);
                ImGui::PopID();
                break;
            }
            ImGui::PopID();
        }

        static char srcName[128] = "";
        static char srcPath[512] = "";
        ImGui::SetNextItemWidth(120.0f);
        ImGui::InputText("Name##repo", srcName, sizeof(srcName));
        ImGui::SameLine();
        ImGui::SetNextItemWidth(250.0f);
        ImGui::InputText("Path##repo", srcPath, sizeof(srcPath));
        ImGui::SameLine();
        if (ImGui::Button("Add Source") && srcName[0] != '\0' && srcPath[0] != '\0') {
            Plugin::RepositorySource source;
            source.name = srcName;
            source.path = srcPath;
            m_PluginRepository.AddSource(source);
            srcName[0] = '\0';
            srcPath[0] = '\0';
        }

        if (ImGui::Button("Save Sources")) {
            m_PluginRepository.SaveSources("plugins/repositories.json");
        }
        ImGui::SameLine();
        if (ImGui::Button("Load Sources")) {
            m_PluginRepository.LoadSources("plugins/repositories.json");
        }
    }

    ImGui::End();
}

void EditorLayer::DrawProceduralGenPanel() {
    // Render the graph editor window if open
    if (m_ProcGraphEditor.IsOpen()) {
        m_ProcGraphEditor.Render();
    }

    bool panelOpen = true;
    if (!ImGui::Begin("Procedural Generation", &panelOpen)) {
        if (!panelOpen) SetPanelVisibility(EditorPanel::ProceduralGen, false);
        ImGui::End();
        return;
    }
    if (!panelOpen) { SetPanelVisibility(EditorPanel::ProceduralGen, false); ImGui::End(); return; }

    // Graph editor toggle
    if (ImGui::Button("Open Graph Editor")) {
        m_ProcGraphEditor.SetGraph(&m_ProcGraphData);
        m_ProcGraphEditor.SetOpen(true);
    }
    ImGui::SetItemTooltip("Visual node-based pipeline for composing procedural generation algorithms");
    ImGui::SameLine();
    ImGui::TextDisabled("(Graph: %zu nodes, %zu links)",
        m_ProcGraphData.nodes.size(), m_ProcGraphData.links.size());
    ImGui::Separator();

    const char* algorithms[] = {
        "Cellular Automata", "Random Walker", "BSP Dungeon", "Diamond-Square Heightmap",
        "L-System", "Wave Function Collapse", "Voronoi Diagram", "Shape Grammar",
        "Prefab Assembler"
    };
    if (ImGui::Combo("Algorithm", &m_ProceduralAlgorithm, algorithms, 9)) {
        m_ProceduralPreviewDirty = true;
    }

    ImGui::Separator();

    // Seed input
    int seed = static_cast<int>(m_ProceduralSeed);
    if (ImGui::InputInt("Seed (0 = random)", &seed)) {
        m_ProceduralSeed = static_cast<u32>(std::max(0, seed));
        m_ProceduralPreviewDirty = true;
    }

    ImGui::Separator();

    // Algorithm-specific parameters
    bool paramsChanged = false;
    switch (m_ProceduralAlgorithm) {
    case 0: { // Cellular Automata
        int w = static_cast<int>(m_CAParams.width);
        int h = static_cast<int>(m_CAParams.height);
        int fill = static_cast<int>(m_CAParams.fillPercent);
        int birth = static_cast<int>(m_CAParams.birthLimit);
        int death = static_cast<int>(m_CAParams.deathLimit);
        int iter = static_cast<int>(m_CAParams.iterations);
        paramsChanged |= ImGui::SliderInt("Width", &w, 16, 256);
        paramsChanged |= ImGui::SliderInt("Height", &h, 16, 256);
        paramsChanged |= ImGui::SliderInt("Fill %", &fill, 20, 80);
        paramsChanged |= ImGui::SliderInt("Birth Limit", &birth, 1, 8);
        paramsChanged |= ImGui::SliderInt("Death Limit", &death, 1, 8);
        paramsChanged |= ImGui::SliderInt("Iterations", &iter, 1, 20);
        m_CAParams.width = static_cast<u32>(w);
        m_CAParams.height = static_cast<u32>(h);
        m_CAParams.fillPercent = static_cast<u32>(fill);
        m_CAParams.birthLimit = static_cast<u32>(birth);
        m_CAParams.deathLimit = static_cast<u32>(death);
        m_CAParams.iterations = static_cast<u32>(iter);
        break;
    }
    case 1: { // Random Walker
        int w = static_cast<int>(m_RWParams.width);
        int h = static_cast<int>(m_RWParams.height);
        int steps = static_cast<int>(m_RWParams.steps);
        paramsChanged |= ImGui::SliderInt("Width", &w, 16, 256);
        paramsChanged |= ImGui::SliderInt("Height", &h, 16, 256);
        paramsChanged |= ImGui::SliderInt("Steps", &steps, 100, 10000);
        paramsChanged |= ImGui::SliderFloat("Turn Chance", &m_RWParams.turnChance, 0.0f, 1.0f);
        m_RWParams.width = static_cast<u32>(w);
        m_RWParams.height = static_cast<u32>(h);
        m_RWParams.steps = static_cast<u32>(steps);
        break;
    }
    case 2: { // BSP
        int w = static_cast<int>(m_BSPParams.width);
        int h = static_cast<int>(m_BSPParams.height);
        int minRoom = static_cast<int>(m_BSPParams.minRoomSize);
        int depth = static_cast<int>(m_BSPParams.splitDepth);
        int corridor = static_cast<int>(m_BSPParams.corridorWidth);
        paramsChanged |= ImGui::SliderInt("Width", &w, 32, 256);
        paramsChanged |= ImGui::SliderInt("Height", &h, 32, 256);
        paramsChanged |= ImGui::SliderInt("Min Room Size", &minRoom, 3, 20);
        paramsChanged |= ImGui::SliderInt("Split Depth", &depth, 2, 8);
        paramsChanged |= ImGui::SliderInt("Corridor Width", &corridor, 1, 5);
        m_BSPParams.width = static_cast<u32>(w);
        m_BSPParams.height = static_cast<u32>(h);
        m_BSPParams.minRoomSize = static_cast<u32>(minRoom);
        m_BSPParams.splitDepth = static_cast<u32>(depth);
        m_BSPParams.corridorWidth = static_cast<u32>(corridor);
        break;
    }
    case 3: { // Diamond-Square
        int size = static_cast<int>(m_DSParams.size);
        paramsChanged |= ImGui::SliderInt("Size (2^n+1)", &size, 33, 257);
        paramsChanged |= ImGui::SliderFloat("Roughness", &m_DSParams.roughness, 0.1f, 2.0f);
        m_DSParams.size = static_cast<u32>(size);
        break;
    }
    case 4: { // L-System
        static char axiomBuf[64] = "F";
        static char rulesBuf[256] = "F=F[+F]F[-F]F";
        paramsChanged |= ImGui::InputText("Axiom", axiomBuf, sizeof(axiomBuf));
        paramsChanged |= ImGui::InputText("Rules (F=...)", rulesBuf, sizeof(rulesBuf));
        int iter = static_cast<int>(m_LSParams.iterations);
        paramsChanged |= ImGui::SliderInt("Iterations", &iter, 1, 6);
        paramsChanged |= ImGui::SliderFloat("Angle", &m_LSParams.angle, 10.0f, 90.0f);
        m_LSParams.axiom = axiomBuf;
        m_LSParams.iterations = static_cast<u32>(iter);
        // Parse simple rules
        m_LSParams.rules.clear();
        std::string rulesStr(rulesBuf);
        size_t pos = 0;
        while (pos < rulesStr.size()) {
            size_t eq = rulesStr.find('=', pos);
            if (eq == std::string::npos || eq == pos) break;
            char sym = rulesStr[pos];
            size_t end = rulesStr.find(';', eq);
            if (end == std::string::npos) end = rulesStr.size();
            m_LSParams.rules[sym] = rulesStr.substr(eq + 1, end - eq - 1);
            pos = end + 1;
        }
        break;
    }
    case 5: { // WFC
        int w = static_cast<int>(m_WFCParams.width);
        int h = static_cast<int>(m_WFCParams.height);
        paramsChanged |= ImGui::SliderInt("Width", &w, 8, 64);
        paramsChanged |= ImGui::SliderInt("Height", &h, 8, 64);
        m_WFCParams.width = static_cast<u32>(w);
        m_WFCParams.height = static_cast<u32>(h);
        ImGui::TextDisabled("Add tiles via code (WFCParams.tiles)");
        break;
    }
    case 6: { // Voronoi
        int w = static_cast<int>(m_VoronoiParams.width);
        int h = static_cast<int>(m_VoronoiParams.height);
        int seeds = static_cast<int>(m_VoronoiParams.numSeeds);
        paramsChanged |= ImGui::SliderInt("Width", &w, 32, 256);
        paramsChanged |= ImGui::SliderInt("Height", &h, 32, 256);
        paramsChanged |= ImGui::SliderInt("Seed Points", &seeds, 4, 100);
        int distType = static_cast<int>(m_VoronoiParams.distanceType);
        const char* distTypes[] = { "Euclidean", "Manhattan", "Chebyshev" };
        paramsChanged |= ImGui::Combo("Distance", &distType, distTypes, 3);
        m_VoronoiParams.width = static_cast<u32>(w);
        m_VoronoiParams.height = static_cast<u32>(h);
        m_VoronoiParams.numSeeds = static_cast<u32>(seeds);
        m_VoronoiParams.distanceType = static_cast<Procedural::VoronoiGenerator::DistanceType>(distType);
        break;
    }
    case 7: { // Grammar
        int iter = static_cast<int>(m_GrammarParams.iterations);
        paramsChanged |= ImGui::SliderInt("Iterations", &iter, 1, 5);
        m_GrammarParams.iterations = static_cast<u32>(iter);
        ImGui::TextDisabled("Add grammar rules via code");
        break;
    }
    case 8: { // Prefab Assembler
        int maxRooms = static_cast<int>(m_PAParams.maxRooms);
        paramsChanged |= ImGui::SliderInt("Max Rooms", &maxRooms, 1, 50);
        m_PAParams.maxRooms = static_cast<u32>(maxRooms);
        ImGui::TextDisabled("Add prefab slots via code");
        break;
    }
    }

    if (paramsChanged) m_ProceduralPreviewDirty = true;

    ImGui::Separator();

    if (ImGui::Button("Generate Preview", ImVec2(200, 30)) || m_ProceduralPreviewDirty) {
        m_ProceduralPreviewDirty = false;
        m_ProceduralPreview.clear();
        m_ProceduralHeightmap.clear();

        switch (m_ProceduralAlgorithm) {
        case 0: {
            m_CAParams.seed = m_ProceduralSeed;
            auto result = Procedural::CellularAutomata::Generate(m_CAParams);
            m_ProceduralPreview = result.grid;
            m_ProceduralPreviewW = result.width;
            m_ProceduralPreviewH = result.height;
            break;
        }
        case 1: {
            m_RWParams.seed = m_ProceduralSeed;
            auto result = Procedural::RandomWalker::Generate(m_RWParams);
            m_ProceduralPreview = result.grid;
            m_ProceduralPreviewH = static_cast<u32>(result.grid.size());
            m_ProceduralPreviewW = m_ProceduralPreviewH > 0 ? static_cast<u32>(result.grid[0].size()) : 0;
            break;
        }
        case 2: {
            m_BSPParams.seed = m_ProceduralSeed;
            auto result = Procedural::BSPGenerator::Generate(m_BSPParams);
            m_ProceduralPreview = result.grid;
            m_ProceduralPreviewH = static_cast<u32>(result.grid.size());
            m_ProceduralPreviewW = m_ProceduralPreviewH > 0 ? static_cast<u32>(result.grid[0].size()) : 0;
            break;
        }
        case 3: {
            m_DSParams.seed = m_ProceduralSeed;
            auto result = Procedural::DiamondSquare::Generate(m_DSParams);
            m_ProceduralHeightmap = result.heightmap;
            m_ProceduralPreviewW = result.size;
            m_ProceduralPreviewH = result.size;
            break;
        }
        case 6: {
            m_VoronoiParams.seed = m_ProceduralSeed;
            auto result = Procedural::VoronoiGenerator::Generate(m_VoronoiParams);
            // Convert u32 grid to u8 grid (mod 256 for color variation)
            m_ProceduralPreview.resize(result.grid.size());
            for (u32 y = 0; y < static_cast<u32>(result.grid.size()); y++) {
                m_ProceduralPreview[y].resize(result.grid[y].size());
                for (u32 x = 0; x < static_cast<u32>(result.grid[y].size()); x++) {
                    m_ProceduralPreview[y][x] = static_cast<u8>(result.grid[y][x] % 256);
                }
            }
            m_ProceduralPreviewH = static_cast<u32>(result.grid.size());
            m_ProceduralPreviewW = m_ProceduralPreviewH > 0 ? static_cast<u32>(result.grid[0].size()) : 0;
            break;
        }
        default:
            break;
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("Apply to Tilemap", ImVec2(150, 30))) {
        if (!m_ProceduralPreview.empty() && m_PrimarySelected != ECS::INVALID_ENTITY) {
            ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "Applied to selected entity's tilemap");
        }
    }

    // --- One-Click Forest Generation ---
    ImGui::Separator();
    if (ImGui::CollapsingHeader("Generate Forest", ImGuiTreeNodeFlags_DefaultOpen)) {
        static i32 forestType = 0; // 0=Mixed, 1=Dense Conifer, 2=Deciduous, 3=Sparse Savanna
        static f32 forestAreaHalf = 30.0f;
        static u32 treeDensity = 120;
        static u32 shrubDensity = 200;
        static u32 grassDensity = 8000;
        static bool includeShrubs = true;
        static bool includeGrass = true;

        const char* forestTypes[] = { "Mixed Forest", "Dense Conifer", "Deciduous Park", "Sparse Savanna" };
        ImGui::Combo("Forest Type", &forestType, forestTypes, 4);
        ImGui::DragFloat("Area Radius", &forestAreaHalf, 1.0f, 5.0f, 200.0f, "%.0f m");
        ImGui::DragScalar("Tree Density", ImGuiDataType_U32, &treeDensity, 5.0f);
        ImGui::Checkbox("Include Shrubs", &includeShrubs);
        if (includeShrubs)
            ImGui::DragScalar("Shrub Density", ImGuiDataType_U32, &shrubDensity, 10.0f);
        ImGui::Checkbox("Include Grass", &includeGrass);
        if (includeGrass)
            ImGui::DragScalar("Grass Density", ImGuiDataType_U32, &grassDensity, 100.0f);

        if (ImGui::Button("Generate Forest", ImVec2(-1, 32))) {
            if (m_World) {
                // Create tree volume entity
                auto treeEnt = m_World->CreateEntity();
                m_World->AddComponent<ECS::NameComponent>(treeEnt, ECS::NameComponent{"Forest Trees"});
                auto& treeTf = m_World->AddComponent<ECS::TransformComponent>(treeEnt);
                treeTf.position = Math::Vector3(0, 0, 0);
                auto& tree = m_World->AddComponent<ECS::TreeVolumeComponent>(treeEnt);
                tree.halfExtents = Math::Vector3(forestAreaHalf, 0, forestAreaHalf);
                tree.density = treeDensity;
                switch (forestType) {
                    case 1: // Dense Conifer
                        tree.treeType = ECS::TreeType::Evergreen;
                        tree.trunkHeight = 3.0f;
                        tree.canopyRadius = 0.8f;
                        tree.canopyBaseColor = Math::Vector3(0.05f, 0.25f, 0.08f);
                        tree.canopyTipColor = Math::Vector3(0.08f, 0.35f, 0.1f);
                        break;
                    case 2: // Deciduous Park
                        tree.treeType = ECS::TreeType::Deciduous;
                        tree.trunkHeight = 2.5f;
                        tree.canopyRadius = 1.5f;
                        tree.canopyBaseColor = Math::Vector3(0.15f, 0.45f, 0.1f);
                        tree.canopyTipColor = Math::Vector3(0.3f, 0.6f, 0.2f);
                        tree.windSwayStrength = 0.2f;
                        break;
                    case 3: // Sparse Savanna
                        tree.treeType = ECS::TreeType::Deciduous;
                        tree.trunkHeight = 3.5f;
                        tree.trunkWidth = 0.2f;
                        tree.canopyRadius = 2.0f;
                        tree.canopyOffset = 2.5f;
                        tree.canopyBaseColor = Math::Vector3(0.25f, 0.4f, 0.1f);
                        tree.canopyTipColor = Math::Vector3(0.35f, 0.55f, 0.15f);
                        break;
                    default: break; // Mixed uses defaults
                }

                // Shrub volume
                if (includeShrubs) {
                    auto shrubEnt = m_World->CreateEntity();
                    m_World->AddComponent<ECS::NameComponent>(shrubEnt, ECS::NameComponent{"Forest Shrubs"});
                    auto& shrubTf = m_World->AddComponent<ECS::TransformComponent>(shrubEnt);
                    shrubTf.position = Math::Vector3(0, 0, 0);
                    auto& shrub = m_World->AddComponent<ECS::ShrubVolumeComponent>(shrubEnt);
                    shrub.halfExtents = Math::Vector3(forestAreaHalf, 0, forestAreaHalf);
                    shrub.density = shrubDensity;
                    if (forestType == 3) { // Savanna: sparse low brush
                        shrub.shrubHeight = 0.3f;
                        shrub.baseColor = Math::Vector3(0.3f, 0.4f, 0.15f);
                        shrub.tipColor = Math::Vector3(0.5f, 0.55f, 0.25f);
                    }
                }

                // Grass volume
                if (includeGrass) {
                    auto grassEnt = m_World->CreateEntity();
                    m_World->AddComponent<ECS::NameComponent>(grassEnt, ECS::NameComponent{"Forest Grass"});
                    auto& grassTf = m_World->AddComponent<ECS::TransformComponent>(grassEnt);
                    grassTf.position = Math::Vector3(0, 0, 0);
                    auto& grass = m_World->AddComponent<ECS::GrassVolumeComponent>(grassEnt);
                    grass.halfExtents = Math::Vector3(forestAreaHalf, 0, forestAreaHalf);
                    grass.density = grassDensity;
                    if (forestType == 3) { // Savanna: dry grass
                        grass.baseColor = Math::Vector3(0.4f, 0.45f, 0.15f);
                        grass.tipColor = Math::Vector3(0.6f, 0.55f, 0.25f);
                        grass.bladeHeight = 0.5f;
                    }
                }

                ENJIN_LOG_INFO(Editor, "Generated forest: %u trees, %u shrubs, %u grass blades over %.0f m area",
                    treeDensity, includeShrubs ? shrubDensity : 0, includeGrass ? grassDensity : 0, forestAreaHalf * 2.0f);
            }
        }
        ImGui::SetItemTooltip("Creates Tree + Shrub + Grass volume entities centered at origin");
    }

    // Preview canvas (256x256 max display)
    ImGui::Separator();
    ImGui::Text("Preview:");
    ImVec2 canvasPos = ImGui::GetCursorScreenPos();
    ImVec2 canvasSize(256, 256);
    ImGui::InvisibleButton("##procgen_canvas", canvasSize);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(canvasPos, ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y),
                      IM_COL32(20, 20, 20, 255));

    if (!m_ProceduralPreview.empty() && m_ProceduralPreviewW > 0 && m_ProceduralPreviewH > 0) {
        f32 cellW = canvasSize.x / static_cast<f32>(m_ProceduralPreviewW);
        f32 cellH = canvasSize.y / static_cast<f32>(m_ProceduralPreviewH);
        for (u32 y = 0; y < m_ProceduralPreviewH && y < static_cast<u32>(m_ProceduralPreview.size()); y++) {
            for (u32 x = 0; x < m_ProceduralPreviewW && x < static_cast<u32>(m_ProceduralPreview[y].size()); x++) {
                u8 val = m_ProceduralPreview[y][x];
                if (val == 0 && m_ProceduralAlgorithm != 6) continue; // Skip empty for most algos
                ImU32 color;
                if (m_ProceduralAlgorithm == 6) {
                    // Voronoi: color by region ID
                    u8 r = static_cast<u8>((val * 67) % 200 + 55);
                    u8 g = static_cast<u8>((val * 131) % 200 + 55);
                    u8 b = static_cast<u8>((val * 197) % 200 + 55);
                    color = IM_COL32(r, g, b, 255);
                } else {
                    color = IM_COL32(180, 180, 180, 255);
                }
                f32 px = canvasPos.x + x * cellW;
                f32 py = canvasPos.y + y * cellH;
                dl->AddRectFilled(ImVec2(px, py), ImVec2(px + cellW, py + cellH), color);
            }
        }
    } else if (!m_ProceduralHeightmap.empty() && m_ProceduralPreviewW > 0 && m_ProceduralPreviewH > 0) {
        f32 cellW = canvasSize.x / static_cast<f32>(m_ProceduralPreviewW);
        f32 cellH = canvasSize.y / static_cast<f32>(m_ProceduralPreviewH);
        for (u32 y = 0; y < m_ProceduralPreviewH && y < static_cast<u32>(m_ProceduralHeightmap.size()); y++) {
            for (u32 x = 0; x < m_ProceduralPreviewW && x < static_cast<u32>(m_ProceduralHeightmap[y].size()); x++) {
                f32 h = m_ProceduralHeightmap[y][x];
                u8 gray = static_cast<u8>(std::clamp(h * 255.0f, 0.0f, 255.0f));
                f32 px = canvasPos.x + x * cellW;
                f32 py = canvasPos.y + y * cellH;
                dl->AddRectFilled(ImVec2(px, py), ImVec2(px + cellW, py + cellH),
                                  IM_COL32(gray, gray, gray, 255));
            }
        }
    }

    ImGui::End();
}

// --- Gamepad Editor Navigation ---


void EditorLayer::DrawNetworkPanel() {
    bool panelOpen = true;
    if (!ImGui::Begin("Network", &panelOpen, ImGuiWindowFlags_None)) {
        if (!panelOpen) SetPanelVisibility(EditorPanel::NetworkPanel, false);
        ImGui::End();
        return;
    }
    if (!panelOpen) { SetPanelVisibility(EditorPanel::NetworkPanel, false); ImGui::End(); return; }

    auto* netSystem = m_PlayMode.GetNetworkSystem();
    if (!netSystem) {
        DrawEmptyState("NET", "No Network", "Enter play mode to access networking");
        ImGui::End();
        return;
    }

    Networking::NetworkRole role = netSystem->GetRole();
    Networking::ConnectionState connState = netSystem->GetConnectionState();

    // Connection controls
    if (role == Networking::NetworkRole::None) {
        ImGui::Text("Player Name:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(150);
        ImGui::InputText("##NetName", m_NetworkPlayerName, sizeof(m_NetworkPlayerName));

        ImGui::Separator();

        // Host
        ImGui::Text("Port:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(80);
        ImGui::InputInt("##NetPort", &m_NetworkPort, 0, 0);
        ImGui::SameLine();
        if (ImGui::Button("Host Game")) {
            netSystem->HostGame(static_cast<u16>(m_NetworkPort), m_NetworkPlayerName);
        }

        ImGui::Spacing();

        // Join
        ImGui::Text("Server IP:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(150);
        ImGui::InputText("##NetIP", m_NetworkIP, sizeof(m_NetworkIP));
        ImGui::SameLine();
        if (ImGui::Button("Join Game")) {
            netSystem->JoinGame(m_NetworkIP, static_cast<u16>(m_NetworkPort), m_NetworkPlayerName);
        }
    } else {
        // Status line
        const char* roleStr = (role == Networking::NetworkRole::Host) ? "Host" : "Client";
        const char* stateStr = "Unknown";
        switch (connState) {
            case Networking::ConnectionState::Disconnected: stateStr = "Disconnected"; break;
            case Networking::ConnectionState::Connecting:   stateStr = "Connecting..."; break;
            case Networking::ConnectionState::Connected:    stateStr = "Connected"; break;
            case Networking::ConnectionState::Disconnecting: stateStr = "Disconnecting..."; break;
        }
        ImGui::Text("Role: %s | Status: %s | Port: %u", roleStr, stateStr, netSystem->GetConfig().port);

        if (ImGui::Button("Disconnect")) {
            netSystem->Disconnect();
        }

        ImGui::SameLine();
        bool ready = false;
        for (const auto& lp : netSystem->GetLobbyPlayers()) {
            if (lp.id == netSystem->GetLocalPlayerId()) { ready = lp.ready; break; }
        }
        if (ImGui::Checkbox("Ready", &ready)) {
            netSystem->SetReady(ready);
        }

        ImGui::Separator();

        // Player list
        ImGui::Text("Players (%u):", netSystem->GetConnectedPlayerCount());
        if (ImGui::BeginTable("##Players", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
            ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthFixed, 30);
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Ping", ImGuiTableColumnFlags_WidthFixed, 60);
            ImGui::TableSetupColumn("Ready", ImGuiTableColumnFlags_WidthFixed, 50);
            ImGui::TableHeadersRow();

            for (const auto& player : netSystem->GetLobbyPlayers()) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%u", player.id);
                ImGui::TableSetColumnIndex(1);
                if (player.isHost) {
                    ImGui::TextColored(ImVec4(0.9f, 0.85f, 0.5f, 1.0f), "%s (Host)", player.name.c_str());
                } else {
                    ImGui::Text("%s", player.name.c_str());
                }
                ImGui::TableSetColumnIndex(2);
                if (player.id == netSystem->GetLocalPlayerId()) {
                    ImGui::Text("--");
                } else {
                    ImGui::Text("%.0f ms", netSystem->GetPing());
                }
                ImGui::TableSetColumnIndex(3);
                ImGui::Text("%s", player.ready ? "Yes" : "No");
            }
            ImGui::EndTable();
        }

        ImGui::Separator();

        // Stats
        ImGui::Text("Ping: %.1f ms", netSystem->GetPing());
        ImGui::Text("Packet Loss: %.1f%%", netSystem->GetPacketLoss());
        ImGui::Text("Upload: %.2f KB/s", netSystem->GetUploadKBps());
        ImGui::Text("Download: %.2f KB/s", netSystem->GetDownloadKBps());
    }

    ImGui::End();
}

// ============================================================================
// Collaboration Panel
// ============================================================================

void EditorLayer::DrawCollaborationPanel() {
    bool panelOpen = true;
    if (!ImGui::Begin("Collaboration", &panelOpen, ImGuiWindowFlags_None)) {
        if (!panelOpen) SetPanelVisibility(EditorPanel::Collaboration, false);
        ImGui::End();
        return;
    }
    if (!panelOpen) { SetPanelVisibility(EditorPanel::Collaboration, false); ImGui::End(); return; }

    CollabSessionState state = m_CollabSystem.GetState();

    // --- Connection Section ---
    if (state == CollabSessionState::Disconnected) {
        ImGui::Text("User Name:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(150);
        ImGui::InputText("##CollabName", m_CollabUserName, sizeof(m_CollabUserName));

        ImGui::Separator();

        // Host session
        ImGui::Text("Port:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(80);
        ImGui::InputInt("##CollabPort", &m_CollabPort, 0, 0);
        ImGui::SameLine();
        if (ImGui::Button("Host Session")) {
            auto* netSystem = m_PlayMode.GetNetworkSystem();
            if (netSystem) {
                m_CollabSystem.Initialize(m_World, netSystem);
                m_CollabSystem.HostSession(static_cast<u16>(m_CollabPort), m_CollabUserName);
            }
        }

        ImGui::Spacing();

        // Join session
        ImGui::Text("Host IP:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(150);
        ImGui::InputText("##CollabIP", m_CollabHostIP, sizeof(m_CollabHostIP));
        ImGui::SameLine();
        if (ImGui::Button("Join Session")) {
            auto* netSystem = m_PlayMode.GetNetworkSystem();
            if (netSystem) {
                m_CollabSystem.Initialize(m_World, netSystem);
                m_CollabSystem.JoinSession(m_CollabHostIP, static_cast<u16>(m_CollabPort), m_CollabUserName);
            }
        }
    } else {
        // Active session
        const char* stateStr = "Unknown";
        ImVec4 stateColor = ImVec4(0.7f, 0.7f, 0.7f, 1.0f);
        switch (state) {
            case CollabSessionState::Hosting:
                stateStr = "Hosting";
                stateColor = ImVec4(0.4f, 1.0f, 0.4f, 1.0f);
                break;
            case CollabSessionState::Joining:
                stateStr = "Joining...";
                stateColor = ImVec4(1.0f, 0.8f, 0.3f, 1.0f);
                break;
            case CollabSessionState::Connected:
                stateStr = "Connected";
                stateColor = ImVec4(0.4f, 0.8f, 1.0f, 1.0f);
                break;
            case CollabSessionState::Syncing:
                stateStr = "Syncing...";
                stateColor = ImVec4(1.0f, 0.6f, 0.3f, 1.0f);
                break;
            default: break;
        }
        ImGui::TextColored(stateColor, "Status: %s", stateStr);

        if (ImGui::Button("Leave Session")) {
            m_CollabSystem.LeaveSession();
        }

        ImGui::Separator();

        // Conflict strategy
        if (ImGui::TreeNode("Settings")) {
            const char* strategies[] = { "Last Writer Wins", "Host Authority", "Merge", "Ask (Manual)" };
            int currentStrategy = static_cast<int>(m_CollabSystem.GetConflictStrategy());
            if (ImGui::Combo("Conflict Resolution", &currentStrategy, strategies, IM_ARRAYSIZE(strategies))) {
                m_CollabSystem.SetConflictStrategy(static_cast<ConflictStrategy>(currentStrategy));
            }
            ImGui::TreePop();
        }

        ImGui::Separator();

        // Peers
        const auto& peers = m_CollabSystem.GetPeers();
        if (ImGui::TreeNodeEx("Peers", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (ImGui::BeginTable("##CollabPeers", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
                ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, 80);
                ImGui::TableSetupColumn("Editing", ImGuiTableColumnFlags_WidthFixed, 80);
                ImGui::TableHeadersRow();

                for (const auto& peer : peers) {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    if (peer.peerId == m_CollabSystem.GetCurrentSequence()) {
                        ImGui::TextColored(ImVec4(0.9f, 0.85f, 0.5f, 1.0f), "%s (You)", peer.name.c_str());
                    } else {
                        ImGui::Text("%s", peer.name.c_str());
                    }
                    ImGui::TableSetColumnIndex(1);
                    ImGui::TextColored(peer.connected ? ImVec4(0.3f, 0.9f, 0.3f, 1.0f) : ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
                                       peer.connected ? "Online" : "Offline");
                    ImGui::TableSetColumnIndex(2);
                    if (peer.cursorEntityId != 0 && m_World) {
                        auto* name = m_World->GetComponent<ECS::NameComponent>(static_cast<ECS::Entity>(peer.cursorEntityId));
                        if (name) {
                            ImGui::TextDisabled("%s", name->name.c_str());
                        } else {
                            ImGui::TextDisabled("#%u", peer.cursorEntityId);
                        }
                    } else {
                        ImGui::TextDisabled("--");
                    }
                }
                ImGui::EndTable();
            }
            ImGui::TreePop();
        }

        // Conflicts
        const auto& conflicts = m_CollabSystem.GetConflicts();
        if (!conflicts.empty()) {
            ImGui::Separator();
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.3f, 1.0f), "Conflicts (%zu)", conflicts.size());
            for (usize i = 0; i < conflicts.size(); ++i) {
                const auto& c = conflicts[i];
                ImGui::PushID(static_cast<int>(i));
                ImGui::Text("Entity #%llu: %s vs %s",
                            (unsigned long long)c.localOp.entityId,
                            c.localOp.authorName.c_str(),
                            c.remoteOp.authorName.c_str());
                ImGui::SameLine();
                if (ImGui::SmallButton("Keep Local")) {
                    m_CollabSystem.ResolveConflict(i, ConflictStrategy::Reject);
                }
                ImGui::SameLine();
                if (ImGui::SmallButton("Accept Remote")) {
                    m_CollabSystem.ResolveConflict(i, ConflictStrategy::LastWriterWins);
                }
                ImGui::PopID();
            }
        }

        // Operation log
        ImGui::Separator();
        if (ImGui::TreeNode("Operation Log")) {
            const auto& log = m_CollabSystem.GetOperationLog();
            ImGui::Text("%zu operations", log.size());

            f32 maxH = std::min(200.0f, ImGui::GetContentRegionAvail().y);
            ImGui::BeginChild("##OpLog", ImVec2(0, maxH), true);

            // Show most recent first
            for (auto it = log.rbegin(); it != log.rend(); ++it) {
                const auto& op = *it;
                const char* typeStr = "?";
                switch (op.type) {
                    case EditOpType::CreateEntity:    typeStr = "Create"; break;
                    case EditOpType::DeleteEntity:    typeStr = "Delete"; break;
                    case EditOpType::RenameEntity:    typeStr = "Rename"; break;
                    case EditOpType::SetComponent:    typeStr = "SetComp"; break;
                    case EditOpType::RemoveComponent: typeStr = "RmComp"; break;
                    case EditOpType::ModifyTransform: typeStr = "Transform"; break;
                    case EditOpType::SetParent:       typeStr = "Parent"; break;
                    case EditOpType::LockEntity:      typeStr = "Lock"; break;
                    case EditOpType::UnlockEntity:    typeStr = "Unlock"; break;
                }
                ImGui::TextDisabled("[%llu]", (unsigned long long)op.sequenceId);
                ImGui::SameLine();
                ImGui::Text("%s #%llu", typeStr, (unsigned long long)op.entityId);
                ImGui::SameLine();
                ImGui::TextDisabled("by %s", op.authorName.empty() ? "local" : op.authorName.c_str());
            }

            ImGui::EndChild();
            ImGui::TreePop();
        }
    }

    ImGui::End();
}

// ============================================================================
// Command Palette
// ============================================================================

void EditorLayer::RegisterPaletteCommands() {
    m_CommandPalette.ClearCommands();

    // Entity commands
    m_CommandPalette.RegisterCommand({
        "Create Empty Entity", "Entity", "Ctrl+Shift+N",
        "Create a new empty entity in the scene",
        [this]() {
            if (!m_World) return;
            auto entity = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(entity, ECS::NameComponent{"New Entity"});
            m_World->AddComponent<ECS::TransformComponent>(entity);
            SelectEntity(entity, false);
        }
    });
    m_CommandPalette.RegisterCommand({
        "Delete Selected", "Entity", "Delete",
        "Delete all selected entities",
        [this]() { DeleteSelectedEntities(); }
    });
    m_CommandPalette.RegisterCommand({
        "Duplicate Selected", "Entity", "Ctrl+D",
        "Duplicate the selected entity",
        [this]() { DuplicateSelectedEntities(); }
    });
    m_CommandPalette.RegisterCommand({
        "Focus Selection", "Entity", "F",
        "Focus the camera on the selected entity",
        [this]() { if (!m_SelectedEntities.empty()) FocusOnSelection(); }
    });
    m_CommandPalette.RegisterCommand({
        "Select All", "Entity", "Ctrl+A",
        "Select all entities in the scene",
        [this]() {
            if (!m_World) return;
            auto entities = m_World->GetAllEntities();
            for (auto e : entities) SelectEntity(e, true);
        }
    });
    m_CommandPalette.RegisterCommand({
        "Deselect All", "Entity", "",
        "Clear entity selection",
        [this]() { ClearSelection(); }
    });

    // Scene commands
    m_CommandPalette.RegisterCommand({
        "Save Scene", "Scene", "Ctrl+S",
        "Save the current scene",
        [this]() {
            if (!m_CurrentScenePath.empty()) SaveScene(m_CurrentScenePath);
        }
    });
    m_CommandPalette.RegisterCommand({
        "Save Scene As...", "Scene", "Ctrl+Shift+S",
        "Save the current scene to a new file",
        [this]() {
            std::vector<FileFilter> filters = {{ "Enjin Scene", "*.enjin" }};
            std::string path = FileDialog::SaveFile("Save Scene As", filters, "", "scene.enjin");
            if (!path.empty()) SaveScene(path);
        }
    });
    m_CommandPalette.RegisterCommand({
        "Open Scene...", "Scene", "Ctrl+O",
        "Open an existing scene file",
        [this]() {
            std::vector<FileFilter> filters = {{ "Enjin Scene", "*.enjin" }};
            std::string path = FileDialog::OpenFile("Open Scene", filters);
            if (!path.empty()) OpenScene(path);
        }
    });
    m_CommandPalette.RegisterCommand({
        "New Scene", "Scene", "Ctrl+N",
        "Create a new empty scene",
        [this]() {
            if (m_World) {
                m_World->Clear();
                if (m_RenderSystem) m_RenderSystem->OnSceneClear();
                ClearSelection();
                m_CurrentScenePath.clear();
            }
        }
    });

    // View commands
    m_CommandPalette.RegisterCommand({
        "Toggle Hierarchy Panel", "View", "Ctrl+1",
        "Show or hide the hierarchy panel",
        [this]() { SetPanelVisibility(EditorPanel::Hierarchy, !IsPanelVisible(EditorPanel::Hierarchy)); }
    });
    m_CommandPalette.RegisterCommand({
        "Toggle Inspector Panel", "View", "Ctrl+2",
        "Show or hide the inspector panel",
        [this]() { SetPanelVisibility(EditorPanel::Inspector, !IsPanelVisible(EditorPanel::Inspector)); }
    });
    m_CommandPalette.RegisterCommand({
        "Toggle Console", "View", "Ctrl+4",
        "Show or hide the console panel",
        [this]() { SetPanelVisibility(EditorPanel::Console, !IsPanelVisible(EditorPanel::Console)); }
    });
    m_CommandPalette.RegisterCommand({
        "Toggle Asset Browser", "View", "Ctrl+5",
        "Show or hide the asset browser",
        [this]() { SetPanelVisibility(EditorPanel::AssetBrowser, !IsPanelVisible(EditorPanel::AssetBrowser)); }
    });
    m_CommandPalette.RegisterCommand({
        "Toggle Game View", "View", "",
        "Show or hide the game view",
        [this]() { SetPanelVisibility(EditorPanel::GameView, !IsPanelVisible(EditorPanel::GameView)); }
    });
    m_CommandPalette.RegisterCommand({
        "Toggle Profiler", "View", "",
        "Show or hide the profiler panel",
        [this]() { SetPanelVisibility(EditorPanel::Profiler, !IsPanelVisible(EditorPanel::Profiler)); }
    });

    // Settings commands
    m_CommandPalette.RegisterCommand({
        "System Settings", "Settings", "",
        "Open system settings (theme, accessibility, camera, performance)",
        [this]() { OpenSettings(0); }
    });
    m_CommandPalette.RegisterCommand({
        "Project Settings", "Settings", "",
        "Open project settings (physics, audio, build config)",
        [this]() { OpenSettings(1); }
    });
    m_CommandPalette.RegisterCommand({
        "Scene Settings", "Settings", "",
        "Open scene settings (rendering, post-processing, retro effects)",
        [this]() { OpenSettings(2); }
    });

    // Gizmo commands
    m_CommandPalette.RegisterCommand({
        "Translate Mode", "Gizmo", "1",
        "Switch gizmo to translate mode",
        [this]() { m_GizmoOperation = GizmoOperation::Translate; }
    });
    m_CommandPalette.RegisterCommand({
        "Rotate Mode", "Gizmo", "2",
        "Switch gizmo to rotate mode",
        [this]() { m_GizmoOperation = GizmoOperation::Rotate; }
    });
    m_CommandPalette.RegisterCommand({
        "Scale Mode", "Gizmo", "3",
        "Switch gizmo to scale mode",
        [this]() { m_GizmoOperation = GizmoOperation::Scale; }
    });

    // Play mode commands
    m_CommandPalette.RegisterCommand({
        "Play", "PlayMode", "F5",
        "Start play mode",
        [this]() {
            if (!m_PlayMode.IsPlaying()) {
                m_PlayMode.Play();
            }
        }
    });
    m_CommandPalette.RegisterCommand({
        "Stop", "PlayMode", "Escape",
        "Stop play mode",
        [this]() {
            if (m_PlayMode.IsPlaying() || m_PlayMode.IsPaused()) {
                m_PlayMode.Stop();
                ClearSelection();
            }
        }
    });

    // Accessibility commands
    m_CommandPalette.RegisterCommand({
        "Toggle Command Palette", "Accessibility", "Ctrl+P",
        "Open or close the command palette",
        [this]() { m_CommandPalette.Close(); }
    });
    m_CommandPalette.RegisterCommand({
        "Toggle Keyboard Navigation", "Accessibility", "",
        "Enable or disable keyboard navigation and gizmo nudge",
        [this]() { m_EditorSettings.keyboardNavEnabled = !m_EditorSettings.keyboardNavEnabled; }
    });
    m_CommandPalette.RegisterCommand({
        "Toggle Announcer", "Accessibility", "",
        "Enable or disable the accessibility announcer status bar",
        [this]() { m_Announcer.enabled = !m_Announcer.enabled; }
    });

    // Tools
    m_CommandPalette.RegisterCommand({
        "Undo", "Edit", "Ctrl+Z",
        "Undo the last action",
        [this]() { m_UndoRedo.Undo(); }
    });
    m_CommandPalette.RegisterCommand({
        "Redo", "Edit", "Ctrl+Y",
        "Redo the last undone action",
        [this]() { m_UndoRedo.Redo(); }
    });

    // Feedback / Bug Reporting
    m_CommandPalette.RegisterCommand({
        "Report Bug", "Help", "Ctrl+Shift+B",
        "Open the Discord bug report dialog",
        [this]() {
            m_ShowDiscordBugDialog = true;
            m_DiscordSendState = DiscordSendState::Idle;
        }
    });
    m_CommandPalette.RegisterCommand({
        "Send Feedback", "Help", "",
        "Open the feedback form",
        [this]() {
            SetPanelVisibility(EditorPanel::FeedbackPanel, true);
            m_FeedbackTab = FeedbackTab::NewFeedback;
        }
    });
    m_CommandPalette.RegisterCommand({
        "Browse Bug Reports", "Help", "",
        "View all bug reports and feedback",
        [this]() {
            SetPanelVisibility(EditorPanel::FeedbackPanel, true);
            m_FeedbackTab = FeedbackTab::BugReports;
        }
    });
}

// ============================================================================
// Keyboard Gizmo Nudge
// ============================================================================


void EditorLayer::DrawFlashTimelinePanel() {
    bool panelOpen = true;
    if (!ImGui::Begin("Flash Timeline", &panelOpen)) {
        if (!panelOpen) SetPanelVisibility(EditorPanel::FlashTimeline, false);
        ImGui::End();
        return;
    }
    if (!panelOpen) { SetPanelVisibility(EditorPanel::FlashTimeline, false); ImGui::End(); return; }

    if (!m_World) {
        DrawEmptyState("[ ]", "No World Loaded", "Open or create a scene to begin");
        ImGui::End();
        return;
    }

    // Tab bar: Timeline | SWF Import | AS Transpiler | Templates
    if (ImGui::BeginTabBar("FlashTabs")) {

        // --- Timeline Tab ---
        if (ImGui::BeginTabItem("Timeline")) {
            // Initialize timeline data if needed
            if (m_FlashTimelineEditor.GetTimeline() == nullptr) {
                m_FlashTimelineEditor.SetTimeline(&m_FlashTimelineData);
            }

            // Entity assignment for selected layer
            if (!m_FlashTimelineData.layers.empty()) {
                u32 layerIdx = 0; // Use first layer for now
                if (m_PrimarySelected != ECS::INVALID_ENTITY) {
                    auto& layer = m_FlashTimelineData.layers[layerIdx];
                    if (layer.entity == 0) {
                        ImGui::TextColored(ImVec4(1, 0.8f, 0, 1),
                            "Select an entity and click Assign to link it to the current layer");
                        ImGui::SameLine();
                        if (ImGui::Button("Assign Entity")) {
                            layer.entity = m_PrimarySelected;
                            auto* name = m_World->GetComponent<ECS::NameComponent>(m_PrimarySelected);
                            if (name) layer.name = name->name;
                        }
                    }
                }
            }

            // Update playback
            m_FlashTimelineEditor.Update(ImGui::GetIO().DeltaTime);

            // Render timeline editor
            m_FlashTimelineEditor.Render(m_World, m_EditorSettings);

            // Convert to engine timeline button
            ImGui::Separator();
            if (ImGui::Button("Convert to Engine Timeline")) {
                m_FlashTimelineEditor.ConvertToTimeline(m_World);
            }
            ImGui::SameLine();
            ImGui::TextDisabled("Converts keyframes to TimelineComponent property tracks");

            ImGui::EndTabItem();
        }

        // --- SWF Import Tab ---
        if (ImGui::BeginTabItem("SWF Import")) {
            ImGui::Text("Import Adobe SWF files");
            ImGui::Separator();

            if (ImGui::Button("Open SWF File...")) {
                std::vector<FileFilter> filters = {
                    { "SWF Files", "*.swf" },
                    { "All Files", "*.*" }
                };
                std::string path = FileDialog::OpenFile("Import SWF", filters);
                if (!path.empty()) {
                    auto result = Assets::SWFLoader::Parse(path);
                    if (result.success) {
                        auto& doc = result.document;
                        ENJIN_LOG_INFO(Editor, "SWF loaded: v%u, %.0fx%.0f @ %.1f fps, "
                                       "%zu shapes, %zu sprites, %zu bitmaps",
                                       doc.version, doc.stageWidth, doc.stageHeight,
                                       doc.frameRate,
                                       doc.shapes.size(), doc.sprites.size(),
                                       doc.bitmaps.size());

                        // Configure timeline from SWF
                        m_FlashTimelineData.name = std::filesystem::path(path).stem().string();
                        m_FlashTimelineData.frameRate = doc.frameRate;
                        m_FlashTimelineData.totalFrames = static_cast<u32>(doc.mainTimeline.size());
                        m_FlashTimelineData.stageWidth = doc.stageWidth;
                        m_FlashTimelineData.stageHeight = doc.stageHeight;

                        // Create layers from sprites
                        m_FlashTimelineData.layers.clear();
                        for (auto& [id, sprite] : doc.sprites) {
                            std::string layerName = sprite.exportName.empty()
                                ? ("Sprite " + std::to_string(id))
                                : sprite.exportName;
                            m_FlashTimelineData.AddLayer(layerName);

                            // Convert sprite frames to keyframes
                            auto& layer = m_FlashTimelineData.layers.back();
                            for (auto& frame : sprite.frames) {
                                if (!frame.placements.empty()) {
                                    auto& kf = layer.GetOrCreateKeyframe(frame.frameIndex);
                                    auto& placement = frame.placements[0];
                                    kf.position.x = placement.matrix.translateX;
                                    kf.position.y = placement.matrix.translateY;
                                    kf.scale.x = placement.matrix.scaleX;
                                    kf.scale.y = placement.matrix.scaleY;
                                    if (frame.hasLabel) {
                                        kf.label = frame.label.name;
                                    }
                                }
                            }
                        }

                        // If no sprites, create a main timeline layer
                        if (m_FlashTimelineData.layers.empty()) {
                            m_FlashTimelineData.AddLayer("Main Timeline");
                        }

                        m_FlashTimelineEditor.SetTimeline(&m_FlashTimelineData);

                        // Show warnings
                        for (auto& w : doc.warnings) {
                            ENJIN_LOG_WARN(Editor, "SWF: %s", w.c_str());
                        }
                    } else {
                        ENJIN_LOG_ERROR(Editor, "SWF parse failed: %s",
                                        result.errorMessage.c_str());
                    }
                }
            }

            ImGui::SameLine();
            if (ImGui::Button("Export as SVG")) {
                // Export shapes from timeline data as SVG preview
                ImGui::TextDisabled("(Exports SWF vector shapes as SVG)");
            }

            // Display info about loaded SWF timeline
            if (!m_FlashTimelineData.layers.empty()) {
                ImGui::Separator();
                ImGui::Text("Timeline: %s", m_FlashTimelineData.name.c_str());
                ImGui::Text("Stage: %.0f x %.0f", m_FlashTimelineData.stageWidth,
                            m_FlashTimelineData.stageHeight);
                ImGui::Text("Frames: %u @ %.1f fps", m_FlashTimelineData.totalFrames,
                            m_FlashTimelineData.frameRate);
                ImGui::Text("Layers: %zu", m_FlashTimelineData.layers.size());

                // Symbol library
                if (!m_FlashTimelineData.symbolLibrary.empty()) {
                    if (ImGui::TreeNode("Symbol Library")) {
                        for (auto& [name, path] : m_FlashTimelineData.symbolLibrary) {
                            ImGui::BulletText("%s -> %s", name.c_str(), path.c_str());
                        }
                        ImGui::TreePop();
                    }
                }
            }

            ImGui::EndTabItem();
        }

        // --- AS3 Transpiler Tab ---
        if (ImGui::BeginTabItem("AS Transpiler")) {
            ImGui::Text("ActionScript 2/3 to AngelScript Transpiler");
            ImGui::Separator();

            // Input area
            ImGui::Text("ActionScript Input:");
            ImGui::InputTextMultiline("##ASInput", m_AS3TranspileInput,
                                       sizeof(m_AS3TranspileInput),
                                       ImVec2(-1, 120));

            // Options
            static Scripting::TranspilerConfig config;
            static int versionIdx = 2;  // Auto
            const char* versions[] = { "AS2", "AS3", "Auto-detect" };
            ImGui::Combo("Source Version", &versionIdx, versions, 3);
            config.sourceVersion = static_cast<Scripting::ASVersion>(versionIdx);

            ImGui::Checkbox("Preserve Comments", &config.preserveComments);
            ImGui::SameLine();
            ImGui::Checkbox("Wrap in TegeBehavior", &config.wrapInClass);
            ImGui::SameLine();
            ImGui::Checkbox("Verbose", &config.verbose);

            // Transpile button
            if (ImGui::Button("Transpile")) {
                auto result = m_AS3Transpiler.Transpile(m_AS3TranspileInput, config);
                if (result.success) {
                    m_AS3TranspileOutput = result.angelScript;
                    ENJIN_LOG_INFO(Editor, "Transpiled: %u lines, %u matches, %u warnings",
                                   result.linesProcessed, result.patternsMatched,
                                   static_cast<u32>(result.warnings.size()));
                } else {
                    m_AS3TranspileOutput = "// Error: " + result.errorMessage;
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Load File...")) {
                std::vector<FileFilter> filters = {
                    { "ActionScript", "*.as;*.as3;*.as2" },
                    { "All Files", "*.*" }
                };
                std::string path = FileDialog::OpenFile("Open ActionScript", filters);
                if (!path.empty()) {
                    std::ifstream file(path);
                    if (file.is_open()) {
                        std::string content((std::istreambuf_iterator<char>(file)),
                                             std::istreambuf_iterator<char>());
                        strncpy(m_AS3TranspileInput, content.c_str(),
                                sizeof(m_AS3TranspileInput) - 1);
                        m_AS3TranspileInput[sizeof(m_AS3TranspileInput) - 1] = '\0';
                    }
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Save Output...")) {
                std::vector<FileFilter> filters = {
                    { "AngelScript", "*.as" },
                    { "All Files", "*.*" }
                };
                std::string path = FileDialog::SaveFile("Save AngelScript", filters, "", "converted.as");
                if (!path.empty()) {
                    std::ofstream out(path);
                    if (out.is_open()) {
                        out << m_AS3TranspileOutput;
                        ENJIN_LOG_INFO(Editor, "Saved transpiled output to %s", path.c_str());
                    }
                }
            }

            // Output area
            ImGui::Separator();
            ImGui::Text("AngelScript Output:");
            // Display as read-only
            ImGui::InputTextMultiline("##ASOutput",
                                       const_cast<char*>(m_AS3TranspileOutput.c_str()),
                                       m_AS3TranspileOutput.size() + 1,
                                       ImVec2(-1, 120),
                                       ImGuiInputTextFlags_ReadOnly);

            ImGui::EndTabItem();
        }

        // --- Newgrounds Tab ---
        if (ImGui::BeginTabItem("Newgrounds")) {
            ImGui::Text("Newgrounds.io Integration");
            ImGui::Separator();

            // App credentials
            ImGui::Text("App ID:");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(200);
            ImGui::InputText("##NGAppId", m_NGAppId, sizeof(m_NGAppId));

            ImGui::Text("Encryption Key:");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(200);
            ImGui::InputText("##NGKey", m_NGEncryptionKey, sizeof(m_NGEncryptionKey),
                              ImGuiInputTextFlags_Password);

            // Connect button
            if (m_NewgroundsAPI.IsConnected()) {
                ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.3f, 1), "Connected as: %s",
                                   m_NewgroundsAPI.GetSession().userName.c_str());
                ImGui::SameLine();
                if (ImGui::Button("Refresh")) {
                    m_NewgroundsAPI.CheckSession();
                }
            } else {
                if (ImGui::Button("Connect")) {
                    if (strlen(m_NGAppId) > 0 && strlen(m_NGEncryptionKey) > 0) {
                        m_NewgroundsAPI.Initialize(m_NGAppId, m_NGEncryptionKey);
                        m_NewgroundsAPI.StartSession();
                    }
                }
                ImGui::SameLine();
                if (ImGui::Button("Check Session")) {
                    m_NewgroundsAPI.CheckSession();
                }

                std::string passport = m_NewgroundsAPI.GetPassportUrl();
                if (!passport.empty()) {
                    ImGui::TextWrapped("Passport URL: %s", passport.c_str());
                    ImGui::SameLine();
                    if (ImGui::Button("Copy URL")) {
                        ImGui::SetClipboardText(passport.c_str());
                    }
                }
            }

            ImGui::Separator();

            // Medals section
            if (ImGui::TreeNode("Medals")) {
                if (ImGui::Button("Refresh Medals")) {
                    // Medals are fetched on demand
                }
                static std::vector<Networking::NGMedal> cachedMedals;
                if (ImGui::IsItemClicked()) {
                    cachedMedals = m_NewgroundsAPI.GetMedals();
                }
                for (auto& medal : cachedMedals) {
                    ImGui::BulletText("%s (%d pts) %s",
                                      medal.name.c_str(), medal.value,
                                      medal.unlocked ? "[Unlocked]" : "");
                }
                ImGui::TreePop();
            }

            // Scoreboards section
            if (ImGui::TreeNode("Scoreboards")) {
                static std::vector<Networking::NGScoreBoard> cachedBoards;
                if (ImGui::Button("Refresh Boards")) {
                    cachedBoards = m_NewgroundsAPI.GetScoreBoards();
                }
                for (auto& board : cachedBoards) {
                    ImGui::BulletText("[%d] %s", board.id, board.name.c_str());
                }

                static i32 testBoardId = 0;
                static i32 testScore = 100;
                ImGui::SetNextItemWidth(80);
                ImGui::InputInt("Board ID", &testBoardId);
                ImGui::SameLine();
                ImGui::SetNextItemWidth(80);
                ImGui::InputInt("Score", &testScore);
                ImGui::SameLine();
                if (ImGui::Button("Post Test Score")) {
                    m_NewgroundsAPI.PostScore(testBoardId, testScore);
                }
                ImGui::TreePop();
            }

            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::End();
}

// ============================================================================
// Vector Drawing Panel
// ============================================================================

void EditorLayer::DrawVectorDrawingPanel() {
    bool panelOpen = true;
    if (!ImGui::Begin("Vector Drawing", &panelOpen)) {
        if (!panelOpen) SetPanelVisibility(EditorPanel::VectorDrawing, false);
        ImGui::End();
        return;
    }
    if (!panelOpen) { SetPanelVisibility(EditorPanel::VectorDrawing, false); ImGui::End(); return; }

    m_VectorDrawingEditor.Render(m_EditorSettings);

    // Symbol library integration with Flash Timeline
    if (m_VectorDrawingEditor.HasDocument()) {
        ImGui::Separator();
        static char symbolName[128] = "MySymbol";
        ImGui::SetNextItemWidth(150);
        ImGui::InputText("Symbol Name", symbolName, sizeof(symbolName));
        ImGui::SameLine();
        if (ImGui::Button("Save as Flash Symbol")) {
            if (m_VectorDrawingEditor.SaveAsSymbol(symbolName, ".")) {
                std::string svgPath = std::string(symbolName) + ".svg";
                m_FlashTimelineData.symbolLibrary[symbolName] = svgPath;
            }
        }
    }

    ImGui::End();
}

// ============================================================================
// HTML5 Export Dialog
// ============================================================================


void EditorLayer::DrawSaveDebugPanel() {
    bool open = true;
    ImGui::Begin("Save Debug", &open);
    if (!open) {
        SetPanelVisibility(EditorPanel::SaveDebug, false);
        ImGui::End();
        return;
    }

    auto* saveSystem = m_PlayMode.GetTieredSaveSystem();
    if (!saveSystem) {
        ImGui::Text("Save system not available.");
        ImGui::End();
        return;
    }

    // Session info
    ImGui::Text("Session Play Time: %.1f s", saveSystem->GetSessionPlayTime());
    ImGui::Text("Current Scene: %s", saveSystem->GetCurrentScene().empty()
        ? "(none)" : saveSystem->GetCurrentScene().c_str());
    ImGui::Separator();

    // Auto-save config
    if (ImGui::CollapsingHeader("Auto-Save Config")) {
        auto& cfg = saveSystem->GetAutoSaveConfig();
        ImGui::Checkbox("Enabled", &cfg.enabled);
        ImGui::Checkbox("On Scene Transition", &cfg.onSceneTransition);
        ImGui::Checkbox("On Timed Interval", &cfg.onTimedInterval);
        if (cfg.onTimedInterval) {
            ImGui::SliderFloat("Interval (seconds)", &cfg.intervalSeconds, 30.0f, 900.0f, "%.0f");
        }
        ImGui::Checkbox("On Checkpoint", &cfg.onCheckpoint);
    }

    // Save slots grid
    if (ImGui::CollapsingHeader("Save Slots", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto slots = saveSystem->GetAllSlots();

        if (ImGui::BeginTable("SaveSlots", 6, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                              ImGuiTableFlags_Resizable)) {
            ImGui::TableSetupColumn("Slot", ImGuiTableColumnFlags_WidthFixed, 40.0f);
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Scene");
            ImGui::TableSetupColumn("Time");
            ImGui::TableSetupColumn("Date");
            ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed, 150.0f);
            ImGui::TableHeadersRow();

            for (const auto& slot : slots) {
                ImGui::TableNextRow();
                ImGui::PushID(static_cast<int>(slot.slotIndex));

                ImGui::TableNextColumn();
                if (slot.isAutoSave) {
                    ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "A%u",
                                       slot.slotIndex - Gameplay::TieredSaveSystem::AUTO_SAVE_SLOT_START + 1);
                } else {
                    ImGui::Text("%u", slot.slotIndex);
                }

                ImGui::TableNextColumn();
                ImGui::Text("%s", slot.isEmpty ? "(empty)" : slot.displayName.c_str());

                ImGui::TableNextColumn();
                ImGui::Text("%s", slot.sceneName.c_str());

                ImGui::TableNextColumn();
                if (!slot.isEmpty) {
                    i32 mins = static_cast<i32>(slot.playTime) / 60;
                    i32 secs = static_cast<i32>(slot.playTime) % 60;
                    ImGui::Text("%d:%02d", mins, secs);
                }

                ImGui::TableNextColumn();
                ImGui::Text("%s", slot.timestamp.c_str());

                ImGui::TableNextColumn();
                if (m_PlayMode.IsPlaying() || m_PlayMode.IsPaused()) {
                    if (ImGui::SmallButton("Save")) {
                        saveSystem->SaveToSlot(slot.slotIndex, m_World,
                            saveSystem->GetCurrentScene());
                    }
                    ImGui::SameLine();
                    if (!slot.isEmpty && ImGui::SmallButton("Load")) {
                        saveSystem->LoadFromSlot(slot.slotIndex, m_World);
                    }
                    ImGui::SameLine();
                }
                if (!slot.isEmpty && ImGui::SmallButton("Del")) {
                    saveSystem->DeleteSlot(slot.slotIndex);
                }

                ImGui::PopID();
            }
            ImGui::EndTable();
        }
    }

    // Meta-progression viewer
    if (ImGui::CollapsingHeader("Meta-Progression")) {
        const auto& floats = saveSystem->GetMetaFloats();
        const auto& ints = saveSystem->GetMetaInts();
        const auto& bools = saveSystem->GetMetaBools();
        const auto& strings = saveSystem->GetMetaStrings();

        bool hasAny = !floats.empty() || !ints.empty() || !bools.empty() || !strings.empty();
        if (!hasAny) {
            ImGui::TextDisabled("No meta-progression data");
        } else {
            if (!floats.empty() && ImGui::TreeNode("Floats")) {
                for (const auto& [k, v] : floats) {
                    ImGui::Text("%s: %.3f", k.c_str(), v);
                }
                ImGui::TreePop();
            }
            if (!ints.empty() && ImGui::TreeNode("Ints")) {
                for (const auto& [k, v] : ints) {
                    ImGui::Text("%s: %d", k.c_str(), v);
                }
                ImGui::TreePop();
            }
            if (!bools.empty() && ImGui::TreeNode("Bools")) {
                for (const auto& [k, v] : bools) {
                    ImGui::Text("%s: %s", k.c_str(), v ? "true" : "false");
                }
                ImGui::TreePop();
            }
            if (!strings.empty() && ImGui::TreeNode("Strings")) {
                for (const auto& [k, v] : strings) {
                    ImGui::Text("%s: %s", k.c_str(), v.c_str());
                }
                ImGui::TreePop();
            }
        }

        ImGui::Separator();
        if (ImGui::Button("Save Meta")) {
            saveSystem->SaveMeta();
        }
        ImGui::SameLine();
        if (ImGui::Button("Reload Meta")) {
            saveSystem->LoadMeta();
        }
    }

    // Cloud sync
    if (saveSystem->GetCloudBackend()) {
        if (ImGui::CollapsingHeader("Cloud Sync")) {
            ImGui::Text("Backend: %s", saveSystem->GetCloudBackend()->GetName().c_str());
            if (ImGui::Button("Sync To Cloud")) {
                saveSystem->SyncToCloud();
            }
            ImGui::SameLine();
            if (ImGui::Button("Sync From Cloud")) {
                saveSystem->SyncFromCloud();
            }
        }
    }

    ImGui::End();
}


void EditorLayer::DrawFeedbackPanel() {
    bool open = true;
    ImGui::Begin("Bug Reports & Feedback", &open);
    if (!open) {
        SetPanelVisibility(EditorPanel::FeedbackPanel, false);
        ImGui::End();
        return;
    }

    // Tab bar
    if (ImGui::BeginTabBar("FeedbackTabs")) {
        if (ImGui::BeginTabItem("Bug Reports")) {
            m_FeedbackTab = FeedbackTab::BugReports;
            DrawBugReportList();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Feedback")) {
            m_FeedbackTab = FeedbackTab::Feedback;
            DrawFeedbackList();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("New Bug Report")) {
            m_FeedbackTab = FeedbackTab::NewBug;
            DrawNewBugReportForm();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("New Feedback")) {
            m_FeedbackTab = FeedbackTab::NewFeedback;
            DrawNewFeedbackForm();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("GitHub Issues")) {
            m_FeedbackTab = FeedbackTab::GitHubIssues;
            DrawGitHubIssuesTab();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Settings")) {
            m_FeedbackTab = FeedbackTab::GitHubSettings;
            DrawGitHubSettingsTab();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    ImGui::End();
}

void EditorLayer::DrawBugReportList() {
    auto& reports = m_FeedbackManager.GetBugReports();
    if (reports.empty()) {
        DrawEmptyState("!", "No Bug Reports", "Reports you file will appear here.",
                        "Report a Bug", [this]() {
                            m_FeedbackTab = FeedbackTab::NewBug;
                        });
        return;
    }

    // Search and filter row
    ImGui::SetNextItemWidth(200);
    ImGui::InputTextWithHint("##BugSearch", "Search...", m_FeedbackSearchBuf, sizeof(m_FeedbackSearchBuf));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(100);
    const char* severityOpts[] = { "All", "Low", "Medium", "High", "Critical" };
    ImGui::Combo("Severity##BugFilter", &m_BugSeverityFilter, severityOpts, 5);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(110);
    const char* statusOpts[] = { "All", "Draft", "Submitted", "Acknowledged", "Resolved", "Closed" };
    ImGui::Combo("Status##BugFilter", &m_BugStatusFilter, statusOpts, 6);

    // Stats row
    usize total = m_FeedbackManager.GetTotalBugReports();
    usize open = m_FeedbackManager.GetOpenBugReports();
    ImGui::TextDisabled("%zu open / %zu total", open, total);
    ImGui::Separator();

    // Apply filters: -1 means "All" (first entry is "All" so actual enum starts at index 1)
    i32 actualSeverity = m_BugSeverityFilter > 0 ? m_BugSeverityFilter - 1 : -1;
    i32 actualStatus = m_BugStatusFilter > 0 ? m_BugStatusFilter - 1 : -1;
    auto filtered = m_FeedbackManager.FilterBugReports(actualStatus, actualSeverity);

    // Further filter by search text
    std::string searchStr(m_FeedbackSearchBuf);
    std::vector<BugReport*> displayList;
    for (auto* r : filtered) {
        if (searchStr.empty() ||
            FeedbackManager::CaseInsensitiveContains(r->title, searchStr) ||
            FeedbackManager::CaseInsensitiveContains(r->description, searchStr)) {
            displayList.push_back(r);
        }
    }

    // Scrollable list
    ImGui::BeginChild("BugList", ImVec2(0, m_SelectedBugReportId > 0 ? 200 : 0), true);
    for (auto* r : displayList) {
        // Severity color indicator
        ImVec4 sevColor(0.5f, 0.5f, 0.5f, 1.0f);
        switch (r->severity) {
            case ReportSeverity::Low:      sevColor = ImVec4(0.4f, 0.7f, 0.4f, 1.0f); break;
            case ReportSeverity::Medium:   sevColor = ImVec4(0.9f, 0.8f, 0.2f, 1.0f); break;
            case ReportSeverity::High:     sevColor = ImVec4(0.9f, 0.5f, 0.2f, 1.0f); break;
            case ReportSeverity::Critical: sevColor = ImVec4(0.9f, 0.2f, 0.2f, 1.0f); break;
        }
        ImGui::PushStyleColor(ImGuiCol_Text, sevColor);
        ImGui::Text("[%s]", ReportSeverityLabel(r->severity));
        ImGui::PopStyleColor();
        ImGui::SameLine();

        char label[256];
        snprintf(label, sizeof(label), "#%llu %s##bug_%llu",
                 static_cast<unsigned long long>(r->id), r->title.c_str(),
                 static_cast<unsigned long long>(r->id));
        bool selected = (m_SelectedBugReportId == r->id);
        if (ImGui::Selectable(label, selected)) {
            m_SelectedBugReportId = r->id;
        }
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 120);
        ImGui::TextDisabled("%s | %s", ReportTypeLabel(r->type), ReportStatusLabel(r->status));
    }
    ImGui::EndChild();

    // Detail view for selected report
    if (m_SelectedBugReportId > 0) {
        BugReport* sel = m_FeedbackManager.GetBugReport(m_SelectedBugReportId);
        if (sel) {
            ImGui::Separator();
            DrawBugReportDetail(*sel);
        }
    }
}

void EditorLayer::DrawBugReportDetail(BugReport& report) {
    ImGui::BeginChild("BugDetail", ImVec2(0, 0), false);

    ImGui::Text("Bug Report #%llu: %s", static_cast<unsigned long long>(report.id), report.title.c_str());
    ImGui::Separator();

    ImGui::TextWrapped("Type: %s | Severity: %s | Status: %s",
                       ReportTypeLabel(report.type),
                       ReportSeverityLabel(report.severity),
                       ReportStatusLabel(report.status));
    ImGui::TextDisabled("Created: %s | Updated: %s", report.createdAt.c_str(), report.updatedAt.c_str());
    ImGui::Spacing();

    if (!report.description.empty()) {
        ImGui::Text("Description:");
        ImGui::TextWrapped("%s", report.description.c_str());
        ImGui::Spacing();
    }
    if (!report.stepsToReproduce.empty()) {
        ImGui::Text("Steps to Reproduce:");
        ImGui::TextWrapped("%s", report.stepsToReproduce.c_str());
        ImGui::Spacing();
    }
    if (!report.expectedBehavior.empty()) {
        ImGui::Text("Expected:");
        ImGui::TextWrapped("%s", report.expectedBehavior.c_str());
    }
    if (!report.actualBehavior.empty()) {
        ImGui::Text("Actual:");
        ImGui::TextWrapped("%s", report.actualBehavior.c_str());
        ImGui::Spacing();
    }

    // Diagnostics summary
    if (ImGui::TreeNode("Diagnostics")) {
        auto& d = report.diagnostics;
        ImGui::Text("Engine: %s | Platform: %s", d.engineVersion.c_str(), d.platform.c_str());
        ImGui::Text("FPS: %.1f | Frame: %.2fms | Draw Calls: %u", d.fps, d.frameTimeMs, d.drawCalls);
        ImGui::Text("Entities: %u | Triangles: %u", d.entityCount, d.triangleCount);
        ImGui::Text("RAM: %.1f MB used / %.1f MB total",
                     d.ramProcess / (1024.0f * 1024.0f),
                     d.ramTotal / (1024.0f * 1024.0f));
        if (!d.scenePath.empty()) ImGui::Text("Scene: %s", d.scenePath.c_str());
        ImGui::Text("Timestamp: %s", d.timestamp.c_str());
        if (!d.consoleLogTail.empty() && ImGui::TreeNode("Console Log (last 50)")) {
            for (const auto& line : d.consoleLogTail)
                ImGui::TextUnformatted(line.c_str());
            ImGui::TreePop();
        }
        ImGui::TreePop();
    }

    // Attachments
    if (!report.attachmentPaths.empty() && ImGui::TreeNode("Attachments")) {
        for (const auto& path : report.attachmentPaths)
            ImGui::BulletText("%s", path.c_str());
        ImGui::TreePop();
    }

    ImGui::Spacing();
    ImGui::Separator();

    // Action buttons
    if (ImGui::Button("Export JSON")) {
        std::vector<FileFilter> filters = {{"JSON", "*.json"}};
        char defaultName[64];
        snprintf(defaultName, sizeof(defaultName), "bug_%llu.json",
                 static_cast<unsigned long long>(report.id));
        std::string path = FileDialog::SaveFile("Export Bug Report", filters, "", defaultName);
        if (!path.empty()) {
            m_FeedbackManager.ExportBugReportAsJson(report.id, path);
            m_ConsoleLog.push_back("[Feedback] Exported bug report #" + std::to_string(report.id));
        }
    }
    ImGui::SameLine();
    if (m_FeedbackEndpointBuf[0] != '\0' && ImGui::Button("Submit")) {
        if (m_FeedbackManager.SubmitBugReport(report.id, std::string(m_FeedbackEndpointBuf))) {
            m_ConsoleLog.push_back("[Feedback] Bug report #" + std::to_string(report.id) + " submitted");
        } else {
            m_ConsoleLog.push_back("[Feedback] Failed to submit bug report #" + std::to_string(report.id));
        }
    }
    ImGui::SameLine();
    // Submit to GitHub Issues
    if (m_FeedbackManager.IsGitHubConfigured()) {
        if (ImGui::Button("Submit to GitHub")) {
            if (m_FeedbackManager.SubmitBugReportToGitHub(report.id)) {
                m_ConsoleLog.push_back({
                    "[Feedback] Bug report #" + std::to_string(report.id) + " submitted to GitHub Issues"});
            } else {
                m_ConsoleLog.push_back({
                    "[Feedback] Failed to submit bug report #" + std::to_string(report.id) + " to GitHub"});
            }
        }
    } else {
        ImGui::BeginDisabled();
        ImGui::Button("Submit to GitHub");
        ImGui::EndDisabled();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
            ImGui::SetTooltip("Configure GitHub token in the Settings tab");
        }
    }
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.2f, 0.2f, 1.0f));
    if (ImGui::Button("Delete")) {
        m_FeedbackManager.DeleteBugReport(report.id);
        m_SelectedBugReportId = 0;
        m_ConsoleLog.push_back("[Feedback] Deleted bug report #" + std::to_string(report.id));
    }
    ImGui::PopStyleColor();

    // Endpoint config
    ImGui::SameLine();
    ImGui::SetNextItemWidth(200);
    ImGui::InputTextWithHint("##Endpoint", "Submit endpoint URL", m_FeedbackEndpointBuf, sizeof(m_FeedbackEndpointBuf));

    ImGui::EndChild();
}

void EditorLayer::DrawNewBugReportForm() {
    ImGui::Text("Title *");
    ImGui::SetNextItemWidth(-1);
    ImGui::InputText("##BugTitle", m_BugTitleBuf, sizeof(m_BugTitleBuf));

    // Type and severity on same row
    ImGui::Text("Type:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(120);
    const char* typeOpts[] = { "Bug", "Crash", "Performance", "Visual", "Audio", "Other" };
    ImGui::Combo("##BugType", &m_BugTypeSel, typeOpts, 6);
    ImGui::SameLine();
    ImGui::Text("Severity:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(100);
    const char* sevOpts[] = { "Low", "Medium", "High", "Critical" };
    // Color the severity label
    ImVec4 sevColors[] = {
        {0.4f, 0.7f, 0.4f, 1.0f},
        {0.9f, 0.8f, 0.2f, 1.0f},
        {0.9f, 0.5f, 0.2f, 1.0f},
        {0.9f, 0.2f, 0.2f, 1.0f}
    };
    ImGui::PushStyleColor(ImGuiCol_Text, sevColors[m_BugSeveritySel]);
    ImGui::Combo("##BugSeverity", &m_BugSeveritySel, sevOpts, 4);
    ImGui::PopStyleColor();

    ImGui::Spacing();
    ImGui::Text("Description:");
    ImGui::InputTextMultiline("##BugDesc", m_BugDescriptionBuf, sizeof(m_BugDescriptionBuf),
                               ImVec2(-1, 80));

    ImGui::Text("Steps to Reproduce:");
    ImGui::InputTextMultiline("##BugSteps", m_BugStepsBuf, sizeof(m_BugStepsBuf),
                               ImVec2(-1, 60));

    ImGui::Text("Expected Behavior:");
    ImGui::InputTextMultiline("##BugExpected", m_BugExpectedBuf, sizeof(m_BugExpectedBuf),
                               ImVec2(-1, 40));

    ImGui::Text("Actual Behavior:");
    ImGui::InputTextMultiline("##BugActual", m_BugActualBuf, sizeof(m_BugActualBuf),
                               ImVec2(-1, 40));

    ImGui::Spacing();
    ImGui::Checkbox("Include console logs", &m_BugIncludeLogs);
    ImGui::SameLine();
    ImGui::Checkbox("Include scene snapshot", &m_BugIncludeScene);

    // Live diagnostics preview
    ImGui::Spacing();
    if (ImGui::TreeNode("Live Diagnostics Preview")) {
        f32 fps = m_FrameTimeAvg > 0.0f ? 1000.0f / m_FrameTimeAvg : 0.0f;
        u32 entityCount = m_World ? static_cast<u32>(m_World->GetEntityCount()) : 0;
        ImGui::Text("FPS: %.1f | Frame: %.2fms", fps, m_FrameTimeAvg);
        ImGui::Text("Draw Calls: %u | Triangles: %u", m_PerfMetrics.drawCallCount, m_PerfMetrics.triangleCount);
        ImGui::Text("Entities: %u | Selected: %zu", entityCount, m_SelectedEntities.size());
        ImGui::Text("RAM: %.1f MB / %.1f MB",
                     m_PerfMetrics.processMemoryBytes / (1024.0f * 1024.0f),
                     m_PerfMetrics.totalPhysicalMemory / (1024.0f * 1024.0f));
        if (!m_CurrentScenePath.empty()) ImGui::Text("Scene: %s", m_CurrentScenePath.c_str());
        ImGui::TreePop();
    }

    ImGui::Spacing();
    ImGui::Separator();

    // Submit / Save Draft buttons
    bool titleEmpty = (m_BugTitleBuf[0] == '\0');
    if (titleEmpty) {
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
        ImGui::Button("Submit Report");
        ImGui::PopStyleVar();
        ImGui::SameLine();
        ImGui::TextDisabled("(title required)");
    } else {
        if (ImGui::Button("Submit Report")) {
            u64 reportId = m_FeedbackManager.CreateBugReport();
            if (auto* report = m_FeedbackManager.GetBugReport(reportId)) {
                report->title = m_BugTitleBuf;
                report->type = static_cast<ReportType>(m_BugTypeSel);
                report->severity = static_cast<ReportSeverity>(m_BugSeveritySel);
                report->description = m_BugDescriptionBuf;
                report->stepsToReproduce = m_BugStepsBuf;
                report->expectedBehavior = m_BugExpectedBuf;
                report->actualBehavior = m_BugActualBuf;
                report->diagnostics = CaptureDiagnostics(m_BugIncludeScene);
                if (!m_BugIncludeLogs) report->diagnostics.consoleLogTail.clear();
                report->status = ReportStatus::Draft;
                m_FeedbackManager.SaveAll();
                m_ConsoleLog.push_back("[Feedback] Created bug report #" + std::to_string(reportId) + ": " + report->title);
            }
            ResetBugReportForm();
        }
    }
    ImGui::SameLine();
    if (!titleEmpty && ImGui::Button("Save Draft")) {
        u64 reportId = m_FeedbackManager.CreateBugReport();
        if (auto* report = m_FeedbackManager.GetBugReport(reportId)) {
            report->title = m_BugTitleBuf;
            report->type = static_cast<ReportType>(m_BugTypeSel);
            report->severity = static_cast<ReportSeverity>(m_BugSeveritySel);
            report->description = m_BugDescriptionBuf;
            report->stepsToReproduce = m_BugStepsBuf;
            report->expectedBehavior = m_BugExpectedBuf;
            report->actualBehavior = m_BugActualBuf;
            report->diagnostics = CaptureDiagnostics(m_BugIncludeScene);
            if (!m_BugIncludeLogs) report->diagnostics.consoleLogTail.clear();
            report->status = ReportStatus::Draft;
            m_FeedbackManager.SaveAll();
            m_ConsoleLog.push_back("[Feedback] Saved draft bug report #" + std::to_string(reportId));
        }
        ResetBugReportForm();
    }
}

void EditorLayer::DrawFeedbackList() {
    auto& entries = m_FeedbackManager.GetFeedbackEntries();
    if (entries.empty()) {
        DrawEmptyState("?", "No Feedback", "Feedback you submit will appear here.",
                        "Send Feedback", [this]() {
                            m_FeedbackTab = FeedbackTab::NewFeedback;
                        });
        return;
    }

    // Search
    ImGui::SetNextItemWidth(250);
    ImGui::InputTextWithHint("##FbSearch", "Search feedback...", m_FeedbackSearchBuf, sizeof(m_FeedbackSearchBuf));

    usize total = m_FeedbackManager.GetTotalFeedback();
    ImGui::SameLine();
    ImGui::TextDisabled("%zu entries", total);
    ImGui::Separator();

    auto searchResults = m_FeedbackManager.SearchFeedback(std::string(m_FeedbackSearchBuf));

    ImGui::BeginChild("FeedbackList", ImVec2(0, m_SelectedFeedbackId > 0 ? 200 : 0), true);
    for (auto* f : searchResults) {
        char label[256];
        snprintf(label, sizeof(label), "#%llu %s##fb_%llu",
                 static_cast<unsigned long long>(f->id), f->title.c_str(),
                 static_cast<unsigned long long>(f->id));
        bool selected = (m_SelectedFeedbackId == f->id);
        if (ImGui::Selectable(label, selected)) {
            m_SelectedFeedbackId = f->id;
        }
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 130);
        // Star rating display
        i32 stars = static_cast<i32>(f->satisfaction);
        char starBuf[16] = {};
        for (i32 s = 0; s < 5; ++s) starBuf[s] = (s < stars) ? '*' : '-';
        starBuf[5] = '\0';
        ImGui::TextDisabled("%s | %s | %s", FeedbackTypeLabel(f->type),
                            FeedbackPriorityLabel(f->priority), starBuf);
    }
    ImGui::EndChild();

    if (m_SelectedFeedbackId > 0) {
        FeedbackEntry* sel = m_FeedbackManager.GetFeedback(m_SelectedFeedbackId);
        if (sel) {
            ImGui::Separator();
            DrawFeedbackDetail(*sel);
        }
    }
}

void EditorLayer::DrawFeedbackDetail(FeedbackEntry& entry) {
    ImGui::BeginChild("FeedbackDetail", ImVec2(0, 0), false);

    ImGui::Text("Feedback #%llu: %s", static_cast<unsigned long long>(entry.id), entry.title.c_str());
    ImGui::Separator();

    ImGui::Text("Type: %s | Priority: %s", FeedbackTypeLabel(entry.type), FeedbackPriorityLabel(entry.priority));
    if (!entry.category.empty()) ImGui::Text("Category: %s", entry.category.c_str());

    // Satisfaction stars
    i32 stars = static_cast<i32>(entry.satisfaction);
    if (stars > 0) {
        ImGui::Text("Satisfaction: ");
        ImGui::SameLine();
        for (i32 s = 1; s <= 5; ++s) {
            if (s <= stars)
                ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.0f, 1.0f), "*");
            else
                ImGui::TextDisabled("*");
            if (s < 5) ImGui::SameLine(0, 2);
        }
    }

    ImGui::TextDisabled("Created: %s | Updated: %s", entry.createdAt.c_str(), entry.updatedAt.c_str());
    ImGui::Spacing();

    if (!entry.description.empty()) {
        ImGui::Text("Description:");
        ImGui::TextWrapped("%s", entry.description.c_str());
    }

    if (entry.includeDiagnostics && ImGui::TreeNode("Diagnostics")) {
        auto& d = entry.diagnostics;
        ImGui::Text("Engine: %s | Platform: %s", d.engineVersion.c_str(), d.platform.c_str());
        ImGui::Text("FPS: %.1f | Frame: %.2fms | Entities: %u", d.fps, d.frameTimeMs, d.entityCount);
        ImGui::TreePop();
    }

    ImGui::Spacing();
    ImGui::Separator();

    if (m_FeedbackEndpointBuf[0] != '\0' && ImGui::Button("Submit")) {
        if (m_FeedbackManager.SubmitFeedback(entry.id, std::string(m_FeedbackEndpointBuf))) {
            m_ConsoleLog.push_back("[Feedback] Feedback #" + std::to_string(entry.id) + " submitted");
        } else {
            m_ConsoleLog.push_back("[Feedback] Failed to submit feedback #" + std::to_string(entry.id));
        }
    }
    if (m_FeedbackEndpointBuf[0] != '\0') ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.2f, 0.2f, 1.0f));
    if (ImGui::Button("Delete")) {
        m_FeedbackManager.DeleteFeedback(entry.id);
        m_SelectedFeedbackId = 0;
        m_ConsoleLog.push_back("[Feedback] Deleted feedback #" + std::to_string(entry.id));
    }
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::SetNextItemWidth(200);
    ImGui::InputTextWithHint("##FbEndpoint", "Submit endpoint URL", m_FeedbackEndpointBuf, sizeof(m_FeedbackEndpointBuf));

    ImGui::EndChild();
}

void EditorLayer::DrawNewFeedbackForm() {
    ImGui::Text("Title *");
    ImGui::SetNextItemWidth(-1);
    ImGui::InputText("##FbTitle", m_FeedbackTitleBuf, sizeof(m_FeedbackTitleBuf));

    // Type and priority
    ImGui::Text("Type:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(140);
    const char* typeOpts[] = { "General", "Feature Request", "Usability", "Documentation", "Praise" };
    ImGui::Combo("##FbType", &m_FeedbackTypeSel, typeOpts, 5);
    ImGui::SameLine();
    ImGui::Text("Priority:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(100);
    const char* prioOpts[] = { "Low", "Medium", "High" };
    ImGui::Combo("##FbPriority", &m_FeedbackPrioritySel, prioOpts, 3);

    ImGui::Text("Category:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(200);
    ImGui::InputTextWithHint("##FbCategory", "e.g. Editor, Rendering...", m_FeedbackCategoryBuf, sizeof(m_FeedbackCategoryBuf));

    // Satisfaction rating (clickable stars)
    ImGui::Text("Satisfaction:");
    ImGui::SameLine();
    for (i32 s = 1; s <= 5; ++s) {
        ImGui::PushID(s);
        bool filled = (s <= m_FeedbackSatisfaction);
        if (filled)
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.85f, 0.0f, 1.0f));
        else
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));

        char starLabel[8];
        snprintf(starLabel, sizeof(starLabel), "*##s%d", s);
        if (ImGui::Selectable(starLabel, false, 0, ImVec2(16, 0))) {
            m_FeedbackSatisfaction = (m_FeedbackSatisfaction == s) ? 0 : s;
        }
        ImGui::PopStyleColor();
        ImGui::PopID();
        if (s < 5) ImGui::SameLine(0, 2);
    }
    const char* ratingLabels[] = { "", "Very Poor", "Poor", "Average", "Good", "Excellent" };
    if (m_FeedbackSatisfaction > 0 && m_FeedbackSatisfaction <= 5) {
        ImGui::SameLine();
        ImGui::TextDisabled("(%s)", ratingLabels[m_FeedbackSatisfaction]);
    }

    ImGui::Spacing();
    ImGui::Text("Description:");
    ImGui::InputTextMultiline("##FbDesc", m_FeedbackDescBuf, sizeof(m_FeedbackDescBuf),
                               ImVec2(-1, 120));

    ImGui::Checkbox("Include diagnostics", &m_FeedbackIncludeDiag);

    ImGui::Spacing();
    ImGui::Separator();

    bool titleEmpty = (m_FeedbackTitleBuf[0] == '\0');
    if (titleEmpty) {
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
        ImGui::Button("Submit Feedback");
        ImGui::PopStyleVar();
        ImGui::SameLine();
        ImGui::TextDisabled("(title required)");
    } else {
        if (ImGui::Button("Submit Feedback")) {
            u64 entryId = m_FeedbackManager.CreateFeedback();
            if (auto* entry = m_FeedbackManager.GetFeedback(entryId)) {
                entry->title = m_FeedbackTitleBuf;
                entry->type = static_cast<FeedbackType>(m_FeedbackTypeSel);
                entry->priority = static_cast<FeedbackPriority>(m_FeedbackPrioritySel);
                entry->satisfaction = static_cast<SatisfactionRating>(m_FeedbackSatisfaction);
                entry->description = m_FeedbackDescBuf;
                entry->category = m_FeedbackCategoryBuf;
                entry->includeDiagnostics = m_FeedbackIncludeDiag;
                if (m_FeedbackIncludeDiag) {
                    entry->diagnostics = CaptureDiagnostics(false);
                }
                m_FeedbackManager.SaveAll();
                m_ConsoleLog.push_back("[Feedback] Created feedback #" + std::to_string(entryId) + ": " + entry->title);
            }
            ResetFeedbackForm();
        }
    }
}

// ============================================================================
// QUICK BUG REPORT (F5)
// ============================================================================

void EditorLayer::QuickBugReport() {
    // Ensure feedback system is loaded
    if (!m_FeedbackLoaded) {
        m_FeedbackManager.LoadAll();
        m_FeedbackLoaded = true;
    }

    // Create a bug report with auto-captured diagnostics
    u64 id = m_FeedbackManager.CreateBugReport();
    auto* report = m_FeedbackManager.GetBugReport(id);
    if (!report) return;

    report->title = "Quick report at " + FeedbackManager::CurrentTimestamp();
    report->type = ReportType::Bug;
    report->severity = ReportSeverity::Medium;
    report->description = "Quick bug report submitted via F5.";
    report->diagnostics = CaptureDiagnostics(false);

    // If GitHub is configured, submit directly
    if (m_FeedbackManager.IsGitHubConfigured()) {
        if (m_FeedbackManager.SubmitBugReportToGitHub(id)) {
            m_ConsoleLog.push_back({
                "[Bug Report] Quick report #" + std::to_string(id) + " submitted to GitHub"});
            ENJIN_LOG_INFO(Editor, "Quick bug report #%llu submitted to GitHub", id);
        } else {
            m_ConsoleLog.push_back({
                "[Bug Report] Quick report #" + std::to_string(id) + " saved locally (GitHub submit failed)"});
        }
    } else {
        m_ConsoleLog.push_back({
            "[Bug Report] Quick report #" + std::to_string(id) + " saved locally (GitHub not configured)"});
    }

    m_FeedbackManager.SaveAll();

    // Open the feedback panel so the user can add more details
    SetPanelVisibility(EditorPanel::FeedbackPanel, true);
    m_FeedbackTab = FeedbackTab::BugReports;
    m_SelectedBugReportId = id;
}

// ============================================================================
// GITHUB ISSUES TAB
// ============================================================================

void EditorLayer::DrawGitHubIssuesTab() {
    if (!m_FeedbackManager.IsGitHubConfigured()) {
        DrawEmptyState("!", "GitHub Not Configured",
            "Add your GitHub personal access token in the Settings tab to view and submit issues.",
            "Go to Settings", [this]() { m_FeedbackTab = FeedbackTab::GitHubSettings; });
        return;
    }

    // Toolbar
    if (ImGui::Button("Refresh")) {
        m_FeedbackManager.FetchGitHubIssues(true);
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(200);
    ImGui::InputTextWithHint("##GHSearch", "Search issues...", m_GitHubIssueSearchBuf, sizeof(m_GitHubIssueSearchBuf));
    ImGui::SameLine();

    const auto& issues = m_FeedbackManager.GetGitHubIssues();
    ImGui::Text("%zu issues", issues.size());

    ImGui::Separator();

    // Issue list
    ImGui::BeginChild("GHIssueList", ImVec2(0, 0), true);

    std::string searchFilter = m_GitHubIssueSearchBuf;

    for (i32 i = 0; i < static_cast<i32>(issues.size()); ++i) {
        const auto& issue = issues[i];

        // Apply search filter
        if (!searchFilter.empty()) {
            if (!FeedbackManager::CaseInsensitiveContains(issue.title, searchFilter) &&
                !FeedbackManager::CaseInsensitiveContains(issue.body, searchFilter))
                continue;
        }

        ImGui::PushID(i);

        // State indicator
        if (issue.state == "open") {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 0.8f, 0.3f, 1.0f));
            ImGui::Text("[open]");
        } else {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.3f, 0.8f, 1.0f));
            ImGui::Text("[closed]");
        }
        ImGui::PopStyleColor();
        ImGui::SameLine();

        // Issue number + title (selectable)
        char label[512];
        snprintf(label, sizeof(label), "#%d %s", issue.number, issue.title.c_str());
        bool selected = (m_SelectedGitHubIssue == i);
        if (ImGui::Selectable(label, selected)) {
            m_SelectedGitHubIssue = (m_SelectedGitHubIssue == i) ? -1 : i;
        }

        // Labels on same line
        if (!issue.labels.empty()) {
            ImGui::Indent(24);
            for (const auto& lbl : issue.labels) {
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(0.5f, 0.7f, 1.0f, 1.0f), "[%s]", lbl.c_str());
            }
            ImGui::Unindent(24);
        }

        // Show detail when selected
        if (m_SelectedGitHubIssue == i) {
            ImGui::Indent(16);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.7f, 1.0f));
            ImGui::Text("Author: %s | Created: %s", issue.authorLogin.c_str(), issue.createdAt.c_str());
            ImGui::PopStyleColor();

            if (!issue.body.empty()) {
                ImGui::Spacing();
                ImGui::TextWrapped("%s", issue.body.c_str());
            }

            ImGui::Spacing();
            if (!issue.htmlUrl.empty()) {
                if (ImGui::SmallButton("Copy URL")) {
                    ImGui::SetClipboardText(issue.htmlUrl.c_str());
                }
            }
            ImGui::Unindent(16);
            ImGui::Separator();
        }

        ImGui::PopID();
    }

    ImGui::EndChild();
}

// ============================================================================
// GITHUB SETTINGS TAB
// ============================================================================

void EditorLayer::DrawGitHubSettingsTab() {
    auto& config = m_FeedbackManager.GetGitHubConfig();

    ImGui::TextWrapped("Configure GitHub integration to submit bug reports and crash reports as GitHub Issues, "
                       "and to view incoming issues in the GitHub Issues tab.");
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Sync buffers from config on first draw
    static bool initialized = false;
    if (!initialized) {
        strncpy(m_GitHubOwnerBuf, config.owner.c_str(), sizeof(m_GitHubOwnerBuf) - 1);
        strncpy(m_GitHubRepoBuf, config.repo.c_str(), sizeof(m_GitHubRepoBuf) - 1);
        strncpy(m_GitHubTokenBuf, config.token.c_str(), sizeof(m_GitHubTokenBuf) - 1);
        initialized = true;
    }

    ImGui::Text("Repository Owner");
    ImGui::SetNextItemWidth(-1);
    ImGui::InputText("##GHOwner", m_GitHubOwnerBuf, sizeof(m_GitHubOwnerBuf));

    ImGui::Spacing();
    ImGui::Text("Repository Name");
    ImGui::SetNextItemWidth(-1);
    ImGui::InputText("##GHRepo", m_GitHubRepoBuf, sizeof(m_GitHubRepoBuf));

    ImGui::Spacing();
    ImGui::Text("Personal Access Token");
    ImGui::SetNextItemWidth(-1);
    ImGui::InputText("##GHToken", m_GitHubTokenBuf, sizeof(m_GitHubTokenBuf),
                     ImGuiInputTextFlags_Password);
    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.7f, 1.0f),
        "Needs 'repo' scope. Settings > Developer settings > Personal access tokens on GitHub.");

    ImGui::Spacing();
    bool enabled = config.enabled;
    if (ImGui::Checkbox("Enable GitHub Integration", &enabled)) {
        config.enabled = enabled;
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (ImGui::Button("Save Settings")) {
        config.owner = m_GitHubOwnerBuf;
        config.repo = m_GitHubRepoBuf;
        config.token = m_GitHubTokenBuf;
        m_FeedbackManager.SaveAll();
        ENJIN_LOG_INFO(Editor, "GitHub settings saved: %s/%s", config.owner.c_str(), config.repo.c_str());
        m_ConsoleLog.push_back({"[Feedback] GitHub settings saved"});
    }

    ImGui::SameLine();
    if (ImGui::Button("Test Connection")) {
        // Temporarily apply buffer values
        config.owner = m_GitHubOwnerBuf;
        config.repo = m_GitHubRepoBuf;
        config.token = m_GitHubTokenBuf;

        if (m_FeedbackManager.FetchGitHubIssues(false)) {
            auto count = m_FeedbackManager.GetGitHubIssues().size();
            m_ConsoleLog.push_back({
                "[Feedback] GitHub connection OK! Fetched " + std::to_string(count) + " issues"});
            ENJIN_LOG_INFO(Editor, "GitHub test: fetched %zu issues", count);
        } else {
            m_ConsoleLog.push_back({"[Feedback] GitHub connection FAILED. Check token and repo."});
            ENJIN_LOG_ERROR(Editor, "GitHub connection test failed");
        }
    }

    // Status
    ImGui::Spacing();
    if (m_FeedbackManager.IsGitHubConfigured()) {
        ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.3f, 1.0f), "Status: Configured (%s/%s)",
            config.owner.c_str(), config.repo.c_str());
    } else {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.3f, 1.0f), "Status: Not configured (token required)");
    }
}

// ============================================================================
// AUDIO MIXER WINDOW
// ============================================================================

void EditorLayer::DrawAudioMixer() {
    ImGui::SetNextWindowSize(ImVec2(700, 560), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Audio Mixer", &m_ShowAudioMixer)) {
        ImGui::End();
        return;
    }

    if (!m_World) {
        DrawEmptyState("\xef\x80\xa8", "No World Loaded", "Load a scene to see audio sources.", nullptr, nullptr);
        ImGui::End();
        return;
    }

    Audio::SimpleAudio* audio = m_PlayMode.IsPlaying() ? m_PlayMode.GetSimpleAudio() : nullptr;
    // Get mixer reference (always available even when not playing)
    Audio::SimpleAudio* audioRef = m_PlayMode.GetSimpleAudio();
    if (!audioRef) {
        ImGui::TextDisabled("Audio system not initialized");
        ImGui::End();
        return;
    }
    const auto& mixer = audioRef->GetMixer();

    static const char* channelNames[] = {"SFX", "Music", "UI", "Voice"};
    static const ImU32 channelColorsU32[] = {
        Editor::Theme::BusSFX, Editor::Theme::BusMusic,
        Editor::Theme::BusUI, Editor::Theme::BusVoice
    };
    static const ImVec4 channelColors[] = {
        Editor::Theme::BusSFXV, Editor::Theme::BusMusicV,
        Editor::Theme::BusUIV, Editor::Theme::BusVoiceV
    };

    // ================================================================
    // Bus strips — horizontal mixer board layout
    // ================================================================
    ImGui::TextColored(Editor::Theme::HeadingV, "Bus Mixer");
    ImGui::Separator();

    f32 stripW = 130.0f;
    f32 meterH = 120.0f;

    // Master + 4 default buses side by side
    ImGui::BeginGroup();
    {
        // Master strip
        ImGui::BeginGroup();
        ImGui::Text("Master");
        f32 masterVol = audio ? audio->GetMasterVolume() : 1.0f;
        ImGui::VSliderFloat("##MasterVol", ImVec2(20, meterH), &masterVol, 0.0f, 1.0f, "");
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Master: %.0f%%", masterVol * 100.0f);
        if (audio) audio->SetMasterVolume(masterVol);

        // Master VU bar next to fader
        ImGui::SameLine();
        {
            ImVec2 pos = ImGui::GetCursorScreenPos();
            ImDrawList* dl = ImGui::GetWindowDrawList();
            f32 barW = 8.0f;
            dl->AddRectFilled(pos, ImVec2(pos.x + barW, pos.y + meterH), IM_COL32(30, 30, 30, 255));
            f32 level = 0.0f;
            for (const auto* bus : mixer.GetAllBuses()) level += bus->vuLevel;
            level = Math::Clamp(level * 0.25f, 0.0f, 1.0f);
            f32 barH = level * meterH;
            ImU32 meterCol = (level > 0.8f) ? IM_COL32(255, 60, 60, 255) : IM_COL32(100, 255, 120, 255);
            dl->AddRectFilled(ImVec2(pos.x, pos.y + meterH - barH), ImVec2(pos.x + barW, pos.y + meterH), meterCol);
            ImGui::Dummy(ImVec2(barW, meterH));
        }

        ImGui::Text("%.0f%%", masterVol * 100.0f);
        ImGui::EndGroup();
    }

    // Bus strips
    for (int ch = 0; ch < 4; ch++) {
        ImGui::SameLine(0, 12);
        ImGui::BeginGroup();

        ImGui::TextColored(channelColors[ch], "%s", channelNames[ch]);

        auto channel = static_cast<Audio::AudioChannel>(ch);
        f32 chVol = audio ? audio->GetChannelVolume(channel) : 1.0f;

        // Volume fader
        char faderId[32]; snprintf(faderId, sizeof(faderId), "##BusVol%d", ch);
        ImGui::VSliderFloat(faderId, ImVec2(20, meterH), &chVol, 0.0f, 1.0f, "");
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s: %.0f%%", channelNames[ch], chVol * 100.0f);
        if (audio) audio->SetChannelVolume(channel, chVol);

        // VU meter
        ImGui::SameLine();
        {
            ImVec2 pos = ImGui::GetCursorScreenPos();
            ImDrawList* dl = ImGui::GetWindowDrawList();
            f32 barW = 8.0f;
            dl->AddRectFilled(pos, ImVec2(pos.x + barW, pos.y + meterH), IM_COL32(30, 30, 30, 255));
            const Audio::AudioBus* bus = mixer.GetBus(channelNames[ch]);
            f32 level = bus ? Math::Clamp(bus->vuLevel, 0.0f, 1.0f) : 0.0f;
            f32 peak = bus ? Math::Clamp(bus->vuPeak, 0.0f, 1.0f) : 0.0f;
            f32 barH = level * meterH;
            dl->AddRectFilled(ImVec2(pos.x, pos.y + meterH - barH), ImVec2(pos.x + barW, pos.y + meterH), channelColorsU32[ch]);
            // Peak marker
            if (peak > 0.01f) {
                f32 peakY = pos.y + meterH - peak * meterH;
                dl->AddLine(ImVec2(pos.x, peakY), ImVec2(pos.x + barW, peakY), IM_COL32(255, 255, 255, 200), 1.5f);
            }
            ImGui::Dummy(ImVec2(barW, meterH));
        }

        // Sound count + Mute/Solo
        u32 soundCount = 0;
        const Audio::AudioBus* bus = mixer.GetBus(channelNames[ch]);
        if (bus) soundCount = bus->activeSoundCount;
        ImGui::Text("%u snd", soundCount);

        // Mute button
        bool muted = bus && bus->muted;
        if (muted) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
        char muteId[16]; snprintf(muteId, sizeof(muteId), "M##M%d", ch);
        if (ImGui::SmallButton(muteId)) {
            if (audio) {
                Audio::AudioBus* mutBus = audio->GetMixer().GetBus(channelNames[ch]);
                if (mutBus) mutBus->muted = !mutBus->muted;
            }
        }
        if (muted) ImGui::PopStyleColor();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Mute %s", channelNames[ch]);

        // Solo button
        ImGui::SameLine();
        bool soloed = bus && bus->solo;
        if (soloed) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.7f, 0.1f, 1.0f));
        char soloId[16]; snprintf(soloId, sizeof(soloId), "S##S%d", ch);
        if (ImGui::SmallButton(soloId)) {
            if (audio) {
                Audio::AudioBus* solBus = audio->GetMixer().GetBus(channelNames[ch]);
                if (solBus) solBus->solo = !solBus->solo;
            }
        }
        if (soloed) ImGui::PopStyleColor();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Solo %s", channelNames[ch]);

        // Mini EQ curve
        {
            ImVec2 eqPos = ImGui::GetCursorScreenPos();
            f32 eqW = 50.0f, eqH = 30.0f;
            ImDrawList* dl = ImGui::GetWindowDrawList();
            dl->AddRectFilled(eqPos, ImVec2(eqPos.x + eqW, eqPos.y + eqH), IM_COL32(20, 20, 25, 200), 2.0f);

            if (bus) {
                // Draw 3-band EQ curve approximation
                f32 midY = eqPos.y + eqH * 0.5f;
                ImVec2 prev(eqPos.x, midY - bus->eqLow.gain * 1.5f);
                for (int x = 1; x <= static_cast<int>(eqW); x++) {
                    f32 t = static_cast<f32>(x) / eqW;
                    f32 gain = 0.0f;
                    if (t < 0.3f) gain = bus->eqLow.gain * (1.0f - t / 0.3f);
                    else if (t < 0.7f) gain = bus->eqMid.gain * std::exp(-((t - 0.5f) * (t - 0.5f)) / (0.02f * bus->eqMid.q));
                    else gain = bus->eqHigh.gain * ((t - 0.7f) / 0.3f);
                    ImVec2 cur(eqPos.x + x, midY - gain * 1.5f);
                    dl->AddLine(prev, cur, channelColorsU32[ch], 1.5f);
                    prev = cur;
                }
                // Center line
                dl->AddLine(ImVec2(eqPos.x, midY), ImVec2(eqPos.x + eqW, midY), IM_COL32(80, 80, 80, 100));
            }
            ImGui::Dummy(ImVec2(eqW, eqH));
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("EQ: Low %.1fdB Mid %.1fdB High %.1fdB",
                bus ? bus->eqLow.gain : 0.0f, bus ? bus->eqMid.gain : 0.0f, bus ? bus->eqHigh.gain : 0.0f);
        }

        ImGui::EndGroup();
    }
    ImGui::EndGroup();

    ImGui::Separator();

    // ================================================================
    // Sound strips — individual playing sounds
    // ================================================================

    // Channel filter tabs
    if (ImGui::BeginTabBar("MixerTabs")) {
        struct TabDef { const char* label; i32 filter; };
        TabDef tabs[] = {{"All", -1}, {"SFX", 0}, {"Music", 1}, {"UI", 2}, {"Voice", 3}};

        for (auto& tab : tabs) {
            if (ImGui::BeginTabItem(tab.label)) {
                m_MixerChannelTab = tab.filter;
                ImGui::EndTabItem();
            }
        }
        ImGui::EndTabBar();
    }

    // --- Audio source list ---
    auto entities = m_World->GetEntitiesWithComponent<ECS::AudioSourceComponent>();
    i32 displayCount = 0;

    if (ImGui::BeginChild("MixerSources", ImVec2(0, 0), ImGuiChildFlags_None)) {
        for (ECS::Entity entity : entities) {
            auto* asc = m_World->GetComponent<ECS::AudioSourceComponent>(entity);
            if (!asc) continue;

            i32 chIdx = static_cast<i32>(asc->channel);

            // Filter by selected tab
            if (m_MixerChannelTab >= 0 && chIdx != m_MixerChannelTab) continue;

            // Get entity name
            auto* nameComp = m_World->GetComponent<ECS::NameComponent>(entity);
            const char* entityName = nameComp ? nameComp->name.c_str() : "(unnamed)";

            // Get clip filename
            std::string clipName;
            if (!asc->clipPath.empty()) {
                auto lastSlash = asc->clipPath.find_last_of("/\\");
                clipName = (lastSlash != std::string::npos)
                    ? asc->clipPath.substr(lastSlash + 1) : asc->clipPath;
            } else {
                clipName = "(no clip)";
            }

            const char* chName = (chIdx >= 0 && chIdx < 4) ? channelNames[chIdx] : "?";
            ImVec4 chColor = (chIdx >= 0 && chIdx < 4) ? channelColors[chIdx] : ImVec4(0.5f, 0.5f, 0.5f, 1.0f);

            ImGui::PushID(static_cast<int>(entity));

            // Channel badge + entity name
            ImGui::TextColored(chColor, "[%s]", chName);
            ImGui::SameLine();
            bool isSelected = IsSelected(entity);
            if (ImGui::Selectable(entityName, isSelected, ImGuiSelectableFlags_None, ImVec2(0, 0))) {
                ClearSelection();
                SelectEntity(entity);
            }

            // Clip name (dimmed)
            ImGui::SameLine(ImGui::GetContentRegionAvail().x - 120);
            ImGui::TextDisabled("%s", clipName.c_str());

            // Controls row
            ImGui::Indent(30);

            // Volume slider
            ImGui::SetNextItemWidth(150);
            char volId[32];
            snprintf(volId, sizeof(volId), "##Vol%llu", (unsigned long long)entity);
            if (ImGui::SliderFloat(volId, &asc->volume, 0.0f, 1.0f, "Vol %.2f")) {
                if (audio && asc->soundHandle != 0) {
                    audio->SetVolume(asc->soundHandle, asc->volume);
                }
            }

            ImGui::SameLine();

            // Pitch slider
            ImGui::SetNextItemWidth(100);
            char pitchId[32];
            snprintf(pitchId, sizeof(pitchId), "##Pitch%llu", (unsigned long long)entity);
            if (ImGui::SliderFloat(pitchId, &asc->pitch, 0.1f, 3.0f, "x%.1f")) {
                if (audio && asc->soundHandle != 0) {
                    audio->SetPitch(asc->soundHandle, asc->pitch);
                }
            }

            ImGui::SameLine();

            // Loop indicator
            if (asc->loop) {
                ImGui::TextDisabled("LOOP");
                ImGui::SameLine();
            }

            // 3D indicator
            bool channelForces2D = asc->channel == ECS::AudioChannel::Music || asc->channel == ECS::AudioChannel::UI;
            if (asc->is3D && !channelForces2D) {
                ImGui::TextDisabled("3D");
                ImGui::SameLine();
            }

            // Playing indicator
            bool isPlaying = asc->isPlaying;
            if (isPlaying && audio && asc->soundHandle != 0) {
                isPlaying = audio->IsPlaying(asc->soundHandle);
            }
            if (isPlaying) {
                ImGui::TextColored(ImVec4(0.2f, 0.9f, 0.3f, 1.0f), "PLAYING");
            } else {
                ImGui::TextDisabled("stopped");
            }

            ImGui::Unindent(30);
            ImGui::Separator();

            displayCount++;
            ImGui::PopID();
        }

        if (displayCount == 0) {
            const char* filterName = m_MixerChannelTab >= 0 && m_MixerChannelTab < 4
                ? channelNames[m_MixerChannelTab] : "any";
            DrawEmptyState("\xef\x80\xa8", "No Audio Sources",
                m_MixerChannelTab >= 0
                    ? (std::string("No ") + filterName + " audio sources in scene.").c_str()
                    : "Add AudioSourceComponent to entities to see them here.",
                nullptr, nullptr);
        }
    }
    ImGui::EndChild();

    ImGui::End();
}

// (Removed duplicate DrawGameDebugPanel — canonical version is after DrawDebugWorkstation)
#if 0

    if (ImGui::BeginTabBar("GameDebugTabs")) {

        // === Scene Overview ===
        if (ImGui::BeginTabItem("Scene")) {
            if (m_World) {
                ImGui::Text("Entity Count: %zu", m_World->GetEntityCount());
                ImGui::Separator();

                // Count entities with key game components
                u32 meshCount = 0, lightCount = 0, scriptCount = 0, audioCount = 0;
                u32 cameraCount = 0, animatorCount = 0, body2dCount = 0;
                u32 visibleCount = 0, hiddenCount = 0;

                for (auto e : m_World->GetAllEntities()) {
                    if (!m_World->IsValid(e)) continue;
                    if (m_World->HasComponent<ECS::MeshComponent>(e)) meshCount++;
                    if (m_World->HasComponent<ECS::LightComponent>(e)) lightCount++;
                    if (m_World->HasComponent<ECS::ScriptComponent>(e)) scriptCount++;
                    if (m_World->HasComponent<ECS::AudioSourceComponent>(e)) audioCount++;
                    if (m_World->HasComponent<ECS::CameraComponent>(e)) cameraCount++;
                    if (m_World->HasComponent<ECS::AnimatorComponent>(e)) animatorCount++;
                    if (m_World->HasComponent<Physics::Body2DComponent>(e)) body2dCount++;
                    if (auto* t = m_World->GetComponent<ECS::TransformComponent>(e)) {
                        if (t->visible) visibleCount++; else hiddenCount++;
                    }
                }

                ImGui::Columns(2, "SceneStats", false);
                ImGui::Text("Meshes: %u", meshCount);
                ImGui::Text("Lights: %u", lightCount);
                ImGui::Text("Cameras: %u", cameraCount);
                ImGui::Text("Animators: %u", animatorCount);
                ImGui::NextColumn();
                ImGui::Text("Scripts: %u", scriptCount);
                ImGui::Text("Audio Sources: %u", audioCount);
                ImGui::Text("Physics Bodies: %u", body2dCount);
                ImGui::Text("Visible/Hidden: %u / %u", visibleCount, hiddenCount);
                ImGui::Columns(1);

                ImGui::Separator();
                ImGui::TextColored(ImVec4(0.7f, 0.9f, 1.0f, 1.0f), "Scene: %s",
                    m_SceneManager.GetCurrentSceneName().empty() ? "(unsaved)" : m_SceneManager.GetCurrentSceneName().c_str());
            } else {
                ImGui::TextDisabled("No world loaded");
            }
            ImGui::EndTabItem();
        }

        // === Physics ===
        if (ImGui::BeginTabItem("Physics")) {
            if (m_World) {
                u32 bodyCount = 0, sensorCount = 0, kinematicCount = 0;
                // Count 2D physics bodies
                for (auto e : m_World->GetAllEntities()) {
                    if (!m_World->IsValid(e)) continue;
                    if (auto* b = m_World->GetComponent<Physics::Body2DComponent>(e)) {
                        bodyCount++;
                        if (b->isSensor) sensorCount++;
                        if (b->isKinematic) kinematicCount++;
                    }
                }

                // Count 3D rigidbodies
                u32 rigidCount = 0;
                for (auto e : m_World->GetAllEntities()) {
                    if (!m_World->IsValid(e)) continue;
                    if (m_World->HasComponent<ECS::RigidbodyComponent>(e)) rigidCount++;
                }

                ImGui::Text("2D Bodies: %u", bodyCount);
                ImGui::Text("  Sensors: %u", sensorCount);
                ImGui::Text("  Kinematic: %u", kinematicCount);
                ImGui::Text("  Dynamic: %u", bodyCount - sensorCount - kinematicCount);
                if (rigidCount > 0)
                    ImGui::Text("3D Rigidbodies: %u", rigidCount);
                ImGui::Separator();

                // Collider visualization toggle
                bool showColliders = m_ShowColliderWireframes;
                if (ImGui::Checkbox("Show Collider Wireframes", &showColliders)) {
                    m_ShowColliderWireframes = showColliders;
                }

                // List bodies with positions
                ImGui::Separator();
                ImGui::Text("Body Details:");
                ImGui::BeginChild("PhysicsBodies", ImVec2(0, 0), true);
                for (auto e : m_World->GetAllEntities()) {
                    if (!m_World->IsValid(e)) continue;
                    if (!m_World->HasComponent<Physics::Body2DComponent>(e)) continue;
                    auto* b = m_World->GetComponent<Physics::Body2DComponent>(e);
                    auto* t = m_World->GetComponent<ECS::TransformComponent>(e);
                    std::string name = "Entity " + std::to_string(e);
                    if (auto* nc = m_World->GetComponent<ECS::NameComponent>(e)) name = nc->name;

                    const char* typeStr = b->isStatic ? "Static" :
                                          b->isKinematic ? "Kinematic" : "Dynamic";
                    ImGui::BulletText("[%llu] %s — %s%s", static_cast<unsigned long long>(e), name.c_str(),
                        typeStr, b->isSensor ? " (Sensor)" : "");
                    if (t) {
                        ImGui::SameLine();
                        ImGui::TextDisabled("(%.1f, %.1f)", t->position.x, t->position.y);
                    }
                }
                ImGui::EndChild();
            } else {
                ImGui::TextDisabled("No world loaded");
            }
            ImGui::EndTabItem();
        }

        // === Scripts ===
        if (ImGui::BeginTabItem("Scripts")) {
            if (m_World) {
                u32 scriptCount = 0;
                ImGui::BeginChild("ScriptList", ImVec2(0, 0), true);
                for (auto e : m_World->GetAllEntities()) {
                    if (!m_World->IsValid(e)) continue;
                    if (auto* sc = m_World->GetComponent<ECS::ScriptComponent>(e)) {
                        scriptCount++;
                        std::string name = "Entity " + std::to_string(e);
                        if (auto* nc = m_World->GetComponent<ECS::NameComponent>(e)) name = nc->name;

                        bool open = ImGui::TreeNode(reinterpret_cast<void*>(static_cast<uintptr_t>(e)),
                            "[%llu] %s (%zu scripts)", static_cast<unsigned long long>(e), name.c_str(), sc->scripts.size());
                        if (open) {
                            for (const auto& att : sc->scripts) {
                                ImVec4 col = att.hasError ? ImVec4(1, 0.3f, 0.3f, 1) :
                                             att.enabled ? ImVec4(0.8f, 0.8f, 0.8f, 1) : ImVec4(0.5f, 0.5f, 0.5f, 1);
                                ImGui::TextColored(col, "  %s — %s%s",
                                    att.className.c_str(),
                                    att.scriptPath.empty() ? "(inline)" : att.scriptPath.c_str(),
                                    att.hasError ? " [ERROR]" : "");
                                if (att.hasError && !att.lastError.empty()) {
                                    ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "    %s", att.lastError.c_str());
                                }
                            }
                            ImGui::TreePop();
                        }
                    }
                }
                if (scriptCount == 0) {
                    ImGui::TextDisabled("No scripted entities in scene");
                }
                ImGui::EndChild();
            } else {
                ImGui::TextDisabled("No world loaded");
            }
            ImGui::EndTabItem();
        }

        // === Audio ===
        if (ImGui::BeginTabItem("Audio")) {
            if (m_World) {
                u32 sourceCount = 0, playingCount = 0;
                ImGui::BeginChild("AudioList", ImVec2(0, 0), true);
                for (auto e : m_World->GetAllEntities()) {
                    if (!m_World->IsValid(e)) continue;
                    if (auto* asc = m_World->GetComponent<ECS::AudioSourceComponent>(e)) {
                        sourceCount++;
                        if (asc->isPlaying) playingCount++;

                        std::string name = "Entity " + std::to_string(e);
                        if (auto* nc = m_World->GetComponent<ECS::NameComponent>(e)) name = nc->name;

                        ImGui::PushID(static_cast<int>(e));
                        if (asc->isPlaying) {
                            ImGui::TextColored(ImVec4(0.2f, 0.9f, 0.3f, 1.0f), ">> ");
                        } else {
                            ImGui::TextDisabled("   ");
                        }
                        ImGui::SameLine();
                        ImGui::Text("[%llu] %s — %s", static_cast<unsigned long long>(e), name.c_str(),
                            asc->clipPath.empty() ? "(no file)" : asc->clipPath.c_str());
                        ImGui::SameLine();
                        ImGui::TextDisabled("vol:%.0f%%", asc->volume * 100.0f);
                        ImGui::PopID();
                    }
                }
                if (sourceCount == 0) {
                    ImGui::TextDisabled("No audio sources in scene");
                } else {
                    ImGui::Separator();
                    ImGui::Text("Total: %u sources, %u playing", sourceCount, playingCount);
                }
                ImGui::EndChild();
            } else {
                ImGui::TextDisabled("No world loaded");
            }
            ImGui::EndTabItem();
        }

        // === Gameplay ===
        if (ImGui::BeginTabItem("Gameplay")) {
            // Play mode status
            ImGui::TextColored(ImVec4(0.7f, 0.9f, 1.0f, 1.0f), "Play Mode:");
            ImGui::SameLine();
            if (m_PlayMode.IsPlaying()) {
                ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "PLAYING");
            } else if (m_PlayMode.IsPaused()) {
                ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.2f, 1.0f), "PAUSED");
            } else {
                ImGui::TextDisabled("STOPPED");
            }

            ImGui::Separator();

            // Quick play controls
            if (m_PlayMode.IsStopped()) {
                if (ImGui::Button("Play")) { StartPlayMode(); }
            } else {
                if (m_PlayMode.IsPlaying()) {
                    if (ImGui::Button("Pause")) { m_PlayMode.Pause(); }
                } else {
                    if (ImGui::Button("Resume")) { m_PlayMode.Play(); }
                }
                ImGui::SameLine();
                if (ImGui::Button("Stop")) { m_PlayMode.Stop(); }
            }

            ImGui::Separator();
            ImGui::TextColored(ImVec4(0.7f, 0.9f, 1.0f, 1.0f), "Scene Info:");
            ImGui::Text("Scene: %s",
                m_SceneManager.GetCurrentSceneName().empty() ? "(unsaved)" : m_SceneManager.GetCurrentSceneName().c_str());
            if (m_World) {
                ImGui::Text("Entities: %zu", m_World->GetEntityCount());
            }

            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::End();
}
#endif // DUPLICATE

// ---------------------------------------------------------------------------
// Audio Meter Strip — thin VU bar above Scene/Game view showing per-bus levels
// ---------------------------------------------------------------------------
void EditorLayer::DrawAudioMeterStrip() {
    Audio::SimpleAudio* audio = m_PlayMode.IsPlaying() ? m_PlayMode.GetSimpleAudio() : nullptr;
    if (!audio) return;

    const auto& mixer = audio->GetMixer();
    const auto& buses = mixer.GetAllBuses();
    if (buses.empty()) return;

    // Colors per bus type (from EditorTheme)
    static const ImU32 busColors[] = {
        Editor::Theme::TextWhite,    // Master
        Editor::Theme::BusSFX,       // SFX
        Editor::Theme::BusMusic,     // Music
        Editor::Theme::BusUI,        // UI
        Editor::Theme::BusVoice,     // Voice
    };

    f32 barHeight = 6.0f;
    f32 totalHeight = barHeight * 4 + 3.0f;  // 4 buses + gaps (skip Master)
    ImVec2 pos = ImGui::GetCursorScreenPos();
    f32 width = ImGui::GetContentRegionAvail().x;
    ImDrawList* dl = ImGui::GetWindowDrawList();

    // Background
    dl->AddRectFilled(pos, ImVec2(pos.x + width, pos.y + totalHeight), IM_COL32(20, 20, 25, 200), 2.0f);

    // Draw a bar per bus (skip Master, show SFX/Music/UI/Voice)
    f32 y = pos.y;
    for (usize i = 1; i < buses.size() && i <= 4; ++i) {
        const auto* bus = buses[i];
        f32 level = bus->vuLevel * bus->GetEffectiveVolume();
        f32 peak = bus->vuPeak * bus->GetEffectiveVolume();
        f32 barW = Math::Clamp(level, 0.0f, 1.0f) * width;
        f32 peakX = Math::Clamp(peak, 0.0f, 1.0f) * width;

        ImU32 color = (i < 5) ? busColors[i] : IM_COL32(160, 160, 160, 255);

        // Level bar
        if (barW > 0.0f) {
            dl->AddRectFilled(ImVec2(pos.x, y), ImVec2(pos.x + barW, y + barHeight), color);
        }

        // Peak hold marker (thin line)
        if (peakX > 1.0f) {
            dl->AddLine(ImVec2(pos.x + peakX, y), ImVec2(pos.x + peakX, y + barHeight),
                IM_COL32(255, 255, 255, 180), 1.5f);
        }

        // Bus name label (small, left-aligned)
        dl->AddText(ImVec2(pos.x + 2, y - 1), IM_COL32(200, 200, 200, 140), bus->name.c_str());

        // Active sound count (right-aligned, only if > 0)
        if (bus->activeSoundCount > 0) {
            char countBuf[16];
            snprintf(countBuf, sizeof(countBuf), "%u", bus->activeSoundCount);
            ImVec2 textSize = ImGui::CalcTextSize(countBuf);
            dl->AddText(ImVec2(pos.x + width - textSize.x - 4, y - 1), color, countBuf);
        }

        y += barHeight + 1.0f;
    }

    // Tooltip on hover
    ImVec2 mousePos = ImGui::GetMousePos();
    if (mousePos.x >= pos.x && mousePos.x <= pos.x + width &&
        mousePos.y >= pos.y && mousePos.y <= pos.y + totalHeight) {
        ImGui::BeginTooltip();
        for (usize i = 1; i < buses.size() && i <= 4; ++i) {
            const auto* bus = buses[i];
            ImGui::Text("%s: %.0f%% vol, %u sounds", bus->name.c_str(),
                bus->GetEffectiveVolume() * 100.0f, bus->activeSoundCount);
        }
        ImGui::EndTooltip();
    }

    ImGui::Dummy(ImVec2(width, totalHeight));
}

// ---------------------------------------------------------------------------
// Debug Workstation (F2) — editor/engine debug metrics & tools window
// ---------------------------------------------------------------------------
void EditorLayer::DrawDebugWorkstation() {
    ImGui::SetNextWindowSize(ImVec2(700, 500), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Debug Workstation", &m_ShowDebugWorkstation)) {
        ImGui::End();
        return;
    }

    if (ImGui::BeginTabBar("DebugTabs")) {

        // =====================================================================
        // Tab 1 — Performance
        // =====================================================================
        if (ImGui::BeginTabItem("Performance")) {
            ImGuiIO& io = ImGui::GetIO();
            f32 fps = io.Framerate;
            f32 frameMs = fps > 0.0f ? 1000.0f / fps : 0.0f;

            // Color FPS
            ImVec4 fpsColor = fps >= 60.0f ? ImVec4(0.2f, 1.0f, 0.2f, 1.0f) :
                              fps >= 30.0f ? ImVec4(1.0f, 1.0f, 0.2f, 1.0f) :
                                             ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
            ImGui::TextColored(fpsColor, "FPS: %.1f", fps);
            ImGui::SameLine(200);
            ImGui::Text("Frame Time: %.2f ms", frameMs);

            ImGui::Separator();

            // Frame time stats from the existing ring buffer
            ImGui::Text("Min: %.2f ms  Max: %.2f ms  Avg: %.2f ms", m_FrameTimeMin, m_FrameTimeMax, m_FrameTimeAvg);
            ImGui::Text("P50: %.2f ms  P95: %.2f ms  P99: %.2f ms", m_FrameTimeP50, m_FrameTimeP95, m_FrameTimeP99);

            // FPS graph using the existing frame time ring buffer
            ImGui::Separator();
            ImGui::Text("Frame Time History:");
            f32 graphMax = m_FrameTimeMax > 0.0f ? m_FrameTimeMax * 1.5f : 33.3f;
            if (graphMax < 16.7f) graphMax = 33.3f;

            auto getter = [](void* data, int idx) -> float {
                EditorLayer* self = static_cast<EditorLayer*>(data);
                usize actualIdx = (self->m_FrameTimeIndex + static_cast<usize>(idx)) % FRAME_TIME_HISTORY_SIZE;
                return self->m_FrameTimeHistory[actualIdx];
            };
            char overlay[64];
            snprintf(overlay, sizeof(overlay), "%.1f ms", frameMs);
            ImGui::PlotLines("##DbgFrameTime", getter, this,
                             static_cast<int>(FRAME_TIME_HISTORY_SIZE),
                             0, overlay, 0.0f, graphMax, ImVec2(ImGui::GetContentRegionAvail().x, 80));

            ImGui::Separator();
            ImGui::TextColored(ImVec4(0.7f, 0.9f, 1.0f, 1.0f), "-- Render Stats --");
            if (m_RenderSystem) {
                ImGui::Text("Draw Calls: %u", m_RenderSystem->GetDrawCallCount());
                u32 tris = m_RenderSystem->GetTriangleCount();
                if (tris > 1000000)
                    ImGui::Text("Triangles: %.2f M", static_cast<f32>(tris) / 1000000.0f);
                else if (tris > 1000)
                    ImGui::Text("Triangles: %.1f K", static_cast<f32>(tris) / 1000.0f);
                else
                    ImGui::Text("Triangles: %u", tris);

                u32 totalDesc = m_RenderSystem->GetDescriptorCacheHits() + m_RenderSystem->GetDescriptorCacheWrites();
                if (totalDesc > 0) {
                    f32 hitRate = static_cast<f32>(m_RenderSystem->GetDescriptorCacheHits()) / static_cast<f32>(totalDesc) * 100.0f;
                    ImGui::Text("Descriptor Cache Hit Rate: %.0f%%", hitRate);
                }
            } else {
                ImGui::TextDisabled("RenderSystem not available");
            }

            if (m_World) {
                ImGui::Text("Entities: %zu", m_World->GetEntityCount());
            }

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
                ImGui::Text("GPU VRAM: %.1f / %.0f MB", gpuAllocMB, gpuTotalMB);
            }

            // GPU per-pass timing from timestamp queries
            ImGui::Separator();
            ImGui::TextColored(ImVec4(0.7f, 0.9f, 1.0f, 1.0f), "-- GPU Pass Times --");
            if (m_Renderer) {
                const f32* gpu = m_Renderer->GetGPUPassTimes();
                ImGui::Text("Shadow:       %.2f ms", gpu[0]);
                ImGui::Text("Main Geo:     %.2f ms", gpu[1]);
                ImGui::Text("Ray Tracing:  %.2f ms", gpu[2]);
                ImGui::Text("Post-Process: %.2f ms", gpu[3]);
                f32 total = gpu[0] + gpu[1] + gpu[2] + gpu[3];
                ImVec4 gpuColor = total < 8.0f ? ImVec4(0.2f, 1.0f, 0.2f, 1.0f) : ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
                ImGui::TextColored(gpuColor, "GPU Total:    %.2f ms", total);
            }

            ImGui::Separator();
            ImGui::TextColored(ImVec4(0.7f, 0.9f, 1.0f, 1.0f), "-- CPU Frame Breakdown --");
            ImGui::Text("Update:       %.2f ms", m_CPUUpdateMs);
            ImGui::Text("Shadow:       %.2f ms", m_CPUShadowMs);
            ImGui::Text("Render:       %.2f ms", m_CPURenderMs);
            ImGui::Text("ImGui:        %.2f ms", m_CPUImGuiMs);
            ImGui::Text("Present:      %.2f ms", m_CPUPresentMs);
            f32 cpuTotal = m_CPUUpdateMs + m_CPUShadowMs + m_CPURenderMs + m_CPUImGuiMs + m_CPUPresentMs;
            ImVec4 cpuColor = cpuTotal < 8.0f ? ImVec4(0.2f, 1.0f, 0.2f, 1.0f) : ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
            ImGui::TextColored(cpuColor, "CPU Total:    %.2f ms", cpuTotal);

            ImGui::Separator();
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "-- Pipeline Stalls --");
            if (m_Renderer) {
                f32 fenceMs = m_Renderer->GetFenceWaitMs();
                f32 acquireMs = m_Renderer->GetAcquireMs();
                ImVec4 fenceColor = fenceMs < 1.0f ? ImVec4(0.2f, 1.0f, 0.2f, 1.0f) : ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
                ImGui::TextColored(fenceColor, "Fence Wait:   %.2f ms", fenceMs);
                ImGui::Text("Acquire Image:%.2f ms", acquireMs);
                ImGui::TextDisabled("(Fence > 1ms = GPU can't keep up with CPU)");
            }

            ImGui::EndTabItem();
        }

        // =====================================================================
        // Tab 2 — Renderer
        // =====================================================================
        if (ImGui::BeginTabItem("Renderer")) {
            if (m_RenderSystem) {
                // Scene classification
                const auto& comp = m_RenderSystem->GetSceneComposition();
                const char* modeStr = "Scene3D";
                if (comp.mode == ECS::SceneRenderMode::Scene2D) modeStr = "Scene2D";
                else if (comp.mode == ECS::SceneRenderMode::Scene2_5D) modeStr = "Scene2.5D";
                ImGui::Text("Scene Mode: %s", modeStr);
                ImGui::Text("  Sprites: %u  Tilemaps: %u  3D Meshes: %u",
                            comp.spriteCount, comp.tilemapCount, comp.mesh3DCount);
                ImGui::Text("  Shadow-casting Lights: %s", comp.hasShadowCastingLights ? "Yes" : "No");

                ImGui::Separator();

                // Shadows
                ImGui::TextColored(ImVec4(0.7f, 0.9f, 1.0f, 1.0f), "-- Shadows --");
                ImGui::Text("Shadows: %s", m_RenderSystem->IsShadowsEnabled() ? "Enabled" : "Disabled");
                ImGui::Text("Shadow Distance: %.1f", m_RenderSystem->GetShadowDistance());
                ImGui::Text("Shadow Resolution: %u", m_RenderSystem->GetShadowResolution());
                ImGui::Text("Progressive Cascades: %s", m_RenderSystem->IsCascadeProgressiveUpdate() ? "On" : "Off");

                ImGui::Separator();

#if !ENJIN_RENDERER_WEBGPU
                // Ray Tracing
                ImGui::TextColored(ImVec4(0.7f, 0.9f, 1.0f, 1.0f), "-- Ray Tracing --");
                ImGui::Text("RT Supported: %s", m_RenderSystem->IsRayTracingSupported() ? "Yes" : "No");
                ImGui::Text("RT Enabled: %s", m_RenderSystem->IsRayTracingEnabled() ? "Yes" : "No");
                if (m_RenderSystem->IsRayTracingEnabled()) {
                    u32 rtMode = m_RenderSystem->GetRTMode();
                    const char* rtModeNames[] = {"Hybrid", "Path Trace"};
                    ImGui::Text("RT Mode: %s", rtMode < 2 ? rtModeNames[rtMode] : "Unknown");
                    u32 denoiser = m_RenderSystem->GetDenoiserType();
                    const char* denoiserNames[] = {"SVGF", "OIDN", "OptiX"};
                    ImGui::Text("Denoiser: %s", denoiser < 3 ? denoiserNames[denoiser] : "Unknown");
                    auto* restir = m_RenderSystem->GetReSTIR();
                    ImGui::Text("ReSTIR: %s", (restir && restir->GetConfig().enabled) ? "Enabled" : "Disabled");
                    auto* radianceCache = m_RenderSystem->GetRadianceCache();
                    ImGui::Text("Radiance Cache: %s", (radianceCache && radianceCache->GetConfig().enabled) ? "Enabled" : "Disabled");
                    if (radianceCache && radianceCache->GetConfig().enabled) {
                        ImGui::Text("  Tiles: %ux%u (%u total, %upx)",
                            radianceCache->GetTileCountX(), radianceCache->GetTileCountY(),
                            radianceCache->GetTotalTileCount(), radianceCache->GetConfig().tileSize);
                    }
                    auto* surfelCache = m_RenderSystem->GetSurfelRadianceCache();
                    ImGui::Text("Surfel Cache: %s", (surfelCache && surfelCache->GetConfig().enabled) ? "Enabled" : "Disabled");
                    if (surfelCache && surfelCache->GetConfig().enabled) {
                        ImGui::Text("  Surfels: %u / %u (%.1fm radius)",
                            surfelCache->GetActiveSurfelCount(),
                            surfelCache->GetMaxSurfels(),
                            surfelCache->GetConfig().cameraRadius);
                    }
                }

                ImGui::Text("OIT: %s", m_RenderSystem->IsOITEnabled() ? "Enabled" : "Disabled");

                ImGui::Separator();
#endif

                // Anti-aliasing / upscaling
                ImGui::TextColored(ImVec4(0.7f, 0.9f, 1.0f, 1.0f), "-- Anti-Aliasing --");
                u32 aaMode = m_RenderSystem->GetAAMode();
                const char* aaNames[] = {"None", "FXAA", "TAA", "SMAA", "MSAA 2x", "MSAA 4x", "MSAA 8x"};
                ImGui::Text("AA Mode: %s", aaMode < 7 ? aaNames[aaMode] : "Unknown");
                if (m_RenderSystem->IsUpscalerActive()) {
                    u32 upType = m_RenderSystem->GetUpscalerType();
                    const char* upNames[] = {"None", "FSR 2", "DLSS", "XeSS"};
                    ImGui::Text("Upscaler: %s (sharpness %.2f)", upType < 4 ? upNames[upType] : "Unknown",
                                m_RenderSystem->GetUpscalerSharpness());
                }

                ImGui::Separator();

                // Shading
                ImGui::TextColored(ImVec4(0.7f, 0.9f, 1.0f, 1.0f), "-- Shading --");
                ImGui::Text("Wireframe: %s", m_RenderSystem->IsWireframeEnabled() ? "On" : "Off");
                ImGui::Text("Backface Culling: %s", m_RenderSystem->IsBackfaceCullingEnabled() ? "On" : "Off");
                ImGui::Text("Cel Shading: %s", m_RenderSystem->IsCelShadingEnabled() ? "On" : "Off");
                ImGui::Text("Fog Density: %.3f", m_RenderSystem->GetFogDensity());

                ImGui::Separator();

                // Render targets
                ImGui::TextColored(ImVec4(0.7f, 0.9f, 1.0f, 1.0f), "-- Render Targets --");
                ImGui::Text("Editor Viewport: %u x %u", m_EditorViewportWidth, m_EditorViewportHeight);
                ImGui::Text("Game View: %u x %u", m_GameViewWidth, m_GameViewHeight);
            } else {
                ImGui::TextDisabled("RenderSystem not available");
            }

            // Post-processing
            if (m_PostProcessing) {
                ImGui::Separator();
                ImGui::TextColored(ImVec4(0.7f, 0.9f, 1.0f, 1.0f), "-- Post-Processing --");
                const auto& ppSettings = m_PostProcessing->GetSettings();
                const char* tmNames[] = {"None", "Reinhard", "Reinhard Extended", "ACES", "Uncharted 2", "AgX"};
                u32 tmMode = ppSettings.toneMappingMode;
                ImGui::Text("Tone Mapping: %s", tmMode < 6 ? tmNames[tmMode] : "Unknown");
                ImGui::Text("Exposure: %.2f  Gamma: %.2f", ppSettings.exposure, ppSettings.gamma);
                ImGui::Text("Bloom: %s", ppSettings.bloomEnabled ? "On" : "Off");
                ImGui::Text("Vignette: %s", ppSettings.vignetteEnabled ? "On" : "Off");
                ImGui::Text("Chromatic Aberration: %s", ppSettings.chromaticAberrationEnabled ? "On" : "Off");
            }

            ImGui::EndTabItem();
        }

        // =====================================================================
        // Tab 3 — ECS
        // =====================================================================
        if (ImGui::BeginTabItem("ECS")) {
            if (m_World) {
                usize entityCount = m_World->GetEntityCount();
                ImGui::Text("Total Entities: %zu", entityCount);

                ImGui::Separator();
                ImGui::TextColored(ImVec4(0.7f, 0.9f, 1.0f, 1.0f), "-- Component Counts --");

                // Query counts for major component types
                auto countOf = [&](const char* name, usize count) {
                    if (count > 0)
                        ImGui::Text("  %-28s %zu", name, count);
                };
                countOf("TransformComponent",   m_World->GetEntitiesWithComponent<ECS::TransformComponent>().size());
                countOf("MeshComponent",         m_World->GetEntitiesWithComponent<ECS::MeshComponent>().size());
                countOf("MaterialComponent",     m_World->GetEntitiesWithComponent<ECS::MaterialComponent>().size());
                countOf("LightComponent",        m_World->GetEntitiesWithComponent<ECS::LightComponent>().size());
                countOf("CameraComponent",       m_World->GetEntitiesWithComponent<ECS::CameraComponent>().size());
                countOf("NameComponent",         m_World->GetEntitiesWithComponent<ECS::NameComponent>().size());
                countOf("NotesComponent",        m_World->GetEntitiesWithComponent<ECS::NotesComponent>().size());
                countOf("TextComponent",         m_World->GetEntitiesWithComponent<ECS::TextComponent>().size());
                countOf("ScriptComponent",       m_World->GetEntitiesWithComponent<ECS::ScriptComponent>().size());
                countOf("SkeletonComponent",     m_World->GetEntitiesWithComponent<ECS::SkeletonComponent>().size());
                countOf("LODComponent",          m_World->GetEntitiesWithComponent<ECS::LODComponent>().size());
                countOf("ParentComponent",       m_World->GetEntitiesWithComponent<ECS::ParentComponent>().size());
                countOf("TweenComponent",        m_World->GetEntitiesWithComponent<ECS::TweenComponent>().size());

                ImGui::Separator();
                ImGui::TextColored(ImVec4(0.7f, 0.9f, 1.0f, 1.0f), "-- Selection --");
                if (m_PrimarySelected != ECS::INVALID_ENTITY) {
                    ImGui::Text("Primary Selected: %llu", static_cast<unsigned long long>(m_PrimarySelected));
                    auto* name = m_World->GetComponent<ECS::NameComponent>(m_PrimarySelected);
                    if (name)
                        ImGui::Text("  Name: %s", name->name.c_str());
                    auto* transform = m_World->GetComponent<ECS::TransformComponent>(m_PrimarySelected);
                    if (transform) {
                        ImGui::Text("  Pos: (%.2f, %.2f, %.2f)", transform->position.x, transform->position.y, transform->position.z);
                        ImGui::Text("  Scale: (%.2f, %.2f, %.2f)", transform->scale.x, transform->scale.y, transform->scale.z);
                    }
                } else {
                    ImGui::TextDisabled("No entity selected");
                }
                ImGui::Text("Multi-select count: %zu", m_SelectedEntities.size());
            } else {
                ImGui::TextDisabled("World not available");
            }
            ImGui::EndTabItem();
        }

        // =====================================================================
        // Tab 4 — Scene
        // =====================================================================
        if (ImGui::BeginTabItem("Scene")) {
            ImGui::TextColored(ImVec4(0.7f, 0.9f, 1.0f, 1.0f), "-- Scene Info --");
            if (!m_CurrentScenePath.empty()) {
                ImGui::Text("Scene Path: %s", m_CurrentScenePath.c_str());
            } else {
                ImGui::TextDisabled("No scene loaded (unsaved)");
            }

            ImGui::Text("Scene Name: %s", m_SceneManager.GetCurrentSceneName().c_str());
            ImGui::Text("Project: %s", m_SceneManager.GetProjectName().c_str());
            if (!m_SceneManager.GetProjectPath().empty()) {
                ImGui::Text("Project Path: %s", m_SceneManager.GetProjectPath().c_str());
            }
            ImGui::Text("Scene Count: %zu", m_SceneManager.GetSceneCount());

            ImGui::Separator();
            ImGui::TextColored(ImVec4(0.7f, 0.9f, 1.0f, 1.0f), "-- Physics --");
            auto physType = m_SceneManager.GetPhysicsBackendType();
            const char* physName = "Auto";
            if (physType == Physics::PhysicsBackendType::Jolt) physName = "Jolt";
            else if (physType == Physics::PhysicsBackendType::Box2D) physName = "Box2D";
            ImGui::Text("Physics Backend: %s", physName);

            ImGui::Separator();
            ImGui::TextColored(ImVec4(0.7f, 0.9f, 1.0f, 1.0f), "-- Play Mode --");
            if (m_PlayMode.IsPlaying())
                ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "Status: Playing");
            else if (m_PlayMode.IsPaused())
                ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.2f, 1.0f), "Status: Paused");
            else
                ImGui::TextDisabled("Status: Stopped");
            ImGui::Text("Focus Mode: %s", m_FocusMode ? "On" : "Off");

            ImGui::EndTabItem();
        }

        // =====================================================================
        // Tab 5 — System
        // =====================================================================
        if (ImGui::BeginTabItem("System")) {
            ImGui::TextColored(ImVec4(0.7f, 0.9f, 1.0f, 1.0f), "-- Engine --");
            ImGui::Text("Engine: Enjin (TEGE)");
#ifdef ENJIN_VERSION_STRING
            ImGui::Text("Version: %s", ENJIN_VERSION_STRING);
#endif
            ImGui::Text("ImGui Version: %s", IMGUI_VERSION);

            ImGui::Separator();
            ImGui::TextColored(ImVec4(0.7f, 0.9f, 1.0f, 1.0f), "-- GPU --");
            if (!m_CachedGPUName.empty()) {
                ImGui::Text("GPU: %s", m_CachedGPUName.c_str());
            } else if (m_Renderer && m_Renderer->GetContext()) {
                VkPhysicalDeviceProperties props;
                vkGetPhysicalDeviceProperties(m_Renderer->GetContext()->GetPhysicalDevice(), &props);
                m_CachedGPUName = props.deviceName;
                ImGui::Text("GPU: %s", m_CachedGPUName.c_str());
            } else {
                ImGui::TextDisabled("GPU info not available");
            }

#if !ENJIN_RENDERER_WEBGPU
            if (m_Renderer && m_Renderer->GetContext()) {
                VkPhysicalDeviceProperties props;
                vkGetPhysicalDeviceProperties(m_Renderer->GetContext()->GetPhysicalDevice(), &props);
                ImGui::Text("Vulkan API: %u.%u.%u",
                            VK_VERSION_MAJOR(props.apiVersion),
                            VK_VERSION_MINOR(props.apiVersion),
                            VK_VERSION_PATCH(props.apiVersion));
                ImGui::Text("Driver Version: %u.%u.%u",
                            VK_VERSION_MAJOR(props.driverVersion),
                            VK_VERSION_MINOR(props.driverVersion),
                            VK_VERSION_PATCH(props.driverVersion));
            }

            if (m_Renderer) {
                VkExtent2D extent = m_Renderer->GetSwapchainExtent();
                ImGui::Text("Swapchain: %u x %u", extent.width, extent.height);
                ImGui::Text("HDR Output: %s", m_RenderSystem && m_RenderSystem->IsHDREnabled() ? "Yes" : "No");
            }
#endif

            ImGui::Separator();
            ImGui::TextColored(ImVec4(0.7f, 0.9f, 1.0f, 1.0f), "-- Window --");
            if (m_Window) {
                ImGui::Text("Window Size: %u x %u", m_Window->GetWidth(), m_Window->GetHeight());
            }
            ImGuiIO& io = ImGui::GetIO();
            ImGui::Text("Display Size: %.0f x %.0f", io.DisplaySize.x, io.DisplaySize.y);
            ImGui::Text("Display Scale: %.2f x %.2f", io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);

            ImGui::Separator();
            ImGui::TextColored(ImVec4(0.7f, 0.9f, 1.0f, 1.0f), "-- Build Configuration --");
#ifdef NDEBUG
            ImGui::Text("Build: Release");
#else
            ImGui::Text("Build: Debug");
#endif
#ifdef _WIN32
            ImGui::Text("Platform: Windows");
#elif defined(__linux__)
            ImGui::Text("Platform: Linux");
#elif defined(__APPLE__)
            ImGui::Text("Platform: macOS");
#else
            ImGui::Text("Platform: Unknown");
#endif

            ImGui::EndTabItem();
        }

        // =====================================================================
        // Tab 6 — Memory
        // =====================================================================
        if (ImGui::BeginTabItem("Memory")) {
            ImGui::TextColored(ImVec4(0.7f, 0.9f, 1.0f, 1.0f), "-- Entity Budget --");

            if (m_World) {
                u32 entityCount = 0;
                for (auto e : m_World->GetEntitiesWithComponent<ECS::TransformComponent>()) {
                    (void)e; entityCount++;
                }
                u32 meshCount = 0;
                for (auto e : m_World->GetEntitiesWithComponent<ECS::MeshComponent>()) {
                    (void)e; meshCount++;
                }
                ImGui::Text("Entities (with Transform): %u", entityCount);
                ImGui::Text("Meshes: %u", meshCount);
            }

            // Render system memory
            if (m_RenderSystem) {
                ImGui::Separator();
                ImGui::TextColored(ImVec4(0.7f, 0.9f, 1.0f, 1.0f), "-- Render Buffers --");
                ImGui::Text("Sorted render list: %zu entries", m_RenderSystem->GetSortedRenderListSize());
                ImGui::Text("Entity render data: %zu slots", m_RenderSystem->GetEntityRenderDataSize());
            }

            // Audio
            ImGui::Separator();
            ImGui::TextColored(ImVec4(0.7f, 0.9f, 1.0f, 1.0f), "-- Audio --");
            Audio::SimpleAudio* audio = m_PlayMode.IsPlaying() ? m_PlayMode.GetSimpleAudio() : nullptr;
            if (audio) {
                u32 totalSounds = 0;
                for (const auto* bus : audio->GetMixer().GetAllBuses()) totalSounds += bus->activeSoundCount;
                ImGui::Text("Active sounds: %u", totalSounds);
                ImGui::Text("Buses: %zu", audio->GetMixer().GetAllBuses().size());
            } else {
                ImGui::TextDisabled("Not in play mode");
            }

            // Rewind
            if (m_World) {
                ImGui::Separator();
                ImGui::TextColored(ImVec4(0.7f, 0.9f, 1.0f, 1.0f), "-- Rewind System --");
                bool any = false;
                for (auto entity : m_World->GetEntitiesWithComponent<ECS::RecordRewindComponent>()) {
                    auto* rr = m_World->GetComponent<ECS::RecordRewindComponent>(entity);
                    if (rr) { any = true;
                        ImGui::Text("Entity: %u/%u frames, ~%.1f KB",
                            rr->history.Count(), rr->history.Capacity(),
                            static_cast<f32>(rr->history.MemoryUsage()) / 1024.0f);
                    }
                }
                for (auto entity : m_World->GetEntitiesWithComponent<ECS::SceneRewindComponent>()) {
                    auto* sr = m_World->GetComponent<ECS::SceneRewindComponent>(entity);
                    if (sr) { any = true;
                        ImGui::Text("Scene: %u/%u frames, ~%.1f KB, cache: %zu",
                            sr->history.Count(), sr->history.Capacity(),
                            static_cast<f32>(sr->history.MemoryUsage()) / 1024.0f,
                            sr->prevFrameCache.size());
                    }
                }
                if (!any) ImGui::TextDisabled("No rewind components");
            }

            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::End();
}

// ---------------------------------------------------------------------------
// Debug HUD Overlay (F1) — transparent always-on-top performance strip
// ---------------------------------------------------------------------------
void EditorLayer::DrawDebugOverlay() {
    // Position at bottom-left of the main viewport
    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImVec2 overlaySize(vp->WorkSize.x, 0); // auto height

    // Anchor to bottom-left using pivot (0,1) so it grows upward
    ImVec2 overlayPos(vp->WorkPos.x, vp->WorkPos.y + vp->WorkSize.y);
    ImGui::SetNextWindowPos(overlayPos, ImGuiCond_Always, ImVec2(0.0f, 1.0f));
    ImGui::SetNextWindowSize(ImVec2(overlaySize.x, 0));
    ImGui::SetNextWindowBgAlpha(0.82f); // Strong opaque background for readability

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav |
        ImGuiWindowFlags_NoDocking;

    // Larger font for accessibility — scale up 1.4x
    ImGui::SetWindowFontScale(1.4f);
    if (!ImGui::Begin("##DebugOverlay", &m_ShowDebugOverlay, flags)) {
        ImGui::SetWindowFontScale(1.0f);
        ImGui::End();
        return;
    }

    ImGuiIO& io = ImGui::GetIO();
    f32 fps = io.Framerate;
    f32 frameMs = fps > 0.0f ? 1000.0f / fps : 0.0f;

    // FPS color
    ImVec4 fpsColor = fps >= 60.0f ? ImVec4(0.3f, 1.0f, 0.4f, 1.0f) :
                      fps >= 30.0f ? ImVec4(1.0f, 1.0f, 0.3f, 1.0f) :
                                     ImVec4(1.0f, 0.35f, 0.35f, 1.0f);

    // --- Compact line: FPS | Frame Time | Draw Calls | Triangles | Entities | Play Mode ---
    ImGui::TextColored(fpsColor, "%.0f FPS", fps);
    ImGui::SameLine(0, 20);
    ImGui::TextColored(ImVec4(0.7f, 0.8f, 0.9f, 1.0f), "%.2f ms", frameMs);

    if (m_RenderSystem) {
        ImGui::SameLine(0, 16);
        ImGui::Text("DC: %u", m_RenderSystem->GetDrawCallCount());

        u32 tris = m_RenderSystem->GetTriangleCount();
        ImGui::SameLine(0, 16);
        if (tris > 1000000)
            ImGui::Text("Tri: %.1fM", static_cast<f32>(tris) / 1000000.0f);
        else if (tris > 1000)
            ImGui::Text("Tri: %.1fK", static_cast<f32>(tris) / 1000.0f);
        else
            ImGui::Text("Tri: %u", tris);
    }

    if (m_World) {
        ImGui::SameLine(0, 16);
        ImGui::Text("Ent: %zu", m_World->GetEntityCount());
    }

    // Play mode indicator
    if (!m_PlayMode.IsStopped()) {
        ImGui::SameLine(0, 16);
        if (m_PlayMode.IsPlaying())
            ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "PLAYING");
        else if (m_PlayMode.IsPaused())
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.3f, 1.0f), "PAUSED");
    }

    // F1/F2 hint
    ImGui::SameLine(ImGui::GetContentRegionMax().x - 195);
    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 0.7f), "F1 Game Debug | F2 Engine Debug");

    // --- Detailed line (F2 to toggle) ---
    if (m_DebugOverlayDetail >= 1) {
        // Percentile stats
        ImGui::TextColored(ImVec4(0.6f, 0.7f, 0.8f, 0.9f),
            "P50: %.1f ms  P95: %.1f ms  P99: %.1f ms  |  Min: %.1f  Max: %.1f",
            m_FrameTimeP50, m_FrameTimeP95, m_FrameTimeP99,
            m_FrameTimeMin, m_FrameTimeMax);

        if (m_RenderSystem) {
            u32 hits = m_RenderSystem->GetDescriptorCacheHits();
            u32 total = hits + m_RenderSystem->GetDescriptorCacheWrites();
            ImGui::SameLine(0, 16);
            if (total > 0) {
                f32 hitRate = static_cast<f32>(hits) / static_cast<f32>(total) * 100.0f;
                ImGui::Text("Cache: %.0f%%", hitRate);
            }
        }

        // Memory with budget
        f32 processMB = static_cast<f32>(m_PerfMetrics.processMemoryBytes) / (1024.0f * 1024.0f);
        ImGui::SameLine(0, 16);
        ImGui::Text("RAM: %.0f MB", processMB);

        // GPU VRAM budget
        if (m_RenderSystem && m_RenderSystem->GetRenderer()) {
            auto* ctx = m_RenderSystem->GetRenderer()->GetContext();
            if (ctx) {
                u64 gpuBudget = ctx->GetGPUMemoryBudget();
                if (gpuBudget > 0) {
                    f32 budgetMB = static_cast<f32>(gpuBudget) / (1024.0f * 1024.0f);
                    ImGui::SameLine(0, 16);
                    ImGui::Text("VRAM: %.0f MB", budgetMB);
                }
            }
        }

        // Selected entity
        if (m_World && m_PrimarySelected != ECS::INVALID_ENTITY && m_World->IsValid(m_PrimarySelected)) {
            auto* nameComp = m_World->GetComponent<ECS::NameComponent>(m_PrimarySelected);
            ImGui::SameLine(0, 16);
            ImGui::TextColored(ImVec4(0.8f, 0.9f, 1.0f, 0.8f), "Sel: %s",
                nameComp ? nameComp->name.c_str() : "(unnamed)");
        }

        // Frame time graph (small, inline)
        f32 graphMax = m_FrameTimeMax > 0.0f ? m_FrameTimeMax * 1.5f : 33.3f;
        if (graphMax < 16.7f) graphMax = 33.3f;
        auto getter = [](void* data, int idx) -> float {
            EditorLayer* self = static_cast<EditorLayer*>(data);
            usize actualIdx = (self->m_FrameTimeIndex + static_cast<usize>(idx)) % FRAME_TIME_HISTORY_SIZE;
            return self->m_FrameTimeHistory[actualIdx];
        };
        ImGui::PlotLines("##OverlayGraph", getter, this,
                         static_cast<int>(FRAME_TIME_HISTORY_SIZE),
                         0, nullptr, 0.0f, graphMax, ImVec2(ImGui::GetContentRegionAvail().x, 36));
    }

    ImGui::SetWindowFontScale(1.0f);
    ImGui::End();
}

// ---------------------------------------------------------------------------
// F1 — Toggle Game Debug group
// Console panel + debug overlay (FPS, draw calls, entities)
// ---------------------------------------------------------------------------
void EditorLayer::ToggleGameDebug() {
    m_GameDebugActive = !m_GameDebugActive;

    // Close Engine Debug when opening Game Debug (only one debug group at a time)
    if (m_GameDebugActive && m_EngineDebugActive) {
        m_EngineDebugActive = false;
        SetPanelVisibility(EditorPanel::Profiler, false);
        SetPanelVisibility(EditorPanel::Rendering, false);
        SetPanelVisibility(EditorPanel::PostProcessing, false);
        SetPanelVisibility(EditorPanel::RetroEffects, false);
        SetPanelVisibility(EditorPanel::SaveDebug, false);
        m_ShowColliderWireframes = false;
        m_DebugOverlayDetail = 0;
    }

    // Toggle Console panel + Game Debug panel
    SetPanelVisibility(EditorPanel::Console, m_GameDebugActive);
    m_ShowGameDebug = m_GameDebugActive;

    // Toggle the compact debug overlay
    m_ShowDebugOverlay = m_GameDebugActive;

    // If turning off game debug, reset detail level so F2 starts clean
    if (!m_GameDebugActive) {
        m_DebugOverlayDetail = 0;
    }
}

// ---------------------------------------------------------------------------
// F2 — Toggle Engine Debug group
// Profiler, Rendering, PostProcessing, RetroEffects, SaveDebug panels +
// detailed overlay + collider wireframes
// ---------------------------------------------------------------------------
void EditorLayer::ToggleEngineDebug() {
    m_EngineDebugActive = !m_EngineDebugActive;

    // Close Game Debug when opening Engine Debug (only one debug group at a time)
    if (m_EngineDebugActive && m_GameDebugActive) {
        m_GameDebugActive = false;
        SetPanelVisibility(EditorPanel::Console, false);
    }

    // Toggle engine/editor debug panels
    SetPanelVisibility(EditorPanel::Profiler, m_EngineDebugActive);
    SetPanelVisibility(EditorPanel::Rendering, m_EngineDebugActive);
    SetPanelVisibility(EditorPanel::PostProcessing, m_EngineDebugActive);
    SetPanelVisibility(EditorPanel::RetroEffects, m_EngineDebugActive);
    SetPanelVisibility(EditorPanel::SaveDebug, m_EngineDebugActive);

    // Show detailed overlay line when engine debug is active
    m_DebugOverlayDetail = m_EngineDebugActive ? 1 : 0;

    // Also ensure the overlay HUD itself is visible when engine debug is on
    m_ShowDebugOverlay = m_EngineDebugActive;

    // Toggle collider wireframe visualization
    m_ShowColliderWireframes = m_EngineDebugActive;
}

// ---------------------------------------------------------------------------
// Debug Mode Status Indicator — bottom-right pill showing active debug groups
// ---------------------------------------------------------------------------
void EditorLayer::DrawDebugModeIndicator() {
    if (!m_GameDebugActive && !m_EngineDebugActive) return;

    ImGuiViewport* vp = ImGui::GetMainViewport();
    f32 padding = 10.0f;

    // Calculate indicator width based on content
    const char* f1Label = "F1: Game Debug";
    const char* f2Label = "F2: Engine Debug";
    f32 f1Width = m_GameDebugActive ? ImGui::CalcTextSize(f1Label).x : 0.0f;
    f32 f2Width = m_EngineDebugActive ? ImGui::CalcTextSize(f2Label).x : 0.0f;
    f32 dotWidth = 8.0f;   // colored dot
    f32 spacing = 12.0f;   // between dot and text
    f32 groupSpacing = 20.0f; // between F1 and F2 groups
    f32 hPad = 12.0f;      // horizontal padding inside pill

    f32 totalWidth = hPad * 2;
    if (m_GameDebugActive) totalWidth += dotWidth + spacing + f1Width;
    if (m_GameDebugActive && m_EngineDebugActive) totalWidth += groupSpacing;
    if (m_EngineDebugActive) totalWidth += dotWidth + spacing + f2Width;

    f32 indicatorH = 26.0f;
    ImVec2 indicatorPos(
        vp->WorkPos.x + vp->WorkSize.x - totalWidth - padding,
        vp->WorkPos.y + vp->WorkSize.y - indicatorH - padding);

    ImGui::SetNextWindowPos(indicatorPos);
    ImGui::SetNextWindowSize(ImVec2(totalWidth, indicatorH));
    ImGui::SetNextWindowBgAlpha(0.75f);

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav |
        ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoInputs;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(hPad, 4.0f));

    if (ImGui::Begin("##DebugModeIndicator", nullptr, flags)) {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 cursor = ImGui::GetCursorScreenPos();
        f32 textY = cursor.y + 1.0f;

        if (m_GameDebugActive) {
            // Green dot + "F1: Game Debug"
            ImVec2 dotCenter(cursor.x + dotWidth * 0.5f, textY + ImGui::GetTextLineHeight() * 0.5f);
            dl->AddCircleFilled(dotCenter, dotWidth * 0.5f, IM_COL32(80, 220, 120, 255));
            ImGui::SetCursorScreenPos(ImVec2(cursor.x + dotWidth + spacing, textY));
            ImGui::TextColored(ImVec4(0.5f, 0.9f, 0.6f, 1.0f), "%s", f1Label);
            ImGui::SameLine(0, groupSpacing);
            cursor = ImGui::GetCursorScreenPos();
            textY = cursor.y;
        }

        if (m_EngineDebugActive) {
            // Orange dot + "F2: Engine Debug"
            ImVec2 dotCenter(cursor.x + dotWidth * 0.5f, textY + ImGui::GetTextLineHeight() * 0.5f);
            dl->AddCircleFilled(dotCenter, dotWidth * 0.5f, IM_COL32(255, 180, 60, 255));
            ImGui::SetCursorScreenPos(ImVec2(cursor.x + dotWidth + spacing, textY));
            ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.3f, 1.0f), "%s", f2Label);
        }
    }
    ImGui::End();
    ImGui::PopStyleVar(2);
}

// ---------------------------------------------------------------------------
// Game Debug Panel (F1 legacy) — focused on debugging the user's game
// ---------------------------------------------------------------------------
void EditorLayer::DrawGameDebugPanel() {
    ImGui::SetNextWindowSize(ImVec2(600, 500), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Game Debug (F1)", &m_ShowGameDebug)) {
        ImGui::End();
        return;
    }

    if (ImGui::BeginTabBar("GameDebugTabs")) {

        // =================================================================
        // Tab 1 — Scene
        // =================================================================
        if (ImGui::BeginTabItem("Scene")) {
            // Current scene path
            ImGui::TextColored(ImVec4(0.7f, 0.9f, 1.0f, 1.0f), "-- Scene Info --");
            ImGui::Text("Scene Path: %s", m_CurrentScenePath.empty() ? "(unsaved)" : m_CurrentScenePath.c_str());

            // Play mode status
            const char* playStatus = "Stopped";
            ImVec4 statusColor = ImVec4(0.6f, 0.6f, 0.6f, 1.0f);
            if (m_PlayMode.IsPlaying()) {
                playStatus = "Playing";
                statusColor = ImVec4(0.2f, 1.0f, 0.2f, 1.0f);
            } else if (m_PlayMode.IsPaused()) {
                playStatus = "Paused";
                statusColor = ImVec4(1.0f, 1.0f, 0.2f, 1.0f);
            }
            ImGui::TextColored(statusColor, "Play Mode: %s", playStatus);

            // Entity count
            if (m_World) {
                ImGui::Text("Entity Count: %zu", m_World->GetEntityCount());
            } else {
                ImGui::TextDisabled("No world loaded");
            }

            ImGui::Separator();

            // Selected entity info
            ImGui::TextColored(ImVec4(0.7f, 0.9f, 1.0f, 1.0f), "-- Selected Entity --");
            if (m_World && m_PrimarySelected != ECS::INVALID_ENTITY && m_World->IsValid(m_PrimarySelected)) {
                auto* nameComp = m_World->GetComponent<ECS::NameComponent>(m_PrimarySelected);
                ImGui::Text("Name: %s", nameComp ? nameComp->name.c_str() : "(unnamed)");
                ImGui::Text("ID: %llu", static_cast<unsigned long long>(m_PrimarySelected));

                // List components on the selected entity
                ImGui::Text("Components:");
                if (m_World->HasComponent<ECS::TransformComponent>(m_PrimarySelected))     ImGui::BulletText("Transform");
                if (m_World->HasComponent<ECS::MeshComponent>(m_PrimarySelected))           ImGui::BulletText("Mesh");
                if (m_World->HasComponent<ECS::MaterialComponent>(m_PrimarySelected))       ImGui::BulletText("Material");
                if (m_World->HasComponent<ECS::LightComponent>(m_PrimarySelected))          ImGui::BulletText("Light");
                if (m_World->HasComponent<ECS::CameraComponent>(m_PrimarySelected))         ImGui::BulletText("Camera");
                if (m_World->HasComponent<ECS::RigidbodyComponent>(m_PrimarySelected))      ImGui::BulletText("Rigidbody");
                if (m_World->HasComponent<ECS::BoxColliderComponent>(m_PrimarySelected))    ImGui::BulletText("BoxCollider");
                if (m_World->HasComponent<ECS::SphereColliderComponent>(m_PrimarySelected)) ImGui::BulletText("SphereCollider");
                if (m_World->HasComponent<ECS::CapsuleColliderComponent>(m_PrimarySelected))ImGui::BulletText("CapsuleCollider");
                if (m_World->HasComponent<ECS::ScriptComponent>(m_PrimarySelected))         ImGui::BulletText("Script");
                if (m_World->HasComponent<ECS::AudioSourceComponent>(m_PrimarySelected))    ImGui::BulletText("AudioSource");
                if (m_World->HasComponent<ECS::ParticleEmitterComponent>(m_PrimarySelected))ImGui::BulletText("ParticleEmitter");
                if (m_World->HasComponent<ECS::TweenComponent>(m_PrimarySelected))          ImGui::BulletText("Tween");
                if (m_World->HasComponent<ECS::HealthComponent>(m_PrimarySelected))         ImGui::BulletText("Health");
                if (m_World->HasComponent<ECS::GravityZoneComponent>(m_PrimarySelected))    ImGui::BulletText("GravityZone");
                if (m_World->HasComponent<ECS::FluidVolumeComponent>(m_PrimarySelected))    ImGui::BulletText("FluidVolume");
            } else {
                ImGui::TextDisabled("No entity selected");
            }

            ImGui::Separator();

            // List all entities
            ImGui::TextColored(ImVec4(0.7f, 0.9f, 1.0f, 1.0f), "-- All Entities --");
            if (m_World) {
                const auto& entities = m_World->GetAllEntities();
                if (entities.empty()) {
                    ImGui::TextDisabled("No entities in scene");
                } else {
                    ImGui::BeginChild("EntityList", ImVec2(0, 0), true);
                    for (auto entity : entities) {
                        if (!m_World->IsValid(entity)) continue;
                        auto* nameComp = m_World->GetComponent<ECS::NameComponent>(entity);
                        const char* name = nameComp ? nameComp->name.c_str() : "(unnamed)";
                        char label[256];
                        snprintf(label, sizeof(label), "[%llu] %s", static_cast<unsigned long long>(entity), name);
                        bool isSelected = (entity == m_PrimarySelected);
                        if (ImGui::Selectable(label, isSelected)) {
                            SelectEntity(entity);
                        }
                    }
                    ImGui::EndChild();
                }
            }

            ImGui::EndTabItem();
        }

        // =================================================================
        // Tab 2 — Physics
        // =================================================================
        if (ImGui::BeginTabItem("Physics")) {
            ImGui::TextColored(ImVec4(0.7f, 0.9f, 1.0f, 1.0f), "-- Physics Backend --");
            auto backendType = m_SceneManager.GetPhysicsBackendType();
            const char* backendName = "Auto";
            if (backendType == Physics::PhysicsBackendType::Jolt) backendName = "Jolt (3D)";
            else if (backendType == Physics::PhysicsBackendType::Box2D) backendName = "Box2D (2D)";
            ImGui::Text("Backend: %s", backendName);

            ImGui::Separator();

            // Collider wireframe toggle
            ImGui::Checkbox("Show Collider Wireframes", &m_ShowColliderWireframes);

            ImGui::Separator();

            if (m_World) {
                const auto& entities = m_World->GetAllEntities();

                // Count physics entities
                u32 rigidbodyCount = 0;
                u32 boxColliderCount = 0;
                u32 sphereColliderCount = 0;
                u32 capsuleColliderCount = 0;
                u32 gravityZoneCount = 0;
                u32 fluidVolumeCount = 0;

                for (auto entity : entities) {
                    if (!m_World->IsValid(entity)) continue;
                    if (m_World->HasComponent<ECS::RigidbodyComponent>(entity))       rigidbodyCount++;
                    if (m_World->HasComponent<ECS::BoxColliderComponent>(entity))      boxColliderCount++;
                    if (m_World->HasComponent<ECS::SphereColliderComponent>(entity))   sphereColliderCount++;
                    if (m_World->HasComponent<ECS::CapsuleColliderComponent>(entity))  capsuleColliderCount++;
                    if (m_World->HasComponent<ECS::GravityZoneComponent>(entity))      gravityZoneCount++;
                    if (m_World->HasComponent<ECS::FluidVolumeComponent>(entity))      fluidVolumeCount++;
                }

                ImGui::TextColored(ImVec4(0.7f, 0.9f, 1.0f, 1.0f), "-- Counts --");
                ImGui::Text("Rigidbodies: %u", rigidbodyCount);
                ImGui::Text("Box Colliders: %u", boxColliderCount);
                ImGui::Text("Sphere Colliders: %u", sphereColliderCount);
                ImGui::Text("Capsule Colliders: %u", capsuleColliderCount);
                ImGui::Text("Gravity Zones: %u", gravityZoneCount);
                ImGui::Text("Fluid Volumes: %u", fluidVolumeCount);

                ImGui::Separator();

                // List entities with physics bodies
                ImGui::TextColored(ImVec4(0.7f, 0.9f, 1.0f, 1.0f), "-- Physics Entities --");
                ImGui::BeginChild("PhysicsEntityList", ImVec2(0, 0), true);
                for (auto entity : entities) {
                    if (!m_World->IsValid(entity)) continue;
                    bool hasPhysics = m_World->HasComponent<ECS::RigidbodyComponent>(entity) ||
                                      m_World->HasComponent<ECS::BoxColliderComponent>(entity) ||
                                      m_World->HasComponent<ECS::SphereColliderComponent>(entity) ||
                                      m_World->HasComponent<ECS::CapsuleColliderComponent>(entity);
                    if (!hasPhysics) continue;

                    auto* nameComp = m_World->GetComponent<ECS::NameComponent>(entity);
                    const char* name = nameComp ? nameComp->name.c_str() : "(unnamed)";

                    // Build a tag string showing which physics components it has
                    std::string tags;
                    if (m_World->HasComponent<ECS::RigidbodyComponent>(entity)) {
                        auto* rb = m_World->GetComponent<ECS::RigidbodyComponent>(entity);
                        if (rb) {
                            const char* bodyTypeStr = "Dynamic";
                            if (rb->bodyType == ECS::RigidbodyComponent::BodyType::Kinematic) bodyTypeStr = "Kinematic";
                            else if (rb->bodyType == ECS::RigidbodyComponent::BodyType::Static) bodyTypeStr = "Static";
                            tags += bodyTypeStr;
                        }
                    }
                    if (m_World->HasComponent<ECS::BoxColliderComponent>(entity))      { if (!tags.empty()) tags += " | "; tags += "Box"; }
                    if (m_World->HasComponent<ECS::SphereColliderComponent>(entity))    { if (!tags.empty()) tags += " | "; tags += "Sphere"; }
                    if (m_World->HasComponent<ECS::CapsuleColliderComponent>(entity))   { if (!tags.empty()) tags += " | "; tags += "Capsule"; }

                    char label[512];
                    snprintf(label, sizeof(label), "[%llu] %s  (%s)",
                             static_cast<unsigned long long>(entity), name, tags.c_str());
                    if (ImGui::Selectable(label, entity == m_PrimarySelected)) {
                        SelectEntity(entity);
                    }
                }
                ImGui::EndChild();
            } else {
                ImGui::TextDisabled("No world loaded");
            }

            ImGui::EndTabItem();
        }

        // =================================================================
        // Tab 3 — Scripts
        // =================================================================
        if (ImGui::BeginTabItem("Scripts")) {
            ImGui::TextColored(ImVec4(0.7f, 0.9f, 1.0f, 1.0f), "-- Script Components --");

            if (m_World) {
                const auto& entities = m_World->GetAllEntities();
                u32 scriptEntityCount = 0;
                u32 totalScripts = 0;
                u32 errorCount = 0;

                // First pass: count
                for (auto entity : entities) {
                    if (!m_World->IsValid(entity)) continue;
                    auto* sc = m_World->GetComponent<ECS::ScriptComponent>(entity);
                    if (!sc) continue;
                    scriptEntityCount++;
                    totalScripts += static_cast<u32>(sc->scripts.size());
                    for (const auto& attachment : sc->scripts) {
                        if (attachment.hasError) errorCount++;
                    }
                }

                ImGui::Text("Entities with scripts: %u", scriptEntityCount);
                ImGui::Text("Total script attachments: %u", totalScripts);
                if (errorCount > 0) {
                    ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Scripts with errors: %u", errorCount);
                } else {
                    ImGui::Text("Scripts with errors: 0");
                }

                ImGui::Separator();

                ImGui::BeginChild("ScriptList", ImVec2(0, 0), true);
                for (auto entity : entities) {
                    if (!m_World->IsValid(entity)) continue;
                    auto* sc = m_World->GetComponent<ECS::ScriptComponent>(entity);
                    if (!sc || sc->scripts.empty()) continue;

                    auto* nameComp = m_World->GetComponent<ECS::NameComponent>(entity);
                    const char* name = nameComp ? nameComp->name.c_str() : "(unnamed)";

                    bool nodeOpen = ImGui::TreeNode(reinterpret_cast<void*>(static_cast<uintptr_t>(entity)),
                                                     "[%llu] %s", static_cast<unsigned long long>(entity), name);
                    if (nodeOpen) {
                        for (const auto& attachment : sc->scripts) {
                            ImVec4 color = attachment.hasError ? ImVec4(1.0f, 0.3f, 0.3f, 1.0f) :
                                           attachment.enabled ? ImVec4(0.8f, 0.8f, 0.8f, 1.0f) :
                                                                ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
                            ImGui::TextColored(color, "  %s", attachment.scriptPath.empty() ? "(no path)" : attachment.scriptPath.c_str());
                            if (!attachment.className.empty()) {
                                ImGui::SameLine();
                                ImGui::TextDisabled("(%s)", attachment.className.c_str());
                            }
                            // Status indicators
                            ImGui::SameLine();
                            if (attachment.hasError) {
                                ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "[ERROR]");
                                if (!attachment.lastError.empty() && ImGui::IsItemHovered()) {
                                    ImGui::SetTooltip("%s", attachment.lastError.c_str());
                                }
                            } else if (!attachment.enabled) {
                                ImGui::TextDisabled("[disabled]");
                            } else if (attachment.started) {
                                ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "[running]");
                            } else if (attachment.initialized) {
                                ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.2f, 1.0f), "[initialized]");
                            }
                        }
                        ImGui::TreePop();
                    }
                }
                ImGui::EndChild();
            } else {
                ImGui::TextDisabled("No world loaded");
            }

            ImGui::EndTabItem();
        }

        // =================================================================
        // Tab 4 — Audio
        // =================================================================
        if (ImGui::BeginTabItem("Audio")) {
            ImGui::TextColored(ImVec4(0.7f, 0.9f, 1.0f, 1.0f), "-- Audio Sources --");

            if (m_World) {
                const auto& entities = m_World->GetAllEntities();
                u32 audioCount = 0;
                u32 playingCount = 0;

                // First pass: count
                for (auto entity : entities) {
                    if (!m_World->IsValid(entity)) continue;
                    auto* audio = m_World->GetComponent<ECS::AudioSourceComponent>(entity);
                    if (!audio) continue;
                    audioCount++;
                    if (audio->isPlaying) playingCount++;
                }

                ImGui::Text("Audio sources: %u", audioCount);
                ImGui::Text("Currently playing: %u", playingCount);

                ImGui::Separator();

                ImGui::BeginChild("AudioList", ImVec2(0, 0), true);
                for (auto entity : entities) {
                    if (!m_World->IsValid(entity)) continue;
                    auto* audio = m_World->GetComponent<ECS::AudioSourceComponent>(entity);
                    if (!audio) continue;

                    auto* nameComp = m_World->GetComponent<ECS::NameComponent>(entity);
                    const char* name = nameComp ? nameComp->name.c_str() : "(unnamed)";

                    // Channel name
                    const char* channelName = "SFX";
                    if (audio->channel == ECS::AudioChannel::Music) channelName = "Music";
                    else if (audio->channel == ECS::AudioChannel::UI) channelName = "UI";
                    else if (audio->channel == ECS::AudioChannel::Voice) channelName = "Voice";

                    bool nodeOpen = ImGui::TreeNode(reinterpret_cast<void*>(static_cast<uintptr_t>(entity)),
                                                     "[%llu] %s", static_cast<unsigned long long>(entity), name);
                    if (nodeOpen) {
                        ImGui::Text("  Clip: %s", audio->clipPath.empty() ? "(none)" : audio->clipPath.c_str());
                        ImVec4 audioColor = audio->isPlaying ? ImVec4(0.2f, 1.0f, 0.2f, 1.0f) : ImVec4(0.6f, 0.6f, 0.6f, 1.0f);
                        ImGui::TextColored(audioColor, "  Status: %s", audio->isPlaying ? "Playing" : "Stopped");
                        ImGui::Text("  Channel: %s", channelName);
                        ImGui::Text("  Volume: %.2f  Pitch: %.2f", audio->volume, audio->pitch);
                        ImGui::Text("  3D: %s  Loop: %s", audio->is3D ? "Yes" : "No", audio->loop ? "Yes" : "No");
                        ImGui::TreePop();
                    }
                }
                ImGui::EndChild();
            } else {
                ImGui::TextDisabled("No world loaded");
            }

            ImGui::EndTabItem();
        }

        // =================================================================
        // Tab 5 — Gameplay
        // =================================================================
        if (ImGui::BeginTabItem("Gameplay")) {
            if (m_World) {
                const auto& entities = m_World->GetAllEntities();

                // Tweens
                ImGui::TextColored(ImVec4(0.7f, 0.9f, 1.0f, 1.0f), "-- Active Tweens --");
                u32 tweenEntityCount = 0;
                u32 activeTweenCount = 0;
                for (auto entity : entities) {
                    if (!m_World->IsValid(entity)) continue;
                    auto* tc = m_World->GetComponent<ECS::TweenComponent>(entity);
                    if (!tc) continue;
                    tweenEntityCount++;
                    for (const auto& t : tc->tweens) {
                        if (t.isPlaying) activeTweenCount++;
                    }
                }
                ImGui::Text("Entities with tweens: %u", tweenEntityCount);
                ImGui::Text("Active tweens: %u", activeTweenCount);

                ImGui::Separator();

                // Particle emitters
                ImGui::TextColored(ImVec4(0.7f, 0.9f, 1.0f, 1.0f), "-- Particle Emitters --");
                u32 emitterCount = 0;
                u32 playingEmitters = 0;
                for (auto entity : entities) {
                    if (!m_World->IsValid(entity)) continue;
                    auto* pe = m_World->GetComponent<ECS::ParticleEmitterComponent>(entity);
                    if (!pe) continue;
                    emitterCount++;
                    if (pe->isPlaying) playingEmitters++;
                }
                ImGui::Text("Particle emitters: %u", emitterCount);
                ImGui::Text("Currently playing: %u", playingEmitters);

                ImGui::Separator();

                // Health components
                ImGui::TextColored(ImVec4(0.7f, 0.9f, 1.0f, 1.0f), "-- Health --");
                u32 healthCount = 0;
                u32 deadCount = 0;
                for (auto entity : entities) {
                    if (!m_World->IsValid(entity)) continue;
                    auto* hc = m_World->GetComponent<ECS::HealthComponent>(entity);
                    if (!hc) continue;
                    healthCount++;
                    if (hc->isDead) deadCount++;
                }
                ImGui::Text("Entities with health: %u", healthCount);
                if (deadCount > 0) {
                    ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Dead entities: %u", deadCount);
                } else {
                    ImGui::Text("Dead entities: 0");
                }

                ImGui::Separator();

                // Interactables and pickups
                ImGui::TextColored(ImVec4(0.7f, 0.9f, 1.0f, 1.0f), "-- Interaction --");
                u32 interactableCount = 0;
                u32 pickupCount = 0;
                u32 triggerZoneCount = 0;
                for (auto entity : entities) {
                    if (!m_World->IsValid(entity)) continue;
                    if (m_World->HasComponent<ECS::InteractableComponent>(entity))  interactableCount++;
                    if (m_World->HasComponent<ECS::PickupComponent>(entity))        pickupCount++;
                    if (m_World->HasComponent<ECS::TriggerZoneComponent>(entity))   triggerZoneCount++;
                }
                ImGui::Text("Interactables: %u", interactableCount);
                ImGui::Text("Pickups: %u", pickupCount);
                ImGui::Text("Trigger Zones: %u", triggerZoneCount);
            } else {
                ImGui::TextDisabled("No world loaded");
            }

            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::End();
}

// ---------------------------------------------------------------------------
// Quake/Doom-style drop-down console
// ---------------------------------------------------------------------------
void EditorLayer::DrawDropConsole(f32 deltaTime) {
    // Animate slide: lerp m_DropConsoleAnim toward 1 (open) or 0 (closed)
    f32 target = m_ShowDropConsole ? 1.0f : 0.0f;
    constexpr f32 speed = 8.0f;
    if (m_DropConsoleAnim < target)
        m_DropConsoleAnim = std::min(m_DropConsoleAnim + speed * deltaTime, target);
    else if (m_DropConsoleAnim > target)
        m_DropConsoleAnim = std::max(m_DropConsoleAnim - speed * deltaTime, target);

    if (m_DropConsoleAnim <= 0.001f) return;  // fully hidden — nothing to draw

    ImGuiIO& io = ImGui::GetIO();
    f32 consoleH = io.DisplaySize.y * 0.4f;
    f32 visibleH = consoleH * m_DropConsoleAnim;

    // Position: slides up from the bottom edge
    ImGui::SetNextWindowPos(ImVec2(0, io.DisplaySize.y - visibleH));
    ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x, consoleH));

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNav;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12, 8));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.04f, 0.05f, 0.07f, 0.92f));

    if (ImGui::Begin("##DropConsole", nullptr, flags)) {
        // Title bar
        ImGui::TextColored(ImVec4(0.6f, 0.8f, 0.6f, 1.0f), "CONSOLE");
        ImGui::SameLine(io.DisplaySize.x - 200);
        ImGui::TextColored(ImVec4(0.4f, 0.45f, 0.5f, 1.0f), "Press ` to close");
        ImGui::Separator();

        // Scrollable log area (fill available space minus input line height)
        f32 inputH = ImGui::GetFrameHeightWithSpacing() + 4;
        ImGui::BeginChild("##DropConsoleLog", ImVec2(0, -inputH), false);

        for (const auto& entry : m_ConsoleLog) {
            // Color based on log level / content
            ImVec4 color(0.75f, 0.75f, 0.75f, 1.0f);
            if (entry.level >= LogLevel::Error)
                color = ImVec4(1.0f, 0.35f, 0.35f, 1.0f);
            else if (entry.level == LogLevel::Warn)
                color = ImVec4(1.0f, 0.85f, 0.3f, 1.0f);
            else if (!entry.message.empty() && entry.message[0] == '>')
                color = ImVec4(0.5f, 0.8f, 0.5f, 1.0f);  // User input echo = green

            ImGui::PushStyleColor(ImGuiCol_Text, color);
            ImGui::TextUnformatted(entry.message.c_str());
            ImGui::PopStyleColor();
        }

        // Auto-scroll to bottom when new output arrives
        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 4.0f)
            ImGui::SetScrollHereY(1.0f);

        ImGui::EndChild();

        // Input line
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.5f, 0.8f, 0.5f, 1.0f), ">");
        ImGui::SameLine();

        // Auto-focus the input field when the console is open and animation is mostly done
        if (m_ShowDropConsole && m_DropConsoleAnim > 0.5f) {
            ImGui::SetKeyboardFocusHere();
        }

        ImGui::PushItemWidth(-1);
        ImGuiInputTextFlags inputFlags = ImGuiInputTextFlags_EnterReturnsTrue |
            ImGuiInputTextFlags_CallbackHistory | ImGuiInputTextFlags_CallbackCharFilter;

        // Combined callback: history (up/down) + character filter (eat backtick)
        auto callback = [](ImGuiInputTextCallbackData* data) -> int {
            EditorLayer* self = static_cast<EditorLayer*>(data->UserData);

            if (data->EventFlag == ImGuiInputTextFlags_CallbackCharFilter) {
                // Eat the backtick/tilde character so it doesn't appear in the input
                if (data->EventChar == '`' || data->EventChar == '~')
                    return 1;  // Reject character
                return 0;
            }

            if (data->EventFlag == ImGuiInputTextFlags_CallbackHistory) {
                const int historySize = static_cast<int>(self->m_DropConsoleHistory.size());
                if (data->EventKey == ImGuiKey_UpArrow) {
                    if (self->m_DropConsoleHistoryPos < historySize - 1) {
                        self->m_DropConsoleHistoryPos++;
                        int idx = historySize - 1 - self->m_DropConsoleHistoryPos;
                        data->DeleteChars(0, data->BufTextLen);
                        data->InsertChars(0, self->m_DropConsoleHistory[idx].c_str());
                    }
                } else if (data->EventKey == ImGuiKey_DownArrow) {
                    if (self->m_DropConsoleHistoryPos > 0) {
                        self->m_DropConsoleHistoryPos--;
                        int idx = historySize - 1 - self->m_DropConsoleHistoryPos;
                        data->DeleteChars(0, data->BufTextLen);
                        data->InsertChars(0, self->m_DropConsoleHistory[idx].c_str());
                    } else {
                        self->m_DropConsoleHistoryPos = -1;
                        data->DeleteChars(0, data->BufTextLen);
                    }
                }
            }
            return 0;
        };

        if (ImGui::InputText("##DropConsoleInput", m_DropConsoleInput, sizeof(m_DropConsoleInput),
                             inputFlags, callback, this)) {
            if (m_DropConsoleInput[0] != '\0') {
                std::string cmd(m_DropConsoleInput);
                m_ConsoleLog.push_back("> " + cmd);
                m_DropConsoleHistory.push_back(cmd);
                m_DropConsoleHistoryPos = -1;
                ExecuteConsoleCommand(cmd);
                m_DropConsoleInput[0] = '\0';
            }
        }
        ImGui::PopItemWidth();
    }
    ImGui::End();

    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
}

// ============================================================================
// Bone Weight Visualization
// ============================================================================

void EditorLayer::ApplyBoneWeightColors(ECS::Entity entity, i32 boneIndex) {
    if (!m_World || boneIndex < 0) return;

    auto* mesh = m_World->GetComponent<ECS::MeshComponent>(entity);
    if (!mesh || mesh->vertices.empty()) return;

    // Save original colors if this is a new entity or we haven't saved yet
    if (m_BoneWeightEntity != entity || m_BoneWeightOriginalColors.empty()) {
        m_BoneWeightOriginalColors.resize(mesh->vertices.size());
        for (usize i = 0; i < mesh->vertices.size(); ++i) {
            m_BoneWeightOriginalColors[i] = mesh->vertices[i].color;
        }
        m_BoneWeightEntity = entity;
    }

    // Apply heat map colors based on the selected bone's weight
    u32 targetBone = static_cast<u32>(boneIndex);
    for (usize i = 0; i < mesh->vertices.size(); ++i) {
        auto& vert = mesh->vertices[i];

        // Find the weight for this bone on this vertex
        f32 weight = 0.0f;
        for (int j = 0; j < 4; ++j) {
            if (vert.boneIndices[j] == targetBone) {
                // boneWeights is a Vector4, access by component index
                switch (j) {
                    case 0: weight = vert.boneWeights.x; break;
                    case 1: weight = vert.boneWeights.y; break;
                    case 2: weight = vert.boneWeights.z; break;
                    case 3: weight = vert.boneWeights.w; break;
                }
                break;
            }
        }

        // Heat map: blue (0) -> green (0.5) -> red (1.0)
        f32 r = 0.0f, g = 0.0f, b = 0.0f;
        if (weight < 0.5f) {
            // Blue to Green
            f32 t = weight * 2.0f;
            b = 1.0f - t;
            g = t;
        } else {
            // Green to Red
            f32 t = (weight - 0.5f) * 2.0f;
            g = 1.0f - t;
            r = t;
        }
        vert.color = Math::Vector4(r, g, b, 1.0f);
    }

    // Mark the mesh AABB dirty so the renderer re-uploads vertex data
    mesh->aabbDirty = true;
}

void EditorLayer::RestoreBoneWeightColors(ECS::Entity entity) {
    if (!m_World || m_BoneWeightEntity != entity || m_BoneWeightOriginalColors.empty()) return;

    auto* mesh = m_World->GetComponent<ECS::MeshComponent>(entity);
    if (!mesh) return;

    // Restore original vertex colors
    usize count = (std::min)(m_BoneWeightOriginalColors.size(), mesh->vertices.size());
    for (usize i = 0; i < count; ++i) {
        mesh->vertices[i].color = m_BoneWeightOriginalColors[i];
    }

    mesh->aabbDirty = true;
    m_BoneWeightOriginalColors.clear();
    m_BoneWeightEntity = ECS::INVALID_ENTITY;
}

// ============================================================================
// UV Preview Panel
// ============================================================================

void EditorLayer::DrawUVPreviewPanel() {
    if (!m_ShowUVPreview) return;

    ImGui::SetNextWindowSize(ImVec2(400, 420), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("UV Preview", &m_ShowUVPreview)) {
        ImGui::End();
        return;
    }

    if (!m_World || m_PrimarySelected == ECS::INVALID_ENTITY) {
        ImGui::TextDisabled("Select an entity with a MeshComponent to preview UVs.");
        ImGui::End();
        return;
    }

    auto* mesh = m_World->GetComponent<ECS::MeshComponent>(m_PrimarySelected);
    if (!mesh || mesh->vertices.empty() || mesh->indices.empty()) {
        ImGui::TextDisabled("Selected entity has no mesh data.");
        ImGui::End();
        return;
    }

    const auto& verts = mesh->vertices;
    const auto& indices = mesh->indices;

    // Compute UV statistics
    f32 uvMinU = 1e9f, uvMinV = 1e9f;
    f32 uvMaxU = -1e9f, uvMaxV = -1e9f;
    for (const auto& v : verts) {
        if (v.uv.x < uvMinU) uvMinU = v.uv.x;
        if (v.uv.y < uvMinV) uvMinV = v.uv.y;
        if (v.uv.x > uvMaxU) uvMaxU = v.uv.x;
        if (v.uv.y > uvMaxV) uvMaxV = v.uv.y;
    }

    // UV statistics header
    if (ImGui::CollapsingHeader("UV Statistics", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("Vertices: %zu  Triangles: %zu", verts.size(), indices.size() / 3);
        ImGui::Text("UV Range: (%.3f, %.3f) to (%.3f, %.3f)", uvMinU, uvMinV, uvMaxU, uvMaxV);

        // Coverage: approximate area of UV triangles / total 0-1 space
        f64 totalArea = 0.0;
        for (usize i = 0; i + 2 < indices.size(); i += 3) {
            const Math::Vector2& uv0 = verts[indices[i]].uv;
            const Math::Vector2& uv1 = verts[indices[i + 1]].uv;
            const Math::Vector2& uv2 = verts[indices[i + 2]].uv;
            // Signed area of triangle in UV space (cross product / 2)
            f64 area = 0.5 * std::abs(
                static_cast<f64>(uv1.x - uv0.x) * static_cast<f64>(uv2.y - uv0.y) -
                static_cast<f64>(uv2.x - uv0.x) * static_cast<f64>(uv1.y - uv0.y));
            totalArea += area;
        }
        f32 coverage = static_cast<f32>(totalArea * 100.0);
        ImGui::Text("UV Coverage: %.1f%%", coverage);
    }

    ImGui::Separator();

    // Get available space for UV drawing
    ImVec2 avail = ImGui::GetContentRegionAvail();
    f32 drawSize = (avail.x < avail.y) ? avail.x : avail.y;
    if (drawSize < 50.0f) drawSize = 50.0f;

    ImVec2 canvasPos = ImGui::GetCursorScreenPos();
    ImVec2 canvasSize(drawSize, drawSize);

    // Reserve space
    ImGui::InvisibleButton("##UVCanvas", canvasSize);

    ImDrawList* drawList = ImGui::GetWindowDrawList();

    // Background (dark)
    drawList->AddRectFilled(canvasPos,
        ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y),
        IM_COL32(30, 30, 30, 255));

    // Optional: show base color texture as background
    // (Requires ImGui texture descriptor which we may not have for this entity)
    // Check if material has a texture loaded in the ImGui cache
    auto* material = m_World->GetComponent<ECS::MaterialComponent>(m_PrimarySelected);
    if (material && !material->baseColorTexturePath.empty()) {
        VkDescriptorSet texDS = GetImGuiTexture(material->baseColorTexturePath);
        if (texDS) {
            drawList->AddImage(
                static_cast<ImTextureID>(reinterpret_cast<uintptr_t>(texDS)),
                canvasPos,
                ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y),
                ImVec2(0, 0), ImVec2(1, 1),
                IM_COL32(255, 255, 255, 80));  // Low opacity background
        }
    }

    // Draw border (UV 0-1 space)
    drawList->AddRect(canvasPos,
        ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y),
        IM_COL32(80, 80, 80, 255));

    // Helper: map UV to screen position
    auto uvToScreen = [&](f32 u, f32 v) -> ImVec2 {
        return ImVec2(
            canvasPos.x + u * canvasSize.x,
            canvasPos.y + v * canvasSize.y
        );
    };

    // Draw UV wireframe (all triangles)
    ImU32 wireColor = IM_COL32(200, 200, 200, 100);
    ImU32 wireColorSelected = IM_COL32(255, 220, 50, 200);

    for (usize i = 0; i + 2 < indices.size(); i += 3) {
        const Math::Vector2& uv0 = verts[indices[i]].uv;
        const Math::Vector2& uv1 = verts[indices[i + 1]].uv;
        const Math::Vector2& uv2 = verts[indices[i + 2]].uv;

        ImVec2 p0 = uvToScreen(uv0.x, uv0.y);
        ImVec2 p1 = uvToScreen(uv1.x, uv1.y);
        ImVec2 p2 = uvToScreen(uv2.x, uv2.y);

        // Check if mouse is hovering this triangle for highlighting
        ImVec2 mousePos = ImGui::GetMousePos();
        bool hovered = false;

        // Barycentric test for point-in-triangle
        {
            f32 d00 = (p1.x - p0.x) * (p1.x - p0.x) + (p1.y - p0.y) * (p1.y - p0.y);
            f32 d01 = (p1.x - p0.x) * (p2.x - p0.x) + (p1.y - p0.y) * (p2.y - p0.y);
            f32 d11 = (p2.x - p0.x) * (p2.x - p0.x) + (p2.y - p0.y) * (p2.y - p0.y);
            f32 d20 = (mousePos.x - p0.x) * (p1.x - p0.x) + (mousePos.y - p0.y) * (p1.y - p0.y);
            f32 d21 = (mousePos.x - p0.x) * (p2.x - p0.x) + (mousePos.y - p0.y) * (p2.y - p0.y);
            f32 denom = d00 * d11 - d01 * d01;
            if (std::abs(denom) > 1e-10f) {
                f32 bv = (d11 * d20 - d01 * d21) / denom;
                f32 bw = (d00 * d21 - d01 * d20) / denom;
                f32 bu = 1.0f - bv - bw;
                if (bu >= 0.0f && bv >= 0.0f && bw >= 0.0f) {
                    hovered = true;
                }
            }
        }

        ImU32 color = hovered ? wireColorSelected : wireColor;
        f32 thickness = hovered ? 2.0f : 1.0f;

        drawList->AddLine(p0, p1, color, thickness);
        drawList->AddLine(p1, p2, color, thickness);
        drawList->AddLine(p2, p0, color, thickness);

        // Show triangle index tooltip on hover
        if (hovered) {
            ImGui::BeginTooltip();
            ImGui::Text("Triangle %zu", i / 3);
            ImGui::Text("UV0: (%.3f, %.3f)", uv0.x, uv0.y);
            ImGui::Text("UV1: (%.3f, %.3f)", uv1.x, uv1.y);
            ImGui::Text("UV2: (%.3f, %.3f)", uv2.x, uv2.y);
            ImGui::EndTooltip();
        }
    }

    // Draw UV space grid lines (0.25 intervals)
    ImU32 gridColor = IM_COL32(60, 60, 60, 120);
    for (int g = 1; g < 4; ++g) {
        f32 t = g * 0.25f;
        drawList->AddLine(
            ImVec2(canvasPos.x + t * canvasSize.x, canvasPos.y),
            ImVec2(canvasPos.x + t * canvasSize.x, canvasPos.y + canvasSize.y),
            gridColor);
        drawList->AddLine(
            ImVec2(canvasPos.x, canvasPos.y + t * canvasSize.y),
            ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + t * canvasSize.y),
            gridColor);
    }

    // Draw axis labels
    drawList->AddText(ImVec2(canvasPos.x + 2, canvasPos.y + canvasSize.y + 2), IM_COL32(150, 150, 150, 200), "0,1");
    drawList->AddText(ImVec2(canvasPos.x + canvasSize.x - 20, canvasPos.y + canvasSize.y + 2), IM_COL32(150, 150, 150, 200), "1,1");
    drawList->AddText(ImVec2(canvasPos.x + 2, canvasPos.y - 14), IM_COL32(150, 150, 150, 200), "0,0");

    ImGui::End();
}

} // namespace Editor
} // namespace Enjin
