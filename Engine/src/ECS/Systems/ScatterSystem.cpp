#include "Enjin/ECS/Systems/ScatterSystem.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Hierarchy.h"
#include "Enjin/Assets/Prefab.h"
#include "Enjin/Logging/Log.h"
#include "Enjin/Math/Vector.h"
#include <random>
#include <cmath>
#include <vector>
#include <utility>
#include <algorithm>

namespace Enjin {
namespace ECS {

// Deterministic xorshift32 so a given seed reproduces the same layout on every
// platform (matches the RandomBag PRNG rather than std::mt19937).
static inline u32 NextRand(u32& state) {
    u32 x = state ? state : 0x9E3779B9u;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    state = x;
    return x;
}
// float in [0,1)
static inline f32 RandF(u32& state) {
    return static_cast<f32>(NextRand(state) >> 8) / static_cast<f32>(1u << 24);
}
// float in [lo,hi)
static inline f32 RandRange(u32& state, f32 lo, f32 hi) {
    return lo + (hi - lo) * RandF(state);
}

// Hard ceiling on placed instances regardless of authored count/region — spawning
// tens of thousands of prefabs is a foot-gun, so cap and log if we hit it.
static constexpr u32 kMaxInstances = 20000u;

// A sampled point in the region's local 2D frame, origin at region centre.
struct Pt2 { f32 a; f32 b; };

// ---- Uniform: independent random points (may clump) ------------------------
static void SampleUniform(u32& rng, f32 w, f32 h, u32 count, std::vector<Pt2>& out) {
    u32 n = count > kMaxInstances ? kMaxInstances : count;
    out.reserve(n);
    for (u32 i = 0; i < n; ++i)
        out.push_back({ RandRange(rng, -w * 0.5f, w * 0.5f), RandRange(rng, -h * 0.5f, h * 0.5f) });
}

// ---- JitteredGrid: one point per grid cell, nudged inside the cell ----------
static void SampleJitteredGrid(u32& rng, f32 w, f32 h, u32 count, std::vector<Pt2>& out) {
    if (count == 0 || w <= 0.0f || h <= 0.0f) return;
    if (count > kMaxInstances) count = kMaxInstances;
    // Choose a column/row split that keeps cells roughly square for the region.
    f32 aspect = (h > 0.0f) ? (w / h) : 1.0f;
    u32 cols = static_cast<u32>(std::round(std::sqrt(static_cast<f32>(count) * aspect)));
    if (cols < 1) cols = 1;
    u32 rows = (count + cols - 1) / cols;
    if (rows < 1) rows = 1;
    f32 cw = w / static_cast<f32>(cols);
    f32 ch = h / static_cast<f32>(rows);
    u32 placed = 0;
    for (u32 r = 0; r < rows && placed < count; ++r) {
        for (u32 c = 0; c < cols && placed < count; ++c) {
            f32 cx = -w * 0.5f + (static_cast<f32>(c) + 0.5f) * cw;
            f32 cy = -h * 0.5f + (static_cast<f32>(r) + 0.5f) * ch;
            out.push_back({ cx + RandRange(rng, -cw * 0.5f, cw * 0.5f),
                            cy + RandRange(rng, -ch * 0.5f, ch * 0.5f) });
            ++placed;
        }
    }
}

// ---- Voronoi (Lloyd relaxation): random points nudged to their cell centroids.
// Each pass assigns a coarse sample grid to its nearest point, then moves every
// point to the centroid of the samples it owns. A few passes spread clumped
// points into an organically even layout — softer than Poisson's hard minimum
// spacing, and it always yields exactly `count` points. Nearest-point search
// uses a bucket grid so a pass is O(samples), not O(samples * count).
static void SampleVoronoi(u32& rng, f32 w, f32 h, u32 count, u32 relaxIterations,
                          std::vector<Pt2>& out) {
    if (count == 0 || w <= 0.0f || h <= 0.0f) return;
    if (count > kMaxInstances) count = kMaxInstances;

    // Start from plain uniform samples (in [0,w]x[0,h] space for simpler math).
    std::vector<Pt2> pts;
    pts.reserve(count);
    for (u32 i = 0; i < count; ++i)
        pts.push_back({ RandRange(rng, 0.0f, w), RandRange(rng, 0.0f, h) });

    // Sample-grid resolution: ~16 samples per point, clamped so a pass stays cheap.
    u32 res = static_cast<u32>(std::ceil(std::sqrt(static_cast<f32>(count) * 16.0f)));
    res = std::clamp(res, 32u, 256u);
    const f32 sx = w / static_cast<f32>(res);
    const f32 sy = h / static_cast<f32>(res);

    // Bucket grid over the points for nearest-point lookup (rebuilt each pass).
    const u32 B = std::clamp(static_cast<u32>(std::sqrt(static_cast<f32>(count))), 1u, 64u);
    std::vector<std::vector<u32>> buckets(static_cast<usize>(B) * B);
    auto bucketOf = [&](f32 x, f32 y) {
        u32 bx = std::min(B - 1, static_cast<u32>(x / w * static_cast<f32>(B)));
        u32 by = std::min(B - 1, static_cast<u32>(y / h * static_cast<f32>(B)));
        return static_cast<usize>(by) * B + bx;
    };

    const u32 iters = std::min(relaxIterations, 16u);
    std::vector<f32> cx(count), cy(count);
    std::vector<u32> cn(count);
    for (u32 it = 0; it < iters; ++it) {
        for (auto& b : buckets) b.clear();
        for (u32 i = 0; i < count; ++i) buckets[bucketOf(pts[i].a, pts[i].b)].push_back(i);
        std::fill(cx.begin(), cx.end(), 0.0f);
        std::fill(cy.begin(), cy.end(), 0.0f);
        std::fill(cn.begin(), cn.end(), 0u);

        for (u32 gy = 0; gy < res; ++gy) {
            for (u32 gx = 0; gx < res; ++gx) {
                f32 px = (static_cast<f32>(gx) + 0.5f) * sx;
                f32 py = (static_cast<f32>(gy) + 0.5f) * sy;
                // Search the sample's bucket ring outward until a point is found,
                // then one extra ring to guarantee the true nearest.
                i32 bx = static_cast<i32>(px / w * static_cast<f32>(B));
                i32 by = static_cast<i32>(py / h * static_cast<f32>(B));
                u32 best = 0; f32 bestD = 3.4e38f; bool found = false;
                for (i32 ring = 0; ring < static_cast<i32>(B); ++ring) {
                    for (i32 dy = -ring; dy <= ring; ++dy) {
                        for (i32 dx = -ring; dx <= ring; ++dx) {
                            if (std::max(std::abs(dx), std::abs(dy)) != ring) continue;
                            i32 nx = bx + dx, ny = by + dy;
                            if (nx < 0 || ny < 0 || nx >= static_cast<i32>(B) || ny >= static_cast<i32>(B)) continue;
                            for (u32 pi : buckets[static_cast<usize>(ny) * B + nx]) {
                                f32 ddx = pts[pi].a - px, ddy = pts[pi].b - py;
                                f32 d = ddx * ddx + ddy * ddy;
                                if (d < bestD) { bestD = d; best = pi; }
                            }
                        }
                    }
                    if (found) break;             // finished the +1 guarantee ring
                    if (bestD < 3.4e38f) found = true;  // found something: do one more ring
                }
                cx[best] += px; cy[best] += py; ++cn[best];
            }
        }
        for (u32 i = 0; i < count; ++i) {
            if (cn[i] == 0) continue;   // point owns no samples this pass; leave it
            pts[i].a = cx[i] / static_cast<f32>(cn[i]);
            pts[i].b = cy[i] / static_cast<f32>(cn[i]);
        }
    }

    out.reserve(count);
    for (const auto& p : pts) out.push_back({ p.a - w * 0.5f, p.b - h * 0.5f });
}

// ---- Poisson-disk (Bridson): blue-noise, every point >= radius apart --------
static void SamplePoisson(u32& rng, f32 w, f32 h, f32 radius, std::vector<Pt2>& out) {
    if (w <= 0.0f || h <= 0.0f) return;
    if (radius < 0.05f) radius = 0.05f;               // guard: avoids a runaway grid
    const f32 cell = radius / 1.41421356f;            // radius / sqrt(2)
    const i32 gw = static_cast<i32>(std::ceil(w / cell));
    const i32 gh = static_cast<i32>(std::ceil(h / cell));
    if (gw <= 0 || gh <= 0) return;
    // Grid stores an index into `pts` (in [0,w]x[0,h] space), -1 = empty.
    std::vector<i32> grid(static_cast<usize>(gw) * gh, -1);
    std::vector<Pt2> pts;      // in [0,w]x[0,h]
    std::vector<i32> active;
    const i32 kTries = 30;

    auto gridIdx = [&](f32 x, f32 y) {
        i32 gx = static_cast<i32>(x / cell);
        i32 gy = static_cast<i32>(y / cell);
        if (gx < 0) gx = 0; if (gx >= gw) gx = gw - 1;
        if (gy < 0) gy = 0; if (gy >= gh) gy = gh - 1;
        return static_cast<usize>(gy) * gw + gx;
    };
    auto fits = [&](f32 x, f32 y) -> bool {
        if (x < 0.0f || y < 0.0f || x >= w || y >= h) return false;
        i32 gx = static_cast<i32>(x / cell);
        i32 gy = static_cast<i32>(y / cell);
        for (i32 dy = -2; dy <= 2; ++dy) {
            for (i32 dx = -2; dx <= 2; ++dx) {
                i32 nx = gx + dx, ny = gy + dy;
                if (nx < 0 || ny < 0 || nx >= gw || ny >= gh) continue;
                i32 idx = grid[static_cast<usize>(ny) * gw + nx];
                if (idx < 0) continue;
                f32 ddx = pts[idx].a - x, ddy = pts[idx].b - y;
                if (ddx * ddx + ddy * ddy < radius * radius) return false;
            }
        }
        return true;
    };
    auto add = [&](f32 x, f32 y) {
        i32 id = static_cast<i32>(pts.size());
        pts.push_back({ x, y });
        grid[gridIdx(x, y)] = id;
        active.push_back(id);
    };

    add(RandRange(rng, 0.0f, w), RandRange(rng, 0.0f, h));
    while (!active.empty() && pts.size() < kMaxInstances) {
        u32 ai = NextRand(rng) % static_cast<u32>(active.size());
        i32 pid = active[ai];
        bool found = false;
        for (i32 t = 0; t < kTries; ++t) {
            f32 ang = RandF(rng) * 6.2831853f;
            f32 rad = radius * (1.0f + RandF(rng));   // annulus [radius, 2*radius]
            f32 nx = pts[pid].a + std::cos(ang) * rad;
            f32 ny = pts[pid].b + std::sin(ang) * rad;
            if (fits(nx, ny)) { add(nx, ny); found = true; break; }
        }
        if (!found) { active[ai] = active.back(); active.pop_back(); }
    }
    // Shift into the centred local frame.
    out.reserve(pts.size());
    for (auto& p : pts) out.push_back({ p.a - w * 0.5f, p.b - h * 0.5f });
}

u32 ScatterSystem::Clear(World* world, Entity entity) {
    if (!world || entity == INVALID_ENTITY) return 0;
    // Copy the child list — DestroyEntity is deferred, but the marked children are
    // identified up front so a same-frame re-generate can't touch fresh instances.
    std::vector<Entity> kids = GetChildren(world, entity); // by value
    u32 removed = 0;
    for (Entity child : kids) {
        if (world->IsValid(child) && world->HasComponent<ScatterInstanceComponent>(child)) {
            world->DestroyEntity(child);
            ++removed;
        }
    }
    return removed;
}

u32 ScatterSystem::Generate(World* world, Entity entity, ScatterComponent& scatter) {
    if (!world || entity == INVALID_ENTITY) return 0;
    // Structural mutation (Instantiate/Add/Destroy below) asserts owner-thread
    // internally per adr-0004, so no explicit guard is needed here.

    Clear(world, entity);

    scatter.lastCount = 0;
    if (scatter.prefabPath.empty()) {
        ENJIN_LOG_WARN(Build, "Scatter: no prefab set — nothing to place");
        return 0;
    }
    auto prefab = Assets::PrefabManager::Get().LoadPrefab(scatter.prefabPath);
    if (!prefab) {
        ENJIN_LOG_WARN(Build, "Scatter: could not load prefab '%s'", scatter.prefabPath.c_str());
        return 0;
    }

    // seed 0 = fresh non-deterministic seed; else reproducible.
    u32 seed = scatter.seed;
    if (seed == 0) {
        std::random_device rd;
        seed = rd();
        if (seed == 0) seed = 1;
    }
    scatter.lastSeed = seed;
    u32 rng = seed;

    f32 w = scatter.regionWidth  < 0.0f ? 0.0f : scatter.regionWidth;
    f32 h = scatter.regionHeight < 0.0f ? 0.0f : scatter.regionHeight;

    std::vector<Pt2> points;
    switch (scatter.distribution) {
        case ScatterComponent::Distribution::Uniform:
            SampleUniform(rng, w, h, scatter.targetCount, points); break;
        case ScatterComponent::Distribution::Poisson:
            SamplePoisson(rng, w, h, scatter.minSpacing, points); break;
        case ScatterComponent::Distribution::JitteredGrid:
            SampleJitteredGrid(rng, w, h, scatter.targetCount, points); break;
        case ScatterComponent::Distribution::Voronoi:
            SampleVoronoi(rng, w, h, scatter.targetCount, scatter.relaxIterations, points); break;
    }

    f32 sMin = scatter.scaleMin, sMax = scatter.scaleMax;
    if (sMax < sMin) std::swap(sMin, sMax);

    // Terrain conform: sample the surface of THIS entity's terrain (same local
    // frame as the scatter offsets). XZ plane only.
    const TerrainComponent* conformTerrain = nullptr;
    if (scatter.conformToTerrain && scatter.plane == ScatterComponent::Plane::XZ) {
        conformTerrain = world->GetComponent<TerrainComponent>(entity);
        if (!conformTerrain)
            ENJIN_LOG_WARN(Build, "Scatter: conformToTerrain is on but this entity has no "
                                  "Terrain — add the Scatter to the terrain entity");
    }

    u32 placed = 0;
    for (const Pt2& p : points) {
        if (placed >= kMaxInstances) break;

        // Map the 2D sample onto the chosen plane, plus optional relief along the
        // plane normal. Offsets are LOCAL — SetParent below makes them relative to
        // the source entity, so the whole batch inherits its transform.
        f32 jitter = (scatter.heightJitter > 0.0f)
                       ? RandRange(rng, -scatter.heightJitter, scatter.heightJitter) : 0.0f;
        Math::Vector3 offset;
        Math::Vector3 rot;   // euler degrees; Instantiate converts + multiplies
        if (scatter.plane == ScatterComponent::Plane::XZ) {
            f32 ground = 0.0f;
            if (conformTerrain) {
                f32 slopeDeg = 0.0f;
                if (!SampleTerrainHeight(*conformTerrain, p.a, p.b, ground, slopeDeg))
                    continue;   // off the terrain: cull
                if (slopeDeg > scatter.maxSlopeDeg)
                    continue;   // too steep: cull (keeps trees off cliffs)
            }
            offset = Math::Vector3(p.a, ground + jitter, p.b);
            if (scatter.randomYaw) rot.y = RandRange(rng, 0.0f, 360.0f);
        } else { // XY
            offset = Math::Vector3(p.a, p.b, jitter);
            if (scatter.randomYaw) rot.z = RandRange(rng, 0.0f, 360.0f);
        }

        f32 s = (sMax > sMin) ? RandRange(rng, sMin, sMax) : sMin;
        Math::Vector3 scl(s, s, s);

        Entity inst = Assets::PrefabManager::Get().Instantiate(world, *prefab, offset, rot, scl);
        if (inst == INVALID_ENTITY) continue;
        world->AddComponent<ScatterInstanceComponent>(inst);
        SetParent(world, inst, entity);
        ++placed;
    }

    scatter.lastCount = placed;
    ENJIN_LOG_INFO(Build, "Scatter: placed %u instances of '%s' (seed %u)",
                   placed, scatter.prefabPath.c_str(), seed);
    return placed;
}

bool ScatterSystem::SampleTerrainHeight(const TerrainComponent& terrain, f32 x, f32 z,
                                        f32& outHeight, f32& outSlopeDeg) {
    const u32 w = terrain.gridWidth, h = terrain.gridHeight;
    if (w < 2 || h < 2 || terrain.heightmap.size() < static_cast<usize>(w) * h) return false;
    const f32 cell = terrain.cellSize > 1e-4f ? terrain.cellSize : 1.0f;
    // CreateTerrain centres the mesh: grid (gx,gz) sits at (gx*cell - halfW, gz*cell - halfH).
    const f32 halfW = (w - 1) * cell * 0.5f;
    const f32 halfH = (h - 1) * cell * 0.5f;
    const f32 fx = (x + halfW) / cell;
    const f32 fz = (z + halfH) / cell;
    if (fx < 0.0f || fz < 0.0f || fx > static_cast<f32>(w - 1) || fz > static_cast<f32>(h - 1))
        return false;   // outside the terrain's extent

    const u32 x0 = std::min(static_cast<u32>(fx), w - 2);
    const u32 z0 = std::min(static_cast<u32>(fz), h - 2);
    const f32 tx = fx - static_cast<f32>(x0);
    const f32 tz = fz - static_cast<f32>(z0);
    auto H = [&](u32 gx, u32 gz) { return terrain.heightmap[static_cast<usize>(gz) * w + gx]; };
    const f32 h00 = H(x0, z0),     h10 = H(x0 + 1, z0);
    const f32 h01 = H(x0, z0 + 1), h11 = H(x0 + 1, z0 + 1);
    outHeight = h00 * (1 - tx) * (1 - tz) + h10 * tx * (1 - tz)
              + h01 * (1 - tx) * tz       + h11 * tx * tz;

    // Slope from the cell's edge gradients (world units per unit distance).
    const f32 dx = ((h10 - h00) * (1 - tz) + (h11 - h01) * tz) / cell;
    const f32 dz = ((h01 - h00) * (1 - tx) + (h11 - h10) * tx) / cell;
    outSlopeDeg = std::atan(std::sqrt(dx * dx + dz * dz)) * 57.2957795f;
    return true;
}

void ScatterSystem::GenerateAll(World* world) {
    if (!world) return;
    // Copy the entity list — Generate creates/destroys entities, which must not
    // happen while iterating the live component set.
    std::vector<Entity> owners;
    for (Entity e : world->GetEntitiesWithComponent<ScatterComponent>()) owners.push_back(e);
    for (Entity e : owners) {
        auto* sc = world->GetComponent<ScatterComponent>(e);
        if (sc && sc->generateOnStart) Generate(world, e, *sc);
    }
}

} // namespace ECS
} // namespace Enjin
