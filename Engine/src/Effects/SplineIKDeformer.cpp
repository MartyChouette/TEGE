#include "Enjin/Effects/SplineIKDeformer.h"
#include <cmath>
#include <algorithm>

namespace Enjin {
namespace Effects {

// ---------------------------------------------------------------------------
// LookRotation — build quaternion from forward + up (like Unity's Quaternion.LookRotation)
// ---------------------------------------------------------------------------
Math::Quaternion SplineIKSystem::LookRotation(const Math::Vector3& forward,
                                               const Math::Vector3& up) {
    Math::Vector3 fwd = forward.Normalized();

    // Handle degenerate cases
    if (fwd.LengthSquared() < Math::EPSILON) {
        return Math::Quaternion::Identity();
    }

    // Gram-Schmidt orthogonalization to get right and true up
    Math::Vector3 right = up.Cross(fwd).Normalized();
    if (right.LengthSquared() < Math::EPSILON) {
        // Forward and up are parallel; pick an arbitrary perpendicular
        Math::Vector3 altUp = (Math::Abs(fwd.y) < 0.999f)
            ? Math::Vector3(0.0f, 1.0f, 0.0f)
            : Math::Vector3(1.0f, 0.0f, 0.0f);
        right = altUp.Cross(fwd).Normalized();
    }
    Math::Vector3 trueUp = fwd.Cross(right);

    // Build rotation matrix and convert to quaternion
    // Column-major: col0 = right, col1 = trueUp, col2 = forward
    f32 m00 = right.x,   m01 = trueUp.x, m02 = fwd.x;
    f32 m10 = right.y,   m11 = trueUp.y, m12 = fwd.y;
    f32 m20 = right.z,   m21 = trueUp.z, m22 = fwd.z;

    // Shepperd's method for robust matrix-to-quaternion conversion
    f32 trace = m00 + m11 + m22;
    Math::Quaternion q;

    if (trace > 0.0f) {
        f32 s = Math::Sqrt(trace + 1.0f) * 2.0f;
        q.w = 0.25f * s;
        q.x = (m21 - m12) / s;
        q.y = (m02 - m20) / s;
        q.z = (m10 - m01) / s;
    } else if (m00 > m11 && m00 > m22) {
        f32 s = Math::Sqrt(1.0f + m00 - m11 - m22) * 2.0f;
        q.w = (m21 - m12) / s;
        q.x = 0.25f * s;
        q.y = (m01 + m10) / s;
        q.z = (m02 + m20) / s;
    } else if (m11 > m22) {
        f32 s = Math::Sqrt(1.0f + m11 - m00 - m22) * 2.0f;
        q.w = (m02 - m20) / s;
        q.x = (m01 + m10) / s;
        q.y = 0.25f * s;
        q.z = (m12 + m21) / s;
    } else {
        f32 s = Math::Sqrt(1.0f + m22 - m00 - m11) * 2.0f;
        q.w = (m10 - m01) / s;
        q.x = (m02 + m20) / s;
        q.y = (m12 + m21) / s;
        q.z = 0.25f * s;
    }

    return q.Normalized();
}

// ---------------------------------------------------------------------------
// InitializeChain — create evenly-spaced joints descending from root
// ---------------------------------------------------------------------------
void SplineIKSystem::InitializeChain(SplineIKComponent& chain,
                                      const Math::Vector3& rootPosition) {
    chain.joints.resize(static_cast<usize>(chain.jointCount));

    f32 segmentLength = (chain.jointCount > 1)
        ? chain.totalLength / static_cast<f32>(chain.jointCount - 1)
        : chain.totalLength;

    for (i32 i = 0; i < chain.jointCount; ++i) {
        IKJoint& joint = chain.joints[static_cast<usize>(i)];
        // Initialize hanging straight down (gravity direction)
        joint.position = rootPosition + Math::Vector3(0.0f, -segmentLength * static_cast<f32>(i), 0.0f);
        joint.rotation = Math::Quaternion::Identity();
        joint.length = segmentLength;
        joint.stiffness = chain.stiffness;
        joint.damping = chain.damping;
        joint.velocity = Math::Vector3(0.0f, 0.0f, 0.0f);
    }

    chain.initialized = true;
}

// ---------------------------------------------------------------------------
// PhysicsStep — Verlet-style integration with gravity and damping
// ---------------------------------------------------------------------------
void SplineIKSystem::PhysicsStep(SplineIKComponent& chain, f32 deltaTime) {
    if (!chain.usePhysics || chain.joints.empty()) return;

    // Root joint is pinned to entity transform (updated separately)
    // Simulate from joint 1 onward
    for (usize i = 1; i < chain.joints.size(); ++i) {
        IKJoint& joint = chain.joints[i];

        // Apply gravity
        if (chain.useGravity) {
            joint.velocity.y += chain.gravity * deltaTime;
        }

        // Stiffness: pull toward rest direction (straight from previous joint)
        f32 stiff = joint.stiffness * chain.stiffness;
        if (stiff > 0.0f && i > 0) {
            const IKJoint& prev = chain.joints[i - 1];
            // Rest position: directly below/behind previous joint
            Math::Vector3 restDir(0.0f, -1.0f, 0.0f);
            if (i >= 2) {
                // Use direction from two joints back as rest direction
                Math::Vector3 prevDir = prev.position - chain.joints[i - 2].position;
                if (prevDir.LengthSquared() > Math::EPSILON) {
                    restDir = prevDir.Normalized();
                }
            }
            Math::Vector3 restPos = prev.position + restDir * joint.length;
            Math::Vector3 restForce = (restPos - joint.position) * stiff * 10.0f;
            joint.velocity += restForce * deltaTime;
        }

        // Damping
        f32 dampFactor = 1.0f - Math::Clamp(joint.damping + chain.damping, 0.0f, 1.0f) * Math::Min(deltaTime * 10.0f, 1.0f);
        joint.velocity *= dampFactor;

        // Integrate position
        joint.position += joint.velocity * deltaTime;
    }

    // Constraint relaxation (distance constraints between adjacent joints)
    SolveConstraints(chain.joints, 5);
}

// ---------------------------------------------------------------------------
// SolveConstraints — enforce distance constraints between adjacent joints
// ---------------------------------------------------------------------------
void SplineIKSystem::SolveConstraints(std::vector<IKJoint>& joints, i32 iterations) {
    if (joints.size() < 2) return;

    for (i32 iter = 0; iter < iterations; ++iter) {
        for (usize i = 0; i < joints.size() - 1; ++i) {
            IKJoint& a = joints[i];
            IKJoint& b = joints[i + 1];

            Math::Vector3 delta = b.position - a.position;
            f32 dist = Math::Sqrt(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);

            if (dist < Math::EPSILON) continue;

            f32 targetDist = b.length;
            f32 error = dist - targetDist;
            Math::Vector3 correction = delta * (error / dist);

            if (i == 0) {
                // Root is pinned — only move b
                b.position -= correction;
            } else {
                // Both joints move proportionally
                f32 aWeight = 0.5f;
                f32 bWeight = 0.5f;

                // Weight by inverse stiffness (stiffer joints move less)
                f32 aStiff = a.stiffness + 0.01f;
                f32 bStiff = b.stiffness + 0.01f;
                f32 totalStiff = aStiff + bStiff;
                aWeight = bStiff / totalStiff;
                bWeight = aStiff / totalStiff;

                a.position += correction * aWeight;
                b.position -= correction * bWeight;
            }
        }
    }
}

// ---------------------------------------------------------------------------
// FABRIKSolve — Forward And Backward Reaching Inverse Kinematics
// ---------------------------------------------------------------------------
void SplineIKSystem::FABRIKSolve(std::vector<IKJoint>& joints,
                                   const Math::Vector3& rootPosition,
                                   const Math::Vector3& targetPosition,
                                   i32 iterations) {
    if (joints.size() < 2) return;

    f32 tolerance = 0.001f;

    for (i32 iter = 0; iter < iterations; ++iter) {
        // Check if we've converged
        Math::Vector3 endDiff = joints.back().position - targetPosition;
        if (endDiff.LengthSquared() < tolerance * tolerance) break;

        // --- Forward pass: end effector toward target ---
        joints.back().position = targetPosition;
        for (i32 i = static_cast<i32>(joints.size()) - 2; i >= 0; --i) {
            usize idx = static_cast<usize>(i);
            usize nextIdx = idx + 1;

            Math::Vector3 dir = joints[idx].position - joints[nextIdx].position;
            f32 dist = dir.Length();
            if (dist < Math::EPSILON) {
                dir = Math::Vector3(0.0f, 1.0f, 0.0f);
            } else {
                dir = dir * (1.0f / dist);
            }

            joints[idx].position = joints[nextIdx].position + dir * joints[nextIdx].length;
        }

        // --- Backward pass: root back to root position ---
        joints[0].position = rootPosition;
        for (usize i = 1; i < joints.size(); ++i) {
            usize prevIdx = i - 1;

            Math::Vector3 dir = joints[i].position - joints[prevIdx].position;
            f32 dist = dir.Length();
            if (dist < Math::EPSILON) {
                dir = Math::Vector3(0.0f, -1.0f, 0.0f);
            } else {
                dir = dir * (1.0f / dist);
            }

            joints[i].position = joints[prevIdx].position + dir * joints[i].length;
        }
    }
}

// ---------------------------------------------------------------------------
// ComputeJointRotations — orient each joint toward the next
// ---------------------------------------------------------------------------
void SplineIKSystem::ComputeJointRotations(std::vector<IKJoint>& joints) {
    if (joints.size() < 2) return;

    Math::Vector3 prevUp(0.0f, 1.0f, 0.0f);

    for (usize i = 0; i < joints.size() - 1; ++i) {
        Math::Vector3 forward = joints[i + 1].position - joints[i].position;
        if (forward.LengthSquared() < Math::EPSILON) {
            joints[i].rotation = Math::Quaternion::Identity();
            continue;
        }
        forward.Normalize();

        joints[i].rotation = LookRotation(forward, prevUp);

        // Use this joint's up as hint for the next to reduce twist
        prevUp = joints[i].rotation.GetUp();
    }

    // Last joint inherits previous joint's rotation
    if (joints.size() >= 2) {
        joints.back().rotation = joints[joints.size() - 2].rotation;
    }
}

// ---------------------------------------------------------------------------
// CatmullRomPoint — standard Catmull-Rom interpolation
// ---------------------------------------------------------------------------
Math::Vector3 SplineIKSystem::CatmullRomPoint(const Math::Vector3& p0,
                                                const Math::Vector3& p1,
                                                const Math::Vector3& p2,
                                                const Math::Vector3& p3,
                                                f32 t) {
    f32 t2 = t * t;
    f32 t3 = t2 * t;

    // Catmull-Rom matrix coefficients
    Math::Vector3 a = p1 * 2.0f;
    Math::Vector3 b = (p2 - p0);
    Math::Vector3 c = p0 * 2.0f - p1 * 5.0f + p2 * 4.0f - p3;
    Math::Vector3 d = (p3 - p0) + (p1 - p2) * 3.0f;

    return (a + b * t + c * t2 + d * t3) * 0.5f;
}

// ---------------------------------------------------------------------------
// EvaluateSpline — evaluate position and tangent at parameter t [0,1]
// ---------------------------------------------------------------------------
void SplineIKSystem::EvaluateSpline(const std::vector<IKJoint>& joints,
                                     f32 t,
                                     Math::Vector3& outPosition,
                                     Math::Vector3& outTangent) {
    if (joints.empty()) {
        outPosition = Math::Vector3(0.0f);
        outTangent = Math::Vector3(0.0f, 0.0f, 1.0f);
        return;
    }
    if (joints.size() == 1) {
        outPosition = joints[0].position;
        outTangent = Math::Vector3(0.0f, -1.0f, 0.0f);
        return;
    }

    t = Math::Clamp(t, 0.0f, 1.0f);

    i32 segCount = static_cast<i32>(joints.size()) - 1;
    f32 segT = t * static_cast<f32>(segCount);
    i32 seg = static_cast<i32>(Math::Floor(segT));
    f32 localT = segT - static_cast<f32>(seg);

    // Clamp segment index
    if (seg >= segCount) {
        seg = segCount - 1;
        localT = 1.0f;
    }

    // Get 4 control points for Catmull-Rom (clamped at ends)
    i32 i0 = std::max(0, seg - 1);
    i32 i1 = seg;
    i32 i2 = std::min(seg + 1, segCount);
    i32 i3 = std::min(seg + 2, segCount);

    const Math::Vector3& p0 = joints[static_cast<usize>(i0)].position;
    const Math::Vector3& p1 = joints[static_cast<usize>(i1)].position;
    const Math::Vector3& p2 = joints[static_cast<usize>(i2)].position;
    const Math::Vector3& p3 = joints[static_cast<usize>(i3)].position;

    outPosition = CatmullRomPoint(p0, p1, p2, p3, localT);

    // Compute tangent via central difference
    f32 dt = 0.001f;
    f32 tMinus = Math::Max(0.0f, localT - dt);
    f32 tPlus = Math::Min(1.0f, localT + dt);

    Math::Vector3 pMinus = CatmullRomPoint(p0, p1, p2, p3, tMinus);
    Math::Vector3 pPlus = CatmullRomPoint(p0, p1, p2, p3, tPlus);

    outTangent = (pPlus - pMinus).Normalized();
}

// ---------------------------------------------------------------------------
// SubdivideSpline — generate smooth spline points with radius interpolation
// ---------------------------------------------------------------------------
std::vector<SplinePoint> SplineIKSystem::SubdivideSpline(const SplineIKComponent& chain,
                                                          i32 subdivisions) {
    std::vector<SplinePoint> result;
    if (chain.joints.empty()) return result;

    i32 totalPoints = (static_cast<i32>(chain.joints.size()) - 1) * subdivisions + 1;
    result.reserve(static_cast<usize>(totalPoints));

    for (i32 i = 0; i < totalPoints; ++i) {
        f32 t = static_cast<f32>(i) / static_cast<f32>(std::max(1, totalPoints - 1));

        SplinePoint sp;
        EvaluateSpline(chain.joints, t, sp.position, sp.tangent);

        // Interpolate radius from root to tip
        sp.radius = Math::Lerp(chain.meshRadius, chain.meshRadiusTip, t);

        result.push_back(sp);
    }

    return result;
}

// ---------------------------------------------------------------------------
// GenerateTubeMesh — build a tube mesh along the spline
// ---------------------------------------------------------------------------
SplineMeshData SplineIKSystem::GenerateTubeMesh(const SplineIKComponent& chain) {
    SplineMeshData meshData;
    if (chain.joints.size() < 2 || chain.meshSegments < 3) return meshData;

    // Subdivide the spline for smoothness
    std::vector<SplinePoint> points = SubdivideSpline(chain, 4);
    if (points.size() < 2) return meshData;

    i32 rings = static_cast<i32>(points.size());
    i32 segs = chain.meshSegments;

    // Pre-allocate
    meshData.vertices.reserve(static_cast<usize>(rings) * static_cast<usize>(segs));
    meshData.indices.reserve(static_cast<usize>((rings - 1) * segs * 6));

    // Track accumulated distance for UV v coordinate
    f32 totalDist = 0.0f;
    std::vector<f32> distances(points.size(), 0.0f);
    for (usize i = 1; i < points.size(); ++i) {
        Math::Vector3 d = points[i].position - points[i - 1].position;
        totalDist += Math::Sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
        distances[i] = totalDist;
    }
    if (totalDist < Math::EPSILON) totalDist = 1.0f;

    Math::Vector3 prevUp(0.0f, 1.0f, 0.0f);

    for (i32 ring = 0; ring < rings; ++ring) {
        const SplinePoint& sp = points[static_cast<usize>(ring)];

        // Build local coordinate frame
        Math::Vector3 tangent = sp.tangent;
        if (tangent.LengthSquared() < Math::EPSILON) {
            tangent = Math::Vector3(0.0f, -1.0f, 0.0f);
        }

        Math::Vector3 right = prevUp.Cross(tangent).Normalized();
        if (right.LengthSquared() < Math::EPSILON) {
            Math::Vector3 altUp = (Math::Abs(tangent.y) < 0.999f)
                ? Math::Vector3(0.0f, 1.0f, 0.0f)
                : Math::Vector3(1.0f, 0.0f, 0.0f);
            right = altUp.Cross(tangent).Normalized();
        }
        Math::Vector3 up = tangent.Cross(right).Normalized();
        prevUp = up;

        f32 v = (distances[static_cast<usize>(ring)] / totalDist) * chain.uvTiling;

        for (i32 seg = 0; seg < segs; ++seg) {
            f32 angle = static_cast<f32>(seg) / static_cast<f32>(segs) * Math::PI_2;
            f32 cosA = Math::Cos(angle);
            f32 sinA = Math::Sin(angle);

            // Position on the ring
            Math::Vector3 localOffset = right * cosA * sp.radius + up * sinA * sp.radius;
            Math::Vector3 vertPos = sp.position + localOffset;

            // Normal: outward from ring center
            Math::Vector3 normal = localOffset.Normalized();

            // UV
            f32 u = static_cast<f32>(seg) / static_cast<f32>(segs);

            // Tangent for normal mapping
            Math::Vector3 tangentDir = tangent;
            f32 handedness = 1.0f;

            ECS::MeshComponent::Vertex vert;
            vert.position = vertPos;
            vert.normal = normal;
            vert.uv = Math::Vector2(u, v);
            vert.color = Math::Vector4(1.0f, 1.0f, 1.0f, 1.0f);
            vert.tangent = Math::Vector4(tangentDir.x, tangentDir.y, tangentDir.z, handedness);

            meshData.vertices.push_back(vert);
        }
    }

    // Generate indices: connect adjacent rings with triangle strips
    for (i32 ring = 0; ring < rings - 1; ++ring) {
        for (i32 seg = 0; seg < segs; ++seg) {
            u32 current = static_cast<u32>(ring * segs + seg);
            u32 next = static_cast<u32>(ring * segs + (seg + 1) % segs);
            u32 currentNext = static_cast<u32>((ring + 1) * segs + seg);
            u32 nextNext = static_cast<u32>((ring + 1) * segs + (seg + 1) % segs);

            // Two triangles per quad
            meshData.indices.push_back(current);
            meshData.indices.push_back(currentNext);
            meshData.indices.push_back(next);

            meshData.indices.push_back(next);
            meshData.indices.push_back(currentNext);
            meshData.indices.push_back(nextNext);
        }
    }

    return meshData;
}

// ---------------------------------------------------------------------------
// GenerateRibbonMesh — flat ribbon mesh along the spline
// ---------------------------------------------------------------------------
SplineMeshData SplineIKSystem::GenerateRibbonMesh(const SplineIKComponent& chain) {
    SplineMeshData meshData;
    if (chain.joints.size() < 2) return meshData;

    // Subdivide for smoothness
    std::vector<SplinePoint> points = SubdivideSpline(chain, 4);
    if (points.size() < 2) return meshData;

    i32 count = static_cast<i32>(points.size());

    meshData.vertices.reserve(static_cast<usize>(count) * 2);
    meshData.indices.reserve(static_cast<usize>((count - 1) * 6));

    // Compute accumulated distance for UV
    f32 totalDist = 0.0f;
    std::vector<f32> distances(points.size(), 0.0f);
    for (usize i = 1; i < points.size(); ++i) {
        Math::Vector3 d = points[i].position - points[i - 1].position;
        totalDist += Math::Sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
        distances[i] = totalDist;
    }
    if (totalDist < Math::EPSILON) totalDist = 1.0f;

    Math::Vector3 prevRight(1.0f, 0.0f, 0.0f);

    for (i32 i = 0; i < count; ++i) {
        const SplinePoint& sp = points[static_cast<usize>(i)];

        // Compute local right vector
        Math::Vector3 tangent = sp.tangent;
        if (tangent.LengthSquared() < Math::EPSILON) {
            tangent = Math::Vector3(0.0f, -1.0f, 0.0f);
        }

        Math::Vector3 up(0.0f, 1.0f, 0.0f);
        Math::Vector3 right = up.Cross(tangent).Normalized();
        if (right.LengthSquared() < Math::EPSILON) {
            right = prevRight;
        }
        prevRight = right;

        Math::Vector3 normal = tangent.Cross(right).Normalized();

        f32 v = (distances[static_cast<usize>(i)] / totalDist) * chain.uvTiling;

        // Left vertex
        {
            ECS::MeshComponent::Vertex vert;
            vert.position = sp.position - right * sp.radius;
            vert.normal = normal;
            vert.uv = Math::Vector2(0.0f, v);
            vert.color = Math::Vector4(1.0f, 1.0f, 1.0f, 1.0f);
            vert.tangent = Math::Vector4(tangent.x, tangent.y, tangent.z, 1.0f);
            meshData.vertices.push_back(vert);
        }

        // Right vertex
        {
            ECS::MeshComponent::Vertex vert;
            vert.position = sp.position + right * sp.radius;
            vert.normal = normal;
            vert.uv = Math::Vector2(1.0f, v);
            vert.color = Math::Vector4(1.0f, 1.0f, 1.0f, 1.0f);
            vert.tangent = Math::Vector4(tangent.x, tangent.y, tangent.z, 1.0f);
            meshData.vertices.push_back(vert);
        }
    }

    // Indices: connect adjacent pairs with two triangles
    for (i32 i = 0; i < count - 1; ++i) {
        u32 bl = static_cast<u32>(i * 2);      // Bottom-left
        u32 br = static_cast<u32>(i * 2 + 1);  // Bottom-right
        u32 tl = static_cast<u32>((i + 1) * 2);     // Top-left
        u32 tr = static_cast<u32>((i + 1) * 2 + 1); // Top-right

        meshData.indices.push_back(bl);
        meshData.indices.push_back(tl);
        meshData.indices.push_back(br);

        meshData.indices.push_back(br);
        meshData.indices.push_back(tl);
        meshData.indices.push_back(tr);
    }

    return meshData;
}

// ---------------------------------------------------------------------------
// Update — main update loop for all SplineIK entities
// ---------------------------------------------------------------------------
void SplineIKSystem::Update(ECS::World* world, f32 deltaTime) {
    if (!world) return;

    m_ActiveChainCount = 0;
    m_TotalJointCount = 0;

    for (ECS::Entity entity : world->GetEntitiesWithComponent<SplineIKComponent>()) {
        auto* chain = world->GetComponent<SplineIKComponent>(entity);
        auto* transform = world->GetComponent<ECS::TransformComponent>(entity);
        if (!chain || !transform) continue;

        // Ensure valid joint count
        if (chain->jointCount < 2) chain->jointCount = 2;
        if (chain->jointCount > 128) chain->jointCount = 128;

        // Initialize if needed
        if (!chain->initialized || static_cast<i32>(chain->joints.size()) != chain->jointCount) {
            InitializeChain(*chain, transform->position);
        }

        // Pin root joint to entity transform
        chain->joints[0].position = transform->position;

        // Step 1: Physics simulation (gravity, damping, velocity integration)
        PhysicsStep(*chain, deltaTime);

        // Step 2: FABRIK solve toward target (if enabled)
        if (chain->useTarget) {
            FABRIKSolve(chain->joints, transform->position,
                         chain->targetPosition, 10);
        }

        // Step 3: Compute joint rotations
        ComputeJointRotations(chain->joints);

        // Step 4: Generate mesh and apply to MeshComponent
        SplineMeshData meshData;
        switch (chain->mode) {
            case SplineIKComponent::DeformMode::Tube:
                meshData = GenerateTubeMesh(*chain);
                break;
            case SplineIKComponent::DeformMode::Ribbon:
                meshData = GenerateRibbonMesh(*chain);
                break;
            case SplineIKComponent::DeformMode::Custom:
                // Custom mode: user provides their own mesh deformation
                // Fall back to tube for now
                meshData = GenerateTubeMesh(*chain);
                break;
        }

        // Apply generated mesh to the entity's MeshComponent
        if (!meshData.vertices.empty()) {
            auto* mesh = world->GetComponent<ECS::MeshComponent>(entity);
            if (!mesh) {
                // Create MeshComponent if it doesn't exist
                world->AddComponent<ECS::MeshComponent>(entity);
                mesh = world->GetComponent<ECS::MeshComponent>(entity);
            }
            if (mesh) {
                mesh->vertices = std::move(meshData.vertices);
                mesh->indices = std::move(meshData.indices);
                mesh->aabbDirty = true;
            }
        }

        m_ActiveChainCount++;
        m_TotalJointCount += static_cast<u32>(chain->joints.size());
    }
}

} // namespace Effects
} // namespace Enjin
