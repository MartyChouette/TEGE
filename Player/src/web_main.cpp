// Enjin Engine — Web Player Entry Point (Emscripten / WebGPU)
// Production web player: WebRenderPipeline for PBR rendering, proper frame
// timing, responsive canvas via ResizeObserver, all gameplay systems active.

#include "Enjin/Platform/Platform.h"
#if ENJIN_PLATFORM_WEB

#include "Enjin/Core/Application.h"
#include "Enjin/Core/Version.h"
#include "Enjin/Logging/Log.h"
#include "Enjin/Platform/Input.h"
#include "Enjin/Platform/Window.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Camera.h"
#include "Enjin/ECS/Components/Light.h"
#include "Enjin/ECS/Components/Mesh.h"
#include "Enjin/ECS/Components/Material.h"
#include "Enjin/ECS/Components/Name.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/Renderer/WebGPU/WebGPURenderer.h"
#include "Enjin/ECS/Systems/RenderSystem.h"
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
#include <emscripten/html5.h>
#include <string>
#include <memory>
#include <fstream>
#include <algorithm>

static constexpr const char* PACK_KEY = "enjin_default_pack_key";

// Forward declare for extern "C" callbacks
class WebGamePlayer;
static WebGamePlayer* g_Player = nullptr;

// ============================================================================
// WebGamePlayer — standalone web game player (no Application base, no GLFW)
// ============================================================================
class WebGamePlayer {
public:
    void Initialize() {
        ENJIN_LOG_INFO(Player, "Enjin Web Player starting...");

        // Fetch game.enjpak from the server into the WASM virtual filesystem
        bool hasPack = false;
        {
            void* buf = nullptr;
            int len = 0;
            int err = 0;
            emscripten_wget_data("game.enjpak", &buf, &len, &err);
            if (!err && buf && len > 0) {
                FILE* f = fopen("game.enjpak", "wb");
                if (f) { fwrite(buf, 1, len, f); fclose(f); }
                free(buf);
                hasPack = m_AssetReader.Open("game.enjpak", "");
                if (!hasPack) hasPack = m_AssetReader.Open("game.enjpak", PACK_KEY);
            } else {
                ENJIN_LOG_WARN(Player, "No game.enjpak available on server");
            }
        }

        // Fallback: try fetching a loose scene file
        if (!hasPack) {
            void* buf = nullptr;
            int len = 0;
            int err = 0;
            emscripten_wget_data("scene.enjin", &buf, &len, &err);
            if (!err && buf && len > 0) {
                FILE* f = fopen("scene.enjin", "wb");
                if (f) { fwrite(buf, 1, len, f); fclose(f); }
                free(buf);
            }
        }

        // Read build manifest
        std::vector<Enjin::u8> manifestData;
        if (hasPack) manifestData = m_AssetReader.ReadFile("_build/manifest.json");
        if (!manifestData.empty()) {
            try {
                std::string manifestStr(manifestData.begin(), manifestData.end());
                auto manifest = nlohmann::json::parse(manifestStr);
                m_WindowTitle = manifest.value("windowTitle", "Enjin Game");
                m_WindowWidth = manifest.value("windowWidth", 1280u);
                m_WindowHeight = manifest.value("windowHeight", 720u);
                m_StartScene = manifest.value("startScene", "");
                m_PhysicsBackendType = manifest.value("physicsBackend", 0u);
                m_ProjectMode = manifest.value("projectMode", 1u);
            } catch (...) {
                ENJIN_LOG_WARN(Player, "Manifest parse failed — using defaults");
            }
        }
        if (m_WindowTitle.empty()) m_WindowTitle = "TEGE Web Demo";
        if (m_WindowWidth == 0) m_WindowWidth = 1280;
        if (m_WindowHeight == 0) m_WindowHeight = 720;
        ENJIN_LOG_INFO(Player, "Game: %s (%ux%u)", m_WindowTitle.c_str(), m_WindowWidth, m_WindowHeight);

        // --- WebGPU renderer ---
        m_Renderer = std::make_unique<Enjin::Renderer::WebGPURenderer>();
        if (!m_Renderer->Initialize(nullptr)) {
            ENJIN_LOG_ERROR(Player, "WebGPU initialization failed");
            m_Renderer.reset();
            return;
        }
        ENJIN_LOG_INFO(Player, "WebGPU renderer initialized");

        // --- Camera ---
        m_Camera = std::make_unique<Enjin::Renderer::Camera>();
        m_Camera->SetPerspective(45.0f, 16.0f / 9.0f, 0.1f, 1000.0f);
        m_Camera->SetLookAt(
            Enjin::Math::Vector3(0.0f, 4.0f, 10.0f),
            Enjin::Math::Vector3(0.0f, 1.0f, -3.0f),
            Enjin::Math::Vector3(0.0f, 1.0f, 0.0f));

        // --- Input ---
        Enjin::Input::Initialize(nullptr);
        EM_ASM({
            var c = document.getElementById('game-canvas');
            if (!c) { console.warn('TEGE: no #game-canvas element found'); return; }
            if (!c.hasAttribute('tabindex')) c.setAttribute('tabindex', '0');
            c.addEventListener('click', function(){ c.focus(); });
            c.addEventListener('contextmenu', function(e){ e.preventDefault(); });
            c.focus();
        });

        m_CameraController = std::make_unique<Enjin::Renderer::CameraController>(m_Camera.get());

        // --- ECS World ---
        m_World = std::make_unique<Enjin::ECS::World>();

        // --- Audio ---
        Enjin::Audio::AudioManager::Get().Initialize();

        // --- Scripting ---
        if (m_ScriptEngine.Initialize()) {
            Enjin::Scripting::RegisterAllBindings(m_ScriptEngine.GetASEngine());
            m_ScriptEngine.SetWorld(m_World.get());
            m_ScriptEngine.SetScriptDirectory("scripts");
            m_ScriptSystem.SetWorld(m_World.get());
            m_ScriptSystem.SetScriptEngine(&m_ScriptEngine);
            m_ScriptSystem.SetCoroutineScheduler(&m_CoroutineScheduler);
            m_CoroutineScheduler.SetEngine(m_ScriptEngine.GetASEngine());
            m_ScriptEventBus.SetScriptEngine(&m_ScriptEngine);
        }

        // --- Physics ---
        auto backendType = static_cast<Enjin::Physics::PhysicsBackendType>(
            m_PhysicsBackendType <= 3 ? m_PhysicsBackendType : 0);
        auto projectMode = static_cast<Enjin::Scene::ProjectMode>(
            m_ProjectMode <= 2 ? m_ProjectMode : 1);
        m_Physics = Enjin::Physics::CreatePhysicsBackend(backendType, projectMode);
        if (m_Physics) m_Physics->SetWorld(m_World.get());
        m_Physics2D = Enjin::Physics::CreatePhysicsBackend2D(backendType, projectMode);
        if (m_Physics2D) m_Physics2D->Initialize(m_World.get());

        // --- Gameplay systems ---
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

        // --- Script bindings ---
        Enjin::Scripting::SetBindingsWorld(m_World.get());
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

        // --- Load scene ---
        bool sceneLoaded = false;

        // Option 1: From enjpak
        if (hasPack) {
            if (m_StartScene.empty()) {
                for (const auto& f : m_AssetReader.ListFiles()) {
                    if (f.size() > 6 && f.substr(f.size() - 6) == ".enjin") {
                        m_StartScene = f;
                        ENJIN_LOG_INFO(Player, "Auto-detected start scene: %s", f.c_str());
                        break;
                    }
                }
            }
            if (!m_StartScene.empty()) {
                auto sceneData = m_AssetReader.ReadFile(m_StartScene);
                if (!sceneData.empty()) {
                    std::string sceneStr(sceneData.begin(), sceneData.end());
                    Enjin::Scene::SceneSerializer serializer(m_World.get());
                    serializer.LoadFromString(sceneStr);
                    sceneLoaded = true;
                    ENJIN_LOG_INFO(Player, "Loaded scene: %s", m_StartScene.c_str());
                }
            }
        }

        // Option 2: Loose scene.enjin
        if (!sceneLoaded) {
            std::ifstream sceneFile("scene.enjin", std::ios::binary);
            if (sceneFile.good()) {
                std::string sceneStr((std::istreambuf_iterator<char>(sceneFile)),
                                     std::istreambuf_iterator<char>());
                if (!sceneStr.empty()) {
                    Enjin::Scene::SceneSerializer serializer(m_World.get());
                    serializer.LoadFromString(sceneStr);
                    sceneLoaded = true;
                    ENJIN_LOG_INFO(Player, "Loaded loose scene: scene.enjin");
                }
            }
        }

        // Option 3: Procedural demo fallback
        if (!sceneLoaded) {
            CreateDemoScene();
        }

        // Log loaded entities
        if (m_World) {
            auto meshEntities = m_World->GetEntitiesWithComponent<Enjin::ECS::MeshComponent>();
            ENJIN_LOG_INFO(Player, "Loaded %d mesh entities", static_cast<int>(meshEntities.size()));
        }

        // --- RenderSystem (same system as desktop, uses abstract IRenderBackend) ---
        m_RenderSystem = m_World->RegisterSystem<Enjin::ECS::RenderSystem>(m_World.get(), m_Renderer.get());
        m_RenderSystem->SetCamera(m_Camera.get());
        m_RenderSystem->Initialize();

        m_Initialized = true;
        ENJIN_LOG_INFO(Player, "Web Player initialized");
    }

