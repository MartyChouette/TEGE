#pragma once
// WHOLE_FILE_WEBGPU_GUARD
#if !ENJIN_RENDERER_WEBGPU

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Math/Vector.h"
#if !ENJIN_RENDERER_WEBGPU
#include <vulkan/vulkan.h>
#endif
#include <vector>

namespace Enjin {
namespace Renderer {

class VulkanContext;

// GPU-side BVH node for importance-weighted light selection (48 bytes, std430).
// Internal nodes: leftChild >= 0, rightChild >= 0, lightIndex = -1
// Leaf nodes: leftChild = -1, rightChild = -1, lightIndex = index into NEE light SSBO
struct LightBVHNodeGPU {
    f32 boundsMin[3];  // AABB minimum
    f32 totalPower;    // Sum of all light luminances in subtree
    f32 boundsMax[3];  // AABB maximum
    i32 leftChild;     // -1 for leaf
    i32 rightChild;    // -1 for leaf
    i32 lightIndex;    // Index into NEE light SSBO (-1 for internal nodes)
    i32 lightCount;    // Number of lights in subtree
    f32 _pad;
};
static_assert(sizeof(LightBVHNodeGPU) == 48, "LightBVHNode must be 48 bytes for GPU alignment");

// CPU-side light entry used during BVH construction
struct LightBVHEntry {
    Math::Vector3 position;   // World position (center for point/spot, origin for directional)
    f32 power;                // Luminance * intensity
    f32 range;                // Attenuation range (0 = infinite/directional)
    u32 lightIndex;           // Index in the NEE SSBO
};

// Configuration for light BVH construction
struct LightBVHConfig {
    bool enabled = false;
    u32 maxLights = 32768;            // Cap for BVH construction
    u32 minLightsForBVH = 16;         // Below this, uniform random is fine
    bool rebuildEveryFrame = false;    // Force rebuild (default: only on light changes)
};

// CPU-side binary BVH over scene lights for importance-weighted selection in ReSTIR.
//
// Instead of uniform random light selection (O(1) but wastes samples on distant/occluded
// lights), the BVH enables O(log N) importance-proportional traversal:
//   1. Start at root
//   2. At each internal node, compute importance for left/right children based on
//      their total power and AABB distance to the shading point
//   3. Choose child proportional to its importance
//   4. Repeat until reaching a leaf -> selected light
//
// The source PDF is the product of selection probabilities along the traversal path,
// giving proper importance weights for unbiased ReSTIR.
//
// Built on CPU, uploaded as an SSBO (binding 27 in RT descriptor set).
class ENJIN_API LightBVH {
public:
    LightBVH(VulkanContext* context);
    ~LightBVH();

    bool Initialize(u32 maxNodes);
    void Shutdown();

    // Build BVH from the current frame's light data. Call after NEE light upload.
    // Returns the number of BVH nodes built (0 if insufficient lights or disabled).
    u32 Build(const std::vector<LightBVHEntry>& lights);

    // Upload the built BVH to the GPU SSBO. Returns true on success.
    bool Upload();

    VkBuffer GetNodeBuffer() const { return m_NodeBuffer; }
    u32 GetNodeCount() const { return m_NodeCount; }
    bool IsValid() const { return m_NodeCount > 0 && m_NodeBuffer != VK_NULL_HANDLE; }

    LightBVHConfig& GetConfig() { return m_Config; }
    const LightBVHConfig& GetConfig() const { return m_Config; }

private:
    // Recursive BVH build. Returns index of created node in m_Nodes.
    u32 BuildRecursive(std::vector<LightBVHEntry>& lights, u32 start, u32 end);

    VulkanContext* m_Context = nullptr;
    LightBVHConfig m_Config;

    // CPU-side node storage (flattened binary tree)
    std::vector<LightBVHNodeGPU> m_Nodes;
    u32 m_NodeCount = 0;

    // GPU buffer (binding 27 in RT descriptor set)
    VkBuffer m_NodeBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_NodeMemory = VK_NULL_HANDLE;
    void* m_NodeMapped = nullptr;
    u32 m_MaxNodes = 0;

    bool m_Initialized = false;
};

} // namespace Renderer
} // namespace Enjin
#endif // !ENJIN_RENDERER_WEBGPU
