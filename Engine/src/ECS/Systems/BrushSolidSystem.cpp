#include "Enjin/ECS/Systems/BrushSolidSystem.h"
#include "Enjin/ECS/Components/BrushSolid.h"
#include "Enjin/ECS/Components/Mesh.h"
#include "Enjin/ECS/Components/ProceduralMesh.h"
#include "Enjin/ECS/Components/Gameplay.h"   // MeshColliderComponent
#include "Enjin/Geometry/CSG.h"
#include "Enjin/ECS/MeshEdit.h"
#include "Enjin/Logging/Log.h"

namespace Enjin {
namespace ECS {

bool BrushSolidSystem::Rebuild(World* world, Entity entity) {
    if (!world) return false;
    auto* solid = world->GetComponent<BrushSolidComponent>(entity);
    if (!solid) return false;

    const std::vector<Geometry::BrushEntry> entries = solid->ToBrushEntries();
    const std::vector<Geometry::BrushFace> faces = Geometry::BuildSolid(entries);

    solid->lastFaceCount = static_cast<u32>(faces.size());
    solid->dirty = false;
    solid->builtHash = solid->ContentHash();

    if (faces.empty()) {
        // An empty solid is a legal state, not an error: a fresh component has
        // no brushes yet, and a list of nothing but Subtracts carves air. Leave
        // whatever mesh is there rather than blanking it, so an in-progress edit
        // does not flash the model away and back.
        solid->lastTriangleCount = 0;
        return false;
    }

    ECS::MeshComponent built = Geometry::ToMesh(faces, solid->uvScale);

    if (!world->HasComponent<MeshComponent>(entity)) {
        world->AddComponent<MeshComponent>(entity, MeshComponent{});
    }
    auto* mesh = world->GetComponent<MeshComponent>(entity);
    if (!mesh) return false;

    const bool topologyChanged = mesh->vertices.size() != built.vertices.size() ||
                                 mesh->indices.size() != built.indices.size();

    mesh->vertices = std::move(built.vertices);
    mesh->indices = std::move(built.indices);
    mesh->subMeshes = std::move(built.subMeshes);
    mesh->aabbDirty = true;

    // Collision is the same triangles, welded. Convex shapes cannot express a
    // hole, so anything else leaves a doorway you can walk into.
    if (solid->generateCollider) {
        if (!world->HasComponent<MeshColliderComponent>(entity)) {
            world->AddComponent<MeshColliderComponent>(entity, MeshColliderComponent{});
        }
        if (auto* col = world->GetComponent<MeshColliderComponent>(entity)) {
            Geometry::FillMeshCollider(*col, entries);
            solid->lastTriangleCount = static_cast<u32>(col->indices.size() / 3);
        }
    } else {
        solid->lastTriangleCount = static_cast<u32>(mesh->indices.size() / 3);
    }

    // Hand the GPU side to the one place that knows the upload protocol.
    MarkMeshChanged(*world, entity, ProceduralMeshComponent::Source::Csg, topologyChanged);

    return true;
}

u32 BrushSolidSystem::Update(World* world) {
    if (!world) return 0;

    u32 rebuilt = 0;
    // Collected first, because Rebuild adds Mesh, MeshCollider and
    // ProceduralMesh components, and structural mutation while iterating the
    // storage it is iterating is exactly what adr-0004 forbids.
    const auto entities = world->GetEntitiesWithComponent<BrushSolidComponent>();

    for (Entity e : entities) {
        auto* solid = world->GetComponent<BrushSolidComponent>(e);
        if (!solid) continue;

        // The flag is the fast path; the hash is the truth. Undo writes an old
        // value straight back through a raw pointer without touching the flag,
        // and so would a script or any tool that edits the list directly. The
        // hash means the geometry follows the data whoever wrote it.
        if (!solid->dirty && solid->ContentHash() == solid->builtHash) continue;

        if (Rebuild(world, e)) ++rebuilt;
    }

    if (rebuilt > 0) {
        ENJIN_LOG_INFO(Editor, "Rebuilt %u brush solid%s", rebuilt, rebuilt == 1 ? "" : "s");
    }
    return rebuilt;
}

} // namespace ECS
} // namespace Enjin
