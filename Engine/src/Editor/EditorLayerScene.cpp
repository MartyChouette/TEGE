#include "Enjin/Editor/EditorLayer.h"
#include "Enjin/Editor/DropImport.h"
#include "Enjin/Assets/AssetPipeline.h"
#include "Enjin/Editor/InspectorUndo.h"
#include "Enjin/Editor/ScenePicker.h"
#include "Enjin/Core/Version.h"
#include "Enjin/Scene/SceneAssetValidator.h"
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

// ---------------------------------------------------------------------------
// Project-first workflow: auto-detect or auto-create a project
// ---------------------------------------------------------------------------

void EditorLayer::AutoDetectProjectForScene(const std::string& scenePath) {
    if (scenePath.empty()) return;

    namespace fs = std::filesystem;
    fs::path sceneFile(scenePath);
    if (!fs::exists(sceneFile)) return;

    // A project is already loaded. Make sure this scene actually belongs to it — opening
    // a scene from a DIFFERENT project used to keep the old script/asset roots silently,
    // so the scene's scripts and assets would fail to resolve with no explanation. Warn.
    // (We do not auto-switch projects here: that could discard unsaved work in this one.)
    if (!m_SceneManager.GetProjectPath().empty()) {
        fs::path curRoot = fs::path(m_SceneManager.GetProjectPath()).parent_path().lexically_normal();
        fs::path rel = sceneFile.lexically_normal().lexically_relative(curRoot);
        bool underCurrent = !rel.empty() && rel.string().rfind("..", 0) != 0;
        if (!underCurrent) {
            fs::path dir = sceneFile.parent_path();
            for (int depth = 0; depth < 3 && !dir.empty(); ++depth) {
                std::error_code ec;
                for (auto& entry : fs::directory_iterator(dir, ec)) {
                    if (entry.path().extension() == ".enjinproject" && entry.is_regular_file()) {
                        ShowNotification("This scene belongs to project '" +
                            entry.path().stem().string() +
                            "'. Open that project so its scripts and assets resolve.",
                            NotificationType::Warning);
                        ENJIN_LOG_WARN(Editor, "Opened scene '%s' from a different project than the loaded '%s' — its scripts/assets may not resolve until you open it",
                            scenePath.c_str(), m_SceneManager.GetProjectName().c_str());
                        return;
                    }
                }
                fs::path parent = dir.parent_path();
                if (parent == dir) break;
                dir = parent;
            }
        }
        return;  // scene is under the current project (or has no project of its own)
    }

    // Walk up from scene directory looking for a .enjinproject file
    fs::path dir = sceneFile.parent_path();
    for (int depth = 0; depth < 3 && !dir.empty(); ++depth) {
        std::error_code ec;
        for (auto& entry : fs::directory_iterator(dir, ec)) {
            if (entry.path().extension() == ".enjinproject" && entry.is_regular_file()) {
                if (m_SceneManager.LoadProject(entry.path().string())) {
                    MigrateEditorSettingsToProject();
                    ENJIN_LOG_INFO(Editor, "Auto-detected project: %s",
                                   m_SceneManager.GetProjectName().c_str());
                    m_EditorSettings.AddRecentProject(entry.path().string());
                    m_EditorSettings.lastProjectDir = dir.parent_path().string();
                    m_EditorSettings.Save();
                    ShowNotification("Loaded project '" + m_SceneManager.GetProjectName() + "'",
                                    NotificationType::Info);
                    return;
                }
            }
        }
        fs::path parent = dir.parent_path();
        if (parent == dir) break;  // filesystem root
        dir = parent;
    }

    // No project found — auto-create one from the scene
    EnsureProjectForScene(scenePath);
}

void EditorLayer::EnsureProjectForScene(const std::string& scenePath) {
    // Already have a loaded project — nothing to do
    if (!m_SceneManager.GetProjectPath().empty()) return;
    if (scenePath.empty()) return;

    namespace fs = std::filesystem;
    fs::path sceneFile(scenePath);
    fs::path sceneDir = sceneFile.parent_path();

    // Derive project name from the scene filename (e.g. "Level1.enjin" -> "Level1")
    std::string projName = sceneFile.stem().string();
    if (projName.empty()) projName = "MyGame";

    // Determine the project root: if scene is inside a "scenes" folder, go up one
    // level.  Otherwise use the scene's directory directly.
    fs::path projRoot = sceneDir;
    if (sceneDir.filename() == "scenes") {
        projRoot = sceneDir.parent_path();
    }

    // Create directory structure
    std::error_code ec;
    fs::create_directories(projRoot / "scenes", ec);
    fs::create_directories(projRoot / "assets", ec);
    fs::create_directories(projRoot / "scripts", ec);

    // Ensure scene file lives in projRoot/scenes/ (not the root).
    // If it's in the root, move it into scenes/ so the manifest path matches.
    fs::path scenesDir = projRoot / "scenes";
    fs::path expectedScenePath = scenesDir / sceneFile.filename();
    if (sceneFile.parent_path() != scenesDir && fs::exists(sceneFile)) {
        std::error_code moveEc;
        fs::rename(sceneFile, expectedScenePath, moveEc);
        if (!moveEc) {
            // Update the current scene path to the new location
            m_CurrentScenePath = expectedScenePath.string();
        }
    }
    std::string relPath = "scenes/" + sceneFile.filename().string();

    // Initialize project
    m_SceneManager.NewProject(projName);
    m_SceneManager.AddScene(sceneFile.stem().string(), relPath);
    m_SceneManager.SetStartScene(0);
    m_SceneManager.SetProjectMode(Scene::ProjectMode::Mixed);

    // Save manifest
    fs::path manifestPath = projRoot / (projName + ".enjinproject");
    if (m_SceneManager.SaveProject(manifestPath.string())) {
        m_EditorSettings.AddRecentProject(manifestPath.string());
        m_EditorSettings.lastProjectDir = projRoot.parent_path().string();
        m_EditorSettings.Save();
        ShowNotification("Created project '" + projName + "'", NotificationType::Success);
        ENJIN_LOG_INFO(Editor, "Auto-created project '%s' at %s",
                       projName.c_str(), projRoot.string().c_str());
    }
}

