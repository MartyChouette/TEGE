#include "Enjin/SimpleApp.h"
#include "Enjin/Logging/Log.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Systems/RenderSystem.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Mesh.h"
#include "Enjin/ECS/Components/Material.h"
#include "Enjin/ECS/Components/Light.h"
#include "Enjin/ECS/Components/Name.h"
#include "Enjin/ECS/Components/Camera.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/Renderer/Vulkan/VulkanRenderer.h"
#include "Enjin/Renderer/Camera.h"
#include "Enjin/Renderer/CameraController.h"
#include "Enjin/Assets/GLTFLoader.h"
#include "Enjin/Assets/SceneImporter.h"
#include <cmath>

namespace Enjin {

// ─── Mesh generation helpers ────────────────────────────────────────────────

static void GenerateCubeMesh(ECS::MeshComponent& mesh) {
    // 24 vertices (4 per face for correct normals)
    const f32 h = 0.5f;
    struct FaceVert { f32 px, py, pz, nx, ny, nz, u, v; };
    static const FaceVert verts[] = {
        // Front (+Z)
        {-h,-h, h,  0, 0, 1,  0,0}, { h,-h, h,  0, 0, 1,  1,0},
        { h, h, h,  0, 0, 1,  1,1}, {-h, h, h,  0, 0, 1,  0,1},
        // Back (-Z)
        { h,-h,-h,  0, 0,-1,  0,0}, {-h,-h,-h,  0, 0,-1,  1,0},
        {-h, h,-h,  0, 0,-1,  1,1}, { h, h,-h,  0, 0,-1,  0,1},
        // Top (+Y)
        {-h, h, h,  0, 1, 0,  0,0}, { h, h, h,  0, 1, 0,  1,0},
        { h, h,-h,  0, 1, 0,  1,1}, {-h, h,-h,  0, 1, 0,  0,1},
        // Bottom (-Y)
        {-h,-h,-h,  0,-1, 0,  0,0}, { h,-h,-h,  0,-1, 0,  1,0},
        { h,-h, h,  0,-1, 0,  1,1}, {-h,-h, h,  0,-1, 0,  0,1},
        // Right (+X)
        { h,-h, h,  1, 0, 0,  0,0}, { h,-h,-h,  1, 0, 0,  1,0},
        { h, h,-h,  1, 0, 0,  1,1}, { h, h, h,  1, 0, 0,  0,1},
        // Left (-X)
        {-h,-h,-h, -1, 0, 0,  0,0}, {-h,-h, h, -1, 0, 0,  1,0},
        {-h, h, h, -1, 0, 0,  1,1}, {-h, h,-h, -1, 0, 0,  0,1},
    };

    mesh.vertices.resize(24);
    for (int i = 0; i < 24; i++) {
        auto& v = mesh.vertices[i];
        v.position = Math::Vector3(verts[i].px, verts[i].py, verts[i].pz);
        v.normal = Math::Vector3(verts[i].nx, verts[i].ny, verts[i].nz);
        v.uv = Math::Vector2(verts[i].u, verts[i].v);
        v.color = Math::Vector4(1.0f);
    }

    mesh.indices.reserve(36);
    for (int face = 0; face < 6; face++) {
        u32 base = face * 4;
        mesh.indices.push_back(base + 0);
        mesh.indices.push_back(base + 1);
        mesh.indices.push_back(base + 2);
        mesh.indices.push_back(base + 0);
        mesh.indices.push_back(base + 2);
        mesh.indices.push_back(base + 3);
    }
}

static void GeneratePlaneMesh(ECS::MeshComponent& mesh, f32 size) {
    f32 h = size * 0.5f;
    mesh.vertices.resize(4);

    mesh.vertices[0].position = Math::Vector3(-h, 0, -h);
    mesh.vertices[0].normal = Math::Vector3(0, 1, 0);
    mesh.vertices[0].uv = Math::Vector2(0, 0);
    mesh.vertices[0].color = Math::Vector4(1.0f);

    mesh.vertices[1].position = Math::Vector3(h, 0, -h);
    mesh.vertices[1].normal = Math::Vector3(0, 1, 0);
    mesh.vertices[1].uv = Math::Vector2(1, 0);
    mesh.vertices[1].color = Math::Vector4(1.0f);

    mesh.vertices[2].position = Math::Vector3(h, 0, h);
    mesh.vertices[2].normal = Math::Vector3(0, 1, 0);
    mesh.vertices[2].uv = Math::Vector2(1, 1);
    mesh.vertices[2].color = Math::Vector4(1.0f);

    mesh.vertices[3].position = Math::Vector3(-h, 0, h);
    mesh.vertices[3].normal = Math::Vector3(0, 1, 0);
    mesh.vertices[3].uv = Math::Vector2(0, 1);
    mesh.vertices[3].color = Math::Vector4(1.0f);

    mesh.indices = {0, 1, 2, 0, 2, 3};
}

// ─── SimpleApp ──────────────────────────────────────────────────────────────

SimpleApp::SimpleApp() = default;
SimpleApp::~SimpleApp() = default;

void SimpleApp::Initialize() {
    ENJIN_LOG_INFO(Game, "SimpleApp initializing...");

    // Renderer
    m_Renderer = std::make_unique<Renderer::VulkanRenderer>();
    if (!m_Renderer->Initialize(GetWindow())) {
        ENJIN_LOG_FATAL(Game, "Failed to initialize Vulkan renderer");
        m_Renderer.reset();
        return;
    }

    GetWindow()->SetResizeCallback([this](u32, u32) {
        if (m_Renderer) m_Renderer->SetFramebufferResized(true);
    });

    // Camera
    m_Camera = std::make_unique<Renderer::Camera>();
    m_Camera->SetPerspective(45.0f, 16.0f / 9.0f, 0.1f, 1000.0f);
    m_Camera->SetPosition(Math::Vector3(0.0f, 3.0f, 8.0f));

    m_CameraController = std::make_unique<Renderer::CameraController>(m_Camera.get());
    m_CameraController->SetMoveSpeed(5.0f);
    m_CameraController->SetLookSensitivity(0.15f);

    // ECS
    m_World = std::make_unique<ECS::World>();
    m_RenderSystem = m_World->RegisterSystem<ECS::RenderSystem>(m_World.get(), m_Renderer.get());
    m_RenderSystem->SetCamera(m_Camera.get());
    m_RenderSystem->Initialize();

    ENJIN_LOG_INFO(Game, "SimpleApp ready — calling OnStart()");
    OnStart();
}

void SimpleApp::Shutdown() {
    ENJIN_LOG_INFO(Game, "SimpleApp shutting down...");
    OnShutdown();

    if (m_RenderSystem) {
        m_RenderSystem->Shutdown();
        m_RenderSystem = nullptr;
    }
    m_World.reset();
    m_CameraController.reset();
    m_Camera.reset();
    m_Renderer.reset();
}

void SimpleApp::Update(f32 deltaTime) {
    m_LastDeltaTime = deltaTime;

    if (m_FlyCamEnabled && m_CameraController) {
        m_CameraController->Update(deltaTime);
    }

    // Update camera aspect ratio
    if (m_Renderer && m_Camera) {
        auto extent = m_Renderer->GetSwapchainExtent();
        if (extent.width > 0 && extent.height > 0) {
            f32 aspect = static_cast<f32>(extent.width) / static_cast<f32>(extent.height);
            m_Camera->SetPerspective(m_Camera->GetFOV(), aspect,
                                     m_Camera->GetNearPlane(), m_Camera->GetFarPlane());
        }
    }

    OnUpdate(deltaTime);
}

void SimpleApp::Render() {
    if (!m_Renderer) return;
    if (!m_Renderer->BeginFrame()) return;

    if (m_RenderSystem) {
        m_RenderSystem->FlushPendingChanges();
    }

    if (m_World) {
        m_World->Update(m_LastDeltaTime);
    }

    m_Renderer->EndFrame();
}

// ── Scene building ──

ECS::Entity SimpleApp::LoadModel(const std::string& path, Math::Vector3 position, Math::Vector3 scale) {
    if (!m_World) return ECS::Entity(0);

    Assets::ImportOptions options;
    options.scale = 1.0f;
    auto result = Assets::SceneImporter::Import(path, m_World.get(), options);

    if (!result.success) {
        ENJIN_LOG_ERROR(Game, "Failed to load model: %s — %s", path.c_str(), result.errorMessage.c_str());
        return ECS::Entity(0);
    }

    // Set position/scale on root entity
    if (result.rootEntity != ECS::INVALID_ENTITY) {
        auto* transform = m_World->GetComponent<ECS::TransformComponent>(result.rootEntity);
        if (transform) {
            transform->position = position;
            transform->scale = scale;
        }
        return result.rootEntity;
    }

    return ECS::Entity(0);
}

ECS::Entity SimpleApp::AddCube(Math::Vector3 position, Math::Vector3 scale, Math::Vector3 color) {
    if (!m_World) return ECS::Entity(0);

    auto entity = m_World->CreateEntity();
    m_World->AddComponent<ECS::NameComponent>(entity, ECS::NameComponent{"Cube"});

    ECS::TransformComponent transform;
    transform.position = position;
    transform.scale = scale;
    m_World->AddComponent(entity, transform);

    ECS::MeshComponent mesh;
    GenerateCubeMesh(mesh);
    m_World->AddComponent(entity, mesh);

    ECS::MaterialComponent material;
    material.baseColor = color;
    material.roughness = 0.5f;
    material.metallic = 0.0f;
    m_World->AddComponent(entity, material);

    return entity;
}

ECS::Entity SimpleApp::AddPlane(Math::Vector3 position, f32 size, Math::Vector3 color) {
    if (!m_World) return ECS::Entity(0);

    auto entity = m_World->CreateEntity();
    m_World->AddComponent<ECS::NameComponent>(entity, ECS::NameComponent{"Ground"});

    ECS::TransformComponent transform;
    transform.position = position;
    m_World->AddComponent(entity, transform);

    ECS::MeshComponent mesh;
    GeneratePlaneMesh(mesh, size);
    m_World->AddComponent(entity, mesh);

    ECS::MaterialComponent material;
    material.baseColor = color;
    material.roughness = 0.8f;
    material.metallic = 0.0f;
    m_World->AddComponent(entity, material);

    return entity;
}

ECS::Entity SimpleApp::AddDirectionalLight(Math::Vector3 direction, Math::Vector3 color, f32 intensity) {
    if (!m_World) return ECS::Entity(0);

    auto entity = m_World->CreateEntity();
    m_World->AddComponent<ECS::NameComponent>(entity, ECS::NameComponent{"Sun"});

    // Compute rotation quaternion from direction
    Math::Vector3 dir = direction.Normalized();
    Math::Vector3 forward(0.0f, 0.0f, -1.0f);
    Math::Quaternion rotation = Math::Quaternion::FromToRotation(forward, dir);

    ECS::TransformComponent transform;
    transform.rotation = rotation;
    m_World->AddComponent(entity, transform);

    ECS::LightComponent light;
    light.type = ECS::LightType::Directional;
    light.color = color;
    light.intensity = intensity;
    light.castShadows = true;
    m_World->AddComponent(entity, light);

    return entity;
}

ECS::Entity SimpleApp::AddPointLight(Math::Vector3 position, Math::Vector3 color, f32 intensity, f32 range) {
    if (!m_World) return ECS::Entity(0);

    auto entity = m_World->CreateEntity();
    m_World->AddComponent<ECS::NameComponent>(entity, ECS::NameComponent{"Point Light"});

    ECS::TransformComponent transform;
    transform.position = position;
    m_World->AddComponent(entity, transform);

    ECS::LightComponent light;
    light.type = ECS::LightType::Point;
    light.color = color;
    light.intensity = intensity;
    light.range = range;
    m_World->AddComponent(entity, light);

    return entity;
}

void SimpleApp::SetCameraPosition(Math::Vector3 position) {
    if (m_Camera) m_Camera->SetPosition(position);
}

void SimpleApp::SetCameraRotation(f32 yaw, f32 pitch) {
    if (!m_Camera) return;
    // Convert yaw/pitch (degrees) to quaternion via Euler angles
    m_Camera->SetRotation(Math::Quaternion::FromEuler(Math::Vector3(pitch, yaw, 0.0f)));
}

void SimpleApp::EnableFlyCam(bool enable) {
    m_FlyCamEnabled = enable;
}

void SimpleApp::SetPosition(ECS::Entity entity, Math::Vector3 position) {
    if (!m_World) return;
    auto* t = m_World->GetComponent<ECS::TransformComponent>(entity);
    if (t) t->position = position;
}

void SimpleApp::SetRotation(ECS::Entity entity, Math::Quaternion rotation) {
    if (!m_World) return;
    auto* t = m_World->GetComponent<ECS::TransformComponent>(entity);
    if (t) t->rotation = rotation;
}

void SimpleApp::SetScale(ECS::Entity entity, Math::Vector3 scale) {
    if (!m_World) return;
    auto* t = m_World->GetComponent<ECS::TransformComponent>(entity);
    if (t) t->scale = scale;
}

void SimpleApp::SetColor(ECS::Entity entity, Math::Vector3 color) {
    if (!m_World) return;
    auto* mat = m_World->GetComponent<ECS::MaterialComponent>(entity);
    if (mat) mat->baseColor = color;
}

void SimpleApp::SetVisible(ECS::Entity entity, bool visible) {
    if (!m_World) return;
    auto* t = m_World->GetComponent<ECS::TransformComponent>(entity);
    if (t) t->visible = visible;
}

void SimpleApp::DestroyEntity(ECS::Entity entity) {
    if (m_World) m_World->DestroyEntity(entity);
}

// ─── SimpleApp2D ────────────────────────────────────────────────────────────

SimpleApp2D::SimpleApp2D() = default;

void SimpleApp2D::Initialize() {
    // Initialize base (renderer, world, render system)
    SimpleApp::Initialize();

    // Switch camera to orthographic for 2D
    if (m_Camera && m_Renderer) {
        auto extent = m_Renderer->GetSwapchainExtent();
        f32 w = static_cast<f32>(extent.width);
        f32 h = static_cast<f32>(extent.height);
        m_Camera->SetOrthographic(0.0f, w, 0.0f, h, -100.0f, 100.0f);
        m_Camera->SetPosition(Math::Vector3(0.0f, 0.0f, 10.0f));
    }

    // Disable fly camera for 2D — user controls camera manually
    EnableFlyCam(false);
}

ECS::Entity SimpleApp2D::AddSprite(const std::string& texturePath, Math::Vector2 position, Math::Vector2 size) {
    if (!m_World) return ECS::Entity(0);

    auto entity = m_World->CreateEntity();
    m_World->AddComponent<ECS::NameComponent>(entity, ECS::NameComponent{"Sprite"});

    ECS::TransformComponent transform;
    transform.position = Math::Vector3(position.x, position.y, 0.0f);
    transform.scale = Math::Vector3(size.x, size.y, 1.0f);
    m_World->AddComponent(entity, transform);

    ECS::Sprite2DComponent sprite;
    sprite.texturePath = texturePath;
    sprite.size = size;
    m_World->AddComponent(entity, sprite);

    return entity;
}

ECS::Entity SimpleApp2D::AddRect(Math::Vector2 position, Math::Vector2 size, Math::Vector3 color) {
    if (!m_World) return ECS::Entity(0);

    auto entity = m_World->CreateEntity();
    m_World->AddComponent<ECS::NameComponent>(entity, ECS::NameComponent{"Rect"});

    ECS::TransformComponent transform;
    transform.position = Math::Vector3(position.x, position.y, 0.0f);
    transform.scale = Math::Vector3(1.0f);
    m_World->AddComponent(entity, transform);

    ECS::Sprite2DComponent sprite;
    sprite.tint = color;
    sprite.size = size;
    m_World->AddComponent(entity, sprite);

    return entity;
}

void SimpleApp2D::SetPosition2D(ECS::Entity entity, Math::Vector2 position) {
    SetPosition(entity, Math::Vector3(position.x, position.y, 0.0f));
}

void SimpleApp2D::SetSize(ECS::Entity entity, Math::Vector2 size) {
    SetScale(entity, Math::Vector3(size.x, size.y, 1.0f));
}

} // namespace Enjin
