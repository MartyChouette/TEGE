#include "EnjinTest.h"
#include "Enjin/Effects/ParticleSystem.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Hierarchy.h"
#include "Enjin/ECS/Components/Gameplay.h"

using namespace Enjin;
using namespace Enjin::Effects;
using namespace Enjin::Math;

// ===========================================================================
// Emitter transform: particle sizes and emission are in WORLD units, and used
// to ignore everything about the emitter entity except its LOCAL position.
// Scaling it did nothing, rotating it did nothing, and a parented emitter
// spawned at its offset from the parent instead of where it sits.
// ===========================================================================

namespace {

// A deterministic emitter: a zero-angle cone (direction is exactly +Y, no
// randomness), no size falloff, no gravity, no variance, no spawn radius.
// Anything that lands elsewhere landed there because of the transform.
ECS::Entity MakeEmitter(ECS::World& world) {
    ECS::Entity e = world.CreateEntity();
    world.AddComponent<ECS::TransformComponent>(e, ECS::TransformComponent{});

    ECS::ParticleEmitterComponent em;
    em.shape = ECS::ParticleEmitterComponent::EmitterShape::Cone;
    em.coneAngle = 0.0f;
    em.shapeRadius = 0.0f;
    em.emissionRate = 100.0f;
    em.startSpeed = 2.0f;
    em.speedVariance = 0.0f;
    em.lifetime = 10.0f;
    em.lifetimeVariance = 0.0f;
    em.startSize = 0.5f;
    em.endSize = 0.5f;          // flat curve: size stays the spawn size
    em.gravity = Vector3(0.0f, 0.0f, 0.0f);
    em.drag = 0.0f;
    em.playOnAwake = true;
    world.AddComponent<ECS::ParticleEmitterComponent>(e, em);
    return e;
}

const ECS::Particle& FirstParticle(ECS::World& world, ECS::Entity e) {
    return world.GetComponent<ECS::ParticleEmitterComponent>(e)->pool.particles[0];
}

} // namespace

ENJIN_TEST(ParticleEmitterTransform, SpawnsAtAll) {
    ECS::World world;
    ECS::Entity e = MakeEmitter(world);

    ParticleSystem ps;
    ps.Update(0.05f, &world);

    auto* em = world.GetComponent<ECS::ParticleEmitterComponent>(e);
    ENJIN_ASSERT_TRUE(em->pool.activeCount > 0);
}

// Size is a world-space width, so the emitter's scale has to reach it. This
// was the whole bug: a 10x emitter drew the same particles as a 1x one.
ENJIN_TEST(ParticleEmitterTransform, ScaleMultipliesParticleSize) {
    ECS::World world;
    ECS::Entity e = MakeEmitter(world);

    ParticleSystem ps;
    ps.Update(0.05f, &world);
    ENJIN_EXPECT_FLOAT_NEAR(FirstParticle(world, e).size, 0.5f, 0.001f);

    ECS::World scaled;
    ECS::Entity se = MakeEmitter(scaled);
    scaled.GetComponent<ECS::TransformComponent>(se)->scale = Vector3(2.0f, 2.0f, 2.0f);

    ParticleSystem ps2;
    ps2.Update(0.05f, &scaled);
    ENJIN_EXPECT_FLOAT_NEAR(FirstParticle(scaled, se).size, 1.0f, 0.001f);
}

// The size-over-life curve is evaluated every frame, so it needs the scale
// too. Applying it only at spawn would size a particle correctly and then
// snap it back on the next frame.
ENJIN_TEST(ParticleEmitterTransform, ScaleSurvivesTheSizeCurve) {
    ECS::World world;
    ECS::Entity e = MakeEmitter(world);
    world.GetComponent<ECS::TransformComponent>(e)->scale = Vector3(3.0f, 3.0f, 3.0f);

    ParticleSystem ps;
    ps.Update(0.05f, &world);
    ps.Update(0.05f, &world);
    ps.Update(0.05f, &world);

    ENJIN_EXPECT_FLOAT_NEAR(FirstParticle(world, e).size, 1.5f, 0.001f);
}

// Rotating the entity aims the emitter. +Y turned 90 degrees about Z is -X.
ENJIN_TEST(ParticleEmitterTransform, RotationAimsTheCone) {
    ECS::World world;
    ECS::Entity e = MakeEmitter(world);

    ParticleSystem ps;
    ps.Update(0.05f, &world);
    const ECS::Particle& up = FirstParticle(world, e);
    ENJIN_EXPECT_FLOAT_NEAR(up.velocity.x, 0.0f, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(up.velocity.y, 2.0f, 0.001f);

    ECS::World turned;
    ECS::Entity te = MakeEmitter(turned);
    turned.GetComponent<ECS::TransformComponent>(te)->rotation =
        Quaternion::FromEulerDegrees(Vector3(0.0f, 0.0f, 90.0f));

    ParticleSystem ps2;
    ps2.Update(0.05f, &turned);
    const ECS::Particle& sideways = FirstParticle(turned, te);
    ENJIN_EXPECT_FLOAT_NEAR(sideways.velocity.x, -2.0f, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(sideways.velocity.y, 0.0f, 0.001f);
}

// TransformComponent holds the LOCAL transform. Reading position straight off
// it put a parented emitter wherever its offset from the parent happened to
// land near the origin.
ENJIN_TEST(ParticleEmitterTransform, ParentedEmitterSpawnsInWorldSpace) {
    ECS::World world;

    ECS::Entity parent = world.CreateEntity();
    ECS::TransformComponent pt;
    pt.position = Vector3(10.0f, 0.0f, 0.0f);
    world.AddComponent<ECS::TransformComponent>(parent, pt);

    ECS::Entity child = MakeEmitter(world);
    world.GetComponent<ECS::TransformComponent>(child)->position = Vector3(1.0f, 0.0f, 0.0f);
    ECS::ParentComponent pc;
    pc.parent = parent;
    world.AddComponent<ECS::ParentComponent>(child, pc);

    ParticleSystem ps;
    ps.Update(0.05f, &world);

    // Particles travel along +Y, so X is untouched by the first step.
    ENJIN_EXPECT_FLOAT_NEAR(FirstParticle(world, child).position.x, 11.0f, 0.001f);
}

// A degenerate scale must not erase the effect: zero would multiply every
// particle down to nothing with no way to tell why.
ENJIN_TEST(ParticleEmitterTransform, ZeroScaleFallsBackToOne) {
    ECS::World world;
    ECS::Entity e = MakeEmitter(world);
    world.GetComponent<ECS::TransformComponent>(e)->scale = Vector3(0.0f, 0.0f, 0.0f);

    ParticleSystem ps;
    ps.Update(0.05f, &world);

    ENJIN_EXPECT_FLOAT_NEAR(FirstParticle(world, e).size, 0.5f, 0.001f);
}

ENJIN_TEST_MAIN()
