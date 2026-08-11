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

    // Reference to the source asset this mesh was imported from. Empty path = an
    // authored/procedural mesh with no source (always serialized inline). When set,
    // the geometry is reproducible by re-importing meshIndex from sourcePath with the
    // recorded axis conversion, which lets scenes store a compact reference instead of
    // inline vertices and lets the engine share/free CPU copies.
    struct SourceRef {
        std::string sourcePath;    // .fbx/.gltf/... the mesh was imported from
        i32   meshIndex = -1;      // which mesh/node within that file
        bool  axisZToY = false;    // Z-up -> Y-up conversion applied to vertices at import
        bool  axisLToR = false;    // left- -> right-handed conversion applied at import
        u64   contentHash = 0;     // hash of the imported vertex+index bytes (dedup + drift check)
        bool  Valid() const { return !sourcePath.empty() && meshIndex >= 0; }
    };
    SourceRef source;

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

    // FNV-1a hash over the raw vertex + index bytes. Identifies mesh content for
    // dedup and for detecting when a referenced source file has drifted from what
    // a scene was saved against. Stable across runs for identical geometry.
    static u64 ComputeContentHash(const std::vector<Vertex>& verts,
                                  const std::vector<u32>& inds) {
        u64 h = 1469598103934665603ULL;  // FNV offset basis
        auto mix = [&h](const void* data, usize bytes) {
            const u8* p = static_cast<const u8*>(data);
            for (usize i = 0; i < bytes; ++i) { h ^= p[i]; h *= 1099511628211ULL; }
        };
        if (!verts.empty()) mix(verts.data(), verts.size() * sizeof(Vertex));
        if (!inds.empty())  mix(inds.data(), inds.size() * sizeof(u32));
        return h;
    }
};

// Convenience alias so ECS::Vertex works without qualifying MeshComponent::
using Vertex = MeshComponent::Vertex;

} // namespace ECS
} // namespace Enjin
