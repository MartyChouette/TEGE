#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/ECS/Component.h"
#include "Enjin/Math/Vector.h"
#include <string>
#include <vector>

namespace Enjin {
namespace ECS {

// Mesh component - stores vertex data
struct ENJIN_API MeshComponent : public IComponent {
    struct Vertex {
        Math::Vector3 position;
        Math::Vector3 normal;
        Math::Vector2 uv;
        Math::Vector4 color = Math::Vector4(1.0f, 1.0f, 1.0f, 1.0f); // vertex color (default white)
        Math::Vector4 tangent = Math::Vector4(0.0f, 0.0f, 0.0f, 1.0f); // xyz=tangent dir, w=handedness
        // Skeletal animation bone data (first 4 influences)
        Math::Vector4 boneWeights = Math::Vector4(0.0f, 0.0f, 0.0f, 0.0f);
        u32 boneIndices[4] = {0, 0, 0, 0};
        // --- Appended fields (keep existing offsets above unchanged) ---
        Math::Vector2 uv1 = Math::Vector2(0.0f, 0.0f);  // second UV channel (lightmap/detail; offset 96)
        // Bone influences 5-8 for dense rigs (offset 104). All-zero weights = no extra influence.
        Math::Vector4 boneWeights2 = Math::Vector4(0.0f, 0.0f, 0.0f, 0.0f);
        u32 boneIndices2[4] = {0, 0, 0, 0};  // offset 120, stride now 136
    };

    std::vector<Vertex> vertices;
    std::vector<u32> indices;

    // Sub-mesh support for multi-material rendering.
    // When populated, each SubMesh defines a range within the shared vertex/index
    // buffers that should be drawn with a specific material slot.
    struct SubMesh {
        u32 indexOffset = 0;   // Offset into the indices array
        u32 indexCount = 0;    // Number of indices for this sub-mesh
        i32 materialSlot = 0;  // Index into MaterialSlotsComponent::slots
        std::string name;      // Optional name (e.g., "Head", "Body")
    };
    std::vector<SubMesh> subMeshes;

    // Cached local-space AABB (computed lazily from vertices, avoids per-frame recomputation)
    Math::Vector3 cachedAABBMin = Math::Vector3(1.0f, 1.0f, 1.0f);   // min > max signals dirty
    Math::Vector3 cachedAABBMax = Math::Vector3(-1.0f, -1.0f, -1.0f);
    bool aabbDirty = true;

    bool IsValid() const {
        return !vertices.empty() && !indices.empty();
    }

    bool HasSubMeshes() const {
        return subMeshes.size() > 1;
    }
};

// Convenience alias so ECS::Vertex works without qualifying MeshComponent::
using Vertex = MeshComponent::Vertex;

} // namespace ECS
} // namespace Enjin
