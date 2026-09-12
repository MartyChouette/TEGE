#include "Enjin/ECS/Systems/VoxelVolumeSystem.h"

#include "Enjin/ECS/Components/VoxelVolume.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Mesh.h"
#include "Enjin/ECS/Components/Gameplay.h"     // MeshColliderComponent
#include "Enjin/ECS/Components/ProceduralMesh.h"
#include "Enjin/ECS/MeshEdit.h"
#include "Enjin/Geometry/SurfaceNets.h"

namespace Enjin {
namespace ECS {

namespace {

// The component's field, presented as the mesher's grid.
//
// A copy, which is the honest cost of keeping the mesher free of any knowledge
// about components. It is one pass over the field per remesh, against a mesher
// that walks it several times; the alternative is a mesher that includes an ECS
// header, and that trade is not worth making for geometry that has to stay
// testable on its own.
Geometry::ScalarGrid ToGrid(const VoxelVolumeComponent& v, const Math::Vector3& origin) {
    Geometry::ScalarGrid g;
    g.dimX = v.dimX;
    g.dimY = v.dimY;
    g.dimZ = v.dimZ;
    g.voxelSize = v.voxelSize;
    g.origin = origin;
    if (v.field.size() == v.Count()) {
        g.values = v.field;
    } else {
        // An uncarved volume is air, not garbage. Meshing it produces nothing,
        // which is correct: there is no rock in it yet.
        g.values.assign(v.Count(), v.Band());
    }
    return g;
}

} // namespace

bool VoxelVolumeSystem::Rebuild(World* world, Entity entity) {
    if (!world) return false;
    auto* volume = world->GetComponent<VoxelVolumeComponent>(entity);
    if (!volume) return false;

    volume->meshDirty = false;

    const auto* xf = world->GetComponent<TransformComponent>(entity);
    const Math::Vector3 worldOrigin =
        volume->GridOrigin(xf ? xf->position : Math::Vector3(0.0f));

    // The mesh is built in the ENTITY's local space, because the transform is
    // applied to it again on the way to the screen. Building in world space and
    // then transforming would move a carved cave twice as far from the origin
    // as it was carved -- the same double-application that put every pond in
    // the scene back at 0,0,0 until Water3D's position was untangled.
    const Math::Vector3 localOrigin(worldOrigin.x - (xf ? xf->position.x : 0.0f),
                                    worldOrigin.y - (xf ? xf->position.y : 0.0f),
                                    worldOrigin.z - (xf ? xf->position.z : 0.0f));

    const Geometry::SurfaceMesh built =
        Geometry::BuildSurfaceNet(ToGrid(*volume, localOrigin), 0.0f);

    if (built.Empty()) {
        // A volume of pure air is a legal state, not an error -- it is what a
        // freshly placed volume is, and what one carved completely away
        // becomes. Leave whatever mesh is there rather than blanking it, so an
        // in-progress edit does not flash the geometry away and back.
        return false;
    }

    if (!world->HasComponent<MeshComponent>(entity)) {
        world->AddComponent<MeshComponent>(entity, MeshComponent{});
    }
    auto* mesh = world->GetComponent<MeshComponent>(entity);
    if (!mesh) return false;

    const bool topologyChanged = mesh->vertices.size() != built.positions.size() ||
                                 mesh->indices.size() != built.indices.size();

    mesh->vertices.clear();
    mesh->vertices.reserve(built.positions.size());
    for (usize i = 0; i < built.positions.size(); ++i) {
        MeshComponent::Vertex v;
        v.position = built.positions[i];
        v.normal = (i < built.normals.size()) ? built.normals[i]
                                              : Math::Vector3(0.0f, 1.0f, 0.0f);
        // Triplanar in the shader would be the right answer for a cave; a
        // planar guess is what the rest of the generated geometry uses, and
        // a wrong-but-consistent UV is easier to replace later than a per-mesh
        // special case.
        v.uv = Math::Vector2(v.position.x * 0.25f, v.position.z * 0.25f);
        v.color = Math::Vector4(1.0f, 1.0f, 1.0f, 1.0f);
        mesh->vertices.push_back(v);
    }
    mesh->indices = built.indices;
    mesh->subMeshes.clear();
    mesh->aabbDirty = true;

    // Collision is the same triangles. Nothing convex can describe a cave: the
    // convex hull of a cavern is a solid lump, and a player would stand on the
    // outside of it.
    if (!world->HasComponent<MeshColliderComponent>(entity)) {
        world->AddComponent<MeshColliderComponent>(entity, MeshColliderComponent{});
    }
    if (auto* col = world->GetComponent<MeshColliderComponent>(entity)) {
        col->vertices = built.positions;
        col->indices = built.indices;
        col->generated = true;
        col->convex = false;
    }

    MarkMeshChanged(*world, entity, ProceduralMeshComponent::Source::VoxelVolume,
                    topologyChanged);
    return true;
}

u32 VoxelVolumeSystem::Update(World* world) {
    if (!world) return 0;
    u32 rebuilt = 0;
    for (Entity entity : world->GetEntitiesWithComponent<VoxelVolumeComponent>()) {
        auto* volume = world->GetComponent<VoxelVolumeComponent>(entity);
        if (!volume || !volume->meshDirty) continue;
        if (Rebuild(world, entity)) ++rebuilt;
    }
    return rebuilt;
}

} // namespace ECS
} // namespace Enjin
