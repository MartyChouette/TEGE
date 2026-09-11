#include "Enjin/AI/NavmeshBake.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Hierarchy.h"
#include "Enjin/ECS/Components/Mesh.h"
#include "Enjin/ECS/Components/Name.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/Logging/Log.h"

#include <cstdio>
#include <vector>

namespace Enjin {
namespace AI {

namespace {

// Same tag rule as GameplaySystem: TagComponent, or the entity name. Two systems
// disagreeing about what "crate" means is a bug an author cannot see.
bool MatchesTag(ECS::World* world, ECS::Entity entity, const std::string& tag) {
    if (tag.empty()) return false;
    if (auto* tags = world->GetComponent<ECS::TagComponent>(entity)) {
        if (tags->HasTag(tag)) return true;
    }
    if (auto* name = world->GetComponent<ECS::NameComponent>(entity)) {
        if (name->name == tag) return true;
    }
    return false;
}

bool InsideBounds(const Math::Vector3& p, const Math::Vector3& lo,
                  const Math::Vector3& hi) {
    return p.x >= lo.x && p.x <= hi.x &&
           p.y >= lo.y && p.y <= hi.y &&
           p.z >= lo.z && p.z <= hi.z;
}

} // namespace

ECS::Entity FindNavmeshVolume(ECS::World* world) {
    if (!world) return ECS::INVALID_ENTITY;

    ECS::Entity found = ECS::INVALID_ENTITY;
    u32 count = 0;
    for (ECS::Entity e : world->GetEntitiesWithComponent<ECS::NavmeshVolumeComponent>()) {
        if (found == ECS::INVALID_ENTITY) found = e;
        ++count;
    }
    if (count > 1) {
        // The pathfinder holds ONE navmesh. Silently taking the first would make
        // which volume wins depend on entity creation order, which is invisible.
        ENJIN_LOG_WARN(AI, "Scene has %u navmesh volumes; only the first is used. "
                           "Merge them into one volume covering the whole level.",
                       count);
    }
    return found;
}

NavmeshBakeResult BakeNavmeshVolume(ECS::World* world, ECS::Entity entity,
                                    NavmeshGenerator& generator) {
    NavmeshBakeResult result;

    if (!world || entity == ECS::INVALID_ENTITY) {
        result.message = "No world or no volume entity.";
        return result;
    }
    auto* volume = world->GetComponent<ECS::NavmeshVolumeComponent>(entity);
    if (!volume) {
        result.message = "Entity has no NavmeshVolumeComponent.";
        return result;
    }

    const Math::Vector3 lo = volume->boundsMin;
    const Math::Vector3 hi = volume->boundsMax;
    if (hi.x <= lo.x || hi.y <= lo.y || hi.z <= lo.z) {
        result.message = "Bounds are empty or inverted: max must exceed min on "
                         "every axis.";
        volume->baked = false;
        volume->lastBakeMessage = result.message;
        return result;
    }

    // --- Grid ---------------------------------------------------------------
    if (volume->source == ECS::NavmeshVolumeComponent::Source::Grid) {
        if (volume->gridCellSize <= 0.0f) {
            result.message = "Grid cell size must be greater than zero.";
            volume->baked = false;
            volume->lastBakeMessage = result.message;
            return result;
        }
        if (!generator.GenerateGrid(lo, hi, volume->gridCellSize, volume->gridHeight)) {
            result.message = "Grid generation failed.";
            volume->baked = false;
            volume->lastBakeMessage = result.message;
            return result;
        }
        result.success = true;
        result.polygons = static_cast<u32>(generator.GetNavmesh().GetPolygons().size());
        char buf[192];
        std::snprintf(buf, sizeof(buf), "Grid: %u cells at height %.2f.",
                      result.polygons, volume->gridHeight);
        result.message = buf;

        volume->baked = true;
        volume->bakedPolygons = result.polygons;
        volume->sourceMeshes = 0;
        volume->sourceTriangles = 0;
        volume->lastBakeMessage = result.message;
        return result;
    }

    // --- Scene geometry -----------------------------------------------------
    std::vector<Math::Vector3> positions;
    std::vector<u32> indices;
    u32 meshCount = 0;
    u32 skippedByTag = 0;
    u32 skippedByBounds = 0;

    for (ECS::Entity e : world->GetEntitiesWithComponent<ECS::MeshComponent>()) {
        if (!volume->includeTag.empty() && !MatchesTag(world, e, volume->includeTag)) {
            ++skippedByTag;
            continue;
        }
        if (!volume->excludeTag.empty() && MatchesTag(world, e, volume->excludeTag)) {
            ++skippedByTag;
            continue;
        }

        const auto* mesh = world->GetComponent<ECS::MeshComponent>(e);
        const auto* xf = world->GetComponent<ECS::TransformComponent>(e);
        if (!mesh || !xf || mesh->vertices.empty() || mesh->indices.empty()) continue;

        // The full parent chain, not this entity's local transform: a floor under a
        // rotated room root would otherwise be baked in the wrong place, and the
        // agent would path across empty air.
        const Math::Matrix4 model = ECS::ComputeWorldMatrix(world, e);

        std::vector<Math::Vector3> world_verts;
        world_verts.reserve(mesh->vertices.size());
        for (const auto& v : mesh->vertices) {
            const Math::Vector4 wp =
                model * Math::Vector4(v.position.x, v.position.y, v.position.z, 1.0f);
            world_verts.push_back(Math::Vector3(wp.x, wp.y, wp.z));
        }

        // Keep a triangle when ANY of its corners is inside the volume. Requiring
        // all three would drop the floor triangles that cross the boundary and
        // leave a ring of holes around the edge of every volume.
        const u32 base = static_cast<u32>(positions.size());
        bool used = false;
        for (usize i = 0; i + 2 < mesh->indices.size(); i += 3) {
            const u32 i0 = mesh->indices[i], i1 = mesh->indices[i + 1],
                      i2 = mesh->indices[i + 2];
            if (i0 >= world_verts.size() || i1 >= world_verts.size() ||
                i2 >= world_verts.size()) {
                continue;
            }
            if (!InsideBounds(world_verts[i0], lo, hi) &&
                !InsideBounds(world_verts[i1], lo, hi) &&
                !InsideBounds(world_verts[i2], lo, hi)) {
                ++skippedByBounds;
                continue;
            }
            indices.push_back(base + i0);
            indices.push_back(base + i1);
            indices.push_back(base + i2);
            used = true;
        }

        if (used) {
            positions.insert(positions.end(), world_verts.begin(), world_verts.end());
            ++meshCount;
        }
    }

    result.meshes = meshCount;
    result.triangles = static_cast<u32>(indices.size() / 3);

    if (indices.empty()) {
        // Three different nothings, three different fixes. Saying "bake failed"
        // for all of them is the whole reason a bake button gets pressed twice and
        // then abandoned.
        char buf[256];
        if (skippedByTag > 0 && meshCount == 0) {
            std::snprintf(buf, sizeof(buf),
                          "No meshes matched the tag filter (%u skipped). Check "
                          "Include Tag / Exclude Tag.", skippedByTag);
        } else if (skippedByBounds > 0) {
            std::snprintf(buf, sizeof(buf),
                          "Every triangle was outside the bounds (%u skipped). "
                          "Widen Bounds Min / Bounds Max to cover the level.",
                          skippedByBounds);
        } else {
            std::snprintf(buf, sizeof(buf),
                          "No meshes in the scene have vertex data to bake from.");
        }
        result.message = buf;
        volume->baked = false;
        volume->bakedPolygons = 0;
        volume->sourceMeshes = 0;
        volume->sourceTriangles = 0;
        volume->lastBakeMessage = result.message;
        return result;
    }

    if (!generator.Generate(positions, indices, volume->settings)) {
        result.message = "Generation failed on " + std::to_string(result.triangles) +
                         " triangles.";
        volume->baked = false;
        volume->lastBakeMessage = result.message;
        return result;
    }

    result.success = true;
    result.polygons = static_cast<u32>(generator.GetNavmesh().GetPolygons().size());

    char buf[256];
    if (result.polygons == 0) {
        // Succeeded and produced nothing walkable. That is a real answer -- every
        // surface was too steep, or the agent does not fit -- and it must not read
        // as a completed bake.
        std::snprintf(buf, sizeof(buf),
                      "0 walkable polygons from %u triangles in %u mesh(es). Every "
                      "surface failed the slope (%.0f deg) or agent size "
                      "(r %.2f, h %.2f) test.",
                      result.triangles, result.meshes, volume->settings.agentMaxSlope,
                      volume->settings.agentRadius, volume->settings.agentHeight);
        result.success = false;
    } else {
        std::snprintf(buf, sizeof(buf), "%u polygons from %u triangles in %u mesh(es).",
                      result.polygons, result.triangles, result.meshes);
    }
    result.message = buf;

    volume->baked = result.success;
    volume->bakedPolygons = result.polygons;
    volume->sourceMeshes = result.meshes;
    volume->sourceTriangles = result.triangles;
    volume->lastBakeMessage = result.message;

    ENJIN_LOG_INFO(AI, "Navmesh bake: %s", result.message.c_str());
    return result;
}

} // namespace AI
} // namespace Enjin
