#include "Enjin/Renderer/ShadowBudget.h"
#include "Enjin/Logging/Log.h"
#include <algorithm>

#if !ENJIN_RENDERER_WEBGPU

namespace Enjin {
namespace Renderer {

void ShadowBudget::SubmitCandidate(const ShadowCandidate& candidate) {
    if (!candidate.castShadows) return;
    m_Candidates.push_back(candidate);
}

const std::vector<ShadowAllocation>& ShadowBudget::Allocate() {
    m_Allocations.clear();
    m_EntityToAllocation.clear();
    m_AllocatedTileCount = 0;

    if (!m_Atlas || m_Candidates.empty()) return m_Allocations;

    m_Atlas->ResetAllocations();

    // Sort candidates by pixel weight descending (most screen impact first).
    // Hysteresis boost: lights that had shadows last frame get +20% weight bonus
    // to prevent flickering at the budget boundary.
    for (auto& c : m_Candidates) {
        for (ECS::Entity prev : m_PreviousAllocatedEntities) {
            if (prev == c.entity) {
                c.pixelWeight *= 1.2f; // Hysteresis boost
                break;
            }
        }
    }

    std::sort(m_Candidates.begin(), m_Candidates.end(),
              [](const ShadowCandidate& a, const ShadowCandidate& b) {
                  return a.pixelWeight > b.pixelWeight;
              });

    u32 tilesRemaining = m_MaxTiles;

    for (const auto& candidate : m_Candidates) {
        if (tilesRemaining < candidate.tilesNeeded) continue;

        // Try to allocate all tiles for this light
        std::vector<ShadowTile> tiles;
        tiles.reserve(candidate.tilesNeeded);
        bool allocationOk = true;

        for (u32 t = 0; t < candidate.tilesNeeded; ++t) {
            ShadowTile tile = m_Atlas->AllocateTile(candidate.tileSize);
            if (!tile.allocated) {
                // Ran out of atlas space — free what we got and skip this light
                for (auto& allocated : tiles) m_Atlas->FreeTile(allocated);
                allocationOk = false;
                break;
            }
            tiles.push_back(tile);
        }

        if (!allocationOk) continue;

        // Compute view-projection matrices for each tile
        ShadowAllocation alloc;
        alloc.entity = candidate.entity;
        alloc.tiles = std::move(tiles);
        alloc.allocated = true;

        alloc.viewProjs.resize(alloc.tiles.size());
        for (u32 t = 0; t < static_cast<u32>(alloc.tiles.size()); ++t) {
            switch (candidate.lightType) {
            case 0: // Directional — VP computed externally by cascade system
                // Placeholder: caller fills in cascade VPs after allocation
                alloc.viewProjs[t] = Math::Matrix4::Identity();
                break;
            case 1: // Point — 6 cubemap faces
                alloc.viewProjs[t] = ComputePointFaceVP(candidate.position, candidate.range, t);
                break;
            case 2: // Spot
                alloc.viewProjs[t] = ComputeSpotVP(candidate.position, candidate.direction,
                                                     candidate.outerConeAngle, candidate.range);
                break;
            }
        }

        tilesRemaining -= static_cast<u32>(alloc.tiles.size());
        m_AllocatedTileCount += static_cast<u32>(alloc.tiles.size());
        m_EntityToAllocation[alloc.entity] = m_Allocations.size();
        m_Allocations.push_back(std::move(alloc));
    }

    // Update hysteresis tracking
    m_PreviousAllocatedEntities.clear();
    for (const auto& alloc : m_Allocations) {
        m_PreviousAllocatedEntities.push_back(alloc.entity);
    }

    return m_Allocations;
}

void ShadowBudget::Reset() {
    m_Candidates.clear();
    // Don't clear m_PreviousAllocatedEntities — needed for hysteresis
}

const ShadowAllocation* ShadowBudget::GetAllocation(ECS::Entity entity) const {
    auto it = m_EntityToAllocation.find(entity);
    if (it == m_EntityToAllocation.end()) return nullptr;
    return &m_Allocations[it->second];
}

// --- View-projection computation ---

Math::Matrix4 ShadowBudget::ComputePointFaceVP(const Math::Vector3& lightPos, f32 range, u32 face) {
    // Cubemap face directions (+X, -X, +Y, -Y, +Z, -Z)
    static const Math::Vector3 directions[6] = {
        { 1, 0, 0}, {-1, 0, 0},
        { 0, 1, 0}, { 0,-1, 0},
        { 0, 0, 1}, { 0, 0,-1}
    };
    static const Math::Vector3 ups[6] = {
        {0,-1, 0}, {0,-1, 0},
        {0, 0, 1}, {0, 0,-1},
        {0,-1, 0}, {0,-1, 0}
    };

    if (face >= 6) face = 0;
    Math::Matrix4 view = Math::Matrix4::LookAt(lightPos, lightPos + directions[face], ups[face]);
    Math::Matrix4 proj = Math::Matrix4::Perspective(Math::PI * 0.5f, 1.0f, 0.1f, range);
    return proj * view;
}

Math::Matrix4 ShadowBudget::ComputeSpotVP(const Math::Vector3& lightPos, const Math::Vector3& lightDir,
                                            f32 outerConeAngle, f32 range) {
    Math::Vector3 target = lightPos + lightDir;
    Math::Vector3 up(0.0f, 1.0f, 0.0f);
    // Avoid degenerate up vector when light points straight up/down
    if (Math::Abs(lightDir.y) > 0.99f) up = Math::Vector3(1.0f, 0.0f, 0.0f);

    Math::Matrix4 view = Math::Matrix4::LookAt(lightPos, target, up);
    f32 fov = outerConeAngle * 2.0f; // Full cone angle
    if (fov < 0.01f) fov = Math::PI * 0.5f; // Fallback
    Math::Matrix4 proj = Math::Matrix4::Perspective(fov, 1.0f, 0.1f, range);
    return proj * view;
}

} // namespace Renderer
} // namespace Enjin

#endif // !ENJIN_RENDERER_WEBGPU
