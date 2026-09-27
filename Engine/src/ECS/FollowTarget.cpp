#include "Enjin/ECS/FollowTarget.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/ECS/Components/Hierarchy.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/Math/Math.h"
#include <algorithm>
#include <cmath>

namespace Enjin {
namespace ECS {

namespace {

// Critically damped spring toward target (Game Programming Gems 4, 1.10),
// reaching it in roughly smoothTime, capped at maxSpeed.
Math::Vector3 SmoothDamp(const Math::Vector3& current, const Math::Vector3& target,
                         Math::Vector3& velocity, f32 smoothTime, f32 maxSpeed, f32 dt) {
    smoothTime = std::max(smoothTime, 0.0001f);
    const f32 omega = 2.0f / smoothTime;
    const f32 x = omega * dt;
    const f32 decay = 1.0f / (1.0f + x + 0.48f * x * x + 0.235f * x * x * x);

    Math::Vector3 change = current - target;
    const f32 maxChange = maxSpeed * smoothTime;
    const f32 len = change.Length();
    if (maxSpeed > 0.0f && len > maxChange) change = change * (maxChange / len);
    const Math::Vector3 clampedTarget = current - change;

    const Math::Vector3 temp = (velocity + change * omega) * dt;
    velocity = (velocity - temp * omega) * decay;
    Math::Vector3 out = clampedTarget + (change + temp) * decay;

    // No overshoot past the original target
    const Math::Vector3 toTarget = target - current;
    const Math::Vector3 toOut = out - target;
    if (toTarget.Dot(toOut) > 0.0f) {
        out = target;
        velocity = Math::Vector3(0.0f);
    }
    return out;
}

Math::Quaternion RotateTowards(const Math::Quaternion& from, const Math::Quaternion& to, f32 maxDegrees) {
    f32 d = std::abs(from.x * to.x + from.y * to.y + from.z * to.z + from.w * to.w);
    d = std::min(d, 1.0f);
    const f32 angle = 2.0f * std::acos(d) * 180.0f / 3.14159265f;
    if (angle <= maxDegrees || angle < 1e-4f) return to;
    return Math::Quaternion::Slerp(from, to, maxDegrees / angle);
}

} // namespace

void UpdateFollowTargets(World* world, f32 deltaTime) {
    if (!world || deltaTime <= 0.0f) return;
    for (Entity entity : world->GetEntitiesWithComponent<FollowTargetComponent>()) {
        auto* transform = world->GetComponent<TransformComponent>(entity);
        auto* follow = world->GetComponent<FollowTargetComponent>(entity);
        if (!transform || !follow) continue;
        if (follow->target == INVALID_ENTITY || follow->target == entity || !world->IsValid(follow->target)) continue;
        if (!world->GetComponent<TransformComponent>(follow->target)) continue;

        Math::Vector3 targetPos, selfPos;
        Math::Quaternion targetRot, selfRot;
        GetWorldTransform(world, follow->target, targetPos, targetRot);
        GetWorldTransform(world, entity, selfPos, selfRot);

        const Math::Vector3 goal = targetPos +
            (follow->useLocalOffset ? targetRot.Rotate(follow->offset) : follow->offset);

        Math::Vector3 newPos = selfPos;
        const Math::Vector3 fromGoal = selfPos - goal;
        const f32 d = fromGoal.Length();
        const bool gaveUp = follow->maxDistance > 0.0f && d > follow->maxDistance;
        const bool tooClose = d < std::min(follow->minDistance, follow->followDistance);
        if (gaveUp || tooClose) {
            follow->currentVelocity = Math::Vector3(0.0f);
        } else {
            Math::Vector3 dest = goal;
            if (follow->followDistance > 0.0f && d > 1e-4f) dest = goal + fromGoal * (follow->followDistance / d);
            if (follow->smoothTime <= 0.0f) {
                newPos = dest;
                follow->currentVelocity = Math::Vector3(0.0f);
            } else {
                newPos = SmoothDamp(selfPos, dest, follow->currentVelocity,
                                    follow->smoothTime, follow->moveSpeed, deltaTime);
            }
        }

        Math::Quaternion newRot = selfRot;
        if (follow->matchTargetRotation) {
            newRot = RotateTowards(selfRot, targetRot, follow->rotationSpeed * deltaTime);
        }

        Math::Vector3 localPos;
        Math::Quaternion localRot;
        WorldToLocalTransform(world, entity, newPos, newRot, localPos, localRot);
        transform->position = localPos;
        if (follow->matchTargetRotation) transform->rotation = localRot.Normalized();
        transform->worldMatrixDirty = true;
    }
}

} // namespace ECS
} // namespace Enjin