// ---------------------------------------------------------------------------

void EditorLayer::OpenProjectFromPath(const std::string& projectPath) {
    try {
        namespace fs = std::filesystem;

        if (projectPath.empty()) {
            ShowNotification("Empty project path", NotificationType::Error);
            return;
        }
        if (!fs::exists(projectPath)) {
            ShowNotification("Project file not found", NotificationType::Error);
            ENJIN_LOG_ERROR(Editor, "Cannot open project: '%s' does not exist", projectPath.c_str());
            m_EditorSettings.RemoveRecentProject(projectPath);
            m_EditorSettings.Save();
            return;
        }

        ENJIN_LOG_INFO(Editor, "Loading project: %s", projectPath.c_str());

        if (!m_SceneManager.LoadProject(projectPath)) {
            ShowNotification("Failed to load project", NotificationType::Error);
            ENJIN_LOG_ERROR(Editor, "LoadProject failed for '%s'", projectPath.c_str());
            return;
        }

        MigrateEditorSettingsToProject();
        m_EditorSettings.AddRecentProject(projectPath);
        fs::path projFile(projectPath);
        if (projFile.has_parent_path())
            m_EditorSettings.lastProjectDir = projFile.parent_path().string();
        m_EditorSettings.Save();

        // Open the first scene in the project
        auto scenes = m_SceneManager.GetScenes(); // Copy to avoid reference invalidation
        if (!scenes.empty()) {
            fs::path projDir = projFile.parent_path();
            fs::path scenePath = projDir / scenes[0].path;
            if (fs::exists(scenePath)) {
                OpenScene(scenePath.string());
            } else {
                ENJIN_LOG_WARN(Editor, "Scene not found: %s", scenePath.string().c_str());
                ShowNotification("Scene not found: " + scenes[0].path, NotificationType::Warning);
            }
        }

        m_ShowProjectHub = false;
        ENJIN_LOG_INFO(Editor, "Opened project: %s", projectPath.c_str());
    } catch (const std::exception& e) {
        ENJIN_LOG_ERROR(Editor, "Exception opening project: %s", e.what());
        ShowNotification("Error opening project", NotificationType::Error);
    } catch (...) {
        ENJIN_LOG_ERROR(Editor, "Unknown exception opening project");
        ShowNotification("Error opening project", NotificationType::Error);
    }
}

// ---------------------------------------------------------------------------

void EditorLayer::SaveScene(const std::string& path) {
    if (!m_World) {
        ENJIN_LOG_ERROR(Editor, "Cannot save scene: no world loaded");
        m_ConsoleLog.push_back("[Error] Cannot save scene: no world loaded");
        return;
    }

    if (path.empty()) {
        ENJIN_LOG_ERROR(Editor, "Cannot save scene: path is empty");
        m_ConsoleLog.push_back("[Error] Cannot save scene: path is empty");
        return;
    }

    Scene::SceneSerializer serializer(m_World);
    if (m_RenderSystem) {
        serializer.SetSkyboxConfig(m_RenderSystem->GetSkyboxConfig());
        serializer.SetWater2DConfig(m_RenderSystem->GetWater2DConfig());
    }
    serializer.SetContentFlags(m_SceneContentFlags);

    // Capture current render settings for serialization
    auto renderSettings = Renderer::SceneRenderSettings::CaptureFromRuntime(
        m_RenderSystem, m_PostProcessing ? &m_PostProcessing->GetSettings() : nullptr);
    renderSettings.useProjectDefaults = m_CurrentSceneUsesProjectDefaults;
    serializer.SetRenderSettings(renderSettings);

    auto result = serializer.Save(path);

    if (result.success) {
        m_CurrentScenePath = path;
        ClearDirty();
        RecordOpenSceneDiskTime();  // we just wrote it — this is the new baseline

        // Auto-create a project if none is loaded (project-first workflow)
        EnsureProjectForScene(path);

        // Delete any auto-save file after a successful save
        std::string autoSavePath = path + ".autosave";
        std::error_code ec;
        std::filesystem::remove(autoSavePath, ec);

        // VWS: persist the layer session beside the scene. NOTE the .enjin just
        // written holds the RESOLVED world (live state), not the pristine base —
        // after a save+reopen, disabling a layer restores the values baked at
        // this save, not the pre-layer originals. Base-vs-resolved save
        // semantics are a deliberate open design item.
        if (!m_LayerSystem.Stack().layers.empty()) {
            if (m_LayerSystem.SaveLayers(path + ".layers")) {
                m_ConsoleLog.push_back("[Info] Saved " +
                    std::to_string(m_LayerSystem.Stack().layers.size()) + " layer(s) beside the scene");
            }
        }

        usize entityCount = m_World->GetEntityCount();
        std::stringstream ss;
        ss << "[Info] Saved scene to " << path << " (" << entityCount << " entities)";
        m_ConsoleLog.push_back(ss.str());
        ENJIN_LOG_INFO(Editor, "Saved scene to %s (%zu entities)", path.c_str(), entityCount);

        // Warn about asset references that don't resolve on disk. Resolution
        // order mirrors the build pipeline: project root, then scene directory
        // (covers standalone scenes saved without a project).
        std::vector<std::string> searchRoots;
        if (!m_SceneManager.GetProjectPath().empty()) {
            searchRoots.push_back(std::filesystem::path(m_SceneManager.GetProjectPath()).parent_path().string());
        }
        searchRoots.push_back(std::filesystem::path(path).parent_path().string());
        result.warnings = Scene::FindMissingAssetPaths(m_World, searchRoots);

        std::string filename = std::filesystem::path(path).filename().string();
        if (!result.warnings.empty()) {
            for (const auto& warning : result.warnings) {
                m_ConsoleLog.push_back("[Warning] " + warning);
                ENJIN_LOG_WARN(Editor, "%s", warning.c_str());
            }
            ShowNotification("Scene saved: " + filename + " (" +
                std::to_string(result.warnings.size()) + " missing asset reference(s), see Console)",
                NotificationType::Warning);
        } else {
            ShowNotification("Scene saved: " + filename, NotificationType::Success);
        }

        // Track in recent projects
        m_EditorSettings.AddRecentProject(path);
        m_EditorSettings.Save();
    } else {
        std::stringstream ss;
        ss << "[Error] Failed to save scene: " << result.error;
        m_ConsoleLog.push_back(ss.str());
        ENJIN_LOG_ERROR(Editor, "Failed to save scene to %s: %s", path.c_str(), result.error.c_str());
        ShowNotification("Failed to save scene: " + result.error, NotificationType::Error);
    }
}

