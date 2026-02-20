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

void EditorLayer::DrawConsolePanel() {
    ImGui::Begin("Console");

    // Console output
    ImGui::BeginChild("ConsoleOutput", ImVec2(0, -ImGui::GetFrameHeightWithSpacing()), true);
    if (m_ConsoleLog.empty()) {
        DrawEmptyState(">>", "Console Empty", "Messages will appear here");
    }
    for (const auto& line : m_ConsoleLog) {
        ImGui::TextUnformatted(line.c_str());
    }
    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
        ImGui::SetScrollHereY(1.0f);
    }
    ImGui::EndChild();

    // Input line
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
    ImGui::Begin("Asset Browser");

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
        if (bytes < 1024) return std::to_string(bytes) + " B";
        if (bytes < 1024 * 1024) return std::to_string(bytes / 1024) + " KB";
        return std::to_string(bytes / (1024 * 1024)) + " MB";
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

                // Drag source for future drag-to-viewport
                if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
                    ImGui::SetDragDropPayload("ASSET_PATH", entry.fullPath.c_str(), entry.fullPath.size() + 1);
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
    ImGui::Begin("Scene List", nullptr, ImGuiWindowFlags_None);

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
        m_ConsoleLog.push_back("Available commands:");
        m_ConsoleLog.push_back("  help              - Show this help");
        m_ConsoleLog.push_back("  clear             - Clear console");
        m_ConsoleLog.push_back("  list              - List all entities");
        m_ConsoleLog.push_back("  select <id>       - Select entity by ID");
        m_ConsoleLog.push_back("  create <name>     - Create empty entity");
        m_ConsoleLog.push_back("  delete            - Delete selected entity");
        m_ConsoleLog.push_back("  pos <x> <y> <z>   - Set selected entity position");
        m_ConsoleLog.push_back("  wireframe         - Toggle wireframe mode");
        m_ConsoleLog.push_back("  shadows           - Toggle shadows");
        m_ConsoleLog.push_back("  stats             - Show scene statistics");
        m_ConsoleLog.push_back("  save <path>       - Save scene");
        m_ConsoleLog.push_back("  load <path>       - Load scene");
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
    } else {
        m_ConsoleLog.push_back("Unknown command: " + cmd + " (type 'help' for commands)");
    }

    // Trim console log
    while (m_ConsoleLog.size() > MAX_CONSOLE_LINES) {
        m_ConsoleLog.erase(m_ConsoleLog.begin());
    }
}