    void Shutdown() {
        ENJIN_LOG_INFO(Player, "Web Player shutting down...");

        m_ObjectPool.DestroyAll(m_World.get());

        Enjin::Scripting::SetBindingsWorld(nullptr);
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

        // RenderSystem is owned by World — just destroy the world
        m_World.reset();
        m_CameraController.reset();
        m_Camera.reset();
        m_Renderer.reset();
        m_AssetReader.Close();
    }

    void Update(Enjin::f32 deltaTime) {
        if (!m_Initialized) return;

        Enjin::Audio::AudioManager::Get().Update();
        m_SimpleAudio.Update(deltaTime);
        m_SimpleAudio.UpdateAudioSources(deltaTime);

        Enjin::Input::Update();
        m_InputMap.Update(deltaTime);

        if (m_Physics) m_Physics->Update(deltaTime);
        if (m_Physics2D) m_Physics2D->Update(deltaTime);

        m_ControllerSystem.Update(deltaTime);

        if (m_CameraController && m_CameraController->IsEnabled()) {
            m_CameraController->Update(deltaTime);
        }

        m_ScriptSystem.Update(deltaTime);
        m_CoroutineScheduler.EndOfFrame();
        Enjin::Scripting::FlushDeferredEntityDestroys();

        m_TweenSystem.Update(m_World.get(), deltaTime);
        m_VisualScriptSystem.Update(deltaTime);
        m_BehaviorTreeSystem.Update(deltaTime);

        m_QuestSystem.Update(m_World.get(), deltaTime);
        m_ObjectPool.Update(m_World.get(), deltaTime);
        m_EntityEventBus.ProcessDeferred();

        m_WindSystem.Update(deltaTime);
        m_WorldTime.Update(deltaTime);
        if (m_Camera) {
            m_WeatherSystem.Update(deltaTime, m_Camera->GetPosition());
        }

        m_ParticleSystem.Update(deltaTime, m_World.get());
    }

