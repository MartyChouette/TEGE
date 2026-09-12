#include "Enjin/Audio/AcousticScene.h"

#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Material.h"
#include "Enjin/ECS/Components/Gameplay.h"

#include <algorithm>
#include <cmath>

namespace Enjin {
namespace Audio {

namespace {

// Position and rotation, WITHOUT scale.
//
// Collider sizes are world space in this engine -- Jolt and Box2D do not
// multiply them by the transform scale -- so neither may this. The original
// gatherer used the full world matrix, which meant a collider on an entity
// scaled five times emitted acoustic geometry five times too large: the
// physics wall and the acoustic wall were different sizes and in different
// places, and the sound of a room came off surfaces that were not where the
// room was. A test catches it now.
Math::Matrix4 RigidTransform(const ECS::TransformComponent& xf) {
    Math::Matrix4 m = xf.rotation.ToMatrix();
    m(0, 3) = xf.position.x;
    m(1, 3) = xf.position.y;
    m(2, 3) = xf.position.z;
    return m;
}

Math::Vector3 TransformPoint(const Math::Matrix4& m, f32 x, f32 y, f32 z) {
    return Math::Vector3(m(0, 0) * x + m(0, 1) * y + m(0, 2) * z + m(0, 3),
                         m(1, 0) * x + m(1, 1) * y + m(1, 2) * z + m(1, 3),
                         m(2, 0) * x + m(2, 1) * y + m(2, 2) * z + m(2, 3));
}

// A box, as twelve triangles, wound outwards.
void EmitBox(AcousticScene& scene, const Math::Matrix4& world, const Math::Vector3& centre,
             const Math::Vector3& halfExtents, i32 materialIndex) {
    const i32 base = static_cast<i32>(scene.vertices.size());
    for (i32 corner = 0; corner < 8; ++corner) {
        const f32 sx = (corner & 1) ? 1.0f : -1.0f;
        const f32 sy = (corner & 2) ? 1.0f : -1.0f;
        const f32 sz = (corner & 4) ? 1.0f : -1.0f;
        scene.vertices.push_back(TransformPoint(world,
                                                centre.x + sx * halfExtents.x,
                                                centre.y + sy * halfExtents.y,
                                                centre.z + sz * halfExtents.z));
    }
    // Corner bit 0 = +x, 1 = +y, 2 = +z.
    static const i32 kTris[36] = {
        0,2,1, 1,2,3,   // -z
        4,5,6, 5,7,6,   // +z
        0,1,4, 1,5,4,   // -y
        2,6,3, 3,6,7,   // +y
        0,4,2, 2,4,6,   // -x
        1,3,5, 3,7,5,   // +x
    };
    for (i32 i = 0; i < 36; ++i) scene.indices.push_back(base + kTris[i]);
    for (i32 t = 0; t < 12; ++t) scene.materialIndices.push_back(materialIndex);
}

// An icosahedron, which is a sphere as far as a reflection is concerned.
void EmitSphere(AcousticScene& scene, const Math::Matrix4& world,
                const Math::Vector3& centre, f32 radius, i32 materialIndex) {
    static constexpr f32 PHI = 1.6180339887f;
    const f32 norm = 1.0f / std::sqrt(1.0f + PHI * PHI);
    const f32 a = norm, b = PHI * norm;
    const Math::Vector3 ico[12] = {
        {-a, b, 0}, { a, b, 0}, {-a,-b, 0}, { a,-b, 0},
        { 0,-a, b}, { 0, a, b}, { 0,-a,-b}, { 0, a,-b},
        { b, 0,-a}, { b, 0, a}, {-b, 0,-a}, {-b, 0, a},
    };
    static const i32 kTris[60] = {
        0,11,5, 0,5,1, 0,1,7, 0,7,10, 0,10,11,
        1,5,9, 5,11,4, 11,10,2, 10,7,6, 7,1,8,
        3,9,4, 3,4,2, 3,2,6, 3,6,8, 3,8,9,
        4,9,5, 2,4,11, 6,2,10, 8,6,7, 9,8,1,
    };
    const i32 base = static_cast<i32>(scene.vertices.size());
    for (i32 i = 0; i < 12; ++i) {
        scene.vertices.push_back(TransformPoint(world,
                                                centre.x + ico[i].x * radius,
                                                centre.y + ico[i].y * radius,
                                                centre.z + ico[i].z * radius));
    }
    for (i32 i = 0; i < 60; ++i) scene.indices.push_back(base + kTris[i]);
    for (i32 t = 0; t < 20; ++t) scene.materialIndices.push_back(materialIndex);
}

} // namespace

AcousticScene BuildAcousticScene(ECS::World* world, const AcousticSceneOptions& options) {
    AcousticScene scene;
    if (!world) return scene;

    // What an entity is made of, resolved once per entity rather than per
    // triangle: the answer cannot change between two faces of the same box.
    auto surfaceOf = [&](ECS::Entity e) {
        return ResolveSurface(world->GetComponent<ECS::AudioCollisionComponent>(e),
                              world->GetComponent<ECS::MaterialComponent>(e));
    };

    if (options.includeBoxColliders) {
        for (ECS::Entity e : world->GetEntitiesWithComponent<ECS::BoxColliderComponent>()) {
            const auto* box = world->GetComponent<ECS::BoxColliderComponent>(e);
            const auto* xf = world->GetComponent<ECS::TransformComponent>(e);
            if (!box || !xf) continue;
            const i32 mat = static_cast<i32>(scene.materials.IndexFor(surfaceOf(e)));
            // Collider sizes are WORLD space and are not multiplied by the
            // transform scale -- the same rule the physics backends follow. A
            // box collider that grew with its entity here and not in Jolt would
            // put the sound of a wall somewhere the wall is not.
            EmitBox(scene, RigidTransform(*xf), box->center,
                    Math::Vector3(box->size.x * 0.5f, box->size.y * 0.5f, box->size.z * 0.5f),
                    mat);
        }
    }

    if (options.includeSphereColliders) {
        for (ECS::Entity e : world->GetEntitiesWithComponent<ECS::SphereColliderComponent>()) {
            const auto* sphere = world->GetComponent<ECS::SphereColliderComponent>(e);
            const auto* xf = world->GetComponent<ECS::TransformComponent>(e);
            if (!sphere || !xf) continue;
            const i32 mat = static_cast<i32>(scene.materials.IndexFor(surfaceOf(e)));
            EmitSphere(scene, RigidTransform(*xf), sphere->center, sphere->radius, mat);
        }
    }

    if (options.includeMeshColliders) {
        for (ECS::Entity e : world->GetEntitiesWithComponent<ECS::MeshColliderComponent>()) {
            const auto* col = world->GetComponent<ECS::MeshColliderComponent>(e);
            const auto* xf = world->GetComponent<ECS::TransformComponent>(e);
            if (!col || !xf || col->vertices.empty() || col->indices.size() < 3) continue;

            const i32 mat = static_cast<i32>(scene.materials.IndexFor(surfaceOf(e)));
            // A mesh collider's vertices are already in the entity's local space and
            // the physics backend applies no scale to them either.
            const Math::Matrix4 world4 = RigidTransform(*xf);
            const usize tris = col->indices.size() / 3;

            // A carved cave is hundreds of thousands of triangles and sound does
            // not care about a centimetre of surface detail. Past the budget the
            // mesh contributes its bounding box: still the right room shape, at
            // a cost the simulator can carry. Better an approximate room than an
            // absent one, which is what it was before.
            if (options.meshTriangleBudget > 0 && tris > options.meshTriangleBudget) {
                Math::Vector3 lo = col->vertices[0], hi = col->vertices[0];
                for (const auto& v : col->vertices) {
                    lo.x = std::min(lo.x, v.x); lo.y = std::min(lo.y, v.y); lo.z = std::min(lo.z, v.z);
                    hi.x = std::max(hi.x, v.x); hi.y = std::max(hi.y, v.y); hi.z = std::max(hi.z, v.z);
                }
                EmitBox(scene, world4,
                        Math::Vector3((lo.x + hi.x) * 0.5f, (lo.y + hi.y) * 0.5f, (lo.z + hi.z) * 0.5f),
                        Math::Vector3((hi.x - lo.x) * 0.5f, (hi.y - lo.y) * 0.5f, (hi.z - lo.z) * 0.5f),
                        mat);
                continue;
            }

            const i32 base = static_cast<i32>(scene.vertices.size());
            for (const auto& v : col->vertices) {
                scene.vertices.push_back(TransformPoint(world4, v.x, v.y, v.z));
            }
            const usize vertexCount = col->vertices.size();
            for (usize i = 0; i + 2 < col->indices.size(); i += 3) {
                // An index past the end of the vertex list is a corrupt collider,
                // and feeding it to a simulator is a read off the end of an array
                // inside somebody else's library.
                if (col->indices[i] >= vertexCount || col->indices[i + 1] >= vertexCount ||
                    col->indices[i + 2] >= vertexCount) {
                    continue;
                }
                scene.indices.push_back(base + static_cast<i32>(col->indices[i]));
                scene.indices.push_back(base + static_cast<i32>(col->indices[i + 1]));
                scene.indices.push_back(base + static_cast<i32>(col->indices[i + 2]));
                scene.materialIndices.push_back(mat);
            }
        }
    }

    return scene;
}

} // namespace Audio
} // namespace Enjin