static void ApplyParticlePreset(ECS::ParticleEmitterComponent& e, const std::string& preset) {
    // Reset pool so particles respawn with new settings
    e.pool.activeCount = 0;
    e.pool.spawnAccumulator = 0.0f;
    e.pool.burstTimer = 0.0f;
    e.pool.systemAge = 0.0f;
    e.isPlaying = true;
    e.loop = true;

    if (preset == "Fire") {
        e.emissionRate = 60.0f;
        e.burstCount = 0;
        e.burstInterval = 0.0f;
        e.lifetime = 1.2f;
        e.lifetimeVariance = 0.3f;
        e.startSpeed = 2.5f;
        e.speedVariance = 0.5f;
        e.startSize = 0.4f;
        e.sizeMid = 0.6f;
        e.endSize = 0.05f;
        e.startColor = Math::Vector3(1.0f, 0.6f, 0.1f);
        e.endColor = Math::Vector3(0.8f, 0.1f, 0.0f);
        e.startAlpha = 0.9f;
        e.endAlpha = 0.0f;
        e.speedMultiplierMid = 0.8f;
        e.speedMultiplierEnd = 0.3f;
        e.shape = ECS::ParticleEmitterComponent::EmitterShape::Cone;
        e.shapeRadius = 0.2f;
        e.coneAngle = 15.0f;
        e.gravity = Math::Vector3(0, 1.5f, 0);
        e.drag = 0.5f;
        e.rotationSpeed = 0.5f;
        e.rotationSpeedVariance = 1.0f;
        e.maxParticles = 512;
    } else if (preset == "Smoke") {
        e.emissionRate = 20.0f;
        e.burstCount = 0;
        e.burstInterval = 0.0f;
        e.lifetime = 3.0f;
        e.lifetimeVariance = 0.5f;
        e.startSpeed = 1.0f;
        e.speedVariance = 0.3f;
        e.startSize = 0.3f;
        e.sizeMid = 0.8f;
        e.endSize = 1.5f;
        e.startColor = Math::Vector3(0.4f, 0.4f, 0.4f);
        e.endColor = Math::Vector3(0.2f, 0.2f, 0.2f);
        e.startAlpha = 0.6f;
        e.endAlpha = 0.0f;
        e.speedMultiplierMid = 0.7f;
        e.speedMultiplierEnd = 0.2f;
        e.shape = ECS::ParticleEmitterComponent::EmitterShape::Cone;
        e.shapeRadius = 0.3f;
        e.coneAngle = 20.0f;
        e.gravity = Math::Vector3(0, 0.5f, 0);
        e.drag = 0.8f;
        e.rotationSpeed = 0.3f;
        e.rotationSpeedVariance = 0.5f;
        e.maxParticles = 256;
    } else if (preset == "Sparks") {
        e.emissionRate = 5.0f;
        e.burstCount = 10;
        e.burstInterval = 0.5f;
        e.lifetime = 0.8f;
        e.lifetimeVariance = 0.3f;
        e.startSpeed = 8.0f;
        e.speedVariance = 3.0f;
        e.startSize = 0.1f;
        e.sizeMid = -1.0f;
        e.endSize = 0.02f;
        e.startColor = Math::Vector3(1.0f, 0.9f, 0.3f);
        e.endColor = Math::Vector3(1.0f, 0.3f, 0.0f);
        e.startAlpha = 1.0f;
        e.endAlpha = 0.0f;
        e.speedMultiplierMid = 0.6f;
        e.speedMultiplierEnd = 0.1f;
        e.shape = ECS::ParticleEmitterComponent::EmitterShape::Point;
        e.shapeRadius = 0.05f;
        e.coneAngle = 30.0f;
        e.gravity = Math::Vector3(0, -9.8f, 0);
        e.drag = 0.2f;
        e.rotationSpeed = 0.0f;
        e.rotationSpeedVariance = 0.0f;
        e.maxParticles = 256;
    } else if (preset == "Snow") {
        e.emissionRate = 40.0f;
        e.burstCount = 0;
        e.burstInterval = 0.0f;
        e.lifetime = 4.0f;
        e.lifetimeVariance = 1.0f;
        e.startSpeed = 0.5f;
        e.speedVariance = 0.2f;
        e.startSize = 0.15f;
        e.sizeMid = -1.0f;
        e.endSize = 0.1f;
        e.startColor = Math::Vector3(1.0f, 1.0f, 1.0f);
        e.endColor = Math::Vector3(0.9f, 0.95f, 1.0f);
        e.startAlpha = 0.8f;
        e.endAlpha = 0.0f;
        e.speedMultiplierMid = 1.0f;
        e.speedMultiplierEnd = 0.8f;
        e.shape = ECS::ParticleEmitterComponent::EmitterShape::Box;
        e.shapeRadius = 5.0f;
        e.coneAngle = 30.0f;
        e.gravity = Math::Vector3(0.3f, -1.5f, 0.1f);
        e.drag = 0.3f;
        e.rotationSpeed = 0.5f;
        e.rotationSpeedVariance = 1.0f;
        e.maxParticles = 512;
    } else if (preset == "Rain") {
        e.emissionRate = 80.0f;
        e.burstCount = 0;
        e.burstInterval = 0.0f;
        e.lifetime = 1.5f;
        e.lifetimeVariance = 0.3f;
        e.startSpeed = 12.0f;
        e.speedVariance = 2.0f;
        e.startSize = 0.05f;
        e.sizeMid = -1.0f;
        e.endSize = 0.03f;
        e.startColor = Math::Vector3(0.5f, 0.6f, 0.8f);
        e.endColor = Math::Vector3(0.4f, 0.5f, 0.7f);
        e.startAlpha = 0.6f;
        e.endAlpha = 0.1f;
        e.speedMultiplierMid = 1.0f;
        e.speedMultiplierEnd = 1.0f;
        e.shape = ECS::ParticleEmitterComponent::EmitterShape::Box;
        e.shapeRadius = 5.0f;
        e.coneAngle = 30.0f;
        e.gravity = Math::Vector3(0.5f, -15.0f, 0);
        e.drag = 0.0f;
        e.rotationSpeed = 0.0f;
        e.rotationSpeedVariance = 0.0f;
        e.maxParticles = 1024;
    } else if (preset == "Magic") {
        e.emissionRate = 30.0f;
        e.burstCount = 5;
        e.burstInterval = 1.0f;
        e.lifetime = 2.0f;
        e.lifetimeVariance = 0.5f;
        e.startSpeed = 1.5f;
        e.speedVariance = 0.5f;
        e.startSize = 0.2f;
        e.sizeMid = 0.35f;
        e.endSize = 0.0f;
        e.startColor = Math::Vector3(0.3f, 0.5f, 1.0f);
        e.endColor = Math::Vector3(0.8f, 0.2f, 1.0f);
        e.startAlpha = 1.0f;
        e.endAlpha = 0.0f;
        e.speedMultiplierMid = 1.2f;
        e.speedMultiplierEnd = 0.4f;
        e.shape = ECS::ParticleEmitterComponent::EmitterShape::Sphere;
        e.shapeRadius = 0.5f;
        e.coneAngle = 30.0f;
        e.gravity = Math::Vector3(0, 0.5f, 0);
        e.drag = 0.3f;
        e.rotationSpeed = 2.0f;
        e.rotationSpeedVariance = 1.5f;
        e.maxParticles = 512;
    } else if (preset == "Explosion") {
        e.emissionRate = 0.0f;
        e.burstCount = 80;
        e.burstInterval = 0.0f;
        e.lifetime = 1.0f;
        e.lifetimeVariance = 0.3f;
        e.startSpeed = 10.0f;
        e.speedVariance = 4.0f;
        e.startSize = 0.5f;
        e.sizeMid = 0.8f;
        e.endSize = 0.1f;
        e.startColor = Math::Vector3(1.0f, 0.8f, 0.2f);
        e.endColor = Math::Vector3(0.3f, 0.1f, 0.0f);
        e.startAlpha = 1.0f;
        e.endAlpha = 0.0f;
        e.speedMultiplierMid = 0.4f;
        e.speedMultiplierEnd = 0.05f;
        e.shape = ECS::ParticleEmitterComponent::EmitterShape::Point;
        e.shapeRadius = 0.1f;
        e.coneAngle = 30.0f;
        e.gravity = Math::Vector3(0, -3.0f, 0);
        e.drag = 1.5f;
        e.rotationSpeed = 3.0f;
        e.rotationSpeedVariance = 3.0f;
        e.loop = false;
        e.maxParticles = 256;
    } else if (preset == "Water Splash") {
        e.emissionRate = 0.0f;
        e.burstCount = 40;
        e.burstInterval = 0.0f;
        e.lifetime = 0.8f;
        e.lifetimeVariance = 0.2f;
        e.startSpeed = 8.0f;
        e.speedVariance = 3.0f;
        e.startSize = 0.15f;
        e.sizeMid = 0.2f;
        e.endSize = 0.05f;
        e.startColor = Math::Vector3(0.4f, 0.7f, 1.0f);
        e.endColor = Math::Vector3(0.2f, 0.5f, 0.9f);
        e.startAlpha = 0.9f;
        e.endAlpha = 0.1f;
        e.speedMultiplierMid = 0.6f;
        e.speedMultiplierEnd = 0.1f;
        e.shape = ECS::ParticleEmitterComponent::EmitterShape::Hemisphere;
        e.shapeRadius = 0.3f;
        e.coneAngle = 45.0f;
        e.gravity = Math::Vector3(0, -12.0f, 0);
        e.drag = 0.3f;
        e.rotationSpeed = 0.0f;
        e.rotationSpeedVariance = 0.0f;
        e.loop = false;
        e.maxParticles = 256;
        e.renderMode = ECS::ParticleEmitterComponent::RenderMode::VelocityStretch;
        e.velocityStretchScale = 1.5f;
    } else if (preset == "Blood/Sap") {
        e.emissionRate = 15.0f;
        e.burstCount = 0;
        e.burstInterval = 0.0f;
        e.lifetime = 1.5f;
        e.lifetimeVariance = 0.4f;
        e.startSpeed = 3.0f;
        e.speedVariance = 1.0f;
        e.startSize = 0.12f;
        e.sizeMid = 0.18f;
        e.endSize = 0.06f;
        e.startColor = Math::Vector3(0.6f, 0.05f, 0.05f);
        e.endColor = Math::Vector3(0.3f, 0.0f, 0.0f);
        e.startAlpha = 1.0f;
        e.endAlpha = 0.3f;
        e.speedMultiplierMid = 0.7f;
        e.speedMultiplierEnd = 0.2f;
        e.shape = ECS::ParticleEmitterComponent::EmitterShape::Cone;
        e.shapeRadius = 0.05f;
        e.coneAngle = 20.0f;
        e.gravity = Math::Vector3(0, -6.0f, 0);
        e.drag = 2.0f;
        e.rotationSpeed = 0.0f;
        e.rotationSpeedVariance = 0.0f;
        e.maxParticles = 256;
        e.renderMode = ECS::ParticleEmitterComponent::RenderMode::VelocityStretch;
        e.velocityStretchScale = 2.0f;
    } else if (preset == "Lava") {
        e.emissionRate = 8.0f;
        e.burstCount = 0;
        e.burstInterval = 0.0f;
        e.lifetime = 3.0f;
        e.lifetimeVariance = 0.8f;
        e.startSpeed = 2.0f;
        e.speedVariance = 0.8f;
        e.startSize = 0.6f;
        e.sizeMid = 0.8f;
        e.endSize = 0.3f;
        e.startColor = Math::Vector3(1.0f, 0.5f, 0.0f);
        e.endColor = Math::Vector3(0.4f, 0.05f, 0.0f);
        e.startAlpha = 1.0f;
        e.endAlpha = 0.6f;
        e.speedMultiplierMid = 0.5f;
        e.speedMultiplierEnd = 0.1f;
        e.shape = ECS::ParticleEmitterComponent::EmitterShape::Hemisphere;
        e.shapeRadius = 0.5f;
        e.coneAngle = 30.0f;
        e.gravity = Math::Vector3(0, -2.0f, 0);
        e.drag = 1.0f;
        e.rotationSpeed = 0.2f;
        e.rotationSpeedVariance = 0.3f;
        e.maxParticles = 256;
        e.renderMode = ECS::ParticleEmitterComponent::RenderMode::Billboard;
        e.velocityStretchScale = 0.0f;
    } else if (preset == "Fountain") {
        e.emissionRate = 40.0f;
        e.burstCount = 0;
        e.burstInterval = 0.0f;
        e.lifetime = 2.0f;
        e.lifetimeVariance = 0.3f;
        e.startSpeed = 10.0f;
        e.speedVariance = 2.0f;
        e.startSize = 0.1f;
        e.sizeMid = 0.15f;
        e.endSize = 0.05f;
        e.startColor = Math::Vector3(0.6f, 0.85f, 1.0f);
        e.endColor = Math::Vector3(0.3f, 0.6f, 0.9f);
        e.startAlpha = 0.8f;
        e.endAlpha = 0.0f;
        e.speedMultiplierMid = 0.5f;
        e.speedMultiplierEnd = 0.1f;
        e.shape = ECS::ParticleEmitterComponent::EmitterShape::Cone;
        e.shapeRadius = 0.1f;
        e.coneAngle = 10.0f;
        e.gravity = Math::Vector3(0, -9.8f, 0);
        e.drag = 0.2f;
        e.rotationSpeed = 0.0f;
        e.rotationSpeedVariance = 0.0f;
        e.maxParticles = 512;
        e.renderMode = ECS::ParticleEmitterComponent::RenderMode::VelocityStretch;
        e.velocityStretchScale = 1.2f;
    } else if (preset == "Drip") {
        e.emissionRate = 2.0f;
        e.burstCount = 0;
        e.burstInterval = 0.0f;
        e.lifetime = 2.5f;
        e.lifetimeVariance = 0.5f;
        e.startSpeed = 0.5f;
        e.speedVariance = 0.2f;
        e.startSize = 0.1f;
        e.sizeMid = 0.15f;
        e.endSize = 0.08f;
        e.startColor = Math::Vector3(0.3f, 0.6f, 0.9f);
        e.endColor = Math::Vector3(0.2f, 0.4f, 0.7f);
        e.startAlpha = 0.9f;
        e.endAlpha = 0.2f;
        e.speedMultiplierMid = 1.5f;
        e.speedMultiplierEnd = 2.0f;
        e.shape = ECS::ParticleEmitterComponent::EmitterShape::Point;
        e.shapeRadius = 0.02f;
        e.coneAngle = 5.0f;
        e.gravity = Math::Vector3(0, -9.8f, 0);
        e.drag = 0.5f;
        e.rotationSpeed = 0.0f;
        e.rotationSpeedVariance = 0.0f;
        e.maxParticles = 64;
        e.renderMode = ECS::ParticleEmitterComponent::RenderMode::VelocityStretch;
        e.velocityStretchScale = 2.5f;
    }
}

