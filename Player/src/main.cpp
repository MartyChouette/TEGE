#include "Enjin/Core/Application.h"
#include "Enjin/Core/Version.h"
#include "Enjin/Debug/CrashHandler.h"
#include "Enjin/Logging/Log.h"
#include "Enjin/Platform/Input.h"
#include "Enjin/Platform/Paths.h"
#include "Enjin/Platform/Window.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Systems/RenderSystem.h"
#include "Enjin/ECS/Components/Camera.h"
#include "Enjin/ECS/Components/Light.h"
#include "Enjin/ECS/Components/Name.h"
#include "Enjin/ECS/Components/Text.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/Renderer/Skybox.h"
#include "Enjin/Renderer/Vulkan/VulkanRenderer.h"
#include "Enjin/Renderer/Camera.h"
#include "Enjin/Renderer/CameraController.h"
#include "Enjin/Scene/SceneSerializer.h"
#include "Enjin/Scene/SceneManager.h"
#include "Enjin/Scene/LevelStreaming.h"
#include "Enjin/Input/InputAction.h"
#include "Enjin/Input/MIDIInput.h"
#include "Enjin/GUI/GameMenus.h"
#include "Enjin/GUI/ImGuiLayer.h"
#include "Enjin/GUI/UITemplates.h"
#include "Enjin/GUI/UISystem.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include <imgui.h>
#include <backends/imgui_impl_vulkan.h>
#include "Enjin/Effects/Weather.h"
#include "Enjin/Effects/Destructible.h"
#include "Enjin/Effects/InteractiveWater.h"
#include "Enjin/Effects/Wind.h"
#include "Enjin/Effects/FluidSimulation.h"
#include "Enjin/Effects/FluidTerrainCoupling.h"
#include "Enjin/Effects/CurlNoiseSystem.h"
#include "Enjin/Effects/ElementalSystem.h"
#include "Enjin/Audio/AudioReactiveSystem.h"
#include "Enjin/Effects/WorldTime.h"
#include "Enjin/Effects/SeasonalWeather.h"
#include "Enjin/Effects/Water.h"
#include "Enjin/ECS/Components/Water3D.h"
#include "Enjin/Audio/AudioSystem.h"
#include "Enjin/Audio/SimpleAudio.h"
#include "Enjin/Build/AssetReader.h"
#include "Enjin/Scripting/ScriptEngine.h"
#include "Enjin/GUI/EngineSplash.h"
#include "Enjin/Scripting/ScriptSystem.h"
#include "Enjin/Scripting/ScriptBindings.h"
#include "Enjin/Scripting/FlashAPIShim.h"
#include "Enjin/Scripting/CoroutineScheduler.h"
#include "Enjin/Scripting/ScriptEvents.h"
#include "Enjin/ECS/Systems/DialogueSystem.h"
#include "Enjin/Physics/IPhysicsBackend.h"
#include "Enjin/Physics/IPhysicsBackend2D.h"
#include "Enjin/Physics/PhysicsBackendFactory.h"
#include "Enjin/Physics/PhysicsBackendType.h"
#include "Enjin/ECS/Systems/ControllerSystem.h"
#include "Enjin/ECS/Systems/FlowerSystem.h"
#include "Enjin/ECS/Systems/TweenSystem.h"
#include "Enjin/ECS/Systems/StateMachineSystem.h"
#include "Enjin/ECS/Systems/VisualScriptSystem.h"
#include "Enjin/ECS/Systems/BehaviorTreeSystem.h"
#include "Enjin/ECS/Systems/AISystem.h"
#include "Enjin/Gameplay/RecordRewindSystem.h"
#include "Enjin/ECS/EntityEventBus.h"
#include "Enjin/Gameplay/QuestSystem.h"
#include "Enjin/Gameplay/FootstepSystem.h"
#include "Enjin/Gameplay/CinematicSystem.h"
#include "Enjin/Gameplay/ObjectPool.h"
#include "Enjin/Gameplay/TieredSaveSystem.h"
#include "Enjin/Gameplay/QuestFlow.h"
#include "Enjin/Networking/NetworkSystem.h"
#include "Enjin/Effects/ParticleSystem.h"
#include "Enjin/Accessibility/SubtitleSystem.h"
#include "Enjin/Accessibility/AlternativeInput.h"
#include "Enjin/Accessibility/AudioVisualIndicator.h"
#include "Enjin/Accessibility/ContentWarning.h"
#include "Enjin/Accessibility/Announcer.h"
#include "Enjin/Accessibility/AccessibilitySettings.h"
#include "Enjin/Accessibility/FontLibrary.h"
#include "Enjin/Accessibility/ColorblindPalette.h"
#include "Enjin/Renderer/PostProcessing.h"
#include "Enjin/Renderer/RenderTarget.h"
#include "Enjin/Renderer/RayTracing/PathTracer.h"
#include "Enjin/Renderer/RayTracing/RTHybridApply.h"
#include "Enjin/Renderer/RayTracing/RTCompositor.h"
#include "Enjin/ECS/Components/PostProcessVolume.h"
#include "Enjin/Audio/AudioEventGraph.h"
#include "Enjin/Assets/DataAsset.h"
#include "Enjin/Assets/MeshAssetCache.h"
#include "Enjin/Gameplay/GameplayLoop.h"
#include <nlohmann/json.hpp>
#include <iostream>
#include <memory>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>

namespace fs = std::filesystem;

// Crash context — file-scope pointers for function-pointer providers
static Enjin::ECS::World* s_CrashWorld = nullptr;
static char s_PlayerGPUNameBuf[256] = {};
static char s_PlayerSceneNameBuf[256] = {};

