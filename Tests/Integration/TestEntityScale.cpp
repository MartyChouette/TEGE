// T1 — Entity scaling stress test (MASTER_VALIDATION.md §3).
// The 90%-likelihood month-one limit: how far do entity counts go before the
// engine degrades, and does it ever crash? Four scenarios, escalating tiers.
//
// PASS CRITERIA (enforced, deliberately loose to stay CI-stable):
//   - No crash at any tier, including 500K transform-only entities.
//   - Per-entity query cost stays roughly flat as counts grow (no superlinear
//     blowup: the 500K per-entity cost must be < 8x the 10K per-entity cost).
//   - Physics settle at 5K dynamic bodies steps in < 250 ms/step on CI-class
//     hardware (Jolt headless; generous bound — this catches order-of-
//     magnitude regressions, not tuning drift).
// Timings are printed for the record either way; docs quote them so users can
// plan scene budgets.

#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Name.h"
#include "Enjin/ECS/Components/Hierarchy.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/Physics/IPhysicsBackend.h"
#include "Enjin/Physics/PhysicsBackendFactory.h"

#include <chrono>
#include <cstdio>
#include <random>
#include <vector>

using namespace Enjin;
using Clock = std::chrono::high_resolution_clock;

static f64 Ms(Clock::time_point t0) {
    return std::chrono::duration<f64, std::milli>(Clock::now() - t0).count();
}

static int s_Failures = 0;
#define T1_CHECK(cond, ...) do { if (!(cond)) { ++s_Failures; \
    std::printf("  FAIL: " __VA_ARGS__); std::printf("\n"); } } while (0)

