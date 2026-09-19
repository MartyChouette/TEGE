// A post-process volume applies its authored strength, and stops applying when
// you leave it.
//
// Neither was true. All three runtimes blended the LIVE settings into
// themselves every frame -- lerp(live, volume, w) written back over live -- so
// the result re-lerped an already-lerped value. A volume authored at weight 0.5
// was at 0.75 on frame two and 0.875 on frame three, converging on full
// strength regardless of what the author asked for, and blendRadius edge
// falloff washed out the same way. Then, because a volume that stops
// contributing is simply skipped rather than unwound, nothing ever put the
// original values back: one walk through a tinted corridor tinted the whole
// session. In the editor it also overwrote the PostProcessing panel's values in
// place, so the authored numbers were gone for good.
//
// The comment at the old call site said the base was "captured once and
// restored before blending". Nothing captured it and nothing restored it.
//
// BlendPostProcessVolumes takes the base as a separate parameter, which is what
// makes frame N+1 start from the same place frame N did. These tests call it
// repeatedly with a fixed base, the way a runtime does.
#include "EnjinTest.h"
#include "Enjin/ECS/PostProcessVolumeBlend.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"

#include <cmath>

using namespace Enjin;

namespace {

// A volume at `position` that pushes saturation to `target`.
ECS::Entity MakeSaturationVolume(ECS::World& world, const Math::Vector3& position,
                                 f32 target, f32 weight, f32 halfExtent = 5.0f) {
    ECS::Entity e = world.CreateEntity();
    ECS::TransformComponent t;
    t.position = position;
    world.AddComponent<ECS::TransformComponent>(e, t);

    ECS::PostProcessVolumeComponent vol;
    vol.shape = ECS::PPVolumeShape::Box;
    vol.halfExtents = Math::Vector3(halfExtent, halfExtent, halfExtent);
    vol.blendRadius = 0.0f;          // hard edge, so weight is the only variable
    vol.weight = weight;
    vol.overrideMask = ECS::PostProcessVolumeComponent::OverrideColorGrading;
    vol.settings.saturation = target;
    world.AddComponent<ECS::PostProcessVolumeComponent>(e, vol);
    return e;
}

Renderer::PostProcessSettings BaseWithSaturation(f32 saturation) {
    Renderer::PostProcessSettings s;
    s.saturation = saturation;
    return s;
}

} // namespace

ENJIN_TEST(PostProcessVolumes, test_a_volume_at_half_weight_stays_at_half_weight) {
    // The headline bug. Ten frames inside the same volume must give the same
    // answer as one frame; before the fix this converged on 2.0.
    // Arrange
    ECS::World world;
    MakeSaturationVolume(world, Math::Vector3(0, 0, 0), /*target*/ 2.0f, /*weight*/ 0.5f);
    const Renderer::PostProcessSettings base = BaseWithSaturation(1.0f);
    const Math::Vector3 inside(0, 0, 0);

    // Act: the same camera position, frame after frame.
    Renderer::PostProcessSettings out = base;
    for (int frame = 0; frame < 10; ++frame) {
        ENJIN_EXPECT_TRUE(ECS::BlendPostProcessVolumes(&world, inside, base, out));
    }

    // Assert: lerp(1.0, 2.0, 0.5) every time.
    ENJIN_EXPECT_TRUE(std::abs(out.saturation - 1.5f) < 0.001f);
}

ENJIN_TEST(PostProcessVolumes, test_leaving_a_volume_reports_no_contribution) {
    // The runtime restores its base when this returns false. Before the fix the
    // volume was skipped and the stale blended value simply stayed on screen.
    // Arrange
    ECS::World world;
    MakeSaturationVolume(world, Math::Vector3(0, 0, 0), 2.0f, 1.0f, /*halfExtent*/ 5.0f);
    const Renderer::PostProcessSettings base = BaseWithSaturation(1.0f);

    // Act
    Renderer::PostProcessSettings out = base;
    const bool insideContributed =
        ECS::BlendPostProcessVolumes(&world, Math::Vector3(0, 0, 0), base, out);
    const bool outsideContributed =
        ECS::BlendPostProcessVolumes(&world, Math::Vector3(100, 0, 0), base, out);

    // Assert
    ENJIN_EXPECT_TRUE(insideContributed);
    ENJIN_EXPECT_FALSE(outsideContributed);
}

ENJIN_TEST(PostProcessVolumes, test_walking_out_and_back_in_gives_the_same_value) {
    // The session-long tint, stated as a round trip.
    // Arrange
    ECS::World world;
    MakeSaturationVolume(world, Math::Vector3(0, 0, 0), 2.0f, 0.5f);
    const Renderer::PostProcessSettings base = BaseWithSaturation(1.0f);

    // Act
    Renderer::PostProcessSettings first = base;
    ECS::BlendPostProcessVolumes(&world, Math::Vector3(0, 0, 0), base, first);

    Renderer::PostProcessSettings outside = base;
    ECS::BlendPostProcessVolumes(&world, Math::Vector3(100, 0, 0), base, outside);

    Renderer::PostProcessSettings second = base;
    ECS::BlendPostProcessVolumes(&world, Math::Vector3(0, 0, 0), base, second);

    // Assert
    ENJIN_EXPECT_TRUE(std::abs(first.saturation - second.saturation) < 0.001f);
}

