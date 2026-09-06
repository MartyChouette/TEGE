#include "Enjin/Gameplay/SimulationClock.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/ECS/Components/Controllers/CharacterController.h"
#include <cmath>

namespace Enjin {
namespace Gameplay {

namespace {
    // Does the FIXED STEP own this entity's transform?
    //
    // Anything advanced inside the tick loop has to be interpolated or it
    // renders at the tick pose while the world around it renders smoothly.
    // That is two populations, not one:
    //
    //   - a DYNAMIC 3D rigidbody, moved by the physics solver
    //   - a 3D character or vehicle controller, moved by
    //     ControllerSystem::Update, which ADR-0005 put inside the step loop
    //
    // The second was missing, and it is the one the player IS. First- and
    // third-person characters are driven by JPH::CharacterVirtual and write
    // their transform directly, so they typically carry no dynamic rigidbody
    // at all -- which made the player the only juddering object on a 144Hz
    // display while every loose crate around it was smooth.
    //
    // Kinematic/static bodies stay out: they are moved by code and are already
    // frame-paced. 2D controllers stay out too, matching the 2D rigidbody
    // stance -- they step fixed and render at the tick pose (v1, acceptable at
    // 60Hz, revisit if 2D stutter shows on 144Hz).
    bool IsInterpolated(ECS::World* world, ECS::Entity e) {
        auto* rb = world->GetComponent<ECS::RigidbodyComponent>(e);
        if (rb && rb->bodyType == ECS::RigidbodyComponent::BodyType::Dynamic) return true;

        return world->HasComponent<ECS::FirstPersonController>(e)
            || world->HasComponent<ECS::ThirdPersonController>(e)
            || world->HasComponent<ECS::TopDown3DController>(e)
            || world->HasComponent<ECS::SurfaceAlignedController>(e)
            || world->HasComponent<ECS::VehicleController>(e)
            || world->HasComponent<ECS::WaterVehicleController>(e);
    }

    // Visits every entity the fixed step can own, with its transform.
    //
    // The two pose loops used to iterate GetEntitiesWithComponent<Rigidbody>
    // directly, so widening IsInterpolated alone changed nothing: an entity
    // with no rigidbody was never a candidate in the first place. Both gates
    // have to agree, and this is the one that decides who is even looked at.
    //
    // Iterating every TransformComponent instead would walk the whole scene
    // twice per tick to find a handful of players, so the controller storages
    // are visited explicitly. An entity carrying both a rigidbody and a
    // controller is visited twice; the pose writes are idempotent, so that is
    // harmless.
    template <typename Fn>
    void ForEachSimOwned(ECS::World* world, Fn&& fn) {
        auto visit = [&](ECS::Entity e) {
            if (!IsInterpolated(world, e)) return;
            if (auto* tr = world->GetComponent<ECS::TransformComponent>(e)) fn(e, *tr);
        };
        for (ECS::Entity e : world->GetEntitiesWithComponent<ECS::RigidbodyComponent>())      visit(e);
        for (ECS::Entity e : world->GetEntitiesWithComponent<ECS::FirstPersonController>())   visit(e);
        for (ECS::Entity e : world->GetEntitiesWithComponent<ECS::ThirdPersonController>())   visit(e);
        for (ECS::Entity e : world->GetEntitiesWithComponent<ECS::TopDown3DController>())     visit(e);
        for (ECS::Entity e : world->GetEntitiesWithComponent<ECS::SurfaceAlignedController>())visit(e);
        for (ECS::Entity e : world->GetEntitiesWithComponent<ECS::VehicleController>())       visit(e);
        for (ECS::Entity e : world->GetEntitiesWithComponent<ECS::WaterVehicleController>())  visit(e);
    }

