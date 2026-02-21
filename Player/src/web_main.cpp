// Enjin Engine — Web Player Entry Point (Emscripten / WebGPU)
// Stripped-down player for browser: no ImGui, no Vulkan, WebGPU renderer.

#include "Enjin/Platform/Platform.h"
#if ENJIN_PLATFORM_WEB

#include "Enjin/Core/Application.h"
#include "Enjin/Core/Version.h"
#include "Enjin/Logging/Log.h"
#include "Enjin/Platform/Input.h"
#include "Enjin/Platform/Window.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Systems/RenderSystem.h"
#include "Enjin/ECS/Components/Camera.h"
#include "Enjin/ECS/Components/Light.h"
#include "Enjin/ECS/Components/Name.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/Renderer/WebGPU/WebGPURenderer.h"
#include "Enjin/Renderer/Camera.h"
#include "Enjin/Renderer/CameraController.h"
#include "Enjin/Scene/SceneSerializer.h"
#include "Enjin/Scene/SceneManager.h"
#include "Enjin/Input/InputAction.h"
#include "Enjin/GUI/UISystem.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/Effects/Weather.h"
#include "Enjin/Effects/Wind.h"
#include "Enjin/Effects/WorldTime.h"
#include "Enjin/Audio/AudioSystem.h"
#include "Enjin/Audio/SimpleAudio.h"
#include "Enjin/Build/AssetReader.h"
#include "Enjin/Scripting/ScriptEngine.h"
#include "Enjin/Scripting/ScriptSystem.h"
#include "Enjin/Scripting/ScriptBindings.h"
#include "Enjin/Scripting/CoroutineScheduler.h"
#include "Enjin/Scripting/ScriptEvents.h"
#include "Enjin/ECS/Systems/ControllerSystem.h"
#include "Enjin/ECS/Systems/TweenSystem.h"
#include "Enjin/ECS/Systems/VisualScriptSystem.h"
#include "Enjin/ECS/Systems/BehaviorTreeSystem.h"
#include "Enjin/ECS/EntityEventBus.h"
#include "Enjin/Gameplay/HUDSystem.h"
#include "Enjin/Gameplay/QuestSystem.h"
#include "Enjin/Gameplay/ObjectPool.h"
#include "Enjin/Gameplay/TieredSaveSystem.h"
#include "Enjin/Physics/IPhysicsBackend.h"
#include "Enjin/Physics/IPhysicsBackend2D.h"
#include "Enjin/Physics/PhysicsBackendFactory.h"
#include "Enjin/Physics/PhysicsBackendType.h"
#include "Enjin/Networking/NetworkSystem.h"
#include "Enjin/Effects/ParticleSystem.h"
#include <nlohmann/json.hpp>
#include <emscripten.h>
#include <string>
#include <memory>

static constexpr const char* PACK_KEY = "enjin_default_pack_key";

