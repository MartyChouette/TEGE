// Enjin Engine — Web Player Entry Point (Emscripten / WebGPU)
// Production web player: WebRenderPipeline for PBR rendering, proper frame
// timing, responsive canvas via ResizeObserver, all gameplay systems active.

#include "Enjin/Platform/Platform.h"
#include <filesystem>
#include <set>
#include <cctype>
#include <cstdio>
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
#include "Enjin/Renderer/WebGPU/WebGPUParticleSystem.h"
#include "Enjin/ECS/Components/GPUParticleEmitter.h"
#include "Enjin/Effects/ParticleColliders.h"
#include "Enjin/Renderer/WebGPU/WebGPUVegetationSystem.h"
#if defined(ENJIN_WEBGPU_COMPUTE_SMOKETEST)
#include "Enjin/Renderer/WebGPU/WebGPUComputeSmokeTest.h"
#endif
#include "Enjin/ECS/Systems/RenderSystem.h"
#include "Enjin/Renderer/Camera.h"
#include "Enjin/Renderer/CameraController.h"
#include "Enjin/Scene/SceneSerializer.h"
#include "Enjin/Scene/SceneManager.h"
#include "Enjin/Renderer/SceneRenderSettings.h"
#include "Enjin/Input/InputAction.h"
#include "Enjin/GUI/UISystem.h"
#include "Enjin/GUI/UITemplates.h"
#include "Enjin/Renderer/WebGPU/WebGPUTypes.h"
#include <imgui.h>
#include <imgui_impl_wgpu.h>
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/ECS/Components/WeatherZone.h"
#include "Enjin/ECS/Components/TemperatureZone.h"
#include <climits>
#include "Enjin/Effects/Weather.h"
#include "Enjin/Effects/TreeRenderer.h"
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
#include "Enjin/Gameplay/CameraDirector.h"
#include "Enjin/Gameplay/RecordRewindSystem.h"
#include "Enjin/Effects/InteractiveWater.h"
#include "Enjin/Gameplay/SimulationClock.h"
#include "Enjin/ECS/Systems/ParallaxSystem.h"
#include "Enjin/ECS/Systems/TweenSystem.h"
#include "Enjin/ECS/Systems/VisualScriptSystem.h"
#include "Enjin/ECS/Systems/BehaviorTreeSystem.h"
#include "Enjin/ECS/Systems/AISystem.h"
#include "Enjin/ECS/Systems/StateMachineSystem.h"
#include "Enjin/ECS/Systems/DialogueSystem.h"
#include "Enjin/Gameplay/CinematicSystem.h"
#include "Enjin/Effects/ElementalSystem.h"
#include "Enjin/ECS/EntityEventBus.h"
#include "Enjin/ECS/Components/Skeleton.h"
#include "Enjin/Gameplay/GameplayLoop.h"
#include "Enjin/Gameplay/FootstepSystem.h"
#include "Enjin/Accessibility/SubtitleSystem.h"
#include "Enjin/Accessibility/Announcer.h"
#include "Enjin/Accessibility/AccessibilitySettings.h"
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
#include <emscripten/heap.h>
#include <emscripten/html5.h>
#include <string>
#include <memory>
#include <fstream>
#include <sstream>
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
    // Boot is asynchronous — no ASYNCIFY, so nothing may block:
    //   StartBoot (pak fetch) → StartRendererInit (adapter/device) → FinishInitialize.
    // The main loop runs from the first frame and no-ops until m_Initialized flips.
    void StartBoot() {
        ENJIN_LOG_INFO(Player, "Enjin Web Player starting...");

        // Persist saves across page reloads. The save directory (/saves, see
        // SaveSystem::GetSaveDirectory) is otherwise MEMFS and vanishes on
        // reload. Mount an IndexedDB-backed FS there and pull existing saves in.
        // This is async, but it runs during the pak fetch + renderer init and
        // completes long before any user-triggered load. Each write flushes back
        // via FS.syncfs(false) (LocalSaveBackend::PersistWebSaves).
        EM_ASM({
            try {
                FS.mkdirTree('/saves');
                FS.mount(IDBFS, {}, '/saves');
                FS.syncfs(true, function(err) {
                    if (err) console.warn('[SAVES] IDBFS initial load error', err);
                    else console.log('[SAVES] persistent saves ready (/saves via IndexedDB)');
                });
            } catch (e) { console.warn('[SAVES] IDBFS mount failed', e); }
        });

        // Fetch game.enjpak from the server into the WASM virtual filesystem.
        // The download buffer is freed when onload returns, so persist it first.
        emscripten_async_wget_data("game.enjpak", this,
            [](void* arg, void* buf, int len) {
                auto* self = static_cast<WebGamePlayer*>(arg);
                FILE* f = fopen("game.enjpak", "wb");
                if (f) { fwrite(buf, 1, static_cast<size_t>(len), f); fclose(f); }
                self->m_HasPack = self->m_AssetReader.Open("game.enjpak", "");
                if (!self->m_HasPack) self->m_HasPack = self->m_AssetReader.Open("game.enjpak", PACK_KEY);
                self->ExtractPackAssetsToMemFS();
                self->StartRendererInit();
            },
            [](void* arg) {
                auto* self = static_cast<WebGamePlayer*>(arg);
                ENJIN_LOG_WARN(Player, "No game.enjpak available on server");
                // Fallback: try fetching a loose scene file
                emscripten_async_wget_data("scene.enjin", self,
                    [](void* arg2, void* buf, int len) {
                        auto* self2 = static_cast<WebGamePlayer*>(arg2);
                        FILE* f = fopen("scene.enjin", "wb");
                        if (f) { fwrite(buf, 1, static_cast<size_t>(len), f); fclose(f); }
                        self2->StartRendererInit();
                    },
                    [](void* arg2) {
                        static_cast<WebGamePlayer*>(arg2)->StartRendererInit();
                    });
            });
    }

    // Extract model/texture/audio/font files from the pak to the WASM MEMFS so
    // the disk-based loaders find them by path. GLTFLoader (cgltf_parse_file)
    // and the raster texture/audio loaders read straight from disk; on web the
    // game ships only as game.enjpak, so without this, models and textures
    // never load. Scenes/scripts already load through the AssetReader directly.
    void ExtractPackAssetsToMemFS() {
        if (!m_HasPack) return;
        static const std::set<std::string> kDiskExts = {
            ".glb", ".gltf", ".fbx", ".obj", ".dae", ".3ds", ".ply", ".vox",
            ".png", ".jpg", ".jpeg", ".tga", ".bmp", ".svg", ".hdr",
            ".wav", ".ogg", ".mp3", ".flac", ".ttf", ".otf"
        };
        int extracted = 0;
        for (const auto& vpath : m_AssetReader.ListFiles()) {
            auto dot = vpath.find_last_of('.');
            if (dot == std::string::npos) continue;
            std::string ext = vpath.substr(dot);
            for (auto& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (kDiskExts.find(ext) == kDiskExts.end()) continue;

            std::vector<Enjin::u8> data = m_AssetReader.ReadFile(vpath);
            if (data.empty()) continue;

            std::error_code ec;
            std::filesystem::path p(vpath);
            if (p.has_parent_path()) std::filesystem::create_directories(p.parent_path(), ec);
            FILE* f = fopen(vpath.c_str(), "wb");
            if (f) { fwrite(data.data(), 1, data.size(), f); fclose(f); ++extracted; }
        }
        ENJIN_LOG_INFO(Player, "Extracted %d pak assets to MEMFS (models/textures/audio/fonts)", extracted);
    }

    void StartRendererInit() {
        // Read build manifest
        std::vector<Enjin::u8> manifestData;
        if (m_HasPack) manifestData = m_AssetReader.ReadFile("_build/manifest.json");
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
                if (manifest.contains("frameSettings")) {
                    const auto& fs = manifest["frameSettings"];
                    m_SimClock.Configure(fs.value("fixedTimestep", false),
                                         static_cast<Enjin::f32>(fs.value("physicsTicksPerSecond", 60u)));
                    m_ScriptSystem.SetExternalFixedClock(m_SimClock.IsEnabled());
                    m_ControllerSystem.SetExternalFixedClock(m_SimClock.IsEnabled());
                }
            } catch (...) {
                ENJIN_LOG_WARN(Player, "Manifest parse failed — using defaults");
            }
        }
        if (m_WindowTitle.empty()) m_WindowTitle = "TEGE Web Demo";
        if (m_WindowWidth == 0) m_WindowWidth = 1280;
        if (m_WindowHeight == 0) m_WindowHeight = 720;
        ENJIN_LOG_INFO(Player, "Game: %s (%ux%u)", m_WindowTitle.c_str(), m_WindowWidth, m_WindowHeight);

        // --- WebGPU renderer (async: adapter/device come back via callbacks) ---
        m_Renderer = std::make_unique<Enjin::Renderer::WebGPURenderer>();
        m_Renderer->InitializeAsync(nullptr, [this](bool ok) {
            if (!ok) {
                ENJIN_LOG_ERROR(Player, "WebGPU initialization failed");
                m_Renderer.reset();
                EM_ASM({
                    var el = document.getElementById('loading');
                    if (el) el.textContent = 'WebGPU initialization failed';
                });
                return;
            }
            ENJIN_LOG_INFO(Player, "WebGPU renderer initialized");
#if defined(ENJIN_WEBGPU_COMPUTE_SMOKETEST)
            // One-shot verification of the GPU-compute path (device is ready here).
            // Look for "[compute-smoke] PASS" in the browser console. Enable with
            // -DENJIN_WEBGPU_COMPUTE_SMOKETEST=ON; off by default so the player stays clean.
            Enjin::Renderer::RunWebGPUComputeSmokeTest(m_Renderer.get());
#endif
            FinishInitialize();
        });
    }

    void FinishInitialize() {
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
            c.addEventListener('click', function(){
                c.focus();
                // Re-lock only while the game wants capture (Web Demo menu
                // mode releases the cursor for the on-screen UI)
                if (Module.tegeWantPointerLock !== false) c.requestPointerLock();
            });
            c.addEventListener('contextmenu', function(e){ e.preventDefault(); });
            c.focus();
        });

        m_CameraController = std::make_unique<Enjin::Renderer::CameraController>(m_Camera.get());
        m_CameraController->SyncFromCamera();  // Sync yaw/pitch to match initial SetLookAt

        // --- ECS World ---
        m_World = std::make_unique<Enjin::ECS::World>();

        // --- Scripting ---
        if (m_ScriptEngine.Initialize()) {
            Enjin::Scripting::RegisterAllBindings(m_ScriptEngine.GetASEngine());
            m_ScriptEngine.SetWorld(m_World.get());
            m_ScriptEngine.SetScriptDirectory("scripts");
            // Web has no loose script files — sources come from the pak
            // (desktop wires this too, main.cpp "packed script loading").
            m_ScriptEngine.SetAssetReader(&m_AssetReader);
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
        m_CameraDirector.Reset();
        m_CameraDirector.SetEnabled(true);
        Enjin::Scripting::SetBindingsCameraDirector(&m_CameraDirector);
        m_RecordRewindSystem.SetWorld(m_World.get());
        Enjin::Scripting::SetBindingsRewindSystem(&m_RecordRewindSystem);
        m_InteractiveWaterSystem.SetEventBus(&m_EntityEventBus);
        m_ControllerSystem.SetInputActionMap(&m_InputMap);
        m_ControllerSystem.SetEnabled(true);
        m_TweenSystem.SetScriptEngine(&m_ScriptEngine);
        m_VisualScriptSystem.SetWorld(m_World.get());
        m_BehaviorTreeSystem.SetWorld(m_World.get());
        // AI, state machines, dialogue, and cinematics — previously desktop-only,
        // so NPCs froze and dialogue never advanced on web. Same wiring as the
        // desktop Player (main.cpp): AISystem drives navigation/perception,
        // StateMachineSystem runs FSMs (needs the script engine), DialogueSystem
        // advances conversations (wired to the event bus, quest, cinematic, and
        // save systems the web build has), CinematicSystem plays cutscenes.
        m_AISystem.SetWorld(m_World.get());
        m_AISystem.SetEnabled(true);
        m_StateMachineSystem.SetScriptEngine(&m_ScriptEngine);
        m_CinematicSystem.SetEnabled(true);
        m_DialogueSystem.SetEventBus(&m_EntityEventBus);
        // NOTE: the web player does not run InteractiveWaterSystem (no member); water
        // simulation/events are desktop-only for now. (Was erroneously wired here.)
        m_DialogueSystem.SetQuestSystem(&m_QuestSystem);
        m_DialogueSystem.SetCinematicSystem(&m_CinematicSystem);
        m_DialogueSystem.SetTieredSaveSystem(&m_TieredSaveSystem);
        // Accessibility: dialogue lines flow into the subtitle overlay (rendered
        // via the ImGui UI overlay -- unblocked by the web UI unification).
        m_DialogueSystem.SetSubtitleSystem(&m_SubtitleSystem);
        m_FootstepSystem.SetEnabled(true);
        m_TieredSaveSystem.LoadMeta();
        m_SimpleAudio.Initialize();
        m_SimpleAudio.SetWorld(m_World.get());
        m_WeatherSystem.Initialize();
        // Elemental fire/water/earth/air simulation. Its particle visuals aren't
        // rendered on web yet, but BuildFireLights feeds the renderer's transient
        // point lights (which the web PBR consumes), so fire lights the scene.
        m_ElementalSystem.Initialize(&m_WindSystem, &m_WeatherSystem);
        m_FireLights.reserve(Enjin::Effects::ElementalSystem::MAX_FIRE_LIGHTS);
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
        Enjin::Scripting::SetBindingsCinematicSystem(&m_CinematicSystem);
        Enjin::Scripting::SetBindingsInputActionMap(&m_InputMap);
        // Accessibility bindings — without these every Subtitle_/Announcer_/
        // Colorblind_/Accessibility_ script call is a silent no-op on web.
        Enjin::Scripting::SetBindingsSubtitles(&m_SubtitleSystem);
        Enjin::Scripting::SetBindingsAnnouncer(&m_Announcer);
        Enjin::Scripting::SetBindingsAccessibilitySettings(&m_AccessibilitySettings);
        Enjin::Scripting::SetBindingsAccessibilitySaveCallback([this]() {
            SaveWebAccessibilitySettings();
            SaveWebInputBindings();
        });
        Enjin::Scripting::SetBindingsAccessibilityApplyCallback([this]() { ApplyWebAccessibilitySettings(); });

        // Restore persisted accessibility settings + control bindings (IDBFS
        // mounted in StartBoot; the initial syncfs completed during the pak
        // fetch) and push them into every consumer.
        LoadWebAccessibilitySettings();
        LoadWebInputBindings();
        ApplyWebAccessibilitySettings();

        // Audio-visual sound indicators: ImGui draw lists don't exist on web,
        // so indicators are DOM elements (same pattern as the DOM HUD).
        m_SimpleAudio.SetOnSoundPlayed([this](const std::string& soundName) {
            if (!m_AccessibilitySettings.audioIndicatorsEnabled) return;
            EM_ASM({
                var label = UTF8ToString($0);
                var c = document.getElementById('enjin-audio-indicators');
                if (!c) {
                    c = document.createElement('div');
                    c.id = 'enjin-audio-indicators';
                    c.style.cssText = 'position:fixed;top:5%;right:2%;z-index:1000;pointer-events:none;font-family:sans-serif;font-size:14px;text-align:right;';
                    document.body.appendChild(c);
                }
                var d = document.createElement('div');
                d.textContent = '♪ ' + label;
                d.style.cssText = 'color:#66ccff;background:rgba(10,15,25,0.75);padding:2px 8px;margin-top:4px;border-radius:4px;transition:opacity 0.5s;';
                c.appendChild(d);
                setTimeout(function() { d.style.opacity = '0'; }, 1000);
                setTimeout(function() { d.remove(); }, 1600);
            }, soundName.c_str());
        });

        // --- Load scene ---
        bool sceneLoaded = false;

        // Option 1: From enjpak
        if (m_HasPack) {
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
                    m_SceneRenderSettings = serializer.GetRenderSettings();
                    if (m_RenderSystem) m_RenderSystem->SetSkybox(serializer.GetSkyboxConfig());
                    sceneLoaded = true;
                    m_CurrentWebScenePath = m_StartScene;
                    ENJIN_LOG_INFO(Player, "Loaded scene: %s", m_StartScene.c_str());
                    ShowWebContentWarnings(sceneStr);
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
                    m_SceneRenderSettings = serializer.GetRenderSettings();
                    if (m_RenderSystem) m_RenderSystem->SetSkybox(serializer.GetSkyboxConfig());
                    sceneLoaded = true;
                    ENJIN_LOG_INFO(Player, "Loaded loose scene: scene.enjin");
                    ShowWebContentWarnings(sceneStr);
                }
            }
        }

        // Option 3: Procedural demo fallback
        if (!sceneLoaded) {
            CreateDemoScene();
        }

        InitWebSceneRuntime();

        // --- RenderSystem (same system as desktop, uses abstract IRenderBackend) ---
        m_RenderSystem = m_World->RegisterSystem<Enjin::ECS::RenderSystem>(m_World.get(), m_Renderer.get());
        // NOTE (wiring audit 2026-08-28): SetBindingsRenderSystem is deliberately
        // NOT called here - ScriptBindings_Render.cpp is excluded from the web
        // build (WebStubs.cpp no-ops RegisterRenderBindings), so no Render_*
        // script functions exist on web. Porting them is a WebGPU-parity item.
        m_RenderSystem->SetCamera(m_Camera.get());
        m_RenderSystem->SetAssetReader(&m_AssetReader);
        m_RenderSystem->Initialize();
        m_RenderSystem->SetWindSystem(&m_WindSystem);   // wind -> lighting UBO (water waves)

        // GPU particles on web: same emitter component as desktop, driven each frame.
        m_Vegetation = std::make_unique<Enjin::Renderer::WebGPUVegetationSystem>();
        if (!m_Vegetation->Initialize(m_Renderer.get())) {
            ENJIN_LOG_WARN(Player, "Web vegetation unavailable (init failed) - flora volumes will not draw");
            m_Vegetation.reset();
        }
        m_Particles = std::make_unique<Enjin::Renderer::WebGPUParticleSystem>(m_Renderer.get());
        if (!m_Particles->Initialize()) {
            ENJIN_LOG_WARN(Player, "WebGPU particle system init failed — GPU particles disabled");
            m_Particles.reset();
        } else {
            // Draw particles inside the scene pass (real depth + scene tonemap). The
            // overlay draw in RenderUIOverlay stays as the fallback for the direct-to-
            // swapchain path and no-ops when this ran.
            m_RenderSystem->SetWebScenePassHook([this](void* scenePass) {
                // Vegetation first (opaque, depth-writing), then particles over it.
                if (m_Vegetation && m_Camera) {
                    Enjin::Math::Vector4 wv = m_WindSystem.GetWindVector();
                    m_Vegetation->SetWind(Enjin::Math::Vector3(wv.x, wv.y, wv.z), wv.w);
                    m_Vegetation->RenderScene(static_cast<WGPURenderPassEncoder>(scenePass),
                                              m_Camera->GetViewMatrix(), m_Camera->GetProjectionMatrix(),
                                              m_World.get());
                }
                if (m_Particles && m_Camera) {
                    m_Particles->RenderScene(static_cast<WGPURenderPassEncoder>(scenePass),
                                             m_Camera->GetViewMatrix(), m_Camera->GetProjectionMatrix());
                }
            });
        }

        // Apply scene render settings (ambient, shadows, etc.)
        m_SceneRenderSettings.rtEnabled = false;
        m_SceneRenderSettings.ApplyToRuntime(m_RenderSystem, nullptr);

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
        Enjin::Scripting::SetBindingsSubtitles(nullptr);
        Enjin::Scripting::SetBindingsAnnouncer(nullptr);
        Enjin::Scripting::SetBindingsAccessibilitySettings(nullptr);

        m_ScriptSystem.ShutdownAllScripts();
        m_ScriptSystem.SetEnabled(false);
        m_CoroutineScheduler.Clear();
        m_ScriptEventBus.Clear();
        m_ScriptEngine.Shutdown();

        m_TieredSaveSystem.SaveMeta();

        // RenderSystem is owned by World — just destroy the world
        m_World.reset();
        m_CameraController.reset();
        m_Camera.reset();
        m_Renderer.reset();
        m_AssetReader.Close();
    }

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

    void TogglePauseMenu() {
        if (m_Paused) { ClosePauseMenu(); return; }
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
        CloseOptionsMenu(false);
        if (SceneWantsMouseCapture()) Enjin::Input::SetMouseCaptured(true);
    }

    // The built-in options screen (parity with the PC build) — the same shared
    // UITemplates canvas. We swap the pause canvas for the options canvas while
    // staying paused, so gameplay keeps frozen. Back returns to the pause menu.
    void ShowOptionsMenu() {
        if (m_PauseMenuEntity != Enjin::ECS::INVALID_ENTITY && m_World->IsValid(m_PauseMenuEntity)) {
            m_World->DestroyEntity(m_PauseMenuEntity);
            m_PauseMenuEntity = Enjin::ECS::INVALID_ENTITY;
        }
        m_OptionsMenuEntity = m_World->CreateEntity();
        m_World->AddComponent<Enjin::ECS::NameComponent>(m_OptionsMenuEntity, "Options Menu UI");
        m_World->AddComponent<Enjin::GUI::UICanvasComponent>(m_OptionsMenuEntity,
            Enjin::GUI::UITemplates::CreateOptionsMenu());
        Enjin::Input::SetMouseCaptured(false);
    }

    void CloseOptionsMenu(bool reopenPause) {
        if (m_OptionsMenuEntity != Enjin::ECS::INVALID_ENTITY && m_World->IsValid(m_OptionsMenuEntity)) {
            m_World->DestroyEntity(m_OptionsMenuEntity);
        }
        m_OptionsMenuEntity = Enjin::ECS::INVALID_ENTITY;
        if (reopenPause && m_Paused) {
            m_PauseMenuEntity = m_World->CreateEntity();
            m_World->AddComponent<Enjin::ECS::NameComponent>(m_PauseMenuEntity, "Pause Menu UI");
            m_World->AddComponent<Enjin::GUI::UICanvasComponent>(m_PauseMenuEntity,
                Enjin::GUI::UITemplates::CreatePauseMenu());
        }
    }

    void Update(Enjin::f32 deltaTime) {
        // Global time scale (Time_SetScale): scales gameplay dt only.
        deltaTime *= Enjin::Scripting::GetTimeScale();

        if (!m_Initialized) return;
        m_LastDeltaTime = deltaTime;

        // Pause menu (UI unification: the same UITemplates pause canvas as any
        // platform). Escape toggles; gameplay freezes while the menu is up but
        // rendering + UI keep running so the menu is interactive.
        if (!m_AtMainMenu && Enjin::Input::IsKeyPressed(Enjin::KeyCode::Escape)) {
            if (m_OptionsMenuEntity != Enjin::ECS::INVALID_ENTITY) CloseOptionsMenu(true);  // back to pause
            else TogglePauseMenu();
        }

        m_SimpleAudio.Update(deltaTime);
        m_SimpleAudio.UpdateAudioSources(deltaTime);

        // Input must keep rotating its per-frame state while paused, or the
        // Escape pressed-edge (and every other key edge) would freeze.
        Enjin::Input::Update();
        if (m_Paused || m_AtMainMenu) {
            // World::Update still runs so deferred entity destroys flush (the
            // pause canvas removal on resume) -- gameplay systems stay skipped.
            m_World->Update(0.0f);
            return;
        }
        m_InputMap.Update(deltaTime);

        // Tick skeletal animators (desktop: main.cpp:818-823)
        for (auto entity : m_World->GetEntitiesWithComponent<Enjin::ECS::AnimatorComponent>()) {
            auto* anim = m_World->GetComponent<Enjin::ECS::AnimatorComponent>(entity);
            if (anim) anim->Update(deltaTime);
        }

        // Flush deferred entity destroys from previous frame
        m_World->Update(deltaTime);

        // WASM workaround: pass collider entities from this TU to physics backend
        // (GetEntitiesWithComponent returns 0 in JoltBackend.cpp due to WASM template static bug)
        if (m_Physics) {
            std::vector<Enjin::ECS::Entity> colliderEntities;
            for (auto e : m_World->GetEntitiesWithComponent<Enjin::ECS::BoxColliderComponent>()) colliderEntities.push_back(e);
            for (auto e : m_World->GetEntitiesWithComponent<Enjin::ECS::SphereColliderComponent>()) colliderEntities.push_back(e);
            for (auto e : m_World->GetEntitiesWithComponent<Enjin::ECS::CapsuleColliderComponent>()) colliderEntities.push_back(e);
            for (auto e : m_World->GetEntitiesWithComponent<Enjin::ECS::MeshColliderComponent>()) colliderEntities.push_back(e);
            m_Physics->SetColliderEntities(colliderEntities);
        }
        // T2 web-limit guard: the wasm heap is hard-capped (536MB link max).
        // Silent OOM near the ceiling is the worst failure mode on web, so
        // check every ~5s and warn loudly at 80%/95% - once each.
        if (++m_HeapCheckCounter >= 300) {
            m_HeapCheckCounter = 0;
            double used = static_cast<double>(emscripten_get_heap_size());
            constexpr double kMax = 536870912.0;   // keep in sync with MAXIMUM_MEMORY
            double frac = used / kMax;
            if (frac > 0.95 && !m_Warned95) {
                m_Warned95 = true;
                ENJIN_LOG_ERROR(Player, "WASM heap at %.0f%% of the %dMB ceiling - "
                                "out-of-memory crash imminent, reduce scene/assets",
                                frac * 100.0, 512);
            } else if (frac > 0.80 && !m_Warned80) {
                m_Warned80 = true;
                ENJIN_LOG_WARN(Player, "WASM heap at %.0f%% of the %dMB ceiling",
                               frac * 100.0, 512);
            }
        }

        // Script scene requests (Scene_LoadScene / Scene_Restart), consumed at
        // this safe point - no scripts on the stack, nothing recorded yet.
        {
            std::string reqScene;
            auto req = m_SceneManager.TakeSceneRequest(reqScene);
            if (req == Enjin::Scene::SceneManager::SceneRequest::Restart) {
                if (!m_CurrentWebScenePath.empty()) DoWebSceneTransition(m_CurrentWebScenePath);
                else ENJIN_LOG_WARN(Player, "Scene_Restart: no current scene to restart");
            } else if (req == Enjin::Scene::SceneManager::SceneRequest::Load) {
                const auto* entry = m_SceneManager.GetSceneByName(reqScene);
                if (entry) DoWebSceneTransition(entry->path);
                else ENJIN_LOG_WARN(Player, "Scene request '%s' not in scene list", reqScene.c_str());
            }
        }

        // ADR-0005: SimulationClock owns stepping (fixed tick + interpolation
        // when the project enables it; legacy variable step otherwise).
        m_ControllerSystem.PumpFrameInput();
        m_SimClock.Tick(m_World.get(), deltaTime, [this](Enjin::f32 stepDt) {
            if (m_Physics) m_Physics->Update(stepDt);
            if (m_Physics2D) m_Physics2D->Update(stepDt);
            // OnFixedUpdate lands exactly once per physics tick
            if (m_SimClock.IsEnabled()) m_ScriptSystem.FixedUpdate(stepDt);
            // Controllers on the tick: deterministic movement/jumps
            if (m_SimClock.IsEnabled()) m_ControllerSystem.Update(stepDt);
        });

        // Fixed-timestep projects tick controllers inside the SimClock loop
        if (!m_SimClock.IsEnabled()) m_ControllerSystem.Update(deltaTime);
        m_ControllerSystem.UpdateRealtime(deltaTime);  // bullet-time controllers

        // Only use fly camera if no character controller exists in the scene
        if (m_CameraController && m_CameraController->IsEnabled()) {
            bool hasPlayerController = !m_World->GetEntitiesWithComponent<
                Enjin::ECS::FirstPersonController>().empty()
                || !m_World->GetEntitiesWithComponent<
                Enjin::ECS::ThirdPersonController>().empty()
                || !m_World->GetEntitiesWithComponent<
                Enjin::ECS::Platformer2DController>().empty()
                || !m_World->GetEntitiesWithComponent<
                Enjin::ECS::TopDown2DController>().empty()
                || !m_World->GetEntitiesWithComponent<
                Enjin::ECS::TopDown3DController>().empty()
                || !m_World->GetEntitiesWithComponent<
                Enjin::ECS::VehicleController>().empty()
                || !m_World->GetEntitiesWithComponent<
                Enjin::ECS::SurfaceAlignedController>().empty();
            if (!hasPlayerController) {
                m_CameraController->Update(deltaTime);
            }
        }

        m_ScriptSystem.Update(deltaTime);
        m_CoroutineScheduler.EndOfFrame();
        Enjin::Scripting::FlushDeferredEntityDestroys();

        m_TweenSystem.Update(m_World.get(), deltaTime);
        m_VisualScriptSystem.Update(deltaTime);
        m_BehaviorTreeSystem.Update(deltaTime);

        // Ticking these is what makes NPCs move and dialogue advance on web
        // (order mirrors the desktop Player).
        m_StateMachineSystem.Update(m_World.get(), deltaTime);
        m_AISystem.Update(deltaTime);
        m_CinematicSystem.Update(m_World.get(), m_Camera.get(), deltaTime);
        // Virtual cameras + parallax were editor/desktop-only until the 2026-08-28
        // wiring audit; dormant when the scene has no vcams / parallax layers.
        // Interactive water: sim + water_enter events + regenerated surface mesh
        // (plain MeshComponents, which the web renderer draws). Mirrors desktop.
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
        m_RecordRewindSystem.Update(deltaTime);
        m_CameraDirector.Update(m_World.get(), m_Camera.get(), deltaTime);
        Enjin::ECS::ParallaxSystem::ApplyParallaxLayers(m_World.get(), deltaTime);
        m_DialogueSystem.Update(m_World.get(), deltaTime);

        m_QuestSystem.Update(m_World.get(), deltaTime);
        m_ObjectPool.Update(m_World.get(), deltaTime);
        m_EntityEventBus.ProcessDeferred();

        m_WindSystem.Update(deltaTime);
        m_WorldTime.Update(deltaTime);
        UpdateWeatherZones();
        if (m_Camera) {
            m_WeatherSystem.Update(deltaTime, m_Camera->GetPosition());
            m_RenderSystem->SetWeatherSkyBlend(m_WeatherSystem.GetRainIntensity(),
                                               m_WeatherSystem.GetSnowIntensity());
        }

        m_ParticleSystem.Update(deltaTime, m_World.get());

        // Elemental sim + fire lighting (particle visuals pending a web renderer
        // path; the lights show now). Mirrors the desktop Player.
        if (m_Camera && m_RenderSystem) {
            m_ElementalSystem.Update(m_World.get(), deltaTime, m_Camera->GetPosition());
            m_EffectsTime += deltaTime;
            m_ElementalSystem.BuildFireLights(m_EffectsTime, m_FireLights);
            m_RenderSystem->ClearTransientPointLights();
            for (const auto& fl : m_FireLights) {
                m_RenderSystem->AddTransientPointLight(fl.position, fl.range, fl.color, fl.intensity);
            }
        }

        // --- Gameplay loop (web parity with editor PlayMode / desktop Player) ---
        // Without this the web build never processes pickups, hazards, health,
        // trigger zones, or win/lose — games looked alive but weren't playable.
        {
            std::vector<Enjin::ECS::Entity> deferred;
            if (m_Physics) {
                Enjin::Gameplay::GameplayLoop::DispatchCollisionEvents3D(
                    m_World.get(), m_Physics.get(), &m_VisualScriptSystem, deltaTime, deferred);
            }
            m_FootstepSystem.Update(m_World.get(), deltaTime);
            m_SubtitleSystem.Update(deltaTime);
            m_Announcer.Update(deltaTime);

            // Live accessibility sync: scripts change these mid-game (Accessibility
            // Demo). Colorblind/brightness/contrast go through the WebGPU
            // post-process params; font scale through UISystem.
            if (m_RenderSystem) {
                m_RenderSystem->SetWebAccessibility(
                    static_cast<Enjin::u32>(m_AccessibilitySettings.colorblindMode),
                    m_AccessibilitySettings.colorblindStrength,
                    m_AccessibilitySettings.screenBrightness,
                    m_AccessibilitySettings.screenContrast);
                // Options preview split: alive while the colorblind slider was
                // touched recently (5 = colorblind in the post shader)
                if (m_ColorblindPreviewTimer > 0.0f) {
                    m_ColorblindPreviewTimer -= deltaTime;
                    m_RenderSystem->SetWebAccessibilityPreview(5u, 0.5f);
                } else {
                    m_RenderSystem->SetWebAccessibilityPreview(0u, 0.5f);
                }
            }
            m_UISystem.SetFontScale(m_AccessibilitySettings.fontScale);
            Enjin::Gameplay::GameplayLoop::CheckHazardOverlaps(m_World.get(), deltaTime, deferred);
            Enjin::Gameplay::GameplayLoop::CheckHazardOverlaps3D(m_World.get(), deferred);
            Enjin::Gameplay::GameplayLoop::CheckEnemyOverlaps2D(m_World.get(), deltaTime, deferred);
            Enjin::Gameplay::GameplayLoop::CheckPickupOverlaps3D(m_World.get(), deferred);
            Enjin::Gameplay::GameplayLoop::CheckPickupOverlaps2D(m_World.get(), deferred);
            Enjin::Gameplay::GameplayLoop::UpdateHealthSystems(m_World.get(), deltaTime, deferred);
            Enjin::Gameplay::GameplayLoop::UpdateTriggerZones(m_World.get());
            (void)Enjin::Gameplay::GameplayLoop::UpdateGameOverState(m_World.get(), deltaTime);
            Enjin::Gameplay::GameplayLoop::FlushDeferredDestroys(m_World.get(), deferred);

            // Game-over UI is the UICanvas screen GameplayLoop spawns — rendered by
            // the same UISystem as desktop (one source). Release pointer lock so the
            // player can click its "Play Again" button, and log the transition once.
            {
                static bool s_goLogged = false;
                if (!s_goLogged) {
                    for (auto goe : m_World->GetEntitiesWithComponent<Enjin::ECS::GameOverComponent>()) {
                        auto* go = m_World->GetComponent<Enjin::ECS::GameOverComponent>(goe);
                        if (go && go->triggered) {
                            s_goLogged = true;
                            Enjin::Input::SetMouseCaptured(false);
                            EM_ASM({ console.log('[GAMEOVER] ' + ($0 ? 'VICTORY' : 'DEFEAT')); }, go->won ? 1 : 0);
                            break;
                        }
                    }
                }
            }
        }

        // Sync Camera to the first CameraComponent entity (character controller drives the entity,
        // camera follows it). This mirrors what the desktop Player does.
        {
            const auto& camEntities = m_World->GetEntitiesWithComponent<Enjin::ECS::CameraComponent>();
            if (!camEntities.empty() && m_Camera) {
                auto camEntity = camEntities[0];
                auto* xf = m_World->GetComponent<Enjin::ECS::TransformComponent>(camEntity);
                auto* cc = m_World->GetComponent<Enjin::ECS::CameraComponent>(camEntity);
                if (xf && cc) {
                    Enjin::Math::Vector3 pos = xf->position;
                    Enjin::Math::Vector3 fwd = xf->rotation.GetForward();
                    Enjin::Math::Vector3 up = xf->rotation.GetUp();
                    m_Camera->SetLookAt(pos, pos + fwd, up);
                    if (cc->fieldOfView > 0.0f) {
                        Enjin::f32 aspect = static_cast<Enjin::f32>(m_Renderer->GetSwapChainWidth()) /
                                     static_cast<Enjin::f32>(std::max(m_Renderer->GetSwapChainHeight(), 1u));
                        // Options FOV override (desktop-menu parity): the slider
                        // wins over the authored camera when the player set it.
                        Enjin::f32 fov = (m_OptionsFov > 0.0f) ? m_OptionsFov : cc->fieldOfView;
                        m_Camera->SetPerspective(fov, aspect, cc->nearPlane, cc->farPlane);
                    }
                }
            }
        }
    }

    // Draw the game's authored UI (UICanvasComponent + HUDWidgetComponent) into the
    // swapchain via ImGui's WebGPU backend. This is the SAME UISystem code
    // the desktop player runs — one UI source, web/PC parity.
    // Touch controls overlay: virtual stick + jump button, drawn only after
    // the first touch so desktop browsers never see it.
    void RenderTouchOverlay() {
        auto st = Enjin::Input::GetTouchOverlay();
        if (!st.active) return;
        ImDrawList* dl = ImGui::GetForegroundDrawList();
        const ImU32 ring = IM_COL32(255, 255, 255, 70);
        const ImU32 fill = IM_COL32(255, 255, 255, 40);
        const ImU32 nub  = IM_COL32(255, 255, 255, 150);
        if (st.stickHeld) {
            dl->AddCircle(ImVec2(st.stickBaseX, st.stickBaseY), st.stickRadius, ring, 32, 3.0f);
            dl->AddCircleFilled(ImVec2(st.stickNubX, st.stickNubY), st.stickRadius * 0.4f, nub);
        }
        dl->AddCircleFilled(ImVec2(st.jumpX, st.jumpY), st.jumpR,
                            st.jumpHeld ? nub : fill);
        dl->AddCircle(ImVec2(st.jumpX, st.jumpY), st.jumpR, ring, 32, 3.0f);
        // W2 mobile gamepad: sprint (hold) + action buttons with labels
        const ImU32 label = IM_COL32(255, 255, 255, 190);
        dl->AddCircleFilled(ImVec2(st.sprintX, st.sprintY), st.sprintR,
                            st.sprintHeld ? nub : fill);
        dl->AddCircle(ImVec2(st.sprintX, st.sprintY), st.sprintR, ring, 32, 2.5f);
        dl->AddText(ImVec2(st.sprintX - st.sprintR * 0.55f, st.sprintY - 8.0f), label, "RUN");
        dl->AddCircleFilled(ImVec2(st.actionX, st.actionY), st.actionR,
                            st.actionHeld ? nub : fill);
        dl->AddCircle(ImVec2(st.actionX, st.actionY), st.actionR, ring, 32, 2.5f);
        dl->AddText(ImVec2(st.actionX - 4.0f, st.actionY - 8.0f), label, "E");
    }

    void RenderUIOverlay() {
        if (!m_Renderer || !m_Renderer->IsInitialized()) return;

        if (!m_WebImGuiInit) {
            m_WebImGuiInit = true;
            IMGUI_CHECKVERSION();
            ImGui::CreateContext();
            ImGuiIO& initIO = ImGui::GetIO();
            initIO.IniFilename = nullptr;  // no imgui.ini in the browser sandbox
            ImGui::StyleColorsDark();

            ImGui_ImplWGPU_InitInfo info;
            info.Device = m_Renderer->GetDevice();
            info.NumFramesInFlight = 3;
            info.RenderTargetFormat = Enjin::Renderer::GetPreferredSwapChainFormat();
            // Depth format must match the main pass so the UI pipeline is
            // pass-compatible (UI draws depth-test-always, no writes).
            info.DepthStencilFormat = Enjin::Renderer::GetDepthStencilFormat();
            if (!ImGui_ImplWGPU_Init(&info)) {
                ENJIN_LOG_ERROR(Player, "ImGui WebGPU backend init failed — game UI disabled");
                return;
            }
            // The UICanvas game-over screen's "Play Again" button — on web the
            // cleanest full restart is a page reload (fresh WASM + scene).
            m_UISystem.GetEventBus().Listen("gameover_restart",
                [](const Enjin::GUI::UIEventData&) {
                    EM_ASM({ location.reload(); });
                });

            // Pause menu buttons. Options now opens the built-in options canvas
            // (parity with the PC build); its controls are wired below. Events still
            // forward to scripts via the bridge, so games can also react.
            m_UISystem.GetEventBus().Listen("pause_resume",
                [this](const Enjin::GUI::UIEventData&) { ClosePauseMenu(); });
            m_UISystem.GetEventBus().Listen("pause_options",
                [this](const Enjin::GUI::UIEventData&) { ShowOptionsMenu(); });
            m_UISystem.GetEventBus().Listen("options_back",
                [this](const Enjin::GUI::UIEventData&) { CloseOptionsMenu(true); });
            m_UISystem.GetEventBus().Listen("options_fov",
                [this](const Enjin::GUI::UIEventData& e) {
                    // Slider is normalized; map to 40..120 degrees
                    m_OptionsFov = 40.0f + e.floatValue * 80.0f;
                });
            m_UISystem.GetEventBus().Listen("options_master_volume",
                [this](const Enjin::GUI::UIEventData& e) { m_SimpleAudio.SetMasterVolume(e.floatValue); });
            m_UISystem.GetEventBus().Listen("options_sfx_volume",
                [this](const Enjin::GUI::UIEventData& e) {
                    m_SimpleAudio.SetChannelVolume(Enjin::Audio::AudioChannel::SFX, e.floatValue);
                });
            m_UISystem.GetEventBus().Listen("options_music_volume",
                [this](const Enjin::GUI::UIEventData& e) {
                    m_SimpleAudio.SetChannelVolume(Enjin::Audio::AudioChannel::Music, e.floatValue);
                });
            m_UISystem.GetEventBus().Listen("options_fullscreen",
                [](const Enjin::GUI::UIEventData& e) {
                    if (e.boolValue) {
                        EM_ASM({ var c = Module.canvas || document.getElementById('canvas');
                                 if (c && c.requestFullscreen) c.requestFullscreen(); });
                    } else {
                        EM_ASM({ if (document.exitFullscreen) document.exitFullscreen(); });
                    }
                });
            // Accessibility toggles — write the runtime settings; colorblind/font
            // re-apply every frame, and we apply colorblind immediately so it shows
            // live even while the menu is up.
            m_UISystem.GetEventBus().Listen("options_reduced_motion",
                [this](const Enjin::GUI::UIEventData& e) { m_AccessibilitySettings.reducedMotion = e.boolValue; });
            m_UISystem.GetEventBus().Listen("options_subtitles",
                [this](const Enjin::GUI::UIEventData& e) { m_AccessibilitySettings.subtitlesEnabled = e.boolValue; });
            m_UISystem.GetEventBus().Listen("options_dyslexia",
                [this](const Enjin::GUI::UIEventData& e) {
                    m_AccessibilitySettings.dyslexiaFriendly = e.boolValue;
                    m_UISystem.SetFontScale(m_AccessibilitySettings.fontScale);
                });
            m_UISystem.GetEventBus().Listen("options_colorblind",
                [this](const Enjin::GUI::UIEventData& e) {
                    Enjin::u32 mode = static_cast<Enjin::u32>(e.floatValue * 8.0f + 0.5f);
                    if (mode > 8) mode = 8;
                    m_AccessibilitySettings.colorblindMode = static_cast<Enjin::Accessibility::ColorblindMode>(mode);
                    // Desktop-menu parity: dragging the slider splits the screen,
                    // left = without correction, right = with (fades after a beat)
                    m_ColorblindPreviewTimer = 1.5f;
                    if (m_RenderSystem) {
                        m_RenderSystem->SetWebAccessibility(mode, m_AccessibilitySettings.colorblindStrength,
                            m_AccessibilitySettings.screenBrightness, m_AccessibilitySettings.screenContrast);
                    }
                });
            // Authored MainMenu canvas buttons: hide (not destroy -- authored
            // content) and unfreeze gameplay.
            auto startGame = [this]() {
                if (!m_AtMainMenu) return;
                m_AtMainMenu = false;
                for (auto e : m_World->GetEntitiesWithComponent<Enjin::GUI::UICanvasComponent>()) {
                    auto* c = m_World->GetComponent<Enjin::GUI::UICanvasComponent>(e);
                    if (c && c->canvasName == "MainMenu") c->visible = false;
                }
                if (SceneWantsMouseCapture()) Enjin::Input::SetMouseCaptured(true);
            };
            m_UISystem.GetEventBus().Listen("menu_newgame",
                [startGame](const Enjin::GUI::UIEventData&) { startGame(); });
            m_UISystem.GetEventBus().Listen("menu_continue",
                [startGame](const Enjin::GUI::UIEventData&) { startGame(); });
            m_UISystem.GetEventBus().Listen("menu_quit",
                [](const Enjin::GUI::UIEventData&) { EM_ASM({ location.reload(); }); });
            m_UISystem.GetEventBus().Listen("pause_quit",
                [](const Enjin::GUI::UIEventData&) { EM_ASM({ location.reload(); }); });

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
            ENJIN_LOG_INFO(Player, "Web game UI initialized (ImGui WebGPU backend)");
        }

        Enjin::u32 w = m_Renderer->GetSwapChainWidth();
        Enjin::u32 h = m_Renderer->GetSwapChainHeight();
        if (w == 0 || h == 0) return;

        ImGuiIO& io = ImGui::GetIO();
        io.DisplaySize = ImVec2(static_cast<float>(w), static_cast<float>(h));
        io.DeltaTime = m_LastDeltaTime > 0.0f ? m_LastDeltaTime : 1.0f / 60.0f;
        // Mouse comes in CSS pixels from the HTML5 callbacks; the UI layout space
        // is swapchain pixels — scale by devicePixelRatio.
        Enjin::Math::Vector2 mp = Enjin::Input::GetMousePosition();
        io.MousePos = ImVec2(mp.x * m_LastDPR, mp.y * m_LastDPR);
        io.MouseDown[0] = Enjin::Input::IsMouseButtonDown(Enjin::MouseButton::Left);
        io.MouseDown[1] = Enjin::Input::IsMouseButtonDown(Enjin::MouseButton::Right);

        ImGui_ImplWGPU_NewFrame();
        ImGui::NewFrame();

        // Authored UICanvas UI (menus, dialogue boxes, in-game panels, HUD).
        // HUDSystem is retired: hudWidget data migrates to UICanvas on load,
        // so this is the ONE UI path on web too (camera = world-space tags).
        m_UISystem.Update(m_World.get(), static_cast<Enjin::f32>(w), static_cast<Enjin::f32>(h),
                          io.DeltaTime, 0.0f, 0.0f, m_Camera.get());
        // Subtitle overlay (accessibility) -- same draw code as desktop
        m_SubtitleSystem.RenderOverlay(w, h);
        // Screen reader status bar (announcements also speak via Web Speech API)
        m_Announcer.RenderStatusBar();

        ImGui::Render();
        ImDrawData* drawData = ImGui::GetDrawData();
        bool haveUI = drawData && drawData->TotalVtxCount > 0;
        // Draw GPU particles into the overlay pass (which carries the scene color +
        // depth), then the UI on top. Particles depth-test against the scene (occluded
        // by nearer geometry) but don't write depth.
        if (haveUI || m_Particles) {
            WGPURenderPassEncoder pass = m_Renderer->GetOrBeginUIOverlayPass();
            if (pass) {
                if (m_Particles && m_Camera) {
                    m_Particles->Render(pass, m_Camera->GetViewMatrix(), m_Camera->GetProjectionMatrix());
                }
                if (haveUI) ImGui_ImplWGPU_RenderDrawData(drawData, pass);
            }
        }
    }

    // Spawn from every GPUParticleEmitterComponent (burst + continuous), mirroring
    // the desktop RenderSystem::TickGPUEmitters so the same emitter authors both.
    void DriveParticles(Enjin::f32 dt) {
        if (!m_Particles || !m_World) return;
#if defined(ENJIN_WEBGPU_COMPUTE_SMOKETEST)
        // Debug fountain (gated with the compute smoke test): a Fire emitter 4 units in
        // FRONT of the camera so it's unmissable in any scene, verifying the particle
        // sim + draw path without authoring an emitter. Off in shipping builds.
        if (m_Camera) {
            Enjin::Math::Vector3 at = m_Camera->GetPosition() + m_Camera->GetForward() * 4.0f;
            m_Particles->SpawnWithParams(40, at, Enjin::Math::Vector3(0.0f, 1.0f, 0.0f),
                Enjin::Effects::PresetSpawnParams(Enjin::Effects::GPUParticlePreset::Fire));
            static bool s_loggedFountain = false;
            if (!s_loggedFountain) {
                s_loggedFountain = true;
                ENJIN_LOG_INFO(Player, "[particle-debug] Fire fountain spawning in front of the camera");
            }
        }
#endif
        // Weather precipitation: rain/snow ride the GPU particle pool. The desktop
        // instanced weather pass is a stub on the WebGPU backend, so until now the
        // weather SIMULATED here but drew nothing. Spawned in a box above the
        // camera so it always falls through the view; wind slants the fall.
        if (m_Camera) {
            const Enjin::f32 rain = m_WeatherSystem.GetRainIntensity();
            const Enjin::f32 snow = m_WeatherSystem.GetSnowIntensity();
            const Enjin::Math::Vector3 wind3 = m_WeatherSystem.GetWindDirection() * m_WeatherSystem.GetWindStrength();
            const Enjin::Math::Vector3 spawnAt = m_Camera->GetPosition() + Enjin::Math::Vector3(wind3.x * -2.0f, 11.0f, wind3.z * -2.0f);
            if (rain > 0.01f) {
                m_RainAccum += rain * 700.0f * dt;
                Enjin::u32 n = static_cast<Enjin::u32>(m_RainAccum);
                if (n > 0) {
                    m_RainAccum -= static_cast<Enjin::f32>(n);
                    if (n > 1024) n = 1024;
                    Enjin::Effects::ParticleSpawnParams p;
                    p.color = {0.62f, 0.72f, 0.85f, 0.5f};
                    p.size = 0.16f; p.sizeJitter = 0.3f;
                    p.lifetime = 1.6f; p.speed = 14.0f; p.spread = 0.03f;
                    p.gravityScale = 2.2f; p.drag = 0.0f;
                    p.sprite = 3; p.softness = 0.4f;      // streak card
                    p.fixedRotation = 0.0f;               // vertical streaks, not confetti
                    p.collide = false;
                    Enjin::Math::Vector3 dir(wind3.x * 0.06f, -1.0f, wind3.z * 0.06f);
                    m_Particles->SpawnWithParams(n, spawnAt, dir, p, 4 /*box*/, 13.0f);
                }
            }
            if (snow > 0.01f) {
                m_SnowAccum += snow * 320.0f * dt;
                Enjin::u32 n = static_cast<Enjin::u32>(m_SnowAccum);
                if (n > 0) {
                    m_SnowAccum -= static_cast<Enjin::f32>(n);
                    if (n > 1024) n = 1024;
                    Enjin::Effects::ParticleSpawnParams p;
                    p.color = {1.0f, 1.0f, 1.0f, 0.85f};
                    p.size = 0.07f; p.sizeJitter = 0.5f;
                    p.lifetime = 6.0f; p.speed = 0.9f; p.spread = 0.5f;
                    p.gravityScale = 0.12f; p.drag = 1.4f;
                    p.sprite = 1; p.softness = 0.7f;
                    p.collide = false;
                    Enjin::Math::Vector3 dir(wind3.x * 0.2f, -1.0f, wind3.z * 0.2f);
                    m_Particles->SpawnWithParams(n, spawnAt, dir, p, 4 /*box*/, 13.0f);
                }
            }
        }

        // Elemental fire: every fire light source gets a small continuous Fire
        // plume (the desktop elemental particle pass is also a web stub).
        if (!m_FireLights.empty()) {
            m_FireAccum += 45.0f * dt;
            Enjin::u32 per = static_cast<Enjin::u32>(m_FireAccum);
            if (per > 0) {
                m_FireAccum -= static_cast<Enjin::f32>(per);
                if (per > 6) per = 6;
                Enjin::Effects::ParticleSpawnParams fp =
                    Enjin::Effects::PresetSpawnParams(Enjin::Effects::GPUParticlePreset::Fire);
                fp.collide = false;
                for (const auto& fl : m_FireLights) {
                    fp.size = 0.28f + 0.12f * fl.intensity;
                    m_Particles->SpawnWithParams(per, fl.position, Enjin::Math::Vector3(0.0f, 1.0f, 0.0f),
                                                 fp, 1 /*sphere*/, 0.18f);
                }
            }
        }

        for (Enjin::ECS::Entity e : m_World->GetEntitiesWithComponent<Enjin::ECS::GPUParticleEmitterComponent>()) {
            auto* em = m_World->GetComponent<Enjin::ECS::GPUParticleEmitterComponent>(e);
            if (!em) continue;
            Enjin::Math::Vector3 pos(0.0f);
            if (auto* t = m_World->GetComponent<Enjin::ECS::TransformComponent>(e)) pos = t->position;
            const Enjin::u8 shape = static_cast<Enjin::u8>(em->shape);
            if (em->burstNow && em->burstCount > 0) {
                m_Particles->SpawnWithParams(em->burstCount, pos, em->direction, em->ResolveParams(), shape, em->shapeSize);
                em->burstNow = false;
            }
            if (em->emitting && em->spawnRate > 0.0f) {
                em->accumulator += em->spawnRate * dt;
                Enjin::u32 n = static_cast<Enjin::u32>(em->accumulator);
                if (n > 0) {
                    em->accumulator -= static_cast<Enjin::f32>(n);
                    if (n > 4096) n = 4096;
                    m_Particles->SpawnWithParams(n, pos, em->direction, em->ResolveParams(), shape, em->shapeSize);
                }
            }
        }
    }

    void Render() {
        if (!m_Initialized || !m_RenderSystem) return;

        m_RenderSystem->FlushPendingChanges();

        // BeginFrame + render pass + EndFrame via RenderSystem::Update
        if (!m_Renderer->BeginFrameWebGPU()) return;
        // GPU particles: spawn from emitters, then run the sim in a compute pass
        // BEFORE any render pass opens (a compute pass can't overlap a render pass).
        if (m_Particles) {
            DriveParticles(m_LastDeltaTime);
            static std::vector<Enjin::Effects::ParticleColliderShape> s_ParticleColliders;
            Enjin::Effects::GatherParticleColliders(m_World.get(), s_ParticleColliders);
            m_Particles->SetColliders(s_ParticleColliders);
            m_Particles->Simulate(m_LastDeltaTime, m_ParticleFrame++, Enjin::Math::Vector3(0.0f, 0.0f, 0.0f));
        }
        m_RenderSystem->FlushPendingChanges();
        m_RenderSystem->Update(0.0f);  // deltaTime handled separately in Update()
        // Fallback clear ONLY if nothing rendered to the swapchain (e.g. RenderSystem
        // early-out) — running it unconditionally wipes the post-processed frame
        if (!m_Renderer->SwapchainWrittenThisFrame()) {
            m_Renderer->BeginMainRenderPass();
        }

        // Authored game UI (UICanvas + HUD widgets) via ImGui — same systems and
        // draw code as the desktop player (UI unification: one source, parity).
        RenderUIOverlay();
        RenderTouchOverlay();

        m_Renderer->EndFrame();

        // Sync CameraController after the first camera entity override
        if (!m_CameraControllerSynced && m_CameraController) {
            m_CameraController->SyncFromCamera();
            m_CameraControllerSynced = true;
        }
    }

    void OnCanvasResize(int w, int h, float dpr) {
        if (w <= 0 || h <= 0) return;
        m_LastDPR = dpr > 0.0f ? dpr : 1.0f;
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
    void SetColorblindMode(Enjin::u32 mode, float strength, float brightness, float contrast) {
        if (m_RenderSystem) m_RenderSystem->SetWebAccessibility(mode, strength, brightness, contrast);
    }

private:
    // Zone-driven weather on web (mirrors desktop Player UpdateWeatherZones, minus
    // the pieces web can't render: season state has no web TreeRenderer, water
    // freeze has no web water surface). Feeds rain/snow intensity, fog, and the
    // wind override; the existing GPU-particle precip spawn reads those back.
    // Custom precip sprite textures are Phase 3 (WebGPU has no bindless path yet).
    void UpdateWeatherZones() {
        using namespace Enjin;
        if (!m_World || !m_Camera || !m_RenderSystem) return;

        Math::Vector3 camPos = m_Camera->GetPosition();

        ECS::WeatherZoneComponent* activeWeatherZone = nullptr;
        i32 bestWeatherPriority = INT_MIN;
        for (ECS::Entity entity : m_World->GetEntitiesWithComponent<ECS::WeatherZoneComponent>()) {
            auto* zone = m_World->GetComponent<ECS::WeatherZoneComponent>(entity);
            auto* zoneTransform = m_World->GetComponent<ECS::TransformComponent>(entity);
            if (zone && zoneTransform && zone->priority > bestWeatherPriority) {
                if (zone->ContainsPoint(zoneTransform->position, camPos)) {
                    activeWeatherZone = zone;
                    bestWeatherPriority = zone->priority;
                }
            }
        }

        ECS::TemperatureZoneComponent* activeTempZone = nullptr;
        i32 bestTempPriority = INT_MIN;
        for (ECS::Entity entity : m_World->GetEntitiesWithComponent<ECS::TemperatureZoneComponent>()) {
            auto* zone = m_World->GetComponent<ECS::TemperatureZoneComponent>(entity);
            auto* zoneTransform = m_World->GetComponent<ECS::TransformComponent>(entity);
            if (zone && zoneTransform && zone->priority > bestTempPriority) {
                if (zone->ContainsPoint(zoneTransform->position, camPos)) {
                    activeTempZone = zone;
                    bestTempPriority = zone->priority;
                }
            }
        }

        bool isRain = false;
        bool hasPrecip = false;
        if (activeWeatherZone && activeWeatherZone->weatherType > 0) {
            Effects::WeatherType wType = static_cast<Effects::WeatherType>(activeWeatherZone->weatherType);
            hasPrecip = (activeWeatherZone->weatherType == 2 || activeWeatherZone->weatherType == 3 ||
                         activeWeatherZone->weatherType == 4 || activeWeatherZone->weatherType == 6);

            if (hasPrecip && activeTempZone) {
                f32 temp = activeTempZone->temperature;
                f32 baseInt = (activeWeatherZone->weatherType == 4)
                    ? activeWeatherZone->snowIntensity : activeWeatherZone->rainIntensity;
                if (temp <= 0.0f) {
                    m_WeatherSystem.SetWeather(Effects::WeatherType::Snow, 0.1f);
                    m_WeatherSystem.SetRainIntensity(0.0f);
                    m_WeatherSystem.SetSnowIntensity(baseInt);
                } else if (temp <= 5.0f) {
                    f32 blend = temp / 5.0f;
                    m_WeatherSystem.SetWeather(wType, 0.1f);
                    m_WeatherSystem.SetRainIntensity(baseInt * blend);
                    m_WeatherSystem.SetSnowIntensity(baseInt * (1.0f - blend));
                    isRain = (blend > 0.5f);
                } else {
                    m_WeatherSystem.SetWeather(
                        activeWeatherZone->weatherType == 6 ? Effects::WeatherType::Storm : Effects::WeatherType::Rain, 0.1f);
                    m_WeatherSystem.SetRainIntensity(baseInt);
                    m_WeatherSystem.SetSnowIntensity(0.0f);
                    isRain = true;
                }
            } else {
                m_WeatherSystem.SetWeather(wType, 0.1f);
                if (activeWeatherZone->weatherType == 4) {
                    m_WeatherSystem.SetRainIntensity(0.0f);
                    m_WeatherSystem.SetSnowIntensity(activeWeatherZone->snowIntensity);
                } else {
                    m_WeatherSystem.SetRainIntensity(activeWeatherZone->rainIntensity);
                    m_WeatherSystem.SetSnowIntensity(0.0f);
                    isRain = true;
                }
            }

            m_WeatherSystem.SetFogDensity(activeWeatherZone->fogDensity);
            m_WeatherSystem.SetFogColor(activeWeatherZone->fogColor);
            m_WeatherSystem.SetFogStart(activeWeatherZone->fogStart);
            m_WeatherSystem.SetFogEnd(activeWeatherZone->fogEnd);
            m_WindSystem.SetZoneOverride(activeWeatherZone->windDirection, activeWeatherZone->windStrength);
            m_WeatherSystem.SetWindDirection(activeWeatherZone->windDirection);
            m_WeatherSystem.SetWindStrength(activeWeatherZone->windStrength);

            m_RenderSystem->SetFogParams(activeWeatherZone->fogDensity,
                                         activeWeatherZone->fogStart, activeWeatherZone->fogEnd, 0.1f);
            m_RenderSystem->SetFogColor(activeWeatherZone->fogColor);
        } else {
            m_WindSystem.ClearZoneOverride();
            f32 rain = m_WeatherSystem.GetRainIntensity();
            f32 snow = m_WeatherSystem.GetSnowIntensity();
            hasPrecip = (rain > 0.01f || snow > 0.01f);
            isRain = rain >= snow;
            m_RenderSystem->SetFogParams(m_WeatherSystem.GetFogDensity(),
                                         m_WeatherSystem.GetFogStart(), m_WeatherSystem.GetFogEnd(), 0.1f);
            m_RenderSystem->SetFogColor(m_WeatherSystem.GetFogColor());
        }

        m_RenderSystem->SetRainActive(hasPrecip && isRain);
    }

    // Web accessibility persistence: /saves/accessibility.json on the IDBFS
    // mount (StartBoot). Desktop uses accessibility.json next to the exe.
    // Bring a freshly loaded scene fully live. Called after the initial boot
    // load AND after every mid-game transition, so each scene comes up the same
    // way (the desktop player's InitSceneRuntime pattern).
    void InitWebSceneRuntime() {
        // Force initial physics sync while colliders still exist
        // (GetEntitiesWithComponent<ColliderComponent> returns 0 after World::Update on WASM)
        if (m_Physics) m_Physics->Update(0.001f);
        if (m_Physics2D) m_Physics2D->Update(0.001f);

        // Log loaded entities + collider diagnostic
        if (m_World) {
            auto boxCol = m_World->GetEntitiesWithComponent<Enjin::ECS::BoxColliderComponent>();
            auto capsuleCol = m_World->GetEntitiesWithComponent<Enjin::ECS::CapsuleColliderComponent>();
            printf("[SCENE] box_colliders=%zu capsule_colliders=%zu\n", boxCol.size(), capsuleCol.size());
        }

        // --- Post-scene-load system initialization ---
        // VisualScript full init (desktop: main.cpp:1696-1704)
        m_VisualScriptSystem.SetPhysics(m_Physics.get());
        m_VisualScriptSystem.SetPhysics2D(m_Physics2D.get());
        m_VisualScriptSystem.SetScriptEngine(&m_ScriptEngine);
        Enjin::Effects::TreeRenderer::GenerateAllColliders(m_World.get());
        m_VisualScriptSystem.SetDialogue(&m_DialogueSystem);
        m_VisualScriptSystem.Initialize();
        m_BehaviorTreeSystem.Initialize();

        // Start auto-play tweens (desktop: main.cpp:1706)
        m_TweenSystem.PlayAll(m_World.get());

        // Enable and initialize scripts (desktop: main.cpp:1709-1712)
        m_ScriptSystem.SetEnabled(true);
        m_ScriptSystem.InitializeAllScripts();

        // Set game camera entity for controller system (desktop: main.cpp:1668-1674)
        {
            auto cameras = Enjin::ECS::CameraManager::GetAllActiveCameras(m_World.get());
            if (!cameras.empty()) {
                m_ControllerSystem.SetGameCameraEntity(cameras[0]);
            }
        }

        // Authored "MainMenu" canvas takes over the boot flow (one UI source):
        // gameplay stays frozen and the mouse free until New Game / Continue.
        for (auto e : m_World->GetEntitiesWithComponent<Enjin::GUI::UICanvasComponent>()) {
            auto* c = m_World->GetComponent<Enjin::GUI::UICanvasComponent>(e);
            if (c && c->visible && c->canvasName == "MainMenu") { m_AtMainMenu = true; break; }
        }

        // Capture mouse for look-around if any player controller exists
        if (!m_AtMainMenu && SceneWantsMouseCapture()) {
            Enjin::Input::SetMouseCaptured(true);
        }
    }

    // Mid-game scene swap, called ONLY from the safe point at the top of
    // Update (no scripts on the stack, before any rendering). Mirrors the
    // desktop player's DoFlowTransition.
    void DoWebSceneTransition(const std::string& scenePath) {
        ENJIN_LOG_INFO(Player, "Scene transition: '%s'", scenePath.c_str());
        if (m_Renderer) m_Renderer->WaitForAllFrames();
        m_ScriptSystem.ShutdownAllScripts();
        Enjin::Scripting::ClearBindingsEventListeners();
        if (m_RenderSystem) m_RenderSystem->OnSceneClear();

        auto sceneData = m_AssetReader.ReadFile(scenePath);
        if (sceneData.empty()) {
            ENJIN_LOG_ERROR(Player, "Scene transition: cannot read '%s' from pack", scenePath.c_str());
            return;
        }
        std::string sceneStr(sceneData.begin(), sceneData.end());
        Enjin::Scene::SceneSerializer serializer(m_World.get());
        serializer.LoadFromString(sceneStr, true);   // clears the world first
        m_SceneRenderSettings = serializer.GetRenderSettings();
        if (m_RenderSystem) m_RenderSystem->SetSkybox(serializer.GetSkyboxConfig());
        m_CurrentWebScenePath = scenePath;
        m_SimClock.Reset();
        m_WeatherSystem.SetRainIntensity(0.0f);
        m_WeatherSystem.SetSnowIntensity(0.0f);
        m_AtMainMenu = false;   // re-detected in InitWebSceneRuntime if the new scene has one

        InitWebSceneRuntime();
    }

    void LoadWebAccessibilitySettings() {
        std::ifstream f("/saves/accessibility.json");
        if (!f.is_open()) return;
        std::stringstream ss;
        ss << f.rdbuf();
        try {
            auto j = nlohmann::json::parse(ss.str());
            auto& s = m_AccessibilitySettings;
            if (j.contains("colorblindMode")) {
                Enjin::u32 v = j["colorblindMode"].get<Enjin::u32>();
                if (v <= 8) s.colorblindMode = static_cast<Enjin::Accessibility::ColorblindMode>(v);
            }
            if (j.contains("colorblindStrength")) s.colorblindStrength = j["colorblindStrength"].get<Enjin::f32>();
            if (j.contains("screenBrightness")) s.screenBrightness = j["screenBrightness"].get<Enjin::f32>();
            if (j.contains("screenContrast")) s.screenContrast = j["screenContrast"].get<Enjin::f32>();
            if (j.contains("fontScale")) s.fontScale = j["fontScale"].get<Enjin::f32>();
            if (j.contains("reducedMotion")) s.reducedMotion = j["reducedMotion"].get<bool>();
            if (j.contains("subtitlesEnabled")) s.subtitlesEnabled = j["subtitlesEnabled"].get<bool>();
            if (j.contains("screenReaderEnabled")) s.screenReaderEnabled = j["screenReaderEnabled"].get<bool>();
            if (j.contains("audioIndicatorsEnabled")) s.audioIndicatorsEnabled = j["audioIndicatorsEnabled"].get<bool>();
            if (j.contains("dyslexiaFriendly")) s.dyslexiaFriendly = j["dyslexiaFriendly"].get<bool>();
            if (j.contains("dwellClickEnabled")) s.dwellClickEnabled = j["dwellClickEnabled"].get<bool>();
            if (j.contains("dwellClickTime")) s.dwellClickTime = j["dwellClickTime"].get<Enjin::f32>();
            if (j.contains("switchAccessEnabled")) s.switchAccessEnabled = j["switchAccessEnabled"].get<bool>();
            if (j.contains("switchScanSpeed")) s.switchScanSpeed = j["switchScanSpeed"].get<Enjin::f32>();
            if (j.contains("stickyDragEnabled")) s.stickyDragEnabled = j["stickyDragEnabled"].get<bool>();
            // Motion + input options (were desktop-only; audit 2026-08-28)
            if (j.contains("disableScreenShake")) s.disableScreenShake = j["disableScreenShake"].get<bool>();
            if (j.contains("disableFOVEffects")) s.disableFOVEffects = j["disableFOVEffects"].get<bool>();
            if (j.contains("disableFlashingLights")) s.disableFlashingLights = j["disableFlashingLights"].get<bool>();
            if (j.contains("mouseSensitivity")) s.mouseSensitivity = j["mouseSensitivity"].get<Enjin::f32>();
            if (j.contains("invertMouseY")) s.invertMouseY = j["invertMouseY"].get<bool>();
            if (j.contains("sprintMode")) s.sprintMode = j["sprintMode"].get<Enjin::u32>();
            if (j.contains("crouchMode")) s.crouchMode = j["crouchMode"].get<Enjin::u32>();
            // Subtitle sub-options (Apply read them but they never loaded on web)
            if (j.contains("closedCaptionsEnabled")) s.closedCaptionsEnabled = j["closedCaptionsEnabled"].get<bool>();
            if (j.contains("subtitleFontSize")) s.subtitleFontSize = j["subtitleFontSize"].get<Enjin::f32>();
            if (j.contains("subtitleBgOpacity")) s.subtitleBgOpacity = j["subtitleBgOpacity"].get<Enjin::f32>();
            if (j.contains("subtitleSpeakerNames")) s.subtitleSpeakerNames = j["subtitleSpeakerNames"].get<bool>();
            if (j.contains("subtitleDirectionIndicators")) s.subtitleDirectionIndicators = j["subtitleDirectionIndicators"].get<bool>();
            ENJIN_LOG_INFO(Player, "Loaded accessibility settings from /saves/accessibility.json");
        } catch (const std::exception& e) {
            ENJIN_LOG_WARN(Player, "Failed to parse /saves/accessibility.json: %s", e.what());
        }
    }

    void SaveWebAccessibilitySettings() {
        try {
            const auto& s = m_AccessibilitySettings;
            nlohmann::json j;
            j["colorblindMode"] = static_cast<Enjin::u32>(s.colorblindMode);
            j["colorblindStrength"] = s.colorblindStrength;
            j["screenBrightness"] = s.screenBrightness;
            j["screenContrast"] = s.screenContrast;
            j["fontScale"] = s.fontScale;
            j["reducedMotion"] = s.reducedMotion;
            j["subtitlesEnabled"] = s.subtitlesEnabled;
            j["screenReaderEnabled"] = s.screenReaderEnabled;
            j["audioIndicatorsEnabled"] = s.audioIndicatorsEnabled;
            j["dyslexiaFriendly"] = s.dyslexiaFriendly;
            j["dwellClickEnabled"] = s.dwellClickEnabled;
            j["dwellClickTime"] = s.dwellClickTime;
            j["switchAccessEnabled"] = s.switchAccessEnabled;
            j["switchScanSpeed"] = s.switchScanSpeed;
            j["stickyDragEnabled"] = s.stickyDragEnabled;
            j["disableScreenShake"] = s.disableScreenShake;
            j["disableFOVEffects"] = s.disableFOVEffects;
            j["disableFlashingLights"] = s.disableFlashingLights;
            j["mouseSensitivity"] = s.mouseSensitivity;
            j["invertMouseY"] = s.invertMouseY;
            j["sprintMode"] = s.sprintMode;
            j["crouchMode"] = s.crouchMode;
            j["closedCaptionsEnabled"] = s.closedCaptionsEnabled;
            j["subtitleFontSize"] = s.subtitleFontSize;
            j["subtitleBgOpacity"] = s.subtitleBgOpacity;
            j["subtitleSpeakerNames"] = s.subtitleSpeakerNames;
            j["subtitleDirectionIndicators"] = s.subtitleDirectionIndicators;
            {
                std::ofstream f("/saves/accessibility.json");
                f << j.dump(2);
            }
            // Flush MEMFS -> IndexedDB so the settings survive a reload
            EM_ASM({
                FS.syncfs(false, function(err) {
                    if (err) console.warn('[A11Y] settings persist error', err);
                });
            });
            ENJIN_LOG_INFO(Player, "Saved accessibility settings to /saves/accessibility.json");
        } catch (const std::exception& e) {
            ENJIN_LOG_WARN(Player, "Failed to save /saves/accessibility.json: %s", e.what());
        }
    }

    // Web twin of the desktop content-warning overlay (ContentWarning.cpp is
    // ImGui-based, unavailable here) — a DOM overlay dismissed by click.
    void ShowWebContentWarnings(const std::string& sceneStr) {
        try {
            auto sceneJson = nlohmann::json::parse(sceneStr);
            const char* key = sceneJson.contains("accessibility") ? "accessibility"
                            : (sceneJson.contains("contentWarnings") ? "contentWarnings" : nullptr);
            if (!key) return;
            auto& cw = sceneJson[key];
            Enjin::u32 flags = cw.contains("flags") ? cw["flags"].get<Enjin::u32>() : 0u;

            auto escapeHtml = [](const std::string& in) {
                std::string out;
                out.reserve(in.size());
                for (char c : in) {
                    if (c == '<') out += "&lt;";
                    else if (c == '>') out += "&gt;";
                    else if (c == '&') out += "&amp;";
                    else out += c;
                }
                return out;
            };

            static const char* kWarningNames[8] = {
                "Flashing Lights", "Rapid Motion", "Violence", "Heights",
                "Loud Sounds", "Spiders", "Gore", "Drowning"
            };
            std::string items;
            for (int i = 0; i < 8; ++i) {
                if (flags & (1u << i)) {
                    items += "<li>";
                    items += kWarningNames[i];
                    items += "</li>";
                }
            }
            if (cw.contains("customWarnings") && cw["customWarnings"].is_array()) {
                for (const auto& w : cw["customWarnings"]) {
                    std::string s = w.get<std::string>();
                    if (!s.empty()) items += "<li>" + escapeHtml(s) + "</li>";
                }
            }
            if (items.empty()) return;

            EM_ASM({
                var items = UTF8ToString($0);
                var o = document.createElement('div');
                o.id = 'enjin-content-warning';
                o.style.cssText = 'position:fixed;inset:0;background:rgba(5,7,12,0.92);color:#fff;z-index:2000;display:flex;flex-direction:column;align-items:center;justify-content:center;font-family:sans-serif;cursor:pointer;';
                o.innerHTML = '<h2 style="color:#e8c14a;margin:0 0 8px 0;">Content Warning</h2>' +
                    '<p style="color:#aaa;margin:0 0 12px 0;">This game contains the following content:</p>' +
                    '<ul style="text-align:left;line-height:1.6;">' + items + '</ul>' +
                    '<p style="color:#888;margin-top:16px;">Click or press any key to continue</p>';
                var dismiss = function() {
                    o.remove();
                    document.removeEventListener('keydown', dismiss);
                };
                o.addEventListener('click', dismiss);
                document.addEventListener('keydown', dismiss);
                document.body.appendChild(o);
            }, items.c_str());
        } catch (const std::exception&) {
            // Content warnings are best-effort; a parse failure is non-fatal
        }
    }

    // Player-remapped controls persist alongside the accessibility settings
    void LoadWebInputBindings() {
        std::ifstream f("/saves/bindings.json");
        if (!f.is_open()) return;
        std::stringstream ss;
        ss << f.rdbuf();
        if (m_InputMap.FromJson(ss.str())) {
            ENJIN_LOG_INFO(Player, "Loaded control bindings from /saves/bindings.json");
        }
    }

    void SaveWebInputBindings() {
        {
            std::ofstream f("/saves/bindings.json");
            f << m_InputMap.ToJson();
        }
        EM_ASM({
            FS.syncfs(false, function(err) {
                if (err) console.warn('[BINDINGS] persist error', err);
            });
        });
    }

    // Push settings into consumers that only read them when pushed (mirror of
    // the desktop player's ApplyAccessibilitySettings)
    void ApplyWebAccessibilitySettings() {
        auto& s = m_AccessibilitySettings;
        auto& subConfig = m_SubtitleSystem.GetConfig();
        subConfig.enabled = s.subtitlesEnabled;
        subConfig.captionsEnabled = s.closedCaptionsEnabled;
        subConfig.fontSize = s.subtitleFontSize;
        subConfig.backgroundOpacity = s.subtitleBgOpacity;
        subConfig.showSpeakerNames = s.subtitleSpeakerNames;
        subConfig.showDirectionIndicators = s.subtitleDirectionIndicators;
        m_Announcer.enabled = s.screenReaderEnabled;
        // Motion + input (mirror of the desktop apply block)
        m_ControllerSystem.SetReducedMotion(s.reducedMotion);
        m_ControllerSystem.SetDisableScreenShake(s.disableScreenShake);
        m_ControllerSystem.SetDisableFOVEffects(s.disableFOVEffects);
        m_ControllerSystem.SetInvertMouseY(s.invertMouseY);
        m_UISystem.SetReducedMotion(s.reducedMotion);
        m_UISystem.SetFontScale(s.fontScale);
        m_UISystem.SetSwitchAccessEnabled(s.switchAccessEnabled, s.switchScanSpeed);
        m_UISystem.SetDwellClickEnabled(s.dwellClickEnabled, s.dwellClickTime);
        m_UISystem.SetStickyDragEnabled(s.stickyDragEnabled);
        if (m_RenderSystem) {
            m_RenderSystem->SetWebAccessibility(
                static_cast<Enjin::u32>(s.colorblindMode), s.colorblindStrength,
                s.screenBrightness, s.screenContrast);
        }
    }

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
    bool m_HasPack = false;

    // Renderer + render system
    std::unique_ptr<Enjin::Renderer::WebGPURenderer> m_Renderer;
    std::unique_ptr<Enjin::Renderer::WebGPUParticleSystem> m_Particles;
    std::unique_ptr<Enjin::Renderer::WebGPUVegetationSystem> m_Vegetation;
    Enjin::u32 m_ParticleFrame = 0;
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
    Enjin::Gameplay::CameraDirector m_CameraDirector;
    Enjin::Gameplay::RecordRewindSystem m_RecordRewindSystem;
    Enjin::Gameplay::SimulationClock m_SimClock;
    Enjin::Effects::InteractiveWaterSystem m_InteractiveWaterSystem;
    Enjin::ECS::TweenSystem m_TweenSystem;
    Enjin::ECS::VisualScriptSystem m_VisualScriptSystem;
    Enjin::ECS::BehaviorTreeSystem m_BehaviorTreeSystem;
    Enjin::ECS::StateMachineSystem m_StateMachineSystem;
    Enjin::ECS::AISystem m_AISystem;
    Enjin::ECS::DialogueSystem m_DialogueSystem;
    Enjin::Gameplay::CinematicSystem m_CinematicSystem;
    Enjin::ECS::EntityEventBus m_EntityEventBus;
    Enjin::Gameplay::FootstepSystem m_FootstepSystem;
    Enjin::Accessibility::SubtitleSystem m_SubtitleSystem;
    Enjin::Accessibility::AccessibilityAnnouncer m_Announcer;
    Enjin::Accessibility::RuntimeAccessibilitySettings m_AccessibilitySettings;
    Enjin::f32 m_OptionsFov = 0.0f;   // options menu override; 0 = use the authored camera
    Enjin::f32 m_ColorblindPreviewTimer = 0.0f;   // seconds of split-preview left after a slider change
    // One true UI source: the same UISystem that renders UICanvasComponent on
    // desktop renders it on web via ImGui's WebGPU backend (UI unification Phase 1).
    Enjin::GUI::UISystem m_UISystem;
    bool m_Paused = false;
    Enjin::ECS::Entity m_PauseMenuEntity = Enjin::ECS::INVALID_ENTITY;
    Enjin::ECS::Entity m_OptionsMenuEntity = Enjin::ECS::INVALID_ENTITY;
    bool m_AtMainMenu = false;             // Authored "MainMenu" canvas showing at boot
    bool m_WebImGuiInit = false;
    Enjin::f32 m_LastDeltaTime = 1.0f / 60.0f;
    Enjin::f32 m_LastDPR = 1.0f;
    Enjin::Gameplay::QuestSystem m_QuestSystem;
    Enjin::Gameplay::ObjectPool m_ObjectPool;
    Enjin::Gameplay::TieredSaveSystem m_TieredSaveSystem;

    // Audio & effects
    Enjin::Audio::SimpleAudio m_SimpleAudio;
    Enjin::Effects::WeatherSystem m_WeatherSystem;
    Enjin::Effects::WindSystem m_WindSystem;
    Enjin::f32 m_RainAccum = 0.0f;   // weather-particle spawn accumulators (web precip)
    Enjin::f32 m_SnowAccum = 0.0f;
    Enjin::f32 m_FireAccum = 0.0f;
    Enjin::Effects::WorldTimeSystem m_WorldTime;
    Enjin::Effects::ParticleSystem m_ParticleSystem;
    Enjin::Effects::ElementalSystem m_ElementalSystem;
    std::vector<Enjin::Effects::FireLight> m_FireLights;
    Enjin::f32 m_EffectsTime = 0.0f;

    // Scene & networking
    Enjin::Scene::SceneManager m_SceneManager;
    Enjin::Networking::NetworkSystem m_NetworkSystem;
    Enjin::Renderer::SceneRenderSettings m_SceneRenderSettings;

    // Build manifest
    std::string m_WindowTitle;
    Enjin::u32 m_WindowWidth = 1280;
    Enjin::u32 m_WindowHeight = 720;
    std::string m_StartScene;
    std::string m_CurrentWebScenePath;   // for Scene_Restart
    int  m_HeapCheckCounter = 0;   // T2 heap-pressure guard
    bool m_Warned80 = false;
    bool m_Warned95 = false;
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

EMSCRIPTEN_KEEPALIVE void setColorblindMode(int mode, float strength) {
    if (g_Player) {
        g_Player->SetColorblindMode(static_cast<Enjin::u32>(mode), strength, 0.0f, 1.0f);
    }
}

} // extern "C"

// ============================================================================
// Main — Emscripten entry point
// ============================================================================
int main(int argc, char* argv[]) {
    (void)argc; (void)argv;

    // Without this every ENJIN_LOG_* call is dropped (Logger::Log early-outs
    // when uninitialized) — the web player was a diagnostic black box.
    // stdout/stderr reach the browser console; the file lands in MEMFS.
    Enjin::Logger::Get().Initialize("enjin.log");

    static WebGamePlayer player;
    g_Player = &player;
    player.StartBoot();  // async — completes over several event-loop turns

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
