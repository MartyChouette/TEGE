#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Entity.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/Renderer/Camera.h"
#include "Enjin/Effects/Wind.h"
#include <vector>

namespace Enjin {
namespace ECS {

class RenderSystem;
struct FlowerParticleConfigComponent;

// Lightweight particle (no ECS entity — avoids entity creation/destruction crashes)
struct FlowerParticle {
    Math::Vector3 position;
    Math::Vector3 velocity;
    Math::Vector3 color;
    f32 lifetime = 0.0f;
    f32 maxLifetime = 1.0f;
    f32 scale = 0.06f;
    bool isLiquid = false;  // true = render as streak/droplet, false = round burst
};

class ENJIN_API FlowerSystem {
public:
    FlowerSystem() = default;
    ~FlowerSystem() = default;

    void SetWorld(World* world) { m_World = world; }
    World* GetWorld() const { return m_World; }
    void SetCamera(Renderer::Camera* camera) { m_Camera = camera; }
    Renderer::Camera* GetCamera() const { return m_Camera; }
    void SetRenderSystem(RenderSystem* rs) { m_RenderSystem = rs; }
    RenderSystem* GetRenderSystem() const { return m_RenderSystem; }
    void SetWindSystem(Effects::WindSystem* wind) { m_WindSystem = wind; }
    Effects::WindSystem* GetWindSystem() const { return m_WindSystem; }
    void SetEnabled(bool enabled) { m_Enabled = enabled; }
    bool IsEnabled() const { return m_Enabled; }

    // Reset internal state (call when starting/restarting play mode)
    void Reset() {
        m_GrabbedEntity = INVALID_ENTITY;
        m_GrabDepth = 0.0f;
        m_Particles.clear();
        m_DripAccumulator = 0.0f;
        m_JointsInitialized = false;
    }

    // Set the game view bounds in screen space (for mouse coordinate conversion)
    void SetGameViewBounds(f32 minX, f32 minY, f32 maxX, f32 maxY);

    // Set the render target dimensions (for picking coordinate conversion)
    void SetRenderTargetSize(u32 width, u32 height) { m_RTWidth = width; m_RTHeight = height; }

    // Set game camera entity (for building pick camera from CameraComponent)
    void SetGameCameraEntity(Entity entity) { m_GameCameraEntity = entity; }

    // Main update - call each frame
    void Update(f32 deltaTime);

    // Evaluate all flowers and update score displays
    void Evaluate();

    // Access particles for rendering (EditorLayer projects these to screen)
    const std::vector<FlowerParticle>& GetParticles() const { return m_Particles; }

private:
    void ProcessInput();
    void SetupJointsIfNeeded();
    void ProcessGrabForces(f32 dt);
    void UpdateJellyMeshes(f32 dt);
    void UpdateJointTracking();
    void UpdateBrokenParts(f32 dt);
    void UpdateParticles(f32 dt);
    void CheckGroundImpact();

    void SpawnBreakParticles(const Math::Vector3& position, const Math::Vector3& color,
                             const FlowerParticleConfigComponent* config = nullptr);
    void SpawnGroundSplash(const Math::Vector3& position, const Math::Vector3& color,
                           const FlowerParticleConfigComponent* config = nullptr);
    void SpawnTensionDrip(const Math::Vector3& position, const Math::Vector3& color,
                          f32 tension, const Math::Vector3& squirtDir, f32 intensity,
                          const FlowerParticleConfigComponent* config = nullptr);
    void UpdateScoreDisplay();

    // Helper: project screen point to world plane at given depth facing camera
    Math::Vector3 ScreenToWorldOnPlane(f32 screenX, f32 screenY, f32 planeDepth);

    World* m_World = nullptr;
    Renderer::Camera* m_Camera = nullptr;
    RenderSystem* m_RenderSystem = nullptr;
    Effects::WindSystem* m_WindSystem = nullptr;
    bool m_Enabled = false;

    // Game view bounds in screen space
    f32 m_ViewMinX = 0.0f, m_ViewMinY = 0.0f;
    f32 m_ViewMaxX = 0.0f, m_ViewMaxY = 0.0f;
    u32 m_RTWidth = 640, m_RTHeight = 360;

    Entity m_GameCameraEntity = INVALID_ENTITY;

    // Current grab state
    Entity m_GrabbedEntity = INVALID_ENTITY;
    f32 m_GrabDepth = 0.0f;  // Distance from camera at grab time

    // Lightweight particles (break bursts, ground splashes, tension drips)
    std::vector<FlowerParticle> m_Particles;
    static constexpr usize MAX_PARTICLES = 300;
    f32 m_DripAccumulator = 0.0f;

    // Joint setup state
    bool m_JointsInitialized = false;

    // P-19: Reusable per-frame event vectors (avoid per-call allocation)
    struct BreakEvent {
        Entity entity;
        Math::Vector3 junctionPos;
        Math::Vector3 color;
        Entity stemEntity;
        bool hasWitheredTag;
        bool hasHealthyTag;
    };
    std::vector<BreakEvent> m_BreakEvents;

    struct ImpactEvent {
        Entity entity;
        Math::Vector3 position;
        Math::Vector3 color;
    };
    std::vector<ImpactEvent> m_ImpactEvents;
};

} // namespace ECS
} // namespace Enjin
