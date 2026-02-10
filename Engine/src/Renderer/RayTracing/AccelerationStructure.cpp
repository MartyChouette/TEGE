#include "Enjin/Renderer/RayTracing/AccelerationStructure.h"
#include "Enjin/Renderer/Vulkan/VulkanContext.h"
#include "Enjin/Logging/Log.h"
#include <cstring>

namespace Enjin {
namespace Renderer {

// Helper to get function pointers for RT extension functions
static PFN_vkCreateAccelerationStructureKHR s_vkCreateAS = nullptr;
static PFN_vkDestroyAccelerationStructureKHR s_vkDestroyAS = nullptr;
static PFN_vkGetAccelerationStructureBuildSizesKHR s_vkGetASBuildSizes = nullptr;
static PFN_vkCmdBuildAccelerationStructuresKHR s_vkCmdBuildAS = nullptr;
static PFN_vkGetAccelerationStructureDeviceAddressKHR s_vkGetASDeviceAddress = nullptr;
static PFN_vkGetBufferDeviceAddressKHR s_vkGetBufferDeviceAddress = nullptr;

static void LoadRTFunctions(VkDevice device) {
    if (s_vkCreateAS) return;  // Already loaded
    s_vkCreateAS = (PFN_vkCreateAccelerationStructureKHR)vkGetDeviceProcAddr(device, "vkCreateAccelerationStructureKHR");
    s_vkDestroyAS = (PFN_vkDestroyAccelerationStructureKHR)vkGetDeviceProcAddr(device, "vkDestroyAccelerationStructureKHR");
    s_vkGetASBuildSizes = (PFN_vkGetAccelerationStructureBuildSizesKHR)vkGetDeviceProcAddr(device, "vkGetAccelerationStructureBuildSizesKHR");
    s_vkCmdBuildAS = (PFN_vkCmdBuildAccelerationStructuresKHR)vkGetDeviceProcAddr(device, "vkCmdBuildAccelerationStructuresKHR");
    s_vkGetASDeviceAddress = (PFN_vkGetAccelerationStructureDeviceAddressKHR)vkGetDeviceProcAddr(device, "vkGetAccelerationStructureDeviceAddressKHR");
    s_vkGetBufferDeviceAddress = (PFN_vkGetBufferDeviceAddressKHR)vkGetDeviceProcAddr(device, "vkGetBufferDeviceAddressKHR");
}

// ------------------------------------------------------------------
// Helper: create a buffer with memory
// ------------------------------------------------------------------
static bool CreateBufferHelper(VulkanContext* ctx, VkDeviceSize size, VkBufferUsageFlags usage,
                                VkBuffer& outBuffer, VkDeviceMemory& outMemory) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(ctx->GetDevice(), &bufferInfo, nullptr, &outBuffer) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create AS buffer");
        return false;
    }

    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements(ctx->GetDevice(), outBuffer, &memReqs);

    VkMemoryAllocateFlagsInfo allocFlags{};
    allocFlags.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
    allocFlags.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.pNext = &allocFlags;
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = ctx->FindMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (allocInfo.memoryTypeIndex == UINT32_MAX) {
        vkDestroyBuffer(ctx->GetDevice(), outBuffer, nullptr);
        outBuffer = VK_NULL_HANDLE;
        return false;
    }

    if (vkAllocateMemory(ctx->GetDevice(), &allocInfo, nullptr, &outMemory) != VK_SUCCESS) {
        vkDestroyBuffer(ctx->GetDevice(), outBuffer, nullptr);
        outBuffer = VK_NULL_HANDLE;
        return false;
    }

    vkBindBufferMemory(ctx->GetDevice(), outBuffer, outMemory, 0);
    ctx->TrackAllocation(static_cast<usize>(memReqs.size));
    return true;
}

static void FreeBufferHelper(VulkanContext* ctx, VkBuffer& buffer, VkDeviceMemory& memory) {
    if (buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(ctx->GetDevice(), buffer, nullptr);
        buffer = VK_NULL_HANDLE;
    }
    if (memory != VK_NULL_HANDLE) {
        vkFreeMemory(ctx->GetDevice(), memory, nullptr);
        memory = VK_NULL_HANDLE;
    }
}

