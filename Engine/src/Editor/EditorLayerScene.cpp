#include "Enjin/Editor/EditorLayer.h"
#include "Enjin/Editor/DropImport.h"
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
    // Already have a loaded project — nothing to do
    if (!m_SceneManager.GetProjectPath().empty()) return;
    if (scenePath.empty()) return;

    namespace fs = std::filesystem;
    fs::path sceneFile(scenePath);
    if (!fs::exists(sceneFile)) return;

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
    }

    // Capture current render settings for serialization
    auto renderSettings = Renderer::SceneRenderSettings::CaptureFromRuntime(
        m_RenderSystem, m_PostProcessing ? &m_PostProcessing->GetSettings() : nullptr);
    renderSettings.useProjectDefaults = m_CurrentSceneUsesProjectDefaults;
    serializer.SetRenderSettings(renderSettings);

    auto result = serializer.Save(path);

    if (result.success) {
        m_CurrentScenePath = path;
        ClearDirty();

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
    // Defer to Update phase — World::Clear() must not run during Render
    // to avoid invalidating entity references used by the current frame.
    m_PendingSceneLoadPath = path;
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
    for (int i = 0; i < count; ++i) {
        std::filesystem::path filePath(paths[i]);
        std::string ext = filePath.extension().string();
        // Case-insensitive extension comparison
        std::transform(ext.begin(), ext.end(), ext.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        if (ext == ".fbx" || ext == ".obj" || ext == ".gltf" || ext == ".glb" ||
            ext == ".dae" || ext == ".3ds") {
            ENJIN_LOG_INFO(Editor, "Drag-and-drop import: %s", paths[i]);
            m_ConsoleLog.push_back(std::string("[Info] Drag-and-drop import: ") + paths[i]);
            ImportModel(filePath.string());
        } else if (ext == ".enjin") {
            ENJIN_LOG_INFO(Editor, "Drag-and-drop scene open: %s", paths[i]);
            m_ConsoleLog.push_back(std::string("[Info] Drag-and-drop scene open: ") + paths[i]);
            OpenScene(filePath.string());
        } else if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga" || ext == ".bmp") {
            // Assign texture to selected entity's material (or all mesh children if container)
            ECS::Entity selected = m_PrimarySelected;
            if (selected != ECS::INVALID_ENTITY && m_World) {
                std::string texPath = filePath.string();

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
                ECS::Entity e = CreateSpriteEntity(m_World, filePath.string());
                if (e != ECS::INVALID_ENTITY) {
                    if (m_RenderSystem) m_RenderSystem->ClearFailedTexture(filePath.string());
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
        } else {
            ENJIN_LOG_WARN(Editor, "Unsupported file dropped: %s", paths[i]);
            m_ConsoleLog.push_back(std::string("[Warn] Unsupported file type: ") + paths[i]);
        }
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

    ClearSelection();

    if (validEntities.size() == 1) {
        auto cmd = std::make_unique<FullDeleteEntityCommand>(
            m_World, validEntities[0],
            [this](ECS::Entity restored) { SelectEntity(restored); });
        m_UndoRedo.Execute(std::move(cmd));
    } else {
        m_UndoRedo.BeginCompound("Delete Entities");
        for (ECS::Entity e : validEntities) {
            auto cmd = std::make_unique<FullDeleteEntityCommand>(m_World, e);
            m_UndoRedo.Execute(std::move(cmd));
        }
        m_UndoRedo.EndCompound();
    }
    ENJIN_LOG_INFO(Editor, "Deleted %zu entities", validEntities.size());

    // Accessibility announcement
    if (m_Announcer.enabled) {
        m_Announcer.Announce("Deleted " + std::to_string(validEntities.size()) + " entities",
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
    }

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