ENJIN_TEST(PostProcessVolumes, test_a_higher_priority_volume_wins_where_they_overlap) {
    // Lowest priority blends first so the highest one is applied last. Both are
    // full weight and overlap at the origin, so the winner should be exactly
    // the high-priority target.
    // Arrange
    ECS::World world;
    ECS::Entity low  = MakeSaturationVolume(world, Math::Vector3(0, 0, 0), 2.0f, 1.0f);
    ECS::Entity high = MakeSaturationVolume(world, Math::Vector3(0, 0, 0), 0.0f, 1.0f);
    world.GetComponent<ECS::PostProcessVolumeComponent>(low)->priority = 0;
    world.GetComponent<ECS::PostProcessVolumeComponent>(high)->priority = 10;
    const Renderer::PostProcessSettings base = BaseWithSaturation(1.0f);

    // Act
    Renderer::PostProcessSettings out = base;
    ECS::BlendPostProcessVolumes(&world, Math::Vector3(0, 0, 0), base, out);

    // Assert
    ENJIN_EXPECT_TRUE(std::abs(out.saturation - 0.0f) < 0.001f);
}

ENJIN_TEST(PostProcessVolumes, test_an_inactive_volume_contributes_nothing) {
    // Arrange
    ECS::World world;
    ECS::Entity e = MakeSaturationVolume(world, Math::Vector3(0, 0, 0), 2.0f, 1.0f);
    world.GetComponent<ECS::PostProcessVolumeComponent>(e)->isActive = false;
    const Renderer::PostProcessSettings base = BaseWithSaturation(1.0f);

    // Act
    Renderer::PostProcessSettings out = base;
    const bool contributed =
        ECS::BlendPostProcessVolumes(&world, Math::Vector3(0, 0, 0), base, out);

    // Assert
    ENJIN_EXPECT_FALSE(contributed);
}

ENJIN_TEST(PostProcessVolumes, test_a_global_volume_applies_from_anywhere) {
    // isGlobal ignores shape and position, and needs no TransformComponent.
    // Arrange
    ECS::World world;
    ECS::Entity e = world.CreateEntity();
    ECS::PostProcessVolumeComponent vol;
    vol.isGlobal = true;
    vol.weight = 1.0f;
    vol.overrideMask = ECS::PostProcessVolumeComponent::OverrideColorGrading;
    vol.settings.saturation = 0.25f;
    world.AddComponent<ECS::PostProcessVolumeComponent>(e, vol);
    const Renderer::PostProcessSettings base = BaseWithSaturation(1.0f);

    // Act
    Renderer::PostProcessSettings out = base;
    const bool contributed =
        ECS::BlendPostProcessVolumes(&world, Math::Vector3(9999, 9999, 9999), base, out);

    // Assert
    ENJIN_EXPECT_TRUE(contributed);
    ENJIN_EXPECT_TRUE(std::abs(out.saturation - 0.25f) < 0.001f);
}

ENJIN_TEST(PostProcessVolumes, test_the_override_mask_leaves_other_groups_alone) {
    // A volume that only overrides colour grading must not touch the base's
    // vignette. Partial override is the reason the mask exists.
    // Arrange
    ECS::World world;
    MakeSaturationVolume(world, Math::Vector3(0, 0, 0), 2.0f, 1.0f);
    Renderer::PostProcessSettings base = BaseWithSaturation(1.0f);
    base.vignetteEnabled = 1;
    base.vignetteIntensity = 0.75f;

    // Act
    Renderer::PostProcessSettings out = base;
    ECS::BlendPostProcessVolumes(&world, Math::Vector3(0, 0, 0), base, out);

    // Assert
    ENJIN_EXPECT_TRUE(std::abs(out.saturation - 2.0f) < 0.001f);
    ENJIN_EXPECT_TRUE(std::abs(out.vignetteIntensity - 0.75f) < 0.001f);
}

ENJIN_TEST(PostProcessVolumes, test_a_world_with_no_volumes_contributes_nothing) {
    // The early-out every runtime relies on to skip its restore.
    // Arrange
    ECS::World world;
    const Renderer::PostProcessSettings base = BaseWithSaturation(1.0f);

    // Act
    Renderer::PostProcessSettings out = base;
    const bool contributed =
        ECS::BlendPostProcessVolumes(&world, Math::Vector3(0, 0, 0), base, out);

    // Assert
    ENJIN_EXPECT_FALSE(contributed);
}

ENJIN_TEST_MAIN()
