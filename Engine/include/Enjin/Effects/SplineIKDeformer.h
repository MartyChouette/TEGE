#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/Math/Quaternion.h"
#include "Enjin/Math/Matrix.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Mesh.h"
#include <vector>

namespace Enjin {
namespace Effects {

// A single joint in the IK chain
struct IKJoint {
    Math::Vector3 position;
    Math::Quaternion rotation;
    f32 length = 0.625f;           // Distance to next joint
    f32 stiffness = 1.0f;          // Resistance to bending (0=floppy, 1=rigid)
    f32 damping = 0.1f;            // Velocity damping
    Math::Vector3 velocity = {0.0f, 0.0f, 0.0f};  // For physics simulation
};

// Spline IK component — attach to an entity for tentacles, ropes, tails, vines
struct SplineIKComponent {
    std::vector<IKJoint> joints;
    i32 jointCount = 8;                // Number of joints in chain
    f32 totalLength = 5.0f;            // Total chain length
    f32 gravity = -9.81f;              // Gravity force
    f32 stiffness = 0.5f;             // Global stiffness override
    f32 damping = 0.1f;               // Global damping
    f32 meshRadius = 0.2f;            // Radius at root
    f32 meshRadiusTip = 0.05f;        // Radius at tip (taper)
    i32 meshSegments = 8;             // Radial segments for tube mesh

    // Control
    Math::Vector3 targetPosition = {0.0f, 0.0f, 0.0f}; // IK target
    bool useTarget = false;            // FABRIK toward target
    bool useGravity = true;
    bool usePhysics = true;            // Enable spring-mass physics

    // Appearance
    enum class DeformMode : u8 { Tube, Ribbon, Custom } mode = DeformMode::Tube;
    f32 uvTiling = 1.0f;              // Texture tiling along length

    // Runtime state
    bool initialized = false;
};

// Intermediate mesh data produced by the deformer
struct SplineMeshData {
    std::vector<ECS::MeshComponent::Vertex> vertices;
    std::vector<u32> indices;
};

// Point along a subdivided spline
struct SplinePoint {
    Math::Vector3 position;
    Math::Vector3 tangent;
    f32 radius;
};

// Spline IK system — simulates physics, solves IK, and generates tube/ribbon meshes
class ENJIN_API SplineIKSystem {
public:
    SplineIKSystem() = default;
    ~SplineIKSystem() = default;

    // Update all SplineIKComponent entities: physics, IK, mesh generation
    void Update(ECS::World* world, f32 deltaTime);

    // Generate a tube mesh from a joint chain
    static SplineMeshData GenerateTubeMesh(const SplineIKComponent& chain);

    // Generate a ribbon mesh from a joint chain
    static SplineMeshData GenerateRibbonMesh(const SplineIKComponent& chain);

    // Distance constraint solver (Verlet relaxation)
    static void SolveConstraints(std::vector<IKJoint>& joints, i32 iterations = 5);

    // --- Spline interpolation helpers ---

    // Catmull-Rom spline interpolation between 4 points
    static Math::Vector3 CatmullRomPoint(const Math::Vector3& p0,
                                          const Math::Vector3& p1,
                                          const Math::Vector3& p2,
                                          const Math::Vector3& p3,
                                          f32 t);

    // Evaluate the spline at parameter t [0, 1] along the entire chain
    // Returns position and tangent
    static void EvaluateSpline(const std::vector<IKJoint>& joints,
                               f32 t,
                               Math::Vector3& outPosition,
                               Math::Vector3& outTangent);

    // Subdivide the joint chain into smooth spline points
    static std::vector<SplinePoint> SubdivideSpline(const SplineIKComponent& chain,
                                                     i32 subdivisions = 4);

    // Stats
    u32 GetActiveChainCount() const { return m_ActiveChainCount; }
    u32 GetTotalJointCount() const { return m_TotalJointCount; }

private:
    // Initialize a chain's joints from its configuration
    static void InitializeChain(SplineIKComponent& chain, const Math::Vector3& rootPosition);

    // Physics simulation step: gravity, damping, velocity integration
    static void PhysicsStep(SplineIKComponent& chain, f32 deltaTime);

    // Forward And Backward Reaching Inverse Kinematics
    static void FABRIKSolve(std::vector<IKJoint>& joints,
                             const Math::Vector3& rootPosition,
                             const Math::Vector3& targetPosition,
                             i32 iterations = 10);

    // Compute rotation for each joint to face the next joint in chain
    static void ComputeJointRotations(std::vector<IKJoint>& joints);

    // Build a LookRotation quaternion from forward and up vectors
    static Math::Quaternion LookRotation(const Math::Vector3& forward,
                                          const Math::Vector3& up);

    u32 m_ActiveChainCount = 0;
    u32 m_TotalJointCount = 0;
};

} // namespace Effects
} // namespace Enjin