// Standalone game player — no editor, no ImGui
class GamePlayer : public Enjin::Application {
public:
    void Initialize() override {
        ENJIN_LOG_INFO(Player, "Enjin Player starting...");

        // Try to open assets: first check for .enjpak, then fall back to loose files
        std::string exeDir = Enjin::Platform::GetExecutableDirectory();
        std::string pakPath = (fs::path(exeDir) / "game.enjpak").string();
        std::string looseManifestPath = (fs::path(exeDir) / "game.manifest").string();

        if (fs::exists(pakPath)) {
            // Packed mode — open .enjpak (try with default key, then empty key for PackedOpen)
            if (!m_AssetReader.Open(pakPath, PACK_KEY)) {
                // Retry with empty key (PackedOpen mode)
                if (!m_AssetReader.Open(pakPath, "")) {
                    ENJIN_LOG_FATAL(Player, "Failed to open game.enjpak from: %s", pakPath.c_str());
                    return;
                }
            }

            // Read build manifest from pack
            auto manifestData = m_AssetReader.ReadFile("_build/manifest.json");
            if (manifestData.empty()) {
                ENJIN_LOG_FATAL(Player, "Build manifest missing from pack");
                return;
            }

            try {
                std::string manifestStr(manifestData.begin(), manifestData.end());
                auto manifest = nlohmann::json::parse(manifestStr);
                ParseManifestJson(manifest);
            } catch (const std::exception& e) {
                ENJIN_LOG_ERROR(Player, "Error parsing build manifest: %s", e.what());
                return;
            }
        } else if (fs::exists(looseManifestPath)) {
            // Loose files mode — read game.manifest from disk
            m_LooseFilesMode = true;
            m_LooseFilesDir = exeDir;
            ENJIN_LOG_INFO(Player, "No .enjpak found, using loose files from: %s", exeDir.c_str());

            try {
                std::ifstream manifestFile(looseManifestPath);
                if (!manifestFile.is_open()) {
                    ENJIN_LOG_FATAL(Player, "Failed to open game.manifest from: %s", looseManifestPath.c_str());
                    return;
                }
                auto manifest = nlohmann::json::parse(manifestFile);
                manifestFile.close();
                ParseManifestJson(manifest);
            } catch (const std::exception& e) {
                ENJIN_LOG_ERROR(Player, "Error parsing game.manifest: %s", e.what());
                return;
            }
        } else {
            ENJIN_LOG_FATAL(Player, "No game.enjpak or game.manifest found in: %s", exeDir.c_str());
            return;
        }

        // Window title is set via WindowDesc at creation time in Application::Run()
        // (no SetTitle method on Window — title comes from WindowDesc.title)

        // Initialize Vulkan renderer
        m_Renderer = std::make_unique<Enjin::Renderer::VulkanRenderer>();
        if (!m_Renderer->Initialize(GetWindow())) {
            ENJIN_LOG_FATAL(Player, "Failed to initialize Vulkan renderer");
            m_Renderer.reset();
            return;
        }

        GetWindow()->SetResizeCallback([this](Enjin::u32, Enjin::u32) {
            if (m_Renderer) {
                m_Renderer->SetFramebufferResized(true);
            }
        });

        // Apply VSync setting
        if (m_Renderer->GetSwapchain()) {
            // Only apply VSync if not uncapped
            bool useVSync = (m_TargetFPS != 0) && m_VSync;
            m_Renderer->GetSwapchain()->SetVSyncEnabled(useVSync);
        }

        // Set up frame rate limiting callback
        SetTargetFPSCallback([this]() -> Enjin::f32 {
            // Handle background behavior when unfocused
            if (!IsFocused()) {
                switch (m_BackgroundBehavior) {
                    case 2: // Pause
                        return 5.0f; // Very low FPS when paused
                    case 1: // ReduceTo30
                        return 30.0f;
                    case 0: // RunNormally
                    default:
                        break;
                }
            }

            // Return target FPS (0 = uncapped)
            return m_TargetFPS > 0 ? static_cast<Enjin::f32>(m_TargetFPS) : 0.0f;
        });

        // Setup camera
        m_Camera = std::make_unique<Enjin::Renderer::Camera>();
        m_Camera->SetPerspective(45.0f, 16.0f / 9.0f, 0.1f, 1000.0f);
        m_Camera->SetPosition(Enjin::Math::Vector3(0.0f, 2.5f, 7.0f));

        m_CameraController = std::make_unique<Enjin::Renderer::CameraController>(m_Camera.get());

        // Create ECS world
        m_World = std::make_unique<Enjin::ECS::World>();

        // Setup render system
        m_RenderSystem = m_World->RegisterSystem<Enjin::ECS::RenderSystem>(m_World.get(), m_Renderer.get());
        m_RenderSystem->SetCamera(m_Camera.get());
        // Disable ray tracing in built games — RT compute shaders use
        // placeholder SPIR-V that crashes the NVIDIA driver.
        m_RenderSystem->SetRayTracingEnabled(false);
        m_RenderSystem->SetPlayerMode(true);  // Skip GPU compute shaders not embedded in builds
        m_RenderSystem->Initialize();

        // 120-FPS-No-Matter-What pillar: hold the frame rate by scaling shadow quality
        // under load (adr / AdaptiveQualitySystem). Target 60 by default; a high-refresh
        // build can raise it. Shipped games opt in here; the editor leaves it off.
        m_RenderSystem->SetAdaptiveQualityTargetFPS(60.0f);
        m_RenderSystem->SetAdaptiveQualityEnabled(true);

        // Initialize ImGui layer for pause menu and dialogue overlays
        m_ImGuiLayer = std::make_unique<Enjin::GUI::ImGuiLayer>();
        if (!m_ImGuiLayer->Initialize(GetWindow(), m_Renderer.get())) {
            ENJIN_LOG_WARN(Player, "Failed to initialize ImGui layer — menus will not render");
            m_ImGuiLayer.reset();
        }

        // Connect game menu to input system
        m_GameMenu.SetInputMap(&m_InputMap);
        m_GameMenu.SetGameTitle(m_WindowTitle.empty() ? "Game" : m_WindowTitle);

        // Restore the player's saved control bindings (bindings.json next to the
        // exe — written whenever the Options menu closes after a rebind).
        LoadInputBindings();

        // Wire menu button callbacks for title screen and pause menu
        // The UICanvas game-over screen (spawned by GameplayLoop, one UI source on
        // all platforms) dispatches "gameover_restart" on the UI event bus.
        m_UISystem.GetEventBus().Listen("gameover_restart",
            [this](const Enjin::GUI::UIEventData&) { RestartGameSession(); });
        m_UISystem.GetEventBus().Listen("pause_resume",
            [this](const Enjin::GUI::UIEventData&) { ClosePauseMenu(); });
        // Authored MainMenu canvas buttons (UITemplates::CreateMainMenu events).
        // Hide (not destroy) the canvas -- it's authored scene content.
        auto startFromAuthoredMenu = [this]() {
            Enjin::ECS::Entity menu = FindAuthoredMainMenu();
            if (menu != Enjin::ECS::INVALID_ENTITY) {
                if (auto* c = m_World->GetComponent<Enjin::GUI::UICanvasComponent>(menu)) c->visible = false;
            }
            m_GameStarted = true;
            if (SceneWantsMouseCapture()) Enjin::Input::SetMouseCaptured(true);
        };
        m_UISystem.GetEventBus().Listen("menu_newgame",
            [startFromAuthoredMenu](const Enjin::GUI::UIEventData&) { startFromAuthoredMenu(); });
        m_UISystem.GetEventBus().Listen("menu_continue",
            [startFromAuthoredMenu](const Enjin::GUI::UIEventData&) { startFromAuthoredMenu(); });
        m_UISystem.GetEventBus().Listen("menu_options",
            [this](const Enjin::GUI::UIEventData&) {
                m_GameMenu.ShowScreen(Enjin::GUI::MenuScreen::Options);
            });
        m_UISystem.GetEventBus().Listen("menu_quit",
            [this](const Enjin::GUI::UIEventData&) {
                if (GetWindow()) GetWindow()->Close();
            });
        m_UISystem.GetEventBus().Listen("pause_options",
            [this](const Enjin::GUI::UIEventData&) {
                m_GameMenu.ShowScreen(Enjin::GUI::MenuScreen::Options);
            });
        m_UISystem.GetEventBus().Listen("pause_quit",
            [this](const Enjin::GUI::UIEventData&) {
                ClosePauseMenu();
                m_GameMenu.ShowScreen(Enjin::GUI::MenuScreen::MainMenu);
                m_GameStarted = false;
                Enjin::Input::SetMouseCaptured(false);
            });

        // Bridge every UI event into the script event bus so game scripts can
        // react to authored buttons/sliders via Events_Listen("<onClickEvent>", ...).
        m_UISystem.GetEventBus().SetForwarder([this](const Enjin::GUI::UIEventData& e) {
            Enjin::Scripting::EventData data;
            data.SetString("source", "ui");
            data.SetEntity("canvas", static_cast<Enjin::u64>(e.canvasEntity));
            data.SetInt("elementId", static_cast<Enjin::i32>(e.elementId));
            data.SetFloat("value", e.floatValue);
            data.SetInt("checked", e.boolValue ? 1 : 0);
            data.SetString("text", e.stringValue);
            m_ScriptEventBus.Send(e.eventName, data);
        });

        m_GameMenu.SetCallback([this](const std::string& action) {
            if (action == "new_game" || action == "continue") {
                m_GameMenu.HideAll();
                m_GameStarted = true;
                if (SceneWantsMouseCapture()) Enjin::Input::SetMouseCaptured(true);
            } else if (action == "resume") {
                m_GameMenu.HideAll();
                if (SceneWantsMouseCapture()) Enjin::Input::SetMouseCaptured(true);
            } else if (action == "options") {
                m_GameMenu.ShowScreen(Enjin::GUI::MenuScreen::Options);
            } else if (action == "how_to_play") {
                m_GameMenu.ShowScreen(Enjin::GUI::MenuScreen::HowToPlay);
            } else if (action == "restart") {
                RestartGameSession();
            } else if (action == "quit_to_menu") {
                m_GameMenu.ShowScreen(Enjin::GUI::MenuScreen::MainMenu);
                m_GameStarted = false;
                Enjin::Input::SetMouseCaptured(false);
            } else if (action == "quit") {
                if (GetWindow()) GetWindow()->Close();
            } else if (action == "game_over_restart") {
                RestartGameSession();
            } else if (action == "game_over_menu") {
                m_GameMenu.ShowScreen(Enjin::GUI::MenuScreen::MainMenu);
                m_GameStarted = false;
                Enjin::Input::SetMouseCaptured(false);
            }
        });

        // Fill the Options menu from live state when it opens — otherwise Back
        // applies the menu's struct defaults (bloom=true) over the scene's
        // render settings every time the player visits Options.
        m_GameMenu.SetSettingsSyncCallback([this](Enjin::GUI::GraphicsSettings& gfx,
                                                  Enjin::GUI::AudioSettings& audio) {
            if (m_PostProcessing) {
                gfx.bloom = m_PostProcessing->GetSettings().bloomEnabled != 0;
                gfx.fxaa = m_PostProcessing->GetSettings().fxaaEnabled != 0;
            }
            if (m_RenderSystem) gfx.shadows = m_RenderSystem->IsShadowsEnabled();
            audio.masterVolume = m_SimpleAudio.GetMasterVolume();
        });

        // Accessibility tab: the menu edits the live settings struct in place;
        // every change re-applies the boot-time consumers and persists.
        m_GameMenu.SetAccessibilitySettings(&m_AccessibilitySettings);
        m_GameMenu.SetAccessibilityChangedCallback([this]() {
            ApplyAccessibilitySettings();
            SaveAccessibilitySettings();
            // The tab's one-handed/gamepad preset buttons rewrite bindings
            SaveInputBindings();
        });

        // Apply graphics/audio settings when user exits Options menu.
        // Some changes (VSync, fullscreen) must be deferred — they recreate the
        // swapchain/window, which is unsafe mid-frame. Store pending changes and
        // apply them at the start of the next Update().
        m_GameMenu.SetSettingsCallback([this](const Enjin::GUI::GraphicsSettings& gfx,
                                              const Enjin::GUI::AudioSettings& audio) {
            // Persist any Controls-tab rebinds along with the settings exit
            SaveInputBindings();
            // --- Audio (safe to apply immediately) ---
            m_SimpleAudio.SetMasterVolume(audio.masterMute ? 0.0f : audio.masterVolume);
            m_SimpleAudio.SetChannelVolume(Enjin::Audio::AudioChannel::Music, audio.musicMute ? 0.0f : audio.musicVolume);
            m_SimpleAudio.SetChannelVolume(Enjin::Audio::AudioChannel::SFX, audio.sfxMute ? 0.0f : audio.sfxVolume);
            m_SimpleAudio.SetChannelVolume(Enjin::Audio::AudioChannel::Voice, audio.voiceMute ? 0.0f : audio.voiceVolume);

            if (!m_Renderer || !m_RenderSystem) return;

            // --- VSync (deferred — recreates swapchain) ---
            m_Renderer->RequestVSyncChange(gfx.vsync);

            // --- Fullscreen (deferred to next Update) ---
            m_PendingFullscreen = gfx.fullscreen;
            m_FullscreenChangeRequested = true;

            // --- Shadows (safe — just flags) ---
            m_RenderSystem->SetShadowsEnabled(gfx.shadows);
            if (gfx.shadows) {
                static const Enjin::u32 shadowRes[] = { 512, 1024, 2048, 4096 };
                Enjin::u32 idx = gfx.shadowQuality < 4 ? gfx.shadowQuality : 2;
                m_RenderSystem->SetShadowResolution(shadowRes[idx]);
            }

            // --- Post-processing (safe — just flags) ---
            if (m_PostProcessing) {
                m_PostProcessing->GetSettings().bloomEnabled = gfx.bloom;
                m_PostProcessing->GetSettings().fxaaEnabled = gfx.fxaa;
            }

            // --- FOV (applied to the active camera next frame) ---
            m_PendingFOV = gfx.fieldOfView;

            ENJIN_LOG_INFO(Player, "Settings applied: vsync=%d fullscreen=%d fov=%.0f shadows=%d bloom=%d fxaa=%d",
                (int)gfx.vsync, (int)gfx.fullscreen, gfx.fieldOfView, (int)gfx.shadows, (int)gfx.bloom, (int)gfx.fxaa);
        });

        // Initialize scripting engine. Anchor all roots to the exe directory —
        // the CWD is whatever launched us (shortcuts, double-click from another
        // folder), and everything the build emits sits next to the exe.
        std::string gameRoot = Enjin::Platform::GetExecutableDirectory();
        if (m_ScriptEngine.Initialize()) {
            Enjin::Scripting::RegisterAllBindings(m_ScriptEngine.GetASEngine());
            m_ScriptEngine.SetWorld(m_World.get());
            m_ScriptEngine.SetScriptDirectory(
                (std::filesystem::path(gameRoot) / "scripts").string());

            m_ScriptSystem.SetWorld(m_World.get());
            m_ScriptSystem.SetScriptEngine(&m_ScriptEngine);
            m_ScriptSystem.SetCoroutineScheduler(&m_CoroutineScheduler);
            m_ScriptSystem.SetScriptRoot(gameRoot);

            m_CoroutineScheduler.SetEngine(m_ScriptEngine.GetASEngine());
            m_ScriptEventBus.SetScriptEngine(&m_ScriptEngine);

            ENJIN_LOG_INFO(Player, "Script engine initialized");
        } else {
            ENJIN_LOG_WARN(Player, "Failed to initialize script engine");
        }

        // Initialize physics with backend from build manifest
        auto backendType = static_cast<Enjin::Physics::PhysicsBackendType>(
            m_PhysicsBackendType <= 3 ? m_PhysicsBackendType : 0);
        auto projectMode = static_cast<Enjin::Scene::ProjectMode>(
            m_ProjectMode <= 2 ? m_ProjectMode : 1);
        m_Physics = Enjin::Physics::CreatePhysicsBackend(backendType, projectMode);
        if (m_Physics) m_Physics->SetWorld(m_World.get());
        m_Physics2D = Enjin::Physics::CreatePhysicsBackend2D(backendType, projectMode);
        if (m_Physics2D) m_Physics2D->Initialize(m_World.get());

        // Initialize gameplay systems
        m_ControllerSystem.SetWorld(m_World.get());
        m_ControllerSystem.SetCamera(m_Camera.get());
        m_ControllerSystem.SetPhysics(m_Physics.get());
        m_ControllerSystem.SetPhysics2D(m_Physics2D.get());
        m_ControllerSystem.SetInputActionMap(&m_InputMap);

        m_FlowerSystem.SetWorld(m_World.get());
        m_FlowerSystem.SetCamera(m_Camera.get());
        m_FlowerSystem.SetRenderSystem(m_RenderSystem);

        m_TweenSystem.SetScriptEngine(&m_ScriptEngine);
        m_StateMachineSystem.SetScriptEngine(&m_ScriptEngine);

        m_VisualScriptSystem.SetWorld(m_World.get());
        m_BehaviorTreeSystem.SetWorld(m_World.get());
        m_AISystem.SetWorld(m_World.get());
        m_RecordRewindSystem.SetWorld(m_World.get());
        m_RecordRewindSystem.SetPhysics(m_Physics.get());
        m_RecordRewindSystem.SetPhysics2D(m_Physics2D.get());

        // Initialize save system with local backend
        m_TieredSaveSystem.LoadMeta();

        // Initialize systems needed for script bindings
        m_SimpleAudio.Initialize();
        m_SimpleAudio.SetWorld(m_World.get());
        m_SimpleAudio.SetAssetRoot(gameRoot);
        // Resolve project-relative mesh references against the game root (loose assets
        // ship next to the exe). NOTE: for this to work in an exported game the source
        // mesh files must ship with the build; otherwise reference-mode scenes need to
        // be baked inline at build time. Tracked as a follow-up.
        Enjin::Assets::MeshAssetCache::Get().SetSearchRoot(gameRoot);
        m_WeatherSystem.Initialize();
        m_ElementalSystem.Initialize(&m_WindSystem, &m_WeatherSystem, &m_SeasonalWeather);
        m_FireLights.reserve(Enjin::Effects::ElementalSystem::MAX_FIRE_LIGHTS);
        m_AudioReactiveSystem.SetWorld(m_World.get());
        m_AudioReactiveSystem.SetAudio(&m_SimpleAudio);
        m_AudioReactiveSystem.SetMIDI(&m_MIDIInput);
        m_DestructibleSystem.Initialize(m_World.get());
        m_StreamingManager.SetWorld(m_World.get());
        m_SceneManager.SetWorld(m_World.get());
        if (!m_LooseFilesMode) {
            m_SceneManager.SetAssetReader(&m_AssetReader);
        }

        // Populate scene list from project manifest
        {
            std::string projStr;
            if (m_LooseFilesMode) {
                // Read project manifest from disk
                std::string projPath = (fs::path(m_LooseFilesDir) / "project.enjinproject").string();
                std::ifstream projFile(projPath);
                if (projFile.is_open()) {
                    projStr = std::string((std::istreambuf_iterator<char>(projFile)),
                                           std::istreambuf_iterator<char>());
                    projFile.close();
                }
            } else {
                // Read from .enjpak
                auto projData = m_AssetReader.ReadFile("project.enjinproject");
                if (!projData.empty()) {
                    projStr = std::string(projData.begin(), projData.end());
                }
            }

            if (!projStr.empty()) {
                try {
                    auto projJson = nlohmann::json::parse(projStr);
                    if (projJson.contains("scenes") && projJson["scenes"].is_array()) {
                        for (const auto& sj : projJson["scenes"]) {
                            Enjin::Scene::SceneEntry entry;
                            entry.name = sj.value("name", "Unnamed Scene");
                            entry.path = sj.value("path", "");
                            entry.buildIndex = sj.value("buildIndex", -1);
                            entry.isStartScene = sj.value("isStartScene", false);
                            m_SceneManager.AddScene(entry);
                        }
                    }
                    // Load collision group names
                    if (projJson.contains("collisionGroups") && projJson["collisionGroups"].is_array()) {
                        auto& groups = m_SceneManager.GetCollisionGroupNames();
                        const auto& cg = projJson["collisionGroups"];
                        for (Enjin::usize i = 0; i < cg.size() && i < 32; ++i) {
                            groups[i] = cg[i].get<std::string>();
                        }
                    }
                    ENJIN_LOG_INFO(Player, "Loaded %zu scenes from project manifest",
                        m_SceneManager.GetSceneCount());
                } catch (const std::exception& e) {
                    ENJIN_LOG_ERROR(Player, "Failed to parse project manifest: %s", e.what());
                }
            } else {
                ENJIN_LOG_WARN(Player, "No project.enjinproject found — SceneManager scene list empty");
            }
        }

        // Load data assets (.enjschema and .enjdata)
        {
            auto& registry = Enjin::Assets::DataAssetRegistry::Get();
            Enjin::u32 schemaCount = 0, assetCount = 0;

            // Collect file list — from pack or from filesystem
            std::vector<std::string> fileList;
            if (m_LooseFilesMode) {
                try {
                    for (auto& entry : fs::recursive_directory_iterator(m_LooseFilesDir,
                             fs::directory_options::skip_permission_denied)) {
                        if (!entry.is_regular_file()) continue;
                        auto rel = fs::relative(entry.path(), fs::path(m_LooseFilesDir));
                        fileList.push_back(rel.generic_string());
                    }
                } catch (const std::exception& e) {
                    ENJIN_LOG_WARN(Player, "Error scanning loose files: %s", e.what());
                }
            } else {
                fileList = m_AssetReader.ListFiles();
            }

            // Helper: read a file from pack or disk
            auto readFileContent = [&](const std::string& virtualPath) -> std::string {
                if (m_LooseFilesMode) {
                    std::string fullPath = (fs::path(m_LooseFilesDir) / virtualPath).string();
                    std::ifstream f(fullPath);
                    if (!f.is_open()) return "";
                    return std::string((std::istreambuf_iterator<char>(f)),
                                        std::istreambuf_iterator<char>());
                } else {
                    auto data = m_AssetReader.ReadFile(virtualPath);
                    if (data.empty()) return "";
                    return std::string(data.begin(), data.end());
                }
            };

            for (const auto& file : fileList) {
                if (file.size() > 11 && file.substr(file.size() - 11) == ".enjschema") {
                    std::string str = readFileContent(file);
                    if (!str.empty()) {
                        try {
                            auto j = nlohmann::json::parse(str);
                            Enjin::Assets::DataAssetSchema schema;
                            schema.name = j.value("name", "");
                            schema.description = j.value("description", "");
                            if (j.contains("fields") && j["fields"].is_array()) {
                                for (const auto& fj : j["fields"]) {
                                    Enjin::Assets::DataAssetField field;
                                    field.name = fj.value("name", "");
                                    field.type = Enjin::Assets::DataFieldTypeFromString(fj.value("type", "String"));
                                    schema.fields.push_back(field);
                                }
                            }
                            if (!schema.name.empty()) {
                                registry.RegisterSchema(schema);
                                schemaCount++;
                            }
                        } catch (const std::exception& e) {
                            ENJIN_LOG_ERROR(Player, "Failed to parse schema '%s': %s", file.c_str(), e.what());
                        }
                    }
                }
            }
            for (const auto& file : fileList) {
                if (file.size() > 9 && file.substr(file.size() - 9) == ".enjdata") {
                    std::string str = readFileContent(file);
                    if (!str.empty()) {
                        try {
                            auto j = nlohmann::json::parse(str);
                            Enjin::Assets::DataAsset asset;
                            asset.name = j.value("name", "");
                            asset.schemaName = j.value("schema", "");
                            asset.filePath = file;
                            // Parse values map
                            if (j.contains("values") && j["values"].is_object()) {
                                for (auto& [key, val] : j["values"].items()) {
                                    if (val.is_string()) {
                                        asset.values[key] = val.get<std::string>();
                                    } else if (val.is_number_float()) {
                                        asset.values[key] = val.get<Enjin::f32>();
                                    } else if (val.is_number_integer()) {
                                        asset.values[key] = val.get<Enjin::i32>();
                                    } else if (val.is_boolean()) {
                                        asset.values[key] = val.get<bool>();
                                    }
                                }
                            }
                            if (!asset.name.empty()) {
                                registry.CreateAsset(asset);
                                assetCount++;
                            }
                        } catch (const std::exception& e) {
                            ENJIN_LOG_ERROR(Player, "Failed to parse data asset '%s': %s", file.c_str(), e.what());
                        }
                    }
                }
            }
            if (schemaCount > 0 || assetCount > 0) {
                ENJIN_LOG_INFO(Player, "Loaded %u schemas and %u data assets", schemaCount, assetCount);
            }
        }

        m_NetworkSystem.SetWorld(m_World.get());

        ENJIN_LOG_INFO(Player, "Gameplay systems initialized");

        // Skip splash screen — set timer to 0 so EndSplashScreen fires
        // on the first render frame (after BeginFrame succeeds).
        m_ShowingSplash = true;
        m_SplashTimer = 0.0f;

        // Register crash context providers
        s_CrashWorld = m_World.get();
        if (m_Renderer && m_Renderer->GetContext()) {
            VkPhysicalDeviceProperties props = {};
            vkGetPhysicalDeviceProperties(m_Renderer->GetContext()->GetPhysicalDevice(), &props);
            snprintf(s_PlayerGPUNameBuf, sizeof(s_PlayerGPUNameBuf), "%s", props.deviceName);
        }
        snprintf(s_PlayerSceneNameBuf, sizeof(s_PlayerSceneNameBuf), "%s", m_StartScene.c_str());

        Enjin::Debug::CrashContext ctx;
        ctx.engineVersion = []() -> const char* { return ENJIN_VERSION_STRING; };
        ctx.gpuName = []() -> const char* { return s_PlayerGPUNameBuf; };
        ctx.sceneName = []() -> const char* { return s_PlayerSceneNameBuf; };
        ctx.entityCount = []() -> Enjin::u32 {
            return s_CrashWorld ? static_cast<Enjin::u32>(s_CrashWorld->GetEntityCount()) : 0;
        };
        Enjin::Debug::SetCrashContext(ctx);

        m_Initialized = true;
        ENJIN_LOG_INFO(Player, "Player initialized");
    }

    void Shutdown() override {
        ENJIN_LOG_INFO(Player, "Player shutting down...");

        s_CrashWorld = nullptr;
        Enjin::Debug::SetCrashContext({});

        // Clear 2D physics collision callbacks before destroying systems they reference
        if (m_Physics2D) {
            m_Physics2D->SetOnCollisionEnter(nullptr);
            m_Physics2D->SetOnCollisionExit(nullptr);
            m_Physics2D->SetOnSensorEnter(nullptr);
            m_Physics2D->SetOnSensorExit(nullptr);
        }

        // Destroy pooled objects before shutting down scripts
        m_ObjectPool.DestroyAll(m_World.get());

        // Clear visual script system externs
        {
            extern Enjin::Gameplay::TieredSaveSystem* s_VisualScriptSaveSystem;
            extern Enjin::Effects::WeatherSystem* s_VisualScriptWeather;
            extern Enjin::Effects::Water3D* s_VisualScriptWater;
            extern Enjin::GUI::UISystem* s_VisualScriptUI;
            extern Enjin::Accessibility::SubtitleSystem* s_VisualScriptSubtitleSystem;
            extern Enjin::Accessibility::AccessibilityAnnouncer* s_VisualScriptAnnouncer;
            extern Enjin::Audio::SimpleAudio* s_VisualScriptAudio;
            extern Enjin::Renderer::PostProcessing* s_VisualScriptPostProcessing;
            extern Enjin::Audio::AudioEventGraphRuntime* s_VisualScriptAudioGraphRuntime;
            extern Enjin::Gameplay::ObjectPool* s_VisualScriptObjectPool;
            extern Enjin::Gameplay::QuestSystem* s_VisualScriptQuestSystem;
            extern Enjin::Gameplay::CinematicSystem* s_VisualScriptCinematic;
            s_VisualScriptSaveSystem = nullptr;
            s_VisualScriptWeather = nullptr;
            s_VisualScriptWater = nullptr;
            s_VisualScriptUI = nullptr;
            s_VisualScriptSubtitleSystem = nullptr;
            s_VisualScriptAnnouncer = nullptr;
            s_VisualScriptAudio = nullptr;
            s_VisualScriptPostProcessing = nullptr;
            s_VisualScriptAudioGraphRuntime = nullptr;
            s_VisualScriptObjectPool = nullptr;
            s_VisualScriptQuestSystem = nullptr;
            s_VisualScriptCinematic = nullptr;
        }

        // Shutdown gameplay systems before world is destroyed
        m_DialogueSystem.Clear();
        m_VisualScriptSystem.Shutdown();
        m_BehaviorTreeSystem.Shutdown();
        m_AISystem.SetEnabled(false);
        m_ControllerSystem.SetEnabled(false);
        m_FlowerSystem.SetEnabled(false);
        m_UISystem.SetHUDEnabled(false);
        m_QuestSystem.SetEnabled(false);
        m_FootstepSystem.SetEnabled(false);
        m_CinematicSystem.SetEnabled(false);

        // Shutdown script-bound systems
        m_AudioGraphRuntime.Shutdown();
        m_CurlNoiseSystem.Shutdown();
        m_DestructibleSystem.Shutdown();
        m_WeatherSystem.Shutdown();
        m_SimpleAudio.Shutdown();
        m_StreamingManager.SetEnabled(false);
        m_StreamingManager.ClearChunks();

        // Shutdown post-processing
        if (m_PostProcessing) {
            m_PostProcessing->Shutdown();
            m_PostProcessing.reset();
        }
        if (m_RTHybridApply) {
            m_RTHybridApply->Shutdown();
            m_RTHybridApply.reset();
        }

        // Clear accessibility
        m_SubtitleSystem.Clear();
        m_AlternativeInput.ClearScanTargets();
        m_AudioIndicators.Clear();
        m_Announcer.Clear();
        m_SimpleAudio.SetOnSoundPlayed(nullptr);

        // Clear script bindings
        Enjin::Scripting::SetBindingsWorld(nullptr);
        Enjin::Scripting::SetBindingsRenderSystem(nullptr);
        Enjin::Scripting::SetBindingsPhysics(nullptr);
        Enjin::Scripting::SetBindingsDialogueSystem(nullptr);
        Enjin::Scripting::SetBindingsSaveSystem(nullptr);
        Enjin::Scripting::SetFlashShimSaveSystem(nullptr);
        Enjin::Scripting::SetBindingsCoroutineScheduler(nullptr);
        Enjin::Scripting::SetBindingsEventBus(nullptr);
        Enjin::Scripting::SetBindingsScriptEngine(nullptr);
        Enjin::Scripting::SetBindingsQuestSystem(nullptr);
        Enjin::Scripting::SetBindingsCinematicSystem(nullptr);
        Enjin::Scripting::SetBindingsObjectPool(nullptr);
        Enjin::Scripting::SetBindingsFlower(nullptr);
        Enjin::Scripting::SetBindingsSubtitles(nullptr);
        Enjin::Scripting::SetBindingsAnnouncer(nullptr);
        Enjin::Scripting::SetBindingsAccessibilitySettings(nullptr);
        Enjin::Scripting::SetBindingsAudio(nullptr);
        Enjin::Scripting::SetBindingsWeather(nullptr);
        Enjin::Scripting::SetBindingsDestructible(nullptr);
        Enjin::Scripting::SetBindingsRewindSystem(nullptr);
        Enjin::Scripting::SetBindingsProcedural(nullptr);
        Enjin::Scripting::SetBindingsStreaming(nullptr);
        Enjin::Scripting::SetBindingsSceneManager(nullptr);
        Enjin::Scripting::SetBindingsPostProcessing(nullptr);
        Enjin::Scripting::SetBindingsPhysics2D(nullptr);
        Enjin::Scripting::SetBindingsNetworking(nullptr);
        Enjin::Scripting::SetBindingsPluginSystem(nullptr);
        Enjin::Scripting::SetBindingsAudioGraphRuntime(nullptr);
        m_MIDIInput.Shutdown();
        Enjin::Scripting::SetBindingsMIDI(nullptr);
        Enjin::Scripting::SetBindingsInputActionMap(nullptr);

        // Shutdown scripts
        m_ScriptSystem.ShutdownAllScripts();
        m_ScriptSystem.SetEnabled(false);
        m_CoroutineScheduler.Clear();
        m_ScriptEventBus.Clear();
        m_ScriptEngine.Shutdown();

        // Save meta-progression before exit
        m_TieredSaveSystem.SaveMeta();

        // Clean up ImGui texture descriptors before ImGui shutdown
        for (auto& [path, ds] : m_ImGuiTextureCache) {
            if (ds != VK_NULL_HANDLE) ImGui_ImplVulkan_RemoveTexture(ds);
        }
        m_ImGuiTextureCache.clear();

        if (m_ImGuiLayer) {
            m_ImGuiLayer->Shutdown();
            m_ImGuiLayer.reset();
        }

        if (m_RenderSystem) {
            m_RenderSystem->Shutdown();
            m_RenderSystem = nullptr;
        }
        m_World.reset();
        m_CameraController.reset();
        m_Camera.reset();
        m_Renderer.reset();
        m_AssetReader.Close();
    }