void EditorLayer::OpenScene(const std::string& path) {
    // Guard: a scene resolves its scripts and assets against the open project's
    // root, so loading a scene that belongs to a DIFFERENT project silently
    // breaks every path. If we detect that, prompt the user to switch projects
    // instead of quietly loading it wrong.
    std::string owning = FindMismatchedProjectForScene(path);
    if (!owning.empty()) {
        m_WrongProjectScenePath = path;
        m_WrongProjectManifest = owning;
        m_ShowWrongProjectDialog = true;
        return;
    }

    // Defer to Update phase — World::Clear() must not run during Render
    // to avoid invalidating entity references used by the current frame.
    m_PendingSceneLoadPath = path;
}

std::string EditorLayer::FindMismatchedProjectForScene(const std::string& scenePath) {
    namespace fs = std::filesystem;
    if (scenePath.empty()) return {};
    // No project loaded yet: let AutoDetectProjectForScene adopt/create one.
    if (m_SceneManager.GetProjectPath().empty()) return {};

    std::error_code ec;
    fs::path sceneFile = fs::path(scenePath).lexically_normal();
    if (!fs::exists(sceneFile, ec)) return {};

    fs::path curManifest = fs::path(m_SceneManager.GetProjectPath()).lexically_normal();
    fs::path curRoot = curManifest.parent_path();

    // Under the current project root → fine.
    fs::path rel = sceneFile.lexically_relative(curRoot);
    if (!rel.empty() && rel.string().rfind("..", 0) != 0) return {};

    // Walk up from the scene looking for its owning .enjinproject.
    fs::path dir = sceneFile.parent_path();
    for (int depth = 0; depth < 4 && !dir.empty(); ++depth) {
        for (auto& entry : fs::directory_iterator(dir, ec)) {
            if (ec) break;
            if (entry.path().extension() == ".enjinproject" && entry.is_regular_file(ec)) {
                fs::path found = entry.path().lexically_normal();
                // Belongs to a different project → mismatch. Its own project is
                // the current one → fine.
                return (found != curManifest) ? found.string() : std::string{};
            }
        }
        fs::path parent = dir.parent_path();
        if (parent == dir) break;  // filesystem root
        dir = parent;
    }
    // No owning project found (orphan scene): keep the current project. Low
    // risk, and we must not fabricate a project switch out of nothing.
    return {};
}

void EditorLayer::RecordOpenSceneDiskTime() {
    m_HasOpenSceneDiskTime = false;
    if (m_CurrentScenePath.empty()) return;
    std::error_code ec;
    auto t = std::filesystem::last_write_time(m_CurrentScenePath, ec);
    if (!ec) {
        m_OpenSceneDiskTime = t;
        m_HasOpenSceneDiskTime = true;
    }
    m_SceneWatchTimer = 0.0f;
}

void EditorLayer::CheckExternalSceneChange(f32 deltaTime) {
    // Only meaningful in edit mode with a known baseline. Play doesn't write the
    // scene file, and we don't want to interrupt it.
    if (!m_HasOpenSceneDiskTime || m_CurrentScenePath.empty()) return;
    if (!m_PlayMode.IsStopped()) return;
    if (m_ShowExternalSceneChangeDialog) return;  // already prompting

    m_SceneWatchTimer += deltaTime;
    if (m_SceneWatchTimer < 1.0f) return;  // throttle disk stat to ~1 Hz
    m_SceneWatchTimer = 0.0f;

    std::error_code ec;
    if (!std::filesystem::exists(m_CurrentScenePath, ec)) return;
    auto t = std::filesystem::last_write_time(m_CurrentScenePath, ec);
    if (ec) return;
    if (t != m_OpenSceneDiskTime) {
        m_ShowExternalSceneChangeDialog = true;
    }
}

