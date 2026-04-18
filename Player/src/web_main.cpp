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
#if !ENJIN_RENDERER_WEBGPU
#include "Enjin/ECS/Systems/RenderSystem.h"
#endif
#include "Enjin/ECS/Components/Camera.h"
#include "Enjin/ECS/Components/Light.h"
#include "Enjin/ECS/Components/Mesh.h"
#include "Enjin/ECS/Components/Material.h"
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
#include <unordered_map>
#include <fstream>

static constexpr const char* PACK_KEY = "enjin_default_pack_key";

// Web game player — standalone (no Application base class, no GLFW)
class WebGamePlayer {
public:
    void Initialize() {
        EM_ASM(console.log("=== TEGE Initialize() called ==="));
        ENJIN_LOG_INFO(Player, "Enjin Web Player starting...");

        // Fetch game.enjpak from the server into the WASM virtual filesystem.
        // emscripten_wget_data is used because emscripten_wget aborts on 404.
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
                EM_ASM(console.log("=== game.enjpak downloaded, size:", $0, "==="), len);
                // Try empty key first (Build Game default), then the built-in key
                hasPack = m_AssetReader.Open("game.enjpak", "");
                if (!hasPack) hasPack = m_AssetReader.Open("game.enjpak", PACK_KEY);
                EM_ASM(console.log("=== enjpak open:", $0, "==="), hasPack ? 1 : 0);
            } else {
                ENJIN_LOG_WARN(Player, "No game.enjpak available on server");
            }
        }

        // If no enjpak, try fetching a loose scene file
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

        // Read build manifest (or use defaults)
        std::vector<Enjin::u8> manifestData;
        if (hasPack) manifestData = m_AssetReader.ReadFile("_build/manifest.json");
        if (manifestData.empty()) {
            ENJIN_LOG_INFO(Player, "Using default settings (no manifest)");
        }

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
        // Defaults for demo mode
        if (m_WindowTitle.empty()) m_WindowTitle = "TEGE Web Demo";
        if (m_WindowWidth == 0) m_WindowWidth = 1280;
        if (m_WindowHeight == 0) m_WindowHeight = 720;
        ENJIN_LOG_INFO(Player, "Game: %s (%ux%u)", m_WindowTitle.c_str(), m_WindowWidth, m_WindowHeight);

        // Initialize WebGPU renderer
        EM_ASM(console.log("=== Creating WebGPU renderer ==="));
        m_Renderer = std::make_unique<Enjin::Renderer::WebGPURenderer>();
        if (!m_Renderer->Initialize(nullptr)) {
            EM_ASM(console.error("=== WebGPU init FAILED ==="));
            m_Renderer.reset();
            return;
        }
        EM_ASM(console.log("=== WebGPU renderer OK ==="));

        // Setup camera
        EM_ASM(console.log("=== Setting up camera ==="));
        m_Camera = std::make_unique<Enjin::Renderer::Camera>();
        m_Camera->SetPerspective(45.0f, 16.0f / 9.0f, 0.1f, 1000.0f);
        m_Camera->SetLookAt(
            Enjin::Math::Vector3(0.0f, 4.0f, 10.0f),  // eye: up and back
            Enjin::Math::Vector3(0.0f, 1.0f, -3.0f),   // look at the cubes
            Enjin::Math::Vector3(0.0f, 1.0f, 0.0f));  // up

        // Initialize input system (web doesn't need a Window — uses canvas selectors directly)
        Enjin::Input::Initialize(nullptr);

        // Wire up canvas focus + suppress browser context menu so RMB-look works.
        // This runs from the player itself so any HTML host (web-demo, editor's
        // HTML5 build, custom embed) works without extra setup, as long as the
        // canvas has id="game-canvas".
        EM_ASM({
            var c = document.getElementById('game-canvas');
            if (!c) { console.warn('TEGE: no #game-canvas element found'); return; }
            if (!c.hasAttribute('tabindex')) c.setAttribute('tabindex', '0');
            c.addEventListener('click', function(){ c.focus(); });
            c.addEventListener('contextmenu', function(e){ e.preventDefault(); });
            c.focus();
        });

        m_CameraController = std::make_unique<Enjin::Renderer::CameraController>(m_Camera.get());

        // Create ECS world
        EM_ASM(console.log("=== Creating ECS world ==="));
        m_World = std::make_unique<Enjin::ECS::World>();

        // Setup render system
