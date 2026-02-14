#include "Enjin/Core/Application.h"
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
#include "Enjin/GUI/GameMenus.h"
#include "Enjin/GUI/ImGuiLayer.h"
#include "Enjin/GUI/UISystem.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include <imgui.h>
#include <backends/imgui_impl_vulkan.h>
#include "Enjin/Effects/Weather.h"
#include "Enjin/Effects/Destructible.h"
#include "Enjin/Effects/Wind.h"
#include "Enjin/Effects/FluidSimulation.h"
#include "Enjin/Effects/WorldTime.h"
#include "Enjin/Effects/SeasonalWeather.h"
#include "Enjin/Audio/AudioSystem.h"
#include "Enjin/Audio/SimpleAudio.h"
#include "Enjin/Build/AssetReader.h"
#include "Enjin/Scripting/ScriptEngine.h"
#include "Enjin/Scripting/ScriptSystem.h"
#include "Enjin/Scripting/ScriptBindings.h"
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
#include "Enjin/Accessibility/Announcer.h"
#include "Enjin/Accessibility/AccessibilitySettings.h"
#include "Enjin/Renderer/PostProcessing.h"
#include "Enjin/Editor/AudioEventGraph.h"
#include "Enjin/Assets/DataAsset.h"
#include <nlohmann/json.hpp>
#include <iostream>
#include <memory>
#include <filesystem>

namespace fs = std::filesystem;

