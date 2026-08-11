#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Scene/SceneSerializer.h"
#include "Enjin/Math/Vector.h"
#include <string>
#include <vector>

namespace Enjin {
namespace Scene {

// ---------------------------------------------------------------------------
// Override layers (the "DAW track" model for scenes).
//
// A scene on disk is a pristine BASE (.enjin). A Layer is a sparse, non-
// destructive set of edits stacked over that base: tweak a value, add a
// component, spawn or hide an entity. The base is never mutated; layers
// resolve over it at load time. Delete a layer and its edits vanish; merge a
// layer and its edits fold into the base.
//
// Everything is addressed by StableIdComponent::id (see StableId.h), NOT the
// runtime Entity handle, so a delta stays valid across reloads.
//
// Resolution happens at the JSON level: the base scene JSON is the document,
// each enabled layer patches it (set/erase a component key, append/drop an
// entity object), and the merged document is loaded once through the normal
// SceneSerializer path. That means layers need no per-component plumbing and
// cost about one ordinary scene load. The merged-JSON form is also exactly
// what a per-layer file on disk serializes, which is what gives layers their
// clean Git story: distinct layers are distinct files, so they never conflict.
// ---------------------------------------------------------------------------

// One edited component on one entity.
//   json  = the serialized component object (same shape as the scene-JSON value
//           under that key, i.e. what SceneSerializer::SerializeOneComponent
//           returns).
//   An EMPTY json means "remove this component from the resolved entity".
struct ComponentDelta {
    std::string key;   // serializer component key, e.g. "transform", "light"
    std::string json;  // component object as JSON text; empty => remove
};

// All edits a layer makes to one entity (addressed by stable id).
struct EntityDelta {
    u64  stableId  = 0;
    bool created   = false;  // entity is introduced by this layer
    bool destroyed = false;  // tombstone: hide a base-scene entity
    std::vector<ComponentDelta> components;
};

// A single non-destructive layer over the base scene.
struct ENJIN_API Layer {
    std::string name;
    bool enabled = true;
    bool locked  = false;            // UI hint: edits to this layer are blocked
    // Viewport color-coding: every entity this layer touches is marked in `color`
    // when `highlight` is on, so you can see at a glance which layer owns what.
    Math::Vector3 color = Math::Vector3(0.4f, 0.8f, 1.0f);
    bool highlight = true;
    std::vector<EntityDelta> entities;

    // Serialize/deserialize this layer to its own JSON document (per-layer file).
    std::string ToJson(bool prettyPrint = true) const;
    static Layer FromJson(const std::string& jsonStr);

    // Find (or create) the delta for a given stable id.
    EntityDelta& EntityFor(u64 stableId);
    const EntityDelta* FindEntity(u64 stableId) const;
};

// An ordered stack of layers applied bottom-to-top over a base scene.
struct ENJIN_API LayerStack {
    std::vector<Layer> layers;   // index 0 = bottom, applied first

    // Merge all ENABLED layers over the given base scene JSON and return the
    // resolved scene JSON. The base string is not modified.
    std::string Resolve(const std::string& baseSceneJson) const;

    // Convenience: resolve, then load the result into `world` (clears it first).
    DeserializationResult ResolveInto(ECS::World& world, const std::string& baseSceneJson) const;
};

} // namespace Scene
} // namespace Enjin
