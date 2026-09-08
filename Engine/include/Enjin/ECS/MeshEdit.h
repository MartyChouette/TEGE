#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/ECS/Entity.h"
#include "Enjin/ECS/Components/ProceduralMesh.h"

namespace Enjin {
namespace ECS {

class World;

// Tell the renderer that an entity's MeshComponent data changed and has to be
// re-uploaded.
//
// The renderer builds an entity's GPU buffers once and then reuses them, so a
// MeshComponent rewritten in place keeps drawing the OLD geometry until
// something says otherwise. ProceduralMeshComponent's dirty flags are that
// something -- its own inspector line says "Removing it stops GPU uploads" --
// and this is the one place that knows the protocol:
//
//   topologyChanged : the vertex or index COUNT moved, so the buffers have to
//                     be destroyed and rebuilt at the new size.
//   otherwise       : the same number of vertices moved, so the existing
//                     buffer is re-filled.
//
// Getting that distinction wrong means either a stale buffer on screen or a
// needless reallocation every edit.
ENJIN_API void MarkMeshChanged(World& world, Entity entity,
                               ProceduralMeshComponent::Source source,
                               bool topologyChanged);

} // namespace ECS
} // namespace Enjin
