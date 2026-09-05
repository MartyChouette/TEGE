#include "Enjin/ECS/Systems/HoverHighlightSystem.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/CameraMath.h"
#include "Enjin/ECS/Components/HoverHighlight.h"
#include "Enjin/ECS/Components/Hierarchy.h"
#include "Enjin/Physics/IPhysicsBackend.h"

namespace Enjin {
namespace ECS {

namespace {
// A pick further out than this is not something a player is pointing at, and
// an unbounded ray makes every miss cost a full broadphase traversal.
constexpr f32 kMaxPickDistance = 1000.0f;
// Guards a cycle in a hand-edited hierarchy: walking parents forever would
// hang the frame rather than fail visibly.
constexpr int kMaxAncestorWalk = 32;
}

Entity HoverHighlightSystem::ResolveHighlightOwner(Entity hit) const {
    if (!m_World || hit == INVALID_ENTITY) return INVALID_ENTITY;

    // The entity the ray actually hit wins, whatever its ancestors say.
    if (auto* own = m_World->GetComponent<HoverHighlightComponent>(hit)) {
        if (own->enabled) return hit;
    }

    // Otherwise the nearest ancestor that opted into covering its children.
    // Pointing at a chair leg should light the chair.
    Entity cur = hit;
    for (int step = 0; step < kMaxAncestorWalk; ++step) {
        auto* h = m_World->GetComponent<ParentComponent>(cur);
        if (!h || h->parent == INVALID_ENTITY) break;
        cur = h->parent;
        if (!m_World->IsValid(cur)) break;

        auto* hl = m_World->GetComponent<HoverHighlightComponent>(cur);
        if (hl && hl->enabled && hl->includeChildren) return cur;
    }
    return INVALID_ENTITY;
}

void HoverHighlightSystem::Clear() {
    m_Hovered = INVALID_ENTITY;
    if (!m_World) return;
    for (Entity e : m_World->GetEntitiesWithComponent<HoverHighlightComponent>()) {
        if (auto* h = m_World->GetComponent<HoverHighlightComponent>(e)) h->hovered = false;
    }
}

void HoverHighlightSystem::Update(const Math::Matrix4& viewProjection,
                                  const Math::Vector2& cursor,
                                  f32 viewportWidth, f32 viewportHeight,
                                  bool cursorOverUI) {
    if (!m_World) return;

    Entity nowHovered = INVALID_ENTITY;

    // A cursor over a menu is not pointing at the world. Without this the
    // highlight tracks through an open panel, which reads as a bug.
    if (!cursorOverUI && m_Physics && viewportWidth > 0.0f && viewportHeight > 0.0f) {
        Math::Vector3 origin, dir;
        if (ScreenToRayVP(viewProjection, cursor, viewportWidth, viewportHeight, origin, dir)) {
            Physics::Ray ray;
            ray.origin = origin;
            ray.direction = dir;
            const Physics::RaycastHit hit = m_Physics->Raycast(ray, kMaxPickDistance);
            if (hit.hit) nowHovered = ResolveHighlightOwner(hit.entity);
        }
    }

    if (nowHovered != INVALID_ENTITY && !m_World->IsValid(nowHovered)) {
        nowHovered = INVALID_ENTITY;
    }

    // Nothing changed: leave the flags alone rather than rewriting every one
    // of them every frame.
    if (nowHovered == m_Hovered) return;

    if (m_Hovered != INVALID_ENTITY) {
        if (auto* prev = m_World->GetComponent<HoverHighlightComponent>(m_Hovered)) {
            prev->hovered = false;
        }
    }
    if (nowHovered != INVALID_ENTITY) {
        if (auto* cur = m_World->GetComponent<HoverHighlightComponent>(nowHovered)) {
            cur->hovered = true;
        }
    }
    m_Hovered = nowHovered;
}

} // namespace ECS
} // namespace Enjin