// ==================================================================
// BLAS
// ==================================================================
BLAS::BLAS(VulkanContext* context) : m_Context(context) {
    LoadRTFunctions(context->GetDevice());
}

BLAS::~BLAS() {
    Destroy();
}

bool BLAS::Build(VkCommandBuffer cmd,
                 VkDeviceAddress vertexBufferAddress, u32 vertexCount, u32 vertexStride,
                 VkDeviceAddress indexBufferAddress, u32 indexCount,
                 VkGeometryFlagsKHR geometryFlags) {
    if (!s_vkCreateAS || !s_vkGetASBuildSizes || !s_vkCmdBuildAS) {
        ENJIN_LOG_ERROR(Renderer, "RT function pointers not loaded");
        return false;
    }

    // Define geometry
    VkAccelerationStructureGeometryKHR geometry{};
    geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
    geometry.flags = geometryFlags;
    geometry.geometry.triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
    geometry.geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
    geometry.geometry.triangles.vertexData.deviceAddress = vertexBufferAddress;
    geometry.geometry.triangles.vertexStride = vertexStride;
    geometry.geometry.triangles.maxVertex = vertexCount - 1;
    geometry.geometry.triangles.indexType = VK_INDEX_TYPE_UINT32;
    geometry.geometry.triangles.indexData.deviceAddress = indexBufferAddress;

    u32 primitiveCount = indexCount / 3;

    // Query build sizes
    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{};
    buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR |
                      VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR;
    buildInfo.geometryCount = 1;
    buildInfo.pGeometries = &geometry;

    VkAccelerationStructureBuildSizesInfoKHR sizeInfo{};
    sizeInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
    s_vkGetASBuildSizes(m_Context->GetDevice(), VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                         &buildInfo, &primitiveCount, &sizeInfo);

    // Create AS buffer
    if (!CreateBuffer(sizeInfo.accelerationStructureSize,
                      VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
                      VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                      m_Buffer, m_Memory)) {
        return false;
    }
    m_AllocatedSize = static_cast<usize>(sizeInfo.accelerationStructureSize);

    // Create AS handle
    VkAccelerationStructureCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    createInfo.buffer = m_Buffer;
    createInfo.size = sizeInfo.accelerationStructureSize;
    createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;

    if (s_vkCreateAS(m_Context->GetDevice(), &createInfo, nullptr, &m_Handle) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create BLAS");
        return false;
    }

    // Create scratch buffer
    if (!CreateBuffer(sizeInfo.buildScratchSize,
                      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                      m_ScratchBuffer, m_ScratchMemory)) {
        return false;
    }

    // Get scratch buffer address
    VkBufferDeviceAddressInfo scratchAddrInfo{};
    scratchAddrInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    scratchAddrInfo.buffer = m_ScratchBuffer;
    VkDeviceAddress scratchAddress = s_vkGetBufferDeviceAddress(m_Context->GetDevice(), &scratchAddrInfo);

    // Build
    buildInfo.dstAccelerationStructure = m_Handle;
    buildInfo.scratchData.deviceAddress = scratchAddress;

    VkAccelerationStructureBuildRangeInfoKHR rangeInfo{};
    rangeInfo.primitiveCount = primitiveCount;
    rangeInfo.primitiveOffset = 0;
    rangeInfo.firstVertex = 0;
    rangeInfo.transformOffset = 0;

    const VkAccelerationStructureBuildRangeInfoKHR* pRangeInfo = &rangeInfo;
    s_vkCmdBuildAS(cmd, 1, &buildInfo, &pRangeInfo);

    // Memory barrier after build
    VkMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
    barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
        VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR | VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
        0, 1, &barrier, 0, nullptr, 0, nullptr);

    // Get device address
    VkAccelerationStructureDeviceAddressInfoKHR addrInfo{};
    addrInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
    addrInfo.accelerationStructure = m_Handle;
    m_DeviceAddress = s_vkGetASDeviceAddress(m_Context->GetDevice(), &addrInfo);

    return true;
}

bool BLAS::Compact(VkCommandBuffer cmd) {
    // Compaction is optional optimization — skip for now, can be added later
    (void)cmd;
    return true;
}

