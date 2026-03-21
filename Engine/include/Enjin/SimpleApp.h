#pragma once

// SimpleApp — Quick-start facade for Enjin rendering
//
// Usage (3D):
//   class MyApp : public Enjin::SimpleApp {
//       void OnStart() override {
//           LoadModel("scene.gltf");
//           AddDirectionalLight({1, -1, -0.5f}, {1, 1, 0.95f}, 2.0f);
//           SetCameraPosition({0, 5, 10});
//       }
//       void OnUpdate(f32 dt) override { /* your logic */ }
//   };
//   ENJIN_SIMPLE_MAIN(MyApp)
//
// Usage (2D):
//   class MyApp : public Enjin::SimpleApp2D {
//       void OnStart() override {
//           AddSprite("player.png", {400, 300});
//       }
//       void OnUpdate(f32 dt) override { /* your logic */ }
//   };
//   ENJIN_SIMPLE_MAIN(MyApp)
//
// The full ECS world, renderer, and camera are accessible via
// GetWorld(), GetRenderer(), GetCamera() for advanced usage.

#include "Enjin/Core/Application.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/Math/Quaternion.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Entity.h"
#include <memory>
#include <string>

namespace Enjin {

namespace Renderer {
    class VulkanRenderer;
    class Camera;
    class CameraController;
}

namespace ECS {
    class RenderSystem;
}

// ─── SimpleApp (3D) ─────────────────────────────────────────────────────────

class ENJIN_API SimpleApp : public Application {
public:
    SimpleApp();
    virtual ~SimpleApp();

    // Override these in your app
    virtual void OnStart() {}
    virtual void OnUpdate(f32 deltaTime) {}
    virtual void OnShutdown() {}

    // ── Scene building ──

    // Load a GLTF/GLB model file. Returns the root entity.
    ECS::Entity LoadModel(const std::string& path,
                          Math::Vector3 position = Math::Vector3(0.0f),
                          Math::Vector3 scale = Math::Vector3(1.0f));

    // Create a cube primitive. Returns the entity.
    ECS::Entity AddCube(Math::Vector3 position = Math::Vector3(0.0f),
                        Math::Vector3 scale = Math::Vector3(1.0f),
                        Math::Vector3 color = Math::Vector3(0.8f));

    // Create a plane (ground). Returns the entity.
    ECS::Entity AddPlane(Math::Vector3 position = Math::Vector3(0.0f),
                         f32 size = 10.0f,
                         Math::Vector3 color = Math::Vector3(0.4f));

    // ── Lighting ──

    ECS::Entity AddDirectionalLight(Math::Vector3 direction = Math::Vector3(0, -1, -0.5f),
                                    Math::Vector3 color = Math::Vector3(1.0f),
                                    f32 intensity = 1.5f);

    ECS::Entity AddPointLight(Math::Vector3 position,
                              Math::Vector3 color = Math::Vector3(1.0f),
                              f32 intensity = 1.0f,
                              f32 range = 10.0f);

    // ── Camera ──

    void SetCameraPosition(Math::Vector3 position);
    void SetCameraRotation(f32 yaw, f32 pitch);
    void EnableFlyCam(bool enable = true);  // WASD + mouse (default: on)

    // ── Entity manipulation ──

    void SetPosition(ECS::Entity entity, Math::Vector3 position);
    void SetRotation(ECS::Entity entity, Math::Quaternion rotation);
    void SetScale(ECS::Entity entity, Math::Vector3 scale);
    void SetColor(ECS::Entity entity, Math::Vector3 color);
    void SetVisible(ECS::Entity entity, bool visible);
    void DestroyEntity(ECS::Entity entity);

    // ── Access to internals (for advanced usage) ──

    ECS::World* GetWorld() { return m_World.get(); }
    Renderer::VulkanRenderer* GetRenderer() { return m_Renderer.get(); }
    Renderer::Camera* GetCamera() { return m_Camera.get(); }

protected:
    // Application overrides (don't override these — use OnStart/OnUpdate instead)
    void Initialize() override;
    void Shutdown() override;
    void Update(f32 deltaTime) override;
    void Render() override;

    std::unique_ptr<Renderer::VulkanRenderer> m_Renderer;
    std::unique_ptr<Renderer::Camera> m_Camera;
    std::unique_ptr<Renderer::CameraController> m_CameraController;
    std::unique_ptr<ECS::World> m_World;
    ECS::RenderSystem* m_RenderSystem = nullptr;
    f32 m_LastDeltaTime = 0.0f;
    bool m_FlyCamEnabled = true;
};

// ─── SimpleApp2D ────────────────────────────────────────────────────────────

class ENJIN_API SimpleApp2D : public SimpleApp {
public:
    SimpleApp2D();

    // Override these in your app
    void OnStart() override {}
    void OnUpdate(f32 deltaTime) override {}

    // ── 2D scene building ──

    // Add a textured sprite. Position is in pixels from bottom-left.
    ECS::Entity AddSprite(const std::string& texturePath,
                          Math::Vector2 position = Math::Vector2(0.0f),
                          Math::Vector2 size = Math::Vector2(64.0f));

    // Add a colored rectangle (no texture).
    ECS::Entity AddRect(Math::Vector2 position,
                        Math::Vector2 size,
                        Math::Vector3 color = Math::Vector3(1.0f));

    // ── 2D entity helpers ──

    void SetPosition2D(ECS::Entity entity, Math::Vector2 position);
    void SetSize(ECS::Entity entity, Math::Vector2 size);

protected:
    void Initialize() override;
};

} // namespace Enjin

// Macro to create the entry point — use instead of writing main() yourself
#define ENJIN_SIMPLE_MAIN(AppClass) \
    Enjin::Application* CreateApplication() { return new AppClass(); } \
    int main(int argc, char* argv[]) { \
        (void)argc; (void)argv; \
        auto* app = CreateApplication(); \
        int result = app->Run(); \
        delete app; \
        return result; \
    }