void EditorLayer::DrawExternalSceneChangeDialog() {
    const char* kPopup = "Scene Changed on Disk##extchg";
    if (m_ShowExternalSceneChangeDialog && !ImGui::IsPopupOpen(kPopup)) {
        ImGui::OpenPopup(kPopup);
    }

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal(kPopup, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        std::string sceneName = std::filesystem::path(m_CurrentScenePath).filename().string();
        ImGui::Text("\"%s\" was modified on disk outside the editor.", sceneName.c_str());
        ImGui::TextWrapped("Another tool, a git operation, or a second editor changed the file. "
                           "If you keep editing and save, your version overwrites theirs.");
        if (m_SceneDirty) {
            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.72f, 0.2f, 1.0f));
            ImGui::TextWrapped("Reloading discards the unsaved changes you have in the editor.");
            ImGui::PopStyleColor();
        }
        ImGui::Separator();
        if (ImGui::Button("Reload from Disk", ImVec2(160, 0))) {
            m_ShowExternalSceneChangeDialog = false;
            ImGui::CloseCurrentPopup();
            OpenScene(m_CurrentScenePath);  // reloads; re-baselines mtime on load
        }
        ImGui::SameLine();
        if (ImGui::Button("Keep My Version", ImVec2(150, 0))) {
            m_ShowExternalSceneChangeDialog = false;
            ImGui::CloseCurrentPopup();
            RecordOpenSceneDiskTime();  // accept theirs as the new baseline; stop nagging
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Keep editing. Your next Save overwrites the on-disk version.");
        ImGui::EndPopup();
    } else if (m_ShowExternalSceneChangeDialog) {
        // Dismissed (Escape) without choosing — treat as "keep mine" so we don't
        // reopen every frame, and re-baseline to the on-disk mtime.
        m_ShowExternalSceneChangeDialog = false;
        RecordOpenSceneDiskTime();
    }
}

