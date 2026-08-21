#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include <string>
#include <vector>

namespace Enjin {
namespace ECS {

// The stream-feeder of the procgen suite. Where the DungeonGenerator builds a
// whole space once, the RandomBag hands out ONE item per pull, on demand, from
// script or gameplay code. It is stateful by design: a bag is not "random each
// call" — the no-replacement modes shuffle a queue and draw it down before
// refilling, which is what makes a 7-piece Tetris bag or a shuffled card deck
// feel fair instead of streaky.
//
// Author the item list + mode in the inspector; pull from AngelScript with
// RandomBag_Draw(self). Runtime draw logic lives in RandomBagSystem.
struct RandomBagComponent {
    enum class Mode : u8 {
        Uniform = 0,   // equal chance every pull, WITH replacement (plain dice roll)
        Weighted,      // chance proportional to each item's weight, WITH replacement (loot table)
        NoReplace,     // shuffle all items, draw each once, reshuffle when empty (Tetris 7-bag)
        Deck           // like NoReplace but each item appears `weight` times (a deck of cards)
    };

    struct Item {
        std::string name;   // what Draw() returns (e.g. "I_piece", "sword", "goblin")
        f32 weight = 1.0f;  // Weighted: relative probability. Deck: copy count (rounded up, min 1). Uniform/NoReplace: ignored
    };

    Mode mode = Mode::NoReplace;
    std::vector<Item> items;
    u32 seed = 0;                     // 0 = pick a fresh non-deterministic seed on first draw / reset
    bool avoidImmediateRepeat = false; // Uniform/Weighted only: never return the same item twice in a row

    // ---- Runtime state (NOT serialized; rebuilt on first draw or Reset) ----
    bool  rngInitialized = false;
    u32   rngState = 0;               // xorshift PRNG state, seeded from `seed`
    u32   lastSeed = 0;               // the seed actually used (shown in inspector)
    std::vector<u32> queue;           // remaining item indices for NoReplace/Deck (drawn from the back)
    i32   lastDrawn = -1;             // index of the previous draw (for avoidImmediateRepeat)
    std::string editorPreview;        // last "Test Draw" result shown in the inspector (editor-only)
};

} // namespace ECS
} // namespace Enjin
