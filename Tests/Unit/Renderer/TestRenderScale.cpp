// One Render Scale number, honoured rather than merely stored.
//
// GraphicsSettings::renderScale was written by the menu slider AND by all four
// quality presets, serialized with the rest of the settings, and read by
// nothing in any runtime. The slider offered 0.25 to 2.0, a range in which
// only part is renderable at all: no path renders below 0.5, and 2.0 promised
// supersampling that has never existed.
//
// Desktop reaches a lower render resolution through the FSR 2 presets, so the
// continuous number has to land on one of four. This covers that mapping,
// which is the part with rules rather than plumbing.
#include "EnjinTest.h"
#include "Enjin/Renderer/Upscaling/IUpscaler.h"

using namespace Enjin;
using namespace Enjin::Renderer;

#if !ENJIN_RENDERER_WEBGPU

ENJIN_TEST(RenderScale, EachPresetsOwnScaleSelectsThatPreset) {
    // Arrange / Act / Assert: the mapping must be the exact inverse of the
    // table it reads, or a preset becomes unreachable from the slider.
    const UpscalerQuality all[] = {
        UpscalerQuality::Performance, UpscalerQuality::Balanced,
        UpscalerQuality::Quality,     UpscalerQuality::UltraQuality
    };
    for (UpscalerQuality q : all) {
        const f32 scale = IUpscaler::GetRenderScaleForQuality(q);
        ENJIN_EXPECT_TRUE(IUpscaler::GetQualityForRenderScale(scale) == q);
    }
}

ENJIN_TEST(RenderScale, AScaleBetweenPresetsPicksTheNearer) {
    // Arrange: 0.60 sits between Balanced (0.58) and Quality (0.67), nearer
    // Balanced. A mapping that rounded in one direction only would answer
    // Quality here and quietly cost performance the player asked to save.
    // Act / Assert
    ENJIN_EXPECT_TRUE(IUpscaler::GetQualityForRenderScale(0.60f) == UpscalerQuality::Balanced);
    ENJIN_EXPECT_TRUE(IUpscaler::GetQualityForRenderScale(0.65f) == UpscalerQuality::Quality);
}

ENJIN_TEST(RenderScale, TheLowestRequestGetsTheCheapestPreset) {
    // Arrange: a player dragging the slider to the floor wants the fastest
    // frame available, not the nearest preset to some clamped midpoint.
    // Act / Assert
    ENJIN_EXPECT_TRUE(IUpscaler::GetQualityForRenderScale(0.5f) == UpscalerQuality::Performance);
    ENJIN_EXPECT_TRUE(IUpscaler::GetQualityForRenderScale(0.1f) == UpscalerQuality::Performance);
}

ENJIN_TEST(RenderScale, NativeIsAboveEveryPresetSoTheUpscalerCanBeSwitchedOff) {
    // Arrange: the cutoff exists so that "1.0" means a native frame with no
    // upscaler running, rather than UltraQuality's 0.77. If the cutoff ever
    // fell at or below a preset's scale, that preset would be unreachable and
    // asking for it would silently disable upscaling instead.
    // Act / Assert
    ENJIN_EXPECT_TRUE(IUpscaler::kNativeRenderScale > IUpscaler::GetRenderScaleForQuality(UpscalerQuality::UltraQuality));
    ENJIN_EXPECT_TRUE(IUpscaler::kNativeRenderScale <= 1.0f);
}

ENJIN_TEST(RenderScale, EveryPresetStaysBelowNativeAndAboveTheFloor) {
    // Arrange: the presets are what the slider's 0.5-1.0 range promises to be
    // able to reach. A preset outside it would be unreachable from the menu.
    const UpscalerQuality all[] = {
        UpscalerQuality::Performance, UpscalerQuality::Balanced,
        UpscalerQuality::Quality,     UpscalerQuality::UltraQuality
    };
    // Act / Assert
    for (UpscalerQuality q : all) {
        const f32 s = IUpscaler::GetRenderScaleForQuality(q);
        ENJIN_EXPECT_TRUE(s >= 0.5f);
        ENJIN_EXPECT_TRUE(s < 1.0f);
    }
}

ENJIN_TEST(RenderScale, RenderResolutionIsEvenAsUpscalersRequire) {
    // Arrange: an odd render width fails FSR 2's dispatch assumptions, and
    // 1080p at 0.58 is exactly the kind of size that rounds to odd.
    u32 w = 0, h = 0;

    // Act
    IUpscaler::GetRenderResolution(1920, 1080, UpscalerQuality::Balanced, w, h);

    // Assert
    ENJIN_EXPECT_TRUE((w % 2u) == 0u);
    ENJIN_EXPECT_TRUE((h % 2u) == 0u);
    ENJIN_EXPECT_TRUE(w < 1920u && w > 960u);
}

#endif // !ENJIN_RENDERER_WEBGPU

ENJIN_TEST_MAIN()