    void Update(Enjin::f32 deltaTime) override {
        if (!m_Initialized) return;
        m_FrameDeltaTime = deltaTime;  // Render() needs it for the compute pre-pass

        // Apply deferred fullscreen change (safe between frames)
        if (m_FullscreenChangeRequested) {
            m_FullscreenChangeRequested = false;
            if (GetWindow()) GetWindow()->SetFullscreen(m_PendingFullscreen);
        }

        // Tilde console toggle (Quake-style)
        if (Enjin::Input::IsKeyPressed(Enjin::KeyCode::GraveAccent)) {
            m_ShowConsole = !m_ShowConsole;
            if (m_ShowConsole) {
                m_ConsoleInput[0] = '\0';
                m_ConsoleHistoryPos = -1;
            }
        }

        // Splash screen phase — if timer is 0 (skipped), go straight to game
        if (m_ShowingSplash) {
            if (m_SplashTimer <= 0.0f) {
                // Immediate load — no splash animation
                m_ShowingSplash = false;
                EndSplashScreen();
            } else {
                m_SplashTimer -= deltaTime;
                // Fade in/out
                if (m_SplashTimer > m_SplashDuration - 1.0f)
                    m_SplashAlpha = 1.0f - (m_SplashTimer - (m_SplashDuration - 1.0f));
                else if (m_SplashTimer < 0.5f)
                    m_SplashAlpha = m_SplashTimer / 0.5f;
                else
                    m_SplashAlpha = 1.0f;
                UpdateSplashAlpha();
                // Skip on input
                if (m_SplashTimer < m_SplashDuration - 0.3f) {
                    if (Enjin::Input::IsKeyPressed(Enjin::KeyCode::Escape) ||
                        Enjin::Input::IsKeyPressed(Enjin::KeyCode::Space) ||
                        Enjin::Input::IsKeyPressed(Enjin::KeyCode::Enter) ||
                        Enjin::Input::IsMouseButtonPressed(Enjin::MouseButton::Left)) {
                        m_SplashTimer = 0.0f;
                    }
                }
                if (m_SplashTimer <= 0.0f) EndSplashScreen();
            }
            return;
        }

        // Update audio
        m_SimpleAudio.Update(deltaTime);
        m_SimpleAudio.UpdateAudioSources(deltaTime);
        m_AudioGraphRuntime.Update(deltaTime);
        m_MIDIInput.Update();

        // Update input
        m_InputMap.Update(deltaTime);

        // ESC: state-aware menu navigation. The pause ROOT is the unified
        // UICanvas menu (one UI source, parity with web/editor); GameMenus'
        // bespoke screens remain for main menu / options / how-to-play.
        if (Enjin::Input::IsKeyPressed(Enjin::KeyCode::Escape)) {
            auto screen = m_GameMenu.GetCurrentScreen();
            if (screen == Enjin::GUI::MenuScreen::MainMenu) {
                // On title screen — ESC does nothing
            } else if (screen == Enjin::GUI::MenuScreen::Options ||
                       screen == Enjin::GUI::MenuScreen::HowToPlay) {
                // In sub-menu — back to parent (pause canvas stays open under it)
                if (m_GameStarted) {
                    m_GameMenu.HideAll();
                    if (!m_Paused) OpenPauseMenu();
                } else {
                    m_GameMenu.ShowScreen(Enjin::GUI::MenuScreen::MainMenu);
                }
            } else if (m_Paused) {
                // Pause canvas open — resume gameplay
                ClosePauseMenu();
            } else if (m_GameMenu.IsMenuOpen()) {
                // Legacy screen open — close it
                m_GameMenu.HideAll();
                if (SceneWantsMouseCapture()) Enjin::Input::SetMouseCaptured(true);
            } else {
                // In gameplay — pause
                OpenPauseMenu();
            }
        }

        // RenderSystem::Update (called via World::Update above) handles:
        // RefreshStorageCache, UpdateFrameUniforms, BeginMainRenderPass, entity drawing
        // Tick skeletal animators manually
        if (m_World) {
            for (auto entity : m_World->GetEntitiesWithComponent<Enjin::ECS::AnimatorComponent>()) {
                auto* ac = m_World->GetComponent<Enjin::ECS::AnimatorComponent>(entity);
                if (ac) ac->Update(deltaTime);
            }
        }

        // Skip gameplay updates when paused, on title screen, or content warning is shown
        if (m_GameMenu.IsMenuOpen() || m_Paused || !m_GameStarted) return;
        if (m_ContentWarnings.IsVisible()) return;

        // --- Physics (must run first) ---
        if (m_Physics) m_Physics->Update(deltaTime);
        if (m_Physics2D) m_Physics2D->Update(deltaTime);

        // Dispatch 3D collision events to visual scripts and gameplay systems
        Enjin::Gameplay::GameplayLoop::DispatchCollisionEvents3D(
            m_World.get(), m_Physics.get(), &m_VisualScriptSystem, deltaTime, m_DeferredDestroys);

        // --- Controllers & vegetation ---
        m_ControllerSystem.Update(deltaTime);
        // Update flower system viewport (full screen in player)
        if (m_Renderer) {
            auto ext = m_Renderer->GetSwapchainExtent();
            m_FlowerSystem.SetGameViewBounds(0.0f, 0.0f,
                static_cast<Enjin::f32>(ext.width), static_cast<Enjin::f32>(ext.height));
            m_FlowerSystem.SetRenderTargetSize(ext.width, ext.height);
        }
        m_FlowerSystem.Update(deltaTime);

        // --- Camera (after controllers, which may drive it) ---
        // Only update free-fly camera when no game controller is driving it
        if (m_CameraController && m_CameraController->IsEnabled()) {
            m_CameraController->Update(deltaTime);
        }

        // --- AngelScript ---
        m_ScriptSystem.Update(deltaTime);
        m_CoroutineScheduler.EndOfFrame();
        Enjin::Scripting::FlushDeferredEntityDestroys();

        // --- Gameplay systems ---
        m_TweenSystem.Update(m_World.get(), deltaTime);
        m_StateMachineSystem.Update(m_World.get(), deltaTime);
        m_VisualScriptSystem.Update(deltaTime);
        m_BehaviorTreeSystem.Update(deltaTime);
        m_AISystem.Update(deltaTime);

        // Record & Rewind (Braid / Sands of Time mechanic)
        m_RecordRewindSystem.Update(deltaTime);

        // Dialogue
        UpdateDialogue(deltaTime);

        // Cinematic, quests, footsteps, object pool, events
        m_CinematicSystem.Update(m_World.get(), m_Camera.get(), deltaTime);
        m_QuestSystem.Update(m_World.get(), deltaTime);

        // Quest flow graphs
        if (m_World) {
            for (auto entity : m_World->GetEntitiesWithComponent<Enjin::ECS::QuestFlowComponent>()) {
                Enjin::Gameplay::AdvanceQuestFlow(m_World.get(), entity, deltaTime);
            }
        }

        m_FootstepSystem.Update(m_World.get(), deltaTime);
        m_ObjectPool.Update(m_World.get(), deltaTime);
        m_EntityEventBus.ProcessDeferred();

        // Weather, destructible, wind, world time, seasonal weather, and streaming
        m_WindSystem.Update(deltaTime);
        m_WorldTime.Update(deltaTime);
        m_SeasonalWeather.Update(deltaTime, m_WorldTime.GetState(), m_WeatherSystem);
        if (m_Camera) {
            m_WeatherSystem.Update(deltaTime, m_Camera->GetPosition());
            m_StreamingManager.Update(m_Camera->GetPosition(), deltaTime);
        }
        m_DestructibleSystem.Update(deltaTime);
        m_InteractiveWaterSystem.Update(m_World.get(), deltaTime);
        for (auto entity : m_World->GetEntitiesWithComponent<Enjin::Effects::InteractiveWaterComponent>()) {
            auto* water = m_World->GetComponent<Enjin::Effects::InteractiveWaterComponent>(entity);
            auto* transform = m_World->GetComponent<Enjin::ECS::TransformComponent>(entity);
            if (!water || !transform || !water->initialized) continue;
            auto mesh = m_InteractiveWaterSystem.GenerateMesh(*water, *transform);
            if (m_World->HasComponent<Enjin::ECS::MeshComponent>(entity))
                *m_World->GetComponent<Enjin::ECS::MeshComponent>(entity) = std::move(mesh);
            else
                m_World->AddComponent<Enjin::ECS::MeshComponent>(entity, std::move(mesh));
        }
        // Update Water3D animated surfaces
        m_Water3D.Update(deltaTime);
        for (auto entity : m_World->GetEntitiesWithComponent<Enjin::ECS::Water3DComponent>()) {
            auto* water3d = m_World->GetComponent<Enjin::ECS::Water3DComponent>(entity);
            if (!water3d || !water3d->meshCreated) continue;
            m_Water3D.Initialize(water3d->settings);
            m_Water3D.UpdateEntityMesh(m_World.get(), entity);
        }
        m_FluidSimulation.Update(deltaTime, m_World.get());
        m_FluidTerrainCoupling.Update(deltaTime, m_World.get(), m_FluidSimulation);
        m_CurlNoiseSystem.Update(deltaTime);

        // Particle emitter simulation
        m_ParticleSystem.Update(deltaTime, m_World.get());

        // Audio-reactive system (beat sync, FFT-driven parameters) — editor parity.
        m_AudioReactiveSystem.Update(deltaTime);

        // Elemental system (fire/water/earth/air) + fire-light injection. Mirrors
        // EditorLayer: fire emitters become transient point lights that light
        // surfaces here and participating media via clustered lighting.
        if (m_Camera && m_RenderSystem) {
            m_ElementalSystem.Update(m_World.get(), deltaTime, m_Camera->GetPosition());
            m_EffectsTime += deltaTime;
            m_ElementalSystem.BuildFireLights(m_EffectsTime, m_FireLights);
            m_RenderSystem->ClearTransientPointLights();
            for (const auto& fl : m_FireLights) {
                m_RenderSystem->AddTransientPointLight(fl.position, fl.range, fl.color, fl.intensity);
            }
        }

        // Post-processing time update + volume evaluation
        // Check if the active camera has post-processing enabled
        bool ppEnabled = true;
        if (m_World) {
            auto activeCam = Enjin::ECS::CameraManager::GetActiveCamera(m_World.get());
            if (activeCam != Enjin::ECS::INVALID_ENTITY) {
                auto* cc = m_World->GetComponent<Enjin::ECS::CameraComponent>(activeCam);
                if (cc) ppEnabled = cc->enablePostProcessing;
            }
        }
        if (m_PostProcessing && ppEnabled) {
            m_PostProcessing->Update(deltaTime);
            if (m_Camera)
                m_PostProcessing->SetCameraPlanes(m_Camera->GetNearPlane(), m_Camera->GetFarPlane());

            // Screen-space effects: compute invViewProj + light direction + light screen pos
            if (m_Camera && m_World) {
                Enjin::Math::Matrix4 viewMat = m_Camera->GetViewMatrix();
                Enjin::Math::Matrix4 projMat = m_Camera->GetProjectionMatrix();
                Enjin::Math::Matrix4 vp = projMat * viewMat;
                Enjin::Math::Matrix4 invVP = vp.Inverse();
                m_PostProcessing->SetInverseViewProjection(
                    Enjin::Math::Vector4(invVP.m[0], invVP.m[1], invVP.m[2], invVP.m[3]),
                    Enjin::Math::Vector4(invVP.m[4], invVP.m[5], invVP.m[6], invVP.m[7]),
                    Enjin::Math::Vector4(invVP.m[8], invVP.m[9], invVP.m[10], invVP.m[11]),
                    Enjin::Math::Vector4(invVP.m[12], invVP.m[13], invVP.m[14], invVP.m[15]));
                // Forward view-projection — SSAO sample projection and the
                // contact-shadow world-space march project world points to screen
                m_PostProcessing->SetViewProjection(
                    Enjin::Math::Vector4(vp.m[0], vp.m[1], vp.m[2], vp.m[3]),
                    Enjin::Math::Vector4(vp.m[4], vp.m[5], vp.m[6], vp.m[7]),
                    Enjin::Math::Vector4(vp.m[8], vp.m[9], vp.m[10], vp.m[11]),
                    Enjin::Math::Vector4(vp.m[12], vp.m[13], vp.m[14], vp.m[15]));

                // Find first directional light for god rays / contact shadows
                Enjin::Math::Vector3 lightDir(0.0f, -1.0f, 0.0f);
                for (auto e : m_World->GetEntitiesWithComponent<Enjin::ECS::LightComponent>()) {
                    auto* lc = m_World->GetComponent<Enjin::ECS::LightComponent>(e);
                    if (lc && lc->type == Enjin::ECS::LightType::Directional) {
                        auto* tc = m_World->GetComponent<Enjin::ECS::TransformComponent>(e);
                        if (tc) lightDir = tc->rotation.GetForward();
                        break;
                    }
                }
                m_PostProcessing->SetLightDirection(lightDir);

                // Project sun position to screen space for god rays
                Enjin::Math::Vector3 sunFar = m_Camera->GetPosition() - lightDir * 500.0f;
                Enjin::f32 clipX = vp.m[0]*sunFar.x + vp.m[4]*sunFar.y + vp.m[8]*sunFar.z + vp.m[12];
                Enjin::f32 clipY = vp.m[1]*sunFar.x + vp.m[5]*sunFar.y + vp.m[9]*sunFar.z + vp.m[13];
                Enjin::f32 clipW = vp.m[3]*sunFar.x + vp.m[7]*sunFar.y + vp.m[11]*sunFar.z + vp.m[15];
                Enjin::f32 sunOnScreen = (clipW > 0.001f) ? 1.0f : 0.0f;
                if (clipW > 0.001f) {
                    Enjin::f32 ndcX = clipX / clipW;
                    Enjin::f32 ndcY = clipY / clipW;
                    m_PostProcessing->SetLightScreenPos(Enjin::Math::Vector4(
                        ndcX * 0.5f + 0.5f, ndcY * 0.5f + 0.5f, 0.0f, sunOnScreen));
                } else {
                    m_PostProcessing->SetLightScreenPos(Enjin::Math::Vector4(0.5f, 0.5f, 0.0f, 0.0f));
                }
            }

            // Evaluate post-process volumes
            if (m_World && m_Camera) {
                EvaluatePostProcessVolumes(m_Camera->GetPosition());
            }
        }

        // Accessibility systems
        m_SubtitleSystem.Update(deltaTime);
        m_AlternativeInput.Update(deltaTime);
        m_AudioIndicators.Update(deltaTime);
        m_Announcer.Update(deltaTime);

        // Networking
        m_NetworkSystem.Update(deltaTime);

        // Save system (auto-save timer)
        m_TieredSaveSystem.Update(deltaTime, m_World.get(), m_StartScene);

        // Hazard/pickup overlap checks (for CharacterVirtual which doesn't fire collision events)
        Enjin::Gameplay::GameplayLoop::CheckHazardOverlaps(m_World.get(), deltaTime, m_DeferredDestroys);
        Enjin::Gameplay::GameplayLoop::CheckHazardOverlaps3D(m_World.get(), m_DeferredDestroys);
        Enjin::Gameplay::GameplayLoop::CheckEnemyOverlaps2D(m_World.get(), deltaTime, m_DeferredDestroys);
        Enjin::Gameplay::GameplayLoop::CheckPickupOverlaps3D(m_World.get(), m_DeferredDestroys);
        Enjin::Gameplay::GameplayLoop::CheckPickupOverlaps2D(m_World.get(), m_DeferredDestroys);

        // Health system (regen, invulnerability, death) and deferred entity destruction
        Enjin::Gameplay::GameplayLoop::UpdateHealthSystems(m_World.get(), deltaTime, m_DeferredDestroys);

        // Trigger zones (fills entitiesInside — required for reach-the-goal victory)
        Enjin::Gameplay::GameplayLoop::UpdateTriggerZones(m_World.get());

        // Game over state (player death / victory detection). The game-over UI
        // itself is the UICanvas screen GameplayLoop spawns (one source, rendered
        // by UISystem on every platform) — just release the mouse so the player
        // can click its "Play Again" button.
        if (Enjin::Gameplay::GameplayLoop::UpdateGameOverState(m_World.get(), deltaTime)) {
            Enjin::Input::SetMouseCaptured(false);
        }

        Enjin::Gameplay::GameplayLoop::FlushDeferredDestroys(m_World.get(), m_DeferredDestroys);

        // Resource regeneration
        if (m_World) {
            for (auto entity : m_World->GetEntitiesWithComponent<Enjin::ECS::ResourceComponent>()) {
                auto* res = m_World->GetComponent<Enjin::ECS::ResourceComponent>(entity);
                if (res) res->Regenerate(deltaTime);
            }
        }
    }

