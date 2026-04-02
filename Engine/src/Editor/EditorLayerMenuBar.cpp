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
#include "Enjin/ECS/Components/Water3D.h"
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

void EditorLayer::DrawMenuBar() {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New Scene", "Ctrl+N")) {
                if (m_SceneDirty) {
                    m_UnsavedChangesAction = UnsavedAction::NewScene;
                    m_ShowUnsavedChangesDialog = true;
                } else if (m_World) {
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
                    // Reset rendering to project defaults
                    m_SceneManager.GetDefaultRenderSettings().ApplyToRuntime(
                        m_RenderSystem, m_PostProcessing ? &m_PostProcessing->GetSettings() : nullptr);
                    m_CurrentSceneUsesProjectDefaults = true;
                    ENJIN_LOG_INFO(Editor, "Created new scene");
                }
            }
            if (ImGui::MenuItem("Open Scene...", "Ctrl+O")) {
                std::vector<FileFilter> filters = {
                    { "Enjin Scene", "*.enjin" },
                    { "All Files", "*.*" }
                };
                auto sceneDir = m_CurrentScenePath.empty()
                    ? std::filesystem::path(m_SceneManager.GetProjectPath()).parent_path().string()
                    : std::filesystem::path(m_CurrentScenePath).parent_path().string();
                std::string path = FileDialog::OpenFile("Open Scene", filters, sceneDir);
                if (!path.empty()) {
                    if (m_SceneDirty) {
                        m_PendingOpenPath = path;
                        m_UnsavedChangesAction = UnsavedAction::OpenScene;
                        m_ShowUnsavedChangesDialog = true;
                    } else {
                        OpenScene(path);
                    }
                }
            }
            if (ImGui::MenuItem("Save Scene", "Ctrl+S")) {
                if (!m_CurrentScenePath.empty()) {
                    SaveScene(m_CurrentScenePath);
                } else {
                    // No current path, open Save As dialog
                    std::vector<FileFilter> filters = {
                        { "Enjin Scene", "*.enjin" },
                        { "All Files", "*.*" }
                    };
                    auto projRoot = std::filesystem::path(m_SceneManager.GetProjectPath()).parent_path().string();
                    std::string path = FileDialog::SaveFile("Save Scene", filters, projRoot, "scene.enjin");
                    if (!path.empty()) {
                        SaveScene(path);
                    }
                }
            }
            if (ImGui::MenuItem("Save Scene As...", "Ctrl+Shift+S")) {
                std::vector<FileFilter> filters = {
                    { "Enjin Scene", "*.enjin" },
                    { "All Files", "*.*" }
                };
                std::string defaultName = m_CurrentScenePath.empty() ? "scene.enjin" :
                    std::filesystem::path(m_CurrentScenePath).filename().string();
                auto saveDir = m_CurrentScenePath.empty()
                    ? std::filesystem::path(m_SceneManager.GetProjectPath()).parent_path().string()
                    : std::filesystem::path(m_CurrentScenePath).parent_path().string();
                std::string path = FileDialog::SaveFile("Save Scene As", filters, saveDir, defaultName);
                if (!path.empty()) {
                    SaveScene(path);
                }
            }
            if (ImGui::MenuItem("Save as Template...")) {
                ImGui::OpenPopup("SaveTemplatePopup");
            }
            // The popup must be at this scope level to persist across frames
            ImGui::Separator();

            // Project operations
            if (ImGui::MenuItem("New Project...")) {
                m_ShowNewProjectDialog = true;
                // Pre-fill default location
                std::string defaultLoc;
                if (!m_EditorSettings.lastProjectDir.empty() &&
                    std::filesystem::exists(m_EditorSettings.lastProjectDir)) {
                    defaultLoc = m_EditorSettings.lastProjectDir;
                } else {
#ifdef _WIN32
                    const char* userProfile = std::getenv("USERPROFILE");
                    defaultLoc = userProfile ? (std::string(userProfile) + "\\Documents\\EnjinProjects") : ".";
#else
                    const char* home = std::getenv("HOME");
                    defaultLoc = home ? (std::string(home) + "/Documents/EnjinProjects") : ".";
#endif
                }
                std::strncpy(m_NewProjDlgLocation, defaultLoc.c_str(), sizeof(m_NewProjDlgLocation) - 1);
                m_NewProjDlgLocation[sizeof(m_NewProjDlgLocation) - 1] = '\0';
                std::strncpy(m_NewProjDlgName, "MyGame", sizeof(m_NewProjDlgName));
                std::strncpy(m_NewProjDlgScene, "Main", sizeof(m_NewProjDlgScene));
                m_NewProjDlgTemplate = 0;
                ENJIN_LOG_INFO(Editor, "Opened new project dialog");
            }
            if (ImGui::MenuItem("Open Project...")) {
                std::vector<FileFilter> filters = {
                    { "Enjin Project", "*.enjinproject" },
                    { "All Files", "*.*" }
                };
                auto projDir = std::filesystem::path(m_SceneManager.GetProjectPath()).parent_path().string();
                std::string path = FileDialog::OpenFile("Open Project", filters, projDir);
                if (!path.empty()) {
                    if (m_SceneManager.LoadProject(path)) {
                        MigrateEditorSettingsToProject();
                        ENJIN_LOG_INFO(Editor, "Loaded project: %s", m_SceneManager.GetProjectName().c_str());
                        // Persist last project directory (grandparent: .enjinproject -> project dir -> parent)
                        m_EditorSettings.lastProjectDir = std::filesystem::path(path).parent_path().parent_path().string();
                        m_EditorSettings.Save();
                        // OpenScene defers to Update to avoid World::Clear during Render
                        auto& scenes = m_SceneManager.GetScenes();
                        std::string scenePath;
                        for (const auto& s : scenes) {
                            if (s.isStartScene) {
                                scenePath = (std::filesystem::path(m_SceneManager.GetProjectPath()).parent_path() / s.path).string();
                                break;
                            }
                        }
                        if (scenePath.empty() && !scenes.empty()) {
                            scenePath = (std::filesystem::path(m_SceneManager.GetProjectPath()).parent_path() / scenes[0].path).string();
                        }
                        if (!scenePath.empty()) {
                            OpenScene(scenePath);
                        }
                    }
                }
            }
            if (ImGui::MenuItem("Save Project")) {
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
            if (ImGui::MenuItem("Save Project As...")) {
                std::vector<FileFilter> filters = {
                    { "Enjin Project", "*.enjinproject" },
                    { "All Files", "*.*" }
                };
                std::string defaultName = m_SceneManager.GetProjectPath().empty() ? "project.enjinproject" :
                    std::filesystem::path(m_SceneManager.GetProjectPath()).filename().string();
                std::string path = FileDialog::SaveFile("Save Project As", filters, "", defaultName);
                if (!path.empty()) {
                    if (m_SceneManager.SaveProject(path)) {
                        // Switch to the new project — you are now IN this project
                        m_EditorSettings.AddRecentProject(path);
                        m_EditorSettings.lastProjectDir = std::filesystem::path(path).parent_path().string();
                        m_EditorSettings.Save();
                        ShowNotification("Project saved as: " + std::filesystem::path(path).stem().string(), NotificationType::Success);
                    } else {
                        ShowNotification("Failed to save project", NotificationType::Error);
                    }
                }
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Import Model...", "Ctrl+I")) {
                std::vector<FileFilter> filters = {
                    { "3D Models", "*.gltf;*.glb;*.fbx;*.obj;*.dae;*.3ds" },
                    { "glTF Files", "*.gltf;*.glb" },
                    { "All Files", "*.*" }
                };
                auto projRoot = std::filesystem::path(m_SceneManager.GetProjectPath()).parent_path().string();
                std::string path = FileDialog::OpenFile("Import Model", filters, projRoot);
                if (!path.empty()) {
                    ImportModel(path);
                }
            }
            if (ImGui::MenuItem("Re-import Last Model...", nullptr, false, !m_LastImportedModelPath.empty())) {
                ImportModel(m_LastImportedModelPath);
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Build Game...", "Ctrl+B")) {
                m_ShowBuildDialog = true;
                m_BuildFinished = false;
                m_BuildInProgress = false;
                m_BuildProgress = 0.0f;
                m_BuildResult = Build::BuildResult{};
                // Default output dir next to project
                if (m_BuildConfig.outputDir.empty() && !m_SceneManager.GetProjectPath().empty()) {
                    auto projDir = std::filesystem::path(m_SceneManager.GetProjectPath()).parent_path();
                    m_BuildConfig.outputDir = (projDir / "Build").string();
                }
                if (m_BuildConfig.windowTitle.empty()) {
                    m_BuildConfig.windowTitle = m_SceneManager.GetProjectName();
                }
            }
            if (ImGui::MenuItem("Export HTML5...")) {
                m_ShowHTML5ExportDialog = true;
                if (m_HTML5Config.title.empty() || m_HTML5Config.title == "My Game") {
                    m_HTML5Config.title = m_SceneManager.GetProjectName();
                }
                if (m_HTML5Config.outputDir.empty() && !m_SceneManager.GetProjectPath().empty()) {
                    auto projDir = std::filesystem::path(m_SceneManager.GetProjectPath()).parent_path();
                    m_HTML5Config.outputDir = (projDir / "HTML5Build").string();
                }
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Exit", "Alt+F4")) {
                if (m_SceneDirty) {
                    m_UnsavedChangesAction = UnsavedAction::Quit;
                    m_ShowUnsavedChangesDialog = true;
                } else {
                    m_ShowQuitFeedbackDialog = true;
                }
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Edit")) {
            if (ImGui::MenuItem("Undo", "Ctrl+Z", false, m_UndoRedo.CanUndo())) {
                m_UndoRedo.Undo();
            }
            if (ImGui::MenuItem("Redo", "Ctrl+Y", false, m_UndoRedo.CanRedo())) {
                m_UndoRedo.Redo();
            }
            ImGui::Separator();
            bool hasSelection = !m_SelectedEntities.empty() && m_World;
            if (ImGui::MenuItem("Cut", "Ctrl+X", false, hasSelection)) {
                Scene::SceneSerializer serializer(m_World);
                Scene::SerializationOptions opts;
                opts.includeVertexData = true;
                m_ClipboardEntityJson = serializer.SaveToString(opts);
                m_ClipboardIsCut = true;
                m_ClipboardSourceEntity = m_PrimarySelected;
                DeleteSelectedEntities();
            }
            if (ImGui::MenuItem("Copy", "Ctrl+C", false, hasSelection)) {
                Scene::SceneSerializer serializer(m_World);
                Scene::SerializationOptions opts;
                opts.includeVertexData = true;
                m_ClipboardEntityJson = serializer.SaveToString(opts);
                m_ClipboardIsCut = false;
                m_ClipboardSourceEntity = m_PrimarySelected;
            }
            if (ImGui::MenuItem("Paste", "Ctrl+V", false, !m_ClipboardEntityJson.empty())) {
                Scene::SceneSerializer serializer(m_World);
                auto result = serializer.LoadFromString(m_ClipboardEntityJson, false);
                if (result.success && !result.entities.empty()) {
                    // Wrap all pasted entities in undo commands
                    if (result.entities.size() == 1) {
                        auto cmd = std::make_unique<FullCreateEntityCommand>(
                            m_World, result.entities[0],
                            [this](ECS::Entity restored) { SelectEntity(restored); });
                        m_UndoRedo.Execute(std::move(cmd));
                    } else {
                        m_UndoRedo.BeginCompound("Paste Entities");
                        for (ECS::Entity e : result.entities) {
                            auto cmd = std::make_unique<FullCreateEntityCommand>(m_World, e);
                            m_UndoRedo.Execute(std::move(cmd));
                        }
                        m_UndoRedo.EndCompound();
                    }
                    if (result.rootEntity != ECS::INVALID_ENTITY) {
                        SelectEntity(result.rootEntity);
                    }
                }
                if (m_ClipboardIsCut) {
                    m_ClipboardEntityJson.clear();
                    m_ClipboardIsCut = false;
                }
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View")) {
            if (ImGui::BeginMenu("Panels")) {
                bool hierarchy = IsPanelVisible(EditorPanel::Hierarchy);
                bool inspector = IsPanelVisible(EditorPanel::Inspector);
                bool console = IsPanelVisible(EditorPanel::Console);
                bool assets = IsPanelVisible(EditorPanel::AssetBrowser);
                if (ImGui::MenuItem("Hierarchy", nullptr, &hierarchy)) {
                    SetPanelVisibility(EditorPanel::Hierarchy, hierarchy);
                }
                if (ImGui::MenuItem("Inspector", nullptr, &inspector)) {
                    SetPanelVisibility(EditorPanel::Inspector, inspector);
                }
                if (ImGui::MenuItem("Console", nullptr, &console)) {
                    SetPanelVisibility(EditorPanel::Console, console);
                }
                if (ImGui::MenuItem("Asset Browser", nullptr, &assets)) {
                    SetPanelVisibility(EditorPanel::AssetBrowser, assets);
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Settings")) {
                if (ImGui::MenuItem("System Settings")) {
                    OpenSettings(0);
                }
                if (ImGui::MenuItem("Project Settings")) {
                    OpenSettings(1);
                }
                if (ImGui::MenuItem("Scene Settings")) {
                    OpenSettings(2);
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Tools")) {
                // --- Scripting & Logic ---
                if (ImGui::BeginMenu("Scripting & Logic")) {
                    bool visualScript = IsPanelVisible(EditorPanel::VisualScript);
                    if (ImGui::MenuItem("Visual Script", nullptr, &visualScript)) {
                        SetPanelVisibility(EditorPanel::VisualScript, visualScript);
                    }
                    bool behaviorTree = IsPanelVisible(EditorPanel::BehaviorTree);
                    if (ImGui::MenuItem("Behavior Tree", nullptr, &behaviorTree)) {
                        SetPanelVisibility(EditorPanel::BehaviorTree, behaviorTree);
                    }
                    bool questFlow = IsPanelVisible(EditorPanel::QuestFlow);
                    if (ImGui::MenuItem("Quest Flow", nullptr, &questFlow)) {
                        SetPanelVisibility(EditorPanel::QuestFlow, questFlow);
                    }
                    bool dialogue = IsPanelVisible(EditorPanel::Dialogue);
                    if (ImGui::MenuItem("Dialogue Editor", nullptr, &dialogue)) {
                        SetPanelVisibility(EditorPanel::Dialogue, dialogue);
                    }
                    ImGui::EndMenu();
                }
                // --- Art & Animation ---
                if (ImGui::BeginMenu("Art & Animation")) {
                    bool pixelEditor = IsPanelVisible(EditorPanel::PixelEditorPanel);
                    if (ImGui::MenuItem("Pixel Editor", nullptr, &pixelEditor)) {
                        SetPanelVisibility(EditorPanel::PixelEditorPanel, pixelEditor);
                    }
                    bool spriteImporter = IsPanelVisible(EditorPanel::SpriteSheetImport);
                    if (ImGui::MenuItem("Sprite Sheet Importer", nullptr, &spriteImporter)) {
                        SetPanelVisibility(EditorPanel::SpriteSheetImport, spriteImporter);
                    }
                    bool vectorPanel = IsPanelVisible(EditorPanel::VectorDrawing);
                    if (ImGui::MenuItem("Vector Drawing", nullptr, &vectorPanel)) {
                        SetPanelVisibility(EditorPanel::VectorDrawing, vectorPanel);
                    }
                    bool animGraph = IsPanelVisible(EditorPanel::AnimGraph);
                    if (ImGui::MenuItem("Animation Graph", nullptr, &animGraph)) {
                        SetPanelVisibility(EditorPanel::AnimGraph, animGraph);
                    }
                    bool particleEditor = IsPanelVisible(EditorPanel::ParticleEditor);
                    if (ImGui::MenuItem("Particle Editor", nullptr, &particleEditor)) {
                        SetPanelVisibility(EditorPanel::ParticleEditor, particleEditor);
                    }
                    bool flashPanel = IsPanelVisible(EditorPanel::FlashTimeline);
                    if (ImGui::MenuItem("Flash Timeline", nullptr, &flashPanel)) {
                        SetPanelVisibility(EditorPanel::FlashTimeline, flashPanel);
                    }
                    ImGui::EndMenu();
                }
                // --- Rendering & Shaders ---
                if (ImGui::BeginMenu("Rendering & Shaders")) {
                    {
                        bool sgOpen = m_ShaderGraphEditor.IsOpen();
                        if (ImGui::MenuItem("Shader Graph", nullptr, &sgOpen)) {
                            m_ShaderGraphEditor.SetGraph(&m_ShaderGraphData);
                            m_ShaderGraphEditor.SetOpen(sgOpen);
                        }
                    }
                    {
                        bool agOpen = m_AudioGraphEditor.IsOpen();
                        if (ImGui::MenuItem("Audio Event Graph", nullptr, &agOpen)) {
                            m_AudioGraphEditor.SetGraph(&m_AudioGraphData);
                            m_AudioGraphEditor.SetOpen(agOpen);
                        }
                    }
                    bool proceduralGen = IsPanelVisible(EditorPanel::ProceduralGen);
                    if (ImGui::MenuItem("Procedural Generation", nullptr, &proceduralGen)) {
                        SetPanelVisibility(EditorPanel::ProceduralGen, proceduralGen);
                    }
                    ImGui::EndMenu();
                }
                // --- Collaboration & Version Control ---
                if (ImGui::BeginMenu("Collaboration")) {
                    bool gitIntegration = IsPanelVisible(EditorPanel::GitIntegration);
                    if (ImGui::MenuItem("Git Integration", nullptr, &gitIntegration)) {
                        SetPanelVisibility(EditorPanel::GitIntegration, gitIntegration);
                    }
                    bool collabPanel = IsPanelVisible(EditorPanel::Collaboration);
                    if (ImGui::MenuItem("Collaborative Editing", nullptr, &collabPanel)) {
                        SetPanelVisibility(EditorPanel::Collaboration, collabPanel);
                    }
                    bool networkPanel = IsPanelVisible(EditorPanel::NetworkPanel);
                    if (ImGui::MenuItem("Network Panel", nullptr, &networkPanel)) {
                        SetPanelVisibility(EditorPanel::NetworkPanel, networkPanel);
                    }
                    ImGui::EndMenu();
                }
                // --- Data & Debug ---
                if (ImGui::BeginMenu("Data & Debug")) {
                    bool profiler = IsPanelVisible(EditorPanel::Profiler);
                    if (ImGui::MenuItem("Profiler", nullptr, &profiler)) {
                        SetPanelVisibility(EditorPanel::Profiler, profiler);
                    }
                    bool dataAssets = IsPanelVisible(EditorPanel::DataAssets);
                    if (ImGui::MenuItem("Data Asset Editor", nullptr, &dataAssets)) {
                        SetPanelVisibility(EditorPanel::DataAssets, dataAssets);
                    }
                    bool pluginBrowser = IsPanelVisible(EditorPanel::PluginBrowser);
                    if (ImGui::MenuItem("Plugin Browser", nullptr, &pluginBrowser)) {
                        SetPanelVisibility(EditorPanel::PluginBrowser, pluginBrowser);
                    }
                    bool saveDebug = IsPanelVisible(EditorPanel::SaveDebug);
                    if (ImGui::MenuItem("Save Debug", nullptr, &saveDebug)) {
                        SetPanelVisibility(EditorPanel::SaveDebug, saveDebug);
                    }
                    ImGui::EndMenu();
                }
                {
                    bool pgOpen = m_ParticleGraphEditor.IsOpen();
                    if (ImGui::MenuItem("Particle Graph", nullptr, &pgOpen)) {
                        m_ParticleGraphEditor.SetGraph(&m_ParticleGraphData);
                        m_ParticleGraphEditor.SetOpen(pgOpen);
                    }
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Audio Mixer", nullptr, &m_ShowAudioMixer)) {}
                ImGui::Separator();
                if (ImGui::MenuItem("Template Creator", nullptr, &m_ShowTemplateCreator)) {
                    if (m_ShowTemplateCreator) m_TmplNeedsRescan = true;
                }
                {
                    bool mpOpen = m_TemplateMarketplace.IsOpen();
                    if (ImGui::MenuItem("Template Marketplace", nullptr, &mpOpen)) {
                        if (mpOpen && m_TemplateMarketplace.GetCatalog().empty()) {
                            m_TemplateMarketplace.Initialize("templates");
                        }
                        m_TemplateMarketplace.SetOpen(mpOpen);
                    }
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Generate Documentation...")) {
                    m_DocGenerator.SetOutputDirectory("docs/generated");
                    if (m_DocGenerator.GenerateAll()) {
                        ENJIN_LOG_INFO(Editor, "Documentation generated: %zu files",
                                       m_DocGenerator.GetGeneratedFiles().size());
                    } else {
                        ENJIN_LOG_ERROR(Editor, "Documentation generation failed: %s",
                                        m_DocGenerator.GetLastError().c_str());
                    }
                }
                ImGui::EndMenu();
            }
            ImGui::Separator();
            bool gameView = IsPanelVisible(EditorPanel::GameView);
            if (ImGui::MenuItem("Game View", nullptr, &gameView)) {
                SetPanelVisibility(EditorPanel::GameView, gameView);
            }
            bool sceneList = IsPanelVisible(EditorPanel::SceneList);
            if (ImGui::MenuItem("Scene List", nullptr, &sceneList)) {
                SetPanelVisibility(EditorPanel::SceneList, sceneList);
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Game Debug", "F1", m_GameDebugActive)) {
                ToggleGameDebug();
            }
            ImGui::SetItemTooltip("Toggle Console panel + FPS/stats debug overlay");
            if (ImGui::MenuItem("Engine Debug", "F2", m_EngineDebugActive)) {
                ToggleEngineDebug();
            }
            ImGui::SetItemTooltip("Toggle Profiler, Rendering, PostProcessing, SaveDebug panels + colliders");
            ImGui::Separator();
            ImGui::MenuItem("UV Preview", nullptr, &m_ShowUVPreview);
            ImGui::MenuItem("Show Colliders", nullptr, &m_ShowColliderWireframes);
            ImGui::MenuItem("Gamepad Editor", nullptr, &m_GamepadEditorEnabled);
            ImGui::SetItemTooltip("RB=Tools, LB=File, Start=Play, Y=Create (radial menus)");
            ImGui::MenuItem("Stats Overlay", nullptr, &m_ShowStatsOverlay);
            ImGui::MenuItem("ImGui Demo", nullptr, &m_ShowDemoWindow);
            ImGui::Separator();
            if (ImGui::MenuItem("Reset Layout")) {
                m_DockingInitialized = false;
                m_ForceLayout = true;  // Force positions on next frame
            }
            if (ImGui::MenuItem("Show All Panels")) {
                m_VisiblePanels = EditorPanel::All;
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Entity")) {
            if (ImGui::MenuItem("Create Empty")) {
                if (m_World) {
                    ECS::Entity entity = m_World->CreateEntity();
                    m_World->AddComponent<ECS::TransformComponent>(entity);
                    SelectEntity(entity);
                    if (m_CollabSystem.IsActive()) {
                        m_CollabSystem.OnEntityCreated(entity,
                            Scene::SceneSerializer::SerializeEntityToString(m_World, entity));
                    }
                }
            }
            // Helper: create a primitive with transform + mesh + default material + name
            auto createPrimitive = [&](const char* name, ECS::MeshComponent mesh) {
                ECS::Entity entity = m_World->CreateEntity();
                m_World->AddComponent<ECS::NameComponent>(entity, name);
                m_World->AddComponent<ECS::TransformComponent>(entity);
                m_World->AddComponent<ECS::MeshComponent>(entity, std::move(mesh));
                m_World->AddComponent<ECS::MaterialComponent>(entity);
                SelectEntity(entity);
            };

            if (ImGui::BeginMenu("3D Object")) {
                if (ImGui::MenuItem("Cube")) {
                    if (m_World) createPrimitive("Cube", Renderer::MeshFactory::CreateCube(1.0f));
                }
                if (ImGui::MenuItem("Sphere")) {
                    if (m_World) createPrimitive("Sphere", Renderer::MeshFactory::CreateSphere(0.5f));
                }
                if (ImGui::MenuItem("Plane")) {
                    if (m_World) createPrimitive("Plane", Renderer::MeshFactory::CreatePlane(10.0f, 10.0f));
                }
                if (ImGui::MenuItem("Cylinder")) {
                    if (m_World) createPrimitive("Cylinder", Renderer::MeshFactory::CreateCylinder(0.5f, 1.0f));
                }
                if (ImGui::MenuItem("Cone")) {
                    if (m_World) createPrimitive("Cone", Renderer::MeshFactory::CreateCone(0.5f, 1.0f));
                }
                if (ImGui::MenuItem("Capsule")) {
                    if (m_World) createPrimitive("Capsule", Renderer::MeshFactory::CreateCapsule(0.3f, 1.0f));
                }
                if (ImGui::MenuItem("Pyramid")) {
                    if (m_World) createPrimitive("Pyramid", Renderer::MeshFactory::CreatePyramid(1.0f, 1.0f));
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("2D Object")) {
                if (ImGui::MenuItem("Triangle")) {
                    if (m_World) createPrimitive("Triangle", Renderer::MeshFactory::CreateTriangle(1.0f));
                }
                if (ImGui::MenuItem("Quad")) {
                    if (m_World) createPrimitive("Quad", Renderer::MeshFactory::CreateQuad(1.0f, 1.0f));
                }
                if (ImGui::MenuItem("Sprite")) {
                    if (m_World) {
                        ECS::Entity entity = m_World->CreateEntity();
                        auto& transform = m_World->AddComponent<ECS::TransformComponent>(entity);
                        transform.rotation = Math::Quaternion(Math::Vector3(1, 0, 0), Math::Radians(-90.0f));  // Face camera by default
                        m_World->AddComponent<ECS::MeshComponent>(entity, Renderer::MeshFactory::CreateQuad(1.0f, 1.0f));
                        auto& material = m_World->AddComponent<ECS::MaterialComponent>(entity);
                        material.alphaMode = ECS::MaterialComponent::AlphaMode::Blend;  // Support transparency for sprites
                        m_World->AddComponent<ECS::NameComponent>(entity, "Sprite");
                        SelectEntity(entity);
                    }
                }
                if (ImGui::MenuItem("Circle")) {
                    if (m_World) {
                        ECS::Entity entity = m_World->CreateEntity();
                        m_World->AddComponent<ECS::NameComponent>(entity, "Circle");
                        auto& transform = m_World->AddComponent<ECS::TransformComponent>(entity);
                        transform.rotation = Math::Quaternion(Math::Vector3(1, 0, 0), Math::Radians(-90.0f));
                        m_World->AddComponent<ECS::MeshComponent>(entity, Renderer::MeshFactory::CreateSphere(0.5f, 32, 1));
                        m_World->AddComponent<ECS::MaterialComponent>(entity);
                        SelectEntity(entity);
                    }
                }
                if (ImGui::MenuItem("Capsule 2D")) {
                    if (m_World) createPrimitive("Capsule 2D", Renderer::MeshFactory::CreateCapsule2D(0.8f, 1.6f));
                }
                if (ImGui::MenuItem("Ground Strip")) {
                    if (m_World) {
                        ECS::Entity entity = m_World->CreateEntity();
                        auto& transform = m_World->AddComponent<ECS::TransformComponent>(entity);
                        transform.scale = Math::Vector3(20.0f, 0.5f, 1.0f);
                        m_World->AddComponent<ECS::MeshComponent>(entity, Renderer::MeshFactory::CreateQuad(1.0f, 1.0f));
                        auto& material = m_World->AddComponent<ECS::MaterialComponent>(entity);
                        material.baseColor = Math::Vector3(0.35f, 0.55f, 0.3f);
                        m_World->AddComponent<ECS::NameComponent>(entity, "Ground");
                        auto& collider = m_World->AddComponent<ECS::BoxColliderComponent>(entity);
                        collider.size = Math::Vector3(20.0f, 0.5f, 1.0f);
                        SelectEntity(entity);
                    }
                }
                if (ImGui::MenuItem("Panel (UI)")) {
                    if (m_World) {
                        ECS::Entity entity = m_World->CreateEntity();
                        m_World->AddComponent<ECS::TransformComponent>(entity);
                        m_World->AddComponent<ECS::MeshComponent>(entity, Renderer::MeshFactory::CreateQuad(2.0f, 1.0f));
                        auto& material = m_World->AddComponent<ECS::MaterialComponent>(entity);
                        material.baseColor = Math::Vector3(0.2f, 0.2f, 0.2f);
                        material.opacity = 0.9f;
                        material.alphaMode = ECS::MaterialComponent::AlphaMode::Blend;
                        m_World->AddComponent<ECS::NameComponent>(entity, "UI Panel");
                        SelectEntity(entity);
                    }
                }
                ImGui::EndMenu();
            }
            if (ImGui::MenuItem("Ground Plane")) {
                if (m_World) {
                    ECS::Entity entity = m_World->CreateEntity();
                    auto& transform = m_World->AddComponent<ECS::TransformComponent>(entity);
                    transform.scale = Math::Vector3(50.0f, 1.0f, 50.0f);
                    m_World->AddComponent<ECS::MeshComponent>(entity, Renderer::MeshFactory::CreatePlane(1.0f, 1.0f));
                    auto& material = m_World->AddComponent<ECS::MaterialComponent>(entity);
                    material.baseColor = Math::Vector3(0.5f, 0.5f, 0.5f);
                    m_World->AddComponent<ECS::NameComponent>(entity, "Ground");
                    auto& collider = m_World->AddComponent<ECS::BoxColliderComponent>(entity);
                    collider.size = Math::Vector3(50.0f, 0.1f, 50.0f);
                    collider.center = Math::Vector3(0.0f, -0.05f, 0.0f);
                    SelectEntity(entity);
                }
            }
            ImGui::Separator();
            if (ImGui::BeginMenu("Light")) {
                if (ImGui::MenuItem("Directional Light (Sun)")) {
                    if (m_World) {
                        ECS::Entity entity = m_World->CreateEntity();
                        m_World->AddComponent<ECS::NameComponent>(entity, "Sun");
                        auto& transform = m_World->AddComponent<ECS::TransformComponent>(entity);
                        transform.rotation = Math::Quaternion(Math::Vector3(1, 0, 0), Math::Radians(-45.0f));
                        auto& light = m_World->AddComponent<ECS::LightComponent>(entity);
                        light.type = ECS::LightType::Directional;
                        light.intensity = 1.0f;
                        light.castShadows = true;
                        SelectEntity(entity);
                    }
                }
                if (ImGui::MenuItem("Point Light")) {
                    if (m_World) {
                        ECS::Entity entity = m_World->CreateEntity();
                        m_World->AddComponent<ECS::NameComponent>(entity, "Point Light");
                        auto& transform = m_World->AddComponent<ECS::TransformComponent>(entity);
                        transform.position = Math::Vector3(0, 2, 0);
                        auto& light = m_World->AddComponent<ECS::LightComponent>(entity);
                        light.type = ECS::LightType::Point;
                        SelectEntity(entity);
                    }
                }
                if (ImGui::MenuItem("Spot Light")) {
                    if (m_World) {
                        ECS::Entity entity = m_World->CreateEntity();
                        m_World->AddComponent<ECS::NameComponent>(entity, "Spot Light");
                        auto& transform = m_World->AddComponent<ECS::TransformComponent>(entity);
                        transform.position = Math::Vector3(0, 3, 0);
                        auto& light = m_World->AddComponent<ECS::LightComponent>(entity);
                        light.type = ECS::LightType::Spot;
                        SelectEntity(entity);
                    }
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Camera")) {
                if (ImGui::MenuItem("Perspective Camera")) {
                    if (m_World) {
                        ECS::Entity entity = m_World->CreateEntity();
                        auto& transform = m_World->AddComponent<ECS::TransformComponent>(entity);
                        transform.position = Math::Vector3(0, 2, -5);
                        auto& cam = m_World->AddComponent<ECS::CameraComponent>(entity);
                        cam.projectionType = ECS::ProjectionType::Perspective;
                        cam.fieldOfView = 60.0f;
                        m_World->AddComponent<ECS::NameComponent>(entity, "Game Camera");
                        SelectEntity(entity);
                    }
                }
                if (ImGui::MenuItem("Orthographic Camera")) {
                    if (m_World) {
                        ECS::Entity entity = m_World->CreateEntity();
                        auto& transform = m_World->AddComponent<ECS::TransformComponent>(entity);
                        transform.position = Math::Vector3(0, 10, 0);
                        transform.rotation = Math::Quaternion(Math::Vector3(1, 0, 0), Math::Radians(-90.0f));
                        auto& cam = m_World->AddComponent<ECS::CameraComponent>(entity);
                        cam.projectionType = ECS::ProjectionType::Orthographic;
                        cam.orthoSize = 10.0f;
                        m_World->AddComponent<ECS::NameComponent>(entity, "2D Camera");
                        SelectEntity(entity);
                    }
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Effects")) {
                if (ImGui::MenuItem("Weather Zone")) {
                    if (m_World) {
                        ECS::Entity entity = m_World->CreateEntity();
                        m_World->AddComponent<ECS::TransformComponent>(entity);
                        m_World->AddComponent<ECS::WeatherZoneComponent>(entity);
                        m_World->AddComponent<ECS::NameComponent>(entity, "Weather Zone");
                        SelectEntity(entity);
                    }
                }
                if (ImGui::MenuItem("Water Volume")) {
                    if (m_World) {
                        ECS::Entity entity = m_World->CreateEntity();
                        m_World->AddComponent<ECS::TransformComponent>(entity);
                        m_World->AddComponent<ECS::WaterVolumeComponent>(entity);
                        m_World->AddComponent<ECS::NameComponent>(entity, "Water Volume");
                        SelectEntity(entity);
                    }
                }
                if (ImGui::MenuItem("Water 3D")) {
                    if (m_World) {
                        ECS::Entity entity = m_World->CreateEntity();
                        m_World->AddComponent<ECS::TransformComponent>(entity);
                        m_World->AddComponent<ECS::Water3DComponent>(entity);
                        m_World->AddComponent<ECS::NameComponent>(entity, "Water 3D");
                        SelectEntity(entity);
                    }
                }
                if (ImGui::MenuItem("Grass Volume")) {
                    if (m_World) {
                        ECS::Entity entity = m_World->CreateEntity();
                        m_World->AddComponent<ECS::TransformComponent>(entity);
                        m_World->AddComponent<ECS::GrassVolumeComponent>(entity);
                        m_World->AddComponent<ECS::NameComponent>(entity, "Grass Volume");
                        SelectEntity(entity);
                    }
                }
                if (ImGui::MenuItem("Shrub Volume")) {
                    if (m_World) {
                        ECS::Entity entity = m_World->CreateEntity();
                        m_World->AddComponent<ECS::TransformComponent>(entity);
                        m_World->AddComponent<ECS::ShrubVolumeComponent>(entity);
                        m_World->AddComponent<ECS::NameComponent>(entity, "Shrub Volume");
                        SelectEntity(entity);
                    }
                }
                if (ImGui::MenuItem("Tree Volume")) {
                    if (m_World) {
                        ECS::Entity entity = m_World->CreateEntity();
                        m_World->AddComponent<ECS::TransformComponent>(entity);
                        m_World->AddComponent<ECS::TreeVolumeComponent>(entity);
                        m_World->AddComponent<ECS::NameComponent>(entity, "Tree Volume");
                        SelectEntity(entity);
                    }
                }
                if (ImGui::MenuItem("Camera Trigger")) {
                    if (m_World) {
                        ECS::Entity entity = m_World->CreateEntity();
                        m_World->AddComponent<ECS::TransformComponent>(entity);
                        m_World->AddComponent<ECS::CameraTriggerComponent>(entity);
                        m_World->AddComponent<ECS::NameComponent>(entity, "Camera Trigger");
                        SelectEntity(entity);
                    }
                }
                if (ImGui::MenuItem("Temperature Zone")) {
                    if (m_World) {
                        ECS::Entity entity = m_World->CreateEntity();
                        m_World->AddComponent<ECS::TransformComponent>(entity);
                        m_World->AddComponent<ECS::TemperatureZoneComponent>(entity);
                        m_World->AddComponent<ECS::NameComponent>(entity, "Temperature Zone");
                        SelectEntity(entity);
                    }
                }
                ImGui::EndMenu();
            }

            ImGui::Separator();

            if (ImGui::MenuItem("Terrain")) {
                if (m_World) {
                    ECS::Entity entity = m_World->CreateEntity();
                    m_World->AddComponent<ECS::TransformComponent>(entity);
                    auto& terrain = m_World->AddComponent<ECS::TerrainComponent>(entity);
                    terrain.InitializeFlat(0.0f);
                    m_World->AddComponent<ECS::MaterialComponent>(entity);
                    m_World->AddComponent<ECS::NameComponent>(entity, "Terrain");
                    SelectEntity(entity);
                }
            }

            if (ImGui::MenuItem("2D Terrain")) {
                if (m_World) {
                    ECS::Entity entity = m_World->CreateEntity();
                    m_World->AddComponent<ECS::TransformComponent>(entity);
                    auto& terrain2d = m_World->AddComponent<ECS::Terrain2DComponent>(entity);
                    terrain2d.AddPoint(Math::Vector2(-10.0f, 0.0f));
                    terrain2d.AddPoint(Math::Vector2(-3.0f, 2.0f));
                    terrain2d.AddPoint(Math::Vector2(3.0f, -1.0f));
                    terrain2d.AddPoint(Math::Vector2(10.0f, 0.0f));
                    m_World->AddComponent<ECS::MaterialComponent>(entity);
                    m_World->AddComponent<ECS::NameComponent>(entity, "2D Terrain");
                    SelectEntity(entity);
                }
            }

            if (ImGui::BeginMenu("Examples")) {
                if (ImGui::MenuItem("NPC with Dialogue")) {
                    if (m_World) {
                        ECS::Entity entity = m_World->CreateEntity();
                        auto& t = m_World->AddComponent<ECS::TransformComponent>(entity);
                        t.position = Math::Vector3(0.0f, 0.5f, 0.0f);
                        m_World->AddComponent<ECS::MeshComponent>(entity, Renderer::MeshFactory::CreateCapsule(0.3f, 1.0f));
                        auto& mat = m_World->AddComponent<ECS::MaterialComponent>(entity);
                        mat.baseColor = Math::Vector3(0.7f, 0.5f, 0.3f);
                        auto& interact = m_World->AddComponent<ECS::InteractableComponent>(entity);
                        interact.interactionRange = 3.0f;
                        interact.promptText = "Talk";
                        auto& dialogue = m_World->AddComponent<ECS::DialogueComponent>(entity);
                        dialogue.speakerName = "NPC";
                        dialogue.dialogueLines.push_back("Hello, traveler!");
                        dialogue.dialogueLines.push_back("The weather has been strange lately.");
                        dialogue.dialogueLines.push_back("Be careful out there.");
                        m_World->AddComponent<ECS::StateMachineComponent>(entity);
                        m_World->AddComponent<ECS::NameComponent>(entity, "NPC");
                        SelectEntity(entity);
                    }
                }
                if (ImGui::MenuItem("Health Pickup")) {
                    if (m_World) {
                        ECS::Entity entity = m_World->CreateEntity();
                        auto& t = m_World->AddComponent<ECS::TransformComponent>(entity);
                        t.position = Math::Vector3(2.0f, 0.3f, 0.0f);
                        t.scale = Math::Vector3(0.3f, 0.3f, 0.3f);
                        m_World->AddComponent<ECS::MeshComponent>(entity, Renderer::MeshFactory::CreateSphere(0.5f));
                        auto& mat = m_World->AddComponent<ECS::MaterialComponent>(entity);
                        mat.baseColor = Math::Vector3(0.2f, 0.8f, 0.2f);
                        mat.emissiveColor = Math::Vector3(0.1f, 0.4f, 0.1f);
                        mat.emissiveStrength = 0.5f;
                        auto& pickup = m_World->AddComponent<ECS::PickupComponent>(entity);
                        pickup.type = ECS::PickupComponent::PickupType::Health;
                        pickup.value = 25.0f;
                        pickup.magnetRange = 2.0f;
                        m_World->AddComponent<ECS::NameComponent>(entity, "Health Pickup");
                        SelectEntity(entity);
                    }
                }
                if (ImGui::MenuItem("Coin Collectible")) {
                    if (m_World) {
                        ECS::Entity entity = m_World->CreateEntity();
                        auto& t = m_World->AddComponent<ECS::TransformComponent>(entity);
                        t.position = Math::Vector3(4.0f, 0.5f, 0.0f);
                        t.scale = Math::Vector3(0.2f, 0.3f, 0.2f);
                        m_World->AddComponent<ECS::MeshComponent>(entity, Renderer::MeshFactory::CreateCylinder(0.5f, 0.1f));
                        auto& mat = m_World->AddComponent<ECS::MaterialComponent>(entity);
                        mat.baseColor = Math::Vector3(1.0f, 0.84f, 0.0f);
                        mat.metallic = 0.9f;
                        mat.emissiveColor = Math::Vector3(0.4f, 0.33f, 0.0f);
                        mat.emissiveStrength = 0.3f;
                        auto& pickup = m_World->AddComponent<ECS::PickupComponent>(entity);
                        pickup.type = ECS::PickupComponent::PickupType::Coin;
                        pickup.value = 1.0f;
                        m_World->AddComponent<ECS::NameComponent>(entity, "Coin");
                        SelectEntity(entity);
                    }
                }
                if (ImGui::MenuItem("Damage Zone")) {
                    if (m_World) {
                        ECS::Entity entity = m_World->CreateEntity();
                        auto& t = m_World->AddComponent<ECS::TransformComponent>(entity);
                        t.position = Math::Vector3(-3.0f, 0.5f, 0.0f);
                        m_World->AddComponent<ECS::MeshComponent>(entity, Renderer::MeshFactory::CreateCube(1.0f));
                        auto& mat = m_World->AddComponent<ECS::MaterialComponent>(entity);
                        mat.baseColor = Math::Vector3(0.8f, 0.1f, 0.1f);
                        mat.opacity = 0.5f;
                        auto& trigger = m_World->AddComponent<ECS::TriggerZoneComponent>(entity);
                        trigger.shape = ECS::TriggerZoneComponent::Shape::Box;
                        trigger.boxSize = Math::Vector3(0.5f, 0.5f, 0.5f);
                        auto& dmg = m_World->AddComponent<ECS::DamageComponent>(entity);
                        dmg.damage = 5.0f;
                        dmg.damageInterval = 1.0f;
                        m_World->AddComponent<ECS::NameComponent>(entity, "Damage Zone");
                        SelectEntity(entity);
                    }
                }
                if (ImGui::MenuItem("Patrol Enemy")) {
                    if (m_World) {
                        ECS::Entity entity = m_World->CreateEntity();
                        auto& t = m_World->AddComponent<ECS::TransformComponent>(entity);
                        t.position = Math::Vector3(5.0f, 0.5f, 5.0f);
                        m_World->AddComponent<ECS::MeshComponent>(entity, Renderer::MeshFactory::CreateCapsule(0.3f, 1.0f));
                        auto& mat = m_World->AddComponent<ECS::MaterialComponent>(entity);
                        mat.baseColor = Math::Vector3(0.8f, 0.15f, 0.1f);
                        auto& ai = m_World->AddComponent<ECS::AIControllerComponent>(entity);
                        ai.currentState = ECS::AIControllerComponent::AIState::Patrol;
                        ai.moveSpeed = 2.0f;
                        auto& health = m_World->AddComponent<ECS::HealthComponent>(entity);
                        health.maxHealth = 50.0f;
                        health.currentHealth = 50.0f;
                        auto& dmg = m_World->AddComponent<ECS::DamageComponent>(entity);
                        dmg.damage = 10.0f;
                        m_World->AddComponent<ECS::NameComponent>(entity, "Patrol Enemy");
                        SelectEntity(entity);
                    }
                }
                if (ImGui::MenuItem("Interactable Chest")) {
                    if (m_World) {
                        ECS::Entity entity = m_World->CreateEntity();
                        auto& t = m_World->AddComponent<ECS::TransformComponent>(entity);
                        t.position = Math::Vector3(-5.0f, 0.3f, 0.0f);
                        t.scale = Math::Vector3(0.6f, 0.5f, 0.4f);
                        m_World->AddComponent<ECS::MeshComponent>(entity, Renderer::MeshFactory::CreateCube(1.0f));
                        auto& mat = m_World->AddComponent<ECS::MaterialComponent>(entity);
                        mat.baseColor = Math::Vector3(0.45f, 0.3f, 0.15f);
                        auto& interact = m_World->AddComponent<ECS::InteractableComponent>(entity);
                        interact.interactionRange = 2.0f;
                        interact.promptText = "Open";
                        auto& inv = m_World->AddComponent<ECS::InventoryComponent>(entity);
                        inv.maxSlots = 5;
                        m_World->AddComponent<ECS::NameComponent>(entity, "Chest");
                        SelectEntity(entity);
                    }
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Help")) {
            bool userManual = IsPanelVisible(EditorPanel::UserManual);
            if (ImGui::MenuItem("User Manual", nullptr, &userManual)) {
                SetPanelVisibility(EditorPanel::UserManual, userManual);
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Report Bug...", "Ctrl+Shift+B")) {
                m_ShowDiscordBugDialog = true;
                m_DiscordSendState = DiscordSendState::Idle;
            }
            if (ImGui::MenuItem("Send Feedback...")) {
                SetPanelVisibility(EditorPanel::FeedbackPanel, true);
                m_FeedbackTab = FeedbackTab::NewFeedback;
            }
            bool feedbackVisible = IsPanelVisible(EditorPanel::FeedbackPanel);
            if (ImGui::MenuItem("Bug Reports & Feedback", nullptr, &feedbackVisible)) {
                SetPanelVisibility(EditorPanel::FeedbackPanel, feedbackVisible);
            }
            ImGui::Separator();
            if (ImGui::MenuItem("About Enjin")) {
                m_ShowAboutDialog = true;
            }
            ImGui::EndMenu();
        }

        // --- Play mode controls (centered) ---
        {
            // Measure play control widths to center them
            const char* playLabel = m_PlayMode.IsStopped() ? "  Play  " :
                                    m_PlayMode.IsPlaying() ? " Pause " : " Resume";
            f32 playBtnW = ImGui::CalcTextSize(playLabel).x + ImGui::GetStyle().FramePadding.x * 2.0f;
            f32 stopBtnW = ImGui::CalcTextSize(" Stop ").x + ImGui::GetStyle().FramePadding.x * 2.0f;
            f32 stateW = 0.0f;
            if (m_PlayMode.IsPlaying()) stateW = ImGui::CalcTextSize("[PLAYING]").x + 8.0f;
            else if (m_PlayMode.IsPaused()) stateW = ImGui::CalcTextSize("[PAUSED]").x + 8.0f;
            f32 totalW = playBtnW + (m_PlayMode.IsStopped() ? 0.0f : 4.0f + stopBtnW) + stateW;

            f32 barW = ImGui::GetWindowWidth();
            f32 centerX = (barW - totalW) * 0.5f;
            f32 cursorX = ImGui::GetCursorPosX();
            if (centerX > cursorX) {
                ImGui::SetCursorPosX(centerX);
            }
        }

        // Color the background based on play state
        if (m_PlayMode.IsPlaying()) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
        } else if (m_PlayMode.IsPaused()) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.6f, 0.2f, 1.0f));
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
        }

        // Play/Pause button
        if (m_PlayMode.IsStopped()) {
            if (ImGui::Button("  Play  ")) {
                // Save render settings before play mode
                m_PrePlayRenderSettings = Renderer::SceneRenderSettings::CaptureFromRuntime(
                    m_RenderSystem, m_PostProcessing ? &m_PostProcessing->GetSettings() : nullptr);
                StartPlayMode();
                if (m_EditorSettings.autoFocusMode) {
                    m_FocusMode = true;
                    Input::SetMouseCaptured(true);
                }
            }
        } else if (m_PlayMode.IsPlaying()) {
            if (ImGui::Button(" Pause ")) {
                m_PlayMode.Pause();
            }
        } else {  // Paused
            if (ImGui::Button(" Resume")) {
                m_PlayMode.Resume();
            }
        }
        ImGui::PopStyleColor();

        // Stop button (only when playing or paused)
        if (!m_PlayMode.IsStopped()) {
            ImGui::SameLine(0.0f, 4.0f);
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.2f, 0.2f, 1.0f));
            if (ImGui::Button(" Stop ")) {
                m_PendingPlayStop = true;
            }
            ImGui::PopStyleColor();
        }

        // Show play state indicator
        if (m_PlayMode.IsPlaying()) {
            ImGui::SameLine(0.0f, 8.0f);
            ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "[PLAYING]");
        } else if (m_PlayMode.IsPaused()) {
            ImGui::SameLine(0.0f, 8.0f);
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.3f, 1.0f), "[PAUSED]");
        }

        // Show collab status indicator
        if (m_CollabSystem.IsActive()) {
            ImGui::SameLine(0.0f, 8.0f);
            auto collabState = m_CollabSystem.GetState();
            ImVec4 collabColor = (collabState == Editor::CollabSessionState::Connected)
                ? ImVec4(0.3f, 0.8f, 1.0f, 1.0f)
                : ImVec4(1.0f, 0.8f, 0.3f, 1.0f);
            ImGui::TextColored(collabColor, "[COLLAB: %zu peers]", m_CollabSystem.GetPeers().size());
        }

        ImGui::EndMainMenuBar();
    }

    // Play mode diff dialog
    if (m_PlayMode.HasPendingDiff()) {
        DrawPlayModeDiffDialog();
    }

    // About dialog
    if (m_ShowAboutDialog) {
        ImGui::OpenPopup("About Enjin");
        m_ShowAboutDialog = false;
    }
    if (ImGui::BeginPopupModal("About Enjin", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("TEGE - The Enjin Game Engine");
        ImGui::Separator();
        ImGui::Text("Version %s", ENJIN_VERSION_STRING);
        ImGui::Spacing();
        ImGui::Text("A game engine built with C++20 and Vulkan.");
        ImGui::Spacing();
        ImGui::Text("Third-Party Libraries:");
        ImGui::BulletText("Vulkan SDK - Graphics API");
        ImGui::BulletText("GLFW - Window/Input");
        ImGui::BulletText("Dear ImGui - Editor UI");
        ImGui::BulletText("ImGuizmo - Transform Gizmos");
        ImGui::BulletText("Assimp - 3D Model Import");
        ImGui::BulletText("stb_image - Image Loading");
        ImGui::Spacing();
        ImGui::Separator();
        if (ImGui::Button("OK", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // Save as Template dialog
    if (ImGui::BeginPopupModal("SaveTemplatePopup", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Save current scene as a reusable template.");
        ImGui::Separator();

        static char templateNameBuf[128] = "My Template";
        ImGui::InputText("Template Name", templateNameBuf, sizeof(templateNameBuf));

        ImGui::Spacing();
        if (ImGui::Button("Save", ImVec2(120, 0))) {
            std::string name(templateNameBuf);
            if (!name.empty()) {
                SaveCustomTemplate(name);
                m_ConsoleLog.push_back("[Template] Saved template: " + name);
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}


} // namespace Editor
} // namespace Enjin
