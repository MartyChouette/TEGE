#include "Enjin/ECS/MeshEdit.h"
#include "Enjin/ECS/World.h"

namespace Enjin {
namespace ECS {

void MarkMeshChanged(World& world, Entity entity,
                     ProceduralMeshComponent::Source source,
                     bool topologyChanged) {
    if (!world.HasComponent<ProceduralMeshComponent>(entity)) {
        ProceduralMeshComponent pm;
        pm.source = source;
        // A mesh that has never been uploaded needs its buffers built, which is
        // what topologyDirty asks for, whatever kind of change brought us here.
        pm.topologyDirty = true;
        world.AddComponent<ProceduralMeshComponent>(entity, pm);
        return;
    }

    auto* pm = world.GetComponent<ProceduralMeshComponent>(entity);
    if (!pm) return;
    pm->source = source;
    if (topologyChanged) pm->topologyDirty = true;
    else                 pm->meshDirty = true;
}

} // namespace ECS
} // namespace Enjin
