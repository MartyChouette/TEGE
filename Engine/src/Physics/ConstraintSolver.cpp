#include "Enjin/Physics/ConstraintSolver.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/Math/Math.h"
#include "Enjin/Logging/Log.h"
#include <algorithm>

namespace Enjin {
namespace Physics {

ConstraintSolver::ConstraintSolver() = default;

void ConstraintSolver::SolveConstraints(f32 deltaTime) {
    if (!m_World || deltaTime <= 0.0f) return;

    // Warm start: apply cached impulses from previous frame to contact constraints
    for (auto& contact : m_Contacts) {
        if (contact.accumulatedNormalImpulse == 0.0f && contact.accumulatedTangentImpulse == 0.0f)
            continue;

        auto* rbA = m_World->GetComponent<ECS::RigidbodyComponent>(contact.entityA);
        auto* rbB = m_World->GetComponent<ECS::RigidbodyComponent>(contact.entityB);
        if (!rbA || !rbB) continue;

        f32 invMassA = (rbA->bodyType == ECS::RigidbodyComponent::BodyType::Dynamic && rbA->mass > 0.0f)
                        ? 1.0f / rbA->mass : 0.0f;
        f32 invMassB = (rbB->bodyType == ECS::RigidbodyComponent::BodyType::Dynamic && rbB->mass > 0.0f)
                        ? 1.0f / rbB->mass : 0.0f;

        // Apply previously accumulated normal impulse
        Math::Vector3 normalImpulse = contact.contactNormal * contact.accumulatedNormalImpulse;
        rbA->velocity = rbA->velocity - normalImpulse * invMassA;
        rbB->velocity = rbB->velocity + normalImpulse * invMassB;
    }

    // Sequential impulse iterations
    for (u32 i = 0; i < m_Iterations; ++i) {
        SolveJointConstraints(deltaTime);
        SolveContactConstraints(deltaTime);
    }

    // Position correction pass to prevent drift
    ApplyBaumgarteStabilization(deltaTime);
}

void ConstraintSolver::AddContactConstraint(const ContactConstraint& contact) {
    m_Contacts.push_back(contact);
}

void ConstraintSolver::ClearContactConstraints() {
    m_Contacts.clear();
}

// ---------------------------------------------------------------------------
// Joint constraint dispatch
// ---------------------------------------------------------------------------

void ConstraintSolver::SolveJointConstraints(f32 deltaTime) {
    // Distance joints
    for (ECS::Entity entity : m_World->GetEntitiesWithComponent<ECS::DistanceJointComponent>()) {
        SolveDistanceJoint(entity, deltaTime);
    }
    for (ECS::Entity broken : m_BrokenDistanceJoints) {
        if (m_World->HasComponent<ECS::DistanceJointComponent>(broken))
            m_World->RemoveComponent<ECS::DistanceJointComponent>(broken);
    }
    m_BrokenDistanceJoints.clear();

    // Hinge joints
    for (ECS::Entity entity : m_World->GetEntitiesWithComponent<ECS::HingeJointComponent>()) {
        SolveHingeJoint(entity, deltaTime);
    }
    for (ECS::Entity broken : m_BrokenHingeJoints) {
        if (m_World->HasComponent<ECS::HingeJointComponent>(broken))
            m_World->RemoveComponent<ECS::HingeJointComponent>(broken);
    }
    m_BrokenHingeJoints.clear();

    // Ball-socket joints
    for (ECS::Entity entity : m_World->GetEntitiesWithComponent<ECS::BallSocketJointComponent>()) {
        SolveBallSocketJoint(entity, deltaTime);
    }
    for (ECS::Entity broken : m_BrokenBallSocketJoints) {
        if (m_World->HasComponent<ECS::BallSocketJointComponent>(broken))
            m_World->RemoveComponent<ECS::BallSocketJointComponent>(broken);
    }
    m_BrokenBallSocketJoints.clear();

    // Spring joints
    for (ECS::Entity entity : m_World->GetEntitiesWithComponent<ECS::SpringJointComponent>()) {
        SolveSpringJoint(entity, deltaTime);
    }
    for (ECS::Entity broken : m_BrokenSpringJoints) {
        if (m_World->HasComponent<ECS::SpringJointComponent>(broken))
            m_World->RemoveComponent<ECS::SpringJointComponent>(broken);
    }
    m_BrokenSpringJoints.clear();

    // Fixed joints
    for (ECS::Entity entity : m_World->GetEntitiesWithComponent<ECS::FixedJointComponent>()) {
        SolveFixedJoint(entity, deltaTime);
    }
    for (ECS::Entity broken : m_BrokenFixedJoints) {
        if (m_World->HasComponent<ECS::FixedJointComponent>(broken))
            m_World->RemoveComponent<ECS::FixedJointComponent>(broken);
    }
    m_BrokenFixedJoints.clear();

    // Slider joints
    for (ECS::Entity entity : m_World->GetEntitiesWithComponent<ECS::SliderJointComponent>()) {
        SolveSliderJoint(entity, deltaTime);
    }
    for (ECS::Entity broken : m_BrokenSliderJoints) {
        if (m_World->HasComponent<ECS::SliderJointComponent>(broken))
            m_World->RemoveComponent<ECS::SliderJointComponent>(broken);
    }
    m_BrokenSliderJoints.clear();
}

// ---------------------------------------------------------------------------
// Contact constraint solver (normal + friction impulses)
// ---------------------------------------------------------------------------

void ConstraintSolver::SolveContactConstraints(f32 deltaTime) {
    for (auto& contact : m_Contacts) {
        // Re-fetch components each iteration — entities may have been destroyed by
        // broken joint removal earlier in the same solve pass. Each individual access
        // below is guarded (rbA && ...) / (rbB && ...) so one-sided contacts are safe.
        auto* rbA = m_World->GetComponent<ECS::RigidbodyComponent>(contact.entityA);
        auto* rbB = m_World->GetComponent<ECS::RigidbodyComponent>(contact.entityB);
        if (!rbA && !rbB) continue;

        f32 invMassA = 0.0f;
        f32 invMassB = 0.0f;
        Math::Vector3 velA(0, 0, 0);
        Math::Vector3 velB(0, 0, 0);

        if (rbA && rbA->bodyType == ECS::RigidbodyComponent::BodyType::Dynamic && rbA->mass > 0.0f) {
            invMassA = 1.0f / rbA->mass;
            velA = rbA->velocity;
        }
        if (rbB && rbB->bodyType == ECS::RigidbodyComponent::BodyType::Dynamic && rbB->mass > 0.0f) {
            invMassB = 1.0f / rbB->mass;
            velB = rbB->velocity;
        }

        f32 invMassSum = invMassA + invMassB;
        if (invMassSum <= 0.0f) continue;  // Both immovable

        // Relative velocity at contact point
        Math::Vector3 relVel = velB - velA;
        f32 relVelNormal = relVel.Dot(contact.contactNormal);

        // Only resolve if bodies are approaching
        if (relVelNormal > 0.0f) continue;

        // Normal impulse magnitude (restitution = 0 for stable stacking)
        f32 normalImpulseMag = -relVelNormal / invMassSum;

        // Clamp accumulated impulse (must be non-negative to prevent pulling)
        f32 oldAccumulated = contact.accumulatedNormalImpulse;
        contact.accumulatedNormalImpulse = Math::Max(oldAccumulated + normalImpulseMag, 0.0f);
        normalImpulseMag = contact.accumulatedNormalImpulse - oldAccumulated;

        // Apply normal impulse
        Math::Vector3 normalImpulse = contact.contactNormal * normalImpulseMag;
        if (rbA && rbA->bodyType == ECS::RigidbodyComponent::BodyType::Dynamic) {
            rbA->velocity = rbA->velocity - normalImpulse * invMassA;
        }
        if (rbB && rbB->bodyType == ECS::RigidbodyComponent::BodyType::Dynamic) {
            rbB->velocity = rbB->velocity + normalImpulse * invMassB;
        }

        // --- Friction impulse (tangent direction) ---
        // Re-read velocities after normal impulse
        velA = (rbA && rbA->bodyType == ECS::RigidbodyComponent::BodyType::Dynamic) ? rbA->velocity : Math::Vector3(0, 0, 0);
        velB = (rbB && rbB->bodyType == ECS::RigidbodyComponent::BodyType::Dynamic) ? rbB->velocity : Math::Vector3(0, 0, 0);
        relVel = velB - velA;

        // Tangent: component of relative velocity perpendicular to normal
        Math::Vector3 tangent = relVel - contact.contactNormal * relVel.Dot(contact.contactNormal);
        f32 tangentLen = tangent.Length();
        if (tangentLen < Math::EPSILON) continue;
        tangent = tangent * (1.0f / tangentLen);

        f32 tangentImpulseMag = -relVel.Dot(tangent) / invMassSum;

        // Coulomb friction clamp: |friction impulse| <= mu * |normal impulse|
        f32 frictionCoeff = 0.5f;  // Default friction coefficient
        f32 maxFriction = frictionCoeff * contact.accumulatedNormalImpulse;

        f32 oldTangent = contact.accumulatedTangentImpulse;
        contact.accumulatedTangentImpulse = Math::Clamp(
            oldTangent + tangentImpulseMag, -maxFriction, maxFriction);
        tangentImpulseMag = contact.accumulatedTangentImpulse - oldTangent;

        Math::Vector3 frictionImpulse = tangent * tangentImpulseMag;
        if (rbA && rbA->bodyType == ECS::RigidbodyComponent::BodyType::Dynamic) {
            rbA->velocity = rbA->velocity - frictionImpulse * invMassA;
        }
        if (rbB && rbB->bodyType == ECS::RigidbodyComponent::BodyType::Dynamic) {
            rbB->velocity = rbB->velocity + frictionImpulse * invMassB;
        }
    }
}

// ---------------------------------------------------------------------------
// Baumgarte position stabilization
// ---------------------------------------------------------------------------

void ConstraintSolver::ApplyBaumgarteStabilization(f32 deltaTime) {
    if (deltaTime <= 0.0f) return;

    f32 baumgarte = m_BaumgarteScale / deltaTime;

    // Stabilize contact penetrations
    for (auto& contact : m_Contacts) {
        if (contact.penetration <= 0.0f) continue;

        // Re-fetch all components fresh — entities may have been invalidated during solving
        auto* transformA = m_World->GetComponent<ECS::TransformComponent>(contact.entityA);
        auto* transformB = m_World->GetComponent<ECS::TransformComponent>(contact.entityB);
        if (!transformA && !transformB) continue;
        auto* rbA = m_World->GetComponent<ECS::RigidbodyComponent>(contact.entityA);
        auto* rbB = m_World->GetComponent<ECS::RigidbodyComponent>(contact.entityB);

        f32 invMassA = 0.0f;
        f32 invMassB = 0.0f;
        if (rbA && rbA->bodyType == ECS::RigidbodyComponent::BodyType::Dynamic && rbA->mass > 0.0f)
            invMassA = 1.0f / rbA->mass;
        if (rbB && rbB->bodyType == ECS::RigidbodyComponent::BodyType::Dynamic && rbB->mass > 0.0f)
            invMassB = 1.0f / rbB->mass;

        f32 invMassSum = invMassA + invMassB;
        if (invMassSum <= 0.0f) continue;

        // Slop: allow small penetration to prevent jitter
        f32 slop = 0.005f;
        f32 correction = Math::Max(contact.penetration - slop, 0.0f) * baumgarte / invMassSum;

        Math::Vector3 correctionVec = contact.contactNormal * correction;
        if (transformA && rbA && rbA->bodyType == ECS::RigidbodyComponent::BodyType::Dynamic) {
            transformA->position = transformA->position - correctionVec * invMassA;
        }
        if (transformB && rbB && rbB->bodyType == ECS::RigidbodyComponent::BodyType::Dynamic) {
            transformB->position = transformB->position + correctionVec * invMassB;
        }
    }

    // Stabilize joint anchor drift (distance, ball-socket, fixed, hinge, slider)
    for (ECS::Entity entity : m_World->GetEntitiesWithComponent<ECS::DistanceJointComponent>()) {
        auto* joint = m_World->GetComponent<ECS::DistanceJointComponent>(entity);
        if (!joint) continue;

        auto* tA = m_World->GetComponent<ECS::TransformComponent>(joint->entityA);
        auto* tB = m_World->GetComponent<ECS::TransformComponent>(joint->entityB);
        auto* rbA = m_World->GetComponent<ECS::RigidbodyComponent>(joint->entityA);
        auto* rbB = m_World->GetComponent<ECS::RigidbodyComponent>(joint->entityB);
        if (!tA || !tB) continue;

        Math::Vector3 worldAnchorA = tA->position + joint->anchorA;
        Math::Vector3 worldAnchorB = tB->position + joint->anchorB;
        Math::Vector3 delta = worldAnchorB - worldAnchorA;
        f32 dist = delta.Length();
        if (dist < Math::EPSILON) continue;

        f32 error = dist - joint->restDistance;
        if (Math::Abs(error) <= joint->tolerance) continue;

        f32 invMassA = (rbA && rbA->bodyType == ECS::RigidbodyComponent::BodyType::Dynamic && rbA->mass > 0.0f)
                        ? 1.0f / rbA->mass : 0.0f;
        f32 invMassB = (rbB && rbB->bodyType == ECS::RigidbodyComponent::BodyType::Dynamic && rbB->mass > 0.0f)
                        ? 1.0f / rbB->mass : 0.0f;
        f32 invSum = invMassA + invMassB;
        if (invSum <= 0.0f) continue;

        Math::Vector3 dir = delta * (1.0f / dist);
        f32 correction = error * baumgarte / invSum;
        Math::Vector3 corrVec = dir * correction;

        if (rbA && rbA->bodyType == ECS::RigidbodyComponent::BodyType::Dynamic)
            tA->position = tA->position + corrVec * invMassA;
        if (rbB && rbB->bodyType == ECS::RigidbodyComponent::BodyType::Dynamic)
            tB->position = tB->position - corrVec * invMassB;
    }

    for (ECS::Entity entity : m_World->GetEntitiesWithComponent<ECS::BallSocketJointComponent>()) {
        auto* joint = m_World->GetComponent<ECS::BallSocketJointComponent>(entity);
        if (!joint) continue;

        auto* tA = m_World->GetComponent<ECS::TransformComponent>(joint->entityA);
        auto* tB = m_World->GetComponent<ECS::TransformComponent>(joint->entityB);
        auto* rbA = m_World->GetComponent<ECS::RigidbodyComponent>(joint->entityA);
        auto* rbB = m_World->GetComponent<ECS::RigidbodyComponent>(joint->entityB);
        if (!tA || !tB) continue;

        Math::Vector3 worldAnchorA = tA->position + joint->anchorA;
        Math::Vector3 worldAnchorB = tB->position + joint->anchorB;
        Math::Vector3 error = worldAnchorB - worldAnchorA;
        f32 errorLen = error.Length();
        if (errorLen < Math::EPSILON) continue;

        f32 invMassA = (rbA && rbA->bodyType == ECS::RigidbodyComponent::BodyType::Dynamic && rbA->mass > 0.0f)
                        ? 1.0f / rbA->mass : 0.0f;
        f32 invMassB = (rbB && rbB->bodyType == ECS::RigidbodyComponent::BodyType::Dynamic && rbB->mass > 0.0f)
                        ? 1.0f / rbB->mass : 0.0f;
        f32 invSum = invMassA + invMassB;
        if (invSum <= 0.0f) continue;

        Math::Vector3 correction = error * (baumgarte / invSum);
        if (rbA && rbA->bodyType == ECS::RigidbodyComponent::BodyType::Dynamic)
            tA->position = tA->position + correction * invMassA;
        if (rbB && rbB->bodyType == ECS::RigidbodyComponent::BodyType::Dynamic)
            tB->position = tB->position - correction * invMassB;
    }
}

// ---------------------------------------------------------------------------
// Distance joint: maintain fixed distance between anchors
// ---------------------------------------------------------------------------

void ConstraintSolver::SolveDistanceJoint(ECS::Entity jointEntity, f32 deltaTime) {
    auto* joint = m_World->GetComponent<ECS::DistanceJointComponent>(jointEntity);
    if (!joint) return;

    auto* tA = m_World->GetComponent<ECS::TransformComponent>(joint->entityA);
    auto* tB = m_World->GetComponent<ECS::TransformComponent>(joint->entityB);
    auto* rbA = m_World->GetComponent<ECS::RigidbodyComponent>(joint->entityA);
    auto* rbB = m_World->GetComponent<ECS::RigidbodyComponent>(joint->entityB);
    if (!tA || !tB) return;

    // Compute world-space anchors
    Math::Vector3 worldAnchorA = tA->position + joint->anchorA;
    Math::Vector3 worldAnchorB = tB->position + joint->anchorB;

    Math::Vector3 delta = worldAnchorB - worldAnchorA;
    f32 currentDist = delta.Length();
    if (currentDist < Math::EPSILON) return;

    // Constraint error
    f32 error = currentDist - joint->restDistance;
    if (Math::Abs(error) <= joint->tolerance) return;

    Math::Vector3 dir = delta * (1.0f / currentDist);

    // Inverse masses
    f32 invMassA = (rbA && rbA->bodyType == ECS::RigidbodyComponent::BodyType::Dynamic && rbA->mass > 0.0f)
                    ? 1.0f / rbA->mass : 0.0f;
    f32 invMassB = (rbB && rbB->bodyType == ECS::RigidbodyComponent::BodyType::Dynamic && rbB->mass > 0.0f)
                    ? 1.0f / rbB->mass : 0.0f;
    f32 invMassSum = invMassA + invMassB;
    if (invMassSum <= 0.0f) return;

    // Relative velocity along constraint axis
    Math::Vector3 velA = rbA ? rbA->velocity : Math::Vector3(0, 0, 0);
    Math::Vector3 velB = rbB ? rbB->velocity : Math::Vector3(0, 0, 0);
    f32 relVelAlongDir = (velB - velA).Dot(dir);

    // Sequential impulse: correct velocity error with stiffness scaling
    f32 impulseMag = -(relVelAlongDir + error / deltaTime * joint->stiffness) / invMassSum;

    // Track stress for breakable joints
    joint->currentStress = Math::Abs(impulseMag);
    if (joint->breakable && joint->currentStress > joint->breakForce) {
        ENJIN_LOG_INFO(Physics, "Distance joint on entity %llu broke (stress %.2f > breakForce %.2f)",
                       static_cast<unsigned long long>(jointEntity), joint->currentStress, joint->breakForce);
        m_BrokenDistanceJoints.push_back(jointEntity);
        return;
    }

    Math::Vector3 impulse = dir * impulseMag;
    if (rbA && rbA->bodyType == ECS::RigidbodyComponent::BodyType::Dynamic)
        rbA->velocity = rbA->velocity - impulse * invMassA;
    if (rbB && rbB->bodyType == ECS::RigidbodyComponent::BodyType::Dynamic)
        rbB->velocity = rbB->velocity + impulse * invMassB;
}

// ---------------------------------------------------------------------------
// Hinge joint: allow rotation around one axis only, apply limits
// ---------------------------------------------------------------------------

void ConstraintSolver::SolveHingeJoint(ECS::Entity jointEntity, f32 deltaTime) {
    auto* joint = m_World->GetComponent<ECS::HingeJointComponent>(jointEntity);
    if (!joint) return;

    auto* tA = m_World->GetComponent<ECS::TransformComponent>(joint->entityA);
    auto* tB = m_World->GetComponent<ECS::TransformComponent>(joint->entityB);
    auto* rbA = m_World->GetComponent<ECS::RigidbodyComponent>(joint->entityA);
    auto* rbB = m_World->GetComponent<ECS::RigidbodyComponent>(joint->entityB);
    if (!tA || !tB) return;

    f32 invMassA = (rbA && rbA->bodyType == ECS::RigidbodyComponent::BodyType::Dynamic && rbA->mass > 0.0f)
                    ? 1.0f / rbA->mass : 0.0f;
    f32 invMassB = (rbB && rbB->bodyType == ECS::RigidbodyComponent::BodyType::Dynamic && rbB->mass > 0.0f)
                    ? 1.0f / rbB->mass : 0.0f;
    f32 invMassSum = invMassA + invMassB;
    if (invMassSum <= 0.0f) return;

    // --- Point-to-point constraint (keep anchors coincident) ---
    Math::Vector3 worldAnchorA = tA->position + joint->anchorA;
    Math::Vector3 worldAnchorB = tB->position + joint->anchorB;
    Math::Vector3 posError = worldAnchorB - worldAnchorA;
    f32 posErrorLen = posError.Length();

    if (posErrorLen > Math::EPSILON) {
        Math::Vector3 dir = posError * (1.0f / posErrorLen);
        Math::Vector3 velA = rbA ? rbA->velocity : Math::Vector3(0, 0, 0);
        Math::Vector3 velB = rbB ? rbB->velocity : Math::Vector3(0, 0, 0);
        f32 relVel = (velB - velA).Dot(dir);

        f32 impulseMag = -(relVel + posErrorLen / deltaTime) / invMassSum;
        Math::Vector3 impulse = dir * impulseMag;

        // Track stress
        joint->currentStress = Math::Abs(impulseMag);
        if (joint->breakable && joint->currentStress > joint->breakForce) {
            ENJIN_LOG_INFO(Physics, "Hinge joint on entity %llu broke (stress %.2f > breakForce %.2f)",
                           static_cast<unsigned long long>(jointEntity), joint->currentStress, joint->breakForce);
            m_BrokenHingeJoints.push_back(jointEntity);
            return;
        }

        if (rbA && rbA->bodyType == ECS::RigidbodyComponent::BodyType::Dynamic)
            rbA->velocity = rbA->velocity - impulse * invMassA;
        if (rbB && rbB->bodyType == ECS::RigidbodyComponent::BodyType::Dynamic)
            rbB->velocity = rbB->velocity + impulse * invMassB;
    }

    // --- Angular constraint: remove velocity components not along the hinge axis ---
    Math::Vector3 axis = joint->axis.Normalized();

    // Relative angular velocity (approximate via linear velocity difference)
    Math::Vector3 angVelA = rbA ? rbA->angularVelocity : Math::Vector3(0, 0, 0);
    Math::Vector3 angVelB = rbB ? rbB->angularVelocity : Math::Vector3(0, 0, 0);
    Math::Vector3 relAngVel = angVelB - angVelA;

    // Remove angular velocity perpendicular to hinge axis
    Math::Vector3 perpAngVel = relAngVel - axis * relAngVel.Dot(axis);
    f32 perpLen = perpAngVel.Length();
    if (perpLen > Math::EPSILON) {
        // Apply corrective angular impulse to both bodies
        Math::Vector3 angCorrection = perpAngVel * 0.5f;
        if (rbA && rbA->bodyType == ECS::RigidbodyComponent::BodyType::Dynamic)
            rbA->angularVelocity = rbA->angularVelocity + angCorrection;
        if (rbB && rbB->bodyType == ECS::RigidbodyComponent::BodyType::Dynamic)
            rbB->angularVelocity = rbB->angularVelocity - angCorrection;
    }

    // --- Hinge angle limits ---
    if (joint->useLimits) {
        // Estimate current hinge angle from relative position projected onto plane
        Math::Vector3 relPos = tB->position - tA->position;
        Math::Vector3 projected = relPos - axis * relPos.Dot(axis);
        f32 projLen = projected.Length();

        if (projLen > Math::EPSILON) {
            // Build a reference frame: use world up or forward to get a reference direction
            Math::Vector3 ref(0, 1, 0);
            if (Math::Abs(axis.Dot(ref)) > 0.9f) ref = Math::Vector3(1, 0, 0);
            Math::Vector3 refDir = (ref - axis * ref.Dot(axis)).Normalized();
            Math::Vector3 perpDir = axis.Cross(refDir).Normalized();

            f32 angle = Math::Degrees(Math::Atan2(projected.Dot(perpDir), projected.Dot(refDir)));
            joint->currentAngle = angle;

            // Clamp to limits
            if (angle < joint->lowerLimit) {
                f32 violation = joint->lowerLimit - angle;
                f32 correctionTorque = Math::Radians(violation) / deltaTime;
                Math::Vector3 torqueImpulse = axis * correctionTorque * 0.5f;
                if (rbA && rbA->bodyType == ECS::RigidbodyComponent::BodyType::Dynamic)
                    rbA->angularVelocity = rbA->angularVelocity - torqueImpulse * invMassA;
                if (rbB && rbB->bodyType == ECS::RigidbodyComponent::BodyType::Dynamic)
                    rbB->angularVelocity = rbB->angularVelocity + torqueImpulse * invMassB;
            } else if (angle > joint->upperLimit) {
                f32 violation = angle - joint->upperLimit;
                f32 correctionTorque = Math::Radians(violation) / deltaTime;
                Math::Vector3 torqueImpulse = axis * correctionTorque * 0.5f;
                if (rbA && rbA->bodyType == ECS::RigidbodyComponent::BodyType::Dynamic)
                    rbA->angularVelocity = rbA->angularVelocity + torqueImpulse * invMassA;
                if (rbB && rbB->bodyType == ECS::RigidbodyComponent::BodyType::Dynamic)
                    rbB->angularVelocity = rbB->angularVelocity - torqueImpulse * invMassB;
            }
        }
    }

    // --- Motor ---
    if (joint->useMotor) {
        f32 targetAngVel = Math::Radians(joint->motorSpeed);
        Math::Vector3 angVelOnAxis = axis * (angVelB - angVelA).Dot(axis);
        f32 currentAngVel = angVelOnAxis.Dot(axis);
        f32 motorError = targetAngVel - currentAngVel;

        f32 motorImpulse = Math::Clamp(motorError, -joint->motorMaxForce * deltaTime, joint->motorMaxForce * deltaTime);
        Math::Vector3 torqueImpulse = axis * motorImpulse;

        if (rbA && rbA->bodyType == ECS::RigidbodyComponent::BodyType::Dynamic)
            rbA->angularVelocity = rbA->angularVelocity - torqueImpulse * invMassA;
        if (rbB && rbB->bodyType == ECS::RigidbodyComponent::BodyType::Dynamic)
            rbB->angularVelocity = rbB->angularVelocity + torqueImpulse * invMassB;
    }
}

// ---------------------------------------------------------------------------
// Ball-socket joint: keep anchor points coincident (3 DOF rotation)
// ---------------------------------------------------------------------------

void ConstraintSolver::SolveBallSocketJoint(ECS::Entity jointEntity, f32 deltaTime) {
    auto* joint = m_World->GetComponent<ECS::BallSocketJointComponent>(jointEntity);
    if (!joint) return;

    auto* tA = m_World->GetComponent<ECS::TransformComponent>(joint->entityA);
    auto* tB = m_World->GetComponent<ECS::TransformComponent>(joint->entityB);
    auto* rbA = m_World->GetComponent<ECS::RigidbodyComponent>(joint->entityA);
    auto* rbB = m_World->GetComponent<ECS::RigidbodyComponent>(joint->entityB);
    if (!tA || !tB) return;

    f32 invMassA = (rbA && rbA->bodyType == ECS::RigidbodyComponent::BodyType::Dynamic && rbA->mass > 0.0f)
                    ? 1.0f / rbA->mass : 0.0f;
    f32 invMassB = (rbB && rbB->bodyType == ECS::RigidbodyComponent::BodyType::Dynamic && rbB->mass > 0.0f)
                    ? 1.0f / rbB->mass : 0.0f;
    f32 invMassSum = invMassA + invMassB;
    if (invMassSum <= 0.0f) return;

    // Point-to-point: solve each axis independently for stability
    Math::Vector3 worldAnchorA = tA->position + joint->anchorA;
    Math::Vector3 worldAnchorB = tB->position + joint->anchorB;
    Math::Vector3 posError = worldAnchorB - worldAnchorA;

    Math::Vector3 velA = rbA ? rbA->velocity : Math::Vector3(0, 0, 0);
    Math::Vector3 velB = rbB ? rbB->velocity : Math::Vector3(0, 0, 0);
    Math::Vector3 relVel = velB - velA;

    // Solve constraint along each world axis for better convergence
    Math::Vector3 axes[3] = {
        Math::Vector3(1, 0, 0),
        Math::Vector3(0, 1, 0),
        Math::Vector3(0, 0, 1)
    };

    f32 totalStress = 0.0f;

    for (i32 a = 0; a < 3; ++a) {
        f32 err = posError.Dot(axes[a]);
        f32 vel = relVel.Dot(axes[a]);

        f32 impulseMag = -(vel + err / deltaTime) / invMassSum;
        totalStress += Math::Abs(impulseMag);

        Math::Vector3 impulse = axes[a] * impulseMag;
        if (rbA && rbA->bodyType == ECS::RigidbodyComponent::BodyType::Dynamic)
            rbA->velocity = rbA->velocity - impulse * invMassA;
        if (rbB && rbB->bodyType == ECS::RigidbodyComponent::BodyType::Dynamic)
            rbB->velocity = rbB->velocity + impulse * invMassB;

        // Re-read for next axis
        velA = rbA ? rbA->velocity : Math::Vector3(0, 0, 0);
        velB = rbB ? rbB->velocity : Math::Vector3(0, 0, 0);
        relVel = velB - velA;
    }

    joint->currentStress = totalStress;
    if (joint->breakable && joint->currentStress > joint->breakForce) {
        ENJIN_LOG_INFO(Physics, "Ball-socket joint on entity %llu broke (stress %.2f > breakForce %.2f)",
                       static_cast<unsigned long long>(jointEntity), joint->currentStress, joint->breakForce);
        m_BrokenBallSocketJoints.push_back(jointEntity);
        return;
    }

    // --- Cone limit ---
    if (joint->useConeLimit) {
        Math::Vector3 connectionAxis = (tB->position - tA->position);
        f32 connectionLen = connectionAxis.Length();
        if (connectionLen > Math::EPSILON) {
            connectionAxis = connectionAxis * (1.0f / connectionLen);

            // Reference axis: use body A's local Y as the rest direction
            Math::Vector3 restAxis(0, 1, 0);
            f32 angle = Math::Degrees(Math::Acos(Math::Clamp(connectionAxis.Dot(restAxis), -1.0f, 1.0f)));

            if (angle > joint->coneAngleLimit) {
                // Push bodies back within the cone
                Math::Vector3 rotAxis = restAxis.Cross(connectionAxis);
                f32 rotAxisLen = rotAxis.Length();
                if (rotAxisLen > Math::EPSILON) {
                    rotAxis = rotAxis * (1.0f / rotAxisLen);
                    f32 violation = Math::Radians(angle - joint->coneAngleLimit);
                    Math::Vector3 correction = rotAxis * violation / deltaTime * 0.5f;

                    if (rbA && rbA->bodyType == ECS::RigidbodyComponent::BodyType::Dynamic)
                        rbA->angularVelocity = rbA->angularVelocity + correction * invMassA;
                    if (rbB && rbB->bodyType == ECS::RigidbodyComponent::BodyType::Dynamic)
                        rbB->angularVelocity = rbB->angularVelocity - correction * invMassB;
                }
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Spring joint: apply Hooke's law force based on distance
// ---------------------------------------------------------------------------

void ConstraintSolver::SolveSpringJoint(ECS::Entity jointEntity, f32 deltaTime) {
    auto* joint = m_World->GetComponent<ECS::SpringJointComponent>(jointEntity);
    if (!joint) return;

    auto* tA = m_World->GetComponent<ECS::TransformComponent>(joint->entityA);
    auto* tB = m_World->GetComponent<ECS::TransformComponent>(joint->entityB);
    auto* rbA = m_World->GetComponent<ECS::RigidbodyComponent>(joint->entityA);
    auto* rbB = m_World->GetComponent<ECS::RigidbodyComponent>(joint->entityB);
    if (!tA || !tB) return;

    Math::Vector3 worldAnchorA = tA->position + joint->anchorA;
    Math::Vector3 worldAnchorB = tB->position + joint->anchorB;
    Math::Vector3 delta = worldAnchorB - worldAnchorA;
    f32 currentDist = delta.Length();

    if (currentDist < Math::EPSILON) return;

    Math::Vector3 dir = delta * (1.0f / currentDist);

    // Distance limits
    f32 effectiveDist = currentDist;
    if (joint->minDistance > 0.0f && effectiveDist < joint->minDistance)
        effectiveDist = joint->minDistance;
    if (joint->maxDistance > 0.0f && effectiveDist > joint->maxDistance)
        effectiveDist = joint->maxDistance;

    // Hooke's law: F = -k * (x - restLength)
    f32 displacement = currentDist - joint->restLength;
    f32 springForce = joint->springConstant * displacement;

    // Damping: F_damp = -c * v_rel_along_spring
    Math::Vector3 velA = rbA ? rbA->velocity : Math::Vector3(0, 0, 0);
    Math::Vector3 velB = rbB ? rbB->velocity : Math::Vector3(0, 0, 0);
    f32 relVelAlongSpring = (velB - velA).Dot(dir);
    f32 dampingForce = joint->dampingCoefficient * relVelAlongSpring;

    f32 totalForce = springForce + dampingForce;

    // Track stress
    joint->currentStress = Math::Abs(totalForce);
    if (joint->breakable && joint->currentStress > joint->breakForce) {
        ENJIN_LOG_INFO(Physics, "Spring joint on entity %llu broke (stress %.2f > breakForce %.2f)",
                       static_cast<unsigned long long>(jointEntity), joint->currentStress, joint->breakForce);
        // Defer removal — removing during iteration invalidates the entity list
        m_BrokenSpringJoints.push_back(jointEntity);
        return;
    }

    // Convert force to impulse: impulse = force * dt
    Math::Vector3 impulse = dir * totalForce * deltaTime;

    // Apply to bodies (spring pulls A toward B and B toward A)
    if (rbA && rbA->bodyType == ECS::RigidbodyComponent::BodyType::Dynamic && rbA->mass > 0.0f)
        rbA->velocity = rbA->velocity + impulse * (1.0f / rbA->mass);
    if (rbB && rbB->bodyType == ECS::RigidbodyComponent::BodyType::Dynamic && rbB->mass > 0.0f)
        rbB->velocity = rbB->velocity - impulse * (1.0f / rbB->mass);
}

// ---------------------------------------------------------------------------
// Fixed joint: keep relative transform constant
// ---------------------------------------------------------------------------

void ConstraintSolver::SolveFixedJoint(ECS::Entity jointEntity, f32 deltaTime) {
    auto* joint = m_World->GetComponent<ECS::FixedJointComponent>(jointEntity);
    if (!joint) return;

    auto* tA = m_World->GetComponent<ECS::TransformComponent>(joint->entityA);
    auto* tB = m_World->GetComponent<ECS::TransformComponent>(joint->entityB);
    auto* rbA = m_World->GetComponent<ECS::RigidbodyComponent>(joint->entityA);
    auto* rbB = m_World->GetComponent<ECS::RigidbodyComponent>(joint->entityB);
    if (!tA || !tB) return;

    // Capture initial relative transform on first solve
    if (!joint->initialized) {
        joint->relativePosition = tB->position - tA->position;
        // Store rotation difference as Euler (simplified; sufficient for fixed joints)
        joint->relativeRotation = Math::Vector3(
            tB->rotation.ToEuler().x - tA->rotation.ToEuler().x,
            tB->rotation.ToEuler().y - tA->rotation.ToEuler().y,
            tB->rotation.ToEuler().z - tA->rotation.ToEuler().z
        );
        joint->initialized = true;
        return;
    }

    f32 invMassA = (rbA && rbA->bodyType == ECS::RigidbodyComponent::BodyType::Dynamic && rbA->mass > 0.0f)
                    ? 1.0f / rbA->mass : 0.0f;
    f32 invMassB = (rbB && rbB->bodyType == ECS::RigidbodyComponent::BodyType::Dynamic && rbB->mass > 0.0f)
                    ? 1.0f / rbB->mass : 0.0f;
    f32 invMassSum = invMassA + invMassB;
    if (invMassSum <= 0.0f) return;

    // --- Position constraint ---
    Math::Vector3 targetPosB = tA->position + joint->relativePosition;
    Math::Vector3 posError = targetPosB - tB->position;

    Math::Vector3 velA = rbA ? rbA->velocity : Math::Vector3(0, 0, 0);
    Math::Vector3 velB = rbB ? rbB->velocity : Math::Vector3(0, 0, 0);

    f32 totalStress = 0.0f;

    // Solve each axis
    Math::Vector3 axes[3] = {
        Math::Vector3(1, 0, 0),
        Math::Vector3(0, 1, 0),
        Math::Vector3(0, 0, 1)
    };

    for (i32 a = 0; a < 3; ++a) {
        f32 err = posError.Dot(axes[a]);
        Math::Vector3 relVel = velB - velA;
        f32 vel = relVel.Dot(axes[a]);

        // The correction should push B toward the target (note sign: error = target - current)
        f32 impulseMag = (vel - err / deltaTime) / invMassSum;
        totalStress += Math::Abs(impulseMag);

        Math::Vector3 impulse = axes[a] * impulseMag;
        if (rbA && rbA->bodyType == ECS::RigidbodyComponent::BodyType::Dynamic)
            rbA->velocity = rbA->velocity + impulse * invMassA;
        if (rbB && rbB->bodyType == ECS::RigidbodyComponent::BodyType::Dynamic)
            rbB->velocity = rbB->velocity - impulse * invMassB;

        // Re-read for next axis
        velA = rbA ? rbA->velocity : Math::Vector3(0, 0, 0);
        velB = rbB ? rbB->velocity : Math::Vector3(0, 0, 0);
    }

    // --- Angular constraint: match angular velocities to prevent relative rotation ---
    Math::Vector3 angVelA = rbA ? rbA->angularVelocity : Math::Vector3(0, 0, 0);
    Math::Vector3 angVelB = rbB ? rbB->angularVelocity : Math::Vector3(0, 0, 0);
    Math::Vector3 relAngVel = angVelB - angVelA;
    f32 relAngLen = relAngVel.Length();

    if (relAngLen > Math::EPSILON) {
        Math::Vector3 angCorrection = relAngVel * 0.5f;
        totalStress += relAngLen;

        if (rbA && rbA->bodyType == ECS::RigidbodyComponent::BodyType::Dynamic)
            rbA->angularVelocity = rbA->angularVelocity + angCorrection;
        if (rbB && rbB->bodyType == ECS::RigidbodyComponent::BodyType::Dynamic)
            rbB->angularVelocity = rbB->angularVelocity - angCorrection;
    }

    joint->currentStress = totalStress;
    if (joint->breakable && joint->currentStress > joint->breakForce) {
        ENJIN_LOG_INFO(Physics, "Fixed joint on entity %llu broke (stress %.2f > breakForce %.2f)",
                       static_cast<unsigned long long>(jointEntity), joint->currentStress, joint->breakForce);
        m_BrokenFixedJoints.push_back(jointEntity);
    }
}

// ---------------------------------------------------------------------------
// Slider joint: constrain to one axis of movement
// ---------------------------------------------------------------------------

void ConstraintSolver::SolveSliderJoint(ECS::Entity jointEntity, f32 deltaTime) {
    auto* joint = m_World->GetComponent<ECS::SliderJointComponent>(jointEntity);
    if (!joint) return;

    auto* tA = m_World->GetComponent<ECS::TransformComponent>(joint->entityA);
    auto* tB = m_World->GetComponent<ECS::TransformComponent>(joint->entityB);
    auto* rbA = m_World->GetComponent<ECS::RigidbodyComponent>(joint->entityA);
    auto* rbB = m_World->GetComponent<ECS::RigidbodyComponent>(joint->entityB);
    if (!tA || !tB) return;

    f32 invMassA = (rbA && rbA->bodyType == ECS::RigidbodyComponent::BodyType::Dynamic && rbA->mass > 0.0f)
                    ? 1.0f / rbA->mass : 0.0f;
    f32 invMassB = (rbB && rbB->bodyType == ECS::RigidbodyComponent::BodyType::Dynamic && rbB->mass > 0.0f)
                    ? 1.0f / rbB->mass : 0.0f;
    f32 invMassSum = invMassA + invMassB;
    if (invMassSum <= 0.0f) return;

    Math::Vector3 slideAxis = joint->slideAxis.Normalized();
    if (slideAxis.LengthSquared() < Math::EPSILON) return;

    // World-space anchors
    Math::Vector3 worldAnchorA = tA->position + joint->anchorA;
    Math::Vector3 worldAnchorB = tB->position + joint->anchorB;
    Math::Vector3 delta = worldAnchorB - worldAnchorA;

    // Decompose relative position into slide-axis component and off-axis error
    f32 displacement = delta.Dot(slideAxis);
    Math::Vector3 onAxis = slideAxis * displacement;
    Math::Vector3 offAxisError = delta - onAxis;

    // Track current displacement
    joint->currentDisplacement = displacement;

    f32 totalStress = 0.0f;

    // --- Remove off-axis motion (constrain to slide axis) ---
    f32 offAxisLen = offAxisError.Length();
    if (offAxisLen > Math::EPSILON) {
        Math::Vector3 offDir = offAxisError * (1.0f / offAxisLen);

        Math::Vector3 velA = rbA ? rbA->velocity : Math::Vector3(0, 0, 0);
        Math::Vector3 velB = rbB ? rbB->velocity : Math::Vector3(0, 0, 0);
        f32 relVel = (velB - velA).Dot(offDir);

        f32 impulseMag = -(relVel + offAxisLen / deltaTime) / invMassSum;
        totalStress += Math::Abs(impulseMag);

        Math::Vector3 impulse = offDir * impulseMag;
        if (rbA && rbA->bodyType == ECS::RigidbodyComponent::BodyType::Dynamic)
            rbA->velocity = rbA->velocity - impulse * invMassA;
        if (rbB && rbB->bodyType == ECS::RigidbodyComponent::BodyType::Dynamic)
            rbB->velocity = rbB->velocity + impulse * invMassB;
    }

    // --- Slider limits ---
    if (joint->useLimits) {
        Math::Vector3 velA = rbA ? rbA->velocity : Math::Vector3(0, 0, 0);
        Math::Vector3 velB = rbB ? rbB->velocity : Math::Vector3(0, 0, 0);
        f32 relVelOnAxis = (velB - velA).Dot(slideAxis);

        if (displacement < joint->lowerLimit) {
            f32 violation = joint->lowerLimit - displacement;
            f32 impulseMag = (relVelOnAxis + violation / deltaTime) / invMassSum;
            // Only push apart (positive impulse along axis)
            if (impulseMag > 0.0f) {
                totalStress += impulseMag;
                Math::Vector3 impulse = slideAxis * impulseMag;
                if (rbA && rbA->bodyType == ECS::RigidbodyComponent::BodyType::Dynamic)
                    rbA->velocity = rbA->velocity - impulse * invMassA;
                if (rbB && rbB->bodyType == ECS::RigidbodyComponent::BodyType::Dynamic)
                    rbB->velocity = rbB->velocity + impulse * invMassB;
            }
        } else if (displacement > joint->upperLimit) {
            f32 violation = displacement - joint->upperLimit;
            f32 impulseMag = (-relVelOnAxis + violation / deltaTime) / invMassSum;
            // Only push together (negative impulse along axis)
            if (impulseMag > 0.0f) {
                totalStress += impulseMag;
                Math::Vector3 impulse = slideAxis * (-impulseMag);
                if (rbA && rbA->bodyType == ECS::RigidbodyComponent::BodyType::Dynamic)
                    rbA->velocity = rbA->velocity - impulse * invMassA;
                if (rbB && rbB->bodyType == ECS::RigidbodyComponent::BodyType::Dynamic)
                    rbB->velocity = rbB->velocity + impulse * invMassB;
            }
        }
    }

    // --- Slider motor ---
    if (joint->useMotor) {
        Math::Vector3 velA = rbA ? rbA->velocity : Math::Vector3(0, 0, 0);
        Math::Vector3 velB = rbB ? rbB->velocity : Math::Vector3(0, 0, 0);
        f32 currentVel = (velB - velA).Dot(slideAxis);
        f32 motorError = joint->motorSpeed - currentVel;

        f32 motorImpulse = Math::Clamp(motorError / invMassSum,
                                        -joint->motorMaxForce * deltaTime,
                                        joint->motorMaxForce * deltaTime);
        totalStress += Math::Abs(motorImpulse);

        Math::Vector3 impulse = slideAxis * motorImpulse;
        if (rbA && rbA->bodyType == ECS::RigidbodyComponent::BodyType::Dynamic)
            rbA->velocity = rbA->velocity - impulse * invMassA;
        if (rbB && rbB->bodyType == ECS::RigidbodyComponent::BodyType::Dynamic)
            rbB->velocity = rbB->velocity + impulse * invMassB;
    }

    // --- Remove relative angular velocity (slider should not rotate) ---
    Math::Vector3 angVelA = rbA ? rbA->angularVelocity : Math::Vector3(0, 0, 0);
    Math::Vector3 angVelB = rbB ? rbB->angularVelocity : Math::Vector3(0, 0, 0);
    Math::Vector3 relAngVel = angVelB - angVelA;
    f32 relAngLen = relAngVel.Length();

    if (relAngLen > Math::EPSILON) {
        Math::Vector3 angCorrection = relAngVel * 0.5f;
        if (rbA && rbA->bodyType == ECS::RigidbodyComponent::BodyType::Dynamic)
            rbA->angularVelocity = rbA->angularVelocity + angCorrection;
        if (rbB && rbB->bodyType == ECS::RigidbodyComponent::BodyType::Dynamic)
            rbB->angularVelocity = rbB->angularVelocity - angCorrection;
    }

    joint->currentStress = totalStress;
    if (joint->breakable && joint->currentStress > joint->breakForce) {
        ENJIN_LOG_INFO(Physics, "Slider joint on entity %llu broke (stress %.2f > breakForce %.2f)",
                       static_cast<unsigned long long>(jointEntity), joint->currentStress, joint->breakForce);
        m_BrokenSliderJoints.push_back(jointEntity);
    }
}

} // namespace Physics
} // namespace Enjin
