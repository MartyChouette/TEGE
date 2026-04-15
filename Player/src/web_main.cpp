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

static constexpr const char* PACK_KEY = "enjin_default_pack_key";

// Web game player — no editor, no ImGui, WebGPU renderer
class WebGamePlayer : public Enjin::Application {
public:
    void Initialize() override {
        ENJIN_LOG_INFO(Player, "Enjin Web Player starting...");

        // In web builds, game.enjpak is preloaded into the virtual filesystem
        std::string pakPath = "game.enjpak";
        bool hasPack = m_AssetReader.Open(pakPath, PACK_KEY);

        if (!hasPack) {
            ENJIN_LOG_WARN(Player, "No game.enjpak found — creating demo scene");
        }

        // Read build manifest (or use defaults)
        std::vector<Enjin::u8> manifestData;
        if (hasPack) manifestData = m_AssetReader.ReadFile("_build/manifest.json");
        if (manifestData.empty()) {
            ENJIN_LOG_INFO(Player, "Using default settings (no manifest)");
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
// [WEBGPU-STUB]         m_RenderSystem = m_World->RegisterSystem<Enjin::ECS::RenderSystem>(m_World.get(), m_Renderer.get());
// [WEBGPU-STUB]         m_RenderSystem->SetCamera(m_Camera.get());
// [WEBGPU-STUB]         m_RenderSystem->Initialize();

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
        bool sceneLoaded = false;
        if (hasPack && !m_StartScene.empty()) {
            auto sceneData = m_AssetReader.ReadFile(m_StartScene);
            if (!sceneData.empty()) {
                std::string sceneStr(sceneData.begin(), sceneData.end());
                Enjin::Scene::SceneSerializer serializer(m_World.get());
                serializer.LoadFromString(sceneStr);
                sceneLoaded = true;
                ENJIN_LOG_INFO(Player, "Loaded start scene: %s", m_StartScene.c_str());
            }
        }

        if (!sceneLoaded) {
            // Create a procedural demo scene
            CreateDemoScene();
        }

        // Setup WebGPU rendering pipeline
        SetupWebRenderPipeline();

        m_Initialized = true;
        ENJIN_LOG_INFO(Player, "Web Player initialized");
    }

    void Shutdown() override {
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

    void CreateDemoScene() {
        if (!m_World) return;
        ENJIN_LOG_INFO(Player, "Creating procedural demo scene...");

        // Ground plane
        auto ground = m_World->CreateEntity();
        auto& gxf = m_World->AddComponent<Enjin::ECS::TransformComponent>(ground);
        gxf.position = {0, 0, 0};
        gxf.scale = {20, 0.1f, 20};
        auto& gmesh = m_World->AddComponent<Enjin::ECS::MeshComponent>(ground);
        // Simple box mesh
        gmesh.vertices = {
            {{-0.5f,-0.5f,-0.5f}, {0,1,0}, {0,0}}, {{0.5f,-0.5f,-0.5f}, {0,1,0}, {1,0}},
            {{0.5f,-0.5f,0.5f}, {0,1,0}, {1,1}}, {{-0.5f,-0.5f,0.5f}, {0,1,0}, {0,1}},
            {{-0.5f,0.5f,-0.5f}, {0,1,0}, {0,0}}, {{0.5f,0.5f,-0.5f}, {0,1,0}, {1,0}},
            {{0.5f,0.5f,0.5f}, {0,1,0}, {1,1}}, {{-0.5f,0.5f,0.5f}, {0,1,0}, {0,1}},
        };
        gmesh.indices = {4,5,6, 4,6,7, 0,2,1, 0,3,2, 0,1,5, 0,5,4, 2,3,7, 2,7,6, 1,2,6, 1,6,5, 0,4,7, 0,7,3};
        auto& gmat = m_World->AddComponent<Enjin::ECS::MaterialComponent>(ground);
        gmat.baseColor = {0.3f, 0.6f, 0.2f};

        // Some cubes
        for (int i = 0; i < 5; ++i) {
            auto cube = m_World->CreateEntity();
            auto& cxf = m_World->AddComponent<Enjin::ECS::TransformComponent>(cube);
            cxf.position = {static_cast<Enjin::f32>(i * 3 - 6), 1.0f, -3.0f};
            cxf.scale = {1, 1, 1};
            auto& cmesh = m_World->AddComponent<Enjin::ECS::MeshComponent>(cube);
            cmesh.vertices = gmesh.vertices; // Reuse box
            cmesh.indices = gmesh.indices;
            auto& cmat = m_World->AddComponent<Enjin::ECS::MaterialComponent>(cube);
            cmat.baseColor = {0.2f + i * 0.15f, 0.3f, 0.8f - i * 0.1f};
        }

        ENJIN_LOG_INFO(Player, "Demo scene created (ground + 5 cubes)");
    }

    void SetupWebRenderPipeline() {
        if (!m_Renderer) return;

        // Create uniform buffers
        m_ViewUniformBuffer = m_Renderer->CreateBuffer(256, WGPUBufferUsage_Uniform, nullptr);
        m_ObjectUniformBuffer = m_Renderer->CreateBuffer(256, WGPUBufferUsage_Uniform, nullptr);

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

        // Create pipeline using the renderer's factory
        m_WebPipeline = m_Renderer->CreatePipeline(shaderModule, shaderModule, nullptr);
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

    void Render() override {
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
                    auto eid = static_cast<Enjin::usize>(entity);
                    if (eid >= m_EntityGPUData.size()) m_EntityGPUData.resize(eid + 1);
                    auto& gpuData = m_EntityGPUData[eid];

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
        ubo.view = m_Camera->GetViewMatrix();
        ubo.proj = m_Camera->GetProjectionMatrix();
        ubo.cameraPos = m_Camera->GetPosition();
        ubo.lightDir = Enjin::Math::Vector3(0.5f, 0.8f, 0.3f).Normalized();
        ubo.lightIntensity = 1.2f;
        ubo.lightColor = Enjin::Math::Vector3(1.0f, 0.95f, 0.9f);
        ubo.ambientIntensity = 0.15f;
        ubo.ambientColor = Enjin::Math::Vector3(0.1f, 0.1f, 0.15f);
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
        ubo.model = xform ? Enjin::Math::Matrix4::Identity() : Enjin::Math::Matrix4::Identity();
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

    // Per-entity GPU data
    struct EntityGPUData {
        Enjin::Renderer::WebGPUBufferHandle vertexBuffer;
        Enjin::Renderer::WebGPUBufferHandle indexBuffer;
        WGPUBindGroup objectBindGroup = nullptr;
        Enjin::u32 indexCount = 0;
        bool uploaded = false;
    };
    std::vector<EntityGPUData> m_EntityGPUData;

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
