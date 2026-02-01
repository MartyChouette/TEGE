#include "Enjin/Editor/EditorLayer.h"
#include "Enjin/Editor/ScenePicker.h"
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
#include "Enjin/ECS/Components/Text.h"
#include "Enjin/ECS/Components/IKComponents.h"
#include "Enjin/ECS/Components/Flower.h"
#include "Enjin/ECS/Components/LOD.h"
#include "Enjin/ECS/Components/Hierarchy.h"
#include "Enjin/Renderer/MeshSimplifier.h"
#include "Enjin/Renderer/Skybox.h"
#include "Enjin/ECS/Systems/RenderSystem.h"
#include "Enjin/Assets/SceneImporter.h"
#include "Enjin/Scene/SceneSerializer.h"
#include "Enjin/Renderer/MeshFactory.h"
#include "Enjin/Renderer/PostProcessing.h"
#include "Enjin/Platform/Input.h"
#include "Enjin/Platform/FileDialog.h"
#include "Enjin/Assets/Prefab.h"
#include "Enjin/Build/BuildPipeline.h"
#include "Enjin/Audio/AudioSystem.h"
#include "Enjin/Math/Math.h"
#include <imgui.h>
#include <ImGuizmo.h>
#include <vulkan/vulkan.h>
#include <sstream>
#include <filesystem>
#include <cstdio>
#include <cstring>
#include <climits>
#include <cmath>

namespace Enjin {
namespace Editor {

EditorLayer::EditorLayer() {
}

EditorLayer::~EditorLayer() {
    Shutdown();
}

bool EditorLayer::Initialize(Window* window, Renderer::VulkanRenderer* renderer) {
    m_Window = window;
    m_Renderer = renderer;

    m_ImGuiLayer = std::make_unique<GUI::ImGuiLayer>();
    if (!m_ImGuiLayer->Initialize(window, renderer)) {
        ENJIN_LOG_ERROR(Editor, "Failed to initialize ImGui layer");
        return false;
    }

    // Initialize play mode (will be fully set up when SetWorld/SetCamera are called)

    // Initialize weather system with more particles for better visibility
    m_WeatherSystem.Initialize(8000);  // Large pool for dense rain/snow

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

    // Post-processing pipeline (applies effects from scene RT to game view RT)
    if (m_GameViewRenderTarget && m_GameViewRenderTarget->IsValid()) {
        m_PostProcessing = std::make_unique<Renderer::PostProcessing>();
        if (!m_PostProcessing->Initialize(renderer->GetContext(),
                m_GameViewRenderTarget->GetRenderPass(),
                m_GameViewWidth, m_GameViewHeight)) {
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
    m_ImGuiLayer->ApplyTheme(m_EditorSettings.theme);
    m_ImGuiLayer->SetGlobalScale(m_EditorSettings.uiScale);

    // Initialize in-game pause menu system
    m_GameMenu.SetInputMap(&m_InputMap);
    m_GameMenu.SetEditorSettings(&m_EditorSettings);
    m_GameMenu.SetCallback([this](const std::string& action) {
        if (action == "resume") {
            m_GameMenu.HideAll();
            m_PlayMode.Resume();
            if (m_FocusMode) Input::SetMouseCaptured(true);
        } else if (action == "options") {
            m_GameMenu.ShowScreen(GUI::MenuScreen::Options);
        } else if (action == "how_to_play") {
            m_GameMenu.ShowScreen(GUI::MenuScreen::HowToPlay);
        } else if (action == "quit_to_menu") {
            m_GameMenu.HideAll();
            m_PlayMode.Stop();
        } else if (action == "quit") {
            if (m_Window) m_Window->Close();
        }
    });

    ENJIN_LOG_INFO(Editor, "EditorLayer initialized");
    return true;
}

void EditorLayer::InitializePlayMode() {
    if (m_World && m_Camera && m_CameraController) {
        m_PlayMode.Initialize(m_World, m_Camera, m_CameraController);

        // Wire accessibility input map and motion settings
        auto* ctrlSys = m_PlayMode.GetControllerSystem();
        if (ctrlSys) {
            ctrlSys->SetInputActionMap(&m_InputMap);
            ctrlSys->SetReducedMotion(m_EditorSettings.reducedMotion);
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

    if (m_ImGuiLayer) {
        m_ImGuiLayer->Shutdown();
        m_ImGuiLayer.reset();
    }
}

void EditorLayer::Update(f32 deltaTime) {
    // Update input action map each frame
    m_InputMap.Update(deltaTime);

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
        Editor::PerformanceStats::UpdateSystemMemory(m_PerfMetrics);
        if (m_Renderer && m_Renderer->GetContext()) {
            Editor::PerformanceStats::QueryGPUMemory(m_Renderer->GetContext(), m_PerfMetrics);
        }
        if (m_RenderSystem) {
            m_PerfMetrics.drawCallCount = m_RenderSystem->GetDrawCallCount();
            m_PerfMetrics.triangleCount = m_RenderSystem->GetTriangleCount();
        }
    }

    // Update scene transitions (fade in/out between scenes)
    m_SceneManager.UpdateTransition(deltaTime);

    // Update wind system (always ticks, affects weather + vegetation + grass)
    m_WindSystem.Update(deltaTime);
    if (m_RenderSystem && !m_RenderSystem->GetWindSystem()) {
        m_RenderSystem->SetWindSystem(&m_WindSystem);
    }

    // Camera controller handles its own input - disable during play mode, text input, or gizmo use
    if (m_CameraController) {
        bool usingGizmo = ImGuizmo::IsUsing();
        bool inPlayMode = !m_PlayMode.IsStopped();
        m_CameraController->SetEnabled(!WantsKeyboardInput() && !usingGizmo && !inPlayMode);

        // Set orbit target to selected entity position for MMB orbit
        if (m_SelectedEntity != ECS::INVALID_ENTITY && m_World) {
            auto* transform = m_World->GetComponent<ECS::TransformComponent>(m_SelectedEntity);
            if (transform) {
                m_CameraController->SetOrbitTarget(transform->position);
            }
        } else {
            // When no entity is selected, orbit around origin
            m_CameraController->SetOrbitTarget(Math::Vector3(0.0f, 0.0f, 0.0f));
        }
    }

    // Gizmo mode shortcuts (1=translate, 2=rotate, 3=scale, 4=toggle space)
    // Using number keys to avoid conflict with WASD camera movement
    // Skip during play mode so keys go to game controllers
    if (!WantsKeyboardInput() && m_PlayMode.IsStopped()) {
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

        // Delete selected entity
        if (Input::IsKeyPressed(KeyCode::Delete) && m_SelectedEntity != ECS::INVALID_ENTITY) {
            DeleteSelectedEntity();
        }

        // Duplicate selected entity (Ctrl+D)
        if (Input::IsKeyDown(KeyCode::LeftControl) && Input::IsKeyPressed(KeyCode::D)) {
            if (m_SelectedEntity != ECS::INVALID_ENTITY) {
                DuplicateEntity(m_SelectedEntity);
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
                std::string path = FileDialog::SaveFile("Save Scene", filters, "", "scene.enjin");
                if (!path.empty()) {
                    SaveScene(path);
                }
            }
        }

        // Focus on selected entity (F key)
        if (Input::IsKeyPressed(KeyCode::F) && m_SelectedEntity != ECS::INVALID_ENTITY) {
            FocusOnEntity(m_SelectedEntity);
        }
    }

    // Focus mode toggle (F11) and exit (Escape)
    // F11 toggles between editor view and fullscreen game view while playing
    if (Input::IsKeyPressed(KeyCode::F11)) {
        m_FocusMode = !m_FocusMode;
        if (m_FocusMode) {
            if (m_PlayMode.IsStopped()) {
                m_PlayMode.Play();  // Auto-play when entering focus mode
            }
            // Capture mouse for immersive gameplay (hides cursor, enables free look)
            Input::SetMouseCaptured(true);
        } else {
            // Leaving focus mode: release mouse capture
            Input::SetMouseCaptured(false);
        }
    }
    if (Input::IsKeyPressed(KeyCode::Escape)) {
        if (m_GameViewMouseCaptured || (m_FocusMode && Input::IsMouseCaptured())) {
            // First ESC: release mouse capture so cursor is visible for menu
            m_GameViewMouseCaptured = false;
            Input::SetMouseCaptured(false);
        } else if (m_GameMenu.IsMenuOpen()) {
            // Menu is open: close it and resume the game
            m_GameMenu.HideAll();
            m_PlayMode.Resume();
            if (m_FocusMode) Input::SetMouseCaptured(true);
        } else if (m_PlayMode.IsPlaying()) {
            // Playing (focus or editor): open pause menu and pause game
            m_GameMenu.ShowScreen(GUI::MenuScreen::PauseMenu);
            m_PlayMode.Pause();
            if (m_FocusMode) Input::SetMouseCaptured(false);
        } else if (m_PlayMode.IsPaused() && !m_GameMenu.IsMenuOpen()) {
            // Paused without menu (fallback): stop play mode
            m_PlayMode.Stop();
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

    // Handle viewport picking (left-click to select, but not when using gizmo)
    // Only allow picking in editor mode, not play mode
    if (!ImGuizmo::IsOver() && m_PlayMode.IsStopped()) {
        HandleViewportPicking();
    }

    // Pass game view bounds and render system to FlowerSystem each frame
    {
        auto* flowerSys = m_PlayMode.GetFlowerSystem();
        flowerSys->SetGameViewBounds(m_GameViewImageMinX, m_GameViewImageMinY,
                                     m_GameViewImageMaxX, m_GameViewImageMaxY);
        flowerSys->SetRenderTargetSize(m_GameViewWidth, m_GameViewHeight);
        flowerSys->SetRenderSystem(m_RenderSystem);
        flowerSys->SetGameCameraEntity(m_SelectedGameCamera);
    }

    // Update play mode
    m_PlayMode.Update(deltaTime);

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
}

void EditorLayer::RenderOffscreen(VkCommandBuffer commandBuffer) {
    if (!m_GameViewRenderTarget || !m_GameViewRenderTarget->IsValid()) {
        return;
    }

    // In focus mode, render at full display resolution
    if (m_FocusMode) {
        ImGuiIO& io = ImGui::GetIO();
        m_GameViewWidth = static_cast<u32>(io.DisplaySize.x);
        m_GameViewHeight = static_cast<u32>(io.DisplaySize.y);
    }

    // Resize render targets if game view panel dimensions changed
    if (m_GameViewRenderTarget->GetWidth() != m_GameViewWidth ||
        m_GameViewRenderTarget->GetHeight() != m_GameViewHeight) {
        if (m_GameViewWidth > 0 && m_GameViewHeight > 0) {
            if (m_SceneRenderTarget) {
                m_SceneRenderTarget->Resize(m_GameViewWidth, m_GameViewHeight);
            }
            m_GameViewRenderTarget->Resize(m_GameViewWidth, m_GameViewHeight);

            // Determine which render pass to use for effect pipelines
            VkRenderPass effectRenderPass = (m_SceneRenderTarget && m_SceneRenderTarget->IsValid())
                ? m_SceneRenderTarget->GetRenderPass()
                : m_GameViewRenderTarget->GetRenderPass();

            if (m_RenderSystem) {
                m_RenderSystem->RecreateEffectPipelinesForRenderPass(effectRenderPass);
            }

            // Update post-processing: rebind source image and resize
            if (m_PostProcessing && m_SceneRenderTarget && m_SceneRenderTarget->IsValid()) {
                m_PostProcessing->OnResize(m_GameViewWidth, m_GameViewHeight);
                m_PostProcessing->UpdateSourceImage(
                    m_SceneRenderTarget->GetColorImageView(),
                    m_SceneRenderTarget->GetSampler());
            }
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
        // Find player entity (first entity with any CharacterController component)
        ECS::Entity playerEntity = ECS::INVALID_ENTITY;
        for (ECS::Entity entity : m_World->GetAllEntities()) {
            if (m_World->HasComponent<ECS::Platformer2DController>(entity) ||
                m_World->HasComponent<ECS::TopDown2DController>(entity) ||
                m_World->HasComponent<ECS::TopDown3DController>(entity) ||
                m_World->HasComponent<ECS::ThirdPersonController>(entity) ||
                m_World->HasComponent<ECS::FirstPersonController>(entity)) {
                playerEntity = entity;
                break;
            }
        }

        if (playerEntity != ECS::INVALID_ENTITY) {
            auto* playerTransform = m_World->GetComponent<ECS::TransformComponent>(playerEntity);
            if (playerTransform) {
                i32 bestCamPriority = INT_MIN;
                for (ECS::Entity entity : m_World->GetAllEntities()) {
                    if (!m_World->HasComponent<ECS::CameraTriggerComponent>(entity)) continue;
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

    for (ECS::Entity entity : m_World->GetAllEntities()) {
        if (m_World->HasComponent<ECS::WeatherZoneComponent>(entity)) {
            auto* zone = m_World->GetComponent<ECS::WeatherZoneComponent>(entity);
            auto* zoneTransform = m_World->GetComponent<ECS::TransformComponent>(entity);
            if (zone && zoneTransform && zone->priority > bestWeatherPriority) {
                if (zone->ContainsPoint(zoneTransform->position, cameraTransform->position)) {
                    activeWeatherZone = zone;
                    bestWeatherPriority = zone->priority;
                }
            }
        }
    }

    // Find active temperature zone containing the game camera
    ECS::TemperatureZoneComponent* activeTempZone = nullptr;
    i32 bestTempPriority = INT_MIN;

    for (ECS::Entity entity : m_World->GetAllEntities()) {
        if (m_World->HasComponent<ECS::TemperatureZoneComponent>(entity)) {
            auto* zone = m_World->GetComponent<ECS::TemperatureZoneComponent>(entity);
            auto* zoneTransform = m_World->GetComponent<ECS::TransformComponent>(entity);
            if (zone && zoneTransform && zone->priority > bestTempPriority) {
                if (zone->ContainsPoint(zoneTransform->position, cameraTransform->position)) {
                    activeTempZone = zone;
                    bestTempPriority = zone->priority;
                }
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
    for (ECS::Entity waterEntity : m_World->GetAllEntities()) {
        if (!m_World->HasComponent<ECS::WaterVolumeComponent>(waterEntity)) continue;
        auto* waterVol = m_World->GetComponent<ECS::WaterVolumeComponent>(waterEntity);
        auto* waterTransform = m_World->GetComponent<ECS::TransformComponent>(waterEntity);
        if (!waterVol || !waterTransform) continue;

        // Find highest-priority temperature zone containing this water entity
        ECS::TemperatureZoneComponent* waterTempZone = nullptr;
        i32 bestWaterTempPri = INT_MIN;
        for (ECS::Entity tzEntity : m_World->GetAllEntities()) {
            if (!m_World->HasComponent<ECS::TemperatureZoneComponent>(tzEntity)) continue;
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
        for (ECS::Entity entity : m_World->GetAllEntities()) {
            if (m_World->HasComponent<ECS::LightComponent>(entity)) {
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

    u32 rtWidth = m_GameViewRenderTarget->GetWidth();
    u32 rtHeight = m_GameViewRenderTarget->GetHeight();

    bool usePostProcessing = m_PostProcessing && m_PostProcessing->IsInitialized() &&
                             m_SceneRenderTarget && m_SceneRenderTarget->IsValid();

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

    // Render scene + effects into the chosen target
    sceneTarget->Begin(commandBuffer);
    if (useSplitscreen && !splitViewports.empty()) {
        m_RenderSystem->RenderSplitscreen(sceneTarget, splitViewports);
        if (hasWeatherParticles) {
            m_RenderSystem->RenderWeatherParticles(m_WeatherSystem, isRain, rtWidth, rtHeight);
        }
    } else {
        m_RenderSystem->RenderToTarget(sceneTarget, &gameCamera);
        m_RenderSystem->RenderGrass(rtWidth, rtHeight);
        m_RenderSystem->RenderShrubs(rtWidth, rtHeight);
        m_RenderSystem->RenderTrees(rtWidth, rtHeight);
        if (hasWeatherParticles) {
            m_RenderSystem->RenderWeatherParticles(m_WeatherSystem, isRain, rtWidth, rtHeight);
        }
    }
    sceneTarget->End(commandBuffer);

    // Apply post-processing: read from scene RT, write to game view RT
    if (usePostProcessing) {
        m_GameViewRenderTarget->Begin(commandBuffer);
        m_PostProcessing->ApplyToCurrentPass(commandBuffer, rtWidth, rtHeight);
        m_GameViewRenderTarget->End(commandBuffer);
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
    if (m_ShowTemplateSelector) {
        DrawTemplateSelector();
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

        ImGui::End();

        // Render dialogue overlay on top of fullscreen game view
        if (m_PlayMode.IsPlaying() || m_PlayMode.IsPaused()) {
            DrawDialogueOverlay();
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
    f32 menuBarH = 22.0f;
    f32 leftW = screenW * m_Layout.leftWidth;
    f32 rightW = screenW * m_Layout.rightWidth;
    f32 bottomH = screenH * m_Layout.bottomHeight;
    f32 centerW = screenW - leftW - rightW;
    f32 centerH = screenH - menuBarH - bottomH;

    // When force layout is set (after template change), override positions once
    ImGuiCond layoutCond = m_ForceLayout ? ImGuiCond_Always : ImGuiCond_FirstUseEver;

    // Game View position/size (auto-compute if -1)
    f32 gvX = m_Layout.gameViewX >= 0 ? m_Layout.gameViewX : (leftW + 20.0f);
    f32 gvY = m_Layout.gameViewY >= 0 ? m_Layout.gameViewY : (menuBarH + 20.0f);
    f32 gvW = m_Layout.gameViewW;
    f32 gvH = m_Layout.gameViewH;

    // Panels with layout-driven positions
    if (HasPanel(m_VisiblePanels, EditorPanel::Hierarchy)) {
        ImGui::SetNextWindowPos(ImVec2(0, menuBarH), layoutCond);
        ImGui::SetNextWindowSize(ImVec2(leftW, centerH), layoutCond);
        DrawHierarchyPanel();
    }
    if (HasPanel(m_VisiblePanels, EditorPanel::Inspector)) {
        ImGui::SetNextWindowPos(ImVec2(screenW - rightW, menuBarH), layoutCond);
        ImGui::SetNextWindowSize(ImVec2(rightW, centerH * m_Layout.inspectorSplit), layoutCond);
        DrawInspectorPanel();
    }
    if (HasPanel(m_VisiblePanels, EditorPanel::Settings)) {
        ImGui::SetNextWindowPos(ImVec2(screenW - rightW, menuBarH + centerH * m_Layout.inspectorSplit), layoutCond);
        ImGui::SetNextWindowSize(ImVec2(rightW, centerH * (1.0f - m_Layout.inspectorSplit)), layoutCond);
        DrawSettingsPanel();
    }
    if (HasPanel(m_VisiblePanels, EditorPanel::Console)) {
        ImGui::SetNextWindowPos(ImVec2(leftW, screenH - bottomH), layoutCond);
        ImGui::SetNextWindowSize(ImVec2(centerW * 0.5f, bottomH), layoutCond);
        DrawConsolePanel();
    }
    if (HasPanel(m_VisiblePanels, EditorPanel::AssetBrowser)) {
        ImGui::SetNextWindowPos(ImVec2(leftW + centerW * 0.5f, screenH - bottomH), layoutCond);
        ImGui::SetNextWindowSize(ImVec2(centerW * 0.5f, bottomH), layoutCond);
        DrawAssetBrowserPanel();
    }
    if (HasPanel(m_VisiblePanels, EditorPanel::PostProcessing)) {
        ImGui::SetNextWindowPos(ImVec2(screenW - rightW - 300, menuBarH + 50), layoutCond);
        ImGui::SetNextWindowSize(ImVec2(280, 400), layoutCond);
        DrawPostProcessingPanel();
    }
    if (HasPanel(m_VisiblePanels, EditorPanel::Effects)) {
        ImGui::SetNextWindowPos(ImVec2(screenW - rightW - 300, menuBarH + 100), layoutCond);
        ImGui::SetNextWindowSize(ImVec2(280, 450), layoutCond);
        DrawEffectsPanel();
    }
    if (HasPanel(m_VisiblePanels, EditorPanel::GameView)) {
        ImGui::SetNextWindowPos(ImVec2(gvX, gvY), layoutCond);
        ImGui::SetNextWindowSize(ImVec2(gvW, gvH), layoutCond);
        DrawGameViewPanel();
    }
    if (HasPanel(m_VisiblePanels, EditorPanel::SceneList)) {
        ImGui::SetNextWindowPos(ImVec2(0, screenH - bottomH), layoutCond);
        ImGui::SetNextWindowSize(ImVec2(leftW, bottomH), layoutCond);
        DrawSceneListPanel();
    }
    if (HasPanel(m_VisiblePanels, EditorPanel::Skybox)) {
        ImGui::SetNextWindowPos(ImVec2(leftW + 20, menuBarH + 440), layoutCond);
        ImGui::SetNextWindowSize(ImVec2(300, 400), layoutCond);
        DrawSkyboxPanel();
    }

    // Clear the force flag after one frame
    if (m_ForceLayout) m_ForceLayout = false;

    // Draw scene transition overlay (fade to/from black/white)
    if (m_SceneManager.IsTransitioning()) {
        f32 alpha = m_SceneManager.GetTransitionAlpha();
        ImU32 color = IM_COL32(0, 0, 0, static_cast<u8>(alpha * 255));
        ImGui::GetForegroundDrawList()->AddRectFilled(
            ImVec2(0, 0), io.DisplaySize, color);
    }

    // Draw gizmos for selected entity
    DrawGizmos();

    // Draw grid overlay
    if (m_ShowGrid) {
        DrawGrid();
    }

    // Draw camera frustum for all camera entities (or selected camera)
    if (m_World) {
        for (ECS::Entity entity : m_World->GetAllEntities()) {
            if (m_World->HasComponent<ECS::CameraComponent>(entity)) {
                DrawCameraFrustum(entity);
            }
        }
    }

    // Draw wireframe bounding boxes for weather zones and water volumes
    if (m_World && m_Camera && m_Renderer) {
        auto extent = m_Renderer->GetSwapchainExtent();
        if (extent.width > 0 && extent.height > 0) {
            Math::Matrix4 viewMat = m_Camera->GetViewMatrix();
            Math::Matrix4 projMat = m_Camera->GetProjectionMatrix();
            Math::Matrix4 viewProj = projMat * viewMat;
            f32 sw = static_cast<f32>(extent.width);
            f32 sh = static_cast<f32>(extent.height);

            auto worldToScreen = [&](const Math::Vector3& worldPos, ImVec2& screenPos) -> bool {
                Math::Vector4 clipPos = viewProj * Math::Vector4(worldPos.x, worldPos.y, worldPos.z, 1.0f);
                if (clipPos.w <= 0.001f) return false;
                f32 ndcX = clipPos.x / clipPos.w;
                f32 ndcY = clipPos.y / clipPos.w;
                f32 ndcZ = clipPos.z / clipPos.w;
                if (ndcZ < 0.0f || ndcZ > 1.0f) return false;
                screenPos.x = (ndcX + 1.0f) * 0.5f * sw;
                screenPos.y = (ndcY + 1.0f) * 0.5f * sh;
                return true;
            };

            auto drawLine3D = [&](ImDrawList* dl, const Math::Vector3& from, const Math::Vector3& to, ImU32 color, f32 thickness) {
                ImVec2 screenFrom, screenTo;
                if (worldToScreen(from, screenFrom) && worldToScreen(to, screenTo)) {
                    dl->AddLine(screenFrom, screenTo, color, thickness);
                }
            };

            auto drawWireBox = [&](ImDrawList* dl, const Math::Vector3& center, const Math::Vector3& halfExt, ImU32 color, f32 thickness) {
                // 8 corners of the AABB
                Math::Vector3 corners[8] = {
                    center + Math::Vector3(-halfExt.x, -halfExt.y, -halfExt.z),
                    center + Math::Vector3( halfExt.x, -halfExt.y, -halfExt.z),
                    center + Math::Vector3( halfExt.x, -halfExt.y,  halfExt.z),
                    center + Math::Vector3(-halfExt.x, -halfExt.y,  halfExt.z),
                    center + Math::Vector3(-halfExt.x,  halfExt.y, -halfExt.z),
                    center + Math::Vector3( halfExt.x,  halfExt.y, -halfExt.z),
                    center + Math::Vector3( halfExt.x,  halfExt.y,  halfExt.z),
                    center + Math::Vector3(-halfExt.x,  halfExt.y,  halfExt.z),
                };
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

            ImDrawList* bgDrawList = ImGui::GetBackgroundDrawList();

            for (ECS::Entity entity : m_World->GetAllEntities()) {
                // Weather zone wireframe (light blue)
                if (m_World->HasComponent<ECS::WeatherZoneComponent>(entity)) {
                    auto* zone = m_World->GetComponent<ECS::WeatherZoneComponent>(entity);
                    auto* transform = m_World->GetComponent<ECS::TransformComponent>(entity);
                    if (zone && transform) {
                        bool isSelected = (entity == m_SelectedEntity);
                        ImU32 color = isSelected ? IM_COL32(100, 180, 255, 200) : IM_COL32(100, 180, 255, 80);
                        f32 thickness = isSelected ? 2.0f : 1.0f;
                        drawWireBox(bgDrawList, transform->position, zone->halfExtents, color, thickness);
                    }
                }
                // Water volume wireframe (cyan/teal)
                if (m_World->HasComponent<ECS::WaterVolumeComponent>(entity)) {
                    auto* volume = m_World->GetComponent<ECS::WaterVolumeComponent>(entity);
                    auto* transform = m_World->GetComponent<ECS::TransformComponent>(entity);
                    if (volume && transform) {
                        bool isSelected = (entity == m_SelectedEntity);
                        ImU32 color = isSelected ? IM_COL32(50, 220, 200, 200) : IM_COL32(50, 220, 200, 80);
                        f32 thickness = isSelected ? 2.0f : 1.0f;
                        drawWireBox(bgDrawList, transform->position, volume->halfExtents, color, thickness);
                    }
                }
            }
        }
    }

    // Stats overlay
    if (m_ShowStatsOverlay) {
        DrawStatsOverlay();
    }

    // Demo window (for testing)
    if (m_ShowDemoWindow) {
        ImGui::ShowDemoWindow(&m_ShowDemoWindow);
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

    // Render pause menu overlay on top of editor panels
    if (m_GameMenu.IsMenuOpen()) {
        m_GameMenu.Render(io.DisplaySize.x, io.DisplaySize.y);
    }

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

void EditorLayer::DrawMenuBar() {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New Scene", "Ctrl+N")) {
                if (m_World) {
                    m_World->Clear();
                    m_SelectedEntity = ECS::INVALID_ENTITY;
                    m_CurrentScenePath.clear();
                    m_ShowTemplateSelector = true;
                    ENJIN_LOG_INFO(Editor, "Created new scene");
                }
            }
            if (ImGui::MenuItem("Open Scene...", "Ctrl+O")) {
                std::vector<FileFilter> filters = {
                    { "Enjin Scene", "*.enjin" },
                    { "All Files", "*.*" }
                };
                std::string path = FileDialog::OpenFile("Open Scene", filters);
                if (!path.empty()) {
                    OpenScene(path);
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
                    std::string path = FileDialog::SaveFile("Save Scene", filters, "", "scene.enjin");
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
                std::string path = FileDialog::SaveFile("Save Scene As", filters, "", defaultName);
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
                m_SceneManager.NewProject("Untitled Project");
                if (m_World) {
                    m_World->Clear();
                    m_SelectedEntity = ECS::INVALID_ENTITY;
                    m_CurrentScenePath.clear();
                    m_ShowTemplateSelector = true;
                }
                ENJIN_LOG_INFO(Editor, "Created new project");
            }
            if (ImGui::MenuItem("Open Project...")) {
                std::vector<FileFilter> filters = {
                    { "Enjin Project", "*.enjinproject" },
                    { "All Files", "*.*" }
                };
                std::string path = FileDialog::OpenFile("Open Project", filters);
                if (!path.empty()) {
                    if (m_SceneManager.LoadProject(path)) {
                        ENJIN_LOG_INFO(Editor, "Loaded project: %s", m_SceneManager.GetProjectName().c_str());
                        // Auto-load start scene if available
                        if (m_SceneManager.GetSceneCount() > 0) {
                            m_SceneManager.LoadStartScene();
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
                        m_SceneManager.SaveProject(path);
                    }
                } else {
                    m_SceneManager.SaveProject();
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
                    m_SceneManager.SaveProject(path);
                }
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Import Model...", "Ctrl+I")) {
                std::vector<FileFilter> filters = {
                    { "glTF Files", "*.gltf;*.glb" },
                    { "All Files", "*.*" }
                };
                std::string path = FileDialog::OpenFile("Import Model", filters);
                if (!path.empty()) {
                    ImportModel(path);
                }
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
            ImGui::Separator();
            if (ImGui::MenuItem("Exit", "Alt+F4")) {
                if (m_Window) {
                    m_Window->Close();
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
            bool hasSelection = m_SelectedEntity != ECS::INVALID_ENTITY && m_World;
            if (ImGui::MenuItem("Cut", "Ctrl+X", false, hasSelection)) {
                Scene::SceneSerializer serializer(m_World);
                Scene::SerializationOptions opts;
                opts.includeVertexData = true;
                m_ClipboardEntityJson = serializer.SaveToString(opts);
                m_ClipboardIsCut = true;
                m_ClipboardSourceEntity = m_SelectedEntity;
                DeleteSelectedEntity();
            }
            if (ImGui::MenuItem("Copy", "Ctrl+C", false, hasSelection)) {
                Scene::SceneSerializer serializer(m_World);
                Scene::SerializationOptions opts;
                opts.includeVertexData = true;
                m_ClipboardEntityJson = serializer.SaveToString(opts);
                m_ClipboardIsCut = false;
                m_ClipboardSourceEntity = m_SelectedEntity;
            }
            if (ImGui::MenuItem("Paste", "Ctrl+V", false, !m_ClipboardEntityJson.empty())) {
                Scene::SceneSerializer serializer(m_World);
                auto result = serializer.LoadFromString(m_ClipboardEntityJson, false);
                if (result.success && result.rootEntity != ECS::INVALID_ENTITY) {
                    m_SelectedEntity = result.rootEntity;
                }
                if (m_ClipboardIsCut) {
                    m_ClipboardEntityJson.clear();
                    m_ClipboardIsCut = false;
                }
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View")) {
            bool hierarchy = IsPanelVisible(EditorPanel::Hierarchy);
            bool inspector = IsPanelVisible(EditorPanel::Inspector);
            bool console = IsPanelVisible(EditorPanel::Console);
            bool assets = IsPanelVisible(EditorPanel::AssetBrowser);
            bool settings = IsPanelVisible(EditorPanel::Settings);
            bool postProcessing = IsPanelVisible(EditorPanel::PostProcessing);
            bool effects = IsPanelVisible(EditorPanel::Effects);
            bool gameView = IsPanelVisible(EditorPanel::GameView);

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
            if (ImGui::MenuItem("Settings", nullptr, &settings)) {
                SetPanelVisibility(EditorPanel::Settings, settings);
            }
            if (ImGui::MenuItem("Post Processing", nullptr, &postProcessing)) {
                SetPanelVisibility(EditorPanel::PostProcessing, postProcessing);
            }
            if (ImGui::MenuItem("Effects (Retro)", nullptr, &effects)) {
                SetPanelVisibility(EditorPanel::Effects, effects);
            }
            if (ImGui::MenuItem("Game View", nullptr, &gameView)) {
                SetPanelVisibility(EditorPanel::GameView, gameView);
            }
            bool sceneList = IsPanelVisible(EditorPanel::SceneList);
            if (ImGui::MenuItem("Scene List", nullptr, &sceneList)) {
                SetPanelVisibility(EditorPanel::SceneList, sceneList);
            }
            bool skybox = IsPanelVisible(EditorPanel::Skybox);
            if (ImGui::MenuItem("Skybox", nullptr, &skybox)) {
                SetPanelVisibility(EditorPanel::Skybox, skybox);
            }
            ImGui::Separator();
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
                    m_SelectedEntity = entity;
                }
            }
            if (ImGui::BeginMenu("3D Object")) {
                if (ImGui::MenuItem("Cube")) {
                    if (m_World) {
                        ECS::Entity entity = m_World->CreateEntity();
                        m_World->AddComponent<ECS::TransformComponent>(entity);
                        m_World->AddComponent<ECS::MeshComponent>(entity, Renderer::MeshFactory::CreateCube(1.0f));
                        m_SelectedEntity = entity;
                    }
                }
                if (ImGui::MenuItem("Sphere")) {
                    if (m_World) {
                        ECS::Entity entity = m_World->CreateEntity();
                        m_World->AddComponent<ECS::TransformComponent>(entity);
                        m_World->AddComponent<ECS::MeshComponent>(entity, Renderer::MeshFactory::CreateSphere(0.5f));
                        m_SelectedEntity = entity;
                    }
                }
                if (ImGui::MenuItem("Plane")) {
                    if (m_World) {
                        ECS::Entity entity = m_World->CreateEntity();
                        m_World->AddComponent<ECS::TransformComponent>(entity);
                        m_World->AddComponent<ECS::MeshComponent>(entity, Renderer::MeshFactory::CreatePlane(10.0f, 10.0f));
                        m_SelectedEntity = entity;
                    }
                }
                if (ImGui::MenuItem("Cylinder")) {
                    if (m_World) {
                        ECS::Entity entity = m_World->CreateEntity();
                        m_World->AddComponent<ECS::TransformComponent>(entity);
                        m_World->AddComponent<ECS::MeshComponent>(entity, Renderer::MeshFactory::CreateCylinder(0.5f, 1.0f));
                        m_SelectedEntity = entity;
                    }
                }
                if (ImGui::MenuItem("Cone")) {
                    if (m_World) {
                        ECS::Entity entity = m_World->CreateEntity();
                        m_World->AddComponent<ECS::TransformComponent>(entity);
                        m_World->AddComponent<ECS::MeshComponent>(entity, Renderer::MeshFactory::CreateCone(0.5f, 1.0f));
                        m_SelectedEntity = entity;
                    }
                }
                if (ImGui::MenuItem("Capsule")) {
                    if (m_World) {
                        ECS::Entity entity = m_World->CreateEntity();
                        m_World->AddComponent<ECS::TransformComponent>(entity);
                        m_World->AddComponent<ECS::MeshComponent>(entity, Renderer::MeshFactory::CreateCapsule(0.3f, 1.0f));
                        m_SelectedEntity = entity;
                    }
                }
                if (ImGui::MenuItem("Pyramid")) {
                    if (m_World) {
                        ECS::Entity entity = m_World->CreateEntity();
                        m_World->AddComponent<ECS::TransformComponent>(entity);
                        m_World->AddComponent<ECS::MeshComponent>(entity, Renderer::MeshFactory::CreatePyramid(1.0f, 1.0f));
                        m_SelectedEntity = entity;
                    }
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("2D Object")) {
                if (ImGui::MenuItem("Triangle")) {
                    if (m_World) {
                        ECS::Entity entity = m_World->CreateEntity();
                        m_World->AddComponent<ECS::TransformComponent>(entity);
                        m_World->AddComponent<ECS::MeshComponent>(entity, Renderer::MeshFactory::CreateTriangle(1.0f));
                        m_World->AddComponent<ECS::NameComponent>(entity, "Triangle");
                        m_SelectedEntity = entity;
                    }
                }
                if (ImGui::MenuItem("Quad")) {
                    if (m_World) {
                        ECS::Entity entity = m_World->CreateEntity();
                        m_World->AddComponent<ECS::TransformComponent>(entity);
                        m_World->AddComponent<ECS::MeshComponent>(entity, Renderer::MeshFactory::CreateQuad(1.0f, 1.0f));
                        m_World->AddComponent<ECS::NameComponent>(entity, "Quad");
                        m_SelectedEntity = entity;
                    }
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
                        m_SelectedEntity = entity;
                    }
                }
                if (ImGui::MenuItem("Circle")) {
                    if (m_World) {
                        ECS::Entity entity = m_World->CreateEntity();
                        auto& transform = m_World->AddComponent<ECS::TransformComponent>(entity);
                        transform.rotation = Math::Quaternion(Math::Vector3(1, 0, 0), Math::Radians(-90.0f));  // Flat on XZ by default
                        // Use a highly tessellated plane to approximate a circle (flat disc)
                        m_World->AddComponent<ECS::MeshComponent>(entity, Renderer::MeshFactory::CreateSphere(0.5f, 32, 1));
                        m_World->AddComponent<ECS::NameComponent>(entity, "Circle");
                        m_SelectedEntity = entity;
                    }
                }
                if (ImGui::MenuItem("Capsule 2D")) {
                    if (m_World) {
                        ECS::Entity entity = m_World->CreateEntity();
                        m_World->AddComponent<ECS::TransformComponent>(entity);
                        m_World->AddComponent<ECS::MeshComponent>(entity, Renderer::MeshFactory::CreateCapsule2D(0.8f, 1.6f));
                        m_World->AddComponent<ECS::NameComponent>(entity, "Capsule 2D");
                        m_SelectedEntity = entity;
                    }
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
                        m_SelectedEntity = entity;
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
                        m_SelectedEntity = entity;
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
                    m_SelectedEntity = entity;
                }
            }
            ImGui::Separator();
            if (ImGui::BeginMenu("Light")) {
                if (ImGui::MenuItem("Directional Light")) {
                    if (m_World) {
                        ECS::Entity entity = m_World->CreateEntity();
                        auto& transform = m_World->AddComponent<ECS::TransformComponent>(entity);
                        transform.rotation = Math::Quaternion(Math::Vector3(1, 0, 0), Math::Radians(-45.0f));
                        auto& light = m_World->AddComponent<ECS::LightComponent>(entity);
                        light.type = ECS::LightType::Directional;
                        light.intensity = 1.0f;
                        m_SelectedEntity = entity;
                    }
                }
                if (ImGui::MenuItem("Point Light")) {
                    if (m_World) {
                        ECS::Entity entity = m_World->CreateEntity();
                        auto& transform = m_World->AddComponent<ECS::TransformComponent>(entity);
                        transform.position = Math::Vector3(0, 2, 0);
                        auto& light = m_World->AddComponent<ECS::LightComponent>(entity);
                        light.type = ECS::LightType::Point;
                        m_SelectedEntity = entity;
                    }
                }
                if (ImGui::MenuItem("Spot Light")) {
                    if (m_World) {
                        ECS::Entity entity = m_World->CreateEntity();
                        auto& transform = m_World->AddComponent<ECS::TransformComponent>(entity);
                        transform.position = Math::Vector3(0, 3, 0);
                        auto& light = m_World->AddComponent<ECS::LightComponent>(entity);
                        light.type = ECS::LightType::Spot;
                        m_SelectedEntity = entity;
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
                        m_SelectedEntity = entity;
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
                        m_SelectedEntity = entity;
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
                        m_SelectedEntity = entity;
                    }
                }
                if (ImGui::MenuItem("Water Volume")) {
                    if (m_World) {
                        ECS::Entity entity = m_World->CreateEntity();
                        m_World->AddComponent<ECS::TransformComponent>(entity);
                        m_World->AddComponent<ECS::WaterVolumeComponent>(entity);
                        m_World->AddComponent<ECS::NameComponent>(entity, "Water Volume");
                        m_SelectedEntity = entity;
                    }
                }
                if (ImGui::MenuItem("Grass Volume")) {
                    if (m_World) {
                        ECS::Entity entity = m_World->CreateEntity();
                        m_World->AddComponent<ECS::TransformComponent>(entity);
                        m_World->AddComponent<ECS::GrassVolumeComponent>(entity);
                        m_World->AddComponent<ECS::NameComponent>(entity, "Grass Volume");
                        m_SelectedEntity = entity;
                    }
                }
                if (ImGui::MenuItem("Shrub Volume")) {
                    if (m_World) {
                        ECS::Entity entity = m_World->CreateEntity();
                        m_World->AddComponent<ECS::TransformComponent>(entity);
                        m_World->AddComponent<ECS::ShrubVolumeComponent>(entity);
                        m_World->AddComponent<ECS::NameComponent>(entity, "Shrub Volume");
                        m_SelectedEntity = entity;
                    }
                }
                if (ImGui::MenuItem("Tree Volume")) {
                    if (m_World) {
                        ECS::Entity entity = m_World->CreateEntity();
                        m_World->AddComponent<ECS::TransformComponent>(entity);
                        m_World->AddComponent<ECS::TreeVolumeComponent>(entity);
                        m_World->AddComponent<ECS::NameComponent>(entity, "Tree Volume");
                        m_SelectedEntity = entity;
                    }
                }
                if (ImGui::MenuItem("Camera Trigger")) {
                    if (m_World) {
                        ECS::Entity entity = m_World->CreateEntity();
                        m_World->AddComponent<ECS::TransformComponent>(entity);
                        m_World->AddComponent<ECS::CameraTriggerComponent>(entity);
                        m_World->AddComponent<ECS::NameComponent>(entity, "Camera Trigger");
                        m_SelectedEntity = entity;
                    }
                }
                if (ImGui::MenuItem("Temperature Zone")) {
                    if (m_World) {
                        ECS::Entity entity = m_World->CreateEntity();
                        m_World->AddComponent<ECS::TransformComponent>(entity);
                        m_World->AddComponent<ECS::TemperatureZoneComponent>(entity);
                        m_World->AddComponent<ECS::NameComponent>(entity, "Temperature Zone");
                        m_SelectedEntity = entity;
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
                    m_SelectedEntity = entity;
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
                    m_SelectedEntity = entity;
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
                        m_SelectedEntity = entity;
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
                        m_SelectedEntity = entity;
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
                        m_SelectedEntity = entity;
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
                        m_SelectedEntity = entity;
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
                        m_SelectedEntity = entity;
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
                        m_SelectedEntity = entity;
                    }
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Help")) {
            if (ImGui::MenuItem("About Enjin")) {
                m_ShowAboutDialog = true;
            }
            ImGui::EndMenu();
        }

        // Play mode controls (centered in menu bar)
        ImGui::Separator();

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
            if (ImGui::Button(" > Play ")) {
                m_PlayMode.Play();
            }
        } else if (m_PlayMode.IsPlaying()) {
            if (ImGui::Button(" || Pause ")) {
                m_PlayMode.Pause();
            }
        } else {  // Paused
            if (ImGui::Button(" > Resume ")) {
                m_PlayMode.Resume();
            }
        }
        ImGui::PopStyleColor();

        // Stop button (only when playing or paused)
        if (!m_PlayMode.IsStopped()) {
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.2f, 0.2f, 1.0f));
            if (ImGui::Button(" [] Stop ")) {
                m_PlayMode.Stop();
                m_FocusMode = false;  // Return to editor on stop
            }
            ImGui::PopStyleColor();
        }

        // Show play state indicator
        ImGui::SameLine();
        if (m_PlayMode.IsPlaying()) {
            ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "[PLAYING]");
        } else if (m_PlayMode.IsPaused()) {
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.3f, 1.0f), "[PAUSED]");
        }

        ImGui::EndMainMenuBar();
    }

    // About dialog
    if (m_ShowAboutDialog) {
        ImGui::OpenPopup("About Enjin");
        m_ShowAboutDialog = false;
    }
    if (ImGui::BeginPopupModal("About Enjin", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("TEGE - The Enjin Game Engine");
        ImGui::Separator();
        ImGui::Text("Version 0.1.0 (Development)");
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

void EditorLayer::DrawHierarchyPanel() {
    ImGuiWindowFlags flags = 0;
    if (!m_PlayMode.IsStopped()) {
        flags |= ImGuiWindowFlags_NoInputs;
    }
    ImGui::Begin("Hierarchy", nullptr, flags);

    if (m_World) {
        // Get all entities
        const auto& entities = m_World->GetAllEntities();

        for (ECS::Entity entity : entities) {
            std::string name;

            // Use name component if available
            if (m_World->HasComponent<ECS::NameComponent>(entity)) {
                name = m_World->GetComponent<ECS::NameComponent>(entity)->name;
            } else {
                std::stringstream ss;
                ss << "Entity " << entity;
                name = ss.str();
            }

            DrawEntityNode(entity, name);
        }

        // Right-click context menu
        if (ImGui::BeginPopupContextWindow("HierarchyContextMenu", ImGuiPopupFlags_NoOpenOverItems)) {
            if (ImGui::MenuItem("Create Empty Entity")) {
                ECS::Entity entity = m_World->CreateEntity();
                m_World->AddComponent<ECS::TransformComponent>(entity);
                m_SelectedEntity = entity;
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Load Prefab...")) {
                std::vector<FileFilter> filters = {{ "Prefab Files", "*.enjprefab;*.json" }};
                std::string path = FileDialog::OpenFile("Load Prefab", filters);
                if (!path.empty()) {
                    auto prefab = Assets::PrefabManager::Get().LoadPrefab(path);
                    if (prefab) {
                        ECS::Entity root = Assets::PrefabManager::Get().Instantiate(
                            m_World, *prefab);
                        if (root != ECS::INVALID_ENTITY) {
                            m_SelectedEntity = root;
                        }
                    }
                }
            }
            ImGui::EndPopup();
        }
    } else {
        ImGui::TextDisabled("No world loaded");
    }

    ImGui::End();
}

void EditorLayer::DrawEntityNode(ECS::Entity entity, const std::string& name) {
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow |
                               ImGuiTreeNodeFlags_SpanAvailWidth |
                               ImGuiTreeNodeFlags_Leaf; // No children for now

    if (entity == m_SelectedEntity) {
        flags |= ImGuiTreeNodeFlags_Selected;
    }

    bool opened = ImGui::TreeNodeEx((void*)(uintptr_t)entity, flags, "%s", name.c_str());

    if (ImGui::IsItemClicked()) {
        m_SelectedEntity = entity;
        if (m_OnEntitySelected) {
            m_OnEntitySelected(entity);
        }
    }

    // Double-click to focus camera on entity
    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        FocusOnEntity(entity);
    }

    // Context menu
    if (ImGui::BeginPopupContextItem()) {
        if (ImGui::MenuItem("Delete", "Del")) {
            m_World->DestroyEntity(entity);
            if (m_SelectedEntity == entity) {
                m_SelectedEntity = ECS::INVALID_ENTITY;
            }
        }
        if (ImGui::MenuItem("Duplicate", "Ctrl+D")) {
            DuplicateEntity(entity);
        }
        if (ImGui::MenuItem("Focus", "F")) {
            FocusOnEntity(entity);
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Save as Prefab...")) {
            std::string defaultName = "prefab.enjprefab";
            if (m_World->HasComponent<ECS::NameComponent>(entity)) {
                defaultName = m_World->GetComponent<ECS::NameComponent>(entity)->name + ".enjprefab";
            }
            std::vector<FileFilter> filters = {{ "Prefab Files", "*.enjprefab" }};
            std::string path = FileDialog::SaveFile("Save as Prefab", filters, "", defaultName);
            if (!path.empty()) {
                std::string prefabName = defaultName.substr(0, defaultName.find_last_of('.'));
                auto prefab = Assets::PrefabManager::Get().CreateFromEntity(
                    m_World, entity, prefabName);
                if (prefab) {
                    Assets::PrefabManager::Get().SavePrefab(*prefab, path);
                }
            }
        }
        bool isPrefabInstance = Assets::PrefabUtils::IsPrefabInstance(m_World, entity);
        if (isPrefabInstance) {
            if (ImGui::MenuItem("Unpack Prefab")) {
                Assets::PrefabManager::Get().UnpackInstance(m_World, entity);
            }
        }
        ImGui::EndPopup();
    }

    if (opened) {
        ImGui::TreePop();
    }
}

void EditorLayer::DrawInspectorPanel() {
    ImGuiWindowFlags flags = 0;
    if (!m_PlayMode.IsStopped()) {
        flags |= ImGuiWindowFlags_NoInputs;
    }
    ImGui::Begin("Inspector", nullptr, flags);

    if (m_SelectedEntity != ECS::INVALID_ENTITY && m_World) {
        // Entity name (editable)
        ECS::NameComponent* nameComp = m_World->GetComponent<ECS::NameComponent>(m_SelectedEntity);
        if (nameComp) {
            char nameBuffer[256];
            strncpy(nameBuffer, nameComp->name.c_str(), sizeof(nameBuffer) - 1);
            nameBuffer[sizeof(nameBuffer) - 1] = '\0';
            ImGui::SetNextItemWidth(-1);
            if (ImGui::InputText("##EntityName", nameBuffer, sizeof(nameBuffer))) {
                nameComp->name = nameBuffer;
            }
        } else {
            // No name component - show entity ID and add button
            ImGui::Text("Entity %llu", (unsigned long long)m_SelectedEntity);
            ImGui::SameLine();
            if (ImGui::SmallButton("Add Name")) {
                m_World->AddComponent<ECS::NameComponent>(m_SelectedEntity, "Unnamed");
            }
        }
        // Prefab instance indicator
        if (Assets::PrefabUtils::IsPrefabInstance(m_World, m_SelectedEntity)) {
            auto* prefabInst = m_World->GetComponent<Assets::PrefabInstanceComponent>(m_SelectedEntity);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.7f, 1.0f, 1.0f));
            ImGui::Text("Prefab Instance");
            ImGui::PopStyleColor();
            if (prefabInst && !prefabInst->prefabPath.empty()) {
                ImGui::SameLine();
                ImGui::TextDisabled("(%s)", std::filesystem::path(prefabInst->prefabPath).filename().string().c_str());
            }
        }
        ImGui::Separator();

        // Transform component
        if (m_World->HasComponent<ECS::TransformComponent>(m_SelectedEntity)) {
            DrawTransformComponent(m_SelectedEntity);
        }

        // Mesh component
        if (m_World->HasComponent<ECS::MeshComponent>(m_SelectedEntity)) {
            DrawMeshComponent(m_SelectedEntity);
        }

        // LOD component
        if (m_World->HasComponent<ECS::LODComponent>(m_SelectedEntity)) {
            DrawLODComponent(m_SelectedEntity);
        }

        // Material component
        if (m_World->HasComponent<ECS::MaterialComponent>(m_SelectedEntity)) {
            DrawMaterialComponent(m_SelectedEntity);
        }

        // Light component
        if (m_World->HasComponent<ECS::LightComponent>(m_SelectedEntity)) {
            DrawLightComponent(m_SelectedEntity);
        }

        // Camera component
        if (m_World->HasComponent<ECS::CameraComponent>(m_SelectedEntity)) {
            DrawCameraComponent(m_SelectedEntity);
        }

        // Weather Zone component
        if (m_World->HasComponent<ECS::WeatherZoneComponent>(m_SelectedEntity)) {
            DrawWeatherZoneComponent(m_SelectedEntity);
        }

        // Water Volume component
        if (m_World->HasComponent<ECS::WaterVolumeComponent>(m_SelectedEntity)) {
            DrawWaterVolumeComponent(m_SelectedEntity);
        }

        // Grass Volume component
        if (m_World->HasComponent<ECS::GrassVolumeComponent>(m_SelectedEntity)) {
            DrawGrassVolumeComponent(m_SelectedEntity);
        }

        // Shrub Volume component
        if (m_World->HasComponent<ECS::ShrubVolumeComponent>(m_SelectedEntity)) {
            DrawShrubVolumeComponent(m_SelectedEntity);
        }

        // Tree Volume component
        if (m_World->HasComponent<ECS::TreeVolumeComponent>(m_SelectedEntity)) {
            DrawTreeVolumeComponent(m_SelectedEntity);
        }

        // 3D Terrain component
        if (m_World->HasComponent<ECS::TerrainComponent>(m_SelectedEntity)) {
            DrawTerrainComponent(m_SelectedEntity);
        }

        // 2D Terrain component
        if (m_World->HasComponent<ECS::Terrain2DComponent>(m_SelectedEntity)) {
            DrawTerrain2DComponent(m_SelectedEntity);
        }

        // Vegetation component
        if (m_World->HasComponent<ECS::VegetationComponent>(m_SelectedEntity)) {
            DrawVegetationComponent(m_SelectedEntity);
        }

        // Camera Trigger component
        if (m_World->HasComponent<ECS::CameraTriggerComponent>(m_SelectedEntity)) {
            DrawCameraTriggerComponent(m_SelectedEntity);
        }

        // Temperature Zone component
        if (m_World->HasComponent<ECS::TemperatureZoneComponent>(m_SelectedEntity)) {
            DrawTemperatureZoneComponent(m_SelectedEntity);
        }

        // Gravity Zone component
        if (m_World->HasComponent<ECS::GravityZoneComponent>(m_SelectedEntity)) {
            DrawGravityZoneComponent(m_SelectedEntity);
        }

        // Notes component
        if (m_World->HasComponent<ECS::NotesComponent>(m_SelectedEntity)) {
            DrawNotesComponent(m_SelectedEntity);
        }

        // Text component
        if (m_World->HasComponent<ECS::TextComponent>(m_SelectedEntity)) {
            DrawTextComponent(m_SelectedEntity);
        }

        // Character Controller components
        if (m_World->HasComponent<ECS::Platformer2DController>(m_SelectedEntity)) {
            DrawPlatformer2DController(m_SelectedEntity);
        }
        if (m_World->HasComponent<ECS::TopDown2DController>(m_SelectedEntity)) {
            DrawTopDown2DController(m_SelectedEntity);
        }
        if (m_World->HasComponent<ECS::TopDown3DController>(m_SelectedEntity)) {
            DrawTopDown3DController(m_SelectedEntity);
        }
        if (m_World->HasComponent<ECS::ThirdPersonController>(m_SelectedEntity)) {
            DrawThirdPersonController(m_SelectedEntity);
        }
        if (m_World->HasComponent<ECS::FirstPersonController>(m_SelectedEntity)) {
            DrawFirstPersonController(m_SelectedEntity);
        }

        // Gameplay components
        if (m_World->HasComponent<ECS::HealthComponent>(m_SelectedEntity)) {
            DrawHealthComponent(m_SelectedEntity);
        }
        if (m_World->HasComponent<ECS::RigidbodyComponent>(m_SelectedEntity)) {
            DrawRigidbodyComponent(m_SelectedEntity);
        }
        if (m_World->HasComponent<ECS::BoxColliderComponent>(m_SelectedEntity)) {
            DrawBoxColliderComponent(m_SelectedEntity);
        }
        if (m_World->HasComponent<ECS::SphereColliderComponent>(m_SelectedEntity)) {
            DrawSphereColliderComponent(m_SelectedEntity);
        }
        if (m_World->HasComponent<ECS::CapsuleColliderComponent>(m_SelectedEntity)) {
            DrawCapsuleColliderComponent(m_SelectedEntity);
        }
        if (m_World->HasComponent<ECS::TriggerZoneComponent>(m_SelectedEntity)) {
            DrawTriggerZoneComponent(m_SelectedEntity);
        }
        if (m_World->HasComponent<ECS::DamageComponent>(m_SelectedEntity)) {
            DrawDamageComponent(m_SelectedEntity);
        }
        if (m_World->HasComponent<ECS::InteractableComponent>(m_SelectedEntity)) {
            DrawInteractableComponent(m_SelectedEntity);
        }
        if (m_World->HasComponent<ECS::PickupComponent>(m_SelectedEntity)) {
            DrawPickupComponent(m_SelectedEntity);
        }
        if (m_World->HasComponent<ECS::InventoryComponent>(m_SelectedEntity)) {
            DrawInventoryComponent(m_SelectedEntity);
        }
        if (m_World->HasComponent<ECS::TimerComponent>(m_SelectedEntity)) {
            DrawTimerComponent(m_SelectedEntity);
        }
        if (m_World->HasComponent<ECS::AudioSourceComponent>(m_SelectedEntity)) {
            DrawAudioSourceComponent(m_SelectedEntity);
        }
        if (m_World->HasComponent<ECS::AudioListenerComponent>(m_SelectedEntity)) {
            DrawAudioListenerComponent(m_SelectedEntity);
        }

        // AI components
        if (m_World->HasComponent<ECS::AIControllerComponent>(m_SelectedEntity)) {
            DrawAIControllerComponent(m_SelectedEntity);
        }
        if (m_World->HasComponent<ECS::FollowTargetComponent>(m_SelectedEntity)) {
            DrawFollowTargetComponent(m_SelectedEntity);
        }
        if (m_World->HasComponent<ECS::LookAtTargetComponent>(m_SelectedEntity)) {
            DrawLookAtTargetComponent(m_SelectedEntity);
        }
        if (m_World->HasComponent<ECS::WaypointComponent>(m_SelectedEntity)) {
            DrawWaypointComponent(m_SelectedEntity);
        }

        // IK Components
        if (m_World->HasComponent<ECS::LookAtIKComponent>(m_SelectedEntity)) {
            if (ImGui::CollapsingHeader("Look-At IK")) {
                auto* ik = m_World->GetComponent<ECS::LookAtIKComponent>(m_SelectedEntity);
                char headBone[128];
                strncpy(headBone, ik->headBoneName.c_str(), sizeof(headBone) - 1);
                headBone[sizeof(headBone) - 1] = '\0';
                if (ImGui::InputText("Head Bone", headBone, sizeof(headBone))) {
                    ik->headBoneName = headBone;
                }
                char neckBone[128];
                strncpy(neckBone, ik->neckBoneName.c_str(), sizeof(neckBone) - 1);
                neckBone[sizeof(neckBone) - 1] = '\0';
                if (ImGui::InputText("Neck Bone", neckBone, sizeof(neckBone))) {
                    ik->neckBoneName = neckBone;
                }
                ImGui::SliderFloat("Max Rotation", &ik->maxRotation, 0.0f, 90.0f);
                ImGui::SliderFloat("Smooth Speed##LookAtIK", &ik->smoothSpeed, 0.1f, 20.0f);
                ImGui::SliderFloat("Look Weight", &ik->lookWeight, 0.0f, 1.0f);
                ImGui::DragFloat3("Target Pos", &ik->targetWorldPos.x, 0.1f);
            }
        }

        if (m_World->HasComponent<ECS::InteractionIKComponent>(m_SelectedEntity)) {
            if (ImGui::CollapsingHeader("Interaction IK")) {
                auto* ik = m_World->GetComponent<ECS::InteractionIKComponent>(m_SelectedEntity);
                char handBone[128];
                strncpy(handBone, ik->handBoneName.c_str(), sizeof(handBone) - 1);
                handBone[sizeof(handBone) - 1] = '\0';
                if (ImGui::InputText("Hand Bone", handBone, sizeof(handBone))) {
                    ik->handBoneName = handBone;
                }
                char elbowBone[128];
                strncpy(elbowBone, ik->elbowBoneName.c_str(), sizeof(elbowBone) - 1);
                elbowBone[sizeof(elbowBone) - 1] = '\0';
                if (ImGui::InputText("Elbow Bone", elbowBone, sizeof(elbowBone))) {
                    ik->elbowBoneName = elbowBone;
                }
                char shoulderBone[128];
                strncpy(shoulderBone, ik->shoulderBoneName.c_str(), sizeof(shoulderBone) - 1);
                shoulderBone[sizeof(shoulderBone) - 1] = '\0';
                if (ImGui::InputText("Shoulder Bone", shoulderBone, sizeof(shoulderBone))) {
                    ik->shoulderBoneName = shoulderBone;
                }
                ImGui::SliderFloat("Radius", &ik->interactionRadius, 0.1f, 10.0f);
                ImGui::SliderFloat("IK Weight", &ik->ikWeight, 0.0f, 1.0f);
                ImGui::SliderFloat("Smooth Speed##InteractionIK", &ik->smoothSpeed, 0.1f, 20.0f);
                char ikTag[128];
                strncpy(ikTag, ik->interactionTag.c_str(), sizeof(ikTag) - 1);
                ikTag[sizeof(ikTag) - 1] = '\0';
                if (ImGui::InputText("Interaction Tag", ikTag, sizeof(ikTag))) {
                    ik->interactionTag = ikTag;
                }
            }
        }

        // Visual components
        if (m_World->HasComponent<ECS::BillboardComponent>(m_SelectedEntity)) {
            DrawBillboardComponent(m_SelectedEntity);
        }
        if (m_World->HasComponent<ECS::ParticleEmitterComponent>(m_SelectedEntity)) {
            DrawParticleEmitterComponent(m_SelectedEntity);
        }

        // 2D components
        if (m_World->HasComponent<ECS::Sprite2DComponent>(m_SelectedEntity)) {
            DrawSprite2DComponent(m_SelectedEntity);
        }
        if (m_World->HasComponent<ECS::AnimatedSprite2DComponent>(m_SelectedEntity)) {
            DrawAnimatedSprite2DComponent(m_SelectedEntity);
        }
        if (m_World->HasComponent<ECS::TilemapComponent>(m_SelectedEntity)) {
            DrawTilemapComponent(m_SelectedEntity);
        }
        if (m_World->HasComponent<ECS::StateMachineComponent>(m_SelectedEntity)) {
            DrawStateMachineComponent(m_SelectedEntity);
        }
        if (m_World->HasComponent<ECS::Camera2DBoundsComponent>(m_SelectedEntity)) {
            DrawCamera2DBoundsComponent(m_SelectedEntity);
        }
        if (m_World->HasComponent<ECS::DialogueComponent>(m_SelectedEntity)) {
            DrawDialogueComponent(m_SelectedEntity);
        }

        // Other components
        if (m_World->HasComponent<ECS::TagComponent>(m_SelectedEntity)) {
            DrawTagComponent(m_SelectedEntity);
        }
        if (m_World->HasComponent<ECS::SpawnPointComponent>(m_SelectedEntity)) {
            DrawSpawnPointComponent(m_SelectedEntity);
        }

        // Flower components
        if (m_World->HasComponent<ECS::JellyMeshComponent>(m_SelectedEntity)) {
            DrawJellyMeshComponent(m_SelectedEntity);
        }
        if (m_World->HasComponent<ECS::TetherComponent>(m_SelectedEntity)) {
            DrawTetherComponent(m_SelectedEntity);
        }
        if (m_World->HasComponent<ECS::GrabbableComponent>(m_SelectedEntity)) {
            DrawGrabbableComponent(m_SelectedEntity);
        }
        if (m_World->HasComponent<ECS::FlowerStemComponent>(m_SelectedEntity)) {
            DrawFlowerStemComponent(m_SelectedEntity);
        }

        ImGui::Separator();

        // Add component button
        if (ImGui::Button("Add Component")) {
            ImGui::OpenPopup("AddComponentPopup");
        }

        if (ImGui::BeginPopup("AddComponentPopup")) {
            if (!m_World->HasComponent<ECS::MeshComponent>(m_SelectedEntity)) {
                if (ImGui::MenuItem("Mesh")) {
                    m_World->AddComponent<ECS::MeshComponent>(m_SelectedEntity);
                }
            }
            if (!m_World->HasComponent<ECS::LODComponent>(m_SelectedEntity) &&
                m_World->HasComponent<ECS::MeshComponent>(m_SelectedEntity)) {
                if (ImGui::MenuItem("LOD (Auto-Generate)")) {
                    auto* mesh = m_World->GetComponent<ECS::MeshComponent>(m_SelectedEntity);
                    if (mesh && mesh->IsValid()) {
                        auto& lod = m_World->AddComponent<ECS::LODComponent>(m_SelectedEntity);
                        Renderer::MeshSimplifier::GenerateLODs(*mesh, lod);
                    }
                }
            }
            if (!m_World->HasComponent<ECS::MaterialComponent>(m_SelectedEntity)) {
                if (ImGui::MenuItem("Material")) {
                    m_World->AddComponent<ECS::MaterialComponent>(m_SelectedEntity);
                }
            }
            if (!m_World->HasComponent<ECS::LightComponent>(m_SelectedEntity)) {
                if (ImGui::MenuItem("Light")) {
                    m_World->AddComponent<ECS::LightComponent>(m_SelectedEntity);
                }
            }
            if (!m_World->HasComponent<ECS::CameraComponent>(m_SelectedEntity)) {
                if (ImGui::MenuItem("Camera")) {
                    m_World->AddComponent<ECS::CameraComponent>(m_SelectedEntity);
                }
            }
            if (!m_World->HasComponent<ECS::NotesComponent>(m_SelectedEntity)) {
                if (ImGui::MenuItem("Notes")) {
                    m_World->AddComponent<ECS::NotesComponent>(m_SelectedEntity);
                }
            }
            if (!m_World->HasComponent<ECS::TextComponent>(m_SelectedEntity)) {
                if (ImGui::MenuItem("Text")) {
                    m_World->AddComponent<ECS::TextComponent>(m_SelectedEntity);
                }
            }
            ImGui::Separator();

            // Character Controllers submenu
            if (ImGui::BeginMenu("Character Controller")) {
                if (!m_World->HasComponent<ECS::Platformer2DController>(m_SelectedEntity)) {
                    if (ImGui::MenuItem("2D Platformer")) {
                        m_World->AddComponent<ECS::Platformer2DController>(m_SelectedEntity);
                        SetupCameraForController(m_SelectedEntity, "Platformer2D");
                    }
                }
                if (!m_World->HasComponent<ECS::TopDown2DController>(m_SelectedEntity)) {
                    if (ImGui::MenuItem("2D Top-Down")) {
                        m_World->AddComponent<ECS::TopDown2DController>(m_SelectedEntity);
                        SetupCameraForController(m_SelectedEntity, "TopDown2D");
                    }
                }
                if (!m_World->HasComponent<ECS::TopDown3DController>(m_SelectedEntity)) {
                    if (ImGui::MenuItem("3D Top-Down (Isometric)")) {
                        m_World->AddComponent<ECS::TopDown3DController>(m_SelectedEntity);
                        SetupCameraForController(m_SelectedEntity, "TopDown3D");
                    }
                }
                if (!m_World->HasComponent<ECS::ThirdPersonController>(m_SelectedEntity)) {
                    if (ImGui::MenuItem("3D Third Person")) {
                        m_World->AddComponent<ECS::ThirdPersonController>(m_SelectedEntity);
                        SetupCameraForController(m_SelectedEntity, "ThirdPerson");
                    }
                }
                if (!m_World->HasComponent<ECS::FirstPersonController>(m_SelectedEntity)) {
                    if (ImGui::MenuItem("3D First Person")) {
                        m_World->AddComponent<ECS::FirstPersonController>(m_SelectedEntity);
                        SetupCameraForController(m_SelectedEntity, "FirstPerson");
                    }
                }
                ImGui::EndMenu();
            }

            // Physics submenu
            if (ImGui::BeginMenu("Physics")) {
                if (!m_World->HasComponent<ECS::RigidbodyComponent>(m_SelectedEntity)) {
                    if (ImGui::MenuItem("Rigidbody")) {
                        m_World->AddComponent<ECS::RigidbodyComponent>(m_SelectedEntity);
                    }
                }
                if (!m_World->HasComponent<ECS::BoxColliderComponent>(m_SelectedEntity)) {
                    if (ImGui::MenuItem("Box Collider")) {
                        m_World->AddComponent<ECS::BoxColliderComponent>(m_SelectedEntity);
                    }
                }
                if (!m_World->HasComponent<ECS::SphereColliderComponent>(m_SelectedEntity)) {
                    if (ImGui::MenuItem("Sphere Collider")) {
                        m_World->AddComponent<ECS::SphereColliderComponent>(m_SelectedEntity);
                    }
                }
                if (!m_World->HasComponent<ECS::CapsuleColliderComponent>(m_SelectedEntity)) {
                    if (ImGui::MenuItem("Capsule Collider")) {
                        m_World->AddComponent<ECS::CapsuleColliderComponent>(m_SelectedEntity);
                    }
                }
                if (!m_World->HasComponent<ECS::TriggerZoneComponent>(m_SelectedEntity)) {
                    if (ImGui::MenuItem("Trigger Zone")) {
                        m_World->AddComponent<ECS::TriggerZoneComponent>(m_SelectedEntity);
                    }
                }
                ImGui::EndMenu();
            }

            // Gameplay submenu
            if (ImGui::BeginMenu("Gameplay")) {
                if (!m_World->HasComponent<ECS::HealthComponent>(m_SelectedEntity)) {
                    if (ImGui::MenuItem("Health")) {
                        m_World->AddComponent<ECS::HealthComponent>(m_SelectedEntity);
                    }
                }
                if (!m_World->HasComponent<ECS::DamageComponent>(m_SelectedEntity)) {
                    if (ImGui::MenuItem("Damage")) {
                        m_World->AddComponent<ECS::DamageComponent>(m_SelectedEntity);
                    }
                }
                if (!m_World->HasComponent<ECS::InteractableComponent>(m_SelectedEntity)) {
                    if (ImGui::MenuItem("Interactable")) {
                        m_World->AddComponent<ECS::InteractableComponent>(m_SelectedEntity);
                    }
                }
                if (!m_World->HasComponent<ECS::PickupComponent>(m_SelectedEntity)) {
                    if (ImGui::MenuItem("Pickup")) {
                        m_World->AddComponent<ECS::PickupComponent>(m_SelectedEntity);
                    }
                }
                if (!m_World->HasComponent<ECS::InventoryComponent>(m_SelectedEntity)) {
                    if (ImGui::MenuItem("Inventory")) {
                        m_World->AddComponent<ECS::InventoryComponent>(m_SelectedEntity);
                    }
                }
                if (!m_World->HasComponent<ECS::TimerComponent>(m_SelectedEntity)) {
                    if (ImGui::MenuItem("Timer")) {
                        m_World->AddComponent<ECS::TimerComponent>(m_SelectedEntity);
                    }
                }
                ImGui::EndMenu();
            }

            // AI submenu
            if (ImGui::BeginMenu("AI")) {
                if (!m_World->HasComponent<ECS::AIControllerComponent>(m_SelectedEntity)) {
                    if (ImGui::MenuItem("AI Controller")) {
                        m_World->AddComponent<ECS::AIControllerComponent>(m_SelectedEntity);
                    }
                }
                if (!m_World->HasComponent<ECS::FollowTargetComponent>(m_SelectedEntity)) {
                    if (ImGui::MenuItem("Follow Target")) {
                        m_World->AddComponent<ECS::FollowTargetComponent>(m_SelectedEntity);
                    }
                }
                if (!m_World->HasComponent<ECS::LookAtTargetComponent>(m_SelectedEntity)) {
                    if (ImGui::MenuItem("Look At Target")) {
                        m_World->AddComponent<ECS::LookAtTargetComponent>(m_SelectedEntity);
                    }
                }
                if (!m_World->HasComponent<ECS::WaypointComponent>(m_SelectedEntity)) {
                    if (ImGui::MenuItem("Waypoint")) {
                        m_World->AddComponent<ECS::WaypointComponent>(m_SelectedEntity);
                    }
                }
                ImGui::EndMenu();
            }

            // Audio submenu
            if (ImGui::BeginMenu("Audio")) {
                if (!m_World->HasComponent<ECS::AudioSourceComponent>(m_SelectedEntity)) {
                    if (ImGui::MenuItem("Audio Source")) {
                        m_World->AddComponent<ECS::AudioSourceComponent>(m_SelectedEntity);
                    }
                }
                if (!m_World->HasComponent<ECS::AudioListenerComponent>(m_SelectedEntity)) {
                    if (ImGui::MenuItem("Audio Listener")) {
                        m_World->AddComponent<ECS::AudioListenerComponent>(m_SelectedEntity);
                    }
                }
                ImGui::EndMenu();
            }

            // Visual submenu
            if (ImGui::BeginMenu("Visual")) {
                if (!m_World->HasComponent<ECS::BillboardComponent>(m_SelectedEntity)) {
                    if (ImGui::MenuItem("Billboard")) {
                        m_World->AddComponent<ECS::BillboardComponent>(m_SelectedEntity);
                    }
                }
                if (!m_World->HasComponent<ECS::ParticleEmitterComponent>(m_SelectedEntity)) {
                    if (ImGui::MenuItem("Particle Emitter")) {
                        m_World->AddComponent<ECS::ParticleEmitterComponent>(m_SelectedEntity);
                    }
                }
                ImGui::EndMenu();
            }

            // Effects submenu
            if (ImGui::BeginMenu("Effects")) {
                if (!m_World->HasComponent<ECS::WeatherZoneComponent>(m_SelectedEntity)) {
                    if (ImGui::MenuItem("Weather Zone")) {
                        m_World->AddComponent<ECS::WeatherZoneComponent>(m_SelectedEntity);
                    }
                }
                if (!m_World->HasComponent<ECS::WaterVolumeComponent>(m_SelectedEntity)) {
                    if (ImGui::MenuItem("Water Volume")) {
                        m_World->AddComponent<ECS::WaterVolumeComponent>(m_SelectedEntity);
                    }
                }
                if (!m_World->HasComponent<ECS::GrassVolumeComponent>(m_SelectedEntity)) {
                    if (ImGui::MenuItem("Grass Volume")) {
                        m_World->AddComponent<ECS::GrassVolumeComponent>(m_SelectedEntity);
                    }
                }
                if (!m_World->HasComponent<ECS::ShrubVolumeComponent>(m_SelectedEntity)) {
                    if (ImGui::MenuItem("Shrub Volume")) {
                        m_World->AddComponent<ECS::ShrubVolumeComponent>(m_SelectedEntity);
                    }
                }
                if (!m_World->HasComponent<ECS::TreeVolumeComponent>(m_SelectedEntity)) {
                    if (ImGui::MenuItem("Tree Volume")) {
                        m_World->AddComponent<ECS::TreeVolumeComponent>(m_SelectedEntity);
                    }
                }
                if (!m_World->HasComponent<ECS::VegetationComponent>(m_SelectedEntity)) {
                    if (ImGui::MenuItem("Vegetation")) {
                        m_World->AddComponent<ECS::VegetationComponent>(m_SelectedEntity);
                    }
                }
                if (!m_World->HasComponent<ECS::CameraTriggerComponent>(m_SelectedEntity)) {
                    if (ImGui::MenuItem("Camera Trigger")) {
                        m_World->AddComponent<ECS::CameraTriggerComponent>(m_SelectedEntity);
                    }
                }
                if (!m_World->HasComponent<ECS::TemperatureZoneComponent>(m_SelectedEntity)) {
                    if (ImGui::MenuItem("Temperature Zone")) {
                        m_World->AddComponent<ECS::TemperatureZoneComponent>(m_SelectedEntity);
                    }
                }
                if (!m_World->HasComponent<ECS::GravityZoneComponent>(m_SelectedEntity)) {
                    if (ImGui::MenuItem("Gravity Zone")) {
                        m_World->AddComponent<ECS::GravityZoneComponent>(m_SelectedEntity);
                    }
                }
                ImGui::EndMenu();
            }

            // 2D Graphics submenu
            if (ImGui::BeginMenu("2D Graphics")) {
                if (!m_World->HasComponent<ECS::Sprite2DComponent>(m_SelectedEntity)) {
                    if (ImGui::MenuItem("Sprite")) {
                        m_World->AddComponent<ECS::Sprite2DComponent>(m_SelectedEntity);
                    }
                }
                if (!m_World->HasComponent<ECS::AnimatedSprite2DComponent>(m_SelectedEntity)) {
                    if (ImGui::MenuItem("Animated Sprite")) {
                        m_World->AddComponent<ECS::AnimatedSprite2DComponent>(m_SelectedEntity);
                    }
                }
                if (!m_World->HasComponent<ECS::TilemapComponent>(m_SelectedEntity)) {
                    if (ImGui::MenuItem("Tilemap")) {
                        m_World->AddComponent<ECS::TilemapComponent>(m_SelectedEntity);
                    }
                }
                if (!m_World->HasComponent<ECS::Camera2DBoundsComponent>(m_SelectedEntity)) {
                    if (ImGui::MenuItem("2D Camera Bounds")) {
                        m_World->AddComponent<ECS::Camera2DBoundsComponent>(m_SelectedEntity);
                    }
                }
                ImGui::EndMenu();
            }

            // Other
            if (ImGui::BeginMenu("Other")) {
                if (!m_World->HasComponent<ECS::TagComponent>(m_SelectedEntity)) {
                    if (ImGui::MenuItem("Tags")) {
                        m_World->AddComponent<ECS::TagComponent>(m_SelectedEntity);
                    }
                }
                if (!m_World->HasComponent<ECS::SpawnPointComponent>(m_SelectedEntity)) {
                    if (ImGui::MenuItem("Spawn Point")) {
                        m_World->AddComponent<ECS::SpawnPointComponent>(m_SelectedEntity);
                    }
                }
                ImGui::EndMenu();
            }

            ImGui::EndPopup();
        }
    } else {
        ImGui::TextDisabled("No entity selected");
    }

    ImGui::End();
}

void EditorLayer::DrawTransformComponent(ECS::Entity entity) {
    if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
        ECS::TransformComponent* transform = m_World->GetComponent<ECS::TransformComponent>(entity);
        if (!transform) return;

        // Position
        f32 pos[3] = { transform->position.x, transform->position.y, transform->position.z };
        if (ImGui::DragFloat3("Position", pos, 0.1f)) {
            transform->position = Math::Vector3(pos[0], pos[1], pos[2]);
        }

        // Rotation (euler angles in degrees)
        Math::Vector3 eulerRad = transform->rotation.ToEuler();
        f32 rot[3] = { Math::Degrees(eulerRad.x), Math::Degrees(eulerRad.y), Math::Degrees(eulerRad.z) };
        if (ImGui::DragFloat3("Rotation", rot, 1.0f)) {
            transform->rotation = Math::Quaternion::FromEuler(
                Math::Vector3(Math::Radians(rot[0]), Math::Radians(rot[1]), Math::Radians(rot[2])));
        }

        // Scale
        f32 scale[3] = { transform->scale.x, transform->scale.y, transform->scale.z };
        if (ImGui::DragFloat3("Scale", scale, 0.1f, 0.001f, 1000.0f)) {
            transform->scale = Math::Vector3(scale[0], scale[1], scale[2]);
        }
    }
}

void EditorLayer::DrawMeshComponent(ECS::Entity entity) {
    bool meshOpen = ImGui::CollapsingHeader("Mesh", ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::BeginPopupContextItem("MeshCtx")) {
        if (ImGui::MenuItem("Remove Component")) {
            m_World->RemoveComponent<ECS::MeshComponent>(entity);
            ImGui::EndPopup();
            return;
        }
        ImGui::EndPopup();
    }
    if (meshOpen) {
        ECS::MeshComponent* mesh = m_World->GetComponent<ECS::MeshComponent>(entity);
        if (!mesh) return;

        ImGui::Text("Vertices: %zu", mesh->vertices.size());
        ImGui::Text("Indices: %zu", mesh->indices.size());
    }
}

void EditorLayer::DrawLODComponent(ECS::Entity entity) {
    bool lodOpen = ImGui::CollapsingHeader("LOD", ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::BeginPopupContextItem("LODCtx")) {
        if (ImGui::MenuItem("Remove Component")) {
            m_World->RemoveComponent<ECS::LODComponent>(entity);
            ImGui::EndPopup();
            return;
        }
        if (ImGui::MenuItem("Regenerate LODs")) {
            auto* mesh = m_World->GetComponent<ECS::MeshComponent>(entity);
            auto* lod = m_World->GetComponent<ECS::LODComponent>(entity);
            if (mesh && lod && mesh->IsValid()) {
                Renderer::MeshSimplifier::GenerateLODs(*mesh, *lod);
            }
        }
        ImGui::EndPopup();
    }
    if (lodOpen) {
        ECS::LODComponent* lod = m_World->GetComponent<ECS::LODComponent>(entity);
        if (!lod) return;

        ImGui::Checkbox("Enabled", &lod->enabled);
        ImGui::Text("Levels: %d | Active: LOD %d", lod->levelCount, lod->activeLOD);

        if (ImGui::SliderFloat("Base Distance", &lod->baseDistance, 2.0f, 100.0f, "%.1f")) {
            // Recompute distance thresholds
            for (int i = 0; i < lod->levelCount; ++i) {
                lod->levels[i].maxDistance = lod->baseDistance * std::pow(lod->distanceMultiplier, static_cast<f32>(i));
            }
        }
        if (ImGui::SliderFloat("Distance Mult", &lod->distanceMultiplier, 1.2f, 5.0f, "%.1f")) {
            for (int i = 0; i < lod->levelCount; ++i) {
                lod->levels[i].maxDistance = lod->baseDistance * std::pow(lod->distanceMultiplier, static_cast<f32>(i));
            }
        }

        ImGui::Separator();
        for (int i = 0; i < lod->levelCount; ++i) {
            auto& level = lod->levels[i];
            bool isActive = (i == lod->activeLOD);

            if (isActive) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.3f, 1.0f, 0.4f, 1.0f));
            }

            ImGui::Text("LOD %d: %u verts, %u tris (%.0f%%)  dist < %.1f%s",
                i, level.vertexCount, level.triangleCount,
                level.reductionRatio * 100.0f, level.maxDistance,
                isActive ? "  [ACTIVE]" : "");

            if (isActive) {
                ImGui::PopStyleColor();
            }

            // Reduction ratio slider for regeneration
            char label[32];
            snprintf(label, sizeof(label), "Ratio##lod%d", i);
            if (i > 0) {
                ImGui::SameLine();
                ImGui::PushItemWidth(80);
                ImGui::SliderFloat(label, &lod->reductionRatios[i], 0.01f, 0.99f, "%.2f");
                ImGui::PopItemWidth();
            }
        }
    }
}

void EditorLayer::DrawMaterialComponent(ECS::Entity entity) {
    bool matOpen = ImGui::CollapsingHeader("Material", ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::BeginPopupContextItem("MaterialCtx")) {
        if (ImGui::MenuItem("Remove Component")) {
            m_World->RemoveComponent<ECS::MaterialComponent>(entity);
            ImGui::EndPopup();
            return;
        }
        ImGui::EndPopup();
    }
    if (matOpen) {
        ECS::MaterialComponent* material = m_World->GetComponent<ECS::MaterialComponent>(entity);
        if (!material) return;

        // Base color
        f32 baseColor[3] = { material->baseColor.x, material->baseColor.y, material->baseColor.z };
        if (ImGui::ColorEdit3("Base Color", baseColor)) {
            material->baseColor = Math::Vector3(baseColor[0], baseColor[1], baseColor[2]);
        }

        // Opacity
        ImGui::DragFloat("Opacity", &material->opacity, 0.01f, 0.0f, 1.0f);

        // PBR properties
        ImGui::DragFloat("Metallic", &material->metallic, 0.01f, 0.0f, 1.0f);
        ImGui::DragFloat("Roughness", &material->roughness, 0.01f, 0.0f, 1.0f);

        // Emission
        f32 emissive[3] = { material->emissiveColor.x, material->emissiveColor.y, material->emissiveColor.z };
        if (ImGui::ColorEdit3("Emissive Color", emissive)) {
            material->emissiveColor = Math::Vector3(emissive[0], emissive[1], emissive[2]);
        }
        ImGui::DragFloat("Emissive Strength", &material->emissiveStrength, 0.1f, 0.0f, 100.0f);

        // Rendering options
        ImGui::Checkbox("Double Sided", &material->doubleSided);
        ImGui::Checkbox("Cast Shadows", &material->castShadows);
        ImGui::Checkbox("Receive Shadows", &material->receiveShadows);

        // Alpha mode
        const char* alphaModes[] = { "Opaque", "Mask", "Blend" };
        int currentMode = static_cast<int>(material->alphaMode);
        if (ImGui::Combo("Alpha Mode", &currentMode, alphaModes, 3)) {
            material->alphaMode = static_cast<ECS::MaterialComponent::AlphaMode>(currentMode);
        }

        if (material->alphaMode == ECS::MaterialComponent::AlphaMode::Mask) {
            ImGui::DragFloat("Alpha Cutoff", &material->alphaCutoff, 0.01f, 0.0f, 1.0f);
        }

        // Texture paths
        if (ImGui::TreeNode("Textures")) {
            // Base color texture path
            char basePath[256];
            strncpy(basePath, material->baseColorTexturePath.c_str(), sizeof(basePath) - 1);
            basePath[sizeof(basePath) - 1] = '\0';
            if (ImGui::InputText("Base Color", basePath, sizeof(basePath))) {
                material->baseColorTexturePath = basePath;
                if (material->baseColorTexturePath.empty()) material->baseColorTexture = -1;
            }

            // Normal map texture path
            char normalPath[256];
            strncpy(normalPath, material->normalTexturePath.c_str(), sizeof(normalPath) - 1);
            normalPath[sizeof(normalPath) - 1] = '\0';
            if (ImGui::InputText("Normal Map", normalPath, sizeof(normalPath))) {
                material->normalTexturePath = normalPath;
                if (material->normalTexturePath.empty()) material->normalTexture = -1;
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Tangent-space normal map (RGB encoded)");

            // Metallic-roughness texture path
            char mrPath[256];
            strncpy(mrPath, material->metallicRoughnessTexturePath.c_str(), sizeof(mrPath) - 1);
            mrPath[sizeof(mrPath) - 1] = '\0';
            if (ImGui::InputText("Metallic/Roughness", mrPath, sizeof(mrPath))) {
                material->metallicRoughnessTexturePath = mrPath;
                if (material->metallicRoughnessTexturePath.empty()) material->metallicRoughnessTexture = -1;
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("glTF metallic-roughness (G=roughness, B=metallic)");

            // Emissive texture path
            char emissivePath[256];
            strncpy(emissivePath, material->emissiveTexturePath.c_str(), sizeof(emissivePath) - 1);
            emissivePath[sizeof(emissivePath) - 1] = '\0';
            if (ImGui::InputText("Emissive Map", emissivePath, sizeof(emissivePath))) {
                material->emissiveTexturePath = emissivePath;
                if (material->emissiveTexturePath.empty()) material->emissiveTexture = -1;
            }
            ImGui::TreePop();
        }

        // Parallax / Height mapping
        if (ImGui::TreeNode("Parallax Mapping")) {
            char heightPath[256];
            strncpy(heightPath, material->heightTexturePath.c_str(), sizeof(heightPath) - 1);
            heightPath[sizeof(heightPath) - 1] = '\0';
            if (ImGui::InputText("Height Map Path", heightPath, sizeof(heightPath))) {
                material->heightTexturePath = heightPath;
                if (material->heightTexturePath.empty()) {
                    material->heightTexture = -1;
                }
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Path to grayscale height map texture");

            ImGui::DragFloat("Parallax Scale", &material->parallaxScale, 0.001f, 0.0f, 0.2f, "%.3f");
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Height displacement intensity (0.03-0.05 typical)");

            ImGui::TreePop();
        }

        // Retro rendering effects (per-material)
        if (ImGui::TreeNode("Retro Effects")) {
            ImGui::Checkbox("Flat Shading", &material->flatShading);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Use face normals for faceted look");

            ImGui::Checkbox("Affine Texturing", &material->affineTexturing);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("PS1-style texture warping (no perspective correction)");

            ImGui::Checkbox("Vertex Snapping", &material->vertexSnapping);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("PS1-style vertex wobble from low-precision coordinates");

            if (material->vertexSnapping) {
                int snapRes = static_cast<int>(material->vertexSnapResolution);
                if (ImGui::SliderInt("Snap Resolution", &snapRes, 80, 320)) {
                    material->vertexSnapResolution = static_cast<u8>(snapRes);
                }
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Lower = more wobble (PS1 ~160)");
            }

            ImGui::Checkbox("Stipple Transparency", &material->stippleTransparency);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Screen-door transparency using dither pattern");

            ImGui::Checkbox("UV Quantize (PS1)", &material->uvQuantize);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Snap UVs to 128-step grid for PS1-style texture swimming");

            ImGui::Checkbox("Gouraud Only", &material->gouraudOnly);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Use vertex lighting only (no per-pixel), faceted PS1/N64 look");

            ImGui::TreePop();
        }
    }
}

void EditorLayer::DrawLightComponent(ECS::Entity entity) {
    bool lightOpen = ImGui::CollapsingHeader("Light", ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::BeginPopupContextItem("LightCtx")) {
        if (ImGui::MenuItem("Remove Component")) {
            m_World->RemoveComponent<ECS::LightComponent>(entity);
            ImGui::EndPopup();
            return;
        }
        ImGui::EndPopup();
    }
    if (lightOpen) {
        ECS::LightComponent* light = m_World->GetComponent<ECS::LightComponent>(entity);
        if (!light) return;

        // Light type
        const char* lightTypes[] = { "Directional", "Point", "Spot" };
        int currentType = static_cast<int>(light->type);
        if (ImGui::Combo("Type", &currentType, lightTypes, 3)) {
            light->type = static_cast<ECS::LightType>(currentType);
        }

        // Color
        f32 color[3] = { light->color.x, light->color.y, light->color.z };
        if (ImGui::ColorEdit3("Color", color)) {
            light->color = Math::Vector3(color[0], color[1], color[2]);
        }

        // Intensity
        ImGui::DragFloat("Intensity", &light->intensity, 0.1f, 0.0f, 100.0f);

        // Point/Spot specific
        if (light->type == ECS::LightType::Point || light->type == ECS::LightType::Spot) {
            ImGui::DragFloat("Range", &light->range, 0.5f, 0.1f, 1000.0f);

            if (ImGui::TreeNode("Attenuation")) {
                ImGui::DragFloat("Constant", &light->constantAttenuation, 0.01f, 0.0f, 10.0f);
                ImGui::DragFloat("Linear", &light->linearAttenuation, 0.001f, 0.0f, 1.0f);
                ImGui::DragFloat("Quadratic", &light->quadraticAttenuation, 0.001f, 0.0f, 1.0f);
                ImGui::TreePop();
            }
        }

        // Spot specific
        if (light->type == ECS::LightType::Spot) {
            ImGui::DragFloat("Inner Cone", &light->innerConeAngle, 0.5f, 0.0f, light->outerConeAngle);
            ImGui::DragFloat("Outer Cone", &light->outerConeAngle, 0.5f, light->innerConeAngle, 90.0f);
        }

        // Shadows
        if (ImGui::TreeNode("Shadows")) {
            ImGui::Checkbox("Cast Shadows", &light->castShadows);
            if (light->castShadows) {
                const char* resolutions[] = { "512", "1024", "2048", "4096" };
                int currentRes = 0;
                if (light->shadowMapResolution == 512) currentRes = 0;
                else if (light->shadowMapResolution == 1024) currentRes = 1;
                else if (light->shadowMapResolution == 2048) currentRes = 2;
                else if (light->shadowMapResolution == 4096) currentRes = 3;

                if (ImGui::Combo("Shadow Resolution", &currentRes, resolutions, 4)) {
                    u32 resValues[] = { 512, 1024, 2048, 4096 };
                    light->shadowMapResolution = resValues[currentRes];
                }
            }
            ImGui::TreePop();
        }
    }
}

void EditorLayer::DrawCameraComponent(ECS::Entity entity) {
    bool camOpen = ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::BeginPopupContextItem("CameraCtx")) {
        if (ImGui::MenuItem("Remove Component")) {
            m_World->RemoveComponent<ECS::CameraComponent>(entity);
            ImGui::EndPopup();
            return;
        }
        ImGui::EndPopup();
    }
    if (camOpen) {
        ECS::CameraComponent* camera = m_World->GetComponent<ECS::CameraComponent>(entity);
        if (!camera) return;

        // Projection type
        const char* projTypes[] = { "Perspective", "Orthographic" };
        int currentType = static_cast<int>(camera->projectionType);
        if (ImGui::Combo("Projection", &currentType, projTypes, 2)) {
            camera->projectionType = static_cast<ECS::ProjectionType>(currentType);
        }

        // Perspective settings
        if (camera->projectionType == ECS::ProjectionType::Perspective) {
            ImGui::DragFloat("Field of View", &camera->fieldOfView, 0.5f, 1.0f, 179.0f);
        }

        // Orthographic settings
        if (camera->projectionType == ECS::ProjectionType::Orthographic) {
            ImGui::DragFloat("Ortho Size", &camera->orthoSize, 0.5f, 0.1f, 100.0f);
        }

        // Common settings
        ImGui::DragFloat("Near Plane", &camera->nearPlane, 0.01f, 0.001f, camera->farPlane - 0.01f);
        ImGui::DragFloat("Far Plane", &camera->farPlane, 1.0f, camera->nearPlane + 0.01f, 10000.0f);

        ImGui::Separator();

        // Virtual camera settings
        ImGui::Text("Virtual Camera");
        ImGui::Checkbox("Active", &camera->isActive);
        ImGui::DragInt("Priority", &camera->priority, 1, -100, 100);
        ImGui::TextDisabled("(Higher priority cameras take precedence)");

        ImGui::Separator();

        // Clear settings
        if (ImGui::TreeNode("Clear Settings")) {
            ImGui::Checkbox("Clear Color", &camera->clearColor);
            ImGui::Checkbox("Clear Depth", &camera->clearDepth);
            if (camera->clearColor) {
                f32 bgColor[3] = { camera->backgroundColor.x, camera->backgroundColor.y, camera->backgroundColor.z };
                if (ImGui::ColorEdit3("Background Color", bgColor)) {
                    camera->backgroundColor = Math::Vector3(bgColor[0], bgColor[1], bgColor[2]);
                }
            }
            ImGui::TreePop();
        }

        // Viewport settings
        if (ImGui::TreeNode("Viewport")) {
            ImGui::DragFloat("X", &camera->viewportX, 0.01f, 0.0f, 1.0f);
            ImGui::DragFloat("Y", &camera->viewportY, 0.01f, 0.0f, 1.0f);
            ImGui::DragFloat("Width", &camera->viewportWidth, 0.01f, 0.0f, 1.0f);
            ImGui::DragFloat("Height", &camera->viewportHeight, 0.01f, 0.0f, 1.0f);
            ImGui::TreePop();
        }

    }
}

void EditorLayer::DrawNotesComponent(ECS::Entity entity) {
    bool notesOpen = ImGui::CollapsingHeader("Notes", ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::BeginPopupContextItem("NotesCtx")) {
        if (ImGui::MenuItem("Remove Component")) {
            m_World->RemoveComponent<ECS::NotesComponent>(entity);
            ImGui::EndPopup();
            return;
        }
        ImGui::EndPopup();
    }
    if (notesOpen) {
        ECS::NotesComponent* notes = m_World->GetComponent<ECS::NotesComponent>(entity);
        if (!notes) return;

        // Multi-line text input for notes
        static char notesBuffer[4096];
        strncpy(notesBuffer, notes->notes.c_str(), sizeof(notesBuffer) - 1);
        notesBuffer[sizeof(notesBuffer) - 1] = '\0';

        ImGui::TextDisabled("Developer notes (not exported to builds)");
        if (ImGui::InputTextMultiline("##Notes", notesBuffer, sizeof(notesBuffer),
                                       ImVec2(-1, 100), ImGuiInputTextFlags_AllowTabInput)) {
            notes->notes = notesBuffer;
        }
    }
}

void EditorLayer::DrawTextComponent(ECS::Entity entity) {
    bool textOpen = ImGui::CollapsingHeader("Text", ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::BeginPopupContextItem("TextCtx")) {
        if (ImGui::MenuItem("Remove Component")) {
            m_World->RemoveComponent<ECS::TextComponent>(entity);
            ImGui::EndPopup();
            return;
        }
        ImGui::EndPopup();
    }
    if (textOpen) {
        ECS::TextComponent* text = m_World->GetComponent<ECS::TextComponent>(entity);
        if (!text) return;

        // Multi-line text input
        static char textBuffer[8192];
        strncpy(textBuffer, text->text.c_str(), sizeof(textBuffer) - 1);
        textBuffer[sizeof(textBuffer) - 1] = '\0';

        ImGui::TextDisabled("Text content (multi-line)");
        if (ImGui::InputTextMultiline("##TextContent", textBuffer, sizeof(textBuffer),
                                       ImVec2(-1, 120), ImGuiInputTextFlags_AllowTabInput)) {
            text->text = textBuffer;
            text->dirty = true;
        }

        // Font path with browse button
        static char fontPathBuffer[512];
        strncpy(fontPathBuffer, text->fontPath.c_str(), sizeof(fontPathBuffer) - 1);
        fontPathBuffer[sizeof(fontPathBuffer) - 1] = '\0';

        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 80);
        if (ImGui::InputText("##FontPath", fontPathBuffer, sizeof(fontPathBuffer))) {
            text->fontPath = fontPathBuffer;
            text->dirty = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Browse##Font")) {
            std::string path = FileDialog::OpenFile("Select Font", {{ "Font Files", "*.ttf;*.otf" }});
            if (!path.empty()) {
                text->fontPath = path;
                text->dirty = true;
            }
        }
        ImGui::SameLine();
        ImGui::TextDisabled("Font");

        // Font size
        if (ImGui::DragFloat("Font Size", &text->fontSize, 0.5f, 4.0f, 256.0f)) {
            text->dirty = true;
        }

        // Texture dimensions
        int texW = static_cast<int>(text->textureWidth);
        int texH = static_cast<int>(text->textureHeight);
        if (ImGui::DragInt("Texture Width", &texW, 16, 64, 4096)) {
            text->textureWidth = static_cast<u32>(texW);
            text->dirty = true;
        }
        if (ImGui::DragInt("Texture Height", &texH, 16, 64, 4096)) {
            text->textureHeight = static_cast<u32>(texH);
            text->dirty = true;
        }

        // Wrap width
        if (ImGui::DragFloat("Wrap Width", &text->wrapWidth, 1.0f, 32.0f, 4096.0f)) {
            text->dirty = true;
        }

        // Padding
        if (ImGui::DragFloat("Padding X", &text->paddingX, 0.5f, 0.0f, 256.0f)) {
            text->dirty = true;
        }
        if (ImGui::DragFloat("Padding Y", &text->paddingY, 0.5f, 0.0f, 256.0f)) {
            text->dirty = true;
        }

        // Alignment
        const char* alignItems[] = { "Left", "Center", "Right" };
        int currentAlign = static_cast<int>(text->horizontalAlign);
        if (ImGui::Combo("Alignment", &currentAlign, alignItems, 3)) {
            text->horizontalAlign = static_cast<ECS::TextAlign>(currentAlign);
            text->dirty = true;
        }

        // Text color
        f32 textCol[3] = { text->textColor.x, text->textColor.y, text->textColor.z };
        if (ImGui::ColorEdit3("Text Color", textCol)) {
            text->textColor = Math::Vector3(textCol[0], textCol[1], textCol[2]);
            text->dirty = true;
        }

        // Background color and opacity
        f32 bgCol[3] = { text->bgColor.x, text->bgColor.y, text->bgColor.z };
        if (ImGui::ColorEdit3("Background", bgCol)) {
            text->bgColor = Math::Vector3(bgCol[0], bgCol[1], bgCol[2]);
            text->dirty = true;
        }
        if (ImGui::DragFloat("BG Opacity", &text->bgOpacity, 0.01f, 0.0f, 1.0f)) {
            text->dirty = true;
        }
    }
}

void EditorLayer::DrawWeatherZoneComponent(ECS::Entity entity) {
    bool wzOpen = ImGui::CollapsingHeader("Weather Zone", ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::BeginPopupContextItem("WeatherZoneCtx")) {
        if (ImGui::MenuItem("Remove Component")) {
            m_World->RemoveComponent<ECS::WeatherZoneComponent>(entity);
            ImGui::EndPopup();
            return;
        }
        ImGui::EndPopup();
    }
    if (wzOpen) {
        ECS::WeatherZoneComponent* zone = m_World->GetComponent<ECS::WeatherZoneComponent>(entity);
        if (!zone) return;

        // Bounding box
        f32 extents[3] = { zone->halfExtents.x, zone->halfExtents.y, zone->halfExtents.z };
        if (ImGui::DragFloat3("Half Extents", extents, 0.5f, 0.1f, 500.0f)) {
            zone->halfExtents = Math::Vector3(extents[0], extents[1], extents[2]);
        }
        ImGui::DragInt("Priority", &zone->priority, 1, -100, 100);

        ImGui::Separator();

        // Weather type
        const char* weatherTypes[] = { "Clear", "Cloudy", "Rain", "Heavy Rain", "Snow", "Fog", "Storm" };
        int currentWeather = static_cast<int>(zone->weatherType);
        if (ImGui::Combo("Weather Type", &currentWeather, weatherTypes, 7)) {
            zone->weatherType = static_cast<u32>(currentWeather);
        }

        // Show relevant controls based on weather type
        if (zone->weatherType == 2 || zone->weatherType == 3 || zone->weatherType == 6) {
            ImGui::SliderFloat("Rain Intensity", &zone->rainIntensity, 0.0f, 1.0f);
        }
        if (zone->weatherType == 4) {
            ImGui::SliderFloat("Snow Intensity", &zone->snowIntensity, 0.0f, 1.0f);
        }
        if (zone->weatherType == 6) {
            ImGui::Checkbox("Lightning Enabled", &zone->lightningEnabled);
            if (zone->lightningEnabled) {
                ImGui::DragFloat("Lightning Min Interval", &zone->lightningMinInterval, 0.1f, 0.1f, zone->lightningMaxInterval);
                ImGui::DragFloat("Lightning Max Interval", &zone->lightningMaxInterval, 0.1f, zone->lightningMinInterval, 60.0f);
            }
        }

        // Wind settings (for rain/snow/storm)
        if (zone->weatherType >= 2 && zone->weatherType != 5) {
            ImGui::Spacing();
            ImGui::Text("Wind");
            f32 windDir[3] = { zone->windDirection.x, zone->windDirection.y, zone->windDirection.z };
            if (ImGui::DragFloat3("Wind Direction", windDir, 0.05f, -1.0f, 1.0f)) {
                zone->windDirection = Math::Vector3(windDir[0], windDir[1], windDir[2]);
            }
            ImGui::DragFloat("Wind Strength", &zone->windStrength, 0.1f, 0.0f, 20.0f);
        }

        // Fog settings
        if (zone->weatherType >= 1) {
            ImGui::Spacing();
            ImGui::Text("Fog Settings");
            ImGui::SliderFloat("Fog Density", &zone->fogDensity, 0.0f, 1.0f);
            f32 fogCol[3] = { zone->fogColor.x, zone->fogColor.y, zone->fogColor.z };
            if (ImGui::ColorEdit3("Fog Color", fogCol)) {
                zone->fogColor = Math::Vector3(fogCol[0], fogCol[1], fogCol[2]);
            }
            ImGui::DragFloat("Fog Start", &zone->fogStart, 1.0f, 0.0f, zone->fogEnd);
            ImGui::DragFloat("Fog End", &zone->fogEnd, 1.0f, zone->fogStart, 500.0f);
        }
    }
}

void EditorLayer::DrawWaterVolumeComponent(ECS::Entity entity) {
    bool wvOpen = ImGui::CollapsingHeader("Water Volume", ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::BeginPopupContextItem("WaterVolumeCtx")) {
        if (ImGui::MenuItem("Remove Component")) {
            m_World->RemoveComponent<ECS::WaterVolumeComponent>(entity);
            ImGui::EndPopup();
            return;
        }
        ImGui::EndPopup();
    }
    if (wvOpen) {
        ECS::WaterVolumeComponent* volume = m_World->GetComponent<ECS::WaterVolumeComponent>(entity);
        if (!volume) return;

        // Track changes that require mesh regeneration
        auto oldExtents = volume->halfExtents;
        auto oldWaterType = volume->waterType;

        // Bounding box
        f32 extents[3] = { volume->halfExtents.x, volume->halfExtents.y, volume->halfExtents.z };
        if (ImGui::DragFloat3("Half Extents", extents, 0.5f, 0.1f, 500.0f)) {
            volume->halfExtents = Math::Vector3(extents[0], extents[1], extents[2]);
        }
        ImGui::DragInt("Priority", &volume->priority, 1, -100, 100);

        ImGui::Separator();

        // Water type
        const char* waterTypeNames[] = { "Lake", "Ocean", "River", "Pond" };
        int currentType = static_cast<int>(volume->waterType);
        if (ImGui::Combo("Water Type", &currentType, waterTypeNames, 4)) {
            volume->waterType = static_cast<ECS::WaterType>(currentType);
        }

        // Water settings
        f32 waterCol[3] = { volume->waterColor.x, volume->waterColor.y, volume->waterColor.z };
        if (ImGui::ColorEdit3("Water Color", waterCol)) {
            volume->waterColor = Math::Vector3(waterCol[0], waterCol[1], waterCol[2]);
        }
        ImGui::SliderFloat("Opacity", &volume->opacity, 0.0f, 1.0f);
        ImGui::DragFloat("Wave Speed", &volume->waveSpeed, 0.1f, 0.0f, 10.0f);
        ImGui::DragFloat("Wave Height", &volume->waveHeight, 0.01f, 0.0f, 2.0f);

        ImGui::Separator();

        // Shore & Foam
        ImGui::Checkbox("Enable Shore Foam", &volume->enableShore);
        if (volume->enableShore) {
            ImGui::SliderFloat("Shore Width", &volume->shoreWidth, 0.0f, 0.5f, "%.2f");
            ImGui::SliderFloat("Foam Intensity", &volume->foamIntensity, 0.0f, 1.0f, "%.2f");
            ImGui::DragFloat("Foam Scale", &volume->foamScale, 0.5f, 1.0f, 50.0f, "%.1f");
            f32 shoreCol[3] = { volume->shoreColor.x, volume->shoreColor.y, volume->shoreColor.z };
            if (ImGui::ColorEdit3("Shore Color", shoreCol)) {
                volume->shoreColor = Math::Vector3(shoreCol[0], shoreCol[1], shoreCol[2]);
            }
        }

        ImGui::Separator();

        // Freeze Settings
        if (ImGui::TreeNodeEx("Freeze Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
            f32 iceCol[3] = { volume->iceColor.x, volume->iceColor.y, volume->iceColor.z };
            if (ImGui::ColorEdit3("Ice Color", iceCol)) {
                volume->iceColor = Math::Vector3(iceCol[0], iceCol[1], iceCol[2]);
            }
            ImGui::SliderFloat("Ice Opacity", &volume->iceOpacity, 0.0f, 1.0f);
            ImGui::DragFloat("Freeze Rate", &volume->freezeRate, 0.01f, 0.01f, 5.0f, "%.2f");
            ImGui::DragFloat("Thaw Rate", &volume->thawRate, 0.01f, 0.01f, 5.0f, "%.2f");

            // Read-only freeze progress indicator
            ImGui::Spacing();
            ImGui::ProgressBar(volume->freezeProgress, ImVec2(-1, 0),
                volume->isFrozen ? "Frozen" : (volume->freezeProgress > 0.01f ? "Freezing..." : "Liquid"));
            ImGui::TreePop();
        }

        // Regenerate mesh if extents or water type changed
        bool needsRegen = (oldExtents.x != volume->halfExtents.x ||
                          oldExtents.y != volume->halfExtents.y ||
                          oldExtents.z != volume->halfExtents.z ||
                          oldWaterType != volume->waterType);
        if (needsRegen) {
            volume->meshCreated = false;
            m_World->RemoveComponent<ECS::MeshComponent>(entity);
            m_World->RemoveComponent<ECS::MaterialComponent>(entity);
            if (m_RenderSystem) {
                m_RenderSystem->OnEntityRemoved(entity);
            }
        }

        // Info
        ImGui::Spacing();
        ImGui::TextDisabled("Water surface is at entity's Y position");
        ImGui::TextDisabled("Half Extents define the area and depth");
    }
}

void EditorLayer::DrawGrassVolumeComponent(ECS::Entity entity) {
    bool gvOpen = ImGui::CollapsingHeader("Grass Volume", ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::BeginPopupContextItem("GrassVolumeCtx")) {
        if (ImGui::MenuItem("Remove Component")) {
            m_World->RemoveComponent<ECS::GrassVolumeComponent>(entity);
            ImGui::EndPopup();
            return;
        }
        ImGui::EndPopup();
    }
    if (gvOpen) {
        ECS::GrassVolumeComponent* grass = m_World->GetComponent<ECS::GrassVolumeComponent>(entity);
        if (!grass) return;

        f32 extents[3] = { grass->halfExtents.x, grass->halfExtents.y, grass->halfExtents.z };
        if (ImGui::DragFloat3("Half Extents", extents, 0.5f, 0.1f, 500.0f)) {
            grass->halfExtents = Math::Vector3(extents[0], extents[1], extents[2]);
        }

        int density = static_cast<int>(grass->density);
        if (ImGui::DragInt("Density", &density, 100, 100, 50000)) {
            grass->density = static_cast<u32>(density);
        }

        ImGui::Separator();

        // Blade geometry
        ImGui::DragFloat("Blade Height", &grass->bladeHeight, 0.01f, 0.01f, 2.0f);
        ImGui::DragFloat("Height Variance", &grass->bladeHeightVariance, 0.01f, 0.0f, 1.0f);
        ImGui::DragFloat("Blade Width", &grass->bladeWidth, 0.005f, 0.005f, 0.5f);

        ImGui::Separator();

        // Colors
        f32 baseCol[3] = { grass->baseColor.x, grass->baseColor.y, grass->baseColor.z };
        if (ImGui::ColorEdit3("Base Color", baseCol)) {
            grass->baseColor = Math::Vector3(baseCol[0], baseCol[1], baseCol[2]);
        }
        f32 tipCol[3] = { grass->tipColor.x, grass->tipColor.y, grass->tipColor.z };
        if (ImGui::ColorEdit3("Tip Color", tipCol)) {
            grass->tipColor = Math::Vector3(tipCol[0], tipCol[1], tipCol[2]);
        }

        ImGui::Separator();
        ImGui::DragFloat("Wind Sway", &grass->windSwayStrength, 0.05f, 0.0f, 5.0f);

        ImGui::Spacing();
        ImGui::TextDisabled("Grass sits on the XZ plane at entity's Y position");
    }
}

void EditorLayer::DrawShrubVolumeComponent(ECS::Entity entity) {
    bool svOpen = ImGui::CollapsingHeader("Shrub Volume", ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::BeginPopupContextItem("ShrubVolumeCtx")) {
        if (ImGui::MenuItem("Remove Component")) {
            m_World->RemoveComponent<ECS::ShrubVolumeComponent>(entity);
            ImGui::EndPopup();
            return;
        }
        ImGui::EndPopup();
    }
    if (svOpen) {
        ECS::ShrubVolumeComponent* shrub = m_World->GetComponent<ECS::ShrubVolumeComponent>(entity);
        if (!shrub) return;

        f32 extents[3] = { shrub->halfExtents.x, shrub->halfExtents.y, shrub->halfExtents.z };
        if (ImGui::DragFloat3("Half Extents", extents, 0.5f, 0.1f, 500.0f)) {
            shrub->halfExtents = Math::Vector3(extents[0], extents[1], extents[2]);
        }

        int density = static_cast<int>(shrub->density);
        if (ImGui::DragInt("Density", &density, 50, 10, 10000)) {
            shrub->density = static_cast<u32>(density);
        }

        ImGui::Separator();

        ImGui::DragFloat("Shrub Height", &shrub->shrubHeight, 0.01f, 0.05f, 3.0f);
        ImGui::DragFloat("Height Variance", &shrub->heightVariance, 0.01f, 0.0f, 1.0f);
        ImGui::DragFloat("Width", &shrub->width, 0.01f, 0.05f, 2.0f);

        ImGui::Separator();

        f32 baseCol[3] = { shrub->baseColor.x, shrub->baseColor.y, shrub->baseColor.z };
        if (ImGui::ColorEdit3("Base Color", baseCol)) {
            shrub->baseColor = Math::Vector3(baseCol[0], baseCol[1], baseCol[2]);
        }
        f32 tipCol[3] = { shrub->tipColor.x, shrub->tipColor.y, shrub->tipColor.z };
        if (ImGui::ColorEdit3("Tip Color", tipCol)) {
            shrub->tipColor = Math::Vector3(tipCol[0], tipCol[1], tipCol[2]);
        }

        ImGui::Separator();
        ImGui::DragFloat("Wind Sway", &shrub->windSwayStrength, 0.05f, 0.0f, 5.0f);

        ImGui::Spacing();
        ImGui::TextDisabled("Shrubs sit on the XZ plane at entity's Y position");
    }
}

void EditorLayer::DrawTreeVolumeComponent(ECS::Entity entity) {
    bool tvOpen = ImGui::CollapsingHeader("Tree Volume", ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::BeginPopupContextItem("TreeVolumeCtx")) {
        if (ImGui::MenuItem("Remove Component")) {
            m_World->RemoveComponent<ECS::TreeVolumeComponent>(entity);
            ImGui::EndPopup();
            return;
        }
        ImGui::EndPopup();
    }
    if (tvOpen) {
        ECS::TreeVolumeComponent* tree = m_World->GetComponent<ECS::TreeVolumeComponent>(entity);
        if (!tree) return;

        f32 extents[3] = { tree->halfExtents.x, tree->halfExtents.y, tree->halfExtents.z };
        if (ImGui::DragFloat3("Half Extents", extents, 0.5f, 0.1f, 500.0f)) {
            tree->halfExtents = Math::Vector3(extents[0], extents[1], extents[2]);
        }

        int density = static_cast<int>(tree->density);
        if (ImGui::DragInt("Density", &density, 10, 1, 5000)) {
            tree->density = static_cast<u32>(density);
        }

        const char* treeTypes[] = { "Deciduous", "Evergreen" };
        int treeTypeIdx = static_cast<int>(tree->treeType);
        if (ImGui::Combo("Tree Type", &treeTypeIdx, treeTypes, 2)) {
            tree->treeType = static_cast<ECS::TreeType>(treeTypeIdx);
        }

        ImGui::Separator();
        ImGui::Text("Trunk");
        ImGui::DragFloat("Trunk Height", &tree->trunkHeight, 0.05f, 0.1f, 10.0f);
        ImGui::DragFloat("Trunk Width", &tree->trunkWidth, 0.01f, 0.02f, 1.0f);
        f32 trunkCol[3] = { tree->trunkColor.x, tree->trunkColor.y, tree->trunkColor.z };
        if (ImGui::ColorEdit3("Trunk Color", trunkCol)) {
            tree->trunkColor = Math::Vector3(trunkCol[0], trunkCol[1], trunkCol[2]);
        }

        ImGui::Separator();
        ImGui::Text("Canopy");
        ImGui::DragFloat("Canopy Radius", &tree->canopyRadius, 0.05f, 0.1f, 5.0f);
        ImGui::DragFloat("Canopy Offset", &tree->canopyOffset, 0.05f, 0.0f, 10.0f);
        f32 canopyBaseCol[3] = { tree->canopyBaseColor.x, tree->canopyBaseColor.y, tree->canopyBaseColor.z };
        if (ImGui::ColorEdit3("Canopy Base", canopyBaseCol)) {
            tree->canopyBaseColor = Math::Vector3(canopyBaseCol[0], canopyBaseCol[1], canopyBaseCol[2]);
        }
        f32 canopyTipCol[3] = { tree->canopyTipColor.x, tree->canopyTipColor.y, tree->canopyTipColor.z };
        if (ImGui::ColorEdit3("Canopy Tip", canopyTipCol)) {
            tree->canopyTipColor = Math::Vector3(canopyTipCol[0], canopyTipCol[1], canopyTipCol[2]);
        }

        if (tree->treeType == ECS::TreeType::Deciduous) {
            ImGui::Separator();
            ImGui::Text("Seasonal Colors");
            f32 springCol[3] = { tree->springCanopyColor.x, tree->springCanopyColor.y, tree->springCanopyColor.z };
            if (ImGui::ColorEdit3("Spring", springCol)) {
                tree->springCanopyColor = Math::Vector3(springCol[0], springCol[1], springCol[2]);
            }
            f32 summerCol[3] = { tree->summerCanopyColor.x, tree->summerCanopyColor.y, tree->summerCanopyColor.z };
            if (ImGui::ColorEdit3("Summer", summerCol)) {
                tree->summerCanopyColor = Math::Vector3(summerCol[0], summerCol[1], summerCol[2]);
            }
            f32 fallCol[3] = { tree->fallCanopyColor.x, tree->fallCanopyColor.y, tree->fallCanopyColor.z };
            if (ImGui::ColorEdit3("Fall", fallCol)) {
                tree->fallCanopyColor = Math::Vector3(fallCol[0], fallCol[1], fallCol[2]);
            }
            ImGui::TextDisabled("Winter: bare branches (no canopy)");
        }

        ImGui::Separator();
        ImGui::Text("Height Variance");
        ImGui::DragFloat("Min Scale", &tree->minHeightScale, 0.05f, 0.1f, 2.0f);
        ImGui::DragFloat("Max Scale", &tree->maxHeightScale, 0.05f, 0.1f, 3.0f);
        if (tree->minHeightScale > tree->maxHeightScale) tree->maxHeightScale = tree->minHeightScale;

        ImGui::Separator();
        ImGui::DragFloat("Wind Sway", &tree->windSwayStrength, 0.05f, 0.0f, 5.0f);

        ImGui::Separator();
        ImGui::Text("Textures");
        // Bark texture
        if (!tree->barkTexturePath.empty()) {
            size_t lastSlash = tree->barkTexturePath.find_last_of("/\\");
            std::string filename = (lastSlash != std::string::npos) ? tree->barkTexturePath.substr(lastSlash + 1) : tree->barkTexturePath;
            ImGui::Text("Bark: %s", filename.c_str());
            ImGui::SameLine();
            if (ImGui::SmallButton("X##BarkTex")) {
                tree->barkTexturePath.clear();
            }
        } else {
            if (ImGui::Button("Load Bark Texture")) {
                std::string path = FileDialog::OpenFile("Bark Texture", {{ "Images", "*.png;*.jpg;*.jpeg;*.bmp;*.tga" }});
                if (!path.empty()) tree->barkTexturePath = path;
            }
        }
        // Canopy texture
        if (!tree->canopyTexturePath.empty()) {
            size_t lastSlash = tree->canopyTexturePath.find_last_of("/\\");
            std::string filename = (lastSlash != std::string::npos) ? tree->canopyTexturePath.substr(lastSlash + 1) : tree->canopyTexturePath;
            ImGui::Text("Canopy: %s", filename.c_str());
            ImGui::SameLine();
            if (ImGui::SmallButton("X##CanopyTex")) {
                tree->canopyTexturePath.clear();
            }
        } else {
            if (ImGui::Button("Load Canopy Texture")) {
                std::string path = FileDialog::OpenFile("Canopy Texture", {{ "Images", "*.png;*.jpg;*.jpeg;*.bmp;*.tga" }});
                if (!path.empty()) tree->canopyTexturePath = path;
            }
        }

        ImGui::Spacing();
        ImGui::TextDisabled("Trees sit on the XZ plane at entity's Y position");
    }
}

void EditorLayer::DrawTerrainComponent(ECS::Entity entity) {
    bool open = ImGui::CollapsingHeader("3D Terrain", ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::BeginPopupContextItem("TerrainCtx")) {
        if (ImGui::MenuItem("Remove Component")) {
            m_World->RemoveComponent<ECS::TerrainComponent>(entity);
            ImGui::EndPopup();
            return;
        }
        ImGui::EndPopup();
    }
    if (open) {
        ECS::TerrainComponent* terrain = m_World->GetComponent<ECS::TerrainComponent>(entity);
        if (!terrain) return;

        int w = static_cast<int>(terrain->gridWidth);
        int h = static_cast<int>(terrain->gridHeight);
        bool resized = false;
        resized |= ImGui::DragInt("Grid Width", &w, 1, 4, 512);
        resized |= ImGui::DragInt("Grid Height", &h, 1, 4, 512);
        if (resized) {
            terrain->gridWidth = static_cast<u32>(w);
            terrain->gridHeight = static_cast<u32>(h);
            terrain->InitializeFlat(0.0f);
        }
        ImGui::DragFloat("Cell Size", &terrain->cellSize, 0.1f, 0.1f, 10.0f);
        ImGui::DragFloat("Max Height", &terrain->maxHeight, 1.0f, 1.0f, 200.0f);

        if (terrain->heightmap.empty()) {
            if (ImGui::Button("Initialize Flat")) {
                terrain->InitializeFlat(0.0f);
            }
        }

        ImGui::Separator();
        ImGui::Text("Terrain Brush");
        ImGui::Checkbox("Edit Mode", &m_TerrainEditMode);

        if (m_TerrainEditMode) {
            const char* brushModes[] = { "Raise", "Lower", "Flatten", "Smooth", "Paint" };
            int brushIdx = static_cast<int>(m_TerrainBrush.mode);
            if (ImGui::Combo("Brush Mode", &brushIdx, brushModes, 5)) {
                m_TerrainBrush.mode = static_cast<TerrainBrushMode>(brushIdx);
            }
            ImGui::DragFloat("Radius", &m_TerrainBrush.radius, 0.1f, 0.5f, 50.0f);
            ImGui::DragFloat("Strength", &m_TerrainBrush.strength, 0.01f, 0.01f, 10.0f);
            ImGui::DragFloat("Falloff", &m_TerrainBrush.falloff, 0.01f, 0.0f, 1.0f);

            if (m_TerrainBrush.mode == TerrainBrushMode::Flatten) {
                ImGui::DragFloat("Flatten Height", &m_TerrainBrush.flattenHeight, 0.1f, 0.0f, terrain->maxHeight);
            }
            if (m_TerrainBrush.mode == TerrainBrushMode::Paint) {
                int layer = static_cast<int>(m_TerrainBrush.paintLayer);
                if (ImGui::DragInt("Paint Layer", &layer, 1, 0, 3)) {
                    m_TerrainBrush.paintLayer = static_cast<u32>(layer);
                }
            }
        }

        ImGui::Separator();
        ImGui::Text("Texture Layers");
        for (int i = 0; i < 4; ++i) {
            ImGui::PushID(i);
            char label[32];
            std::snprintf(label, sizeof(label), "Layer %d", i);
            if (ImGui::TreeNode(label)) {
                if (!terrain->layers[i].texturePath.empty()) {
                    ImGui::Text("Texture: %s", terrain->layers[i].texturePath.c_str());
                    if (ImGui::SmallButton("Clear")) {
                        terrain->layers[i].texturePath.clear();
                    }
                } else {
                    if (ImGui::Button("Load Texture")) {
                        std::string path = FileDialog::OpenFile("Texture", {{ "Images", "*.png;*.jpg;*.jpeg;*.bmp;*.tga" }});
                        if (!path.empty()) terrain->layers[i].texturePath = path;
                    }
                }
                ImGui::DragFloat("Tile Scale", &terrain->layers[i].tileScale, 0.1f, 0.1f, 100.0f);
                ImGui::TreePop();
            }
            ImGui::PopID();
        }
    }
}

void EditorLayer::DrawTerrain2DComponent(ECS::Entity entity) {
    bool open = ImGui::CollapsingHeader("2D Terrain", ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::BeginPopupContextItem("Terrain2DCtx")) {
        if (ImGui::MenuItem("Remove Component")) {
            m_World->RemoveComponent<ECS::Terrain2DComponent>(entity);
            ImGui::EndPopup();
            return;
        }
        ImGui::EndPopup();
    }
    if (open) {
        ECS::Terrain2DComponent* terrain = m_World->GetComponent<ECS::Terrain2DComponent>(entity);
        if (!terrain) return;

        ImGui::DragFloat("Depth", &terrain->depth, 0.1f, 0.1f, 50.0f);
        ImGui::DragFloat("UV Scale", &terrain->uvScale, 0.01f, 0.01f, 10.0f);
        ImGui::Checkbox("Auto Colliders", &terrain->autoColliders);

        ImGui::Separator();
        ImGui::Text("Control Points (%zu)", terrain->controlPoints.size());

        if (ImGui::Button("Add Point")) {
            f32 x = terrain->controlPoints.empty() ? 0.0f : terrain->controlPoints.back().x + 2.0f;
            terrain->AddPoint(Math::Vector2(x, 0.0f));
        }

        for (usize i = 0; i < terrain->controlPoints.size(); ++i) {
            ImGui::PushID(static_cast<int>(i));
            f32 pt[2] = { terrain->controlPoints[i].x, terrain->controlPoints[i].y };
            char label[32];
            std::snprintf(label, sizeof(label), "Point %zu", i);
            if (ImGui::DragFloat2(label, pt, 0.1f)) {
                terrain->controlPoints[i] = Math::Vector2(pt[0], pt[1]);
                terrain->meshDirty = true;
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("X") && terrain->controlPoints.size() > 2) {
                terrain->controlPoints.erase(terrain->controlPoints.begin() + static_cast<std::ptrdiff_t>(i));
                terrain->meshDirty = true;
                ImGui::PopID();
                break;
            }
            ImGui::PopID();
        }
    }
}

void EditorLayer::DrawVegetationComponent(ECS::Entity entity) {
    bool vegOpen = ImGui::CollapsingHeader("Vegetation", ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::BeginPopupContextItem("VegetationCtx")) {
        if (ImGui::MenuItem("Remove Component")) {
            m_World->RemoveComponent<ECS::VegetationComponent>(entity);
            ImGui::EndPopup();
            return;
        }
        ImGui::EndPopup();
    }
    if (vegOpen) {
        ECS::VegetationComponent* veg = m_World->GetComponent<ECS::VegetationComponent>(entity);
        if (!veg) return;

        ImGui::DragFloat("Sway Strength", &veg->swayStrength, 0.05f, 0.0f, 5.0f);
        ImGui::DragFloat("Sway Frequency", &veg->swayFrequency, 0.05f, 0.0f, 5.0f);
        ImGui::Checkbox("Use Vertex Color Weight", &veg->useVertexColorWeight);

        ImGui::Spacing();
        ImGui::TextDisabled("Red vertex color channel = sway weight");
        ImGui::TextDisabled("Trunk (red=0) stays still, leaves (red=1) sway");
    }
}

void EditorLayer::DrawCameraTriggerComponent(ECS::Entity entity) {
    if (ImGui::CollapsingHeader("Camera Trigger", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto* trigger = m_World->GetComponent<ECS::CameraTriggerComponent>(entity);
        if (!trigger) return;

        // Half-extents
        f32 halfExt[3] = { trigger->halfExtents.x, trigger->halfExtents.y, trigger->halfExtents.z };
        if (ImGui::DragFloat3("Half Extents", halfExt, 0.5f, 0.1f, 500.0f)) {
            trigger->halfExtents = Math::Vector3(halfExt[0], halfExt[1], halfExt[2]);
        }

        // Priority
        ImGui::DragInt("Priority", &trigger->priority, 1, -100, 100);

        // Blend time
        ImGui::DragFloat("Blend Time", &trigger->blendTime, 0.05f, 0.0f, 5.0f, "%.2f s");

        // Target camera dropdown
        std::vector<ECS::Entity> cameraEntities;
        if (m_World) {
            for (ECS::Entity e : m_World->GetAllEntities()) {
                if (m_World->HasComponent<ECS::CameraComponent>(e)) {
                    cameraEntities.push_back(e);
                }
            }
        }

        std::string currentName = "(None)";
        if (trigger->targetCamera != ECS::INVALID_ENTITY) {
            if (m_World->HasComponent<ECS::NameComponent>(trigger->targetCamera)) {
                currentName = m_World->GetComponent<ECS::NameComponent>(trigger->targetCamera)->name;
            } else {
                currentName = "Camera (Entity " + std::to_string(trigger->targetCamera) + ")";
            }
        }

        ImGui::SetNextItemWidth(200);
        if (ImGui::BeginCombo("Target Camera", currentName.c_str())) {
            // None option
            if (ImGui::Selectable("(None)", trigger->targetCamera == ECS::INVALID_ENTITY)) {
                trigger->targetCamera = ECS::INVALID_ENTITY;
            }
            for (ECS::Entity camEntity : cameraEntities) {
                std::string name;
                if (m_World->HasComponent<ECS::NameComponent>(camEntity)) {
                    name = m_World->GetComponent<ECS::NameComponent>(camEntity)->name;
                } else {
                    name = "Camera (Entity " + std::to_string(camEntity) + ")";
                }
                bool isSelected = (camEntity == trigger->targetCamera);
                if (ImGui::Selectable(name.c_str(), isSelected)) {
                    trigger->targetCamera = camEntity;
                }
                if (isSelected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        ImGui::Spacing();
        ImGui::TextDisabled("Zone activates target camera when player enters");

        // Remove component button
        if (ImGui::Button("Remove##CameraTrigger")) {
            m_World->RemoveComponent<ECS::CameraTriggerComponent>(entity);
        }
    }
}

void EditorLayer::DrawTemperatureZoneComponent(ECS::Entity entity) {
    if (ImGui::CollapsingHeader("Temperature Zone", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto* tempZone = m_World->GetComponent<ECS::TemperatureZoneComponent>(entity);
        if (!tempZone) return;

        // Half-extents
        f32 halfExt[3] = { tempZone->halfExtents.x, tempZone->halfExtents.y, tempZone->halfExtents.z };
        if (ImGui::DragFloat3("Half Extents##TempZone", halfExt, 0.5f, 0.1f, 500.0f)) {
            tempZone->halfExtents = Math::Vector3(halfExt[0], halfExt[1], halfExt[2]);
        }

        // Temperature slider with color-coded display
        ImGui::DragFloat("Temperature (C)", &tempZone->temperature, 0.5f, -40.0f, 50.0f, "%.1f");

        // Visual temperature indicator
        if (tempZone->IsFreezing()) {
            ImGui::TextColored(ImVec4(0.5f, 0.7f, 1.0f, 1.0f), "Freezing (Snow/Ice)");
        } else if (tempZone->IsNearFreezing()) {
            ImGui::TextColored(ImVec4(0.7f, 0.8f, 0.9f, 1.0f), "Near Freezing (Sleet/Mix)");
        } else {
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.4f, 1.0f), "Warm (Rain)");
        }

        // Priority
        ImGui::DragInt("Priority##TempZone", &tempZone->priority, 1, -100, 100);

        ImGui::Spacing();
        ImGui::TextDisabled("Affects precipitation type in overlapping weather zones");

        // Remove component
        if (ImGui::Button("Remove##TemperatureZone")) {
            m_World->RemoveComponent<ECS::TemperatureZoneComponent>(entity);
        }
    }
}

void EditorLayer::DrawGravityZoneComponent(ECS::Entity entity) {
    if (ImGui::CollapsingHeader("Gravity Zone", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto* zone = m_World->GetComponent<ECS::GravityZoneComponent>(entity);
        if (!zone) return;

        ImGui::Checkbox("Active##GravZone", &zone->isActive);

        // Shape selector
        const char* shapes[] = { "Box", "Sphere" };
        int shapeIdx = static_cast<int>(zone->shape);
        if (ImGui::Combo("Shape##GravZone", &shapeIdx, shapes, 2)) {
            zone->shape = static_cast<ECS::GravityZoneShape>(shapeIdx);
        }

        if (zone->shape == ECS::GravityZoneShape::Sphere) {
            // Sphere: single radius
            f32 radius = zone->halfExtents.x;
            if (ImGui::DragFloat("Radius##GravZone", &radius, 0.5f, 0.1f, 500.0f)) {
                zone->halfExtents = Math::Vector3(radius, radius, radius);
            }
        } else {
            // Box: half-extents
            f32 halfExt[3] = { zone->halfExtents.x, zone->halfExtents.y, zone->halfExtents.z };
            if (ImGui::DragFloat3("Half Extents##GravZone", halfExt, 0.5f, 0.1f, 500.0f)) {
                zone->halfExtents = Math::Vector3(halfExt[0], halfExt[1], halfExt[2]);
            }
        }

        // Mode selector
        const char* modes[] = { "Directional", "Point (Planetary)" };
        int modeIdx = static_cast<int>(zone->mode);
        if (ImGui::Combo("Mode##GravZone", &modeIdx, modes, 2)) {
            zone->mode = static_cast<ECS::GravityZoneMode>(modeIdx);
        }

        if (zone->mode == ECS::GravityZoneMode::Directional) {
            // Gravity direction
            f32 dir[3] = { zone->gravityDirection.x, zone->gravityDirection.y, zone->gravityDirection.z };
            if (ImGui::DragFloat3("Direction##GravZone", dir, 0.01f, -1.0f, 1.0f)) {
                zone->gravityDirection = Math::Vector3(dir[0], dir[1], dir[2]);
                f32 len = zone->gravityDirection.Length();
                if (len > 0.001f) {
                    zone->gravityDirection = zone->gravityDirection * (1.0f / len);
                }
            }

            // Quick direction presets
            ImGui::Text("Presets:");
            ImGui::SameLine();
            if (ImGui::SmallButton("Down")) { zone->gravityDirection = Math::Vector3(0, -1, 0); }
            ImGui::SameLine();
            if (ImGui::SmallButton("Up")) { zone->gravityDirection = Math::Vector3(0, 1, 0); }
            ImGui::SameLine();
            if (ImGui::SmallButton("Left")) { zone->gravityDirection = Math::Vector3(-1, 0, 0); }
            ImGui::SameLine();
            if (ImGui::SmallButton("Right")) { zone->gravityDirection = Math::Vector3(1, 0, 0); }
        } else {
            ImGui::TextDisabled("Gravity pulls toward this entity's position");
            ImGui::TextDisabled("(Mario Galaxy-style planetary gravity)");

            // For sphere shape + point mode, this is a classic planetary body
            if (zone->shape == ECS::GravityZoneShape::Sphere) {
                ImGui::TextDisabled("Tip: Use Sphere shape for natural planet gravity");
            }
        }

        // Gravity strength
        ImGui::DragFloat("Strength##GravZone", &zone->gravityStrength, 0.1f, 0.0f, 100.0f, "%.2f m/s^2");
        ImGui::SameLine();
        if (ImGui::SmallButton("Zero-G")) { zone->gravityStrength = 0.0f; }

        // Planet presets (for point mode)
        if (zone->mode == ECS::GravityZoneMode::Point) {
            ImGui::Text("Planet Presets:");
            ImGui::SameLine();
            if (ImGui::SmallButton("Earth##PP")) { zone->gravityStrength = 9.81f; }
            ImGui::SameLine();
            if (ImGui::SmallButton("Moon##PP")) { zone->gravityStrength = 1.62f; }
            ImGui::SameLine();
            if (ImGui::SmallButton("Jupiter##PP")) { zone->gravityStrength = 24.79f; }
        }

        // Priority
        ImGui::DragInt("Priority##GravZone", &zone->priority, 1, -100, 100);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Higher priority zones override lower ones when overlapping");

        // Remove component
        if (ImGui::Button("Remove##GravityZone")) {
            m_World->RemoveComponent<ECS::GravityZoneComponent>(entity);
        }
    }
}

void EditorLayer::DrawConsolePanel() {
    ImGui::Begin("Console");

    // Console output
    ImGui::BeginChild("ConsoleOutput", ImVec2(0, -ImGui::GetFrameHeightWithSpacing()), true);
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

void EditorLayer::DrawAssetBrowserPanel() {
    ImGui::Begin("Asset Browser");

    // Initialize browse path to current working directory
    if (m_AssetBrowserPath.empty()) {
        m_AssetBrowserPath = ".";
    }

    // Navigation bar
    ImGui::Text("Path: %s", m_AssetBrowserPath.c_str());
    ImGui::SameLine();
    if (ImGui::Button("Up")) {
        // Go up one directory
        auto pos = m_AssetBrowserPath.find_last_of("/\\");
        if (pos != std::string::npos && pos > 0) {
            m_AssetBrowserPath = m_AssetBrowserPath.substr(0, pos);
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Refresh")) {
        // Just re-scan (the loop below does it each frame, but this clears selection)
        m_AssetBrowserSelected.clear();
    }

    ImGui::Separator();

    // Import buttons
    if (ImGui::Button("Import Model...")) {
        std::vector<FileFilter> filters = {
            { "3D Models", "*.gltf;*.glb;*.fbx;*.obj" },
            { "All Files", "*.*" }
        };
        std::string path = FileDialog::OpenFile("Import Model", filters);
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
        std::string path = FileDialog::OpenFile("Open Scene", filters);
        if (!path.empty()) {
            OpenScene(path);
        }
    }

    ImGui::Separator();

    // File listing
    ImGui::BeginChild("FileList", ImVec2(0, 0), true);

    // List directories and files using std::filesystem
    try {
        namespace fs = std::filesystem;
        fs::path browsePath(m_AssetBrowserPath);

        if (fs::exists(browsePath) && fs::is_directory(browsePath)) {
            // Directories first
            for (const auto& entry : fs::directory_iterator(browsePath)) {
                if (entry.is_directory()) {
                    std::string name = "[DIR] " + entry.path().filename().string();
                    if (ImGui::Selectable(name.c_str(), false, ImGuiSelectableFlags_AllowDoubleClick)) {
                        if (ImGui::IsMouseDoubleClicked(0)) {
                            m_AssetBrowserPath = entry.path().string();
                        }
                    }
                }
            }

            // Then files
            for (const auto& entry : fs::directory_iterator(browsePath)) {
                if (!entry.is_regular_file()) continue;

                std::string ext = entry.path().extension().string();
                std::string filename = entry.path().filename().string();

                // Color-code by file type
                bool isModel = (ext == ".gltf" || ext == ".glb" || ext == ".fbx" || ext == ".obj");
                bool isScene = (ext == ".enjin" || ext == ".json");
                bool isShader = (ext == ".vert" || ext == ".frag" || ext == ".glsl" || ext == ".spv");
                bool isImage = (ext == ".png" || ext == ".jpg" || ext == ".tga" || ext == ".bmp");

                if (isModel) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.8f, 1.0f, 1.0f));
                else if (isScene) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 1.0f, 0.4f, 1.0f));
                else if (isShader) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.4f, 1.0f));
                else if (isImage) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.6f, 1.0f, 1.0f));

                bool selected = (m_AssetBrowserSelected == entry.path().string());
                if (ImGui::Selectable(filename.c_str(), selected, ImGuiSelectableFlags_AllowDoubleClick)) {
                    m_AssetBrowserSelected = entry.path().string();

                    if (ImGui::IsMouseDoubleClicked(0)) {
                        // Double-click: import models, open scenes
                        if (isModel) {
                            ImportModel(entry.path().string());
                        } else if (isScene) {
                            OpenScene(entry.path().string());
                        }
                    }
                }

                if (isModel || isScene || isShader || isImage) {
                    ImGui::PopStyleColor();
                }
            }
        } else {
            ImGui::TextDisabled("Directory not found");
            if (ImGui::Button("Reset to Project Root")) {
                m_AssetBrowserPath = ".";
            }
        }
    } catch (...) {
        ImGui::TextDisabled("Error reading directory");
    }

    ImGui::EndChild();

    ImGui::End();
}

void EditorLayer::DrawSettingsPanel() {
    ImGui::Begin("Settings");

    if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (m_CameraController) {
            // View preset buttons
            ImGui::Text("View Presets:");
            if (ImGui::Button("Persp")) {
                m_CameraController->SetViewPreset(Renderer::ViewPreset::Perspective);
            }
            ImGui::SameLine();
            if (ImGui::Button("Top")) {
                m_CameraController->SetViewPreset(Renderer::ViewPreset::Top);
            }
            ImGui::SameLine();
            if (ImGui::Button("Front")) {
                m_CameraController->SetViewPreset(Renderer::ViewPreset::Front);
            }
            ImGui::SameLine();
            if (ImGui::Button("Right")) {
                m_CameraController->SetViewPreset(Renderer::ViewPreset::Right);
            }

            if (ImGui::Button("Bottom")) {
                m_CameraController->SetViewPreset(Renderer::ViewPreset::Bottom);
            }
            ImGui::SameLine();
            if (ImGui::Button("Back")) {
                m_CameraController->SetViewPreset(Renderer::ViewPreset::Back);
            }
            ImGui::SameLine();
            if (ImGui::Button("Left")) {
                m_CameraController->SetViewPreset(Renderer::ViewPreset::Left);
            }

            ImGui::Separator();

            // Orthographic toggle
            bool isOrtho = m_CameraController->IsOrthographic();
            if (ImGui::Checkbox("Orthographic", &isOrtho)) {
                m_CameraController->SetOrthographic(isOrtho);
            }

            if (isOrtho) {
                f32 orthoSize = m_CameraController->GetOrthoSize();
                if (ImGui::DragFloat("Ortho Size", &orthoSize, 0.5f, 1.0f, 100.0f)) {
                    m_CameraController->SetOrthoSize(orthoSize);
                    m_CameraController->SetOrthographic(true);  // Refresh projection
                }
            }

            ImGui::Separator();

            f32 moveSpeed = m_CameraController->GetMoveSpeed();
            if (ImGui::DragFloat("Move Speed", &moveSpeed, 0.5f, 0.1f, 100.0f)) {
                m_CameraController->SetMoveSpeed(moveSpeed);
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("WASD movement speed in the viewport");
            }

            f32 sensitivity = m_CameraController->GetLookSensitivity();
            if (ImGui::DragFloat("Look Sensitivity", &sensitivity, 0.01f, 0.01f, 1.0f)) {
                m_CameraController->SetLookSensitivity(sensitivity);
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Mouse look sensitivity (Right-click + drag)");
            }

            // Camera mode
            const char* modes[] = { "Fly", "Orbit", "First Person" };
            int currentMode = static_cast<int>(m_CameraController->GetMode());
            if (ImGui::Combo("Mode", &currentMode, modes, 3)) {
                m_CameraController->SetMode(static_cast<Renderer::CameraMode>(currentMode));
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Fly: WASD + RMB look\nOrbit: MMB to orbit around selection\nFirst Person: Ground-level movement");
            }
        }

        if (m_Camera) {
            Math::Vector3 pos = m_Camera->GetPosition();
            ImGui::Text("Position: %.2f, %.2f, %.2f", pos.x, pos.y, pos.z);

            f32 yaw = m_CameraController ? m_CameraController->GetYaw() : 0.0f;
            f32 pitch = m_CameraController ? m_CameraController->GetPitch() : 0.0f;
            ImGui::Text("Yaw: %.1f  Pitch: %.1f", yaw, pitch);
        }

        ImGui::Separator();
        ImGui::Text("Game Camera:");

        // Gather all cameras for the selector
        std::vector<ECS::Entity> settingsCameraEntities;
        if (m_World) {
            for (ECS::Entity entity : m_World->GetAllEntities()) {
                if (m_World->HasComponent<ECS::CameraComponent>(entity)) {
                    settingsCameraEntities.push_back(entity);
                }
            }
        }

        // Camera selector (shared with game view)
        if (!settingsCameraEntities.empty() && m_Camera && m_CameraController) {
            // Validate selection
            if (m_SelectedGameCamera == ECS::INVALID_ENTITY) {
                m_SelectedGameCamera = settingsCameraEntities[0];
            }

            // Dropdown to pick camera
            if (settingsCameraEntities.size() > 1) {
                std::string currentName = "None";
                if (m_SelectedGameCamera != ECS::INVALID_ENTITY && m_World->HasComponent<ECS::NameComponent>(m_SelectedGameCamera)) {
                    currentName = m_World->GetComponent<ECS::NameComponent>(m_SelectedGameCamera)->name;
                } else if (m_SelectedGameCamera != ECS::INVALID_ENTITY) {
                    currentName = "Camera " + std::to_string(m_SelectedGameCamera);
                }
                ImGui::SetNextItemWidth(-1);
                if (ImGui::BeginCombo("##SettingsCamSelect", currentName.c_str())) {
                    for (ECS::Entity camEnt : settingsCameraEntities) {
                        std::string name;
                        if (m_World->HasComponent<ECS::NameComponent>(camEnt)) {
                            name = m_World->GetComponent<ECS::NameComponent>(camEnt)->name;
                        } else {
                            name = "Camera " + std::to_string(camEnt);
                        }
                        bool selected = (camEnt == m_SelectedGameCamera);
                        if (ImGui::Selectable(name.c_str(), selected)) {
                            m_SelectedGameCamera = camEnt;
                        }
                        if (selected) ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
            }

            ECS::Entity gameCamEntity = m_SelectedGameCamera;
            auto* gameCamComp = m_World->GetComponent<ECS::CameraComponent>(gameCamEntity);
            auto* gameCamTransform = m_World->GetComponent<ECS::TransformComponent>(gameCamEntity);

            if (gameCamComp && gameCamTransform) {
                std::string camName = "Game Camera";
                if (m_World->HasComponent<ECS::NameComponent>(gameCamEntity)) {
                    camName = m_World->GetComponent<ECS::NameComponent>(gameCamEntity)->name;
                }
                ImGui::Text("Selected: %s", camName.c_str());

                // Apply editor view to game camera (ONE-SHOT)
                if (ImGui::Button("Apply Editor View to Game Camera")) {
                    gameCamTransform->position = m_Camera->GetPosition();

                    f32 yawRad = Math::Radians(m_CameraController->GetYaw());
                    f32 pitchRad = Math::Radians(m_CameraController->GetPitch());
                    Math::Quaternion yawQuat(Math::Vector3(0.0f, 1.0f, 0.0f), yawRad);
                    Math::Quaternion pitchQuat(Math::Vector3(1.0f, 0.0f, 0.0f), pitchRad);
                    gameCamTransform->rotation = yawQuat * pitchQuat;

                    if (m_CameraController->IsOrthographic()) {
                        gameCamComp->projectionType = ECS::ProjectionType::Orthographic;
                        gameCamComp->orthoSize = m_CameraController->GetOrthoSize();
                    } else {
                        gameCamComp->projectionType = ECS::ProjectionType::Perspective;
                    }

                    ENJIN_LOG_INFO(Editor, "Applied editor view to game camera '%s'", camName.c_str());
                }
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("One-shot: copies current editor position/rotation to the game camera");

                // Apply game camera view to editor (ONE-SHOT)
                if (ImGui::Button("Snap Editor to Game Camera")) {
                    m_Camera->SetPosition(gameCamTransform->position);

                    Math::Vector3 forward = gameCamTransform->rotation.Rotate(Math::Vector3(0.0f, 0.0f, -1.0f));
                    Math::Vector3 up = gameCamTransform->rotation.Rotate(Math::Vector3(0.0f, 1.0f, 0.0f));
                    m_Camera->SetLookAt(gameCamTransform->position,
                                         gameCamTransform->position + forward, up);
                    m_CameraController->SyncFromCamera();

                    if (gameCamComp->projectionType == ECS::ProjectionType::Orthographic) {
                        m_CameraController->SetOrthoSize(gameCamComp->orthoSize);
                        m_CameraController->SetOrthographic(true);
                    } else {
                        m_CameraController->SetOrthographic(false);
                    }

                    ENJIN_LOG_INFO(Editor, "Snapped editor to game camera '%s'", camName.c_str());
                }
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("One-shot: moves editor camera to match the game camera's view");
            }
        } else {
            ImGui::TextDisabled("No game camera in scene");
        }
    }

    if (ImGui::CollapsingHeader("Grid", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox("Show Grid", &m_ShowGrid);
        if (m_ShowGrid) {
            ImGui::DragFloat("Grid Size", &m_GridSize, 1.0f, 1.0f, 500.0f);
            ImGui::DragInt("Grid Lines", &m_GridLines, 1, 5, 400);
        }
    }

    if (ImGui::CollapsingHeader("Gizmos", ImGuiTreeNodeFlags_DefaultOpen)) {
        // Gizmo operation
        const char* operations[] = { "Translate (1)", "Rotate (2)", "Scale (3)" };
        int currentOp = static_cast<int>(m_GizmoOperation);
        if (ImGui::Combo("Operation", &currentOp, operations, 3)) {
            m_GizmoOperation = static_cast<GizmoOperation>(currentOp);
        }

        // Gizmo space
        const char* spaces[] = { "Local", "World" };
        int currentSpace = static_cast<int>(m_GizmoSpace);
        if (ImGui::Combo("Space (4)", &currentSpace, spaces, 2)) {
            m_GizmoSpace = static_cast<GizmoSpace>(currentSpace);
        }

        // Snap settings
        ImGui::Checkbox("Enable Snap", &m_UseSnap);
        if (m_UseSnap) {
            ImGui::DragFloat("Translate Snap", &m_TranslateSnap, 0.1f, 0.1f, 10.0f);
            ImGui::DragFloat("Rotate Snap", &m_RotateSnap, 1.0f, 1.0f, 90.0f);
            ImGui::DragFloat("Scale Snap", &m_ScaleSnap, 0.01f, 0.01f, 1.0f);
        }
    }

    if (ImGui::CollapsingHeader("Rendering")) {
        if (m_RenderSystem) {
            // Shadows
            bool shadows = m_RenderSystem->IsShadowsEnabled();
            if (ImGui::Checkbox("Shadows", &shadows)) {
                m_RenderSystem->SetShadowsEnabled(shadows);
            }

            // Backface culling
            bool culling = m_RenderSystem->IsBackfaceCullingEnabled();
            if (ImGui::Checkbox("Backface Culling", &culling)) {
                m_RenderSystem->SetBackfaceCullingEnabled(culling);
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Cull back-facing triangles for better performance");

            // Wireframe
            bool wireframe = m_RenderSystem->IsWireframeEnabled();
            if (ImGui::Checkbox("Wireframe", &wireframe)) {
                m_RenderSystem->SetWireframeEnabled(wireframe);
            }

            ImGui::Separator();

            // Ambient lighting
            Math::Vector3 ambientColor = m_RenderSystem->GetAmbientColor();
            f32 ambient[3] = { ambientColor.x, ambientColor.y, ambientColor.z };
            if (ImGui::ColorEdit3("Ambient Color", ambient)) {
                m_RenderSystem->SetAmbientColor(Math::Vector3(ambient[0], ambient[1], ambient[2]));
            }

            f32 ambientIntensity = m_RenderSystem->GetAmbientIntensity();
            if (ImGui::DragFloat("Ambient Intensity", &ambientIntensity, 0.05f, 0.0f, 5.0f)) {
                m_RenderSystem->SetAmbientIntensity(ambientIntensity);
            }
        } else {
            ImGui::TextDisabled("RenderSystem not available");
        }

        ImGui::Separator();
        ImGui::Text("Post-Processing:");

        if (m_PostProcessing) {
            auto& settings = m_PostProcessing->GetSettings();

            // Tone mapping
            const char* toneModes[] = { "None", "Reinhard", "Reinhard Ext", "ACES", "Uncharted 2", "AgX" };
            int toneMode = static_cast<int>(settings.toneMappingMode);
            if (ImGui::Combo("Tone Mapping", &toneMode, toneModes, 6)) {
                settings.toneMappingMode = static_cast<u32>(toneMode);
            }

            ImGui::DragFloat("Exposure", &settings.exposure, 0.05f, 0.1f, 10.0f);
            ImGui::DragFloat("Gamma", &settings.gamma, 0.05f, 0.5f, 3.0f);

            // FXAA
            bool fxaa = settings.fxaaEnabled != 0;
            if (ImGui::Checkbox("FXAA", &fxaa)) {
                settings.fxaaEnabled = fxaa ? 1 : 0;
            }

            // Bloom
            bool bloom = settings.bloomEnabled != 0;
            if (ImGui::Checkbox("Bloom", &bloom)) {
                settings.bloomEnabled = bloom ? 1 : 0;
            }
            if (bloom) {
                ImGui::DragFloat("Bloom Threshold", &settings.bloomThreshold, 0.05f, 0.0f, 5.0f);
                ImGui::DragFloat("Bloom Intensity", &settings.bloomIntensity, 0.05f, 0.0f, 5.0f);
            }

            // Vignette
            bool vignette = settings.vignetteEnabled != 0;
            if (ImGui::Checkbox("Vignette", &vignette)) {
                settings.vignetteEnabled = vignette ? 1 : 0;
            }
            if (vignette) {
                ImGui::DragFloat("Vignette Intensity", &settings.vignetteIntensity, 0.05f, 0.0f, 3.0f);
            }

            // Film Grain
            bool grain = settings.filmGrainEnabled != 0;
            if (ImGui::Checkbox("Film Grain", &grain)) {
                settings.filmGrainEnabled = grain ? 1 : 0;
            }
            if (grain) {
                ImGui::DragFloat("Grain Intensity", &settings.filmGrainIntensity, 0.005f, 0.0f, 0.5f);
            }

            // Chromatic Aberration
            bool chrAb = settings.chromaticAberrationEnabled != 0;
            if (ImGui::Checkbox("Chromatic Aberration", &chrAb)) {
                settings.chromaticAberrationEnabled = chrAb ? 1 : 0;
            }
            if (chrAb) {
                ImGui::DragFloat("CA Intensity", &settings.chromaticAberrationIntensity, 0.001f, 0.0f, 0.1f);
            }

            ImGui::Separator();
            ImGui::Text("Color Grading:");
            ImGui::DragFloat("Brightness", &settings.brightness, 0.01f, -1.0f, 1.0f);
            ImGui::DragFloat("Contrast", &settings.contrast, 0.01f, 0.0f, 3.0f);
            ImGui::DragFloat("Saturation", &settings.saturation, 0.01f, 0.0f, 3.0f);
            f32 filter[3] = { settings.colorFilter.x, settings.colorFilter.y, settings.colorFilter.z };
            if (ImGui::ColorEdit3("Color Filter", filter)) {
                settings.colorFilter = Math::Vector3(filter[0], filter[1], filter[2]);
            }
        } else {
            ImGui::TextDisabled("PostProcessing not available");
        }
    }

    if (ImGui::CollapsingHeader("Physics")) {
        Physics::SimplePhysics* physics = m_PlayMode.GetPhysics();
        Math::Vector3 gravity = physics->GetGravity();

        f32 grav[3] = { gravity.x, gravity.y, gravity.z };
        if (ImGui::DragFloat3("Global Gravity", grav, 0.1f, -100.0f, 100.0f)) {
            physics->SetGravity(Math::Vector3(grav[0], grav[1], grav[2]));
        }

        // Quick presets
        ImGui::Text("Presets:");
        ImGui::SameLine();
        if (ImGui::SmallButton("Earth")) { physics->SetGravity(Math::Vector3(0.0f, -9.81f, 0.0f)); }
        ImGui::SameLine();
        if (ImGui::SmallButton("Moon")) { physics->SetGravity(Math::Vector3(0.0f, -1.62f, 0.0f)); }
        ImGui::SameLine();
        if (ImGui::SmallButton("Mars")) { physics->SetGravity(Math::Vector3(0.0f, -3.72f, 0.0f)); }
        ImGui::SameLine();
        if (ImGui::SmallButton("Zero")) { physics->SetGravity(Math::Vector3(0.0f, 0.0f, 0.0f)); }

        f32 strength = physics->GetGravity().Length();
        ImGui::TextDisabled("Strength: %.2f m/s^2", strength);

        ImGui::Spacing();
        ImGui::TextDisabled("Use Gravity Zone components for regional overrides");
    }

    if (ImGui::CollapsingHeader("Gamepad")) {
        // Global dead zone setting
        f32 deadZone = Input::GetGamepadDeadZone();
        if (ImGui::SliderFloat("Dead Zone", &deadZone, 0.01f, 0.5f, "%.2f")) {
            Input::SetGamepadDeadZone(deadZone);
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Analog stick dead zone threshold");

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Show status of each gamepad slot
        for (i32 gp = 0; gp < 4; ++gp) {
            bool connected = Input::IsGamepadConnected(gp);
            ImGui::PushID(gp);

            // Header with connection indicator
            ImVec4 headerCol = connected ? ImVec4(0.2f, 0.6f, 0.2f, 1.0f) : ImVec4(0.4f, 0.4f, 0.4f, 1.0f);
            ImGui::PushStyleColor(ImGuiCol_Text, headerCol);
            char label[64];
            if (connected) {
                snprintf(label, sizeof(label), "Gamepad %d: %s", gp, Input::GetGamepadName(gp));
            } else {
                snprintf(label, sizeof(label), "Gamepad %d: Not Connected", gp);
            }
            bool open = ImGui::TreeNode(label);
            ImGui::PopStyleColor();

            if (open) {
                if (connected) {
                    // Stick visualization using progress bars
                    Math::Vector2 leftStick = Input::GetGamepadLeftStick(gp);
                    Math::Vector2 rightStick = Input::GetGamepadRightStick(gp);

                    ImGui::Text("Left Stick:");
                    ImGui::SameLine(120);
                    ImGui::Text("X: %+.2f  Y: %+.2f", leftStick.x, leftStick.y);

                    ImGui::Text("Right Stick:");
                    ImGui::SameLine(120);
                    ImGui::Text("X: %+.2f  Y: %+.2f", rightStick.x, rightStick.y);

                    // Triggers
                    f32 lt = Input::GetGamepadLeftTrigger(gp);
                    f32 rt = Input::GetGamepadRightTrigger(gp);
                    ImGui::Text("L Trigger:");
                    ImGui::SameLine(120);
                    ImGui::ProgressBar(lt, ImVec2(100, 14), "");
                    ImGui::Text("R Trigger:");
                    ImGui::SameLine(120);
                    ImGui::ProgressBar(rt, ImVec2(100, 14), "");

                    // Button states in a compact grid
                    ImGui::Spacing();
                    ImGui::Text("Buttons:");

                    struct BtnInfo { const char* name; GamepadButton btn; };
                    BtnInfo buttons[] = {
                        {"A", GamepadButton::A}, {"B", GamepadButton::B},
                        {"X", GamepadButton::X}, {"Y", GamepadButton::Y},
                        {"LB", GamepadButton::LeftBumper}, {"RB", GamepadButton::RightBumper},
                        {"Back", GamepadButton::Back}, {"Start", GamepadButton::Start},
                    };

                    for (int b = 0; b < 8; ++b) {
                        bool pressed = Input::IsGamepadButtonDown(buttons[b].btn, gp);
                        if (pressed) {
                            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.7f, 0.3f, 1.0f));
                        }
                        ImGui::SmallButton(buttons[b].name);
                        if (pressed) {
                            ImGui::PopStyleColor();
                        }
                        if (b < 7 && (b % 4) != 3) ImGui::SameLine();
                    }
                } else {
                    ImGui::TextDisabled("Connect a controller to see live input");
                }
                ImGui::TreePop();
            }
            ImGui::PopID();
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::TextDisabled("Gamepad Button Mapping:");
        ImGui::BulletText("Left Stick - Movement");
        ImGui::BulletText("Right Stick - Camera look");
        ImGui::BulletText("A - Jump");
        ImGui::BulletText("B - Crouch");
        ImGui::BulletText("LB / L3 - Sprint");
        ImGui::BulletText("RB - Dash");
        ImGui::BulletText("LT / RT - Move down / up (editor)");
        ImGui::BulletText("D-pad Up/Down - Adjust editor speed");
    }

    if (ImGui::CollapsingHeader("Keyboard Shortcuts")) {
        ImGui::BulletText("RMB + WASD - Fly camera (horizontal plane)");
        ImGui::BulletText("Space / Q - Move up / down");
        ImGui::BulletText("Shift - Sprint");
        ImGui::BulletText("Left Ctrl - Move down (alt)");
        ImGui::BulletText("RMB + Drag - Look around");
        ImGui::BulletText("MMB + Drag - Orbit around selection");
        ImGui::BulletText("Scroll Wheel - Adjust speed / zoom");
        ImGui::BulletText("F - Focus on selected entity");
        ImGui::BulletText("Delete - Delete selected entity");
        ImGui::BulletText("Ctrl+D - Duplicate entity");
        ImGui::BulletText("1/2/3 - Translate/Rotate/Scale gizmo");
        ImGui::BulletText("4 - Toggle Local/World space");
        ImGui::BulletText("Ctrl+S - Save scene");
        ImGui::BulletText("F11 - Toggle focus mode");
    }

    if (ImGui::CollapsingHeader("Accessibility")) {
        bool settingsChanged = false;

        // Theme
        const char* themeNames[] = { "Dark", "Light", "High Contrast Dark", "High Contrast Light" };
        int currentTheme = static_cast<int>(m_EditorSettings.theme);
        if (ImGui::Combo("Theme", &currentTheme, themeNames, 4)) {
            m_EditorSettings.theme = static_cast<EditorTheme>(currentTheme);
            m_ImGuiLayer->ApplyTheme(m_EditorSettings.theme);
            settingsChanged = true;
        }

        // UI Scale
        if (ImGui::SliderFloat("UI Scale", &m_EditorSettings.uiScale, 0.75f, 2.0f, "%.2f")) {
            m_ImGuiLayer->SetGlobalScale(m_EditorSettings.uiScale);
            settingsChanged = true;
        }

        ImGui::Separator();

        // -- Visual Accessibility --
        if (ImGui::TreeNode("Visual")) {
            const char* cbModes[] = {
                "Off", "Protanopia", "Deuteranopia", "Tritanopia",
                "Protanomaly", "Deuteranomaly", "Tritanomaly", "Achromatopsia"
            };
            int cbMode = static_cast<int>(m_EditorSettings.colorblindMode);
            if (ImGui::Combo("Colorblind Mode", &cbMode, cbModes, 8)) {
                m_EditorSettings.colorblindMode = static_cast<u32>(cbMode);
                settingsChanged = true;
            }

            if (m_EditorSettings.colorblindMode > 0) {
                if (ImGui::SliderFloat("Correction Strength", &m_EditorSettings.colorblindStrength, 0.0f, 1.0f)) {
                    settingsChanged = true;
                }
            }

            if (ImGui::SliderFloat("Screen Brightness", &m_EditorSettings.screenBrightness, -0.5f, 0.5f)) {
                settingsChanged = true;
            }
            if (ImGui::SliderFloat("Screen Contrast", &m_EditorSettings.screenContrast, 0.5f, 2.0f)) {
                settingsChanged = true;
            }

            ImGui::TreePop();
        }

        // -- Motion --
        if (ImGui::TreeNode("Motion")) {
            if (ImGui::Checkbox("Reduced Motion", &m_EditorSettings.reducedMotion)) settingsChanged = true;
            if (ImGui::Checkbox("Disable Screen Shake", &m_EditorSettings.disableScreenShake)) settingsChanged = true;
            if (ImGui::Checkbox("Disable FOV Effects", &m_EditorSettings.disableFOVEffects)) settingsChanged = true;
            ImGui::TreePop();
        }

        // -- Subtitles / Cognitive --
        if (ImGui::TreeNode("Cognitive")) {
            if (ImGui::Checkbox("Subtitles", &m_EditorSettings.subtitlesEnabled)) settingsChanged = true;
            if (ImGui::Checkbox("Closed Captions", &m_EditorSettings.closedCaptionsEnabled)) settingsChanged = true;

            if (m_EditorSettings.subtitlesEnabled || m_EditorSettings.closedCaptionsEnabled) {
                if (ImGui::SliderFloat("Subtitle Size", &m_EditorSettings.subtitleFontSize, 16.0f, 48.0f, "%.0f")) settingsChanged = true;
                if (ImGui::SliderFloat("Background Opacity", &m_EditorSettings.subtitleBgOpacity, 0.0f, 1.0f)) settingsChanged = true;
                if (ImGui::Checkbox("Show Speaker Names", &m_EditorSettings.subtitleSpeakerNames)) settingsChanged = true;
            }

            ImGui::Separator();
            if (ImGui::Checkbox("Simplified Editor", &m_EditorSettings.simplifiedEditor)) settingsChanged = true;
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Hides advanced panels and collapses complex inspector sections");
            }

            ImGui::TreePop();
        }

        // -- Input Accessibility --
        if (ImGui::TreeNode("Input")) {
            const char* holdToggle[] = { "Hold", "Toggle" };

            int sprintMode = static_cast<int>(m_EditorSettings.sprintMode);
            if (ImGui::Combo("Sprint Mode", &sprintMode, holdToggle, 2)) {
                m_EditorSettings.sprintMode = static_cast<u32>(sprintMode);
                settingsChanged = true;
            }

            int crouchMode = static_cast<int>(m_EditorSettings.crouchMode);
            if (ImGui::Combo("Crouch Mode", &crouchMode, holdToggle, 2)) {
                m_EditorSettings.crouchMode = static_cast<u32>(crouchMode);
                settingsChanged = true;
            }

            if (ImGui::SliderFloat("Mouse Sensitivity", &m_EditorSettings.mouseSensitivity, 0.1f, 3.0f)) {
                settingsChanged = true;
            }

            ImGui::Separator();
            ImGui::TextDisabled("Input Presets");
            const char* presetNames[] = { "Default", "Left Hand Only", "Right Hand Only", "Gamepad Only" };
            int preset = static_cast<int>(m_EditorSettings.inputPreset);
            if (ImGui::Combo("Preset", &preset, presetNames, 4)) {
                m_EditorSettings.inputPreset = static_cast<u32>(preset);
                settingsChanged = true;
            }

            ImGui::TreePop();
        }

        ImGui::Separator();

        // -- Quick Presets --
        ImGui::TextDisabled("Quick Presets");
        if (ImGui::Button("Low Vision")) {
            m_EditorSettings.theme = EditorTheme::HighContrastDark;
            m_EditorSettings.uiScale = 1.5f;
            m_ImGuiLayer->ApplyTheme(m_EditorSettings.theme);
            m_ImGuiLayer->SetGlobalScale(m_EditorSettings.uiScale);
            settingsChanged = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Motor Impaired")) {
            m_EditorSettings.inputPreset = 3; // Gamepad only
            m_EditorSettings.sprintMode = 1;  // Toggle
            m_EditorSettings.crouchMode = 1;  // Toggle
            settingsChanged = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Photosensitive")) {
            m_EditorSettings.reducedMotion = true;
            m_EditorSettings.disableScreenShake = true;
            m_EditorSettings.disableFOVEffects = true;
            // Disable film grain, CRT, VHS if post-processing is active
            if (m_PostProcessing) {
                auto& ppSettings = m_PostProcessing->GetSettings();
                ppSettings.filmGrainEnabled = 0;
                ppSettings.crtEnabled = 0;
                ppSettings.vhsEnabled = 0;
            }
            settingsChanged = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Reset All")) {
            m_EditorSettings = EditorSettings{};
            m_ImGuiLayer->ApplyTheme(m_EditorSettings.theme);
            m_ImGuiLayer->SetGlobalScale(m_EditorSettings.uiScale);
            settingsChanged = true;
        }

        // Auto-save when settings change
        if (settingsChanged) {
            m_EditorSettings.Save();

            // Apply colorblind + brightness/contrast to post-processing
            if (m_PostProcessing) {
                auto& ppSettings = m_PostProcessing->GetSettings();
                ppSettings.colorblindMode = m_EditorSettings.colorblindMode;
                ppSettings.colorblindStrength = m_EditorSettings.colorblindStrength;
            }
        }
    }

    if (ImGui::CollapsingHeader("Fonts")) {
        static char bodyFontPath[512] = "";
        static char headingFontPath[512] = "";
        static char monoFontPath[512] = "";
        static f32 bodySize = 15.0f;
        static f32 headingSize = 20.0f;
        static f32 monoSize = 14.0f;

        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 80);
        ImGui::InputText("##BodyFont", bodyFontPath, sizeof(bodyFontPath));
        ImGui::SameLine();
        if (ImGui::Button("Browse##BodyFont")) {
            std::string path = FileDialog::OpenFile("Select Font", {{ "Font Files", "*.ttf;*.otf" }});
            if (!path.empty()) {
                strncpy(bodyFontPath, path.c_str(), sizeof(bodyFontPath) - 1);
            }
        }
        ImGui::SameLine();
        ImGui::TextDisabled("Body");
        ImGui::DragFloat("Body Size", &bodySize, 0.5f, 8.0f, 48.0f);

        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 80);
        ImGui::InputText("##HeadingFont", headingFontPath, sizeof(headingFontPath));
        ImGui::SameLine();
        if (ImGui::Button("Browse##HeadingFont")) {
            std::string path = FileDialog::OpenFile("Select Font", {{ "Font Files", "*.ttf;*.otf" }});
            if (!path.empty()) {
                strncpy(headingFontPath, path.c_str(), sizeof(headingFontPath) - 1);
            }
        }
        ImGui::SameLine();
        ImGui::TextDisabled("Heading");
        ImGui::DragFloat("Heading Size", &headingSize, 0.5f, 8.0f, 64.0f);

        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 80);
        ImGui::InputText("##MonoFont", monoFontPath, sizeof(monoFontPath));
        ImGui::SameLine();
        if (ImGui::Button("Browse##MonoFont")) {
            std::string path = FileDialog::OpenFile("Select Font", {{ "Font Files", "*.ttf;*.otf" }});
            if (!path.empty()) {
                strncpy(monoFontPath, path.c_str(), sizeof(monoFontPath) - 1);
            }
        }
        ImGui::SameLine();
        ImGui::TextDisabled("Mono");
        ImGui::DragFloat("Mono Size", &monoSize, 0.5f, 8.0f, 48.0f);

        if (ImGui::Button("Reload Fonts")) {
            GUI::EditorFontConfig fontConfig;
            fontConfig.bodyFontPath = bodyFontPath;
            fontConfig.headingFontPath = headingFontPath;
            fontConfig.monoFontPath = monoFontPath;
            fontConfig.bodyFontSize = bodySize;
            fontConfig.headingFontSize = headingSize;
            fontConfig.monoFontSize = monoSize;
            m_ImGuiLayer->ReloadFonts(fontConfig);
        }
    }

    ImGui::End();
}

void EditorLayer::DrawPostProcessingPanel() {
    ImGui::Begin("Post Processing");

    if (!m_PostProcessing) {
        ImGui::TextDisabled("Post-processing not initialized");
        ImGui::End();
        return;
    }

    auto& settings = m_PostProcessing->GetSettings();

    // Tone Mapping
    if (ImGui::CollapsingHeader("Tone Mapping", ImGuiTreeNodeFlags_DefaultOpen)) {
        const char* toneMappingModes[] = { "None", "Reinhard", "Reinhard Extended", "ACES", "Uncharted 2", "AgX" };
        int currentMode = static_cast<int>(settings.toneMappingMode);
        if (ImGui::Combo("Mode", &currentMode, toneMappingModes, 6)) {
            settings.toneMappingMode = static_cast<u32>(currentMode);
        }

        ImGui::DragFloat("Exposure", &settings.exposure, 0.01f, 0.01f, 10.0f);
        ImGui::DragFloat("Gamma", &settings.gamma, 0.01f, 1.0f, 3.0f);

        if (settings.toneMappingMode == 2) { // Reinhard Extended
            ImGui::DragFloat("White Point", &settings.whitePoint, 0.1f, 1.0f, 20.0f);
        }
    }

    // Bloom
    if (ImGui::CollapsingHeader("Bloom")) {
        bool bloomEnabled = settings.bloomEnabled != 0;
        if (ImGui::Checkbox("Enabled##Bloom", &bloomEnabled)) {
            settings.bloomEnabled = bloomEnabled ? 1 : 0;
        }

        if (settings.bloomEnabled) {
            ImGui::DragFloat("Threshold", &settings.bloomThreshold, 0.01f, 0.0f, 5.0f);
            ImGui::DragFloat("Intensity##Bloom", &settings.bloomIntensity, 0.01f, 0.0f, 2.0f);
            ImGui::DragFloat("Radius", &settings.bloomRadius, 0.001f, 0.001f, 0.1f);
        }
    }

    // Vignette
    if (ImGui::CollapsingHeader("Vignette")) {
        bool vignetteEnabled = settings.vignetteEnabled != 0;
        if (ImGui::Checkbox("Enabled##Vignette", &vignetteEnabled)) {
            settings.vignetteEnabled = vignetteEnabled ? 1 : 0;
        }

        if (settings.vignetteEnabled) {
            ImGui::DragFloat("Intensity##Vignette", &settings.vignetteIntensity, 0.01f, 0.0f, 2.0f);
            ImGui::DragFloat("Smoothness", &settings.vignetteSmoothness, 0.01f, 0.0f, 1.0f);
        }
    }

    // Chromatic Aberration
    if (ImGui::CollapsingHeader("Chromatic Aberration")) {
        bool caEnabled = settings.chromaticAberrationEnabled != 0;
        if (ImGui::Checkbox("Enabled##CA", &caEnabled)) {
            settings.chromaticAberrationEnabled = caEnabled ? 1 : 0;
        }

        if (settings.chromaticAberrationEnabled) {
            ImGui::DragFloat("Intensity##CA", &settings.chromaticAberrationIntensity, 0.001f, 0.0f, 0.05f);
        }
    }

    // Color Grading
    if (ImGui::CollapsingHeader("Color Grading")) {
        f32 colorFilter[3] = { settings.colorFilter.x, settings.colorFilter.y, settings.colorFilter.z };
        if (ImGui::ColorEdit3("Color Filter", colorFilter)) {
            settings.colorFilter = Math::Vector3(colorFilter[0], colorFilter[1], colorFilter[2]);
        }

        ImGui::DragFloat("Saturation", &settings.saturation, 0.01f, 0.0f, 2.0f);
        ImGui::DragFloat("Contrast", &settings.contrast, 0.01f, 0.5f, 2.0f);
        ImGui::DragFloat("Brightness", &settings.brightness, 0.01f, -1.0f, 1.0f);
    }

    // Film Grain
    if (ImGui::CollapsingHeader("Film Grain")) {
        bool grainEnabled = settings.filmGrainEnabled != 0;
        if (ImGui::Checkbox("Enabled##Grain", &grainEnabled)) {
            settings.filmGrainEnabled = grainEnabled ? 1 : 0;
        }

        if (settings.filmGrainEnabled) {
            ImGui::DragFloat("Intensity##Grain", &settings.filmGrainIntensity, 0.001f, 0.0f, 0.2f);
        }
    }

    // FXAA
    if (ImGui::CollapsingHeader("Anti-Aliasing (FXAA)")) {
        bool fxaaEnabled = settings.fxaaEnabled != 0;
        if (ImGui::Checkbox("Enabled##FXAA", &fxaaEnabled)) {
            settings.fxaaEnabled = fxaaEnabled ? 1 : 0;
        }

        if (settings.fxaaEnabled) {
            ImGui::DragFloat("Span Max", &settings.fxaaSpanMax, 0.5f, 2.0f, 16.0f);
            ImGui::DragFloat("Reduce Min", &settings.fxaaReduceMin, 0.001f, 0.0f, 0.1f, "%.4f");
            ImGui::DragFloat("Reduce Mul", &settings.fxaaReduceMul, 0.01f, 0.0f, 0.5f);
        }
    }

    // LUT Color Grading
    if (ImGui::CollapsingHeader("LUT Color Grading")) {
        bool lutEnabled = settings.lutEnabled != 0;
        if (ImGui::Checkbox("Enabled##LUT", &lutEnabled)) {
            settings.lutEnabled = lutEnabled ? 1 : 0;
        }

        if (settings.lutEnabled) {
            ImGui::DragFloat("Strength##LUT", &settings.lutStrength, 0.01f, 0.0f, 1.0f);

            if (m_PostProcessing->IsLUTLoaded()) {
                std::string lutPath = m_PostProcessing->GetLUTPath();
                // Show just the filename
                size_t lastSlash = lutPath.find_last_of("/\\");
                std::string filename = (lastSlash != std::string::npos) ? lutPath.substr(lastSlash + 1) : lutPath;
                ImGui::Text("Loaded: %s", filename.c_str());

                if (ImGui::Button("Clear LUT")) {
                    m_PostProcessing->ClearLUT();
                    settings.lutEnabled = 0;
                }
            }

            ImGui::SameLine();
            if (ImGui::Button("Load LUT")) {
                std::string path = FileDialog::OpenFile("Load LUT", {{ "PNG Images", "*.png" }});
                if (!path.empty()) {
                    m_PostProcessing->LoadLUT(path);
                }
            }
        }
    }

    // Color Palette Lock
    if (ImGui::CollapsingHeader("Palette Lock")) {
        bool paletteEnabled = settings.paletteEnabled != 0;
        if (ImGui::Checkbox("Enabled##Palette", &paletteEnabled)) {
            settings.paletteEnabled = paletteEnabled ? 1 : 0;
        }

        if (settings.paletteEnabled) {
            int colors = static_cast<int>(settings.paletteColors);
            if (ImGui::SliderInt("Colors", &colors, 2, 256)) {
                settings.paletteColors = static_cast<u32>(colors);
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Number of color levels per channel");
        }
    }

    ImGui::End();
}

void EditorLayer::DrawEffectsPanel() {
    ImGui::Begin("Effects (Retro)");

    // === RETRO EFFECTS (PS1/N64/PS2/GameCube presets) ===
    if (ImGui::CollapsingHeader("Retro Effects", ImGuiTreeNodeFlags_DefaultOpen)) {
        bool retroEnabled = m_RetroEffects.IsEnabled();
        if (ImGui::Checkbox("Enable Retro Effects", &retroEnabled)) {
            m_RetroEffects.SetEnabled(retroEnabled);
        }

        if (retroEnabled) {
            ImGui::Text("Quick Presets:");
            if (ImGui::Button("PS1")) { m_RetroEffects.ApplyPS1Preset(); }
            ImGui::SameLine();
            if (ImGui::Button("N64")) { m_RetroEffects.ApplyN64Preset(); }
            ImGui::SameLine();
            if (ImGui::Button("PS2")) { m_RetroEffects.ApplyPS2Preset(); }
            ImGui::SameLine();
            if (ImGui::Button("GameCube")) { m_RetroEffects.ApplyGameCubePreset(); }

            if (ImGui::Button("SNES")) { m_RetroEffects.ApplySNESPreset(); }
            ImGui::SameLine();
            if (ImGui::Button("Dreamcast")) { m_RetroEffects.ApplyDreamcastPreset(); }
            ImGui::SameLine();
            if (ImGui::Button("Clear All")) { m_RetroEffects.ClearAllEffects(); }

            ImGui::Separator();

            // Resolution settings
            if (ImGui::TreeNode("Resolution")) {
                auto& res = m_RetroEffects.GetResolution();
                int width = static_cast<int>(res.renderWidth);
                int height = static_cast<int>(res.renderHeight);
                if (ImGui::DragInt("Render Width", &width, 1, 160, 1920)) {
                    res.renderWidth = static_cast<u32>(width);
                }
                if (ImGui::DragInt("Render Height", &height, 1, 120, 1080)) {
                    res.renderHeight = static_cast<u32>(height);
                }
                ImGui::Checkbox("Point Filtering", &res.pointFiltering);
                ImGui::Checkbox("Integer Scaling", &res.integerScaling);
                ImGui::DragFloat("Aspect Ratio", &res.aspectRatio, 0.01f, 1.0f, 2.5f);
                ImGui::TreePop();
            }

            // Dithering
            if (ImGui::TreeNode("Dithering")) {
                const char* ditherPatterns[] = { "None", "Bayer 2x2", "Bayer 4x4", "Bayer 8x8", "Blue Noise", "Ordered" };
                int currentDither = static_cast<int>(m_RetroEffects.GetDitherPattern());
                if (ImGui::Combo("Pattern", &currentDither, ditherPatterns, 6)) {
                    m_RetroEffects.SetDitherPattern(static_cast<Effects::DitherPattern>(currentDither));
                }
                ImGui::TreePop();
            }

            // Color Mode
            if (ImGui::TreeNode("Color Mode")) {
                const char* colorModes[] = { "True Color (24-bit)", "High Color (16-bit)", "256 Colors", "16 Colors", "Monochrome" };
                int currentMode = static_cast<int>(m_RetroEffects.GetColorMode());
                if (ImGui::Combo("Mode", &currentMode, colorModes, 5)) {
                    m_RetroEffects.SetColorMode(static_cast<Effects::ColorMode>(currentMode));
                }
                ImGui::TreePop();
            }

            // Vertex Jitter (PS1 style)
            if (ImGui::TreeNode("Vertex Jitter (PS1)")) {
                auto& jitter = m_RetroEffects.GetVertexJitter();
                ImGui::Checkbox("Enabled##Jitter", &jitter.enabled);
                if (jitter.enabled) {
                    ImGui::DragFloat("Amount", &jitter.jitterAmount, 0.1f, 0.0f, 5.0f);
                    ImGui::Checkbox("Snap to Grid", &jitter.snapToGrid);
                    int gridRes = static_cast<int>(jitter.gridResolution);
                    if (ImGui::DragInt("Grid Resolution", &gridRes, 1, 80, 320)) {
                        jitter.gridResolution = static_cast<u32>(gridRes);
                    }
                }
                ImGui::TreePop();
            }

            // Affine Texture Warping (PS1)
            if (ImGui::TreeNode("Affine Warping (PS1)")) {
                auto& affine = m_RetroEffects.GetAffineSettings();
                ImGui::Checkbox("Enabled##Affine", &affine.enabled);
                if (affine.enabled) {
                    ImGui::DragFloat("Warp Strength", &affine.warpStrength, 0.1f, 0.0f, 2.0f);
                    ImGui::Checkbox("Vertex Snapping", &affine.vertexSnapping);
                    ImGui::DragFloat("Snap Grid Size", &affine.snapGridSize, 0.1f, 0.5f, 4.0f);
                }
                ImGui::TreePop();
            }

            // CRT Filter
            if (ImGui::TreeNode("CRT Filter")) {
                auto& crt = m_RetroEffects.GetCRTSettings();
                ImGui::Checkbox("Enabled##CRT", &crt.enabled);
                if (crt.enabled) {
                    ImGui::DragFloat("Scanline Intensity", &crt.scanlineIntensity, 0.01f, 0.0f, 1.0f);
                    ImGui::DragFloat("Scanline Width", &crt.scanlineWidth, 0.1f, 0.5f, 3.0f);
                    ImGui::Checkbox("Curved Screen", &crt.curvedScreen);
                    if (crt.curvedScreen) {
                        ImGui::DragFloat("Curvature", &crt.curvature, 0.01f, 0.0f, 0.5f);
                    }
                    ImGui::DragFloat("Vignette", &crt.vignette, 0.01f, 0.0f, 1.0f);
                    ImGui::Checkbox("Phosphor Glow", &crt.phosphorGlow);
                    if (crt.phosphorGlow) {
                        ImGui::DragFloat("Glow Strength", &crt.glowStrength, 0.01f, 0.0f, 1.0f);
                    }
                }
                ImGui::TreePop();
            }

            // CRT Phosphor Subpixel Blending
            if (ImGui::TreeNode("CRT Phosphor")) {
                auto& crt = m_RetroEffects.GetCRTSettings();
                const char* maskTypes[] = { "Aperture Grille", "Shadow Mask", "Slot Mask" };
                int maskType = static_cast<int>(crt.maskType);
                if (ImGui::Combo("Mask Type", &maskType, maskTypes, 3)) {
                    crt.maskType = static_cast<u32>(maskType);
                }
                ImGui::DragFloat("Mask Pitch", &crt.maskPitch, 0.1f, 0.5f, 4.0f);
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Spacing between RGB triplets in pixels");
                ImGui::DragFloat("Bloom Radius##Phosphor", &crt.bloomRadius, 0.1f, 0.5f, 5.0f);
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("How far phosphor light bleeds between dots");
                ImGui::DragFloat("Bloom Strength##Phosphor", &crt.bloomStrength, 0.01f, 0.0f, 1.0f);
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Intensity of phosphor bleeding effect");
                ImGui::TreePop();
            }

            // VHS Filter
            if (ImGui::TreeNode("VHS Filter")) {
                auto& vhs = m_RetroEffects.GetVHSSettings();
                ImGui::Checkbox("Enabled##VHS", &vhs.enabled);
                if (vhs.enabled) {
                    ImGui::DragFloat("Tracking Intensity", &vhs.trackingIntensity, 0.01f, 0.0f, 1.0f);
                    ImGui::DragFloat("Tracking Speed", &vhs.trackingSpeed, 0.1f, 0.1f, 5.0f);
                    ImGui::DragFloat("Wobble Intensity", &vhs.wobbleIntensity, 0.0005f, 0.0f, 0.02f, "%.4f");
                    ImGui::DragFloat("Wobble Speed", &vhs.wobbleSpeed, 0.1f, 0.1f, 10.0f);
                    ImGui::DragFloat("Color Bleed", &vhs.colorBleedAmount, 0.0005f, 0.0f, 0.02f, "%.4f");
                    ImGui::DragFloat("Noise Intensity", &vhs.noiseIntensity, 0.005f, 0.0f, 0.3f);
                    ImGui::DragFloat("Blue Shift", &vhs.blueShift, 0.01f, 0.0f, 0.3f);
                    ImGui::Checkbox("Screen Tear", &vhs.screenTear);
                    ImGui::Checkbox("Interlacing", &vhs.interlacing);
                }
                ImGui::TreePop();
            }

            // Global Gouraud-only mode
            if (ImGui::TreeNode("Global Retro Shading")) {
                bool gouraud = m_RetroEffects.GetGouraudOnly();
                if (ImGui::Checkbox("Force Gouraud Shading", &gouraud)) {
                    m_RetroEffects.SetGouraudOnly(gouraud);
                }
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Use vertex-interpolated lighting for all entities (faceted look)");
                ImGui::TreePop();
            }

            // Retro Fog
            if (ImGui::TreeNode("Fog (Distance)")) {
                auto& fog = m_RetroEffects.GetFogSettings();
                ImGui::Checkbox("Enabled##Fog", &fog.enabled);
                if (fog.enabled) {
                    f32 fogColor[3] = { fog.color.x, fog.color.y, fog.color.z };
                    if (ImGui::ColorEdit3("Color##Fog", fogColor)) {
                        fog.color = Math::Vector3(fogColor[0], fogColor[1], fogColor[2]);
                    }
                    ImGui::DragFloat("Start Distance", &fog.start, 0.5f, 0.0f, 100.0f);
                    ImGui::DragFloat("End Distance", &fog.end, 0.5f, 1.0f, 200.0f);
                    ImGui::Checkbox("Hard Cutoff", &fog.hardCutoff);
                    if (fog.hardCutoff) {
                        ImGui::DragFloat("Cutoff Distance", &fog.cutoffDistance, 1.0f, 10.0f, 200.0f);
                    }
                }
                ImGui::TreePop();
            }
        }
    }

    // === WORLD TIME / DAY-NIGHT CYCLE ===
    if (ImGui::CollapsingHeader("World Time / Day-Night")) {
        ImGui::Checkbox("Enable World Time", &m_WorldTimeEnabled);

        if (m_WorldTimeEnabled) {
            auto& state = const_cast<Effects::WorldTimeState&>(m_WorldTime.GetState());
            auto& calConfig = m_WorldTime.GetCalendarConfig();
            auto& dayConfig = m_WorldTime.GetDaylightConfig();

            ImGui::Checkbox("Paused", &calConfig.paused);

            f32 timeOfDay = state.timeOfDay;
            if (ImGui::SliderFloat("Time of Day", &timeOfDay, 0.0f, 23.99f, "%.2f h")) {
                m_WorldTime.SetTime(timeOfDay, state.day, state.month, state.year);
            }

            int day = static_cast<int>(state.day);
            int month = static_cast<int>(state.month);
            int year = static_cast<int>(state.year);
            bool changed = false;
            changed |= ImGui::DragInt("Day", &day, 1, 1, static_cast<int>(calConfig.daysPerMonth));
            changed |= ImGui::DragInt("Month", &month, 1, 1, static_cast<int>(calConfig.monthsPerYear));
            changed |= ImGui::DragInt("Year", &year, 1, 1, 9999);
            if (changed) {
                m_WorldTime.SetTime(state.timeOfDay, static_cast<u32>(day),
                                   static_cast<u32>(month), static_cast<u32>(year));
            }

            ImGui::DragFloat("Seconds/Game Hour", &calConfig.secondsPerGameHour, 1.0f, 1.0f, 600.0f);

            const char* seasonNames[] = { "Spring", "Summer", "Fall", "Winter" };
            ImGui::Text("Season: %s (%.0f%%)", seasonNames[static_cast<int>(state.season)],
                       m_WorldTime.GetSeasonProgress() * 100.0f);
            ImGui::Text("Daylight: %.1f hours  %s", state.daylightHours, state.isNight ? "[Night]" : "[Day]");
            ImGui::Text("Sun Elevation: %.2f", state.sunElevation);

            ImGui::Separator();
            ImGui::Text("Daylight Config");
            ImGui::DragFloat("Spring Daylight", &dayConfig.springDaylight, 0.1f, 6.0f, 20.0f);
            ImGui::DragFloat("Summer Daylight", &dayConfig.summerDaylight, 0.1f, 6.0f, 22.0f);
            ImGui::DragFloat("Fall Daylight", &dayConfig.fallDaylight, 0.1f, 6.0f, 18.0f);
            ImGui::DragFloat("Winter Daylight", &dayConfig.winterDaylight, 0.1f, 4.0f, 16.0f);

            ImGui::Separator();
            ImGui::Checkbox("Seasonal Weather", &m_SeasonalWeatherEnabled);
            if (m_SeasonalWeatherEnabled) {
                auto& sConfig = m_SeasonalWeather.GetConfig();
                ImGui::Text("Temperature: %.1f C", m_SeasonalWeather.GetCurrentTemperature());
                ImGui::DragFloat("Weather Interval (s)", &sConfig.weatherChangeInterval, 10.0f, 10.0f, 3600.0f);

                if (ImGui::TreeNode("Temperature Ranges")) {
                    ImGui::DragFloatRange2("Spring", &sConfig.spring.minTemp, &sConfig.spring.maxTemp, 0.5f, -30.0f, 50.0f);
                    ImGui::DragFloatRange2("Summer", &sConfig.summer.minTemp, &sConfig.summer.maxTemp, 0.5f, -30.0f, 50.0f);
                    ImGui::DragFloatRange2("Fall", &sConfig.fall.minTemp, &sConfig.fall.maxTemp, 0.5f, -30.0f, 50.0f);
                    ImGui::DragFloatRange2("Winter", &sConfig.winter.minTemp, &sConfig.winter.maxTemp, 0.5f, -30.0f, 50.0f);
                    ImGui::TreePop();
                }
            }
        }
    }

    // === WORLD CURVATURE ===
    if (ImGui::CollapsingHeader("World Curvature")) {
        ImGui::Checkbox("Enable Curvature", &m_WorldCurvatureEnabled);
        if (m_WorldCurvatureEnabled) {
            ImGui::DragFloat("Strength", &m_WorldCurvature, 0.00001f, 0.0f, 0.01f, "%.5f");
            ImGui::TextDisabled("Bends distant geometry downward. Try 0.0001-0.001.");
        }
    }

    // === WEATHER & WATER (Entity-Based Zones) ===
    if (ImGui::CollapsingHeader("Weather & Water")) {
        ImGui::TextWrapped("Weather and Water are entity-based game objects with bounding boxes.");
        ImGui::Spacing();
        ImGui::TextWrapped("Create via Entity > Effects menu, or add components to existing entities.");
        ImGui::Spacing();

        // List weather zones
        const char* weatherTypeNames[] = { "Clear", "Cloudy", "Rain", "Heavy Rain", "Snow", "Fog", "Storm" };
        u32 weatherZoneCount = 0;
        u32 waterVolumeCount = 0;
        if (m_World) {
            if (ImGui::TreeNode("Weather Zones")) {
                for (ECS::Entity entity : m_World->GetAllEntities()) {
                    if (m_World->HasComponent<ECS::WeatherZoneComponent>(entity)) {
                        weatherZoneCount++;
                        auto* zone = m_World->GetComponent<ECS::WeatherZoneComponent>(entity);
                        auto* name = m_World->GetComponent<ECS::NameComponent>(entity);
                        const char* label = name ? name->name.c_str() : "Unnamed";
                        const char* typeName = (zone->weatherType < 7) ? weatherTypeNames[zone->weatherType] : "Unknown";

                        ImGui::BulletText("%s [%s] (priority: %d)", label, typeName, zone->priority);
                        if (ImGui::IsItemClicked()) {
                            m_SelectedEntity = entity;
                        }
                    }
                }
                if (weatherZoneCount == 0) {
                    ImGui::TextDisabled("No weather zones in scene");
                }
                ImGui::TreePop();
            }

            if (ImGui::TreeNode("Water Volumes")) {
                for (ECS::Entity entity : m_World->GetAllEntities()) {
                    if (m_World->HasComponent<ECS::WaterVolumeComponent>(entity)) {
                        waterVolumeCount++;
                        auto* volume = m_World->GetComponent<ECS::WaterVolumeComponent>(entity);
                        auto* name = m_World->GetComponent<ECS::NameComponent>(entity);
                        auto* transform = m_World->GetComponent<ECS::TransformComponent>(entity);
                        const char* label = name ? name->name.c_str() : "Unnamed";
                        f32 surfaceY = transform ? transform->position.y : 0.0f;

                        ImGui::BulletText("%s [Y=%.1f] (priority: %d)", label, surfaceY, volume->priority);
                        if (ImGui::IsItemClicked()) {
                            m_SelectedEntity = entity;
                        }
                    }
                }
                if (waterVolumeCount == 0) {
                    ImGui::TextDisabled("No water volumes in scene");
                }
                ImGui::TreePop();
            }
        }

        ImGui::Spacing();
        ImGui::Text("Active Particles: %u / 8000", m_WeatherSystem.GetActiveParticleCount());
        if (m_WeatherSystem.IsLightningActive()) {
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.5f, 1.0f), "LIGHTNING ACTIVE!");
        }
    }

    // === WIND ===
    if (ImGui::CollapsingHeader("Wind")) {
        ImGui::TextWrapped("Global wind affects weather particles, vegetation sway, and grass.");
        ImGui::Spacing();

        Effects::WindParams params = m_WindSystem.GetGlobalParams();
        bool changed = false;

        float dir[3] = { params.direction.x, params.direction.y, params.direction.z };
        if (ImGui::DragFloat3("Direction", dir, 0.01f, -1.0f, 1.0f)) {
            params.direction = Math::Vector3(dir[0], dir[1], dir[2]);
            // Normalize if non-zero
            f32 len = params.direction.Length();
            if (len > 0.001f) params.direction = params.direction * (1.0f / len);
            changed = true;
        }
        if (ImGui::DragFloat("Strength", &params.strength, 0.05f, 0.0f, 10.0f)) changed = true;
        if (ImGui::DragFloat("Gust Strength", &params.gustStrength, 0.05f, 0.0f, 5.0f)) changed = true;
        if (ImGui::DragFloat("Gust Frequency", &params.gustFrequency, 0.01f, 0.0f, 2.0f, "%.2f Hz")) changed = true;
        if (ImGui::DragFloat("Turbulence", &params.turbulence, 0.01f, 0.0f, 2.0f)) changed = true;

        if (changed) {
            m_WindSystem.SetGlobalWind(params);
        }

        if (m_WindSystem.HasZoneOverride()) {
            ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.3f, 1.0f), "Zone override active");
        }
    }

    ImGui::End();
}

void EditorLayer::DrawGameViewPanel() {
    // Set window to be larger by default for Game View
    ImGui::SetNextWindowSize(ImVec2(640, 480), ImGuiCond_FirstUseEver);

    // Add a colored title bar when playing, and lock the window so clicks go to the game
    bool isPlaying = m_PlayMode.IsPlaying();
    bool isPlayActive = !m_PlayMode.IsStopped();
    if (isPlaying) {
        ImGui::PushStyleColor(ImGuiCol_TitleBg, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_TitleBgActive, ImVec4(0.3f, 0.7f, 0.3f, 1.0f));
    }

    ImGuiWindowFlags gameViewFlags = 0;
    if (isPlayActive) {
        gameViewFlags |= ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize;
    }
    ImGui::Begin("Game View", nullptr, gameViewFlags);

    if (isPlaying) {
        ImGui::PopStyleColor(2);
    }

    // Gather all camera entities in the scene
    std::vector<ECS::Entity> cameraEntities;
    if (m_World) {
        for (ECS::Entity entity : m_World->GetAllEntities()) {
            if (m_World->HasComponent<ECS::CameraComponent>(entity)) {
                cameraEntities.push_back(entity);
            }
        }
    }

    // Validate current selection
    if (m_SelectedGameCamera != ECS::INVALID_ENTITY) {
        bool found = false;
        for (ECS::Entity e : cameraEntities) {
            if (e == m_SelectedGameCamera) { found = true; break; }
        }
        if (!found) m_SelectedGameCamera = ECS::INVALID_ENTITY;
    }

    // Auto-select first camera if nothing selected
    if (m_SelectedGameCamera == ECS::INVALID_ENTITY && !cameraEntities.empty()) {
        m_SelectedGameCamera = cameraEntities[0];
    }

    ECS::Entity gameCameraEntity = m_SelectedGameCamera;
    ECS::CameraComponent* gameCameraComp = nullptr;
    ECS::TransformComponent* gameCameraTransform = nullptr;
    if (gameCameraEntity != ECS::INVALID_ENTITY && m_World) {
        gameCameraComp = m_World->GetComponent<ECS::CameraComponent>(gameCameraEntity);
        gameCameraTransform = m_World->GetComponent<ECS::TransformComponent>(gameCameraEntity);
    }

    // Show play mode status
    if (isPlaying) {
        ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "PLAYING");
        ImGui::SameLine();
        if (ImGui::Button("Stop")) {
            m_PlayMode.Stop();
        }
    } else if (m_PlayMode.IsPaused()) {
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.3f, 1.0f), "PAUSED");
        ImGui::SameLine();
        if (ImGui::Button("Resume")) {
            m_PlayMode.Play();
        }
        ImGui::SameLine();
        if (ImGui::Button("Stop")) {
            m_PlayMode.Stop();
        }
    } else {
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "STOPPED");
        ImGui::SameLine();
        if (ImGui::Button("Play")) {
            m_PlayMode.Play();
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Focus (F11)")) {
        m_FocusMode = true;
        if (m_PlayMode.IsStopped()) {
            m_PlayMode.Play();
        }
    }

    // Evaluate Flower button (shown when playing and a FlowerStemComponent exists)
    if (isPlaying && m_World) {
        bool hasFlowerStem = false;
        for (ECS::Entity entity : m_World->GetAllEntities()) {
            if (m_World->HasComponent<ECS::FlowerStemComponent>(entity)) {
                hasFlowerStem = true;
                break;
            }
        }
        if (hasFlowerStem) {
            ImGui::SameLine();
            if (ImGui::Button("Evaluate Flower")) {
                m_PlayMode.GetFlowerSystem()->Evaluate();
            }
        }
    }

    // Camera selector dropdown (when multiple cameras exist)
    if (cameraEntities.size() > 1) {
        std::string currentName = "None";
        if (m_SelectedGameCamera != ECS::INVALID_ENTITY && m_World->HasComponent<ECS::NameComponent>(m_SelectedGameCamera)) {
            currentName = m_World->GetComponent<ECS::NameComponent>(m_SelectedGameCamera)->name;
        } else if (m_SelectedGameCamera != ECS::INVALID_ENTITY) {
            currentName = "Camera (Entity " + std::to_string(m_SelectedGameCamera) + ")";
        }

        ImGui::SetNextItemWidth(200);
        if (ImGui::BeginCombo("Camera", currentName.c_str())) {
            for (ECS::Entity camEntity : cameraEntities) {
                std::string name;
                if (m_World->HasComponent<ECS::NameComponent>(camEntity)) {
                    name = m_World->GetComponent<ECS::NameComponent>(camEntity)->name;
                } else {
                    name = "Camera (Entity " + std::to_string(camEntity) + ")";
                }

                bool isSelected = (camEntity == m_SelectedGameCamera);
                if (ImGui::Selectable(name.c_str(), isSelected)) {
                    m_SelectedGameCamera = camEntity;
                    gameCameraEntity = camEntity;
                    gameCameraComp = m_World->GetComponent<ECS::CameraComponent>(camEntity);
                    gameCameraTransform = m_World->GetComponent<ECS::TransformComponent>(camEntity);
                }
                if (isSelected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
    }

    ImGui::Separator();

    if (gameCameraEntity == ECS::INVALID_ENTITY) {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.3f, 1.0f), "No Camera Found!");
        ImGui::TextWrapped("Add a Camera component to an entity to see the game view.");
        ImGui::Spacing();
        ImGui::TextWrapped("Go to Entity > Create Empty, then Add Component > Camera.");

        // Quick button to create a camera
        if (m_World && ImGui::Button("Create Game Camera")) {
            ECS::Entity camEntity = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(camEntity, "Game Camera");
            auto& transform = m_World->AddComponent<ECS::TransformComponent>(camEntity);
            transform.position = Math::Vector3(0, 2, 5);
            // Looking at origin (rotate 180 degrees around Y axis)
            transform.rotation = Math::Quaternion(Math::Vector3(0, 1, 0), Math::Radians(180.0f));
            auto& cam = m_World->AddComponent<ECS::CameraComponent>(camEntity);
            cam.isActive = true;
            cam.priority = 10;  // High priority to be the main camera
            cam.fieldOfView = 60.0f;
            cam.nearPlane = 0.1f;
            cam.farPlane = 1000.0f;
            m_SelectedEntity = camEntity;
            ENJIN_LOG_INFO(Editor, "Created game camera entity");
        }
    } else {
        // Show camera info
        std::string cameraName = "Game Camera";
        if (m_World->HasComponent<ECS::NameComponent>(gameCameraEntity)) {
            cameraName = m_World->GetComponent<ECS::NameComponent>(gameCameraEntity)->name;
        }

        ImGui::Text("Camera: %s", cameraName.c_str());

        if (gameCameraComp) {
            ImGui::Text("FOV: %.1f", gameCameraComp->fieldOfView);
            ImGui::Text("Near: %.2f  Far: %.1f", gameCameraComp->nearPlane, gameCameraComp->farPlane);
            ImGui::Text("Priority: %d  Active: %s", gameCameraComp->priority, gameCameraComp->isActive ? "Yes" : "No");
        }

        if (gameCameraTransform) {
            ImGui::Text("Position: %.2f, %.2f, %.2f",
                gameCameraTransform->position.x,
                gameCameraTransform->position.y,
                gameCameraTransform->position.z);
        }

        ImGui::Separator();

        // Game View Preview
        ImVec2 availSize = ImGui::GetContentRegionAvail();
        if (availSize.x > 0 && availSize.y > 0) {
            // Calculate aspect ratio for preview area (default 16:9)
            f32 gameAspect = 16.0f / 9.0f;
            f32 previewWidth = availSize.x;
            f32 previewHeight = previewWidth / gameAspect;
            if (previewHeight > availSize.y) {
                previewHeight = availSize.y;
                previewWidth = previewHeight * gameAspect;
            }

            // Update desired render target size (actual resize deferred to RenderOffscreen)
            u32 targetW = static_cast<u32>(previewWidth);
            u32 targetH = static_cast<u32>(previewHeight);
            if (targetW > 0 && targetH > 0) {
                m_GameViewWidth = targetW;
                m_GameViewHeight = targetH;
            }

            // Center the preview
            ImVec2 pos = ImGui::GetCursorScreenPos();
            ImVec2 center((availSize.x - previewWidth) * 0.5f, 0);
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + center.x);

            ImDrawList* drawList = ImGui::GetWindowDrawList();
            ImVec2 p0(pos.x + center.x, pos.y);
            ImVec2 p1(p0.x + previewWidth, p0.y + previewHeight);

            // Display render target texture or fallback dark rect
            VkDescriptorSet texId = m_GameViewRenderTarget ? m_GameViewRenderTarget->GetImGuiTextureID() : VK_NULL_HANDLE;
            bool usedImage = false;
            if (texId != VK_NULL_HANDLE) {
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + center.x);
                ImGui::Image(static_cast<ImTextureID>(reinterpret_cast<uintptr_t>(texId)),
                             ImVec2(previewWidth, previewHeight));
                usedImage = true;
                m_GameViewImageMinX = p0.x;
                m_GameViewImageMinY = p0.y;
                m_GameViewImageMaxX = p1.x;
                m_GameViewImageMaxY = p1.y;
                m_GameViewHovered = ImGui::IsItemHovered();
            } else {
                drawList->AddRectFilled(p0, p1, IM_COL32(20, 20, 30, 255));
            }

            // Weather/grass/fog are now rendered in RenderOffscreen() inside the render target pass.
            // Here we only draw ImGui overlays (lightning flash, water).

            // Find weather zone for lightning overlay
            ECS::WeatherZoneComponent* activeWeatherZone = nullptr;
            i32 bestWeatherPriority = INT_MIN;

            if (m_World && gameCameraTransform) {
                for (ECS::Entity entity : m_World->GetAllEntities()) {
                    if (m_World->HasComponent<ECS::WeatherZoneComponent>(entity)) {
                        auto* zone = m_World->GetComponent<ECS::WeatherZoneComponent>(entity);
                        auto* zoneTransform = m_World->GetComponent<ECS::TransformComponent>(entity);
                        if (zone && zoneTransform && zone->priority > bestWeatherPriority) {
                            if (zone->ContainsPoint(zoneTransform->position, gameCameraTransform->position)) {
                                activeWeatherZone = zone;
                                bestWeatherPriority = zone->priority;
                            }
                        }
                    }
                }
            }

            // Lightning flash overlay (ImGui, not Vulkan)
            if (activeWeatherZone && activeWeatherZone->weatherType == 6 &&
                activeWeatherZone->lightningEnabled && m_WeatherSystem.IsLightningActive()) {
                f32 intensity = m_WeatherSystem.GetLightningIntensity();
                u8 flashAlpha = static_cast<u8>(intensity * 200.0f);
                ImU32 flashColor = IM_COL32(255, 255, 255, flashAlpha);
                drawList->AddRectFilled(p0, p1, flashColor);
            }

            // Water is now rendered as a 3D mesh in RenderToTarget (no ImGui overlay needed)

            // Preview area border
            drawList->AddRect(p0, p1, IM_COL32(100, 100, 100, 255));

            // Status text overlay
            const char* previewText = isPlaying ? "Game Running" : "Game Preview";
            if (activeWeatherZone && activeWeatherZone->weatherType > 0) {
                previewText = isPlaying ? "Game Running (Weather Active)" : "Preview (Weather Active)";
            }
            ImVec2 textSize = ImGui::CalcTextSize(previewText);
            ImVec2 textPos((p0.x + p1.x - textSize.x) * 0.5f, p0.y + 10);
            drawList->AddText(textPos, IM_COL32(200, 200, 200, 200), previewText);

            // Interaction hint during play mode (centered near bottom of preview)
            if (isPlaying && SceneHasMouseLookController()) {
                const char* hintText = m_GameViewMouseCaptured
                    ? "Press ESC to release cursor"
                    : "Click to capture mouse";
                ImVec2 hintSize = ImGui::CalcTextSize(hintText);
                ImVec2 hintPos((p0.x + p1.x - hintSize.x) * 0.5f, p1.y - 40);
                // Semi-transparent background pill
                ImVec2 pillMin(hintPos.x - 8, hintPos.y - 4);
                ImVec2 pillMax(hintPos.x + hintSize.x + 8, hintPos.y + hintSize.y + 4);
                drawList->AddRectFilled(pillMin, pillMax, IM_COL32(0, 0, 0, 140), 6.0f);
                drawList->AddText(hintPos, IM_COL32(255, 255, 255, 200), hintText);
            }

            // Debug: zone detection status at bottom of preview
            char debugBuf[128];
            if (activeWeatherZone) {
                const char* wNames[] = {"Clear","Cloudy","Rain","HeavyRain","Snow","Fog","Storm"};
                const char* wn = (activeWeatherZone->weatherType < 7) ? wNames[activeWeatherZone->weatherType] : "?";
                snprintf(debugBuf, sizeof(debugBuf), "Zone: %s | Particles: %u", wn, m_WeatherSystem.GetActiveParticleCount());
            } else {
                snprintf(debugBuf, sizeof(debugBuf), "No weather zone at camera");
            }
            ImVec2 dbgSize = ImGui::CalcTextSize(debugBuf);
            ImVec2 dbgPos((p0.x + p1.x - dbgSize.x) * 0.5f, p1.y - 20);
            drawList->AddText(dbgPos, IM_COL32(180, 180, 100, 200), debugBuf);

            // Reserve space only if we didn't use ImGui::Image (which reserves its own)
            if (!usedImage) {
                ImGui::Dummy(ImVec2(previewWidth, previewHeight));
                m_GameViewHovered = false;
            }
        }

        ImGui::Spacing();

        // Debug info: show zone detection status
        if (gameCameraTransform) {
            ImGui::TextDisabled("Camera pos: (%.1f, %.1f, %.1f)",
                gameCameraTransform->position.x, gameCameraTransform->position.y, gameCameraTransform->position.z);
        } else if (!gameCameraComp) {
            ImGui::TextDisabled("No camera entity in scene");
        }
        if (!m_GameViewRenderTarget || !m_GameViewRenderTarget->IsValid()) {
            ImGui::TextDisabled("Render target unavailable - using fallback preview");
        }
    }

    ImGui::End();
}

void EditorLayer::DrawSkyboxPanel() {
    ImGui::Begin("Skybox", nullptr, ImGuiWindowFlags_None);

    if (!m_RenderSystem) {
        ImGui::TextDisabled("No render system available");
        ImGui::End();
        return;
    }

    Renderer::SkyboxConfig config = m_RenderSystem->GetSkyboxConfig();
    bool changed = false;

    // Type combo
    int typeIdx = static_cast<int>(config.type);
    const char* skyboxTypes[] = { "None", "Cubemap", "Procedural", "Solid Color" };
    if (ImGui::Combo("Type", &typeIdx, skyboxTypes, 4)) {
        config.type = static_cast<Renderer::SkyboxType>(typeIdx);
        changed = true;
    }

    ImGui::Separator();

    // Procedural sky controls
    if (config.type == Renderer::SkyboxType::Procedural) {
        // Presets
        ImGui::Text("Presets:");
        if (ImGui::Button("Midday")) {
            config.topColor = Math::Vector3(0.1f, 0.3f, 0.8f);
            config.horizonColor = Math::Vector3(0.5f, 0.7f, 1.0f);
            config.bottomColor = Math::Vector3(0.8f, 0.85f, 0.9f);
            config.sunDirection = Math::Vector3(0.0f, 1.0f, 0.0f);
            changed = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Sunset")) {
            config.topColor = Math::Vector3(0.1f, 0.1f, 0.4f);
            config.horizonColor = Math::Vector3(0.9f, 0.4f, 0.1f);
            config.bottomColor = Math::Vector3(0.95f, 0.6f, 0.2f);
            config.sunDirection = Math::Vector3(0.8f, 0.1f, 0.3f);
            changed = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Dawn")) {
            config.topColor = Math::Vector3(0.15f, 0.15f, 0.5f);
            config.horizonColor = Math::Vector3(0.8f, 0.5f, 0.3f);
            config.bottomColor = Math::Vector3(0.6f, 0.4f, 0.3f);
            config.sunDirection = Math::Vector3(-0.8f, 0.15f, 0.2f);
            changed = true;
        }
        if (ImGui::Button("Night")) {
            config.topColor = Math::Vector3(0.01f, 0.01f, 0.05f);
            config.horizonColor = Math::Vector3(0.05f, 0.05f, 0.15f);
            config.bottomColor = Math::Vector3(0.02f, 0.02f, 0.08f);
            config.sunDirection = Math::Vector3(0.0f, -1.0f, 0.0f);
            changed = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Overcast")) {
            config.topColor = Math::Vector3(0.5f, 0.5f, 0.55f);
            config.horizonColor = Math::Vector3(0.6f, 0.6f, 0.63f);
            config.bottomColor = Math::Vector3(0.55f, 0.55f, 0.58f);
            config.sunDirection = Math::Vector3(0.3f, 0.7f, 0.2f);
            changed = true;
        }

        ImGui::Separator();
        ImGui::Text("Colors:");
        changed |= ImGui::ColorEdit3("Top Color", &config.topColor.x);
        changed |= ImGui::ColorEdit3("Horizon Color", &config.horizonColor.x);
        changed |= ImGui::ColorEdit3("Bottom Color", &config.bottomColor.x);

        ImGui::Separator();
        changed |= ImGui::DragFloat3("Sun Direction", &config.sunDirection.x, 0.01f, -1.0f, 1.0f);
    }

    // Solid color controls
    if (config.type == Renderer::SkyboxType::SolidColor) {
        changed |= ImGui::ColorEdit3("Sky Color", &config.solidColor.x);
    }

    // Cubemap controls
    if (config.type == Renderer::SkyboxType::Cubemap) {
        ImGui::Text("Cubemap Faces:");
        const char* faceLabels[] = { "Right (+X)", "Left (-X)", "Top (+Y)", "Bottom (-Y)", "Front (+Z)", "Back (-Z)" };
        for (int i = 0; i < 6; ++i) {
            char buf[256];
            strncpy(buf, config.cubemapPaths[i].c_str(), sizeof(buf) - 1);
            buf[sizeof(buf) - 1] = '\0';
            if (ImGui::InputText(faceLabels[i], buf, sizeof(buf))) {
                config.cubemapPaths[i] = buf;
                changed = true;
            }
        }
    }

    // Rotation slider for all non-None types
    if (config.type != Renderer::SkyboxType::None) {
        ImGui::Separator();
        changed |= ImGui::SliderFloat("Rotation", &config.rotation, 0.0f, 360.0f, "%.1f deg");
    }

    if (changed) {
        m_RenderSystem->SetSkybox(config);
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
            }
            if (ImGui::MenuItem("Load Additive")) {
                m_SceneManager.LoadSceneAdditive(scene.name);
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
                m_SceneManager.SaveProject(path);
            }
        } else {
            m_SceneManager.SaveProject();
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

void EditorLayer::DrawStatsOverlay() {
    const float DISTANCE = 10.0f;
    ImGuiIO& io = ImGui::GetIO();

    ImVec2 windowPos = ImVec2(io.DisplaySize.x - DISTANCE, DISTANCE);
    ImVec2 windowPivot = ImVec2(1.0f, 0.0f);
    ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always, windowPivot);
    ImGui::SetNextWindowBgAlpha(0.35f);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration |
                             ImGuiWindowFlags_AlwaysAutoResize |
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
            ImGui::Text("Entities: %zu", m_World->GetAllEntities().size());
        }

        if (m_CameraController) {
            ImGui::Separator();
            ImGui::Text("Yaw: %.1f  Pitch: %.1f", m_CameraController->GetYaw(), m_CameraController->GetPitch());
        }

        // GPU info (if available from renderer)
        if (m_Renderer && m_Renderer->GetContext()) {
            ImGui::Separator();
            VkPhysicalDeviceProperties props;
            vkGetPhysicalDeviceProperties(m_Renderer->GetContext()->GetPhysicalDevice(), &props);
            ImGui::Text("GPU: %s", props.deviceName);
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
    }
    ImGui::End();
}

void EditorLayer::DrawSplashScreen() {
    ImGuiIO& io = ImGui::GetIO();

    // Calculate fade alpha
    f32 alpha = 1.0f;
    if (m_SplashTimer > m_SplashFadeStart) {
        f32 fadeProgress = (m_SplashTimer - m_SplashFadeStart) / (m_SplashDuration - m_SplashFadeStart);
        alpha = 1.0f - fadeProgress;
    }

    // Full-screen dark overlay
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::SetNextWindowBgAlpha(0.95f * alpha);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration |
                             ImGuiWindowFlags_NoMove |
                             ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoSavedSettings |
                             ImGuiWindowFlags_NoFocusOnAppearing |
                             ImGuiWindowFlags_NoBringToFrontOnFocus |
                             ImGuiWindowFlags_NoNav |
                             ImGuiWindowFlags_NoInputs;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.05f, 0.05f, 0.08f, 1.0f));

    if (ImGui::Begin("##Splash", nullptr, flags)) {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImVec2 center(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f);

        // Draw decorative lines
        u32 lineColor = IM_COL32(100, 120, 180, static_cast<int>(150 * alpha));
        f32 lineWidth = io.DisplaySize.x * 0.3f;
        drawList->AddLine(
            ImVec2(center.x - lineWidth, center.y - 60),
            ImVec2(center.x + lineWidth, center.y - 60),
            lineColor, 2.0f);
        drawList->AddLine(
            ImVec2(center.x - lineWidth, center.y + 60),
            ImVec2(center.x + lineWidth, center.y + 60),
            lineColor, 2.0f);

        // Main title
        const char* title = "TEGE";
        ImGui::PushFont(nullptr);  // Use default font (will be larger if custom font is set)

        // Calculate text size for centering
        ImVec2 titleSize = ImGui::CalcTextSize(title);
        f32 titleScale = 2.5f;  // Scale up the text
        titleSize.x *= titleScale;
        titleSize.y *= titleScale;

        ImVec2 titlePos(center.x - titleSize.x * 0.5f, center.y - titleSize.y * 0.5f);

        // Draw title with glow effect
        u32 glowColor = IM_COL32(80, 100, 200, static_cast<int>(100 * alpha));
        for (int i = 0; i < 3; i++) {
            f32 offset = (i + 1) * 2.0f;
            drawList->AddText(nullptr, 14.0f * titleScale,
                ImVec2(titlePos.x + offset, titlePos.y + offset),
                glowColor, title);
        }

        // Main title text
        u32 titleColor = IM_COL32(200, 210, 255, static_cast<int>(255 * alpha));
        drawList->AddText(nullptr, 14.0f * titleScale, titlePos, titleColor, title);

        ImGui::PopFont();

        // Version info
        const char* version = "v0.1.0 Alpha";
        ImVec2 versionSize = ImGui::CalcTextSize(version);
        ImVec2 versionPos(center.x - versionSize.x * 0.5f, io.DisplaySize.y - 50);
        u32 versionColor = IM_COL32(100, 110, 140, static_cast<int>(180 * alpha));
        drawList->AddText(versionPos, versionColor, version);

        // Loading dots animation
        int dots = static_cast<int>(m_SplashTimer * 3.0f) % 4;
        const char* loadingTexts[] = { "Loading", "Loading.", "Loading..", "Loading..." };
        const char* loadingText = loadingTexts[dots];
        ImVec2 loadingSize = ImGui::CalcTextSize(loadingText);
        ImVec2 loadingPos(center.x - loadingSize.x * 0.5f, center.y + 80);
        u32 loadingColor = IM_COL32(120, 130, 160, static_cast<int>(200 * alpha));
        drawList->AddText(loadingPos, loadingColor, loadingText);

        // "by marty64" credit at bottom
        const char* credit = "by marty64";
        ImVec2 creditSize = ImGui::CalcTextSize(credit);
        ImVec2 creditPos(center.x - creditSize.x * 0.5f, io.DisplaySize.y * 0.75f);
        u32 creditColor = IM_COL32(100, 100, 110, static_cast<int>(140 * alpha));
        drawList->AddText(creditPos, creditColor, credit);
    }
    ImGui::End();

    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
}

void EditorLayer::DrawTemplateSelector() {
    ImGuiIO& io = ImGui::GetIO();

    // Full-screen dark background
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::SetNextWindowBgAlpha(0.97f);

    ImGuiWindowFlags bgFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNav;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.08f, 0.08f, 0.10f, 1.0f));

    if (ImGui::Begin("##TemplateBackground", nullptr, bgFlags)) {
        ImDrawList* drawList = ImGui::GetWindowDrawList();

        // Title
        const char* title = "TEGE";
        ImVec2 titleSize = ImGui::CalcTextSize(title);
        f32 titleScale = 3.0f;
        ImVec2 titlePos(io.DisplaySize.x * 0.5f - titleSize.x * titleScale * 0.5f, 30.0f);
        drawList->AddText(nullptr, 14.0f * titleScale, titlePos,
            IM_COL32(200, 210, 255, 255), title);

        const char* subtitle = "Choose a template to get started";
        ImVec2 subtitleSize = ImGui::CalcTextSize(subtitle);
        drawList->AddText(
            ImVec2(io.DisplaySize.x * 0.5f - subtitleSize.x * 0.5f, 40.0f + 14.0f * titleScale + 15.0f),
            IM_COL32(120, 130, 160, 200), subtitle);

        // Template card layout
        struct TemplateInfo {
            const char* id;
            const char* name;
            const char* description;
            ImVec4 accentColor;
        };

        TemplateInfo builtinTemplates[] = {
            { "blank",       "Blank",           "Empty scene\nStart from scratch",                    ImVec4(0.5f, 0.5f, 0.5f, 1.0f) },
            { "platformer",  "2D Platformer",   "Side-scrolling\nOrtho camera + player",              ImVec4(0.3f, 0.8f, 0.3f, 1.0f) },
            { "topdown2d",   "2D Top-Down",     "Top-down view\nOrtho camera + player",               ImVec4(0.3f, 0.6f, 0.9f, 1.0f) },
            { "isometric",   "3D Isometric",    "45-degree CRPG\nPerspective + player",               ImVec4(0.9f, 0.6f, 0.2f, 1.0f) },
            { "thirdperson", "3D Third Person",  "Over-the-shoulder\nPerspective + player",            ImVec4(0.8f, 0.3f, 0.3f, 1.0f) },
            { "firstperson", "3D First Person",  "Eye-level FPS\nPerspective + player",                ImVec4(0.7f, 0.3f, 0.8f, 1.0f) },
            { "visualnovel", "Visual Novel",     "Story-driven\nDialogue + sprites",                   ImVec4(0.9f, 0.7f, 0.9f, 1.0f) },
            { "rpg_village", "RPG Village",     "3D RPG scene\nNPC + pickups + enemy",                 ImVec4(0.4f, 0.7f, 0.4f, 1.0f) },
            { "survival",    "Survival",        "3D Survival\nHazards + temp zones",                   ImVec4(0.7f, 0.5f, 0.2f, 1.0f) },
            { "gamemanager", "Game Manager",    "Singleton pattern\nScore + state machine",            ImVec4(0.6f, 0.6f, 0.8f, 1.0f) },
            { "narrative",   "3D Narrative",    "Dialogue sequencing\nCondition-based branching",      ImVec4(0.8f, 0.6f, 0.9f, 1.0f) },
            { "racing",      "4P Racing",       "4-player splitscreen\nRace track + vehicles",         ImVec4(0.9f, 0.3f, 0.1f, 1.0f) },
            { "arena",       "Arena Fighter",   "Smash-style 4-8 players\nSide-view + dynamic camera", ImVec4(1.0f, 0.5f, 0.0f, 1.0f) },
            { "ps1rpg",      "PS1 RPG",         "Classic 3D turn-based\nOverworld + battle system",    ImVec4(0.3f, 0.3f, 0.9f, 1.0f) },
            { "citybuilder", "City Builder",   "Isometric city sim\n3D or faux-iso mode",             ImVec4(0.2f, 0.7f, 0.7f, 1.0f) },
            { "fpsarena",    "FPS Arena",      "First-person shooter\nWeapons + respawn + ammo",       ImVec4(0.9f, 0.2f, 0.2f, 1.0f) },
            { "teamsports",  "Team Sports",    "3D soccer/basketball\n2 teams + ball + goals",         ImVec4(0.2f, 0.8f, 0.3f, 1.0f) },
            { "towerdefense","Tower Defense",  "Isometric TD\nPaths + turrets + waves",                ImVec4(0.8f, 0.6f, 0.2f, 1.0f) },
            { "puzzle",      "Puzzle Plat.",   "Pushable blocks\nPlates + doors + switches",           ImVec4(0.5f, 0.8f, 0.9f, 1.0f) },
            { "horror",      "Horror",         "Walking sim\nFlashlight + notes + dark",               ImVec4(0.3f, 0.1f, 0.3f, 1.0f) },
            { "runner",      "Endless Runner", "Auto-scroll\nLanes + obstacles + score",               ImVec4(0.9f, 0.6f, 0.1f, 1.0f) },
            { "flower",      "Flower Garden", "Procedural flower\nPluckable petals + score",             ImVec4(0.9f, 0.4f, 0.6f, 1.0f) },
            { "fixedcam",    "Fixed Camera",  "Fixed-angle 3rd person\nClassic RE / God of War",           ImVec4(0.6f, 0.25f, 0.5f, 1.0f) },
            { "dungeon",     "SMT Dungeon",   "First-person crawler\nGrid dungeon + encounters",          ImVec4(0.15f, 0.6f, 0.15f, 1.0f) },
        };
        int builtinCount = 24;

        f32 cardW = 345.0f;
        f32 cardH = 240.0f;
        f32 cardPad = 22.0f;
        f32 startY = 130.0f;

        // Calculate total cards per row
        int totalCards = builtinCount + static_cast<int>(m_CustomTemplateNames.size());
        f32 totalWidth = totalCards * (cardW + cardPad) - cardPad;
        f32 maxRowWidth = io.DisplaySize.x - 60.0f;

        // Layout cards in rows
        int cardsPerRow = static_cast<int>((maxRowWidth + cardPad) / (cardW + cardPad));
        if (cardsPerRow < 1) cardsPerRow = 1;

        bool templateChosen = false;
        std::string chosenTemplate;

        // Draw builtin templates
        for (int i = 0; i < builtinCount; ++i) {
            int row = i / cardsPerRow;
            int col = i % cardsPerRow;
            int itemsInRow = (row < (builtinCount + (int)m_CustomTemplateNames.size()) / cardsPerRow)
                ? cardsPerRow
                : ((builtinCount + (int)m_CustomTemplateNames.size()) % cardsPerRow);
            if (itemsInRow == 0) itemsInRow = cardsPerRow;

            f32 rowWidth = itemsInRow * (cardW + cardPad) - cardPad;
            f32 rowStartX = (io.DisplaySize.x - rowWidth) * 0.5f;

            ImVec2 cardPos(rowStartX + col * (cardW + cardPad), startY + row * (cardH + cardPad));
            ImVec2 cardEnd(cardPos.x + cardW, cardPos.y + cardH);

            // Hit test
            bool hovered = (io.MousePos.x >= cardPos.x && io.MousePos.x <= cardEnd.x &&
                           io.MousePos.y >= cardPos.y && io.MousePos.y <= cardEnd.y);

            // Card background
            ImU32 bgCol = hovered ? IM_COL32(40, 45, 60, 255) : IM_COL32(25, 28, 35, 255);
            drawList->AddRectFilled(cardPos, cardEnd, bgCol, 8.0f);

            // Accent bar at top
            ImVec4 accent = builtinTemplates[i].accentColor;
            ImU32 accentCol = IM_COL32(
                (int)(accent.x * 255), (int)(accent.y * 255),
                (int)(accent.z * 255), hovered ? 255 : 180);
            drawList->AddRectFilled(cardPos, ImVec2(cardEnd.x, cardPos.y + 4.0f), accentCol, 8.0f, ImDrawFlags_RoundCornersTop);

            // Border
            ImU32 borderCol = hovered ? accentCol : IM_COL32(60, 65, 80, 150);
            drawList->AddRect(cardPos, cardEnd, borderCol, 8.0f, 0, hovered ? 2.0f : 1.0f);

            // Template name
            ImVec2 nameSize = ImGui::CalcTextSize(builtinTemplates[i].name);
            drawList->AddText(
                ImVec2(cardPos.x + (cardW - nameSize.x) * 0.5f, cardPos.y + 18.0f),
                IM_COL32(220, 225, 245, 255), builtinTemplates[i].name);

            // Description (centered, multi-line)
            const char* desc = builtinTemplates[i].description;
            // Split by \n and draw each line
            std::string descStr(desc);
            f32 lineY = cardPos.y + 50.0f;
            std::istringstream iss(descStr);
            std::string line;
            while (std::getline(iss, line, '\n')) {
                ImVec2 lineSize = ImGui::CalcTextSize(line.c_str());
                drawList->AddText(
                    ImVec2(cardPos.x + (cardW - lineSize.x) * 0.5f, lineY),
                    IM_COL32(140, 145, 165, 200), line.c_str());
                lineY += 18.0f;
            }

            // Click handler
            if (hovered && ImGui::IsMouseClicked(0)) {
                templateChosen = true;
                chosenTemplate = builtinTemplates[i].id;
            }
        }

        // Draw custom templates
        for (int i = 0; i < static_cast<int>(m_CustomTemplateNames.size()); ++i) {
            int idx = builtinCount + i;
            int row = idx / cardsPerRow;
            int col = idx % cardsPerRow;
            int totalInRow = cardsPerRow;
            int remaining = totalCards - row * cardsPerRow;
            if (remaining < cardsPerRow) totalInRow = remaining;

            f32 rowWidth = totalInRow * (cardW + cardPad) - cardPad;
            f32 rowStartX = (io.DisplaySize.x - rowWidth) * 0.5f;

            ImVec2 cardPos(rowStartX + col * (cardW + cardPad), startY + row * (cardH + cardPad));
            ImVec2 cardEnd(cardPos.x + cardW, cardPos.y + cardH);

            bool hovered = (io.MousePos.x >= cardPos.x && io.MousePos.x <= cardEnd.x &&
                           io.MousePos.y >= cardPos.y && io.MousePos.y <= cardEnd.y);

            ImU32 bgCol = hovered ? IM_COL32(40, 45, 60, 255) : IM_COL32(25, 28, 35, 255);
            drawList->AddRectFilled(cardPos, cardEnd, bgCol, 8.0f);

            // Custom template accent (teal)
            ImU32 accentCol = hovered ? IM_COL32(0, 200, 180, 255) : IM_COL32(0, 200, 180, 150);
            drawList->AddRectFilled(cardPos, ImVec2(cardEnd.x, cardPos.y + 4.0f), accentCol, 8.0f, ImDrawFlags_RoundCornersTop);

            ImU32 borderCol = hovered ? accentCol : IM_COL32(60, 65, 80, 150);
            drawList->AddRect(cardPos, cardEnd, borderCol, 8.0f, 0, hovered ? 2.0f : 1.0f);

            // Name
            ImVec2 nameSize = ImGui::CalcTextSize(m_CustomTemplateNames[i].c_str());
            drawList->AddText(
                ImVec2(cardPos.x + (cardW - nameSize.x) * 0.5f, cardPos.y + 18.0f),
                IM_COL32(220, 225, 245, 255), m_CustomTemplateNames[i].c_str());

            // "Custom Template" label
            const char* customLabel = "Custom Template";
            ImVec2 labelSize = ImGui::CalcTextSize(customLabel);
            drawList->AddText(
                ImVec2(cardPos.x + (cardW - labelSize.x) * 0.5f, cardPos.y + 55.0f),
                IM_COL32(0, 180, 160, 200), customLabel);

            if (hovered && ImGui::IsMouseClicked(0)) {
                templateChosen = true;
                chosenTemplate = "custom:" + std::to_string(i);
            }
        }

        // --- Recent Projects section ---
        if (!m_EditorSettings.recentProjects.empty()) {
            // Compute row start after all template cards
            int totalTemplateCards = builtinCount + static_cast<int>(m_CustomTemplateNames.size());
            int lastTemplateRow = (totalTemplateCards > 0) ? ((totalTemplateCards - 1) / cardsPerRow) : 0;
            f32 recentSectionY = startY + (lastTemplateRow + 1) * (cardH + cardPad) + 10.0f;

            // Section header
            const char* recentLabel = "RECENT PROJECTS";
            ImVec2 labelSize = ImGui::CalcTextSize(recentLabel);
            drawList->AddText(
                ImVec2((io.DisplaySize.x - labelSize.x) * 0.5f, recentSectionY),
                IM_COL32(160, 165, 185, 220), recentLabel);

            // Divider line
            drawList->AddLine(
                ImVec2(60, recentSectionY + 20.0f),
                ImVec2(io.DisplaySize.x - 60, recentSectionY + 20.0f),
                IM_COL32(60, 65, 80, 150), 1.0f);

            f32 recentStartY = recentSectionY + 30.0f;
            f32 recentCardH = 80.0f; // Shorter cards for recent projects

            int recentCount = static_cast<int>(m_EditorSettings.recentProjects.size());
            for (int i = 0; i < recentCount; ++i) {
                int row = i / cardsPerRow;
                int col = i % cardsPerRow;
                int itemsInRow = recentCount - row * cardsPerRow;
                if (itemsInRow > cardsPerRow) itemsInRow = cardsPerRow;

                f32 rowWidth = itemsInRow * (cardW + cardPad) - cardPad;
                f32 rowStartX = (io.DisplaySize.x - rowWidth) * 0.5f;

                ImVec2 rPos(rowStartX + col * (cardW + cardPad), recentStartY + row * (recentCardH + cardPad));
                ImVec2 rEnd(rPos.x + cardW, rPos.y + recentCardH);

                bool hovered = (io.MousePos.x >= rPos.x && io.MousePos.x <= rEnd.x &&
                               io.MousePos.y >= rPos.y && io.MousePos.y <= rEnd.y);

                ImU32 bgCol = hovered ? IM_COL32(40, 45, 60, 255) : IM_COL32(25, 28, 35, 255);
                drawList->AddRectFilled(rPos, rEnd, bgCol, 8.0f);

                // Accent bar (blue-ish for recent projects)
                ImU32 recentAccent = hovered ? IM_COL32(80, 140, 220, 255) : IM_COL32(60, 110, 180, 180);
                drawList->AddRectFilled(rPos, ImVec2(rEnd.x, rPos.y + 3.0f), recentAccent, 8.0f, ImDrawFlags_RoundCornersTop);

                ImU32 borderCol = hovered ? recentAccent : IM_COL32(60, 65, 80, 150);
                drawList->AddRect(rPos, rEnd, borderCol, 8.0f, 0, hovered ? 2.0f : 1.0f);

                // Extract filename from path for display
                std::filesystem::path fsPath(m_EditorSettings.recentProjects[i]);
                std::string displayName = fsPath.stem().string();
                if (displayName.length() > 28) {
                    displayName = displayName.substr(0, 25) + "...";
                }

                // File icon indicator
                const char* fileIcon = "[Scene]";
                ImVec2 iconSize = ImGui::CalcTextSize(fileIcon);
                drawList->AddText(
                    ImVec2(rPos.x + 12.0f, rPos.y + 12.0f),
                    IM_COL32(80, 140, 220, 200), fileIcon);

                // Project name (bold-style, larger)
                ImVec2 nameSize = ImGui::CalcTextSize(displayName.c_str());
                drawList->AddText(
                    ImVec2(rPos.x + 12.0f + iconSize.x + 8.0f, rPos.y + 12.0f),
                    IM_COL32(220, 225, 245, 255), displayName.c_str());

                // Full path (smaller, dimmed)
                std::string pathStr = m_EditorSettings.recentProjects[i];
                if (pathStr.length() > 55) {
                    pathStr = "..." + pathStr.substr(pathStr.length() - 52);
                }
                drawList->AddText(
                    ImVec2(rPos.x + 12.0f, rPos.y + 36.0f),
                    IM_COL32(120, 125, 145, 180), pathStr.c_str());

                // File exists indicator
                bool exists = std::filesystem::exists(m_EditorSettings.recentProjects[i]);
                const char* statusText = exists ? "Ready" : "Missing";
                ImU32 statusCol = exists ? IM_COL32(80, 200, 120, 200) : IM_COL32(200, 80, 80, 200);
                ImVec2 statusSize = ImGui::CalcTextSize(statusText);
                drawList->AddText(
                    ImVec2(rEnd.x - statusSize.x - 12.0f, rPos.y + 56.0f),
                    statusCol, statusText);

                // Click handler — open the recent project
                if (hovered && exists && ImGui::IsMouseClicked(0)) {
                    templateChosen = false; // Don't apply template
                    m_ShowTemplateSelector = false;
                    OpenScene(m_EditorSettings.recentProjects[i]);
                }
            }
        }

        // Bottom bar with "Save Current as Template" and "Open Scene" buttons
        f32 bottomY = io.DisplaySize.y - 60.0f;
        drawList->AddLine(ImVec2(30, bottomY - 15), ImVec2(io.DisplaySize.x - 30, bottomY - 15),
            IM_COL32(60, 65, 80, 150), 1.0f);

        // Position buttons
        ImGui::SetCursorPos(ImVec2(io.DisplaySize.x * 0.5f - 170.0f, bottomY));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.16f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.22f, 0.24f, 0.3f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.18f, 0.2f, 0.25f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.75f, 0.78f, 0.85f, 1.0f));

        if (ImGui::Button("Open Existing Scene...", ImVec2(160, 30))) {
            std::vector<FileFilter> filters = {{ "Enjin Scene", "*.enjin" }, { "All Files", "*.*" }};
            std::string path = FileDialog::OpenFile("Open Scene", filters);
            if (!path.empty()) {
                m_ShowTemplateSelector = false;
                OpenScene(path);
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Skip (Empty Scene)", ImVec2(160, 30))) {
            m_ShowTemplateSelector = false;
        }

        ImGui::PopStyleColor(4);

        // Apply chosen template
        if (templateChosen) {
            m_ShowTemplateSelector = false;
            ApplyTemplate(chosenTemplate);
        }
    }
    ImGui::End();

    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
}

void EditorLayer::ApplyTemplate(const std::string& templateId) {
    if (!m_World) return;

    // Clear existing scene and undo history
    m_World->Clear();
    m_SelectedEntity = ECS::INVALID_ENTITY;
    m_UndoRedo.Clear();

    // --- Configure editor layout per template ---
    // Reset to defaults first
    m_Layout = LayoutConfig{};
    EditorPanel corePanels = EditorPanel::Hierarchy | EditorPanel::Inspector |
                             EditorPanel::Console | EditorPanel::AssetBrowser;

    if (templateId == "blank") {
        // Full editor: everything visible for exploration
        m_Layout.panels = EditorPanel::All;
    }
    else if (templateId == "platformer" || templateId == "topdown2d") {
        // 2D: compact hierarchy, wide game view, no 3D-specific panels
        m_Layout.panels = corePanels | EditorPanel::GameView | EditorPanel::Settings;
        m_Layout.leftWidth = 0.15f;
        m_Layout.rightWidth = 0.20f;
        m_Layout.bottomHeight = 0.18f;
        m_Layout.gameViewW = 700.0f;
        m_Layout.gameViewH = 450.0f;
    }
    else if (templateId == "isometric" || templateId == "citybuilder" || templateId == "towerdefense") {
        // Top-down 3D: standard layout with scene list + skybox, moderate game view
        m_Layout.panels = corePanels | EditorPanel::GameView | EditorPanel::Skybox |
                          EditorPanel::SceneList | EditorPanel::Settings;
        m_Layout.leftWidth = 0.16f;
        m_Layout.rightWidth = 0.22f;
        m_Layout.gameViewW = 650.0f;
        m_Layout.gameViewH = 420.0f;
    }
    else if (templateId == "thirdperson" || templateId == "arena" || templateId == "teamsports") {
        // Standard 3D action: game view, inspector for controller tuning, skybox
        m_Layout.panels = corePanels | EditorPanel::GameView | EditorPanel::Skybox | EditorPanel::Settings;
        m_Layout.leftWidth = 0.16f;
        m_Layout.rightWidth = 0.23f;
        m_Layout.inspectorSplit = 0.65f;
        m_Layout.gameViewW = 680.0f;
        m_Layout.gameViewH = 440.0f;
    }
    else if (templateId == "firstperson" || templateId == "fpsarena") {
        // FPS: large immersive game view, narrow hierarchy, inspector for mouse look tuning
        m_Layout.panels = corePanels | EditorPanel::GameView | EditorPanel::Skybox;
        m_Layout.leftWidth = 0.13f;
        m_Layout.rightWidth = 0.20f;
        m_Layout.bottomHeight = 0.18f;
        m_Layout.gameViewW = 800.0f;
        m_Layout.gameViewH = 500.0f;
    }
    else if (templateId == "visualnovel" || templateId == "narrative") {
        // Story-driven: large game view for dialogue display, inspector for editing
        m_Layout.panels = corePanels | EditorPanel::GameView;
        m_Layout.leftWidth = 0.14f;
        m_Layout.rightWidth = 0.24f;
        m_Layout.bottomHeight = 0.15f;
        m_Layout.inspectorSplit = 0.7f;
        m_Layout.gameViewW = 750.0f;
        m_Layout.gameViewH = 480.0f;
    }
    else if (templateId == "rpg_village") {
        // Full RPG: all panels including scene list, effects for weather/time
        m_Layout.panels = corePanels | EditorPanel::GameView | EditorPanel::Effects |
                          EditorPanel::SceneList | EditorPanel::Skybox | EditorPanel::Settings;
        m_Layout.leftWidth = 0.17f;
        m_Layout.rightWidth = 0.22f;
        m_Layout.inspectorSplit = 0.55f;
        m_Layout.gameViewW = 640.0f;
        m_Layout.gameViewH = 400.0f;
    }
    else if (templateId == "survival") {
        // Exploration: large game view, effects for weather, console for debug
        m_Layout.panels = corePanels | EditorPanel::GameView | EditorPanel::Effects |
                          EditorPanel::Skybox | EditorPanel::Settings;
        m_Layout.leftWidth = 0.15f;
        m_Layout.rightWidth = 0.22f;
        m_Layout.gameViewW = 720.0f;
        m_Layout.gameViewH = 450.0f;
    }
    else if (templateId == "gamemanager") {
        // System design: console prominent, hierarchy/inspector standard, no game view emphasis
        m_Layout.panels = corePanels | EditorPanel::GameView | EditorPanel::SceneList | EditorPanel::Settings;
        m_Layout.leftWidth = 0.18f;
        m_Layout.rightWidth = 0.22f;
        m_Layout.bottomHeight = 0.28f;  // Larger console area
        m_Layout.gameViewW = 500.0f;
        m_Layout.gameViewH = 350.0f;
    }
    else if (templateId == "racing") {
        // Splitscreen: game view as large as possible for viewport subdivision
        m_Layout.panels = corePanels | EditorPanel::GameView | EditorPanel::Skybox;
        m_Layout.leftWidth = 0.12f;
        m_Layout.rightWidth = 0.18f;
        m_Layout.bottomHeight = 0.16f;
        m_Layout.gameViewW = 900.0f;
        m_Layout.gameViewH = 550.0f;
    }
    else if (templateId == "ps1rpg") {
        // Retro: post-processing + effects visible for retro tuning, game view centered
        m_Layout.panels = corePanels | EditorPanel::GameView | EditorPanel::PostProcessing |
                          EditorPanel::Effects | EditorPanel::Settings;
        m_Layout.leftWidth = 0.15f;
        m_Layout.rightWidth = 0.22f;
        m_Layout.gameViewW = 640.0f;
        m_Layout.gameViewH = 420.0f;
    }
    else if (templateId == "horror") {
        // Atmospheric: large immersive game view, effects + post-processing for mood
        m_Layout.panels = corePanels | EditorPanel::GameView | EditorPanel::Effects |
                          EditorPanel::PostProcessing | EditorPanel::Skybox;
        m_Layout.leftWidth = 0.14f;
        m_Layout.rightWidth = 0.21f;
        m_Layout.bottomHeight = 0.18f;
        m_Layout.gameViewW = 780.0f;
        m_Layout.gameViewH = 500.0f;
    }
    else if (templateId == "puzzle") {
        // Puzzle: game view prominent, inspector for puzzle editing, minimal panels
        m_Layout.panels = corePanels | EditorPanel::GameView;
        m_Layout.leftWidth = 0.15f;
        m_Layout.rightWidth = 0.22f;
        m_Layout.bottomHeight = 0.18f;
        m_Layout.gameViewW = 700.0f;
        m_Layout.gameViewH = 450.0f;
    }
    else if (templateId == "runner") {
        // Endless runner: wide game view (horizontal gameplay), compact panels
        m_Layout.panels = corePanels | EditorPanel::GameView | EditorPanel::Skybox;
        m_Layout.leftWidth = 0.13f;
        m_Layout.rightWidth = 0.19f;
        m_Layout.bottomHeight = 0.16f;
        m_Layout.gameViewW = 850.0f;
        m_Layout.gameViewH = 400.0f;  // Wide aspect for runner
    }
    else if (templateId == "flower") {
        // Interactive art: large centered game view, effects for wind/weather, inspector for physics
        m_Layout.panels = corePanels | EditorPanel::GameView | EditorPanel::Effects |
                          EditorPanel::Skybox | EditorPanel::Settings;
        m_Layout.leftWidth = 0.14f;
        m_Layout.rightWidth = 0.23f;
        m_Layout.inspectorSplit = 0.65f;
        m_Layout.gameViewW = 720.0f;
        m_Layout.gameViewH = 480.0f;
    }
    else if (templateId == "fixedcam") {
        // Fixed camera: large game view (camera framing is key), inspector for zones
        m_Layout.panels = corePanels | EditorPanel::GameView | EditorPanel::Settings;
        m_Layout.leftWidth = 0.15f;
        m_Layout.rightWidth = 0.22f;
        m_Layout.gameViewW = 750.0f;
        m_Layout.gameViewH = 480.0f;
    }
    else if (templateId == "dungeon") {
        // SMT dungeon: large first-person game view, console for game messages, compact hierarchy
        m_Layout.panels = corePanels | EditorPanel::GameView | EditorPanel::Settings;
        m_Layout.leftWidth = 0.14f;
        m_Layout.rightWidth = 0.21f;
        m_Layout.bottomHeight = 0.20f;
        m_Layout.gameViewW = 800.0f;
        m_Layout.gameViewH = 520.0f;
    }
    else {
        // Fallback: standard layout
        m_Layout.panels = EditorPanel::All;
    }

    m_VisiblePanels = m_Layout.panels;
    m_ForceLayout = true;

    // Handle custom templates
    if (templateId.substr(0, 7) == "custom:") {
        int idx = std::stoi(templateId.substr(7));
        if (idx >= 0 && idx < static_cast<int>(m_CustomTemplatePaths.size())) {
            OpenScene(m_CustomTemplatePaths[idx]);
        }
        return;
    }

    if (templateId == "blank") {
        // Just a directional light so the scene isn't dark
        ECS::Entity light = m_World->CreateEntity();
        m_World->AddComponent<ECS::NameComponent>(light, "Directional Light");
        auto& lightTransform = m_World->AddComponent<ECS::TransformComponent>(light);
        lightTransform.position = Math::Vector3(0.0f, 10.0f, 0.0f);
        lightTransform.rotation = Math::Quaternion(Math::Vector3(1, 0, 0), Math::Radians(-45.0f));
        auto& lightComp = m_World->AddComponent<ECS::LightComponent>(light);
        lightComp.type = ECS::LightType::Directional;
        lightComp.intensity = 1.0f;
        return;
    }

    // --- Common setup: ground plane ---
    auto createGround = [&]() -> ECS::Entity {
        ECS::Entity ground = m_World->CreateEntity();
        m_World->AddComponent<ECS::NameComponent>(ground, "Ground");
        auto& gt = m_World->AddComponent<ECS::TransformComponent>(ground);
        gt.position = Math::Vector3(0.0f, 0.0f, 0.0f);
        gt.scale = Math::Vector3(50.0f, 0.1f, 50.0f);
        auto& gmat = m_World->AddComponent<ECS::MaterialComponent>(ground);
        gmat.baseColor = Math::Vector3(0.35f, 0.55f, 0.3f);
        gmat.roughness = 0.9f;

        // Create a simple cube mesh for the ground
        m_World->AddComponent<ECS::MeshComponent>(ground, Renderer::MeshFactory::CreateCube(1.0f));

        auto& col = m_World->AddComponent<ECS::BoxColliderComponent>(ground);
        col.size = Math::Vector3(50.0f, 0.1f, 50.0f);
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

    // --- Common: 3D player entity with capsule mesh ---
    auto createPlayer3D = [&](const std::string& name) -> ECS::Entity {
        ECS::Entity player = m_World->CreateEntity();
        m_World->AddComponent<ECS::NameComponent>(player, name);
        auto& pt = m_World->AddComponent<ECS::TransformComponent>(player);
        pt.position = Math::Vector3(0.0f, 1.0f, 0.0f);
        auto& pmat = m_World->AddComponent<ECS::MaterialComponent>(player);
        pmat.baseColor = Math::Vector3(0.2f, 0.4f, 0.9f);
        m_World->AddComponent<ECS::MeshComponent>(player, Renderer::MeshFactory::CreateCapsule(0.3f, 1.0f));
        return player;
    };

    // --- 2D player entity with capsule2D mesh ---
    auto createPlayer2D = [&](const std::string& name) -> ECS::Entity {
        ECS::Entity player = m_World->CreateEntity();
        m_World->AddComponent<ECS::NameComponent>(player, name);
        auto& pt = m_World->AddComponent<ECS::TransformComponent>(player);
        pt.position = Math::Vector3(0.0f, 1.0f, 0.0f);
        auto& pmat = m_World->AddComponent<ECS::MaterialComponent>(player);
        pmat.baseColor = Math::Vector3(0.2f, 0.4f, 0.9f);
        m_World->AddComponent<ECS::MeshComponent>(player, Renderer::MeshFactory::CreateCapsule2D(0.8f, 1.6f));
        m_World->AddComponent<ECS::Sprite2DComponent>(player);
        return player;
    };

    createLight();

    if (templateId == "platformer") {
        // 2D ground: thin wide quad in XY plane
        {
            ECS::Entity ground = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(ground, "Ground");
            auto& gt = m_World->AddComponent<ECS::TransformComponent>(ground);
            gt.position = Math::Vector3(0.0f, -1.0f, 0.0f);
            gt.scale = Math::Vector3(20.0f, 1.0f, 1.0f);
            auto& gmat = m_World->AddComponent<ECS::MaterialComponent>(ground);
            gmat.baseColor = Math::Vector3(0.35f, 0.55f, 0.3f);
            gmat.roughness = 0.9f;
            m_World->AddComponent<ECS::MeshComponent>(ground, Renderer::MeshFactory::CreateQuad(1.0f, 1.0f));
            auto& col = m_World->AddComponent<ECS::BoxColliderComponent>(ground);
            col.size = Math::Vector3(20.0f, 1.0f, 1.0f);
        }
        ECS::Entity player = createPlayer2D("Player");
        auto& ctrl = m_World->AddComponent<ECS::Platformer2DController>(player);
        ctrl.moveSpeed = 5.0f;
        ctrl.jumpForce = 10.0f;
        SetupCameraForController(player, "Platformer2D");

    } else if (templateId == "topdown2d") {
        // 2D ground: large flat quad in XY plane
        {
            ECS::Entity ground = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(ground, "Ground");
            auto& gt = m_World->AddComponent<ECS::TransformComponent>(ground);
            gt.position = Math::Vector3(0.0f, 0.0f, -0.1f);
            gt.scale = Math::Vector3(30.0f, 30.0f, 1.0f);
            auto& gmat = m_World->AddComponent<ECS::MaterialComponent>(ground);
            gmat.baseColor = Math::Vector3(0.35f, 0.55f, 0.3f);
            gmat.roughness = 0.9f;
            m_World->AddComponent<ECS::MeshComponent>(ground, Renderer::MeshFactory::CreateQuad(1.0f, 1.0f));
            auto& col = m_World->AddComponent<ECS::BoxColliderComponent>(ground);
            col.size = Math::Vector3(30.0f, 30.0f, 0.1f);
        }
        ECS::Entity player = createPlayer2D("Player");
        auto& ctrl = m_World->AddComponent<ECS::TopDown2DController>(player);
        ctrl.moveSpeed = 5.0f;
        SetupCameraForController(player, "TopDown2D");

    } else if (templateId == "isometric") {
        createGround();
        ECS::Entity player = createPlayer3D("Player");
        auto& ctrl = m_World->AddComponent<ECS::TopDown3DController>(player);
        ctrl.moveSpeed = 5.0f;
        ctrl.cameraAngle = 45.0f;
        ctrl.cameraDistance = 15.0f;
        SetupCameraForController(player, "TopDown3D");

    } else if (templateId == "thirdperson") {
        createGround();
        ECS::Entity player = createPlayer3D("Player");
        auto& ctrl = m_World->AddComponent<ECS::ThirdPersonController>(player);
        ctrl.moveSpeed = 5.0f;
        ctrl.cameraDistance = 5.0f;
        ctrl.cameraHeight = 2.0f;
        SetupCameraForController(player, "ThirdPerson");

    } else if (templateId == "firstperson") {
        createGround();
        ECS::Entity player = createPlayer3D("Player");
        auto* pt = m_World->GetComponent<ECS::TransformComponent>(player);
        if (pt) pt->position.y = 1.7f;  // Eye height
        auto& ctrl = m_World->AddComponent<ECS::FirstPersonController>(player);
        ctrl.moveSpeed = 5.0f;
        ctrl.mouseSensitivity = 0.15f;
        SetupCameraForController(player, "FirstPerson");

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

        // Directional Light (even illumination)
        ECS::Entity vnLight = m_World->CreateEntity();
        m_World->AddComponent<ECS::NameComponent>(vnLight, "Light");
        auto& vnLT = m_World->AddComponent<ECS::TransformComponent>(vnLight);
        vnLT.position = Math::Vector3(0.0f, 10.0f, 5.0f);
        auto& vnLC = m_World->AddComponent<ECS::LightComponent>(vnLight);
        vnLC.type = ECS::LightType::Directional;
        vnLC.intensity = 1.2f;
        vnLC.color = Math::Vector3(1.0f, 1.0f, 1.0f);
    }

    else if (templateId == "rpg_village") {
        // Ground
        {
            ECS::Entity ground = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(ground, "Ground");
            auto& gt = m_World->AddComponent<ECS::TransformComponent>(ground);
            gt.scale = Math::Vector3(30.0f, 1.0f, 30.0f);
            gt.position = Math::Vector3(0.0f, -0.5f, 0.0f);
            auto& gmat = m_World->AddComponent<ECS::MaterialComponent>(ground);
            gmat.baseColor = Math::Vector3(0.3f, 0.5f, 0.2f);
            gmat.roughness = 0.9f;
            m_World->AddComponent<ECS::MeshComponent>(ground, Renderer::MeshFactory::CreateCube(1.0f));
            auto& col = m_World->AddComponent<ECS::BoxColliderComponent>(ground);
            col.size = Math::Vector3(30.0f, 1.0f, 30.0f);
        }
        // Sun
        {
            ECS::Entity sun = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(sun, "Sun");
            auto& lt = m_World->AddComponent<ECS::TransformComponent>(sun);
            lt.position = Math::Vector3(0.0f, 10.0f, 0.0f);
            auto& lc = m_World->AddComponent<ECS::LightComponent>(sun);
            lc.type = ECS::LightType::Directional;
            lc.intensity = 1.5f;
            lc.castShadows = true;
        }
        // Player
        {
            ECS::Entity player = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(player, "Player");
            auto& pt = m_World->AddComponent<ECS::TransformComponent>(player);
            pt.position = Math::Vector3(0.0f, 0.5f, 0.0f);
            m_World->AddComponent<ECS::MeshComponent>(player, Renderer::MeshFactory::CreateCapsule(0.3f, 1.0f));
            auto& pm = m_World->AddComponent<ECS::MaterialComponent>(player);
            pm.baseColor = Math::Vector3(0.3f, 0.4f, 0.7f);
            auto& health = m_World->AddComponent<ECS::HealthComponent>(player);
            health.maxHealth = 100.0f;
            health.currentHealth = 100.0f;
            m_World->AddComponent<ECS::InventoryComponent>(player);
            auto& ctrl = m_World->AddComponent<ECS::ThirdPersonController>(player);
            ctrl.moveSpeed = 5.0f;
            SetupCameraForController(player, "ThirdPerson");
        }
        // NPC
        {
            ECS::Entity npc = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(npc, "Village NPC");
            auto& nt = m_World->AddComponent<ECS::TransformComponent>(npc);
            nt.position = Math::Vector3(4.0f, 0.5f, 3.0f);
            m_World->AddComponent<ECS::MeshComponent>(npc, Renderer::MeshFactory::CreateCapsule(0.3f, 1.0f));
            auto& nm = m_World->AddComponent<ECS::MaterialComponent>(npc);
            nm.baseColor = Math::Vector3(0.7f, 0.5f, 0.3f);
            auto& interact = m_World->AddComponent<ECS::InteractableComponent>(npc);
            interact.interactionRange = 3.0f;
            interact.promptText = "Talk";
            auto& dialogue = m_World->AddComponent<ECS::DialogueComponent>(npc);
            dialogue.speakerName = "Villager";
            dialogue.dialogueLines.push_back("Welcome to our village!");
            dialogue.dialogueLines.push_back("Watch out for enemies in the forest.");
        }
        // Health Pickup
        {
            ECS::Entity pickup = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(pickup, "Health Potion");
            auto& pt = m_World->AddComponent<ECS::TransformComponent>(pickup);
            pt.position = Math::Vector3(-3.0f, 0.3f, 2.0f);
            pt.scale = Math::Vector3(0.3f);
            m_World->AddComponent<ECS::MeshComponent>(pickup, Renderer::MeshFactory::CreateSphere(0.5f));
            auto& pm = m_World->AddComponent<ECS::MaterialComponent>(pickup);
            pm.baseColor = Math::Vector3(0.2f, 0.8f, 0.2f);
            pm.emissiveColor = Math::Vector3(0.1f, 0.4f, 0.1f);
            pm.emissiveStrength = 0.5f;
            auto& pc = m_World->AddComponent<ECS::PickupComponent>(pickup);
            pc.type = ECS::PickupComponent::PickupType::Health;
            pc.value = 25.0f;
        }
        // Patrol Enemy
        {
            ECS::Entity enemy = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(enemy, "Forest Enemy");
            auto& et = m_World->AddComponent<ECS::TransformComponent>(enemy);
            et.position = Math::Vector3(8.0f, 0.5f, 8.0f);
            m_World->AddComponent<ECS::MeshComponent>(enemy, Renderer::MeshFactory::CreateCapsule(0.3f, 1.0f));
            auto& em = m_World->AddComponent<ECS::MaterialComponent>(enemy);
            em.baseColor = Math::Vector3(0.8f, 0.15f, 0.1f);
            auto& ai = m_World->AddComponent<ECS::AIControllerComponent>(enemy);
            ai.currentState = ECS::AIControllerComponent::AIState::Patrol;
            ai.moveSpeed = 2.0f;
            auto& eh = m_World->AddComponent<ECS::HealthComponent>(enemy);
            eh.maxHealth = 50.0f;
            eh.currentHealth = 50.0f;
            auto& dmg = m_World->AddComponent<ECS::DamageComponent>(enemy);
            dmg.damage = 10.0f;
        }
    } else if (templateId == "survival") {
        // Ground
        {
            ECS::Entity ground = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(ground, "Ground");
            auto& gt = m_World->AddComponent<ECS::TransformComponent>(ground);
            gt.scale = Math::Vector3(40.0f, 1.0f, 40.0f);
            gt.position = Math::Vector3(0.0f, -0.5f, 0.0f);
            auto& gmat = m_World->AddComponent<ECS::MaterialComponent>(ground);
            gmat.baseColor = Math::Vector3(0.4f, 0.35f, 0.25f);
            gmat.roughness = 0.95f;
            m_World->AddComponent<ECS::MeshComponent>(ground, Renderer::MeshFactory::CreateCube(1.0f));
            auto& col = m_World->AddComponent<ECS::BoxColliderComponent>(ground);
            col.size = Math::Vector3(40.0f, 1.0f, 40.0f);
        }
        // Sun
        {
            ECS::Entity sun = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(sun, "Sun");
            auto& lt = m_World->AddComponent<ECS::TransformComponent>(sun);
            lt.position = Math::Vector3(0.0f, 10.0f, 0.0f);
            auto& lc = m_World->AddComponent<ECS::LightComponent>(sun);
            lc.type = ECS::LightType::Directional;
            lc.intensity = 1.5f;
            lc.castShadows = true;
        }
        // Player
        {
            ECS::Entity player = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(player, "Player");
            auto& pt = m_World->AddComponent<ECS::TransformComponent>(player);
            pt.position = Math::Vector3(0.0f, 0.5f, 0.0f);
            m_World->AddComponent<ECS::MeshComponent>(player, Renderer::MeshFactory::CreateCapsule(0.3f, 1.0f));
            auto& pm = m_World->AddComponent<ECS::MaterialComponent>(player);
            pm.baseColor = Math::Vector3(0.5f, 0.4f, 0.3f);
            auto& health = m_World->AddComponent<ECS::HealthComponent>(player);
            health.maxHealth = 100.0f;
            health.currentHealth = 100.0f;
            auto& ctrl = m_World->AddComponent<ECS::ThirdPersonController>(player);
            ctrl.moveSpeed = 4.0f;
            SetupCameraForController(player, "ThirdPerson");
        }
        // Hot zone
        {
            ECS::Entity hot = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(hot, "Desert Heat");
            auto& ht = m_World->AddComponent<ECS::TransformComponent>(hot);
            ht.position = Math::Vector3(10.0f, 0.0f, 0.0f);
            auto& tz = m_World->AddComponent<ECS::TemperatureZoneComponent>(hot);
            tz.temperature = 45.0f;
            tz.halfExtents = Math::Vector3(8.0f, 5.0f, 8.0f);
        }
        // Cold zone
        {
            ECS::Entity cold = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(cold, "Frozen Tundra");
            auto& ct = m_World->AddComponent<ECS::TransformComponent>(cold);
            ct.position = Math::Vector3(-10.0f, 0.0f, 0.0f);
            auto& tz = m_World->AddComponent<ECS::TemperatureZoneComponent>(cold);
            tz.temperature = -15.0f;
            tz.halfExtents = Math::Vector3(8.0f, 5.0f, 8.0f);
        }
        // Damage hazard
        {
            ECS::Entity hazard = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(hazard, "Lava Pool");
            auto& ht = m_World->AddComponent<ECS::TransformComponent>(hazard);
            ht.position = Math::Vector3(5.0f, 0.05f, -5.0f);
            m_World->AddComponent<ECS::MeshComponent>(hazard, Renderer::MeshFactory::CreatePlane(4.0f, 4.0f));
            auto& hm = m_World->AddComponent<ECS::MaterialComponent>(hazard);
            hm.baseColor = Math::Vector3(0.9f, 0.3f, 0.0f);
            hm.emissiveColor = Math::Vector3(0.9f, 0.3f, 0.0f);
            hm.emissiveStrength = 1.0f;
            auto& trigger = m_World->AddComponent<ECS::TriggerZoneComponent>(hazard);
            trigger.shape = ECS::TriggerZoneComponent::Shape::Box;
            trigger.boxSize = Math::Vector3(2.0f, 0.5f, 2.0f);
            auto& dmg = m_World->AddComponent<ECS::DamageComponent>(hazard);
            dmg.damage = 15.0f;
            dmg.damageInterval = 0.5f;
        }
        // Resource pickup
        {
            ECS::Entity resource = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(resource, "Supply Crate");
            auto& rt = m_World->AddComponent<ECS::TransformComponent>(resource);
            rt.position = Math::Vector3(-4.0f, 0.3f, 4.0f);
            rt.scale = Math::Vector3(0.5f, 0.5f, 0.5f);
            m_World->AddComponent<ECS::MeshComponent>(resource, Renderer::MeshFactory::CreateCube(1.0f));
            auto& rm = m_World->AddComponent<ECS::MaterialComponent>(resource);
            rm.baseColor = Math::Vector3(0.5f, 0.4f, 0.2f);
            auto& pc = m_World->AddComponent<ECS::PickupComponent>(resource);
            pc.type = ECS::PickupComponent::PickupType::Ammo;
            pc.value = 10.0f;
        }
    }

    // ===== Game Manager Template =====
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
    }

    // ===== 3D Narrative Sequencing Template =====
    else if (templateId == "narrative") {
        createGround();

        // Narrative Camera (third person following protagonist)
        ECS::Entity player = createPlayer3D("Protagonist");
        {
            auto& pmat = *m_World->GetComponent<ECS::MaterialComponent>(player);
            pmat.baseColor = Math::Vector3(0.3f, 0.5f, 0.8f);
            auto& ctrl = m_World->AddComponent<ECS::ThirdPersonController>(player);
            ctrl.moveSpeed = 3.5f;
            ctrl.cameraDistance = 4.0f;
            ctrl.cameraHeight = 2.5f;
            SetupCameraForController(player, "ThirdPerson");
        }

        // NPC 1: Quest Giver (dialogue changes after quest completion)
        {
            ECS::Entity npc1 = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(npc1, "Elder Mara");
            auto& nt = m_World->AddComponent<ECS::TransformComponent>(npc1);
            nt.position = Math::Vector3(3.0f, 0.5f, 2.0f);
            m_World->AddComponent<ECS::MeshComponent>(npc1, Renderer::MeshFactory::CreateCapsule(0.3f, 1.0f));
            auto& nm = m_World->AddComponent<ECS::MaterialComponent>(npc1);
            nm.baseColor = Math::Vector3(0.6f, 0.4f, 0.6f);

            auto& interact = m_World->AddComponent<ECS::InteractableComponent>(npc1);
            interact.interactionRange = 3.0f;
            interact.promptText = "Talk to Elder Mara";

            auto& dialogue = m_World->AddComponent<ECS::DialogueComponent>(npc1);
            dialogue.speakerName = "Elder Mara";
            dialogue.dialogueLines.push_back("Welcome, traveler. Our village needs your help.");
            dialogue.dialogueLines.push_back("The forest spirits have grown restless.");
            dialogue.dialogueLines.push_back("Speak to the Guard and the Merchant for supplies.");
            dialogue.dialogueLines.push_back("[After quest] You've done well. The spirits are at peace.");
            dialogue.dialogueLines.push_back("[After quest] Remember, the world changes with your actions.");

            auto& sm = m_World->AddComponent<ECS::StateMachineComponent>(npc1);
            sm.currentState = "QuestAvailable";

            auto& notes = m_World->AddComponent<ECS::NotesComponent>(npc1);
            notes.notes = "NARRATIVE SEQUENCING\n"
                "========================\n"
                "This NPC uses StateMachineComponent to track quest state.\n"
                "DialogueComponent lines change based on current state:\n"
                "  QuestAvailable -> lines 0-2\n"
                "  QuestActive    -> line 2 only (reminder)\n"
                "  QuestComplete  -> lines 3-4\n"
                "\n"
                "To implement branching:\n"
                "  1. Check StateMachine.currentState before showing dialogue\n"
                "  2. Use InteractableComponent.promptText to change prompts\n"
                "  3. Transition states on events (item pickup, zone enter, etc)\n";
        }

        // NPC 2: Guard (gives different info based on quest state)
        {
            ECS::Entity npc2 = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(npc2, "Guard Renn");
            auto& nt = m_World->AddComponent<ECS::TransformComponent>(npc2);
            nt.position = Math::Vector3(-4.0f, 0.5f, 5.0f);
            m_World->AddComponent<ECS::MeshComponent>(npc2, Renderer::MeshFactory::CreateCapsule(0.35f, 1.1f));
            auto& nm = m_World->AddComponent<ECS::MaterialComponent>(npc2);
            nm.baseColor = Math::Vector3(0.4f, 0.4f, 0.5f);

            auto& interact = m_World->AddComponent<ECS::InteractableComponent>(npc2);
            interact.interactionRange = 3.0f;
            interact.promptText = "Talk to Guard";

            auto& dialogue = m_World->AddComponent<ECS::DialogueComponent>(npc2);
            dialogue.speakerName = "Guard Renn";
            dialogue.dialogueLines.push_back("Halt! The forest path is dangerous.");
            dialogue.dialogueLines.push_back("If Elder Mara sent you, take this torch.");
            dialogue.dialogueLines.push_back("[Has torch] Be careful out there.");

            auto& sm = m_World->AddComponent<ECS::StateMachineComponent>(npc2);
            sm.currentState = "Idle";
        }

        // NPC 3: Merchant (inventory-based conversation)
        {
            ECS::Entity npc3 = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(npc3, "Merchant Dalia");
            auto& nt = m_World->AddComponent<ECS::TransformComponent>(npc3);
            nt.position = Math::Vector3(5.0f, 0.5f, -3.0f);
            m_World->AddComponent<ECS::MeshComponent>(npc3, Renderer::MeshFactory::CreateCapsule(0.3f, 1.0f));
            auto& nm = m_World->AddComponent<ECS::MaterialComponent>(npc3);
            nm.baseColor = Math::Vector3(0.7f, 0.6f, 0.3f);

            auto& interact = m_World->AddComponent<ECS::InteractableComponent>(npc3);
            interact.interactionRange = 3.0f;
            interact.promptText = "Talk to Merchant";

            auto& dialogue = m_World->AddComponent<ECS::DialogueComponent>(npc3);
            dialogue.speakerName = "Merchant Dalia";
            dialogue.dialogueLines.push_back("Looking to buy? I have supplies for adventurers.");
            dialogue.dialogueLines.push_back("Hmm, you seem prepared already. Good luck!");
            dialogue.dialogueLines.push_back("[After quest] Word travels fast. Free potions for the hero!");
        }

        // Quest Item Pickup
        {
            ECS::Entity item = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(item, "Spirit Gem");
            auto& it = m_World->AddComponent<ECS::TransformComponent>(item);
            it.position = Math::Vector3(0.0f, 0.5f, 10.0f);
            it.scale = Math::Vector3(0.4f);
            m_World->AddComponent<ECS::MeshComponent>(item, Renderer::MeshFactory::CreateSphere(0.5f));
            auto& im = m_World->AddComponent<ECS::MaterialComponent>(item);
            im.baseColor = Math::Vector3(0.3f, 0.9f, 0.7f);
            im.emissiveColor = Math::Vector3(0.2f, 0.6f, 0.5f);
            im.emissiveStrength = 0.8f;
            auto& pc = m_World->AddComponent<ECS::PickupComponent>(item);
            pc.type = ECS::PickupComponent::PickupType::Key;
            pc.value = 1.0f;

            auto& notes = m_World->AddComponent<ECS::NotesComponent>(item);
            notes.notes = "Quest trigger: picking this up should transition\n"
                "Elder Mara's state to QuestComplete\n"
                "and Guard Renn's state to GaveItem.";
        }
    }

    // ===== 4-Player Splitscreen Racing Template =====
    else if (templateId == "racing") {
        // Race Track Ground
        {
            ECS::Entity track = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(track, "Race Track");
            auto& tt = m_World->AddComponent<ECS::TransformComponent>(track);
            tt.position = Math::Vector3(0.0f, -0.05f, 0.0f);
            tt.scale = Math::Vector3(80.0f, 0.1f, 80.0f);
            auto& tm = m_World->AddComponent<ECS::MaterialComponent>(track);
            tm.baseColor = Math::Vector3(0.25f, 0.25f, 0.28f);
            tm.roughness = 0.6f;
            m_World->AddComponent<ECS::MeshComponent>(track, Renderer::MeshFactory::CreateCube(1.0f));
            auto& col = m_World->AddComponent<ECS::BoxColliderComponent>(track);
            col.size = Math::Vector3(80.0f, 0.1f, 80.0f);
        }

        // Sun
        {
            ECS::Entity sun = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(sun, "Sun");
            auto& lt = m_World->AddComponent<ECS::TransformComponent>(sun);
            lt.position = Math::Vector3(0.0f, 20.0f, 0.0f);
            auto& lc = m_World->AddComponent<ECS::LightComponent>(sun);
            lc.type = ECS::LightType::Directional;
            lc.intensity = 1.2f;
            lc.castShadows = true;
        }

        // Vehicle colors
        Math::Vector3 vehicleColors[4] = {
            Math::Vector3(0.9f, 0.1f, 0.1f),  // Red
            Math::Vector3(0.1f, 0.5f, 0.9f),  // Blue
            Math::Vector3(0.1f, 0.8f, 0.2f),  // Green
            Math::Vector3(0.9f, 0.8f, 0.1f),  // Yellow
        };
        Math::Vector3 startPositions[4] = {
            Math::Vector3(-3.0f, 0.3f, -5.0f),
            Math::Vector3(-1.0f, 0.3f, -5.0f),
            Math::Vector3(1.0f, 0.3f, -5.0f),
            Math::Vector3(3.0f, 0.3f, -5.0f),
        };
        const char* playerNames[4] = { "Player 1", "Player 2", "Player 3", "Player 4" };

        // Create 4 vehicles (cube placeholders for karts)
        for (int i = 0; i < 4; ++i) {
            ECS::Entity vehicle = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(vehicle, playerNames[i]);
            auto& vt = m_World->AddComponent<ECS::TransformComponent>(vehicle);
            vt.position = startPositions[i];
            vt.scale = Math::Vector3(0.8f, 0.4f, 1.2f);
            m_World->AddComponent<ECS::MeshComponent>(vehicle, Renderer::MeshFactory::CreateCube(1.0f));
            auto& vm = m_World->AddComponent<ECS::MaterialComponent>(vehicle);
            vm.baseColor = vehicleColors[i];
            vm.roughness = 0.3f;

            auto& rb = m_World->AddComponent<ECS::RigidbodyComponent>(vehicle);
            rb.mass = 1.0f;
            rb.drag = 0.5f;

            auto& ctrl = m_World->AddComponent<ECS::TopDown3DController>(vehicle);
            ctrl.moveSpeed = 15.0f;
            ctrl.gamepadIndex = i;

            // Each player gets their own camera with a quadrant viewport
            ECS::Entity cam = m_World->CreateEntity();
            char camName[32];
            snprintf(camName, sizeof(camName), "Camera P%d", i + 1);
            m_World->AddComponent<ECS::NameComponent>(cam, camName);
            auto& camT = m_World->AddComponent<ECS::TransformComponent>(cam);
            camT.position = startPositions[i] + Math::Vector3(0.0f, 8.0f, -6.0f);
            // Point camera down toward the vehicle: direction = (0, -8, 6), pitch ≈ -53°
            {
                Math::Vector3 toVehicle = Math::Vector3(0.0f, -8.0f, 6.0f).Normalized();
                // Camera forward is (0,0,-1). Compute rotation from forward to toVehicle.
                // For a simple pitch rotation around X: angle = acos(dot(forward, dir))
                // but we need the signed angle. Use atan2 for pitch.
                f32 pitch = std::atan2(-toVehicle.y, -toVehicle.z); // angle from -Z toward -Y
                camT.rotation = Math::Quaternion::FromEuler(Math::Vector3(pitch, 0.0f, 0.0f));
            }
            auto& camC = m_World->AddComponent<ECS::CameraComponent>(cam);
            camC.fieldOfView = 60.0f;
            camC.nearPlane = 0.1f;
            camC.farPlane = 500.0f;
            // Splitscreen quadrants: TL, TR, BL, BR
            camC.viewportX = (i % 2) * 0.5f;
            camC.viewportY = (i / 2) * 0.5f;
            camC.viewportWidth = 0.5f;
            camC.viewportHeight = 0.5f;

            auto& follow = m_World->AddComponent<ECS::FollowTargetComponent>(cam);
            follow.target = vehicle;
            follow.offset = Math::Vector3(0.0f, 8.0f, -6.0f);
            follow.moveSpeed = 5.0f;
            auto& lookAt = m_World->AddComponent<ECS::LookAtTargetComponent>(cam);
            lookAt.target = vehicle;

            if (i == 0) m_SelectedGameCamera = cam;
        }

        // Start/Finish Line
        {
            ECS::Entity startLine = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(startLine, "Start/Finish Line");
            auto& st = m_World->AddComponent<ECS::TransformComponent>(startLine);
            st.position = Math::Vector3(0.0f, 0.01f, -5.0f);
            st.scale = Math::Vector3(10.0f, 0.01f, 0.5f);
            m_World->AddComponent<ECS::MeshComponent>(startLine, Renderer::MeshFactory::CreateCube(1.0f));
            auto& sm = m_World->AddComponent<ECS::MaterialComponent>(startLine);
            sm.baseColor = Math::Vector3(1.0f, 1.0f, 1.0f);
            auto& trigger = m_World->AddComponent<ECS::TriggerZoneComponent>(startLine);
            trigger.shape = ECS::TriggerZoneComponent::Shape::Box;
            trigger.boxSize = Math::Vector3(10.0f, 2.0f, 0.5f);
        }

        // Track Waypoints (oval course)
        Math::Vector3 waypointPositions[] = {
            Math::Vector3(0.0f, 0.3f, -5.0f),
            Math::Vector3(20.0f, 0.3f, 0.0f),
            Math::Vector3(25.0f, 0.3f, 15.0f),
            Math::Vector3(15.0f, 0.3f, 25.0f),
            Math::Vector3(0.0f, 0.3f, 30.0f),
            Math::Vector3(-15.0f, 0.3f, 25.0f),
            Math::Vector3(-25.0f, 0.3f, 15.0f),
            Math::Vector3(-20.0f, 0.3f, 0.0f),
        };
        for (int i = 0; i < 8; ++i) {
            ECS::Entity wp = m_World->CreateEntity();
            char wpName[32];
            snprintf(wpName, sizeof(wpName), "Waypoint %d", i + 1);
            m_World->AddComponent<ECS::NameComponent>(wp, wpName);
            auto& wt = m_World->AddComponent<ECS::TransformComponent>(wp);
            wt.position = waypointPositions[i];
            wt.scale = Math::Vector3(0.5f);
            m_World->AddComponent<ECS::MeshComponent>(wp, Renderer::MeshFactory::CreateSphere(0.5f));
            auto& wm = m_World->AddComponent<ECS::MaterialComponent>(wp);
            wm.baseColor = Math::Vector3(1.0f, 0.5f, 0.0f);
            wm.emissiveColor = Math::Vector3(1.0f, 0.5f, 0.0f);
            wm.emissiveStrength = 0.3f;
            auto& waypoint = m_World->AddComponent<ECS::WaypointComponent>(wp);
            waypoint.index = i;
        }

        // Track Instructions
        {
            ECS::Entity notes = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(notes, "Racing Setup Notes");
            auto& nt = m_World->AddComponent<ECS::TransformComponent>(notes);
            nt.position = Math::Vector3(0.0f, 0.0f, 0.0f);
            auto& nc = m_World->AddComponent<ECS::NotesComponent>(notes);
            nc.notes = "4-PLAYER SPLITSCREEN RACING\n"
                "==============================\n"
                "Each Player entity has a TopDown3DController with gamepadIndex 0-3.\n"
                "Each player has a dedicated Camera with FollowTarget + LookAtTarget.\n"
                "\n"
                "To implement splitscreen:\n"
                "  1. Render each camera to a quarter of the screen\n"
                "  2. Use viewport scissors: TL, TR, BL, BR\n"
                "  3. Waypoints define the track path (oval circuit)\n"
                "  4. TriggerZone at Start/Finish detects lap completion\n"
                "\n"
                "To add track barriers, create Box entities with colliders.\n";
        }
    }

    // ===== Arena Fighter Template (Smash Bros-style) =====
    else if (templateId == "arena") {
        // Arena Platform (main stage)
        {
            ECS::Entity stage = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(stage, "Main Stage");
            auto& st = m_World->AddComponent<ECS::TransformComponent>(stage);
            st.position = Math::Vector3(0.0f, 0.0f, 0.0f);
            st.scale = Math::Vector3(20.0f, 0.5f, 5.0f);
            m_World->AddComponent<ECS::MeshComponent>(stage, Renderer::MeshFactory::CreateCube(1.0f));
            auto& sm = m_World->AddComponent<ECS::MaterialComponent>(stage);
            sm.baseColor = Math::Vector3(0.3f, 0.3f, 0.35f);
            sm.roughness = 0.7f;
            auto& col = m_World->AddComponent<ECS::BoxColliderComponent>(stage);
            col.size = Math::Vector3(20.0f, 0.5f, 5.0f);
        }

        // Left Floating Platform
        {
            ECS::Entity plat = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(plat, "Left Platform");
            auto& pt = m_World->AddComponent<ECS::TransformComponent>(plat);
            pt.position = Math::Vector3(-6.0f, 3.0f, 0.0f);
            pt.scale = Math::Vector3(4.0f, 0.3f, 4.0f);
            m_World->AddComponent<ECS::MeshComponent>(plat, Renderer::MeshFactory::CreateCube(1.0f));
            auto& pm = m_World->AddComponent<ECS::MaterialComponent>(plat);
            pm.baseColor = Math::Vector3(0.4f, 0.35f, 0.3f);
            auto& col = m_World->AddComponent<ECS::BoxColliderComponent>(plat);
            col.size = Math::Vector3(4.0f, 0.3f, 4.0f);
        }

        // Right Floating Platform
        {
            ECS::Entity plat = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(plat, "Right Platform");
            auto& pt = m_World->AddComponent<ECS::TransformComponent>(plat);
            pt.position = Math::Vector3(6.0f, 3.0f, 0.0f);
            pt.scale = Math::Vector3(4.0f, 0.3f, 4.0f);
            m_World->AddComponent<ECS::MeshComponent>(plat, Renderer::MeshFactory::CreateCube(1.0f));
            auto& pm = m_World->AddComponent<ECS::MaterialComponent>(plat);
            pm.baseColor = Math::Vector3(0.4f, 0.35f, 0.3f);
            auto& col = m_World->AddComponent<ECS::BoxColliderComponent>(plat);
            col.size = Math::Vector3(4.0f, 0.3f, 4.0f);
        }

        // Top Platform
        {
            ECS::Entity plat = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(plat, "Top Platform");
            auto& pt = m_World->AddComponent<ECS::TransformComponent>(plat);
            pt.position = Math::Vector3(0.0f, 5.5f, 0.0f);
            pt.scale = Math::Vector3(5.0f, 0.3f, 4.0f);
            m_World->AddComponent<ECS::MeshComponent>(plat, Renderer::MeshFactory::CreateCube(1.0f));
            auto& pm = m_World->AddComponent<ECS::MaterialComponent>(plat);
            pm.baseColor = Math::Vector3(0.4f, 0.35f, 0.3f);
            auto& col = m_World->AddComponent<ECS::BoxColliderComponent>(plat);
            col.size = Math::Vector3(5.0f, 0.3f, 4.0f);
        }

        // Arena Light
        {
            ECS::Entity sun = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(sun, "Arena Light");
            auto& lt = m_World->AddComponent<ECS::TransformComponent>(sun);
            lt.position = Math::Vector3(0.0f, 15.0f, 10.0f);
            lt.rotation = Math::Quaternion(Math::Vector3(1, 0, 0), Math::Radians(-30.0f));
            auto& lc = m_World->AddComponent<ECS::LightComponent>(sun);
            lc.type = ECS::LightType::Directional;
            lc.intensity = 1.3f;
            lc.castShadows = true;
        }

        // Fighter colors
        Math::Vector3 fighterColors[] = {
            Math::Vector3(0.9f, 0.15f, 0.15f), // Red
            Math::Vector3(0.15f, 0.4f, 0.9f),  // Blue
            Math::Vector3(0.15f, 0.8f, 0.2f),  // Green
            Math::Vector3(0.9f, 0.85f, 0.1f),  // Yellow
            Math::Vector3(0.8f, 0.3f, 0.8f),   // Purple
            Math::Vector3(0.1f, 0.8f, 0.8f),   // Cyan
            Math::Vector3(0.9f, 0.5f, 0.1f),   // Orange
            Math::Vector3(0.6f, 0.6f, 0.6f),   // Gray
        };

        // Create 4 player fighters + 4 NPC fighters
        for (int i = 0; i < 8; ++i) {
            ECS::Entity fighter = m_World->CreateEntity();
            char name[32];
            if (i < 4)
                snprintf(name, sizeof(name), "Fighter P%d", i + 1);
            else
                snprintf(name, sizeof(name), "Fighter NPC%d", i - 3);
            m_World->AddComponent<ECS::NameComponent>(fighter, name);

            auto& ft = m_World->AddComponent<ECS::TransformComponent>(fighter);
            f32 spread = (i - 3.5f) * 2.0f;
            ft.position = Math::Vector3(spread, 1.5f, 0.0f);

            m_World->AddComponent<ECS::MeshComponent>(fighter, Renderer::MeshFactory::CreateCapsule(0.3f, 0.9f));
            auto& fm = m_World->AddComponent<ECS::MaterialComponent>(fighter);
            fm.baseColor = fighterColors[i];

            auto& health = m_World->AddComponent<ECS::HealthComponent>(fighter);
            health.maxHealth = 100.0f;
            health.currentHealth = 100.0f;

            auto& dmg = m_World->AddComponent<ECS::DamageComponent>(fighter);
            dmg.damage = 10.0f;

            if (i < 4) {
                // Player-controlled fighters
                auto& ctrl = m_World->AddComponent<ECS::Platformer2DController>(fighter);
                ctrl.moveSpeed = 8.0f;
                ctrl.jumpForce = 14.0f;
                ctrl.gamepadIndex = i;
            } else {
                // NPC fighters
                auto& ai = m_World->AddComponent<ECS::AIControllerComponent>(fighter);
                ai.currentState = ECS::AIControllerComponent::AIState::Patrol;
                ai.moveSpeed = 5.0f;
            }
        }

        // Dynamic Arena Camera (orthographic side view)
        {
            ECS::Entity cam = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(cam, "Arena Camera");
            auto& camT = m_World->AddComponent<ECS::TransformComponent>(cam);
            camT.position = Math::Vector3(0.0f, 3.0f, 25.0f);
            auto& camC = m_World->AddComponent<ECS::CameraComponent>(cam);
            camC.projectionType = ECS::ProjectionType::Orthographic;
            camC.orthoSize = 14.0f;  // Wide enough for all platforms + fighters
            camC.nearPlane = 0.1f;
            camC.farPlane = 100.0f;
            m_SelectedGameCamera = cam;

            auto& notes = m_World->AddComponent<ECS::NotesComponent>(cam);
            notes.notes = "ARENA CAMERA LOGIC\n"
                "========================\n"
                "This camera should dynamically adjust orthoSize\n"
                "to keep all ALIVE players visible.\n"
                "\n"
                "Algorithm:\n"
                "  1. Find bounding box of all alive fighters\n"
                "  2. Add padding (2-3 units each side)\n"
                "  3. Set camera X to center of bounding box\n"
                "  4. Set orthoSize to max(width/aspect, height) / 2\n"
                "  5. Clamp minimum orthoSize to 8 (prevent zoom-in)\n"
                "  6. Smoothly lerp to new values (don't snap)\n"
                "\n"
                "Kill plane: Y < -10 (off-screen death)\n"
                "Respawn: Random platform after 3 second delay\n";
        }

        // Kill Zone (below stage)
        {
            ECS::Entity killZone = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(killZone, "Kill Zone");
            auto& kzt = m_World->AddComponent<ECS::TransformComponent>(killZone);
            kzt.position = Math::Vector3(0.0f, -15.0f, 0.0f);
            auto& trigger = m_World->AddComponent<ECS::TriggerZoneComponent>(killZone);
            trigger.shape = ECS::TriggerZoneComponent::Shape::Box;
            trigger.boxSize = Math::Vector3(50.0f, 2.0f, 20.0f);
            auto& dmg = m_World->AddComponent<ECS::DamageComponent>(killZone);
            dmg.damage = 9999.0f;
        }
    }

    // ===== PS1-Style 3D RPG with Turn-Based Battles =====
    else if (templateId == "ps1rpg") {
        // Overworld Ground
        {
            ECS::Entity ground = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(ground, "Overworld Ground");
            auto& gt = m_World->AddComponent<ECS::TransformComponent>(ground);
            gt.scale = Math::Vector3(40.0f, 1.0f, 40.0f);
            gt.position = Math::Vector3(0.0f, -0.5f, 0.0f);
            auto& gmat = m_World->AddComponent<ECS::MaterialComponent>(ground);
            gmat.baseColor = Math::Vector3(0.35f, 0.55f, 0.25f);
            gmat.roughness = 0.95f;
            // PS1-style: enable vertex snapping and flat shading
            gmat.flatShading = true;
            gmat.vertexSnapping = true;
            gmat.vertexSnapResolution = 160;
            m_World->AddComponent<ECS::MeshComponent>(ground, Renderer::MeshFactory::CreateCube(1.0f));
            auto& col = m_World->AddComponent<ECS::BoxColliderComponent>(ground);
            col.size = Math::Vector3(40.0f, 1.0f, 40.0f);
        }

        // Sun (warm, angled for PS1 feel)
        {
            ECS::Entity sun = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(sun, "Sun");
            auto& lt = m_World->AddComponent<ECS::TransformComponent>(sun);
            lt.position = Math::Vector3(0.0f, 15.0f, 5.0f);
            lt.rotation = Math::Quaternion(Math::Vector3(1, 0, 0), Math::Radians(-40.0f));
            auto& lc = m_World->AddComponent<ECS::LightComponent>(sun);
            lc.type = ECS::LightType::Directional;
            lc.intensity = 1.4f;
            lc.color = Math::Vector3(1.0f, 0.95f, 0.85f);
            lc.castShadows = true;
        }

        // Party Leader (player-controlled in overworld)
        ECS::Entity leader;
        {
            leader = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(leader, "Party Leader");
            auto& pt = m_World->AddComponent<ECS::TransformComponent>(leader);
            pt.position = Math::Vector3(0.0f, 0.5f, 0.0f);
            m_World->AddComponent<ECS::MeshComponent>(leader, Renderer::MeshFactory::CreateCapsule(0.25f, 0.8f));
            auto& pm = m_World->AddComponent<ECS::MaterialComponent>(leader);
            pm.baseColor = Math::Vector3(0.2f, 0.3f, 0.8f);
            pm.flatShading = true;
            pm.vertexSnapping = true;
            pm.vertexSnapResolution = 160;

            auto& health = m_World->AddComponent<ECS::HealthComponent>(leader);
            health.maxHealth = 200.0f;
            health.currentHealth = 200.0f;
            auto& inv = m_World->AddComponent<ECS::InventoryComponent>(leader);
            (void)inv;

            auto& ctrl = m_World->AddComponent<ECS::ThirdPersonController>(leader);
            ctrl.moveSpeed = 4.0f;
            ctrl.cameraDistance = 6.0f;
            ctrl.cameraHeight = 3.0f;
            SetupCameraForController(leader, "ThirdPerson");
        }

        // Party Member 2
        {
            ECS::Entity member = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(member, "Party Member - Mage");
            auto& pt = m_World->AddComponent<ECS::TransformComponent>(member);
            pt.position = Math::Vector3(-1.5f, 0.5f, -1.0f);
            m_World->AddComponent<ECS::MeshComponent>(member, Renderer::MeshFactory::CreateCapsule(0.25f, 0.8f));
            auto& pm = m_World->AddComponent<ECS::MaterialComponent>(member);
            pm.baseColor = Math::Vector3(0.7f, 0.2f, 0.6f);
            pm.flatShading = true;
            pm.vertexSnapping = true;
            pm.vertexSnapResolution = 160;
            auto& health = m_World->AddComponent<ECS::HealthComponent>(member);
            health.maxHealth = 120.0f;
            health.currentHealth = 120.0f;
            auto& follow = m_World->AddComponent<ECS::FollowTargetComponent>(member);
            follow.target = leader;
            follow.offset = Math::Vector3(-1.5f, 0.0f, -1.0f);
            follow.moveSpeed = 3.5f;
        }

        // Party Member 3
        {
            ECS::Entity member = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(member, "Party Member - Fighter");
            auto& pt = m_World->AddComponent<ECS::TransformComponent>(member);
            pt.position = Math::Vector3(1.5f, 0.5f, -1.0f);
            m_World->AddComponent<ECS::MeshComponent>(member, Renderer::MeshFactory::CreateCapsule(0.3f, 0.9f));
            auto& pm = m_World->AddComponent<ECS::MaterialComponent>(member);
            pm.baseColor = Math::Vector3(0.8f, 0.3f, 0.2f);
            pm.flatShading = true;
            pm.vertexSnapping = true;
            pm.vertexSnapResolution = 160;
            auto& health = m_World->AddComponent<ECS::HealthComponent>(member);
            health.maxHealth = 300.0f;
            health.currentHealth = 300.0f;
            auto& follow = m_World->AddComponent<ECS::FollowTargetComponent>(member);
            follow.target = leader;
            follow.offset = Math::Vector3(1.5f, 0.0f, -1.0f);
            follow.moveSpeed = 3.5f;
        }

        // Town NPC (save point / shop)
        {
            ECS::Entity npc = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(npc, "Inn Keeper");
            auto& nt = m_World->AddComponent<ECS::TransformComponent>(npc);
            nt.position = Math::Vector3(5.0f, 0.5f, 3.0f);
            m_World->AddComponent<ECS::MeshComponent>(npc, Renderer::MeshFactory::CreateCapsule(0.25f, 0.8f));
            auto& nm = m_World->AddComponent<ECS::MaterialComponent>(npc);
            nm.baseColor = Math::Vector3(0.6f, 0.5f, 0.3f);
            nm.flatShading = true;
            nm.vertexSnapping = true;
            nm.vertexSnapResolution = 160;
            auto& interact = m_World->AddComponent<ECS::InteractableComponent>(npc);
            interact.interactionRange = 2.5f;
            interact.promptText = "Rest / Save";
            auto& dialogue = m_World->AddComponent<ECS::DialogueComponent>(npc);
            dialogue.speakerName = "Inn Keeper";
            dialogue.dialogueLines.push_back("Welcome, adventurers. Rest here to restore your strength.");
            dialogue.dialogueLines.push_back("The monsters beyond the bridge grow stronger each day...");
        }

        // Enemy Encounter Zone (trigger for random battles)
        {
            ECS::Entity encounterZone = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(encounterZone, "Battle Encounter Zone");
            auto& zt = m_World->AddComponent<ECS::TransformComponent>(encounterZone);
            zt.position = Math::Vector3(0.0f, 0.0f, 15.0f);
            auto& trigger = m_World->AddComponent<ECS::TriggerZoneComponent>(encounterZone);
            trigger.shape = ECS::TriggerZoneComponent::Shape::Box;
            trigger.boxSize = Math::Vector3(15.0f, 5.0f, 15.0f);

            auto& notes = m_World->AddComponent<ECS::NotesComponent>(encounterZone);
            notes.notes = "RANDOM ENCOUNTER ZONE\n"
                "========================\n"
                "When player enters this trigger zone, roll for random battle.\n"
                "Every N steps (or timer), chance of encounter increases.\n"
                "On encounter: transition to Battle Scene.\n";
        }

        // Battle Scene Root (positioned off to the side, used for turn-based combat)
        {
            ECS::Entity battleRoot = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(battleRoot, "Battle Scene Root");
            auto& bt = m_World->AddComponent<ECS::TransformComponent>(battleRoot);
            bt.position = Math::Vector3(100.0f, 0.0f, 0.0f);  // Off-screen, teleport party here for battles

            auto& notes = m_World->AddComponent<ECS::NotesComponent>(battleRoot);
            notes.notes = "TURN-BASED BATTLE SYSTEM\n"
                "============================\n"
                "Battle flow:\n"
                "  1. Transition: Screen wipe/fade to battle scene\n"
                "  2. Position party on LEFT, enemies on RIGHT\n"
                "  3. Fixed battle camera angle (side view, slight angle)\n"
                "  4. Turn order: Speed stat determines initiative\n"
                "\n"
                "Each turn:\n"
                "  - Show command menu: Attack / Magic / Item / Defend\n"
                "  - Player selects target (highlight with cursor)\n"
                "  - Execute action with animation\n"
                "  - Check victory/defeat conditions\n"
                "\n"
                "Battle end:\n"
                "  - Victory: Show EXP, Gil, items gained\n"
                "  - Defeat: Game Over screen or retry option\n"
                "  - Fade back to overworld at original position\n"
                "\n"
                "Party positions (relative to this root):\n"
                "  Leader:  (+3, 0.5, +1)\n"
                "  Mage:    (+3, 0.5, -1)\n"
                "  Fighter: (+4, 0.5,  0)\n"
                "\n"
                "Enemy positions:\n"
                "  Enemy 1: (-3, 0.5, +1)\n"
                "  Enemy 2: (-3, 0.5, -1)\n"
                "  Enemy 3: (-4, 0.5,  0)\n";
        }

        // Battle Camera
        {
            ECS::Entity battleCam = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(battleCam, "Battle Camera");
            auto& bct = m_World->AddComponent<ECS::TransformComponent>(battleCam);
            bct.position = Math::Vector3(100.0f, 3.0f, 8.0f);
            bct.rotation = Math::Quaternion(Math::Vector3(1, 0, 0), Math::Radians(-15.0f));
            auto& bcc = m_World->AddComponent<ECS::CameraComponent>(battleCam);
            bcc.fieldOfView = 50.0f;
            bcc.nearPlane = 0.1f;
            bcc.farPlane = 100.0f;
        }

        // Battle Ground
        {
            ECS::Entity bg = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(bg, "Battle Ground");
            auto& bgt = m_World->AddComponent<ECS::TransformComponent>(bg);
            bgt.position = Math::Vector3(100.0f, -0.05f, 0.0f);
            bgt.scale = Math::Vector3(15.0f, 0.1f, 10.0f);
            m_World->AddComponent<ECS::MeshComponent>(bg, Renderer::MeshFactory::CreateCube(1.0f));
            auto& bgm = m_World->AddComponent<ECS::MaterialComponent>(bg);
            bgm.baseColor = Math::Vector3(0.3f, 0.25f, 0.2f);
            bgm.flatShading = true;
            bgm.vertexSnapping = true;
            bgm.vertexSnapResolution = 160;
        }

        // Sample Enemies (in battle area)
        Math::Vector3 enemyColors[] = {
            Math::Vector3(0.5f, 0.8f, 0.2f),
            Math::Vector3(0.7f, 0.2f, 0.3f),
            Math::Vector3(0.4f, 0.3f, 0.7f),
        };
        const char* enemyNames[] = { "Goblin", "Imp", "Slime" };
        Math::Vector3 enemyPositions[] = {
            Math::Vector3(97.0f, 0.5f, 1.0f),
            Math::Vector3(97.0f, 0.5f, -1.0f),
            Math::Vector3(96.0f, 0.5f, 0.0f),
        };
        for (int i = 0; i < 3; ++i) {
            ECS::Entity enemy = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(enemy, enemyNames[i]);
            auto& et = m_World->AddComponent<ECS::TransformComponent>(enemy);
            et.position = enemyPositions[i];
            et.scale = Math::Vector3(0.6f + i * 0.1f);
            m_World->AddComponent<ECS::MeshComponent>(enemy, Renderer::MeshFactory::CreateSphere(0.5f));
            auto& em = m_World->AddComponent<ECS::MaterialComponent>(enemy);
            em.baseColor = enemyColors[i];
            em.flatShading = true;
            em.vertexSnapping = true;
            em.vertexSnapResolution = 160;
            auto& eh = m_World->AddComponent<ECS::HealthComponent>(enemy);
            eh.maxHealth = 30.0f + i * 20.0f;
            eh.currentHealth = eh.maxHealth;
            auto& dmg = m_World->AddComponent<ECS::DamageComponent>(enemy);
            dmg.damage = 8.0f + i * 4.0f;
        }

        // UI: Battle Menu placeholder
        {
            ECS::Entity menuText = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(menuText, "Battle Menu Text");
            auto& mt = m_World->AddComponent<ECS::TransformComponent>(menuText);
            mt.position = Math::Vector3(95.0f, -2.0f, 5.0f);
            auto& tc = m_World->AddComponent<ECS::TextComponent>(menuText);
            tc.text = "Attack  Magic  Item  Defend";
            tc.fontSize = 28.0f;
            tc.textColor = Math::Vector3(1.0f, 1.0f, 1.0f);
        }

        // Instructions
        {
            ECS::Entity instrRoot = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(instrRoot, "PS1 RPG Notes");
            auto& irt = m_World->AddComponent<ECS::TransformComponent>(instrRoot);
            irt.position = Math::Vector3(0.0f, 5.0f, 0.0f);
            auto& notes = m_World->AddComponent<ECS::NotesComponent>(instrRoot);
            notes.notes = "PS1-STYLE RPG TEMPLATE\n"
                "============================\n"
                "Retro rendering is enabled on all materials:\n"
                "  - Flat shading (no smooth normals)\n"
                "  - Vertex snapping (PS1 jitter at 160px)\n"
                "\n"
                "For full PS1 effect, enable in Post-Processing:\n"
                "  - Dithering\n"
                "  - Color quantization\n"
                "  - Resolution downscale\n"
                "\n"
                "Overworld: Third-person camera follows Party Leader.\n"
                "Party members follow the leader via FollowTargetComponent.\n"
                "Battle Scene Root is at X=100 (off-screen area).\n"
                "\n"
                "To trigger a battle:\n"
                "  1. Detect player in encounter zone\n"
                "  2. Fade to black, teleport party to battle positions\n"
                "  3. Switch active camera to Battle Camera\n"
                "  4. Run turn-based loop\n"
                "  5. Fade back, restore overworld positions\n";
        }
    }

    // ===== City Builder Template =====
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
    }

    // ===== FPS Arena Template =====
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
    }

    // ===== 3D Team Sports Template =====
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
    }

    // ===== Tower Defense Template =====
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
    }

    // ===== Puzzle Platformer Template =====
    else if (templateId == "puzzle") {
        // Ground
        {
            ECS::Entity ground = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(ground, "Ground");
            auto& gt = m_World->AddComponent<ECS::TransformComponent>(ground);
            gt.position = Math::Vector3(0.0f, -1.0f, 0.0f);
            gt.scale = Math::Vector3(25.0f, 1.0f, 1.0f);
            auto& gm = m_World->AddComponent<ECS::MaterialComponent>(ground);
            gm.baseColor = Math::Vector3(0.4f, 0.4f, 0.45f);
            m_World->AddComponent<ECS::MeshComponent>(ground, Renderer::MeshFactory::CreateQuad(1.0f, 1.0f));
            auto& col = m_World->AddComponent<ECS::BoxColliderComponent>(ground);
            col.size = Math::Vector3(25.0f, 1.0f, 1.0f);
        }
        createLight();

        // Player
        {
            ECS::Entity player = createPlayer2D("Player");
            auto& ctrl = m_World->AddComponent<ECS::Platformer2DController>(player);
            ctrl.moveSpeed = 4.0f;
            ctrl.jumpForce = 10.0f;
            SetupCameraForController(player, "Platformer2D");
        }

        // Pushable Box 1
        {
            ECS::Entity box = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(box, "Pushable Box");
            auto& bt = m_World->AddComponent<ECS::TransformComponent>(box);
            bt.position = Math::Vector3(3.0f, 0.5f, 0.0f);
            bt.scale = Math::Vector3(1.0f);
            m_World->AddComponent<ECS::MeshComponent>(box, Renderer::MeshFactory::CreateCube(1.0f));
            auto& bm = m_World->AddComponent<ECS::MaterialComponent>(box);
            bm.baseColor = Math::Vector3(0.6f, 0.45f, 0.2f);
            auto& rb = m_World->AddComponent<ECS::RigidbodyComponent>(box);
            rb.mass = 2.0f; rb.drag = 3.0f;
            auto& col = m_World->AddComponent<ECS::BoxColliderComponent>(box);
            col.size = Math::Vector3(1.0f);
            auto& tag = m_World->AddComponent<ECS::TagComponent>(box);
            tag.tags.push_back("pushable");
        }

        // Pushable Box 2
        {
            ECS::Entity box = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(box, "Pushable Box 2");
            auto& bt = m_World->AddComponent<ECS::TransformComponent>(box);
            bt.position = Math::Vector3(8.0f, 0.5f, 0.0f);
            bt.scale = Math::Vector3(1.0f);
            m_World->AddComponent<ECS::MeshComponent>(box, Renderer::MeshFactory::CreateCube(1.0f));
            auto& bm = m_World->AddComponent<ECS::MaterialComponent>(box);
            bm.baseColor = Math::Vector3(0.6f, 0.45f, 0.2f);
            auto& rb = m_World->AddComponent<ECS::RigidbodyComponent>(box);
            rb.mass = 2.0f; rb.drag = 3.0f;
            auto& col = m_World->AddComponent<ECS::BoxColliderComponent>(box);
            col.size = Math::Vector3(1.0f);
            auto& tag = m_World->AddComponent<ECS::TagComponent>(box);
            tag.tags.push_back("pushable");
        }

        // Pressure Plate 1
        {
            ECS::Entity plate = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(plate, "Pressure Plate A");
            auto& pt = m_World->AddComponent<ECS::TransformComponent>(plate);
            pt.position = Math::Vector3(5.0f, -0.4f, 0.0f);
            pt.scale = Math::Vector3(1.2f, 0.1f, 1.0f);
            m_World->AddComponent<ECS::MeshComponent>(plate, Renderer::MeshFactory::CreateCube(1.0f));
            auto& pm = m_World->AddComponent<ECS::MaterialComponent>(plate);
            pm.baseColor = Math::Vector3(0.8f, 0.7f, 0.2f);
            auto& trigger = m_World->AddComponent<ECS::TriggerZoneComponent>(plate);
            trigger.shape = ECS::TriggerZoneComponent::Shape::Box;
            trigger.boxSize = Math::Vector3(1.2f, 0.5f, 1.0f);
            auto& tag = m_World->AddComponent<ECS::TagComponent>(plate);
            tag.tags.push_back("pressure_plate");
            tag.tags.push_back("opens:door_a");
        }

        // Pressure Plate 2
        {
            ECS::Entity plate = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(plate, "Pressure Plate B");
            auto& pt = m_World->AddComponent<ECS::TransformComponent>(plate);
            pt.position = Math::Vector3(10.0f, -0.4f, 0.0f);
            pt.scale = Math::Vector3(1.2f, 0.1f, 1.0f);
            m_World->AddComponent<ECS::MeshComponent>(plate, Renderer::MeshFactory::CreateCube(1.0f));
            auto& pm = m_World->AddComponent<ECS::MaterialComponent>(plate);
            pm.baseColor = Math::Vector3(0.8f, 0.7f, 0.2f);
            auto& trigger = m_World->AddComponent<ECS::TriggerZoneComponent>(plate);
            trigger.shape = ECS::TriggerZoneComponent::Shape::Box;
            trigger.boxSize = Math::Vector3(1.2f, 0.5f, 1.0f);
            auto& tag = m_World->AddComponent<ECS::TagComponent>(plate);
            tag.tags.push_back("pressure_plate");
            tag.tags.push_back("opens:door_b");
        }

        // Door A (vertical wall that blocks path)
        {
            ECS::Entity door = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(door, "Door A");
            auto& dt = m_World->AddComponent<ECS::TransformComponent>(door);
            dt.position = Math::Vector3(7.0f, 1.0f, 0.0f);
            dt.scale = Math::Vector3(0.3f, 2.5f, 1.0f);
            m_World->AddComponent<ECS::MeshComponent>(door, Renderer::MeshFactory::CreateCube(1.0f));
            auto& dm = m_World->AddComponent<ECS::MaterialComponent>(door);
            dm.baseColor = Math::Vector3(0.5f, 0.2f, 0.2f);
            auto& col = m_World->AddComponent<ECS::BoxColliderComponent>(door);
            col.size = Math::Vector3(0.3f, 2.5f, 1.0f);
            auto& tag = m_World->AddComponent<ECS::TagComponent>(door);
            tag.tags.push_back("door");
            tag.tags.push_back("id:door_a");
        }

        // Door B
        {
            ECS::Entity door = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(door, "Door B");
            auto& dt = m_World->AddComponent<ECS::TransformComponent>(door);
            dt.position = Math::Vector3(12.0f, 1.0f, 0.0f);
            dt.scale = Math::Vector3(0.3f, 2.5f, 1.0f);
            m_World->AddComponent<ECS::MeshComponent>(door, Renderer::MeshFactory::CreateCube(1.0f));
            auto& dm = m_World->AddComponent<ECS::MaterialComponent>(door);
            dm.baseColor = Math::Vector3(0.2f, 0.2f, 0.5f);
            auto& col = m_World->AddComponent<ECS::BoxColliderComponent>(door);
            col.size = Math::Vector3(0.3f, 2.5f, 1.0f);
            auto& tag = m_World->AddComponent<ECS::TagComponent>(door);
            tag.tags.push_back("door");
            tag.tags.push_back("id:door_b");
        }

        // Exit / Goal
        {
            ECS::Entity goal = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(goal, "Level Exit");
            auto& gt = m_World->AddComponent<ECS::TransformComponent>(goal);
            gt.position = Math::Vector3(15.0f, 0.5f, 0.0f);
            gt.scale = Math::Vector3(0.8f);
            m_World->AddComponent<ECS::MeshComponent>(goal, Renderer::MeshFactory::CreateSphere(0.5f));
            auto& gm = m_World->AddComponent<ECS::MaterialComponent>(goal);
            gm.baseColor = Math::Vector3(0.2f, 0.9f, 0.4f);
            gm.emissiveColor = Math::Vector3(0.1f, 0.5f, 0.2f);
            gm.emissiveStrength = 0.8f;
            auto& trigger = m_World->AddComponent<ECS::TriggerZoneComponent>(goal);
            trigger.shape = ECS::TriggerZoneComponent::Shape::Sphere;
            trigger.sphereRadius = 0.8f;
            auto& tag = m_World->AddComponent<ECS::TagComponent>(goal);
            tag.tags.push_back("level_exit");
        }

        // Platforms
        Math::Vector3 platPositions[] = {
            Math::Vector3(-3.0f, 1.0f, 0.0f), Math::Vector3(-5.0f, 2.5f, 0.0f), Math::Vector3(-2.0f, 4.0f, 0.0f),
        };
        for (int i = 0; i < 3; ++i) {
            ECS::Entity plat = m_World->CreateEntity();
            char name[32]; snprintf(name, sizeof(name), "Platform %d", i + 1);
            m_World->AddComponent<ECS::NameComponent>(plat, name);
            auto& pt = m_World->AddComponent<ECS::TransformComponent>(plat);
            pt.position = platPositions[i];
            pt.scale = Math::Vector3(2.5f, 0.3f, 1.0f);
            m_World->AddComponent<ECS::MeshComponent>(plat, Renderer::MeshFactory::CreateQuad(1.0f, 1.0f));
            auto& pm = m_World->AddComponent<ECS::MaterialComponent>(plat);
            pm.baseColor = Math::Vector3(0.35f, 0.4f, 0.45f);
            auto& col = m_World->AddComponent<ECS::BoxColliderComponent>(plat);
            col.size = Math::Vector3(2.5f, 0.3f, 1.0f);
        }

        // Key collectible (on high platform)
        {
            ECS::Entity key = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(key, "Key");
            auto& kt = m_World->AddComponent<ECS::TransformComponent>(key);
            kt.position = Math::Vector3(-2.0f, 4.8f, 0.0f);
            kt.scale = Math::Vector3(0.3f);
            m_World->AddComponent<ECS::MeshComponent>(key, Renderer::MeshFactory::CreateSphere(0.5f));
            auto& km = m_World->AddComponent<ECS::MaterialComponent>(key);
            km.baseColor = Math::Vector3(1.0f, 0.85f, 0.0f);
            km.emissiveColor = Math::Vector3(1.0f, 0.7f, 0.0f);
            km.emissiveStrength = 0.5f;
            auto& pc = m_World->AddComponent<ECS::PickupComponent>(key);
            pc.type = ECS::PickupComponent::PickupType::Key;
        }
    }

    // ===== Horror / Walking Sim Template =====
    else if (templateId == "horror") {
        // Dark ground
        {
            ECS::Entity ground = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(ground, "Floor");
            auto& gt = m_World->AddComponent<ECS::TransformComponent>(ground);
            gt.scale = Math::Vector3(20.0f, 0.1f, 20.0f);
            gt.position = Math::Vector3(0.0f, -0.05f, 0.0f);
            auto& gm = m_World->AddComponent<ECS::MaterialComponent>(ground);
            gm.baseColor = Math::Vector3(0.12f, 0.1f, 0.1f);
            gm.roughness = 0.9f;
            m_World->AddComponent<ECS::MeshComponent>(ground, Renderer::MeshFactory::CreateCube(1.0f));
            auto& col = m_World->AddComponent<ECS::BoxColliderComponent>(ground);
            col.size = Math::Vector3(20.0f, 0.1f, 20.0f);
        }

        // Very dim ambient light
        {
            ECS::Entity amb = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(amb, "Ambient Light");
            auto& lt = m_World->AddComponent<ECS::TransformComponent>(amb);
            lt.position = Math::Vector3(0.0f, 10.0f, 0.0f);
            auto& lc = m_World->AddComponent<ECS::LightComponent>(amb);
            lc.type = ECS::LightType::Directional;
            lc.intensity = 0.05f;  // Very dim
            lc.color = Math::Vector3(0.4f, 0.4f, 0.6f);  // Cold blue tint
        }

        // Player with flashlight
        ECS::Entity player;
        {
            player = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(player, "Player");
            auto& pt = m_World->AddComponent<ECS::TransformComponent>(player);
            pt.position = Math::Vector3(0.0f, 1.7f, 0.0f);
            m_World->AddComponent<ECS::MeshComponent>(player, Renderer::MeshFactory::CreateCapsule(0.3f, 1.0f));
            auto& pm = m_World->AddComponent<ECS::MaterialComponent>(player);
            pm.baseColor = Math::Vector3(0.3f, 0.3f, 0.35f);
            auto& ctrl = m_World->AddComponent<ECS::FirstPersonController>(player);
            ctrl.moveSpeed = 3.0f;  // Slow, tense movement
            ctrl.mouseSensitivity = 0.12f;
            ctrl.sprintMultiplier = 1.3f;
            SetupCameraForController(player, "FirstPerson");
            m_World->AddComponent<ECS::AudioListenerComponent>(player);
        }

        // Flashlight (spot light attached to player)
        {
            ECS::Entity flashlight = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(flashlight, "Flashlight");
            auto& ft = m_World->AddComponent<ECS::TransformComponent>(flashlight);
            ft.position = Math::Vector3(0.0f, 1.5f, 0.0f);
            auto& lc = m_World->AddComponent<ECS::LightComponent>(flashlight);
            lc.type = ECS::LightType::Spot;
            lc.intensity = 3.0f;
            lc.color = Math::Vector3(1.0f, 0.95f, 0.8f);  // Warm white
            lc.range = 20.0f;
            lc.outerConeAngle = 30.0f;
            lc.castShadows = true;
            auto& follow = m_World->AddComponent<ECS::FollowTargetComponent>(flashlight);
            follow.target = player;
            follow.offset = Math::Vector3(0.3f, -0.2f, 0.0f);
            follow.moveSpeed = 100.0f;
        }

        // Corridor walls
        Math::Vector3 wallPositions[] = {
            Math::Vector3(-3.0f, 1.5f, 0.0f), Math::Vector3(3.0f, 1.5f, 0.0f),
            Math::Vector3(0.0f, 1.5f, 8.0f), Math::Vector3(-3.0f, 1.5f, 14.0f),
            Math::Vector3(3.0f, 1.5f, 14.0f),
        };
        Math::Vector3 wallScales[] = {
            Math::Vector3(0.2f, 3.0f, 16.0f), Math::Vector3(0.2f, 3.0f, 16.0f),
            Math::Vector3(6.0f, 3.0f, 0.2f),  Math::Vector3(0.2f, 3.0f, 4.0f),
            Math::Vector3(0.2f, 3.0f, 4.0f),
        };
        for (int i = 0; i < 5; ++i) {
            ECS::Entity wall = m_World->CreateEntity();
            char name[32]; snprintf(name, sizeof(name), "Wall %d", i + 1);
            m_World->AddComponent<ECS::NameComponent>(wall, name);
            auto& wt = m_World->AddComponent<ECS::TransformComponent>(wall);
            wt.position = wallPositions[i];
            wt.scale = wallScales[i];
            m_World->AddComponent<ECS::MeshComponent>(wall, Renderer::MeshFactory::CreateCube(1.0f));
            auto& wm = m_World->AddComponent<ECS::MaterialComponent>(wall);
            wm.baseColor = Math::Vector3(0.15f, 0.13f, 0.12f);
            auto& col = m_World->AddComponent<ECS::BoxColliderComponent>(wall);
            col.size = wallScales[i];
        }

        // Collectible Notes
        const char* noteTexts[] = {
            "Day 1: The lights went out yesterday. I can hear something in the walls.",
            "Day 3: Found a key in the basement. The door at the end of the hall...",
            "Day 5: It's getting closer. Don't turn around. Never turn around.",
        };
        Math::Vector3 notePositions[] = {
            Math::Vector3(1.0f, 0.8f, 3.0f), Math::Vector3(-1.5f, 0.8f, 7.0f), Math::Vector3(1.0f, 0.8f, 12.0f),
        };
        for (int i = 0; i < 3; ++i) {
            ECS::Entity note = m_World->CreateEntity();
            char name[32]; snprintf(name, sizeof(name), "Note %d", i + 1);
            m_World->AddComponent<ECS::NameComponent>(note, name);
            auto& nt = m_World->AddComponent<ECS::TransformComponent>(note);
            nt.position = notePositions[i];
            nt.scale = Math::Vector3(0.3f, 0.4f, 0.02f);
            m_World->AddComponent<ECS::MeshComponent>(note, Renderer::MeshFactory::CreateQuad(1.0f, 1.0f));
            auto& nm = m_World->AddComponent<ECS::MaterialComponent>(note);
            nm.baseColor = Math::Vector3(0.9f, 0.85f, 0.7f);
            nm.emissiveColor = Math::Vector3(0.3f, 0.28f, 0.2f);
            nm.emissiveStrength = 0.1f;
            auto& interact = m_World->AddComponent<ECS::InteractableComponent>(note);
            interact.interactionRange = 2.0f;
            interact.promptText = "Read Note";
            auto& dialogue = m_World->AddComponent<ECS::DialogueComponent>(note);
            dialogue.speakerName = "Found Note";
            dialogue.dialogueLines.push_back(noteTexts[i]);
            auto& pc = m_World->AddComponent<ECS::PickupComponent>(note);
            pc.type = ECS::PickupComponent::PickupType::Custom;
            pc.customId = "note";
        }

        // Locked door
        {
            ECS::Entity door = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(door, "Locked Door");
            auto& dt = m_World->AddComponent<ECS::TransformComponent>(door);
            dt.position = Math::Vector3(0.0f, 1.5f, 16.0f);
            dt.scale = Math::Vector3(2.5f, 3.0f, 0.2f);
            m_World->AddComponent<ECS::MeshComponent>(door, Renderer::MeshFactory::CreateCube(1.0f));
            auto& dm = m_World->AddComponent<ECS::MaterialComponent>(door);
            dm.baseColor = Math::Vector3(0.25f, 0.15f, 0.1f);
            auto& col = m_World->AddComponent<ECS::BoxColliderComponent>(door);
            col.size = Math::Vector3(2.5f, 3.0f, 0.2f);
            auto& interact = m_World->AddComponent<ECS::InteractableComponent>(door);
            interact.interactionRange = 2.5f;
            interact.promptText = "Locked - Need Key";
            auto& tag = m_World->AddComponent<ECS::TagComponent>(door);
            tag.tags.push_back("locked_door");
        }

        // Key
        {
            ECS::Entity key = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(key, "Basement Key");
            auto& kt = m_World->AddComponent<ECS::TransformComponent>(key);
            kt.position = Math::Vector3(-2.0f, 0.3f, 10.0f);
            kt.scale = Math::Vector3(0.2f);
            m_World->AddComponent<ECS::MeshComponent>(key, Renderer::MeshFactory::CreateCube(1.0f));
            auto& km = m_World->AddComponent<ECS::MaterialComponent>(key);
            km.baseColor = Math::Vector3(0.8f, 0.7f, 0.2f);
            km.emissiveColor = Math::Vector3(0.4f, 0.3f, 0.1f);
            km.emissiveStrength = 0.3f;
            auto& pc = m_World->AddComponent<ECS::PickupComponent>(key);
            pc.type = ECS::PickupComponent::PickupType::Key;
            pc.pickupRange = 1.5f;
        }

        // Ambient sound source
        {
            ECS::Entity ambient = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(ambient, "Ambient Creaks");
            auto& at = m_World->AddComponent<ECS::TransformComponent>(ambient);
            at.position = Math::Vector3(0.0f, 2.0f, 6.0f);
            auto& audio = m_World->AddComponent<ECS::AudioSourceComponent>(ambient);
            audio.loop = true;
            audio.volume = 0.4f;
            audio.is3D = true;
            audio.minDistance = 1.0f;
            audio.maxDistance = 15.0f;
        }
    }

    // ===== Endless Runner Template =====
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
            ct.position = Math::Vector3(lane * 2.0f, 1.0f, 10.0f + i * 8.0f);
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
    }
    else if (templateId == "flower") {
        // Flower Garden template - procedural flower with pluckable leaves and petals

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
            m_World->AddComponent<ECS::FlowerStemComponent>(stemEntity);
        }

        // Petals (arranged radially)
        const int petalCount = 10;
        const float petalHeight = 1.7f;
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
            tether.stemEntity = stemEntity;
            tether.breakDistance = 1.5f;
            tether.tensionRamp = 2.5f;
            auto& grab = m_World->AddComponent<ECS::GrabbableComponent>(petal);
            grab.pullForce = 12.0f;
        }

        // Leaves along the stem
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
            tether.stemEntity = stemEntity;
            tether.breakDistance = 1.5f;
            tether.tensionRamp = 2.0f;
            auto& grab = m_World->AddComponent<ECS::GrabbableComponent>(leaf);
            grab.pullForce = 15.0f;
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
            text.text = "Petals: 0/10 | Leaves: 0/5 | Score: 0";
            text.fontSize = 24.0f;
            text.textColor = Math::Vector3(1.0f, 1.0f, 1.0f);
            auto& scoreTag = m_World->AddComponent<ECS::TagComponent>(scoreText);
            scoreTag.tags.push_back("score_display");
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
    }

    else if (templateId == "dungeon") {
        // SMT-style First-Person Dungeon Crawler
        // Grid-based dungeon layout with corridors, doors, encounter zones, and party HUD

        const f32 CELL = 3.0f;       // Grid cell size in world units
        const f32 WALL_H = 3.0f;     // Wall height
        const f32 WALL_THICK = 0.15f; // Wall thickness

        // --- Dungeon layout (8x8 grid) ---
        // 1 = floor, 0 = solid, 2 = door, 3 = encounter, 4 = stairs, 5 = treasure
        // N/S walls placed on cell edges, E/W walls placed on cell edges
        const int GRID = 8;
        int layout[GRID][GRID] = {
            {0,0,0,0,0,0,0,0},
            {0,1,1,1,0,1,1,0},
            {0,1,0,1,0,1,3,0},
            {0,1,0,2,1,1,1,0},
            {0,1,0,1,0,0,1,0},
            {0,1,1,1,1,3,1,0},
            {0,5,0,0,1,1,4,0},
            {0,0,0,0,0,0,0,0},
        };

        // Helper lambdas for wall creation
        auto makeWall = [&](const char* name, Math::Vector3 pos, Math::Vector3 scale, Math::Vector3 color, bool addCollider = false) {
            ECS::Entity wall = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(wall, name);
            auto& wt = m_World->AddComponent<ECS::TransformComponent>(wall);
            wt.position = pos;
            wt.scale = scale;
            auto& wmat = m_World->AddComponent<ECS::MaterialComponent>(wall);
            wmat.baseColor = color;
            wmat.roughness = 0.85f;
            m_World->AddComponent<ECS::MeshComponent>(wall, Renderer::MeshFactory::CreateCube(1.0f));
            if (addCollider) {
                auto& col = m_World->AddComponent<ECS::BoxColliderComponent>(wall);
                col.size = Math::Vector3(1.0f, 1.0f, 1.0f);  // Unit cube scaled by transform
            }
            return wall;
        };

        // Dungeon palette
        Math::Vector3 floorColor(0.15f, 0.15f, 0.18f);
        Math::Vector3 ceilingColor(0.12f, 0.12f, 0.14f);
        Math::Vector3 wallColor(0.22f, 0.20f, 0.25f);
        Math::Vector3 wallAccent(0.28f, 0.18f, 0.18f);
        Math::Vector3 doorColor(0.45f, 0.30f, 0.15f);
        Math::Vector3 encounterColor(0.6f, 0.1f, 0.1f);
        Math::Vector3 stairsColor(0.5f, 0.5f, 0.55f);
        Math::Vector3 treasureColor(0.8f, 0.7f, 0.2f);

        int wallIdx = 0;
        int floorIdx = 0;
        char nameBuf[64];

        // Player start position (cell 1,1)
        Math::Vector3 playerStart(1.0f * CELL + CELL * 0.5f, 1.5f, 1.0f * CELL + CELL * 0.5f);

        for (int z = 0; z < GRID; ++z) {
            for (int x = 0; x < GRID; ++x) {
                if (layout[z][x] == 0) continue;

                f32 cx = static_cast<f32>(x) * CELL + CELL * 0.5f;
                f32 cz = static_cast<f32>(z) * CELL + CELL * 0.5f;

                // Floor tile
                snprintf(nameBuf, sizeof(nameBuf), "Floor_%d", floorIdx++);
                makeWall(nameBuf,
                    Math::Vector3(cx, 0.0f, cz),
                    Math::Vector3(CELL, 0.1f, CELL),
                    floorColor);

                // Ceiling tile
                snprintf(nameBuf, sizeof(nameBuf), "Ceiling_%d", floorIdx);
                makeWall(nameBuf,
                    Math::Vector3(cx, WALL_H, cz),
                    Math::Vector3(CELL, 0.1f, CELL),
                    ceilingColor);

                // Special cell markers
                if (layout[z][x] == 2) {
                    // Door
                    snprintf(nameBuf, sizeof(nameBuf), "Door_%d_%d", x, z);
                    ECS::Entity door = makeWall(nameBuf,
                        Math::Vector3(cx, WALL_H * 0.4f, cz),
                        Math::Vector3(CELL * 0.7f, WALL_H * 0.8f, WALL_THICK * 2.0f),
                        doorColor);
                    auto& dtag = m_World->AddComponent<ECS::TagComponent>(door);
                    dtag.tags.push_back("door");
                    auto& interact = m_World->AddComponent<ECS::InteractableComponent>(door);
                    interact.promptText = "Open Door";
                    interact.interactionRange = CELL;
                }
                else if (layout[z][x] == 3) {
                    // Encounter zone — red floor marker
                    snprintf(nameBuf, sizeof(nameBuf), "Encounter_%d_%d", x, z);
                    ECS::Entity enc = makeWall(nameBuf,
                        Math::Vector3(cx, 0.06f, cz),
                        Math::Vector3(CELL * 0.6f, 0.02f, CELL * 0.6f),
                        encounterColor);
                    auto& etag = m_World->AddComponent<ECS::TagComponent>(enc);
                    etag.tags.push_back("encounter_zone");
                    // Trigger zone for random encounters
                    auto& trigger = m_World->AddComponent<ECS::TriggerZoneComponent>(enc);
                    trigger.shape = ECS::TriggerZoneComponent::Shape::Sphere;
                    trigger.sphereRadius = CELL * 0.5f;
                    trigger.triggerOnce = false;
                }
                else if (layout[z][x] == 4) {
                    // Stairs down
                    snprintf(nameBuf, sizeof(nameBuf), "Stairs_%d_%d", x, z);
                    ECS::Entity stairs = m_World->CreateEntity();
                    m_World->AddComponent<ECS::NameComponent>(stairs, nameBuf);
                    auto& st = m_World->AddComponent<ECS::TransformComponent>(stairs);
                    st.position = Math::Vector3(cx, 0.3f, cz);
                    st.scale = Math::Vector3(CELL * 0.5f, 0.6f, CELL * 0.5f);
                    st.rotation = Math::Quaternion(Math::Vector3(1, 0, 0), Math::Radians(-20.0f));
                    auto& smat = m_World->AddComponent<ECS::MaterialComponent>(stairs);
                    smat.baseColor = stairsColor;
                    smat.roughness = 0.7f;
                    m_World->AddComponent<ECS::MeshComponent>(stairs, Renderer::MeshFactory::CreateCube(1.0f));
                    auto& stag = m_World->AddComponent<ECS::TagComponent>(stairs);
                    stag.tags.push_back("stairs_down");
                    auto& sinteract = m_World->AddComponent<ECS::InteractableComponent>(stairs);
                    sinteract.promptText = "Descend";
                    sinteract.interactionRange = CELL;
                }
                else if (layout[z][x] == 5) {
                    // Treasure chest
                    snprintf(nameBuf, sizeof(nameBuf), "Treasure_%d_%d", x, z);
                    ECS::Entity chest = m_World->CreateEntity();
                    m_World->AddComponent<ECS::NameComponent>(chest, nameBuf);
                    auto& ct = m_World->AddComponent<ECS::TransformComponent>(chest);
                    ct.position = Math::Vector3(cx, 0.3f, cz);
                    ct.scale = Math::Vector3(0.8f, 0.6f, 0.5f);
                    auto& cmat = m_World->AddComponent<ECS::MaterialComponent>(chest);
                    cmat.baseColor = treasureColor;
                    cmat.roughness = 0.4f;
                    cmat.metallic = 0.6f;
                    m_World->AddComponent<ECS::MeshComponent>(chest, Renderer::MeshFactory::CreateCube(1.0f));
                    auto& ctag = m_World->AddComponent<ECS::TagComponent>(chest);
                    ctag.tags.push_back("treasure");
                    auto& cinteract = m_World->AddComponent<ECS::InteractableComponent>(chest);
                    cinteract.promptText = "Open Chest";
                    cinteract.interactionRange = CELL * 0.8f;
                    cinteract.singleUse = true;
                }

                // Walls on edges adjacent to solid (0) cells or grid boundary
                // North wall (z-1 is solid or out of bounds)
                if (z == 0 || layout[z - 1][x] == 0) {
                    snprintf(nameBuf, sizeof(nameBuf), "Wall_N_%d", wallIdx++);
                    bool accent = (wallIdx % 5 == 0);
                    makeWall(nameBuf,
                        Math::Vector3(cx, WALL_H * 0.5f, cz - CELL * 0.5f),
                        Math::Vector3(CELL, WALL_H, WALL_THICK),
                        accent ? wallAccent : wallColor, true);
                }
                // South wall
                if (z == GRID - 1 || layout[z + 1][x] == 0) {
                    snprintf(nameBuf, sizeof(nameBuf), "Wall_S_%d", wallIdx++);
                    bool accent = (wallIdx % 5 == 0);
                    makeWall(nameBuf,
                        Math::Vector3(cx, WALL_H * 0.5f, cz + CELL * 0.5f),
                        Math::Vector3(CELL, WALL_H, WALL_THICK),
                        accent ? wallAccent : wallColor, true);
                }
                // West wall
                if (x == 0 || layout[z][x - 1] == 0) {
                    snprintf(nameBuf, sizeof(nameBuf), "Wall_W_%d", wallIdx++);
                    bool accent = (wallIdx % 5 == 0);
                    makeWall(nameBuf,
                        Math::Vector3(cx - CELL * 0.5f, WALL_H * 0.5f, cz),
                        Math::Vector3(WALL_THICK, WALL_H, CELL),
                        accent ? wallAccent : wallColor, true);
                }
                // East wall
                if (x == GRID - 1 || layout[z][x + 1] == 0) {
                    snprintf(nameBuf, sizeof(nameBuf), "Wall_E_%d", wallIdx++);
                    bool accent = (wallIdx % 5 == 0);
                    makeWall(nameBuf,
                        Math::Vector3(cx + CELL * 0.5f, WALL_H * 0.5f, cz),
                        Math::Vector3(WALL_THICK, WALL_H, CELL),
                        accent ? wallAccent : wallColor, true);
                }
            }
        }

        // Torches along corridors (point lights)
        int torchIdx = 0;
        int torchCells[][2] = {{1,1}, {3,3}, {1,5}, {5,5}, {3,1}, {6,5}};
        for (auto& tc : torchCells) {
            int tx = tc[0], tz = tc[1];
            if (layout[tz][tx] == 0) continue;

            f32 cx = static_cast<f32>(tx) * CELL + CELL * 0.5f;
            f32 cz = static_cast<f32>(tz) * CELL + CELL * 0.5f;

            snprintf(nameBuf, sizeof(nameBuf), "Torch_%d", torchIdx++);
            ECS::Entity torch = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(torch, nameBuf);
            auto& tt = m_World->AddComponent<ECS::TransformComponent>(torch);
            tt.position = Math::Vector3(cx + 1.2f, WALL_H * 0.7f, cz);
            tt.scale = Math::Vector3(0.1f, 0.3f, 0.1f);
            auto& tmat = m_World->AddComponent<ECS::MaterialComponent>(torch);
            tmat.baseColor = Math::Vector3(0.8f, 0.5f, 0.1f);
            tmat.emissiveColor = Math::Vector3(1.0f, 0.6f, 0.2f);
            tmat.emissiveStrength = 2.0f;
            m_World->AddComponent<ECS::MeshComponent>(torch, Renderer::MeshFactory::CreateCube(1.0f));

            // Point light for the torch
            auto& lc = m_World->AddComponent<ECS::LightComponent>(torch);
            lc.type = ECS::LightType::Point;
            lc.color = Math::Vector3(1.0f, 0.65f, 0.3f);
            lc.intensity = 2.5f;
            lc.range = CELL * 2.5f;
            lc.castShadows = true;
        }

        // Dim ambient directional light (dungeon ambiance)
        {
            ECS::Entity ambLight = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(ambLight, "Ambient Light");
            auto& lt = m_World->AddComponent<ECS::TransformComponent>(ambLight);
            lt.rotation = Math::Quaternion(Math::Vector3(1, 0, 0), Math::Radians(-70.0f));
            auto& lc = m_World->AddComponent<ECS::LightComponent>(ambLight);
            lc.type = ECS::LightType::Directional;
            lc.color = Math::Vector3(0.15f, 0.15f, 0.25f);
            lc.intensity = 0.3f;
            lc.castShadows = false;
        }

        // Player entity at ground level with FPS controller
        ECS::Entity player;
        {
            player = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(player, "Player");
            auto& pt = m_World->AddComponent<ECS::TransformComponent>(player);
            pt.position = Math::Vector3(playerStart.x, 0.0f, playerStart.z);  // Feet on ground

            // FPS character controller for grid-style dungeon navigation
            auto& fps = m_World->AddComponent<ECS::FirstPersonController>(player);
            fps.moveSpeed = 4.0f;
            fps.mouseSensitivity = 0.15f;
            fps.gridMovement = true;
            fps.gridCellSize = CELL;
            fps.gridOrigin = Math::Vector3(CELL * 0.5f, 0.0f, CELL * 0.5f);
            fps.standingHeight = 1.5f;  // Eye height for dungeon
            fps.currentHeight = 1.5f;
            fps.dungeonCrawlerMode = true;  // SMT-style snap turns + facing movement
            fps.snapTurnAngle = 90.0f;
            fps.yaw = 180.0f;  // Face south (into the corridor)

            // Auto-create separate camera entity
            SetupCameraForController(player, "FirstPerson");

            // Override camera settings for dungeon
            ECS::Entity camEntity = ECS::CameraManager::GetActiveCamera(m_World);
            if (camEntity != ECS::INVALID_ENTITY) {
                auto* cc = m_World->GetComponent<ECS::CameraComponent>(camEntity);
                if (cc) {
                    cc->fieldOfView = 70.0f;
                    cc->nearPlane = 0.1f;
                    cc->farPlane = 50.0f;
                }
                m_SelectedGameCamera = camEntity;
            }
        }

        // --- HUD Text Entities ---

        // Party stats display (top-left style)
        {
            ECS::Entity partyHud = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(partyHud, "Party HUD");
            auto& ht = m_World->AddComponent<ECS::TransformComponent>(partyHud);
            ht.position = Math::Vector3(-3.0f, 4.5f, 0.0f);
            auto& htext = m_World->AddComponent<ECS::TextComponent>(partyHud);
            htext.text = "=== PARTY ===\n"
                         "Hero    Lv12  HP 142/142  MP 38/38\n"
                         "Pixie   Lv10  HP  86/86   MP 52/52\n"
                         "Cerberus Lv11 HP 168/168  MP 24/24\n"
                         "Angel   Lv13  HP 120/120  MP 64/64";
            htext.fontSize = 16.0f;
            htext.textColor = Math::Vector3(0.0f, 1.0f, 0.4f);
            auto& htag = m_World->AddComponent<ECS::TagComponent>(partyHud);
            htag.tags.push_back("party_hud");
        }

        // Compass / minimap display (top-right style)
        {
            ECS::Entity compass = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(compass, "Compass");
            auto& cpt = m_World->AddComponent<ECS::TransformComponent>(compass);
            cpt.position = Math::Vector3(3.5f, 4.5f, 0.0f);
            auto& ctext = m_World->AddComponent<ECS::TextComponent>(compass);
            ctext.text = "B1F  N\nW + E\n    S\n(1,1)";
            ctext.fontSize = 18.0f;
            ctext.textColor = Math::Vector3(0.7f, 0.7f, 1.0f);
            auto& ctag = m_World->AddComponent<ECS::TagComponent>(compass);
            ctag.tags.push_back("compass_display");
        }

        // Encounter log / message box (bottom)
        {
            ECS::Entity msgBox = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(msgBox, "Message Box");
            auto& mt = m_World->AddComponent<ECS::TransformComponent>(msgBox);
            mt.position = Math::Vector3(0.0f, 0.5f, 0.0f);
            auto& mtext = m_World->AddComponent<ECS::TextComponent>(msgBox);
            mtext.text = "You enter the dungeon...\nThe air is thick with dread.";
            mtext.fontSize = 18.0f;
            mtext.textColor = Math::Vector3(0.9f, 0.9f, 0.9f);
            auto& mtag = m_World->AddComponent<ECS::TagComponent>(msgBox);
            mtag.tags.push_back("message_box");
        }

        // Decorative pillar entities in larger rooms
        int pillarIdx = 0;
        int pillarCells[][2] = {{5,2}, {5,6}, {1,5}};
        for (auto& pc : pillarCells) {
            int px = pc[0], pz = pc[1];
            if (layout[pz][px] == 0) continue;
            f32 cx = static_cast<f32>(px) * CELL + CELL * 0.5f;
            f32 cz = static_cast<f32>(pz) * CELL + CELL * 0.5f;

            snprintf(nameBuf, sizeof(nameBuf), "Pillar_%d", pillarIdx++);
            ECS::Entity pillar = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(pillar, nameBuf);
            auto& pt = m_World->AddComponent<ECS::TransformComponent>(pillar);
            pt.position = Math::Vector3(cx - 0.8f, WALL_H * 0.5f, cz - 0.8f);
            pt.scale = Math::Vector3(0.3f, WALL_H, 0.3f);
            auto& pmat = m_World->AddComponent<ECS::MaterialComponent>(pillar);
            pmat.baseColor = Math::Vector3(0.30f, 0.28f, 0.32f);
            pmat.roughness = 0.6f;
            m_World->AddComponent<ECS::MeshComponent>(pillar, Renderer::MeshFactory::CreateCube(1.0f));
        }

        // Skybox: dark dungeon atmosphere
        {
            Renderer::SkyboxConfig skyConfig;
            skyConfig.type = Renderer::SkyboxType::Procedural;
            skyConfig.topColor = Math::Vector3(0.02f, 0.02f, 0.05f);
            skyConfig.horizonColor = Math::Vector3(0.05f, 0.04f, 0.08f);
            skyConfig.bottomColor = Math::Vector3(0.01f, 0.01f, 0.02f);
            m_RenderSystem->SetSkybox(skyConfig);
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

void EditorLayer::ImportModel(const std::string& path) {
    if (!m_World) {
        ENJIN_LOG_ERROR(Editor, "Cannot import model: no world loaded");
        m_ConsoleLog.push_back("[Error] Cannot import model: no world loaded");
        return;
    }

    Assets::ImportOptions options;
    options.scale = 1.0f;

    Assets::ImportResult result = Assets::SceneImporter::Import(path, m_World, options);

    if (result.success) {
        std::stringstream ss;
        ss << "[Info] Imported " << result.entities.size() << " entities from " << path;
        m_ConsoleLog.push_back(ss.str());
        ENJIN_LOG_INFO(Editor, "Imported %zu entities from %s", result.entities.size(), path.c_str());

        // Select the root entity
        if (result.rootEntity != ECS::INVALID_ENTITY) {
            m_SelectedEntity = result.rootEntity;
        }
    } else {
        std::stringstream ss;
        ss << "[Error] Failed to import: " << result.errorMessage;
        m_ConsoleLog.push_back(ss.str());
        ENJIN_LOG_ERROR(Editor, "Failed to import %s: %s", path.c_str(), result.errorMessage.c_str());
    }
}

bool EditorLayer::SceneHasMouseLookController() const {
    if (!m_World) return false;
    for (ECS::Entity entity : m_World->GetAllEntities()) {
        if (m_World->HasComponent<ECS::FirstPersonController>(entity) ||
            m_World->HasComponent<ECS::ThirdPersonController>(entity)) {
            return true;
        }
    }
    return false;
}

void EditorLayer::HandleViewportPicking() {
    // Don't pick if ImGui wants the mouse (hovering over a panel)
    if (WantsMouseInput()) {
        return;
    }

    if (!m_World || !m_Camera || !m_Renderer) {
        return;
    }

    // Check for double-click to focus on entity
    static f64 lastClickTime = 0.0;
    static ECS::Entity lastClickedEntity = ECS::INVALID_ENTITY;
    const f64 doubleClickTime = 0.3;  // 300ms

    // Only pick when left mouse is clicked
    if (!Input::IsMouseButtonPressed(MouseButton::Left)) {
        return;
    }

    // Get mouse position and viewport size
    Math::Vector2 mousePos = Input::GetMousePosition();
    auto extent = m_Renderer->GetSwapchainExtent();

    if (extent.width == 0 || extent.height == 0) {
        return;
    }

    // Pick entity at mouse position
    ECS::Entity picked = ScenePicker::PickEntity(
        m_World, m_Camera,
        mousePos.x, mousePos.y,
        static_cast<f32>(extent.width), static_cast<f32>(extent.height)
    );

    // Get current time for double-click detection
    f64 currentTime = ImGui::GetTime();

    if (picked != ECS::INVALID_ENTITY) {
        // Check for double-click on same entity
        if (picked == lastClickedEntity && (currentTime - lastClickTime) < doubleClickTime) {
            FocusOnEntity(picked);
            lastClickedEntity = ECS::INVALID_ENTITY;  // Reset to prevent triple-click
        } else {
            // Single click - select entity
            m_SelectedEntity = picked;
            if (m_OnEntitySelected) {
                m_OnEntitySelected(picked);
            }
            lastClickedEntity = picked;
            lastClickTime = currentTime;
            ENJIN_LOG_DEBUG(Editor, "Selected entity %llu", (unsigned long long)picked);
        }
    } else {
        // Clicked on empty space - deselect
        m_SelectedEntity = ECS::INVALID_ENTITY;
        lastClickedEntity = ECS::INVALID_ENTITY;
    }
}

void EditorLayer::DrawGizmos() {
    if (m_SelectedEntity == ECS::INVALID_ENTITY || !m_World || !m_Camera || !m_Renderer) {
        return;
    }

    // Check if entity has transform
    auto* transform = m_World->GetComponent<ECS::TransformComponent>(m_SelectedEntity);
    if (!transform) {
        return;
    }

    // Get viewport size
    auto extent = m_Renderer->GetSwapchainExtent();
    if (extent.width == 0 || extent.height == 0) {
        return;
    }

    // Set ImGuizmo to use the full screen as the viewport
    ImGuizmo::SetOrthographic(false);
    ImGuizmo::SetDrawlist(ImGui::GetBackgroundDrawList());
    ImGuizmo::SetRect(0, 0, static_cast<f32>(extent.width), static_cast<f32>(extent.height));

    // Get camera matrices (need to convert to float arrays for ImGuizmo)
    Math::Matrix4 viewMat = m_Camera->GetViewMatrix();
    Math::Matrix4 projMat = m_Camera->GetProjectionMatrix();

    // ImGuizmo expects OpenGL Y-up convention; undo the Vulkan negate baked into Perspective
    projMat.m[5] *= -1.0f;

    // Build entity transform matrix
    Math::Matrix4 entityMat = Math::Matrix4::Translation(transform->position) *
                               transform->rotation.ToMatrix() *
                               Math::Matrix4::Scale(transform->scale);

    // Determine ImGuizmo operation
    ImGuizmo::OPERATION op = ImGuizmo::TRANSLATE;
    switch (m_GizmoOperation) {
        case GizmoOperation::Translate: op = ImGuizmo::TRANSLATE; break;
        case GizmoOperation::Rotate: op = ImGuizmo::ROTATE; break;
        case GizmoOperation::Scale: op = ImGuizmo::SCALE; break;
    }

    // Determine ImGuizmo mode (local/world)
    ImGuizmo::MODE mode = (m_GizmoSpace == GizmoSpace::Local) ? ImGuizmo::LOCAL : ImGuizmo::WORLD;

    // Snap values
    f32 snapValues[3] = { 0.0f, 0.0f, 0.0f };
    if (m_UseSnap) {
        switch (m_GizmoOperation) {
            case GizmoOperation::Translate:
                snapValues[0] = snapValues[1] = snapValues[2] = m_TranslateSnap;
                break;
            case GizmoOperation::Rotate:
                snapValues[0] = snapValues[1] = snapValues[2] = m_RotateSnap;
                break;
            case GizmoOperation::Scale:
                snapValues[0] = snapValues[1] = snapValues[2] = m_ScaleSnap;
                break;
        }
    }

    // Track gizmo drag start/end for undo/redo
    bool gizmoActive = ImGuizmo::IsUsing();
    if (gizmoActive && !m_GizmoDragging) {
        // Drag just started - capture the initial transform
        m_GizmoDragging = true;
        m_GizmoStartTransform = entityMat;
    }

    // Draw and manipulate gizmo
    if (ImGuizmo::Manipulate(viewMat.m, projMat.m, op, mode, entityMat.m,
                              nullptr, m_UseSnap ? snapValues : nullptr)) {
        // Decompose the modified matrix back to transform components
        f32 translation[3], rotation[3], scale[3];
        ImGuizmo::DecomposeMatrixToComponents(entityMat.m, translation, rotation, scale);

        transform->position = Math::Vector3(translation[0], translation[1], translation[2]);
        transform->scale = Math::Vector3(scale[0], scale[1], scale[2]);

        // Convert euler angles to quaternion
        f32 rx = Math::Radians(rotation[0]);
        f32 ry = Math::Radians(rotation[1]);
        f32 rz = Math::Radians(rotation[2]);

        Math::Quaternion qx(Math::Vector3(1, 0, 0), rx);
        Math::Quaternion qy(Math::Vector3(0, 1, 0), ry);
        Math::Quaternion qz(Math::Vector3(0, 0, 1), rz);
        transform->rotation = qy * qx * qz; // YXZ order
    }

    // Gizmo drag ended - create undo command
    if (!gizmoActive && m_GizmoDragging) {
        m_GizmoDragging = false;

        // Decompose old transform from saved matrix
        f32 oldTrans[3], oldRot[3], oldScale[3];
        ImGuizmo::DecomposeMatrixToComponents(m_GizmoStartTransform.m, oldTrans, oldRot, oldScale);

        ECS::TransformComponent oldTransform;
        oldTransform.position = Math::Vector3(oldTrans[0], oldTrans[1], oldTrans[2]);
        oldTransform.scale = Math::Vector3(oldScale[0], oldScale[1], oldScale[2]);
        f32 orx = Math::Radians(oldRot[0]);
        f32 ory = Math::Radians(oldRot[1]);
        f32 orz = Math::Radians(oldRot[2]);
        oldTransform.rotation = Math::Quaternion(Math::Vector3(0,1,0), ory)
                              * Math::Quaternion(Math::Vector3(1,0,0), orx)
                              * Math::Quaternion(Math::Vector3(0,0,1), orz);

        ECS::TransformComponent newTransform = *transform;

        auto cmd = std::make_unique<TransformCommand>(m_World, m_SelectedEntity, oldTransform, newTransform);
        m_UndoRedo.Execute(std::move(cmd));
    }
}

void EditorLayer::BuildGridMesh() {
    if (!m_Renderer) return;

    f32 halfExtent = m_GridSize * 0.5f;
    f32 step = m_GridSize / static_cast<f32>(m_GridLines);
    i32 halfLines = m_GridLines / 2;

    // Count lines: (gridLines+1) per axis, minus 1 axis line each = regular lines
    // Layout: [regular lines] [X axis] [Z axis]
    u32 regularPerAxis = static_cast<u32>(m_GridLines); // lines excluding i=0
    u32 regularLines = regularPerAxis * 2;
    u32 totalVertices = (regularLines + 2) * 2; // +2 for axis lines, *2 verts per line

    m_GridRegularCount = regularLines * 2;
    m_GridAxisXStart = m_GridRegularCount;
    m_GridAxisZStart = m_GridRegularCount + 2;
    m_GridVertexCount = totalVertices;

    // 24 floats per vertex: pos(3) normal(3) uv(2) color(4) tangent(4) boneWeights(4) boneIndices(4)
    // Must match pipeline vertex stride (sizeof(f32) * 24 = 96 bytes)
    constexpr u32 FLOATS_PER_VERT = 24;
    std::vector<f32> verts(totalVertices * FLOATS_PER_VERT, 0.0f);
    u32 v = 0;

    auto addVert = [&](f32 x, f32 y, f32 z) {
        usize base = static_cast<usize>(v) * FLOATS_PER_VERT;
        verts[base + 0] = x;
        verts[base + 1] = y;
        verts[base + 2] = z;
        verts[base + 4] = 1.0f; // normal Y
        verts[base + 8]  = 1.0f; // vertex color R
        verts[base + 9]  = 1.0f; // vertex color G
        verts[base + 10] = 1.0f; // vertex color B
        verts[base + 11] = 1.0f; // vertex color A
        // boneWeights (base+16..19) and boneIndices (base+20..23) stay zero
        v++;
    };

    // Regular lines (skip i=0 which is the axis)
    // X-parallel lines
    for (i32 i = -halfLines; i <= halfLines; ++i) {
        if (i == 0) continue;
        f32 z = static_cast<f32>(i) * step;
        addVert(-halfExtent, 0.0f, z);
        addVert( halfExtent, 0.0f, z);
    }
    // Z-parallel lines
    for (i32 i = -halfLines; i <= halfLines; ++i) {
        if (i == 0) continue;
        f32 x = static_cast<f32>(i) * step;
        addVert(x, 0.0f, -halfExtent);
        addVert(x, 0.0f,  halfExtent);
    }

    // X axis line (at z=0, runs along X)
    addVert(-halfExtent, 0.0f, 0.0f);
    addVert( halfExtent, 0.0f, 0.0f);

    // Z axis line (at x=0, runs along Z)
    addVert(0.0f, 0.0f, -halfExtent);
    addVert(0.0f, 0.0f,  halfExtent);

    usize bufferSize = verts.size() * sizeof(f32);
    m_GridVertexBuffer = std::make_unique<Renderer::VulkanBuffer>(m_Renderer->GetContext());
    m_GridVertexBuffer->Create(bufferSize, Renderer::BufferUsage::Vertex, true);
    m_GridVertexBuffer->UploadData(verts.data(), bufferSize);

    m_BuiltGridSize = m_GridSize;
    m_BuiltGridLines = m_GridLines;
}

void EditorLayer::DrawGrid() {
    if (!m_ShowGrid || !m_Camera || !m_Renderer || !m_RenderSystem) {
        return;
    }

    // Rebuild mesh when grid settings change
    if (!m_GridVertexBuffer || m_GridSize != m_BuiltGridSize || m_GridLines != m_BuiltGridLines) {
        BuildGridMesh();
    }

    if (!m_GridVertexBuffer || m_GridVertexCount == 0) return;

    // Regular grid lines (gray, semi-transparent)
    m_RenderSystem->RenderGridLines(m_GridVertexBuffer.get(), m_GridRegularCount,
        0, Math::Vector3(0.22f, 0.22f, 0.22f), 0.47f);

    // X axis (red)
    m_RenderSystem->RenderGridLines(m_GridVertexBuffer.get(), 2,
        m_GridAxisXStart, Math::Vector3(0.7f, 0.24f, 0.24f), 0.8f);

    // Z axis (blue)
    m_RenderSystem->RenderGridLines(m_GridVertexBuffer.get(), 2,
        m_GridAxisZStart, Math::Vector3(0.24f, 0.24f, 0.7f), 0.8f);
}

void EditorLayer::FocusOnEntity(ECS::Entity entity) {
    if (!m_Camera || !m_CameraController || !m_World) {
        return;
    }

    // Get entity transform
    auto* transform = m_World->GetComponent<ECS::TransformComponent>(entity);
    if (!transform) {
        return;
    }

    // Calculate bounding size for appropriate distance
    f32 boundingSize = 2.0f;  // Default size

    // If entity has a mesh, estimate size from scale
    if (m_World->HasComponent<ECS::MeshComponent>(entity)) {
        boundingSize = Math::Max(transform->scale.x, Math::Max(transform->scale.y, transform->scale.z)) * 2.0f;
    }

    // Calculate camera distance based on bounding size
    f32 distance = boundingSize * 2.5f;
    distance = Math::Clamp(distance, 2.0f, 50.0f);

    // Get current camera direction (maintain viewing angle)
    Math::Vector3 cameraForward = m_Camera->GetForward();

    // Calculate new camera position
    Math::Vector3 targetPos = transform->position;
    Math::Vector3 newCameraPos = targetPos - cameraForward * distance;

    // Set camera position and update orbit target
    m_Camera->SetPosition(newCameraPos);
    m_Camera->SetLookAt(newCameraPos, targetPos, Math::Vector3(0.0f, 1.0f, 0.0f));

    // Update controller's orbit target and sync orientation
    m_CameraController->SetOrbitTarget(targetPos);
    m_CameraController->SetOrbitDistance(distance);
    m_CameraController->SyncFromCamera();

    ENJIN_LOG_INFO(Editor, "Focused on entity %llu at (%.2f, %.2f, %.2f)",
        (unsigned long long)entity, targetPos.x, targetPos.y, targetPos.z);
}

void EditorLayer::SaveScene(const std::string& path) {
    if (!m_World) {
        ENJIN_LOG_ERROR(Editor, "Cannot save scene: no world loaded");
        m_ConsoleLog.push_back("[Error] Cannot save scene: no world loaded");
        return;
    }

    Scene::SceneSerializer serializer(m_World);
    if (m_RenderSystem) {
        serializer.SetSkyboxConfig(m_RenderSystem->GetSkyboxConfig());
    }
    auto result = serializer.Save(path);

    if (result.success) {
        m_CurrentScenePath = path;
        usize entityCount = m_World->GetAllEntities().size();
        std::stringstream ss;
        ss << "[Info] Saved scene to " << path << " (" << entityCount << " entities)";
        m_ConsoleLog.push_back(ss.str());
        ENJIN_LOG_INFO(Editor, "Saved scene to %s (%zu entities)", path.c_str(), entityCount);

        // Track in recent projects
        m_EditorSettings.AddRecentProject(path);
        m_EditorSettings.Save();
    } else {
        std::stringstream ss;
        ss << "[Error] Failed to save scene: " << result.error;
        m_ConsoleLog.push_back(ss.str());
        ENJIN_LOG_ERROR(Editor, "Failed to save scene to %s: %s", path.c_str(), result.error.c_str());
    }
}

void EditorLayer::OpenScene(const std::string& path) {
    if (!m_World) {
        ENJIN_LOG_ERROR(Editor, "Cannot open scene: no world loaded");
        m_ConsoleLog.push_back("[Error] Cannot open scene: no world loaded");
        return;
    }

    Scene::SceneSerializer serializer(m_World);
    auto result = serializer.Load(path, true); // Clear existing entities

    // Apply loaded skybox config
    if (result.success && m_RenderSystem) {
        m_RenderSystem->SetSkybox(serializer.GetSkyboxConfig());
    }

    if (result.success) {
        m_CurrentScenePath = path;
        m_SelectedEntity = ECS::INVALID_ENTITY;
        m_UndoRedo.Clear();
        usize entityCount = result.entities.size();
        std::stringstream ss;
        ss << "[Info] Loaded scene from " << path << " (" << entityCount << " entities)";
        m_ConsoleLog.push_back(ss.str());
        ENJIN_LOG_INFO(Editor, "Loaded scene from %s (%zu entities)", path.c_str(), entityCount);

        // Track in recent projects
        m_EditorSettings.AddRecentProject(path);
        m_EditorSettings.Save();
    } else {
        std::stringstream ss;
        ss << "[Error] Failed to load scene: " << result.error;
        m_ConsoleLog.push_back(ss.str());
        ENJIN_LOG_ERROR(Editor, "Failed to load scene from %s: %s", path.c_str(), result.error.c_str());
    }
}


// ============================================================================
// Character Controller Inspector Drawing
// ============================================================================

void EditorLayer::DrawPlatformer2DController(ECS::Entity entity) {
    bool ctrlOpen = ImGui::CollapsingHeader("2D Platformer Controller", ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::BeginPopupContextItem("Platformer2DCtx")) {
        if (ImGui::MenuItem("Remove Component")) {
            m_World->RemoveComponent<ECS::Platformer2DController>(entity);
            ImGui::EndPopup();
            return;
        }
        ImGui::EndPopup();
    }
    if (ctrlOpen) {
        auto* ctrl = m_World->GetComponent<ECS::Platformer2DController>(entity);
        if (!ctrl) return;

        ImGui::Checkbox("Enabled", &ctrl->isEnabled);

        if (ImGui::TreeNode("Input##Platformer2D")) {
            ImGui::Checkbox("WASD", &ctrl->useWASD);
            ImGui::SameLine();
            ImGui::Checkbox("Arrow Keys", &ctrl->useArrowKeys);
            ImGui::SameLine();
            ImGui::Checkbox("Gamepad", &ctrl->useGamepad);
            if (ctrl->useGamepad) {
                ImGui::DragInt("Gamepad Index", &ctrl->gamepadIndex, 1, 0, 3);
                ImGui::DragFloat("Stick Sensitivity", &ctrl->gamepadLookSensitivity, 0.1f, 0.1f, 10.0f);
                bool connected = Input::IsGamepadConnected(ctrl->gamepadIndex);
                ImGui::TextColored(connected ? ImVec4(0.3f,0.9f,0.3f,1) : ImVec4(0.9f,0.3f,0.3f,1),
                    connected ? "Connected: %s" : "Not Connected", connected ? Input::GetGamepadName(ctrl->gamepadIndex) : "");
            }
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Movement")) {
            // Movement mode toggle
            const char* moveMode = ctrl->gridMovement ? "Grid (Tile-based)" : "Free";
            if (ImGui::BeginCombo("Movement Mode##plat2d", moveMode)) {
                if (ImGui::Selectable("Free", !ctrl->gridMovement)) ctrl->gridMovement = false;
                if (ImGui::Selectable("Grid (Tile-based)", ctrl->gridMovement)) ctrl->gridMovement = true;
                ImGui::EndCombo();
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Free: smooth continuous movement\nGrid: snap to tile cells");

            if (ctrl->gridMovement) {
                ImGui::DragFloat("Cell Size##plat2d", &ctrl->gridCellSize, 0.1f, 0.25f, 10.0f);
                ImGui::DragFloat("Grid Move Speed##plat2d", &ctrl->gridMoveSpeed, 0.5f, 1.0f, 30.0f);
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("How fast the entity moves between grid cells");
            } else {
                ImGui::DragFloat("Move Speed", &ctrl->moveSpeed, 0.1f, 0.1f, 50.0f);
                ImGui::DragFloat("Sprint Multiplier", &ctrl->sprintMultiplier, 0.1f, 1.0f, 5.0f);
                ImGui::DragFloat("Acceleration", &ctrl->acceleration, 1.0f, 1.0f, 200.0f);
                ImGui::DragFloat("Deceleration", &ctrl->deceleration, 1.0f, 1.0f, 200.0f);
                ImGui::DragFloat("Air Control", &ctrl->airControl, 0.05f, 0.0f, 1.0f);
            }
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Jumping")) {
            ImGui::DragFloat("Jump Force", &ctrl->jumpForce, 0.1f, 0.1f, 30.0f);
            ImGui::DragFloat("Gravity", &ctrl->gravity, 0.5f, 1.0f, 100.0f);
            ImGui::DragInt("Max Jumps", &ctrl->maxJumps, 1, 1, 5);
            ImGui::DragFloat("Coyote Time", &ctrl->coyoteTime, 0.01f, 0.0f, 0.5f);
            ImGui::DragFloat("Jump Buffer", &ctrl->jumpBufferTime, 0.01f, 0.0f, 0.5f);
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Wall Mechanics")) {
            ImGui::Checkbox("Wall Jump", &ctrl->enableWallJump);
            ImGui::Checkbox("Wall Slide", &ctrl->enableWallSlide);
            if (ctrl->enableWallSlide) {
                ImGui::DragFloat("Slide Speed", &ctrl->wallSlideSpeed, 0.1f, 0.1f, 10.0f);
            }
            if (ctrl->enableWallJump) {
                ImGui::DragFloat("Wall Jump Force", &ctrl->wallJumpForce, 0.1f, 0.1f, 20.0f);
            }
            ImGui::TreePop();
        }

        // State display
        ImGui::Separator();
        ImGui::TextDisabled("State:");
        ImGui::Text("Grounded: %s", ctrl->isGrounded ? "Yes" : "No");
        ImGui::Text("Jumping: %s | Falling: %s", ctrl->isJumping ? "Yes" : "No", ctrl->isFalling ? "Yes" : "No");
        ImGui::Text("Velocity: %.1f, %.1f", ctrl->velocity.x, ctrl->velocity.y);
    }
}

void EditorLayer::DrawTopDown2DController(ECS::Entity entity) {
    bool ctrlOpen = ImGui::CollapsingHeader("2D Top-Down Controller", ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::BeginPopupContextItem("TopDown2DCtx")) {
        if (ImGui::MenuItem("Remove Component")) {
            m_World->RemoveComponent<ECS::TopDown2DController>(entity);
            ImGui::EndPopup();
            return;
        }
        ImGui::EndPopup();
    }
    if (ctrlOpen) {
        auto* ctrl = m_World->GetComponent<ECS::TopDown2DController>(entity);
        if (!ctrl) return;

        ImGui::Checkbox("Enabled", &ctrl->isEnabled);

        if (ImGui::TreeNode("Input##TopDown2D")) {
            ImGui::Checkbox("WASD", &ctrl->useWASD);
            ImGui::SameLine();
            ImGui::Checkbox("Arrow Keys", &ctrl->useArrowKeys);
            ImGui::SameLine();
            ImGui::Checkbox("Gamepad", &ctrl->useGamepad);
            if (ctrl->useGamepad) {
                ImGui::DragInt("Gamepad Index", &ctrl->gamepadIndex, 1, 0, 3);
                ImGui::DragFloat("Stick Sensitivity", &ctrl->gamepadLookSensitivity, 0.1f, 0.1f, 10.0f);
                bool connected = Input::IsGamepadConnected(ctrl->gamepadIndex);
                ImGui::TextColored(connected ? ImVec4(0.3f,0.9f,0.3f,1) : ImVec4(0.9f,0.3f,0.3f,1),
                    connected ? "Connected: %s" : "Not Connected", connected ? Input::GetGamepadName(ctrl->gamepadIndex) : "");
            }
            ImGui::TreePop();
        }

        // Movement mode toggle
        {
            const char* moveMode = ctrl->gridMovement ? "Grid (Tile-based)" : "Free";
            if (ImGui::BeginCombo("Movement Mode##td2d", moveMode)) {
                if (ImGui::Selectable("Free", !ctrl->gridMovement)) ctrl->gridMovement = false;
                if (ImGui::Selectable("Grid (Tile-based)", ctrl->gridMovement)) ctrl->gridMovement = true;
                ImGui::EndCombo();
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Free: smooth continuous movement\nGrid: snap to tile cells");
        }

        if (ctrl->gridMovement) {
            ImGui::DragFloat("Cell Size##td2d", &ctrl->gridCellSize, 0.1f, 0.25f, 10.0f);
            ImGui::DragFloat("Grid Move Speed##td2d", &ctrl->gridMoveSpeed, 0.5f, 1.0f, 30.0f);
        } else {
            ImGui::DragFloat("Move Speed", &ctrl->moveSpeed, 0.1f, 0.1f, 50.0f);
            ImGui::DragFloat("Sprint Multiplier", &ctrl->sprintMultiplier, 0.1f, 1.0f, 5.0f);
            ImGui::DragFloat("Acceleration", &ctrl->acceleration, 1.0f, 1.0f, 200.0f);
            ImGui::DragFloat("Deceleration", &ctrl->deceleration, 1.0f, 1.0f, 200.0f);
        }

        ImGui::Checkbox("Rotate To Face Movement", &ctrl->rotateToFaceMovement);
        if (ctrl->rotateToFaceMovement) {
            ImGui::DragFloat("Rotation Speed", &ctrl->rotationSpeed, 10.0f, 0.0f, 1440.0f);
        }

        if (ImGui::TreeNode("Dash")) {
            ImGui::Checkbox("Enable Dash", &ctrl->enableDash);
            if (ctrl->enableDash) {
                ImGui::DragFloat("Dash Speed", &ctrl->dashSpeed, 0.5f, 5.0f, 50.0f);
                ImGui::DragFloat("Dash Duration", &ctrl->dashDuration, 0.01f, 0.05f, 1.0f);
                ImGui::DragFloat("Dash Cooldown", &ctrl->dashCooldown, 0.1f, 0.0f, 5.0f);
            }
            ImGui::TreePop();
        }
    }
}

void EditorLayer::DrawTopDown3DController(ECS::Entity entity) {
    bool ctrlOpen = ImGui::CollapsingHeader("3D Top-Down Controller", ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::BeginPopupContextItem("TopDown3DCtx")) {
        if (ImGui::MenuItem("Remove Component")) {
            m_World->RemoveComponent<ECS::TopDown3DController>(entity);
            ImGui::EndPopup();
            return;
        }
        ImGui::EndPopup();
    }
    if (ctrlOpen) {
        auto* ctrl = m_World->GetComponent<ECS::TopDown3DController>(entity);
        if (!ctrl) return;

        ImGui::Checkbox("Enabled", &ctrl->isEnabled);

        if (ImGui::TreeNode("Input##TopDown3D")) {
            ImGui::Checkbox("WASD", &ctrl->useWASD);
            ImGui::SameLine();
            ImGui::Checkbox("Arrow Keys", &ctrl->useArrowKeys);
            ImGui::SameLine();
            ImGui::Checkbox("Gamepad", &ctrl->useGamepad);
            if (ctrl->useGamepad) {
                ImGui::DragInt("Gamepad Index", &ctrl->gamepadIndex, 1, 0, 3);
                ImGui::DragFloat("Stick Sensitivity", &ctrl->gamepadLookSensitivity, 0.1f, 0.1f, 10.0f);
                bool connected = Input::IsGamepadConnected(ctrl->gamepadIndex);
                ImGui::TextColored(connected ? ImVec4(0.3f,0.9f,0.3f,1) : ImVec4(0.9f,0.3f,0.3f,1),
                    connected ? "Connected: %s" : "Not Connected", connected ? Input::GetGamepadName(ctrl->gamepadIndex) : "");
            }
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Movement")) {
            const char* moveMode = ctrl->gridMovement ? "Grid (Tile-based)" : "Free";
            if (ImGui::BeginCombo("Movement Mode##td3d", moveMode)) {
                if (ImGui::Selectable("Free", !ctrl->gridMovement)) ctrl->gridMovement = false;
                if (ImGui::Selectable("Grid (Tile-based)", ctrl->gridMovement)) ctrl->gridMovement = true;
                ImGui::EndCombo();
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Free: smooth continuous movement\nGrid: snap to tile cells");

            if (ctrl->gridMovement) {
                ImGui::DragFloat("Cell Size##td3d", &ctrl->gridCellSize, 0.1f, 0.25f, 10.0f);
                ImGui::DragFloat("Grid Move Speed##td3d", &ctrl->gridMoveSpeed, 0.5f, 1.0f, 30.0f);
            } else {
                ImGui::DragFloat("Move Speed", &ctrl->moveSpeed, 0.1f, 0.1f, 50.0f);
                ImGui::DragFloat("Sprint Multiplier", &ctrl->sprintMultiplier, 0.1f, 1.0f, 5.0f);
            }
            ImGui::Checkbox("Rotate To Face Movement", &ctrl->rotateToFaceMovement);
            ImGui::DragFloat("Rotation Speed", &ctrl->rotationSpeed, 10.0f, 0.0f, 1440.0f);
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Camera")) {
            ImGui::DragFloat("Camera Angle", &ctrl->cameraAngle, 1.0f, 0.0f, 90.0f);
            ImGui::DragFloat("Camera Distance", &ctrl->cameraDistance, 0.5f, 5.0f, 50.0f);
            ImGui::DragFloat("Camera Height", &ctrl->cameraHeight, 0.5f, 1.0f, 30.0f);
            ImGui::Checkbox("Lock Camera To Player", &ctrl->lockCameraToPlayer);
            ImGui::TreePop();
        }

        ImGui::Checkbox("Enable Click-To-Move", &ctrl->enableClickToMove);
        ImGui::Checkbox("Enable Dash", &ctrl->enableDash);
    }
}

void EditorLayer::DrawThirdPersonController(ECS::Entity entity) {
    bool ctrlOpen = ImGui::CollapsingHeader("3D Third Person Controller", ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::BeginPopupContextItem("ThirdPersonCtx")) {
        if (ImGui::MenuItem("Remove Component")) {
            m_World->RemoveComponent<ECS::ThirdPersonController>(entity);
            ImGui::EndPopup();
            return;
        }
        ImGui::EndPopup();
    }
    if (ctrlOpen) {
        auto* ctrl = m_World->GetComponent<ECS::ThirdPersonController>(entity);
        if (!ctrl) return;

        ImGui::Checkbox("Enabled", &ctrl->isEnabled);

        if (ImGui::TreeNode("Input##ThirdPerson")) {
            ImGui::Checkbox("WASD", &ctrl->useWASD);
            ImGui::SameLine();
            ImGui::Checkbox("Arrow Keys", &ctrl->useArrowKeys);
            ImGui::SameLine();
            ImGui::Checkbox("Gamepad", &ctrl->useGamepad);
            if (ctrl->useGamepad) {
                ImGui::DragInt("Gamepad Index", &ctrl->gamepadIndex, 1, 0, 3);
                ImGui::DragFloat("Stick Sensitivity", &ctrl->gamepadLookSensitivity, 0.1f, 0.1f, 10.0f);
                bool connected = Input::IsGamepadConnected(ctrl->gamepadIndex);
                ImGui::TextColored(connected ? ImVec4(0.3f,0.9f,0.3f,1) : ImVec4(0.9f,0.3f,0.3f,1),
                    connected ? "Connected: %s" : "Not Connected", connected ? Input::GetGamepadName(ctrl->gamepadIndex) : "");
            }
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Movement")) {
            const char* moveMode = ctrl->gridMovement ? "Grid (Tile-based)" : "Free";
            if (ImGui::BeginCombo("Movement Mode##tps", moveMode)) {
                if (ImGui::Selectable("Free", !ctrl->gridMovement)) ctrl->gridMovement = false;
                if (ImGui::Selectable("Grid (Tile-based)", ctrl->gridMovement)) ctrl->gridMovement = true;
                ImGui::EndCombo();
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Free: smooth continuous movement\nGrid: snap to tile cells");

            if (ctrl->gridMovement) {
                ImGui::DragFloat("Cell Size##tps", &ctrl->gridCellSize, 0.1f, 0.25f, 10.0f);
                ImGui::DragFloat("Grid Move Speed##tps", &ctrl->gridMoveSpeed, 0.5f, 1.0f, 30.0f);
            } else {
                ImGui::DragFloat("Move Speed", &ctrl->moveSpeed, 0.1f, 0.1f, 50.0f);
                ImGui::DragFloat("Sprint Multiplier", &ctrl->sprintMultiplier, 0.1f, 1.0f, 5.0f);
            }
            ImGui::DragFloat("Jump Force", &ctrl->jumpForce, 0.1f, 1.0f, 30.0f);
            ImGui::DragFloat("Gravity", &ctrl->gravity, 0.5f, 1.0f, 100.0f);
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Rotation")) {
            ImGui::Checkbox("Rotate To Face Movement", &ctrl->rotateToFaceMovement);
            ImGui::Checkbox("Rotate To Face Camera", &ctrl->rotateToFaceCamera);
            ImGui::DragFloat("Rotation Speed", &ctrl->rotationSpeed, 10.0f, 0.0f, 1440.0f);
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Camera")) {
            ImGui::Checkbox("Disable Mouse Look##tps", &ctrl->disableMouseLook);
            ImGui::DragFloat("Distance", &ctrl->cameraDistance, 0.1f, ctrl->cameraMinDistance, ctrl->cameraMaxDistance);
            ImGui::DragFloat("Height", &ctrl->cameraHeight, 0.1f, 0.0f, 10.0f);
            ImGui::DragFloat("Sensitivity", &ctrl->cameraSensitivity, 0.1f, 0.1f, 10.0f);
            ImGui::DragFloat("Lerp Speed", &ctrl->cameraLerpSpeed, 0.5f, 1.0f, 50.0f);
            ImGui::DragFloat("Pitch", &ctrl->cameraPitch, 1.0f, ctrl->cameraMinPitch, ctrl->cameraMaxPitch);
            ImGui::Checkbox("Enable Collision", &ctrl->enableCameraCollision);
            ImGui::TreePop();
        }

        ImGui::Checkbox("Enable Lock-On", &ctrl->enableLockOn);

        ImGui::Separator();
        ImGui::TextDisabled("State:");
        ImGui::Text("Grounded: %s | Sprinting: %s", ctrl->isGrounded ? "Yes" : "No", ctrl->isSprinting ? "Yes" : "No");
    }
}

void EditorLayer::DrawFirstPersonController(ECS::Entity entity) {
    bool ctrlOpen = ImGui::CollapsingHeader("3D First Person Controller", ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::BeginPopupContextItem("FirstPersonCtx")) {
        if (ImGui::MenuItem("Remove Component")) {
            m_World->RemoveComponent<ECS::FirstPersonController>(entity);
            ImGui::EndPopup();
            return;
        }
        ImGui::EndPopup();
    }
    if (ctrlOpen) {
        auto* ctrl = m_World->GetComponent<ECS::FirstPersonController>(entity);
        if (!ctrl) return;

        ImGui::Checkbox("Enabled", &ctrl->isEnabled);

        if (ImGui::TreeNode("Input##FirstPerson")) {
            ImGui::Checkbox("WASD", &ctrl->useWASD);
            ImGui::SameLine();
            ImGui::Checkbox("Arrow Keys", &ctrl->useArrowKeys);
            ImGui::SameLine();
            ImGui::Checkbox("Gamepad", &ctrl->useGamepad);
            if (ctrl->useGamepad) {
                ImGui::DragInt("Gamepad Index", &ctrl->gamepadIndex, 1, 0, 3);
                ImGui::DragFloat("Stick Sensitivity", &ctrl->gamepadLookSensitivity, 0.1f, 0.1f, 10.0f);
                bool connected = Input::IsGamepadConnected(ctrl->gamepadIndex);
                ImGui::TextColored(connected ? ImVec4(0.3f,0.9f,0.3f,1) : ImVec4(0.9f,0.3f,0.3f,1),
                    connected ? "Connected: %s" : "Not Connected", connected ? Input::GetGamepadName(ctrl->gamepadIndex) : "");
            }
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Movement")) {
            const char* moveMode = ctrl->gridMovement ? "Grid (Tile-based)" : "Free";
            if (ImGui::BeginCombo("Movement Mode##fps", moveMode)) {
                if (ImGui::Selectable("Free", !ctrl->gridMovement)) ctrl->gridMovement = false;
                if (ImGui::Selectable("Grid (Tile-based)", ctrl->gridMovement)) ctrl->gridMovement = true;
                ImGui::EndCombo();
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Free: smooth continuous movement\nGrid: snap to tile cells (dungeon crawler style)");

            if (ctrl->gridMovement) {
                ImGui::DragFloat("Cell Size##fps", &ctrl->gridCellSize, 0.1f, 0.25f, 10.0f);
                ImGui::DragFloat("Grid Move Speed##fps", &ctrl->gridMoveSpeed, 0.5f, 1.0f, 30.0f);
            } else {
                ImGui::DragFloat("Move Speed", &ctrl->moveSpeed, 0.1f, 0.1f, 50.0f);
                ImGui::DragFloat("Sprint Multiplier", &ctrl->sprintMultiplier, 0.1f, 1.0f, 5.0f);
            }
            ImGui::DragFloat("Jump Force", &ctrl->jumpForce, 0.1f, 1.0f, 30.0f);
            ImGui::DragFloat("Gravity", &ctrl->gravity, 0.5f, 1.0f, 100.0f);
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Mouse Look")) {
            ImGui::Checkbox("Disable Mouse Look##fps", &ctrl->disableMouseLook);
            ImGui::DragFloat("Sensitivity", &ctrl->mouseSensitivity, 0.1f, 0.1f, 10.0f);
            ImGui::Checkbox("Invert Y", &ctrl->invertY);
            ImGui::DragFloat("Pitch", &ctrl->pitch, 1.0f, ctrl->minPitch, ctrl->maxPitch);
            ImGui::DragFloat("Yaw", &ctrl->yaw, 1.0f, -180.0f, 180.0f);
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Crouching")) {
            ImGui::Checkbox("Enable Crouch", &ctrl->enableCrouch);
            ImGui::DragFloat("Standing Height", &ctrl->standingHeight, 0.1f, 0.5f, 3.0f);
            ImGui::DragFloat("Crouching Height", &ctrl->crouchingHeight, 0.1f, 0.3f, 2.0f);
            ImGui::DragFloat("Crouch Speed Mult", &ctrl->crouchSpeed, 0.1f, 0.1f, 1.0f);
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Head Bob")) {
            ImGui::Checkbox("Enable Head Bob", &ctrl->enableHeadBob);
            ImGui::DragFloat("Frequency", &ctrl->headBobFrequency, 0.5f, 1.0f, 20.0f);
            ImGui::DragFloat("Amplitude", &ctrl->headBobAmplitude, 0.01f, 0.0f, 0.2f);
            ImGui::TreePop();
        }

        ImGui::Separator();
        ImGui::TextDisabled("State:");
        ImGui::Text("Grounded: %s | Crouching: %s | Sprinting: %s",
            ctrl->isGrounded ? "Yes" : "No",
            ctrl->isCrouching ? "Yes" : "No",
            ctrl->isSprinting ? "Yes" : "No");
    }
}

// ============================================================================
// Gameplay Component Inspector Drawing
// ============================================================================

void EditorLayer::DrawHealthComponent(ECS::Entity entity) {
    bool healthOpen = ImGui::CollapsingHeader("Health", ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::BeginPopupContextItem("HealthCtx")) {
        if (ImGui::MenuItem("Remove Component")) {
            m_World->RemoveComponent<ECS::HealthComponent>(entity);
            ImGui::EndPopup();
            return;
        }
        ImGui::EndPopup();
    }
    if (healthOpen) {
        auto* health = m_World->GetComponent<ECS::HealthComponent>(entity);
        if (!health) return;

        // Health bar
        f32 healthPercent = health->GetHealthPercent();
        ImVec4 healthColor = healthPercent > 0.5f ? ImVec4(0.2f, 0.8f, 0.2f, 1.0f) :
                             healthPercent > 0.25f ? ImVec4(0.8f, 0.8f, 0.2f, 1.0f) :
                                                     ImVec4(0.8f, 0.2f, 0.2f, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, healthColor);
        ImGui::ProgressBar(healthPercent, ImVec2(-1, 0),
            (std::to_string((int)health->currentHealth) + " / " + std::to_string((int)health->maxHealth)).c_str());
        ImGui::PopStyleColor();

        ImGui::DragFloat("Max Health", &health->maxHealth, 1.0f, 1.0f, 10000.0f);
        ImGui::DragFloat("Current Health", &health->currentHealth, 1.0f, 0.0f, health->maxHealth);

        if (ImGui::TreeNode("Regeneration")) {
            ImGui::DragFloat("Regen Rate (HP/s)", &health->regenRate, 0.5f, 0.0f, 100.0f);
            ImGui::DragFloat("Regen Delay", &health->regenDelay, 0.1f, 0.0f, 10.0f);
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Shield")) {
            ImGui::DragFloat("Max Shield", &health->maxShield, 1.0f, 0.0f, 1000.0f);
            ImGui::DragFloat("Current Shield", &health->currentShield, 1.0f, 0.0f, health->maxShield);
            ImGui::DragFloat("Shield Regen", &health->shieldRegenRate, 0.5f, 0.0f, 100.0f);
            ImGui::TreePop();
        }

        ImGui::Checkbox("Invulnerable", &health->isInvulnerable);
        ImGui::DragFloat("Invuln Time After Hit", &health->invulnerabilityTime, 0.1f, 0.0f, 5.0f);

        ImGui::Separator();
        ImGui::Text("Status: %s", health->isDead ? "DEAD" : "Alive");
    }
}

void EditorLayer::DrawRigidbodyComponent(ECS::Entity entity) {
    bool rbOpen = ImGui::CollapsingHeader("Rigidbody", ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::BeginPopupContextItem("RigidbodyCtx")) {
        if (ImGui::MenuItem("Remove Component")) {
            m_World->RemoveComponent<ECS::RigidbodyComponent>(entity);
            ImGui::EndPopup();
            return;
        }
        ImGui::EndPopup();
    }
    if (rbOpen) {
        auto* rb = m_World->GetComponent<ECS::RigidbodyComponent>(entity);
        if (!rb) return;

        // Body type
        const char* bodyTypes[] = { "Dynamic", "Kinematic", "Static" };
        int currentType = static_cast<int>(rb->bodyType);
        if (ImGui::Combo("Body Type", &currentType, bodyTypes, 3)) {
            rb->bodyType = static_cast<ECS::RigidbodyComponent::BodyType>(currentType);
        }

        ImGui::DragFloat("Mass", &rb->mass, 0.1f, 0.001f, 1000.0f);
        ImGui::DragFloat("Drag", &rb->drag, 0.01f, 0.0f, 10.0f);
        ImGui::DragFloat("Angular Drag", &rb->angularDrag, 0.01f, 0.0f, 10.0f);

        ImGui::Checkbox("Use Gravity", &rb->useGravity);
        if (rb->useGravity) {
            ImGui::DragFloat("Gravity Scale", &rb->gravityScale, 0.1f, -10.0f, 10.0f);
        }

        if (ImGui::TreeNode("Constraints")) {
            ImGui::Checkbox("Freeze X", &rb->freezePositionX);
            ImGui::SameLine();
            ImGui::Checkbox("Freeze Y", &rb->freezePositionY);
            ImGui::SameLine();
            ImGui::Checkbox("Freeze Z", &rb->freezePositionZ);

            ImGui::Checkbox("Freeze Rot X", &rb->freezeRotationX);
            ImGui::SameLine();
            ImGui::Checkbox("Freeze Rot Y", &rb->freezeRotationY);
            ImGui::SameLine();
            ImGui::Checkbox("Freeze Rot Z", &rb->freezeRotationZ);
            ImGui::TreePop();
        }

        ImGui::Separator();
        ImGui::Text("Velocity: %.2f, %.2f, %.2f", rb->velocity.x, rb->velocity.y, rb->velocity.z);
        ImGui::Text("Grounded: %s | Sleeping: %s", rb->isGrounded ? "Yes" : "No", rb->isSleeping ? "Yes" : "No");
    }
}

void EditorLayer::DrawBoxColliderComponent(ECS::Entity entity) {
    bool boxOpen = ImGui::CollapsingHeader("Box Collider", ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::BeginPopupContextItem("BoxColliderCtx")) {
        if (ImGui::MenuItem("Remove Component")) {
            m_World->RemoveComponent<ECS::BoxColliderComponent>(entity);
            ImGui::EndPopup();
            return;
        }
        ImGui::EndPopup();
    }
    if (boxOpen) {
        auto* col = m_World->GetComponent<ECS::BoxColliderComponent>(entity);
        if (!col) return;

        f32 center[3] = { col->center.x, col->center.y, col->center.z };
        if (ImGui::DragFloat3("Center", center, 0.1f)) {
            col->center = Math::Vector3(center[0], center[1], center[2]);
        }

        f32 size[3] = { col->size.x, col->size.y, col->size.z };
        if (ImGui::DragFloat3("Size", size, 0.1f, 0.001f, 1000.0f)) {
            col->size = Math::Vector3(size[0], size[1], size[2]);
        }

        ImGui::Checkbox("Is Trigger", &col->isTrigger);

        if (ImGui::TreeNode("Physics Material")) {
            ImGui::DragFloat("Friction", &col->friction, 0.05f, 0.0f, 1.0f);
            ImGui::DragFloat("Bounciness", &col->bounciness, 0.05f, 0.0f, 1.0f);
            ImGui::TreePop();
        }
    }
}

void EditorLayer::DrawAudioSourceComponent(ECS::Entity entity) {
    bool audioOpen = ImGui::CollapsingHeader("Audio Source", ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::BeginPopupContextItem("AudioSourceCtx")) {
        if (ImGui::MenuItem("Remove Component")) {
            m_World->RemoveComponent<ECS::AudioSourceComponent>(entity);
            ImGui::EndPopup();
            return;
        }
        ImGui::EndPopup();
    }
    if (audioOpen) {
        auto* audio = m_World->GetComponent<ECS::AudioSourceComponent>(entity);
        if (!audio) return;

        // Clip path (would need file browser in real implementation)
        char pathBuffer[256];
        strncpy(pathBuffer, audio->clipPath.c_str(), sizeof(pathBuffer) - 1);
        pathBuffer[sizeof(pathBuffer) - 1] = '\0';
        if (ImGui::InputText("Clip Path", pathBuffer, sizeof(pathBuffer))) {
            audio->clipPath = pathBuffer;
        }

        ImGui::DragFloat("Volume", &audio->volume, 0.01f, 0.0f, 1.0f);
        ImGui::DragFloat("Pitch", &audio->pitch, 0.01f, 0.1f, 3.0f);

        ImGui::Checkbox("Play On Awake", &audio->playOnAwake);
        ImGui::Checkbox("Loop", &audio->loop);
        ImGui::Checkbox("3D Sound", &audio->is3D);

        if (audio->is3D) {
            ImGui::DragFloat("Spatial Blend", &audio->spatialBlend, 0.05f, 0.0f, 1.0f);
            ImGui::DragFloat("Min Distance", &audio->minDistance, 0.5f, 0.1f, 100.0f);
            ImGui::DragFloat("Max Distance", &audio->maxDistance, 5.0f, audio->minDistance, 1000.0f);
        }

        ImGui::DragInt("Priority", &audio->priority, 1, 0, 255);

        ImGui::Separator();
        ImGui::Text("Playing: %s", audio->isPlaying ? "Yes" : "No");

        if (ImGui::Button("Play")) {
            audio->isPlaying = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Stop")) {
            audio->isPlaying = false;
            audio->playbackPosition = 0.0f;
        }
    }
}

void EditorLayer::DrawSprite2DComponent(ECS::Entity entity) {
    bool spriteOpen = ImGui::CollapsingHeader("Sprite 2D", ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::BeginPopupContextItem("Sprite2DCtx")) {
        if (ImGui::MenuItem("Remove Component")) {
            m_World->RemoveComponent<ECS::Sprite2DComponent>(entity);
            ImGui::EndPopup();
            return;
        }
        ImGui::EndPopup();
    }
    if (spriteOpen) {
        auto* sprite = m_World->GetComponent<ECS::Sprite2DComponent>(entity);
        if (!sprite) return;

        // Texture path
        char pathBuffer[256];
        strncpy(pathBuffer, sprite->texturePath.c_str(), sizeof(pathBuffer) - 1);
        pathBuffer[sizeof(pathBuffer) - 1] = '\0';
        if (ImGui::InputText("Texture Path", pathBuffer, sizeof(pathBuffer))) {
            sprite->texturePath = pathBuffer;
        }

        // Source rectangle (for sprite sheets)
        ImGui::Text("Source Rectangle:");
        ImGui::DragFloat("Src X", &sprite->srcX, 1.0f, 0.0f, 4096.0f);
        ImGui::DragFloat("Src Y", &sprite->srcY, 1.0f, 0.0f, 4096.0f);
        ImGui::DragFloat("Src Width", &sprite->srcWidth, 1.0f, 0.0f, 4096.0f);
        ImGui::DragFloat("Src Height", &sprite->srcHeight, 1.0f, 0.0f, 4096.0f);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Set to 0 to use full texture");
        }

        // Size
        f32 size[2] = { sprite->size.x, sprite->size.y };
        if (ImGui::DragFloat2("Size", size, 0.1f, 0.1f, 100.0f)) {
            sprite->size = Math::Vector2(size[0], size[1]);
        }

        // Pivot
        f32 pivot[2] = { sprite->pivot.x, sprite->pivot.y };
        if (ImGui::DragFloat2("Pivot", pivot, 0.01f, 0.0f, 1.0f)) {
            sprite->pivot = Math::Vector2(pivot[0], pivot[1]);
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("0,0 = top-left, 0.5,0.5 = center, 1,1 = bottom-right");
        }

        // Tint
        f32 tint[3] = { sprite->tint.x, sprite->tint.y, sprite->tint.z };
        if (ImGui::ColorEdit3("Tint", tint)) {
            sprite->tint = Math::Vector3(tint[0], tint[1], tint[2]);
        }

        ImGui::DragFloat("Alpha", &sprite->alpha, 0.01f, 0.0f, 1.0f);

        // Sorting
        ImGui::DragInt("Sorting Layer", &sprite->sortingLayer, 1, -100, 100);
        ImGui::DragInt("Order in Layer", &sprite->orderInLayer, 1, -1000, 1000);

        // Flip
        ImGui::Checkbox("Flip X", &sprite->flipX);
        ImGui::SameLine();
        ImGui::Checkbox("Flip Y", &sprite->flipY);

        ImGui::Checkbox("Visible", &sprite->visible);
    }
}

void EditorLayer::DrawAnimatedSprite2DComponent(ECS::Entity entity) {
    bool animOpen = ImGui::CollapsingHeader("Animated Sprite 2D", ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::BeginPopupContextItem("AnimSprite2DCtx")) {
        if (ImGui::MenuItem("Remove Component")) {
            m_World->RemoveComponent<ECS::AnimatedSprite2DComponent>(entity);
            ImGui::EndPopup();
            return;
        }
        ImGui::EndPopup();
    }
    if (animOpen) {
        auto* anim = m_World->GetComponent<ECS::AnimatedSprite2DComponent>(entity);
        if (!anim) return;

        ImGui::Text("Frames: %zu", anim->frames.size());
        ImGui::Text("Current Frame: %u", anim->currentFrame);
        ImGui::Text("Frame Timer: %.2f", anim->frameTimer);

        ImGui::Checkbox("Playing", &anim->playing);
        ImGui::Checkbox("Loop", &anim->loop);
        ImGui::DragFloat("Playback Speed", &anim->playbackSpeed, 0.1f, 0.1f, 10.0f);

        if (anim->animationComplete) {
            ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "Animation Complete");
        }

        // Frame editor
        if (ImGui::TreeNode("Frames")) {
            for (usize i = 0; i < anim->frames.size(); ++i) {
                ImGui::PushID(static_cast<int>(i));
                if (ImGui::TreeNode("Frame", "Frame %zu", i)) {
                    ImGui::DragFloat("Src X", &anim->frames[i].srcX, 1.0f);
                    ImGui::DragFloat("Src Y", &anim->frames[i].srcY, 1.0f);
                    ImGui::DragFloat("Duration", &anim->frames[i].duration, 0.01f, 0.01f, 2.0f);
                    ImGui::TreePop();
                }
                ImGui::PopID();
            }

            if (ImGui::Button("Add Frame")) {
                ECS::AnimatedSprite2DComponent::Frame frame;
                frame.srcX = 0;
                frame.srcY = 0;
                frame.duration = 0.1f;
                anim->frames.push_back(frame);
            }
            ImGui::TreePop();
        }
    }
}

void EditorLayer::DrawTilemapComponent(ECS::Entity entity) {
    bool tilemapOpen = ImGui::CollapsingHeader("Tilemap", ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::BeginPopupContextItem("TilemapCtx")) {
        if (ImGui::MenuItem("Remove Component")) {
            m_World->RemoveComponent<ECS::TilemapComponent>(entity);
            ImGui::EndPopup();
            return;
        }
        ImGui::EndPopup();
    }
    if (tilemapOpen) {
        auto* tilemap = m_World->GetComponent<ECS::TilemapComponent>(entity);
        if (!tilemap) return;

        // Tileset path
        char pathBuffer[256];
        strncpy(pathBuffer, tilemap->tilesetPath.c_str(), sizeof(pathBuffer) - 1);
        pathBuffer[sizeof(pathBuffer) - 1] = '\0';
        if (ImGui::InputText("Tileset Path", pathBuffer, sizeof(pathBuffer))) {
            tilemap->tilesetPath = pathBuffer;
        }

        // Tile size
        ImGui::DragFloat("Tile Width (px)", &tilemap->tileWidth, 1.0f, 1.0f, 256.0f);
        ImGui::DragFloat("Tile Height (px)", &tilemap->tileHeight, 1.0f, 1.0f, 256.0f);

        int cols = static_cast<int>(tilemap->tilesetColumns);
        if (ImGui::DragInt("Tileset Columns", &cols, 1, 1, 64)) {
            tilemap->tilesetColumns = static_cast<u32>(cols);
        }

        // World scale
        ImGui::DragFloat("World Tile Width", &tilemap->worldTileWidth, 0.1f, 0.1f, 10.0f);
        ImGui::DragFloat("World Tile Height", &tilemap->worldTileHeight, 0.1f, 0.1f, 10.0f);

        // Map size
        int w = static_cast<int>(tilemap->width);
        int h = static_cast<int>(tilemap->height);
        bool sizeChanged = false;
        if (ImGui::DragInt("Map Width", &w, 1, 1, 256)) {
            tilemap->width = static_cast<u32>(w);
            sizeChanged = true;
        }
        if (ImGui::DragInt("Map Height", &h, 1, 1, 256)) {
            tilemap->height = static_cast<u32>(h);
            sizeChanged = true;
        }
        if (sizeChanged) {
            tilemap->tiles.resize(tilemap->width * tilemap->height, -1);
        }

        ImGui::Checkbox("Has Collision", &tilemap->hasCollision);

        ImGui::Text("Total Tiles: %zu", tilemap->tiles.size());
    }
}

void EditorLayer::DrawStateMachineComponent(ECS::Entity entity) {
    bool smOpen = ImGui::CollapsingHeader("State Machine", ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::BeginPopupContextItem("StateMachineCtx")) {
        if (ImGui::MenuItem("Remove Component")) {
            m_World->RemoveComponent<ECS::StateMachineComponent>(entity);
            ImGui::EndPopup();
            return;
        }
        ImGui::EndPopup();
    }
    if (smOpen) {
        auto* sm = m_World->GetComponent<ECS::StateMachineComponent>(entity);
        if (!sm) return;

        // Current state (editable)
        char stateBuffer[64];
        strncpy(stateBuffer, sm->currentState.c_str(), sizeof(stateBuffer) - 1);
        stateBuffer[sizeof(stateBuffer) - 1] = '\0';
        if (ImGui::InputText("Current State", stateBuffer, sizeof(stateBuffer))) {
            sm->SetState(stateBuffer);
        }

        ImGui::Text("Previous State: %s", sm->previousState.empty() ? "(none)" : sm->previousState.c_str());
        ImGui::Text("Time in State: %.2f s", sm->stateTimer);

        if (sm->stateJustChanged) {
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.3f, 1.0f), "State just changed!");
        }

        // Parameters
        if (ImGui::TreeNode("Float Parameters")) {
            for (auto& p : sm->floatParams) {
                ImGui::DragFloat(p.first.c_str(), &p.second, 0.1f);
            }
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Bool Parameters")) {
            for (auto& p : sm->boolParams) {
                ImGui::Checkbox(p.first.c_str(), &p.second);
            }
            ImGui::TreePop();
        }
    }
}

void EditorLayer::DrawDialogueComponent(ECS::Entity entity) {
    bool dlgOpen = ImGui::CollapsingHeader("Dialogue", ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::BeginPopupContextItem("DialogueCtx")) {
        if (ImGui::MenuItem("Remove Component")) {
            m_World->RemoveComponent<ECS::DialogueComponent>(entity);
            ImGui::EndPopup();
            return;
        }
        ImGui::EndPopup();
    }
    if (dlgOpen) {
        auto* dialogue = m_World->GetComponent<ECS::DialogueComponent>(entity);
        if (!dialogue) return;

        // Speaker name
        char nameBuffer[64];
        strncpy(nameBuffer, dialogue->speakerName.c_str(), sizeof(nameBuffer) - 1);
        nameBuffer[sizeof(nameBuffer) - 1] = '\0';
        if (ImGui::InputText("Speaker Name", nameBuffer, sizeof(nameBuffer))) {
            dialogue->speakerName = nameBuffer;
        }

        // Portrait path
        char portraitBuffer[256];
        strncpy(portraitBuffer, dialogue->portraitPath.c_str(), sizeof(portraitBuffer) - 1);
        portraitBuffer[sizeof(portraitBuffer) - 1] = '\0';
        if (ImGui::InputText("Portrait Path", portraitBuffer, sizeof(portraitBuffer))) {
            dialogue->portraitPath = portraitBuffer;
        }

        // Typewriter settings
        ImGui::DragFloat("Char Delay", &dialogue->charDelay, 0.01f, 0.01f, 0.5f);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Seconds between characters (typewriter effect)");
        }
        ImGui::Checkbox("Play Type Sound", &dialogue->playTypeSound);

        // Status
        ImGui::Separator();
        ImGui::Text("Lines: %zu", dialogue->dialogueLines.size());
        ImGui::Text("Current Line: %u", dialogue->currentLine);
        ImGui::Text("Typing: %s", dialogue->isTyping ? "Yes" : "No");
        ImGui::Text("Waiting for Input: %s", dialogue->waitingForInput ? "Yes" : "No");

        if (dialogue->IsComplete()) {
            ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "Dialogue Complete");
        }

        // Preview current text
        if (!dialogue->dialogueLines.empty()) {
            ImGui::Separator();
            ImGui::Text("Preview:");
            ImGui::TextWrapped("%s", dialogue->GetVisibleText().c_str());
        }

        // Dialogue lines editor
        if (ImGui::TreeNode("Dialogue Lines")) {
            for (usize i = 0; i < dialogue->dialogueLines.size(); ++i) {
                ImGui::PushID(static_cast<int>(i));
                char lineBuffer[512];
                strncpy(lineBuffer, dialogue->dialogueLines[i].c_str(), sizeof(lineBuffer) - 1);
                lineBuffer[sizeof(lineBuffer) - 1] = '\0';
                if (ImGui::InputTextMultiline("##line", lineBuffer, sizeof(lineBuffer), ImVec2(-1, 60))) {
                    dialogue->dialogueLines[i] = lineBuffer;
                }
                ImGui::PopID();
            }

            if (ImGui::Button("Add Line")) {
                dialogue->dialogueLines.push_back("");
            }
            ImGui::TreePop();
        }

        // Choices editor
        if (ImGui::TreeNode("Choices")) {
            for (usize i = 0; i < dialogue->choices.size(); ++i) {
                ImGui::PushID(static_cast<int>(i));
                if (ImGui::TreeNode("Choice", "Choice %zu", i)) {
                    char choiceBuffer[256];
                    strncpy(choiceBuffer, dialogue->choices[i].text.c_str(), sizeof(choiceBuffer) - 1);
                    choiceBuffer[sizeof(choiceBuffer) - 1] = '\0';
                    if (ImGui::InputText("Text", choiceBuffer, sizeof(choiceBuffer))) {
                        dialogue->choices[i].text = choiceBuffer;
                    }

                    char nextBuffer[64];
                    strncpy(nextBuffer, dialogue->choices[i].nextDialogueId.c_str(), sizeof(nextBuffer) - 1);
                    nextBuffer[sizeof(nextBuffer) - 1] = '\0';
                    if (ImGui::InputText("Next Dialogue ID", nextBuffer, sizeof(nextBuffer))) {
                        dialogue->choices[i].nextDialogueId = nextBuffer;
                    }
                    ImGui::TreePop();
                }
                ImGui::PopID();
            }

            if (ImGui::Button("Add Choice")) {
                ECS::DialogueComponent::Choice choice;
                choice.text = "Choice";
                dialogue->choices.push_back(choice);
            }
            ImGui::TreePop();
        }
    }
}

void EditorLayer::DrawSphereColliderComponent(ECS::Entity entity) {
    if (ImGui::CollapsingHeader("Sphere Collider", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto* col = m_World->GetComponent<ECS::SphereColliderComponent>(entity);
        if (!col) return;

        f32 center[3] = { col->center.x, col->center.y, col->center.z };
        if (ImGui::DragFloat3("Center", center, 0.1f)) {
            col->center = Math::Vector3(center[0], center[1], center[2]);
        }

        ImGui::DragFloat("Radius", &col->radius, 0.05f, 0.001f, 1000.0f);
        ImGui::Checkbox("Is Trigger", &col->isTrigger);

        if (ImGui::TreeNode("Physics Material")) {
            ImGui::DragFloat("Friction", &col->friction, 0.05f, 0.0f, 1.0f);
            ImGui::DragFloat("Bounciness", &col->bounciness, 0.05f, 0.0f, 1.0f);
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Collision Filtering")) {
            int layer = static_cast<int>(col->layer);
            if (ImGui::InputInt("Layer", &layer)) {
                col->layer = static_cast<u32>(layer);
            }
            ImGui::TreePop();
        }

        if (ImGui::BeginPopupContextItem("SphereColliderContext")) {
            if (ImGui::MenuItem("Remove Component")) {
                m_World->RemoveComponent<ECS::SphereColliderComponent>(entity);
            }
            ImGui::EndPopup();
        }
    }
}

void EditorLayer::DrawCapsuleColliderComponent(ECS::Entity entity) {
    if (ImGui::CollapsingHeader("Capsule Collider", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto* col = m_World->GetComponent<ECS::CapsuleColliderComponent>(entity);
        if (!col) return;

        f32 center[3] = { col->center.x, col->center.y, col->center.z };
        if (ImGui::DragFloat3("Center", center, 0.1f)) {
            col->center = Math::Vector3(center[0], center[1], center[2]);
        }

        ImGui::DragFloat("Radius", &col->radius, 0.05f, 0.001f, 100.0f);
        ImGui::DragFloat("Height", &col->height, 0.1f, 0.001f, 100.0f);

        const char* directions[] = { "X", "Y", "Z" };
        int dir = static_cast<int>(col->direction);
        if (ImGui::Combo("Direction", &dir, directions, 3)) {
            col->direction = static_cast<ECS::CapsuleColliderComponent::Direction>(dir);
        }

        ImGui::Checkbox("Is Trigger", &col->isTrigger);

        if (ImGui::TreeNode("Physics Material")) {
            ImGui::DragFloat("Friction", &col->friction, 0.05f, 0.0f, 1.0f);
            ImGui::DragFloat("Bounciness", &col->bounciness, 0.05f, 0.0f, 1.0f);
            ImGui::TreePop();
        }

        if (ImGui::BeginPopupContextItem("CapsuleColliderContext")) {
            if (ImGui::MenuItem("Remove Component")) {
                m_World->RemoveComponent<ECS::CapsuleColliderComponent>(entity);
            }
            ImGui::EndPopup();
        }
    }
}

void EditorLayer::DrawTriggerZoneComponent(ECS::Entity entity) {
    if (ImGui::CollapsingHeader("Trigger Zone", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto* zone = m_World->GetComponent<ECS::TriggerZoneComponent>(entity);
        if (!zone) return;

        const char* shapes[] = { "Box", "Sphere" };
        int shape = static_cast<int>(zone->shape);
        if (ImGui::Combo("Shape", &shape, shapes, 2)) {
            zone->shape = static_cast<ECS::TriggerZoneComponent::Shape>(shape);
        }

        if (zone->shape == ECS::TriggerZoneComponent::Shape::Box) {
            f32 size[3] = { zone->boxSize.x, zone->boxSize.y, zone->boxSize.z };
            if (ImGui::DragFloat3("Box Size", size, 0.1f, 0.01f, 1000.0f)) {
                zone->boxSize = Math::Vector3(size[0], size[1], size[2]);
            }
        } else {
            ImGui::DragFloat("Sphere Radius", &zone->sphereRadius, 0.1f, 0.01f, 1000.0f);
        }

        ImGui::Checkbox("Trigger Once", &zone->triggerOnce);
        if (zone->triggerOnce) {
            ImGui::SameLine();
            ImGui::Text("(%s)", zone->hasTriggered ? "Triggered" : "Not triggered");
        }

        ImGui::Text("Entities Inside: %zu", zone->entitiesInside.size());

        if (ImGui::BeginPopupContextItem("TriggerZoneContext")) {
            if (ImGui::MenuItem("Remove Component")) {
                m_World->RemoveComponent<ECS::TriggerZoneComponent>(entity);
            }
            ImGui::EndPopup();
        }
    }
}

void EditorLayer::DrawDamageComponent(ECS::Entity entity) {
    if (ImGui::CollapsingHeader("Damage", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto* dmg = m_World->GetComponent<ECS::DamageComponent>(entity);
        if (!dmg) return;

        ImGui::DragFloat("Damage", &dmg->damage, 0.5f, 0.0f, 10000.0f);
        ImGui::DragFloat("Knockback Force", &dmg->knockbackForce, 0.5f, 0.0f, 1000.0f);

        const char* types[] = { "Physical", "Fire", "Ice", "Electric", "Poison", "Magic" };
        int type = static_cast<int>(dmg->type);
        if (ImGui::Combo("Damage Type", &type, types, 6)) {
            dmg->type = static_cast<ECS::DamageComponent::DamageType>(type);
        }

        ImGui::Checkbox("Destroy On Hit", &dmg->destroyOnHit);
        ImGui::Checkbox("Damage Once Per Entity", &dmg->damageOnce);

        if (!dmg->damageOnce) {
            ImGui::DragFloat("Damage Interval", &dmg->damageInterval, 0.1f, 0.0f, 10.0f);
        }

        if (ImGui::BeginPopupContextItem("DamageContext")) {
            if (ImGui::MenuItem("Remove Component")) {
                m_World->RemoveComponent<ECS::DamageComponent>(entity);
            }
            ImGui::EndPopup();
        }
    }
}

void EditorLayer::DrawInteractableComponent(ECS::Entity entity) {
    if (ImGui::CollapsingHeader("Interactable", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto* inter = m_World->GetComponent<ECS::InteractableComponent>(entity);
        if (!inter) return;

        char promptBuffer[256];
        strncpy(promptBuffer, inter->promptText.c_str(), sizeof(promptBuffer) - 1);
        promptBuffer[sizeof(promptBuffer) - 1] = '\0';
        if (ImGui::InputText("Prompt Text", promptBuffer, sizeof(promptBuffer))) {
            inter->promptText = promptBuffer;
        }

        ImGui::DragFloat("Interaction Range", &inter->interactionRange, 0.1f, 0.1f, 50.0f);
        ImGui::Checkbox("Requires Look At", &inter->requiresLookAt);
        if (inter->requiresLookAt) {
            ImGui::DragFloat("Look At Angle", &inter->lookAtAngle, 1.0f, 1.0f, 180.0f);
        }

        ImGui::Checkbox("Enabled", &inter->isEnabled);
        ImGui::Checkbox("Single Use", &inter->singleUse);
        if (inter->singleUse) {
            ImGui::SameLine();
            ImGui::Text("(%s)", inter->hasBeenUsed ? "Used" : "Available");
        }

        ImGui::Checkbox("Highlight On Hover", &inter->highlightOnHover);
        if (inter->highlightOnHover) {
            f32 col[3] = { inter->highlightColor.x, inter->highlightColor.y, inter->highlightColor.z };
            if (ImGui::ColorEdit3("Highlight Color", col)) {
                inter->highlightColor = Math::Vector3(col[0], col[1], col[2]);
            }
        }

        if (ImGui::BeginPopupContextItem("InteractableContext")) {
            if (ImGui::MenuItem("Remove Component")) {
                m_World->RemoveComponent<ECS::InteractableComponent>(entity);
            }
            ImGui::EndPopup();
        }
    }
}

void EditorLayer::DrawPickupComponent(ECS::Entity entity) {
    if (ImGui::CollapsingHeader("Pickup", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto* pickup = m_World->GetComponent<ECS::PickupComponent>(entity);
        if (!pickup) return;

        const char* types[] = { "Health", "Ammo", "Coin", "Key", "Powerup", "Custom" };
        int type = static_cast<int>(pickup->type);
        if (ImGui::Combo("Pickup Type", &type, types, 6)) {
            pickup->type = static_cast<ECS::PickupComponent::PickupType>(type);
        }

        ImGui::DragFloat("Value", &pickup->value, 0.5f, 0.0f, 10000.0f);

        if (pickup->type == ECS::PickupComponent::PickupType::Custom) {
            char idBuffer[128];
            strncpy(idBuffer, pickup->customId.c_str(), sizeof(idBuffer) - 1);
            idBuffer[sizeof(idBuffer) - 1] = '\0';
            if (ImGui::InputText("Custom ID", idBuffer, sizeof(idBuffer))) {
                pickup->customId = idBuffer;
            }
        }

        ImGui::DragFloat("Pickup Range", &pickup->pickupRange, 0.1f, 0.1f, 50.0f);
        ImGui::Checkbox("Destroy On Pickup", &pickup->destroyOnPickup);

        if (ImGui::TreeNode("Magnet")) {
            ImGui::Checkbox("Magnet To Player", &pickup->magnetToPlayer);
            if (pickup->magnetToPlayer) {
                ImGui::DragFloat("Magnet Range", &pickup->magnetRange, 0.5f, 0.1f, 50.0f);
                ImGui::DragFloat("Magnet Speed", &pickup->magnetSpeed, 0.5f, 0.1f, 100.0f);
            }
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Respawn")) {
            ImGui::Checkbox("Can Respawn", &pickup->canRespawn);
            if (pickup->canRespawn) {
                ImGui::DragFloat("Respawn Time", &pickup->respawnTime, 0.5f, 0.0f, 300.0f);
            }
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Visual")) {
            ImGui::DragFloat("Bob Speed", &pickup->bobSpeed, 0.1f, 0.0f, 10.0f);
            ImGui::DragFloat("Bob Height", &pickup->bobHeight, 0.01f, 0.0f, 2.0f);
            ImGui::DragFloat("Rotation Speed", &pickup->rotationSpeed, 5.0f, 0.0f, 720.0f);
            ImGui::TreePop();
        }

        ImGui::Separator();
        ImGui::Text("Status: %s", pickup->isCollected ? "Collected" : "Available");

        if (ImGui::BeginPopupContextItem("PickupContext")) {
            if (ImGui::MenuItem("Remove Component")) {
                m_World->RemoveComponent<ECS::PickupComponent>(entity);
            }
            ImGui::EndPopup();
        }
    }
}

void EditorLayer::DrawInventoryComponent(ECS::Entity entity) {
    if (ImGui::CollapsingHeader("Inventory", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto* inv = m_World->GetComponent<ECS::InventoryComponent>(entity);
        if (!inv) return;

        int maxSlots = static_cast<int>(inv->maxSlots);
        if (ImGui::InputInt("Max Slots", &maxSlots)) {
            inv->maxSlots = static_cast<usize>(maxSlots > 0 ? maxSlots : 1);
        }

        ImGui::DragInt("Coins", &inv->coins, 1, 0, 999999);
        ImGui::DragInt("Gems", &inv->gems, 1, 0, 999999);

        if (ImGui::TreeNode("Keys")) {
            for (usize i = 0; i < inv->keys.size(); ++i) {
                ImGui::PushID(static_cast<int>(i));
                char keyBuffer[128];
                strncpy(keyBuffer, inv->keys[i].c_str(), sizeof(keyBuffer) - 1);
                keyBuffer[sizeof(keyBuffer) - 1] = '\0';
                if (ImGui::InputText("##key", keyBuffer, sizeof(keyBuffer))) {
                    inv->keys[i] = keyBuffer;
                }
                ImGui::SameLine();
                if (ImGui::SmallButton("X")) {
                    inv->keys.erase(inv->keys.begin() + i);
                    ImGui::PopID();
                    break;
                }
                ImGui::PopID();
            }
            if (ImGui::SmallButton("Add Key")) {
                inv->keys.push_back("new_key");
            }
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Slots")) {
            ImGui::Text("Used: %zu / %zu", inv->slots.size(), inv->maxSlots);
            for (usize i = 0; i < inv->slots.size(); ++i) {
                ImGui::PushID(static_cast<int>(i));
                auto& slot = inv->slots[i];
                ImGui::Text("[%zu] %s x%d (max %d)", i, slot.itemId.c_str(), slot.quantity, slot.maxStack);
                ImGui::PopID();
            }
            ImGui::TreePop();
        }

        if (ImGui::BeginPopupContextItem("InventoryContext")) {
            if (ImGui::MenuItem("Remove Component")) {
                m_World->RemoveComponent<ECS::InventoryComponent>(entity);
            }
            ImGui::EndPopup();
        }
    }
}

void EditorLayer::DrawTimerComponent(ECS::Entity entity) {
    if (ImGui::CollapsingHeader("Timer", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto* timer = m_World->GetComponent<ECS::TimerComponent>(entity);
        if (!timer) return;

        ImGui::DragFloat("Duration", &timer->duration, 0.1f, 0.01f, 3600.0f);
        ImGui::Checkbox("Loop", &timer->loop);
        ImGui::Checkbox("Auto Start", &timer->autoStart);

        // Progress bar
        f32 progress = timer->GetProgress();
        ImGui::ProgressBar(progress, ImVec2(-1, 0),
            (std::to_string((int)(timer->GetRemaining() * 10) / 10.0f) + "s remaining").c_str());

        ImGui::Separator();
        ImGui::Text("Running: %s", timer->isRunning ? "Yes" : "No");
        ImGui::Text("Loop Count: %d", timer->loopCount);
        ImGui::Text("Complete: %s", timer->IsComplete() ? "Yes" : "No");

        if (ImGui::BeginPopupContextItem("TimerContext")) {
            if (ImGui::MenuItem("Remove Component")) {
                m_World->RemoveComponent<ECS::TimerComponent>(entity);
            }
            ImGui::EndPopup();
        }
    }
}

void EditorLayer::DrawAudioListenerComponent(ECS::Entity entity) {
    if (ImGui::CollapsingHeader("Audio Listener", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto* listener = m_World->GetComponent<ECS::AudioListenerComponent>(entity);
        if (!listener) return;

        ImGui::Checkbox("Active", &listener->isActive);
        ImGui::DragFloat("Volume Scale", &listener->volumeScale, 0.01f, 0.0f, 2.0f);

        if (ImGui::BeginPopupContextItem("AudioListenerContext")) {
            if (ImGui::MenuItem("Remove Component")) {
                m_World->RemoveComponent<ECS::AudioListenerComponent>(entity);
            }
            ImGui::EndPopup();
        }
    }
}

void EditorLayer::DrawAIControllerComponent(ECS::Entity entity) {
    if (ImGui::CollapsingHeader("AI Controller", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto* ai = m_World->GetComponent<ECS::AIControllerComponent>(entity);
        if (!ai) return;

        const char* states[] = { "Idle", "Patrol", "Chase", "Attack", "Flee", "Dead" };
        int state = static_cast<int>(ai->currentState);
        if (ImGui::Combo("Current State", &state, states, 6)) {
            ai->currentState = static_cast<ECS::AIControllerComponent::AIState>(state);
        }

        if (ImGui::TreeNode("Detection")) {
            ImGui::DragFloat("Detection Range", &ai->detectionRange, 0.5f, 0.0f, 200.0f);
            ImGui::DragFloat("Attack Range", &ai->attackRange, 0.5f, 0.0f, 50.0f);
            ImGui::DragFloat("Lose Target Range", &ai->loseTargetRange, 0.5f, 0.0f, 300.0f);
            ImGui::DragFloat("Field of View", &ai->fieldOfView, 1.0f, 0.0f, 360.0f);
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Movement")) {
            ImGui::DragFloat("Move Speed", &ai->moveSpeed, 0.1f, 0.0f, 50.0f);
            ImGui::DragFloat("Turn Speed", &ai->turnSpeed, 5.0f, 0.0f, 720.0f);
            ImGui::DragFloat("Stopping Distance", &ai->stoppingDistance, 0.1f, 0.0f, 10.0f);
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Attack")) {
            ImGui::DragFloat("Attack Cooldown", &ai->attackCooldown, 0.1f, 0.0f, 30.0f);
            ImGui::DragFloat("Attack Damage", &ai->attackDamage, 0.5f, 0.0f, 1000.0f);
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Patrol")) {
            ImGui::DragFloat("Wait Time", &ai->patrolWaitTime, 0.1f, 0.0f, 30.0f);
            ImGui::Text("Patrol Points: %zu", ai->patrolPoints.size());
            ImGui::Text("Current Index: %zu", ai->currentPatrolIndex);
            ImGui::TreePop();
        }

        if (ImGui::BeginPopupContextItem("AIControllerContext")) {
            if (ImGui::MenuItem("Remove Component")) {
                m_World->RemoveComponent<ECS::AIControllerComponent>(entity);
            }
            ImGui::EndPopup();
        }
    }
}

void EditorLayer::DrawFollowTargetComponent(ECS::Entity entity) {
    if (ImGui::CollapsingHeader("Follow Target", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto* follow = m_World->GetComponent<ECS::FollowTargetComponent>(entity);
        if (!follow) return;

        ImGui::Text("Target Entity: %llu", (unsigned long long)follow->target);
        ImGui::DragFloat("Follow Distance", &follow->followDistance, 0.1f, 0.0f, 100.0f);
        ImGui::DragFloat("Min Distance", &follow->minDistance, 0.1f, 0.0f, follow->followDistance);
        ImGui::DragFloat("Max Distance", &follow->maxDistance, 0.5f, follow->followDistance, 1000.0f);
        ImGui::DragFloat("Move Speed", &follow->moveSpeed, 0.1f, 0.0f, 50.0f);
        ImGui::DragFloat("Smooth Time", &follow->smoothTime, 0.01f, 0.0f, 5.0f);

        ImGui::Checkbox("Match Target Rotation", &follow->matchTargetRotation);
        if (follow->matchTargetRotation) {
            ImGui::DragFloat("Rotation Speed", &follow->rotationSpeed, 5.0f, 0.0f, 720.0f);
        }

        f32 offset[3] = { follow->offset.x, follow->offset.y, follow->offset.z };
        if (ImGui::DragFloat3("Offset", offset, 0.1f)) {
            follow->offset = Math::Vector3(offset[0], offset[1], offset[2]);
        }
        ImGui::Checkbox("Use Local Offset", &follow->useLocalOffset);

        if (ImGui::BeginPopupContextItem("FollowTargetContext")) {
            if (ImGui::MenuItem("Remove Component")) {
                m_World->RemoveComponent<ECS::FollowTargetComponent>(entity);
            }
            ImGui::EndPopup();
        }
    }
}

void EditorLayer::DrawLookAtTargetComponent(ECS::Entity entity) {
    if (ImGui::CollapsingHeader("Look At Target", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto* lookAt = m_World->GetComponent<ECS::LookAtTargetComponent>(entity);
        if (!lookAt) return;

        ImGui::Checkbox("Use World Position", &lookAt->useWorldTarget);
        if (lookAt->useWorldTarget) {
            f32 target[3] = { lookAt->worldTarget.x, lookAt->worldTarget.y, lookAt->worldTarget.z };
            if (ImGui::DragFloat3("World Target", target, 0.1f)) {
                lookAt->worldTarget = Math::Vector3(target[0], target[1], target[2]);
            }
        } else {
            ImGui::Text("Target Entity: %llu", (unsigned long long)lookAt->target);
        }

        ImGui::DragFloat("Rotation Speed", &lookAt->rotationSpeed, 5.0f, 0.0f, 720.0f);
        ImGui::Checkbox("Instant", &lookAt->instant);

        if (ImGui::TreeNode("Constraints")) {
            ImGui::Checkbox("Constrain X", &lookAt->constrainX);
            ImGui::Checkbox("Constrain Y", &lookAt->constrainY);
            ImGui::Checkbox("Constrain Z", &lookAt->constrainZ);
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Limits")) {
            ImGui::DragFloat("Min Yaw", &lookAt->minYaw, 1.0f, -180.0f, lookAt->maxYaw);
            ImGui::DragFloat("Max Yaw", &lookAt->maxYaw, 1.0f, lookAt->minYaw, 180.0f);
            ImGui::DragFloat("Min Pitch", &lookAt->minPitch, 1.0f, -89.0f, lookAt->maxPitch);
            ImGui::DragFloat("Max Pitch", &lookAt->maxPitch, 1.0f, lookAt->minPitch, 89.0f);
            ImGui::TreePop();
        }

        if (ImGui::BeginPopupContextItem("LookAtTargetContext")) {
            if (ImGui::MenuItem("Remove Component")) {
                m_World->RemoveComponent<ECS::LookAtTargetComponent>(entity);
            }
            ImGui::EndPopup();
        }
    }
}

void EditorLayer::DrawWaypointComponent(ECS::Entity entity) {
    if (ImGui::CollapsingHeader("Waypoint", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto* wp = m_World->GetComponent<ECS::WaypointComponent>(entity);
        if (!wp) return;

        char idBuffer[128];
        strncpy(idBuffer, wp->waypointId.c_str(), sizeof(idBuffer) - 1);
        idBuffer[sizeof(idBuffer) - 1] = '\0';
        if (ImGui::InputText("Waypoint ID", idBuffer, sizeof(idBuffer))) {
            wp->waypointId = idBuffer;
        }

        ImGui::InputInt("Index", &wp->index);
        ImGui::Text("Next Waypoint: %llu", (unsigned long long)wp->nextWaypoint);
        ImGui::DragFloat("Wait Time", &wp->waitTime, 0.1f, 0.0f, 60.0f);
        ImGui::DragFloat("Radius", &wp->radius, 0.05f, 0.01f, 10.0f);

        if (ImGui::BeginPopupContextItem("WaypointContext")) {
            if (ImGui::MenuItem("Remove Component")) {
                m_World->RemoveComponent<ECS::WaypointComponent>(entity);
            }
            ImGui::EndPopup();
        }
    }
}

void EditorLayer::DrawBillboardComponent(ECS::Entity entity) {
    if (ImGui::CollapsingHeader("Billboard", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto* bb = m_World->GetComponent<ECS::BillboardComponent>(entity);
        if (!bb) return;

        ImGui::Checkbox("Face Camera", &bb->faceCamera);
        ImGui::Checkbox("Lock Y Axis", &bb->lockY);
        ImGui::DragFloat("Rotation Offset", &bb->rotationOffset, 1.0f, -180.0f, 180.0f);

        if (ImGui::BeginPopupContextItem("BillboardContext")) {
            if (ImGui::MenuItem("Remove Component")) {
                m_World->RemoveComponent<ECS::BillboardComponent>(entity);
            }
            ImGui::EndPopup();
        }
    }
}

void EditorLayer::DrawParticleEmitterComponent(ECS::Entity entity) {
    if (ImGui::CollapsingHeader("Particle Emitter", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto* emitter = m_World->GetComponent<ECS::ParticleEmitterComponent>(entity);
        if (!emitter) return;

        ImGui::Checkbox("Playing", &emitter->isPlaying);
        ImGui::Checkbox("Play On Awake", &emitter->playOnAwake);
        ImGui::Checkbox("Loop", &emitter->loop);

        if (ImGui::TreeNode("Emission")) {
            ImGui::DragFloat("Rate (per sec)", &emitter->emissionRate, 0.5f, 0.0f, 1000.0f);
            ImGui::DragInt("Burst Count", &emitter->burstCount, 1, 0, 100);
            if (emitter->burstCount > 0) {
                ImGui::DragFloat("Burst Interval", &emitter->burstInterval, 0.1f, 0.0f, 30.0f);
            }
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Particle Properties")) {
            ImGui::DragFloat("Lifetime", &emitter->lifetime, 0.1f, 0.01f, 60.0f);
            ImGui::DragFloat("Lifetime Variance", &emitter->lifetimeVariance, 0.1f, 0.0f, emitter->lifetime);
            ImGui::DragFloat("Start Speed", &emitter->startSpeed, 0.1f, 0.0f, 100.0f);
            ImGui::DragFloat("Speed Variance", &emitter->speedVariance, 0.1f, 0.0f, emitter->startSpeed);
            ImGui::DragFloat("Start Size", &emitter->startSize, 0.01f, 0.001f, 10.0f);
            ImGui::DragFloat("End Size", &emitter->endSize, 0.01f, 0.0f, 10.0f);
            ImGui::DragFloat("Start Alpha", &emitter->startAlpha, 0.01f, 0.0f, 1.0f);
            ImGui::DragFloat("End Alpha", &emitter->endAlpha, 0.01f, 0.0f, 1.0f);
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Colors")) {
            f32 startCol[3] = { emitter->startColor.x, emitter->startColor.y, emitter->startColor.z };
            if (ImGui::ColorEdit3("Start Color", startCol)) {
                emitter->startColor = Math::Vector3(startCol[0], startCol[1], startCol[2]);
            }
            f32 endCol[3] = { emitter->endColor.x, emitter->endColor.y, emitter->endColor.z };
            if (ImGui::ColorEdit3("End Color", endCol)) {
                emitter->endColor = Math::Vector3(endCol[0], endCol[1], endCol[2]);
            }
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Shape")) {
            const char* shapes[] = { "Point", "Sphere", "Hemisphere", "Cone", "Box" };
            int shape = static_cast<int>(emitter->shape);
            if (ImGui::Combo("Shape", &shape, shapes, 5)) {
                emitter->shape = static_cast<ECS::ParticleEmitterComponent::EmitterShape>(shape);
            }
            ImGui::DragFloat("Shape Radius", &emitter->shapeRadius, 0.05f, 0.0f, 50.0f);
            if (emitter->shape == ECS::ParticleEmitterComponent::EmitterShape::Cone) {
                ImGui::DragFloat("Cone Angle", &emitter->coneAngle, 1.0f, 0.0f, 90.0f);
            }
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Forces")) {
            f32 gravity[3] = { emitter->gravity.x, emitter->gravity.y, emitter->gravity.z };
            if (ImGui::DragFloat3("Gravity", gravity, 0.1f)) {
                emitter->gravity = Math::Vector3(gravity[0], gravity[1], gravity[2]);
            }
            ImGui::DragFloat("Drag", &emitter->drag, 0.01f, 0.0f, 10.0f);
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Texture")) {
            char pathBuffer[256];
            strncpy(pathBuffer, emitter->texturePath.c_str(), sizeof(pathBuffer) - 1);
            pathBuffer[sizeof(pathBuffer) - 1] = '\0';
            if (ImGui::InputText("Texture Path", pathBuffer, sizeof(pathBuffer))) {
                emitter->texturePath = pathBuffer;
            }
            ImGui::DragInt("Sheet X", &emitter->textureSheetX, 1, 1, 16);
            ImGui::DragInt("Sheet Y", &emitter->textureSheetY, 1, 1, 16);
            ImGui::TreePop();
        }

        if (ImGui::BeginPopupContextItem("ParticleEmitterContext")) {
            if (ImGui::MenuItem("Remove Component")) {
                m_World->RemoveComponent<ECS::ParticleEmitterComponent>(entity);
            }
            ImGui::EndPopup();
        }
    }
}

void EditorLayer::DrawCamera2DBoundsComponent(ECS::Entity entity) {
    if (ImGui::CollapsingHeader("Camera 2D Bounds", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto* bounds = m_World->GetComponent<ECS::Camera2DBoundsComponent>(entity);
        if (!bounds) return;

        ImGui::Checkbox("Use Bounds", &bounds->useBounds);
        if (bounds->useBounds) {
            f32 minB[2] = { bounds->minBounds.x, bounds->minBounds.y };
            if (ImGui::DragFloat2("Min Bounds", minB, 0.5f)) {
                bounds->minBounds = Math::Vector2(minB[0], minB[1]);
            }
            f32 maxB[2] = { bounds->maxBounds.x, bounds->maxBounds.y };
            if (ImGui::DragFloat2("Max Bounds", maxB, 0.5f)) {
                bounds->maxBounds = Math::Vector2(maxB[0], maxB[1]);
            }
            ImGui::DragFloat("Padding", &bounds->boundsPadding, 0.1f, 0.0f, 100.0f);
        }

        ImGui::Text("Follow Target: %llu", (unsigned long long)bounds->followTarget);
        ImGui::DragFloat("Follow Smoothing", &bounds->followSmoothing, 0.1f, 0.1f, 50.0f);

        f32 offset[2] = { bounds->followOffset.x, bounds->followOffset.y };
        if (ImGui::DragFloat2("Follow Offset", offset, 0.1f)) {
            bounds->followOffset = Math::Vector2(offset[0], offset[1]);
        }

        if (ImGui::TreeNode("Zoom")) {
            ImGui::DragFloat("Min Zoom", &bounds->minZoom, 0.05f, 0.1f, bounds->maxZoom);
            ImGui::DragFloat("Max Zoom", &bounds->maxZoom, 0.05f, bounds->minZoom, 10.0f);
            ImGui::DragFloat("Current Zoom", &bounds->currentZoom, 0.05f, bounds->minZoom, bounds->maxZoom);
            ImGui::TreePop();
        }

        if (ImGui::BeginPopupContextItem("Camera2DBoundsContext")) {
            if (ImGui::MenuItem("Remove Component")) {
                m_World->RemoveComponent<ECS::Camera2DBoundsComponent>(entity);
            }
            ImGui::EndPopup();
        }
    }
}

void EditorLayer::DrawTagComponent(ECS::Entity entity) {
    if (ImGui::CollapsingHeader("Tags", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto* tags = m_World->GetComponent<ECS::TagComponent>(entity);
        if (!tags) return;

        for (usize i = 0; i < tags->tags.size(); ++i) {
            ImGui::PushID(static_cast<int>(i));
            char tagBuffer[128];
            strncpy(tagBuffer, tags->tags[i].c_str(), sizeof(tagBuffer) - 1);
            tagBuffer[sizeof(tagBuffer) - 1] = '\0';
            if (ImGui::InputText("##tag", tagBuffer, sizeof(tagBuffer))) {
                tags->tags[i] = tagBuffer;
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("X")) {
                tags->tags.erase(tags->tags.begin() + i);
                ImGui::PopID();
                break;
            }
            ImGui::PopID();
        }

        if (ImGui::Button("Add Tag")) {
            tags->tags.push_back("new_tag");
        }

        if (ImGui::BeginPopupContextItem("TagContext")) {
            if (ImGui::MenuItem("Remove Component")) {
                m_World->RemoveComponent<ECS::TagComponent>(entity);
            }
            ImGui::EndPopup();
        }
    }
}

void EditorLayer::DrawSpawnPointComponent(ECS::Entity entity) {
    if (ImGui::CollapsingHeader("Spawn Point", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto* spawn = m_World->GetComponent<ECS::SpawnPointComponent>(entity);
        if (!spawn) return;

        char idBuffer[128];
        strncpy(idBuffer, spawn->spawnId.c_str(), sizeof(idBuffer) - 1);
        idBuffer[sizeof(idBuffer) - 1] = '\0';
        if (ImGui::InputText("Spawn ID", idBuffer, sizeof(idBuffer))) {
            spawn->spawnId = idBuffer;
        }

        char prefabBuffer[256];
        strncpy(prefabBuffer, spawn->prefabToSpawn.c_str(), sizeof(prefabBuffer) - 1);
        prefabBuffer[sizeof(prefabBuffer) - 1] = '\0';
        if (ImGui::InputText("Prefab To Spawn", prefabBuffer, sizeof(prefabBuffer))) {
            spawn->prefabToSpawn = prefabBuffer;
        }

        ImGui::Checkbox("Spawn On Start", &spawn->spawnOnStart);
        ImGui::DragFloat("Spawn Delay", &spawn->spawnDelay, 0.1f, 0.0f, 60.0f);
        ImGui::DragFloat("Respawn Time", &spawn->respawnTime, 0.5f, 0.0f, 300.0f);

        int maxSpawns = spawn->maxSpawns;
        if (ImGui::InputInt("Max Spawns (-1 = unlimited)", &maxSpawns)) {
            spawn->maxSpawns = maxSpawns;
        }

        ImGui::DragFloat("Spawn Radius", &spawn->spawnRadius, 0.1f, 0.0f, 50.0f);
        ImGui::Checkbox("Random Rotation", &spawn->randomRotation);

        ImGui::Separator();
        ImGui::Text("Current Spawns: %d", spawn->currentSpawns);
        ImGui::Text("Active Entities: %zu", spawn->spawnedEntities.size());

        if (ImGui::BeginPopupContextItem("SpawnPointContext")) {
            if (ImGui::MenuItem("Remove Component")) {
                m_World->RemoveComponent<ECS::SpawnPointComponent>(entity);
            }
            ImGui::EndPopup();
        }
    }
}

void EditorLayer::DrawJellyMeshComponent(ECS::Entity entity) {
    if (ImGui::CollapsingHeader("Jelly Mesh", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto* jelly = m_World->GetComponent<ECS::JellyMeshComponent>(entity);
        if (!jelly) return;

        ImGui::SliderFloat("Spring Stiffness##Jelly", &jelly->springStiffness, 1.0f, 200.0f);
        ImGui::SliderFloat("Damping##Jelly", &jelly->damping, 0.1f, 20.0f);
        ImGui::SliderFloat("Max Stretch##Jelly", &jelly->maxStretch, 0.01f, 2.0f);

        if (jelly->initialized) {
            ImGui::TextDisabled("Vertices: %zu", jelly->restPositions.size());
        } else {
            ImGui::TextDisabled("Not initialized (waiting for play mode)");
        }

        if (ImGui::BeginPopupContextItem("JellyMeshContext")) {
            if (ImGui::MenuItem("Remove Component")) {
                m_World->RemoveComponent<ECS::JellyMeshComponent>(entity);
            }
            ImGui::EndPopup();
        }
    }
}

void EditorLayer::DrawTetherComponent(ECS::Entity entity) {
    if (ImGui::CollapsingHeader("Tether", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto* tether = m_World->GetComponent<ECS::TetherComponent>(entity);
        if (!tether) return;

        // Stem entity picker (show name if available)
        char stemLabel[128] = "None";
        if (tether->stemEntity != ECS::INVALID_ENTITY && m_World->HasComponent<ECS::NameComponent>(tether->stemEntity)) {
            auto* name = m_World->GetComponent<ECS::NameComponent>(tether->stemEntity);
            snprintf(stemLabel, sizeof(stemLabel), "%s (%llu)", name->name.c_str(), (unsigned long long)tether->stemEntity);
        } else if (tether->stemEntity != ECS::INVALID_ENTITY) {
            snprintf(stemLabel, sizeof(stemLabel), "Entity %llu", (unsigned long long)tether->stemEntity);
        }
        ImGui::Text("Stem: %s", stemLabel);

        ImGui::DragFloat3("Attach Local Pos", &tether->attachLocalPos.x, 0.01f);
        ImGui::SliderFloat("Rest Length (0=auto)", &tether->restLength, 0.0f, 5.0f);
        ImGui::SliderFloat("Stiffness##Tether", &tether->tetherStiffness, 1.0f, 200.0f);
        ImGui::SliderFloat("Damping##Tether", &tether->tetherDamping, 0.1f, 20.0f);
        ImGui::SliderFloat("Break Distance", &tether->breakDistance, 0.1f, 5.0f);
        ImGui::SliderFloat("Tension Ramp", &tether->tensionRamp, 0.5f, 5.0f);

        // Read-only tension bar
        ImGui::ProgressBar(tether->currentTension, ImVec2(-1, 0), tether->isBroken ? "BROKEN" : nullptr);
        if (tether->isBroken) {
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Tether is broken");
        }

        if (ImGui::BeginPopupContextItem("TetherContext")) {
            if (ImGui::MenuItem("Remove Component")) {
                m_World->RemoveComponent<ECS::TetherComponent>(entity);
            }
            ImGui::EndPopup();
        }
    }
}

void EditorLayer::DrawGrabbableComponent(ECS::Entity entity) {
    if (ImGui::CollapsingHeader("Grabbable", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto* grab = m_World->GetComponent<ECS::GrabbableComponent>(entity);
        if (!grab) return;

        ImGui::SliderFloat("Pull Force", &grab->pullForce, 1.0f, 50.0f);
        ImGui::SliderFloat("Grab Radius", &grab->grabRadius, 0.1f, 5.0f);

        if (grab->isGrabbed) {
            ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "Currently grabbed");
        }

        if (ImGui::BeginPopupContextItem("GrabbableContext")) {
            if (ImGui::MenuItem("Remove Component")) {
                m_World->RemoveComponent<ECS::GrabbableComponent>(entity);
            }
            ImGui::EndPopup();
        }
    }
}

void EditorLayer::DrawFlowerStemComponent(ECS::Entity entity) {
    if (ImGui::CollapsingHeader("Flower Stem", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto* stem = m_World->GetComponent<ECS::FlowerStemComponent>(entity);
        if (!stem) return;

        ImGui::SliderFloat("Healthy Bonus", &stem->healthyBonus, 0.0f, 50.0f);
        ImGui::SliderFloat("Withered Penalty", &stem->witheredPenalty, 0.0f, 50.0f);

        ImGui::Separator();
        ImGui::Text("Parts Removed: %d", stem->partsRemoved);
        ImGui::Text("Healthy: %d  Withered: %d", stem->healthyRemoved, stem->witheredRemoved);
        if (stem->evaluated) {
            ImGui::Text("Score: %.1f", stem->score);
        } else {
            ImGui::TextDisabled("Not evaluated yet");
        }

        if (ImGui::BeginPopupContextItem("FlowerStemContext")) {
            if (ImGui::MenuItem("Remove Component")) {
                m_World->RemoveComponent<ECS::FlowerStemComponent>(entity);
            }
            ImGui::EndPopup();
        }
    }
}

// ============================================================================
// Runtime Dialogue System
// ============================================================================

void EditorLayer::UpdateDialogue(f32 deltaTime) {
    if (!m_World) return;

    // Find active dialogue entity
    ECS::Entity activeDialogue = ECS::INVALID_ENTITY;

    for (ECS::Entity entity : m_World->GetAllEntities()) {
        if (!m_World->HasComponent<ECS::DialogueComponent>(entity)) continue;
        auto* dlg = m_World->GetComponent<ECS::DialogueComponent>(entity);
        if (!dlg || dlg->dialogueLines.empty() || dlg->IsComplete()) continue;

        // This dialogue is active (has lines, not complete)
        activeDialogue = entity;
        auto& d = *dlg;

        // Advance typewriter
        if (d.isTyping && d.currentLine < d.dialogueLines.size()) {
            d.charTimer += deltaTime;
            while (d.charTimer >= d.charDelay && d.currentChar < d.dialogueLines[d.currentLine].size()) {
                d.currentChar++;
                d.charTimer -= d.charDelay;
            }
            // Line fully typed
            if (d.currentChar >= d.dialogueLines[d.currentLine].size()) {
                d.isTyping = false;
                d.waitingForInput = true;
            }
        }

        // Input: advance dialogue
        if (d.waitingForInput) {
            if (Input::IsKeyPressed(KeyCode::Space) || Input::IsKeyPressed(KeyCode::Enter) ||
                Input::IsMouseButtonPressed(MouseButton::Left)) {
                // If choices are available at end of dialogue, don't auto-advance
                if (d.currentLine + 1 >= d.dialogueLines.size() && !d.choices.empty()) {
                    // Wait for choice selection
                } else {
                    d.currentLine++;
                    d.currentChar = 0;
                    d.charTimer = 0.0f;
                    d.waitingForInput = false;
                    if (!d.IsComplete()) {
                        d.isTyping = true;
                    }
                }
            }
        }

        // If typing and player presses advance, skip to end of line
        if (d.isTyping) {
            if (Input::IsKeyPressed(KeyCode::Space) || Input::IsKeyPressed(KeyCode::Enter)) {
                d.currentChar = static_cast<u32>(d.dialogueLines[d.currentLine].size());
                d.isTyping = false;
                d.waitingForInput = true;
            }
        }

        // Choice navigation
        if (d.waitingForInput && !d.choices.empty() &&
            d.currentLine + 1 >= d.dialogueLines.size()) {
            if (Input::IsKeyPressed(KeyCode::Up) || Input::IsKeyPressed(KeyCode::W)) {
                d.selectedChoice--;
                if (d.selectedChoice < 0) d.selectedChoice = static_cast<i32>(d.choices.size()) - 1;
            }
            if (Input::IsKeyPressed(KeyCode::Down) || Input::IsKeyPressed(KeyCode::S)) {
                d.selectedChoice++;
                if (d.selectedChoice >= static_cast<i32>(d.choices.size())) d.selectedChoice = 0;
            }
            // Select choice
            if (Input::IsKeyPressed(KeyCode::Enter) || Input::IsKeyPressed(KeyCode::Space)) {
                // Mark complete (game logic can read selectedChoice)
                d.currentLine = static_cast<u32>(d.dialogueLines.size());
                d.waitingForInput = false;
            }
        }

        break;  // Only handle one active dialogue at a time
    }

    m_ActiveDialogueEntity = activeDialogue;
}

void EditorLayer::DrawDialogueOverlay() {
    if (!m_World || m_ActiveDialogueEntity == ECS::INVALID_ENTITY) return;

    auto* dlg = m_World->GetComponent<ECS::DialogueComponent>(m_ActiveDialogueEntity);
    if (!dlg || dlg->IsComplete()) return;

    ImGuiIO& io = ImGui::GetIO();
    f32 screenW = io.DisplaySize.x;
    f32 screenH = io.DisplaySize.y;

    // Dialogue box dimensions — bottom of screen, classic JRPG style
    f32 boxW = screenW * 0.75f;
    f32 boxH = 140.0f;
    f32 boxX = (screenW - boxW) * 0.5f;
    f32 boxY = screenH - boxH - 30.0f;
    f32 padding = 16.0f;

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNav;

    // Style the dialogue box
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(padding, padding));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 6.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 2.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.05f, 0.05f, 0.1f, 0.92f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.4f, 0.45f, 0.65f, 0.8f));

    ImGui::SetNextWindowPos(ImVec2(boxX, boxY));
    ImGui::SetNextWindowSize(ImVec2(boxW, boxH));

    if (ImGui::Begin("##DialogueBox", nullptr, flags)) {
        // Speaker name header
        if (!dlg->speakerName.empty()) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.75f, 1.0f, 1.0f));
            ImGui::Text("%s", dlg->speakerName.c_str());
            ImGui::PopStyleColor();
            ImGui::Separator();
            ImGui::Spacing();
        }

        // Dialogue text with typewriter
        std::string visibleText = dlg->GetVisibleText();
        if (!visibleText.empty()) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.95f, 0.98f, 1.0f));
            ImGui::TextWrapped("%s", visibleText.c_str());
            ImGui::PopStyleColor();
        }

        // Blinking cursor indicator when typing
        if (dlg->isTyping) {
            ImGui::SameLine(0, 0);
            f32 blink = std::fmod(static_cast<f32>(ImGui::GetTime()) * 3.0f, 2.0f);
            if (blink < 1.0f) {
                ImGui::TextColored(ImVec4(0.7f, 0.8f, 1.0f, 0.8f), "_");
            }
        }

        // "Press to continue" indicator
        if (dlg->waitingForInput && dlg->choices.empty()) {
            f32 bounce = std::sin(static_cast<f32>(ImGui::GetTime()) * 4.0f) * 0.3f + 0.7f;
            ImGui::SetCursorPosY(boxH - padding - ImGui::GetTextLineHeight());
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.6f, 0.8f, bounce));
            ImGui::Text("[Space]");
            ImGui::PopStyleColor();
        }
    }
    ImGui::End();

    // Choice box (appears above dialogue box when choices are shown)
    bool showChoices = dlg->waitingForInput && !dlg->choices.empty() &&
                       dlg->currentLine + 1 >= dlg->dialogueLines.size();
    if (showChoices) {
        f32 choiceH = static_cast<f32>(dlg->choices.size()) * 28.0f + padding * 2.0f;
        f32 choiceW = 300.0f;
        f32 choiceX = boxX + boxW - choiceW - 10.0f;
        f32 choiceY = boxY - choiceH - 8.0f;

        ImGui::SetNextWindowPos(ImVec2(choiceX, choiceY));
        ImGui::SetNextWindowSize(ImVec2(choiceW, choiceH));

        if (ImGui::Begin("##ChoiceBox", nullptr, flags)) {
            for (usize i = 0; i < dlg->choices.size(); ++i) {
                bool selected = (static_cast<i32>(i) == dlg->selectedChoice);

                if (selected) {
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
        ImGui::End();
    }

    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(3);
}

void EditorLayer::DuplicateEntity(ECS::Entity entity) {
    if (!m_World || entity == ECS::INVALID_ENTITY) return;

    // Create new entity
    ECS::Entity newEntity = m_World->CreateEntity();

    // Copy name component (with "(Copy)" suffix)
    if (m_World->HasComponent<ECS::NameComponent>(entity)) {
        auto* name = m_World->GetComponent<ECS::NameComponent>(entity);
        m_World->AddComponent<ECS::NameComponent>(newEntity, name->name + " (Copy)");
    }

    // Copy transform component (offset slightly so it's visible)
    if (m_World->HasComponent<ECS::TransformComponent>(entity)) {
        auto* transform = m_World->GetComponent<ECS::TransformComponent>(entity);
        auto& newTransform = m_World->AddComponent<ECS::TransformComponent>(newEntity);
        newTransform.position = transform->position + Math::Vector3(0.5f, 0.0f, 0.5f);
        newTransform.rotation = transform->rotation;
        newTransform.scale = transform->scale;
    }

    // Copy mesh component
    if (m_World->HasComponent<ECS::MeshComponent>(entity)) {
        auto* mesh = m_World->GetComponent<ECS::MeshComponent>(entity);
        auto& newMesh = m_World->AddComponent<ECS::MeshComponent>(newEntity);
        newMesh.vertices = mesh->vertices;
        newMesh.indices = mesh->indices;
    }

    // Copy material component
    if (m_World->HasComponent<ECS::MaterialComponent>(entity)) {
        auto* mat = m_World->GetComponent<ECS::MaterialComponent>(entity);
        auto& newMat = m_World->AddComponent<ECS::MaterialComponent>(newEntity);
        newMat.baseColor = mat->baseColor;
        newMat.metallic = mat->metallic;
        newMat.roughness = mat->roughness;
        newMat.emissiveColor = mat->emissiveColor;
        newMat.emissiveStrength = mat->emissiveStrength;
        newMat.opacity = mat->opacity;
        newMat.alphaCutoff = mat->alphaCutoff;
        newMat.doubleSided = mat->doubleSided;
    }

    // Copy light component
    if (m_World->HasComponent<ECS::LightComponent>(entity)) {
        auto* light = m_World->GetComponent<ECS::LightComponent>(entity);
        auto& newLight = m_World->AddComponent<ECS::LightComponent>(newEntity);
        newLight.type = light->type;
        newLight.color = light->color;
        newLight.intensity = light->intensity;
        newLight.range = light->range;
        newLight.castShadows = light->castShadows;
    }

    // Copy camera component
    if (m_World->HasComponent<ECS::CameraComponent>(entity)) {
        auto* cam = m_World->GetComponent<ECS::CameraComponent>(entity);
        m_World->AddComponent<ECS::CameraComponent>(newEntity, *cam);
    }

    // Copy weather zone component
    if (m_World->HasComponent<ECS::WeatherZoneComponent>(entity)) {
        auto* zone = m_World->GetComponent<ECS::WeatherZoneComponent>(entity);
        m_World->AddComponent<ECS::WeatherZoneComponent>(newEntity, *zone);
    }

    // Copy water volume component
    if (m_World->HasComponent<ECS::WaterVolumeComponent>(entity)) {
        auto* volume = m_World->GetComponent<ECS::WaterVolumeComponent>(entity);
        m_World->AddComponent<ECS::WaterVolumeComponent>(newEntity, *volume);
    }

    // Copy camera trigger component
    if (m_World->HasComponent<ECS::CameraTriggerComponent>(entity)) {
        auto* trigger = m_World->GetComponent<ECS::CameraTriggerComponent>(entity);
        m_World->AddComponent<ECS::CameraTriggerComponent>(newEntity, *trigger);
    }

    // Copy temperature zone component
    if (m_World->HasComponent<ECS::TemperatureZoneComponent>(entity)) {
        auto* tempZone = m_World->GetComponent<ECS::TemperatureZoneComponent>(entity);
        m_World->AddComponent<ECS::TemperatureZoneComponent>(newEntity, *tempZone);
    }

    // Copy notes component
    if (m_World->HasComponent<ECS::NotesComponent>(entity)) {
        auto* notes = m_World->GetComponent<ECS::NotesComponent>(entity);
        m_World->AddComponent<ECS::NotesComponent>(newEntity, *notes);
    }

    // Select the new entity
    m_SelectedEntity = newEntity;
    ENJIN_LOG_INFO(Editor, "Duplicated entity %llu -> %llu", entity, newEntity);
}

void EditorLayer::DeleteSelectedEntity() {
    if (!m_World || m_SelectedEntity == ECS::INVALID_ENTITY) return;

    ECS::Entity toDelete = m_SelectedEntity;
    m_SelectedEntity = ECS::INVALID_ENTITY;
    m_World->DestroyEntity(toDelete);
    ENJIN_LOG_INFO(Editor, "Deleted entity %llu", toDelete);
}

void EditorLayer::DrawCameraFrustum(ECS::Entity cameraEntity) {
    if (!m_Camera || !m_Renderer || !m_World) {
        return;
    }

    auto* camComp = m_World->GetComponent<ECS::CameraComponent>(cameraEntity);
    auto* transform = m_World->GetComponent<ECS::TransformComponent>(cameraEntity);
    if (!camComp || !transform) {
        return;
    }

    auto extent = m_Renderer->GetSwapchainExtent();
    if (extent.width == 0 || extent.height == 0) {
        return;
    }

    // Get editor camera matrices for projection
    Math::Matrix4 viewMat = m_Camera->GetViewMatrix();
    Math::Matrix4 projMat = m_Camera->GetProjectionMatrix();
    Math::Matrix4 viewProj = projMat * viewMat;

    f32 screenWidth = static_cast<f32>(extent.width);
    f32 screenHeight = static_cast<f32>(extent.height);

    // Project world position to screen (must match DrawGrid's worldToScreen)
    auto worldToScreen = [&](const Math::Vector3& worldPos, ImVec2& screenPos) -> bool {
        Math::Vector4 clipPos = viewProj * Math::Vector4(worldPos.x, worldPos.y, worldPos.z, 1.0f);
        if (clipPos.w <= 0.001f) return false;
        f32 ndcX = clipPos.x / clipPos.w;
        f32 ndcY = clipPos.y / clipPos.w;
        f32 ndcZ = clipPos.z / clipPos.w;
        if (ndcZ < 0.0f || ndcZ > 1.0f) return false;
        screenPos.x = (ndcX + 1.0f) * 0.5f * screenWidth;
        screenPos.y = (ndcY + 1.0f) * 0.5f * screenHeight;
        return true;
    };

    ImDrawList* drawList = ImGui::GetBackgroundDrawList();

    // Calculate camera orientation
    Math::Vector3 camPos = transform->position;
    Math::Vector3 forward = transform->rotation.Rotate(Math::Vector3(0.0f, 0.0f, -1.0f));
    Math::Vector3 up = transform->rotation.Rotate(Math::Vector3(0.0f, 1.0f, 0.0f));
    Math::Vector3 right = transform->rotation.Rotate(Math::Vector3(1.0f, 0.0f, 0.0f));

    // Frustum parameters
    f32 fov = camComp->fieldOfView * (3.14159f / 180.0f);
    f32 aspect = screenWidth / screenHeight;
    f32 nearDist = camComp->nearPlane;
    f32 farDist = Math::Min(camComp->farPlane, 50.0f); // Clamp far plane for visualization

    // Calculate frustum dimensions at near and far planes
    f32 nearHeight = 2.0f * Math::Tan(fov * 0.5f) * nearDist;
    f32 nearWidth = nearHeight * aspect;
    f32 farHeight = 2.0f * Math::Tan(fov * 0.5f) * farDist;
    f32 farWidth = farHeight * aspect;

    // Calculate frustum corner points
    Math::Vector3 nearCenter = camPos + forward * nearDist;
    Math::Vector3 farCenter = camPos + forward * farDist;

    Math::Vector3 nearTopLeft = nearCenter + up * (nearHeight * 0.5f) - right * (nearWidth * 0.5f);
    Math::Vector3 nearTopRight = nearCenter + up * (nearHeight * 0.5f) + right * (nearWidth * 0.5f);
    Math::Vector3 nearBottomLeft = nearCenter - up * (nearHeight * 0.5f) - right * (nearWidth * 0.5f);
    Math::Vector3 nearBottomRight = nearCenter - up * (nearHeight * 0.5f) + right * (nearWidth * 0.5f);

    Math::Vector3 farTopLeft = farCenter + up * (farHeight * 0.5f) - right * (farWidth * 0.5f);
    Math::Vector3 farTopRight = farCenter + up * (farHeight * 0.5f) + right * (farWidth * 0.5f);
    Math::Vector3 farBottomLeft = farCenter - up * (farHeight * 0.5f) - right * (farWidth * 0.5f);
    Math::Vector3 farBottomRight = farCenter - up * (farHeight * 0.5f) + right * (farWidth * 0.5f);

    // Colors - yellow/orange for selected camera, gray for others
    bool isSelected = (cameraEntity == m_SelectedEntity);
    ImU32 frustumColor = isSelected ? IM_COL32(255, 200, 50, 180) : IM_COL32(150, 150, 150, 100);
    ImU32 directionColor = isSelected ? IM_COL32(255, 100, 50, 255) : IM_COL32(200, 100, 50, 180);
    f32 lineThickness = isSelected ? 2.0f : 1.0f;

    // Draw frustum lines from camera position to far corners
    auto drawLine3D = [&](const Math::Vector3& from, const Math::Vector3& to, ImU32 color, f32 thickness) {
        ImVec2 screenFrom, screenTo;
        if (worldToScreen(from, screenFrom) && worldToScreen(to, screenTo)) {
            drawList->AddLine(screenFrom, screenTo, color, thickness);
        }
    };

    // Draw edges from camera to near plane
    drawLine3D(camPos, nearTopLeft, frustumColor, lineThickness);
    drawLine3D(camPos, nearTopRight, frustumColor, lineThickness);
    drawLine3D(camPos, nearBottomLeft, frustumColor, lineThickness);
    drawLine3D(camPos, nearBottomRight, frustumColor, lineThickness);

    // Draw near plane rectangle
    drawLine3D(nearTopLeft, nearTopRight, frustumColor, lineThickness);
    drawLine3D(nearTopRight, nearBottomRight, frustumColor, lineThickness);
    drawLine3D(nearBottomRight, nearBottomLeft, frustumColor, lineThickness);
    drawLine3D(nearBottomLeft, nearTopLeft, frustumColor, lineThickness);

    // Draw far plane rectangle
    drawLine3D(farTopLeft, farTopRight, frustumColor, lineThickness);
    drawLine3D(farTopRight, farBottomRight, frustumColor, lineThickness);
    drawLine3D(farBottomRight, farBottomLeft, frustumColor, lineThickness);
    drawLine3D(farBottomLeft, farTopLeft, frustumColor, lineThickness);

    // Draw edges from near to far plane
    drawLine3D(nearTopLeft, farTopLeft, frustumColor, lineThickness);
    drawLine3D(nearTopRight, farTopRight, frustumColor, lineThickness);
    drawLine3D(nearBottomLeft, farBottomLeft, frustumColor, lineThickness);
    drawLine3D(nearBottomRight, farBottomRight, frustumColor, lineThickness);

    // Draw direction arrow (forward direction)
    f32 arrowLength = 1.5f;
    Math::Vector3 arrowEnd = camPos + forward * arrowLength;
    drawLine3D(camPos, arrowEnd, directionColor, lineThickness + 1.0f);

    // Draw up direction (small)
    Math::Vector3 upEnd = camPos + up * 0.5f;
    drawLine3D(camPos, upEnd, IM_COL32(50, 255, 50, 200), lineThickness);

    // Draw camera icon (small box at camera position)
    ImVec2 screenCamPos;
    if (worldToScreen(camPos, screenCamPos)) {
        f32 iconSize = isSelected ? 8.0f : 5.0f;
        drawList->AddRectFilled(
            ImVec2(screenCamPos.x - iconSize, screenCamPos.y - iconSize),
            ImVec2(screenCamPos.x + iconSize, screenCamPos.y + iconSize),
            isSelected ? IM_COL32(255, 200, 50, 255) : IM_COL32(150, 150, 150, 200)
        );
        drawList->AddRect(
            ImVec2(screenCamPos.x - iconSize, screenCamPos.y - iconSize),
            ImVec2(screenCamPos.x + iconSize, screenCamPos.y + iconSize),
            IM_COL32(255, 255, 255, 255), 0.0f, 0, 1.0f
        );
    }
}

void EditorLayer::SetupCameraForController(ECS::Entity controllerEntity, const std::string& controllerType) {
    if (!m_World) return;

    // Check if a game camera already exists
    ECS::Entity existingCamera = ECS::CameraManager::GetActiveCamera(m_World);

    // Get the controller entity's transform for positioning the camera relative to it
    auto* playerTransform = m_World->GetComponent<ECS::TransformComponent>(controllerEntity);
    Math::Vector3 playerPos = playerTransform ? playerTransform->position : Math::Vector3(0.0f, 0.0f, 0.0f);

    // Create a camera entity if none exists
    ECS::Entity cameraEntity;
    if (existingCamera == ECS::INVALID_ENTITY) {
        cameraEntity = m_World->CreateEntity();
        auto& name = m_World->AddComponent<ECS::NameComponent>(cameraEntity);
        name.name = "Game Camera";
        m_World->AddComponent<ECS::TransformComponent>(cameraEntity);
        m_World->AddComponent<ECS::CameraComponent>(cameraEntity);
        ENJIN_LOG_INFO(Editor, "Auto-created Game Camera for %s controller", controllerType.c_str());
    } else {
        cameraEntity = existingCamera;
    }

    auto* camTransform = m_World->GetComponent<ECS::TransformComponent>(cameraEntity);
    auto* camComp = m_World->GetComponent<ECS::CameraComponent>(cameraEntity);
    if (!camTransform || !camComp) return;

    // Configure camera based on controller type
    if (controllerType == "Platformer2D") {
        // Side-scroller: orthographic, looking along -Z, offset behind player
        camComp->projectionType = ECS::ProjectionType::Orthographic;
        camComp->orthoSize = 8.0f;
        camComp->nearPlane = 0.1f;
        camComp->farPlane = 100.0f;
        camTransform->position = playerPos + Math::Vector3(0.0f, 0.0f, 15.0f);
        camTransform->rotation = Math::Quaternion::Identity();
    } else if (controllerType == "TopDown2D") {
        // Top-down 2D: orthographic, looking along -Z at XY plane
        camComp->projectionType = ECS::ProjectionType::Orthographic;
        camComp->orthoSize = 12.0f;
        camComp->nearPlane = 0.1f;
        camComp->farPlane = 100.0f;
        camTransform->position = playerPos + Math::Vector3(0.0f, 0.0f, 15.0f);
        camTransform->rotation = Math::Quaternion::Identity();
    } else if (controllerType == "TopDown3D") {
        // Isometric/CRPG: perspective, ~45° angle looking down and slightly behind
        camComp->projectionType = ECS::ProjectionType::Perspective;
        camComp->fieldOfView = 50.0f;
        camComp->nearPlane = 0.1f;
        camComp->farPlane = 500.0f;
        f32 distance = 15.0f;
        f32 angle = Math::Radians(45.0f);
        camTransform->position = playerPos + Math::Vector3(
            0.0f,
            distance * Math::Sin(angle),
            distance * Math::Cos(angle));
        // Look down at 45 degrees
        camTransform->rotation = Math::Quaternion(
            Math::Vector3(1.0f, 0.0f, 0.0f), -angle);
    } else if (controllerType == "ThirdPerson") {
        // Over-the-shoulder: perspective, behind and above player
        // Match ThirdPersonController defaults: cameraDistance=5, cameraHeight=2, cameraPitch=20
        camComp->projectionType = ECS::ProjectionType::Perspective;
        camComp->fieldOfView = 60.0f;
        camComp->nearPlane = 0.1f;
        camComp->farPlane = 1000.0f;
        f32 pitchRad = Math::Radians(20.0f);
        f32 dist = 5.0f;
        f32 height = 2.0f;
        camTransform->position = playerPos + Math::Vector3(
            0.0f,
            Math::Sin(pitchRad) * dist + height,
            Math::Cos(pitchRad) * dist);
        camTransform->rotation = Math::Quaternion(
            Math::Vector3(1.0f, 0.0f, 0.0f), -pitchRad);
    } else if (controllerType == "FirstPerson") {
        // First-person: perspective, at player eye height
        camComp->projectionType = ECS::ProjectionType::Perspective;
        camComp->fieldOfView = 75.0f;
        camComp->nearPlane = 0.05f;
        camComp->farPlane = 1000.0f;
        camTransform->position = playerPos + Math::Vector3(0.0f, 1.7f, 0.0f);
        camTransform->rotation = Math::Quaternion::Identity();
    }

    // Make sure the camera is active
    camComp->isActive = true;
    camComp->priority = 10;

    // Select the camera so the user can adjust it
    m_SelectedGameCamera = cameraEntity;
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
            if (m_World->HasComponent<ECS::NameComponent>(entity)) {
                name = m_World->GetComponent<ECS::NameComponent>(entity)->name;
            }
            std::string line = "  [" + std::to_string(entity) + "] " + name;
            if (entity == m_SelectedEntity) line += " (selected)";
            m_ConsoleLog.push_back(line);
        }
    } else if (cmdLower == "select") {
        u64 id = 0;
        if (iss >> id) {
            m_SelectedEntity = static_cast<ECS::Entity>(id);
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
        m_SelectedEntity = entity;
        m_ConsoleLog.push_back("Created entity [" + std::to_string(entity) + "] '" + name + "'");
    } else if (cmdLower == "delete") {
        if (m_SelectedEntity == ECS::INVALID_ENTITY) {
            m_ConsoleLog.push_back("No entity selected");
        } else {
            u64 id = m_SelectedEntity;
            DeleteSelectedEntity();
            m_ConsoleLog.push_back("Deleted entity " + std::to_string(id));
        }
    } else if (cmdLower == "pos") {
        if (m_SelectedEntity == ECS::INVALID_ENTITY || !m_World) {
            m_ConsoleLog.push_back("No entity selected");
            return;
        }
        f32 x, y, z;
        if (iss >> x >> y >> z) {
            auto* transform = m_World->GetComponent<ECS::TransformComponent>(m_SelectedEntity);
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
        const auto& entities = m_World->GetAllEntities();
        u32 meshCount = 0, lightCount = 0, cameraCount = 0;
        u32 totalVerts = 0, totalTris = 0;
        for (ECS::Entity entity : entities) {
            if (m_World->HasComponent<ECS::MeshComponent>(entity)) {
                meshCount++;
                auto* mesh = m_World->GetComponent<ECS::MeshComponent>(entity);
                totalVerts += static_cast<u32>(mesh->vertices.size());
                totalTris += static_cast<u32>(mesh->indices.size()) / 3;
            }
            if (m_World->HasComponent<ECS::LightComponent>(entity)) lightCount++;
            if (m_World->HasComponent<ECS::CameraComponent>(entity)) cameraCount++;
        }
        m_ConsoleLog.push_back("Scene Statistics:");
        m_ConsoleLog.push_back("  Entities: " + std::to_string(entities.size()));
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

void EditorLayer::DrawBuildDialog() {
    ImGui::SetNextWindowSize(ImVec2(550, 500), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Build Game", &m_ShowBuildDialog)) {
        ImGui::End();
        return;
    }

    bool hasProject = !m_SceneManager.GetProjectPath().empty();

    if (!hasProject) {
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "No project loaded. Save a project first.");
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

    // Build key (optional)
    static char buildKey[256] = {};
    ImGui::InputText("Pack Key (optional)", buildKey, sizeof(buildKey),
                     ImGuiInputTextFlags_Password);
    m_BuildConfig.buildKey = buildKey;

    ImGui::Spacing();

    // Project info
    ImGui::Text("Project: %s", m_SceneManager.GetProjectName().c_str());
    ImGui::Text("Scenes: %zu", m_SceneManager.GetSceneCount());

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // --- Build button ---
    bool canBuild = !m_BuildInProgress && !m_BuildConfig.outputDir.empty();
    if (!canBuild) ImGui::BeginDisabled();
    if (ImGui::Button("Build", ImVec2(120, 30))) {
        m_BuildConfig.projectPath = m_SceneManager.GetProjectPath();
        m_BuildInProgress = true;
        m_BuildFinished = false;
        m_BuildProgress = 0.0f;
        m_BuildResult = Build::BuildResult{};

        Build::BuildPipeline pipeline;
        pipeline.SetProgressCallback([this](const std::string& phase, float progress) {
            m_BuildProgressPhase = phase;
            m_BuildProgress = progress;
        });

        m_BuildResult = pipeline.Execute(m_BuildConfig);
        m_BuildInProgress = false;
        m_BuildFinished = true;
    }
    if (!canBuild) ImGui::EndDisabled();

    // Progress bar
    if (m_BuildInProgress || m_BuildFinished) {
        ImGui::SameLine();
        if (m_BuildInProgress) {
            ImGui::ProgressBar(m_BuildProgress, ImVec2(-1, 0),
                               m_BuildProgressPhase.c_str());
        } else if (m_BuildResult.success) {
            ImGui::TextColored(ImVec4(0.2f, 0.9f, 0.2f, 1.0f), "Build succeeded!");
        } else {
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Build failed!");
        }
    }

    // Open output folder button
    if (m_BuildFinished && m_BuildResult.success) {
        ImGui::SameLine();
        if (ImGui::Button("Open Folder")) {
#ifdef ENJIN_PLATFORM_WINDOWS
            std::string cmd = "explorer \"" + m_BuildConfig.outputDir + "\"";
            std::system(cmd.c_str());
#elif defined(ENJIN_PLATFORM_MACOS)
            std::string cmd = "open \"" + m_BuildConfig.outputDir + "\"";
            std::system(cmd.c_str());
#else
            std::string cmd = "xdg-open \"" + m_BuildConfig.outputDir + "\"";
            std::system(cmd.c_str());
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

} // namespace Editor
} // namespace Enjin