void EditorLayer::DrawWrongProjectDialog() {
    if (m_ShowWrongProjectDialog) {
        ImGui::OpenPopup("Different Project##wrongproj");
        m_ShowWrongProjectDialog = false;
    }

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("Different Project##wrongproj", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        std::string projName = std::filesystem::path(m_WrongProjectManifest).stem().string();
        std::string sceneName = std::filesystem::path(m_WrongProjectScenePath).filename().string();

        ImGui::Text("\"%s\" belongs to a different project.", sceneName.c_str());
        ImGui::Separator();
        ImGui::Text("Its project:   %s", projName.c_str());
        ImGui::Text("Open project:  %s", m_SceneManager.GetProjectName().c_str());
        ImGui::Spacing();
        ImGui::TextWrapped("Loading it under the open project would break its scripts and "
                           "assets, because those resolve against the project root.");

        if (m_SceneDirty) {
            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.72f, 0.2f, 1.0f));
            ImGui::TextWrapped("The current scene has unsaved changes that switching will discard.");
            ImGui::PopStyleColor();
        }

        ImGui::Separator();
        if (ImGui::Button("Switch Project & Open", ImVec2(190, 0))) {
            std::string manifest = m_WrongProjectManifest;
            std::string scene = m_WrongProjectScenePath;
            ImGui::CloseCurrentPopup();
            if (m_SceneManager.LoadProject(manifest)) {
                MigrateEditorSettingsToProject();
                m_EditorSettings.AddRecentProject(manifest);
                m_EditorSettings.lastProjectDir =
                    std::filesystem::path(manifest).parent_path().parent_path().string();
                m_EditorSettings.Save();
                ShowNotification("Switched to project '" + m_SceneManager.GetProjectName() + "'",
                                 NotificationType::Info);
                m_PendingSceneLoadPath = scene;  // now loads under the correct project
            } else {
                ShowNotification("Failed to load project '" + projName + "'",
                                 NotificationType::Error);
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Open Anyway", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
            m_PendingSceneLoadPath = m_WrongProjectScenePath;  // override; paths may not resolve
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Load the scene under the CURRENT project.\n"
                              "Its scripts and assets may not resolve.");
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(100, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void EditorLayer::OpenSceneImmediate(const std::string& path) {
    if (!m_World) {
        ENJIN_LOG_ERROR(Editor, "Cannot open scene: no world loaded");
        return;
    }
    if (path.empty()) {
        ENJIN_LOG_ERROR(Editor, "Cannot open scene: path is empty");
        return;
    }

    ENJIN_LOG_INFO(Editor, "Opening scene: %s", path.c_str());

    // Tell the render system the world is being torn down BEFORE the load clears
    // it. serializer.Load(path, true) destroys every component storage; without
    // this the render system's cached storage pointers dangle (ApplyTemplate does
    // this, the scene-open path forgot to).
    if (m_RenderSystem) m_RenderSystem->OnSceneClear();

    Scene::SceneSerializer serializer(m_World);
    Scene::DeserializationResult result;
    try {
        result = serializer.Load(path, true); // Clear existing entities
    } catch (const std::exception& e) {
        ENJIN_LOG_ERROR(Editor, "Exception loading scene '%s': %s", path.c_str(), e.what());
        ShowNotification("Failed to load scene", NotificationType::Error);
        return;
    } catch (...) {
        ENJIN_LOG_ERROR(Editor, "Unknown exception loading scene '%s'", path.c_str());
        ShowNotification("Failed to load scene", NotificationType::Error);
        return;
    }

    // Apply loaded skybox config
    if (result.success && m_RenderSystem) {
        m_RenderSystem->SetSkybox(serializer.GetSkyboxConfig());
        m_RenderSystem->SetWater2D(serializer.GetWater2DConfig());
    }

    // Adopt the scene's content warning flags (authored in Settings > Scene)
    if (result.success) {
        m_SceneContentFlags = serializer.GetContentFlags();
    }

    // Apply loaded render settings
    if (result.success) {
        const auto& loaded = serializer.GetRenderSettings();
        m_CurrentSceneUsesProjectDefaults = loaded.useProjectDefaults;
        if (loaded.useProjectDefaults) {
            m_SceneManager.GetDefaultRenderSettings().ApplyToRuntime(
                m_RenderSystem, m_PostProcessing ? &m_PostProcessing->GetSettings() : nullptr);
        } else {
            loaded.ApplyToRuntime(
                m_RenderSystem, m_PostProcessing ? &m_PostProcessing->GetSettings() : nullptr);
        }
    }

    if (result.success) {
        m_CurrentScenePath = path;
        ClearSelection();
        m_UndoRedo.Clear();
        ClearDirty();
        UpdateWindowTitle();
        RecordOpenSceneDiskTime();  // baseline the file mtime for external-edit detection

        // Auto-detect or auto-create a project (project-first workflow)
        AutoDetectProjectForScene(path);

        // Initialize scene lock manager for this scene
        m_SceneLockManager.SetScenePath(path);

        // VWS: a newly opened scene is a new pristine base — reset the layer
        // session so Rebuild can never resurrect the PREVIOUS scene's base.
        // The base is the file text itself (byte-identical to disk), not a
        // re-serialization of the world.
        {
            ResetLayerSession();
            std::ifstream baseFile(path, std::ios::binary);
            std::string baseText((std::istreambuf_iterator<char>(baseFile)), std::istreambuf_iterator<char>());
            m_LayerSystem.SetBaseScene(baseText);

            // A layer session saved beside the scene reopens automatically.
            // The .layers directory only exists if the user saved one, so
            // auto-resume is opt-in by construction.
            std::string layerDir = path + ".layers";
            std::error_code lec;
            if (std::filesystem::is_directory(layerDir, lec) && !lec) {
                int n = m_LayerSystem.LoadLayers(layerDir);
                if (n > 0) {
                    auto lr = m_LayerSystem.ResolveIntoWorld();
                    if (lr.success) {
                        m_ConsoleLog.push_back("[Info] Resumed layer session (" + std::to_string(n) + " layer(s))");
                        ENJIN_LOG_INFO(Editor, "Resumed %d layer(s) from '%s'", n, layerDir.c_str());
                    } else {
                        // Fall back to the plain base world the serializer just
                        // loaded; the layers stay in the panel for inspection.
                        ENJIN_LOG_WARN(Editor, "Layer session resolve failed: %s", lr.error.c_str());
                    }
                }
            }
        }

        // Check for auto-save file newer than the scene file
        std::string autoSavePath = path + ".autosave";
        std::error_code ec;
        if (std::filesystem::exists(autoSavePath, ec)) {
            auto sceneTime = std::filesystem::last_write_time(path, ec);
            auto autoTime = std::filesystem::last_write_time(autoSavePath, ec);
            if (autoTime > sceneTime) {
                m_AutoSaveRecoveryPath = autoSavePath;
                m_ShowAutoSaveRecoveryDialog = true;
                ENJIN_LOG_INFO(Editor, "Auto-save recovery available: %s", autoSavePath.c_str());
            }
        }

        usize entityCount = result.entities.size();
        std::stringstream ss;
        ss << "[Info] Loaded scene from " << path << " (" << entityCount << " entities)";
        m_ConsoleLog.push_back(ss.str());
        ENJIN_LOG_INFO(Editor, "Loaded scene from %s (%zu entities)", path.c_str(), entityCount);
        m_Telemetry.TrackSceneLoaded();

        // Track in recent projects
        m_EditorSettings.AddRecentProject(path);
        m_EditorSettings.Save();
    } else {
        std::stringstream ss;
        ss << "[Error] Failed to load scene: " << result.error;
        m_ConsoleLog.push_back(ss.str());
        ENJIN_LOG_ERROR(Editor, "Failed to load scene from %s: %s", path.c_str(), result.error.c_str());
        ShowNotification("Failed to load scene: " + std::filesystem::path(path).filename().string(), NotificationType::Error);
    }
}

// Drag-and-drop entity-creation helpers (declared in DropImport.h). Kept as free
// functions so they can be unit-tested against a bare World, no editor required.
ECS::Entity CreateAudioSourceEntity(ECS::World* world, const std::string& filePath) {
    if (!world) return ECS::INVALID_ENTITY;
    std::filesystem::path p(filePath);
    ECS::Entity e = world->CreateEntity();
    world->AddComponent<ECS::NameComponent>(e, p.stem().string());
    world->AddComponent<ECS::TransformComponent>(e);
    auto& src = world->AddComponent<ECS::AudioSourceComponent>(e);
    src.clipPath = filePath;
    return e;
}

ECS::Entity CreateSpriteEntity(ECS::World* world, const std::string& filePath) {
    if (!world) return ECS::INVALID_ENTITY;
    std::filesystem::path p(filePath);
    ECS::Entity e = world->CreateEntity();
    world->AddComponent<ECS::NameComponent>(e, p.stem().string());
    auto& tr = world->AddComponent<ECS::TransformComponent>(e);
    tr.rotation = Math::Quaternion(Math::Vector3(1, 0, 0), Math::Radians(-90.0f)); // face camera
    world->AddComponent<ECS::MeshComponent>(e, Renderer::MeshFactory::CreateQuad(1.0f, 1.0f));
    auto& mat = world->AddComponent<ECS::MaterialComponent>(e);
    mat.alphaMode = ECS::MaterialComponent::AlphaMode::Blend; // sprite transparency
    mat.baseColorTexturePath = filePath;
    return e;
}

void EditorLayer::OnFileDrop(int count, const char** paths) {
    if (!m_EditorSettings.enableDragDropImport) {
        if (count > 0) {
            ENJIN_LOG_INFO(Editor, "Drag-and-drop disabled; ignored %d dropped file(s)", count);
            m_ConsoleLog.push_back("[Info] Drag-and-drop import is off (Settings > System > Workflow)");
        }
        return;
    }
    // Collect model files first: a single model imports immediately ("just works"),
    // a GROUP opens one import dialog with an "apply to all" choice (below the loop).
    std::vector<std::string> modelPaths;

    for (int i = 0; i < count; ++i) {
        std::filesystem::path filePath(paths[i]);
        std::string ext = filePath.extension().string();
        // Case-insensitive extension comparison
        std::transform(ext.begin(), ext.end(), ext.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        if (ext == ".fbx" || ext == ".obj" || ext == ".gltf" || ext == ".glb" ||
            ext == ".dae" || ext == ".3ds") {
            modelPaths.push_back(filePath.string());
        } else if (ext == ".enjin") {
            ENJIN_LOG_INFO(Editor, "Drag-and-drop scene open: %s", paths[i]);
            m_ConsoleLog.push_back(std::string("[Info] Drag-and-drop scene open: ") + paths[i]);
            OpenScene(filePath.string());
        } else if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga" || ext == ".bmp") {
            // Assign texture to selected entity's material (or all mesh children if container)
            std::string texPath = Assets::CopyToProjectAssets(
                filePath.string(),
                std::filesystem::path(m_SceneManager.GetProjectPath()).parent_path().string(),
                "assets/textures");
            ECS::Entity selected = m_PrimarySelected;
            if (selected != ECS::INVALID_ENTITY && m_World) {

                // Helper: assign texture to a single entity
                auto assignTexture = [&](ECS::Entity e) {
                    auto* mat = m_World->GetComponent<ECS::MaterialComponent>(e);
                    if (mat) {
                        mat->baseColorTexturePath = texPath;
                        mat->baseColorTexture = -1;
                        if (m_RenderSystem) m_RenderSystem->ClearFailedTexture(texPath);
                        return true;
                    }
                    if (m_World->HasComponent<ECS::MeshComponent>(e)) {
                        auto& newMat = m_World->AddComponent<ECS::MaterialComponent>(e);
                        newMat.baseColorTexturePath = texPath;
                        if (m_RenderSystem) m_RenderSystem->ClearFailedTexture(texPath);
                        return true;
                    }
                    return false;
                };

                if (assignTexture(selected)) {
                    ENJIN_LOG_INFO(Editor, "Assigned texture to entity %llu: %s",
                        (unsigned long long)selected, paths[i]);
                    m_ConsoleLog.push_back(std::string("[Info] Texture assigned: ") + paths[i]);
                } else {
                    // Selected entity has no mesh — walk all descendants and apply to mesh children
                    int applied = 0;
                    std::function<void(ECS::Entity)> walkChildren = [&](ECS::Entity e) {
                        auto* ch = m_World->GetComponent<ECS::ChildrenComponent>(e);
                        if (!ch) return;
                        for (ECS::Entity child : ch->children) {
                            if (assignTexture(child)) ++applied;
                            walkChildren(child);
                        }
                    };
                    walkChildren(selected);
                    if (applied > 0) {
                        ENJIN_LOG_INFO(Editor, "Assigned texture to %d mesh children: %s", applied, paths[i]);
                        m_ConsoleLog.push_back(std::string("[Info] Texture assigned to ") +
                            std::to_string(applied) + " mesh children: " + paths[i]);
                    } else {
                        ENJIN_LOG_WARN(Editor, "No mesh entities found for texture: %s", paths[i]);
                        m_ConsoleLog.push_back(std::string("[Warn] Select a mesh entity first, then drop texture: ") + paths[i]);
                    }
                }
            } else {
                // Nothing selected — create a sprite quad showing the texture, so a
                // dropped image always produces something visible (Mac-style import).
                ECS::Entity e = CreateSpriteEntity(m_World, texPath);
                if (e != ECS::INVALID_ENTITY) {
                    if (m_RenderSystem) m_RenderSystem->ClearFailedTexture(texPath);
                    SelectEntity(e);
                    ENJIN_LOG_INFO(Editor, "Created sprite from texture: %s", paths[i]);
                    m_ConsoleLog.push_back(std::string("[Info] Created sprite from texture: ") + paths[i]);
                }
            }
        } else if (ext == ".wav" || ext == ".mp3" || ext == ".ogg" || ext == ".flac" ||
                   ext == ".aiff" || ext == ".aif") {
            // Drop a sound -> spawn an audio source entity pointing at the file.
            ECS::Entity e = CreateAudioSourceEntity(m_World, filePath.string());
            if (e != ECS::INVALID_ENTITY) {
                SelectEntity(e);
                ENJIN_LOG_INFO(Editor, "Created audio source from drop: %s", paths[i]);
                m_ConsoleLog.push_back(std::string("[Info] Created audio source: ") + paths[i]);
            }
        } else if (ext != ".fbx" && ext != ".obj" && ext != ".gltf" && ext != ".glb" &&
                   ext != ".dae" && ext != ".3ds") {
            ENJIN_LOG_WARN(Editor, "Unsupported file dropped: %s", paths[i]);
            m_ConsoleLog.push_back(std::string("[Warn] Unsupported file type: ") + paths[i]);
        }
    }

    // Dispatch the collected models. One model imports immediately (no dialog — it
    // "just works"). A GROUP opens a single import dialog with an "apply to all"
    // choice; the actual imports run from that dialog (see DrawImportDialog).
    if (modelPaths.size() == 1) {
        ImportModelImmediate(modelPaths[0], Math::Vector3(0.0f, 0.0f, 0.0f));
    } else if (modelPaths.size() > 1) {
        BeginGroupImport(modelPaths);
    }
}

void EditorLayer::UpdateDialogue(f32 deltaTime) {
    // DialogueSystem handles all logic (tree + legacy) via PlayMode.
    // Just query active entity for overlay rendering.
    m_ActiveDialogueEntity = m_PlayMode.GetDialogueSystem()->GetActiveDialogueEntity();

    // Update subtitle system timers
    m_SubtitleSystem.Update(deltaTime);
}

void EditorLayer::DrawDialogueOverlay() {
    if (!m_World || m_ActiveDialogueEntity == ECS::INVALID_ENTITY) return;

    auto* dlg = m_World->GetComponent<ECS::DialogueComponent>(m_ActiveDialogueEntity);
    if (!dlg) return;

    // Determine speaker, visible text, and choices based on mode
    std::string speaker;
    std::string visibleText;
    bool isTyping = dlg->isTyping;
    bool waiting = dlg->waitingForInput;
    bool hasChoices = false;
    i32 selectedChoice = dlg->selectedChoice;
    i32 choiceCount = 0;

    if (dlg->IsTreeMode()) {
        if (!dlg->treeActive) return;
        speaker = dlg->currentSpeaker;
        visibleText = dlg->GetTreeVisibleText();
        hasChoices = waiting && !dlg->currentChoices.empty();
        choiceCount = static_cast<i32>(dlg->currentChoices.size());
    } else {
        if (dlg->IsComplete()) return;
        speaker = dlg->speakerName;
        visibleText = dlg->GetVisibleText();
        hasChoices = waiting && !dlg->choices.empty() &&
                     dlg->currentLine + 1 >= dlg->dialogueLines.size();
        choiceCount = static_cast<i32>(dlg->choices.size());
    }

    ImGuiIO& io = ImGui::GetIO();
    f32 screenW = io.DisplaySize.x;
    f32 screenH = io.DisplaySize.y;

    f32 boxW = screenW * 0.75f;
    f32 boxH = 140.0f;
    f32 boxX = (screenW - boxW) * 0.5f;
    f32 boxY = screenH - boxH - 30.0f;
    f32 padding = 16.0f;

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNav;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(padding, padding));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 6.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 2.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.05f, 0.05f, 0.1f, 0.92f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.4f, 0.45f, 0.65f, 0.8f));

    ImGui::SetNextWindowPos(ImVec2(boxX, boxY));
    ImGui::SetNextWindowSize(ImVec2(boxW, boxH));

    if (ImGui::Begin("##DialogueBox", nullptr, flags)) {
        if (!speaker.empty()) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.75f, 1.0f, 1.0f));
            ImGui::Text("%s", speaker.c_str());
            ImGui::PopStyleColor();
            ImGui::Separator();
            ImGui::Spacing();
        }

        if (!visibleText.empty()) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.95f, 0.98f, 1.0f));
            ImGui::TextWrapped("%s", visibleText.c_str());
            ImGui::PopStyleColor();
        }

        if (isTyping) {
            ImGui::SameLine(0, 0);
            f32 blink = std::fmod(static_cast<f32>(ImGui::GetTime()) * 3.0f, 2.0f);
            if (blink < 1.0f) {
                ImGui::TextColored(ImVec4(0.7f, 0.8f, 1.0f, 0.8f), "_");
            }
        }

        if (waiting && !hasChoices) {
            f32 bounce = std::sin(static_cast<f32>(ImGui::GetTime()) * 4.0f) * 0.3f + 0.7f;
            ImGui::SetCursorPosY(boxH - padding - ImGui::GetTextLineHeight());
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.6f, 0.8f, bounce));
            ImGui::Text("[Space]");
            ImGui::PopStyleColor();
        }
    }
    ImGui::End();

    if (hasChoices) {
        f32 choiceH = static_cast<f32>(choiceCount) * 28.0f + padding * 2.0f;
        f32 choiceW = 300.0f;
        f32 choiceX = boxX + boxW - choiceW - 10.0f;
        f32 choiceY = boxY - choiceH - 8.0f;

        ImGui::SetNextWindowPos(ImVec2(choiceX, choiceY));
        ImGui::SetNextWindowSize(ImVec2(choiceW, choiceH));

        if (ImGui::Begin("##ChoiceBox", nullptr, flags)) {
            if (dlg->IsTreeMode()) {
                for (usize i = 0; i < dlg->currentChoices.size(); ++i) {
                    bool sel = (static_cast<i32>(i) == selectedChoice);
                    if (sel) {
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 0.6f, 1.0f));
                        ImGui::Text("> %s", dlg->currentChoices[i].text.c_str());
                        ImGui::PopStyleColor();
                    } else {
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.75f, 1.0f));
                        ImGui::Text("  %s", dlg->currentChoices[i].text.c_str());
                        ImGui::PopStyleColor();
                    }
                }
            } else {
                for (usize i = 0; i < dlg->choices.size(); ++i) {
                    bool sel = (static_cast<i32>(i) == selectedChoice);
                    if (sel) {
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 0.6f, 1.0f));
                        ImGui::Text("> %s", dlg->choices[i].text.c_str());
                        ImGui::PopStyleColor();
                    } else {
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.75f, 1.0f));
                        ImGui::Text("  %s", dlg->choices[i].text.c_str());
                        ImGui::PopStyleColor();
                    }
                }
            }
        }
        ImGui::End();
    }

    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(3);
}