    void Render() {
        if (!m_Initialized || !m_RenderSystem) return;

        m_RenderSystem->FlushPendingChanges();

        // BeginFrame + render pass + EndFrame via RenderSystem::Update
        m_Renderer->BeginFrameWebGPU();
        m_RenderSystem->Update(0.0f);  // deltaTime handled separately in Update()
        m_Renderer->EndFrame();

        // Sync CameraController after the first camera entity override
        if (!m_CameraControllerSynced && m_CameraController) {
            m_CameraController->SyncFromCamera();
            m_CameraControllerSynced = true;
        }
    }

    void OnCanvasResize(int w, int h, float dpr) {
        if (w <= 0 || h <= 0) return;
        Enjin::u32 pixelW = static_cast<Enjin::u32>(w * dpr);
        Enjin::u32 pixelH = static_cast<Enjin::u32>(h * dpr);
        if (m_Renderer) m_Renderer->Resize(pixelW, pixelH);
        if (m_Camera) m_Camera->SetPerspective(45.0f, static_cast<float>(w) / static_cast<float>(h), 0.1f, 1000.0f);
    }

    Enjin::u32 GetDrawCalls() const {
        return m_RenderSystem ? m_RenderSystem->GetDrawCallCount() : 0;
    }
    Enjin::u32 GetEntityCount() const {
        return m_RenderSystem ? m_RenderSystem->GetEntityRenderDataSize() : 0;
    }

private:
    void CreateDemoScene() {
        if (!m_World) return;

        auto makeBox = [](Enjin::ECS::MeshComponent& mesh) {
            mesh.vertices.clear();
            mesh.indices.clear();
            mesh.vertices.reserve(24);

            auto addFace = [&](Enjin::Math::Vector3 n,
                               Enjin::Math::Vector3 a, Enjin::Math::Vector3 b,
                               Enjin::Math::Vector3 c, Enjin::Math::Vector3 d) {
                Enjin::u32 base = static_cast<Enjin::u32>(mesh.vertices.size());
                Enjin::ECS::Vertex v{};
                v.normal = n;
                v.position = a; v.uv = {0,0}; mesh.vertices.push_back(v);
                v.position = b; v.uv = {1,0}; mesh.vertices.push_back(v);
                v.position = c; v.uv = {1,1}; mesh.vertices.push_back(v);
                v.position = d; v.uv = {0,1}; mesh.vertices.push_back(v);
                mesh.indices.push_back(base+0); mesh.indices.push_back(base+1); mesh.indices.push_back(base+2);
                mesh.indices.push_back(base+0); mesh.indices.push_back(base+2); mesh.indices.push_back(base+3);
            };

            using V = Enjin::Math::Vector3;
            addFace(V( 0, 1, 0), V(-0.5f, 0.5f,-0.5f), V( 0.5f, 0.5f,-0.5f), V( 0.5f, 0.5f, 0.5f), V(-0.5f, 0.5f, 0.5f));
            addFace(V( 0,-1, 0), V(-0.5f,-0.5f, 0.5f), V( 0.5f,-0.5f, 0.5f), V( 0.5f,-0.5f,-0.5f), V(-0.5f,-0.5f,-0.5f));
            addFace(V( 1, 0, 0), V( 0.5f,-0.5f,-0.5f), V( 0.5f, 0.5f,-0.5f), V( 0.5f, 0.5f, 0.5f), V( 0.5f,-0.5f, 0.5f));
            addFace(V(-1, 0, 0), V(-0.5f,-0.5f, 0.5f), V(-0.5f, 0.5f, 0.5f), V(-0.5f, 0.5f,-0.5f), V(-0.5f,-0.5f,-0.5f));
            addFace(V( 0, 0, 1), V( 0.5f,-0.5f, 0.5f), V( 0.5f, 0.5f, 0.5f), V(-0.5f, 0.5f, 0.5f), V(-0.5f,-0.5f, 0.5f));
            addFace(V( 0, 0,-1), V(-0.5f,-0.5f,-0.5f), V(-0.5f, 0.5f,-0.5f), V( 0.5f, 0.5f,-0.5f), V( 0.5f,-0.5f,-0.5f));
        };

        // Ground plane
        auto ground = m_World->CreateEntity();
        auto& gxf = m_World->AddComponent<Enjin::ECS::TransformComponent>(ground);
        gxf.position = Enjin::Math::Vector3(0, 0, 0);
        gxf.scale = Enjin::Math::Vector3(20, 0.1f, 20);
        auto& gmesh = m_World->AddComponent<Enjin::ECS::MeshComponent>(ground);
        makeBox(gmesh);
        auto& gmat = m_World->AddComponent<Enjin::ECS::MaterialComponent>(ground);
        gmat.baseColor = Enjin::Math::Vector3(0.3f, 0.6f, 0.2f);

        // Colored cubes
        for (int i = 0; i < 5; ++i) {
            auto cube = m_World->CreateEntity();
            auto& cxf = m_World->AddComponent<Enjin::ECS::TransformComponent>(cube);
            cxf.position = Enjin::Math::Vector3(static_cast<Enjin::f32>(i * 3 - 6), 1.0f, -3.0f);
            cxf.scale = Enjin::Math::Vector3(1, 1, 1);
            auto& cmesh = m_World->AddComponent<Enjin::ECS::MeshComponent>(cube);
            makeBox(cmesh);
            auto& cmat = m_World->AddComponent<Enjin::ECS::MaterialComponent>(cube);
            cmat.baseColor = Enjin::Math::Vector3(0.2f + i * 0.15f, 0.3f, 0.8f - i * 0.1f);
        }

        ENJIN_LOG_INFO(Player, "Demo scene created (6 entities)");
    }

