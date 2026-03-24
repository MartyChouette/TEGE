#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/Math/Quaternion.h"
#include <vector>
#include <cmath>
#include <algorithm>

namespace Enjin {
namespace Animation {

class ENJIN_API LookAtIK {
public:
    static Math::Quaternion Solve(
        const Math::Vector3& headWorldPos,
        const Math::Vector3& targetPos,
        const Math::Quaternion& currentRotation,
        f32 maxAngleDeg,
        f32 smoothSpeed,
        f32 dt
    ) {
        Math::Vector3 toTarget = targetPos - headWorldPos;
        f32 dist = std::sqrt(toTarget.x * toTarget.x + toTarget.y * toTarget.y + toTarget.z * toTarget.z);
        if (dist < 0.001f) return currentRotation;

        Math::Vector3 dir(toTarget.x / dist, toTarget.y / dist, toTarget.z / dist);

        // Compute look-at quaternion from direction
        Math::Vector3 forward(0.0f, 0.0f, -1.0f);
        Math::Quaternion targetRot = RotationFromDirection(dir, Math::Vector3(0.0f, 1.0f, 0.0f));

        // Clamp rotation angle
        f32 maxAngleRad = maxAngleDeg * 3.14159265f / 180.0f;
        Math::Quaternion delta = targetRot * currentRotation.Conjugate();
        f32 halfAngle = std::acos(std::clamp(delta.w, -1.0f, 1.0f));
        f32 angle = halfAngle * 2.0f;

        if (angle > maxAngleRad) {
            f32 t = maxAngleRad / angle;
            targetRot = Math::Quaternion::Slerp(currentRotation, targetRot, t);
        }

        // Smooth interpolation
        f32 t = std::clamp(smoothSpeed * dt, 0.0f, 1.0f);
        return Math::Quaternion::Slerp(currentRotation, targetRot, t);
    }