void EditorLayer::DuplicateEntity(ECS::Entity entity) {
    if (!m_World || entity == ECS::INVALID_ENTITY) return;

    // Full-fidelity duplicate via serialize/deserialize (copies all 60+ components)
    std::string snapshot = Scene::SceneSerializer::SerializeEntityToString(m_World, entity);
    if (snapshot.empty()) return;

    ECS::Entity newEntity = Scene::SceneSerializer::DeserializeEntityFromString(m_World, snapshot);
    if (newEntity == ECS::INVALID_ENTITY) return;

    // Append "(Copy)" to name
    if (m_World->HasComponent<ECS::NameComponent>(newEntity)) {
        auto* nameComp = m_World->GetComponent<ECS::NameComponent>(newEntity);
        nameComp->name += " (Copy)";
    }

    // Offset position slightly so the duplicate is visible
    if (m_World->HasComponent<ECS::TransformComponent>(newEntity)) {
        auto* transform = m_World->GetComponent<ECS::TransformComponent>(newEntity);
        transform->position = transform->position + Math::Vector3(0.5f, 0.0f, 0.5f);
    }

    // Track via undo system
    auto cmd = std::make_unique<FullCreateEntityCommand>(
        m_World, newEntity,
        [this](ECS::Entity restored) { SelectEntity(restored); });
    m_UndoRedo.Execute(std::move(cmd));

    SelectEntity(newEntity);
    RecordLayerCreate(newEntity);
    ENJIN_LOG_INFO(Editor, "Duplicated entity %llu -> %llu", entity, newEntity);
}

