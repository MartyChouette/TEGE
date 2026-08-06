#include "Enjin/Renderer/RayTracing/AccelerationStructureManager.h"
#include "Enjin/Renderer/Vulkan/VulkanContext.h"
#include "Enjin/Logging/Log.h"
#include <algorithm>
#include <cstring>

namespace Enjin {
namespace Renderer {

AccelerationStructureManager::AccelerationStructureManager(VulkanContext* context)
    : m_Context(context) {
}

AccelerationStructureManager::~AccelerationStructureManager() {
    Shutdown();
}

void AccelerationStructureManager::Initialize() {
    if (m_Initialized) return;
    m_TLAS = std::make_unique<TLAS>(m_Context);
    m_Initialized = true;
    ENJIN_LOG_INFO(Renderer, "AccelerationStructureManager initialized");
}

void AccelerationStructureManager::Shutdown() {
    if (!m_Initialized) return;
    m_TLAS.reset();
    m_BLASCache.clear();
    m_MeshHashToBLAS.clear();
    m_PendingBuilds.clear();
    m_Instances.clear();
    m_Initialized = false;
}

u32 AccelerationStructureManager::RegisterMesh(u64 meshHash,
                                                VkDeviceAddress vertexAddress, u32 vertexCount, u32 vertexStride,
                                                VkDeviceAddress indexAddress, u32 indexCount) {
    // Check if already registered
    auto it = m_MeshHashToBLAS.find(meshHash);
    if (it != m_MeshHashToBLAS.end()) {
        return it->second;
    }

    // Assign new BLAS ID
    u32 blasId = static_cast<u32>(m_BLASCache.size());
    m_BLASCache.push_back(std::make_unique<BLAS>(m_Context));
    m_MeshHashToBLAS[meshHash] = blasId;

    // Queue for build
    PendingBLASBuild pending;
    pending.meshHash = meshHash;
    pending.vertexAddress = vertexAddress;
    pending.vertexCount = vertexCount;
    pending.vertexStride = vertexStride;
    pending.indexAddress = indexAddress;
    pending.indexCount = indexCount;
    pending.blasId = blasId;
    m_PendingBuilds.push_back(pending);

    return blasId;
}

void AccelerationStructureManager::FlushPendingBLASBuilds(VkCommandBuffer cmd) {
    if (m_PendingBuilds.empty()) return;

    ENJIN_LOG_INFO(Renderer, "Building %zu pending BLAS structures", m_PendingBuilds.size());

    for (auto& build : m_PendingBuilds) {
        if (build.blasId >= m_BLASCache.size()) continue;

        auto& blas = m_BLASCache[build.blasId];
        if (!blas->Build(cmd, build.vertexAddress, build.vertexCount, build.vertexStride,
                         build.indexAddress, build.indexCount)) {
            ENJIN_LOG_ERROR(Renderer, "Failed to build BLAS for mesh hash %llu", build.meshHash);
        }
    }

    m_PendingBuilds.clear();
    m_TLASNeedsFullRebuild = true;  // New geometry means TLAS must be fully rebuilt
}

void AccelerationStructureManager::AddInstance(u32 blasId, const Math::Matrix4& transform,
                                               u32 customIndex, u32 mask, u32 sbtOffset,
                                               VkGeometryInstanceFlagsKHR flags) {
    if (blasId >= m_BLASCache.size() || !m_BLASCache[blasId]->IsValid()) return;

    VkAccelerationStructureInstanceKHR instance{};

    // Convert 4x4 matrix to VkTransformMatrixKHR (3x4 row-major)
    // Our Matrix4 is column-major with operator()(row, col), VkTransformMatrixKHR is row-major 3x4
    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 4; ++col) {
            instance.transform.matrix[row][col] = transform(static_cast<usize>(row), static_cast<usize>(col));
        }
    }

    instance.instanceCustomIndex = customIndex & 0x00FFFFFF;
    instance.mask = mask & 0xFF;
    instance.instanceShaderBindingTableRecordOffset = sbtOffset & 0x00FFFFFF;
    instance.flags = flags;
    instance.accelerationStructureReference = m_BLASCache[blasId]->GetDeviceAddress();

    m_Instances.push_back(instance);
}

void AccelerationStructureManager::BuildTLAS(VkCommandBuffer cmd, bool transformsOnly) {
    if (!m_TLAS) return;

    // Skip TLAS build when there are no instances — avoids 0-size scratch buffer issues
    // and some drivers don't handle 0-instance TLAS builds correctly
    if (m_Instances.empty()) return;

    // An in-place update requires the same instance count as the last full build
    // (entities added/removed = structural change)
    u32 count = static_cast<u32>(m_Instances.size());
    if (count != m_LastBuiltInstanceCount) m_TLASNeedsFullRebuild = true;

    bool updateOnly = transformsOnly && !m_TLASNeedsFullRebuild && m_TLAS->IsValid();
    m_TLAS->Build(cmd, m_Instances.data(), count, updateOnly);

    m_TLASNeedsFullRebuild = false;
    m_LastBuiltInstanceCount = count;
}

void AccelerationStructureManager::ResetInstances() {
    m_Instances.clear();
}

VkAccelerationStructureKHR AccelerationStructureManager::GetTLAS() const {
    return m_TLAS ? m_TLAS->GetHandle() : VK_NULL_HANDLE;
}

bool AccelerationStructureManager::HasValidTLAS() const {
    return m_TLAS && m_TLAS->IsValid();
}

void AccelerationStructureManager::InvalidateAll() {
    m_BLASCache.clear();
    m_MeshHashToBLAS.clear();
    m_PendingBuilds.clear();
    m_Instances.clear();
    if (m_TLAS) m_TLAS->Destroy();
    m_TLASNeedsFullRebuild = true;
    ENJIN_LOG_INFO(Renderer, "AccelerationStructureManager: all BLAS invalidated");
}

void AccelerationStructureManager::InvalidateMesh(u64 meshHash) {
    auto it = m_MeshHashToBLAS.find(meshHash);
    if (it == m_MeshHashToBLAS.end()) return;

    u32 blasId = it->second;

    // Destroy the BLAS GPU resources so no stale handle can be referenced
    if (blasId < m_BLASCache.size() && m_BLASCache[blasId]) {
        m_BLASCache[blasId]->Destroy();
    }

    // Remove from hash map so the address range can be re-registered fresh
    m_MeshHashToBLAS.erase(it);

    // Remove any pending builds for this hash (shouldn't happen, but be safe)
    m_PendingBuilds.erase(
        std::remove_if(m_PendingBuilds.begin(), m_PendingBuilds.end(),
            [meshHash](const PendingBLASBuild& b) { return b.meshHash == meshHash; }),
        m_PendingBuilds.end());

    m_TLASNeedsFullRebuild = true;
}

} // namespace Renderer
} // namespace Enjin
