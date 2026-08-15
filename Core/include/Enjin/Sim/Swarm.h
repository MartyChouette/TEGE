#pragma once

#include "Enjin/Platform/Platform.h"   // ENJIN_FORCE_INLINE (Math.h relies on it)
#include "Enjin/Platform/Types.h"
#include "Enjin/Math/Vector.h"
#include <vector>
#include <cmath>

// =============================================================================
// Swarm — Tier-1 data-oriented batch agent type (prototype).
//
// Stores N lightweight agents as struct-of-arrays and updates them all in a
// single tight, branch-light loop that the compiler auto-vectorizes (SSE/AVX
// on desktop, WASM SIMD128 on web with -msimd128). Positions are exposed as
// contiguous arrays so the whole swarm renders with ONE instanced draw.
//
// This is the "easy fast type" that lives ALONGSIDE the ECS, not a replacement.
// The ECS stays archetype-less/sparse-set; drop a Swarm on the one thing that
// needs DOTS-scale (crowds, bullets, boids, particle-agents) and leave the
// rest normal. Pure Core C++, compiles identically for native and WASM.
// =============================================================================

namespace Enjin {
namespace Sim {

enum class SwarmBehavior : u8 {
    Orbit = 0,   // pulled toward a center, speed-clamped, damped — O(N)
};

struct SwarmConfig {
    u32 count = 10000;
    SwarmBehavior behavior = SwarmBehavior::Orbit;
    Math::Vector3 center = Math::Vector3(0.0f, 0.0f, 0.0f);
    f32 spawnRadius = 50.0f;   // initial scatter around center
    f32 maxSpeed = 10.0f;      // per-agent speed clamp
    f32 pull = 1.0f;           // acceleration toward center
    f32 damping = 0.05f;       // fraction of velocity shed per second
};

class Swarm {
public:
    void Init(const SwarmConfig& cfg, u64 seed) {
        m_Cfg = cfg;
        const u32 n = cfg.count;
        m_PX.resize(n); m_PY.resize(n); m_PZ.resize(n);
        m_VX.resize(n); m_VY.resize(n); m_VZ.resize(n);

        // Deterministic xorshift64 so runs are reproducible (no wall-clock RNG).
        u64 s = seed ? seed : 0x9E3779B97F4A7C15ull;
        auto rnd = [&s]() -> f32 {
            s ^= s << 13; s ^= s >> 7; s ^= s << 17;
            return static_cast<f32>((s >> 40) & 0xFFFFFFull) / static_cast<f32>(0xFFFFFFull);
        };
        for (u32 i = 0; i < n; ++i) {
            m_PX[i] = cfg.center.x + (rnd() * 2.0f - 1.0f) * cfg.spawnRadius;
            m_PY[i] = cfg.center.y + (rnd() * 2.0f - 1.0f) * cfg.spawnRadius;
            m_PZ[i] = cfg.center.z + (rnd() * 2.0f - 1.0f) * cfg.spawnRadius;
            m_VX[i] = (rnd() * 2.0f - 1.0f) * cfg.maxSpeed;
            m_VY[i] = (rnd() * 2.0f - 1.0f) * cfg.maxSpeed;
            m_VZ[i] = (rnd() * 2.0f - 1.0f) * cfg.maxSpeed;
        }
    }

    // Advance the whole swarm one step. Single-threaded and vectorizable; the
    // fork-join worker pool can later split [0,n) across cores for free.
    void Update(f32 dt) {
        const u32 n = m_Cfg.count;
        const f32 cx = m_Cfg.center.x, cy = m_Cfg.center.y, cz = m_Cfg.center.z;
        const f32 pull = m_Cfg.pull;
        const f32 damp = 1.0f - m_Cfg.damping * dt;
        const f32 maxSp = m_Cfg.maxSpeed;
        const f32 maxSp2 = maxSp * maxSp;

        f32* __restrict px = m_PX.data(); f32* __restrict py = m_PY.data(); f32* __restrict pz = m_PZ.data();
        f32* __restrict vx = m_VX.data(); f32* __restrict vy = m_VY.data(); f32* __restrict vz = m_VZ.data();

        for (u32 i = 0; i < n; ++i) {
            const f32 ax = (cx - px[i]) * pull;
            const f32 ay = (cy - py[i]) * pull;
            const f32 az = (cz - pz[i]) * pull;

            f32 nvx = (vx[i] + ax * dt) * damp;
            f32 nvy = (vy[i] + ay * dt) * damp;
            f32 nvz = (vz[i] + az * dt) * damp;

            const f32 sp2 = nvx * nvx + nvy * nvy + nvz * nvz;
            const f32 scale = sp2 > maxSp2 ? (maxSp / std::sqrt(sp2)) : 1.0f;
            nvx *= scale; nvy *= scale; nvz *= scale;

            vx[i] = nvx; vy[i] = nvy; vz[i] = nvz;
            px[i] += nvx * dt; py[i] += nvy * dt; pz[i] += nvz * dt;
        }
    }

    u32 Count() const { return m_Cfg.count; }

    // Contiguous position arrays for a single instanced draw (or GPU upload).
    const f32* PosX() const { return m_PX.data(); }
    const f32* PosY() const { return m_PY.data(); }
    const f32* PosZ() const { return m_PZ.data(); }

private:
    SwarmConfig m_Cfg;
    std::vector<f32> m_PX, m_PY, m_PZ;
    std::vector<f32> m_VX, m_VY, m_VZ;
};

} // namespace Sim
} // namespace Enjin