// [WEBGPU-STUB]         m_RenderSystem = m_World->RegisterSystem<Enjin::ECS::RenderSystem>(m_World.get(), m_Renderer.get());
// [WEBGPU-STUB]         m_RenderSystem->SetCamera(m_Camera.get());
// [WEBGPU-STUB]         m_RenderSystem->Initialize();

        // Initialize audio (miniaudio supports Web Audio natively)
        EM_ASM(console.log("=== Initializing audio ==="));
        Enjin::Audio::AudioManager::Get().Initialize();

        // Initialize scripting engine
        EM_ASM(console.log("=== Initializing scripting ==="));
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
        EM_ASM(console.log("=== Initializing physics ==="));
        auto backendType = static_cast<Enjin::Physics::PhysicsBackendType>(
            m_PhysicsBackendType <= 3 ? m_PhysicsBackendType : 0);
        auto projectMode = static_cast<Enjin::Scene::ProjectMode>(
            m_ProjectMode <= 2 ? m_ProjectMode : 1);
        m_Physics = Enjin::Physics::CreatePhysicsBackend(backendType, projectMode);
        if (m_Physics) m_Physics->SetWorld(m_World.get());
        m_Physics2D = Enjin::Physics::CreatePhysicsBackend2D(backendType, projectMode);
        if (m_Physics2D) m_Physics2D->Initialize(m_World.get());

        // Initialize gameplay systems
        EM_ASM(console.log("=== Initializing gameplay systems ==="));
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
        EM_ASM(console.log("=== Wiring script bindings ==="));
        Enjin::Scripting::SetBindingsWorld(m_World.get());
        // Enjin::Scripting::SetBindingsRenderSystem(m_RenderSystem); // Excluded on web
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

        // Load the start scene (or create demo)
        EM_ASM(console.log("=== Loading scene ==="));
        bool sceneLoaded = false;

        // Option 1: Load from enjpak (full build pipeline)
        if (hasPack) {
            // If no start scene specified in manifest, find the first .enjin in the pack
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
                    ENJIN_LOG_INFO(Player, "Loaded start scene: %s", m_StartScene.c_str());
                }
            }
        }

        // Option 2: Load loose scene.enjin fetched via HTTP
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

        // Log what loaded
        if (m_World) {
            auto meshEntities = m_World->GetEntitiesWithComponent<Enjin::ECS::MeshComponent>();
            EM_ASM(console.log("=== Entities with meshes:", $0, "==="), static_cast<int>(meshEntities.size()));
            for (auto e : meshEntities) {
                auto* name = m_World->GetComponent<Enjin::ECS::NameComponent>(e);
                auto* mesh = m_World->GetComponent<Enjin::ECS::MeshComponent>(e);
                auto* xform = m_World->GetComponent<Enjin::ECS::TransformComponent>(e);
                if (name && mesh && xform) {
                    EM_ASM(console.log("  ", UTF8ToString($0), "verts:", $1, "pos:", $2, $3, $4),
                        name->name.c_str(), static_cast<int>(mesh->vertices.size()),
                        xform->position.x, xform->position.y, xform->position.z);
                }
            }
        }

        // Setup WebGPU rendering pipeline
        EM_ASM(console.log("=== Setting up render pipeline ==="));
        SetupWebRenderPipeline();
        EM_ASM(console.log("=== Pipeline ready:", $0), m_RenderPipelineReady ? 1 : 0);

        m_Initialized = true;
        ENJIN_LOG_INFO(Player, "Web Player initialized");
    }

    void Shutdown() {
        ENJIN_LOG_INFO(Player, "Web Player shutting down...");

        m_ObjectPool.DestroyAll(m_World.get());

        // Clear script bindings
        Enjin::Scripting::SetBindingsWorld(nullptr);
        // Enjin::Scripting::SetBindingsRenderSystem(nullptr); // Excluded on web
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

        if (false) { // RenderSystem excluded on web
            // m_RenderSystem->Shutdown(); // Excluded on web
            // m_RenderSystem = nullptr; // Excluded on web
        }
        m_World.reset();
        m_CameraController.reset();
        m_Camera.reset();
        m_Renderer.reset();
        m_AssetReader.Close();
    }

    void Update(Enjin::f32 deltaTime) {
        if (!m_Initialized) return;

        // Update audio
        Enjin::Audio::AudioManager::Get().Update();
        m_SimpleAudio.Update(deltaTime);
        m_SimpleAudio.UpdateAudioSources(deltaTime);

        // Update input — must be called before any consumer of Input::* queries
        Enjin::Input::Update();
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

    void CreateDemoScene() {
        if (!m_World) return;
        EM_ASM(console.log("=== CreateDemoScene: start ==="));

        try {
            // Helper to build a box mesh with per-face normals (24 verts, 36 indices)
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
                // +Y (top)    -Y (bottom)    +X (right)    -X (left)    +Z (front)   -Z (back)
                addFace(V( 0, 1, 0), V(-0.5f, 0.5f,-0.5f), V( 0.5f, 0.5f,-0.5f), V( 0.5f, 0.5f, 0.5f), V(-0.5f, 0.5f, 0.5f));
                addFace(V( 0,-1, 0), V(-0.5f,-0.5f, 0.5f), V( 0.5f,-0.5f, 0.5f), V( 0.5f,-0.5f,-0.5f), V(-0.5f,-0.5f,-0.5f));
                addFace(V( 1, 0, 0), V( 0.5f,-0.5f,-0.5f), V( 0.5f, 0.5f,-0.5f), V( 0.5f, 0.5f, 0.5f), V( 0.5f,-0.5f, 0.5f));
                addFace(V(-1, 0, 0), V(-0.5f,-0.5f, 0.5f), V(-0.5f, 0.5f, 0.5f), V(-0.5f, 0.5f,-0.5f), V(-0.5f,-0.5f,-0.5f));
                addFace(V( 0, 0, 1), V( 0.5f,-0.5f, 0.5f), V( 0.5f, 0.5f, 0.5f), V(-0.5f, 0.5f, 0.5f), V(-0.5f,-0.5f, 0.5f));
                addFace(V( 0, 0,-1), V(-0.5f,-0.5f,-0.5f), V(-0.5f, 0.5f,-0.5f), V( 0.5f, 0.5f,-0.5f), V( 0.5f,-0.5f,-0.5f));
            };

            // Ground plane
            EM_ASM(console.log("=== CreateDemoScene: ground ==="));
            auto ground = m_World->CreateEntity();
            auto& gxf = m_World->AddComponent<Enjin::ECS::TransformComponent>(ground);
            gxf.position = Enjin::Math::Vector3(0, 0, 0);
            gxf.scale = Enjin::Math::Vector3(20, 0.1f, 20);
            auto& gmesh = m_World->AddComponent<Enjin::ECS::MeshComponent>(ground);
            makeBox(gmesh);
            auto& gmat = m_World->AddComponent<Enjin::ECS::MaterialComponent>(ground);
            gmat.baseColor = Enjin::Math::Vector3(0.3f, 0.6f, 0.2f);

            // Some cubes
            for (int i = 0; i < 5; ++i) {
                EM_ASM(console.log("=== CreateDemoScene: cube", $0, "==="), i);
                auto cube = m_World->CreateEntity();
                auto& cxf = m_World->AddComponent<Enjin::ECS::TransformComponent>(cube);
                cxf.position = Enjin::Math::Vector3(static_cast<Enjin::f32>(i * 3 - 6), 1.0f, -3.0f);
                cxf.scale = Enjin::Math::Vector3(1, 1, 1);
                auto& cmesh = m_World->AddComponent<Enjin::ECS::MeshComponent>(cube);
                makeBox(cmesh);
                auto& cmat = m_World->AddComponent<Enjin::ECS::MaterialComponent>(cube);
                cmat.baseColor = Enjin::Math::Vector3(0.2f + i * 0.15f, 0.3f, 0.8f - i * 0.1f);
            }
            EM_ASM(console.log("=== CreateDemoScene: done ==="));
        } catch (const std::exception& e) {
            EM_ASM(console.error("=== CreateDemoScene FAILED:", UTF8ToString($0)), e.what());
        }
    }

    void SetupWebRenderPipeline() {
        if (!m_Renderer) return;

        // Create uniform buffers
        m_ViewUniformBuffer = m_Renderer->CreateBuffer(256, WGPUBufferUsage_Uniform, nullptr);
        m_ObjectUniformBuffer = m_Renderer->CreateBuffer(256, WGPUBufferUsage_Uniform, nullptr);

        // Configure vertex layout matching Vertex struct (only first 3 attrs used by shader)
        Enjin::Renderer::WebGPUVertexBufferLayout vertexLayout;
        vertexLayout.stride = sizeof(Enjin::ECS::Vertex);
        vertexLayout.attributes = {
            {Enjin::Renderer::WebGPUVertexFormat::Float32x3, offsetof(Enjin::ECS::Vertex, position), 0},  // position
            {Enjin::Renderer::WebGPUVertexFormat::Float32x3, offsetof(Enjin::ECS::Vertex, normal),   1},  // normal
            {Enjin::Renderer::WebGPUVertexFormat::Float32x2, offsetof(Enjin::ECS::Vertex, uv),       2},  // uv
        };

        // Compile WGSL shader
        auto* compiler = m_Renderer->GetShaderCompiler();
        if (!compiler) {
            ENJIN_LOG_ERROR(Player, "No shader compiler available");
            return;
        }

        // Read shader source from embedded or file
        const char* wgslSource = R"(
struct ViewUniforms {
    view: mat4x4<f32>,
    proj: mat4x4<f32>,
    cameraPos: vec3<f32>,
    _pad: f32,
    lightDir: vec3<f32>,
    lightIntensity: f32,
    lightColor: vec3<f32>,
    ambientIntensity: f32,
    ambientColor: vec3<f32>,
    _pad2: f32,
};
struct ObjectUniforms {
    model: mat4x4<f32>,
    baseColor: vec3<f32>,
    metallic: f32,
    emissive: vec3<f32>,
    roughness: f32,
    opacity: f32,
    _pad: vec3<f32>,
};
@group(0) @binding(0) var<uniform> view: ViewUniforms;
@group(1) @binding(0) var<uniform> object: ObjectUniforms;

struct VertexInput {
    @location(0) position: vec3<f32>,
    @location(1) normal: vec3<f32>,
    @location(2) uv: vec2<f32>,
};
struct VertexOutput {
    @builtin(position) clip_position: vec4<f32>,
    @location(0) world_pos: vec3<f32>,
    @location(1) world_normal: vec3<f32>,
    @location(2) uv: vec2<f32>,
};

@vertex fn vs_main(in: VertexInput) -> VertexOutput {
    var out: VertexOutput;
    let wp = object.model * vec4<f32>(in.position, 1.0);
    out.clip_position = view.proj * view.view * wp;
    out.world_pos = wp.xyz;
    out.world_normal = normalize((object.model * vec4<f32>(in.normal, 0.0)).xyz);
    out.uv = in.uv;
    return out;
}
@fragment fn fs_main(in: VertexOutput) -> @location(0) vec4<f32> {
    let N = normalize(in.world_normal);
    let L = normalize(-view.lightDir);
    let ndotl = max(dot(N, L), 0.0);
    let diffuse = object.baseColor * ndotl * view.lightColor * view.lightIntensity;
    let ambient = view.ambientColor * view.ambientIntensity * object.baseColor;
    let color = ambient + diffuse + object.emissive;
    let mapped = color / (color + vec3<f32>(1.0));
    return vec4<f32>(mapped, object.opacity);
}
)";
        WGPUShaderModule shaderModule = compiler->CompileWGSL(wgslSource, "web_pbr");
        if (!shaderModule) {
            ENJIN_LOG_ERROR(Player, "Failed to compile web PBR shader: %s", compiler->GetLastError().c_str());
            return;
        }

        // Create pipeline with vertex layout
        Enjin::Renderer::WebGPURenderPipelineDesc pipelineDesc;
        pipelineDesc.vertexShader = shaderModule;
        pipelineDesc.fragmentShader = shaderModule;
        pipelineDesc.vertexBuffers = { vertexLayout };
        pipelineDesc.colorFormat = Enjin::Renderer::GetPreferredSwapChainFormat();
        pipelineDesc.cullMode = WGPUCullMode_None;  // Demo: skip culling so winding doesn't matter
        m_WebPipeline = m_Renderer->GetPipelineFactory()->CreateRenderPipeline(pipelineDesc);
        if (!m_WebPipeline) {
            ENJIN_LOG_ERROR(Player, "Failed to create web render pipeline");
            return;
        }

        // Create bind groups for uniforms
        // (View at group 0, Object at group 1)
        WGPUBindGroupEntry viewEntry{};
        viewEntry.binding = 0;
        viewEntry.buffer = m_ViewUniformBuffer.buffer;
        viewEntry.offset = 0;
        viewEntry.size = 256;

        WGPUBindGroupLayout viewLayout = wgpuRenderPipelineGetBindGroupLayout(m_WebPipeline, 0);
        m_ViewBindGroup = m_Renderer->CreateBindGroup(viewLayout, {viewEntry});

        WGPUBindGroupEntry objEntry{};
        objEntry.binding = 0;
        objEntry.buffer = m_ObjectUniformBuffer.buffer;
        objEntry.offset = 0;
        objEntry.size = 256;

        WGPUBindGroupLayout objLayout = wgpuRenderPipelineGetBindGroupLayout(m_WebPipeline, 1);
        m_DefaultObjectBindGroup = m_Renderer->CreateBindGroup(objLayout, {objEntry});

        wgpuBindGroupLayoutRelease(viewLayout);
        wgpuBindGroupLayoutRelease(objLayout);
        wgpuShaderModuleRelease(shaderModule);

        m_RenderPipelineReady = true;
        ENJIN_LOG_INFO(Player, "WebGPU render pipeline created");
    }

    void Render() {
        if (!m_Initialized || !m_Renderer) return;

        if (m_Renderer->BeginFrame()) {
            // Render all entities with MeshComponent
            if (m_World && m_Camera && m_RenderPipelineReady) {
                // Update view uniforms
                UpdateViewUniforms();

                // Set pipeline
                m_Renderer->SetPipeline(m_WebPipeline);
                m_Renderer->SetBindGroup(0, m_ViewBindGroup);

                // Draw each entity
                for (auto entity : m_World->GetEntitiesWithComponent<Enjin::ECS::MeshComponent>()) {
                    auto* mesh = m_World->GetComponent<Enjin::ECS::MeshComponent>(entity);
                    auto* xform = m_World->GetComponent<Enjin::ECS::TransformComponent>(entity);
                    if (!mesh || !xform || !xform->visible) continue;
                    if (mesh->vertices.empty() || mesh->indices.empty()) continue;

                    // Get or create GPU buffers for this entity
                    auto& gpuData = m_EntityGPUData[entity];

                    // Upload mesh data if not yet uploaded
                    if (!gpuData.uploaded) {
                        gpuData.vertexBuffer = m_Renderer->CreateBuffer(
                            mesh->vertices.size() * sizeof(Enjin::ECS::Vertex),
                            WGPUBufferUsage_Vertex, mesh->vertices.data());
                        gpuData.indexBuffer = m_Renderer->CreateBuffer(
                            mesh->indices.size() * sizeof(Enjin::u32),
                            WGPUBufferUsage_Index, mesh->indices.data());
                        gpuData.indexCount = static_cast<Enjin::u32>(mesh->indices.size());
                        gpuData.uploaded = true;
                    }

                    // Update per-object uniforms
                    UpdateObjectUniforms(entity);
                    m_Renderer->SetBindGroup(1, gpuData.objectBindGroup ? gpuData.objectBindGroup : m_DefaultObjectBindGroup);

                    // Draw
                    m_Renderer->SetVertexBuffer(0, gpuData.vertexBuffer.buffer, 0, 0);
                    m_Renderer->SetIndexBuffer(gpuData.indexBuffer.buffer, WGPUIndexFormat_Uint32, 0, 0);
                    m_Renderer->DrawIndexed(gpuData.indexCount, 1, 0, 0, 0);
                }
            }

            m_Renderer->EndFrame();
        }
    }

    void UpdateViewUniforms() {
        if (!m_Camera) return;
        struct ViewUBO {
            Enjin::Math::Matrix4 view, proj;
            Enjin::Math::Vector3 cameraPos; float _pad;
            Enjin::Math::Vector3 lightDir; float lightIntensity;
            Enjin::Math::Vector3 lightColor; float ambientIntensity;
            Enjin::Math::Vector3 ambientColor; float _pad2;
        } ubo;

        // Use game camera entity if found (sync on first frame).
        // Fall back to a reasonable default looking at the scene origin.
        if (!m_CameraSynced && m_World) {
            bool found = false;
            for (auto entity : m_World->GetEntitiesWithComponent<Enjin::ECS::CameraComponent>()) {
                auto* xform = m_World->GetComponent<Enjin::ECS::TransformComponent>(entity);
                if (xform) {
                    // Point camera at scene origin from the game camera's position
                    Enjin::Math::Vector3 target(0.0f, 0.0f, 0.0f);
                    m_Camera->SetLookAt(xform->position, target,
                        Enjin::Math::Vector3(0.0f, 1.0f, 0.0f));
                    found = true;
                    EM_ASM(console.log("=== Game camera at:", $0, $1, $2, "==="),
                        xform->position.x, xform->position.y, xform->position.z);
                    break;
                }
            }
            if (!found) {
                m_Camera->SetLookAt(
                    Enjin::Math::Vector3(0.0f, 8.0f, 15.0f),
                    Enjin::Math::Vector3(0.0f, 0.0f, 0.0f),
                    Enjin::Math::Vector3(0.0f, 1.0f, 0.0f));
            }
            if (m_CameraController) m_CameraController->SyncFromCamera();
            m_CameraSynced = true;
        }

        ubo.view = m_Camera->GetViewMatrix();
        ubo.proj = m_Camera->GetProjectionMatrix();
        ubo.cameraPos = m_Camera->GetPosition();

        // Read first directional light from ECS, fall back to defaults
        ubo.lightDir = Enjin::Math::Vector3(0.5f, 0.8f, 0.3f).Normalized();
        ubo.lightIntensity = 1.2f;
        ubo.lightColor = Enjin::Math::Vector3(1.0f, 0.95f, 0.9f);
        if (m_World) {
            for (auto entity : m_World->GetEntitiesWithComponent<Enjin::ECS::LightComponent>()) {
                auto* light = m_World->GetComponent<Enjin::ECS::LightComponent>(entity);
                auto* xform = m_World->GetComponent<Enjin::ECS::TransformComponent>(entity);
                if (light && light->type == Enjin::ECS::LightType::Directional && xform) {
                    ubo.lightDir = (xform->rotation.GetForward() * -1.0f).Normalized();
                    ubo.lightIntensity = light->intensity;
                    ubo.lightColor = light->color;
                    break;
                }
            }
        }

        ubo.ambientIntensity = 0.5f;  // High ambient for web (no GI/shadows/env maps)
        ubo.ambientColor = Enjin::Math::Vector3(0.3f, 0.3f, 0.35f);
        m_Renderer->UpdateBuffer(m_ViewUniformBuffer, &ubo, sizeof(ubo), 0);
    }

    void UpdateObjectUniforms(Enjin::ECS::Entity entity) {
        auto* xform = m_World->GetComponent<Enjin::ECS::TransformComponent>(entity);
        auto* mat = m_World->GetComponent<Enjin::ECS::MaterialComponent>(entity);
        struct ObjUBO {
            Enjin::Math::Matrix4 model;
            Enjin::Math::Vector3 baseColor; float metallic;
            Enjin::Math::Vector3 emissive; float roughness;
            float opacity; Enjin::Math::Vector3 _pad;
        } ubo;
        ubo.model = xform ? xform->ToMatrix() : Enjin::Math::Matrix4::Identity();
        ubo.baseColor = mat ? mat->baseColor : Enjin::Math::Vector3(0.8f, 0.8f, 0.8f);
        ubo.metallic = mat ? mat->metallic : 0.0f;
        ubo.emissive = mat ? mat->emissiveColor * mat->emissiveStrength : Enjin::Math::Vector3(0.0f);
        ubo.roughness = mat ? mat->roughness : 0.5f;
        ubo.opacity = mat ? mat->opacity : 1.0f;
        m_Renderer->UpdateBuffer(m_ObjectUniformBuffer, &ubo, sizeof(ubo), 0);
    }

