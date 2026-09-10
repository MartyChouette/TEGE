// Scene classification, shared by both backends.
//
// This lived twice: a real implementation in the Vulkan half of
// RenderSystem.cpp and, in the WebGPU half, `void
// RenderSystem::ClassifySceneComposition() {}`. An empty body is not a
// simplification here, it is an answer -- m_SceneComposition.mode kept its
// initialiser, Scene3D, forever in a browser. A sprite-only scene never took
// the 2D path there, the sprite and tilemap counts stayed zero whatever the
// scene held, and any decision downstream that reads the mode (lit versus
// unlit sprites, whether to run a shadow pass) took the 3D answer on web and
// the right answer on desktop. That also makes any desktop-to-web comparison
// of a 2D scene unsound, which is the expensive part: the measurement you would
// use to find the bug is itself wrong.
//
// It is one function of plain ECS counting with no backend in it, so it belongs
// in one place rather than in both halves of a #if.

#include "Enjin/ECS/Systems/RenderSystem.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/ECS/Components/Mesh.h"
#include "Enjin/ECS/Components/Light.h"
#include "Enjin/Logging/Log.h"

namespace Enjin {
namespace ECS {

void RenderSystem::ClassifySceneComposition() {
    if (!m_SceneComposition.dirty || !m_World) return;

    m_SceneComposition.spriteCount = 0;
    m_SceneComposition.tilemapCount = 0;
    m_SceneComposition.mesh3DCount = 0;
    m_SceneComposition.hasShadowCastingLights = false;

    // Count sprites and tilemaps using direct container size (avoids iteration)
    m_SceneComposition.spriteCount = static_cast<u32>(m_World->GetEntitiesWithComponent<Sprite2DComponent>().size());
    m_SceneComposition.tilemapCount = static_cast<u32>(m_World->GetEntitiesWithComponent<TilemapComponent>().size());

    // Count 3D meshes: total MeshComponent entities minus sprites and tilemaps
    // (sprites and tilemaps also have MeshComponent, so subtract them)
    {
        u32 totalMesh = static_cast<u32>(m_World->GetEntitiesWithComponent<MeshComponent>().size());
        m_SceneComposition.mesh3DCount = (totalMesh > m_SceneComposition.spriteCount + m_SceneComposition.tilemapCount)
            ? totalMesh - m_SceneComposition.spriteCount - m_SceneComposition.tilemapCount : 0;
    }

    // Check for shadow-casting directional lights and any lights at all
    bool hasAnyLights = !m_CachedLightEntities.empty();
    for (Entity entity : m_CachedLightEntities) {
        auto* light = m_World->GetComponent<LightComponent>(entity);
        if (light && light->type == LightType::Directional && light->castShadows) {
            m_SceneComposition.hasShadowCastingLights = true;
            break;
        }
    }

    // Classify scene mode
    // Scene3D: 3D meshes present — full pipeline (shadows, lighting, normal maps)
    // Scene2_5D: sprites only but lights exist — skip shadows, populate full lighting UBO
    // Scene2D: sprites only, no lights — minimal UBO (ambient/fog only)
    if (m_SceneComposition.mesh3DCount > 0) {
        m_SceneComposition.mode = SceneRenderMode::Scene3D;
    } else if (hasAnyLights) {
        m_SceneComposition.mode = SceneRenderMode::Scene2_5D;
    } else {
        m_SceneComposition.mode = SceneRenderMode::Scene2D;
    }

    m_SceneComposition.dirty = false;

    // Diagnostic warnings (every 300 frames to avoid log spam)
    if (++m_DiagnosticFrameCounter >= 300) {
        m_DiagnosticFrameCounter = 0;

#if !ENJIN_RENDERER_WEBGPU
        // Warn if many unbatched sprites
        if (m_SceneComposition.spriteCount > 100 && !m_SpriteBatchRenderer) {
            ENJIN_LOG_WARN(Renderer, "%u sprites without batching - consider enabling SpriteBatchRenderer",
                m_SceneComposition.spriteCount);
        }

        // Log mixed 2D/3D scene info for debugging
        if (m_SceneComposition.spriteCount > 0 && m_SceneComposition.mesh3DCount > 0) {
            ENJIN_LOG_INFO(Renderer, "Mixed 2D/3D scene: %u sprites, %u meshes, %u shadow casters",
                m_SceneComposition.spriteCount, m_SceneComposition.mesh3DCount,
                static_cast<u32>(m_ShadowCasters.size()));
        }
#else
        if (m_SceneComposition.spriteCount > 0 && m_SceneComposition.mesh3DCount > 0) {
            ENJIN_LOG_INFO(Renderer, "Mixed 2D/3D scene: %u sprites, %u meshes",
                m_SceneComposition.spriteCount, m_SceneComposition.mesh3DCount);
        }
#endif
    }
}

} // namespace ECS
} // namespace Enjin