void EditorLayer::DeleteSelectedEntities() {
    if (!m_World || m_SelectedEntities.empty()) return;

    // Pre-validate: only delete entities that still exist
    std::vector<ECS::Entity> validEntities;
    for (ECS::Entity e : m_SelectedEntities) {
        if (m_World->IsValid(e)) {
            validEntities.push_back(e);
        }
    }
    if (validEntities.empty()) return;

    // Deleting a parent deletes its whole subtree (imported FBX roots, empty
    // group parents). Collect descendants BEFORE issuing any delete (children
    // lists mutate during deletion), breadth-first with dedup, then delete
    // leaf-first so no command ever leaves live children on a dead parent.
    std::vector<ECS::Entity> toDelete;
    auto alreadyListed = [&](ECS::Entity e) {
        return std::find(toDelete.begin(), toDelete.end(), e) != toDelete.end();
    };
    for (ECS::Entity e : validEntities) {
        if (!alreadyListed(e)) toDelete.push_back(e);
    }
    for (usize i = 0; i < toDelete.size(); ++i) {
        for (ECS::Entity child : ECS::GetChildren(m_World, toDelete[i])) {
            if (m_World->IsValid(child) && !alreadyListed(child)) {
                toDelete.push_back(child);
            }
        }
    }
    std::reverse(toDelete.begin(), toDelete.end());  // leaf-first

    ClearSelection();

    if (toDelete.size() == 1) {
        auto cmd = std::make_unique<FullDeleteEntityCommand>(
            m_World, toDelete[0],
            [this](ECS::Entity restored) { SelectEntity(restored); });
        m_UndoRedo.Execute(std::move(cmd));
    } else {
        m_UndoRedo.BeginCompound("Delete Entities");
        for (ECS::Entity e : toDelete) {
            auto cmd = std::make_unique<FullDeleteEntityCommand>(m_World, e);
            m_UndoRedo.Execute(std::move(cmd));
        }
        m_UndoRedo.EndCompound();
    }
    ENJIN_LOG_INFO(Editor, "Deleted %zu entities (%zu selected + descendants)",
                   toDelete.size(), validEntities.size());

    // Accessibility announcement
    if (m_Announcer.enabled) {
        m_Announcer.Announce("Deleted " + std::to_string(toDelete.size()) + " entities",
            Accessibility::AnnouncePriority::Normal);
    }
}