private:
    bool m_Initialized = false;
    bool m_RenderPipelineReady = false;
    bool m_CameraSynced = false;

    // Per-entity GPU data
    struct EntityGPUData {
        Enjin::Renderer::WebGPUBufferHandle vertexBuffer;
        Enjin::Renderer::WebGPUBufferHandle indexBuffer;
        WGPUBindGroup objectBindGroup = nullptr;
        Enjin::u32 indexCount = 0;
        bool uploaded = false;
    };
    std::unordered_map<Enjin::ECS::Entity, EntityGPUData> m_EntityGPUData;

    // WebGPU rendering resources
    WGPURenderPipeline m_WebPipeline = nullptr;
    WGPUBindGroup m_ViewBindGroup = nullptr;
    WGPUBindGroup m_DefaultObjectBindGroup = nullptr;
    Enjin::Renderer::WebGPUBufferHandle m_ViewUniformBuffer;
    Enjin::Renderer::WebGPUBufferHandle m_ObjectUniformBuffer;

    // Core systems
    std::unique_ptr<Enjin::Renderer::WebGPURenderer> m_Renderer;
    std::unique_ptr<Enjin::Renderer::Camera> m_Camera;
    std::unique_ptr<Enjin::Renderer::CameraController> m_CameraController;
    std::unique_ptr<Enjin::ECS::World> m_World;
    // Enjin::ECS::RenderSystem* // m_RenderSystem = nullptr; // Excluded on web // Excluded on web

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

    // Build manifest values
    std::string m_WindowTitle;
    Enjin::u32 m_WindowWidth = 1280;
    Enjin::u32 m_WindowHeight = 720;
    std::string m_StartScene;
    Enjin::u32 m_PhysicsBackendType = 0;
    Enjin::u32 m_ProjectMode = 1;
};

int main(int argc, char* argv[]) {
    (void)argc; (void)argv;
    WebGamePlayer player;
    player.Initialize();
    // Emscripten main loop — browser calls this every frame
    emscripten_set_main_loop_arg([](void* userData) {
        auto* p = static_cast<WebGamePlayer*>(userData);
        p->Update(1.0f / 60.0f); // Fixed timestep for web
        p->Render();
    }, &player, 0, true);
    return 0;
}

#endif // ENJIN_PLATFORM_WEB
