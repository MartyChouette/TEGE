#pragma once

// What the two creative surfaces both build, in one place.
//
// The editor has two deliberate surfaces that place the same objects: the
// Build Palette (a quick-place window in the full editor) and Creative Mode's
// tool rail. Both are keepers. What is not a keeper is each having its own copy
// of what a "grass patch" or a "barrel" IS.
//
// When the rail gained Plants and Prop, the density curves and the vertical
// drop offsets were COPIED out of the palette rather than shared -- so the two
// surfaces agreed only because the same numbers had been typed twice. That is a
// drift waiting to happen, and the drift would be invisible: grass placed one
// way and grass placed the other would differ by an amount nobody would think
// to compare.
//
// So the objects are defined here, once, and both surfaces call in. The density
// curve is pure and tested; the creation functions take an entity that already
// exists so each surface keeps its own naming, selection and undo behaviour.

#include "Enjin/Platform/Types.h"
#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/ECS/Entity.h"

namespace Enjin {
namespace ECS { class World; }

namespace Editor {

// ---------------------------------------------------------------------------
// Plants
// ---------------------------------------------------------------------------

enum class PlantKind : u8 { Grass = 0, Shrubs, Trees, Count };

ENJIN_API const char* PlantKindName(PlantKind kind);

// How many instances a patch of this size gets.
//
// Scales with AREA and is clamped at both ends: a patch the size of a doormat
// still gets enough blades to read as grass, and one the size of a field does
// not try to place a hundred thousand. The per-kind numbers are the difference
// between grass and trees and are the thing most worth not duplicating -- a
// grove at grass density is a solid wall of trunks.
ENJIN_API u32 PlantDensity(PlantKind kind, f32 halfX, f32 halfZ, f32 densityScale);

// Attach the volume component for this kind. Safe to call on an entity that
// already has one: the surfaces create at click and size during a drag.
ENJIN_API void AddPlantVolume(ECS::World* world, ECS::Entity entity, PlantKind kind);

// Set the patch's footprint and instance count. Called every frame of a drag by
// the palette, and once by the rail.
ENJIN_API void SizePlantVolume(ECS::World* world, ECS::Entity entity, PlantKind kind,
                               f32 halfX, f32 halfZ, f32 densityScale);

// ---------------------------------------------------------------------------
// Props
// ---------------------------------------------------------------------------

// Block is here because the Build Palette offers it. Creative Mode does not:
// its Brush tool already makes boxes, and a second way to make a box on the
// same rail would be two buttons for one thing.
enum class PropKind : u8 { Block = 0, Ball, Light, PhysicsBox, Barrel, SpawnPoint, Count };

ENJIN_API const char* PropKindName(PropKind kind);

// Build the prop on `entity`, positioned from a point on the ground.
//
// The vertical offsets live in here and are not decoration: everything lands on
// a ground hit, so a sphere of radius 0.5 has to rise by 0.5 or it is buried to
// its equator, and a physics box starts three metres up so it has somewhere to
// fall from. A box dropped flush with the floor never visibly falls, which reads
// as physics being broken rather than as a placement choice.
//
// Requires a TransformComponent on the entity, which it writes.
ENJIN_API void CreateProp(ECS::World* world, ECS::Entity entity, PropKind kind,
                          const Math::Vector3& groundPoint);

} // namespace Editor
} // namespace Enjin