void EditorLayer::DuplicateSelectedEntities() {
    if (!m_World || m_SelectedEntities.empty()) return;

    auto originals = m_SelectedEntities;
    ClearSelection();
    for (ECS::Entity e : originals) {
        DuplicateEntity(e);
    }
    // After DuplicateEntity calls, the last duplicated entity is selected;
    // re-select all duplicated entities (they were selected one by one via DuplicateEntity's SelectEntity call)
    ENJIN_LOG_INFO(Editor, "Duplicated %zu entities", originals.size());
}

void EditorLayer::AutoSave() {
    if (!m_World || m_CurrentScenePath.empty()) return;

    std::string autoSavePath = m_CurrentScenePath + ".autosave";

    Scene::SceneSerializer serializer(m_World);
    if (m_RenderSystem) {
        serializer.SetSkyboxConfig(m_RenderSystem->GetSkyboxConfig());
        serializer.SetWater2DConfig(m_RenderSystem->GetWater2DConfig());
    }
    serializer.SetContentFlags(m_SceneContentFlags);

    auto renderSettings = Renderer::SceneRenderSettings::CaptureFromRuntime(
        m_RenderSystem, m_PostProcessing ? &m_PostProcessing->GetSettings() : nullptr);
    renderSettings.useProjectDefaults = m_CurrentSceneUsesProjectDefaults;
    serializer.SetRenderSettings(renderSettings);

    auto result = serializer.Save(autoSavePath);
    if (result.success) {
        ENJIN_LOG_INFO(Editor, "Auto-saved to %s", autoSavePath.c_str());
    } else {
        ENJIN_LOG_WARN(Editor, "Auto-save failed: %s", result.error.c_str());
    }
}

} // namespace Editor
} // namespace Enjin