// Standalone game player — no editor, no ImGui
class GamePlayer : public Enjin::Application {
public:
    void Initialize() override {
        ENJIN_LOG_INFO(Player, "Enjin Player starting...");

        // Open .enjpak from the same directory as the executable
        std::string exeDir = Enjin::Platform::GetExecutableDirectory();
        std::string pakPath = (fs::path(exeDir) / "game.enjpak").string();

        if (!m_AssetReader.Open(pakPath, PACK_KEY)) {
            ENJIN_LOG_FATAL(Player, "Failed to open game.enjpak from: %s", pakPath.c_str());
            return;
        }

        // Read build manifest
        auto manifestData = m_AssetReader.ReadFile("_build/manifest.json");
        if (manifestData.empty()) {
            ENJIN_LOG_FATAL(Player, "Build manifest missing from pack");
            return;
        }

        try {
            std::string manifestStr(manifestData.begin(), manifestData.end());
            auto manifest = nlohmann::json::parse(manifestStr);

            m_WindowTitle = manifest.value("windowTitle", "Enjin Game");
            m_WindowWidth = manifest.value("windowWidth", 1280u);
            m_WindowHeight = manifest.value("windowHeight", 720u);
            m_Fullscreen = manifest.value("fullscreen", false);
            m_StartScene = manifest.value("startScene", "");

            // Read frame rate settings
            if (manifest.contains("frameSettings")) {
                auto& fs = manifest["frameSettings"];
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
        } catch (const std::exception& e) {
            ENJIN_LOG_ERROR(Player, "Error parsing build manifest: %s", e.what());
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

        // Initialize audio system
        Enjin::Audio::AudioManager::Get().Initialize();

        // Initialize scripting engine
        if (m_ScriptEngine.Init()) {
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
        m_SceneManager.SetAssetReader(&m_AssetReader);

        // Populate scene list from packed project manifest
        {
            auto projData = m_AssetReader.ReadFile("project.enjinproject");
            if (!projData.empty()) {
                try {
                    std::string projStr(projData.begin(), projData.end());
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
                ENJIN_LOG_WARN(Player, "No project.enjinproject found in pack — SceneManager scene list empty");
            }
        }

        // Load data assets (.enjschema and .enjdata) from pack
        {
            auto& registry = Enjin::Assets::DataAssetRegistry::Get();
            Enjin::u32 schemaCount = 0, assetCount = 0;
            for (const auto& file : m_AssetReader.ListFiles()) {
                if (file.size() > 11 && file.substr(file.size() - 11) == ".enjschema") {
                    auto data = m_AssetReader.ReadFile(file);
                    if (!data.empty()) {
                        try {
                            std::string str(data.begin(), data.end());
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
            for (const auto& file : m_AssetReader.ListFiles()) {
                if (file.size() > 9 && file.substr(file.size() - 9) == ".enjdata") {
                    auto data = m_AssetReader.ReadFile(file);
                    if (!data.empty()) {
                        try {
                            std::string str(data.begin(), data.end());
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
                ENJIN_LOG_INFO(Player, "Loaded %u schemas and %u data assets from pack", schemaCount, assetCount);
            }
        }

        m_NetworkSystem.SetWorld(m_World.get());

        ENJIN_LOG_INFO(Player, "Gameplay systems initialized");

        // Show splash screen before loading game
        SetupSplashScreen();

        m_Initialized = true;
        ENJIN_LOG_INFO(Player, "Player initialized");
    }

    void Shutdown() override {
        ENJIN_LOG_INFO(Player, "Player shutting down...");

        // Destroy pooled objects before shutting down scripts
        m_ObjectPool.DestroyAll(m_World.get());

        // Clear visual script system externs
        {
            extern Enjin::Gameplay::TieredSaveSystem* s_VisualScriptSaveSystem;
            extern Enjin::Gameplay::HUDSystem* s_VisualScriptHUD;
            s_VisualScriptSaveSystem = nullptr;
            s_VisualScriptHUD = nullptr;
        }

        // Shutdown gameplay systems before world is destroyed
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
        m_Announcer.Clear();

        // Clear script bindings
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

        // Splash screen phase
        if (m_ShowingSplash) {
            m_SplashTimer -= deltaTime;

            // Fade in during first second
            if (m_SplashTimer > m_SplashDuration - 1.0f) {
                m_SplashAlpha = 1.0f - (m_SplashTimer - (m_SplashDuration - 1.0f));
            }
            // Fade out during last 0.5 seconds
            else if (m_SplashTimer < 0.5f) {
                m_SplashAlpha = m_SplashTimer / 0.5f;
            } else {
                m_SplashAlpha = 1.0f;
            }

            // Update splash text opacity
            UpdateSplashAlpha();

            // Skip on any key/click
            if (m_SplashTimer < m_SplashDuration - 0.3f) {
                if (Enjin::Input::IsKeyPressed(Enjin::KeyCode::Escape) ||
                    Enjin::Input::IsKeyPressed(Enjin::KeyCode::Space) ||
                    Enjin::Input::IsKeyPressed(Enjin::KeyCode::Enter) ||
                    Enjin::Input::IsMouseButtonPressed(Enjin::MouseButton::Left)) {
                    m_SplashTimer = 0.0f;
                }
            }

            if (m_SplashTimer <= 0.0f) {
                EndSplashScreen();
            }
            return;
        }

        // Update audio
        Enjin::Audio::AudioManager::Get().Update();
        m_SimpleAudio.Update(deltaTime);
        m_SimpleAudio.UpdateAudioSources(deltaTime);
        m_AudioGraphRuntime.Update(deltaTime);

        // Update input
        m_InputMap.Update(deltaTime);

        // ESC to toggle pause menu
        if (Enjin::Input::IsKeyPressed(Enjin::KeyCode::Escape)) {
            if (m_GameMenu.IsMenuOpen()) {
                m_GameMenu.HideAll();
            } else {
                m_GameMenu.ShowScreen(Enjin::GUI::MenuScreen::PauseMenu);
            }
        }

        // Skip gameplay updates when paused
        if (m_GameMenu.IsMenuOpen()) return;

        // --- Physics (must run first) ---
        if (m_Physics) m_Physics->Update(deltaTime);
        if (m_Physics2D) m_Physics2D->Update(deltaTime);

        // Dispatch 3D collision events to visual scripts and gameplay systems
        if (m_Physics) {
            const auto& collisionEvents = m_Physics->GetPendingCollisionEvents();
            for (const auto& evt : collisionEvents) {
                if (evt.isTrigger) {
                    if (evt.type == Enjin::Physics::CollisionEvent::Type::Enter) {
                        m_VisualScriptSystem.OnTriggerEnter(evt.entityA, evt.entityB, deltaTime);
                        m_VisualScriptSystem.OnTriggerEnter(evt.entityB, evt.entityA, deltaTime);
                        ProcessContactDamage(evt.entityA, evt.entityB);
                        ProcessPickup(evt.entityA, evt.entityB);
                    } else {
                        m_VisualScriptSystem.OnTriggerExit(evt.entityA, evt.entityB, deltaTime);
                        m_VisualScriptSystem.OnTriggerExit(evt.entityB, evt.entityA, deltaTime);
                    }
                } else {
                    if (evt.type == Enjin::Physics::CollisionEvent::Type::Enter) {
                        m_VisualScriptSystem.OnCollisionEnter(evt.entityA, evt.entityB, deltaTime);
                        m_VisualScriptSystem.OnCollisionEnter(evt.entityB, evt.entityA, deltaTime);
                        ProcessContactDamage(evt.entityA, evt.entityB);
                        ProcessPickup(evt.entityA, evt.entityB);
                    } else {
                        m_VisualScriptSystem.OnCollisionExit(evt.entityA, evt.entityB, deltaTime);
                        m_VisualScriptSystem.OnCollisionExit(evt.entityB, evt.entityA, deltaTime);
                    }
                }
            }
            m_Physics->ClearPendingCollisionEvents();
        }

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
        if (m_CameraController) {
            m_CameraController->Update(deltaTime);
        }

        // --- AngelScript ---
        m_ScriptSystem.Update(deltaTime);
        m_CoroutineScheduler.EndOfFrame();

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
        m_FluidSimulation.Update(deltaTime, m_World.get());

        // Particle emitter simulation
        m_ParticleSystem.Update(deltaTime, m_World.get());

        // Post-processing time update
        if (m_PostProcessing) m_PostProcessing->Update(deltaTime);

        // Accessibility systems
        m_SubtitleSystem.Update(deltaTime);
        m_AlternativeInput.Update(deltaTime);
        m_Announcer.Update(deltaTime);

        // Networking
        m_NetworkSystem.Update(deltaTime);

        // Save system (auto-save timer)
        m_TieredSaveSystem.Update(deltaTime, m_World.get(), m_StartScene);

        // Health system (regen, invulnerability, death) and deferred entity destruction
        UpdateHealthSystems(deltaTime);
        FlushDeferredDestroys();

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

        // Update camera aspect ratio
        auto extent = m_Renderer->GetSwapchainExtent();
        if (extent.width > 0 && extent.height > 0 && m_Camera) {
            Enjin::f32 aspect = static_cast<Enjin::f32>(extent.width) / static_cast<Enjin::f32>(extent.height);
            m_Camera->SetPerspective(45.0f, aspect, 0.1f, 1000.0f);
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

        if (m_World) {
            m_World->Update(0.0f);
        }

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
                m_SubtitleSystem.RenderOverlay(extent.width, extent.height);
                m_AlternativeInput.RenderOverlay();
                m_Announcer.RenderStatusBar();
            }

            m_ImGuiLayer->EndFrame(cmd);
        }

        m_Renderer->EndFrame();
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
                ENJIN_LOG_WARN(Player, "PostProcessing init failed — script bindings will have null PP");
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

        // Wire visual script system externs (for VS nodes)
        {
            extern Enjin::Gameplay::TieredSaveSystem* s_VisualScriptSaveSystem;
            extern Enjin::Effects::WeatherSystem* s_VisualScriptWeather;
            extern Enjin::Gameplay::HUDSystem* s_VisualScriptHUD;
            s_VisualScriptSaveSystem = &m_TieredSaveSystem;
            s_VisualScriptWeather = &m_WeatherSystem;
            s_VisualScriptHUD = &m_HUDSystem;
        }

        // Wire dialogue system event bus and subtitle system
        m_DialogueSystem.SetEventBus(&m_EntityEventBus);
        m_DialogueSystem.SetSubtitleSystem(&m_SubtitleSystem);

        // Configure subtitle system with defaults
        auto& subConfig = m_SubtitleSystem.GetConfig();
        subConfig.enabled = m_AccessibilitySettings.subtitlesEnabled;
        subConfig.captionsEnabled = m_AccessibilitySettings.closedCaptionsEnabled;
        subConfig.fontSize = m_AccessibilitySettings.subtitleFontSize;
        subConfig.backgroundOpacity = m_AccessibilitySettings.subtitleBgOpacity;
        subConfig.showSpeakerNames = m_AccessibilitySettings.subtitleSpeakerNames;
        subConfig.showDirectionIndicators = m_AccessibilitySettings.subtitleDirectionIndicators;

        // Wire announcer to UISystem for screen reader support
        m_UISystem.SetAnnouncerCallback([this](const std::string& text) {
            m_Announcer.Announce(text);
        });

        // Apply reduced motion setting to controller system
        m_ControllerSystem.SetReducedMotion(m_AccessibilitySettings.reducedMotion);

        // Apply font scale to UISystem
        m_UISystem.SetFontScale(m_AccessibilitySettings.fontScale);

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
        }

        // Wire 2D physics collision callbacks to visual script system
        if (m_Physics2D) {
            m_Physics2D->SetOnCollisionEnter([this](const Enjin::Physics::Contact2D& c) {
                m_VisualScriptSystem.OnCollisionEnter(c.entityA, c.entityB, 0.0f);
                m_VisualScriptSystem.OnCollisionEnter(c.entityB, c.entityA, 0.0f);
                ProcessContactDamage(c.entityA, c.entityB);
                ProcessPickup(c.entityA, c.entityB);
            });
            m_Physics2D->SetOnCollisionExit([this](const Enjin::Physics::Contact2D& c) {
                m_VisualScriptSystem.OnCollisionExit(c.entityA, c.entityB, 0.0f);
                m_VisualScriptSystem.OnCollisionExit(c.entityB, c.entityA, 0.0f);
            });
            m_Physics2D->SetOnSensorEnter([this](const Enjin::Physics::Contact2D& c) {
                m_VisualScriptSystem.OnTriggerEnter(c.entityA, c.entityB, 0.0f);
                m_VisualScriptSystem.OnTriggerEnter(c.entityB, c.entityA, 0.0f);
                ProcessContactDamage(c.entityA, c.entityB);
                ProcessPickup(c.entityA, c.entityB);
            });
            m_Physics2D->SetOnSensorExit([this](const Enjin::Physics::Contact2D& c) {
                m_VisualScriptSystem.OnTriggerExit(c.entityA, c.entityB, 0.0f);
                m_VisualScriptSystem.OnTriggerExit(c.entityB, c.entityA, 0.0f);
            });
        }

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

        ENJIN_LOG_INFO(Player, "Splash screen ended, game loaded");
    }

    bool LoadSceneFromPack(const std::string& scenePath) {
        auto sceneData = m_AssetReader.ReadFile(scenePath);
        if (sceneData.empty()) {
            ENJIN_LOG_ERROR(Player, "Failed to read scene from pack: %s", scenePath.c_str());
            return false;
        }

        std::string sceneStr(sceneData.begin(), sceneData.end());
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
            renderSettings.ApplyToRuntime(m_RenderSystem,
                m_PostProcessing ? &m_PostProcessing->GetSettings() : nullptr);
        }

        ENJIN_LOG_INFO(Player, "Loaded scene: %s (%zu entities)", scenePath.c_str(), result.entities.size());
        return true;
    }

    // Default pack key — matches the build pipeline default
    static constexpr const char* PACK_KEY = "enjin_default_pack_key_2025";

    bool m_Initialized = false;
    std::vector<Enjin::ECS::Entity> m_DeferredDestroys;

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
    Enjin::Scene::StreamingManager m_StreamingManager;
    Enjin::Scene::SceneManager m_SceneManager;
    Enjin::Networking::NetworkSystem m_NetworkSystem;

    // Particle system (CPU simulation for ParticleEmitterComponent)
    Enjin::Effects::ParticleSystem m_ParticleSystem;

    // Fluid simulation, wind, world time, seasonal weather
    Enjin::Effects::FluidSimulation m_FluidSimulation;
    Enjin::Effects::WindSystem m_WindSystem;
    Enjin::Effects::WorldTimeSystem m_WorldTime;
    Enjin::Effects::SeasonalWeatherSystem m_SeasonalWeather;

    // TODO(F11): Water3D — Water3D::Initialize() generates CPU mesh data and wave offsets,
    //   but there is no Vulkan rendering integration yet (no water render pass, no water shader
    //   pipeline in RenderSystem). To wire: add Water3D member, init from scene settings,
    //   call Update(deltaTime) per frame, create a dedicated water mesh entity or custom render
    //   pass in RenderSystem that uses Water3D::GenerateMesh() and wave uniforms.
    // TODO(F11): RetroEffects — RetroEffects is a settings/config class (resolution, dither,
    //   CRT, VHS, fog, color mode). It has no Vulkan rendering integration in RenderSystem or
    //   PostProcessing yet. To wire: add RetroEffects member, apply settings to push constant
    //   retro flags and shader uniforms, integrate CRT/VHS as post-process passes.

    // Audio event graph runtime
    Enjin::Editor::AudioEventGraphRuntime m_AudioGraphRuntime;

    // Accessibility systems
    Enjin::Accessibility::SubtitleSystem m_SubtitleSystem;
    Enjin::Accessibility::AlternativeInputManager m_AlternativeInput;
    Enjin::Accessibility::AccessibilityAnnouncer m_Announcer;
    Enjin::Accessibility::RuntimeAccessibilitySettings m_AccessibilitySettings;

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

    // ========================================================================
    // Gameplay Processing — Contact Damage, Pickups, Health
    // ========================================================================

    void ProcessContactDamage(Enjin::ECS::Entity entityA, Enjin::ECS::Entity entityB) {
        if (!m_World) return;
        auto tryDamage = [this](Enjin::ECS::Entity damager, Enjin::ECS::Entity target) {
            auto* dmg = m_World->GetComponent<Enjin::ECS::DamageComponent>(damager);
            auto* hp = m_World->GetComponent<Enjin::ECS::HealthComponent>(target);
            if (!dmg || !hp || hp->isDead) return;
            if (dmg->damageOnce) {
                for (auto e : dmg->damagedEntities) { if (e == target) return; }
                dmg->damagedEntities.push_back(target);
            }
            if (hp->isInvulnerable || hp->invulnerabilityTimer > 0.0f) return;
            Enjin::f32 remaining = dmg->damage;
            if (hp->currentShield > 0.0f) {
                Enjin::f32 absorbed = std::min(remaining, hp->currentShield);
                hp->currentShield -= absorbed;
                remaining -= absorbed;
            }
            hp->currentHealth -= remaining;
            hp->timeSinceLastDamage = 0.0f;
            if (hp->invulnerabilityTime > 0.0f) hp->invulnerabilityTimer = hp->invulnerabilityTime;
            if (dmg->knockbackForce > 0.0f) {
                auto* ctrl = m_World->GetComponent<Enjin::ECS::Platformer2DController>(target);
                auto* dmgT = m_World->GetComponent<Enjin::ECS::TransformComponent>(damager);
                auto* tgtT = m_World->GetComponent<Enjin::ECS::TransformComponent>(target);
                if (ctrl && dmgT && tgtT) {
                    Enjin::f32 dir = (tgtT->position.x > dmgT->position.x) ? 1.0f : -1.0f;
                    ctrl->velocity.x = dir * dmg->knockbackForce;
                    ctrl->velocity.y = dmg->knockbackForce * 0.5f;
                    ctrl->isGrounded = false;
                }
            }
            if (hp->currentHealth <= 0.0f) { hp->currentHealth = 0.0f; hp->isDead = true; }
            if (dmg->destroyOnHit) m_DeferredDestroys.push_back(damager);
        };
        tryDamage(entityA, entityB);
        tryDamage(entityB, entityA);
    }

    void ProcessPickup(Enjin::ECS::Entity entityA, Enjin::ECS::Entity entityB) {
        if (!m_World) return;
        auto tryPickup = [this](Enjin::ECS::Entity pickupEntity, Enjin::ECS::Entity collector) {
            auto* pickup = m_World->GetComponent<Enjin::ECS::PickupComponent>(pickupEntity);
            if (!pickup || pickup->isCollected) return;
            bool isPlayer = m_World->GetComponent<Enjin::ECS::Platformer2DController>(collector) != nullptr ||
                            m_World->GetComponent<Enjin::ECS::TopDown2DController>(collector) != nullptr ||
                            m_World->GetComponent<Enjin::ECS::FirstPersonController>(collector) != nullptr ||
                            m_World->GetComponent<Enjin::ECS::ThirdPersonController>(collector) != nullptr;
            if (!isPlayer) return;
            if (pickup->type == Enjin::ECS::PickupComponent::PickupType::Health) {
                auto* hp = m_World->GetComponent<Enjin::ECS::HealthComponent>(collector);
                if (hp) hp->currentHealth = std::min(hp->currentHealth + pickup->value, hp->maxHealth);
            }
            pickup->isCollected = true;
            if (pickup->destroyOnPickup && !pickup->canRespawn) {
                m_DeferredDestroys.push_back(pickupEntity);
            } else if (pickup->destroyOnPickup) {
                auto* transform = m_World->GetComponent<Enjin::ECS::TransformComponent>(pickupEntity);
                if (transform) transform->visible = false;
                pickup->respawnTimer = pickup->respawnTime;
            }
        };
        tryPickup(entityA, entityB);
        tryPickup(entityB, entityA);
    }

    void UpdateHealthSystems(Enjin::f32 deltaTime) {
        if (!m_World) return;
        for (auto entity : m_World->GetEntitiesWithComponent<Enjin::ECS::HealthComponent>()) {
            auto* hp = m_World->GetComponent<Enjin::ECS::HealthComponent>(entity);
            if (!hp) continue;
            if (hp->invulnerabilityTimer > 0.0f) hp->invulnerabilityTimer -= deltaTime;
            hp->timeSinceLastDamage += deltaTime;
            if (!hp->isDead && hp->regenRate > 0.0f && hp->timeSinceLastDamage >= hp->regenDelay)
                hp->currentHealth = std::min(hp->currentHealth + hp->regenRate * deltaTime, hp->maxHealth);
            if (!hp->isDead && hp->shieldRegenRate > 0.0f && hp->timeSinceLastDamage >= hp->shieldRegenDelay)
                hp->currentShield = std::min(hp->currentShield + hp->shieldRegenRate * deltaTime, hp->maxShield);
            if (hp->isDead) {
                auto* ctrl = m_World->GetComponent<Enjin::ECS::Platformer2DController>(entity);
                if (ctrl) {
                    hp->isDead = false; hp->currentHealth = hp->maxHealth; hp->currentShield = hp->maxShield;
                    hp->invulnerabilityTimer = 1.0f;
                    auto* transform = m_World->GetComponent<Enjin::ECS::TransformComponent>(entity);
                    if (transform) transform->position = Enjin::Math::Vector3(0.0f, 2.0f, 0.0f);
                    ctrl->velocity = Enjin::Math::Vector3(0.0f);
                    ctrl->isGrounded = false;
                } else {
                    m_DeferredDestroys.push_back(entity);
                }
            }
        }
        for (auto entity : m_World->GetEntitiesWithComponent<Enjin::ECS::PickupComponent>()) {
            auto* pickup = m_World->GetComponent<Enjin::ECS::PickupComponent>(entity);
            if (!pickup || !pickup->isCollected || !pickup->canRespawn) continue;
            pickup->respawnTimer -= deltaTime;
            if (pickup->respawnTimer <= 0.0f) {
                pickup->isCollected = false;
                auto* transform = m_World->GetComponent<Enjin::ECS::TransformComponent>(entity);
                if (transform) transform->visible = true;
            }
        }
    }

    void FlushDeferredDestroys() {
        if (!m_World || m_DeferredDestroys.empty()) return;
        for (auto entity : m_DeferredDestroys) {
            if (m_World->GetComponent<Enjin::ECS::TransformComponent>(entity))
                m_World->DestroyEntity(entity);
        }
        m_DeferredDestroys.clear();
    }
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