    bool NearlyEqual(const Math::Vector3& a, const Math::Vector3& b) {
        constexpr f32 kEps = 1e-5f;
        return std::fabs(a.x - b.x) < kEps && std::fabs(a.y - b.y) < kEps &&
               std::fabs(a.z - b.z) < kEps;
    }
    bool NearlyEqual(const Math::Quaternion& a, const Math::Quaternion& b) {
        constexpr f32 kEps = 1e-5f;
        return std::fabs(a.x - b.x) < kEps && std::fabs(a.y - b.y) < kEps &&
               std::fabs(a.z - b.z) < kEps && std::fabs(a.w - b.w) < kEps;
    }
}

void SimulationClock::Configure(bool enabled, f32 ticksPerSecond) {
    m_Enabled = enabled;
    if (ticksPerSecond < 15.0f) ticksPerSecond = 15.0f;
    if (ticksPerSecond > 240.0f) ticksPerSecond = 240.0f;
    m_FixedDt = 1.0f / ticksPerSecond;
}

void SimulationClock::Reset() {
    m_Accumulator = 0.0f;
    m_Alpha = 0.0f;
    m_LastSteps = 0;
    m_Poses.clear();
}

void SimulationClock::Tick(ECS::World* world, f32 frameDt,
                           const std::function<void(f32)>& stepFn) {
    if (!m_Enabled) {
        m_LastSteps = 1;
        stepFn(frameDt);   // legacy variable-step path, untouched
        return;
    }

    // 1) Put the true simulation pose back on interpolated bodies so physics
    //    never integrates a render-interpolated transform. A transform that no
    //    longer matches what we wrote means a script/teleport moved it — drop
    //    the stored pose and accept the new value as the simulation's truth.
    if (world) RestoreSimPoses(world);

    m_Accumulator += frameDt;
    m_LastSteps = 0;
    while (m_Accumulator >= m_FixedDt && m_LastSteps < kMaxStepsPerFrame) {
        if (world) SnapshotPrevPoses(world);   // pose entering this tick
        stepFn(m_FixedDt);
        ++m_LastSteps;
        m_Accumulator -= m_FixedDt;
    }
    if (m_LastSteps == kMaxStepsPerFrame && m_Accumulator >= m_FixedDt) {
        // Overloaded: drop the backlog. The game slows down instead of
        // spiraling (each frame would otherwise owe ever more steps).
        m_Accumulator = 0.0f;
    }

    if (world) {
        if (m_LastSteps > 0) CaptureSimPoses(world);
        m_Alpha = m_Accumulator / m_FixedDt;
        WriteInterpolatedPoses(world);
    } else {
        m_Alpha = m_Accumulator / m_FixedDt;
    }
}

void SimulationClock::RestoreSimPoses(ECS::World* world) {
    for (auto it = m_Poses.begin(); it != m_Poses.end();) {
        ECS::Entity e = static_cast<ECS::Entity>(it->first);
        auto* tr = world->IsValid(e) ? world->GetComponent<ECS::TransformComponent>(e) : nullptr;
        if (!tr || !IsInterpolated(world, e)) {
            it = m_Poses.erase(it);
            continue;
        }
        if (NearlyEqual(tr->position, it->second.writtenPos) &&
            NearlyEqual(tr->rotation, it->second.writtenRot)) {
            tr->position = it->second.simPos;
            tr->rotation = it->second.simRot;
            ++it;
        } else {
            it = m_Poses.erase(it);   // externally moved: new value is truth
        }
    }
}

void SimulationClock::SnapshotPrevPoses(ECS::World* world) {
    ForEachSimOwned(world, [&](ECS::Entity e, ECS::TransformComponent& tr) {
        auto& pose = m_Poses[static_cast<u64>(e)];
        pose.prevPos = tr.position;
        pose.prevRot = tr.rotation;
    });
}

void SimulationClock::CaptureSimPoses(ECS::World* world) {
    ForEachSimOwned(world, [&](ECS::Entity e, ECS::TransformComponent& tr) {
        auto& pose = m_Poses[static_cast<u64>(e)];
        pose.simPos = tr.position;
        pose.simRot = tr.rotation;
    });
}

void SimulationClock::WriteInterpolatedPoses(ECS::World* world) {
    f32 t = m_Alpha;
    for (auto& [id, pose] : m_Poses) {
        ECS::Entity e = static_cast<ECS::Entity>(id);
        if (!world->IsValid(e)) continue;
        auto* tr = world->GetComponent<ECS::TransformComponent>(e);
        if (!tr) continue;
        tr->position = pose.prevPos + (pose.simPos - pose.prevPos) * t;
        tr->rotation = Math::Quaternion::Slerp(pose.prevRot, pose.simRot, t);
        pose.writtenPos = tr->position;
        pose.writtenRot = tr->rotation;
    }
}

} // namespace Gameplay
} // namespace Enjin
