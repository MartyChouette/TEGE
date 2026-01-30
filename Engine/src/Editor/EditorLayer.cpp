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
#include "Enjin/ECS/Components/Vegetation.h"
#include "Enjin/ECS/Components/CameraTrigger.h"
#include "Enjin/ECS/Components/TemperatureZone.h"
#include "Enjin/ECS/Components/Text.h"
#include "Enjin/ECS/Systems/RenderSystem.h"
#include "Enjin/Assets/SceneImporter.h"
#include "Enjin/Scene/SceneSerializer.h"
#include "Enjin/Renderer/MeshFactory.h"
#include "Enjin/Renderer/PostProcessing.h"
#include "Enjin/Platform/Input.h"
#include "Enjin/Platform/FileDialog.h"
#include "Enjin/Math/Math.h"
#include <imgui.h>
#include <ImGuizmo.h>
#include <vulkan/vulkan.h>
#include <sstream>
#include <filesystem>
#include <cstdio>
#include <cstring>
#include <climits>

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

    // Render target for Game View (offscreen rendering)
    // Fixed size - no resize during frame to avoid Vulkan sync issues
    m_GameViewRenderTarget = std::make_unique<Renderer::RenderTarget>();
    if (!m_GameViewRenderTarget->Create(renderer, m_GameViewWidth, m_GameViewHeight)) {
        ENJIN_LOG_WARN(Editor, "Failed to create Game View render target");
        m_GameViewRenderTarget.reset();
    }

    ENJIN_LOG_INFO(Editor, "EditorLayer initialized");
    return true;
}

void EditorLayer::InitializePlayMode() {
    if (m_World && m_Camera && m_CameraController) {
        m_PlayMode.Initialize(m_World, m_Camera, m_CameraController);
    }
}

