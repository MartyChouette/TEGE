#include "Enjin/Renderer/SDFScene.h"
#include <algorithm>
#include <limits>

namespace Enjin {
namespace Renderer {

u32 SDFScene::AddObject(const SDFObject& obj) {
    SDFObject newObj = obj;
    newObj.id = m_NextId++;
    m_Objects.push_back(newObj);
    return newObj.id;
}

void SDFScene::RemoveObject(u32 objectId) {
    m_Objects.erase(
        std::remove_if(m_Objects.begin(), m_Objects.end(),
            [objectId](const SDFObject& o) { return o.id == objectId; }),
        m_Objects.end());
}

void SDFScene::Clear() {
    m_Objects.clear();
    m_NextId = 1;
}

SDFObject* SDFScene::GetObject(u32 objectId) {
    for (auto& obj : m_Objects) {
        if (obj.id == objectId) return &obj;
    }
    return nullptr;
}

f32 SDFScene::Evaluate(const Math::Vector3& position) const {
    if (m_Objects.empty()) return std::numeric_limits<f32>::max();

    f32 result = std::numeric_limits<f32>::max();
    bool first = true;

    for (const auto& obj : m_Objects) {
        // Transform point into object local space
        Math::Vector3 localPos = position - obj.position;
        // Apply inverse rotation
        Math::Quaternion invRot = obj.rotation.Conjugate();
        localPos = invRot.Rotate(localPos);

        f32 dist = 0.0f;
        switch (obj.type) {
            case SDFPrimitive::Sphere:
                dist = SDFSphere(localPos, obj.scale.x);
                break;
            case SDFPrimitive::Box:
                dist = SDFBox(localPos, obj.scale);
                break;
            case SDFPrimitive::Cylinder:
                dist = SDFCylinder(localPos, obj.scale.x, obj.scale.y);
                break;
            case SDFPrimitive::Torus:
                dist = SDFTorus(localPos, obj.scale.x, obj.torusMinorRadius);
                break;
            case SDFPrimitive::Plane:
                dist = SDFPlane(localPos, Math::Vector3(0.0f, 1.0f, 0.0f), 0.0f);
                break;
            case SDFPrimitive::RoundedBox:
                dist = SDFRoundedBox(localPos, obj.scale, obj.cornerRadius);
                break;
            default:
                dist = std::numeric_limits<f32>::max();
                break;
        }

        if (first) {
            result = dist;
            first = false;
            continue;
        }

        // Apply blend operation
        switch (obj.blendMode) {
            case SDFBlendMode::Union:
                result = OpUnion(result, dist);
                break;
            case SDFBlendMode::Subtract:
                result = OpSubtract(result, dist);
                break;
            case SDFBlendMode::Intersect:
                result = OpIntersect(result, dist);
                break;
            case SDFBlendMode::SmoothUnion:
                result = OpSmoothUnion(result, dist, obj.blendSmoothness);
                break;
            case SDFBlendMode::SmoothSubtract:
                result = OpSmoothSubtract(result, dist, obj.blendSmoothness);
                break;
            case SDFBlendMode::SmoothIntersect:
                result = OpSmoothIntersect(result, dist, obj.blendSmoothness);
                break;
            default:
                result = OpUnion(result, dist);
                break;
        }
    }

    return result;
}

std::vector<SDFObjectGPU> SDFScene::GetObjectBuffer() const {
    std::vector<SDFObjectGPU> buffer;
    buffer.reserve(m_Objects.size());

    for (const auto& obj : m_Objects) {
        SDFObjectGPU gpu;
        gpu.position = obj.position;
        gpu.type = static_cast<f32>(obj.type);
        gpu.scale = obj.scale;
        gpu.blendMode = static_cast<f32>(obj.blendMode);
        gpu.color = obj.color;
        gpu.blendSmoothness = obj.blendSmoothness;
        buffer.push_back(gpu);
    }

    return buffer;
}

// --- Primitive SDF functions ---

f32 SDFScene::SDFSphere(const Math::Vector3& p, f32 radius) {
    return std::sqrt(p.x * p.x + p.y * p.y + p.z * p.z) - radius;
}

f32 SDFScene::SDFBox(const Math::Vector3& p, const Math::Vector3& halfExtents) {
    f32 dx = std::abs(p.x) - halfExtents.x;
    f32 dy = std::abs(p.y) - halfExtents.y;
    f32 dz = std::abs(p.z) - halfExtents.z;

    f32 outsideDist = std::sqrt(
        std::max(dx, 0.0f) * std::max(dx, 0.0f) +
        std::max(dy, 0.0f) * std::max(dy, 0.0f) +
        std::max(dz, 0.0f) * std::max(dz, 0.0f)
    );
    f32 insideDist = std::min(std::max(dx, std::max(dy, dz)), 0.0f);
    return outsideDist + insideDist;
}

f32 SDFScene::SDFCylinder(const Math::Vector3& p, f32 radius, f32 height) {
    f32 dx = std::sqrt(p.x * p.x + p.z * p.z) - radius;
    f32 dy = std::abs(p.y) - height;

    f32 outsideDist = std::sqrt(
        std::max(dx, 0.0f) * std::max(dx, 0.0f) +
        std::max(dy, 0.0f) * std::max(dy, 0.0f)
    );
    f32 insideDist = std::min(std::max(dx, dy), 0.0f);
    return outsideDist + insideDist;
}

f32 SDFScene::SDFTorus(const Math::Vector3& p, f32 majorRadius, f32 minorRadius) {
    f32 qx = std::sqrt(p.x * p.x + p.z * p.z) - majorRadius;
    return std::sqrt(qx * qx + p.y * p.y) - minorRadius;
}

f32 SDFScene::SDFPlane(const Math::Vector3& p, const Math::Vector3& normal, f32 offset) {
    return p.x * normal.x + p.y * normal.y + p.z * normal.z + offset;
}

f32 SDFScene::SDFRoundedBox(const Math::Vector3& p, const Math::Vector3& halfExtents, f32 radius) {
    f32 dx = std::abs(p.x) - halfExtents.x + radius;
    f32 dy = std::abs(p.y) - halfExtents.y + radius;
    f32 dz = std::abs(p.z) - halfExtents.z + radius;

    f32 outsideDist = std::sqrt(
        std::max(dx, 0.0f) * std::max(dx, 0.0f) +
        std::max(dy, 0.0f) * std::max(dy, 0.0f) +
        std::max(dz, 0.0f) * std::max(dz, 0.0f)
    );
    f32 insideDist = std::min(std::max(dx, std::max(dy, dz)), 0.0f);
    return outsideDist + insideDist - radius;
}

// --- Boolean operations ---

f32 SDFScene::OpUnion(f32 a, f32 b) {
    return std::min(a, b);
}

f32 SDFScene::OpSubtract(f32 a, f32 b) {
    return std::max(a, -b);
}

f32 SDFScene::OpIntersect(f32 a, f32 b) {
    return std::max(a, b);
}

f32 SDFScene::OpSmoothUnion(f32 a, f32 b, f32 k) {
    if (k <= 0.0f) return std::min(a, b);
    f32 h = std::max(k - std::abs(a - b), 0.0f) / k;
    return std::min(a, b) - h * h * k * 0.25f;
}

f32 SDFScene::OpSmoothSubtract(f32 a, f32 b, f32 k) {
    if (k <= 0.0f) return std::max(a, -b);
    f32 h = std::max(k - std::abs(a + b), 0.0f) / k;
    return std::max(a, -b) + h * h * k * 0.25f;
}

f32 SDFScene::OpSmoothIntersect(f32 a, f32 b, f32 k) {
    if (k <= 0.0f) return std::max(a, b);
    f32 h = std::max(k - std::abs(a - b), 0.0f) / k;
    return std::max(a, b) + h * h * k * 0.25f;
}

} // namespace Renderer
} // namespace Enjin
