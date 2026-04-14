#pragma once
// WHOLE_FILE_WEBGPU_GUARD
#if !ENJIN_RENDERER_WEBGPU

#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/Math/Quaternion.h"
#include <vector>
#include <cmath>

namespace Enjin {
namespace Renderer {

// SDF primitive types
enum class SDFPrimitive : u8 {
    Sphere,
    Box,
    Cylinder,
    Torus,
    Plane,
    RoundedBox,
    Count
};

// SDF boolean/blend operations
enum class SDFBlendMode : u8 {
    Union,
    Subtract,
    Intersect,
    SmoothUnion,
    SmoothSubtract,
    SmoothIntersect,
    Count
};

// A single SDF object in the scene
struct SDFObject {
    u32 id = 0;
    SDFPrimitive type = SDFPrimitive::Sphere;
    Math::Vector3 position;
    Math::Quaternion rotation;
    Math::Vector3 scale = Math::Vector3(1.0f, 1.0f, 1.0f);

    // Material
    Math::Vector3 color = Math::Vector3(1.0f, 1.0f, 1.0f);
    f32 metallic = 0.0f;
    f32 roughness = 0.5f;

    // Blending
    SDFBlendMode blendMode = SDFBlendMode::Union;
    f32 blendSmoothness = 0.1f;

    // Primitive-specific parameters
    f32 cornerRadius = 0.05f;   // RoundedBox corner radius
    f32 torusMinorRadius = 0.2f; // Torus minor radius (major is scale.x)
};

// GPU-packed SDF object data for shader upload (48 bytes per object)
struct alignas(16) SDFObjectGPU {
    Math::Vector3 position;    // 12
    f32 type;                  // 4  (float-encoded primitive type)
    Math::Vector3 scale;       // 12
    f32 blendMode;             // 4
    Math::Vector3 color;       // 12
    f32 blendSmoothness;       // 4
};

// SDF scene — manages a collection of SDF objects for ray marching
class SDFScene {
public:
    SDFScene() = default;
    ~SDFScene() = default;

    // Object management
    u32 AddObject(const SDFObject& obj);
    void RemoveObject(u32 objectId);
    void Clear();

    // CPU-side signed distance evaluation at a world position
    f32 Evaluate(const Math::Vector3& position) const;

    // Get packed GPU buffer data for shader upload
    std::vector<SDFObjectGPU> GetObjectBuffer() const;

    // Object access
    const std::vector<SDFObject>& GetObjects() const { return m_Objects; }
    SDFObject* GetObject(u32 objectId);
    u32 GetObjectCount() const { return static_cast<u32>(m_Objects.size()); }

private:
    std::vector<SDFObject> m_Objects;
    u32 m_NextId = 1;

    // Primitive SDF evaluators
    static f32 SDFSphere(const Math::Vector3& p, f32 radius);
    static f32 SDFBox(const Math::Vector3& p, const Math::Vector3& halfExtents);
    static f32 SDFCylinder(const Math::Vector3& p, f32 radius, f32 height);
    static f32 SDFTorus(const Math::Vector3& p, f32 majorRadius, f32 minorRadius);
    static f32 SDFPlane(const Math::Vector3& p, const Math::Vector3& normal, f32 offset);
    static f32 SDFRoundedBox(const Math::Vector3& p, const Math::Vector3& halfExtents, f32 radius);

    // Boolean operations
    static f32 OpUnion(f32 a, f32 b);
    static f32 OpSubtract(f32 a, f32 b);
    static f32 OpIntersect(f32 a, f32 b);
    static f32 OpSmoothUnion(f32 a, f32 b, f32 k);
    static f32 OpSmoothSubtract(f32 a, f32 b, f32 k);
    static f32 OpSmoothIntersect(f32 a, f32 b, f32 k);
};

} // namespace Renderer
} // namespace Enjin
#endif // !ENJIN_RENDERER_WEBGPU
