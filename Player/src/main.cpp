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
#include "Enjin/Effects/WorldTime.h"
#include "Enjin/Effects/SeasonalWeather.h"
#include "Enjin/Effects/Water.h"
#include "Enjin/ECS/Components/Water3D.h"
#include "Enjin/Audio/AudioSystem.h"
#include "Enjin/Audio/SimpleAudio.h"
#include "Enjin/Build/AssetReader.h"
#include "Enjin/Scripting/ScriptEngine.h"
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
#include "Enjin/ECS/EntityEventBus.h"
#include "Enjin/Gameplay/HUDSystem.h"
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
#include "Enjin/ECS/Components/PostProcessVolume.h"
#include "Enjin/Editor/AudioEventGraph.h"
#include "Enjin/Assets/DataAsset.h"
#include "Enjin/Gameplay/GameplayLoop.h"
#include <nlohmann/json.hpp>
#include <iostream>
#include <memory>
#include <filesystem>
#include <fstream>
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

        // Initialize ImGui layer for pause menu and dialogue overlays
        m_ImGuiLayer = std::make_unique<Enjin::GUI::ImGuiLayer>();
        if (!m_ImGuiLayer->Initialize(GetWindow(), m_Renderer.get())) {
            ENJIN_LOG_WARN(Player, "Failed to initialize ImGui layer — menus will not render");
            m_ImGuiLayer.reset();
        }

        // Connect game menu to input system
        m_GameMenu.SetInputMap(&m_InputMap);
        m_GameMenu.SetGameTitle(m_WindowTitle.empty() ? "Game" : m_WindowTitle);

        // Wire menu button callbacks for title screen and pause menu
        m_GameMenu.SetCallback([this](const std::string& action) {
            if (action == "new_game" || action == "continue") {
                m_GameMenu.HideAll();
                m_GameStarted = true;
                Enjin::Input::SetMouseCaptured(true);
            } else if (action == "resume") {
                m_GameMenu.HideAll();
                Enjin::Input::SetMouseCaptured(true);
            } else if (action == "options") {
                m_GameMenu.ShowScreen(Enjin::GUI::MenuScreen::Options);
            } else if (action == "how_to_play") {
                m_GameMenu.ShowScreen(Enjin::GUI::MenuScreen::HowToPlay);
            } else if (action == "restart") {
                m_GameMenu.HideAll();
                Enjin::Input::SetMouseCaptured(true);
                // Recreate physics backends so stale bodies/controllers don't persist
                {
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
                    // Re-wire 2D collision callbacks
                    Enjin::Gameplay::GameplayLoop::Wire2DCollisionCallbacks(
                        m_Physics2D.get(), m_World.get(), &m_VisualScriptSystem, m_DeferredDestroys);
                }
                if (!m_StartScene.empty()) LoadSceneFromPack(m_StartScene);
            } else if (action == "quit_to_menu") {
                m_GameMenu.ShowScreen(Enjin::GUI::MenuScreen::MainMenu);
                m_GameStarted = false;
                Enjin::Input::SetMouseCaptured(false);
            } else if (action == "quit") {
                if (GetWindow()) GetWindow()->Close();
            } else if (action == "game_over_restart") {
                m_GameMenu.HideAll();
                Enjin::Input::SetMouseCaptured(true);
                // Recreate physics for clean restart
                {
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
                }
                if (!m_StartScene.empty()) LoadSceneFromPack(m_StartScene);
            } else if (action == "game_over_menu") {
                m_GameMenu.ShowScreen(Enjin::GUI::MenuScreen::MainMenu);
                m_GameStarted = false;
                Enjin::Input::SetMouseCaptured(false);
            }
        });

        // Apply graphics/audio settings when user exits Options menu.
        // Some changes (VSync, fullscreen) must be deferred — they recreate the
        // swapchain/window, which is unsafe mid-frame. Store pending changes and
        // apply them at the start of the next Update().
        m_GameMenu.SetSettingsCallback([this](const Enjin::GUI::GraphicsSettings& gfx,
                                              const Enjin::GUI::AudioSettings& audio) {
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

            ENJIN_LOG_INFO(Player, "Settings applied: vsync=%d fullscreen=%d shadows=%d bloom=%d fxaa=%d",
                (int)gfx.vsync, (int)gfx.fullscreen, (int)gfx.shadows, (int)gfx.bloom, (int)gfx.fxaa);
        });

        // Initialize audio system
        Enjin::Audio::AudioManager::Get().Initialize();

        // Initialize scripting engine
        if (m_ScriptEngine.Initialize()) {
            Enjin::Scripting::RegisterAllBindings(m_ScriptEngine.GetASEngine());
            m_ScriptEngine.SetWorld(m_World.get());
            m_ScriptEngine.SetScriptDirectory("scripts");

            m_ScriptSystem.SetWorld(m_World.get());
            m_ScriptSystem.SetScriptEngine(&m_ScriptEngine);
            m_ScriptSystem.SetCoroutineScheduler(&m_CoroutineScheduler);

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

        // Initialize save system with local backend
        m_TieredSaveSystem.LoadMeta();

        // Initialize systems needed for script bindings
        m_SimpleAudio.Initialize();
        m_SimpleAudio.SetWorld(m_World.get());
        m_WeatherSystem.Initialize();
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
            extern Enjin::Gameplay::HUDSystem* s_VisualScriptHUD;
            extern Enjin::Accessibility::SubtitleSystem* s_VisualScriptSubtitleSystem;
            extern Enjin::Accessibility::AccessibilityAnnouncer* s_VisualScriptAnnouncer;
            extern Enjin::Audio::SimpleAudio* s_VisualScriptAudio;
            extern Enjin::Renderer::PostProcessing* s_VisualScriptPostProcessing;
            extern Enjin::Editor::AudioEventGraphRuntime* s_VisualScriptAudioGraphRuntime;
            extern Enjin::Gameplay::ObjectPool* s_VisualScriptObjectPool;
            s_VisualScriptSaveSystem = nullptr;
            s_VisualScriptWeather = nullptr;
            s_VisualScriptWater = nullptr;
            s_VisualScriptHUD = nullptr;
            s_VisualScriptSubtitleSystem = nullptr;
            s_VisualScriptAnnouncer = nullptr;
            s_VisualScriptAudio = nullptr;
            s_VisualScriptPostProcessing = nullptr;
            s_VisualScriptAudioGraphRuntime = nullptr;
            s_VisualScriptObjectPool = nullptr;
        }

        // Shutdown gameplay systems before world is destroyed
        m_DialogueSystem.Clear();
        m_VisualScriptSystem.Shutdown();
        m_BehaviorTreeSystem.Shutdown();
        m_AISystem.SetEnabled(false);
        m_ControllerSystem.SetEnabled(false);
        m_FlowerSystem.SetEnabled(false);
        m_HUDSystem.SetEnabled(false);
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

        Enjin::Audio::AudioManager::Get().Shutdown();

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

        // Fullscreen toggle is a creation-time window parameter — not supported at runtime.
        if (m_FullscreenChangeRequested) {
            m_FullscreenChangeRequested = false;
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
        Enjin::Audio::AudioManager::Get().Update();
        m_SimpleAudio.Update(deltaTime);
        m_SimpleAudio.UpdateAudioSources(deltaTime);
        m_AudioGraphRuntime.Update(deltaTime);
        m_MIDIInput.Update();

        // Update input
        m_InputMap.Update(deltaTime);

        // ESC: state-aware menu navigation
        if (Enjin::Input::IsKeyPressed(Enjin::KeyCode::Escape)) {
            auto screen = m_GameMenu.GetCurrentScreen();
            if (screen == Enjin::GUI::MenuScreen::MainMenu) {
                // On title screen — ESC does nothing
            } else if (screen == Enjin::GUI::MenuScreen::Options ||
                       screen == Enjin::GUI::MenuScreen::HowToPlay) {
                // In sub-menu — go back to parent menu
                if (m_GameStarted) {
                    m_GameMenu.ShowScreen(Enjin::GUI::MenuScreen::PauseMenu);
                } else {
                    m_GameMenu.ShowScreen(Enjin::GUI::MenuScreen::MainMenu);
                }
            } else if (m_GameMenu.IsMenuOpen()) {
                // Pause menu — resume gameplay
                m_GameMenu.HideAll();
                Enjin::Input::SetMouseCaptured(true);
            } else {
                // In gameplay — pause
                m_GameMenu.ShowScreen(Enjin::GUI::MenuScreen::PauseMenu);
                Enjin::Input::SetMouseCaptured(false);
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
        if (m_GameMenu.IsMenuOpen() || !m_GameStarted) return;
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

        // Post-processing time update + volume evaluation
        if (m_PostProcessing) {
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
        Enjin::Gameplay::GameplayLoop::CheckEnemyOverlaps2D(m_World.get(), deltaTime, m_DeferredDestroys);
        Enjin::Gameplay::GameplayLoop::CheckPickupOverlaps3D(m_World.get(), m_DeferredDestroys);
        Enjin::Gameplay::GameplayLoop::CheckPickupOverlaps2D(m_World.get(), m_DeferredDestroys);

        // Health system (regen, invulnerability, death) and deferred entity destruction
        Enjin::Gameplay::GameplayLoop::UpdateHealthSystems(m_World.get(), deltaTime, m_DeferredDestroys);

        // Game over state (player death / victory detection)
        if (Enjin::Gameplay::GameplayLoop::UpdateGameOverState(m_World.get(), deltaTime)) {
            if (!m_GameMenu.IsGameOverScreen()) {
                for (auto entity : m_World->GetEntitiesWithComponent<Enjin::ECS::GameOverComponent>()) {
                    auto* go = m_World->GetComponent<Enjin::ECS::GameOverComponent>(entity);
                    if (go && go->triggered && go->screenVisible) {
                        const std::string& msg = go->won ? go->victoryMessage : go->defeatMessage;
                        m_GameMenu.ShowGameOver(go->won, msg, go->allowRestart, go->returnToMenu);
                        Enjin::Input::SetMouseCaptured(false);
                        break;
                    }
                }
            }
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

    void Render() override {
        if (!m_Initialized || !m_Renderer) return;

        if (!m_Renderer->BeginFrame()) {
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
                    // Sync camera position/rotation from the entity's TransformComponent
                    auto* camTransform = m_World->GetComponent<Enjin::ECS::TransformComponent>(activeCam);
                    if (camTransform) {
                        m_Camera->SetPosition(camTransform->position);
                        Enjin::Math::Vector3 forward = camTransform->rotation.GetForward();
                        Enjin::Math::Vector3 up = camTransform->rotation.GetUp();
                        m_Camera->SetLookAt(camTransform->position,
                                            camTransform->position + forward, up);
                    }
                }
            }
            m_Camera->SetPerspective(fov, aspect, nearP, farP);
        }

        // Detect splitscreen: multiple active cameras with non-default viewports
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
        }

        // World::Update triggers all registered systems including RenderSystem.
        // RenderSystem::Update starts the main render pass and draws entities.
        if (m_World) {
            m_World->Update(0.0f);
        }

        // Shadow pass runs inside RenderSystem::Update() (called by World::Update above).
        // Do NOT call RenderShadowPassForCamera here — the main render pass is already
        // active at this point, and starting a shadow render pass inside it crashes the
        // NVIDIA driver (nested vkCmdBeginRenderPass).

        // Render ImGui overlays (pause menu, dialogue)
        VkCommandBuffer cmd = m_Renderer->GetCurrentCommandBuffer();
        if (m_ImGuiLayer && cmd != VK_NULL_HANDLE) {
            m_ImGuiLayer->BeginFrame();

            // Pause menu
            if (m_GameMenu.IsMenuOpen()) {
                m_GameMenu.Render(static_cast<Enjin::f32>(extent.width),
                                  static_cast<Enjin::f32>(extent.height));
            }

            // Runtime dialogue overlay
            if (!m_ShowingSplash) {
                DrawDialogueOverlay();
            }

            // HUD widgets (health bars, etc.)
            if (!m_ShowingSplash && m_World) {
                m_HUDSystem.Update(m_World.get(), m_Camera.get(),
                    0.0f, 0.0f,
                    static_cast<Enjin::f32>(extent.width),
                    static_cast<Enjin::f32>(extent.height));
            }

            // Runtime UI canvases
            if (!m_ShowingSplash && m_World) {
                m_UISystem.Update(m_World.get(),
                    static_cast<Enjin::f32>(extent.width),
                    static_cast<Enjin::f32>(extent.height), 0.0f);
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

            // Tilde console (Quake-style, slides up from bottom)
            DrawConsole(cmd);

            m_ImGuiLayer->EndFrame(cmd);
        }

        m_Renderer->EndFrame();
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
        Enjin::Scripting::SetBindingsStreaming(&m_StreamingManager);
        Enjin::Scripting::SetBindingsSceneManager(&m_SceneManager);
        // Initialize post-processing (settings object for script bindings)
        auto ppExtent = m_Renderer->GetSwapchainExtent();
        m_PostProcessing = std::make_unique<Enjin::Renderer::PostProcessing>();
        if (m_Renderer && m_Renderer->GetContext()) {
            if (!m_PostProcessing->Initialize(m_Renderer->GetContext(),
                    m_Renderer->GetRenderPass(),
                    ppExtent.width, ppExtent.height, m_Renderer.get())) {
                ENJIN_LOG_WARN(Player, "PostProcessing init failed");
                m_PostProcessing.reset();
            }
        }
        Enjin::Scripting::SetBindingsPostProcessing(m_PostProcessing.get());
        Enjin::Scripting::SetBindingsPhysics2D(m_Physics2D.get());
        Enjin::Scripting::SetBindingsNetworking(&m_NetworkSystem);
        Enjin::Scripting::SetBindingsSubtitles(&m_SubtitleSystem);
        Enjin::Scripting::SetBindingsAnnouncer(&m_Announcer);
        Enjin::Scripting::SetBindingsAccessibilitySettings(&m_AccessibilitySettings);
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
            extern Enjin::Gameplay::HUDSystem* s_VisualScriptHUD;
            extern Enjin::Accessibility::SubtitleSystem* s_VisualScriptSubtitleSystem;
            extern Enjin::Accessibility::AccessibilityAnnouncer* s_VisualScriptAnnouncer;
            extern Enjin::Audio::SimpleAudio* s_VisualScriptAudio;
            extern Enjin::Renderer::PostProcessing* s_VisualScriptPostProcessing;
            extern Enjin::Editor::AudioEventGraphRuntime* s_VisualScriptAudioGraphRuntime;
            extern Enjin::Gameplay::ObjectPool* s_VisualScriptObjectPool;
            s_VisualScriptSaveSystem = &m_TieredSaveSystem;
            s_VisualScriptWeather = &m_WeatherSystem;
            s_VisualScriptWater = &m_Water3D;
            s_VisualScriptHUD = &m_HUDSystem;
            s_VisualScriptSubtitleSystem = &m_SubtitleSystem;
            s_VisualScriptAnnouncer = &m_Announcer;
            s_VisualScriptAudio = &m_SimpleAudio;
            s_VisualScriptPostProcessing = m_PostProcessing.get();
            s_VisualScriptAudioGraphRuntime = &m_AudioGraphRuntime;
            s_VisualScriptObjectPool = &m_ObjectPool;
        }

        // Wire dialogue system event bus and subtitle system
        m_DialogueSystem.SetEventBus(&m_EntityEventBus);
        m_DialogueSystem.SetSubtitleSystem(&m_SubtitleSystem);

        // Load accessibility settings from accessibility.json next to executable
        LoadAccessibilitySettings();

        // Configure subtitle system from accessibility settings
        auto& subConfig = m_SubtitleSystem.GetConfig();
        subConfig.enabled = m_AccessibilitySettings.subtitlesEnabled;
        subConfig.captionsEnabled = m_AccessibilitySettings.closedCaptionsEnabled;
        subConfig.fontSize = m_AccessibilitySettings.subtitleFontSize;
        subConfig.backgroundOpacity = m_AccessibilitySettings.subtitleBgOpacity;
        subConfig.showSpeakerNames = m_AccessibilitySettings.subtitleSpeakerNames;
        subConfig.showDirectionIndicators = m_AccessibilitySettings.subtitleDirectionIndicators;

        // Wire announcer to UISystem for screen reader support (Task #36)
        m_UISystem.SetAnnouncerCallback([this](const std::string& text) {
            m_Announcer.Announce(text, Enjin::Accessibility::AnnouncePriority::Normal);
        });

        // Apply reduced motion setting to controller system and UI
        m_ControllerSystem.SetReducedMotion(m_AccessibilitySettings.reducedMotion);
        m_ControllerSystem.SetDisableScreenShake(m_AccessibilitySettings.disableScreenShake);
        m_ControllerSystem.SetDisableFOVEffects(m_AccessibilitySettings.disableFOVEffects);
        m_ControllerSystem.SetInvertMouseY(m_AccessibilitySettings.invertMouseY);
        m_UISystem.SetReducedMotion(m_AccessibilitySettings.reducedMotion);

        // Apply font scale to UISystem
        m_UISystem.SetFontScale(m_AccessibilitySettings.fontScale);

        // Apply motor accessibility to UISystem (Task #34, #40)
        m_UISystem.SetSwitchAccessEnabled(m_AccessibilitySettings.switchAccessEnabled,
                                           m_AccessibilitySettings.switchScanSpeed);
        m_UISystem.SetDwellClickEnabled(m_AccessibilitySettings.dwellClickEnabled,
                                         m_AccessibilitySettings.dwellClickTime);
        m_UISystem.SetStickyDragEnabled(m_AccessibilitySettings.stickyDragEnabled);

        // Configure audio visual indicators (Task #38)
        m_AudioIndicators.GetConfig().enabled = m_AccessibilitySettings.audioIndicatorsEnabled;

        // Wire SimpleAudio callback for audio visual indicators
        if (m_AudioIndicators.GetConfig().enabled) {
            m_SimpleAudio.SetOnSoundPlayed([this](const std::string& soundName) {
                m_AudioIndicators.ShowIndicator(soundName,
                    Enjin::Math::Vector3(0.4f, 0.8f, 1.0f), 1.5f);
            });
        }

        // Apply font library settings (Task #35)
        if (m_AccessibilitySettings.dyslexiaFriendly) {
            m_FontLibrary.SetFont(Enjin::Accessibility::FontFamily::OpenDyslexic);
        } else if (m_AccessibilitySettings.fontFamily != Enjin::Accessibility::FontFamily::Default) {
            m_FontLibrary.SetFont(m_AccessibilitySettings.fontFamily);
        }
        {
            Enjin::Accessibility::FontLibraryConfig flConfig;
            flConfig.selectedFamily = m_AccessibilitySettings.fontFamily;
            flConfig.letterSpacing = m_AccessibilitySettings.letterSpacing;
            flConfig.wordSpacing = m_AccessibilitySettings.wordSpacing;
            flConfig.lineSpacing = m_AccessibilitySettings.lineSpacing;
            m_FontLibrary.SetConfig(flConfig);
        }

        // Apply colorblind mode and visual settings to post-processing
        if (m_PostProcessing) {
            m_AccessibilitySettings.ApplyToPostProcessing(m_PostProcessing->GetSettings());
        }

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
        m_HUDSystem.SetEnabled(true);
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
        m_VisualScriptSystem.Initialize();
        m_BehaviorTreeSystem.Initialize();

        // Start auto-play tweens
        m_TweenSystem.PlayAll(m_World.get());

        // Start AngelScript lifecycle
        if (m_ScriptEngine.GetASEngine()) {
            m_ScriptSystem.SetEnabled(true);
            m_ScriptSystem.InitializeAllScripts();
        }

        // Show title screen — gameplay starts when player clicks New Game / Continue
        m_GameMenu.ShowScreen(Enjin::GUI::MenuScreen::MainMenu);

        ENJIN_LOG_INFO(Player, "Splash screen ended, game loaded");
    }

    void ParseManifestJson(const nlohmann::json& manifest) {
        m_WindowTitle = manifest.value("windowTitle", "Enjin Game");
        m_WindowWidth = manifest.value("windowWidth", 1280u);
        m_WindowHeight = manifest.value("windowHeight", 720u);
        m_Fullscreen = manifest.value("fullscreen", false);
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
        if (m_RenderSystem) {
            auto renderSettings = serializer.GetRenderSettings();
            // Force RT off — Player doesn't embed RT compute shaders (crashes NVIDIA driver)
            renderSettings.rtEnabled = false;
            renderSettings.ApplyToRuntime(m_RenderSystem,
                m_PostProcessing ? &m_PostProcessing->GetSettings() : nullptr);
        }

        // Read content warning flags from scene JSON if present (Task #39)
        try {
            auto sceneJson = nlohmann::json::parse(sceneStr);
            if (sceneJson.contains("contentWarnings")) {
                auto& cw = sceneJson["contentWarnings"];
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

    // Default pack key — matches the build pipeline default
    static constexpr const char* PACK_KEY = "enjin_default_pack_key_2025";

    // Loose files mode — when true, assets are loaded from disk instead of .enjpak
    bool m_LooseFilesMode = false;
    std::string m_LooseFilesDir;

    bool m_Initialized = false;
    bool m_GameStarted = false;
    bool m_FullscreenChangeRequested = false;
    bool m_PendingFullscreen = false;
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
    Enjin::ECS::EntityEventBus m_EntityEventBus;
    Enjin::ECS::DialogueSystem m_DialogueSystem;
    Enjin::ECS::Entity m_ActiveDialogueEntity = 0;
    Enjin::Gameplay::HUDSystem m_HUDSystem;
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

    Enjin::Effects::Water3D m_Water3D;

    // TODO(F11): RetroEffects — RetroEffects is a settings/config class (resolution, dither,
    //   CRT, VHS, fog, color mode). It has no Vulkan rendering integration in RenderSystem or
    //   PostProcessing yet. To wire: add RetroEffects member, apply settings to push constant
    //   retro flags and shader uniforms, integrate CRT/VHS as post-process passes.

    // Audio event graph runtime
    Enjin::Editor::AudioEventGraphRuntime m_AudioGraphRuntime;

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

    // Post-processing (settings only — full render pipeline deferred)
    std::unique_ptr<Enjin::Renderer::PostProcessing> m_PostProcessing;

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
