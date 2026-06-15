#pragma once

#include "Enjin/Platform/Platform.h"
#include <random>

namespace Enjin {
namespace ECS {

// StableIdComponent - a persistent, scene-authoring identity for an entity.
//
// The runtime Entity handle (generational u64) is NOT stable across a scene
// reload: SceneSerializer remaps every entity through an oldToNew table on
// load, so the same object comes back with a different handle each time. That
// is fine for runtime references but useless as a durable address.
//
// The stable id is the durable address. It is assigned once (on creation in the
// editor, or backfilled on first load of a legacy scene) and persisted in the
// .enjin file. It is what override layers, the VWS, and future collaborative
// merge key off: "Entity 402" in a layer delta means StableIdComponent::id 402,
// resolved to whatever runtime Entity that scene load happened to produce.
//
// Ids are random 64-bit values, not a per-scene counter, so two people editing
// different copies offline can create entities without colliding when their
// work is later merged.
struct ENJIN_API StableIdComponent {
    u64 id = 0;

    StableIdComponent() = default;
    explicit StableIdComponent(u64 v) : id(v) {}
};

// Generate a fresh, collision-resistant stable id. Never returns 0 (0 = unset).
inline u64 GenerateStableId() {
    // thread_local PRNG seeded once per thread from the OS entropy source.
    // 64 random bits make accidental collision across a project negligible.
    static thread_local std::mt19937_64 rng{ std::random_device{}() };
    static thread_local std::uniform_int_distribution<u64> dist;
    u64 v = dist(rng);
    return v == 0 ? 1 : v;
}

} // namespace ECS
} // namespace Enjin