// Web game player — no editor, no ImGui, WebGPU renderer
class WebGamePlayer : public Enjin::Application {
public:
    void Initialize() override {
        ENJIN_LOG_INFO(Player, "Enjin Web Player starting...");

        // In web builds, game.enjpak is preloaded into the virtual filesystem
        std::string pakPath = "game.enjpak";

        if (!m_AssetReader.Open(pakPath, PACK_KEY)) {
            ENJIN_LOG_FATAL(Player, "Failed to open game.enjpak");
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
            m_StartScene = manifest.value("startScene", "");
            m_PhysicsBackendType = manifest.value("physicsBackend", 0u);
            m_ProjectMode = manifest.value("projectMode", 1u);

            ENJIN_LOG_INFO(Player, "Game: %s (%ux%u)",
                m_WindowTitle.c_str(), m_WindowWidth, m_WindowHeight);
        } catch (const std::exception& e) {
            ENJIN_LOG_ERROR(Player, "Error parsing build manifest: %s", e.what());
            return;
        }

        // Initialize WebGPU renderer
        m_Renderer = std::make_unique<Enjin::Renderer::WebGPURenderer>();
        if (!m_Renderer->Initialize(GetWindow())) {
            ENJIN_LOG_FATAL(Player, "Failed to initialize WebGPU renderer");
            m_Renderer.reset();
            return;
        }

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

        // Initialize audio (miniaudio supports Web Audio natively)
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

        // Initialize physics
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

        m_TweenSystem.SetScriptEngine(&m_ScriptEngine);

        m_VisualScriptSystem.SetWorld(m_World.get());
        m_BehaviorTreeSystem.SetWorld(m_World.get());

        m_TieredSaveSystem.LoadMeta();
        m_SimpleAudio.Initialize();
        m_SimpleAudio.SetWorld(m_World.get());
        m_WeatherSystem.Initialize();
        m_SceneManager.SetWorld(m_World.get());
        m_SceneManager.SetAssetReader(&m_AssetReader);
        m_NetworkSystem.SetWorld(m_World.get());

        // Load scene list from packed project manifest
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
                } catch (const std::exception& e) {
                    ENJIN_LOG_ERROR(Player, "Failed to parse project manifest: %s", e.what());
                }
            }
        }

        // Wire up script bindings
        Enjin::Scripting::SetBindingsWorld(m_World.get());
        Enjin::Scripting::SetBindingsRenderSystem(m_RenderSystem);
        Enjin::Scripting::SetBindingsPhysics(m_Physics.get());
        Enjin::Scripting::SetBindingsSaveSystem(&m_TieredSaveSystem);
        Enjin::Scripting::SetBindingsCoroutineScheduler(&m_CoroutineScheduler);
        Enjin::Scripting::SetBindingsEventBus(&m_ScriptEventBus);
        Enjin::Scripting::SetBindingsScriptEngine(&m_ScriptEngine);
        Enjin::Scripting::SetBindingsQuestSystem(&m_QuestSystem);
        Enjin::Scripting::SetBindingsObjectPool(&m_ObjectPool);
        Enjin::Scripting::SetBindingsAudio(&m_SimpleAudio);
        Enjin::Scripting::SetBindingsWeather(&m_WeatherSystem);
        Enjin::Scripting::SetBindingsSceneManager(&m_SceneManager);
        Enjin::Scripting::SetBindingsPhysics2D(m_Physics2D.get());
        Enjin::Scripting::SetBindingsNetworking(&m_NetworkSystem);
        Enjin::Scripting::SetBindingsInputActionMap(&m_InputMap);

        // Load the start scene
        if (!m_StartScene.empty()) {
            std::string scenePath = m_StartScene;
            auto sceneData = m_AssetReader.ReadFile(scenePath);
            if (!sceneData.empty()) {
                std::string sceneStr(sceneData.begin(), sceneData.end());
                Enjin::Scene::SceneSerializer serializer(m_World.get());
                serializer.DeserializeFromString(sceneStr);
                ENJIN_LOG_INFO(Player, "Loaded start scene: %s", m_StartScene.c_str());
            } else {
                ENJIN_LOG_ERROR(Player, "Failed to load start scene: %s", m_StartScene.c_str());
            }
        }

        m_Initialized = true;
        ENJIN_LOG_INFO(Player, "Web Player initialized");
    }

    void Shutdown() override {
        ENJIN_LOG_INFO(Player, "Web Player shutting down...");

        m_ObjectPool.DestroyAll(m_World.get());

        // Clear script bindings
        Enjin::Scripting::SetBindingsWorld(nullptr);
        Enjin::Scripting::SetBindingsRenderSystem(nullptr);
        Enjin::Scripting::SetBindingsPhysics(nullptr);
        Enjin::Scripting::SetBindingsSaveSystem(nullptr);
        Enjin::Scripting::SetBindingsCoroutineScheduler(nullptr);
        Enjin::Scripting::SetBindingsEventBus(nullptr);
        Enjin::Scripting::SetBindingsScriptEngine(nullptr);
        Enjin::Scripting::SetBindingsQuestSystem(nullptr);
        Enjin::Scripting::SetBindingsObjectPool(nullptr);
        Enjin::Scripting::SetBindingsAudio(nullptr);
        Enjin::Scripting::SetBindingsWeather(nullptr);
        Enjin::Scripting::SetBindingsSceneManager(nullptr);
        Enjin::Scripting::SetBindingsPhysics2D(nullptr);
        Enjin::Scripting::SetBindingsNetworking(nullptr);
        Enjin::Scripting::SetBindingsInputActionMap(nullptr);

        m_ScriptSystem.ShutdownAllScripts();
        m_ScriptSystem.SetEnabled(false);
        m_CoroutineScheduler.Clear();
        m_ScriptEventBus.Clear();
        m_ScriptEngine.Shutdown();

        m_TieredSaveSystem.SaveMeta();
        Enjin::Audio::AudioManager::Get().Shutdown();

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

        // Update audio
        Enjin::Audio::AudioManager::Get().Update();
        m_SimpleAudio.Update(deltaTime);
        m_SimpleAudio.UpdateAudioSources(deltaTime);

        // Update input
        m_InputMap.Update(deltaTime);

        // --- Physics ---
        if (m_Physics) m_Physics->Update(deltaTime);
        if (m_Physics2D) m_Physics2D->Update(deltaTime);

        // --- Controllers ---
        m_ControllerSystem.Update(deltaTime);

        // --- Camera ---
        if (m_CameraController && m_CameraController->IsEnabled()) {
            m_CameraController->Update(deltaTime);
        }

        // --- Scripts ---
        m_ScriptSystem.Update(deltaTime);
        m_CoroutineScheduler.EndOfFrame();
        Enjin::Scripting::FlushDeferredEntityDestroys();

        // --- Gameplay systems ---
        m_TweenSystem.Update(m_World.get(), deltaTime);
        m_VisualScriptSystem.Update(deltaTime);
        m_BehaviorTreeSystem.Update(deltaTime);

        m_QuestSystem.Update(m_World.get(), deltaTime);
        m_ObjectPool.Update(m_World.get(), deltaTime);
        m_EntityEventBus.ProcessDeferred();

        // --- Effects ---
        m_WindSystem.Update(deltaTime);
        m_WorldTime.Update(deltaTime);
        if (m_Camera) {
            m_WeatherSystem.Update(deltaTime, m_Camera->GetPosition());
        }

        m_ParticleSystem.Update(deltaTime, m_World.get());
    }

    void Render() override {
        if (!m_Initialized || !m_Renderer) return;

        if (m_RenderSystem && m_Camera) {
            m_RenderSystem->Render(m_Camera.get());
        }
    }

