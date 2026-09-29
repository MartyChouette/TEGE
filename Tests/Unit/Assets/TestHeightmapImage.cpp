// Terrain heights to and from a 16-bit greyscale PNG. A terrain could only be
// shaped with the in-editor brush: no heightmap in from another tool, and none
// out. The round trip must give back the same ground to within one 16-bit step.
#include "EnjinTest.h"
#include "Enjin/Assets/HeightmapImage.h"
#include "Enjin/ECS/Components/Terrain.h"
#include <stb_image.h>
#include <cmath>

using namespace Enjin;

ENJIN_TEST(HeightmapImage, test_heightmap_png_round_trip_same_ground) {
    // Arrange: a slope with a bump, over the full Min..Max range
    ECS::TerrainComponent t;
    t.gridWidth = 33;
    t.gridHeight = 17;
    t.minHeight = -10.0f;
    t.maxHeight = 30.0f;
    t.InitializeFlat(0.0f);
    for (u32 z = 0; z < t.gridHeight; ++z)
        for (u32 x = 0; x < t.gridWidth; ++x)
            t.heightmap[z * t.gridWidth + x] = -10.0f + 40.0f * x / (t.gridWidth - 1);
    t.heightmap[3 * t.gridWidth + 5] = 12.5f;

    // Act: encode, decode with a real PNG reader, import into a fresh terrain
    const auto grey = Assets::TerrainToGray16(t);
    const auto png = Assets::EncodeGray16PNG(grey.data(), t.gridWidth, t.gridHeight);
    int w = 0, h = 0, ch = 0;
    stbi_us* px = stbi_load_16_from_memory(png.data(), static_cast<int>(png.size()), &w, &h, &ch, 1);
    ENJIN_ASSERT_TRUE(px != nullptr);
    ECS::TerrainComponent back;
    back.minHeight = -10.0f;
    back.maxHeight = 30.0f;
    Assets::TerrainFromGray16(back, px, static_cast<u32>(w), static_cast<u32>(h));
    stbi_image_free(px);

    // Assert
    ENJIN_EXPECT_EQ(w, 33);
    ENJIN_EXPECT_EQ(h, 17);
    ENJIN_EXPECT_EQ(back.gridWidth, 33u);
    ENJIN_EXPECT_EQ(back.gridHeight, 17u);
    f32 worst = 0.0f;
    for (usize i = 0; i < t.heightmap.size(); ++i)
        worst = std::max(worst, std::fabs(t.heightmap[i] - back.heightmap[i]));
    ENJIN_EXPECT_TRUE(worst <= 40.0f / 65535.0f + 1e-4f);
    ENJIN_EXPECT_EQ(back.splatmap.size(), static_cast<usize>(33 * 17 * 4));
}

ENJIN_TEST(HeightmapImage, test_heightmap_import_larger_than_limit_resamples) {
    // Arrange: a 600 x 2 image, wider than the 512 grid ceiling, black to white
    std::vector<u16> img(600 * 2);
    for (u32 y = 0; y < 2; ++y)
        for (u32 x = 0; x < 600; ++x) img[y * 600 + x] = static_cast<u16>(std::lround(65535.0 * x / 599.0));
    ECS::TerrainComponent t;
    t.minHeight = 0.0f;
    t.maxHeight = 10.0f;

    // Act
    Assets::TerrainFromGray16(t, img.data(), 600, 2);

    // Assert: clamped to 512 wide, ends still at Min and Max
    ENJIN_EXPECT_EQ(t.gridWidth, Assets::kHeightmapMaxGrid);
    ENJIN_EXPECT_FLOAT_NEAR(t.heightmap[0], 0.0f, 1e-3f);
    ENJIN_EXPECT_FLOAT_NEAR(t.heightmap[t.gridWidth - 1], 10.0f, 1e-3f);
}

ENJIN_TEST_MAIN()