    bool m_Initialized = false;
    bool m_CameraControllerSynced = false;

    // Renderer + render system
    std::unique_ptr<Enjin::Renderer::WebGPURenderer> m_Renderer;
    Enjin::ECS::RenderSystem* m_RenderSystem = nullptr;  // Owned by World
    std::unique_ptr<Enjin::Renderer::Camera> m_Camera;
    std::unique_ptr<Enjin::Renderer::CameraController> m_CameraController;
    std::unique_ptr<Enjin::ECS::World> m_World;

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
    Enjin::Effects::WorldTimeSystem m_WorldTime;
    Enjin::Effects::ParticleSystem m_ParticleSystem;

    // Scene & networking
    Enjin::Scene::SceneManager m_SceneManager;
    Enjin::Networking::NetworkSystem m_NetworkSystem;

    // Build manifest
    std::string m_WindowTitle;
    Enjin::u32 m_WindowWidth = 1280;
    Enjin::u32 m_WindowHeight = 720;
    std::string m_StartScene;
    Enjin::u32 m_PhysicsBackendType = 0;
    Enjin::u32 m_ProjectMode = 1;
};

// ============================================================================
// Extern C callbacks for JS interop (ResizeObserver, perf HUD)
// ============================================================================
extern "C" {

EMSCRIPTEN_KEEPALIVE void onCanvasResize(int w, int h, float dpr) {
    if (g_Player) g_Player->OnCanvasResize(w, h, dpr);
}

EMSCRIPTEN_KEEPALIVE int getDrawCallCount() {
    return g_Player ? static_cast<int>(g_Player->GetDrawCalls()) : 0;
}

EMSCRIPTEN_KEEPALIVE int getEntityCount() {
    return g_Player ? static_cast<int>(g_Player->GetEntityCount()) : 0;
}

} // extern "C"

// ============================================================================
// Main — Emscripten entry point
// ============================================================================
int main(int argc, char* argv[]) {
    (void)argc; (void)argv;

    static WebGamePlayer player;
    g_Player = &player;
    player.Initialize();

    // Emscripten main loop with proper delta time
    emscripten_set_main_loop_arg([](void* userData) {
        auto* p = static_cast<WebGamePlayer*>(userData);

        // Real delta time from high-resolution timer
        static double lastTime = emscripten_performance_now();
        double now = emscripten_performance_now();
        float dt = static_cast<float>((now - lastTime) / 1000.0);
        lastTime = now;
        dt = std::min(dt, 0.1f);  // Clamp to 100ms (10fps floor)

        p->Update(dt);
        p->Render();
    }, &player, 0, true);

    return 0;
}

#endif // ENJIN_PLATFORM_WEB