    static Math::Quaternion RotationFromDirection(const Math::Vector3& forward, const Math::Vector3& up) {
        Math::Vector3 f = forward.Normalized();
        Math::Vector3 r = up.Cross(f).Normalized();
        Math::Vector3 u = f.Cross(r);

        f32 trace = r.x + u.y + f.z;
        Math::Quaternion q;
        if (trace > 0.0f) {
            f32 s = 0.5f / std::sqrt(trace + 1.0f);
            q.w = 0.25f / s;
            q.x = (u.z - f.y) * s;
            q.y = (f.x - r.z) * s;
            q.z = (r.y - u.x) * s;
        } else if (r.x > u.y && r.x > f.z) {
            f32 s = 2.0f * std::sqrt(1.0f + r.x - u.y - f.z);
            q.w = (u.z - f.y) / s;
            q.x = 0.25f * s;
            q.y = (u.x + r.y) / s;
            q.z = (f.x + r.z) / s;
        } else if (u.y > f.z) {
            f32 s = 2.0f * std::sqrt(1.0f + u.y - r.x - f.z);
            q.w = (f.x - r.z) / s;
            q.x = (u.x + r.y) / s;
            q.y = 0.25f * s;
            q.z = (f.y + u.z) / s;
        } else {
            f32 s = 2.0f * std::sqrt(1.0f + f.z - r.x - u.y);
            q.w = (r.y - u.x) / s;
            q.x = (f.x + r.z) / s;
            q.y = (f.y + u.z) / s;
            q.z = 0.25f * s;
        }
        return q.Normalized();
    }
};

class ENJIN_API FABRIK {
public:
    static void Solve(
        std::vector<Math::Vector3>& positions,
        const Math::Vector3& target,
        u32 iterations = 5
    ) {
        if (positions.size() < 2) return;

        // Compute bone lengths
        std::vector<f32> lengths(positions.size() - 1);
        f32 totalLength = 0.0f;
        for (usize i = 0; i < lengths.size(); ++i) {
            Math::Vector3 diff = positions[i + 1] - positions[i];
            lengths[i] = std::sqrt(diff.x * diff.x + diff.y * diff.y + diff.z * diff.z);
            totalLength += lengths[i];
        }

        // Check if target is reachable
        Math::Vector3 rootToTarget = target - positions[0];
        f32 distToTarget = std::sqrt(rootToTarget.x * rootToTarget.x + rootToTarget.y * rootToTarget.y + rootToTarget.z * rootToTarget.z);

        if (distToTarget > totalLength) {
            // Stretch toward target
            Math::Vector3 dir = rootToTarget * (1.0f / distToTarget);
            for (usize i = 1; i < positions.size(); ++i) {
                positions[i] = positions[i - 1] + dir * lengths[i - 1];
            }
            return;
        }

        Math::Vector3 rootPos = positions[0];

        for (u32 iter = 0; iter < iterations; ++iter) {
            // Forward pass: move end effector to target
            positions.back() = target;
            for (int i = static_cast<int>(positions.size()) - 2; i >= 0; --i) {
                Math::Vector3 dir = positions[i] - positions[i + 1];
                f32 len = std::sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
                if (len > 0.0001f) {
                    dir = dir * (1.0f / len);
                }
                positions[i] = positions[i + 1] + dir * lengths[i];
            }

            // Backward pass: maintain root position
            positions[0] = rootPos;
            for (usize i = 0; i < lengths.size(); ++i) {
                Math::Vector3 dir = positions[i + 1] - positions[i];
                f32 len = std::sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
                if (len > 0.0001f) {
                    dir = dir * (1.0f / len);
                }
                positions[i + 1] = positions[i] + dir * lengths[i];
            }
        }
    }
};

// Two-Bone IK solver - analytic solution using law of cosines.
// Used for arms (shoulder-elbow-hand) and legs (hip-knee-foot).
class ENJIN_API TwoBoneIK {
public:
    // Solve two-bone IK: compute new positions for root, mid, end given a target.
    // rootPos/midPos/endPos: current world positions of the three joints.
    // target: desired world position for the end effector.
    // poleVector: world-space direction hint for the mid joint (elbow/knee bend direction).
    // weight: 0 = keep original pose, 1 = full IK result.
    // outMidPos/outEndPos: solved world positions for mid and end joints.
    static void Solve(
        const Math::Vector3& rootPos,
        const Math::Vector3& midPos,
        const Math::Vector3& endPos,
        const Math::Vector3& target,
        const Math::Vector3& poleVector,
        f32 weight,
        Math::Vector3& outMidPos,
        Math::Vector3& outEndPos
    ) {
        // Bone lengths
        Math::Vector3 upperBone = midPos - rootPos;
        Math::Vector3 lowerBone = endPos - midPos;
        f32 upperLen = std::sqrt(upperBone.x * upperBone.x + upperBone.y * upperBone.y + upperBone.z * upperBone.z);
        f32 lowerLen = std::sqrt(lowerBone.x * lowerBone.x + lowerBone.y * lowerBone.y + lowerBone.z * lowerBone.z);

        if (upperLen < 0.0001f || lowerLen < 0.0001f) {
            outMidPos = midPos;
            outEndPos = endPos;
            return;
        }

        // Distance from root to target
        Math::Vector3 rootToTarget = target - rootPos;
        f32 targetDist = std::sqrt(rootToTarget.x * rootToTarget.x + rootToTarget.y * rootToTarget.y + rootToTarget.z * rootToTarget.z);

        // Clamp target distance to reachable range
        f32 maxReach = upperLen + lowerLen - 0.001f;
        f32 minReach = std::abs(upperLen - lowerLen) + 0.001f;
        targetDist = std::clamp(targetDist, minReach, maxReach);

        // Direction from root to target
        Math::Vector3 targetDir;
        if (targetDist > 0.0001f) {
            targetDir = rootToTarget * (1.0f / std::sqrt(rootToTarget.x * rootToTarget.x + rootToTarget.y * rootToTarget.y + rootToTarget.z * rootToTarget.z));
        } else {
            targetDir = Math::Vector3(0.0f, 1.0f, 0.0f);
        }

        // Law of cosines: find the angle at the root joint
        // cos(A) = (a^2 + c^2 - b^2) / (2ac)
        // where a = upperLen, b = lowerLen, c = targetDist
        f32 cosAngle = (upperLen * upperLen + targetDist * targetDist - lowerLen * lowerLen)
                     / (2.0f * upperLen * targetDist);
        cosAngle = std::clamp(cosAngle, -1.0f, 1.0f);
        f32 angle = std::acos(cosAngle);

        // Build coordinate frame: targetDir as primary axis, pole vector determines the plane
        Math::Vector3 poleDir = poleVector - rootPos;
        f32 poleDirLen = std::sqrt(poleDir.x * poleDir.x + poleDir.y * poleDir.y + poleDir.z * poleDir.z);
        if (poleDirLen > 0.0001f) {
            poleDir = poleDir * (1.0f / poleDirLen);
        } else {
            poleDir = Math::Vector3(0.0f, 0.0f, 1.0f);
        }

        // Gram-Schmidt: make poleDir orthogonal to targetDir
        f32 projDot = poleDir.x * targetDir.x + poleDir.y * targetDir.y + poleDir.z * targetDir.z;
        Math::Vector3 perpDir(
            poleDir.x - projDot * targetDir.x,
            poleDir.y - projDot * targetDir.y,
            poleDir.z - projDot * targetDir.z
        );
        f32 perpLen = std::sqrt(perpDir.x * perpDir.x + perpDir.y * perpDir.y + perpDir.z * perpDir.z);
        if (perpLen > 0.0001f) {
            perpDir = perpDir * (1.0f / perpLen);
        } else {
            // Pole vector parallel to target direction - pick an arbitrary perpendicular
            Math::Vector3 arbitrary = (std::abs(targetDir.y) < 0.9f)
                ? Math::Vector3(0.0f, 1.0f, 0.0f) : Math::Vector3(1.0f, 0.0f, 0.0f);
            perpDir = Math::Vector3(
                targetDir.y * arbitrary.z - targetDir.z * arbitrary.y,
                targetDir.z * arbitrary.x - targetDir.x * arbitrary.z,
                targetDir.x * arbitrary.y - targetDir.y * arbitrary.x
            );
            f32 cpLen = std::sqrt(perpDir.x * perpDir.x + perpDir.y * perpDir.y + perpDir.z * perpDir.z);
            if (cpLen > 0.0001f) perpDir = perpDir * (1.0f / cpLen);
        }

        // Mid joint position: rotate targetDir by angle around perpendicular axis
        f32 sinA = std::sin(angle);
        f32 cosA = std::cos(angle);
        Math::Vector3 ikMidDir(
            targetDir.x * cosA + perpDir.x * sinA,
            targetDir.y * cosA + perpDir.y * sinA,
            targetDir.z * cosA + perpDir.z * sinA
        );

        Math::Vector3 solvedMid = rootPos + ikMidDir * upperLen;
        Math::Vector3 solvedEnd = rootPos + targetDir * targetDist;

        // Blend with original pose using weight
        outMidPos = Math::Vector3(
            midPos.x + (solvedMid.x - midPos.x) * weight,
            midPos.y + (solvedMid.y - midPos.y) * weight,
            midPos.z + (solvedMid.z - midPos.z) * weight
        );
        outEndPos = Math::Vector3(
            endPos.x + (solvedEnd.x - endPos.x) * weight,
            endPos.y + (solvedEnd.y - endPos.y) * weight,
            endPos.z + (solvedEnd.z - endPos.z) * weight
        );
    }
};

} // namespace Animation
} // namespace Enjin
