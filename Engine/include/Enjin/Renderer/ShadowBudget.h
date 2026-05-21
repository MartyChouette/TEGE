#pragma once
// WHOLE_FILE_WEBGPU_GUARD
#if !ENJIN_RENDERER_WEBGPU

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Math/Matrix.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/Renderer/ShadowAtlas.h"
#include <vector>
#include <unordered_map>

namespace Enjin {
namespace ECS { using Entity = u64; }
namespace Renderer {

// A light requesting a shadow map tile from the budget system.
struct ShadowCandidate {
    ECS::Entity entity = 0;
    u32 lightType = 0;        // 0=Directional, 1=Point, 2=Spot
    f32 pixelWeight = 0.0f;   // Cluster-weighted screen impact (higher = more important)
    u32 tilesNeeded = 1;      // Directional=cascadeCount, Point=6, Spot=1
    u32 tileSize = 1024;      // Desired per-tile resolution
    Math::Vector3 position;
    Math::Vector3 direction;
    f32 range = 100.0f;
    f32 outerConeAngle = 0.0f; // Spot only
    bool castShadows = true;
};

// Result of budget allocation for one light
struct ShadowAllocation {
    ECS::Entity entity = 0;
    std::vector<ShadowTile> tiles;          // Allocated atlas tiles
    std::vector<Math::Matrix4> viewProjs;   // Per-tile view-projection matrices
    bool allocated = false;
};

// Dynamic shadow budget system.
// Each frame: ranks lights by screen impact, allocates atlas tiles to top N.
// Lights that don't make the budget render unshadowed.
class ENJIN_API ShadowBudget {
public:
    ShadowBudget() = default;

    // Set the atlas to allocate from
    void SetAtlas(ShadowAtlas* atlas) { m_Atlas = atlas; }

    // Set maximum tile budget (from AdaptiveQualitySystem)
    void SetMaxTiles(u32 maxTiles) { m_MaxTiles = maxTiles; }
    u32 GetMaxTiles() const { return m_MaxTiles; }

    // Submit a light as a shadow candidate for this frame
    void SubmitCandidate(const ShadowCandidate& candidate);

    // Allocate tiles to highest-priority candidates. Call after all candidates submitted.
    // Returns the allocated lights with their atlas tiles and view-projection matrices.
    const std::vector<ShadowAllocation>& Allocate();

    // Reset for next frame
    void Reset();

    // Query: is a specific light allocated this frame?
    const ShadowAllocation* GetAllocation(ECS::Entity entity) const;

    // Stats
    u32 GetAllocatedLightCount() const { return static_cast<u32>(m_Allocations.size()); }
    u32 GetAllocatedTileCount() const { return m_AllocatedTileCount; }
    u32 GetCandidateCount() const { return static_cast<u32>(m_Candidates.size()); }

private:
    // Compute view-projection for a shadow tile
    static Math::Matrix4 ComputeDirectionalVP(const Math::Vector3& lightDir,
                                               const Math::Matrix4& cameraView,
                                               const Math::Matrix4& cameraProj,
                                               f32 nearSplit, f32 farSplit);
    static Math::Matrix4 ComputePointFaceVP(const Math::Vector3& lightPos, f32 range, u32 face);
    static Math::Matrix4 ComputeSpotVP(const Math::Vector3& lightPos, const Math::Vector3& lightDir,
                                        f32 outerConeAngle, f32 range);

    ShadowAtlas* m_Atlas = nullptr;
    u32 m_MaxTiles = 16;
    u32 m_AllocatedTileCount = 0;

    std::vector<ShadowCandidate> m_Candidates;
    std::vector<ShadowAllocation> m_Allocations;
    std::unordered_map<ECS::Entity, usize> m_EntityToAllocation; // Entity → index in m_Allocations

    // Hysteresis: track which entities had shadows last frame to prevent flickering
    std::vector<ECS::Entity> m_PreviousAllocatedEntities;
};

} // namespace Renderer
} // namespace Enjin
#endif // !ENJIN_RENDERER_WEBGPU
