#include "Enjin/ECS/Systems/RandomBagSystem.h"
#include "Enjin/ECS/World.h"
#include <random>
#include <cmath>

namespace Enjin {
namespace ECS {

// Small deterministic PRNG so a given seed always reproduces the same sequence,
// independent of the platform's std::mt19937 implementation. xorshift32.
static inline u32 NextRand(u32& state) {
    u32 x = state ? state : 0x9E3779B9u;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    state = x;
    return x;
}

static inline u32 RandBelow(u32& state, u32 n) {
    if (n == 0) return 0;
    return NextRand(state) % n;
}

// Rebuild the shuffled queue for the no-replacement modes. NoReplace = each item
// once; Deck = each item `weight` times (rounded up, min 1). Fisher-Yates shuffle
// so every ordering is equally likely.
static void RefillQueue(RandomBagComponent& bag) {
    bag.queue.clear();
    if (bag.items.empty()) return;

    if (bag.mode == RandomBagComponent::Mode::Deck) {
        for (u32 i = 0; i < bag.items.size(); ++i) {
            f32 w = bag.items[i].weight;
            u32 copies = (w <= 1.0f) ? 1u : static_cast<u32>(std::ceil(w));
            if (copies > 4096u) copies = 4096u; // guard against absurd authored counts
            for (u32 c = 0; c < copies; ++c) bag.queue.push_back(i);
        }
    } else { // NoReplace
        for (u32 i = 0; i < bag.items.size(); ++i) bag.queue.push_back(i);
    }

    // Fisher-Yates over the whole queue.
    for (usize i = bag.queue.size(); i > 1; --i) {
        u32 j = RandBelow(bag.rngState, static_cast<u32>(i));
        std::swap(bag.queue[i - 1], bag.queue[j]);
    }
}

void RandomBagSystem::Reset(RandomBagComponent& bag) {
    u32 seed = bag.seed;
    if (seed == 0) {
        std::random_device rd;
        seed = rd();
        if (seed == 0) seed = 1;
    }
    bag.lastSeed = seed;
    bag.rngState = seed;
    bag.rngInitialized = true;
    bag.lastDrawn = -1;
    bag.queue.clear();
}

i32 RandomBagSystem::DrawIndex(RandomBagComponent& bag) {
    if (bag.items.empty()) return -1;
    if (!bag.rngInitialized) Reset(bag);

    const u32 count = static_cast<u32>(bag.items.size());

    switch (bag.mode) {
        case RandomBagComponent::Mode::Uniform: {
            i32 pick = static_cast<i32>(RandBelow(bag.rngState, count));
            if (bag.avoidImmediateRepeat && count > 1 && pick == bag.lastDrawn) {
                // Re-roll into the remaining count-1 slots so distribution stays even.
                i32 alt = static_cast<i32>(RandBelow(bag.rngState, count - 1));
                pick = (alt >= bag.lastDrawn) ? alt + 1 : alt;
            }
            bag.lastDrawn = pick;
            return pick;
        }
        case RandomBagComponent::Mode::Weighted: {
            f32 total = 0.0f;
            for (const auto& it : bag.items) total += (it.weight > 0.0f ? it.weight : 0.0f);
            if (total <= 0.0f) { // all weights zero → fall back to uniform
                bag.lastDrawn = static_cast<i32>(RandBelow(bag.rngState, count));
                return bag.lastDrawn;
            }
            // Optionally exclude the previous pick from the roll.
            f32 usable = total;
            if (bag.avoidImmediateRepeat && count > 1 && bag.lastDrawn >= 0 &&
                bag.lastDrawn < static_cast<i32>(count)) {
                f32 lw = bag.items[bag.lastDrawn].weight;
                if (lw > 0.0f && usable - lw > 0.0f) usable -= lw;
            }
            f32 r = (static_cast<f32>(NextRand(bag.rngState)) / static_cast<f32>(0xFFFFFFFFu)) * usable;
            i32 pick = static_cast<i32>(count) - 1;
            for (u32 i = 0; i < count; ++i) {
                if (bag.avoidImmediateRepeat && static_cast<i32>(i) == bag.lastDrawn &&
                    usable != total)
                    continue; // skipped item, not part of this roll
                f32 w = (bag.items[i].weight > 0.0f ? bag.items[i].weight : 0.0f);
                if (r < w) { pick = static_cast<i32>(i); break; }
                r -= w;
            }
            bag.lastDrawn = pick;
            return pick;
        }
        case RandomBagComponent::Mode::NoReplace:
        case RandomBagComponent::Mode::Deck: {
            if (bag.queue.empty()) RefillQueue(bag);
            if (bag.queue.empty()) return -1;
            u32 idx = bag.queue.back();
            bag.queue.pop_back();
            bag.lastDrawn = static_cast<i32>(idx);
            return static_cast<i32>(idx);
        }
    }
    return -1;
}

std::string RandomBagSystem::Draw(RandomBagComponent& bag) {
    i32 idx = DrawIndex(bag);
    if (idx < 0 || idx >= static_cast<i32>(bag.items.size())) return std::string();
    return bag.items[idx].name;
}

void RandomBagSystem::ResetAll(World* world) {
    if (!world) return;
    for (Entity e : world->GetEntitiesWithComponent<RandomBagComponent>()) {
        if (auto* b = world->GetComponent<RandomBagComponent>(e)) Reset(*b);
    }
}

u32 RandomBagSystem::Remaining(const RandomBagComponent& bag) {
    if (bag.mode == RandomBagComponent::Mode::NoReplace ||
        bag.mode == RandomBagComponent::Mode::Deck) {
        return static_cast<u32>(bag.queue.size());
    }
    return static_cast<u32>(bag.items.size());
}

} // namespace ECS
} // namespace Enjin
