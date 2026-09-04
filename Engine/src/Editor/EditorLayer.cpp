#include "Enjin/Editor/EditorLayer.h"
#include "Enjin/Input/TouchActionBridge.h"
#include <nlohmann/json.hpp>
#include "Enjin/Editor/InspectorUndo.h"
#include "Enjin/Editor/ScenePicker.h"
#include "Enjin/Core/Version.h"
#include "Enjin/Debug/CrashHandler.h"
#include "Enjin/Editor/EditorTheme.h"
#include <GLFW/glfw3.h>
#include <chrono>
#include <unordered_set>
#include "Enjin/Logging/Log.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Mesh.h"
#include "Enjin/ECS/Components/Material.h"
#include "Enjin/ECS/Components/Light.h"
#include "Enjin/ECS/Components/Name.h"
#include "Enjin/ECS/Components/Camera.h"
#include "Enjin/ECS/Components/Notes.h"
#include "Enjin/ECS/Components/StableId.h"
#include "Enjin/Assets/MeshAssetCache.h"
#include "Enjin/ECS/Components/Controllers/CharacterController.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/ECS/Components/VisualScript.h"
#include "Enjin/AI/BehaviorTree.h"
#include "Enjin/Gameplay/QuestFlow.h"
#include "Enjin/Gameplay/ClothSystem.h"   // EnsureBuilt: cloth/ropes visible in edit mode
#include "Enjin/Gameplay/TieredSaveSystem.h"
#include "Enjin/Editor/PlayModeDiff.h"
#include "Enjin/Physics/PhysicsBackendType.h"
#include "Enjin/Physics/PhysicsBackendFactory.h"
#include "Enjin/Physics/PhysicsTypes2D.h"
#include "Enjin/ECS/Components/WeatherZone.h"
#include "Enjin/ECS/Components/CustomShader.h"
#include "Enjin/ECS/Components/WaterVolume.h"
#include "Enjin/ECS/Components/BoundaryPolygon.h"
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
#include "Enjin/Effects/Wind.h"
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
#include "Enjin/Platform/Paths.h"
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
#include "Enjin/Scripting/ScriptEngine.h"
#include "Enjin/VisualScript/ScriptApiNodes.h"
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
#include <stb_image_write.h>  // --golden PNG capture (impl lives in the stb TU)
#include <filesystem>
#include <ctime>    // GIF clip timestamped filenames
#include "Enjin/Gameplay/Replay.h"
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

    // Count comes from the default, so the editor and a shipped game agree.
    m_WeatherSystem.Initialize();

    // Initialize elemental system (connects to wind + weather for particle interactions)
    m_ElementalSystem.Initialize(&m_WindSystem, &m_WeatherSystem, &m_SeasonalWeather);
    m_FireLights.reserve(Effects::ElementalSystem::MAX_FIRE_LIGHTS);

    // Wind system is always running (affects weather, vegetation, grass)
    // Will be connected to RenderSystem when SetRenderSystem is called

    // Render targets for Game View (offscreen rendering)
    // Scene RT: raw scene output (input to post-processing)
    // Game View RT: final post-processed output (displayed in ImGui)
    // ImGui texture callbacks — editor-only, enables RenderTarget to display in ImGui panels.
    // RenderTarget itself has no ImGui dependency; these callbacks bridge the gap.
    auto imguiRegister = [](VkSampler s, VkImageView v, VkImageLayout l) -> VkDescriptorSet {
        return ImGui_ImplVulkan_AddTexture(s, v, l);
    };
    auto imguiUnregister = [](VkDescriptorSet ds) {
        ImGui_ImplVulkan_RemoveTexture(ds);
    };

    m_SceneRenderTarget = std::make_unique<Renderer::RenderTarget>();
    m_SceneRenderTarget->SetTextureCallbacks(imguiRegister, imguiUnregister);
    if (!m_SceneRenderTarget->Create(renderer, m_GameViewWidth, m_GameViewHeight)) {
        ENJIN_LOG_WARN(Editor, "Failed to create Scene render target");
        m_SceneRenderTarget.reset();
    }

    m_GameViewRenderTarget = std::make_unique<Renderer::RenderTarget>();
    m_GameViewRenderTarget->SetTextureCallbacks(imguiRegister, imguiUnregister);
    if (!m_GameViewRenderTarget->Create(renderer, m_GameViewWidth, m_GameViewHeight)) {
        ENJIN_LOG_WARN(Editor, "Failed to create Game View render target");
        m_GameViewRenderTarget.reset();
    }

    // Editor viewport render target (offscreen rendering for scene editing camera)
    m_EditorViewportRT = std::make_unique<Renderer::RenderTarget>();
    m_EditorViewportRT->SetTextureCallbacks(imguiRegister, imguiUnregister);
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
    // Apply dyslexia-friendly spacing to ImGui (increased item/frame padding for readability)
    if (m_EditorSettings.dyslexiaFontEnabled) {
        ImGuiStyle& style = ImGui::GetStyle();
        style.ItemSpacing.y = std::max(style.ItemSpacing.y, 6.0f);
        style.FramePadding.y = std::max(style.FramePadding.y, 5.0f);
    }
    Input::SetRawMouseInput(m_EditorSettings.rawMouseInput);
    Input::SetMouseSmoothing(m_EditorSettings.mouseSmoothing);
    // Apply motor accessibility settings to ImGui
    ImGui::GetIO().MouseDragThreshold = m_EditorSettings.dragThreshold;
    ImGui::GetIO().KeyRepeatDelay = m_EditorSettings.holdRepeatDelay;
    ImGui::GetIO().KeyRepeatRate = m_EditorSettings.holdRepeatRate;
    // Apply accessibility visual settings (colorblind, brightness, contrast, flashing) on startup
    if (m_PostProcessing) {
        auto& ppSettings = m_PostProcessing->GetSettings();
        ppSettings.colorblindMode = m_EditorSettings.colorblindMode;
        ppSettings.colorblindStrength = m_EditorSettings.colorblindStrength;
        ppSettings.brightness = m_EditorSettings.screenBrightness;
        ppSettings.contrast = m_EditorSettings.screenContrast;
        if (m_EditorSettings.disableFlashingLights) {
            ppSettings.filmGrainEnabled = 0;
            ppSettings.crtEnabled = 0;
            ppSettings.vhsEnabled = 0;
        }
    }
    // Sync runtime accessibility settings from editor settings
    SyncRuntimeAccessibility();
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
    // Touches (View > Simulate Touch Controls) that land on interactive UI
    // become real pointers instead of being claimed by the move stick.
    InputSystem::SetUIHitTestSystem(&m_UISystem);
    m_GameMenu.SetEditorSettings(&m_EditorSettings);
    // Accessibility tab (same tab exported games get). The menu edits
    // m_RuntimeAccessibility in place; reverse-sync the editor-settings twins
    // so the next SyncRuntimeAccessibility (settings edit / play start) doesn't
    // stomp the player's menu choices, and push the boot-time consumers.
    m_GameMenu.SetAccessibilitySettings(&m_RuntimeAccessibility);
    m_GameMenu.SetAccessibilityChangedCallback([this]() {
        auto& a = m_RuntimeAccessibility;
        auto& s = m_EditorSettings;
        s.colorblindMode = static_cast<decltype(s.colorblindMode)>(a.colorblindMode);
        s.colorblindStrength = a.colorblindStrength;
        s.screenBrightness = a.screenBrightness;
        s.screenContrast = a.screenContrast;
        s.reducedMotion = a.reducedMotion;
        s.disableScreenShake = a.disableScreenShake;
        s.disableFOVEffects = a.disableFOVEffects;
        s.disableFlashingLights = a.disableFlashingLights;
        s.subtitlesEnabled = a.subtitlesEnabled;
        s.closedCaptionsEnabled = a.closedCaptionsEnabled;
        s.subtitleFontSize = a.subtitleFontSize;
        s.subtitleBgOpacity = a.subtitleBgOpacity;
        s.subtitleSpeakerNames = a.subtitleSpeakerNames;
        s.gameFontScale = a.fontScale;
        s.dyslexiaFontEnabled = a.dyslexiaFriendly;
        s.dwellClickEnabled = a.dwellClickEnabled;
        s.dwellClickDelay = a.dwellClickTime;
        s.stickyDragEnabled = a.stickyDragEnabled;

        // Push consumers that only read on demand
        m_Announcer.enabled = a.screenReaderEnabled;
        m_AudioIndicators.GetConfig().enabled = a.audioIndicatorsEnabled;
        auto& subConfig = m_SubtitleSystem.GetConfig();
        subConfig.enabled = a.subtitlesEnabled;
        subConfig.captionsEnabled = a.closedCaptionsEnabled;
        subConfig.fontSize = a.subtitleFontSize;
        subConfig.backgroundOpacity = a.subtitleBgOpacity;
        subConfig.showSpeakerNames = a.subtitleSpeakerNames;
        subConfig.showDirectionIndicators = a.subtitleDirectionIndicators;
        if (auto* ctrlSys = m_PlayMode.GetControllerSystem()) {
            ctrlSys->SetReducedMotion(a.reducedMotion);
            ctrlSys->SetDisableScreenShake(a.disableScreenShake);
            ctrlSys->SetDisableFOVEffects(a.disableFOVEffects);
        }
        if (auto* uiSys = m_PlayMode.GetUISystem()) {
            uiSys->SetReducedMotion(a.reducedMotion);
            uiSys->SetSwitchAccessEnabled(a.switchAccessEnabled, a.switchScanSpeed);
            // Same setting drives the editor-chrome scanner, so the two
            // never disagree about being on or scan at different speeds.
            m_AlternativeInput.ApplyAccessibilitySettings(a.switchAccessEnabled, a.switchScanSpeed);
            uiSys->SetDwellClickEnabled(a.dwellClickEnabled, a.dwellClickTime);
            uiSys->SetStickyDragEnabled(a.stickyDragEnabled);
        }
        // Text scale reaches the UI, subtitles and the screen-reader bar
        // together (it used to reach only the UI).
        Accessibility::ApplyTextScale(a, m_PlayMode.GetUISystem(),
                                      &m_SubtitleSystem, &m_Announcer);
    });
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
        } else if (action == "restart") {
            // Restart: stop play mode and immediately start again (reloads scene)
            m_GameMenu.HideAll();
            m_PendingPlayStop = true;
            m_PendingPlayRestart = true;
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

    // Fill the Options menu from live state when it opens — otherwise Back
    // applies the menu's struct defaults (bloom=true) over the panel settings
    m_GameMenu.SetSettingsSyncCallback([this](GUI::GraphicsSettings& gfx,
                                              GUI::AudioSettings& audio) {
        if (m_PostProcessing) {
            gfx.bloom = m_PostProcessing->GetSettings().bloomEnabled != 0;
            gfx.fxaa = m_PostProcessing->GetSettings().fxaaEnabled != 0;
        }
        if (m_RenderSystem) gfx.shadows = m_RenderSystem->IsShadowsEnabled();
        auto* sa = m_PlayMode.GetSimpleAudio();
        if (sa) audio.masterVolume = sa->GetMasterVolume();
    });

    // Apply graphics/audio settings when user exits Options menu in play mode
    m_GameMenu.SetSettingsCallback([this](const GUI::GraphicsSettings& gfx,
                                          const GUI::AudioSettings& audio) {
        // Audio (via PlayMode's SimpleAudio instance)
        auto* sa = m_PlayMode.GetSimpleAudio();
        if (sa) {
            sa->SetMasterVolume(audio.masterMute ? 0.0f : audio.masterVolume);
            sa->SetChannelVolume(Audio::AudioChannel::Music, audio.musicMute ? 0.0f : audio.musicVolume);
            sa->SetChannelVolume(Audio::AudioChannel::SFX, audio.sfxMute ? 0.0f : audio.sfxVolume);
            sa->SetChannelVolume(Audio::AudioChannel::Voice, audio.voiceMute ? 0.0f : audio.voiceVolume);
        }

        if (!m_RenderSystem) return;

        // VSync (deferred)
        if (m_Renderer) m_Renderer->RequestVSyncChange(gfx.vsync);

        // Shadows
        m_RenderSystem->SetShadowsEnabled(gfx.shadows);
        if (gfx.shadows) {
            static const u32 shadowRes[] = { 512, 1024, 2048, 4096 };
            u32 idx = gfx.shadowQuality < 4 ? gfx.shadowQuality : 2;
            m_RenderSystem->SetShadowResolution(shadowRes[idx]);
        }

        // Post-processing
        if (m_PostProcessing) {
            m_PostProcessing->GetSettings().bloomEnabled = gfx.bloom;
            m_PostProcessing->GetSettings().fxaaEnabled = gfx.fxaa;
        }

        // FOV — update the active game camera component
        if (m_World && gfx.fieldOfView >= 40.0f && gfx.fieldOfView <= 120.0f) {
            auto cam = ECS::CameraManager::GetActiveCamera(m_World);
            if (cam != ECS::INVALID_ENTITY) {
                auto* cc = m_World->GetComponent<ECS::CameraComponent>(cam);
                if (cc) cc->fieldOfView = gfx.fieldOfView;
            }
        }

        // Fullscreen — save pre-play state so we can restore on Stop
        if (m_Window) {
            bool isFS = m_Window->IsFullscreen();
            if (gfx.fullscreen != isFS) {
                if (!m_PlayMode.IsStopped() && !m_PrePlayFullscreenSaved) {
                    m_PrePlayFullscreen = isFS;
                    m_PrePlayFullscreenSaved = true;
                }
                m_Window->SetFullscreen(gfx.fullscreen);
            }
        }

        ENJIN_LOG_INFO(Editor, "Play mode settings applied: vsync=%d fullscreen=%d fov=%.0f shadows=%d bloom=%d fxaa=%d",
            (int)gfx.vsync, (int)gfx.fullscreen, gfx.fieldOfView, (int)gfx.shadows, (int)gfx.bloom, (int)gfx.fxaa);
    });

    // Register file drop callback for drag-and-drop import
    if (m_Window) {
        m_Window->SetDropCallback([this](int count, const char** paths) {
            OnFileDrop(count, paths);
        });
    }

    // Intercept window close to prompt for unsaved changes or show quit feedback
    if (m_Window) {
        m_Window->SetCloseCallback([this]() -> bool {
            // If we're still on the project hub (no project open) or pending quit, close immediately
            if (m_ShowProjectHub || m_PendingQuit) {
                return true; // Allow close
            }
            // If a quit prompt or unsaved changes dialog is already active and user closes again, allow close
            if (m_ShowUnsavedChangesDialog || m_ShowQuitFeedbackDialog) {
                return true; // Allow immediate close
            }
            if (m_SceneDirty) {
                m_UnsavedChangesAction = UnsavedAction::Quit;
                m_ShowUnsavedChangesDialog = true;
                return false; // Cancel close — dialog will handle it
            }
            // Scene is clean — show quit feedback survey
            m_ShowQuitFeedbackDialog = true;
            return false; // Block close — FinalizeQuit() will call Close()
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
                m_World->SetEntityName(entity, op.dataJson);
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

    // If launched with a project file (double-click .enjinproject), open it
    // directly and skip the Project Hub.
    if (!s_LaunchProjectPath.empty()) {
        namespace fs = std::filesystem;
        std::string launchPath = s_LaunchProjectPath;
        s_LaunchProjectPath.clear();

        if (fs::exists(launchPath)) {
            if (launchPath.find(".enjinproject") != std::string::npos) {
                if (m_SceneManager.LoadProject(launchPath)) {
                    m_EditorSettings.AddRecentProject(launchPath);
                    m_EditorSettings.lastProjectDir = fs::path(launchPath).parent_path().string();
                    m_EditorSettings.Save();
                    auto& scenes = m_SceneManager.GetScenes();
                    if (!scenes.empty()) {
                        auto projDir = fs::path(launchPath).parent_path();
                        OpenScene((projDir / scenes[0].path).string());
                    }
                    m_ShowProjectHub = false;
                    m_ShowSplash = false;
                    ENJIN_LOG_INFO(Editor, "Opened project from launch: %s", launchPath.c_str());
                }
            } else if (launchPath.find(".enjin") != std::string::npos) {
                OpenScene(launchPath);
                m_ShowProjectHub = false;
                m_ShowSplash = false;
            }
        }
    }

    // Start telemetry session
    m_Telemetry.Load();
    m_Telemetry.BeginSession();

    // Record session start time for quit survey duration
    m_SessionStartTime = std::chrono::steady_clock::now();

    // VS palette codegen: reflect the whole script API into visual-script
    // nodes so the node menu has them BEFORE the first play session (play
    // would register them too, but the palette would be bare until then).
    // Same throwaway-engine pattern as ExportScriptApiStub; the generated
    // nodes resolve functions by declaration at call time, so the reflection
    // engine's lifetime doesn't matter.
    {
        Scripting::ScriptEngine se;
        if (se.Initialize()) {
            Scripting::RegisterAllBindings(se.GetASEngine());
            VisualScript::RegisterScriptApiNodes(se.GetASEngine());
            se.Shutdown();
        }
    }

    ENJIN_LOG_INFO(Editor, "EditorLayer initialized");
    return true;
}

void EditorLayer::SetRenderSystem(ECS::RenderSystem* renderSystem) {
    m_RenderSystem = renderSystem;
    m_ParallaxSystem.SetRenderSystem(renderSystem);
    // InitializePlayMode() fires from SetCamera, BEFORE this setter runs, so
    // PlayMode captured a null render system there — push the real one through
    // (surface-response particles/impact sounds and cloth wind depend on it).
    m_PlayMode.SetRenderSystem(renderSystem);

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
    m_CachedPlayerEntity = ECS::INVALID_ENTITY;
    m_PlayMode.SetDebugRecording(m_EditorSettings.debugRecordPlay, m_EditorSettings.debugRecordSeconds);
    m_DebugScrubOffset = 0.0f;
    m_PlayMode.Play();
    m_Telemetry.TrackPlayModeEnter();
    if (m_Announcer.enabled) m_Announcer.Announce("Play mode started", Accessibility::AnnouncePriority::Normal);
}

void EditorLayer::InitializePlayMode() {
    if (m_World && m_Camera && m_CameraController) {
        // ONE action map for the whole editor. PlayMode borrows it, so the
        // Controls menu, ControllerSystem, script bindings, ActionTriggers
        // and the touch overlay all read the same bindings. Injected before
        // Initialize, which hands it to ControllerSystem.
        m_PlayMode.SetInputActionMap(&m_InputMap);
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
        SyncRuntimeAccessibility();
        m_PlayMode.SetAccessibilitySettings(&m_RuntimeAccessibility);

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
            ctrlSys->SetDisableScreenShake(m_EditorSettings.disableScreenShake);
            ctrlSys->SetDisableFOVEffects(m_EditorSettings.disableFOVEffects);
        }

        // Wire reduced motion to UISystem
        if (m_PlayMode.GetUISystem()) {
            m_PlayMode.GetUISystem()->SetReducedMotion(m_EditorSettings.reducedMotion);
        }

        // Control preset and sprint/crouch modes are properties of the action
        // map itself now, set from the in-editor Controls menu and persisted
        // with the rest of the bindings.

        // Project-authored input settings: the SAME data an exported game reads
        // from its manifest, so the editor previews the real controls. Applied
        // after the presets above, which reset bindings to defaults.
        auto& inputSettings = m_SceneManager.GetInputSettings();
        inputSettings.ApplyTo(m_InputMap);
        InputSystem::SetTouchProjectSettings(&inputSettings);
    }
}

void EditorLayer::Shutdown() {
    // A build still running on the worker thread must finish before teardown
    // (it only touches files, but the thread must not outlive the editor).
    if (m_BuildThread.joinable()) {
        ENJIN_LOG_INFO(Editor, "Waiting for the in-flight build to finish...");
        m_BuildThread.join();
    }

    // End telemetry session (saves aggregate data to disk)
    m_Telemetry.EndSession();

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

    // Cloth/ropes need their generated mesh even in EDIT mode (the sim only
    // runs during play) - build any uninitialized ones to rest pose so a
    // freshly loaded scene shows them. Cheap flag check when nothing changed.
    if (m_World) Gameplay::ClothSystem::EnsureBuilt(m_World);

    // MCP-injected input (AI-driven testing) - merged with live hardware.
    ProcessMcpInput(deltaTime);

    // Game-view sims tick after PlayMode below (UpdateGameViewSims) - the
    // old render-path accumulator (m_GameViewSimAccum) is gone.

    // Keep the mesh-reference cache pointed at the current project root so imported
    // meshes stored as project-relative references resolve on scene load/save. Cheap
    // no-op once set; only re-clears the cache when the project actually changes.
    {
        const std::string& projPath = m_SceneManager.GetProjectPath();
        if (!projPath.empty()) {
            Assets::MeshAssetCache::Get().SetSearchRoot(
                std::filesystem::path(projPath).parent_path().string());
        }
    }

    // Selection highlight is now drawn as projected bounding boxes in the viewport
    // overlay (EditorLayer::DrawSelectionHighlight) — always visible, works for
    // compute-skinned meshes. Keep the old inverted-hull render pass idle by
    // feeding it an empty set (it produced no visible pixels for skinned meshes).
    if (m_RenderSystem) m_RenderSystem->SetHighlightEntities({});

    // Keep material edits live in the editor viewport. The material SSBO the shader
    // reads has a fast path that re-uploads cached values unless something marks it
    // dirty, so inspector edits (color/opacity/roughness/textures/presets) only
    // showed after Play forced a rebuild — and only some edit paths marked it dirty,
    // which is why it was intermittent. In edit mode (not playing) mark it dirty
    // every frame so every edit path refreshes immediately. Play keeps the cached
    // fast path, so shipped-game performance is unaffected.
    if (m_RenderSystem && !m_PlayMode.IsPlaying()) m_RenderSystem->MarkMaterialsDirty();

    // Deferred quit (Close() called during ImGui rendering crashes some drivers)
    if (m_PendingQuit) {
        m_PendingQuit = false;
        if (m_Window) m_Window->Close();
        return;
    }

    // Begin profiler frame measurement
    Debug::Profiler::Instance().BeginFrame();

    // Surface script errors the moment play starts. A broken script otherwise
    // fails silently into the log and the game just "does nothing" — the
    // single most confusing beginner experience. Toast + force the Console
    // open so the compiler's message (with line numbers) is on screen.
    if (m_PlayMode.IsPlaying()) {
        if (!m_ScriptErrorsChecked) {
            m_ScriptErrorsChecked = true;
            if (m_World) {
                u32 errorCount = 0;
                std::string firstError;
                for (auto entity : m_World->GetEntitiesWithComponent<ECS::ScriptComponent>()) {
                    auto* sc = m_World->GetComponent<ECS::ScriptComponent>(entity);
                    if (!sc) continue;
                    for (const auto& att : sc->scripts) {
                        if (att.hasError) {
                            ++errorCount;
                            if (firstError.empty()) firstError = att.lastError;
                        }
                    }
                }
                if (errorCount > 0) {
                    std::string msg = (errorCount == 1)
                        ? ("Script error: " + firstError)
                        : (std::to_string(errorCount) + " script errors — first: " + firstError);
                    ShowNotification(msg, NotificationType::Error);
                    m_VisiblePanels = m_VisiblePanels | EditorPanel::Console;
                }
            }
        }
    } else {
        m_ScriptErrorsChecked = false;
    }

    // --compute-skinning probe support: force ADR-0002 compute skinning on.
    if (s_ComputeSkinningOnLaunch && m_RenderSystem) {
        s_ComputeSkinningOnLaunch = false;
        m_RenderSystem->SetComputeSkinningEnabled(true);
        ENJIN_LOG_INFO(Editor, "--compute-skinning: compute skinning forced ON");
    }

    // --play probe support: enter play mode ~2s after boot, once the launch
    // project's scene is fully loaded and a few frames have rendered.
    if (s_AutoPlayOnLaunch && m_RenderSystem) {
        static int s_AutoPlayCountdown = 120;
        if (s_AutoPlayCountdown > 0 && --s_AutoPlayCountdown == 0) {
            s_AutoPlayOnLaunch = false;
            if (m_PlayMode.IsStopped()) {
                StartPlayMode();
                ENJIN_LOG_INFO(Editor, "--play: auto-entered play mode");
            }
        }
    }

    // --play-cycle probe support: stop and restart play mode every N frames via
    // the same deferred stop path the toolbar uses. Exercises the
    // play -> stop-restore -> play transition (skinned-mesh crash repro).
    if (s_PlayCycleFrames > 0 && m_RenderSystem) {
        static int s_cycleCountdown = 0;
        static int s_restartDelay = 0;
        if (m_PlayMode.IsPlaying()) {
            if (s_cycleCountdown == 0) s_cycleCountdown = s_PlayCycleFrames;
            if (--s_cycleCountdown == 0) {
                // Destroy a skinned entity first so Stop takes the FULL-RELOAD
                // restore path (recreates skinned entities from JSON — the
                // documented use-after-free shape for the play-transition crash)
                if (m_World) {
                    auto skinned = m_World->GetEntitiesWithComponent<ECS::SkeletonComponent>();
                    if (!skinned.empty()) {
                        m_World->DestroyEntity(skinned.front());
                        ENJIN_LOG_INFO(Editor, "--play-cycle: destroyed skinned entity %u to force full restore",
                            (u32)skinned.front());
                    }
                }
                m_PendingPlayStop = true;
                ENJIN_LOG_INFO(Editor, "--play-cycle: requesting stop");
            }
        } else if (m_PlayMode.IsStopped() && !m_PendingPlayStop && !s_AutoPlayOnLaunch) {
            if (++s_restartDelay >= 30) {  // let the restore settle half a second
                s_restartDelay = 0;
                // T4 stress accounting: one full play->stop->restore cycle done.
                static i32 s_cyclesDone = 0;
                static u64 s_warmupRSS = 0;
                ++s_cyclesDone;
                u64 rss = Platform::GetProcessMemoryBytes();
                if (s_cyclesDone == 3) s_warmupRSS = rss;   // post-warmup baseline
                if (s_PlayCycleMax > 0) {
                    ENJIN_LOG_INFO(Editor, "--play-cycle: cycle %d/%d, rss %.1f MB",
                                   s_cyclesDone, s_PlayCycleMax, rss / (1024.0 * 1024.0));
                    if (s_cyclesDone >= s_PlayCycleMax) {
                        // Pass: memory bounded (< baseline + 50% + 128MB slack).
                        bool ok = (s_warmupRSS == 0) ||
                                  (rss < s_warmupRSS + s_warmupRSS / 2 + 128ull * 1024 * 1024);
                        s_PlayCycleExitCode = ok ? 0 : 2;
                        ENJIN_LOG_INFO(Editor, "--play-cycle: DONE %d cycles, rss %.1f MB "
                                       "(baseline %.1f MB) -> %s",
                                       s_cyclesDone, rss / (1024.0 * 1024.0),
                                       s_warmupRSS / (1024.0 * 1024.0), ok ? "PASS" : "FAIL");
                        if (m_Window) m_Window->Close();
                        return;
                    }
                }
                StartPlayMode();
                ENJIN_LOG_INFO(Editor, "--play-cycle: re-entered play mode");
            }
        }
    }

    // Script scene requests during editor play (Scene_Restart / Scene_LoadScene):
    // consumed here at a safe point, executed as deferred stop -> (open) -> play
    // so a script can restart or switch scenes in the editor exactly like it
    // does in an exported game. Same plumbing as the toolbar stop.
    if (m_PlayMode.IsPlaying()) {
        std::string reqScene;
        auto req = m_SceneManager.TakeSceneRequest(reqScene);
        if (req == Scene::SceneManager::SceneRequest::Restart) {
            m_RestartPlayPending = true;
            m_PendingPlayStop = true;
            ENJIN_LOG_INFO(Editor, "Scene_Restart: restarting play session");
        } else if (req == Scene::SceneManager::SceneRequest::Load) {
            const auto* entry = m_SceneManager.GetSceneByName(reqScene);
            if (entry) {
                std::filesystem::path root =
                    std::filesystem::path(m_SceneManager.GetProjectPath()).parent_path();
                m_RestartPlayScene = (root / entry->path).string();
                m_RestartPlayPending = true;
                m_PendingPlayStop = true;
                ENJIN_LOG_INFO(Editor, "Scene_LoadScene: switching play session to '%s'", reqScene.c_str());
            } else {
                ENJIN_LOG_WARN(Editor, "Scene_LoadScene: '%s' not in the project scene list", reqScene.c_str());
            }
        }
    } else if (m_RestartPlayPending && m_PlayMode.IsStopped() && !m_PendingPlayStop) {
        if (!m_RestartPlayScene.empty()) {
            OpenScene(m_RestartPlayScene);   // deferred load into the editor world
            m_RestartPlayScene.clear();
            m_RestartPlayDelay = 0;
        } else if (m_PendingSceneLoadPath.empty() && ++m_RestartPlayDelay >= 10) {
            m_RestartPlayPending = false;
            m_RestartPlayDelay = 0;
            StartPlayMode();
        }
    }

    // MCP server: reconcile with the setting, pump queued tool calls onto this
    // (the main) thread. adr-0004: all World mutation happens right here.
    {
        bool want = m_EditorSettings.mcpServerEnabled;
        if (want && !m_McpServer.IsRunning()) {
            m_McpServer.SetWorld(m_World);
            m_McpServer.SetSceneInfoHook([this]() {
                nlohmann::json j;
                j["projectPath"] = m_SceneManager.GetProjectPath();
                j["playState"] = m_PlayMode.IsPlaying() ? "playing"
                               : m_PlayMode.IsPaused() ? "paused" : "stopped";
                j["entityCount"] = m_World ? m_World->GetAllEntities().size() : 0;
                if (m_BuildInProgress) {
                    std::lock_guard<std::mutex> lock(m_BuildMutex);
                    j["buildState"] = "building";
                    j["buildPhase"] = m_BuildWorkerPhase;
                    j["buildProgress"] = m_BuildWorkerProgress;
                } else if (m_BuildFinished) {
                    j["buildState"] = m_BuildResult.success ? "succeeded" : "failed";
                }
                return j.dump();
            });
            m_McpServer.SetPlayControlHook([this](const std::string& action) -> std::string {
                if (action == "play")   { if (m_PlayMode.IsStopped()) { StartPlayMode(); return "playing"; } return "already in play mode"; }
                if (action == "pause")  { if (m_PlayMode.IsPlaying()) { m_PlayMode.Pause(); return "paused"; } return "not playing"; }
                if (action == "resume") { if (m_PlayMode.IsPaused()) { m_PlayMode.Resume(); return "resumed"; } return "not paused"; }
                if (action == "stop")   { if (!m_PlayMode.IsStopped()) { m_PendingPlayStop = true; return "stopping"; } return "not in play mode"; }
                return "unknown action '" + action + "' (play|pause|resume|stop)";
            });
            m_McpServer.SetCaptureHook([this]() -> std::string {
                std::string base = (std::filesystem::temp_directory_path() / "tege_mcp_capture").string();
                if (!CaptureGameViewToFile(base)) return "";
                return base + ".png";
            });
            // Tier-0 tools from the Ink_Ribbon feature request: scene
            // switching, save, and log access (the console ring buffer -
            // enjin.log on disk is write-buffered and useless mid-session).
            m_McpServer.SetEditorToolHook([this](const std::string& op,
                                                 const std::string& argsJson) -> std::string {
                nlohmann::json args = nlohmann::json::parse(argsJson, nullptr, false);
                if (args.is_discarded()) args = nlohmann::json::object();

                if (op == "list_scenes") {
                    nlohmann::json j;
                    j["currentScene"] = m_CurrentScenePath;
                    j["playState"] = m_PlayMode.IsPlaying() ? "playing"
                                   : m_PlayMode.IsPaused() ? "paused" : "stopped";
                    nlohmann::json scenes = nlohmann::json::array();
                    for (const auto& s : m_SceneManager.GetScenes()) {
                        scenes.push_back({{"path", s.path},
                                          {"buildIndex", s.buildIndex},
                                          {"isStartScene", s.isStartScene}});
                    }
                    j["scenes"] = std::move(scenes);
                    return j.dump();
                }
                if (op == "open_scene") {
                    if (!m_PlayMode.IsStopped())
                        return "error: stop play mode first (play_control stop)";
                    std::string rel = args.value("path", "");
                    if (rel.empty()) return "error: 'path' is required";
                    std::string projPath = m_SceneManager.GetProjectPath();
                    if (projPath.empty()) return "error: no project open";
                    std::string projDir = std::filesystem::path(projPath).parent_path().string();
                    std::string resolved = Platform::ResolveWithinRoot(projDir, rel);
                    if (resolved.empty()) return "error: path escapes the project root";
                    if (!std::filesystem::exists(resolved))
                        return "error: scene file not found: " + rel;
                    OpenSceneImmediate(resolved);
                    nlohmann::json j{{"opened", rel},
                                     {"entityCount", m_World ? m_World->GetAllEntities().size() : 0}};
                    return j.dump();
                }
                if (op == "save_scene") {
                    if (!m_PlayMode.IsStopped())
                        return "error: refusing to save during play mode (play-state would be baked into the file)";
                    if (m_CurrentScenePath.empty()) return "error: no scene open";
                    SaveScene(m_CurrentScenePath);
                    return "saved " + m_CurrentScenePath;
                }
                if (op == "press_key") {
                    if (!m_PlayMode.IsPlaying()) return std::string("error: start play mode first");
                    std::string key = args.value("key", "");
                    if (key.empty()) return std::string("error: 'key' is required");
                    i32 code = -1;
                    // Single character -> GLFW-style code; a few named keys.
                    if (key.size() == 1) {
                        char c = static_cast<char>(std::toupper(static_cast<unsigned char>(key[0])));
                        if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) code = c;
                    } else {
                        std::string k = key;
                        for (auto& ch : k) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
                        if (k == "space") code = 32;
                        else if (k == "enter") code = 257;
                        else if (k == "escape" || k == "esc") code = 256;
                        else if (k == "tab") code = 258;
                        else if (k == "shift") code = 340;
                        else if (k == "ctrl") code = 341;
                        else if (k == "up") code = 265;
                        else if (k == "down") code = 264;
                        else if (k == "left") code = 263;
                        else if (k == "right") code = 262;
                    }
                    if (code < 0) return "error: unknown key '" + key + "'";
                    McpInputAction a;
                    a.kind = McpInputAction::Kind::Key;
                    a.code = code;
                    a.remainingMs = std::clamp(args.value("hold_ms", 120.0f), 16.0f, 10000.0f);
                    m_McpInputQueue.push_back(std::move(a));
                    return std::string("pressed");
                }
                if (op == "click_at") {
                    if (!m_PlayMode.IsPlaying()) return std::string("error: start play mode first");
                    if (!m_GameViewImageDrawnThisFrame && m_GameViewImageMaxX <= m_GameViewImageMinX)
                        return std::string("error: game view not visible");
                    f32 nx = args.value("x", -1.0f), ny = args.value("y", -1.0f);
                    if (nx < 0.0f || nx > 1.0f || ny < 0.0f || ny > 1.0f)
                        return std::string("error: x/y must be normalized 0..1 (game view space)");
                    McpInputAction a;
                    a.kind = McpInputAction::Kind::Click;
                    a.code = (args.value("button", std::string("left")) == "right") ? 1 : 0;
                    a.x = m_GameViewImageMinX + nx * (m_GameViewImageMaxX - m_GameViewImageMinX);
                    a.y = m_GameViewImageMinY + ny * (m_GameViewImageMaxY - m_GameViewImageMinY);
                    a.remainingMs = 100.0f;
                    m_McpInputQueue.push_back(std::move(a));
                    return std::string("clicked");
                }
                if (op == "type_text") {
                    if (!m_PlayMode.IsPlaying()) return std::string("error: start play mode first");
                    std::string text = args.value("text", "");
                    if (text.empty()) return std::string("error: 'text' is required");
                    if (text.size() > 4096) return std::string("error: text too long (4096 max)");
                    McpInputAction a;
                    a.kind = McpInputAction::Kind::Text;
                    a.text = std::move(text);
                    m_McpInputQueue.push_back(std::move(a));
                    return std::string("typing");
                }
                if (op == "get_log") {
                    int maxLines = args.value("lines", 100);
                    maxLines = std::clamp(maxLines, 1, 2000);
                    std::string minLevel = args.value("min_level", "info");
                    i64 sinceSeq = args.value("since_seq", static_cast<i64>(-1));
                    auto levelRank = [](LogLevel l) {
                        return (l == LogLevel::Error || l == LogLevel::Fatal) ? 2
                             : (l == LogLevel::Warn) ? 1 : 0;
                    };
                    int minRank = (minLevel == "error") ? 2 : (minLevel == "warn") ? 1 : 0;
                    nlohmann::json lines = nlohmann::json::array();
                    // seq = index into the console buffer (resets on console clear)
                    i64 total = static_cast<i64>(m_ConsoleLog.size());
                    for (i64 i = std::max<i64>(sinceSeq + 1, 0); i < total; ++i) {
                        const auto& e = m_ConsoleLog[static_cast<usize>(i)];
                        if (levelRank(e.level) < minRank) continue;
                        lines.push_back({{"seq", i},
                                         {"level", levelRank(e.level) == 2 ? "error"
                                                 : levelRank(e.level) == 1 ? "warn" : "info"},
                                         {"msg", e.message}});
                    }
                    // Keep the LAST maxLines (newest matter most)
                    while (static_cast<int>(lines.size()) > maxLines)
                        lines.erase(lines.begin());
                    nlohmann::json j{{"totalSeq", total - 1}, {"lines", std::move(lines)}};
                    return j.dump();
                }
                return "error: unknown editor tool '" + op + "'";
            });
            m_McpServer.SetSpawnPrefabHook([this](const std::string& relPath,
                                                  f32 x, f32 y, f32 z) -> std::string {
                if (!m_World) return "error: no world";
                std::string projPath = m_SceneManager.GetProjectPath();
                if (projPath.empty()) return "error: no project open";
                std::string projDir = std::filesystem::path(projPath).parent_path().string();
                std::string resolved = Platform::ResolveWithinRoot(projDir, relPath);
                if (resolved.empty()) return "error: path escapes the project root";
                auto prefab = Assets::PrefabManager::Get().LoadPrefab(resolved);
                if (!prefab) return "error: could not load prefab '" + relPath + "'";
                ECS::Entity e = Assets::PrefabManager::Get().Instantiate(
                    m_World, *prefab, Math::Vector3(x, y, z));
                if (e == ECS::INVALID_ENTITY) return "error: instantiate failed";
                return "entity " + std::to_string(static_cast<u64>(e));
            });
            m_McpServer.SetBuildHook([this](const std::string& target, bool run) -> std::string {
                if (m_BuildInProgress) return "error: a build is already in progress";
                if (m_SceneManager.GetProjectPath().empty()) return "error: no project open";
                if (target == "web") m_BuildConfig.target = Build::BuildTargetPlatform::Web;
                else if (target == "desktop") m_BuildConfig.target = Build::BuildTargetPlatform::Desktop;
                else if (!target.empty()) return "error: unknown target '" + target + "' (desktop|web)";
                if (m_BuildConfig.outputDir.empty()) {
                    // Default beside the project, mirroring the Build dialog's suggestion
                    m_BuildConfig.outputDir =
                        (std::filesystem::path(m_SceneManager.GetProjectPath()).parent_path() / "Build").string();
                }
                StartBuildAsync(run);
                return std::string("build started (") +
                       (m_BuildConfig.target == Build::BuildTargetPlatform::Web ? "web" : "desktop") +
                       ") - poll scene_info for progress";
            });
            m_McpServer.SetScriptToolHook([this](const std::string& op, const std::string& relPath,
                                                 const std::string& content) -> std::string {
                std::string projPath = m_SceneManager.GetProjectPath();
                if (projPath.empty()) return "error: no project open";
                std::filesystem::path scriptsDir =
                    std::filesystem::path(projPath).parent_path() / "scripts";

                if (op == "list") {
                    nlohmann::json arr = nlohmann::json::array();
                    std::error_code ec;
                    for (const auto& e : std::filesystem::recursive_directory_iterator(scriptsDir, ec)) {
                        if (!e.is_regular_file() || e.path().extension() != ".as") continue;
                        arr.push_back(std::filesystem::relative(e.path(), scriptsDir, ec).generic_string());
                    }
                    return arr.dump();
                }
                if (op == "errors") {
                    auto* se = m_PlayMode.GetScriptEngine();
                    nlohmann::json j;
                    j["lastError"] = se ? se->GetLastError() : "";
                    j["exceptionCount"] = se ? se->GetExceptionCount() : 0u;
                    return j.dump();
                }

                // read / write: contain the path inside scripts/, .as files only
                std::string resolved = Platform::ResolveWithinRoot(scriptsDir.string(), relPath);
                if (resolved.empty()) return "error: path escapes the scripts folder";
                if (std::filesystem::path(resolved).extension() != ".as")
                    return "error: only .as script files";

                if (op == "read") {
                    std::ifstream f(resolved, std::ios::binary);
                    if (!f.is_open()) return "error: could not open '" + relPath + "'";
                    std::stringstream ss;
                    ss << f.rdbuf();
                    return ss.str();
                }
                if (op == "write") {
                    std::error_code ec;
                    std::filesystem::create_directories(std::filesystem::path(resolved).parent_path(), ec);
                    {
                        std::ofstream f(resolved, std::ios::binary | std::ios::trunc);
                        if (!f.is_open()) return "error: could not write '" + relPath + "'";
                        f.write(content.data(), static_cast<std::streamsize>(content.size()));
                    }
                    // Compile right away so the caller gets diagnostics in the
                    // same round trip (modules recompile fresh on Play anyway).
                    auto* se = m_PlayMode.GetScriptEngine();
                    if (se && se->CompileScript(resolved)) {
                        return "written and compiled cleanly (" +
                               std::to_string(content.size()) + " bytes)";
                    }
                    return std::string("error: compile failed: ") +
                           (se ? se->GetLastError() : "script engine unavailable");
                }
                return "error: unknown script op '" + op + "'";
            });
            m_McpServer.Start(static_cast<u16>(m_EditorSettings.mcpServerPort));
        } else if (!want && m_McpServer.IsRunning()) {
            m_McpServer.Stop();
        }
        if (m_McpServer.IsRunning()) m_McpServer.PumpMainThread();
    }

    // Async build: publish a finished worker build's result on the main thread
    // (notifications, run-after-build, dev web server). Runs even with the
    // build dialog closed.
    PollBuildThread();

    // Replay free camera: fly with the REAL keyboard/mouse while injection
    // replays the recorded inputs into the game (the scope makes Input answer
    // from hardware for the controller's queries only).
    if (m_ReplayFreeCam && m_PlayMode.IsReplaying() && m_CameraController) {
        Input::BeginRealInputScope();
        m_CameraController->Update(ImGui::GetIO().DeltaTime);
        Input::EndRealInputScope();
    }

    // Replay playback swapped the live scene for the replay's snapshot; when
    // the replay session ends, hand the user their real scene back.
    {
        bool replayingNow = m_PlayMode.IsReplaying();
        if (m_WasReplaying && !replayingNow && m_PlayMode.IsStopped() &&
            !m_PreReplaySceneJson.empty() && m_World) {
            Scene::SceneSerializer ser(m_World);
            auto res = ser.LoadFromString(m_PreReplaySceneJson);
            if (res.success) {
                ClearSelection();
                ENJIN_LOG_INFO(Editor, "Replay ended - working scene restored");
            } else {
                ENJIN_LOG_ERROR(Editor, "Replay ended but the working scene failed to restore: %s",
                                res.error.c_str());
            }
            m_PreReplaySceneJson.clear();
        }
        if (m_WasReplaying && !replayingNow) m_ReplayFreeCam = false;
        m_WasReplaying = replayingNow;
    }

    // --golden probe support: after the configured frame count, read back the
    // game view render target, write the reference images, and exit. When
    // --play was also passed, the count starts only once play mode is live -
    // the auto-play countdown is 120 frames, so counting from boot meant short
    // captures (CI uses 30) grabbed the EDIT-mode game view with scripts,
    // physics, and controllers never having run.
    if (!s_GoldenCapturePath.empty() && m_RenderSystem && m_GameViewRenderTarget) {
        if (!s_AutoPlayRequested || m_PlayMode.IsPlaying()) {
            if (++m_GoldenFrameCounter >= s_GoldenCaptureFrame) {
                WriteGoldenCapture();
            }
        }
    }

    // R1 GIF recording: sample the game view at the fidelity's capture rate.
    // The readback stalls the GPU briefly - acceptable while deliberately
    // recording. A game-view resize mid-recording ends the clip (GIF frames
    // must all share one size).
    if (m_GifRecorder.IsRecording() && m_GameViewRenderTarget) {
        static const f32 kGifFps[3] = { 20.0f, 15.0f, 10.0f };
        f32 interval = 1.0f / kGifFps[std::clamp(m_GifFidelity, 0, 2)];
        m_GifCaptureAccum += deltaTime;
        if (m_GifCaptureAccum >= interval) {
            std::vector<u8> pixels = m_GameViewRenderTarget->CaptureToPixels();
            u32 w = m_GameViewRenderTarget->GetWidth();
            u32 h = m_GameViewRenderTarget->GetHeight();
            if (pixels.size() == static_cast<usize>(w) * h * 4) {
                m_GifRecorder.AddFrame(pixels.data(), m_GifCaptureAccum * 1000.0f);
                m_GifCaptureAccum = 0.0f;
            } else {
                m_GifRecorder.Stop();
                ShowNotification("GIF recording stopped: game view was resized",
                                 NotificationType::Warning);
            }
        }
    }

    // Wireframe mode: only applies to the scene view (editor viewport).
    // The global pipeline is toggled for scene view render, then restored
    // for game view. Pipeline recreation is deferred to avoid mid-render crashes.
    if (m_RenderSystem && m_RenderSystem->IsWireframeEnabled() != m_PendingWireframe) {
        m_RenderSystem->SetWireframeEnabled(m_PendingWireframe);
    }
    // After scene view renders (in RenderOffscreen), wireframe is turned off
    // so the game view always renders in fill mode. See restore block at line ~1540.

    // Apply deferred shadow-state descriptor refresh here, before any frame command
    // buffer begins recording. Doing it mid-render recreates the descriptor pool and
    // invalidates bound sets (validation: "commandBuffer not in recording state").
    if (m_PendingShadowRefresh && m_RenderSystem) {
        m_RenderSystem->SetShadowsEnabled(m_PendingShadowState);
        m_RenderSystem->RefreshDescriptorsIfDirty();
        m_PendingShadowRefresh = false;
    }

    // In pure edit mode the World tick never runs, so deferred entity destructions
    // (m_World->DestroyEntity) are never flushed. OnEntityRemoved -- which rebuilds
    // the shadow caster cache, material SSBO, scene composition, etc. -- would not
    // fire until play starts, so a deleted object's SHADOW lingers even though its
    // mesh is gone (IsValid already reports false). Flush here each edit-mode frame
    // so caches rebuild before the shadow pass. Play mode flushes via World::Update.
    if (m_World && !m_PlayMode.IsPlaying() && !m_PlayMode.IsPaused()) {
        m_World->FlushPendingDestructions();
    }

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

    // Handle deferred auto-save recovery (requested from the Render-phase
    // recovery modal — same World::Clear-during-Render hazard as above).
    if (!m_PendingRecoveryLoadPath.empty()) {
        std::string recoveryPath = std::move(m_PendingRecoveryLoadPath);
        m_PendingRecoveryLoadPath.clear();
        if (m_World) {
            if (m_Renderer) m_Renderer->WaitForAllFrames();
            ClearSelection();
            Scene::SceneSerializer serializer(m_World);
            auto result = serializer.Load(recoveryPath, true);
            if (result.success) {
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

    // Handle deferred model import (requested during previous frame's Render).
    // The one-frame delay ensures the "Importing..." overlay is visible on screen
    // before the blocking import call runs.
    if (m_ImportPending) {
        m_ImportPending = false;
        ExecuteImport(m_ImportPendingPath, m_ImportPendingOptions);
        m_ImportPendingPath.clear();
    }

    // Group import "apply to all": import one queued model per frame (so the UI can
    // breathe between blocking imports), spaced into a row; frame them all at the end.
    if (m_ImportBatchActive) {
        if (!m_ImportBatchQueue.empty()) {
            int doneIdx = m_ImportBatchTotal - static_cast<int>(m_ImportBatchQueue.size());
            const f32 spacing = 3.0f;
            f32 x = (static_cast<f32>(doneIdx) - static_cast<f32>(m_ImportBatchTotal - 1) * 0.5f) * spacing;
            std::string p = m_ImportBatchQueue.front();
            m_ImportBatchQueue.erase(m_ImportBatchQueue.begin());
            ExecuteImport(p, m_ImportBatchOptions, Math::Vector3(x, 0.0f, 0.0f), /*showResultDialog=*/false);
            if (m_LastImportResult.rootEntity != ECS::INVALID_ENTITY)
                m_ImportBatchRoots.push_back(m_LastImportResult.rootEntity);
        } else {
            m_ImportBatchActive = false;
            FinishGroupImport();
        }
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
            if (m_Announcer.enabled) m_Announcer.Announce("Play mode stopped", Accessibility::AnnouncePriority::Normal);
            ClearSelection(); // Entities have new IDs after scene restore
            m_PrePlayRenderSettings.ApplyToRuntime(
                m_RenderSystem, m_PostProcessing ? &m_PostProcessing->GetSettings() : nullptr);
            // Restore pre-play fullscreen state (play mode options may have changed it)
            if (m_PrePlayFullscreenSaved && m_Window) {
                m_Window->SetFullscreen(m_PrePlayFullscreen);
                m_PrePlayFullscreenSaved = false;
            }
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

    // Watch the open scene file for out-of-band edits (git pull, another tool).
    CheckExternalSceneChange(deltaTime);

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

    // Bucket frame time into histogram
    ++m_FrameNumber;
    ++m_FrameHistogramTotal;
    if      (frameTimeMs < 1.0f)  ++m_FrameHistogram[0];
    else if (frameTimeMs < 2.0f)  ++m_FrameHistogram[1];
    else if (frameTimeMs < 4.0f)  ++m_FrameHistogram[2];
    else if (frameTimeMs < 8.0f)  ++m_FrameHistogram[3];
    else if (frameTimeMs < 16.0f) ++m_FrameHistogram[4];
    else if (frameTimeMs < 33.0f) ++m_FrameHistogram[5];
    else if (frameTimeMs < 66.0f) ++m_FrameHistogram[6];
    else                          ++m_FrameHistogram[7];

    // Log spike if frame exceeded 8ms
    if (frameTimeMs > 8.0f) {
        SpikeEntry& entry = m_SpikeLog[m_SpikeLogIndex];
        entry.frameTimeMs = frameTimeMs;
        entry.renderMs = m_CPURenderMs;
        entry.fenceMs = m_Renderer ? m_Renderer->GetFenceWaitMs() : 0.0f;
        entry.frameNumber = m_FrameNumber;
        m_SpikeLogIndex = (m_SpikeLogIndex + 1) % 10;
        if (m_SpikeLogCount < 10) ++m_SpikeLogCount;
    }

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
        // Draw call/triangle stats are read at the end of Render() (after
        // RenderSystem::Update increments them). Reading here in Update() would
        // get zeros because ResetFrameCounters runs at the start of each render.
        // The m_PerfMetrics.drawCallCount/triangleCount are updated in Render().
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
    // Wind clock drives SHADER animation (waterfall UV scroll, flipbooks,
    // vegetation sway, cloud drift, water effects) via windData.w - scale it
    // during play so bullet time slows them too (player/web scale their whole
    // dt upstream; the editor's raw dt must scale here).
    m_WindSystem.Update(deltaTime * (m_PlayMode.IsPlaying() ? Scripting::GetTimeScale() : 1.0f));
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

        // Update bone attachments: move attached entities to follow their target bone.
        // Runs after animation update so bones are in their current-frame positions.
        for (auto entity : m_World->GetEntitiesWithComponent<ECS::BoneAttachmentComponent>()) {
            auto* ba = m_World->GetComponent<ECS::BoneAttachmentComponent>(entity);
            if (!ba || ba->targetEntity == ECS::INVALID_ENTITY || ba->targetBoneName.empty()) continue;

            auto* targetAnim = m_World->GetComponent<ECS::AnimatorComponent>(ba->targetEntity);
            if (!targetAnim) continue;

            const auto* skeleton = targetAnim->animator.GetSkeleton();
            if (!skeleton) continue;

            i32 boneIdx = skeleton->FindBoneIndex(ba->targetBoneName);
            if (boneIdx < 0) continue;

            // Get bone world transform (bone-local → entity-world)
            Math::Matrix4 entityWorld = ECS::ComputeWorldMatrix(m_World, ba->targetEntity);
            Math::Matrix4 boneWorld = entityWorld * targetAnim->animator.GetCurrentPose().worldTransforms[boneIdx];

            // Extract position and apply offset
            Math::Vector3 bonePos(boneWorld.m[12], boneWorld.m[13], boneWorld.m[14]);
            Math::Vector3 finalPos = bonePos + ba->positionOffset;

            // Update this entity's transform
            auto* transform = m_World->GetComponent<ECS::TransformComponent>(entity);
            if (transform) {
                transform->position = finalPos;
                // Apply rotation offset (bone rotation * offset)
                // Extract bone rotation from the 3x3 portion of boneWorld
                // For now, just apply the offset rotation directly
                transform->rotation = ba->rotationOffset;
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
        // Viewport gating: look-capture and scroll only start over the Scene
        // viewport image (RMB-drag over the Inspector used to rotate the
        // camera, and scrolling any panel changed fly speed); WASD also works
        // while the viewport has focus. One-frame-stale hover state is fine.
        m_CameraController->SetViewportInputState(m_EditorViewportHovered, m_EditorViewportFocused);

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
                if (m_Announcer.enabled) m_Announcer.Announce("Redo", Accessibility::AnnouncePriority::Low);
            } else if (Input::IsKeyPressed(KeyCode::Z)) {
                m_UndoRedo.Undo();
                if (m_Announcer.enabled) m_Announcer.Announce("Undo", Accessibility::AnnouncePriority::Low);
            } else if (Input::IsKeyPressed(KeyCode::Y)) {
                m_UndoRedo.Redo();
                if (m_Announcer.enabled) m_Announcer.Announce("Redo", Accessibility::AnnouncePriority::Low);
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

        // Bone selection keyboard navigation (Up/Down cycle, Escape deselect)
        if (m_PrimarySelected != ECS::INVALID_ENTITY &&
            m_World->HasComponent<ECS::AnimatorComponent>(m_PrimarySelected)) {
            auto* animComp = m_World->GetComponent<ECS::AnimatorComponent>(m_PrimarySelected);
            if (animComp && animComp->showBones && animComp->selectedBoneIndex >= 0) {
                const auto* skeleton = animComp->animator.GetSkeleton();
                if (skeleton && !skeleton->bones.empty()) {
                    i32 boneCount = static_cast<i32>(skeleton->bones.size());
                    if (Input::IsKeyPressed(KeyCode::Up)) {
                        animComp->selectedBoneIndex = (animComp->selectedBoneIndex - 1 + boneCount) % boneCount;
                    }
                    if (Input::IsKeyPressed(KeyCode::Down)) {
                        animComp->selectedBoneIndex = (animComp->selectedBoneIndex + 1) % boneCount;
                    }
                }
                if (Input::IsKeyPressed(KeyCode::Escape)) {
                    animComp->selectedBoneIndex = -1;
                }
            }
        }

        // Arrow-key selection navigation (Up/Down = previous/next entity in
        // hierarchy order). Active in the default editor, where full keyboard nav
        // (which repurposes the arrows to nudge the gizmo) is off. Skipped while
        // bone-cycling is consuming the arrows. Selection change flows straight to
        // the viewport highlight, which reads the live selection every frame.
        bool boneNavActive = false;
        if (m_PrimarySelected != ECS::INVALID_ENTITY &&
            m_World->HasComponent<ECS::AnimatorComponent>(m_PrimarySelected)) {
            auto* ac = m_World->GetComponent<ECS::AnimatorComponent>(m_PrimarySelected);
            boneNavActive = ac && ac->showBones && ac->selectedBoneIndex >= 0;
        }
        if (!m_EditorSettings.keyboardNavEnabled && !boneNavActive) {
            bool navUp = Input::IsKeyPressed(KeyCode::Up);
            bool navDown = Input::IsKeyPressed(KeyCode::Down);
            if (navUp || navDown) {
                // Flatten the scene into hierarchy order: parentless roots (by
                // entity index) each followed by their subtree in child order.
                std::vector<ECS::Entity> roots;
                for (ECS::Entity e : m_World->GetEntitiesWithComponent<ECS::TransformComponent>()) {
                    if (ECS::GetParent(m_World, e) == ECS::INVALID_ENTITY) roots.push_back(e);
                }
                std::sort(roots.begin(), roots.end(), [](ECS::Entity a, ECS::Entity b) {
                    return ECS::EntityIndex(a) < ECS::EntityIndex(b);
                });
                std::vector<ECS::Entity> order;
                std::vector<ECS::Entity> stack(roots.rbegin(), roots.rend());
                while (!stack.empty()) {
                    ECS::Entity e = stack.back(); stack.pop_back();
                    order.push_back(e);
                    const auto& kids = ECS::GetChildren(m_World, e);
                    for (auto it = kids.rbegin(); it != kids.rend(); ++it) stack.push_back(*it);
                }
                if (!order.empty()) {
                    i32 cur = -1;
                    for (i32 i = 0; i < static_cast<i32>(order.size()); ++i) {
                        if (order[i] == m_PrimarySelected) { cur = i; break; }
                    }
                    i32 n = static_cast<i32>(order.size());
                    i32 next = (cur < 0) ? (navUp ? n - 1 : 0)
                                         : (navUp ? (cur - 1 + n) % n : (cur + 1) % n);
                    SelectEntity(order[next]); // single-select → highlight follows
                }
            }
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

        // Discord bug report (Ctrl+Shift+B)
        if (Input::IsKeyDown(KeyCode::LeftControl) && Input::IsKeyDown(KeyCode::LeftShift) &&
            Input::IsKeyPressed(KeyCode::B)) {
            m_ShowDiscordBugDialog = true;
            m_DiscordSendState = DiscordSendState::Idle;
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

    // Register scan targets for switch access / eye tracking
    if (m_AlternativeInput.IsAnyAlternativeInputActive()) {
        m_AlternativeInput.ClearScanTargets();

        auto extent = m_Renderer->GetSwapchainExtent();
        f32 sw = static_cast<f32>(extent.width);
        f32 sh = static_cast<f32>(extent.height);

        // Core editor actions as scan targets
        // Play/Stop
        m_AlternativeInput.RegisterScanTarget({"Play / Stop", "Toolbar", sw * 0.45f, 30, 80, 30, [this]() {
            if (m_PlayMode.IsStopped()) StartPlayMode();
            else m_PendingPlayStop = true;
        }});

        // Save
        m_AlternativeInput.RegisterScanTarget({"Save Scene", "Toolbar", sw * 0.55f, 30, 80, 30, [this]() {
            if (!m_CurrentScenePath.empty()) SaveScene(m_CurrentScenePath);
        }});

        // Undo
        m_AlternativeInput.RegisterScanTarget({"Undo", "Toolbar", sw * 0.65f, 30, 60, 30, [this]() {
            m_UndoRedo.Undo();
            if (m_Announcer.enabled) m_Announcer.Announce("Undo", Accessibility::AnnouncePriority::Low);
        }});

        // Redo
        m_AlternativeInput.RegisterScanTarget({"Redo", "Toolbar", sw * 0.72f, 30, 60, 30, [this]() {
            m_UndoRedo.Redo();
            if (m_Announcer.enabled) m_Announcer.Announce("Redo", Accessibility::AnnouncePriority::Low);
        }});

        // Delete selected
        m_AlternativeInput.RegisterScanTarget({"Delete", "Toolbar", sw * 0.79f, 30, 60, 30, [this]() {
            if (!m_SelectedEntities.empty()) DeleteSelectedEntities();
        }});

        // Entity selection (first 10 named entities as scan targets)
        if (m_World) {
            u32 entityIdx = 0;
            for (auto e : m_World->GetEntitiesWithComponent<ECS::NameComponent>()) {
                if (entityIdx >= 10) break;
                auto* name = m_World->GetComponent<ECS::NameComponent>(e);
                if (!name) continue;
                ECS::Entity capturedEntity = e;
                m_AlternativeInput.RegisterScanTarget({
                    name->name, "Hierarchy",
                    10.0f, 70.0f + entityIdx * 24.0f, 200.0f, 22.0f,
                    [this, capturedEntity]() {
                        SelectEntity(capturedEntity);
                    }
                });
                entityIdx++;
            }
        }

        // Gizmo modes
        m_AlternativeInput.RegisterScanTarget({"Translate", "Gizmo", 10, sh - 90, 70, 25, [this]() {
            m_GizmoOperation = GizmoOperation::Translate;
            if (m_Announcer.enabled) m_Announcer.Announce("Gizmo: Translate", Accessibility::AnnouncePriority::Low);
        }});
        m_AlternativeInput.RegisterScanTarget({"Rotate", "Gizmo", 85, sh - 90, 60, 25, [this]() {
            m_GizmoOperation = GizmoOperation::Rotate;
            if (m_Announcer.enabled) m_Announcer.Announce("Gizmo: Rotate", Accessibility::AnnouncePriority::Low);
        }});
        m_AlternativeInput.RegisterScanTarget({"Scale", "Gizmo", 150, sh - 90, 55, 25, [this]() {
            m_GizmoOperation = GizmoOperation::Scale;
            if (m_Announcer.enabled) m_Announcer.Announce("Gizmo: Scale", Accessibility::AnnouncePriority::Low);
        }});
    }

    // Update alternative input devices
    m_AlternativeInput.Update(deltaTime);

    // Update gamepad editor navigation + inspector mode
    UpdateGamepadEditor(deltaTime);
    UpdateGamepadInspector(deltaTime);

    // Focus mode toggle (F11) and exit (Escape)
    // F11 toggles between editor view and fullscreen game view while playing
    // F1 = Game Debug group, F2 = Engine Debug group
    if (Input::IsKeyPressed(KeyCode::F1)) {
        ToggleGameDebug();
    }
    if (Input::IsKeyPressed(KeyCode::F2)) {
        ToggleEngineDebug();
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
            // Capture the mouse ONLY when the scene actually mouse-looks
            // (FPS/TPS). Cursor-driven games (point-and-click, UI-heavy) keep
            // the cursor visible in focus mode - a blanket capture left them
            // cursorless (Marty 2026-08-30).
            if (SceneHasMouseLookController()) Input::SetMouseCaptured(true);
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

    // A game script released the cursor mid-play (Web Demo's Tab menu mode):
    // adopt that state and stop click-to-recapturing — the game owns the
    // toggle now, and clicking its on-screen UI must not fight the script.
    // Cleared when the script re-captures (Tab back to play mode).
    if (m_PlayMode.IsPlaying()) {
        if (m_GameViewMouseCaptured && !Input::IsMouseCaptured()) {
            m_GameViewMouseCaptured = false;
            m_GameScriptReleasedCursor = true;
        } else if (m_GameScriptReleasedCursor && Input::IsMouseCaptured()) {
            m_GameViewMouseCaptured = true;
            m_GameScriptReleasedCursor = false;
        }
    } else {
        m_GameScriptReleasedCursor = false;
    }

    // Game View click-to-capture: when ACTIVELY playing, clicking the Game View
    // image captures the mouse so FPS/TPS controllers receive mouse delta for
    // look. Not while paused — a paused game must never own the cursor (the
    // editor is in charge; see the paused safety net below).
    if (!m_FocusMode && !m_GameViewMouseCaptured && !m_GameScriptReleasedCursor &&
        m_PlayMode.IsPlaying() &&
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

    // Creative-mode build placement (drag a lake/trees/grass/shrubs onto the ground).
    // Intercepts the mouse before picking so a click places instead of selects.
    if (m_CreativeTool != CreativeTool::None && m_PlayMode.IsStopped()) {
        HandleCreativePlacement(deltaTime);
    }

    // Deactivate UI edit mode when entering play mode
    if (!m_PlayMode.IsStopped() && m_UIEditMode) {
        m_UIEditMode = false;
    }

    // Handle viewport picking (left-click to select entities in editor viewport)
    // Skip viewport picking while terrain/tilemap/UI/creative edit mode is active to prevent entity deselection
    if (!ImGuizmo::IsOver() && !m_TerrainEditMode && !m_TilemapEditMode && !m_UIEditMode &&
        m_CreativeTool == CreativeTool::None) {
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

    // Game-view sims: update path, AFTER gameplay ticked (fresh camera and
    // player positions). Time-scaled so bullet time slows them (the player
    // runtime scales its whole dt upstream; the editor scales per-clock).
    UpdateGameViewSims(deltaTime * (m_PlayMode.IsPlaying() ? Scripting::GetTimeScale() : 1.0f));

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

    // Safety net: release mouse capture if play mode stopped
    if (m_GameViewMouseCaptured && m_PlayMode.IsStopped()) {
        m_GameViewMouseCaptured = false;
        Input::SetMouseCaptured(false);
    }

    // Safety net: release mouse capture while PAUSED. Escape-pause released it,
    // but the toolbar pause button didn't — the game kept the cursor locked, so
    // the scene view got no hover/look input and the editor felt frozen
    // (Marty, 2026-08-08). The scene view never abides by game-screen capture
    // rules; resuming re-captures via click-to-capture or the Escape-menu path.
    if (m_GameViewMouseCaptured && m_PlayMode.IsPaused()) {
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

    // CPU frame profiler: total Update time (derived from delta)
    m_CPUUpdateMs = m_CPUUpdateMs * 0.9f + (deltaTime * 1000.0f) * 0.1f;
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
        auto* renderer = m_RenderSystem->GetVulkanRenderer();
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

    // Rebuild pipelines whenever the effect render pass CHANGES — not one-shot.
    // RT resizes (scene/template load changes the game-view size) destroy and
    // recreate the render pass; pipelines built against the dead pass render
    // undefined — every mesh came out pitch black until something else happened
    // to trigger a pipeline recreation (wireframe toggle, play/stop side effects).
    // Detecting the pass change and requesting the same full deferred recreation
    // the wireframe toggle uses makes the first frame after any load correct.
    if (m_RenderSystem) {
        VkRenderPass effectRenderPass = (m_SceneRenderTarget && m_SceneRenderTarget->IsValid())
            ? m_SceneRenderTarget->GetRenderPass()
            : m_GameViewRenderTarget->GetRenderPass();
        if (effectRenderPass != m_LastEffectRenderPass) {
            m_RenderSystem->RecreateEffectPipelinesForRenderPass(effectRenderPass);
            if (m_LastEffectRenderPass != VK_NULL_HANDLE) {
                // Pass was REPLACED (resize/reload) — old pipelines reference the
                // destroyed pass; request the full deferred recreation heal.
                m_RenderSystem->RequestPipelineRecreation();
            }
            m_LastEffectRenderPass = effectRenderPass;
        }
    }
}


// Game-view simulations (weather zones, water freeze, world time, seasons,
// particles, parallax, elemental, fluid, terrain coupling, curl noise) -
// moved OUT of RenderOffscreen (the sim-in-render smell, audit 2026-08-31).
// They tick once per editor frame with the time-scaled dt, so the Game View
// FPS throttle only affects RENDERING and no future sim can couple to render
// cadence again (the old m_GameViewSimAccum workaround is gone). Runs when
// the game is playing OR the game view is live (edit-mode preview keeps
// weather visible); a hidden panel during play no longer freezes the world.
void EditorLayer::UpdateGameViewSims(f32 simDt) {
    if (!m_World || !m_RenderSystem) return;
    if (!m_RenderSystem->IsGameViewReady()) return;
    if (!m_PlayMode.IsPlaying() && !m_GameViewVisiblePrev && s_GoldenCapturePath.empty()) return;

    // Build water surface meshes here (a safe pre-render point). The player does this in
    // RenderSystem::Update(), which the editor never calls, so without this the 3D water
    // surface has no mesh and is invisible in the game view.
    m_RenderSystem->EnsureWaterMeshes();
    m_RenderSystem->EnsureWater3DMeshes();

    // Resolve the game camera by the same rules as RenderOffscreen
    // (user selection -> active camera), then the zone-detection block
    // below computes m_CameraZoneOverride, which the render pass reuses.
    ECS::Entity gameCameraEntity = m_SelectedGameCamera;
    if (gameCameraEntity != ECS::INVALID_ENTITY &&
        !m_World->HasComponent<ECS::CameraComponent>(gameCameraEntity)) {
        gameCameraEntity = ECS::INVALID_ENTITY;
    }
    if (gameCameraEntity == ECS::INVALID_ENTITY)
        gameCameraEntity = ECS::CameraManager::GetActiveCamera(m_World);
    if (gameCameraEntity == ECS::INVALID_ENTITY) return;

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
    auto* cameraTransform = m_World->GetComponent<ECS::TransformComponent>(gameCameraEntity);
    if (!cameraTransform) return;

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
    m_GameViewWeatherParticles = false;
    m_GameViewIsRain = false;

    // 2D scenes get precipitation as an XY sheet falling down the screen
    if (m_RenderSystem) {
        m_WeatherSystem.SetMode2D(
            m_RenderSystem->GetSceneComposition().mode != ECS::SceneRenderMode::Scene3D);
    }

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
                m_GameViewIsRain = false;
            } else if (temp <= 5.0f) {
                // Near-freezing: sleet mix (both rain and snow at reduced intensity)
                f32 blend = temp / 5.0f;  // 0 at 0C, 1 at 5C
                f32 baseIntensity = (activeWeatherZone->weatherType == 4)
                    ? activeWeatherZone->snowIntensity
                    : activeWeatherZone->rainIntensity;
                m_WeatherSystem.SetWeather(wType, 0.1f);
                m_WeatherSystem.SetRainIntensity(baseIntensity * blend);
                m_WeatherSystem.SetSnowIntensity(baseIntensity * (1.0f - blend));
                m_GameViewIsRain = (blend > 0.5f);
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
                m_GameViewIsRain = true;
            }
        } else {
            // No temperature zone override - use weather zone as-is
            m_WeatherSystem.SetWeather(wType, 0.1f);

            if (activeWeatherZone->weatherType == 2 || activeWeatherZone->weatherType == 3 ||
                activeWeatherZone->weatherType == 6) {
                m_WeatherSystem.SetRainIntensity(activeWeatherZone->rainIntensity);
                m_WeatherSystem.SetSnowIntensity(0.0f);
                m_GameViewIsRain = true;
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

        // Custom rain/snow sprites: resolve zone texture paths to bindless indices
        // once (cached; -1 = none/failed → built-in procedural look)
        if (m_RenderSystem) {
            if (activeWeatherZone->cachedRainTexIndex == -2)
                activeWeatherZone->cachedRainTexIndex =
                    m_RenderSystem->ResolveBindlessTextureIndex(activeWeatherZone->rainTexturePath);
            if (activeWeatherZone->cachedSnowTexIndex == -2)
                activeWeatherZone->cachedSnowTexIndex =
                    m_RenderSystem->ResolveBindlessTextureIndex(activeWeatherZone->snowTexturePath);
        }
        m_WeatherSystem.SetRainTextureIndex(activeWeatherZone->cachedRainTexIndex);
        m_WeatherSystem.SetSnowTextureIndex(activeWeatherZone->cachedSnowTexIndex);

        if (activeWeatherZone->lightningEnabled) {
            m_WeatherSystem.SetLightningInterval(
                activeWeatherZone->lightningMinInterval,
                activeWeatherZone->lightningMaxInterval);
        }

        // (weather sim update moved BELOW the zone if/else - it must run in
        // the no-zone case too, and with real elapsed time.)

        m_GameViewWeatherParticles = (activeWeatherZone->weatherType == 2 ||
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
        m_WeatherSystem.SetRainTextureIndex(-1);
        m_WeatherSystem.SetSnowTextureIndex(-1);
        m_WindSystem.ClearZoneOverride();
        m_RenderSystem->SetFogParams(0.0f, 20.0f, 100.0f, 0.1f);
        m_RenderSystem->SetFogColor(Math::Vector3(0.5f, 0.5f, 0.6f));
        m_RenderSystem->SetSnowIntensity(0.0f);
    }

    // Weather SIM step - two fixes in one (Marty 2026-08-30):
    // 1. REAL elapsed time, not the editor's per-frame dt. This code runs in
    //    the game-view render path, which the Game View FPS dropdown
    //    throttles - at 30fps it ran 30x/sec with a full-rate dt, so rain
    //    fell at a fraction of real speed ("why is speed of things changing
    //    when I slow down fps"). The accumulator (fed every editor frame in
    //    Update) makes sim time frame-rate independent.
    // 2. Runs for the NO-ZONE case too - scripted weather
    //    (Weather_SetRainIntensity with no WeatherZone entity) previously
    //    never advanced the particle sim in the editor.
    m_WeatherSystem.Update(simDt, cameraTransform->position);
    // Weather-driven sky: rain greys the gradient, snow pales it (live).
    m_RenderSystem->SetWeatherSkyBlend(m_WeatherSystem.GetRainIntensity(),
                                       m_WeatherSystem.GetSnowIntensity());

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

        // Snow weather freezes water even without a temperature zone (Marty:
        // water should freeze in snow). A temperature zone still overrides.
        bool snowFreeze = m_WeatherSystem.GetSnowIntensity() > 0.25f;
        if ((waterTempZone && waterTempZone->IsFreezing()) || snowFreeze) {
            // Freezing: increase freeze progress
            waterVol->freezeProgress += waterVol->freezeRate * simDt;
            if (waterVol->freezeProgress > 1.0f) waterVol->freezeProgress = 1.0f;
        } else if (waterTempZone && waterTempZone->IsNearFreezing()) {
            // Near-freezing (0-5C): lerp toward partial freeze (0.3)
            f32 target = 0.3f;
            if (waterVol->freezeProgress < target) {
                waterVol->freezeProgress += waterVol->freezeRate * 0.5f * simDt;
                if (waterVol->freezeProgress > target) waterVol->freezeProgress = target;
            } else {
                waterVol->freezeProgress -= waterVol->thawRate * 0.5f * simDt;
                if (waterVol->freezeProgress < target) waterVol->freezeProgress = target;
            }
        } else {
            // Warm or no zone: thaw
            waterVol->freezeProgress -= waterVol->thawRate * simDt;
            if (waterVol->freezeProgress < 0.0f) waterVol->freezeProgress = 0.0f;
        }
        waterVol->isFrozen = (waterVol->freezeProgress >= 0.99f);
    }

    // World Time System: advance clock and update sun/ambient
    if (m_WorldTimeEnabled) {
        m_WorldTime.Update(simDt);

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

    // Seasonal Weather System: temperature and weather transitions. The
    // checkbox is the authority in the editor; it drives the system's own
    // enabled flag (which defaults OFF so ungated hosts can't stomp script
    // weather — the desktop player did, every frame).
    m_SeasonalWeather.GetConfig().enabled =
        (m_WorldTimeEnabled && m_SeasonalWeatherEnabled && !activeWeatherZone);
    if (m_SeasonalWeather.GetConfig().enabled) {
        m_SeasonalWeather.Update(simDt, m_WorldTime.GetState(), m_WeatherSystem);
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
    m_RenderSystem->SetRainActive(m_GameViewIsRain);

    // Update particle emitter simulation (push scene wind so wind-driven emitters drift)
    if (auto* ws = m_RenderSystem ? m_RenderSystem->GetWindSystem() : nullptr) {
        Math::Vector4 w = ws->GetWindVector();
        m_ParticleSystem.SetSceneWind(Math::Vector3(w.x, w.y, w.z));
    }
    m_ParticleSystem.Update(simDt, m_World);

    // Update parallax scrolling backgrounds (auto-scroll advance)
    m_ParallaxSystem.Update(simDt);

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
        m_ElementalSystem.Update(m_World, simDt, cameraTransform->position);

        // Feed fire emitters into the renderer as transient point lights. One
        // source lights both surfaces (PBR point lights) and participating media
        // (clustered lighting -> volumetric fog froxels), so fire glows on walls
        // and through nearby smoke/fog in lockstep.
        if (m_RenderSystem) {
            m_EffectsTime += simDt;
            m_ElementalSystem.BuildFireLights(m_EffectsTime, m_FireLights);
            m_RenderSystem->ClearTransientPointLights();
            for (const auto& fl : m_FireLights) {
                m_RenderSystem->AddTransientPointLight(fl.position, fl.range, fl.color, fl.intensity);
            }
        }
    }

    // Update fluid simulation
    m_FluidSimulation.Update(simDt, m_World);

    // Update fluid-terrain coupling (erosion/deposition)
    m_FluidTerrainCoupling.Update(simDt, m_World, m_FluidSimulation);

    // Update curl noise flow fields
    if (m_CurlNoiseSystem) m_CurlNoiseSystem->Update(simDt);
}

void EditorLayer::RenderOffscreen(VkCommandBuffer commandBuffer) {
    ENJIN_PROFILE_SCOPE("Render");

    // Advance the hidden-viewport-skip latches. This runs before the panels draw, so we
    // gate this frame's renders on the PREVIOUS frame's panel visibility; the panels set
    // *ThisFrame again below (in DrawViewportPanel / DrawGameViewPanel).
    m_SceneViewVisiblePrev = m_SceneViewVisibleThisFrame;
    m_GameViewVisiblePrev = m_GameViewVisibleThisFrame;
    m_SceneViewVisibleThisFrame = false;
    m_GameViewVisibleThisFrame = false;

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

    // GPU compute skinning (ADR-0002): skin all skinned meshes once here, OUTSIDE any render pass,
    // before the shadow/viewport/game-view passes so they all read the deformed result. No-op unless
    // compute skinning is enabled (default off → existing vertex-shader skinning path is untouched).
    if (m_RenderSystem) m_RenderSystem->RunComputeSkinningPass(commandBuffer);

    // Fog/DDGI/clustered/GPU-particle compute pre-pass. Update() never
    // reaches it when the skip flag is set (every editor frame with a game
    // view), so without this call the fog froxel volume never receives its
    // one-shot neutral clear: a project opened directly (launch arg, open-
    // last, or before any hub frame ran) rendered BLACK until a play/stop
    // cycle happened to run Update without the skip flag. Idempotent per
    // frame; its internal skinning call no-ops after the line above.
    if (m_RenderSystem) {
        m_RenderSystem->BeginFrame(m_LastDeltaTime);
        m_RenderSystem->RecordComputePrePass(m_LastDeltaTime);
    }

    // --- Editor viewport: render scene from editor camera to offscreen RT ---
    // (Resize is handled by PrepareRenderTargets() before command buffer recording)
    // Skipped when the Scene panel isn't visible (hidden-viewport skip).
    if (m_SceneViewVisiblePrev &&
        m_EditorViewportRT && m_EditorViewportRT->IsValid() &&
        m_EditorViewportWidth > 0 && m_EditorViewportHeight > 0) {

        // Apply scene view mode — save previous state so game view is unaffected.
        bool prevWireframe = false, prevUnlit = false;
        if (m_RenderSystem) {
            prevWireframe = m_RenderSystem->GetEditorWireframe();
            prevUnlit = m_RenderSystem->GetEditorUnlit();

            bool wantShadows = (m_SceneViewMode == SceneViewMode::LitShadows || m_SceneViewMode == SceneViewMode::Full);
            // Wireframe toggle is deferred — SetWireframeEnabled triggers pipeline
            // recreation which is unsafe mid-render. Set a flag and apply in Update.
            m_PendingWireframe = (m_SceneViewMode == SceneViewMode::Wireframe);
            m_RenderSystem->SetEditorUnlit(m_SceneViewMode == SceneViewMode::Solid);

            // Only flip the renderer's shadow state when the editor view mode actually
            // changes — the previous code restored prevShadows at the end of every
            // frame, which marked the descriptor pool dirty every frame, which
            // recreated the entire VkDescriptorPool every frame and orphaned the
            // offscreen render-target descriptor sets (use-after-free crash).
            if (!m_EditorShadowsTracked || m_EditorShadowsApplied != wantShadows) {
                // Defer the actual SetShadowsEnabled + descriptor refresh to Update().
                // RefreshDescriptorsIfDirty recreates the whole VkDescriptorPool, which
                // invalidates any descriptor set bound to a command buffer already
                // recording this frame (the cascade of "commandBuffer not in recording
                // state" validation errors). Update() runs before any command buffer
                // records, same as the wireframe toggle above.
                m_PendingShadowState = wantShadows;
                m_PendingShadowRefresh = true;
                m_EditorShadowsApplied = wantShadows;
                m_EditorShadowsTracked = true;
            }
        }

        // Shadow pass for editor camera (only in shadow modes)
        if (m_Camera && m_RenderSystem &&
            (m_SceneViewMode == SceneViewMode::LitShadows || m_SceneViewMode == SceneViewMode::Full)) {
            m_RenderSystem->RenderShadowPassForCamera(m_Camera);
        }

        // Lock the fly camera's projection aspect to the viewport RT right before we
        // render into it. Setting it in the UI pass (DrawViewportPanel) is a frame late
        // — the offscreen render below runs earlier in the frame — which left the scene
        // rendered at the old aspect and then squeezed into the letterboxed panel (a
        // tall panel stretched everything vertically). Matching it here, at render time,
        // is what the Game View already does.
        if (m_CameraController && m_EditorViewportRT->GetHeight() > 0) {
            m_CameraController->SetViewportAspect(
                static_cast<f32>(m_EditorViewportRT->GetWidth()) /
                static_cast<f32>(m_EditorViewportRT->GetHeight()));
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

        // Restore render state so game view renders with full quality.
        // Wireframe is handled by the offscreen pipeline (scene view only) —
        // the main pipeline always uses fill mode, so no wireframe restore needed.
        // Shadows are NOT restored here — see the per-mode toggle above for why.
        if (m_RenderSystem) {
            m_RenderSystem->SetEditorWireframe(prevWireframe);
            m_RenderSystem->SetEditorUnlit(prevUnlit);
        }
    }

    // Skip main pass rendering (editor viewport is now offscreen)
    if (m_RenderSystem) {
        m_RenderSystem->SetSkipMainPassRendering(true);
    }

    if (!m_GameViewRenderTarget || !m_GameViewRenderTarget->IsValid()) {
        return;
    }

    // Hidden-viewport skip: don't render the Game View when its panel isn't visible.
    // Keep rendering during a golden capture (headless RT verification reads the output).
    if (!m_GameViewVisiblePrev && s_GoldenCapturePath.empty()) {
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

    // Zone-driven camera override was computed by UpdateGameViewSims this
    // frame (sims live in the update path now); apply it for rendering.
    if (m_CameraZoneOverride != ECS::INVALID_ENTITY &&
        m_World->HasComponent<ECS::CameraComponent>(m_CameraZoneOverride)) {
        gameCameraEntity = m_CameraZoneOverride;
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

    // Set camera position and orientation from entity transform. During replay
    // free-cam the fly camera takes the game view instead: the replay drives
    // the world (including the recorded camera entity), you choose the angle.
    if (m_ReplayFreeCam && m_PlayMode.IsReplaying() && m_Camera) {
        gameCamera.SetPosition(m_Camera->GetPosition());
        Math::Vector3 fcFwd = m_Camera->GetForward();
        gameCamera.SetLookAt(m_Camera->GetPosition(), m_Camera->GetPosition() + fcFwd,
                             Math::Vector3(0.0f, 1.0f, 0.0f));
    } else {
        gameCamera.SetPosition(cameraTransform->position);

        // Compute forward/up from the entity's rotation quaternion
        Math::Vector3 forward = cameraTransform->rotation.Rotate(Math::Vector3(0.0f, 0.0f, -1.0f));
        Math::Vector3 up = cameraTransform->rotation.Rotate(Math::Vector3(0.0f, 1.0f, 0.0f));
        Math::Vector3 target = cameraTransform->position + forward;
        gameCamera.SetLookAt(cameraTransform->position, target, up);
    }

    // Hand RT the game camera: ray tracing / path tracing renders the game view,
    // not the editor fly cam (RT dispatch happens in RenderSystem::Update).
    if (m_RenderSystem) {
        m_RTGameCamera = gameCamera;
        m_RenderSystem->SetRTCameraOverride(&m_RTGameCamera);
    }

    // (weather zones, world time, particles, elemental, fluid, parallax and
    // curl-noise sims live in UpdateGameViewSims - update path, not render.)

    u32 rtWidth = m_GameViewRenderTarget->GetWidth();
    u32 rtHeight = m_GameViewRenderTarget->GetHeight();

    // Evaluate post-process volumes: blend active volumes into the current PP settings
    if (m_PostProcessing && m_World && m_Camera) {
        EvaluatePostProcessVolumes(m_Camera->GetPosition());
    }

    // Always render to scene RT then copy to game view RT.
    // Never render directly to game view RT's MRT render pass (causes teal on NVIDIA).
    bool usePostProcessing = m_SceneRenderTarget && m_SceneRenderTarget->IsValid();
    bool cameraPPEnabled = true;
    if (m_World) {
        auto activeCam = ECS::CameraManager::GetActiveCamera(m_World);
        if (activeCam != ECS::INVALID_ENTITY) {
            auto* cc = m_World->GetComponent<ECS::CameraComponent>(activeCam);
            if (cc) cameraPPEnabled = cc->enablePostProcessing;
        }
    }
    // Path tracer display: when RT is in path-trace mode with accumulated samples,
    // the PT image replaces the rasterized scene as the post-process source. The PP
    // shader path is forced even if the camera disables post-processing — the PT
    // image is HDR radiance and needs the tonemap to be displayable.
    bool ptDisplayActive = false;
    if (m_RenderSystem && m_RenderSystem->IsRayTracingEnabled() && m_RenderSystem->GetRTMode() == 1) {
        auto* pathTracer = m_RenderSystem->GetPathTracer();
        ptDisplayActive = pathTracer && pathTracer->GetOutputView() != VK_NULL_HANDLE &&
                          pathTracer->GetAccumulatedSamples() > 0;
    }

    // Hybrid RT overlay: when RT is in hybrid mode with shadows or AO enabled, the
    // post-process shader multiplies the resolved scene by the ray-traced shadow/AO.
    // Forces the PP shader path on (the overlay needs it) even if the camera
    // disables other post effects.
    bool rtHybridActive = m_RenderSystem && m_RenderSystem->IsRTHybridActive();
    if (m_PostProcessing && m_PostProcessing->IsInitialized()) {
        auto& pps = m_PostProcessing->GetSettings();
        if (rtHybridActive) {
            m_PostProcessing->SetRTHybridInputs(m_RenderSystem->GetRTHybridShadowView(),
                                                m_RenderSystem->GetRTHybridAOView(),
                                                m_RenderSystem->GetRTHybridReflectView(),
                                                m_RenderSystem->GetRTHybridGIView(),
                                                m_RenderSystem->GetRTHybridSampler());
            // Per-effect strengths come from the RT compositor config, which the
            // Scene Settings "RT Compositor" sliders drive and which serializes.
            // A disabled effect contributes 0 regardless of its slider.
            f32 shadowS = 1.0f, reflectS = 0.5f, aoS = 1.0f, giS = 0.5f;
            if (auto* comp = m_RenderSystem->GetRTCompositor()) {
                shadowS = comp->GetConfig().shadowStrength;
                reflectS = comp->GetConfig().reflectionStrength;
                aoS = comp->GetConfig().aoStrength;
                giS = comp->GetConfig().giStrength;
            }
            pps.rtHybridEnable = 1;
            pps.rtShadowStrength = (m_RenderSystem->GetRTShadows() &&
                                    m_RenderSystem->GetRTShadows()->GetConfig().enabled) ? shadowS : 0.0f;
            pps.rtAOStrength = (m_RenderSystem->GetRTAO() &&
                                m_RenderSystem->GetRTAO()->GetConfig().enabled) ? aoS : 0.0f;
            pps.rtReflectStrength = (m_RenderSystem->GetRTReflections() &&
                                     m_RenderSystem->GetRTReflections()->GetConfig().enabled) ? reflectS : 0.0f;
            pps.rtGIStrength = (m_RenderSystem->GetRTGI() &&
                                m_RenderSystem->GetRTGI()->GetConfig().enabled) ? giS : 0.0f;
        } else {
            pps.rtHybridEnable = 0;
            m_PostProcessing->SetRTHybridInputs(VK_NULL_HANDLE, VK_NULL_HANDLE,
                                                VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE);
        }
    }

    bool usePPShader = usePostProcessing && m_PostProcessing &&
                       m_PostProcessing->IsInitialized() && (cameraPPEnabled || ptDisplayActive || rtHybridActive);

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

    // Record the RT chain (TLAS + dispatch) for the game view. Update() never
    // reaches RT in editor mode (m_SkipMainPassRendering early-return), so this
    // is the editor's only RT dispatch site. Must be outside a render pass;
    // single-queue path (no async compute) keeps ordering simple.
    m_RenderSystem->RecordRTFrame(false);

    // Script render targets (FR-4): record before the game view so mirror quads
    // in the scene sample this frame's offscreen view. Must be outside the
    // scene target's render pass.
    if (m_RenderSystem->HasScriptRenderTargets()) {
        m_RenderSystem->RenderScriptTargets(commandBuffer);
    }

    // Render scene + effects into the chosen target
    sceneTarget->Begin(commandBuffer);
    if (useSplitscreen && !splitViewports.empty()) {
        m_RenderSystem->RenderSplitscreen(sceneTarget, splitViewports);
        if (m_GameViewWeatherParticles) {
            m_RenderSystem->RenderWeatherParticles(m_WeatherSystem, m_GameViewIsRain, rtWidth, rtHeight,
                                                   /*useOffscreenSets*/ true, /*viewport*/ 0);
        }
        m_RenderSystem->RenderElementalParticles(m_ElementalSystem, rtWidth, rtHeight,
                                                 /*useOffscreenSets*/ true, /*viewport*/ 0);
    } else {
        m_RenderSystem->RenderToTarget(sceneTarget, &gameCamera, 1);
        // CPU particle emitters (e.g. a fountain) into the offscreen target — was
        // never drawn in the offscreen path, only in the direct main pass.
        m_RenderSystem->RenderParticles(rtWidth, rtHeight, /*useOffscreenSets*/ true, /*viewport*/ 1);
        if (m_GameViewWeatherParticles) {
            m_RenderSystem->RenderWeatherParticles(m_WeatherSystem, m_GameViewIsRain, rtWidth, rtHeight,
                                                   /*useOffscreenSets*/ true, /*viewport*/ 1);
        }
        m_RenderSystem->RenderElementalParticles(m_ElementalSystem, rtWidth, rtHeight,
                                                 /*useOffscreenSets*/ true, /*viewport*/ 1);
    }
    sceneTarget->End(commandBuffer);

    // Apply post-processing: read from scene RT, write to game view RT
    if (usePostProcessing) {
        // Camera planes for depth linearization — from the GAME camera: this
        // pass post-processes the game view, and SSAO/contact shadows/DoF all
        // linearize the game view's depth (was m_Camera, the EDITOR fly-cam,
        // whose near/far can differ).
        m_PostProcessing->SetCameraPlanes(gameCamera.GetNearPlane(), gameCamera.GetFarPlane());

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
            // Forward view-projection — SSAO sample projection and the
            // contact-shadow world-space march project world points to screen
            m_PostProcessing->SetViewProjection(
                Math::Vector4(vp.m[0], vp.m[1], vp.m[2], vp.m[3]),
                Math::Vector4(vp.m[4], vp.m[5], vp.m[6], vp.m[7]),
                Math::Vector4(vp.m[8], vp.m[9], vp.m[10], vp.m[11]),
                Math::Vector4(vp.m[12], vp.m[13], vp.m[14], vp.m[15]));

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

        // GPU timestamp: post-process begin
        if (m_Renderer) {
            VkQueryPool tsPool = m_Renderer->GetTimestampPool(m_Renderer->GetCurrentFrameIndex());
            if (tsPool != VK_NULL_HANDLE) {
                vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, tsPool, Renderer::GPU_TS_POSTPROCESS_BEGIN);
            }
        }

        // Temporal resolve: TAA and/or temporal upscaler.
        // When an upscaler is active, TAA runs at the lower render resolution first,
        // then the upscaler (Lanczos + CAS) upsamples to display resolution.
        // When no upscaler is active, TAA runs at display resolution as before.
        // Both compute dispatches must happen outside a render pass.
        bool upscalerActive = m_RenderSystem && m_RenderSystem->IsUpscalerActive();

        // Tracks whether some pass (TAA/upscaler/path tracer) redirected the PP
        // source this frame; when none did, the source is rebound to the scene RT
        // color below (it may still hold last frame's redirect).
        bool ppSourceRedirected = false;

        // TAA resolve pass (runs at render resolution — same as scene target)
        if (cameraPPEnabled && m_PostProcessing->IsTAAEnabled() && m_Renderer && !upscalerActive) {
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
                m_LastPPSourceView = taaOutput;
                ppSourceRedirected = true;
            }
        }

        // Temporal upscaler dispatch (FSR 2 / DLSS / XeSS)
        // Runs after TAA (if active) or directly on the jittered scene color.
        // The upscaler takes the low-res color and upscales to display resolution.
        if (cameraPPEnabled && upscalerActive && m_SceneRenderTarget && m_SceneRenderTarget->IsValid()) {
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
                // Get upscaled output via base class virtual (no dynamic_cast needed)
                VkImageView upscaledOutput = upscaler->GetOutputImageView();
                if (upscaledOutput != VK_NULL_HANDLE) {
                    m_PostProcessing->UpdateSourceImage(upscaledOutput, m_SceneRenderTarget->GetSampler());
                    m_LastPPSourceView = upscaledOutput;
                    ppSourceRedirected = true;
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

        // Path tracer display: bind the accumulated PT image as the PP source.
        // RecordRTFrame already transitioned it to SHADER_READ_ONLY after dispatch
        // (and restores GENERAL before the next dispatch), so no barriers here.
        // The descriptor rebind is change-tracked: in steady state the set is
        // written once, not per frame (the set is plain, no update-after-bind).
        if (ptDisplayActive && usePPShader) {
            auto* pathTracer = m_RenderSystem->GetPathTracer();
            if (pathTracer && pathTracer->GetOutputImage() != VK_NULL_HANDLE) {
                if (m_LastPPSourceView != pathTracer->GetOutputView()) {
                    m_PostProcessing->UpdateSourceImage(pathTracer->GetOutputView(),
                                                        pathTracer->GetOutputSampler());
                    m_LastPPSourceView = pathTracer->GetOutputView();
                    ENJIN_LOG_INFO(Editor, "Path tracer display active: game view shows PT accumulation (%u samples)",
                                   pathTracer->GetAccumulatedSamples());
                }
                ppSourceRedirected = true;
            }
        }

        // No redirect this frame: make sure the source is the scene RT color
        // (the set may still hold last frame's TAA/upscaler/PT view).
        if (!ppSourceRedirected && usePPShader &&
            m_SceneRenderTarget && m_SceneRenderTarget->IsValid() &&
            m_LastPPSourceView != m_SceneRenderTarget->GetColorImageView()) {
            m_PostProcessing->UpdateSourceImage(
                m_SceneRenderTarget->GetColorImageView(),
                m_SceneRenderTarget->GetSampler());
            m_LastPPSourceView = m_SceneRenderTarget->GetColorImageView();
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

        // NOTE: no source-image restore here. The PP descriptor set is recorded
        // into this frame's command buffer — rewriting it after the draw is
        // recorded (pre-submit) makes the draw sample the restored view instead.
        // Stale sources are handled by the change-tracked rebind above instead.

        // GPU timestamp: post-process end
        if (m_Renderer) {
            VkQueryPool tsPool = m_Renderer->GetTimestampPool(m_Renderer->GetCurrentFrameIndex());
            if (tsPool != VK_NULL_HANDLE) {
                vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, tsPool, Renderer::GPU_TS_POSTPROCESS_END);
            }
        }
    }

    // Set weather for main pass (editor viewport) so it renders weather particles too
    if (m_GameViewWeatherParticles) {
        m_RenderSystem->SetMainPassWeather(&m_WeatherSystem, m_GameViewIsRain);
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
            // WARN level on purpose — the console's Warn filter isolates the perf diagnostics.
            ENJIN_LOG_WARN(Editor,
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

    // Apply deferred ImGui pipeline update after MSAA change.
    // The MSAA change itself was applied at the start of RenderSystem::Update()
    // (which runs before this). Now update ImGui to match the new render pass.
    if (m_MSAAImGuiUpdatePending) {
        m_MSAAImGuiUpdatePending = false;
        if (m_RenderSystem && m_RenderSystem->GetRenderer()) {
            m_ImGuiLayer->UpdateRenderPass(
                m_RenderSystem->GetVulkanRenderer()->GetRenderPass(),
                m_RenderSystem->GetVulkanRenderer()->GetMSAASamples());
        }
    }

    // Apply deferred ImGui pipeline + post-process update after an HDR toggle.
    // The swapchain/render-pass recreation ran at the start of RenderSystem::Update();
    // only now (between frames) is it safe to rebuild the ImGui pipeline for the new
    // render-pass format and read back the resolved HDR output mode. Skip until the
    // deferred change has actually been applied.
    if (m_HDRImGuiUpdatePending && m_RenderSystem && !m_RenderSystem->IsHDRChangePending()) {
        m_HDRImGuiUpdatePending = false;
        if (m_RenderSystem->GetRenderer()) {
            m_ImGuiLayer->UpdateRenderPass(
                m_RenderSystem->GetVulkanRenderer()->GetRenderPass(),
                m_RenderSystem->GetVulkanRenderer()->GetMSAASamples());
        }
        if (m_PostProcessing) {
            m_PostProcessing->GetSettings().hdrOutputMode = m_RenderSystem->GetHDROutputMode();
        }
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

        // Render UI canvases during play mode (fullscreen). HUDSystem is
        // retired — legacy hudWidget data migrates to UICanvas on scene load,
        // so canvases are the ONE UI path (world-space tags need the camera).
        if (m_PlayMode.IsPlaying()) {
            m_UISystem.Update(m_World, io.DisplaySize.x, io.DisplaySize.y, m_LastDeltaTime,
                              0.0f, 0.0f, m_Camera);
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
        ImGui::DockBuilderDockWindow("Settings", dockRight);       // Tabbed with Inspector
        ImGui::DockBuilderDockWindow("Console", dockBottom);
        ImGui::DockBuilderDockWindow("Asset Browser", dockBottom); // Tabbed with Console
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
    // VWS override-layer panel (standalone bool: the EditorPanel bitmask is full).
    if (m_ShowLayersPanel) {
        DrawLayersPanel();
    }
    // Undo/redo history panel (standalone bool, same reason)
    if (m_ShowHistoryPanel) {
        DrawHistoryPanel();
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
        bool profilerOpen = true;
        Debug::Profiler::Instance().DrawProfilerPanel(&profilerOpen);
        if (!profilerOpen) {
            SetPanelVisibility(EditorPanel::Profiler, false);
        }
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
    // UV Preview panel (bool-toggled, not in EditorPanel bitfield)
    DrawUVPreviewPanel();

    // Creative-mode build palette (SimCity-style drag-to-place)
    DrawCreativePalette();

    if (m_ShowDebugOverlay) {
        DrawDebugOverlay();
    }
    // Legacy panels (kept for backward compat, hidden by default)
    if (m_ShowGameDebug) {
        DrawGameDebugPanel();
    }
    if (m_ShowDebugWorkstation) {
        DrawDebugWorkstation();
        // X-closing the panel ends the F2 group, so the next F2 reopens it
        if (!m_ShowDebugWorkstation && m_EngineDebugActive) {
            m_EngineDebugActive = false;
            m_DebugOverlayDetail = 0;
            m_ShowDebugOverlay = m_GameDebugActive;
        }
    }
    if (m_ShowHTML5ExportDialog) {
        DrawHTML5ExportDialog();
    }

    // Dialogue tree editor (owns its own window) - legacy, now use DrawDialoguePanel()
    // m_DialogueTreeEditor.Render();

    // Graph editor windows (use IsOpen pattern, not panel bits)
    if (m_ShaderGraphEditor.IsOpen()) {
        // Restore an entity's saved graph into the editor. Two ways in, neither
        // clobbers a graph you're actively building:
        //  - auto: selecting an entity that has a stored graph loads it, but only
        //    when the current editor graph is still the untouched default
        //  - explicit: the "Load Graph from Selected" button always loads it
        bool selHasGraph = false;
        ECS::CustomShaderComponent* selCS = nullptr;
        if (m_World && m_PrimarySelected != ECS::INVALID_ENTITY && m_World->IsValid(m_PrimarySelected)) {
            selCS = m_World->GetComponent<ECS::CustomShaderComponent>(m_PrimarySelected);
            selHasGraph = selCS && !selCS->graphJson.empty();
        }
        if (selHasGraph && m_PrimarySelected != m_ShaderGraphLoadedEntity &&
            m_ShaderGraphEditor.IsDefaultGraph()) {
            if (m_ShaderGraphEditor.FromJsonString(selCS->graphJson)) {
                m_ShaderGraphLoadedEntity = m_PrimarySelected;
                ENJIN_LOG_INFO(Editor, "Loaded stored shader graph from selected entity");
            }
        }

        ImGui::Begin("Shader Graph Entity Link");
        if (selHasGraph) {
            ImGui::TextWrapped("Selected entity has a saved shader graph.");
            if (ImGui::Button("Load Graph from Selected")) {
                if (m_ShaderGraphEditor.FromJsonString(selCS->graphJson)) {
                    m_ShaderGraphLoadedEntity = m_PrimarySelected;
                    ENJIN_LOG_INFO(Editor, "Loaded stored shader graph from selected entity");
                }
            }
        } else if (m_PrimarySelected != ECS::INVALID_ENTITY) {
            ImGui::TextDisabled("Selected entity has no saved graph.");
        } else {
            ImGui::TextDisabled("Select an entity to link a graph.");
        }
        ImGui::End();

        m_ShaderGraphEditor.Render();
        // "Apply to Selected Entity": compile the graph and bind it as a live custom
        // shader on the current selection (RenderSystem shares the main pipeline layout).
        DrawAtlasPackerWindow();
        if (m_ShaderGraphEditor.ConsumeApplyRequest()) {
            if (m_RenderSystem && m_World && m_PrimarySelected != ECS::INVALID_ENTITY &&
                m_World->IsValid(m_PrimarySelected)) {
                m_ShaderGraphEditor.SetTextureResolver([this](const std::string& p) {
                    return m_RenderSystem ? m_RenderSystem->ResolveBindlessTextureIndex(p) : -1;
                });
                auto code = m_ShaderGraphEditor.GenerateGLSL();
                if (code.success) {
                    std::string err;
                    if (m_RenderSystem->SetEntityCustomShader(
                            m_PrimarySelected, code.vertexCode, code.fragmentCode, err)) {
                        // Persist: the assignment now survives save/load (and ships
                        // in exports - FlushPendingChanges re-applies on load).
                        ECS::CustomShaderComponent* cs =
                            m_World->HasComponent<ECS::CustomShaderComponent>(m_PrimarySelected)
                                ? m_World->GetComponent<ECS::CustomShaderComponent>(m_PrimarySelected)
                                : nullptr;
                        if (!cs) {
                            m_World->AddComponent<ECS::CustomShaderComponent>(m_PrimarySelected, {});
                            cs = m_World->GetComponent<ECS::CustomShaderComponent>(m_PrimarySelected);
                        }
                        if (cs) {
                            cs->vertexSource = code.vertexCode;
                            cs->fragmentSource = code.fragmentCode;
                            cs->graphLabel = "shader graph";
                            // Store the editable node graph too, so reopening the
                            // scene restores the graph in the editor, not just the
                            // compiled GLSL.
                            cs->graphJson = m_ShaderGraphEditor.ToJsonString();
                            cs->applied = true;
                            cs->failed = false;
                        }
                        // This graph now belongs to this entity — don't auto-reload over it
                        m_ShaderGraphLoadedEntity = m_PrimarySelected;
                        ENJIN_LOG_INFO(Editor, "Applied shader graph to selected entity (persisted)");
                    } else {
                        ENJIN_LOG_ERROR(Editor, "Custom shader apply failed: %s", err.c_str());
                    }
                } else {
                    ENJIN_LOG_ERROR(Editor, "Shader graph has errors; not applied");
                }
            } else {
                ENJIN_LOG_WARN(Editor, "Apply shader graph: select an entity first");
            }
        }
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

    // Import result dialog (shows after model import)
    if (m_ShowImportResultDialog) {
        DrawImportResultDialog();
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

    // Quit feedback survey dialog
    if (m_ShowQuitFeedbackDialog) {
        DrawQuitFeedbackDialog();
    }

    // Guard: opening a scene that belongs to a different project
    DrawWrongProjectDialog();

    // Guard: the open scene file was changed on disk out-of-band
    DrawExternalSceneChangeDialog();

    // Read render stats AFTER the render pass (counters are reset at start, incremented during draw)
    if (m_RenderSystem) {
        m_PerfMetrics.drawCallCount = m_RenderSystem->GetDrawCallCount();
        m_PerfMetrics.triangleCount = m_RenderSystem->GetTriangleCount();
        m_PerfMetrics.descriptorCacheHits = m_RenderSystem->GetDescriptorCacheHits();
        m_PerfMetrics.descriptorCacheWrites = m_RenderSystem->GetDescriptorCacheWrites();
    }

    // Clear the force flag after one frame
    if (m_ForceLayout) m_ForceLayout = false;

    // Submit onion skin ghosts to RenderSystem for editor viewport rendering
    {
        std::vector<Editor::OnionSkinGhost> allGhosts;

        // 2D Flash timeline onion skin ghosts
        if (m_RenderSystem && m_FlashTimelineEditor.GetTimeline()) {
            auto flashGhosts = m_FlashTimelineEditor.ComputeOnionSkinGhosts();
            allGhosts.insert(allGhosts.end(),
                std::make_move_iterator(flashGhosts.begin()),
                std::make_move_iterator(flashGhosts.end()));
        }

        // 3D Skeletal animation onion skin ghosts
        if (m_RenderSystem && m_World && m_PrimarySelected != ECS::INVALID_ENTITY) {
            auto* animComp = m_World->GetComponent<ECS::AnimatorComponent>(m_PrimarySelected);
            if (animComp && animComp->onionSkin.enabled && animComp->animator.IsPlaying()) {
                auto& animator = animComp->animator;
                const auto& onion = animComp->onionSkin;
                const auto& currentAnimName = animator.GetCurrentAnimationName();
                const auto& animations = animator.GetAnimations();
                auto animIt = animations.find(currentAnimName);

                if (animIt != animations.end() && animIt->second.duration > 0.0f) {
                    f32 duration = animIt->second.duration;
                    f32 currentTime = animator.GetCurrentTime();
                    f32 frameStep = 1.0f / animIt->second.ticksPerSecond;

                    auto* transform = m_World->GetComponent<ECS::TransformComponent>(m_PrimarySelected);
                    Math::Vector3 entityPos = transform ? transform->position : Math::Vector3(0, 0, 0);
                    Math::Vector3 entityRot = transform ? transform->rotation.ToEuler() : Math::Vector3(0, 0, 0);
                    Math::Vector3 entityScale = transform ? transform->scale : Math::Vector3(1, 1, 1);

                    // Ghost frames before current time
                    for (i32 i = 1; i <= onion.framesBefore; ++i) {
                        f32 ghostTime = currentTime - static_cast<f32>(i) * frameStep;
                        if (ghostTime < 0.0f) {
                            if (animIt->second.playMode == Animation::PlayMode::Loop) {
                                ghostTime = std::fmod(ghostTime + duration * 100.0f, duration);
                            } else {
                                continue;
                            }
                        }

                        auto skinningMatrices = animator.SampleSkinningMatricesAtTime(ghostTime);
                        if (skinningMatrices.empty()) continue;

                        f32 falloff = std::pow(onion.opacityFalloff, static_cast<f32>(i - 1));
                        Editor::OnionSkinGhost ghost;
                        ghost.entity = m_PrimarySelected;
                        ghost.position = entityPos;
                        ghost.rotation = entityRot;
                        ghost.scale = entityScale;
                        ghost.alpha = 1.0f;
                        ghost.tint = onion.beforeTint;
                        ghost.ghostOpacity = onion.opacity * falloff;
                        ghost.skinningMatrices = std::move(skinningMatrices);
                        allGhosts.push_back(std::move(ghost));
                    }

                    // Ghost frames after current time
                    for (i32 i = 1; i <= onion.framesAfter; ++i) {
                        f32 ghostTime = currentTime + static_cast<f32>(i) * frameStep;
                        if (ghostTime > duration) {
                            if (animIt->second.playMode == Animation::PlayMode::Loop) {
                                ghostTime = std::fmod(ghostTime, duration);
                            } else {
                                continue;
                            }
                        }

                        auto skinningMatrices = animator.SampleSkinningMatricesAtTime(ghostTime);
                        if (skinningMatrices.empty()) continue;

                        f32 falloff = std::pow(onion.opacityFalloff, static_cast<f32>(i - 1));
                        Editor::OnionSkinGhost ghost;
                        ghost.entity = m_PrimarySelected;
                        ghost.position = entityPos;
                        ghost.rotation = entityRot;
                        ghost.scale = entityScale;
                        ghost.alpha = 1.0f;
                        ghost.tint = onion.afterTint;
                        ghost.ghostOpacity = onion.opacity * falloff;
                        ghost.skinningMatrices = std::move(skinningMatrices);
                        allGhosts.push_back(std::move(ghost));
                    }
                }
            }
        }

        if (m_RenderSystem) {
            if (!allGhosts.empty()) {
                m_RenderSystem->SetOnionSkinGhosts(allGhosts);
            } else {
                m_RenderSystem->ClearOnionSkinGhosts();
            }
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
        ImDrawList* fgOverlay = GetViewportOverlayDrawList();
        fgOverlay->PushClipRect(
            ImVec2(m_EditorViewportImageMinX, m_EditorViewportImageMinY),
            ImVec2(m_EditorViewportImageMaxX, m_EditorViewportImageMaxY), true);

        // Draw gizmos for selected entity
        DrawGizmos();

        // Draw marquee selection rectangle
        DrawMarqueeRect();

        // Highlight the selected entity + its descendants (projected bounding boxes)
        DrawSelectionHighlight();

        // Always-visible camera gizmos (virtual cameras have no mesh to click)
        DrawCameraGizmos();

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

            ImDrawList* bgDrawList = GetViewportOverlayDrawList();
            bgDrawList->PushClipRect(
                ImVec2(m_EditorViewportImageMinX, m_EditorViewportImageMinY),
                ImVec2(m_EditorViewportImageMaxX, m_EditorViewportImageMaxY), true);

            // --- Override-layer color highlight ---
            // Each layer can carry a color; every entity a layer touches is marked
            // in that color so you can see at a glance which layer owns what. Driven
            // by the per-layer `highlight` toggle in the Layers panel.
            {
                const Scene::LayerStack& layerStack = m_LayerSystem.Stack();
                bool anyHighlight = false;
                for (const Scene::Layer& l : layerStack.layers) {
                    if (l.highlight && !l.entities.empty()) { anyHighlight = true; break; }
                }
                if (anyHighlight) {
                    // Map stable id -> live entity once for the whole pass.
                    std::unordered_map<u64, ECS::Entity> bySid;
                    for (ECS::Entity e : m_World->GetEntitiesWithComponent<ECS::StableIdComponent>()) {
                        auto* sid = m_World->GetComponent<ECS::StableIdComponent>(e);
                        if (sid && sid->id != 0) bySid[sid->id] = e;
                    }
                    for (const Scene::Layer& layer : layerStack.layers) {
                        if (!layer.highlight) continue;
                        ImU32 col = IM_COL32(static_cast<int>(layer.color.x * 255.0f),
                                             static_cast<int>(layer.color.y * 255.0f),
                                             static_cast<int>(layer.color.z * 255.0f), 230);
                        for (const Scene::EntityDelta& ed : layer.entities) {
                            auto it = bySid.find(ed.stableId);
                            if (it == bySid.end()) continue;
                            auto* tr = m_World->GetComponent<ECS::TransformComponent>(it->second);
                            if (!tr) continue;
                            ImVec2 sp;
                            if (worldToScreen(tr->position, sp)) {
                                bgDrawList->AddCircleFilled(sp, 5.0f, col);
                                bgDrawList->AddCircle(sp, 9.0f, col, 0, 2.0f);
                            }
                            // Wrap the object when it has a box collider (world-space size).
                            if (auto* bc = m_World->GetComponent<ECS::BoxColliderComponent>(it->second)) {
                                drawWireBox(bgDrawList, tr->position + bc->center, bc->size * 0.5f, col, 1.5f);
                            }
                        }
                    }
                }
            }

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

            // --- Physics debug visualization: colliders + joints + gizmos ---
            // Selected entities ALWAYS show their gizmos/colliders; the
            // m_ShowColliderWireframes flag (View > Show Colliders, F2, Rendering
            // panel) additionally reveals them for every entity in the scene.
            // This block used to be entirely gated on the flag, which defaults
            // off — so colliders and the component gizmos were never visible
            // unless you found the toggle (Marty, 2026-08-07).
            {
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
                        if (!m_ShowColliderWireframes && !sel) continue;
                        ImU32 color = sel ? IM_COL32(255, 220, 50, 220) : IM_COL32(255, 220, 50, 100);
                        f32 thick = sel ? 2.0f : 1.0f;
                        Math::Vector3 halfExt = box->size * 0.5f;
                        Math::Vector3 worldCenter = transform->position + transform->rotation.Rotate(box->center);
                        drawWireBox(bgDrawList, worldCenter, halfExt, color, thick, transform->rotation);
                        if (sel) {
                            ImVec2 lp;
                            if (worldToScreen(worldCenter, lp))
                                bgDrawList->AddText(ImVec2(lp.x + 6.0f, lp.y - 6.0f), color, "Box Collider");
                        }
                    }
                }

                // Sphere colliders (green-yellow, 3 orthogonal circles)
                for (ECS::Entity entity : m_World->GetEntitiesWithComponent<ECS::SphereColliderComponent>()) {
                    auto* sphere = m_World->GetComponent<ECS::SphereColliderComponent>(entity);
                    auto* transform = m_World->GetComponent<ECS::TransformComponent>(entity);
                    if (sphere && transform) {
                        bool sel = IsSelected(entity);
                        if (!m_ShowColliderWireframes && !sel) continue;
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
                        if (!m_ShowColliderWireframes && !sel) continue;
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
                        if (sel) {
                            ImVec2 lp;
                            if (worldToScreen(c, lp)) {
                                // A capsule wireframe is ambiguous: is it a plain collider
                                // or a character controller? Say which so it's not guesswork.
                                bool ctrl = m_World->HasComponent<ECS::Platformer2DController>(entity) ||
                                            m_World->HasComponent<ECS::TopDown2DController>(entity) ||
                                            m_World->HasComponent<ECS::TopDown3DController>(entity) ||
                                            m_World->HasComponent<ECS::ThirdPersonController>(entity) ||
                                            m_World->HasComponent<ECS::FirstPersonController>(entity);
                                bgDrawList->AddText(ImVec2(lp.x + 6.0f, lp.y - 6.0f), color,
                                    ctrl ? "Capsule Collider (Character Controller)" : "Capsule Collider");
                            }
                        }
                    }
                }

                // Creative-mode editable boundary (lake outline): draggable handles on
                // the selected entity's polygon points. Drag a handle on the ground to
                // pull the shoreline; right-click a handle to remove it (min 3);
                // double-click an edge to insert a point. Live: the water mesh rebuilds
                // from the polygon (dirty flag).
                if (m_PrimarySelected != ECS::INVALID_ENTITY) {
                    auto* bpoly = m_World->GetComponent<ECS::BoundaryPolygonComponent>(m_PrimarySelected);
                    auto* bxf   = m_World->GetComponent<ECS::TransformComponent>(m_PrimarySelected);
                    if (bpoly && bxf && bpoly->points.size() >= 3) {
                        const ImVec2 mp = ImGui::GetMousePos();
                        auto ptWorld = [&](usize i) {
                            return Math::Vector3(bxf->position.x + bpoly->points[i].x,
                                                 bxf->position.y,
                                                 bxf->position.z + bpoly->points[i].y);
                        };
                        // Edges
                        for (usize i = 0; i < bpoly->points.size(); ++i)
                            drawLine3D(bgDrawList, ptWorld(i), ptWorld((i + 1) % bpoly->points.size()),
                                       IM_COL32(80, 160, 235, 160), 1.5f);
                        // Handles + hover
                        int hovered = -1;
                        for (usize i = 0; i < bpoly->points.size(); ++i) {
                            ImVec2 sp;
                            if (!worldToScreen(ptWorld(i), sp)) continue;
                            bool hov = (std::abs(sp.x - mp.x) + std::abs(sp.y - mp.y)) < 12.0f;
                            if (hov) hovered = static_cast<int>(i);
                            ImU32 col = (m_BoundaryDragPoint == static_cast<i32>(i))
                                ? IM_COL32(255, 220, 50, 255)
                                : (hov ? IM_COL32(150, 210, 255, 255) : IM_COL32(80, 150, 230, 230));
                            bgDrawList->AddCircleFilled(sp, hov ? 7.0f : 5.0f, col);
                            bgDrawList->AddCircle(sp, hov ? 7.0f : 5.0f, IM_COL32(20, 30, 50, 255), 0, 1.5f);
                        }
                        // Begin drag on a handle
                        if (m_EditorViewportHovered && !ImGuizmo::IsOver() &&
                            ImGui::IsMouseClicked(0) && hovered >= 0)
                            m_BoundaryDragPoint = hovered;
                        // Drag the grabbed handle across the ground plane (Y = entity Y)
                        if (m_BoundaryDragPoint >= 0 && ImGui::IsMouseDown(0) &&
                            m_BoundaryDragPoint < static_cast<i32>(bpoly->points.size())) {
                            Ray r = ScenePicker::ScreenToRay(m_Camera, mp.x - m_EditorViewportImageMinX,
                                                             mp.y - m_EditorViewportImageMinY, sw, sh);
                            if (std::abs(r.direction.y) > 1e-6f) {
                                f32 t = (bxf->position.y - r.origin.y) / r.direction.y;
                                if (t > 0.0f) {
                                    Math::Vector3 hit = r.origin + r.direction * t;
                                    bpoly->points[static_cast<usize>(m_BoundaryDragPoint)] =
                                        Math::Vector2(hit.x - bxf->position.x, hit.z - bxf->position.z);
                                    bpoly->dirty = true;
                                }
                            }
                        }
                        if (!ImGui::IsMouseDown(0)) m_BoundaryDragPoint = -1;
                        // Right-click a handle to delete it (keep at least a triangle)
                        if (hovered >= 0 && ImGui::IsMouseClicked(1) && bpoly->points.size() > 3) {
                            bpoly->points.erase(bpoly->points.begin() + hovered);
                            bpoly->dirty = true;
                        }
                        // Double-click an edge midpoint to insert a new point there
                        if (m_EditorViewportHovered && ImGui::IsMouseDoubleClicked(0) && hovered < 0) {
                            int bestEdge = -1; f32 bestD = 14.0f;
                            for (usize i = 0; i < bpoly->points.size(); ++i) {
                                usize j = (i + 1) % bpoly->points.size();
                                Math::Vector3 mid = (ptWorld(i) + ptWorld(j)) * 0.5f;
                                ImVec2 sp;
                                if (!worldToScreen(mid, sp)) continue;
                                f32 d = std::abs(sp.x - mp.x) + std::abs(sp.y - mp.y);
                                if (d < bestD) { bestD = d; bestEdge = static_cast<int>(i); }
                            }
                            if (bestEdge >= 0) {
                                usize j = (static_cast<usize>(bestEdge) + 1) % bpoly->points.size();
                                Math::Vector2 midXZ = (bpoly->points[static_cast<usize>(bestEdge)] + bpoly->points[j]) * 0.5f;
                                bpoly->points.insert(bpoly->points.begin() + bestEdge + 1, midXZ);
                                bpoly->dirty = true;
                            }
                        }
                    }
                }

                // Mesh colliders (magenta wireframe from cached vertices)
                for (ECS::Entity entity : m_World->GetEntitiesWithComponent<ECS::MeshColliderComponent>()) {
                    auto* meshCol = m_World->GetComponent<ECS::MeshColliderComponent>(entity);
                    auto* transform = m_World->GetComponent<ECS::TransformComponent>(entity);
                    if (meshCol && transform && meshCol->generated && !meshCol->vertices.empty()) {
                        bool sel = IsSelected(entity);
                        if (!m_ShowColliderWireframes && !sel) continue;
                        ImU32 color = sel ? IM_COL32(220, 80, 220, 220) : IM_COL32(220, 80, 220, 80);
                        f32 thick = sel ? 2.0f : 1.0f;

                        // Draw triangle edges if we have indices. Cached collider
                        // vertices are WORLD-SCALE (JoltBackend bakes entity scale
                        // at generation) — apply only rotation + translation here,
                        // matching how the box/sphere/capsule wireframes treat
                        // their world-space sizes.
                        if (!meshCol->indices.empty() && meshCol->indices.size() % 3 == 0) {
                            for (size_t i = 0; i + 2 < meshCol->indices.size(); i += 3) {
                                auto transformVert = [&](const Math::Vector3& v) {
                                    return transform->position + transform->rotation.Rotate(v);
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
                        if (!m_ShowColliderWireframes && !sel) continue;
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

                // Joints draw when the flag is on OR either connected entity is selected
                auto jointVisible = [&](ECS::Entity holder, ECS::Entity eA, ECS::Entity eB) {
                    return m_ShowColliderWireframes || IsSelected(holder) || IsSelected(eA) || IsSelected(eB);
                };
                // Distance joints (white)
                for (ECS::Entity e : m_World->GetEntitiesWithComponent<ECS::DistanceJointComponent>()) {
                    auto* j = m_World->GetComponent<ECS::DistanceJointComponent>(e);
                    if (j && jointVisible(e, j->entityA, j->entityB)) drawJointLine(bgDrawList, j->entityA, j->entityB, j->anchorA, j->anchorB, IM_COL32(255, 255, 255, 180));
                }
                // Hinge joints (cyan)
                for (ECS::Entity e : m_World->GetEntitiesWithComponent<ECS::HingeJointComponent>()) {
                    auto* j = m_World->GetComponent<ECS::HingeJointComponent>(e);
                    if (j && jointVisible(e, j->entityA, j->entityB)) drawJointLine(bgDrawList, j->entityA, j->entityB, j->anchorA, j->anchorB, IM_COL32(0, 220, 255, 180));
                }
                // BallSocket joints (magenta)
                for (ECS::Entity e : m_World->GetEntitiesWithComponent<ECS::BallSocketJointComponent>()) {
                    auto* j = m_World->GetComponent<ECS::BallSocketJointComponent>(e);
                    if (j && jointVisible(e, j->entityA, j->entityB)) drawJointLine(bgDrawList, j->entityA, j->entityB, j->anchorA, j->anchorB, IM_COL32(220, 50, 220, 180));
                }
                // Spring joints (green)
                for (ECS::Entity e : m_World->GetEntitiesWithComponent<ECS::SpringJointComponent>()) {
                    auto* j = m_World->GetComponent<ECS::SpringJointComponent>(e);
                    if (j && jointVisible(e, j->entityA, j->entityB)) drawJointLine(bgDrawList, j->entityA, j->entityB, j->anchorA, j->anchorB, IM_COL32(50, 220, 50, 180));
                }
                // Fixed joints (red)
                for (ECS::Entity e : m_World->GetEntitiesWithComponent<ECS::FixedJointComponent>()) {
                    auto* j = m_World->GetComponent<ECS::FixedJointComponent>(e);
                    if (j && jointVisible(e, j->entityA, j->entityB)) drawJointLine(bgDrawList, j->entityA, j->entityB, j->anchorA, j->anchorB, IM_COL32(220, 50, 50, 180));
                }
                // Slider joints (blue)
                for (ECS::Entity e : m_World->GetEntitiesWithComponent<ECS::SliderJointComponent>()) {
                    auto* j = m_World->GetComponent<ECS::SliderJointComponent>(e);
                    if (j && jointVisible(e, j->entityA, j->entityB)) drawJointLine(bgDrawList, j->entityA, j->entityB, j->anchorA, j->anchorB, IM_COL32(50, 100, 255, 180));
                }

                // Post-Process Volume wireframes (purple)
                for (ECS::Entity e : m_World->GetEntitiesWithComponent<ECS::PostProcessVolumeComponent>()) {
                    auto* vol = m_World->GetComponent<ECS::PostProcessVolumeComponent>(e);
                    auto* transform = m_World->GetComponent<ECS::TransformComponent>(e);
                    if (!vol || !vol->isActive || vol->isGlobal || !transform) continue;
                    bool sel = IsSelected(e);
                    if (!m_ShowColliderWireframes && !sel) continue;
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

                // Gravity Zone wireframes (blue-violet)
                for (ECS::Entity e : m_World->GetEntitiesWithComponent<ECS::GravityZoneComponent>()) {
                    auto* gz = m_World->GetComponent<ECS::GravityZoneComponent>(e);
                    auto* transform = m_World->GetComponent<ECS::TransformComponent>(e);
                    if (!gz || !gz->isActive || !transform) continue;
                    bool sel = IsSelected(e);
                    if (!m_ShowColliderWireframes && !sel) continue;
                    ImU32 color = sel ? IM_COL32(100, 80, 255, 200) : IM_COL32(100, 80, 255, 60);
                    f32 thick = sel ? 2.0f : 1.0f;
                    if (gz->shape == ECS::GravityZoneShape::Sphere) {
                        Math::Vector3 c = transform->position;
                        f32 r = gz->halfExtents.x;
                        drawWireCircle(bgDrawList, c, r, {1,0,0}, {0,1,0}, color, thick);
                        drawWireCircle(bgDrawList, c, r, {1,0,0}, {0,0,1}, color, thick);
                        drawWireCircle(bgDrawList, c, r, {0,1,0}, {0,0,1}, color, thick);
                    } else {
                        drawWireBox(bgDrawList, transform->position, gz->halfExtents, color, thick);
                    }
                }

                // Light range/cone wireframes (yellow/orange)
                for (ECS::Entity e : m_World->GetEntitiesWithComponent<ECS::LightComponent>()) {
                    auto* lc = m_World->GetComponent<ECS::LightComponent>(e);
                    auto* transform = m_World->GetComponent<ECS::TransformComponent>(e);
                    if (!lc || !transform) continue;
                    bool sel = IsSelected(e);
                    if (!m_ShowColliderWireframes && !sel) continue;
                    if (lc->type == ECS::LightType::Point) {
                        ImU32 color = sel ? IM_COL32(255, 220, 50, 180) : IM_COL32(255, 220, 50, 40);
                        f32 thick = sel ? 1.5f : 0.8f;
                        Math::Vector3 c = transform->position;
                        drawWireCircle(bgDrawList, c, lc->range, {1,0,0}, {0,1,0}, color, thick);
                        drawWireCircle(bgDrawList, c, lc->range, {1,0,0}, {0,0,1}, color, thick);
                        drawWireCircle(bgDrawList, c, lc->range, {0,1,0}, {0,0,1}, color, thick);
                    } else if (lc->type == ECS::LightType::Spot && sel) {
                        ImU32 color = IM_COL32(255, 180, 50, 180);
                        Math::Vector3 c = transform->position;
                        // Spot direction from rotation
                        Math::Vector3 dir(
                            2.0f * (transform->rotation.x * transform->rotation.z + transform->rotation.w * transform->rotation.y),
                            2.0f * (transform->rotation.y * transform->rotation.z - transform->rotation.w * transform->rotation.x),
                            1.0f - 2.0f * (transform->rotation.x * transform->rotation.x + transform->rotation.y * transform->rotation.y)
                        );
                        f32 outerRad = lc->range * std::tan(Math::Radians(lc->outerConeAngle));
                        Math::Vector3 tip = c + dir * lc->range;
                        // Draw 4 lines from source to cone edge
                        Math::Vector3 up(0, 1, 0);
                        if (std::abs(dir.y) > 0.99f) up = Math::Vector3(1, 0, 0);
                        Math::Vector3 right = dir.Cross(up).Normalized() * outerRad;
                        Math::Vector3 upPerp = dir.Cross(right).Normalized() * outerRad;
                        ImVec2 sp0, sp1;
                        auto tryLine = [&](Math::Vector3 offset) {
                            if (worldToScreen(c, sp0) && worldToScreen(tip + offset, sp1))
                                bgDrawList->AddLine(sp0, sp1, color, 1.5f);
                        };
                        tryLine(right); tryLine(right * -1.0f); tryLine(upPerp); tryLine(upPerp * -1.0f);
                        drawWireCircle(bgDrawList, tip, outerRad, right.Normalized(), upPerp.Normalized(), color, 1.5f);
                    }
                }

                // Audio source range wireframes (cyan)
                for (ECS::Entity e : m_World->GetEntitiesWithComponent<ECS::AudioSourceComponent>()) {
                    auto* src = m_World->GetComponent<ECS::AudioSourceComponent>(e);
                    auto* transform = m_World->GetComponent<ECS::TransformComponent>(e);
                    if (!src || !src->is3D || !transform) continue;
                    bool sel = IsSelected(e);
                    if (!m_ShowColliderWireframes && !sel) continue;
                    Math::Vector3 c = transform->position;
                    ImU32 innerColor = IM_COL32(50, 200, 255, 140);
                    ImU32 outerColor = IM_COL32(50, 200, 255, 80);
                    drawWireCircle(bgDrawList, c, src->minDistance, {1,0,0}, {0,0,1}, innerColor, 1.5f);
                    drawWireCircle(bgDrawList, c, src->maxDistance, {1,0,0}, {0,0,1}, outerColor, 1.0f);
                }

                // Trigger zone wireframes (lime green)
                for (ECS::Entity e : m_World->GetEntitiesWithComponent<ECS::TriggerZoneComponent>()) {
                    auto* tz = m_World->GetComponent<ECS::TriggerZoneComponent>(e);
                    auto* transform = m_World->GetComponent<ECS::TransformComponent>(e);
                    if (!tz || !transform) continue;
                    bool sel = IsSelected(e);
                    if (!m_ShowColliderWireframes && !sel) continue;
                    ImU32 color = sel ? IM_COL32(100, 255, 100, 180) : IM_COL32(100, 255, 100, 50);
                    f32 thick = sel ? 2.0f : 1.0f;
                    if (tz->shape == ECS::TriggerZoneComponent::Shape::Sphere) {
                        drawWireCircle(bgDrawList, transform->position, tz->sphereRadius, {1,0,0}, {0,0,1}, color, thick);
                    } else {
                        drawWireBox(bgDrawList, transform->position, tz->boxSize * 0.5f, color, thick);
                    }
                }

                // Spawn point markers (magenta cross)
                for (ECS::Entity e : m_World->GetEntitiesWithComponent<ECS::SpawnPointComponent>()) {
                    auto* transform = m_World->GetComponent<ECS::TransformComponent>(e);
                    if (!transform) continue;
                    bool sel = IsSelected(e);
                    if (!m_ShowColliderWireframes && !sel) continue;
                    ImU32 color = sel ? IM_COL32(255, 50, 200, 220) : IM_COL32(255, 50, 200, 80);
                    f32 sz = 0.5f;
                    Math::Vector3 c = transform->position;
                    ImVec2 s0, s1;
                    if (worldToScreen(c + Math::Vector3(sz,0,0), s0) && worldToScreen(c - Math::Vector3(sz,0,0), s1))
                        bgDrawList->AddLine(s0, s1, color, 2.0f);
                    if (worldToScreen(c + Math::Vector3(0,sz,0), s0) && worldToScreen(c - Math::Vector3(0,sz,0), s1))
                        bgDrawList->AddLine(s0, s1, color, 2.0f);
                    if (worldToScreen(c + Math::Vector3(0,0,sz), s0) && worldToScreen(c - Math::Vector3(0,0,sz), s1))
                        bgDrawList->AddLine(s0, s1, color, 2.0f);
                }

                // Waypoint markers (cyan dots + connecting lines).
                // Selecting ANY waypoint reveals the whole chain — a lone dot
                // without its path is meaningless.
                bool anyWaypointSelected = false;
                if (!m_ShowColliderWireframes) {
                    for (ECS::Entity e : m_World->GetEntitiesWithComponent<ECS::WaypointComponent>()) {
                        if (IsSelected(e)) { anyWaypointSelected = true; break; }
                    }
                }
                for (ECS::Entity e : m_World->GetEntitiesWithComponent<ECS::WaypointComponent>()) {
                    if (!m_ShowColliderWireframes && !anyWaypointSelected) break;
                    auto* wp = m_World->GetComponent<ECS::WaypointComponent>(e);
                    auto* transform = m_World->GetComponent<ECS::TransformComponent>(e);
                    if (!wp || !transform) continue;
                    bool sel = IsSelected(e);
                    ImU32 color = sel ? IM_COL32(50, 255, 220, 220) : IM_COL32(50, 255, 220, 100);
                    ImVec2 sp;
                    if (worldToScreen(transform->position, sp)) {
                        bgDrawList->AddCircleFilled(sp, sel ? 5.0f : 3.0f, color);
                    }
                    // Draw line to next waypoint if set
                    if (wp->nextWaypoint != 0 && m_World->IsValid(wp->nextWaypoint)) {
                        auto* nextTransform = m_World->GetComponent<ECS::TransformComponent>(wp->nextWaypoint);
                        if (nextTransform) {
                            ImVec2 sp2;
                            if (worldToScreen(transform->position, sp) && worldToScreen(nextTransform->position, sp2)) {
                                bgDrawList->AddLine(sp, sp2, IM_COL32(50, 255, 220, 120), 1.5f);
                            }
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

                        i32 selBone = animComp->selectedBoneIndex;

                        // Build parent chain set for the selected bone (highlight path to root)
                        std::unordered_set<i32> parentChain;
                        if (selBone >= 0) {
                            i32 walk = selBone;
                            while (walk >= 0) {
                                parentChain.insert(walk);
                                walk = skeleton->bones[walk].parentIndex;
                            }
                        }

                        // Hover detection: find bone nearest to mouse for tooltip
                        ImVec2 cursorPos = ImGui::GetMousePos();
                        f32 hoverBestDist = 12.0f;
                        i32 hoveredBone = -1;

                        for (usize i = 0; i < skeleton->bones.size(); ++i) {
                            const auto& bone = skeleton->bones[i];
                            Math::Matrix4 boneWorld = entityWorld * pose.worldTransforms[i];
                            Math::Vector3 bonePos(boneWorld.m[12], boneWorld.m[13], boneWorld.m[14]);

                            // Determine color and style
                            bool isSelected = (static_cast<i32>(i) == selBone);
                            bool isIKTarget = (!lookAtBone.empty() && bone.name == lookAtBone) ||
                                              (!interactionBone.empty() && bone.name == interactionBone);
                            bool inChain = parentChain.count(static_cast<i32>(i)) > 0;

                            // Color: selected=green, chain=cyan, IK=yellow, normal=white (dimmed if selection active but not in chain)
                            ImU32 boneColor;
                            if (isSelected) boneColor = Editor::Theme::BoneSelected;
                            else if (isIKTarget) boneColor = Editor::Theme::BoneIKTarget;
                            else if (inChain) boneColor = Editor::Theme::BoneChain;
                            else if (selBone >= 0) boneColor = Editor::Theme::BoneDimmed;
                            else boneColor = Editor::Theme::BoneNormal;

                            f32 lineThickness = isSelected ? 2.5f : (inChain ? 2.0f : 1.5f);

                            // Draw line from parent to this bone
                            if (bone.parentIndex >= 0 && bone.parentIndex < static_cast<i32>(skeleton->bones.size())) {
                                Math::Matrix4 parentWorld = entityWorld * pose.worldTransforms[bone.parentIndex];
                                Math::Vector3 parentPos(parentWorld.m[12], parentWorld.m[13], parentWorld.m[14]);
                                drawLine3D(bgDrawList, parentPos, bonePos, boneColor, lineThickness);
                            }

                            // Draw joint marker
                            ImVec2 jointScreen;
                            if (worldToScreen(bonePos, jointScreen)) {
                                // Hover detection for tooltip
                                f32 dx = jointScreen.x - cursorPos.x;
                                f32 dy = jointScreen.y - cursorPos.y;
                                f32 dist = std::sqrt(dx * dx + dy * dy);
                                if (dist < hoverBestDist) {
                                    hoverBestDist = dist;
                                    hoveredBone = static_cast<i32>(i);
                                }

                                if (isSelected) {
                                    // Selected bone: filled circle + outline + name label
                                    bgDrawList->AddCircleFilled(jointScreen, 6.0f, IM_COL32(50, 255, 80, 255));
                                    bgDrawList->AddCircle(jointScreen, 6.0f, IM_COL32(255, 255, 255, 220), 0, 1.5f);
                                    bgDrawList->AddText(
                                        ImVec2(jointScreen.x + 10.0f, jointScreen.y - 6.0f),
                                        IM_COL32(50, 255, 80, 255), bone.name.c_str());
                                } else {
                                    // Unselected: diamond shape (easier to see and click than tiny crosses)
                                    f32 sz = isIKTarget ? 4.5f : 3.5f;
                                    if (inChain) sz = 4.0f;
                                    bgDrawList->AddQuadFilled(
                                        ImVec2(jointScreen.x, jointScreen.y - sz),
                                        ImVec2(jointScreen.x + sz, jointScreen.y),
                                        ImVec2(jointScreen.x, jointScreen.y + sz),
                                        ImVec2(jointScreen.x - sz, jointScreen.y),
                                        boneColor);
                                }
                            }
                        }

                        // Draw tooltip for hovered bone (when not selected)
                        if (hoveredBone >= 0 && hoveredBone != selBone) {
                            const auto& hBone = skeleton->bones[hoveredBone];
                            ImVec2 tooltipPos(cursorPos.x + 14.0f, cursorPos.y - 8.0f);
                            // Background pill
                            ImVec2 textSize = ImGui::CalcTextSize(hBone.name.c_str());
                            bgDrawList->AddRectFilled(
                                ImVec2(tooltipPos.x - 4, tooltipPos.y - 2),
                                ImVec2(tooltipPos.x + textSize.x + 4, tooltipPos.y + textSize.y + 2),
                                IM_COL32(0, 0, 0, 180), 3.0f);
                            bgDrawList->AddText(tooltipPos, IM_COL32(255, 255, 255, 230), hBone.name.c_str());
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

    // Gamepad inspector overlay
    DrawGamepadInspectorOverlay();

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

    // Discord Bug Report dialog
    if (m_ShowDiscordBugDialog) {
        DrawDiscordBugReportDialog();
    }

    // New Project dialog
    if (m_ShowNewProjectDialog) {
        DrawNewProjectDialog();
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

    // Render UI canvases during play mode (editor game view). Pass the game
    // view image ORIGIN too — without it the UI laid out from the window's
    // top-left corner: drawn over the editor panels, clicks offset, and none
    // of it aligned with the game image. HUDSystem is retired: hudWidget data
    // migrates to UICanvas on load, so this is the ONE UI path. The camera
    // drives world-space elements (tags glued to entities).
    // Only when the Game View actually drew this frame - the overlay uses the
    // foreground draw list at the cached image rect, so with the Game View
    // tab hidden it would paint the game's HUD over whatever panel is docked
    // there (the VS-editor-covered-in-game-text bug).
    if (m_PlayMode.IsPlaying() && m_GameViewImageDrawnThisFrame) {
        f32 gvW = m_GameViewImageMaxX - m_GameViewImageMinX;
        f32 gvH = m_GameViewImageMaxY - m_GameViewImageMinY;
        if (gvW > 0 && gvH > 0) {
            m_UISystem.Update(m_World, gvW, gvH, m_LastDeltaTime,
                              m_GameViewImageMinX, m_GameViewImageMinY, m_Camera);
            // One flag for "the UI took the pointer", so a click on a game-view
            // UI button does not also fire in the world (matches both players).
            Input::SetUIConsumedPointer(m_UISystem.WasPointerConsumed());
            // Touch overlay simulation + controls hint over the Game View image,
            // the same drawing the exported game does (parity check for layouts).
            Input::SetTouchSimulation(m_SimulateTouch);
            Input::SetTouchSurface(m_GameViewImageMinX, m_GameViewImageMinY, gvW, gvH);
            if (!m_PlayMode.IsPaused()) {
                InputSystem::DrawTouchOverlay();
                InputSystem::DrawControlsHint(m_GameViewImageMinX, m_GameViewImageMinY, gvW, gvH);
            }
        }
    } else {
        Input::SetTouchSimulation(false);
        Input::SetUIConsumedPointer(false);
    }

    // Gameplay input belongs to the game only while it is actually playing.
    // Editing or paused reads as Menu, so a scene's actions cannot fire while
    // the user is building the level.
    Input::SetInputFocus((m_PlayMode.IsPlaying() && !m_PlayMode.IsPaused())
        ? Input::InputFocus::Gameplay
        : Input::InputFocus::Menu);
    m_GameViewImageDrawnThisFrame = false;   // re-armed by DrawGameViewPanel next frame

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

    // F1/F2 debug mode status indicator
    DrawDebugModeIndicator();

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
    if (!HasPanel(m_VisiblePanels, panel)) return false;

    // In simplified editor mode, hide advanced panels
    if (m_EditorSettings.simplifiedEditor) {
        switch (panel) {
        case EditorPanel::Profiler:
        case EditorPanel::Rendering:
        case EditorPanel::PostProcessing:
        case EditorPanel::RetroEffects:
        case EditorPanel::ParticleEditor:
        case EditorPanel::AnimGraph:
        case EditorPanel::VisualScript:
        case EditorPanel::BehaviorTree:
        case EditorPanel::QuestFlow:
        case EditorPanel::DataAssets:
        case EditorPanel::PluginBrowser:
        case EditorPanel::ProceduralGen:
        case EditorPanel::NetworkPanel:
        case EditorPanel::Collaboration:
        case EditorPanel::GitIntegration:
        case EditorPanel::SaveDebug:
            return false;
        default:
            break;
        }
    }

    return true;
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
        // Restore bone weight colors on the previously selected entity if needed
        if (m_BoneWeightEntity != ECS::INVALID_ENTITY && m_BoneWeightEntity != entity) {
            RestoreBoneWeightColors(m_BoneWeightEntity);
        }
        m_SelectedEntities.clear();
    }
    m_SelectedEntities.insert(entity);
    m_PrimarySelected = entity;
    m_HierarchyScrollToSelected = true; // reveal the row in the Hierarchy next draw
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
    // Restore bone weight colors if active
    if (m_BoneWeightEntity != ECS::INVALID_ENTITY) {
        RestoreBoneWeightColors(m_BoneWeightEntity);
    }
    bool hadSelection = !m_SelectedEntities.empty();
    m_SelectedEntities.clear();
    m_PrimarySelected = ECS::INVALID_ENTITY;
    if (hadSelection && m_Announcer.enabled) {
        m_Announcer.Announce("Selection cleared", Accessibility::AnnouncePriority::Low);
    }
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

void EditorLayer::SyncRuntimeAccessibility() {
    auto& a = m_RuntimeAccessibility;
    auto& s = m_EditorSettings;
    a.colorblindMode = static_cast<Accessibility::ColorblindMode>(s.colorblindMode);
    a.colorblindStrength = s.colorblindStrength;
    a.screenBrightness = s.screenBrightness;
    a.screenContrast = s.screenContrast;
    a.reducedMotion = s.reducedMotion;
    a.disableScreenShake = s.disableScreenShake;
    a.disableFOVEffects = s.disableFOVEffects;
    a.disableFlashingLights = s.disableFlashingLights;
    a.subtitlesEnabled = s.subtitlesEnabled;
    a.closedCaptionsEnabled = s.closedCaptionsEnabled;
    a.subtitleFontSize = s.subtitleFontSize;
    a.subtitleBgOpacity = s.subtitleBgOpacity;
    a.subtitleSpeakerNames = s.subtitleSpeakerNames;
    a.fontScale = s.gameFontScale;
    a.dyslexiaFriendly = s.dyslexiaFontEnabled;
    a.dwellClickEnabled = s.dwellClickEnabled;
    a.dwellClickTime = s.dwellClickDelay;
    a.stickyDragEnabled = s.stickyDragEnabled;
    a.switchAccessEnabled = false; // Switch access configured separately
}

// ============================================================================
// Golden-image capture (--golden)
// ============================================================================

// Export the last play session's replay (scene snapshot + input stream) to
// <project>/replays/replay_<n>.tegereplay. Plain JSON per docs/OPENNESS.md.
void EditorLayer::ExportReplayToProject() {
    if (!m_PlayMode.HasRecording()) {
        ShowNotification("Nothing recorded yet - press Play, do something, then Stop",
                         NotificationType::Warning);
        return;
    }
    std::filesystem::path root =
        std::filesystem::path(m_SceneManager.GetProjectPath()).parent_path();
    if (root.empty()) root = std::filesystem::current_path();
    std::filesystem::path dir = root / "replays";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);

    // Next free numbered slot (no wall-clock in the name: replays are data,
    // and numbered names sort naturally in the browser)
    int n = 1;
    std::filesystem::path out;
    do {
        out = dir / ("replay_" + std::to_string(n++) + ".tegereplay");
    } while (std::filesystem::exists(out) && n < 10000);

    std::ofstream f(out, std::ios::binary);
    if (!f.is_open()) {
        ENJIN_LOG_ERROR(Editor, "Replay export: cannot open %s", out.string().c_str());
        return;
    }
    std::string data = Gameplay::SerializeReplay(m_PlayMode.GetActiveRecording());
    f.write(data.data(), static_cast<std::streamsize>(data.size()));
    ENJIN_LOG_INFO(Editor, "Replay exported: %s (%zu frames, %zu bytes)",
                   out.string().c_str(), m_PlayMode.GetActiveRecording().frames.size(),
                   data.size());
    ShowNotification("Replay exported to replays/" + out.filename().string() + " (" +
                     std::to_string(m_PlayMode.GetActiveRecording().frames.size()) + " frames)",
                     NotificationType::Info);
}

// Load the newest .tegereplay from <project>/replays/, restore its scene
// snapshot, and replay the input stream deterministically.
void EditorLayer::PlayLatestReplay() {
    std::filesystem::path root =
        std::filesystem::path(m_SceneManager.GetProjectPath()).parent_path();
    if (root.empty()) root = std::filesystem::current_path();
    std::filesystem::path dir = root / "replays";
    std::filesystem::path newest;
    std::filesystem::file_time_type newestTime{};
    std::error_code ec;
    for (const auto& e : std::filesystem::directory_iterator(dir, ec)) {
        if (e.path().extension() == ".tegereplay") {
            auto wt = std::filesystem::last_write_time(e.path(), ec);
            if (newest.empty() || wt > newestTime) { newest = e.path(); newestTime = wt; }
        }
    }
    if (newest.empty()) {
        ShowNotification("No replays found in replays/ - play a session, then Export Replay",
                         NotificationType::Warning);
        return;
    }
    std::ifstream f(newest, std::ios::binary);
    std::stringstream ss;
    ss << f.rdbuf();
    Gameplay::ReplayData data;
    if (!Gameplay::ParseReplay(ss.str(), data)) {
        ShowNotification("Could not parse " + newest.filename().string(),
                         NotificationType::Error);
        return;
    }
    if (data.frames.empty()) {
        ShowNotification(newest.filename().string() + " has no recorded frames - nothing to replay",
                         NotificationType::Warning);
        return;
    }
    if (!m_PlayMode.IsStopped()) m_PlayMode.Stop();
    // Load the recorded scene so the input stream lands on identical state -
    // but FIRST preserve the real working scene. The replay snapshot replaces
    // the live world, so without this the editor's stop-restore hands back the
    // replay's scene and the user's actual scene (including unsaved work) is
    // gone - and the autosave timer would then write that over the file.
    if (!data.sceneJson.empty() && m_World) {
        Scene::SceneSerializer ser(m_World);
        {
            Scene::SerializationOptions keep;
            keep.prettyPrint = false;
            keep.includeVertexData = true;
            keep.useMeshReferences = true;
            m_PreReplaySceneJson = ser.SaveToString(keep);
        }
        auto res = ser.LoadFromString(data.sceneJson);
        if (!res.success) {
            m_PreReplaySceneJson.clear();
            ShowNotification("Replay scene failed to load: " + res.error,
                             NotificationType::Error);
            return;
        }
        ClearSelection();
    }
    ENJIN_LOG_INFO(Editor, "Replaying %s", newest.filename().string().c_str());
    ShowNotification("Replaying " + newest.filename().string(), NotificationType::Info);
    m_PlayMode.StartReplay(std::move(data));
}

void EditorLayer::WriteGoldenCapture() {
    // One shot: clear the trigger before anything can early-return
    std::string basePath = s_GoldenCapturePath;
    s_GoldenCapturePath.clear();

    CaptureGameViewToFile(basePath);

    // Done - exit so the harness can move to the next scene
    if (m_Window) m_Window->Close();
}

// MCP input injection: merge queued synthetic actions with the live hardware
// state and feed the result through the replay-injection path each frame.
// Edge semantics come free (injection keeps previous-frame bookkeeping), and
// releasing is just the action expiring - the next frame's merge no longer
// holds the key.
void EditorLayer::ProcessMcpInput(f32 deltaTime) {
    if (m_McpInputQueue.empty()) {
        if (m_McpInjecting) {
            Input::SetReplayInjection(false);
            m_McpInjecting = false;
        }
        return;
    }
    if (!m_PlayMode.IsPlaying()) {   // queued while not playing: drop stale actions
        m_McpInputQueue.clear();
        return;
    }

    bool keys[512];
    bool mouse[8];
    Math::Vector2 mpos;
    Input::CaptureFrameState(keys, mouse, mpos);

    f32 ms = deltaTime * 1000.0f;
    for (auto it = m_McpInputQueue.begin(); it != m_McpInputQueue.end();) {
        bool done = false;
        switch (it->kind) {
            case McpInputAction::Kind::Key:
                if (it->code >= 0 && it->code < 512) keys[it->code] = true;
                it->remainingMs -= ms;
                done = it->remainingMs <= 0.0f;
                break;
            case McpInputAction::Kind::Click:
                if (it->code >= 0 && it->code < 8) mouse[it->code] = true;
                mpos = Math::Vector2(it->x, it->y);
                it->remainingMs -= ms;
                done = it->remainingMs <= 0.0f;
                break;
            case McpInputAction::Kind::Text:
                // Pace characters into ImGui's queue - the same queue the
                // Input_GetTextInput binding reads (the typewriter path).
                it->charTimer -= ms;
                while (it->charTimer <= 0.0f && !it->text.empty()) {
                    unsigned char c = static_cast<unsigned char>(it->text.front());
                    it->text.erase(it->text.begin());
                    if (c >= 32 && c < 128) ImGui::GetIO().AddInputCharacter(c);
                    it->charTimer += 30.0f;   // ~33 chars/sec, human-ish
                }
                done = it->text.empty();
                break;
        }
        it = done ? m_McpInputQueue.erase(it) : it + 1;
    }

    Input::SetReplayInjection(true);
    m_McpInjecting = true;
    Input::InjectFrameState(keys, mouse, mpos);
}

// R1: start/stop game-view GIF recording. Output lands in <project>/captures/
// (falls back to CWD with no project open - the editor CWD is the exe dir).
void EditorLayer::ToggleGifRecording() {
    if (m_GifRecorder.IsRecording()) {
        u32 frames = m_GifRecorder.FrameCount();
        std::string path = m_GifRecorder.Path();
        m_GifRecorder.Stop();
        ShowNotification("GIF saved (" + std::to_string(frames) + " frames): " + path,
                         NotificationType::Success);
        return;
    }
    if (!m_GameViewRenderTarget) {
        ShowNotification("Nothing to record - no game view", NotificationType::Warning);
        return;
    }
    namespace fs = std::filesystem;
    fs::path dir;
    const std::string& manifest = m_SceneManager.GetProjectPath();
    if (!manifest.empty()) dir = fs::path(manifest).parent_path() / "captures";
    else { std::error_code ec; dir = fs::current_path(ec) / "captures"; }
    std::error_code mkEc;
    fs::create_directories(dir, mkEc);

    // Timestamped name so takes never overwrite each other.
    std::time_t now = std::time(nullptr);
    std::tm tmv{};
#ifdef _WIN32
    localtime_s(&tmv, &now);
#else
    localtime_r(&now, &tmv);
#endif
    char name[64];
    std::snprintf(name, sizeof(name), "clip_%04d%02d%02d_%02d%02d%02d.gif",
                  tmv.tm_year + 1900, tmv.tm_mon + 1, tmv.tm_mday,
                  tmv.tm_hour, tmv.tm_min, tmv.tm_sec);

    u32 shift = static_cast<u32>(std::clamp(m_GifFidelity, 0, 2));
    if (m_GifRecorder.Start((dir / name).string(),
                            m_GameViewRenderTarget->GetWidth(),
                            m_GameViewRenderTarget->GetHeight(), shift)) {
        m_GifCaptureAccum = 1e9f;   // capture the first frame immediately
        ShowNotification("Recording game view GIF - use the Tools menu to stop",
                         NotificationType::Info);
    } else {
        ShowNotification("Could not start GIF recording (file open failed)",
                         NotificationType::Error);
    }
}

// Readback the game view and write <basePath>.png/.ppm. Shared by the --golden
// probe harness (which exits afterward) and the MCP capture_view tool (which
// does not). Returns false when there is nothing to read back.
bool EditorLayer::CaptureGameViewToFile(const std::string& basePath) {
    if (!m_GameViewRenderTarget) return false;
    std::vector<u8> pixels = m_GameViewRenderTarget->CaptureToPixels();
    u32 w = m_GameViewRenderTarget->GetWidth();
    u32 h = m_GameViewRenderTarget->GetHeight();

    if (pixels.empty() || w == 0 || h == 0) {
        ENJIN_LOG_ERROR(Editor, "capture: game view readback failed (%ux%u, %zu bytes)",
                        w, h, pixels.size());
    } else {
        // PNG for human eyeballing/diffing
        std::string pngPath = basePath + ".png";
        stbi_write_png(pngPath.c_str(), static_cast<int>(w), static_cast<int>(h), 4,
                       pixels.data(), static_cast<int>(w * 4));

        // P6 PPM for the dependency-free comparer (_golden_compare.py, stdlib only)
        std::string ppmPath = basePath + ".ppm";
        std::ofstream ppm(ppmPath, std::ios::binary);
        if (ppm.is_open()) {
            ppm << "P6\n" << w << " " << h << "\n255\n";
            std::vector<u8> rgb(static_cast<usize>(w) * h * 3);
            for (usize i = 0, j = 0; i < pixels.size(); i += 4, j += 3) {
                rgb[j + 0] = pixels[i + 0];
                rgb[j + 1] = pixels[i + 1];
                rgb[j + 2] = pixels[i + 2];
            }
            ppm.write(reinterpret_cast<const char*>(rgb.data()),
                      static_cast<std::streamsize>(rgb.size()));
        }
        ENJIN_LOG_INFO(Editor, "capture: %ux%u game view -> %s(.png/.ppm)",
                       w, h, basePath.c_str());
    }
    return !pixels.empty() && w != 0 && h != 0;
}

} // namespace Editor
} // namespace Enjin
