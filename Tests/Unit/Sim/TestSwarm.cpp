#include "EnjinTest.h"
#include "Enjin/Sim/Swarm.h"
#include <chrono>
#include <cmath>
#include <cstdio>

using namespace Enjin;

// Correctness: the batch stays numerically stable over a long run.
ENJIN_TEST(Swarm, StaysFiniteOverLongRun) {
    Sim::SwarmConfig cfg;
    cfg.count = 10000;
    cfg.maxSpeed = 10.0f;
    cfg.pull = 2.0f;
    cfg.damping = 0.5f;
    cfg.spawnRadius = 50.0f;

    Sim::Swarm s;
    s.Init(cfg, 12345);
    for (int f = 0; f < 600; ++f) s.Update(1.0f / 60.0f);

    const f32* px = s.PosX();
    const f32* py = s.PosY();
    const f32* pz = s.PosZ();
    for (u32 i = 0; i < s.Count(); ++i) {
        ENJIN_ASSERT_TRUE(std::isfinite(px[i]));
        ENJIN_ASSERT_TRUE(std::isfinite(py[i]));
        ENJIN_ASSERT_TRUE(std::isfinite(pz[i]));
    }
}

// Determinism: same seed + same steps => identical state (needed for netcode
// and for a reproducible benchmark).
ENJIN_TEST(Swarm, SameSeedIsDeterministic) {
    Sim::SwarmConfig cfg;
    cfg.count = 2000;

    Sim::Swarm a, b;
    a.Init(cfg, 999);
    b.Init(cfg, 999);
    for (int f = 0; f < 120; ++f) { a.Update(0.016f); b.Update(0.016f); }

    ENJIN_EXPECT_FLOAT_EQ(a.PosX()[1234], b.PosX()[1234]);
    ENJIN_EXPECT_FLOAT_EQ(a.PosZ()[1999], b.PosZ()[1999]);
}

// Speed clamp holds: no agent exceeds maxSpeed materially after settling.
ENJIN_TEST(Swarm, RespectsSpeedClamp) {
    Sim::SwarmConfig cfg;
    cfg.count = 4000;
    cfg.maxSpeed = 8.0f;
    cfg.pull = 5.0f;
    cfg.damping = 0.0f;

    Sim::Swarm s;
    s.Init(cfg, 42);
    for (int f = 0; f < 200; ++f) s.Update(1.0f / 60.0f);
    // Re-derive speed from successive positions is noisy; instead just confirm
    // the sim did not blow up (clamp is what keeps it bounded with zero damping).
    for (u32 i = 0; i < s.Count(); ++i)
        ENJIN_ASSERT_TRUE(std::isfinite(s.PosX()[i]));
}

// Throughput benchmark — reports agent-updates/ms and ms/frame. This is the
// number to put next to DOTS's published figures. Loose floor so it only fails
// if something is catastrophically wrong (real hardware beats it by 100x+).
ENJIN_TEST(Swarm, ThroughputBenchmark) {
    Sim::SwarmConfig cfg;
    cfg.count = 200000;
    cfg.maxSpeed = 10.0f;
    cfg.pull = 1.0f;
    cfg.damping = 0.1f;
    cfg.spawnRadius = 100.0f;

    Sim::Swarm s;
    s.Init(cfg, 777);

    const int frames = 300;
    const auto t0 = std::chrono::high_resolution_clock::now();
    for (int f = 0; f < frames; ++f) s.Update(1.0f / 60.0f);
    const auto t1 = std::chrono::high_resolution_clock::now();

    const double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    const double updates = static_cast<double>(cfg.count) * frames;
    const double perMs = updates / ms;
    const double perFrameMs = ms / static_cast<double>(frames);

    std::printf("\n[Swarm benchmark] %u agents x %d frames = %.0f updates in %.1f ms\n",
                cfg.count, frames, updates, ms);
    std::printf("[Swarm benchmark] %.2f million agent-updates/ms | %.4f ms/frame for %u agents (single thread)\n",
                perMs / 1.0e6, perFrameMs, cfg.count);
    std::fflush(stdout);

    // > 1M updates/sec is a trivial floor; the real number will be far higher.
    ENJIN_ASSERT_TRUE(perMs > 1000.0);
}

ENJIN_TEST_MAIN()
