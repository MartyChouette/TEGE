#pragma once
#include "Enjin/ECS/Entity.h"
#include <cstddef>
#include <unordered_set>

namespace Enjin {
namespace Physics {

// Two entities in contact, held as their FULL handles.
//
// Both physics backends used to key a contact as one u64, (min << 32) | max.
// An entity handle is 64 bits (generation << 32 | slot), so that shift threw
// away the smaller handle's generation and ORed the larger handle's generation
// over the smaller one's slot. It worked while every slot was on generation 0,
// which is every slot until one is reused. A spawned projectile or respawned
// pickup reuses one, and from then on an Exit decoded from the key named
// neither entity, two different pairs could share a key, and the 2D backend's
// purge on destroy never matched, so its stale pairs were never cleared.
//
// Nothing is packed or decoded now: the pair is the two handles, smaller first.
struct CollisionPair {
    ECS::Entity first = ECS::INVALID_ENTITY;    // the smaller handle
    ECS::Entity second = ECS::INVALID_ENTITY;   // the larger handle

    static CollisionPair Of(ECS::Entity a, ECS::Entity b) {
        return (a < b) ? CollisionPair{a, b} : CollisionPair{b, a};
    }
    bool Involves(ECS::Entity e) const { return first == e || second == e; }
    bool operator==(const CollisionPair& o) const { return first == o.first && second == o.second; }
};

struct CollisionPairHash {
    std::size_t operator()(const CollisionPair& p) const noexcept {
        // splitmix64-style mix of each half, so handles that differ only in
        // generation do not land in the same bucket.
        auto mix = [](u64 x) {
            x ^= x >> 30; x *= 0xbf58476d1ce4e5b9ull;
            x ^= x >> 27; x *= 0x94d049bb133111ebull;
            return x ^ (x >> 31);
        };
        return static_cast<std::size_t>(mix(p.first) ^ (mix(p.second) * 31u));
    }
};

using CollisionPairSet = std::unordered_set<CollisionPair, CollisionPairHash>;

} // namespace Physics
} // namespace Enjin
