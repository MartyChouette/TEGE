#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Renderer/RayTracing/AccelerationStructure.h"
#include "Enjin/Math/Matrix.h"
#include <vulkan/vulkan.h>
#include <unordered_map>
#include <vector>
#include <memory>

namespace Enjin {
namespace Renderer {

class VulkanContext;

// Manages BLAS cache (by mesh hash) and per-frame TLAS rebuild.
// Deduplicates BLAS by vertex+index hash so instanced meshes share one BLAS.
class ENJIN_API AccelerationStructureManager {
public:
    AccelerationStructureManager(VulkanContext* context);
    ~AccelerationStructureManager();

    void Initialize();
    void Shutdown();

    // Register a mesh for BLAS creation. Returns a BLAS ID for use in instance table.
    // Deduplicates by meshHash — same hash reuses existing BLAS.
    u32 RegisterMesh(u64 meshHash,
                     VkDeviceAddress vertexAddress, u32 vertexCount, u32 vertexStride,
                     VkDeviceAddress indexAddress, u32 indexCount);

    // Flush pending BLAS builds on a command buffer (call once per frame before TLAS build)
    void FlushPendingBLASBuilds(VkCommandBuffer cmd);

    // Add an instance to the TLAS for the current frame
    void AddInstance(u32 blasId, const Math::Matrix4& transform, u32 customIndex = 0,
                     u32 mask = 0xFF, u32 sbtOffset = 0,
                     VkGeometryInstanceFlagsKHR flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR);

    // Build/update TLAS from instances added this frame.
    // transformsOnly=true: use UPDATE mode (faster, transform-only changes)
    void BuildTLAS(VkCommandBuffer cmd, bool transformsOnly = false);

    // Clear instance list for next frame
    void ResetInstances();

    // Check if any BLAS builds are pending
    bool HasPendingBuilds() const { return !m_PendingBuilds.empty(); }

    // Get TLAS handle for descriptor binding
    VkAccelerationStructureKHR GetTLAS() const;
    bool HasValidTLAS() const;

    // Get BLAS count and instance count for stats
    u32 GetBLASCount() const { return static_cast<u32>(m_BLASCache.size()); }
    u32 GetInstanceCount() const { return static_cast<u32>(m_Instances.size()); }

    // Invalidate all BLAS (e.g. on scene change)
    void InvalidateAll();

private:
    struct PendingBLASBuild {
        u64 meshHash;
        VkDeviceAddress vertexAddress;
        u32 vertexCount;
        u32 vertexStride;
        VkDeviceAddress indexAddress;
        u32 indexCount;
        u32 blasId;
    };

    VulkanContext* m_Context = nullptr;

    // BLAS cache: meshHash -> BLAS ID
    std::unordered_map<u64, u32> m_MeshHashToBLAS;

    // Dense BLAS storage indexed by BLAS ID
    std::vector<std::unique_ptr<BLAS>> m_BLASCache;

    // Pending BLAS builds for this frame
    std::vector<PendingBLASBuild> m_PendingBuilds;

    // TLAS instance data for current frame
    std::vector<VkAccelerationStructureInstanceKHR> m_Instances;

    // TLAS
    std::unique_ptr<TLAS> m_TLAS;
    bool m_TLASNeedsFullRebuild = true;

    bool m_Initialized = false;
};

} // namespace Renderer
} // namespace Enjin
