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

} // namespace Animation
} // namespace Enjin
