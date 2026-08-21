#include "Enjin/ECS/Systems/WFCSystem.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Hierarchy.h"
#include "Enjin/ECS/Components/Scatter.h"        // reuse ScatterInstanceComponent as the proc-gen instance marker
#include "Enjin/ECS/Systems/ScatterSystem.h"     // reuse ScatterSystem::Clear for reload-stable cleanup
#include "Enjin/Assets/Prefab.h"
#include "Enjin/Procedural/ProceduralAlgorithms.h"
#include "Enjin/Logging/Log.h"
#include "Enjin/Math/Vector.h"
#include <random>
#include <algorithm>
#include <vector>
#include <array>

namespace Enjin {
namespace ECS {

// ---------------------------------------------------------------------------
// Tiles2D: compile edge sockets -> the 2D solver's 4-direction adjacency and run
// WaveFunctionCollapse. Socket edge order is 0=N,1=E,2=S,3=W; the solver's
// allowedNeighbors index is 0=N,1=S,2=E,3=W (see WaveFunctionCollapse::Generate).
// ---------------------------------------------------------------------------
static void BuildTiles2D(const WFCComponent& gen,
                         std::vector<Procedural::WaveFunctionCollapse::WFCTile>& out) {
    const u32 n = static_cast<u32>(gen.tiles.size());
    out.clear();
    out.reserve(n);
    for (u32 i = 0; i < n; ++i) {
        Procedural::WaveFunctionCollapse::WFCTile t;
        t.id = i;
        const auto& ei = gen.tiles[i].edges;
        for (u32 j = 0; j < n; ++j) {
            const auto& ej = gen.tiles[j].edges;
            if (ei[0] == ej[2]) t.allowedNeighbors[0].push_back(j); // N: my N == their S
            if (ei[2] == ej[0]) t.allowedNeighbors[1].push_back(j); // S: my S == their N
            if (ei[1] == ej[3]) t.allowedNeighbors[2].push_back(j); // E: my E == their W
            if (ei[3] == ej[1]) t.allowedNeighbors[3].push_back(j); // W: my W == their E
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

    u32 baseSeed = gen.seed;
    if (baseSeed == 0) { std::random_device rd; baseSeed = rd(); if (baseSeed == 0) baseSeed = 1; }

    Procedural::WaveFunctionCollapse::Params p;
    p.width = w; p.height = h;
    p.maxBacktracks = gen.maxBacktracks;
    BuildTiles2D(gen, p.tiles);

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

// ---------------------------------------------------------------------------
// Modules3D: a compact 6-direction WFC over a W×H×D volume. Uses observe +
// propagate with restart-on-contradiction (the caller's retry loop supplies fresh
// seeds), which is simpler than the 2D solver's internal backtracking and good
// enough for module sets. Fills `outGrid` (flat, idx = (z*H + y)*W + x) with tile
// indices and returns whether every cell collapsed with no contradiction.
// ---------------------------------------------------------------------------
static inline u32 WfcNextRand(u32& s) {
    u32 x = s ? s : 0x9E3779B9u; x ^= x << 13; x ^= x >> 17; x ^= x << 5; s = x; return x;
}

// allowedNeighbors index / offset table. 0=N(+Y),1=S(-Y),2=E(+X),3=W(-X),4=Up(+Z),5=Down(-Z).
static const i32 kDx[6] = { 0, 0,  1, -1, 0,  0 };
static const i32 kDy[6] = { 1,-1,  0,  0, 0,  0 };
static const i32 kDz[6] = { 0, 0,  0,  0, 1, -1 };

static bool WFC3DSolve(const WFCComponent& gen, u32 w, u32 h, u32 d, u32 seed,
                       std::vector<u32>& outGrid) {
    const u32 n = static_cast<u32>(gen.tiles.size());
    const usize total = static_cast<usize>(w) * h * d;

    // Compile 6-direction adjacency from edge sockets (0=N,1=E,2=S,3=W,4=Up,5=Down).
    std::vector<std::array<std::vector<u32>, 6>> adj(n);
    for (u32 i = 0; i < n; ++i) {
        const auto& ei = gen.tiles[i].edges;
        for (u32 j = 0; j < n; ++j) {
            const auto& ej = gen.tiles[j].edges;
            if (ei[0] == ej[2]) adj[i][0].push_back(j); // N:  my N == their S
            if (ei[2] == ej[0]) adj[i][1].push_back(j); // S:  my S == their N
            if (ei[1] == ej[3]) adj[i][2].push_back(j); // E:  my E == their W
            if (ei[3] == ej[1]) adj[i][3].push_back(j); // W:  my W == their E
            if (ei[4] == ej[5]) adj[i][4].push_back(j); // Up: my Up == their Down
            if (ei[5] == ej[4]) adj[i][5].push_back(j); // Down: my Down == their Up
        }
    }

    std::vector<std::vector<bool>> possible(total, std::vector<bool>(n, true));
    std::vector<char> collapsed(total, 0);
    std::vector<u32> chosen(total, 0);
    u32 rng = seed ? seed : 1u;

    auto idxOf = [&](u32 x, u32 y, u32 z) { return (static_cast<usize>(z) * h + y) * w + x; };
    auto entropy = [&](usize c) { u32 k = 0; for (bool b : possible[c]) if (b) ++k; return k; };

    // Propagate constraints outward from a just-changed cell. Returns false on contradiction.
    auto propagate = [&](usize start) -> bool {
        std::vector<usize> stack{ start };
        while (!stack.empty()) {
            usize c = stack.back(); stack.pop_back();
            u32 cx = static_cast<u32>(c % w);
            u32 cy = static_cast<u32>((c / w) % h);
            u32 cz = static_cast<u32>(c / (static_cast<usize>(w) * h));
            for (u32 dir = 0; dir < 6; ++dir) {
                i32 nx = static_cast<i32>(cx) + kDx[dir];
                i32 ny = static_cast<i32>(cy) + kDy[dir];
                i32 nz = static_cast<i32>(cz) + kDz[dir];
                if (nx < 0 || ny < 0 || nz < 0 ||
                    nx >= static_cast<i32>(w) || ny >= static_cast<i32>(h) || nz >= static_cast<i32>(d))
                    continue;
                usize nc = idxOf(static_cast<u32>(nx), static_cast<u32>(ny), static_cast<u32>(nz));
                if (collapsed[nc]) continue;
                bool changed = false;
                for (u32 nt = 0; nt < n; ++nt) {
                    if (!possible[nc][nt]) continue;
                    // nt stays possible only if some possible tile in c allows it in `dir`.
                    bool ok = false;
                    for (u32 ct = 0; ct < n && !ok; ++ct) {
                        if (!possible[c][ct]) continue;
                        for (u32 a : adj[ct][dir]) if (a == nt) { ok = true; break; }
                    }
                    if (!ok) { possible[nc][nt] = false; changed = true; }
                }
                if (changed) {
                    if (entropy(nc) == 0) return false;   // contradiction
                    stack.push_back(nc);
                }
            }
        }
        return true;
    };

    u32 done = 0;
    while (done < total) {
        // Observe: lowest-entropy uncollapsed cell.
        usize best = total; u32 bestE = 0xFFFFFFFFu;
        for (usize c = 0; c < total; ++c) {
            if (collapsed[c]) continue;
            u32 e = entropy(c);
            if (e == 0) return false;
            if (e < bestE) { bestE = e; best = c; }
        }
        if (best == total) break;

        std::vector<u32> opts;
        for (u32 t = 0; t < n; ++t) if (possible[best][t]) opts.push_back(t);
        u32 pick = opts[WfcNextRand(rng) % static_cast<u32>(opts.size())];
        for (u32 t = 0; t < n; ++t) possible[best][t] = (t == pick);
        collapsed[best] = 1; chosen[best] = pick; ++done;

        if (!propagate(best)) return false;
    }

    outGrid.assign(total, 0);
    for (usize c = 0; c < total; ++c) outGrid[c] = chosen[c];
    return true;
}

static bool GenerateModules3D(World* world, Entity entity, WFCComponent& gen) {
    ScatterSystem::Clear(world, entity);   // drop the previous module batch (marked children)
    gen.lastCount = 0;

    if (gen.tiles.empty()) { ENJIN_LOG_WARN(Build, "WFC 3D: no tiles authored"); gen.lastSuccess = false; return false; }

    u32 w = std::clamp(gen.width,  1u, 64u);
    u32 h = std::clamp(gen.height, 1u, 64u);
    u32 d = std::clamp(gen.depth,  1u, 32u);

    u32 baseSeed = gen.seed;
    if (baseSeed == 0) { std::random_device rd; baseSeed = rd(); if (baseSeed == 0) baseSeed = 1; }

    const u32 retries = gen.retryOnFail ? std::max(1u, gen.maxRetries) : 1u;
    std::vector<u32> grid;
    bool ok = false;
    u32 usedSeed = baseSeed;
    for (u32 attempt = 0; attempt < retries; ++attempt) {
        usedSeed = baseSeed + attempt;
        if (WFC3DSolve(gen, w, h, d, usedSeed, grid)) { ok = true; break; }
    }
    gen.lastSeed = usedSeed;
    gen.lastSuccess = ok;

    if (!ok || grid.size() != static_cast<usize>(w) * h * d) {
        ENJIN_LOG_WARN(Build, "WFC 3D: could not collapse %ux%ux%u after %u attempt(s) — rules too tight",
                       w, h, d, retries);
        return false;
    }

    // Instantiate one prefab per solved cell (empty prefab = air). Centre X/Z, Y up from 0.
    // Offsets are LOCAL (parented to the entity), matching Scatter's placement model.
    const f32 halfX = (w - 1) * gen.cellSize * 0.5f;
    const f32 halfZ = (h - 1) * gen.cellSize * 0.5f;
    const u32 tileCount = static_cast<u32>(gen.tiles.size());
    u32 placed = 0;
    static constexpr u32 kMax = 20000u;
    for (u32 z = 0; z < d && placed < kMax; ++z) {
        for (u32 y = 0; y < h && placed < kMax; ++y) {
            for (u32 x = 0; x < w && placed < kMax; ++x) {
                u32 id = grid[(static_cast<usize>(z) * h + y) * w + x];
                if (id >= tileCount) continue;
                const auto& td = gen.tiles[id];
                if (td.prefabPath.empty()) continue;  // air
                auto prefab = Assets::PrefabManager::Get().LoadPrefab(td.prefabPath);
                if (!prefab) continue;
                Math::Vector3 offset(
                    static_cast<f32>(x) * gen.cellSize - halfX,
                    static_cast<f32>(z) * gen.cellSize,          // grid Z -> world up (Y)
                    static_cast<f32>(y) * gen.cellSize - halfZ);
                Entity inst = Assets::PrefabManager::Get().Instantiate(world, *prefab, offset);
                if (inst == INVALID_ENTITY) continue;
                world->AddComponent<ScatterInstanceComponent>(inst);
                SetParent(world, inst, entity);
                ++placed;
            }
        }
    }
    gen.lastCount = placed;
    ENJIN_LOG_INFO(Build, "WFC 3D: collapsed %ux%ux%u, placed %u modules (seed %u)", w, h, d, placed, usedSeed);
    return true;
}

bool WFCSystem::Generate(World* world, Entity entity, WFCComponent& gen) {
    if (!world || entity == INVALID_ENTITY) return false;
    if (gen.mode == WFCComponent::Mode::Modules3D)
        return GenerateModules3D(world, entity, gen);

    // Tiles2D: paint the entity's tilemap.
    auto* tm = world->GetComponent<TilemapComponent>(entity);
    if (!tm) tm = &world->AddComponent<TilemapComponent>(entity);
    return Generate(gen, *tm);
}

void WFCSystem::GenerateAll(World* world) {
    if (!world) return;
    // Copy owners — 3D generate creates/destroys entities, which must not happen while
    // iterating the live component set.
    std::vector<Entity> owners;
    for (Entity e : world->GetEntitiesWithComponent<WFCComponent>()) owners.push_back(e);
    for (Entity e : owners) {
        auto* gen = world->GetComponent<WFCComponent>(e);
        if (gen && gen->generateOnStart) Generate(world, e, *gen);
    }
}

} // namespace ECS
} // namespace Enjin