    // Find a visible authored "MainMenu" canvas in the scene (empty = none).
    // Whether any mouse-look controller in the scene wants the cursor captured.
    // Controllers with captureMouseOnClick=false (Web Demo) keep the cursor
    // free: RMB-hold orbits, on-screen UI stays clickable while playing.
    bool SceneWantsMouseCapture() {
        if (!m_World) return false;
        for (auto e : m_World->GetEntitiesWithComponent<Enjin::ECS::FirstPersonController>()) {
            auto* c = m_World->GetComponent<Enjin::ECS::FirstPersonController>(e);
            if (c && c->captureMouseOnClick) return true;
        }
        for (auto e : m_World->GetEntitiesWithComponent<Enjin::ECS::ThirdPersonController>()) {
            auto* c = m_World->GetComponent<Enjin::ECS::ThirdPersonController>(e);
            if (c && c->captureMouseOnClick) return true;
        }
        return false;
    }

    Enjin::ECS::Entity FindAuthoredMainMenu() {
        for (auto e : m_World->GetEntitiesWithComponent<Enjin::GUI::UICanvasComponent>()) {
            auto* c = m_World->GetComponent<Enjin::GUI::UICanvasComponent>(e);
            if (c && c->visible && c->canvasName == "MainMenu") return e;
        }
        return Enjin::ECS::INVALID_ENTITY;
    }

    // Unified pause menu (same UITemplates canvas as web/editor). GameMenus keeps
    // only the main menu and options screens; the pause ROOT is the canvas.
    void OpenPauseMenu() {
        if (m_Paused) return;
        m_Paused = true;
        m_PauseMenuEntity = m_World->CreateEntity();
        m_World->AddComponent<Enjin::ECS::NameComponent>(m_PauseMenuEntity, "Pause Menu UI");
        m_World->AddComponent<Enjin::GUI::UICanvasComponent>(m_PauseMenuEntity,
            Enjin::GUI::UITemplates::CreatePauseMenu());
        Enjin::Input::SetMouseCaptured(false);
    }

    void ClosePauseMenu() {
        if (!m_Paused) return;
        m_Paused = false;
        if (m_PauseMenuEntity != Enjin::ECS::INVALID_ENTITY && m_World->IsValid(m_PauseMenuEntity)) {
            m_World->DestroyEntity(m_PauseMenuEntity);
        }
        m_PauseMenuEntity = Enjin::ECS::INVALID_ENTITY;
        if (SceneWantsMouseCapture()) Enjin::Input::SetMouseCaptured(true);
    }

    // Restart the current game session: fresh physics backends (stale bodies and
    // controllers must not persist) and reload the start scene. Shared by the pause
    // menu Restart, the GameMenus game-over path, and the UICanvas game-over
    // screen's "gameover_restart" UI event.
    void RestartGameSession() {
        m_GameMenu.HideAll();
        if (SceneWantsMouseCapture()) Enjin::Input::SetMouseCaptured(true);
        auto backendType = static_cast<Enjin::Physics::PhysicsBackendType>(
            m_PhysicsBackendType <= 3 ? m_PhysicsBackendType : 0);
        auto projectMode = static_cast<Enjin::Scene::ProjectMode>(
            m_ProjectMode <= 2 ? m_ProjectMode : 1);
        m_Physics = Enjin::Physics::CreatePhysicsBackend(backendType, projectMode);
        if (m_Physics) m_Physics->SetWorld(m_World.get());
        m_Physics2D = Enjin::Physics::CreatePhysicsBackend2D(backendType, projectMode);
        if (m_Physics2D) m_Physics2D->Initialize(m_World.get());
        m_ControllerSystem.SetPhysics(m_Physics.get());
        m_ControllerSystem.SetPhysics2D(m_Physics2D.get());
        Enjin::Gameplay::GameplayLoop::Wire2DCollisionCallbacks(
            m_Physics2D.get(), m_World.get(), &m_VisualScriptSystem, m_DeferredDestroys);
        if (!m_StartScene.empty()) LoadSceneFromPack(m_StartScene);
    }

    void Render() override {
        if (!m_Initialized || !m_Renderer) return;

        if (!m_Renderer->BeginFrameVulkan()) {
            if (m_Renderer->IsDeviceLost()) {
                ENJIN_LOG_FATAL(Player, "GPU device lost — shutting down.");
                RequestShutdown();
            }
            return;
        }

        // Sync the render camera from the active game CameraComponent entity.
        // This updates FOV, near/far, aspect ratio, AND position/rotation so
        // the built game renders from the same viewpoint as the editor Game View.
        auto extent = m_Renderer->GetSwapchainExtent();
        if (extent.width > 0 && extent.height > 0 && m_Camera) {
            Enjin::f32 aspect = static_cast<Enjin::f32>(extent.width) / static_cast<Enjin::f32>(extent.height);
            Enjin::f32 fov = 45.0f;
            Enjin::f32 nearP = 0.1f;
            Enjin::f32 farP = 1000.0f;
            if (m_World) {
                auto activeCam = Enjin::ECS::CameraManager::GetActiveCamera(m_World.get());
                if (activeCam != Enjin::ECS::INVALID_ENTITY) {
                    auto* cc = m_World->GetComponent<Enjin::ECS::CameraComponent>(activeCam);
                    if (cc) {
                        fov = cc->fieldOfView;
                        nearP = cc->nearPlane;
                        farP = cc->farPlane;
                    }
                    // Override FOV from settings if user changed it
                    if (m_PendingFOV > 0.0f) fov = m_PendingFOV;
                    // Sync camera position/rotation from the entity's TransformComponent
                    auto* camTransform = m_World->GetComponent<Enjin::ECS::TransformComponent>(activeCam);
                    if (camTransform) {
                        m_Camera->SetPosition(camTransform->position);
                        Enjin::Math::Vector3 forward = camTransform->rotation.GetForward();
                        Enjin::Math::Vector3 up = camTransform->rotation.GetUp();
                        m_Camera->SetLookAt(camTransform->position,
                                            camTransform->position + forward, up);
                        // 3D audio listener follows the camera (without this,
                        // all positional sound pans relative to world origin)
                        m_SimpleAudio.SetListenerPosition(camTransform->position, forward, up);
                    }
                }
            }
            m_Camera->SetPerspective(fov, aspect, nearP, farP);
        }

        // Detect splitscreen: multiple active cameras with non-default viewports
        bool splitscreenActive = false;
        if (m_World && m_RenderSystem) {
            auto allCameras = Enjin::ECS::CameraManager::GetAllActiveCameras(m_World.get());
            bool useSplitscreen = false;

            if (allCameras.size() > 1) {
                for (auto camEntity : allCameras) {
                    auto* cc = m_World->GetComponent<Enjin::ECS::CameraComponent>(camEntity);
                    if (cc && (cc->viewportX != 0.0f || cc->viewportY != 0.0f ||
                               cc->viewportWidth != 1.0f || cc->viewportHeight != 1.0f)) {
                        useSplitscreen = true;
                        break;
                    }
                }
            }

            if (useSplitscreen) {
                std::vector<Enjin::ECS::ViewportCamera> viewports;
                for (auto camEntity : allCameras) {
                    auto* cc = m_World->GetComponent<Enjin::ECS::CameraComponent>(camEntity);
                    if (!cc) continue;
                    Enjin::ECS::ViewportCamera vc;
                    vc.entity = camEntity;
                    vc.viewportX = cc->viewportX;
                    vc.viewportY = cc->viewportY;
                    vc.viewportWidth = cc->viewportWidth;
                    vc.viewportHeight = cc->viewportHeight;
                    viewports.push_back(vc);
                    if (viewports.size() >= Enjin::ECS::RenderSystem::MAX_SPLITSCREEN_VIEWPORTS) break;
                }
                m_RenderSystem->SetMainPassSplitscreen(viewports);
            } else {
                m_RenderSystem->SetMainPassSplitscreen({});
            }
            splitscreenActive = useSplitscreen;
        }

        // Live accessibility sync BEFORE the frame renders: game scripts can
        // change colorblind mode / brightness / contrast / font scale mid-game
        // (Accessibility Demo template does exactly this). Without the per-frame
        // re-apply, script changes only took effect after a restart.
        if (m_PostProcessing) {
            m_AccessibilitySettings.ApplyToPostProcessing(m_PostProcessing->GetSettings());
        }
        m_UISystem.SetFontScale(m_AccessibilitySettings.fontScale);

        // Raster post-process path (editor game-view pattern): render the scene
        // into the offscreen target, let RenderSystem::Update open an EMPTY
        // swapchain pass (skip flag), then draw the post-process fullscreen quad
        // sampling the scene target after World::Update. Without this the scene
        // drew straight into the swapchain — unsampleable mid-pass — so nothing
        // in postprocess.frag ever applied in exported games. PT mode keeps its
        // own PP source; splitscreen falls back to the direct path (no PP).
        m_RasterPPThisFrame = false;
        {
            bool ptMode = m_RenderSystem && m_RenderSystem->IsRayTracingEnabled() &&
                          m_RenderSystem->GetRTMode() == 1;
            bool cameraPP = true;
            if (m_World) {
                auto activeCam = Enjin::ECS::CameraManager::GetActiveCamera(m_World.get());
                if (activeCam != Enjin::ECS::INVALID_ENTITY) {
                    auto* cc = m_World->GetComponent<Enjin::ECS::CameraComponent>(activeCam);
                    if (cc) cameraPP = cc->enablePostProcessing;
                }
            }
            if (cameraPP && !ptMode && !splitscreenActive &&
                m_PostProcessing && m_PostProcessing->IsInitialized() &&
                m_ScenePPTarget && m_ScenePPTarget->IsValid() &&
                m_RenderSystem && m_Camera && m_World) {
                VkCommandBuffer preCmd = m_Renderer->GetCurrentCommandBuffer();
                if (preCmd != VK_NULL_HANDLE) {
                    // Pre-recording flush: Update()'s own flush is guarded off
                    // by the skip flag (mid-frame guard), so materials/bindless
                    // must flush here — same contract as the editor loop.
                    m_RenderSystem->FlushPendingChanges();
                    // Compute pre-pass (fog froxels, DDGI, clustered lights,
                    // skinning, GPU particles) — Update() never reaches it
                    // when the skip flag is set. Without this the fog volume
                    // never gets its neutral clear and the fog composite
                    // renders the whole scene black.
                    m_RenderSystem->RecordComputePrePass(m_FrameDeltaTime);
                    m_RenderSystem->RenderShadowPassForCamera(m_Camera.get());
                    // Editor-only RT dispatch site: Update() early-returns on
                    // the skip flag before reaching RT (no-op when RT is off)
                    m_RenderSystem->RecordRTFrame(false);
                    m_ScenePPTarget->Begin(preCmd);
                    m_RenderSystem->RenderToTarget(m_ScenePPTarget.get(), m_Camera.get(), 1);
                    m_RenderSystem->RenderElementalParticles(m_ElementalSystem,
                        m_ScenePPTarget->GetWidth(), m_ScenePPTarget->GetHeight());
                    m_ScenePPTarget->End(preCmd);
                    m_RenderSystem->SetSkipMainPassRendering(true);
                    m_RasterPPThisFrame = true;
                }
            }
        }

        // World::Update triggers all registered systems including RenderSystem.
        // RenderSystem::Update starts the main render pass and draws entities.
        if (m_World) {
            m_World->Update(0.0f);
        }

        // Path tracer display: when RT is in path-trace mode, draw the accumulated
        // PT image over the rasterized scene via the post-process fullscreen pass
        // (built against the swapchain MRT pass, which is open at this point).
        // RecordRTFrame left the PT image in SHADER_READ_ONLY for sampling.
        if (m_RenderSystem && m_PostProcessing && m_PostProcessing->IsInitialized() &&
            m_RenderSystem->IsRayTracingEnabled() && m_RenderSystem->GetRTMode() == 1) {
            auto* pathTracer = m_RenderSystem->GetPathTracer();
            if (pathTracer && pathTracer->GetOutputView() != VK_NULL_HANDLE &&
                pathTracer->GetAccumulatedSamples() > 0) {
                VkCommandBuffer ptCmd = m_Renderer->GetCurrentCommandBuffer();
                if (ptCmd != VK_NULL_HANDLE) {
                    if (m_LastPTSourceView != pathTracer->GetOutputView()) {
                        m_PostProcessing->UpdateSourceImage(pathTracer->GetOutputView(),
                                                            pathTracer->GetOutputSampler());
                        m_LastPTSourceView = pathTracer->GetOutputView();
                        ENJIN_LOG_INFO(Player, "Path tracer display active (%u samples)",
                                       pathTracer->GetAccumulatedSamples());
                    }
                    m_PostProcessing->ApplyToCurrentPass(ptCmd, extent.width, extent.height);
                }
            }
        }

        // Raster PP composite: the scene rendered offscreen this frame and the
        // swapchain pass is open and empty — draw the post-processed scene into
        // it (colorblind/tonemap/brightness/contrast/vignette all live here)
        if (m_RasterPPThisFrame && m_PostProcessing && m_ScenePPTarget) {
            VkCommandBuffer ppCmd = m_Renderer->GetCurrentCommandBuffer();
            if (ppCmd != VK_NULL_HANDLE) {
                if (m_LastPTSourceView != m_ScenePPTarget->GetColorImageView()) {
                    m_PostProcessing->UpdateSourceImage(m_ScenePPTarget->GetColorImageView(),
                                                        m_ScenePPTarget->GetSampler());
                    m_LastPTSourceView = m_ScenePPTarget->GetColorImageView();
                }
                m_PostProcessing->ApplyToCurrentPass(ppCmd, extent.width, extent.height);
            }
        }

        // Hybrid RT overlay: when RT is in hybrid mode with shadows/AO/reflect/GI
        // enabled, blend the ray-traced effects onto the swapchain scene. The
        // effect images were left SHADER_READ_ONLY by RecordRTFrame (inside
        // World::Update). Runs in the still-open swapchain pass.
        if (m_RTHybridApply && m_RenderSystem && m_RenderSystem->IsRTHybridActive()) {
            VkCommandBuffer hCmd = m_Renderer->GetCurrentCommandBuffer();
            if (hCmd != VK_NULL_HANDLE) {
                m_RTHybridApply->SetInputs(m_RenderSystem->GetRTHybridShadowView(),
                                           m_RenderSystem->GetRTHybridAOView(),
                                           m_RenderSystem->GetRTHybridReflectView(),
                                           m_RenderSystem->GetRTHybridGIView(),
                                           m_RenderSystem->GetRTHybridSampler());
                Enjin::f32 sS, aS, rS, gS;
                m_RenderSystem->GetRTHybridStrengths(sS, aS, rS, gS);
                m_RTHybridApply->Apply(hCmd, extent.width, extent.height, sS, aS, rS, gS);
            }
        }

        // Shadow pass runs inside RenderSystem::Update() (called by World::Update above).
        // Do NOT call RenderShadowPassForCamera here — the main render pass is already
        // active at this point, and starting a shadow render pass inside it crashes the
        // NVIDIA driver (nested vkCmdBeginRenderPass).

        // Render ImGui overlays (pause menu, dialogue)
        VkCommandBuffer cmd = m_Renderer->GetCurrentCommandBuffer();
        if (m_ImGuiLayer && cmd != VK_NULL_HANDLE) {
            m_ImGuiLayer->BeginFrame();

            // Pause menu (suppressed while the engine intro card is up so
            // clicks can't reach it through the card)
            if (m_GameMenu.IsMenuOpen() && !EngineSplashActive()) {
                m_GameMenu.Render(static_cast<Enjin::f32>(extent.width),
                                  static_cast<Enjin::f32>(extent.height));
            }

            // Runtime dialogue overlay (only during active gameplay)
            if (!m_ShowingSplash && m_GameStarted && !m_GameMenu.IsMenuOpen()) {
                DrawDialogueOverlay();
            }

            // Runtime UI canvases (suppressed when built-in GameMenu is open or
            // intro card active to prevent double menus). HUDSystem is retired:
            // hudWidget data migrates to UICanvas on load, so canvases are the
            // ONE UI path — the camera drives world-space billboard elements.
            if (!m_ShowingSplash && !EngineSplashActive() && !m_GameMenu.IsMenuOpen() && m_World) {
                m_UISystem.Update(m_World.get(),
                    static_cast<Enjin::f32>(extent.width),
                    static_cast<Enjin::f32>(extent.height), 0.0f,
                    0.0f, 0.0f, m_Camera.get());
            }

            // Accessibility overlays
            if (!m_ShowingSplash) {
                // Content warning overlay blocks game rendering (Task #39)
                if (m_ContentWarnings.IsVisible()) {
                    m_ContentWarnings.RenderWarningOverlay(extent.width, extent.height);
                } else {
                    m_SubtitleSystem.RenderOverlay(extent.width, extent.height);
                    m_AlternativeInput.RenderOverlay();
                    m_AudioIndicators.RenderOverlay(extent.width, extent.height);
                    m_Announcer.RenderStatusBar();
                }
            }

            // Engine intro card ("Made with TEGE") — drawn last, on top of
            // everything, only after the loading splash has finished
            if (!m_ShowingSplash) {
                DrawEngineSplash();
            }

            // Tilde console (Quake-style, slides up from bottom)
            DrawConsole(cmd);

            m_ImGuiLayer->EndFrame(cmd);
        }

        m_Renderer->EndFrame();
    }