private:
    bool m_Initialized = false;

    // Core systems
    std::unique_ptr<Enjin::Renderer::WebGPURenderer> m_Renderer;
    std::unique_ptr<Enjin::Renderer::Camera> m_Camera;
    std::unique_ptr<Enjin::Renderer::CameraController> m_CameraController;
    std::unique_ptr<Enjin::ECS::World> m_World;
    Enjin::ECS::RenderSystem* m_RenderSystem = nullptr;

    // Assets
    Enjin::Build::AssetReader m_AssetReader;

    // Scripting
    Enjin::Scripting::ScriptEngine m_ScriptEngine;
    Enjin::Scripting::ScriptSystem m_ScriptSystem;
    Enjin::Scripting::CoroutineScheduler m_CoroutineScheduler;
    Enjin::Scripting::ScriptEventBus m_ScriptEventBus;

    // Physics
    std::unique_ptr<Enjin::Physics::IPhysicsBackend> m_Physics;
    std::unique_ptr<Enjin::Physics::IPhysicsBackend2D> m_Physics2D;

    // Gameplay
    Enjin::InputSystem::InputActionMap m_InputMap;
    Enjin::ECS::ControllerSystem m_ControllerSystem;
    Enjin::ECS::TweenSystem m_TweenSystem;
    Enjin::ECS::VisualScriptSystem m_VisualScriptSystem;
    Enjin::ECS::BehaviorTreeSystem m_BehaviorTreeSystem;
    Enjin::ECS::EntityEventBus m_EntityEventBus;
    Enjin::Gameplay::HUDSystem m_HUDSystem;
    Enjin::Gameplay::QuestSystem m_QuestSystem;
    Enjin::Gameplay::ObjectPool m_ObjectPool;
    Enjin::Gameplay::TieredSaveSystem m_TieredSaveSystem;

    // Audio & effects
    Enjin::Audio::SimpleAudio m_SimpleAudio;
    Enjin::Effects::WeatherSystem m_WeatherSystem;
    Enjin::Effects::WindSystem m_WindSystem;
    Enjin::Effects::WorldTime m_WorldTime;
    Enjin::Effects::ParticleSystem m_ParticleSystem;

    // Scene & networking
    Enjin::Scene::SceneManager m_SceneManager;
    Enjin::Networking::NetworkSystem m_NetworkSystem;

    // Build manifest values
    std::string m_WindowTitle;
    Enjin::u32 m_WindowWidth = 1280;
    Enjin::u32 m_WindowHeight = 720;
    std::string m_StartScene;
    Enjin::u32 m_PhysicsBackendType = 0;
    Enjin::u32 m_ProjectMode = 1;
};

int main(int argc, char* argv[]) {
    WebGamePlayer player;

    Enjin::WindowDesc desc;
    desc.title = "Enjin Game";
    desc.width = 1280;
    desc.height = 720;

    return player.Run(desc);
}

#endif // ENJIN_PLATFORM_WEB
