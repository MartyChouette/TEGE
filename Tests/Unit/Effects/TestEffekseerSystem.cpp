// EffekseerSystem before it has a renderer.
//
// The system is created on the first frame a scene has an effect in it, and
// every runtime calls into it from several places before that point and on
// machines with no GPU (these tests, the headless tools). None of those calls
// may do anything, and none may touch a component.
#include "EnjinTest.h"
#include "Enjin/Effects/EffekseerSystem.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/EffekseerEffect.h"

using namespace Enjin;

ENJIN_TEST(EffekseerSystem, EveryCallIsSafeBeforeInitialize) {
    Effects::EffekseerSystem system;
    ENJIN_EXPECT_FALSE(system.IsInitialized());

    ECS::World world;
    ECS::Entity e = world.CreateEntity();
    world.AddComponent<ECS::TransformComponent>(e);
    ECS::EffekseerEffectComponent fx;
    fx.effectPath = "does/not/exist.efkefc";
    world.AddComponent<ECS::EffekseerEffectComponent>(e, fx);

    system.Update(nullptr, 1.0f / 60.0f);
    system.Update(&world, 1.0f / 60.0f);
    system.BeginFrame();
    system.Draw(Effects::EffekseerSystem::Pass::Offscreen, nullptr, nullptr,
                Math::Matrix4::Identity(), Math::Matrix4::Identity(), Math::Vector3(0.0f, 0.0f, 0.0f));
    system.StopAll(&world);
    system.ReloadEffects(&world);
    system.Shutdown();

    ENJIN_EXPECT_EQ(system.GetLiveInstanceCount(), 0u);
    ENJIN_EXPECT_EQ(system.GetLoadedEffectCount(), 0u);

    // An uninitialised Update must not have consumed the component's request
    // to load: when the renderer arrives, the effect still has to start.
    const auto* out = world.GetComponent<ECS::EffekseerEffectComponent>(e);
    ENJIN_ASSERT_TRUE(out != nullptr);
    ENJIN_EXPECT_EQ(out->handle, -1);
    ENJIN_EXPECT_FALSE(out->playing);
    ENJIN_EXPECT_TRUE(out->loadError.empty());
}

// Play() and Stop() are requests the system consumes; the last one asked wins.
ENJIN_TEST(EffekseerSystem, PlayAndStopAreRequestsAndTheLastOneWins) {
    ECS::EffekseerEffectComponent fx;
    ENJIN_EXPECT_FALSE(fx.playRequested);
    ENJIN_EXPECT_FALSE(fx.stopRequested);

    fx.Play();
    ENJIN_EXPECT_TRUE(fx.playRequested);
    ENJIN_EXPECT_FALSE(fx.stopRequested);

    fx.Stop();
    ENJIN_EXPECT_FALSE(fx.playRequested);
    ENJIN_EXPECT_TRUE(fx.stopRequested);

    fx.Play();
    ENJIN_EXPECT_TRUE(fx.playRequested);
    ENJIN_EXPECT_FALSE(fx.stopRequested);
}

ENJIN_TEST_MAIN()