void EditorLayer::Shutdown() {
    // Destroy render target before ImGui (it uses ImGui textures)
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
    // Track frame time history
    m_LastDeltaTime = deltaTime;
    f32 frameTimeMs = deltaTime * 1000.0f;
    m_FrameTimeHistory[m_FrameTimeIndex] = frameTimeMs;
    m_FrameTimeIndex = (m_FrameTimeIndex + 1) % FRAME_TIME_HISTORY_SIZE;

    // Calculate min/max/avg
    m_FrameTimeMin = m_FrameTimeHistory[0];
    m_FrameTimeMax = m_FrameTimeHistory[0];
    f32 sum = 0.0f;
    for (usize i = 0; i < FRAME_TIME_HISTORY_SIZE; ++i) {
        f32 ft = m_FrameTimeHistory[i];
        if (ft > 0.0f) {  // Skip uninitialized entries
            if (ft < m_FrameTimeMin) m_FrameTimeMin = ft;
            if (ft > m_FrameTimeMax) m_FrameTimeMax = ft;
            sum += ft;
        }
    }
    m_FrameTimeAvg = sum / static_cast<f32>(FRAME_TIME_HISTORY_SIZE);

    // Update wind system (always ticks, affects weather + vegetation + grass)
    m_WindSystem.Update(deltaTime);
    if (m_RenderSystem && !m_RenderSystem->GetWindSystem()) {
        m_RenderSystem->SetWindSystem(&m_WindSystem);
    }

    // Camera controller handles its own input - only fully disable when typing in a text field
    // The camera controller checks for right-mouse before looking, so it's OK to leave it enabled
    if (m_CameraController) {
        // Only disable when user is typing in a text field or using gizmo
        bool usingGizmo = ImGuizmo::IsUsing();
        m_CameraController->SetEnabled(!WantsKeyboardInput() && !usingGizmo);

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
        if (m_FocusMode && m_PlayMode.IsStopped()) {
            m_PlayMode.Play();  // Auto-play when entering focus mode
        }
    }
    if (Input::IsKeyPressed(KeyCode::Escape)) {
        if (m_FocusMode) {
            // Escape exits focus mode back to editor (game keeps playing)
            m_FocusMode = false;
        } else if (m_PlayMode.IsPlaying() || m_PlayMode.IsPaused()) {
            // Escape in editor view stops play mode
            m_PlayMode.Stop();
        }
    }

    // Handle viewport picking (left-click to select, but not when using gizmo)
    // Only allow picking in editor mode, not play mode
    if (!ImGuizmo::IsOver() && m_PlayMode.IsStopped()) {
        HandleViewportPicking();
    }

    // Update play mode
    m_PlayMode.Update(deltaTime);

    // Weather is now updated per-camera in Game View panel (see DrawGameViewPanel)

    // Update splash screen timer
    if (m_ShowSplash) {
        m_SplashTimer += deltaTime;
        if (m_SplashTimer >= m_SplashDuration) {
            m_ShowSplash = false;
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
    } else if (m_PostProcessing) {
        // When retro effects are disabled, clear the retro post-process fields
        auto& settings = m_PostProcessing->GetSettings();
        settings.ditherEnabled = 0;
        settings.colorQuantEnabled = 0;
        settings.resDownscaleEnabled = 0;
        settings.crtEnabled = 0;
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

    // Resize render target if game view panel dimensions changed
    if (m_GameViewRenderTarget->GetWidth() != m_GameViewWidth ||
        m_GameViewRenderTarget->GetHeight() != m_GameViewHeight) {
        if (m_GameViewWidth > 0 && m_GameViewHeight > 0) {
            m_GameViewRenderTarget->Resize(m_GameViewWidth, m_GameViewHeight);
            // Recreate effect pipelines for the (potentially new) render pass
            if (m_RenderSystem) {
                m_RenderSystem->RecreateEffectPipelinesForRenderPass(
                    m_GameViewRenderTarget->GetRenderPass());
            }
        }
    }

    // One-shot: update effect pipelines for the render target's render pass
    if (!m_EffectPipelinesUpdated && m_RenderSystem && m_GameViewRenderTarget->IsValid()) {
        m_RenderSystem->RecreateEffectPipelinesForRenderPass(
            m_GameViewRenderTarget->GetRenderPass());
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

    // Notify render system whether rain is active (drives water ripple shader)
    m_RenderSystem->SetRainActive(isRain);

    // Begin render target pass and render scene geometry + effects
    u32 rtWidth = m_GameViewRenderTarget->GetWidth();
    u32 rtHeight = m_GameViewRenderTarget->GetHeight();

    m_GameViewRenderTarget->Begin(commandBuffer);
    m_RenderSystem->RenderToTarget(m_GameViewRenderTarget.get(), &gameCamera);

    // Render grass inside the render target pass
    m_RenderSystem->RenderGrass(rtWidth, rtHeight);

    // Render weather particles inside the render target pass
    if (hasWeatherParticles) {
        m_RenderSystem->RenderWeatherParticles(m_WeatherSystem, isRain, rtWidth, rtHeight);
    }

    m_GameViewRenderTarget->End(commandBuffer);
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
        m_ImGuiLayer->EndFrame(commandBuffer);
        return;
    }

    // Menu bar
    DrawMenuBar();

    // Calculate layout dimensions
    f32 screenW = io.DisplaySize.x;
    f32 screenH = io.DisplaySize.y;
    f32 menuBarH = 22.0f;
    f32 leftW = screenW * 0.18f;
    f32 rightW = screenW * 0.22f;
    f32 bottomH = screenH * 0.22f;
    f32 centerW = screenW - leftW - rightW;
    f32 centerH = screenH - menuBarH - bottomH;

    // Panels with initial positions (user can freely move/resize after first launch)
    if (HasPanel(m_VisiblePanels, EditorPanel::Hierarchy)) {
        ImGui::SetNextWindowPos(ImVec2(0, menuBarH), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(leftW, centerH), ImGuiCond_FirstUseEver);
        DrawHierarchyPanel();
    }
    if (HasPanel(m_VisiblePanels, EditorPanel::Inspector)) {
        ImGui::SetNextWindowPos(ImVec2(screenW - rightW, menuBarH), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(rightW, centerH * 0.6f), ImGuiCond_FirstUseEver);
        DrawInspectorPanel();
    }
    if (HasPanel(m_VisiblePanels, EditorPanel::Settings)) {
        ImGui::SetNextWindowPos(ImVec2(screenW - rightW, menuBarH + centerH * 0.6f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(rightW, centerH * 0.4f), ImGuiCond_FirstUseEver);
        DrawSettingsPanel();
    }
    if (HasPanel(m_VisiblePanels, EditorPanel::Console)) {
        ImGui::SetNextWindowPos(ImVec2(leftW, screenH - bottomH), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(centerW * 0.5f, bottomH), ImGuiCond_FirstUseEver);
        DrawConsolePanel();
    }
    if (HasPanel(m_VisiblePanels, EditorPanel::AssetBrowser)) {
        ImGui::SetNextWindowPos(ImVec2(leftW + centerW * 0.5f, screenH - bottomH), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(centerW * 0.5f, bottomH), ImGuiCond_FirstUseEver);
        DrawAssetBrowserPanel();
    }
    if (HasPanel(m_VisiblePanels, EditorPanel::PostProcessing)) {
        ImGui::SetNextWindowPos(ImVec2(screenW - rightW - 300, menuBarH + 50), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(280, 400), ImGuiCond_FirstUseEver);
        DrawPostProcessingPanel();
    }
    if (HasPanel(m_VisiblePanels, EditorPanel::Effects)) {
        ImGui::SetNextWindowPos(ImVec2(screenW - rightW - 300, menuBarH + 100), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(280, 450), ImGuiCond_FirstUseEver);
        DrawEffectsPanel();
    }
    if (HasPanel(m_VisiblePanels, EditorPanel::GameView)) {
        ImGui::SetNextWindowPos(ImVec2(leftW + 20, menuBarH + 20), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_FirstUseEver);
        DrawGameViewPanel();
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
            if (ImGui::MenuItem("Exit", "Alt+F4")) {
                if (m_Window) {
                    m_Window->Close();
                }
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Edit")) {
            if (ImGui::MenuItem("Undo", "Ctrl+Z")) {}
            if (ImGui::MenuItem("Redo", "Ctrl+Y")) {}
            ImGui::Separator();
            if (ImGui::MenuItem("Cut", "Ctrl+X")) {}
            if (ImGui::MenuItem("Copy", "Ctrl+C")) {}
            if (ImGui::MenuItem("Paste", "Ctrl+V")) {}
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
            ImGui::Separator();
            ImGui::MenuItem("Stats Overlay", nullptr, &m_ShowStatsOverlay);
            ImGui::MenuItem("ImGui Demo", nullptr, &m_ShowDemoWindow);
            ImGui::Separator();
            if (ImGui::MenuItem("Reset Layout")) {
                m_DockingInitialized = false;  // Will re-layout on next frame
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
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("2D Object")) {
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
                m_FocusMode = true;  // Auto-enter focus mode when playing
            }
        } else if (m_PlayMode.IsPlaying()) {
            if (ImGui::Button(" || Pause ")) {
                m_PlayMode.Pause();
            }
        } else {  // Paused
            if (ImGui::Button(" > Resume ")) {
                m_PlayMode.Resume();
                m_FocusMode = true;  // Re-enter focus on resume
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
        ImGui::Text("Enjin Engine");
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
        ImGui::Separator();

        // Transform component
        if (m_World->HasComponent<ECS::TransformComponent>(m_SelectedEntity)) {
            DrawTransformComponent(m_SelectedEntity);
        }

        // Mesh component
        if (m_World->HasComponent<ECS::MeshComponent>(m_SelectedEntity)) {
            DrawMeshComponent(m_SelectedEntity);
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
        if (m_World->HasComponent<ECS::AudioSourceComponent>(m_SelectedEntity)) {
            DrawAudioSourceComponent(m_SelectedEntity);
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
        if (m_World->HasComponent<ECS::DialogueComponent>(m_SelectedEntity)) {
            DrawDialogueComponent(m_SelectedEntity);
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
                    }
                }
                if (!m_World->HasComponent<ECS::TopDown2DController>(m_SelectedEntity)) {
                    if (ImGui::MenuItem("2D Top-Down")) {
                        m_World->AddComponent<ECS::TopDown2DController>(m_SelectedEntity);
                    }
                }
                if (!m_World->HasComponent<ECS::TopDown3DController>(m_SelectedEntity)) {
                    if (ImGui::MenuItem("3D Top-Down")) {
                        m_World->AddComponent<ECS::TopDown3DController>(m_SelectedEntity);
                    }
                }
                if (!m_World->HasComponent<ECS::ThirdPersonController>(m_SelectedEntity)) {
                    if (ImGui::MenuItem("3D Third Person")) {
                        m_World->AddComponent<ECS::ThirdPersonController>(m_SelectedEntity);
                    }
                }
                if (!m_World->HasComponent<ECS::FirstPersonController>(m_SelectedEntity)) {
                    if (ImGui::MenuItem("3D First Person")) {
                        m_World->AddComponent<ECS::FirstPersonController>(m_SelectedEntity);
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

            ImGui::Text("Metallic/Roughness: %d", material->metallicRoughnessTexture);
            ImGui::Text("Emissive: %d", material->emissiveTexture);
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

        // Bounding box
        f32 extents[3] = { volume->halfExtents.x, volume->halfExtents.y, volume->halfExtents.z };
        if (ImGui::DragFloat3("Half Extents", extents, 0.5f, 0.1f, 500.0f)) {
            volume->halfExtents = Math::Vector3(extents[0], extents[1], extents[2]);
        }
        ImGui::DragInt("Priority", &volume->priority, 1, -100, 100);

        ImGui::Separator();

        // Water settings
        f32 waterCol[3] = { volume->waterColor.x, volume->waterColor.y, volume->waterColor.z };
        if (ImGui::ColorEdit3("Water Color", waterCol)) {
            volume->waterColor = Math::Vector3(waterCol[0], waterCol[1], waterCol[2]);
        }
        ImGui::SliderFloat("Opacity", &volume->opacity, 0.0f, 1.0f);
        ImGui::DragFloat("Wave Speed", &volume->waveSpeed, 0.1f, 0.0f, 10.0f);
        ImGui::DragFloat("Wave Height", &volume->waveHeight, 0.01f, 0.0f, 2.0f);

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

    // Add a colored title bar when playing
    bool isPlaying = m_PlayMode.IsPlaying();
    if (isPlaying) {
        ImGui::PushStyleColor(ImGuiCol_TitleBg, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_TitleBgActive, ImVec4(0.3f, 0.7f, 0.3f, 1.0f));
    }

    ImGui::Begin("Game View");

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

        // Main title "THE ENJIN ENGINE"
        const char* title = "THE ENJIN ENGINE";
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
    }
    ImGui::End();

    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
}

void EditorLayer::ImportModel(const std::string& path) {
    if (!m_World) {
        ENJIN_LOG_ERROR(Editor, "Cannot import model: no world loaded");
        m_ConsoleLog.push_back("[Error] Cannot import model: no world loaded");
        return;
    }

    Assets::ImportOptions options;
    options.scale = 1.0f;

    Assets::ImportResult result = Assets::SceneImporter::ImportGLTF(path, m_World, options);

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
    auto result = serializer.Save(path);

    if (result.success) {
        m_CurrentScenePath = path;
        usize entityCount = m_World->GetAllEntities().size();
        std::stringstream ss;
        ss << "[Info] Saved scene to " << path << " (" << entityCount << " entities)";
        m_ConsoleLog.push_back(ss.str());
        ENJIN_LOG_INFO(Editor, "Saved scene to %s (%zu entities)", path.c_str(), entityCount);
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

    if (result.success) {
        m_CurrentScenePath = path;
        m_SelectedEntity = ECS::INVALID_ENTITY;
        usize entityCount = result.entities.size();
        std::stringstream ss;
        ss << "[Info] Loaded scene from " << path << " (" << entityCount << " entities)";
        m_ConsoleLog.push_back(ss.str());
        ENJIN_LOG_INFO(Editor, "Loaded scene from %s (%zu entities)", path.c_str(), entityCount);
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
    if (ImGui::CollapsingHeader("2D Platformer Controller", ImGuiTreeNodeFlags_DefaultOpen)) {
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
    if (ImGui::CollapsingHeader("2D Top-Down Controller", ImGuiTreeNodeFlags_DefaultOpen)) {
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
    if (ImGui::CollapsingHeader("3D Top-Down Controller", ImGuiTreeNodeFlags_DefaultOpen)) {
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
    if (ImGui::CollapsingHeader("3D Third Person Controller", ImGuiTreeNodeFlags_DefaultOpen)) {
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
    if (ImGui::CollapsingHeader("3D First Person Controller", ImGuiTreeNodeFlags_DefaultOpen)) {
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
    if (ImGui::CollapsingHeader("Health", ImGuiTreeNodeFlags_DefaultOpen)) {
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
    if (ImGui::CollapsingHeader("Rigidbody", ImGuiTreeNodeFlags_DefaultOpen)) {
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
    if (ImGui::CollapsingHeader("Box Collider", ImGuiTreeNodeFlags_DefaultOpen)) {
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
    if (ImGui::CollapsingHeader("Audio Source", ImGuiTreeNodeFlags_DefaultOpen)) {
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
    if (ImGui::CollapsingHeader("Sprite 2D", ImGuiTreeNodeFlags_DefaultOpen)) {
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
    if (ImGui::CollapsingHeader("Animated Sprite 2D", ImGuiTreeNodeFlags_DefaultOpen)) {
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
    if (ImGui::CollapsingHeader("Tilemap", ImGuiTreeNodeFlags_DefaultOpen)) {
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
    if (ImGui::CollapsingHeader("State Machine", ImGuiTreeNodeFlags_DefaultOpen)) {
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
    if (ImGui::CollapsingHeader("Dialogue", ImGuiTreeNodeFlags_DefaultOpen)) {
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

} // namespace Editor
} // namespace Enjin
