#include "Enjin/Editor/EditorLayer.h"
#include "Enjin/Editor/InspectorUndo.h"
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
#include "Enjin/ECS/Components/BoneAttachment.h"
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
#include "Enjin/Renderer/Upscaling/IUpscaler.h"
#include "Enjin/Renderer/Upscaling/FSR2Upscaler.h"
#include "Enjin/Renderer/Upscaling/DLSSUpscaler.h"
#include "Enjin/Renderer/Upscaling/XeSSUpscaler.h"
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
#include <imgui_internal.h>
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

// Extern for VS node access to Water3D (owned by EditorLayer, wired on play start)
extern Enjin::Effects::Water3D* s_VisualScriptWater;

namespace Enjin {
namespace Editor {

// File-scope pointer for the log callback (same pattern as GLFW callbacks).
static EditorLayer* s_EditorLayerInstance = nullptr;

static void EditorLogCallback(LogLevel level, LogCategory category, const char* formatted) {
    if (!s_EditorLayerInstance) return;

    // formatted already ends with \n — strip it for console display
    std::string line(formatted);
    while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) {
        line.pop_back();
    }
    if (line.empty()) return;

    s_EditorLayerInstance->PushConsoleMessage(level, category, line);
}

// S19/S20/S23: Shell-escape a string for safe interpolation into shell commands (Unix only).
// Wraps the string in single quotes and escapes any embedded single quotes.
#ifndef _WIN32
static std::string ShellEscape(const std::string& s) {
    std::string result = "'";
    for (char c : s) {
        if (c == '\'') result += "'\\''";
        else result += c;
    }
    result += "'";
    return result;
}
#endif

EditorLayer::EditorLayer() {
}

EditorLayer::~EditorLayer() {
    Shutdown();
}

bool EditorLayer::Initialize(Window* window, Renderer::VulkanRenderer* renderer) {
    m_Window = window;
    m_Renderer = renderer;

    // Set file dialog owner so native dialogs appear on top of the editor window
    if (m_Window) {
        void* platformHandle = m_Window->GetPlatformWindowHandle();
        if (platformHandle) {
            FileDialog::SetOwnerWindow(platformHandle);
        }
    }

    m_ImGuiLayer = std::make_unique<GUI::ImGuiLayer>();
    if (!m_ImGuiLayer->Initialize(window, renderer)) {
        ENJIN_LOG_ERROR(Editor, "Failed to initialize ImGui layer");
        return false;
    }

    // Initialize play mode (will be fully set up when SetWorld/SetCamera are called)

    // Initialize weather system with more particles for better visibility
    m_WeatherSystem.Initialize(8000);  // Large pool for dense rain/snow

    // Initialize elemental system (connects to wind + weather for particle interactions)
    m_ElementalSystem.Initialize(&m_WindSystem, &m_WeatherSystem, &m_SeasonalWeather);

    // Wind system is always running (affects weather, vegetation, grass)
    // Will be connected to RenderSystem when SetRenderSystem is called

    // Render targets for Game View (offscreen rendering)
    // Scene RT: raw scene output (input to post-processing)
    // Game View RT: final post-processed output (displayed in ImGui)
    m_SceneRenderTarget = std::make_unique<Renderer::RenderTarget>();
    if (!m_SceneRenderTarget->Create(renderer, m_GameViewWidth, m_GameViewHeight)) {
        ENJIN_LOG_WARN(Editor, "Failed to create Scene render target");
        m_SceneRenderTarget.reset();
    }

    m_GameViewRenderTarget = std::make_unique<Renderer::RenderTarget>();
    if (!m_GameViewRenderTarget->Create(renderer, m_GameViewWidth, m_GameViewHeight)) {
        ENJIN_LOG_WARN(Editor, "Failed to create Game View render target");
        m_GameViewRenderTarget.reset();
    }

    // Editor viewport render target (offscreen rendering for scene editing camera)
    m_EditorViewportRT = std::make_unique<Renderer::RenderTarget>();
    if (!m_EditorViewportRT->Create(renderer, m_EditorViewportWidth, m_EditorViewportHeight)) {
        ENJIN_LOG_WARN(Editor, "Failed to create editor viewport render target");
        m_EditorViewportRT.reset();
    }

    // Post-processing pipeline (applies effects from scene RT to game view RT)
    if (m_GameViewRenderTarget && m_GameViewRenderTarget->IsValid()) {
        m_PostProcessing = std::make_unique<Renderer::PostProcessing>();
        if (!m_PostProcessing->Initialize(renderer->GetContext(),
                m_GameViewRenderTarget->GetPPRenderPass(),
                m_GameViewWidth, m_GameViewHeight, renderer)) {
            ENJIN_LOG_WARN(Editor, "Failed to initialize post-processing");
            m_PostProcessing.reset();
        } else {
            // Point post-processing at the scene render target's image
            if (m_SceneRenderTarget && m_SceneRenderTarget->IsValid()) {
                m_PostProcessing->UpdateSourceImage(
                    m_SceneRenderTarget->GetColorImageView(),
                    m_SceneRenderTarget->GetSampler());
            }
        }
    }

    // Load accessibility / editor settings and apply theme + scale
    m_EditorSettings.Load();
    m_ImGuiLayer->ApplyTheme(m_EditorSettings.theme, &m_EditorSettings.accentColors);
    m_ImGuiLayer->SetGlobalScale(m_EditorSettings.uiScale);
    Input::SetRawMouseInput(m_EditorSettings.rawMouseInput);
    Input::SetMouseSmoothing(m_EditorSettings.mouseSmoothing);
    m_SurfaceSnap = m_EditorSettings.surfaceSnap;
    m_SurfaceAlignNormal = m_EditorSettings.surfaceAlignNormal;
    if (!m_EditorSettings.windowIconPath.empty() && m_Window) {
        m_Window->SetIcon(m_EditorSettings.windowIconPath.c_str());
    }

    // Apply persisted layout
    m_VisiblePanels = static_cast<EditorPanel>(m_EditorSettings.visiblePanels);
    m_Layout.leftWidth = m_EditorSettings.leftPanelWidth;
    m_Layout.rightWidth = m_EditorSettings.rightPanelWidth;
    m_Layout.bottomHeight = m_EditorSettings.bottomPanelHeight;
    m_GizmoOperation = static_cast<GizmoOperation>(m_EditorSettings.gizmoOperation);
    m_GizmoSpace = static_cast<GizmoSpace>(m_EditorSettings.gizmoSpace);

    // Initialize in-game pause menu system
    m_GameMenu.SetInputMap(&m_InputMap);
    m_GameMenu.SetEditorSettings(&m_EditorSettings);
    m_GameMenu.SetCallback([this](const std::string& action) {
        if (action == "resume") {
            m_GameMenu.HideAll();
            m_PlayMode.Resume();
            if (m_FocusMode || SceneHasMouseLookController()) {
                m_GameViewMouseCaptured = !m_FocusMode;
                Input::SetMouseCaptured(true);
            }
        } else if (action == "options") {
            m_GameMenu.ShowScreen(GUI::MenuScreen::Options);
        } else if (action == "how_to_play") {
            m_GameMenu.ShowScreen(GUI::MenuScreen::HowToPlay);
        } else if (action == "quit_to_menu") {
            m_GameMenu.HideAll();
            m_PendingPlayStop = true;
        } else if (action == "quit") {
            if (m_Window) m_Window->Close();
        } else if (action == "game_over_restart") {
            // Restart: stop play mode and immediately start again (reloads scene)
            m_GameMenu.HideAll();
            m_PendingPlayStop = true;
            m_PendingPlayRestart = true;
        } else if (action == "game_over_menu") {
            // Return to menu: stop play mode
            m_GameMenu.HideAll();
            m_PendingPlayStop = true;
        }
    });

    // Register file drop callback for drag-and-drop import
    if (m_Window) {
        m_Window->SetDropCallback([this](int count, const char** paths) {
            OnFileDrop(count, paths);
        });
    }

    // Intercept window close to prompt for unsaved changes
    if (m_Window) {
        m_Window->SetCloseCallback([this]() -> bool {
            if (m_SceneDirty) {
                m_UnsavedChangesAction = UnsavedAction::Quit;
                m_ShowUnsavedChangesDialog = true;
                return false; // Cancel close — dialog will handle it
            }
            return true; // Allow close
        });
    }

    // Mark scene dirty whenever undo/redo state changes
    m_UndoRedo.SetStateChangedCallback([this]() {
        MarkDirty();
    });

    // Set initial window title
    UpdateWindowTitle();

    // Wire collaborative editing callbacks
    m_CollabSystem.SetOnRemoteEdit([this](const Editor::EditOperation& op) {
        if (!m_World) return;
        switch (op.type) {
            case Editor::EditOpType::CreateEntity: {
                if (!op.dataJson.empty()) {
                    Scene::SceneSerializer::DeserializeEntityFromString(m_World, op.dataJson);
                } else {
                    auto entity = m_World->CreateEntity();
                    m_World->AddComponent<ECS::TransformComponent>(entity);
                }
                break;
            }
            case Editor::EditOpType::DeleteEntity: {
                auto entity = static_cast<ECS::Entity>(op.entityId);
                if (m_World->GetComponent<ECS::TransformComponent>(entity)) {
                    m_World->DestroyEntity(entity);
                    DeselectEntity(entity);
                }
                break;
            }
            case Editor::EditOpType::RenameEntity: {
                auto entity = static_cast<ECS::Entity>(op.entityId);
                auto* name = m_World->GetComponent<ECS::NameComponent>(entity);
                if (name) name->name = op.dataJson;
                break;
            }
            case Editor::EditOpType::SetComponent: {
                auto entity = static_cast<ECS::Entity>(op.entityId);
                if (m_World->GetComponent<ECS::TransformComponent>(entity)) {
                    Scene::SceneSerializer::DeserializeOneComponent(
                        m_World, entity, op.componentKey, op.dataJson);
                }
                break;
            }
            case Editor::EditOpType::RemoveComponent: {
                // Component removal handled by key — remove the component type
                // For now, log it; full removal requires type registry lookup
                break;
            }
            case Editor::EditOpType::ModifyTransform: {
                auto entity = static_cast<ECS::Entity>(op.entityId);
                auto* xform = m_World->GetComponent<ECS::TransformComponent>(entity);
                if (xform) {
                    xform->position = op.position;
                    xform->rotation = Math::Quaternion::FromEuler(op.rotation);
                    xform->scale = op.scale;
                }
                break;
            }
            case Editor::EditOpType::SetParent: {
                // Parent-child relationships handled at scene level, not ECS World
                break;
            }
            default: break;
        }
    });
    m_CollabSystem.SetOnSceneSyncRequest([this]() -> std::string {
        if (!m_World) return "{}";
        Scene::SceneSerializer serializer(m_World);
        return serializer.SaveToString();
    });
    m_CollabSystem.SetOnSceneSyncReceived([this](const std::string& json) {
        if (!m_World) return;
        ClearSelection();
        Scene::SceneSerializer serializer(m_World);
        serializer.LoadFromString(json, true);
    });

    // Wire Logger output to the editor console panel
    s_EditorLayerInstance = this;
    Logger::Get().SetLogCallback(EditorLogCallback);

    // Check for crash report from previous session
    CheckForCrashReport();

    ENJIN_LOG_INFO(Editor, "EditorLayer initialized");
    return true;
}

void EditorLayer::SetRenderSystem(ECS::RenderSystem* renderSystem) {
    m_RenderSystem = renderSystem;

    // Wire fluid simulation into render system
    if (m_RenderSystem) {
        m_RenderSystem->SetFluidSimulation(&m_FluidSimulation);
    }

    // Initialize curl noise system
    m_CurlNoiseSystem = std::make_unique<Effects::CurlNoiseSystem>();
    if (m_World) {
        m_CurlNoiseSystem->Initialize(m_World);
    }

    // Wire nine-slice texture resolver for UI system
    m_UISystem.SetTextureResolver([this](const std::string& path, u32& outW, u32& outH) -> void* {
        if (path.empty() || !m_RenderSystem) return nullptr;
        auto tex = m_RenderSystem->LoadTexture(path);
        if (!tex || !tex->IsValid()) return nullptr;
        outW = tex->GetWidth();
        outH = tex->GetHeight();
        VkDescriptorSet ds = GetImGuiTexture(path);
        return reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(ds));
    });
}

void EditorLayer::StartPlayMode() {
    Scripting::SetBindingsWeather(&m_WeatherSystem);
    Scripting::SetBindingsSceneManager(&m_SceneManager);
    s_VisualScriptWater = &m_Water3D;
    m_CachedPlayerEntity = ECS::INVALID_ENTITY; // Invalidate cache for new play session
    m_PlayMode.Play();
}

void EditorLayer::InitializePlayMode() {
    if (m_World && m_Camera && m_CameraController) {
        m_PlayMode.Initialize(m_World, m_Camera, m_CameraController, &m_SceneManager);
        m_PlayMode.SetRenderSystem(m_RenderSystem);
        m_PlayMode.SetPostProcessing(m_PostProcessing.get());
        m_PlayMode.SetWeatherSystem(&m_WeatherSystem);
        m_PlayMode.SetElementalSystem(&m_ElementalSystem);
        m_PlayMode.SetParticleSystem(&m_ParticleSystem);
        m_PlayMode.SetSceneManager(&m_SceneManager);
        m_PlayMode.SetWater3D(&m_Water3D);
        m_PlayMode.SetFluidSimulation(&m_FluidSimulation);
        m_PlayMode.SetFluidTerrainCoupling(&m_FluidTerrainCoupling);
        m_PlayMode.SetCurlNoiseSystem(m_CurlNoiseSystem.get());
        m_PlayMode.SetEditorSettings(&m_EditorSettings);

        // Wire accessibility systems
        m_PlayMode.SetSubtitleSystem(&m_SubtitleSystem);
        m_PlayMode.SetAnnouncer(&m_Announcer);
        m_PlayMode.SetUISystem(&m_UISystem);
        m_PlayMode.SetAlternativeInput(&m_AlternativeInput);
        m_PlayMode.SetAudioIndicators(&m_AudioIndicators);

        // Configure subtitle system from editor settings
        Accessibility::SubtitleConfig subConfig;
        subConfig.enabled = m_EditorSettings.subtitlesEnabled;
        subConfig.captionsEnabled = m_EditorSettings.closedCaptionsEnabled;
        subConfig.fontSize = m_EditorSettings.subtitleFontSize;
        subConfig.backgroundOpacity = m_EditorSettings.subtitleBgOpacity;
        subConfig.showSpeakerNames = m_EditorSettings.subtitleSpeakerNames;
        m_SubtitleSystem.SetConfig(subConfig);

        // Wire accessibility input map and motion settings
        auto* ctrlSys = m_PlayMode.GetControllerSystem();
        if (ctrlSys) {
            ctrlSys->SetInputActionMap(&m_InputMap);
            ctrlSys->SetReducedMotion(m_EditorSettings.reducedMotion);
        }

        // Wire reduced motion to UISystem
        if (m_PlayMode.GetUISystem()) {
            m_PlayMode.GetUISystem()->SetReducedMotion(m_EditorSettings.reducedMotion);
        }

        // Apply sprint/crouch toggle modes from settings
        if (m_EditorSettings.sprintMode == 1) {
            m_InputMap.SetActionMode(InputSystem::GameAction::Sprint, InputSystem::ActionMode::Toggle);
        }
        if (m_EditorSettings.crouchMode == 1) {
            m_InputMap.SetActionMode(InputSystem::GameAction::Crouch, InputSystem::ActionMode::Toggle);
        }
    }
}

void EditorLayer::Shutdown() {
    // Persist current layout state before shutdown
    m_EditorSettings.visiblePanels = static_cast<u32>(m_VisiblePanels);
    m_EditorSettings.leftPanelWidth = m_Layout.leftWidth;
    m_EditorSettings.rightPanelWidth = m_Layout.rightWidth;
    m_EditorSettings.bottomPanelHeight = m_Layout.bottomHeight;
    m_EditorSettings.gizmoOperation = static_cast<u32>(m_GizmoOperation);
    m_EditorSettings.gizmoSpace = static_cast<u32>(m_GizmoSpace);
    m_EditorSettings.Save();

    // Disconnect log callback
    Logger::Get().SetLogCallback(nullptr);
    s_EditorLayerInstance = nullptr;

    // Save feedback data before shutdown
    if (m_FeedbackLoaded) {
        m_FeedbackManager.SaveAll();
    }

    // Destroy post-processing before render targets
    if (m_PostProcessing) {
        m_PostProcessing->Shutdown();
        m_PostProcessing.reset();
    }

    // Destroy render targets before ImGui (they use ImGui textures)
    if (m_SceneRenderTarget) {
        m_SceneRenderTarget->Destroy();
        m_SceneRenderTarget.reset();
    }
    if (m_GameViewRenderTarget) {
        m_GameViewRenderTarget->Destroy();
        m_GameViewRenderTarget.reset();
    }
    if (m_EditorViewportRT) {
        m_EditorViewportRT->Destroy();
        m_EditorViewportRT.reset();
    }

    // Clean up ImGui texture descriptors for sprite/tilemap previews
    CleanupImGuiTextureCache();

    if (m_ImGuiLayer) {
        m_ImGuiLayer->Shutdown();
        m_ImGuiLayer.reset();
    }
}

