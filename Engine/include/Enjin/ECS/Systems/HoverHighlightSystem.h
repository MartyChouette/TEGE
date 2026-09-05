#pragma once
// Decides which entity the cursor is on, once per frame, for every runtime.
//
// The picking itself is ECS::ScreenToRayVP plus a physics raycast -- the same
// pair the script binding uses. Keeping it in one system rather than in each
// runtime is what stops the editor's game view, the desktop player and the
// browser disagreeing about what is under the cursor.
#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Matrix.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/ECS/Entity.h"

namespace Enjin {

namespace Physics { class IPhysicsBackend; }

namespace ECS {

class World;

class ENJIN_API HoverHighlightSystem {
public:
    void SetWorld(World* world) { m_World = world; }
    void SetPhysics(Physics::IPhysicsBackend* physics) { m_Physics = physics; }

    // `viewProjection` and the viewport are whatever the runtime is presenting
    // into: the editor passes its game-view rect, the players pass the window.
    // `cursor` is in pixels from the TOP-LEFT of that rect.
    //
    // `cursorOverUI` comes from the one pointer-capture flag
    // (Input::IsUIConsumedPointer): a cursor over a menu is not pointing at
    // the world, and highlighting through a panel looks broken.
    void Update(const Math::Matrix4& viewProjection,
                const Math::Vector2& cursor,
                f32 viewportWidth, f32 viewportHeight,
                bool cursorOverUI);

    // Clears every hover flag. Called on stop, on scene change, and whenever
    // the cursor leaves the view -- otherwise the last hovered entity stays
    // lit forever with nothing pointing at it.
    void Clear();

    // The entity currently under the cursor, or INVALID_ENTITY. This is the
    // resolved one: with includeChildren set, it is the ancestor that owns the
    // highlight, not the sub-mesh the ray hit.
    Entity GetHovered() const { return m_Hovered; }

private:
    // Walks up the hierarchy for the nearest ancestor carrying an enabled
    // HoverHighlightComponent with includeChildren. Returns `hit` itself when
    // it has one, or INVALID_ENTITY when nothing in the chain does.
    Entity ResolveHighlightOwner(Entity hit) const;

    World* m_World = nullptr;
    Physics::IPhysicsBackend* m_Physics = nullptr;
    Entity m_Hovered = INVALID_ENTITY;
};

} // namespace ECS
} // namespace Enjin
