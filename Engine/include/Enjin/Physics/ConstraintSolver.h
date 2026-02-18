#pragma once

#ifdef ENJIN_PHYSICS_SIMPLE

#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/ECS/World.h"
#include <vector>

namespace Enjin {
namespace Physics {

// Forward declare
struct JointConstraint;

// Constraint contact point (for collision constraints)
struct ContactConstraint {
    ECS::Entity entityA = 0, entityB = 0;
    Math::Vector3 contactPoint;
    Math::Vector3 contactNormal;
    f32 penetration = 0.0f;
    f32 accumulatedNormalImpulse = 0.0f;  // For warm starting
    f32 accumulatedTangentImpulse = 0.0f;
};

class ENJIN_API ConstraintSolver {
public:
    ConstraintSolver();
    ~ConstraintSolver() = default;

    void SetWorld(ECS::World* world) { m_World = world; }

    // Configuration
    void SetIterationCount(u32 iterations) { m_Iterations = (iterations > 0 && iterations <= 128) ? iterations : 8; }
    u32 GetIterationCount() const { return m_Iterations; }
    void SetBaumgarteScale(f32 scale) { m_BaumgarteScale = scale; }

    // Solve all constraints for current frame
    void SolveConstraints(f32 deltaTime);

    // Add/remove contact constraints (from collision detection)
    void AddContactConstraint(const ContactConstraint& contact);
    void ClearContactConstraints();

private:
    void SolveJointConstraints(f32 deltaTime);
    void SolveContactConstraints(f32 deltaTime);
    void ApplyBaumgarteStabilization(f32 deltaTime);

    // Joint solver helpers for each type
    void SolveDistanceJoint(ECS::Entity jointEntity, f32 deltaTime);
    void SolveHingeJoint(ECS::Entity jointEntity, f32 deltaTime);
    void SolveBallSocketJoint(ECS::Entity jointEntity, f32 deltaTime);
    void SolveSpringJoint(ECS::Entity jointEntity, f32 deltaTime);
    void SolveFixedJoint(ECS::Entity jointEntity, f32 deltaTime);
    void SolveSliderJoint(ECS::Entity jointEntity, f32 deltaTime);

    ECS::World* m_World = nullptr;
    u32 m_Iterations = 8;
    f32 m_BaumgarteScale = 0.2f;
    std::vector<ContactConstraint> m_Contacts;

    // Deferred removal lists for joints that break during solving
    // (removing components during GetEntitiesWithComponent iteration is UB)
    std::vector<ECS::Entity> m_BrokenDistanceJoints;
    std::vector<ECS::Entity> m_BrokenHingeJoints;
    std::vector<ECS::Entity> m_BrokenBallSocketJoints;
    std::vector<ECS::Entity> m_BrokenSpringJoints;
    std::vector<ECS::Entity> m_BrokenFixedJoints;
    std::vector<ECS::Entity> m_BrokenSliderJoints;
};

} // namespace Physics
} // namespace Enjin

#endif // ENJIN_PHYSICS_SIMPLE
