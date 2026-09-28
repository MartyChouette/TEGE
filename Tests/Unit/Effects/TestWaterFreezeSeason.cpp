// Water freeze and seasonal tree canopies, now one engine copy each. They were
// written inline in the desktop player and the editor (freeze) and inside the
// Vulkan TreeRenderer (seasons), so the web player had neither: a pond stayed
// open water in snow and a browser's trees kept their summer canopy all year.
#include "EnjinTest.h"
#include "Enjin/Effects/WaterFreeze.h"
#include "Enjin/Effects/TreeSeason.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/WaterVolume.h"

using namespace Enjin;

ENJIN_TEST(WaterFreeze, test_water_freeze_snow_freezes_then_warmth_thaws) {
    // Arrange
    ECS::World world;
    const ECS::Entity pond = world.CreateEntity();
    world.AddComponent<ECS::TransformComponent>(pond, ECS::TransformComponent{});
    ECS::WaterVolumeComponent wv;
    wv.freezeRate = 0.5f;
    wv.thawRate = 0.5f;
    world.AddComponent<ECS::WaterVolumeComponent>(pond, wv);

    // Act: four seconds of heavy snow
    for (int i = 0; i < 240; ++i) Effects::UpdateWaterFreeze(&world, 0.8f, 1.0f / 60.0f);
    const auto* frozen = world.GetComponent<ECS::WaterVolumeComponent>(pond);
    const bool wasFrozen = frozen->isFrozen;

    // then four seconds with none
    for (int i = 0; i < 240; ++i) Effects::UpdateWaterFreeze(&world, 0.0f, 1.0f / 60.0f);

    // Assert
    ENJIN_EXPECT_TRUE(wasFrozen);
    ENJIN_EXPECT_FLOAT_EQ(world.GetComponent<ECS::WaterVolumeComponent>(pond)->freezeProgress, 0.0f);
}

ENJIN_TEST(TreeSeason, test_tree_season_deciduous_bare_in_winter_evergreen_full) {
    // Arrange
    ECS::TreeVolumeComponent oak;
    oak.treeType = ECS::TreeType::Deciduous;
    ECS::TreeVolumeComponent pine = oak;
    pine.treeType = ECS::TreeType::Evergreen;

    // Act
    const auto oakWinter = Effects::ComputeSeasonalCanopy(oak, Effects::Season::Winter, 0.5f);
    const auto oakSummer = Effects::ComputeSeasonalCanopy(oak, Effects::Season::Summer, 0.5f);
    const auto pineWinter = Effects::ComputeSeasonalCanopy(pine, Effects::Season::Winter, 0.5f);

    // Assert
    ENJIN_EXPECT_FLOAT_EQ(oakWinter.scale, 0.0f);
    ENJIN_EXPECT_FLOAT_EQ(oakSummer.scale, 1.0f);
    ENJIN_EXPECT_FLOAT_EQ(oakSummer.base.x, oak.summerCanopyColor.x);
    ENJIN_EXPECT_FLOAT_EQ(pineWinter.scale, 1.0f);
    ENJIN_EXPECT_FLOAT_EQ(pineWinter.base.x, pine.canopyBaseColor.x);
}

ENJIN_TEST_MAIN()
