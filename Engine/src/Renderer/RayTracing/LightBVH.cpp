#include "Enjin/Renderer/RayTracing/LightBVH.h"
#include "Enjin/Renderer/Vulkan/VulkanContext.h"
#include "Enjin/Logging/Log.h"
#include <algorithm>
#include <cstring>
#include <limits>

namespace Enjin {
namespace Renderer {

LightBVH::LightBVH(VulkanContext* context) : m_Context(context) {}

LightBVH::~LightBVH() { Shutdown(); }

bool LightBVH::Initialize(u32 maxNodes) {
    m_MaxNodes = maxNodes;
    m_Nodes.reserve(maxNodes);

    // Create host-visible SSBO for BVH nodes
    VkDeviceSize bufferSize = static_cast<VkDeviceSize>(maxNodes) * sizeof(LightBVHNodeGPU);

    VkBufferCreateInfo bufInfo{};
    bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufInfo.size = bufferSize;
    bufInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(m_Context->GetDevice(), &bufInfo, nullptr, &m_NodeBuffer) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "LightBVH: Failed to create node buffer");
        return false;
    }

    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements(m_Context->GetDevice(), m_NodeBuffer, &memReqs);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = m_Context->FindMemoryType(memReqs.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    if (vkAllocateMemory(m_Context->GetDevice(), &allocInfo, nullptr, &m_NodeMemory) != VK_SUCCESS) {
        vkDestroyBuffer(m_Context->GetDevice(), m_NodeBuffer, nullptr);
        m_NodeBuffer = VK_NULL_HANDLE;
        ENJIN_LOG_ERROR(Renderer, "LightBVH: Failed to allocate node memory");
        return false;
    }

    vkBindBufferMemory(m_Context->GetDevice(), m_NodeBuffer, m_NodeMemory, 0);

    if (vkMapMemory(m_Context->GetDevice(), m_NodeMemory, 0, bufferSize, 0, &m_NodeMapped) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "LightBVH: Failed to map node memory");
        m_NodeMapped = nullptr;
    }

    m_Context->TrackAllocation(static_cast<usize>(memReqs.size));
    m_Initialized = true;
    ENJIN_LOG_INFO(Renderer, "LightBVH initialized (max %u nodes, %u KB)", maxNodes,
                   static_cast<u32>(bufferSize / 1024));
    return true;
}

u32 LightBVH::Build(const std::vector<LightBVHEntry>& lights) {
    m_Nodes.clear();
    m_NodeCount = 0;

    if (lights.empty()) return 0;
    if (lights.size() < m_Config.minLightsForBVH) return 0;

    // Cap to max lights
    u32 lightCount = static_cast<u32>(std::min(static_cast<usize>(m_Config.maxLights), lights.size()));

    // Make a mutable copy for partitioning
    std::vector<LightBVHEntry> sortedLights(lights.begin(), lights.begin() + lightCount);

    // Reserve space for worst-case node count (2*N - 1 for N leaves)
    u32 maxPossibleNodes = std::min(2 * lightCount - 1, m_MaxNodes);
    m_Nodes.reserve(maxPossibleNodes);

    BuildRecursive(sortedLights, 0, lightCount);
    m_NodeCount = static_cast<u32>(m_Nodes.size());

    return m_NodeCount;
}