    static constexpr Enjin::f32 kEngineSplashDuration = 4.0f;   // matches the editor splash choreography

    bool EngineSplashActive() const {
        return m_EngineSplash && !m_ShowingSplash && m_EngineSplashTimer < kEngineSplashDuration;
    }

    // "Made with TEGE" intro card — the same animated splash the editor shows
    // at startup, via the shared Enjin::GUI::DrawEngineSplash. Optional (build
    // setting engineSplash, default on). Any key or click after half a second
    // skips ahead to the fade-out.
    void DrawEngineSplash() {
        if (!EngineSplashActive()) return;

        ImGuiIO& io = ImGui::GetIO();
        m_EngineSplashTimer += io.DeltaTime;

        constexpr Enjin::f32 kFadeStart = kEngineSplashDuration - 1.0f;
        if (m_EngineSplashTimer > 0.5f && m_EngineSplashTimer < kFadeStart &&
            (ImGui::IsMouseClicked(ImGuiMouseButton_Left) || io.InputQueueCharacters.Size > 0 ||
             ImGui::IsKeyPressed(ImGuiKey_Space) || ImGui::IsKeyPressed(ImGuiKey_Enter) ||
             ImGui::IsKeyPressed(ImGuiKey_Escape))) {
            m_EngineSplashTimer = kFadeStart;   // skip straight to the fade-out
        }

        Enjin::GUI::DrawEngineSplash(m_EngineSplashTimer, kEngineSplashDuration,
                                     kFadeStart, "made with");
    }

    void DrawConsole(VkCommandBuffer) {
        // Animate slide
        Enjin::f32 dt = ImGui::GetIO().DeltaTime;
        Enjin::f32 target = m_ShowConsole ? 1.0f : 0.0f;
        constexpr Enjin::f32 speed = 8.0f;
        if (m_ConsoleAnim < target) m_ConsoleAnim = std::min(m_ConsoleAnim + speed * dt, target);
        else if (m_ConsoleAnim > target) m_ConsoleAnim = std::max(m_ConsoleAnim - speed * dt, target);
        if (m_ConsoleAnim <= 0.001f) return;

        ImGuiIO& io = ImGui::GetIO();
        Enjin::f32 consoleH = io.DisplaySize.y * 0.4f;
        Enjin::f32 visibleH = consoleH * m_ConsoleAnim;

        ImGui::SetNextWindowPos(ImVec2(0, io.DisplaySize.y - visibleH));
        ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x, consoleH));

        ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNav;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12, 8));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.04f, 0.05f, 0.07f, 0.92f));

        if (ImGui::Begin("##PlayerConsole", nullptr, flags)) {
            ImGui::SetWindowFontScale(1.2f);
            ImGui::TextColored(ImVec4(0.6f, 0.8f, 0.6f, 1.0f), "CONSOLE");
            ImGui::SameLine(io.DisplaySize.x - 200);
            ImGui::TextColored(ImVec4(0.4f, 0.45f, 0.5f, 1.0f), "Press ~ to close");
            ImGui::Separator();

            Enjin::f32 inputH = ImGui::GetFrameHeightWithSpacing() + 4;
            ImGui::BeginChild("##ConsoleLog", ImVec2(0, -inputH), false);
            for (const auto& line : m_ConsoleLog) {
                ImVec4 color(0.75f, 0.75f, 0.75f, 1.0f);
                if (line.find("[Error]") != std::string::npos) color = ImVec4(1.0f, 0.35f, 0.35f, 1.0f);
                else if (line.find("[Warn]") != std::string::npos) color = ImVec4(1.0f, 0.85f, 0.3f, 1.0f);
                else if (!line.empty() && line[0] == '>') color = ImVec4(0.5f, 0.8f, 0.5f, 1.0f);
                ImGui::PushStyleColor(ImGuiCol_Text, color);
                ImGui::TextUnformatted(line.c_str());
                ImGui::PopStyleColor();
            }
            if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 4.0f)
                ImGui::SetScrollHereY(1.0f);
            ImGui::EndChild();

            ImGui::Separator();
            ImGui::TextColored(ImVec4(0.5f, 0.8f, 0.5f, 1.0f), ">");
            ImGui::SameLine();

            if (m_ShowConsole && m_ConsoleAnim > 0.5f) ImGui::SetKeyboardFocusHere();

            ImGui::PushItemWidth(-1);
            auto histCallback = [](ImGuiInputTextCallbackData* data) -> int {
                auto* self = static_cast<GamePlayer*>(data->UserData);
                if (data->EventFlag == ImGuiInputTextFlags_CallbackHistory) {
                    if (data->EventKey == ImGuiKey_UpArrow && !self->m_ConsoleHistory.empty()) {
                        if (self->m_ConsoleHistoryPos < (int)self->m_ConsoleHistory.size() - 1)
                            self->m_ConsoleHistoryPos++;
                        int idx = (int)self->m_ConsoleHistory.size() - 1 - self->m_ConsoleHistoryPos;
                        data->DeleteChars(0, data->BufTextLen);
                        data->InsertChars(0, self->m_ConsoleHistory[idx].c_str());
                    } else if (data->EventKey == ImGuiKey_DownArrow) {
                        if (self->m_ConsoleHistoryPos > 0) self->m_ConsoleHistoryPos--;
                        else { self->m_ConsoleHistoryPos = -1; data->DeleteChars(0, data->BufTextLen); }
                        if (self->m_ConsoleHistoryPos >= 0) {
                            int idx = (int)self->m_ConsoleHistory.size() - 1 - self->m_ConsoleHistoryPos;
                            data->DeleteChars(0, data->BufTextLen);
                            data->InsertChars(0, self->m_ConsoleHistory[idx].c_str());
                        }
                    }
                } else if (data->EventFlag == ImGuiInputTextFlags_CallbackCharFilter) {
                    if (data->EventChar == '`' || data->EventChar == '~') return 1; // eat tilde
                }
                return 0;
            };

            if (ImGui::InputText("##ConsoleIn", m_ConsoleInput, sizeof(m_ConsoleInput),
                    ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackHistory |
                    ImGuiInputTextFlags_CallbackCharFilter, histCallback, this)) {
                std::string cmd = m_ConsoleInput;
                if (!cmd.empty()) {
                    m_ConsoleLog.push_back("> " + cmd);
                    m_ConsoleHistory.push_back(cmd);
                    m_ConsoleHistoryPos = -1;
                    ExecutePlayerCommand(cmd);
                }
                m_ConsoleInput[0] = '\0';
                ImGui::SetKeyboardFocusHere(-1);
            }
            ImGui::PopItemWidth();
            ImGui::SetWindowFontScale(1.0f);
        }
        ImGui::End();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(2);
    }

    void ExecutePlayerCommand(const std::string& command) {
        std::istringstream iss(command);
        std::string cmd;
        iss >> cmd;
        for (auto& c : cmd) c = static_cast<char>(std::tolower(c));

        if (cmd == "help") {
            m_ConsoleLog.push_back("=== TEGE Player Console ===");
            m_ConsoleLog.push_back("  help          Show commands");
            m_ConsoleLog.push_back("  clear         Clear console");
            m_ConsoleLog.push_back("  god           Toggle invulnerability");
            m_ConsoleLog.push_back("  noclip        Toggle no-clip fly mode");
            m_ConsoleLog.push_back("  kill          Kill player");
            m_ConsoleLog.push_back("  heal [amt]    Heal player (default: full)");
            m_ConsoleLog.push_back("  speed <mult>  Set speed multiplier");
            m_ConsoleLog.push_back("  timescale <f> Set time scale (0.1-10)");
            m_ConsoleLog.push_back("  tp <x> <y> <z> Teleport player");
            m_ConsoleLog.push_back("  restart       Restart level");
            m_ConsoleLog.push_back("  fps           Show FPS");
            m_ConsoleLog.push_back("  stats         Show scene stats");
            m_ConsoleLog.push_back("  quit          Quit game");
        } else if (cmd == "clear") {
            m_ConsoleLog.clear();
        } else if (cmd == "god") {
            if (m_World) {
                bool toggled = false;
                auto toggle = [&](auto entities) {
                    for (auto e : entities) {
                        auto* hp = m_World->GetComponent<Enjin::ECS::HealthComponent>(e);
                        if (hp) { hp->isInvulnerable = !hp->isInvulnerable; toggled = true;
                            m_ConsoleLog.push_back(hp->isInvulnerable ? "God mode ON" : "God mode OFF"); break; }
                    }
                };
                toggle(m_World->GetEntitiesWithComponent<Enjin::ECS::Platformer2DController>());
                if (!toggled) toggle(m_World->GetEntitiesWithComponent<Enjin::ECS::TopDown2DController>());
                if (!toggled) toggle(m_World->GetEntitiesWithComponent<Enjin::ECS::ThirdPersonController>());
                if (!toggled) toggle(m_World->GetEntitiesWithComponent<Enjin::ECS::FirstPersonController>());
                if (!toggled) m_ConsoleLog.push_back("[Error] No player entity found");
            }
        } else if (cmd == "kill") {
            if (m_World) {
                auto killPlayer = [&](auto entities) {
                    for (auto e : entities) {
                        auto* hp = m_World->GetComponent<Enjin::ECS::HealthComponent>(e);
                        if (hp) { hp->currentHealth = 0; hp->isDead = true; m_ConsoleLog.push_back("Player killed"); return true; }
                    }
                    return false;
                };
                if (!killPlayer(m_World->GetEntitiesWithComponent<Enjin::ECS::Platformer2DController>()))
                if (!killPlayer(m_World->GetEntitiesWithComponent<Enjin::ECS::ThirdPersonController>()))
                    m_ConsoleLog.push_back("[Error] No player entity found");
            }
        } else if (cmd == "heal") {
            Enjin::f32 amt = 9999.0f;
            iss >> amt;
            if (m_World) {
                auto healPlayer = [&](auto entities) {
                    for (auto e : entities) {
                        auto* hp = m_World->GetComponent<Enjin::ECS::HealthComponent>(e);
                        if (hp) { hp->currentHealth = std::min(hp->currentHealth + amt, hp->maxHealth);
                            m_ConsoleLog.push_back("Healed to " + std::to_string((int)hp->currentHealth)); return true; }
                    }
                    return false;
                };
                if (!healPlayer(m_World->GetEntitiesWithComponent<Enjin::ECS::Platformer2DController>()))
                if (!healPlayer(m_World->GetEntitiesWithComponent<Enjin::ECS::ThirdPersonController>()))
                    m_ConsoleLog.push_back("[Error] No player entity found");
            }
        } else if (cmd == "speed") {
            Enjin::f32 mult = 1.0f;
            if (iss >> mult) {
                // Apply to all controller types
                if (m_World) {
                    for (auto e : m_World->GetEntitiesWithComponent<Enjin::ECS::Platformer2DController>()) {
                        auto* c = m_World->GetComponent<Enjin::ECS::Platformer2DController>(e);
                        if (c) c->moveSpeed *= mult;
                    }
                    for (auto e : m_World->GetEntitiesWithComponent<Enjin::ECS::ThirdPersonController>()) {
                        auto* c = m_World->GetComponent<Enjin::ECS::ThirdPersonController>(e);
                        if (c) c->moveSpeed *= mult;
                    }
                    m_ConsoleLog.push_back("Speed x" + std::to_string(mult));
                }
            } else { m_ConsoleLog.push_back("[Error] Usage: speed <multiplier>"); }
        } else if (cmd == "tp") {
            Enjin::f32 x, y, z = 0;
            if (iss >> x >> y) {
                iss >> z; // optional Z
                if (m_World) {
                    auto tpPlayer = [&](auto entities) {
                        for (auto e : entities) {
                            auto* t = m_World->GetComponent<Enjin::ECS::TransformComponent>(e);
                            if (t) { t->position = Enjin::Math::Vector3(x, y, z);
                                m_ConsoleLog.push_back("Teleported to " + std::to_string(x) + " " + std::to_string(y) + " " + std::to_string(z)); return true; }
                        }
                        return false;
                    };
                    if (!tpPlayer(m_World->GetEntitiesWithComponent<Enjin::ECS::Platformer2DController>()))
                    if (!tpPlayer(m_World->GetEntitiesWithComponent<Enjin::ECS::ThirdPersonController>()))
                        m_ConsoleLog.push_back("[Error] No player entity found");
                }
            } else { m_ConsoleLog.push_back("[Error] Usage: tp <x> <y> [z]"); }
        } else if (cmd == "restart") {
            if (!m_StartScene.empty()) {
                auto backendType = static_cast<Enjin::Physics::PhysicsBackendType>(m_PhysicsBackendType <= 3 ? m_PhysicsBackendType : 0);
                auto projectMode = static_cast<Enjin::Scene::ProjectMode>(m_ProjectMode <= 2 ? m_ProjectMode : 1);
                m_Physics = Enjin::Physics::CreatePhysicsBackend(backendType, projectMode);
                if (m_Physics) m_Physics->SetWorld(m_World.get());
                m_Physics2D = Enjin::Physics::CreatePhysicsBackend2D(backendType, projectMode);
                if (m_Physics2D) m_Physics2D->Initialize(m_World.get());
                m_ControllerSystem.SetPhysics(m_Physics.get());
                m_ControllerSystem.SetPhysics2D(m_Physics2D.get());
                Enjin::Gameplay::GameplayLoop::Wire2DCollisionCallbacks(m_Physics2D.get(), m_World.get(), &m_VisualScriptSystem, m_DeferredDestroys);
                LoadSceneFromPack(m_StartScene);
                m_ConsoleLog.push_back("Level restarted");
            }
        } else if (cmd == "fps") {
            Enjin::f32 fps = ImGui::GetIO().Framerate;
            m_ConsoleLog.push_back("FPS: " + std::to_string((int)fps) + " (" + std::to_string(1000.0f / fps).substr(0, 5) + " ms)");
        } else if (cmd == "stats") {
            if (m_World) m_ConsoleLog.push_back("Entities: " + std::to_string(m_World->GetEntityCount()));
            if (m_RenderSystem) m_ConsoleLog.push_back("Draw calls: " + std::to_string(m_RenderSystem->GetDrawCallCount()));
        } else if (cmd == "quit") {
            if (GetWindow()) GetWindow()->Close();
        } else {
            m_ConsoleLog.push_back("[Error] Unknown command: " + cmd);
        }
    }

