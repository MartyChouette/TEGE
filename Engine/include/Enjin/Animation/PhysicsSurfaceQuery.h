#pragma once
//
// The surface query that hand IK actually uses in a running game.
//
// Animation::ISurfaceQuery is deliberately an interface so the IK step, which
// runs inside the renderer's pose pass, does not need a physics backend. This
// is the adapter the three runtimes hand it: a thin wrapper over the physics
// raycast, owned by whoever owns the physics.
//
// Tests use a plane instead, which is what lets the solver be checked to the
// millimetre without a scene.

#include "Enjin/Platform/Platform.h"
#include "Enjin/Animation/HandIK.h"
#include "Enjin/Physics/IPhysicsBackend.h"

namespace Enjin {
namespace Animation {

class ENJIN_API PhysicsSurfaceQuery : public ISurfaceQuery {
public:
    explicit PhysicsSurfaceQuery(Physics::IPhysicsBackend* backend = nullptr)
        : m_Backend(backend) {}

    void SetBackend(Physics::IPhysicsBackend* backend) { m_Backend = backend; }

    // Which collision layers count as something a hand can rest on.
    //
    // Worth having: without it a hand conforms to trigger volumes, to the
    // character's own capsule, and to anything else that happens to be a
    // collider. Defaults to everything because a project that has not thought
    // about layers should still see hands work.
    void SetLayerMask(u32 mask) { m_LayerMask = mask; }

    bool Cast(const Math::Vector3& origin, const Math::Vector3& direction,
              f32 maxDistance, SurfaceHit& out) const override {
        out = SurfaceHit{};
        if (!m_Backend) return false;

        Physics::Ray ray;
        ray.origin = origin;
        ray.direction = direction;

        const Physics::RaycastHit hit = m_Backend->Raycast(ray, maxDistance, m_LayerMask);
        if (!hit.hit) return false;

        out.hit = true;
        out.point = hit.point;
        out.normal = hit.normal;
        out.distance = hit.distance;
        return true;
    }

private:
    Physics::IPhysicsBackend* m_Backend = nullptr;
    u32 m_LayerMask = 0xFFFFFFFFu;
};

} // namespace Animation
} // namespace Enjin
