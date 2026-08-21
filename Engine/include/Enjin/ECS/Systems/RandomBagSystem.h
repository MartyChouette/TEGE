#pragma once

#include "Enjin/ECS/Components/RandomBag.h"

namespace Enjin {
namespace ECS {

class World;

// Draw logic for RandomBagComponent. Kept out of the component header so the
// stateful PRNG + queue handling stays in one place and is unit-testable. All
// calls mutate the bag's runtime state and run on the main thread (scripts pull
// during play, which is main-thread per adr-0004).
class RandomBagSystem {
public:
    // Reset the bag: reseed the PRNG (from bag.seed, or a fresh random seed if 0)
    // and clear the draw queue. Called lazily on the first Draw and by RandomBag_Reset.
    static void Reset(RandomBagComponent& bag);

    // Pull the next item. Returns its index into bag.items, or -1 if the bag is
    // empty. Advances all runtime state (queue, lastDrawn, PRNG).
    static i32 DrawIndex(RandomBagComponent& bag);

    // Convenience: the drawn item's name (empty string if the bag is empty).
    static std::string Draw(RandomBagComponent& bag);

    // Items left before a reshuffle. For Uniform/Weighted (infinite), returns the
    // item count. For NoReplace/Deck, returns the current queue size.
    static u32 Remaining(const RandomBagComponent& bag);

    // Play-start pass: reset every RandomBagComponent so editor "Test Draw" state
    // (and any leftover queue/seed) never leaks into play. Main-thread only.
    static void ResetAll(World* world);
};

} // namespace ECS
} // namespace Enjin
