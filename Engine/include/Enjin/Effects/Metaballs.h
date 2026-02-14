#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/Math/Math.h"
#include "Enjin/ECS/Entity.h"
#include "Enjin/ECS/World.h"
#include <vector>
#include <unordered_map>

namespace Enjin {
namespace Effects {

// ---------------------------------------------------------------------------
// MetaballComponent -- ECS component for individual metaball blobs
// ---------------------------------------------------------------------------
struct ENJIN_API MetaballComponent {
    f32 radius = 1.0f;           // Influence radius
    f32 strength = 1.0f;         // Field strength (positive = blob, negative = anti-blob)
    f32 threshold = 1.0f;        // Isosurface threshold for this group
    i32 groupId = 0;             // Metaballs merge only within same group
    Math::Vector3 color = {1, 1, 1};
};

// ---------------------------------------------------------------------------
// MetaballConfig -- configuration for the metaball mesh generation
// ---------------------------------------------------------------------------
struct MetaballConfig {
    i32 gridResolution = 32;    // Voxels per axis (clamped to 16-64)
    f32 gridSize = 10.0f;       // World-space extent of the evaluation grid
    bool smoothNormals = true;   // Use gradient-based normals (vs face normals)
    bool autoCenter = true;      // Auto-center grid on metaball group centroid
    f32 updateRate = 60.0f;      // Mesh rebuild rate in Hz (0 = every frame)
};

// ---------------------------------------------------------------------------
// MetaballVertex -- vertex data for generated metaball meshes
// ---------------------------------------------------------------------------
struct MetaballVertex {
    Math::Vector3 position;
    Math::Vector3 normal;
    Math::Vector2 uv;
    Math::Vector3 color;
};

// ---------------------------------------------------------------------------
// MetaballMeshData -- generated mesh output per group
// ---------------------------------------------------------------------------
struct MetaballMeshData {
    std::vector<MetaballVertex> vertices;
    std::vector<u32> indices;
    i32 groupId = 0;
    u32 triangleCount = 0;
};

// ---------------------------------------------------------------------------
// MetaballSystem -- evaluates metaball fields and generates isosurface meshes
//
// For each group of metaballs, the system:
// 1. Evaluates the scalar field f(p) = sum(strength_i * r_i^2 / |p - c_i|^2)
// 2. Runs marching cubes on a 3D grid to extract the isosurface
// 3. Computes normals from the field gradient
// 4. Blends colors based on field contribution weights
// ---------------------------------------------------------------------------
class ENJIN_API MetaballSystem {
public:
    MetaballSystem() = default;
    ~MetaballSystem() = default;

    // Set configuration
    void SetConfig(const MetaballConfig& config) { m_Config = config; }
    const MetaballConfig& GetConfig() const { return m_Config; }
    MetaballConfig& GetConfig() { return m_Config; }

    // Update: evaluate fields and regenerate meshes for all groups
    void Update(ECS::World* world);

    // Get generated mesh data for a specific group (returns nullptr if not found)
    const MetaballMeshData* GetGroupMesh(i32 groupId) const;

    // Get all generated meshes
    const std::vector<MetaballMeshData>& GetAllMeshes() const { return m_Meshes; }

    // Evaluate the scalar field at a world position for a specific group
    f32 EvaluateField(const Math::Vector3& position, i32 groupId) const;

    // Evaluate field gradient (for normals) at a position via central differences
    Math::Vector3 EvaluateGradient(const Math::Vector3& position, i32 groupId) const;

    // Get the blended color at a position based on field contributions
    Math::Vector3 EvaluateColor(const Math::Vector3& position, i32 groupId) const;

private:
    // Internal metaball data (position + component data)
    struct MetaballData {
        Math::Vector3 position;
        f32 radius;
        f32 strength;
        f32 threshold;
        Math::Vector3 color;
    };

    // Group data: all metaballs belonging to the same group
    struct GroupData {
        i32 groupId = 0;
        f32 threshold = 1.0f;
        Math::Vector3 centroid;
        std::vector<MetaballData> balls;
    };

    // Gather metaballs from ECS world, grouped by groupId
    void GatherGroups(ECS::World* world);

    // Generate isosurface mesh for one group using marching cubes
    void GenerateGroupMesh(const GroupData& group, MetaballMeshData& outMesh);

    // Evaluate field for a specific group's balls
    f32 EvaluateFieldForGroup(
        const Math::Vector3& position,
        const std::vector<MetaballData>& balls
    ) const;

    // Evaluate gradient for a specific group's balls
    Math::Vector3 EvaluateGradientForGroup(
        const Math::Vector3& position,
        const std::vector<MetaballData>& balls
    ) const;

    // Evaluate blended color for a specific group's balls
    Math::Vector3 EvaluateColorForGroup(
        const Math::Vector3& position,
        const std::vector<MetaballData>& balls
    ) const;

    // Interpolate vertex position along a marching cubes edge
    Math::Vector3 InterpolateEdge(
        const Math::Vector3& p0, const Math::Vector3& p1,
        f32 v0, f32 v1, f32 threshold
    ) const;

    MetaballConfig m_Config;
    std::vector<GroupData> m_Groups;
    std::vector<MetaballMeshData> m_Meshes;
    f32 m_UpdateTimer = 0.0f;

    // Marching cubes lookup tables
    static const u16 s_EdgeTable[256];
    static const i32 s_TriTable[256][16];
};

} // namespace Effects
} // namespace Enjin