void EditorLayer::DrawParticleEditorPanel() {
    if (!ImGui::Begin("Particle Editor")) {
        ImGui::End();
        return;
    }

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
                ApplyParticlePreset(*emitter, presets[i]);
            }
        }
        ImGui::Spacing();
        ImGui::TextDisabled("Liquids:");
        const char* liquidPresets[] = { "Water Splash", "Blood/Sap", "Lava", "Fountain", "Drip" };
        for (int i = 0; i < 5; ++i) {
            if (i > 0) ImGui::SameLine();
            if (ImGui::Button(liquidPresets[i])) {
                ApplyParticlePreset(*emitter, liquidPresets[i]);
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
    if (!ImGui::Begin("Animation Graph")) {
        ImGui::End();
        return;
    }

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
    if (!ImGui::Begin("Dialogue Editor")) {
        ImGui::End();
        return;
    }

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
    if (!ImGui::Begin("Visual Script Editor")) {
        ImGui::End();
        return;
    }

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
    if (!ImGui::Begin("Pixel Editor")) {
        ImGui::End();
        return;
    }

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
    if (!ImGui::Begin("Sprite Sheet Importer")) {
        ImGui::End();
        return;
    }

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
    if (!ImGui::Begin("Behavior Tree")) {
        ImGui::End();
        return;
    }

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
    if (!ImGui::Begin("Quest Flow")) {
        ImGui::End();
        return;
    }

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
    if (!ImGui::Begin("User Manual", nullptr)) {
        ImGui::End();
        return;
    }

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
    if (!ImGui::Begin("Data Asset Editor", nullptr)) {
        ImGui::End();
        return;
    }

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
    if (!ImGui::Begin("Plugin Browser", nullptr)) {
        ImGui::End();
        return;
    }

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

    if (!ImGui::Begin("Procedural Generation", nullptr)) {
        ImGui::End();
        return;
    }

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
    if (!ImGui::Begin("Network", nullptr, ImGuiWindowFlags_None)) {
        ImGui::End();
        return;
    }

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
    if (!ImGui::Begin("Collaboration", nullptr, ImGuiWindowFlags_None)) {
        ImGui::End();
        return;
    }

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
        "Open the bug report form",
        [this]() {
            SetPanelVisibility(EditorPanel::FeedbackPanel, true);
            m_FeedbackTab = FeedbackTab::NewBug;
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
    if (!ImGui::Begin("Flash Timeline")) {
        ImGui::End();
        return;
    }

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
    if (!ImGui::Begin("Vector Drawing")) {
        ImGui::End();
        return;
    }

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
            auto& report = m_FeedbackManager.CreateBugReport();
            report.title = m_BugTitleBuf;
            report.type = static_cast<ReportType>(m_BugTypeSel);
            report.severity = static_cast<ReportSeverity>(m_BugSeveritySel);
            report.description = m_BugDescriptionBuf;
            report.stepsToReproduce = m_BugStepsBuf;
            report.expectedBehavior = m_BugExpectedBuf;
            report.actualBehavior = m_BugActualBuf;
            report.diagnostics = CaptureDiagnostics(m_BugIncludeScene);
            if (!m_BugIncludeLogs) report.diagnostics.consoleLogTail.clear();
            report.status = ReportStatus::Draft;
            m_FeedbackManager.SaveAll();
            m_ConsoleLog.push_back("[Feedback] Created bug report #" + std::to_string(report.id) + ": " + report.title);
            ResetBugReportForm();
        }
    }
    ImGui::SameLine();
    if (!titleEmpty && ImGui::Button("Save Draft")) {
        auto& report = m_FeedbackManager.CreateBugReport();
        report.title = m_BugTitleBuf;
        report.type = static_cast<ReportType>(m_BugTypeSel);
        report.severity = static_cast<ReportSeverity>(m_BugSeveritySel);
        report.description = m_BugDescriptionBuf;
        report.stepsToReproduce = m_BugStepsBuf;
        report.expectedBehavior = m_BugExpectedBuf;
        report.actualBehavior = m_BugActualBuf;
        report.diagnostics = CaptureDiagnostics(m_BugIncludeScene);
        if (!m_BugIncludeLogs) report.diagnostics.consoleLogTail.clear();
        report.status = ReportStatus::Draft;
        m_FeedbackManager.SaveAll();
        m_ConsoleLog.push_back("[Feedback] Saved draft bug report #" + std::to_string(report.id));
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
            auto& entry = m_FeedbackManager.CreateFeedback();
            entry.title = m_FeedbackTitleBuf;
            entry.type = static_cast<FeedbackType>(m_FeedbackTypeSel);
            entry.priority = static_cast<FeedbackPriority>(m_FeedbackPrioritySel);
            entry.satisfaction = static_cast<SatisfactionRating>(m_FeedbackSatisfaction);
            entry.description = m_FeedbackDescBuf;
            entry.category = m_FeedbackCategoryBuf;
            entry.includeDiagnostics = m_FeedbackIncludeDiag;
            if (m_FeedbackIncludeDiag) {
                entry.diagnostics = CaptureDiagnostics(false);
            }
            m_FeedbackManager.SaveAll();
            m_ConsoleLog.push_back("[Feedback] Created feedback #" + std::to_string(entry.id) + ": " + entry.title);
            ResetFeedbackForm();
        }
    }
}