// ── Scenario A: transform-only entities, 10K → 500K ─────────────────────────
// Measures creation, full iteration (the render-culling access pattern), and
// random GetComponent lookups. The scaling law check lives here.
static void ScenarioTransformOnly() {
    std::printf("\n[A] Transform-only scaling\n");
    std::printf("%10s %14s %16s %18s\n", "entities", "create ms", "iterate ms", "ns/entity lookup");

    f64 perEntity10k = 0.0, perEntity500k = 0.0;
    for (u32 count : {10'000u, 50'000u, 100'000u, 500'000u}) {
        ECS::World world;
        std::mt19937 rng(1234);
        std::uniform_real_distribution<f32> pos(-1000.0f, 1000.0f);

        auto t0 = Clock::now();
        std::vector<ECS::Entity> ents;
        ents.reserve(count);
        for (u32 i = 0; i < count; ++i) {
            ECS::Entity e = world.CreateEntity();
            auto& t = world.AddComponent<ECS::TransformComponent>(e);
            t.position = Math::Vector3(pos(rng), pos(rng), pos(rng));
            ents.push_back(e);
        }
        f64 createMs = Ms(t0);

        // Full iteration, 5 passes (steady-state access pattern)
        t0 = Clock::now();
        f32 sum = 0.0f;
        for (int pass = 0; pass < 5; ++pass) {
            for (ECS::Entity e : world.GetEntitiesWithComponent<ECS::TransformComponent>()) {
                sum += world.GetComponent<ECS::TransformComponent>(e)->position.x;
            }
        }
        f64 iterMs = Ms(t0) / 5.0;

        // Random lookups (script/gameplay access pattern)
        std::uniform_int_distribution<u32> pick(0, count - 1);
        constexpr u32 kLookups = 200'000;
        t0 = Clock::now();
        for (u32 i = 0; i < kLookups; ++i) {
            sum += world.GetComponent<ECS::TransformComponent>(ents[pick(rng)])->position.y;
        }
        f64 lookupNs = Ms(t0) * 1e6 / kLookups;

        std::printf("%10u %14.1f %16.2f %18.1f   (checksum %.1f)\n",
                    count, createMs, iterMs, lookupNs, sum);
        if (count == 10'000u) perEntity10k = iterMs / count;
        if (count == 500'000u) perEntity500k = iterMs / count;
    }

    // Scaling law: per-entity iteration cost must not blow up superlinearly.
    T1_CHECK(perEntity500k < perEntity10k * 8.0,
             "iteration cost scaled superlinearly: %.1f ns/entity at 10K vs %.1f at 500K",
             perEntity10k * 1e6, perEntity500k * 1e6);
}

// ── Scenario B: colliders attached, to 100K ─────────────────────────────────
// Component-heavy entities (3 components each); catches storage blowups.
static void ScenarioColliders() {
    std::printf("\n[B] Transform+collider scaling\n");
    for (u32 count : {10'000u, 50'000u, 100'000u}) {
        ECS::World world;
        std::mt19937 rng(99);
        std::uniform_real_distribution<f32> pos(-500.0f, 500.0f);
        auto t0 = Clock::now();
        for (u32 i = 0; i < count; ++i) {
            ECS::Entity e = world.CreateEntity();
            auto& t = world.AddComponent<ECS::TransformComponent>(e);
            t.position = Math::Vector3(pos(rng), pos(rng), pos(rng));
            auto& box = world.AddComponent<ECS::BoxColliderComponent>(e);
            box.size = Math::Vector3(1, 1, 1);
        }
        std::printf("%10u entities+colliders created in %.1f ms\n", count, Ms(t0));
    }
}

// ── Scenario C: dynamic rigidbodies, headless Jolt settle ───────────────────
// A pile of falling boxes over a static floor, stepped at the fixed tick.
static void ScenarioPhysicsSettle() {
    std::printf("\n[C] Physics settle (headless Jolt)\n");
    for (u32 count : {1'000u, 5'000u}) {
        ECS::World world;
        // Static floor
        {
            ECS::Entity floor = world.CreateEntity();
            auto& t = world.AddComponent<ECS::TransformComponent>(floor);
            t.position = Math::Vector3(0, -1, 0);
            auto& box = world.AddComponent<ECS::BoxColliderComponent>(floor);
            box.size = Math::Vector3(400, 2, 400);
            auto& rb = world.AddComponent<ECS::RigidbodyComponent>(floor);
            rb.bodyType = ECS::RigidbodyComponent::BodyType::Static;
            rb.useGravity = false;
        }
        // Falling boxes in a loose grid above
        std::mt19937 rng(7);
        std::uniform_real_distribution<f32> jitter(-0.2f, 0.2f);
        u32 side = static_cast<u32>(std::sqrt(static_cast<f32>(count))) + 1;
        u32 spawned = 0;
        for (u32 z = 0; z < side && spawned < count; ++z) {
            for (u32 x = 0; x < side && spawned < count; ++x, ++spawned) {
                ECS::Entity e = world.CreateEntity();
                auto& t = world.AddComponent<ECS::TransformComponent>(e);
                t.position = Math::Vector3(
                    (static_cast<f32>(x) - side * 0.5f) * 1.5f + jitter(rng),
                    3.0f + static_cast<f32>((x + z) % 7) * 1.2f,
                    (static_cast<f32>(z) - side * 0.5f) * 1.5f + jitter(rng));
                auto& box = world.AddComponent<ECS::BoxColliderComponent>(e);
                box.size = Math::Vector3(1, 1, 1);
                auto& rb = world.AddComponent<ECS::RigidbodyComponent>(e);
                rb.useGravity = true;
                rb.mass = 1.0f;
            }
        }

        auto backend = Physics::CreatePhysicsBackend(Physics::PhysicsBackendType::Auto);
        if (!backend) { std::printf("  no physics backend — skipping\n"); return; }
        backend->SetWorld(&world);

        constexpr int kSteps = 120;  // 2 simulated seconds at 60Hz
        f64 worstMs = 0.0, totalMs = 0.0;
        for (int i = 0; i < kSteps; ++i) {
            auto t0 = Clock::now();
            backend->Update(1.0f / 60.0f);
            f64 ms = Ms(t0);
            totalMs += ms;
            if (ms > worstMs) worstMs = ms;
        }
        std::printf("%10u bodies: avg %.2f ms/step, worst %.2f ms over %d steps\n",
                    count, totalMs / kSteps, worstMs, kSteps);
        if (count == 5'000u) {
            T1_CHECK(worstMs < 250.0,
                     "5K-body settle step took %.1f ms (order-of-magnitude regression)", worstMs);
        }
    }
}

// ── Scenario D: deep hierarchy ──────────────────────────────────────────────
// 20-deep parent chains x 2000 roots — transform-propagation shape.
static void ScenarioHierarchy() {
    std::printf("\n[D] Deep hierarchy (20 levels x 2000 chains = 40K entities)\n");
    ECS::World world;
    auto t0 = Clock::now();
    for (u32 c = 0; c < 2000; ++c) {
        ECS::Entity parent = ECS::INVALID_ENTITY;
        for (u32 d = 0; d < 20; ++d) {
            ECS::Entity e = world.CreateEntity();
            auto& t = world.AddComponent<ECS::TransformComponent>(e);
            t.position = Math::Vector3(0.0f, 1.0f, 0.0f);
            if (parent != ECS::INVALID_ENTITY) {
                auto& pc = world.AddComponent<ECS::ParentComponent>(e);
                pc.parent = parent;
            }
            parent = e;
        }
    }
    std::printf("  built in %.1f ms\n", Ms(t0));

    // Walk every chain leaf->root 5 times (world-matrix composition pattern)
    t0 = Clock::now();
    u64 walked = 0;
    for (int pass = 0; pass < 5; ++pass) {
        for (ECS::Entity e : world.GetEntitiesWithComponent<ECS::ParentComponent>()) {
            ECS::Entity cur = e;
            int guard = 0;
            while (cur != ECS::INVALID_ENTITY && guard++ < 64) {
                auto* pc = world.GetComponent<ECS::ParentComponent>(cur);
                cur = pc ? pc->parent : ECS::INVALID_ENTITY;
                ++walked;
            }
        }
    }
    std::printf("  chain walk: %.2f ms/pass (%llu hops)\n", Ms(t0) / 5.0,
                static_cast<unsigned long long>(walked));
}

int main() {
    std::printf("=== T1: Entity scaling stress (MASTER_VALIDATION §3) ===\n");
    auto t0 = Clock::now();
    ScenarioTransformOnly();
    ScenarioColliders();
    ScenarioPhysicsSettle();
    ScenarioHierarchy();
    std::printf("\nTotal: %.1f s — %s\n", Ms(t0) / 1000.0,
                s_Failures == 0 ? "PASS" : "FAIL");
    return s_Failures == 0 ? 0 : 1;
}