u32 LightBVH::BuildRecursive(std::vector<LightBVHEntry>& lights, u32 start, u32 end) {
    if (start >= end || m_Nodes.size() >= m_MaxNodes) return UINT32_MAX;

    u32 nodeIndex = static_cast<u32>(m_Nodes.size());
    m_Nodes.emplace_back();
    LightBVHNodeGPU& node = m_Nodes[nodeIndex];

    u32 count = end - start;

    // Compute AABB and total power for this range
    f32 minX = std::numeric_limits<f32>::max(), minY = minX, minZ = minX;
    f32 maxX = -std::numeric_limits<f32>::max(), maxY = maxX, maxZ = maxX;
    f32 totalPower = 0.0f;

    for (u32 i = start; i < end; ++i) {
        const auto& l = lights[i];
        minX = std::min(minX, l.position.x);
        minY = std::min(minY, l.position.y);
        minZ = std::min(minZ, l.position.z);
        maxX = std::max(maxX, l.position.x);
        maxY = std::max(maxY, l.position.y);
        maxZ = std::max(maxZ, l.position.z);
        totalPower += l.power;
    }

    node.boundsMin[0] = minX; node.boundsMin[1] = minY; node.boundsMin[2] = minZ;
    node.boundsMax[0] = maxX; node.boundsMax[1] = maxY; node.boundsMax[2] = maxZ;
    node.totalPower = totalPower;
    node.lightCount = static_cast<i32>(count);
    node._pad = 0.0f;

    // Leaf node: single light
    if (count == 1) {
        node.leftChild = -1;
        node.rightChild = -1;
        node.lightIndex = static_cast<i32>(lights[start].lightIndex);
        return nodeIndex;
    }

    // Internal node: split along the longest axis using spatial median
    node.lightIndex = -1;

    f32 extentX = maxX - minX;
    f32 extentY = maxY - minY;
    f32 extentZ = maxZ - minZ;

    // Choose split axis (longest extent)
    u32 axis = 0;
    if (extentY > extentX && extentY > extentZ) axis = 1;
    else if (extentZ > extentX && extentZ > extentY) axis = 2;

    // Sort along split axis
    u32 mid = start + count / 2;
    std::nth_element(lights.begin() + start, lights.begin() + mid, lights.begin() + end,
        [axis](const LightBVHEntry& a, const LightBVHEntry& b) {
            if (axis == 0) return a.position.x < b.position.x;
            if (axis == 1) return a.position.y < b.position.y;
            return a.position.z < b.position.z;
        });

    // Build children (note: m_Nodes may reallocate, so we store nodeIndex and refetch)
    u32 leftIdx = BuildRecursive(lights, start, mid);
    u32 rightIdx = BuildRecursive(lights, mid, end);

    // Refetch node reference after potential reallocation
    m_Nodes[nodeIndex].leftChild = (leftIdx != UINT32_MAX) ? static_cast<i32>(leftIdx) : -1;
    m_Nodes[nodeIndex].rightChild = (rightIdx != UINT32_MAX) ? static_cast<i32>(rightIdx) : -1;

    return nodeIndex;
}

bool LightBVH::Upload() {
    if (!m_NodeMapped || m_NodeCount == 0) return false;

    usize uploadSize = static_cast<usize>(m_NodeCount) * sizeof(LightBVHNodeGPU);
    usize maxSize = static_cast<usize>(m_MaxNodes) * sizeof(LightBVHNodeGPU);
    if (uploadSize > maxSize) uploadSize = maxSize;

    std::memcpy(m_NodeMapped, m_Nodes.data(), uploadSize);
    return true;
}

void LightBVH::Shutdown() {
    if (!m_Initialized) return;

    if (m_NodeMapped) {
        vkUnmapMemory(m_Context->GetDevice(), m_NodeMemory);
        m_NodeMapped = nullptr;
    }
    if (m_NodeBuffer) {
        vkDestroyBuffer(m_Context->GetDevice(), m_NodeBuffer, nullptr);
        m_NodeBuffer = VK_NULL_HANDLE;
    }
    if (m_NodeMemory) {
        vkFreeMemory(m_Context->GetDevice(), m_NodeMemory, nullptr);
        m_NodeMemory = VK_NULL_HANDLE;
    }

    m_Nodes.clear();
    m_NodeCount = 0;
    m_Initialized = false;
    ENJIN_LOG_INFO(Renderer, "LightBVH shut down");
}

} // namespace Renderer
} // namespace Enjin
