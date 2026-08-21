#include "Enjin/ECS/Systems/WFCSystem.h"
#include "Enjin/ECS/World.h"
#include "Enjin/Procedural/ProceduralAlgorithms.h"
#include "Enjin/Logging/Log.h"
#include <random>
#include <algorithm>

namespace Enjin {
namespace ECS {

// Compile the component's edge-socket tiles into the solver's per-tile adjacency.
// Two tiles may sit next to each other only when the labels on their touching edges
// match. Socket edge order is 0=N,1=E,2=S,3=W; the solver's allowedNeighbors index
// is 0=N,1=S,2=E,3=W (see WaveFunctionCollapse::Generate).
static void BuildTiles(const WFCComponent& gen,
                       std::vector<Procedural::WaveFunctionCollapse::WFCTile>& out) {
    const u32 n = static_cast<u32>(gen.tiles.size());
    out.clear();
    out.reserve(n);
    for (u32 i = 0; i < n; ++i) {
        Procedural::WaveFunctionCollapse::WFCTile t;
        t.id = i;   // id == index so the result grid maps straight back to gen.tiles
        const auto& ei = gen.tiles[i].edges;
        for (u32 j = 0; j < n; ++j) {
            const auto& ej = gen.tiles[j].edges;
            if (ei[0] == ej[2]) t.allowedNeighbors[0].push_back(j); // North: my N == their S
            if (ei[2] == ej[0]) t.allowedNeighbors[1].push_back(j); // South: my S == their N
            if (ei[1] == ej[3]) t.allowedNeighbors[2].push_back(j); // East:  my E == their W
            if (ei[3] == ej[1]) t.allowedNeighbors[3].push_back(j); // West:  my W == their E
        }
        out.push_back(std::move(t));
    }
}

bool WFCSystem::Generate(WFCComponent& gen, TilemapComponent& tilemap) {
    u32 w = std::clamp(gen.width,  1u, 1024u);
    u32 h = std::clamp(gen.height, 1u, 1024u);

    if (gen.tiles.empty()) {
        ENJIN_LOG_WARN(Build, "WFC: no tiles authored — nothing to solve");
        gen.lastSuccess = false;
        return false;
    }

    // seed 0 = fresh non-deterministic base; else reproducible.
    u32 baseSeed = gen.seed;
    if (baseSeed == 0) {
        std::random_device rd;
        baseSeed = rd();
        if (baseSeed == 0) baseSeed = 1;
    }

    Procedural::WaveFunctionCollapse::Params p;
    p.width = w; p.height = h;
    p.maxBacktracks = gen.maxBacktracks;
    BuildTiles(gen, p.tiles);

    // Reseed on contradiction: the solver backtracks internally, but a genuinely
    // over-constrained rule set can still fail, so try a few fresh seeds. Keep the
    // final result (success or best partial) to paint either way.
    const u32 retries = gen.retryOnFail ? std::max(1u, gen.maxRetries) : 1u;
    Procedural::WaveFunctionCollapse::Result result;
    u32 usedSeed = baseSeed;
    for (u32 attempt = 0; attempt < retries; ++attempt) {
        usedSeed = baseSeed + attempt;
        p.seed = usedSeed;
        result = Procedural::WaveFunctionCollapse::Generate(p);
        if (result.success) break;
    }

    gen.lastSeed = usedSeed;
    gen.lastSuccess = result.success;

    // Paint the tilemap from the (possibly partial) grid.
    tilemap.width = w;
    tilemap.height = h;
    tilemap.tiles.assign(static_cast<usize>(w) * h, -1);
    if (gen.fillCollision) tilemap.collisionMask.assign(static_cast<usize>(w) * h, false);

    const u32 tileCount = static_cast<u32>(gen.tiles.size());
    for (u32 y = 0; y < h; ++y) {
        for (u32 x = 0; x < w; ++x) {
            u32 id = (y < result.grid.size() && x < result.grid[y].size()) ? result.grid[y][x] : 0u;
            if (id >= tileCount) id = 0;
            const auto& td = gen.tiles[id];
            usize cell = static_cast<usize>(y) * w + x;
            tilemap.tiles[cell] = td.tileIndex;
            if (gen.fillCollision) tilemap.collisionMask[cell] = td.solid;
        }
    }
    if (gen.fillCollision) tilemap.hasCollision = true;
    tilemap.meshDirty = true;

    if (result.success)
        ENJIN_LOG_INFO(Build, "WFC: collapsed %ux%u grid (seed %u)", w, h, usedSeed);
    else
        ENJIN_LOG_WARN(Build, "WFC: could not fully collapse after %u attempt(s) — "
                              "rules may be too tight (painted best partial)", retries);
    return result.success;
}

void WFCSystem::GenerateAll(World* world) {
    if (!world) return;
    for (Entity e : world->GetEntitiesWithComponent<WFCComponent>()) {
        auto* gen = world->GetComponent<WFCComponent>(e);
        if (!gen || !gen->generateOnStart) continue;
        auto* tm = world->GetComponent<TilemapComponent>(e);
        if (!tm) tm = &world->AddComponent<TilemapComponent>(e);
        Generate(*gen, *tm);
    }
}

} // namespace ECS
} // namespace Enjin