void EditorLayer::Update(f32 deltaTime) {
    // Begin profiler frame measurement
    Debug::Profiler::Instance().BeginFrame();

    // Handle deferred template application (requested during Render-phase ImGui).
    // World::Clear() must not run during Render to avoid invalidating GPU resources
    // still referenced by in-flight Vulkan command buffers.
    if (!m_PendingTemplateId.empty()) {
        std::string tmplId = std::move(m_PendingTemplateId);
        std::string scenePath = std::move(m_PendingSceneLoadPath);
        m_PendingTemplateId.clear();
        m_PendingSceneLoadPath.clear();
        ApplyTemplate(tmplId);
        if (!scenePath.empty()) {
            SaveScene(scenePath);
        }
    }

    // Handle deferred scene load (requested during Render-phase ImGui callbacks).
    // World::Clear() must not run during Render to avoid invalidating entity
    // references still in use by the current frame's draw calls.
    if (!m_PendingSceneLoadPath.empty()) {
        std::string path = std::move(m_PendingSceneLoadPath);
        m_PendingSceneLoadPath.clear();
        if (!m_PlayMode.IsStopped()) {
            if (m_Renderer) m_Renderer->WaitForAllFrames();
            m_PlayMode.Stop();
            ClearSelection();
        }
        OpenSceneImmediate(path);
    }

    // Handle deferred model import (requested during previous frame's Render).
    // The one-frame delay ensures the "Importing..." overlay is visible on screen
    // before the blocking import call runs.
    if (m_ImportPending) {
        m_ImportPending = false;
        ExecuteImport(m_ImportPendingPath, m_ImportPendingOptions);
        m_ImportPendingPath.clear();
    }

    // Handle deferred play mode stop (requested during previous frame's Render)
    if (m_PendingPlayStop) {
        m_PendingPlayStop = false;
        bool wantsRestart = m_PendingPlayRestart;
        m_PendingPlayRestart = false;
        if (!m_PlayMode.IsStopped()) {
            // Wait for GPU to finish all in-flight frames before Stop clears
            // the world — otherwise the GPU may read destroyed entity data.
            if (m_Renderer) m_Renderer->WaitForAllFrames();
            m_PlayMode.Stop();
            ClearSelection(); // Entities have new IDs after scene restore
            m_PrePlayRenderSettings.ApplyToRuntime(
                m_RenderSystem, m_PostProcessing ? &m_PostProcessing->GetSettings() : nullptr);
            if (m_FocusMode) {
                m_FocusMode = false;
                Input::SetMouseCaptured(false);
            }
            // Restore VSync state from settings
            if (m_Renderer) {
                m_Renderer->RequestVSyncChange(m_EditorSettings.editorVSync);
            }
            // Skip rendering this frame — the world was just rebuilt and the
            // render system's entity caches are stale.  Marking the flag lets
            // RenderOffscreen() early-out safely; caches refresh next frame.
            m_SkipNextRender = true;

            // If restart was requested, re-enter play mode next frame
            if (wantsRestart) {
                m_PendingPlayStart = true;
            }
        }
    }

    // Handle deferred play restart (game over -> restart)
    if (m_PendingPlayStart) {
        m_PendingPlayStart = false;
        if (m_PlayMode.IsStopped()) {
            m_PlayMode.Play();
        }
    }

    // Lazy-load feedback data on first frame
    if (!m_FeedbackLoaded) {
        m_FeedbackManager.LoadAll();
        m_FeedbackLoaded = true;
    }

    // Auto-save (only when dirty, not in play mode, has a save path)
    if (m_EditorSettings.autoSaveEnabled && m_SceneDirty &&
        !m_CurrentScenePath.empty() && m_PlayMode.IsStopped()) {
        m_AutoSaveTimer += deltaTime;
        if (m_AutoSaveTimer >= m_EditorSettings.autoSaveIntervalMinutes * 60.0f) {
            m_AutoSaveTimer = 0.0f;
            AutoSave();
        }
    }

    // Update input action map each frame
    m_InputMap.Update(deltaTime);

    // Update collaborative editing (process remote ops, broadcast transforms)
    if (m_CollabSystem.IsActive()) {
        m_CollabSystem.SetLocalCameraPosition(m_Camera ? m_Camera->GetPosition() : Math::Vector3());
        if (m_PrimarySelected != ECS::INVALID_ENTITY) {
            m_CollabSystem.SetLocalCursorEntity(m_PrimarySelected);
        }
        m_CollabSystem.Update(deltaTime);
    }

    // Track frame time history
    m_LastDeltaTime = deltaTime;
    f32 frameTimeMs = deltaTime * 1000.0f;
    m_FrameTimeHistory[m_FrameTimeIndex] = frameTimeMs;
    m_FrameTimeIndex = (m_FrameTimeIndex + 1) % FRAME_TIME_HISTORY_SIZE;

    // Calculate min/max/avg
    m_FrameTimeMin = 99999.0f;
    m_FrameTimeMax = 0.0f;
    f32 sum = 0.0f;
    u32 validCount = 0;
    for (usize i = 0; i < FRAME_TIME_HISTORY_SIZE; ++i) {
        f32 ft = m_FrameTimeHistory[i];
        if (ft > 0.0f) {  // Skip uninitialized entries
            if (ft < m_FrameTimeMin) m_FrameTimeMin = ft;
            if (ft > m_FrameTimeMax) m_FrameTimeMax = ft;
            sum += ft;
            ++validCount;
        }
    }
    if (validCount == 0) { m_FrameTimeMin = 0.0f; m_FrameTimeMax = 0.0f; }
    m_FrameTimeAvg = validCount > 0 ? sum / static_cast<f32>(validCount) : 0.0f;

    // Update performance metrics periodically (every 0.5s)
    m_PerfUpdateTimer += deltaTime;
    if (m_PerfUpdateTimer >= 0.5f) {
        m_PerfUpdateTimer = 0.0f;

        // Compute percentiles (P50, P95, P99) via sort — only every 0.5s, not every frame
        if (validCount >= 2) {
            f32 sorted[FRAME_TIME_HISTORY_SIZE];
            u32 n = 0;
            for (usize i = 0; i < FRAME_TIME_HISTORY_SIZE; ++i) {
                if (m_FrameTimeHistory[i] > 0.0f) sorted[n++] = m_FrameTimeHistory[i];
            }
            std::sort(sorted, sorted + n);
            m_FrameTimeP50 = sorted[n * 50 / 100];
            m_FrameTimeP95 = sorted[n * 95 / 100];
            m_FrameTimeP99 = sorted[std::min(n * 99 / 100, n - 1)];
        }

        Editor::PerformanceStats::UpdateSystemMemory(m_PerfMetrics);
        if (m_Renderer && m_Renderer->GetContext()) {
            Editor::PerformanceStats::QueryGPUMemory(m_Renderer->GetContext(), m_PerfMetrics);
        }
        if (m_RenderSystem) {
            m_PerfMetrics.drawCallCount = m_RenderSystem->GetDrawCallCount();
            m_PerfMetrics.triangleCount = m_RenderSystem->GetTriangleCount();
            m_PerfMetrics.descriptorCacheHits = m_RenderSystem->GetDescriptorCacheHits();
            m_PerfMetrics.descriptorCacheWrites = m_RenderSystem->GetDescriptorCacheWrites();
        }
    }

    // Feed counters to profiler
    if (m_World) {
        Debug::Profiler::Instance().SetEntityCount(static_cast<u32>(m_World->GetEntityCount()));
    }
    if (m_RenderSystem) {
        Debug::Profiler::Instance().SetDrawCalls(m_RenderSystem->GetDrawCallCount());
        Debug::Profiler::Instance().SetTriangleCount(m_RenderSystem->GetTriangleCount());
    }

    // Refresh scene locks periodically (every 5 seconds)
    m_LockRefreshTimer += deltaTime;
    if (m_LockRefreshTimer >= 5.0f) {
        m_LockRefreshTimer = 0.0f;
        m_SceneLockManager.Refresh();
    }

    // Dwell-click: auto-click after hovering in place
    if (m_EditorSettings.dwellClickEnabled && !m_PlayMode.IsPlaying()) {
        ImVec2 mousePos = ImGui::GetMousePos();
        f32 dist = std::sqrt(
            (mousePos.x - m_DwellPos.x) * (mousePos.x - m_DwellPos.x) +
            (mousePos.y - m_DwellPos.y) * (mousePos.y - m_DwellPos.y));
        if (dist > m_EditorSettings.clickThreshold) {
            m_DwellPos = mousePos;
            m_DwellTimer = 0.0f;
            m_DwellActive = false;
        } else {
            m_DwellTimer += deltaTime;
            if (m_DwellTimer >= m_EditorSettings.dwellClickDelay && !m_DwellActive) {
                m_DwellActive = true;
                // Simulate left click via ImGui IO
                ImGui::GetIO().AddMouseButtonEvent(0, true);
                ImGui::GetIO().AddMouseButtonEvent(0, false);
            }
        }
    }

    // Update scene transitions (fade in/out between scenes)
    m_SceneManager.UpdateTransition(deltaTime);

    // Update wind system (always ticks, affects weather + vegetation + grass)
    m_WindSystem.Update(deltaTime);
    if (m_RenderSystem && !m_RenderSystem->GetWindSystem()) {
        m_RenderSystem->SetWindSystem(&m_WindSystem);
    }

    // Update skeletal animators (advance bone animation each frame).
    // RenderSystem::Update() is not called by the editor because it handles
    // terrain/sprite/tilemap regeneration that the editor manages separately.
    // Skeletal animation must be ticked here so imported FBX models animate.
    if (m_World) {
        for (auto entity : m_World->GetEntitiesWithComponent<ECS::AnimatorComponent>()) {
            auto* animComp = m_World->GetComponent<ECS::AnimatorComponent>(entity);
            if (animComp) {
                animComp->Update(deltaTime);
            }
        }
    }

    // Camera controller handles its own input - disable during text input or gizmo use.
    // During active play (not paused), require RMB held so WASD doesn't conflict with game controllers.
    // When paused, allow free camera navigation in scene view.
    if (m_CameraController) {
        bool usingGizmo = ImGuizmo::IsUsing();
        bool activelyPlaying = m_PlayMode.IsPlaying();  // True only when running, not when paused
        bool canUseCamera = !activelyPlaying ||
            (Input::IsMouseButtonDown(MouseButton::Right) && !m_GameViewMouseCaptured);
        m_CameraController->SetEnabled(!ImGui::GetIO().WantTextInput && !usingGizmo && canUseCamera);

        // Set orbit target to selected entity position for MMB orbit
        if (m_PrimarySelected != ECS::INVALID_ENTITY && m_World && m_World->IsValid(m_PrimarySelected)) {
            auto* transform = m_World->GetComponent<ECS::TransformComponent>(m_PrimarySelected);
            if (transform) {
                m_CameraController->SetOrbitTarget(transform->position);
            }
        } else {
            // When no entity is selected, orbit around origin
            m_CameraController->SetOrbitTarget(Math::Vector3(0.0f, 0.0f, 0.0f));
        }

        m_CameraController->Update(deltaTime);
    }

    // Gizmo mode shortcuts (1=translate, 2=rotate, 3=scale, 4=toggle space)
    // Using number keys to avoid conflict with WASD camera movement
    // Use WantTextInput (not WantCaptureKeyboard) so shortcuts work when panels
    // have focus but no text field is being edited. WantCaptureKeyboard is true whenever
    // any ImGui window is focused, which blocks Delete/Ctrl+D/gizmo keys after clicking
    // in the hierarchy or any other panel.
    if (!ImGui::GetIO().WantTextInput) {
        if (Input::IsKeyPressed(KeyCode::Num1)) {
            m_GizmoOperation = GizmoOperation::Translate;
        }
        if (Input::IsKeyPressed(KeyCode::Num2)) {
            m_GizmoOperation = GizmoOperation::Rotate;
        }
        if (Input::IsKeyPressed(KeyCode::Num3)) {
            m_GizmoOperation = GizmoOperation::Scale;
        }
        if (Input::IsKeyPressed(KeyCode::Num4)) {
            // Toggle between local and world space
            m_GizmoSpace = (m_GizmoSpace == GizmoSpace::World) ? GizmoSpace::Local : GizmoSpace::World;
        }

        // Undo (Ctrl+Z) / Redo (Ctrl+Y or Ctrl+Shift+Z)
        if (Input::IsKeyDown(KeyCode::LeftControl)) {
            if (Input::IsKeyDown(KeyCode::LeftShift) && Input::IsKeyPressed(KeyCode::Z)) {
                m_UndoRedo.Redo();
            } else if (Input::IsKeyPressed(KeyCode::Z)) {
                m_UndoRedo.Undo();
            } else if (Input::IsKeyPressed(KeyCode::Y)) {
                m_UndoRedo.Redo();
            }
        }

        // Delete selected entities — show confirmation dialog
        if (Input::IsKeyPressed(KeyCode::Delete) && !m_SelectedEntities.empty()) {
            m_PendingDeleteEntities.assign(m_SelectedEntities.begin(), m_SelectedEntities.end());
            m_ShowDeleteConfirm = true;
        }

        // Duplicate selected entities (Ctrl+D)
        if (Input::IsKeyDown(KeyCode::LeftControl) && Input::IsKeyPressed(KeyCode::D)) {
            if (!m_SelectedEntities.empty()) {
                DuplicateSelectedEntities();
            }
        }

        // Save scene (Ctrl+S)
        if (Input::IsKeyDown(KeyCode::LeftControl) && Input::IsKeyPressed(KeyCode::S)) {
            if (!m_CurrentScenePath.empty()) {
                SaveScene(m_CurrentScenePath);
            } else {
                // No path yet — open Save As dialog
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

        // Focus on selected entity/entities (F key)
        if (Input::IsKeyPressed(KeyCode::F) && !m_SelectedEntities.empty()) {
            FocusOnSelection();
        }

        // Keyboard gizmo nudge (arrow keys when entity selected)
        if (m_EditorSettings.keyboardNavEnabled) {
            HandleKeyboardGizmoNudge();
        }

        // Panel focus shortcuts (Ctrl+1..5)
        if (m_EditorSettings.keyboardNavEnabled && Input::IsKeyDown(KeyCode::LeftControl)) {
            if (Input::IsKeyPressed(KeyCode::Num1)) m_FocusedPanel = FocusedPanel::Hierarchy;
            else if (Input::IsKeyPressed(KeyCode::Num2)) m_FocusedPanel = FocusedPanel::Inspector;
            else if (Input::IsKeyPressed(KeyCode::Num3)) m_FocusedPanel = FocusedPanel::Viewport;
            else if (Input::IsKeyPressed(KeyCode::Num4)) m_FocusedPanel = FocusedPanel::Console;
            else if (Input::IsKeyPressed(KeyCode::Num5)) m_FocusedPanel = FocusedPanel::AssetBrowser;
            m_ShowFocusRing = (m_FocusedPanel != FocusedPanel::None);
        }

        // Command palette (Ctrl+P)
        if (Input::IsKeyDown(KeyCode::LeftControl) && Input::IsKeyPressed(KeyCode::P)) {
            m_CommandPalette.Toggle();
        }

        // Keyboard shortcuts help (Ctrl+Shift+/)
        if (Input::IsKeyDown(KeyCode::LeftControl) && Input::IsKeyDown(KeyCode::LeftShift) &&
            Input::IsKeyPressed(KeyCode::Slash)) {
            m_ShowShortcutsHelp = !m_ShowShortcutsHelp;
            m_ShortcutSearchBuf[0] = '\0';
        }
    }

    // Quake-style drop-down console (backtick/tilde toggle)
    // Placed outside WantTextInput so the console itself can capture text input.
    if (Input::IsKeyPressed(KeyCode::GraveAccent)) {
        m_ShowDropConsole = !m_ShowDropConsole;
        if (m_ShowDropConsole) {
            m_DropConsoleInput[0] = '\0';
            m_DropConsoleHistoryPos = -1;
        }
    }

    // Register palette commands on first use
    if (!m_CommandsRegistered) {
        RegisterPaletteCommands();
        m_CommandsRegistered = true;
    }

    // Update alternative input devices
    m_AlternativeInput.Update(deltaTime);

    // Update gamepad editor navigation
    UpdateGamepadEditor(deltaTime);

    // Focus mode toggle (F11) and exit (Escape)
    // F11 toggles between editor view and fullscreen game view while playing
    // F1 = Debug HUD overlay, F2 = cycle detail level
    if (Input::IsKeyPressed(KeyCode::F1)) {
        m_ShowDebugOverlay = !m_ShowDebugOverlay;
    }
    if (Input::IsKeyPressed(KeyCode::F2)) {
        if (m_ShowDebugOverlay) {
            m_DebugOverlayDetail = (m_DebugOverlayDetail + 1) % 2;
        } else {
            m_ShowDebugOverlay = true;  // F2 also opens if closed
        }
    }

    // F5 = Quick Bug Report (instant capture and submit to GitHub)
    if (Input::IsKeyPressed(KeyCode::F5)) {
        QuickBugReport();
    }

    if (Input::IsKeyPressed(KeyCode::F11)) {
        m_FocusMode = !m_FocusMode;
        if (m_FocusMode) {
            if (m_PlayMode.IsStopped()) {
                m_PrePlayRenderSettings = Renderer::SceneRenderSettings::CaptureFromRuntime(
                    m_RenderSystem, m_PostProcessing ? &m_PostProcessing->GetSettings() : nullptr);
                StartPlayMode();  // Auto-play when entering focus mode
            }
            // Capture mouse for immersive gameplay (hides cursor, enables free look)
            Input::SetMouseCaptured(true);
        } else {
            // Leaving focus mode: release mouse capture
            Input::SetMouseCaptured(false);
        }
    }
    if (Input::IsKeyPressed(KeyCode::Escape)) {
        if (m_GameMenu.IsMenuOpen()) {
            // Menu open: close it, resume, recapture if needed
            m_GameMenu.HideAll();
            m_PlayMode.Resume();
            if (m_FocusMode || SceneHasMouseLookController()) {
                m_GameViewMouseCaptured = !m_FocusMode;
                Input::SetMouseCaptured(true);
            }
        } else if (m_PlayMode.IsPlaying()) {
            // Playing: single-press pause — release mouse + open menu
            m_GameViewMouseCaptured = false;
            Input::SetMouseCaptured(false);
            m_GameMenu.ShowScreen(GUI::MenuScreen::PauseMenu);
            m_PlayMode.Pause();
        } else if (m_PlayMode.IsPaused()) {
            // Paused without menu: stop play mode
            if (m_Renderer) m_Renderer->WaitForAllFrames();
            m_PlayMode.Stop();
            ClearSelection();
            m_PrePlayRenderSettings.ApplyToRuntime(
                m_RenderSystem, m_PostProcessing ? &m_PostProcessing->GetSettings() : nullptr);
        }
    }

    // Game View click-to-capture: when playing, clicking the Game View image
    // captures the mouse so FPS/TPS controllers receive mouse delta for look
    if (!m_FocusMode && !m_GameViewMouseCaptured &&
        (m_PlayMode.IsPlaying() || m_PlayMode.IsPaused()) &&
        m_GameViewHovered && Input::IsMouseButtonPressed(MouseButton::Left)) {
        if (SceneHasMouseLookController()) {
            m_GameViewMouseCaptured = true;
            Input::SetMouseCaptured(true);
        }
    }

    // Handle terrain brush painting (intercepts mouse before viewport picking)
    if (m_TerrainEditMode && m_PlayMode.IsStopped()) {
        HandleTerrainBrush(deltaTime);
    }

    // Handle tilemap brush painting (intercepts mouse before viewport picking)
    if (m_TilemapEditMode && m_PlayMode.IsStopped()) {
        HandleTilemapBrush();
    }

    // Handle UI editor viewport interaction (intercepts mouse before viewport picking)
    if (m_UIEditMode && m_PlayMode.IsStopped()) {
        HandleUIEditorInput();
    }

    // Deactivate UI edit mode when entering play mode
    if (!m_PlayMode.IsStopped() && m_UIEditMode) {
        m_UIEditMode = false;
    }

    // Handle viewport picking (left-click to select entities in editor viewport)
    // Skip viewport picking while terrain/tilemap/UI edit mode is active to prevent entity deselection
    if (!ImGuizmo::IsOver() && !m_TerrainEditMode && !m_TilemapEditMode && !m_UIEditMode) {
        HandleViewportPicking();
    }

    // Pass game view bounds, render system, and wind to FlowerSystem each frame
    {
        auto* flowerSys = m_PlayMode.GetFlowerSystem();
        flowerSys->SetGameViewBounds(m_GameViewImageMinX, m_GameViewImageMinY,
                                     m_GameViewImageMaxX, m_GameViewImageMaxY);
        flowerSys->SetRenderTargetSize(m_GameViewWidth, m_GameViewHeight);
        flowerSys->SetRenderSystem(m_RenderSystem);
        flowerSys->SetGameCameraEntity(m_SelectedGameCamera);
        flowerSys->SetWindSystem(&m_WindSystem);
    }

    // Update play mode
    m_PlayMode.Update(deltaTime);

    // Show game over screen when ready (player died or victory condition met)
    if (m_PlayMode.IsGameOverReady() && !m_GameMenu.IsGameOverScreen()) {
        // Find the GameOverComponent to get display parameters
        for (auto entity : m_World->GetEntitiesWithComponent<ECS::GameOverComponent>()) {
            auto* go = m_World->GetComponent<ECS::GameOverComponent>(entity);
            if (go && go->triggered && go->screenVisible) {
                const std::string& msg = go->won ? go->victoryMessage : go->defeatMessage;
                m_GameMenu.ShowGameOver(go->won, msg, go->allowRestart, go->returnToMenu);
                // Release mouse capture so the player can click buttons
                if (m_GameViewMouseCaptured) {
                    m_GameViewMouseCaptured = false;
                    Input::SetMouseCaptured(false);
                }
                break;
            }
        }
    }

    // Update audio during play mode
    if (!m_PlayMode.IsStopped()) {
        Audio::AudioManager::Get().Update();
    }

    // Safety net: release mouse capture if play mode stopped
    if (m_GameViewMouseCaptured && m_PlayMode.IsStopped()) {
        m_GameViewMouseCaptured = false;
        Input::SetMouseCaptured(false);
    }

    // Safety net: close pause menu if play mode stopped (e.g. via toolbar button)
    if (m_GameMenu.IsMenuOpen() && m_PlayMode.IsStopped()) {
        m_GameMenu.HideAll();
    }

    // Update dialogue typewriter during play mode
    if (m_PlayMode.IsPlaying()) {
        UpdateDialogue(deltaTime);
    }

    // Update post-processing time for animated effects (film grain, etc.)
    if (m_PostProcessing) {
        m_PostProcessing->Update(deltaTime);
    }

    // Weather is now updated per-camera in Game View panel (see DrawGameViewPanel)

    // Update splash screen timer
    if (m_ShowSplash) {
        m_SplashTimer += deltaTime;
        if (m_SplashTimer >= m_SplashDuration) {
            m_ShowSplash = false;
            LoadCustomTemplates();
            m_EditorFadeIn = 0.0f;  // Start editor fade-in
        }
    } else if (m_EditorFadeIn < 1.0f) {
        // Fade in the editor over 0.5 seconds
        m_EditorFadeIn += deltaTime * 2.0f;
        if (m_EditorFadeIn > 1.0f) m_EditorFadeIn = 1.0f;
    }

    // Sync RetroEffects settings to PostProcessSettings each frame
    if (m_PostProcessing && m_RetroEffects.IsEnabled()) {
        auto& settings = m_PostProcessing->GetSettings();

        // Dithering
        auto ditherPattern = m_RetroEffects.GetDitherPattern();
        settings.ditherEnabled = (ditherPattern != Effects::DitherPattern::None) ? 1 : 0;
        if (settings.ditherEnabled) {
            // Map enum to shader pattern index (0=Bayer2x2, 1=Bayer4x4, 2=Bayer8x8)
            switch (ditherPattern) {
                case Effects::DitherPattern::Bayer2x2: settings.ditherPattern = 0; break;
                case Effects::DitherPattern::Bayer4x4: settings.ditherPattern = 1; break;
                default: settings.ditherPattern = 2; break; // Bayer8x8 and others
            }
            settings.ditherStrength = 1.0f;
        }

        // Color quantization
        auto colorMode = m_RetroEffects.GetColorMode();
        settings.colorQuantEnabled = (colorMode != Effects::ColorMode::TrueColor) ? 1 : 0;
        if (settings.colorQuantEnabled) {
            switch (colorMode) {
                case Effects::ColorMode::HighColor:  settings.colorBitDepth = 5; break;
                case Effects::ColorMode::Palette256:  settings.colorBitDepth = 3; break;
                case Effects::ColorMode::Palette16:   settings.colorBitDepth = 2; break;
                case Effects::ColorMode::Monochrome:  settings.colorBitDepth = 1; break;
                default: settings.colorBitDepth = 8; break;
            }
        }

        // Resolution downscaling
        auto& res = m_RetroEffects.GetResolution();
        settings.resDownscaleEnabled = 1;
        settings.internalWidth = res.renderWidth;
        settings.internalHeight = res.renderHeight;
        settings.usePointFiltering = res.pointFiltering ? 1 : 0;

        // CRT
        auto& crt = m_RetroEffects.GetCRTSettings();
        settings.crtEnabled = crt.enabled ? 1 : 0;
        settings.scanlineIntensity = crt.scanlineIntensity;
        settings.scanlineWidth = crt.scanlineWidth;
        settings.crtCurvature = crt.curvedScreen ? crt.curvature : 0.0f;

        // CRT Phosphor
        settings.crtPhosphorEnabled = (crt.enabled && crt.phosphorGlow) ? 1 : 0;
        settings.crtMaskType = crt.maskType;
        settings.crtMaskPitch = crt.maskPitch;
        settings.crtBloomRadius = crt.bloomRadius;
        settings.crtBloomStrength = crt.bloomStrength;
        settings.crtBloomSigma = crt.bloomSigma;
        settings.crtTVL = crt.tvl;

        // VHS
        auto& vhs = m_RetroEffects.GetVHSSettings();
        settings.vhsEnabled = vhs.enabled ? 1 : 0;
        settings.vhsTrackingIntensity = vhs.trackingIntensity;
        settings.vhsTrackingSpeed = vhs.trackingSpeed;
        settings.vhsWobbleIntensity = vhs.wobbleIntensity;
        settings.vhsWobbleSpeed = vhs.wobbleSpeed;
        settings.vhsColorBleed = vhs.colorBleedAmount;
        settings.vhsNoiseIntensity = vhs.noiseIntensity;
        settings.vhsBlueShift = vhs.blueShift;
        settings.vhsScreenTear = vhs.screenTear ? 1 : 0;
        settings.vhsTearOffset = vhs.tearOffset;
        settings.vhsInterlacing = vhs.interlacing ? 1 : 0;
        settings.vhsTapeDropout = vhs.tapeDropout;

        // Sync per-object retro overrides to RenderSystem
        if (m_RenderSystem) {
            auto& affine = m_RetroEffects.GetAffineSettings();
            auto& jitter = m_RetroEffects.GetVertexJitter();
            m_RenderSystem->SetGlobalAffineTexturing(affine.enabled);
            m_RenderSystem->SetGlobalVertexSnapping(affine.vertexSnapping || jitter.enabled);
            m_RenderSystem->SetGlobalVertexSnapResolution(
                jitter.enabled ? static_cast<u8>(jitter.gridResolution) : 160);
            m_RenderSystem->SetGlobalGouraudOnly(m_RetroEffects.GetGouraudOnly());
            m_RenderSystem->SetGlobalUVQuantize(affine.enabled);
            m_RenderSystem->SetTexturePageSize(affine.texturePageSize);
            m_RenderSystem->SetDepthSortJitter(jitter.depthSortJitter);
        }
    } else if (m_PostProcessing) {
        // When retro effects are disabled, clear the retro post-process fields
        auto& settings = m_PostProcessing->GetSettings();
        settings.ditherEnabled = 0;
        settings.colorQuantEnabled = 0;
        settings.resDownscaleEnabled = 0;
        settings.crtEnabled = 0;
        settings.crtPhosphorEnabled = 0;
        settings.vhsEnabled = 0;
    }

    // Clear global retro overrides when retro is disabled
    if (!m_RetroEffects.IsEnabled() && m_RenderSystem) {
        m_RenderSystem->SetGlobalFlatShading(false);
        m_RenderSystem->SetGlobalAffineTexturing(false);
        m_RenderSystem->SetGlobalVertexSnapping(false);
        m_RenderSystem->SetGlobalStippleTransparency(false);
        m_RenderSystem->SetGlobalUVQuantize(false);
        m_RenderSystem->SetGlobalGouraudOnly(false);
        m_RenderSystem->SetTexturePageSize(0.0f);
        m_RenderSystem->SetDepthSortJitter(0.0f);
    }
}

void EditorLayer::PrepareRenderTargets() {
    // Resize render targets BEFORE command buffer recording to avoid
    // destroying/recreating Vulkan resources while a command buffer is active.
    // This prevents crashes with Vulkan hooks (OBS, RenderDoc) that hold
    // references to resources during command buffer recording.

    // After scene clear, drain ALL GPU work before any resize.
    // Previous frames' command buffers may still reference the old render target
    // resources. WaitForGPU ensures those are fully processed before we destroy them.
    if (m_RenderSystem && m_RenderSystem->IsSceneClearActive()) {
        auto* renderer = m_RenderSystem->GetRenderer();
        if (renderer && renderer->GetContext()) {
            renderer->GetContext()->WaitForGPU();
        }
    }

    // Editor viewport resize
    if (m_EditorViewportRT && m_EditorViewportRT->IsValid() &&
        m_EditorViewportWidth > 0 && m_EditorViewportHeight > 0) {
        constexpr u32 VP_RESIZE_THRESHOLD = 8;
        u32 curW = m_EditorViewportRT->GetWidth();
        u32 curH = m_EditorViewportRT->GetHeight();
        i32 dw = static_cast<i32>(m_EditorViewportWidth) - static_cast<i32>(curW);
        i32 dh = static_cast<i32>(m_EditorViewportHeight) - static_cast<i32>(curH);
        if ((dw < 0 ? -dw : dw) > VP_RESIZE_THRESHOLD ||
            (dh < 0 ? -dh : dh) > VP_RESIZE_THRESHOLD) {
            m_EditorViewportRT->Resize(m_EditorViewportWidth, m_EditorViewportHeight);
        }
    }

    // Game View resize
    if (!m_GameViewRenderTarget || !m_GameViewRenderTarget->IsValid()) return;
    if (m_GameViewWidth == 0 || m_GameViewHeight == 0) return;

    // In focus mode, render at full display resolution
    if (m_FocusMode) {
        ImGuiIO& io = ImGui::GetIO();
        m_GameViewWidth = static_cast<u32>(io.DisplaySize.x);
        m_GameViewHeight = static_cast<u32>(io.DisplaySize.y);
    }

    constexpr u32 RESIZE_THRESHOLD = 8;
    u32 currentW = m_GameViewRenderTarget->GetWidth();
    u32 currentH = m_GameViewRenderTarget->GetHeight();
    i32 diffW = static_cast<i32>(m_GameViewWidth) - static_cast<i32>(currentW);
    i32 diffH = static_cast<i32>(m_GameViewHeight) - static_cast<i32>(currentH);
    bool needsResize = (diffW < 0 ? -diffW : diffW) > RESIZE_THRESHOLD ||
                       (diffH < 0 ? -diffH : diffH) > RESIZE_THRESHOLD;

    if (needsResize) {
        if (m_SceneRenderTarget) {
            m_SceneRenderTarget->Resize(m_GameViewWidth, m_GameViewHeight);
        }
        m_GameViewRenderTarget->Resize(m_GameViewWidth, m_GameViewHeight);

        VkRenderPass effectRenderPass = (m_SceneRenderTarget && m_SceneRenderTarget->IsValid())
            ? m_SceneRenderTarget->GetRenderPass()
            : m_GameViewRenderTarget->GetRenderPass();

        if (m_RenderSystem) {
            m_RenderSystem->RecreateEffectPipelinesForRenderPass(effectRenderPass);
        }

        if (m_PostProcessing && m_SceneRenderTarget && m_SceneRenderTarget->IsValid()) {
            m_PostProcessing->OnResize(m_GameViewWidth, m_GameViewHeight);
            m_PostProcessing->UpdateSourceImage(
                m_SceneRenderTarget->GetColorImageView(),
                m_SceneRenderTarget->GetSampler());
            // Recreate PP pipeline against the new PP render pass (old handle was destroyed by resize)
            m_PostProcessing->UpdateRenderPass(m_GameViewRenderTarget->GetPPRenderPass());
        }
    }

    // One-shot: update effect pipelines for the appropriate render target's render pass
    if (!m_EffectPipelinesUpdated && m_RenderSystem) {
        VkRenderPass effectRenderPass = (m_SceneRenderTarget && m_SceneRenderTarget->IsValid())
            ? m_SceneRenderTarget->GetRenderPass()
            : m_GameViewRenderTarget->GetRenderPass();
        m_RenderSystem->RecreateEffectPipelinesForRenderPass(effectRenderPass);
        m_EffectPipelinesUpdated = true;
    }
}

void EditorLayer::RenderOffscreen(VkCommandBuffer commandBuffer) {
    ENJIN_PROFILE_SCOPE("Render");

    // Skip one frame after PlayMode::Stop — the world was just rebuilt and
    // the render system has stale entity caches that would crash.
    if (m_SkipNextRender) {
        m_SkipNextRender = false;
        return;
    }

    // Skip ALL offscreen rendering for a few frames after scene clear.
    // The GPU may still be processing command buffers that reference destroyed
    // resources from the previous scene. Waiting ensures those are fully drained.
    if (m_RenderSystem && !m_RenderSystem->IsGameViewReady()) {
        if (m_RenderSystem) m_RenderSystem->SetSkipMainPassRendering(true);
        return;
    }

    // --- Editor viewport: render scene from editor camera to offscreen RT ---
    // (Resize is handled by PrepareRenderTargets() before command buffer recording)
    if (m_EditorViewportRT && m_EditorViewportRT->IsValid() &&
        m_EditorViewportWidth > 0 && m_EditorViewportHeight > 0) {

        // Shadow pass for editor camera (must run before RT render pass)
        if (m_Camera && m_RenderSystem) {
            m_RenderSystem->RenderShadowPassForCamera(m_Camera);
        }

        // Render scene to editor viewport RT
        m_EditorViewportRT->Begin(commandBuffer);
        if (m_RenderSystem && m_Camera) {
            m_RenderSystem->RenderToTarget(m_EditorViewportRT.get(), m_Camera, 0);

            // Render grid lines into the RT (uses offscreen descriptors written by RenderToTarget)
            if (m_ShowGrid && m_GridVertexBuffer && m_GridVertexCount > 0) {
                u32 vtW = m_EditorViewportRT->GetWidth();
                u32 vtH = m_EditorViewportRT->GetHeight();
                bool is2D = m_SceneManager.GetProjectMode() == Scene::ProjectMode::Mode2D;

                // Rebuild grid mesh if needed
                if (!m_GridVertexBuffer || m_GridSize != m_BuiltGridSize ||
                    m_GridLines != m_BuiltGridLines || is2D != m_BuiltGridIs2D) {
                    BuildGridMesh();
                }
                if (m_GridVertexBuffer && m_GridVertexCount > 0) {
                    m_RenderSystem->RenderGridLines(m_GridVertexBuffer.get(), m_GridRegularCount,
                        0, Math::Vector3(0.22f, 0.22f, 0.22f), 0.47f, vtW, vtH);
                    m_RenderSystem->RenderGridLines(m_GridVertexBuffer.get(), 2,
                        m_GridAxisXStart, Math::Vector3(0.7f, 0.24f, 0.24f), 0.8f, vtW, vtH);
                    if (is2D) {
                        m_RenderSystem->RenderGridLines(m_GridVertexBuffer.get(), 2,
                            m_GridAxisZStart, Math::Vector3(0.24f, 0.7f, 0.24f), 0.8f, vtW, vtH);
                    } else {
                        m_RenderSystem->RenderGridLines(m_GridVertexBuffer.get(), 2,
                            m_GridAxisZStart, Math::Vector3(0.24f, 0.24f, 0.7f), 0.8f, vtW, vtH);
                    }
                }
            }

            // Render weather particles in editor viewport
            if (m_RenderSystem->GetMainPassWeather()) {
                bool isRain = m_RenderSystem->GetMainPassWeatherIsRain();
                m_RenderSystem->RenderWeatherParticles(
                    *m_RenderSystem->GetMainPassWeather(), isRain,
                    m_EditorViewportRT->GetWidth(), m_EditorViewportRT->GetHeight());
            }
        }
        m_EditorViewportRT->End(commandBuffer);
    }

    // Skip main pass rendering (editor viewport is now offscreen)
    if (m_RenderSystem) {
        m_RenderSystem->SetSkipMainPassRendering(true);
    }

    if (!m_GameViewRenderTarget || !m_GameViewRenderTarget->IsValid()) {
        return;
    }

    // Game View frame rate limiting (doesn't affect editor, only game view updates)
    {
        f64 currentTime = glfwGetTime();
        f64 targetInterval = 0.0;  // 0 = unlimited

        // VSync takes priority (simulates ~60fps)
        if (m_GameViewVSync) {
            targetInterval = 1.0 / 60.0;
        } else {
            // FPS options: 0=Max, 1=24, 2=30, 3=60, 4=120, 5=144, 6=240
            const i32 fpsValues[] = { 0, 24, 30, 60, 120, 144, 240 };
            i32 targetFPS = fpsValues[m_GameViewFPSIndex];
            if (targetFPS > 0) {
                targetInterval = 1.0 / static_cast<f64>(targetFPS);
            }
        }

        // Skip render if not enough time has passed
        if (targetInterval > 0.0) {
            f64 elapsed = currentTime - m_GameViewLastRenderTime;
            if (elapsed < targetInterval) {
                return;  // Skip this frame, keep previous render target content
            }
        }
        m_GameViewLastRenderTime = currentTime;
    }

    auto renderTimingStart = std::chrono::high_resolution_clock::now();

    // Render target resize is handled by PrepareRenderTargets() before command buffer recording.

    // Find game camera entity (use user-selected camera, or fall back to active camera)
    if (!m_World || !m_RenderSystem) {
        return;
    }

    ECS::Entity gameCameraEntity = m_SelectedGameCamera;
    // Validate selected camera still exists and has a CameraComponent
    if (gameCameraEntity != ECS::INVALID_ENTITY) {
        if (!m_World->HasComponent<ECS::CameraComponent>(gameCameraEntity)) {
            gameCameraEntity = ECS::INVALID_ENTITY;
            m_SelectedGameCamera = ECS::INVALID_ENTITY;
        }
    }
    // Fall back to active camera if no selection
    if (gameCameraEntity == ECS::INVALID_ENTITY) {
        gameCameraEntity = ECS::CameraManager::GetActiveCamera(m_World);
    }
    if (gameCameraEntity == ECS::INVALID_ENTITY) {
        return;
    }

    // Camera zone detection: find the player entity and check CameraTrigger zones
    m_CameraZoneOverride = ECS::INVALID_ENTITY;
    {
        // Use cached player entity; re-scan only if invalid
        if (m_CachedPlayerEntity == ECS::INVALID_ENTITY || !m_World->IsValid(m_CachedPlayerEntity) ||
            (!m_World->HasComponent<ECS::Platformer2DController>(m_CachedPlayerEntity) &&
             !m_World->HasComponent<ECS::TopDown2DController>(m_CachedPlayerEntity) &&
             !m_World->HasComponent<ECS::TopDown3DController>(m_CachedPlayerEntity) &&
             !m_World->HasComponent<ECS::ThirdPersonController>(m_CachedPlayerEntity) &&
             !m_World->HasComponent<ECS::FirstPersonController>(m_CachedPlayerEntity))) {
            m_CachedPlayerEntity = ECS::INVALID_ENTITY;
            auto tryFindController = [&](auto entities) {
                for (ECS::Entity entity : entities) {
                    m_CachedPlayerEntity = entity;
                    return;
                }
            };
            tryFindController(m_World->GetEntitiesWithComponent<ECS::Platformer2DController>());
            if (m_CachedPlayerEntity == ECS::INVALID_ENTITY)
                tryFindController(m_World->GetEntitiesWithComponent<ECS::TopDown2DController>());
            if (m_CachedPlayerEntity == ECS::INVALID_ENTITY)
                tryFindController(m_World->GetEntitiesWithComponent<ECS::TopDown3DController>());
            if (m_CachedPlayerEntity == ECS::INVALID_ENTITY)
                tryFindController(m_World->GetEntitiesWithComponent<ECS::ThirdPersonController>());
            if (m_CachedPlayerEntity == ECS::INVALID_ENTITY)
                tryFindController(m_World->GetEntitiesWithComponent<ECS::FirstPersonController>());
        }
        ECS::Entity playerEntity = m_CachedPlayerEntity;

        if (playerEntity != ECS::INVALID_ENTITY) {
            auto* playerTransform = m_World->GetComponent<ECS::TransformComponent>(playerEntity);
            if (playerTransform) {
                i32 bestCamPriority = INT_MIN;
                for (ECS::Entity entity : m_World->GetEntitiesWithComponent<ECS::CameraTriggerComponent>()) {
                    auto* trigger = m_World->GetComponent<ECS::CameraTriggerComponent>(entity);
                    auto* trigTransform = m_World->GetComponent<ECS::TransformComponent>(entity);
                    if (trigger && trigTransform && trigger->priority > bestCamPriority) {
                        if (trigger->ContainsPoint(trigTransform->position, playerTransform->position)) {
                            // Validate the target camera exists
                            if (trigger->targetCamera != ECS::INVALID_ENTITY &&
                                m_World->HasComponent<ECS::CameraComponent>(trigger->targetCamera)) {
                                m_CameraZoneOverride = trigger->targetCamera;
                                bestCamPriority = trigger->priority;
                            }
                        }
                    }
                }
            }
        }

        // Override game camera if a zone-driven camera was found
        if (m_CameraZoneOverride != ECS::INVALID_ENTITY) {
            gameCameraEntity = m_CameraZoneOverride;
        }
    }

    if (!m_World->IsValid(gameCameraEntity)) return;
    auto* cameraComp = m_World->GetComponent<ECS::CameraComponent>(gameCameraEntity);
    auto* cameraTransform = m_World->GetComponent<ECS::TransformComponent>(gameCameraEntity);
    if (!cameraComp || !cameraTransform) {
        return;
    }

    // Build a temporary Camera object from the CameraComponent + TransformComponent
    Renderer::Camera gameCamera;
    f32 aspect = cameraComp->GetAspectRatio(m_GameViewWidth, m_GameViewHeight);

    if (cameraComp->projectionType == ECS::ProjectionType::Perspective) {
        gameCamera.SetPerspective(cameraComp->fieldOfView, aspect,
                                   cameraComp->nearPlane, cameraComp->farPlane);
    } else {
        f32 halfH = cameraComp->orthoSize;
        f32 halfW = halfH * aspect;
        gameCamera.SetOrthographic(-halfW, halfW, -halfH, halfH,
                                    cameraComp->nearPlane, cameraComp->farPlane);
    }

    // Set camera position and orientation from entity transform
    gameCamera.SetPosition(cameraTransform->position);

    // Compute forward/up from the entity's rotation quaternion
    Math::Vector3 forward = cameraTransform->rotation.Rotate(Math::Vector3(0.0f, 0.0f, -1.0f));
    Math::Vector3 up = cameraTransform->rotation.Rotate(Math::Vector3(0.0f, 1.0f, 0.0f));
    Math::Vector3 target = cameraTransform->position + forward;
    gameCamera.SetLookAt(cameraTransform->position, target, up);

    // Find active weather zone containing the game camera
    ECS::WeatherZoneComponent* activeWeatherZone = nullptr;
    i32 bestWeatherPriority = INT_MIN;

    for (ECS::Entity entity : m_World->GetEntitiesWithComponent<ECS::WeatherZoneComponent>()) {
        auto* zone = m_World->GetComponent<ECS::WeatherZoneComponent>(entity);
        auto* zoneTransform = m_World->GetComponent<ECS::TransformComponent>(entity);
        if (zone && zoneTransform && zone->priority > bestWeatherPriority) {
            if (zone->ContainsPoint(zoneTransform->position, cameraTransform->position)) {
                activeWeatherZone = zone;
                bestWeatherPriority = zone->priority;
            }
        }
    }

    // Find active temperature zone containing the game camera
    ECS::TemperatureZoneComponent* activeTempZone = nullptr;
    i32 bestTempPriority = INT_MIN;

    for (ECS::Entity entity : m_World->GetEntitiesWithComponent<ECS::TemperatureZoneComponent>()) {
        auto* zone = m_World->GetComponent<ECS::TemperatureZoneComponent>(entity);
        auto* zoneTransform = m_World->GetComponent<ECS::TransformComponent>(entity);
        if (zone && zoneTransform && zone->priority > bestTempPriority) {
            if (zone->ContainsPoint(zoneTransform->position, cameraTransform->position)) {
                activeTempZone = zone;
                bestTempPriority = zone->priority;
            }
        }
    }

    // Configure weather system from active zone
    bool hasWeatherParticles = false;
    bool isRain = false;
    if (activeWeatherZone && activeWeatherZone->weatherType > 0) {
        Effects::WeatherType wType = static_cast<Effects::WeatherType>(activeWeatherZone->weatherType);

        // Check if temperature zone overrides precipitation type
        // Weather types: 2=Rain, 3=HeavyRain, 4=Snow, 6=Storm
        bool hasPrecipitation = (activeWeatherZone->weatherType == 2 ||
                                  activeWeatherZone->weatherType == 3 ||
                                  activeWeatherZone->weatherType == 4 ||
                                  activeWeatherZone->weatherType == 6);

        if (hasPrecipitation && activeTempZone) {
            f32 temp = activeTempZone->temperature;
            if (temp <= 0.0f) {
                // Freezing: force snow regardless of weather zone type
                wType = Effects::WeatherType::Snow;
                m_WeatherSystem.SetWeather(wType, 0.1f);
                m_WeatherSystem.SetRainIntensity(0.0f);
                f32 snowInt = (activeWeatherZone->weatherType == 4)
                    ? activeWeatherZone->snowIntensity
                    : activeWeatherZone->rainIntensity;
                m_WeatherSystem.SetSnowIntensity(snowInt);
                isRain = false;
            } else if (temp <= 5.0f) {
                // Near-freezing: sleet mix (both rain and snow at reduced intensity)
                f32 blend = temp / 5.0f;  // 0 at 0C, 1 at 5C
                f32 baseIntensity = (activeWeatherZone->weatherType == 4)
                    ? activeWeatherZone->snowIntensity
                    : activeWeatherZone->rainIntensity;
                m_WeatherSystem.SetWeather(wType, 0.1f);
                m_WeatherSystem.SetRainIntensity(baseIntensity * blend);
                m_WeatherSystem.SetSnowIntensity(baseIntensity * (1.0f - blend));
                isRain = (blend > 0.5f);
            } else {
                // Warm: force rain regardless of weather zone type
                wType = (activeWeatherZone->weatherType == 6)
                    ? Effects::WeatherType::Storm
                    : Effects::WeatherType::Rain;
                m_WeatherSystem.SetWeather(wType, 0.1f);
                f32 rainInt = (activeWeatherZone->weatherType == 4)
                    ? activeWeatherZone->snowIntensity
                    : activeWeatherZone->rainIntensity;
                m_WeatherSystem.SetRainIntensity(rainInt);
                m_WeatherSystem.SetSnowIntensity(0.0f);
                isRain = true;
            }
        } else {
            // No temperature zone override - use weather zone as-is
            m_WeatherSystem.SetWeather(wType, 0.1f);

            if (activeWeatherZone->weatherType == 2 || activeWeatherZone->weatherType == 3 ||
                activeWeatherZone->weatherType == 6) {
                m_WeatherSystem.SetRainIntensity(activeWeatherZone->rainIntensity);
                m_WeatherSystem.SetSnowIntensity(0.0f);
                isRain = true;
            } else if (activeWeatherZone->weatherType == 4) {
                m_WeatherSystem.SetRainIntensity(0.0f);
                m_WeatherSystem.SetSnowIntensity(activeWeatherZone->snowIntensity);
            } else {
                m_WeatherSystem.SetRainIntensity(0.0f);
                m_WeatherSystem.SetSnowIntensity(0.0f);
            }
        }
        m_WeatherSystem.SetFogDensity(activeWeatherZone->fogDensity);
        m_WeatherSystem.SetFogColor(activeWeatherZone->fogColor);
        m_WeatherSystem.SetFogStart(activeWeatherZone->fogStart);
        m_WeatherSystem.SetFogEnd(activeWeatherZone->fogEnd);

        m_WindSystem.SetZoneOverride(activeWeatherZone->windDirection, activeWeatherZone->windStrength);
        m_WeatherSystem.SetWindDirection(activeWeatherZone->windDirection);
        m_WeatherSystem.SetWindStrength(activeWeatherZone->windStrength);

        if (activeWeatherZone->lightningEnabled) {
            m_WeatherSystem.SetLightningInterval(
                activeWeatherZone->lightningMinInterval,
                activeWeatherZone->lightningMaxInterval);
        }

        m_WeatherSystem.Update(m_LastDeltaTime, cameraTransform->position);

        hasWeatherParticles = (activeWeatherZone->weatherType == 2 ||
                               activeWeatherZone->weatherType == 3 ||
                               activeWeatherZone->weatherType == 4 ||
                               activeWeatherZone->weatherType == 6);

        // Feed fog parameters to render system for shader-based fog
        m_RenderSystem->SetFogParams(activeWeatherZone->fogDensity,
                                     activeWeatherZone->fogStart,
                                     activeWeatherZone->fogEnd, 0.1f);
        m_RenderSystem->SetFogColor(activeWeatherZone->fogColor);

        // Feed snow intensity for surface accumulation (temperature-aware)
        f32 snowAccum = 0.0f;
        if (activeTempZone && hasPrecipitation) {
            // Temperature zone drives whether snow accumulates
            if (activeTempZone->temperature <= 0.0f) {
                f32 intensity = (activeWeatherZone->weatherType == 4)
                    ? activeWeatherZone->snowIntensity
                    : activeWeatherZone->rainIntensity;
                snowAccum = intensity;
            } else if (activeTempZone->temperature <= 5.0f) {
                f32 blend = activeTempZone->temperature / 5.0f;
                f32 intensity = (activeWeatherZone->weatherType == 4)
                    ? activeWeatherZone->snowIntensity
                    : activeWeatherZone->rainIntensity;
                snowAccum = intensity * (1.0f - blend);
            }
        } else if (activeWeatherZone->weatherType == 4) {
            snowAccum = activeWeatherZone->snowIntensity;
        }
        m_RenderSystem->SetSnowIntensity(snowAccum);
    } else {
        m_WeatherSystem.SetWeather(Effects::WeatherType::Clear, 0.5f);
        m_WindSystem.ClearZoneOverride();
        m_RenderSystem->SetFogParams(0.0f, 20.0f, 100.0f, 0.1f);
        m_RenderSystem->SetFogColor(Math::Vector3(0.5f, 0.5f, 0.6f));
        m_RenderSystem->SetSnowIntensity(0.0f);
    }

    // Water freeze/thaw driven by temperature zones
    for (ECS::Entity waterEntity : m_World->GetEntitiesWithComponent<ECS::WaterVolumeComponent>()) {
        auto* waterVol = m_World->GetComponent<ECS::WaterVolumeComponent>(waterEntity);
        auto* waterTransform = m_World->GetComponent<ECS::TransformComponent>(waterEntity);
        if (!waterVol || !waterTransform) continue;

        // Find highest-priority temperature zone containing this water entity
        ECS::TemperatureZoneComponent* waterTempZone = nullptr;
        i32 bestWaterTempPri = INT_MIN;
        for (ECS::Entity tzEntity : m_World->GetEntitiesWithComponent<ECS::TemperatureZoneComponent>()) {
            auto* tz = m_World->GetComponent<ECS::TemperatureZoneComponent>(tzEntity);
            auto* tzTransform = m_World->GetComponent<ECS::TransformComponent>(tzEntity);
            if (tz && tzTransform && tz->priority > bestWaterTempPri) {
                if (tz->ContainsPoint(tzTransform->position, waterTransform->position)) {
                    waterTempZone = tz;
                    bestWaterTempPri = tz->priority;
                }
            }
        }

        if (waterTempZone && waterTempZone->IsFreezing()) {
            // Freezing: increase freeze progress
            waterVol->freezeProgress += waterVol->freezeRate * m_LastDeltaTime;
            if (waterVol->freezeProgress > 1.0f) waterVol->freezeProgress = 1.0f;
        } else if (waterTempZone && waterTempZone->IsNearFreezing()) {
            // Near-freezing (0-5C): lerp toward partial freeze (0.3)
            f32 target = 0.3f;
            if (waterVol->freezeProgress < target) {
                waterVol->freezeProgress += waterVol->freezeRate * 0.5f * m_LastDeltaTime;
                if (waterVol->freezeProgress > target) waterVol->freezeProgress = target;
            } else {
                waterVol->freezeProgress -= waterVol->thawRate * 0.5f * m_LastDeltaTime;
                if (waterVol->freezeProgress < target) waterVol->freezeProgress = target;
            }
        } else {
            // Warm or no zone: thaw
            waterVol->freezeProgress -= waterVol->thawRate * m_LastDeltaTime;
            if (waterVol->freezeProgress < 0.0f) waterVol->freezeProgress = 0.0f;
        }
        waterVol->isFrozen = (waterVol->freezeProgress >= 0.99f);
    }

    // World Time System: advance clock and update sun/ambient
    if (m_WorldTimeEnabled) {
        m_WorldTime.Update(m_LastDeltaTime);

        const auto& timeState = m_WorldTime.GetState();

        // Override sun direction on the first directional light
        Math::Vector3 sunDir = m_WorldTime.GetSunDirection();
        for (ECS::Entity entity : m_World->GetEntitiesWithComponent<ECS::LightComponent>()) {
            auto* light = m_World->GetComponent<ECS::LightComponent>(entity);
            auto* lightTransform = m_World->GetComponent<ECS::TransformComponent>(entity);
            if (light && light->type == ECS::LightType::Directional && lightTransform) {
                // Encode sun direction into rotation
                lightTransform->rotation = Math::Quaternion::FromEuler(
                    Math::Vector3(
                        std::asin(-sunDir.y) * 57.29578f,
                        std::atan2(-sunDir.x, -sunDir.z) * 57.29578f,
                        0.0f
                    ));
                light->intensity = m_WorldTime.GetAmbientIntensity() * 1.5f;
                light->color = Math::Vector3(1.0f, 0.95f, 0.9f);
                if (timeState.isNight) {
                    light->color = Math::Vector3(0.3f, 0.35f, 0.5f);
                    light->intensity = 0.3f;
                }
                break;
            }
        }

        // Update ambient
        m_RenderSystem->SetAmbientColor(m_WorldTime.GetAmbientColor());
        m_RenderSystem->SetAmbientIntensity(m_WorldTime.GetAmbientIntensity());
    }

    // Seasonal Weather System: temperature and weather transitions
    if (m_WorldTimeEnabled && m_SeasonalWeatherEnabled && !activeWeatherZone) {
        m_SeasonalWeather.Update(m_LastDeltaTime, m_WorldTime.GetState(), m_WeatherSystem);
    }

    // World curvature
    if (m_WorldCurvatureEnabled) {
        m_RenderSystem->SetWorldCurvature(m_WorldCurvature);
    } else {
        m_RenderSystem->SetWorldCurvature(0.0f);
    }

    // Pass season state to tree renderer
    if (m_WorldTimeEnabled && m_RenderSystem) {
        auto* treeRenderer = m_RenderSystem->GetTreeRenderer();
        if (treeRenderer) {
            treeRenderer->SetSeasonState(m_WorldTime.GetCurrentSeason(), m_WorldTime.GetSeasonProgress());
        }
    }

    // Notify render system whether rain is active (drives water ripple shader)
    m_RenderSystem->SetRainActive(isRain);

    // Update particle emitter simulation
    m_ParticleSystem.Update(m_LastDeltaTime, m_World);

    // Update parallax scrolling backgrounds (auto-scroll advance)
    m_ParallaxSystem.Update(m_LastDeltaTime);

    // Update elemental system (fire/water/earth/air particle simulation)
    if (cameraTransform) {
        // Register fire thermal feedback to wind system
        m_WindSystem.ClearHeatSources();
        const auto& elemPool = m_ElementalSystem.GetPool();
        for (u32 i = 0; i < elemPool.activeCount && i < 8192; ++i) {
            if (elemPool.elements[i].x > 0.5f && elemPool.intensities[i] > 0.3f) {
                m_WindSystem.RegisterHeatSource(elemPool.positions[i], elemPool.intensities[i]);
            }
        }
        m_ElementalSystem.Update(m_World, m_LastDeltaTime, cameraTransform->position);
    }

    // Update fluid simulation
    m_FluidSimulation.Update(m_LastDeltaTime, m_World);

    // Update fluid-terrain coupling (erosion/deposition)
    m_FluidTerrainCoupling.Update(m_LastDeltaTime, m_World, m_FluidSimulation);

    // Update curl noise flow fields
    if (m_CurlNoiseSystem) m_CurlNoiseSystem->Update(m_LastDeltaTime);

    u32 rtWidth = m_GameViewRenderTarget->GetWidth();
    u32 rtHeight = m_GameViewRenderTarget->GetHeight();

    // Evaluate post-process volumes: blend active volumes into the current PP settings
    if (m_PostProcessing && m_World && m_Camera) {
        EvaluatePostProcessVolumes(m_Camera->GetPosition());
    }

    // Always render to scene RT then copy to game view RT.
    // Never render directly to game view RT's MRT render pass (causes teal on NVIDIA).
    bool usePostProcessing = m_SceneRenderTarget && m_SceneRenderTarget->IsValid();
    bool usePPShader = usePostProcessing && m_PostProcessing &&
                       m_PostProcessing->IsInitialized();

    // Choose render target: scene RT when post-processing is active, game view RT otherwise
    Renderer::RenderTarget* sceneTarget = usePostProcessing
        ? m_SceneRenderTarget.get()
        : m_GameViewRenderTarget.get();

    // Check for splitscreen: multiple active cameras with non-default viewport rects
    bool useSplitscreen = false;
    std::vector<ECS::ViewportCamera> splitViewports;
    {
        auto allCameras = ECS::CameraManager::GetAllActiveCameras(m_World);
        if (allCameras.size() > 1) {
            for (auto camEntity : allCameras) {
                auto* cc = m_World->GetComponent<ECS::CameraComponent>(camEntity);
                if (cc && (cc->viewportX != 0.0f || cc->viewportY != 0.0f ||
                           cc->viewportWidth != 1.0f || cc->viewportHeight != 1.0f)) {
                    useSplitscreen = true;
                    break;
                }
            }
            if (useSplitscreen) {
                for (auto camEntity : allCameras) {
                    auto* cc = m_World->GetComponent<ECS::CameraComponent>(camEntity);
                    if (!cc) continue;
                    ECS::ViewportCamera vc;
                    vc.entity = camEntity;
                    vc.viewportX = cc->viewportX;
                    vc.viewportY = cc->viewportY;
                    vc.viewportWidth = cc->viewportWidth;
                    vc.viewportHeight = cc->viewportHeight;
                    splitViewports.push_back(vc);
                    if (splitViewports.size() >= ECS::RenderSystem::MAX_SPLITSCREEN_VIEWPORTS) break;
                }
            }
        }
    }

    m_RenderSystem->RenderShadowPassForCamera(&gameCamera);

    // Render scene + effects into the chosen target
    sceneTarget->Begin(commandBuffer);
    if (useSplitscreen && !splitViewports.empty()) {
        m_RenderSystem->RenderSplitscreen(sceneTarget, splitViewports);
        if (hasWeatherParticles) {
            m_RenderSystem->RenderWeatherParticles(m_WeatherSystem, isRain, rtWidth, rtHeight);
        }
        m_RenderSystem->RenderElementalParticles(m_ElementalSystem, rtWidth, rtHeight);
    } else {
        m_RenderSystem->RenderToTarget(sceneTarget, &gameCamera, 1);
        if (hasWeatherParticles) {
            m_RenderSystem->RenderWeatherParticles(m_WeatherSystem, isRain, rtWidth, rtHeight);
        }
        m_RenderSystem->RenderElementalParticles(m_ElementalSystem, rtWidth, rtHeight);
    }
    sceneTarget->End(commandBuffer);

    // Apply post-processing: read from scene RT, write to game view RT
    if (usePostProcessing) {
        // Pass camera planes for depth linearization (DoF/Tilt-Shift)
        if (m_Camera) {
            m_PostProcessing->SetCameraPlanes(m_Camera->GetNearPlane(), m_Camera->GetFarPlane());
        }

        // Pass inverse view-projection + light data for screen-space effects
        {
            Math::Matrix4 viewMat = gameCamera.GetViewMatrix();
            Math::Matrix4 projMat = gameCamera.GetProjectionMatrix();
            Math::Matrix4 vp = projMat * viewMat;
            Math::Matrix4 invVP = vp.Inverse();
            // Extract columns (column-major: m[0..3]=col0, m[4..7]=col1, etc.)
            m_PostProcessing->SetInverseViewProjection(
                Math::Vector4(invVP.m[0], invVP.m[1], invVP.m[2], invVP.m[3]),
                Math::Vector4(invVP.m[4], invVP.m[5], invVP.m[6], invVP.m[7]),
                Math::Vector4(invVP.m[8], invVP.m[9], invVP.m[10], invVP.m[11]),
                Math::Vector4(invVP.m[12], invVP.m[13], invVP.m[14], invVP.m[15]));

            // Find first directional light for god rays / contact shadows / fog shafts
            Math::Vector3 lightDir(0.0f, -1.0f, 0.0f);
            for (ECS::Entity e : m_World->GetEntitiesWithComponent<ECS::LightComponent>()) {
                auto* light = m_World->GetComponent<ECS::LightComponent>(e);
                auto* lt = m_World->GetComponent<ECS::TransformComponent>(e);
                if (light && lt && light->type == ECS::LightType::Directional) {
                    lightDir = lt->rotation.Rotate(Math::Vector3(0.0f, 0.0f, -1.0f));
                    break;
                }
            }
            m_PostProcessing->SetLightDirection(Math::Vector3(-lightDir.x, -lightDir.y, -lightDir.z));

            // Project light position to screen space for god rays
            // Use a far-away point in light direction as the "sun" position
            Math::Vector3 camPos = gameCamera.GetPosition();
            Math::Vector3 sunWorldPos = camPos - lightDir * 1000.0f;
            Math::Vector4 sunClip;
            {
                Math::Vector4 wp(sunWorldPos, 1.0f);
                // vp * wp (column-major multiply)
                sunClip.x = vp.m[0]*wp.x + vp.m[4]*wp.y + vp.m[8]*wp.z + vp.m[12]*wp.w;
                sunClip.y = vp.m[1]*wp.x + vp.m[5]*wp.y + vp.m[9]*wp.z + vp.m[13]*wp.w;
                sunClip.z = vp.m[2]*wp.x + vp.m[6]*wp.y + vp.m[10]*wp.z + vp.m[14]*wp.w;
                sunClip.w = vp.m[3]*wp.x + vp.m[7]*wp.y + vp.m[11]*wp.z + vp.m[15]*wp.w;
            }
            if (sunClip.w > 0.001f) {
                Math::Vector3 ndc(sunClip.x / sunClip.w, sunClip.y / sunClip.w, sunClip.z / sunClip.w);
                Math::Vector2 screenUV(ndc.x * 0.5f + 0.5f, ndc.y * 0.5f + 0.5f);
                m_PostProcessing->SetLightScreenPos(Math::Vector4(screenUV.x, screenUV.y, ndc.z, 1.0f));
            } else {
                m_PostProcessing->SetLightScreenPos(Math::Vector4(0.5f, 0.5f, 0.0f, 0.0f));
            }
        }

        // Temporal resolve: TAA and/or temporal upscaler.
        // When an upscaler is active, TAA runs at the lower render resolution first,
        // then the upscaler (Lanczos + CAS) upsamples to display resolution.
        // When no upscaler is active, TAA runs at display resolution as before.
        // Both compute dispatches must happen outside a render pass.
        bool upscalerActive = m_RenderSystem && m_RenderSystem->IsUpscalerActive();

        // TAA resolve pass (runs at render resolution — same as scene target)
        if (m_PostProcessing->IsTAAEnabled() && m_Renderer && !upscalerActive) {
            auto* swapchain = m_Renderer->GetSwapchain();
            if (swapchain) {
                m_PostProcessing->SetVelocityImageView(swapchain->GetVelocityImageView());
            }
            if (m_SceneRenderTarget && m_SceneRenderTarget->IsValid()) {
                m_PostProcessing->SetDepthImageView(m_SceneRenderTarget->GetDepthImageView());
            }
            m_PostProcessing->ApplyTAA(commandBuffer);

            // Redirect post-processing input to TAA output
            VkImageView taaOutput = m_PostProcessing->GetTAAOutputImageView();
            if (taaOutput != VK_NULL_HANDLE && m_SceneRenderTarget) {
                m_PostProcessing->UpdateSourceImage(taaOutput, m_SceneRenderTarget->GetSampler());
            }
        }

        // Temporal upscaler dispatch (FSR 2 / DLSS / XeSS)
        // Runs after TAA (if active) or directly on the jittered scene color.
        // The upscaler takes the low-res color and upscales to display resolution.
        if (upscalerActive && m_SceneRenderTarget && m_SceneRenderTarget->IsValid()) {
            auto* upscaler = m_RenderSystem->GetUpscaler();
            if (upscaler) {
                // Determine input: use TAA output if TAA ran, else scene color directly
                VkImageView colorInput = VK_NULL_HANDLE;
                if (m_PostProcessing->IsTAAEnabled()) {
                    // TAA already ran at low res — use its output
                    auto* swapchain = m_Renderer->GetSwapchain();
                    if (swapchain) {
                        m_PostProcessing->SetVelocityImageView(swapchain->GetVelocityImageView());
                    }
                    m_PostProcessing->SetDepthImageView(m_SceneRenderTarget->GetDepthImageView());
                    m_PostProcessing->ApplyTAA(commandBuffer);
                    colorInput = m_PostProcessing->GetTAAOutputImageView();
                }

                // Fall back to scene render target color if TAA didn't run
                if (colorInput == VK_NULL_HANDLE) {
                    colorInput = m_SceneRenderTarget->GetColorImageView();
                }

                // Build upscaler input
                Renderer::UpscalerInput upInput{};
                upInput.colorInput = colorInput;
                upInput.depthInput = m_SceneRenderTarget->GetDepthImageView();
                upInput.velocityInput = VK_NULL_HANDLE;
                if (m_Renderer->GetSwapchain()) {
                    upInput.velocityInput = m_Renderer->GetSwapchain()->GetVelocityImageView();
                }
                upInput.output = VK_NULL_HANDLE;  // Upscaler writes to its own internal image
                upInput.jitterX = 0.0f;
                upInput.jitterY = 0.0f;
                upInput.deltaTime = m_LastDeltaTime;
                upInput.cameraNear = 0.1f;
                upInput.cameraFar = 1000.0f;
                upInput.sharpness = m_RenderSystem->GetUpscalerSharpness();
                upInput.cameraCut = false;

                // Dispatch upscaler (Lanczos upsample + CAS sharpen)
                upscaler->Dispatch(commandBuffer, upInput);

                // Redirect post-processing to read from the upscaled output.
                // Each backend exposes GetOutputImageView() — try all known types.
                VkImageView upscaledOutput = VK_NULL_HANDLE;
                if (auto* fsr2 = dynamic_cast<Renderer::FSR2Upscaler*>(upscaler)) {
                    upscaledOutput = fsr2->GetOutputImageView();
                } else if (auto* dlss = dynamic_cast<Renderer::DLSSUpscaler*>(upscaler)) {
                    upscaledOutput = dlss->GetOutputImageView();
                } else if (auto* xess = dynamic_cast<Renderer::XeSSUpscaler*>(upscaler)) {
                    upscaledOutput = xess->GetOutputImageView();
                }
                if (upscaledOutput != VK_NULL_HANDLE) {
                    m_PostProcessing->UpdateSourceImage(upscaledOutput, m_SceneRenderTarget->GetSampler());
                }
            }
        }

        // Transition scene depth for shader reading by post-process effects
        // (caustics, SSAO, contact shadows, DoF, tilt-shift, cel outline, fog shafts, god rays)
        bool depthBound = false;
        if (m_SceneRenderTarget && m_SceneRenderTarget->IsValid() &&
            m_SceneRenderTarget->GetDepthImage() != VK_NULL_HANDLE) {
            VkImageMemoryBarrier depthBarrier{};
            depthBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            depthBarrier.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            depthBarrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
            depthBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            depthBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            depthBarrier.image = m_SceneRenderTarget->GetDepthImage();
            depthBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
            depthBarrier.subresourceRange.baseMipLevel = 0;
            depthBarrier.subresourceRange.levelCount = 1;
            depthBarrier.subresourceRange.baseArrayLayer = 0;
            depthBarrier.subresourceRange.layerCount = 1;
            depthBarrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            depthBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            vkCmdPipelineBarrier(commandBuffer,
                VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                0, 0, nullptr, 0, nullptr, 1, &depthBarrier);

            m_PostProcessing->UpdateDepthSource(m_SceneRenderTarget->GetDepthImageView());
            depthBound = true;
        }

        if (usePPShader) {
            // PP shader path: fullscreen triangle with effects
            m_GameViewRenderTarget->BeginPPPass(commandBuffer);
            m_PostProcessing->ApplyToCurrentPass(commandBuffer, rtWidth, rtHeight);
            m_GameViewRenderTarget->EndPPPass(commandBuffer);
        } else {
            // Blit fallback: direct copy without effects
            VkImageMemoryBarrier barriers[2]{};
            barriers[0] = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
            barriers[0].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            barriers[0].newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barriers[0].image = m_GameViewRenderTarget->GetColorImage();
            barriers[0].subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
            barriers[0].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barriers[1] = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
            barriers[1].oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            barriers[1].newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            barriers[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barriers[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barriers[1].image = m_SceneRenderTarget->GetColorImage();
            barriers[1].subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
            barriers[1].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
            barriers[1].dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            vkCmdPipelineBarrier(commandBuffer,
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 2, barriers);
            VkImageBlit region{};
            region.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
            region.srcOffsets[1] = {(i32)rtWidth, (i32)rtHeight, 1};
            region.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
            region.dstOffsets[1] = {(i32)m_GameViewRenderTarget->GetWidth(),
                                   (i32)m_GameViewRenderTarget->GetHeight(), 1};
            vkCmdBlitImage(commandBuffer,
                m_SceneRenderTarget->GetColorImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                m_GameViewRenderTarget->GetColorImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                1, &region, VK_FILTER_LINEAR);
            VkImageMemoryBarrier restores[2]{};
            restores[0] = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
            restores[0].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            restores[0].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            restores[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            restores[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            restores[0].image = m_GameViewRenderTarget->GetColorImage();
            restores[0].subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
            restores[0].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            restores[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            restores[1] = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
            restores[1].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            restores[1].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            restores[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            restores[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            restores[1].image = m_SceneRenderTarget->GetColorImage();
            restores[1].subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
            restores[1].srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            restores[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 2, restores);
        }

        // Transition scene depth back to attachment layout for next frame
        if (depthBound) {
            VkImageMemoryBarrier depthRestore{};
            depthRestore.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            depthRestore.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
            depthRestore.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            depthRestore.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            depthRestore.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            depthRestore.image = m_SceneRenderTarget->GetDepthImage();
            depthRestore.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
            depthRestore.subresourceRange.baseMipLevel = 0;
            depthRestore.subresourceRange.levelCount = 1;
            depthRestore.subresourceRange.baseArrayLayer = 0;
            depthRestore.subresourceRange.layerCount = 1;
            depthRestore.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
            depthRestore.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

            vkCmdPipelineBarrier(commandBuffer,
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
                0, 0, nullptr, 0, nullptr, 1, &depthRestore);
        }

        // Restore original source image for next frame (avoid stale TAA reference)
        if (m_PostProcessing->IsTAAEnabled() && m_SceneRenderTarget && m_SceneRenderTarget->IsValid()) {
            m_PostProcessing->UpdateSourceImage(
                m_SceneRenderTarget->GetColorImageView(),
                m_SceneRenderTarget->GetSampler());
        }
    }

    // Set weather for main pass (editor viewport) so it renders weather particles too
    if (hasWeatherParticles) {
        m_RenderSystem->SetMainPassWeather(&m_WeatherSystem, isRain);
    } else {
        m_RenderSystem->ClearMainPassWeather();
    }

    // Render profiling: log average CPU-side render time during play mode
    if (!m_PlayMode.IsStopped()) {
        auto renderEnd = std::chrono::high_resolution_clock::now();
        f32 renderMs = std::chrono::duration<f32, std::milli>(renderEnd - renderTimingStart).count();
        m_RenderProfileAccum += renderMs;
        m_RenderProfileFrames++;

        if (m_RenderProfileFrames >= 120) {
            f32 n = static_cast<f32>(m_RenderProfileFrames);
            ENJIN_LOG_INFO(Editor,
                "RenderOffscreen avg (%u frames): %.2fms  (RT: %ux%u)",
                m_RenderProfileFrames,
                m_RenderProfileAccum / n,
                rtWidth, rtHeight);
            m_RenderProfileAccum = 0.0f;
            m_RenderProfileFrames = 0;
        }
    }
}

void EditorLayer::Render(VkCommandBuffer commandBuffer) {
    if (!m_ImGuiLayer) {
        return;
    }

    m_ImGuiLayer->BeginFrame();

    // Initialize ImGuizmo for this frame
    ImGuizmo::BeginFrame();

    // During splash screen, only render the splash
    if (m_ShowSplash) {
        DrawSplashScreen();
        m_ImGuiLayer->EndFrame(commandBuffer);
        return;
    }

    // Template selector (shown after splash, before editor)
    if (m_ShowProjectHub) {
        DrawProjectHub();
        m_ImGuiLayer->EndFrame(commandBuffer);
        return;
    }

    ImGuiIO& io = ImGui::GetIO();

    // Focus mode: fullscreen game view, no editor panels
    if (m_FocusMode) {
        ImGuiWindowFlags focusFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(io.DisplaySize);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::Begin("##FocusView", nullptr, focusFlags);
        ImGui::PopStyleVar(2);

        VkDescriptorSet texId = m_GameViewRenderTarget ? m_GameViewRenderTarget->GetImGuiTextureID() : VK_NULL_HANDLE;
        if (texId != VK_NULL_HANDLE) {
            ImGui::Image(static_cast<ImTextureID>(reinterpret_cast<uintptr_t>(texId)),
                         io.DisplaySize);
        }

        // Render parallax scrolling backgrounds (2D scenes)
        m_ParallaxSystem.Render(io.DisplaySize.x, io.DisplaySize.y);

        ImGui::End();

        // Render dialogue overlay on top of fullscreen game view
        if (m_PlayMode.IsPlaying() || m_PlayMode.IsPaused()) {
            DrawDialogueOverlay();
        }

        // Render subtitle overlay (accessibility)
        if (m_PlayMode.IsPlaying() || m_PlayMode.IsPaused()) {
            m_SubtitleSystem.RenderOverlay(
                static_cast<u32>(io.DisplaySize.x),
                static_cast<u32>(io.DisplaySize.y));
        }

        // Render HUD widgets during play mode (fullscreen)
        if (m_PlayMode.IsPlaying()) {
            m_PlayMode.GetHUDSystem()->Update(m_World, m_Camera,
                0.0f, 0.0f, io.DisplaySize.x, io.DisplaySize.y);
        }

        // Render UI canvases during play mode (fullscreen)
        if (m_PlayMode.IsPlaying()) {
            m_UISystem.Update(m_World, io.DisplaySize.x, io.DisplaySize.y, m_LastDeltaTime);
        }

        // Render pause menu overlay on top of fullscreen game view
        if (m_GameMenu.IsMenuOpen()) {
            m_GameMenu.Render(io.DisplaySize.x, io.DisplaySize.y);
        }

        m_ImGuiLayer->EndFrame(commandBuffer);
        return;
    }

    // Menu bar
    DrawMenuBar();

    // Calculate layout dimensions from config
    f32 screenW = io.DisplaySize.x;
    f32 screenH = io.DisplaySize.y;
    f32 s = m_EditorSettings.uiScale;
    f32 menuBarH = ImGui::GetFrameHeight();  // Actual menu bar height

    // --- DockSpace covering the full area below the menu bar ---
    ImGuiID dockspaceId = ImGui::GetID("EnjinDockSpace");

    // Build the default dock layout on first use or when reset is requested
    if (!m_DockingInitialized || m_ForceLayout) {
        m_DockingInitialized = true;

        ImGui::DockBuilderRemoveNode(dockspaceId);
        ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockspaceId, ImVec2(screenW, screenH - menuBarH));
        ImGui::DockBuilderSetNodePos(dockspaceId, ImVec2(0, menuBarH));

        // Split: left panel (Hierarchy)
        ImGuiID dockLeft, dockRemaining;
        ImGui::DockBuilderSplitNode(dockspaceId, ImGuiDir_Left, m_Layout.leftWidth, &dockLeft, &dockRemaining);

        // Split: right panel (Inspector)
        ImGuiID dockRight;
        f32 rightRatio = m_Layout.rightWidth / (1.0f - m_Layout.leftWidth);
        ImGui::DockBuilderSplitNode(dockRemaining, ImGuiDir_Right, rightRatio, &dockRight, &dockRemaining);

        // Split: bottom panel (Console + Asset Browser)
        ImGuiID dockBottom;
        ImGui::DockBuilderSplitNode(dockRemaining, ImGuiDir_Down, m_Layout.bottomHeight, &dockBottom, &dockRemaining);

        // Split left panel vertically: Hierarchy (top) + Scene List (bottom)
        ImGuiID dockLeftBottom;
        ImGui::DockBuilderSplitNode(dockLeft, ImGuiDir_Down, m_Layout.bottomHeight, &dockLeftBottom, &dockLeft);

        // Split bottom panel: Console (left) + Asset Browser (right)
        ImGuiID dockBottomRight;
        ImGui::DockBuilderSplitNode(dockBottom, ImGuiDir_Right, 0.5f, &dockBottomRight, &dockBottom);

        // Dock core panels into their nodes
        ImGui::DockBuilderDockWindow("Hierarchy", dockLeft);
        ImGui::DockBuilderDockWindow("Inspector", dockRight);
        ImGui::DockBuilderDockWindow("Console", dockBottom);
        ImGui::DockBuilderDockWindow("Asset Browser", dockBottomRight);
        ImGui::DockBuilderDockWindow("Scene List", dockLeftBottom);

        // Dock Scene and Game View into the center (tabbed)
        ImGui::DockBuilderDockWindow("Scene", dockRemaining);
        ImGui::DockBuilderDockWindow("Game View", dockRemaining);

        ImGui::DockBuilderFinish(dockspaceId);
    }

    // Submit the DockSpace
    ImGui::SetNextWindowPos(ImVec2(0, menuBarH));
    ImGui::SetNextWindowSize(ImVec2(screenW, screenH - menuBarH));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGuiWindowFlags dockWindowFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoDocking;
    ImGui::Begin("##DockSpaceHost", nullptr, dockWindowFlags);
    ImGui::PopStyleVar(2);
    ImGui::DockSpace(dockspaceId, ImVec2(0, 0), ImGuiDockNodeFlags_None);
    ImGui::End();

    // --- Core docked panels (positions managed by DockSpace) ---
    if (HasPanel(m_VisiblePanels, EditorPanel::Hierarchy)) {
        DrawHierarchyPanel();
    }
    if (HasPanel(m_VisiblePanels, EditorPanel::Inspector)) {
        DrawInspectorPanel();
    }
    // Unified settings window — any of the 5 old settings bits activates it
    {
        bool anySettingsBit =
            HasPanel(m_VisiblePanels, EditorPanel::EditorSettings) ||
            HasPanel(m_VisiblePanels, EditorPanel::ProjectSettings) ||
            HasPanel(m_VisiblePanels, EditorPanel::PostProcessing) ||
            HasPanel(m_VisiblePanels, EditorPanel::RetroEffects) ||
            HasPanel(m_VisiblePanels, EditorPanel::Rendering);
        if (anySettingsBit) {
            // Route old bits to the correct tab (one-shot on first open)
            if (HasPanel(m_VisiblePanels, EditorPanel::ProjectSettings) &&
                !HasPanel(m_VisiblePanels, EditorPanel::EditorSettings)) {
                m_SettingsActiveTab = 1;
            }
            if (HasPanel(m_VisiblePanels, EditorPanel::Rendering) ||
                HasPanel(m_VisiblePanels, EditorPanel::PostProcessing) ||
                HasPanel(m_VisiblePanels, EditorPanel::RetroEffects)) {
                if (!HasPanel(m_VisiblePanels, EditorPanel::EditorSettings) &&
                    !HasPanel(m_VisiblePanels, EditorPanel::ProjectSettings)) {
                    m_SettingsActiveTab = 2;
                }
            }
            // Consolidate all bits into EditorSettings for the unified window
            SetPanelVisibility(EditorPanel::EditorSettings, true);
            SetPanelVisibility(EditorPanel::ProjectSettings, false);
            SetPanelVisibility(EditorPanel::PostProcessing, false);
            SetPanelVisibility(EditorPanel::RetroEffects, false);
            SetPanelVisibility(EditorPanel::Rendering, false);

            DrawSettingsWindow();
        }
    }
    if (HasPanel(m_VisiblePanels, EditorPanel::Console)) {
        DrawConsolePanel();
    }
    if (HasPanel(m_VisiblePanels, EditorPanel::AssetBrowser)) {
        DrawAssetBrowserPanel();
    }
    // PostProcessing and RetroEffects panels are now in the unified Settings window
    if (HasPanel(m_VisiblePanels, EditorPanel::Viewport)) {
        DrawViewportPanel();
    }
    if (HasPanel(m_VisiblePanels, EditorPanel::GameView)) {
        DrawGameViewPanel();
    }
    if (HasPanel(m_VisiblePanels, EditorPanel::SceneList)) {
        DrawSceneListPanel();
    }
    // Rendering panel is now in the unified Settings window

    // --- Floating tool windows (not pre-docked, but user can dock them) ---
    if (HasPanel(m_VisiblePanels, EditorPanel::Profiler)) {
        ImGui::SetNextWindowSize(ImVec2(520 * s, 450 * s), ImGuiCond_FirstUseEver);
        Debug::Profiler::Instance().DrawProfilerPanel();
    }
    if (HasPanel(m_VisiblePanels, EditorPanel::ParticleEditor)) {
        ImGui::SetNextWindowSize(ImVec2(380 * s, 600 * s), ImGuiCond_FirstUseEver);
        DrawParticleEditorPanel();
    }
    if (HasPanel(m_VisiblePanels, EditorPanel::AnimGraph)) {
        ImGui::SetNextWindowSize(ImVec2(700 * s, 500 * s), ImGuiCond_FirstUseEver);
        DrawAnimGraphPanel();
    }
    if (HasPanel(m_VisiblePanels, EditorPanel::Dialogue)) {
        ImGui::SetNextWindowSize(ImVec2(750 * s, 550 * s), ImGuiCond_FirstUseEver);
        DrawDialoguePanel();
    }
    if (HasPanel(m_VisiblePanels, EditorPanel::VisualScript)) {
        ImGui::SetNextWindowSize(ImVec2(800 * s, 600 * s), ImGuiCond_FirstUseEver);
        DrawVisualScriptPanel();
    }
    if (HasPanel(m_VisiblePanels, EditorPanel::SpriteSheetImport)) {
        ImGui::SetNextWindowSize(ImVec2(700 * s, 500 * s), ImGuiCond_FirstUseEver);
        DrawSpriteSheetImporterPanel();
    }
    if (HasPanel(m_VisiblePanels, EditorPanel::PixelEditorPanel)) {
        ImGui::SetNextWindowSize(ImVec2(800 * s, 600 * s), ImGuiCond_FirstUseEver);
        DrawPixelEditorPanel();
    }
    if (HasPanel(m_VisiblePanels, EditorPanel::BehaviorTree)) {
        ImGui::SetNextWindowSize(ImVec2(800 * s, 600 * s), ImGuiCond_FirstUseEver);
        DrawBehaviorTreePanel();
    }
    if (HasPanel(m_VisiblePanels, EditorPanel::QuestFlow)) {
        ImGui::SetNextWindowSize(ImVec2(800 * s, 600 * s), ImGuiCond_FirstUseEver);
        DrawQuestFlowPanel();
    }
    if (HasPanel(m_VisiblePanels, EditorPanel::UserManual)) {
        ImGui::SetNextWindowSize(ImVec2(700 * s, 600 * s), ImGuiCond_FirstUseEver);
        DrawUserManualPanel();
    }
    if (HasPanel(m_VisiblePanels, EditorPanel::DataAssets)) {
        ImGui::SetNextWindowSize(ImVec2(800 * s, 550 * s), ImGuiCond_FirstUseEver);
        DrawDataAssetPanel();
    }
    if (HasPanel(m_VisiblePanels, EditorPanel::PluginBrowser)) {
        ImGui::SetNextWindowSize(ImVec2(800 * s, 500 * s), ImGuiCond_FirstUseEver);
        DrawPluginBrowserPanel();
    }
    if (HasPanel(m_VisiblePanels, EditorPanel::ProceduralGen)) {
        ImGui::SetNextWindowSize(ImVec2(650 * s, 600 * s), ImGuiCond_FirstUseEver);
        DrawProceduralGenPanel();
    }
    if (HasPanel(m_VisiblePanels, EditorPanel::GitIntegration)) {
        ImGui::SetNextWindowSize(ImVec2(550 * s, 600 * s), ImGuiCond_FirstUseEver);
        DrawGitIntegrationPanel();
    }
    if (HasPanel(m_VisiblePanels, EditorPanel::NetworkPanel)) {
        ImGui::SetNextWindowSize(ImVec2(450 * s, 500 * s), ImGuiCond_FirstUseEver);
        DrawNetworkPanel();
    }
    if (HasPanel(m_VisiblePanels, EditorPanel::Collaboration)) {
        ImGui::SetNextWindowSize(ImVec2(450 * s, 550 * s), ImGuiCond_FirstUseEver);
        DrawCollaborationPanel();
    }
    if (HasPanel(m_VisiblePanels, EditorPanel::FlashTimeline)) {
        ImGui::SetNextWindowSize(ImVec2(800 * s, 350 * s), ImGuiCond_FirstUseEver);
        DrawFlashTimelinePanel();
    }
    if (HasPanel(m_VisiblePanels, EditorPanel::VectorDrawing)) {
        ImGui::SetNextWindowSize(ImVec2(700 * s, 550 * s), ImGuiCond_FirstUseEver);
        DrawVectorDrawingPanel();
    }
    if (HasPanel(m_VisiblePanels, EditorPanel::FeedbackPanel)) {
        ImGui::SetNextWindowSize(ImVec2(720 * s, 580 * s), ImGuiCond_FirstUseEver);
        DrawFeedbackPanel();
    }
    if (HasPanel(m_VisiblePanels, EditorPanel::SaveDebug)) {
        ImGui::SetNextWindowSize(ImVec2(620 * s, 500 * s), ImGuiCond_FirstUseEver);
        DrawSaveDebugPanel();
    }
    if (m_ShowDebugOverlay) {
        DrawDebugOverlay();
    }
    // Legacy panels (kept for backward compat, hidden by default)
    if (m_ShowGameDebug) {
        DrawGameDebugPanel();
    }
    if (m_ShowDebugWorkstation) {
        DrawDebugWorkstation();
    }
    if (m_ShowHTML5ExportDialog) {
        DrawHTML5ExportDialog();
    }

    // Dialogue tree editor (owns its own window) - legacy, now use DrawDialoguePanel()
    // m_DialogueTreeEditor.Render();

    // Graph editor windows (use IsOpen pattern, not panel bits)
    if (m_ShaderGraphEditor.IsOpen()) {
        m_ShaderGraphEditor.Render();
    }
    if (m_AudioGraphEditor.IsOpen()) {
        m_AudioGraphEditor.Render();
    }
    if (m_ParticleGraphEditor.IsOpen()) {
        m_ParticleGraphEditor.Render();
    }

    // Template Creator window
    if (m_ShowTemplateCreator) {
        DrawTemplateCreatorWindow();
    }

    // Template Marketplace window
    if (m_TemplateMarketplace.IsOpen()) {
        DrawTemplateMarketplaceWindow();
    }

    // Audio Mixer window
    if (m_ShowAudioMixer) {
        DrawAudioMixer();
    }

    // Keyboard Shortcuts Help modal
    if (m_ShowShortcutsHelp) {
        DrawShortcutsHelpModal();
    }

    // Entity delete confirmation modal
    if (m_ShowDeleteConfirm) {
        DrawDeleteConfirmModal();
    }

    // Crash report dialog (previous session)
    if (m_ShowCrashDialog) {
        DrawCrashReportDialog();
    }

    // Unsaved changes confirmation dialog
    if (m_ShowUnsavedChangesDialog) {
        DrawUnsavedChangesDialog();
    }

    // Auto-save recovery dialog
    if (m_ShowAutoSaveRecoveryDialog) {
        DrawAutoSaveRecoveryDialog();
    }

    // Clear the force flag after one frame
    if (m_ForceLayout) m_ForceLayout = false;

    // Submit onion skin ghosts to RenderSystem for editor viewport rendering
    if (m_RenderSystem && m_FlashTimelineEditor.GetTimeline()) {
        auto ghosts = m_FlashTimelineEditor.ComputeOnionSkinGhosts();
        if (!ghosts.empty()) {
            m_RenderSystem->SetOnionSkinGhosts(ghosts);
        } else {
            m_RenderSystem->ClearOnionSkinGhosts();
        }
    }

    // Record mode: draw red border around viewport
    if (m_FlashTimelineEditor.IsRecordMode()) {
        ImDrawList* fgDL = ImGui::GetForegroundDrawList();
        f32 blink = std::fmod(static_cast<f32>(ImGui::GetTime()), 1.0f);
        u8 alpha = static_cast<u8>(150 + 105 * (blink < 0.5f ? blink * 2.0f : 2.0f - blink * 2.0f));
        fgDL->AddRect(ImVec2(0, 0), io.DisplaySize,
                       IM_COL32(255, 40, 40, alpha), 0.0f, 0, 3.0f);
    }

    // Draw scene transition overlay (fade to/from black/white)
    if (m_SceneManager.IsTransitioning()) {
        f32 alpha = m_SceneManager.GetTransitionAlpha();
        ImU32 color = IM_COL32(0, 0, 0, static_cast<u8>(alpha * 255));
        ImGui::GetForegroundDrawList()->AddRectFilled(
            ImVec2(0, 0), io.DisplaySize, color);
    }

    // Clip all viewport overlays (gizmos, marquee, frustums) to the editor viewport panel
    // Only draw when the viewport panel is actually visible
    if (HasPanel(m_VisiblePanels, EditorPanel::Viewport)) {
        ImDrawList* fgOverlay = ImGui::GetForegroundDrawList();
        fgOverlay->PushClipRect(
            ImVec2(m_EditorViewportImageMinX, m_EditorViewportImageMinY),
            ImVec2(m_EditorViewportImageMaxX, m_EditorViewportImageMaxY), true);

        // Draw gizmos for selected entity
        DrawGizmos();

        // Draw marquee selection rectangle
        DrawMarqueeRect();

        // Grid is now rendered into the editor viewport RT in RenderOffscreen()

        // Draw camera frustum for all camera entities (or selected camera)
        if (m_World) {
            for (ECS::Entity entity : m_World->GetEntitiesWithComponent<ECS::CameraComponent>()) {
                DrawCameraFrustum(entity);
            }
        }

        fgOverlay->PopClipRect();
    }

    // Draw wireframe bounding boxes for weather zones and water volumes
    // Overlays project world-space positions into the editor viewport's screen rect
    if (HasPanel(m_VisiblePanels, EditorPanel::Viewport) && m_World && m_Camera) {
        f32 sw = m_EditorViewportImageMaxX - m_EditorViewportImageMinX;
        f32 sh = m_EditorViewportImageMaxY - m_EditorViewportImageMinY;
        if (sw > 0 && sh > 0) {
            Math::Matrix4 viewMat = m_Camera->GetViewMatrix();
            Math::Matrix4 projMat = m_Camera->GetProjectionMatrix();
            Math::Matrix4 viewProj = projMat * viewMat;

            auto worldToScreen = [&](const Math::Vector3& worldPos, ImVec2& screenPos) -> bool {
                Math::Vector4 clipPos = viewProj * Math::Vector4(worldPos.x, worldPos.y, worldPos.z, 1.0f);
                if (clipPos.w <= 0.001f) return false;
                f32 ndcX = clipPos.x / clipPos.w;
                f32 ndcY = clipPos.y / clipPos.w;
                f32 ndcZ = clipPos.z / clipPos.w;
                if (ndcZ < 0.0f || ndcZ > 1.0f) return false;
                screenPos.x = (ndcX + 1.0f) * 0.5f * sw + m_EditorViewportImageMinX;
                screenPos.y = (ndcY + 1.0f) * 0.5f * sh + m_EditorViewportImageMinY;
                return true;
            };

            auto drawLine3D = [&](ImDrawList* dl, const Math::Vector3& from, const Math::Vector3& to, ImU32 color, f32 thickness) {
                ImVec2 screenFrom, screenTo;
                if (worldToScreen(from, screenFrom) && worldToScreen(to, screenTo)) {
                    dl->AddLine(screenFrom, screenTo, color, thickness);
                }
            };

            auto drawWireBox = [&](ImDrawList* dl, const Math::Vector3& center, const Math::Vector3& halfExt, ImU32 color, f32 thickness, const Math::Quaternion& rotation = Math::Quaternion::Identity()) {
                // 8 corners of the OBB (oriented bounding box)
                Math::Vector3 localCorners[8] = {
                    {-halfExt.x, -halfExt.y, -halfExt.z},
                    { halfExt.x, -halfExt.y, -halfExt.z},
                    { halfExt.x, -halfExt.y,  halfExt.z},
                    {-halfExt.x, -halfExt.y,  halfExt.z},
                    {-halfExt.x,  halfExt.y, -halfExt.z},
                    { halfExt.x,  halfExt.y, -halfExt.z},
                    { halfExt.x,  halfExt.y,  halfExt.z},
                    {-halfExt.x,  halfExt.y,  halfExt.z},
                };
                Math::Vector3 corners[8];
                for (int i = 0; i < 8; ++i) {
                    corners[i] = center + rotation.Rotate(localCorners[i]);
                }
                // 12 edges: bottom 4, top 4, vertical 4
                drawLine3D(dl, corners[0], corners[1], color, thickness);
                drawLine3D(dl, corners[1], corners[2], color, thickness);
                drawLine3D(dl, corners[2], corners[3], color, thickness);
                drawLine3D(dl, corners[3], corners[0], color, thickness);
                drawLine3D(dl, corners[4], corners[5], color, thickness);
                drawLine3D(dl, corners[5], corners[6], color, thickness);
                drawLine3D(dl, corners[6], corners[7], color, thickness);
                drawLine3D(dl, corners[7], corners[4], color, thickness);
                drawLine3D(dl, corners[0], corners[4], color, thickness);
                drawLine3D(dl, corners[1], corners[5], color, thickness);
                drawLine3D(dl, corners[2], corners[6], color, thickness);
                drawLine3D(dl, corners[3], corners[7], color, thickness);
            };

            ImDrawList* bgDrawList = ImGui::GetForegroundDrawList();
            bgDrawList->PushClipRect(
                ImVec2(m_EditorViewportImageMinX, m_EditorViewportImageMinY),
                ImVec2(m_EditorViewportImageMaxX, m_EditorViewportImageMaxY), true);

            // Weather zone wireframe (light blue)
            for (ECS::Entity entity : m_World->GetEntitiesWithComponent<ECS::WeatherZoneComponent>()) {
                auto* zone = m_World->GetComponent<ECS::WeatherZoneComponent>(entity);
                auto* transform = m_World->GetComponent<ECS::TransformComponent>(entity);
                if (zone && transform) {
                    bool isSelected = IsSelected(entity);
                    ImU32 color = isSelected ? IM_COL32(100, 180, 255, 200) : IM_COL32(100, 180, 255, 80);
                    f32 thickness = isSelected ? 2.0f : 1.0f;
                    drawWireBox(bgDrawList, transform->position, zone->halfExtents, color, thickness);
                }
            }
            // Water volume wireframe (cyan/teal)
            for (ECS::Entity entity : m_World->GetEntitiesWithComponent<ECS::WaterVolumeComponent>()) {
                auto* volume = m_World->GetComponent<ECS::WaterVolumeComponent>(entity);
                auto* transform = m_World->GetComponent<ECS::TransformComponent>(entity);
                if (volume && transform) {
                    bool isSelected = IsSelected(entity);
                    ImU32 color = isSelected ? IM_COL32(50, 220, 200, 200) : IM_COL32(50, 220, 200, 80);
                    f32 thickness = isSelected ? 2.0f : 1.0f;
                    drawWireBox(bgDrawList, transform->position, volume->halfExtents, color, thickness);
                }
            }
            // Grass volume wireframe (green)
            for (ECS::Entity entity : m_World->GetEntitiesWithComponent<ECS::GrassVolumeComponent>()) {
                auto* grass = m_World->GetComponent<ECS::GrassVolumeComponent>(entity);
                auto* transform = m_World->GetComponent<ECS::TransformComponent>(entity);
                if (grass && transform) {
                    bool isSelected = IsSelected(entity);
                    ImU32 color = isSelected ? IM_COL32(80, 200, 80, 200) : IM_COL32(80, 200, 80, 60);
                    f32 thickness = isSelected ? 2.0f : 1.0f;
                    drawWireBox(bgDrawList, transform->position, grass->halfExtents, color, thickness);
                }
            }
            // Shrub volume wireframe (yellow-green)
            for (ECS::Entity entity : m_World->GetEntitiesWithComponent<ECS::ShrubVolumeComponent>()) {
                auto* shrub = m_World->GetComponent<ECS::ShrubVolumeComponent>(entity);
                auto* transform = m_World->GetComponent<ECS::TransformComponent>(entity);
                if (shrub && transform) {
                    bool isSelected = IsSelected(entity);
                    ImU32 color = isSelected ? IM_COL32(160, 200, 60, 200) : IM_COL32(160, 200, 60, 60);
                    f32 thickness = isSelected ? 2.0f : 1.0f;
                    drawWireBox(bgDrawList, transform->position, shrub->halfExtents, color, thickness);
                }
            }
            // Tree volume wireframe (dark green)
            for (ECS::Entity entity : m_World->GetEntitiesWithComponent<ECS::TreeVolumeComponent>()) {
                auto* tree = m_World->GetComponent<ECS::TreeVolumeComponent>(entity);
                auto* transform = m_World->GetComponent<ECS::TransformComponent>(entity);
                if (tree && transform) {
                    bool isSelected = IsSelected(entity);
                    ImU32 color = isSelected ? IM_COL32(40, 160, 40, 200) : IM_COL32(40, 160, 40, 60);
                    f32 thickness = isSelected ? 2.0f : 1.0f;
                    drawWireBox(bgDrawList, transform->position, tree->halfExtents, color, thickness);
                }
            }

            // --- Physics debug visualization: colliders + joints ---
            if (m_ShowColliderWireframes) {
                // Wire circle helper (draws N-segment circle in a plane)
                auto drawWireCircle = [&](ImDrawList* dl, const Math::Vector3& center, f32 radius,
                                          const Math::Vector3& axisU, const Math::Vector3& axisV,
                                          ImU32 color, f32 thickness, i32 segments = 24) {
                    constexpr f32 PI2 = 6.2831853f;
                    for (i32 i = 0; i < segments; ++i) {
                        f32 a0 = PI2 * static_cast<f32>(i) / static_cast<f32>(segments);
                        f32 a1 = PI2 * static_cast<f32>(i + 1) / static_cast<f32>(segments);
                        Math::Vector3 p0 = center + axisU * (std::cos(a0) * radius) + axisV * (std::sin(a0) * radius);
                        Math::Vector3 p1 = center + axisU * (std::cos(a1) * radius) + axisV * (std::sin(a1) * radius);
                        drawLine3D(dl, p0, p1, color, thickness);
                    }
                };

                // Box colliders (yellow, oriented to entity rotation)
                for (ECS::Entity entity : m_World->GetEntitiesWithComponent<ECS::BoxColliderComponent>()) {
                    auto* box = m_World->GetComponent<ECS::BoxColliderComponent>(entity);
                    auto* transform = m_World->GetComponent<ECS::TransformComponent>(entity);
                    if (box && transform) {
                        bool sel = IsSelected(entity);
                        ImU32 color = sel ? IM_COL32(255, 220, 50, 220) : IM_COL32(255, 220, 50, 100);
                        f32 thick = sel ? 2.0f : 1.0f;
                        Math::Vector3 halfExt = box->size * 0.5f;
                        Math::Vector3 worldCenter = transform->position + transform->rotation.Rotate(box->center);
                        drawWireBox(bgDrawList, worldCenter, halfExt, color, thick, transform->rotation);
                    }
                }

                // Sphere colliders (green-yellow, 3 orthogonal circles)
                for (ECS::Entity entity : m_World->GetEntitiesWithComponent<ECS::SphereColliderComponent>()) {
                    auto* sphere = m_World->GetComponent<ECS::SphereColliderComponent>(entity);
                    auto* transform = m_World->GetComponent<ECS::TransformComponent>(entity);
                    if (sphere && transform) {
                        bool sel = IsSelected(entity);
                        ImU32 color = sel ? IM_COL32(180, 230, 50, 220) : IM_COL32(180, 230, 50, 100);
                        f32 thick = sel ? 2.0f : 1.0f;
                        Math::Vector3 c = transform->position + sphere->center;
                        f32 r = sphere->radius;
                        drawWireCircle(bgDrawList, c, r, {1,0,0}, {0,1,0}, color, thick); // XY
                        drawWireCircle(bgDrawList, c, r, {1,0,0}, {0,0,1}, color, thick); // XZ
                        drawWireCircle(bgDrawList, c, r, {0,1,0}, {0,0,1}, color, thick); // YZ
                    }
                }

                // Capsule colliders (orange, proper capsule wireframe)
                for (ECS::Entity entity : m_World->GetEntitiesWithComponent<ECS::CapsuleColliderComponent>()) {
                    auto* capsule = m_World->GetComponent<ECS::CapsuleColliderComponent>(entity);
                    auto* transform = m_World->GetComponent<ECS::TransformComponent>(entity);
                    if (capsule && transform) {
                        bool sel = IsSelected(entity);
                        ImU32 color = sel ? IM_COL32(255, 160, 40, 220) : IM_COL32(255, 160, 40, 100);
                        f32 thick = sel ? 2.0f : 1.0f;
                        Math::Vector3 c = transform->position + transform->rotation.Rotate(capsule->center);
                        f32 r = capsule->radius;
                        f32 halfH = capsule->height * 0.5f;
                        f32 stemHalf = halfH - r;  // Half-height of the cylindrical section

                        // Determine local axis and perpendicular axes
                        Math::Vector3 localAxis, localU, localV;
                        switch (capsule->direction) {
                            case ECS::CapsuleColliderComponent::Direction::X:
                                localAxis = {1,0,0}; localU = {0,1,0}; localV = {0,0,1}; break;
                            case ECS::CapsuleColliderComponent::Direction::Z:
                                localAxis = {0,0,1}; localU = {1,0,0}; localV = {0,1,0}; break;
                            default: // Y
                                localAxis = {0,1,0}; localU = {1,0,0}; localV = {0,0,1}; break;
                        }
                        // Apply entity rotation
                        Math::Vector3 axis = transform->rotation.Rotate(localAxis);
                        Math::Vector3 u = transform->rotation.Rotate(localU);
                        Math::Vector3 v = transform->rotation.Rotate(localV);

                        Math::Vector3 top = c + axis * stemHalf;
                        Math::Vector3 bot = c - axis * stemHalf;

                        // End circles (at cylinder/hemisphere boundary)
                        drawWireCircle(bgDrawList, top, r, u, v, color, thick, 20);
                        drawWireCircle(bgDrawList, bot, r, u, v, color, thick, 20);

                        // 4 vertical lines connecting the circles
                        drawLine3D(bgDrawList, top + u * r, bot + u * r, color, thick);
                        drawLine3D(bgDrawList, top - u * r, bot - u * r, color, thick);
                        drawLine3D(bgDrawList, top + v * r, bot + v * r, color, thick);
                        drawLine3D(bgDrawList, top - v * r, bot - v * r, color, thick);

                        // Middle ring (shows cylinder section)
                        drawWireCircle(bgDrawList, c, r, u, v, color, thick, 20);

                        // Hemisphere arcs (semicircles, outward-facing half only)
                        constexpr f32 PI = 3.14159265f;
                        auto drawSemiCircle = [&](const Math::Vector3& center, f32 radius,
                                                   const Math::Vector3& aU, const Math::Vector3& aV,
                                                   bool flipSign, i32 segs = 12) {
                            for (i32 i = 0; i < segs; ++i) {
                                f32 a0 = PI * static_cast<f32>(i) / static_cast<f32>(segs);
                                f32 a1 = PI * static_cast<f32>(i + 1) / static_cast<f32>(segs);
                                f32 sign = flipSign ? -1.0f : 1.0f;
                                Math::Vector3 p0 = center + aU * (std::cos(a0) * radius * sign) + aV * (std::sin(a0) * radius);
                                Math::Vector3 p1 = center + aU * (std::cos(a1) * radius * sign) + aV * (std::sin(a1) * radius);
                                drawLine3D(bgDrawList, p0, p1, color, thick);
                            }
                        };
                        drawSemiCircle(top, r, axis, u, false, 12);
                        drawSemiCircle(top, r, axis, v, false, 12);
                        drawSemiCircle(bot, r, axis, u, true, 12);
                        drawSemiCircle(bot, r, axis, v, true, 12);
                    }
                }

                // Mesh colliders (magenta wireframe from cached vertices)
                for (ECS::Entity entity : m_World->GetEntitiesWithComponent<ECS::MeshColliderComponent>()) {
                    auto* meshCol = m_World->GetComponent<ECS::MeshColliderComponent>(entity);
                    auto* transform = m_World->GetComponent<ECS::TransformComponent>(entity);
                    if (meshCol && transform && meshCol->generated && !meshCol->vertices.empty()) {
                        bool sel = IsSelected(entity);
                        ImU32 color = sel ? IM_COL32(220, 80, 220, 220) : IM_COL32(220, 80, 220, 80);
                        f32 thick = sel ? 2.0f : 1.0f;

                        // Draw triangle edges if we have indices
                        if (!meshCol->indices.empty() && meshCol->indices.size() % 3 == 0) {
                            for (size_t i = 0; i + 2 < meshCol->indices.size(); i += 3) {
                                auto transformVert = [&](const Math::Vector3& v) {
                                    Math::Vector3 scaled(v.x * transform->scale.x,
                                                        v.y * transform->scale.y,
                                                        v.z * transform->scale.z);
                                    return transform->position + transform->rotation.Rotate(scaled);
                                };
                                Math::Vector3 a = transformVert(meshCol->vertices[meshCol->indices[i]]);
                                Math::Vector3 b = transformVert(meshCol->vertices[meshCol->indices[i + 1]]);
                                Math::Vector3 c = transformVert(meshCol->vertices[meshCol->indices[i + 2]]);
                                drawLine3D(bgDrawList, a, b, color, thick);
                                drawLine3D(bgDrawList, b, c, color, thick);
                                drawLine3D(bgDrawList, c, a, color, thick);
                            }
                        }
                    }
                }

                // Body2D colliders (cyan)
                for (ECS::Entity entity : m_World->GetEntitiesWithComponent<Physics::Body2DComponent>()) {
                    auto* body2d = m_World->GetComponent<Physics::Body2DComponent>(entity);
                    auto* transform = m_World->GetComponent<ECS::TransformComponent>(entity);
                    if (body2d && transform) {
                        bool sel = IsSelected(entity);
                        ImU32 color = sel ? IM_COL32(50, 220, 255, 220) : IM_COL32(50, 220, 255, 100);
                        f32 thick = sel ? 2.0f : 1.0f;
                        Math::Vector3 pos = transform->position;
                        if (body2d->shapeType == Physics::Shape2DType::Box) {
                            Math::Vector3 offset(body2d->box.offset.x, body2d->box.offset.y, 0.0f);
                            Math::Vector3 halfExt(body2d->box.halfExtents.x, body2d->box.halfExtents.y, 0.01f);
                            drawWireBox(bgDrawList, pos + offset, halfExt, color, thick);
                        } else if (body2d->shapeType == Physics::Shape2DType::Circle) {
                            Math::Vector3 offset(body2d->circle.offset.x, body2d->circle.offset.y, 0.0f);
                            Math::Vector3 c = pos + offset;
                            f32 r = body2d->circle.radius;
                            drawWireCircle(bgDrawList, c, r, {1,0,0}, {0,1,0}, color, thick, 24);
                        }
                    }
                }

                // Joint visualization — draw lines between connected entities
                auto drawJointLine = [&](ImDrawList* dl, ECS::Entity eA, ECS::Entity eB,
                                          const Math::Vector3& anchorA, const Math::Vector3& anchorB,
                                          ImU32 color) {
                    auto* tA = m_World->GetComponent<ECS::TransformComponent>(eA);
                    auto* tB = m_World->GetComponent<ECS::TransformComponent>(eB);
                    if (!tA || !tB) return;
                    drawLine3D(dl, tA->position + anchorA, tB->position + anchorB, color, 1.5f);
                };

                // Distance joints (white)
                for (ECS::Entity e : m_World->GetEntitiesWithComponent<ECS::DistanceJointComponent>()) {
                    auto* j = m_World->GetComponent<ECS::DistanceJointComponent>(e);
                    if (j) drawJointLine(bgDrawList, j->entityA, j->entityB, j->anchorA, j->anchorB, IM_COL32(255, 255, 255, 180));
                }
                // Hinge joints (cyan)
                for (ECS::Entity e : m_World->GetEntitiesWithComponent<ECS::HingeJointComponent>()) {
                    auto* j = m_World->GetComponent<ECS::HingeJointComponent>(e);
                    if (j) drawJointLine(bgDrawList, j->entityA, j->entityB, j->anchorA, j->anchorB, IM_COL32(0, 220, 255, 180));
                }
                // BallSocket joints (magenta)
                for (ECS::Entity e : m_World->GetEntitiesWithComponent<ECS::BallSocketJointComponent>()) {
                    auto* j = m_World->GetComponent<ECS::BallSocketJointComponent>(e);
                    if (j) drawJointLine(bgDrawList, j->entityA, j->entityB, j->anchorA, j->anchorB, IM_COL32(220, 50, 220, 180));
                }
                // Spring joints (green)
                for (ECS::Entity e : m_World->GetEntitiesWithComponent<ECS::SpringJointComponent>()) {
                    auto* j = m_World->GetComponent<ECS::SpringJointComponent>(e);
                    if (j) drawJointLine(bgDrawList, j->entityA, j->entityB, j->anchorA, j->anchorB, IM_COL32(50, 220, 50, 180));
                }
                // Fixed joints (red)
                for (ECS::Entity e : m_World->GetEntitiesWithComponent<ECS::FixedJointComponent>()) {
                    auto* j = m_World->GetComponent<ECS::FixedJointComponent>(e);
                    if (j) drawJointLine(bgDrawList, j->entityA, j->entityB, j->anchorA, j->anchorB, IM_COL32(220, 50, 50, 180));
                }
                // Slider joints (blue)
                for (ECS::Entity e : m_World->GetEntitiesWithComponent<ECS::SliderJointComponent>()) {
                    auto* j = m_World->GetComponent<ECS::SliderJointComponent>(e);
                    if (j) drawJointLine(bgDrawList, j->entityA, j->entityB, j->anchorA, j->anchorB, IM_COL32(50, 100, 255, 180));
                }

                // Post-Process Volume wireframes (purple)
                for (ECS::Entity e : m_World->GetEntitiesWithComponent<ECS::PostProcessVolumeComponent>()) {
                    auto* vol = m_World->GetComponent<ECS::PostProcessVolumeComponent>(e);
                    auto* transform = m_World->GetComponent<ECS::TransformComponent>(e);
                    if (!vol || !vol->isActive || vol->isGlobal || !transform) continue;
                    bool sel = IsSelected(e);
                    ImU32 color = sel ? IM_COL32(180, 100, 255, 220) : IM_COL32(180, 100, 255, 80);
                    f32 thick = sel ? 2.0f : 1.0f;
                    if (vol->shape == ECS::PPVolumeShape::Box) {
                        drawWireBox(bgDrawList, transform->position, vol->halfExtents, color, thick);
                        // Blend radius outer box (dashed feel via thinner line)
                        if (vol->blendRadius > 0.01f) {
                            Math::Vector3 outer = vol->halfExtents + Math::Vector3(vol->blendRadius, vol->blendRadius, vol->blendRadius);
                            drawWireBox(bgDrawList, transform->position, outer,
                                sel ? IM_COL32(180, 100, 255, 120) : IM_COL32(180, 100, 255, 40), thick * 0.5f);
                        }
                    } else {
                        Math::Vector3 c = transform->position;
                        f32 r = vol->halfExtents.x;
                        drawWireCircle(bgDrawList, c, r, {1,0,0}, {0,1,0}, color, thick);
                        drawWireCircle(bgDrawList, c, r, {1,0,0}, {0,0,1}, color, thick);
                        drawWireCircle(bgDrawList, c, r, {0,1,0}, {0,0,1}, color, thick);
                        if (vol->blendRadius > 0.01f) {
                            ImU32 outerColor = sel ? IM_COL32(180, 100, 255, 120) : IM_COL32(180, 100, 255, 40);
                            drawWireCircle(bgDrawList, c, r + vol->blendRadius, {1,0,0}, {0,1,0}, outerColor, thick * 0.5f);
                        }
                    }
                }
            }

            // --- SH Light Probe visualization ---
            if (m_ShowSHProbes || m_ShowSHGridBounds) {
                auto* shLighting = m_RenderSystem->GetSHLighting();
                if (shLighting) {
                    // Wire circle helper for probe spheres
                    auto drawProbeCircle = [&](ImDrawList* dl, const Math::Vector3& center, f32 radius,
                                              const Math::Vector3& axisU, const Math::Vector3& axisV,
                                              ImU32 color, f32 thickness, i32 segments = 16) {
                        constexpr f32 PI2 = 6.2831853f;
                        ImVec2 prev;
                        bool prevValid = false;
                        for (i32 i = 0; i <= segments; ++i) {
                            f32 angle = PI2 * f32(i) / f32(segments);
                            Math::Vector3 p = center + axisU * (std::cos(angle) * radius) + axisV * (std::sin(angle) * radius);
                            ImVec2 sp;
                            bool valid = worldToScreen(p, sp);
                            if (valid && prevValid) dl->AddLine(prev, sp, color, thickness);
                            prev = sp;
                            prevValid = valid;
                        }
                    };

                    if (m_ShowSHProbes) {
                        const f32 probeRadius = 0.3f;
                        for (const auto& probe : shLighting->GetProbes()) {
                            // Green if baked, red if empty
                            ImU32 color = probe.baked ? IM_COL32(50, 220, 50, 180) : IM_COL32(220, 50, 50, 180);
                            f32 thick = 1.5f;
                            // Draw 3 orthogonal circles
                            drawProbeCircle(bgDrawList, probe.position, probeRadius, {1,0,0}, {0,1,0}, color, thick);
                            drawProbeCircle(bgDrawList, probe.position, probeRadius, {1,0,0}, {0,0,1}, color, thick);
                            drawProbeCircle(bgDrawList, probe.position, probeRadius, {0,1,0}, {0,0,1}, color, thick);

                            // Draw probe ID label
                            ImVec2 labelPos;
                            if (worldToScreen(probe.position + Math::Vector3(0, probeRadius + 0.1f, 0), labelPos)) {
                                char idBuf[16];
                                snprintf(idBuf, sizeof(idBuf), "P%u", probe.id);
                                bgDrawList->AddText(ImVec2(labelPos.x - 8, labelPos.y - 8), color, idBuf);
                            }
                        }
                    }

                    if (m_ShowSHGridBounds) {
                        const auto& grid = shLighting->GetGrid();
                        Math::Vector3 center = (grid.boundsMin + grid.boundsMax) * 0.5f;
                        Math::Vector3 halfExt = (grid.boundsMax - grid.boundsMin) * 0.5f;
                        if (halfExt.x > 0.001f || halfExt.y > 0.001f || halfExt.z > 0.001f) {
                            drawWireBox(bgDrawList, center, halfExt, IM_COL32(255, 200, 50, 120), 1.0f);
                        }
                    }
                }
            }
            // --- Bone visualization: wireframe skeleton for selected entities ---
            if (m_PrimarySelected != ECS::INVALID_ENTITY &&
                m_World->HasComponent<ECS::AnimatorComponent>(m_PrimarySelected)) {
                auto* animComp = m_World->GetComponent<ECS::AnimatorComponent>(m_PrimarySelected);
                if (animComp && animComp->showBones) {
                    const auto* skeleton = animComp->animator.GetSkeleton();
                    const auto& pose = animComp->animator.GetCurrentPose();
                    if (skeleton && !skeleton->bones.empty() &&
                        pose.worldTransforms.size() == skeleton->bones.size()) {
                        // Get entity world matrix for transforming bone-local positions
                        Math::Matrix4 entityWorld = ECS::ComputeWorldMatrix(m_World, m_PrimarySelected);

                        // Check for IK target bones (yellow highlight)
                        std::string lookAtBone, interactionBone;
                        auto* lookAtIK = m_World->GetComponent<ECS::LookAtIKComponent>(m_PrimarySelected);
                        if (lookAtIK) lookAtBone = lookAtIK->headBoneName;
                        auto* interactionIK = m_World->GetComponent<ECS::InteractionIKComponent>(m_PrimarySelected);
                        if (interactionIK) interactionBone = interactionIK->handBoneName;

                        for (usize i = 0; i < skeleton->bones.size(); ++i) {
                            const auto& bone = skeleton->bones[i];
                            // Get this bone's world position (bone-space -> entity-space -> world-space)
                            Math::Matrix4 boneWorld = entityWorld * pose.worldTransforms[i];
                            Math::Vector3 bonePos(boneWorld.m[12], boneWorld.m[13], boneWorld.m[14]);

                            // Determine color: yellow for IK targets, white for normal
                            bool isIKTarget = (!lookAtBone.empty() && bone.name == lookAtBone) ||
                                              (!interactionBone.empty() && bone.name == interactionBone);
                            ImU32 boneColor = isIKTarget ? IM_COL32(255, 220, 50, 220)
                                                         : IM_COL32(255, 255, 255, 200);

                            // Draw line from parent to this bone
                            if (bone.parentIndex >= 0 && bone.parentIndex < static_cast<i32>(skeleton->bones.size())) {
                                Math::Matrix4 parentWorld = entityWorld * pose.worldTransforms[bone.parentIndex];
                                Math::Vector3 parentPos(parentWorld.m[12], parentWorld.m[13], parentWorld.m[14]);
                                drawLine3D(bgDrawList, parentPos, bonePos, boneColor, 1.5f);
                            }

                            // Draw a small cross at each joint
                            ImVec2 jointScreen;
                            if (worldToScreen(bonePos, jointScreen)) {
                                f32 sz = isIKTarget ? 4.0f : 2.5f;
                                bgDrawList->AddLine(
                                    ImVec2(jointScreen.x - sz, jointScreen.y),
                                    ImVec2(jointScreen.x + sz, jointScreen.y),
                                    boneColor, 1.0f);
                                bgDrawList->AddLine(
                                    ImVec2(jointScreen.x, jointScreen.y - sz),
                                    ImVec2(jointScreen.x, jointScreen.y + sz),
                                    boneColor, 1.0f);
                            }
                        }
                    }
                }
            }

            bgDrawList->PopClipRect();
        }
    }

    // Stats overlay
    if (m_ShowStatsOverlay) {
        DrawStatsOverlay();
    }

    // Gamepad radial menu overlay
    if (m_RadialMenuActive != RadialMenuType::None) {
        DrawRadialMenu(m_RadialMenuActive);
    }

    // Demo window (for testing)
    if (m_ShowDemoWindow) {
        ImGui::ShowDemoWindow(&m_ShowDemoWindow);
    }

    // Import dialog
    if (m_ShowImportDialog) {
        DrawImportDialog();
    }

    // Deferred import: show loading overlay for one frame, then execute on next frame
    if (m_ImportPending) {
        DrawImportLoadingOverlay();
    }

    // Build dialog
    if (m_ShowBuildDialog) {
        DrawBuildDialog();
    }

    // Weather is now rendered per-camera in Game View panel only
    // (see DrawGameViewPanel for weather rendering)

    // Fade-in overlay (fades from black to transparent after splash)
    if (m_EditorFadeIn < 1.0f) {
        f32 overlayAlpha = 1.0f - m_EditorFadeIn;
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(io.DisplaySize);
        ImGui::SetNextWindowBgAlpha(overlayAlpha);

        ImGuiWindowFlags overlayFlags = ImGuiWindowFlags_NoDecoration |
                                        ImGuiWindowFlags_NoMove |
                                        ImGuiWindowFlags_NoResize |
                                        ImGuiWindowFlags_NoSavedSettings |
                                        ImGuiWindowFlags_NoFocusOnAppearing |
                                        ImGuiWindowFlags_NoBringToFrontOnFocus |
                                        ImGuiWindowFlags_NoNav |
                                        ImGuiWindowFlags_NoInputs;

        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.05f, 0.05f, 0.08f, 1.0f));
        ImGui::Begin("##FadeOverlay", nullptr, overlayFlags);
        ImGui::End();
        ImGui::PopStyleColor();
    }

    // Render dialogue overlay on top of editor panels during play
    if (m_PlayMode.IsPlaying() || m_PlayMode.IsPaused()) {
        DrawDialogueOverlay();
    }

    // Render subtitle overlay (accessibility) during play mode
    if (m_PlayMode.IsPlaying() || m_PlayMode.IsPaused()) {
        m_SubtitleSystem.RenderOverlay(
            static_cast<u32>(io.DisplaySize.x),
            static_cast<u32>(io.DisplaySize.y));
    }

    // Render audio visual indicators (accessibility)
    m_AudioIndicators.Update(m_LastDeltaTime);
    m_AudioIndicators.RenderOverlay(
        static_cast<u32>(io.DisplaySize.x),
        static_cast<u32>(io.DisplaySize.y));

    // Render accessibility announcer status bar
    m_Announcer.Update(m_LastDeltaTime);
    m_Announcer.RenderStatusBar();

    // Render command palette
    if (m_CommandPalette.IsOpen()) {
        if (m_CommandPalette.Render()) {
            m_Announcer.Announce("Executed: " + m_CommandPalette.GetLastExecutedCommand(),
                                Accessibility::AnnouncePriority::Normal);
        }
    }

    // Render alternative input overlays (switch scanning highlight, gaze indicator)
    m_AlternativeInput.RenderOverlay();

    // Render HUD widgets during play mode (editor game view)
    if (m_PlayMode.IsPlaying()) {
        m_PlayMode.GetHUDSystem()->Update(m_World, m_Camera,
            m_GameViewImageMinX, m_GameViewImageMinY,
            m_GameViewImageMaxX - m_GameViewImageMinX,
            m_GameViewImageMaxY - m_GameViewImageMinY);
    }

    // Render UI canvases during play mode (editor game view)
    if (m_PlayMode.IsPlaying()) {
        f32 gvW = m_GameViewImageMaxX - m_GameViewImageMinX;
        f32 gvH = m_GameViewImageMaxY - m_GameViewImageMinY;
        if (gvW > 0 && gvH > 0) {
            m_UISystem.Update(m_World, gvW, gvH, m_LastDeltaTime);
        }
    }

    // Render UI editor overlay (design-time WYSIWYG preview in Game View)
    if (m_UIEditMode && m_PlayMode.IsStopped()) {
        DrawUIEditorOverlay();
    }

    // Pause menu is now rendered inside the Game View panel (DrawGameViewPanel)
    // to prevent it from overlapping the Scene tab.

    // Quake-style drop-down console (always on top of editor panels)
    DrawDropConsole(m_LastDeltaTime);

    // Draw notification toasts (always on top)
    DrawNotifications(m_LastDeltaTime);

    // End profiler frame measurement
    Debug::Profiler::Instance().EndFrame();

    m_ImGuiLayer->EndFrame(commandBuffer);
}

void EditorLayer::SetPanelVisibility(EditorPanel panel, bool visible) {
    if (visible) {
        m_VisiblePanels = m_VisiblePanels | panel;
    } else {
        m_VisiblePanels = static_cast<EditorPanel>(
            static_cast<u32>(m_VisiblePanels) & ~static_cast<u32>(panel));
    }
}

bool EditorLayer::IsPanelVisible(EditorPanel panel) const {
    return HasPanel(m_VisiblePanels, panel);
}

bool EditorLayer::WantsKeyboardInput() const {
    return ImGui::GetIO().WantCaptureKeyboard;
}

bool EditorLayer::WantsMouseInput() const {
    return ImGui::GetIO().WantCaptureMouse;
}

// --- Multi-select helpers ---

void EditorLayer::SelectEntity(ECS::Entity entity, bool addToSelection) {
    if (entity == ECS::INVALID_ENTITY) return;
    if (!addToSelection) {
        m_SelectedEntities.clear();
    }
    m_SelectedEntities.insert(entity);
    m_PrimarySelected = entity;
    if (m_OnEntitySelected) m_OnEntitySelected(entity);

    // Accessibility announcement
    if (m_Announcer.enabled && m_World) {
        std::string name = "Entity";
        auto* nc = m_World->GetComponent<ECS::NameComponent>(entity);
        if (nc) name = nc->name;
        m_Announcer.Announce("Selected: " + name, Accessibility::AnnouncePriority::Low);
    }
}

void EditorLayer::DeselectEntity(ECS::Entity entity) {
    m_SelectedEntities.erase(entity);
    if (m_PrimarySelected == entity) {
        m_PrimarySelected = m_SelectedEntities.empty() ? ECS::INVALID_ENTITY : *m_SelectedEntities.begin();
    }
}

void EditorLayer::ClearSelection() {
    m_SelectedEntities.clear();
    m_PrimarySelected = ECS::INVALID_ENTITY;
}

bool EditorLayer::IsSelected(ECS::Entity entity) const {
    return m_SelectedEntities.count(entity) > 0;
}

void EditorLayer::SetSelectedEntity(ECS::Entity entity) {
    ClearSelection();
    if (entity != ECS::INVALID_ENTITY) {
        SelectEntity(entity);
    }
}

void EditorLayer::SelectRange(ECS::Entity from, ECS::Entity to) {
    if (!m_World) return;
    const auto& entities = m_World->GetAllEntities();

    // Find indices of from and to in the entity list
    i64 fromIdx = -1, toIdx = -1;
    for (usize i = 0; i < entities.size(); i++) {
        if (entities[i] == from) fromIdx = static_cast<i64>(i);
        if (entities[i] == to) toIdx = static_cast<i64>(i);
    }
    if (fromIdx < 0 || toIdx < 0) return;

    // Ensure from <= to
    if (fromIdx > toIdx) std::swap(fromIdx, toIdx);

    for (i64 i = fromIdx; i <= toIdx; i++) {
        m_SelectedEntities.insert(entities[static_cast<usize>(i)]);
    }
    m_PrimarySelected = to;
}

void EditorLayer::SelectEntitiesInRect(ImVec2 min, ImVec2 max) {
    if (!m_World || !m_Camera) return;
    f32 vpW = m_EditorViewportImageMaxX - m_EditorViewportImageMinX;
    f32 vpH = m_EditorViewportImageMaxY - m_EditorViewportImageMinY;
    if (vpW <= 0 || vpH <= 0) return;

    // Convert screen-space marquee to viewport-local coords
    auto entities = ScenePicker::PickEntitiesInScreenRect(
        m_World, m_Camera,
        min.x - m_EditorViewportImageMinX, min.y - m_EditorViewportImageMinY,
        max.x - m_EditorViewportImageMinX, max.y - m_EditorViewportImageMinY,
        vpW, vpH);

    for (ECS::Entity e : entities) {
        m_SelectedEntities.insert(e);
        m_PrimarySelected = e;
    }
}


void EditorLayer::MarkDirty() {
    if (!m_SceneDirty) {
        m_SceneDirty = true;
        UpdateWindowTitle();
    }
}

void EditorLayer::ClearDirty() {
    if (m_SceneDirty) {
        m_SceneDirty = false;
        m_AutoSaveTimer = 0.0f;
        UpdateWindowTitle();
    }
}

void EditorLayer::UpdateWindowTitle() {
    if (!m_Window) return;
    std::string title = "TEGE";
    if (!m_CurrentScenePath.empty()) {
        auto fname = std::filesystem::path(m_CurrentScenePath).filename().string();
        title += " - " + fname;
    }
    if (m_SceneDirty) title += " *";
    m_Window->SetTitle(title.c_str());
}

} // namespace Editor
} // namespace Enjin