private:
    void SetupSplashScreen() {
        if (!m_World || !m_RenderSystem) return;

        m_ShowingSplash = true;
        m_SplashTimer = m_SplashDuration;
        m_SplashAlpha = 0.0f;

        // Dark background via skybox
        Enjin::Renderer::SkyboxConfig splashSky;
        splashSky.type = Enjin::Renderer::SkyboxType::SolidColor;
        splashSky.solidColor = Enjin::Math::Vector3(0.04f, 0.04f, 0.06f);
        m_RenderSystem->SetSkybox(splashSky);

        // Camera looking at origin
        if (m_Camera) {
            m_Camera->SetPosition(Enjin::Math::Vector3(0.0f, 0.0f, 5.0f));
            m_Camera->SetLookAt(
                Enjin::Math::Vector3(0.0f, 0.0f, 5.0f),
                Enjin::Math::Vector3(0.0f, 0.0f, 0.0f),
                Enjin::Math::Vector3(0.0f, 1.0f, 0.0f));
        }

        // Title: "TEGE"
        m_SplashTitleEntity = m_World->CreateEntity();
        m_World->AddComponent<Enjin::ECS::NameComponent>(m_SplashTitleEntity, "SplashTitle");
        auto& titleTransform = m_World->AddComponent<Enjin::ECS::TransformComponent>(m_SplashTitleEntity);
        titleTransform.position = Enjin::Math::Vector3(0.0f, 0.3f, 0.0f);
        auto& titleText = m_World->AddComponent<Enjin::ECS::TextComponent>(m_SplashTitleEntity);
        titleText.text = "TEGE";
        titleText.fontSize = 72.0f;
        titleText.textColor = Enjin::Math::Vector3(0.85f, 0.88f, 1.0f);

        // Subtitle: "by marty64"
        m_SplashSubEntity = m_World->CreateEntity();
        m_World->AddComponent<Enjin::ECS::NameComponent>(m_SplashSubEntity, "SplashSub");
        auto& subTransform = m_World->AddComponent<Enjin::ECS::TransformComponent>(m_SplashSubEntity);
        subTransform.position = Enjin::Math::Vector3(0.0f, -0.5f, 0.0f);
        auto& subText = m_World->AddComponent<Enjin::ECS::TextComponent>(m_SplashSubEntity);
        subText.text = "by marty64";
        subText.fontSize = 28.0f;
        subText.textColor = Enjin::Math::Vector3(0.5f, 0.52f, 0.6f);

        // Light so text is visible
        m_SplashLightEntity = m_World->CreateEntity();
        m_World->AddComponent<Enjin::ECS::NameComponent>(m_SplashLightEntity, "SplashLight");
        auto& lightTransform = m_World->AddComponent<Enjin::ECS::TransformComponent>(m_SplashLightEntity);
        lightTransform.rotation = Enjin::Math::Quaternion(
            Enjin::Math::Vector3(1, 0, 0), Enjin::Math::Radians(-30.0f));
        auto& light = m_World->AddComponent<Enjin::ECS::LightComponent>(m_SplashLightEntity);
        light.type = Enjin::ECS::LightType::Directional;
        light.color = Enjin::Math::Vector3(1.0f, 1.0f, 1.0f);
        light.intensity = 2.0f;

        ENJIN_LOG_INFO(Player, "Splash screen displayed");
    }

    void UpdateSplashAlpha() {
        if (!m_World) return;
        auto* titleText = m_World->GetComponent<Enjin::ECS::TextComponent>(m_SplashTitleEntity);
        auto* subText = m_World->GetComponent<Enjin::ECS::TextComponent>(m_SplashSubEntity);
        if (titleText) {
            titleText->textColor = Enjin::Math::Vector3(0.85f * m_SplashAlpha, 0.88f * m_SplashAlpha, 1.0f * m_SplashAlpha);
        }
        if (subText) {
            subText->textColor = Enjin::Math::Vector3(0.5f * m_SplashAlpha, 0.52f * m_SplashAlpha, 0.6f * m_SplashAlpha);
        }
    }

    void EndSplashScreen() {
        m_ShowingSplash = false;

        // Remove splash entities
        if (m_World) {
            m_World->DestroyEntity(m_SplashTitleEntity);
            m_World->DestroyEntity(m_SplashSubEntity);
            m_World->DestroyEntity(m_SplashLightEntity);
        }
        m_SplashTitleEntity = 0;
        m_SplashSubEntity = 0;
        m_SplashLightEntity = 0;

        // Wire AssetReader for packed script loading from .enjpak
        m_ScriptEngine.SetAssetReader(&m_AssetReader);

        // Load the actual game scene
        if (!m_StartScene.empty()) {
            LoadSceneFromPack(m_StartScene);
        }

        // Wire all script bindings so AngelScript functions work
        Enjin::Scripting::SetBindingsWorld(m_World.get());
        Enjin::Scripting::SetBindingsRenderSystem(m_RenderSystem);
        Enjin::Scripting::SetBindingsPhysics(m_Physics.get());
        Enjin::Scripting::SetBindingsDialogueSystem(&m_DialogueSystem);
        Enjin::Scripting::SetBindingsSaveSystem(&m_TieredSaveSystem);
        Enjin::Scripting::SetFlashShimSaveSystem(&m_TieredSaveSystem);
        Enjin::Scripting::SetBindingsCoroutineScheduler(&m_CoroutineScheduler);
        Enjin::Scripting::SetBindingsEventBus(&m_ScriptEventBus);
        Enjin::Scripting::SetBindingsScriptEngine(&m_ScriptEngine);
        Enjin::Scripting::SetBindingsQuestSystem(&m_QuestSystem);
        Enjin::Scripting::SetBindingsCinematicSystem(&m_CinematicSystem);
        Enjin::Scripting::SetBindingsObjectPool(&m_ObjectPool);
        Enjin::Scripting::SetBindingsFlower(m_World.get());
        Enjin::Scripting::SetBindingsProcedural(nullptr);
        Enjin::Scripting::SetBindingsAudio(&m_SimpleAudio);
        Enjin::Scripting::SetBindingsWeather(&m_WeatherSystem);
        Enjin::Scripting::SetBindingsDestructible(&m_DestructibleSystem);
        Enjin::Scripting::SetBindingsRewindSystem(&m_RecordRewindSystem);
        Enjin::Scripting::SetBindingsStreaming(&m_StreamingManager);
        Enjin::Scripting::SetBindingsSceneManager(&m_SceneManager);
        // Initialize post-processing (settings object for script bindings)
        auto ppExtent = m_Renderer->GetSwapchainExtent();
        m_PostProcessing = std::make_unique<Enjin::Renderer::PostProcessing>();
        if (m_Renderer && m_Renderer->GetContext()) {
            // Swapchain MRT pass (color + velocity) — blend state needs 2 attachments (VUID-07609)
            if (!m_PostProcessing->Initialize(m_Renderer->GetContext(),
                    m_Renderer->GetRenderPass(),
                    ppExtent.width, ppExtent.height, m_Renderer.get(), 2)) {
                ENJIN_LOG_WARN(Player, "PostProcessing init failed");
                m_PostProcessing.reset();
            }
        }
        // React to swapchain recreation (fullscreen toggle, resolution change,
        // window resize): post-processing holds targets and descriptors sized
        // and bound against the old swapchain. Without this rebind, the first
        // frame after a fullscreen switch sampled destroyed images — instant
        // driver crash from the options menu. The callback fires inside
        // OnWindowResize, after WaitForAllFrames and outside recording.
        if (m_PostProcessing) {
            m_Renderer->AddResizeCallback([this](Enjin::u32 w, Enjin::u32 h) {
                if (m_PostProcessing && m_Renderer) {
                    m_PostProcessing->OnResize(w, h);
                    m_PostProcessing->UpdateRenderPass(m_Renderer->GetRenderPass(), 2);
                }
                if (m_ScenePPTarget) {
                    m_ScenePPTarget->Resize(w, h);
                    // Force the PP source rebind — the color view just changed
                    m_LastPTSourceView = VK_NULL_HANDLE;
                }
            });
        }

        // Offscreen scene target for the raster post-process path — DISABLED.
        // First attempt (2026-08-09) drew geometry unlit-black and crashed on
        // scene entry: RecreateEffectPipelinesForRenderPass points the shared
        // effect renderers (weather/sprite/particle) at the offscreen pass,
        // breaking them for the swapchain path, and the offscreen lighting
        // path needs more than RenderToTarget provides in player mode. Needs
        // a validation-layer session before this can ship. Until then exported
        // games run WITHOUT the post-process pass (colorblind/brightness/
        // contrast filters work in the editor and web player only).
        if (kEnableRasterPP && m_PostProcessing) {
            m_ScenePPTarget = std::make_unique<Enjin::Renderer::RenderTarget>();
            if (!m_ScenePPTarget->Create(m_Renderer.get(), ppExtent.width, ppExtent.height)) {
                ENJIN_LOG_WARN(Player, "Scene PP target creation failed — post-processing disabled in raster path");
                m_ScenePPTarget.reset();
            } else if (m_RenderSystem) {
                m_RenderSystem->RecreateEffectPipelinesForRenderPass(m_ScenePPTarget->GetRenderPass());
            }
        }

        // Player hybrid RT overlay (blends RT shadow/AO/reflect/GI onto the
        // swapchain scene). Same swapchain MRT pass as post-processing.
        if (m_Renderer && m_Renderer->GetContext()) {
            m_RTHybridApply = std::make_unique<Enjin::Renderer::RTHybridApply>(m_Renderer->GetContext());
            if (!m_RTHybridApply->Initialize(m_Renderer->GetRenderPass(), 2)) {
                ENJIN_LOG_WARN(Player, "RTHybridApply init failed — hybrid RT overlay disabled in player");
                m_RTHybridApply.reset();
            } else {
                m_Renderer->AddResizeCallback([this](Enjin::u32, Enjin::u32) {
                    if (m_RTHybridApply && m_Renderer)
                        m_RTHybridApply->UpdateRenderPass(m_Renderer->GetRenderPass(), 2);
                });
            }
        }
        Enjin::Scripting::SetBindingsPostProcessing(m_PostProcessing.get());
        Enjin::Scripting::SetBindingsPhysics2D(m_Physics2D.get());
        Enjin::Scripting::SetBindingsNetworking(&m_NetworkSystem);
        Enjin::Scripting::SetBindingsSubtitles(&m_SubtitleSystem);
        Enjin::Scripting::SetBindingsAnnouncer(&m_Announcer);
        Enjin::Scripting::SetBindingsAccessibilitySettings(&m_AccessibilitySettings);
        Enjin::Scripting::SetBindingsAccessibilitySaveCallback([this]() { SaveAccessibilitySettings(); });
        Enjin::Scripting::SetBindingsAccessibilityApplyCallback([this]() { ApplyAccessibilitySettings(); });
        Enjin::Scripting::SetBindingsDyslexiaFontCallback([this](bool on) {
            m_FontLibrary.SetFont(on ? Enjin::Accessibility::FontFamily::OpenDyslexic
                                     : Enjin::Accessibility::FontFamily::Default);
        });
        Enjin::Scripting::SetBindingsPluginSystem(nullptr);  // No PluginSystem in player — null-safe
        Enjin::Scripting::SetBindingsAudioGraphRuntime(&m_AudioGraphRuntime);
        m_MIDIInput.Initialize();
        Enjin::Scripting::SetBindingsMIDI(&m_MIDIInput);
        Enjin::Scripting::SetBindingsInputActionMap(&m_InputMap);

        // Wire visual script system externs (for VS nodes)
        {
            extern Enjin::Gameplay::TieredSaveSystem* s_VisualScriptSaveSystem;
            extern Enjin::Effects::WeatherSystem* s_VisualScriptWeather;
            extern Enjin::Effects::Water3D* s_VisualScriptWater;
            extern Enjin::GUI::UISystem* s_VisualScriptUI;
            extern Enjin::Accessibility::SubtitleSystem* s_VisualScriptSubtitleSystem;
            extern Enjin::Accessibility::AccessibilityAnnouncer* s_VisualScriptAnnouncer;
            extern Enjin::Audio::SimpleAudio* s_VisualScriptAudio;
            extern Enjin::Renderer::PostProcessing* s_VisualScriptPostProcessing;
            extern Enjin::Audio::AudioEventGraphRuntime* s_VisualScriptAudioGraphRuntime;
            extern Enjin::Gameplay::ObjectPool* s_VisualScriptObjectPool;
            extern Enjin::Gameplay::QuestSystem* s_VisualScriptQuestSystem;
            extern Enjin::Gameplay::CinematicSystem* s_VisualScriptCinematic;
            s_VisualScriptSaveSystem = &m_TieredSaveSystem;
            s_VisualScriptWeather = &m_WeatherSystem;
            s_VisualScriptWater = &m_Water3D;
            s_VisualScriptUI = &m_UISystem;
            s_VisualScriptSubtitleSystem = &m_SubtitleSystem;
            s_VisualScriptAnnouncer = &m_Announcer;
            s_VisualScriptAudio = &m_SimpleAudio;
            s_VisualScriptPostProcessing = m_PostProcessing.get();
            s_VisualScriptAudioGraphRuntime = &m_AudioGraphRuntime;
            s_VisualScriptObjectPool = &m_ObjectPool;
            s_VisualScriptQuestSystem = &m_QuestSystem;
            s_VisualScriptCinematic = &m_CinematicSystem;
        }

        // Wire dialogue system event bus, subtitle system, and narrative systems
        m_DialogueSystem.SetEventBus(&m_EntityEventBus);
        m_InteractiveWaterSystem.SetEventBus(&m_EntityEventBus);  // water_enter events
        m_DialogueSystem.SetSubtitleSystem(&m_SubtitleSystem);
        m_DialogueSystem.SetQuestSystem(&m_QuestSystem);
        m_DialogueSystem.SetCinematicSystem(&m_CinematicSystem);
        m_DialogueSystem.SetTieredSaveSystem(&m_TieredSaveSystem);

        // Load accessibility settings from accessibility.json next to executable
        LoadAccessibilitySettings();

        // Wire announcer to UISystem for screen reader support (Task #36)
        m_UISystem.SetAnnouncerCallback([this](const std::string& text) {
            m_Announcer.Announce(text, Enjin::Accessibility::AnnouncePriority::Normal);
        });

        // Push settings into every consumer (also re-run by the in-game
        // Accessibility menu tab whenever the player changes something)
        ApplyAccessibilitySettings();

        // Wire UISystem texture resolver (loads textures via RenderSystem, registers with ImGui)
        m_UISystem.SetTextureResolver([this](const std::string& path, Enjin::u32& outW, Enjin::u32& outH) -> void* {
            if (path.empty() || !m_RenderSystem) return nullptr;
            auto tex = m_RenderSystem->LoadTexture(path);
            if (!tex || !tex->IsValid()) return nullptr;
            outW = tex->GetWidth();
            outH = tex->GetHeight();
            VkDescriptorSet ds = GetImGuiTexture(path);
            return reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(ds));
        });

        // Wire fluid simulation and wind system to renderer
        m_RenderSystem->SetFluidSimulation(&m_FluidSimulation);
        m_RenderSystem->SetWindSystem(&m_WindSystem);

        // Initialize curl noise system
        m_CurlNoiseSystem.Initialize(m_World.get());

        // Initialize audio event graph runtime
        m_AudioGraphRuntime.Initialize(&m_SimpleAudio);

        // Enable all gameplay systems
        m_ControllerSystem.SetEnabled(true);
        m_FlowerSystem.SetEnabled(true);
        m_AISystem.SetEnabled(true);
        m_UISystem.SetHUDEnabled(true);
        m_QuestSystem.SetEnabled(true);
        m_FootstepSystem.SetEnabled(true);
        m_CinematicSystem.SetEnabled(true);
        m_StreamingManager.SetEnabled(true);

        // Initialize quest flow runtime state
        if (m_World) {
            for (auto entity : m_World->GetEntitiesWithComponent<Enjin::ECS::QuestFlowComponent>()) {
                auto* qf = m_World->GetComponent<Enjin::ECS::QuestFlowComponent>(entity);
                if (qf) qf->ResetRuntimeState();
            }
        }

        // Find game camera entity for controller system
        if (m_World) {
            auto cameras = Enjin::ECS::CameraManager::GetAllActiveCameras(m_World.get());
            if (!cameras.empty()) {
                m_ControllerSystem.SetGameCameraEntity(cameras[0]);
                m_FlowerSystem.SetGameCameraEntity(cameras[0]);
            }

            // Disable the free-fly CameraController when the scene has character
            // controllers that drive the camera — matches PlayMode behavior.
            bool hasGameController =
                !m_World->GetEntitiesWithComponent<Enjin::ECS::FirstPersonController>().empty() ||
                !m_World->GetEntitiesWithComponent<Enjin::ECS::ThirdPersonController>().empty() ||
                !m_World->GetEntitiesWithComponent<Enjin::ECS::Platformer2DController>().empty() ||
                !m_World->GetEntitiesWithComponent<Enjin::ECS::TopDown2DController>().empty() ||
                !m_World->GetEntitiesWithComponent<Enjin::ECS::TopDown3DController>().empty() ||
                !m_World->GetEntitiesWithComponent<Enjin::ECS::VehicleController>().empty() ||
                !m_World->GetEntitiesWithComponent<Enjin::ECS::SurfaceAlignedController>().empty();
            if (hasGameController && m_CameraController) {
                m_CameraController->SetEnabled(false);
                ENJIN_LOG_INFO(Player, "Game controller found — free-fly camera disabled");
            }
        }

        // Wire 2D physics collision callbacks to visual script system and gameplay processing
        Enjin::Gameplay::GameplayLoop::Wire2DCollisionCallbacks(
            m_Physics2D.get(), m_World.get(), &m_VisualScriptSystem, m_DeferredDestroys);

        // Initialize visual scripts and behavior trees
        m_VisualScriptSystem.SetPhysics(m_Physics.get());
        m_VisualScriptSystem.SetPhysics2D(m_Physics2D.get());
        m_VisualScriptSystem.SetNetworking(&m_NetworkSystem);
        m_VisualScriptSystem.SetScriptEngine(&m_ScriptEngine);
        m_VisualScriptSystem.SetStreaming(&m_StreamingManager);
        m_VisualScriptSystem.SetDialogue(&m_DialogueSystem);
        m_VisualScriptSystem.Initialize();
        m_BehaviorTreeSystem.Initialize();

        // Start auto-play tweens
        m_TweenSystem.PlayAll(m_World.get());

        // Start AngelScript lifecycle
        if (m_ScriptEngine.GetASEngine()) {
            m_ScriptSystem.SetEnabled(true);
            m_ScriptSystem.InitializeAllScripts();
        }

        // Show title screen — gameplay starts when player clicks New Game / Continue.
        // An AUTHORED "MainMenu" UICanvas in the scene takes precedence over the
        // built-in GameMenus title screen (one UI source; author it via the
        // editor's View -> UI Editor -> New Canvas -> Main Menu).
        if (FindAuthoredMainMenu() != Enjin::ECS::INVALID_ENTITY) {
            Enjin::Input::SetMouseCaptured(false);
            ENJIN_LOG_INFO(Player, "Authored MainMenu canvas found — using it as the title screen");
        } else {
            m_GameMenu.ShowScreen(Enjin::GUI::MenuScreen::MainMenu);
        }

        ENJIN_LOG_INFO(Player, "Splash screen ended, game loaded");
    }

    void ParseManifestJson(const nlohmann::json& manifest) {
        m_WindowTitle = manifest.value("windowTitle", "Enjin Game");
        m_WindowWidth = manifest.value("windowWidth", 1280u);
        m_WindowHeight = manifest.value("windowHeight", 720u);
        m_Fullscreen = manifest.value("fullscreen", false);
        m_EngineSplash = manifest.value("engineSplash", true);
        m_StartScene = manifest.value("startScene", "");

        // Read frame rate settings
        if (manifest.contains("frameSettings")) {
            const auto& fs = manifest["frameSettings"];
            m_TargetFPS = fs.value("targetFrameRate", 60u);
            m_VSync = fs.value("vSync", true);
            m_BackgroundBehavior = fs.value("backgroundBehavior", 1u);
        }

        // Read physics backend and project mode
        m_PhysicsBackendType = manifest.value("physicsBackend", 0u);
        m_ProjectMode = manifest.value("projectMode", 1u);

        ENJIN_LOG_INFO(Player, "Game: %s (%ux%u, %s FPS, VSync: %s)",
            m_WindowTitle.c_str(), m_WindowWidth, m_WindowHeight,
            m_TargetFPS == 0 ? "Uncapped" : std::to_string(m_TargetFPS).c_str(),
            m_VSync ? "ON" : "OFF");
    }

    bool LoadSceneFromPack(const std::string& scenePath) {
        std::string sceneStr;

        if (m_LooseFilesMode) {
            // Read scene file directly from disk
            std::string fullPath = (fs::path(m_LooseFilesDir) / scenePath).string();
            std::ifstream file(fullPath);
            if (!file.is_open()) {
                ENJIN_LOG_ERROR(Player, "Failed to read scene from disk: %s", fullPath.c_str());
                return false;
            }
            sceneStr = std::string((std::istreambuf_iterator<char>(file)),
                                    std::istreambuf_iterator<char>());
            file.close();
        } else {
            // Read scene from .enjpak
            auto sceneData = m_AssetReader.ReadFile(scenePath);
            if (sceneData.empty()) {
                ENJIN_LOG_ERROR(Player, "Failed to read scene from pack: %s", scenePath.c_str());
                return false;
            }
            sceneStr = std::string(sceneData.begin(), sceneData.end());
        }
        Enjin::Scene::SceneSerializer serializer(m_World.get());
        auto result = serializer.LoadFromString(sceneStr, true);

        if (!result.success) {
            ENJIN_LOG_ERROR(Player, "Failed to load scene: %s (%s)", scenePath.c_str(), result.error.c_str());
            return false;
        }

        // Apply skybox
        if (m_RenderSystem) {
            m_RenderSystem->SetSkybox(serializer.GetSkyboxConfig());
        }

        // Apply scene render settings (shadows, ambient, cel shading, post-processing, etc.)
        // RT is allowed through since 0.9.7: the historical force-off dated from when
        // RTShaderData.h held invalid placeholder SPIR-V (crashed the NVIDIA driver);
        // all RT/compute shaders are real embedded SPIR-V now, and unsupported GPUs
        // bail out gracefully in InitializeRayTracing (IsRayTracingSupported check).
        if (m_RenderSystem) {
            auto renderSettings = serializer.GetRenderSettings();
            renderSettings.ApplyToRuntime(m_RenderSystem,
                m_PostProcessing ? &m_PostProcessing->GetSettings() : nullptr);
        }

        // Read content warning flags from scene JSON if present (Task #39).
        // SceneSerializer writes them under "accessibility"; "contentWarnings"
        // stays supported for hand-authored scene files.
        try {
            auto sceneJson = nlohmann::json::parse(sceneStr);
            const char* cwKey = sceneJson.contains("accessibility") ? "accessibility"
                              : (sceneJson.contains("contentWarnings") ? "contentWarnings" : nullptr);
            if (cwKey) {
                auto& cw = sceneJson[cwKey];
                Enjin::Accessibility::SceneContentFlags flags;
                if (cw.contains("flags")) {
                    flags.flags = static_cast<Enjin::Accessibility::ContentWarningType>(
                        cw["flags"].get<Enjin::u32>());
                }
                if (cw.contains("customWarnings") && cw["customWarnings"].is_array()) {
                    for (const auto& w : cw["customWarnings"]) {
                        flags.customWarnings.push_back(w.get<std::string>());
                    }
                }
                m_ContentWarnings.SetSceneFlags(flags);
                if (m_ContentWarnings.HasWarnings()) {
                    ENJIN_LOG_INFO(Player, "Scene has content warnings — showing overlay");
                }
            }
        } catch (const std::exception&) {
            // JSON parse failure for content warnings is non-fatal
        }

        // Build audio occlusion scene from colliders
#ifdef ENJIN_AUDIO_STEAM_AUDIO
        m_SimpleAudio.BuildSteamAudioScene();
#endif

        ENJIN_LOG_INFO(Player, "Loaded scene: %s (%zu entities)", scenePath.c_str(), result.entities.size());
        return true;
    }

    // Push m_AccessibilitySettings into every consumer. Called at boot and by
    // the in-game Accessibility menu tab on every change — several consumers
    // (subtitle config, indicators, announcer, UISystem motor toggles, fonts)
    // only read these values when pushed, unlike the per-frame PP apply.
    void ApplyAccessibilitySettings() {
        // Subtitles
        auto& subConfig = m_SubtitleSystem.GetConfig();
        subConfig.enabled = m_AccessibilitySettings.subtitlesEnabled;
        subConfig.captionsEnabled = m_AccessibilitySettings.closedCaptionsEnabled;
        subConfig.fontSize = m_AccessibilitySettings.subtitleFontSize;
        subConfig.backgroundOpacity = m_AccessibilitySettings.subtitleBgOpacity;
        subConfig.showSpeakerNames = m_AccessibilitySettings.subtitleSpeakerNames;
        subConfig.showDirectionIndicators = m_AccessibilitySettings.subtitleDirectionIndicators;

        // Screen reader (visual status bar + platform TTS)
        m_Announcer.enabled = m_AccessibilitySettings.screenReaderEnabled;

        // Motion
        m_ControllerSystem.SetReducedMotion(m_AccessibilitySettings.reducedMotion);
        m_ControllerSystem.SetDisableScreenShake(m_AccessibilitySettings.disableScreenShake);
        m_ControllerSystem.SetDisableFOVEffects(m_AccessibilitySettings.disableFOVEffects);
        m_ControllerSystem.SetInvertMouseY(m_AccessibilitySettings.invertMouseY);
        m_UISystem.SetReducedMotion(m_AccessibilitySettings.reducedMotion);

        // Font scale + motor accessibility
        m_UISystem.SetFontScale(m_AccessibilitySettings.fontScale);
        m_UISystem.SetSwitchAccessEnabled(m_AccessibilitySettings.switchAccessEnabled,
                                           m_AccessibilitySettings.switchScanSpeed);
        m_UISystem.SetDwellClickEnabled(m_AccessibilitySettings.dwellClickEnabled,
                                         m_AccessibilitySettings.dwellClickTime);
        m_UISystem.SetStickyDragEnabled(m_AccessibilitySettings.stickyDragEnabled);

        // Audio visual indicators (callback wired unconditionally — the overlay
        // render gates on config.enabled, so a disabled state just drops events)
        m_AudioIndicators.GetConfig().enabled = m_AccessibilitySettings.audioIndicatorsEnabled;
        m_SimpleAudio.SetOnSoundPlayed([this](const std::string& soundName) {
            if (m_AudioIndicators.GetConfig().enabled) {
                m_AudioIndicators.ShowIndicator(soundName,
                    Enjin::Math::Vector3(0.4f, 0.8f, 1.0f), 1.5f);
            }
        });

        // Fonts (dyslexia mode swaps to OpenDyslexic when embedded, else spacing)
        if (m_AccessibilitySettings.dyslexiaFriendly) {
            m_FontLibrary.SetFont(Enjin::Accessibility::FontFamily::OpenDyslexic);
        } else {
            m_FontLibrary.SetFont(m_AccessibilitySettings.fontFamily);
        }
        {
            Enjin::Accessibility::FontLibraryConfig flConfig;
            flConfig.selectedFamily = m_AccessibilitySettings.dyslexiaFriendly
                ? Enjin::Accessibility::FontFamily::OpenDyslexic
                : m_AccessibilitySettings.fontFamily;
            flConfig.letterSpacing = m_AccessibilitySettings.letterSpacing;
            flConfig.wordSpacing = m_AccessibilitySettings.wordSpacing;
            flConfig.lineSpacing = m_AccessibilitySettings.lineSpacing;
            m_FontLibrary.SetConfig(flConfig);
        }

        // Colorblind + brightness/contrast into post-processing
        if (m_PostProcessing) {
            m_AccessibilitySettings.ApplyToPostProcessing(m_PostProcessing->GetSettings());
        }
        if (m_RenderSystem) {
            m_RenderSystem->SetWebAccessibility(
                static_cast<Enjin::u32>(m_AccessibilitySettings.colorblindMode),
                m_AccessibilitySettings.colorblindStrength,
                m_AccessibilitySettings.screenBrightness,
                m_AccessibilitySettings.screenContrast);
        }
    }

    // Player-remapped controls persist next to accessibility.json. Ship-time
    // defaults come from the pak/scene; this file only exists after a rebind.
    void LoadInputBindings() {
        std::string exeDir = Enjin::Platform::GetExecutableDirectory();
        std::string path = (fs::path(exeDir) / "bindings.json").string();
        if (!fs::exists(path)) return;
        std::ifstream file(path);
        if (!file.is_open()) return;
        std::stringstream ss;
        ss << file.rdbuf();
        if (m_InputMap.FromJson(ss.str())) {
            ENJIN_LOG_INFO(Player, "Loaded control bindings from %s", path.c_str());
        } else {
            ENJIN_LOG_WARN(Player, "Failed to parse bindings.json — using defaults");
        }
    }

    void SaveInputBindings() {
        std::string exeDir = Enjin::Platform::GetExecutableDirectory();
        std::string path = (fs::path(exeDir) / "bindings.json").string();
        try {
            std::ofstream file(path);
            file << m_InputMap.ToJson();
            ENJIN_LOG_INFO(Player, "Saved control bindings to %s", path.c_str());
        } catch (const std::exception& e) {
            ENJIN_LOG_WARN(Player, "Failed to save bindings.json: %s", e.what());
        }
    }

    void LoadAccessibilitySettings() {
        std::string exeDir = Enjin::Platform::GetExecutableDirectory();
        std::string settingsPath = (fs::path(exeDir) / "accessibility.json").string();

        // Try loading from pack first, then from file system
        std::string jsonStr;
        auto packData = m_AssetReader.ReadFile("accessibility.json");
        if (!packData.empty()) {
            jsonStr.assign(packData.begin(), packData.end());
        } else if (fs::exists(settingsPath)) {
            std::ifstream file(settingsPath);
            if (file.is_open()) {
                jsonStr.assign(std::istreambuf_iterator<char>(file),
                               std::istreambuf_iterator<char>());
            }
        }

        if (jsonStr.empty()) {
            ENJIN_LOG_INFO(Player, "No accessibility.json found — using defaults");
            return;
        }

        try {
            auto j = nlohmann::json::parse(jsonStr);

            // Visual settings
            if (j.contains("colorblindMode"))
                m_AccessibilitySettings.colorblindMode = static_cast<Enjin::Accessibility::ColorblindMode>(
                    j["colorblindMode"].get<Enjin::u32>());
            if (j.contains("colorblindStrength"))
                m_AccessibilitySettings.colorblindStrength = j["colorblindStrength"].get<Enjin::f32>();
            if (j.contains("screenBrightness"))
                m_AccessibilitySettings.screenBrightness = j["screenBrightness"].get<Enjin::f32>();
            if (j.contains("screenContrast"))
                m_AccessibilitySettings.screenContrast = j["screenContrast"].get<Enjin::f32>();

            // Motion settings
            if (j.contains("reducedMotion"))
                m_AccessibilitySettings.reducedMotion = j["reducedMotion"].get<bool>();
            if (j.contains("disableScreenShake"))
                m_AccessibilitySettings.disableScreenShake = j["disableScreenShake"].get<bool>();
            if (j.contains("disableFOVEffects"))
                m_AccessibilitySettings.disableFOVEffects = j["disableFOVEffects"].get<bool>();
            if (j.contains("disableFlashingLights"))
                m_AccessibilitySettings.disableFlashingLights = j["disableFlashingLights"].get<bool>();

            // Subtitle settings
            if (j.contains("subtitlesEnabled"))
                m_AccessibilitySettings.subtitlesEnabled = j["subtitlesEnabled"].get<bool>();
            if (j.contains("closedCaptionsEnabled"))
                m_AccessibilitySettings.closedCaptionsEnabled = j["closedCaptionsEnabled"].get<bool>();
            if (j.contains("subtitleFontSize"))
                m_AccessibilitySettings.subtitleFontSize = j["subtitleFontSize"].get<Enjin::f32>();
            if (j.contains("subtitleBgOpacity"))
                m_AccessibilitySettings.subtitleBgOpacity = j["subtitleBgOpacity"].get<Enjin::f32>();
            if (j.contains("subtitleSpeakerNames"))
                m_AccessibilitySettings.subtitleSpeakerNames = j["subtitleSpeakerNames"].get<bool>();
            if (j.contains("subtitleDirectionIndicators"))
                m_AccessibilitySettings.subtitleDirectionIndicators = j["subtitleDirectionIndicators"].get<bool>();

            // Font scaling
            if (j.contains("fontScale")) {
                Enjin::f32 scale = j["fontScale"].get<Enjin::f32>();
                m_AccessibilitySettings.fontScale = std::clamp(scale, 0.5f, 3.0f);
            }

            // Dyslexia-friendly settings
            if (j.contains("dyslexiaFriendly"))
                m_AccessibilitySettings.dyslexiaFriendly = j["dyslexiaFriendly"].get<bool>();
            if (j.contains("letterSpacing"))
                m_AccessibilitySettings.letterSpacing = j["letterSpacing"].get<Enjin::f32>();
            if (j.contains("wordSpacing"))
                m_AccessibilitySettings.wordSpacing = j["wordSpacing"].get<Enjin::f32>();
            if (j.contains("lineSpacing"))
                m_AccessibilitySettings.lineSpacing = std::clamp(j["lineSpacing"].get<Enjin::f32>(), 1.0f, 3.0f);
            if (j.contains("fontFamily")) {
                Enjin::u32 ff = j["fontFamily"].get<Enjin::u32>();
                if (ff <= 2) m_AccessibilitySettings.fontFamily = static_cast<Enjin::Accessibility::FontFamily>(ff);
            }

            // Motor accessibility settings (Task #40)
            if (j.contains("dwellClickEnabled"))
                m_AccessibilitySettings.dwellClickEnabled = j["dwellClickEnabled"].get<bool>();
            if (j.contains("dwellClickTime"))
                m_AccessibilitySettings.dwellClickTime = std::clamp(j["dwellClickTime"].get<Enjin::f32>(), 0.3f, 3.0f);
            if (j.contains("stickyDragEnabled"))
                m_AccessibilitySettings.stickyDragEnabled = j["stickyDragEnabled"].get<bool>();
            if (j.contains("switchAccessEnabled"))
                m_AccessibilitySettings.switchAccessEnabled = j["switchAccessEnabled"].get<bool>();
            if (j.contains("switchScanSpeed"))
                m_AccessibilitySettings.switchScanSpeed = std::clamp(j["switchScanSpeed"].get<Enjin::f32>(), 0.5f, 5.0f);

            // Audio visual indicators (Task #38)
            if (j.contains("audioIndicatorsEnabled"))
                m_AccessibilitySettings.audioIndicatorsEnabled = j["audioIndicatorsEnabled"].get<bool>();

            // Screen reader (announcer status bar + TTS)
            if (j.contains("screenReaderEnabled"))
                m_AccessibilitySettings.screenReaderEnabled = j["screenReaderEnabled"].get<bool>();

            // Alternative input device settings (Task #37)
            if (j.contains("alternativeInput")) {
                auto& ai = j["alternativeInput"];
                if (ai.contains("switchAccess")) {
                    auto& sa = ai["switchAccess"];
                    auto config = m_AlternativeInput.GetSwitchConfig();
                    if (sa.contains("enabled")) config.enabled = sa["enabled"].get<bool>();
                    if (sa.contains("scanSpeed")) config.scanSpeed = sa["scanSpeed"].get<Enjin::f32>();
                    if (sa.contains("switchCount")) config.switchCount = sa["switchCount"].get<Enjin::i32>();
                    m_AlternativeInput.SetSwitchConfig(config);
                }
                if (ai.contains("eyeTracking")) {
                    auto& et = ai["eyeTracking"];
                    auto config = m_AlternativeInput.GetEyeTrackingConfig();
                    if (et.contains("enabled")) config.enabled = et["enabled"].get<bool>();
                    if (et.contains("dwellTime")) config.dwellTime = et["dwellTime"].get<Enjin::f32>();
                    if (et.contains("smoothingFactor")) config.smoothingFactor = et["smoothingFactor"].get<Enjin::f32>();
                    m_AlternativeInput.SetEyeTrackingConfig(config);
                }
                if (ai.contains("headTracking")) {
                    auto& ht = ai["headTracking"];
                    auto config = m_AlternativeInput.GetHeadTrackingConfig();
                    if (ht.contains("enabled")) config.enabled = ht["enabled"].get<bool>();
                    if (ht.contains("sensitivity")) config.sensitivity = ht["sensitivity"].get<Enjin::f32>();
                    if (ht.contains("invertX")) config.invertX = ht["invertX"].get<bool>();
                    if (ht.contains("invertY")) config.invertY = ht["invertY"].get<bool>();
                    m_AlternativeInput.SetHeadTrackingConfig(config);
                }
            }

            // Input settings
            if (j.contains("sprintMode"))
                m_AccessibilitySettings.sprintMode = j["sprintMode"].get<Enjin::u32>();
            if (j.contains("crouchMode"))
                m_AccessibilitySettings.crouchMode = j["crouchMode"].get<Enjin::u32>();
            if (j.contains("mouseSensitivity"))
                m_AccessibilitySettings.mouseSensitivity = j["mouseSensitivity"].get<Enjin::f32>();
            if (j.contains("invertMouseY"))
                m_AccessibilitySettings.invertMouseY = j["invertMouseY"].get<bool>();

            ENJIN_LOG_INFO(Player, "Loaded accessibility settings (colorblind=%u, reducedMotion=%s, fontScale=%.1f, subtitles=%s, dyslexia=%s, dwellClick=%s, stickyDrag=%s, switchAccess=%s, audioIndicators=%s)",
                static_cast<Enjin::u32>(m_AccessibilitySettings.colorblindMode),
                m_AccessibilitySettings.reducedMotion ? "ON" : "OFF",
                m_AccessibilitySettings.fontScale,
                m_AccessibilitySettings.subtitlesEnabled ? "ON" : "OFF",
                m_AccessibilitySettings.dyslexiaFriendly ? "ON" : "OFF",
                m_AccessibilitySettings.dwellClickEnabled ? "ON" : "OFF",
                m_AccessibilitySettings.stickyDragEnabled ? "ON" : "OFF",
                m_AccessibilitySettings.switchAccessEnabled ? "ON" : "OFF",
                m_AccessibilitySettings.audioIndicatorsEnabled ? "ON" : "OFF");
        } catch (const std::exception& e) {
            ENJIN_LOG_WARN(Player, "Failed to parse accessibility.json: %s", e.what());
        }
    }

    void SaveAccessibilitySettings() {
        std::string exeDir = Enjin::Platform::GetExecutableDirectory();
        std::string settingsPath = (fs::path(exeDir) / "accessibility.json").string();

        try {
            nlohmann::json j;
            j["colorblindMode"] = static_cast<Enjin::u32>(m_AccessibilitySettings.colorblindMode);
            j["colorblindStrength"] = m_AccessibilitySettings.colorblindStrength;
            j["screenBrightness"] = m_AccessibilitySettings.screenBrightness;
            j["screenContrast"] = m_AccessibilitySettings.screenContrast;
            j["reducedMotion"] = m_AccessibilitySettings.reducedMotion;
            j["disableScreenShake"] = m_AccessibilitySettings.disableScreenShake;
            j["disableFOVEffects"] = m_AccessibilitySettings.disableFOVEffects;
            j["disableFlashingLights"] = m_AccessibilitySettings.disableFlashingLights;
            j["subtitlesEnabled"] = m_AccessibilitySettings.subtitlesEnabled;
            j["closedCaptionsEnabled"] = m_AccessibilitySettings.closedCaptionsEnabled;
            j["subtitleFontSize"] = m_AccessibilitySettings.subtitleFontSize;
            j["subtitleBgOpacity"] = m_AccessibilitySettings.subtitleBgOpacity;
            j["subtitleSpeakerNames"] = m_AccessibilitySettings.subtitleSpeakerNames;
            j["subtitleDirectionIndicators"] = m_AccessibilitySettings.subtitleDirectionIndicators;
            j["fontScale"] = m_AccessibilitySettings.fontScale;
            j["dyslexiaFriendly"] = m_AccessibilitySettings.dyslexiaFriendly;
            j["letterSpacing"] = m_AccessibilitySettings.letterSpacing;
            j["wordSpacing"] = m_AccessibilitySettings.wordSpacing;
            j["lineSpacing"] = m_AccessibilitySettings.lineSpacing;
            j["fontFamily"] = static_cast<Enjin::u32>(m_AccessibilitySettings.fontFamily);
            j["dwellClickEnabled"] = m_AccessibilitySettings.dwellClickEnabled;
            j["dwellClickTime"] = m_AccessibilitySettings.dwellClickTime;
            j["stickyDragEnabled"] = m_AccessibilitySettings.stickyDragEnabled;
            j["switchAccessEnabled"] = m_AccessibilitySettings.switchAccessEnabled;
            j["switchScanSpeed"] = m_AccessibilitySettings.switchScanSpeed;
            j["audioIndicatorsEnabled"] = m_AccessibilitySettings.audioIndicatorsEnabled;
            j["screenReaderEnabled"] = m_AccessibilitySettings.screenReaderEnabled;
            j["sprintMode"] = m_AccessibilitySettings.sprintMode;
            j["crouchMode"] = m_AccessibilitySettings.crouchMode;
            j["mouseSensitivity"] = m_AccessibilitySettings.mouseSensitivity;
            j["invertMouseY"] = m_AccessibilitySettings.invertMouseY;

            std::ofstream file(settingsPath);
            file << j.dump(2);
            ENJIN_LOG_INFO(Player, "Saved accessibility settings to %s", settingsPath.c_str());
        } catch (const std::exception& e) {
            ENJIN_LOG_WARN(Player, "Failed to save accessibility.json: %s", e.what());
        }
    }

    // Default pack key — matches the build pipeline default
    static constexpr const char* PACK_KEY = "enjin_default_pack_key_2025";

    // Loose files mode — when true, assets are loaded from disk instead of .enjpak
    bool m_LooseFilesMode = false;
    std::string m_LooseFilesDir;

    bool m_Initialized = false;
    bool m_GameStarted = false;
    bool m_Paused = false;                 // Unified canvas pause (desktop)
    Enjin::ECS::Entity m_PauseMenuEntity = Enjin::ECS::INVALID_ENTITY;
    bool m_FullscreenChangeRequested = false;
    bool m_PendingFullscreen = false;
    Enjin::f32 m_PendingFOV = 0.0f;  // 0 = use camera component's FOV
    std::vector<Enjin::ECS::Entity> m_DeferredDestroys;

    // Tilde console
    bool m_ShowConsole = false;
    Enjin::f32 m_ConsoleAnim = 0.0f;
    char m_ConsoleInput[512] = {};
    std::vector<std::string> m_ConsoleLog;
    std::vector<std::string> m_ConsoleHistory;
    int m_ConsoleHistoryPos = -1;

    // Splash screen
    bool m_ShowingSplash = false;
    bool m_EngineSplash = true;          // "Made with TEGE" intro card (build setting)
    Enjin::f32 m_EngineSplashTimer = 0.0f;
    Enjin::f32 m_SplashTimer = 0.0f;
    Enjin::f32 m_SplashAlpha = 0.0f;
    static constexpr Enjin::f32 m_SplashDuration = 3.0f;
    Enjin::ECS::Entity m_SplashTitleEntity = 0;
    Enjin::ECS::Entity m_SplashSubEntity = 0;
    Enjin::ECS::Entity m_SplashLightEntity = 0;

    // Build manifest data
    std::string m_WindowTitle;
    Enjin::u32 m_WindowWidth = 1280;
    Enjin::u32 m_WindowHeight = 720;
    bool m_Fullscreen = false;
    std::string m_StartScene;

    // Frame rate settings
    Enjin::u32 m_TargetFPS = 60;
    bool m_VSync = true;
    Enjin::u32 m_BackgroundBehavior = 1; // 0=RunNormally, 1=ReduceTo30, 2=Pause

    // Physics backend settings (from build manifest)
    Enjin::u32 m_PhysicsBackendType = 0;  // 0=Auto, 1=Jolt, 2=Box2D, 3=Simple
    Enjin::u32 m_ProjectMode = 1;         // 0=2D, 1=3D, 2=Mixed

    // Core systems
    std::unique_ptr<Enjin::Renderer::VulkanRenderer> m_Renderer;
    std::unique_ptr<Enjin::Renderer::Camera> m_Camera;
    std::unique_ptr<Enjin::Renderer::CameraController> m_CameraController;
    std::unique_ptr<Enjin::ECS::World> m_World;
    Enjin::ECS::RenderSystem* m_RenderSystem = nullptr;

    // Asset reader
    Enjin::Build::AssetReader m_AssetReader;

    // Input & menus
    Enjin::InputSystem::InputActionMap m_InputMap;
    Enjin::GUI::GameMenuSystem m_GameMenu;

    // Runtime UI system
    Enjin::GUI::UISystem m_UISystem;

    // Scripting
    Enjin::Scripting::ScriptEngine m_ScriptEngine;
    Enjin::Scripting::ScriptSystem m_ScriptSystem;
    Enjin::Scripting::CoroutineScheduler m_CoroutineScheduler;
    Enjin::Scripting::ScriptEventBus m_ScriptEventBus;

    // ImGui overlay
    std::unique_ptr<Enjin::GUI::ImGuiLayer> m_ImGuiLayer;

    // Physics (created via factory)
    std::unique_ptr<Enjin::Physics::IPhysicsBackend> m_Physics;
    std::unique_ptr<Enjin::Physics::IPhysicsBackend2D> m_Physics2D;

    // Gameplay systems
    Enjin::ECS::ControllerSystem m_ControllerSystem;
    Enjin::ECS::FlowerSystem m_FlowerSystem;
    Enjin::ECS::TweenSystem m_TweenSystem;
    Enjin::ECS::StateMachineSystem m_StateMachineSystem;
    Enjin::ECS::VisualScriptSystem m_VisualScriptSystem;
    Enjin::ECS::BehaviorTreeSystem m_BehaviorTreeSystem;
    Enjin::ECS::AISystem m_AISystem;
    Enjin::Gameplay::RecordRewindSystem m_RecordRewindSystem;
    Enjin::ECS::EntityEventBus m_EntityEventBus;
    Enjin::ECS::DialogueSystem m_DialogueSystem;
    Enjin::ECS::Entity m_ActiveDialogueEntity = 0;
    Enjin::Gameplay::QuestSystem m_QuestSystem;
    Enjin::Gameplay::FootstepSystem m_FootstepSystem;
    Enjin::Gameplay::ObjectPool m_ObjectPool;
    Enjin::Gameplay::CinematicSystem m_CinematicSystem;
    Enjin::Gameplay::TieredSaveSystem m_TieredSaveSystem;

    // Systems for script bindings
    Enjin::Audio::SimpleAudio m_SimpleAudio;
    Enjin::Effects::WeatherSystem m_WeatherSystem;
    Enjin::Effects::DestructibleSystem m_DestructibleSystem;
    Enjin::Effects::InteractiveWaterSystem m_InteractiveWaterSystem;
    Enjin::Scene::StreamingManager m_StreamingManager;
    Enjin::Scene::SceneManager m_SceneManager;
    Enjin::Networking::NetworkSystem m_NetworkSystem;

    // Particle system (CPU simulation for ParticleEmitterComponent)
    Enjin::Effects::ParticleSystem m_ParticleSystem;

    // Fluid simulation, terrain coupling, curl noise, wind, world time, seasonal weather
    Enjin::Effects::FluidSimulation m_FluidSimulation;
    Enjin::Effects::FluidTerrainCoupling m_FluidTerrainCoupling;
    Enjin::Effects::CurlNoiseSystem m_CurlNoiseSystem;
    Enjin::Effects::WindSystem m_WindSystem;
    Enjin::Effects::WorldTimeSystem m_WorldTime;
    Enjin::Effects::SeasonalWeatherSystem m_SeasonalWeather;

    // Elemental system (fire/water/earth/air) + fire-light injection buffers.
    // Parity with the editor (EditorLayer) so shipped games run the same sim and
    // fire emitters light the scene as transient point lights.
    Enjin::Effects::ElementalSystem m_ElementalSystem;
    Enjin::f32 m_EffectsTime = 0.0f;
    std::vector<Enjin::Effects::FireLight> m_FireLights;

    // Audio-reactive system (beat sync / FFT-driven parameters) — editor parity.
    Enjin::Audio::AudioReactiveSystem m_AudioReactiveSystem;

    Enjin::Effects::Water3D m_Water3D;

    // Retro effects (dither/CRT/VHS/vertex-snap/etc.) are not driven here: they
    // are baked into the scene's SceneRenderSettings by the editor and applied
    // at load via renderSettings.ApplyToRuntime() (see LoadSceneFromPack).

    // Audio event graph runtime
    Enjin::Audio::AudioEventGraphRuntime m_AudioGraphRuntime;

    // MIDI input
    Enjin::InputSystem::MIDIInput m_MIDIInput;

    // Accessibility systems
    Enjin::Accessibility::SubtitleSystem m_SubtitleSystem;
    Enjin::Accessibility::AlternativeInputManager m_AlternativeInput;
    Enjin::Accessibility::AudioVisualIndicatorSystem m_AudioIndicators;
    Enjin::Accessibility::ContentWarningSystem m_ContentWarnings;
    Enjin::Accessibility::AccessibilityAnnouncer m_Announcer;
    Enjin::Accessibility::RuntimeAccessibilitySettings m_AccessibilitySettings;
    Enjin::Accessibility::FontLibrary m_FontLibrary;

    // Post-processing: settings for script bindings + the path tracer display draw
    std::unique_ptr<Enjin::Renderer::PostProcessing> m_PostProcessing;
    // Offscreen scene target for the raster post-process path: the scene
    // renders here (like the editor's game view) so the post-process pass has
    // something to sample. Without it, postprocess.frag (colorblind, tonemap,
    // brightness/contrast, vignette, FXAA) NEVER ran in exported games.
    // Gated off — see the creation site for what broke on the first attempt.
    static constexpr bool kEnableRasterPP = true;
    std::unique_ptr<Enjin::Renderer::RenderTarget> m_ScenePPTarget;
    bool m_RasterPPThisFrame = false;
    Enjin::f32 m_FrameDeltaTime = 0.016f;

    // Player hybrid RT overlay: blends ray-traced shadow/AO/reflect/GI onto the
    // swapchain scene (the player has no offscreen target for the editor's PP
    // overlay path). See RTHybridApply.
    std::unique_ptr<Enjin::Renderer::RTHybridApply> m_RTHybridApply;

    // Last view bound as the PP source (plain descriptor set — only rewrite on change)
    VkImageView m_LastPTSourceView = VK_NULL_HANDLE;

    // ImGui texture descriptor cache for UISystem texture resolver
    std::unordered_map<std::string, VkDescriptorSet> m_ImGuiTextureCache;

    VkDescriptorSet GetImGuiTexture(const std::string& path) {
        if (path.empty() || !m_RenderSystem) return VK_NULL_HANDLE;
        auto it = m_ImGuiTextureCache.find(path);
        if (it != m_ImGuiTextureCache.end()) return it->second;
        auto tex = m_RenderSystem->LoadTexture(path);
        if (!tex || !tex->IsValid()) return VK_NULL_HANDLE;
        VkDescriptorSet ds = ImGui_ImplVulkan_AddTexture(
            tex->GetSampler(), tex->GetImageView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        if (ds != VK_NULL_HANDLE) m_ImGuiTextureCache[path] = ds;
        return ds;
    }

    void UpdateDialogue(Enjin::f32 deltaTime) {
        m_DialogueSystem.Update(m_World.get(), deltaTime);
        m_ActiveDialogueEntity = m_DialogueSystem.GetActiveDialogueEntity();
    }

    void DrawDialogueOverlay() {
        if (!m_World || m_ActiveDialogueEntity == 0) return;

        auto* dlg = m_World->GetComponent<Enjin::ECS::DialogueComponent>(m_ActiveDialogueEntity);
        if (!dlg) return;

        // Determine speaker, visible text, and choices based on mode
        std::string speaker;
        std::string visibleText;
        bool isTyping = dlg->isTyping;
        bool waiting = dlg->waitingForInput;
        bool hasChoices = false;
        Enjin::i32 selectedChoice = dlg->selectedChoice;
        Enjin::i32 choiceCount = 0;

        if (dlg->IsTreeMode()) {
            if (!dlg->treeActive) return;
            speaker = dlg->currentSpeaker;
            visibleText = dlg->GetTreeVisibleText();
            hasChoices = waiting && !dlg->currentChoices.empty();
            choiceCount = static_cast<Enjin::i32>(dlg->currentChoices.size());
        } else {
            if (dlg->IsComplete()) return;
            speaker = dlg->speakerName;
            visibleText = dlg->GetVisibleText();
            hasChoices = waiting && !dlg->choices.empty() &&
                         dlg->currentLine + 1 >= dlg->dialogueLines.size();
            choiceCount = static_cast<Enjin::i32>(dlg->choices.size());
        }

        ImGuiIO& io = ImGui::GetIO();
        Enjin::f32 screenW = io.DisplaySize.x;
        Enjin::f32 screenH = io.DisplaySize.y;

        Enjin::f32 boxW = screenW * 0.75f;
        Enjin::f32 boxH = 140.0f;
        Enjin::f32 boxX = (screenW - boxW) * 0.5f;
        Enjin::f32 boxY = screenH - boxH - 30.0f;
        Enjin::f32 padding = 16.0f;

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
                Enjin::f32 blink = std::fmod(static_cast<Enjin::f32>(ImGui::GetTime()) * 3.0f, 2.0f);
                if (blink < 1.0f) {
                    ImGui::TextColored(ImVec4(0.7f, 0.8f, 1.0f, 0.8f), "_");
                }
            }

            if (waiting && !hasChoices) {
                Enjin::f32 bounce = std::sin(static_cast<Enjin::f32>(ImGui::GetTime()) * 4.0f) * 0.3f + 0.7f;
                ImGui::SetCursorPosY(boxH - padding - ImGui::GetTextLineHeight());
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.6f, 0.8f, bounce));
                ImGui::Text("[Space]");
                ImGui::PopStyleColor();
            }
        }
        ImGui::End();

        if (hasChoices) {
            Enjin::f32 choiceH = static_cast<Enjin::f32>(choiceCount) * 28.0f + padding * 2.0f;
            Enjin::f32 choiceW = 300.0f;
            Enjin::f32 choiceX = boxX + boxW - choiceW - 10.0f;
            Enjin::f32 choiceY = boxY - choiceH - 8.0f;

            ImGui::SetNextWindowPos(ImVec2(choiceX, choiceY));
            ImGui::SetNextWindowSize(ImVec2(choiceW, choiceH));

            if (ImGui::Begin("##ChoiceBox", nullptr, flags)) {
                if (dlg->IsTreeMode()) {
                    for (Enjin::usize i = 0; i < dlg->currentChoices.size(); ++i) {
                        bool sel = (static_cast<Enjin::i32>(i) == selectedChoice);
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
                    for (Enjin::usize i = 0; i < dlg->choices.size(); ++i) {
                        bool sel = (static_cast<Enjin::i32>(i) == selectedChoice);
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

    void EvaluatePostProcessVolumes(const Enjin::Math::Vector3& cameraPosition) {
        if (!m_PostProcessing || !m_World) return;

        auto volumeEntities = m_World->GetEntitiesWithComponent<Enjin::ECS::PostProcessVolumeComponent>();
        if (volumeEntities.empty()) return;

        auto& currentSettings = m_PostProcessing->GetSettings();

        struct VolumeEntry {
            const Enjin::ECS::PostProcessVolumeComponent* vol;
            Enjin::f32 blendWeight;
        };
        std::vector<VolumeEntry> activeVolumes;
        activeVolumes.reserve(volumeEntities.size());

        for (auto entity : volumeEntities) {
            auto* vol = m_World->GetComponent<Enjin::ECS::PostProcessVolumeComponent>(entity);
            if (!vol || !vol->isActive) continue;

            Enjin::Math::Vector3 center(0, 0, 0);
            if (!vol->isGlobal) {
                auto* transform = m_World->GetComponent<Enjin::ECS::TransformComponent>(entity);
                if (!transform) continue;
                center = transform->position;
            }

            Enjin::f32 w = vol->GetBlendWeight(center, cameraPosition);
            if (w <= 0.001f) continue;

            activeVolumes.push_back({ vol, w });
        }

        if (activeVolumes.empty()) return;

        std::sort(activeVolumes.begin(), activeVolumes.end(),
            [](const VolumeEntry& a, const VolumeEntry& b) {
                return a.vol->priority < b.vol->priority;
            });

        Enjin::Renderer::PostProcessSettings blended = currentSettings;
        for (auto& entry : activeVolumes) {
            Enjin::ECS::BlendPostProcessSettings(blended, blended, entry.vol->settings,
                entry.blendWeight, entry.vol->overrideMask);
        }

        currentSettings = blended;
    }

    // Gameplay processing methods (ProcessContactDamage, ProcessPickup,
    // UpdateHealthSystems, FlushDeferredDestroys) are now in
    // Enjin::Gameplay::GameplayLoop (Engine/src/Gameplay/GameplayLoop.cpp).
};

Enjin::Application* CreateApplication() {
    return new GamePlayer();
}

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    // Set working directory to exe location so relative paths work
    Enjin::Platform::SetWorkingDirectoryToExecutableDirectory();

    Enjin::Application* app = CreateApplication();
    int result = app->Run();
    delete app;
    return result;
}