// ============================================================================
// AUDIO MIXER WINDOW
// ============================================================================

void EditorLayer::DrawAudioMixer() {
    ImGui::SetNextWindowSize(ImVec2(520, 460), ImGuiCond_FirstUseEver);
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

    // --- Master / Channel volume strip at top ---
    {
        f32 masterVol = audio ? audio->GetMasterVolume() : 1.0f;
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Master");
        ImGui::SameLine(70);
        ImGui::SetNextItemWidth(-1);
        if (ImGui::SliderFloat("##MasterVol", &masterVol, 0.0f, 1.0f, "%.2f")) {
            if (audio) audio->SetMasterVolume(masterVol);
        }
    }

    ImGui::Separator();

    // Channel volume strips
    static const char* channelNames[] = {"SFX", "Music", "UI", "Voice"};
    static const ImVec4 channelColors[] = {
        {0.3f, 0.7f, 0.4f, 1.0f},  // SFX — green
        {0.5f, 0.4f, 0.9f, 1.0f},  // Music — purple
        {0.2f, 0.6f, 0.9f, 1.0f},  // UI — blue
        {0.9f, 0.6f, 0.2f, 1.0f},  // Voice — orange
    };

    for (int ch = 0; ch < 4; ch++) {
        auto channel = static_cast<Audio::AudioChannel>(ch);
        f32 chVol = audio ? audio->GetChannelVolume(channel) : 1.0f;

        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, channelColors[ch]);
        ImGui::AlignTextToFramePadding();
        ImGui::Text("%-6s", channelNames[ch]);
        ImGui::SameLine(70);
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 50);
        char sliderId[32];
        snprintf(sliderId, sizeof(sliderId), "##ChVol%d", ch);
        if (ImGui::SliderFloat(sliderId, &chVol, 0.0f, 1.0f, "%.2f")) {
            if (audio) audio->SetChannelVolume(channel, chVol);
        }
        ImGui::SameLine();
        char muteBtnId[32];
        snprintf(muteBtnId, sizeof(muteBtnId), "M##Mute%d", ch);
        if (ImGui::SmallButton(muteBtnId)) {
            if (audio) audio->SetChannelVolume(channel, chVol > 0.0f ? 0.0f : 1.0f);
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Mute/unmute %s channel", channelNames[ch]);
        ImGui::PopStyleColor();
    }

    ImGui::Separator();

    // --- Channel filter tabs ---
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

} // namespace Editor
} // namespace Enjin
