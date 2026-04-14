#pragma once
// WHOLE_FILE_WEBGPU_GUARD
#if !ENJIN_RENDERER_WEBGPU

#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/Math/Matrix.h"
#if !ENJIN_RENDERER_WEBGPU
#include <vulkan/vulkan.h>
#endif

namespace Enjin {
namespace Renderer {

class VulkanContext;

// Update the material resolve descriptor set with visibility buffer, depth, output, and data bindings
bool UpdateResolveDescriptors(
    VulkanContext* context,
    VkDescriptorSet descriptorSet,
    VkImageView visibilityBufferView,
    VkSampler depthSampler,
    VkImageView depthBufferView,
    VkImageView outputColorView,
    VkBuffer vertexBuffer,
    VkDeviceSize vertexBufferSize,
    VkBuffer objectDataBuffer,
    VkDeviceSize objectDataBufferSize
);

// Push constants for material resolve compute shader
struct MaterialResolvePushConstants {
    Math::Matrix4 viewProj;       // 64 bytes
    Math::Matrix4 invViewProj;    // 64 bytes
    u32 screenWidth;              // 4 bytes
    u32 screenHeight;             // 4 bytes
    u32 objectCount;              // 4 bytes
    u32 _pad;                     // 4 bytes
};
static_assert(sizeof(MaterialResolvePushConstants) == 144, "MaterialResolvePushConstants must be 144 bytes");

// Vertex format expected by the material resolve shader (matches engine vertex layout)
struct ResolveVertex {
    Math::Vector3 position;
    Math::Vector3 normal;
    Math::Vector2 uv;
    Math::Vector4 tangent;
};

} // namespace Renderer
} // namespace Enjin
#endif // !ENJIN_RENDERER_WEBGPU