void BLAS::Destroy() {
    if (m_Handle != VK_NULL_HANDLE && s_vkDestroyAS) {
        s_vkDestroyAS(m_Context->GetDevice(), m_Handle, nullptr);
        m_Handle = VK_NULL_HANDLE;
    }
    FreeBuffer(m_Buffer, m_Memory);
    FreeBuffer(m_ScratchBuffer, m_ScratchMemory);
    m_DeviceAddress = 0;
    m_AllocatedSize = 0;
    m_Compacted = false;
}

bool BLAS::CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                        VkBuffer& outBuffer, VkDeviceMemory& outMemory) {
    return CreateBufferHelper(m_Context, size, usage, outBuffer, outMemory);
}

void BLAS::FreeBuffer(VkBuffer& buffer, VkDeviceMemory& memory) {
    FreeBufferHelper(m_Context, buffer, memory);
}

// ==================================================================
// TLAS
// ==================================================================
TLAS::TLAS(VulkanContext* context) : m_Context(context) {
    LoadRTFunctions(context->GetDevice());
}

TLAS::~TLAS() {
    Destroy();
}

bool TLAS::Build(VkCommandBuffer cmd,
                 const VkAccelerationStructureInstanceKHR* instances, u32 instanceCount,
                 bool updateOnly) {
    if (!s_vkCreateAS || !s_vkGetASBuildSizes || !s_vkCmdBuildAS) {
        ENJIN_LOG_ERROR(Renderer, "RT function pointers not loaded");
        return false;
    }

    // Upload instance data to GPU buffer
    VkDeviceSize instanceDataSize = sizeof(VkAccelerationStructureInstanceKHR) * instanceCount;
    if (instanceDataSize == 0) instanceDataSize = sizeof(VkAccelerationStructureInstanceKHR);

    // Recreate instance buffer if capacity is insufficient
    if (instanceCount > m_MaxInstanceCount || m_InstanceBuffer == VK_NULL_HANDLE) {
        FreeBuffer(m_InstanceBuffer, m_InstanceMemory);
        u32 newCapacity = instanceCount > 0 ? instanceCount * 2 : 64;  // Over-allocate

        // Instance buffer needs host-visible for upload + shader device address
        VkBufferCreateInfo bufInfo{};
        bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufInfo.size = sizeof(VkAccelerationStructureInstanceKHR) * newCapacity;
        bufInfo.usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
                        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
        bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateBuffer(m_Context->GetDevice(), &bufInfo, nullptr, &m_InstanceBuffer) != VK_SUCCESS) {
            ENJIN_LOG_ERROR(Renderer, "Failed to create TLAS instance buffer");
            return false;
        }

        VkMemoryRequirements memReqs;
        vkGetBufferMemoryRequirements(m_Context->GetDevice(), m_InstanceBuffer, &memReqs);

        VkMemoryAllocateFlagsInfo allocFlags{};
        allocFlags.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
        allocFlags.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.pNext = &allocFlags;
        allocInfo.allocationSize = memReqs.size;
        allocInfo.memoryTypeIndex = m_Context->FindMemoryType(memReqs.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        if (vkAllocateMemory(m_Context->GetDevice(), &allocInfo, nullptr, &m_InstanceMemory) != VK_SUCCESS) {
            ENJIN_LOG_ERROR(Renderer, "Failed to allocate TLAS instance memory");
            return false;
        }
        vkBindBufferMemory(m_Context->GetDevice(), m_InstanceBuffer, m_InstanceMemory, 0);
        m_MaxInstanceCount = newCapacity;
        m_Context->TrackAllocation(static_cast<usize>(memReqs.size));
    }

    // Copy instance data
    if (instanceCount > 0) {
        void* mapped = nullptr;
        vkMapMemory(m_Context->GetDevice(), m_InstanceMemory, 0, instanceDataSize, 0, &mapped);
        std::memcpy(mapped, instances, static_cast<size_t>(instanceDataSize));
        vkUnmapMemory(m_Context->GetDevice(), m_InstanceMemory);
    }

    // Get instance buffer address
    VkBufferDeviceAddressInfo instanceAddrInfo{};
    instanceAddrInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    instanceAddrInfo.buffer = m_InstanceBuffer;
    VkDeviceAddress instanceAddress = s_vkGetBufferDeviceAddress(m_Context->GetDevice(), &instanceAddrInfo);

    // TLAS geometry
    VkAccelerationStructureGeometryKHR geometry{};
    geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    geometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
    geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
    geometry.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
    geometry.geometry.instances.arrayOfPointers = VK_FALSE;
    geometry.geometry.instances.data.deviceAddress = instanceAddress;

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{};
    buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR |
                      VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR;
    buildInfo.geometryCount = 1;
    buildInfo.pGeometries = &geometry;

    if (updateOnly && m_Handle != VK_NULL_HANDLE) {
        buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR;
        buildInfo.srcAccelerationStructure = m_Handle;
    } else {
        buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    }

    // Query sizes
    VkAccelerationStructureBuildSizesInfoKHR sizeInfo{};
    sizeInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
    s_vkGetASBuildSizes(m_Context->GetDevice(), VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                         &buildInfo, &instanceCount, &sizeInfo);

    // Create or recreate TLAS buffer if needed (only on full build)
    if (buildInfo.mode == VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR) {
        // Destroy old
        if (m_Handle != VK_NULL_HANDLE) {
            s_vkDestroyAS(m_Context->GetDevice(), m_Handle, nullptr);
            m_Handle = VK_NULL_HANDLE;
        }
        FreeBuffer(m_Buffer, m_Memory);
        FreeBuffer(m_ScratchBuffer, m_ScratchMemory);

        // Create buffer
        if (!CreateBuffer(sizeInfo.accelerationStructureSize,
                          VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
                          VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                          m_Buffer, m_Memory)) {
            return false;
        }
        m_AllocatedSize = static_cast<usize>(sizeInfo.accelerationStructureSize);

        // Create handle
        VkAccelerationStructureCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
        createInfo.buffer = m_Buffer;
        createInfo.size = sizeInfo.accelerationStructureSize;
        createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;

        if (s_vkCreateAS(m_Context->GetDevice(), &createInfo, nullptr, &m_Handle) != VK_SUCCESS) {
            ENJIN_LOG_ERROR(Renderer, "Failed to create TLAS");
            return false;
        }
    }

    // Create scratch buffer
    FreeBuffer(m_ScratchBuffer, m_ScratchMemory);
    if (!CreateBuffer(sizeInfo.buildScratchSize,
                      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                      m_ScratchBuffer, m_ScratchMemory)) {
        return false;
    }

    VkBufferDeviceAddressInfo scratchAddrInfo{};
    scratchAddrInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    scratchAddrInfo.buffer = m_ScratchBuffer;
    VkDeviceAddress scratchAddress = s_vkGetBufferDeviceAddress(m_Context->GetDevice(), &scratchAddrInfo);

    // Build
    buildInfo.dstAccelerationStructure = m_Handle;
    buildInfo.scratchData.deviceAddress = scratchAddress;

    VkAccelerationStructureBuildRangeInfoKHR rangeInfo{};
    rangeInfo.primitiveCount = instanceCount;
    const VkAccelerationStructureBuildRangeInfoKHR* pRangeInfo = &rangeInfo;
    s_vkCmdBuildAS(cmd, 1, &buildInfo, &pRangeInfo);

    // Barrier
    VkMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
    barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
        VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
        0, 1, &barrier, 0, nullptr, 0, nullptr);

    return true;
}

void TLAS::Destroy() {
    if (m_Handle != VK_NULL_HANDLE && s_vkDestroyAS) {
        s_vkDestroyAS(m_Context->GetDevice(), m_Handle, nullptr);
        m_Handle = VK_NULL_HANDLE;
    }
    FreeBuffer(m_Buffer, m_Memory);
    FreeBuffer(m_ScratchBuffer, m_ScratchMemory);
    FreeBuffer(m_InstanceBuffer, m_InstanceMemory);
    m_AllocatedSize = 0;
    m_MaxInstanceCount = 0;
}

bool TLAS::CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                        VkBuffer& outBuffer, VkDeviceMemory& outMemory) {
    return CreateBufferHelper(m_Context, size, usage, outBuffer, outMemory);
}

void TLAS::FreeBuffer(VkBuffer& buffer, VkDeviceMemory& memory) {
    FreeBufferHelper(m_Context, buffer, memory);
}

} // namespace Renderer
} // namespace Enjin
