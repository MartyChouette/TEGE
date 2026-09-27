#pragma once
#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Math/Vector.h"

namespace Enjin {
namespace ECS {

class World;

// Turns every BillboardComponent with faceCamera set toward a camera.
//
// The component was authored, saved and scriptable, and nothing read it, so a
// billboard never turned (SD-27). This is the one rule all three runtimes use:
// the players call it from RenderSystem::Update with the camera they render
// through, and the editor calls it before each viewport draw with that
// viewport's camera.
//
// The entity's local +Z is pointed at the camera, because that is the side the
// Entity menu's Quad faces. lockY keeps the entity upright and turns it about
// world Y only (trees, signs); off, it tilts to face the camera fully.
// rotationOffset is degrees about the entity's own Y after facing.
//
// A billboard owns its rotation while faceCamera is on, the way a physics body
// owns its position: whatever rotation it was given is replaced. Parented
// billboards are handled in world space and written back as local rotation.
//
// Returns how many billboards changed rotation. Each one that did has its world
// matrix cache and its children's cleared, since the editor never runs the
// per-frame cache reset.
ENJIN_API u32 FaceBillboards(World* world, const Math::Vector3& cameraPosition);

} // namespace ECS
} // namespace Enjin
