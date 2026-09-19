// Temporal jitter is only applied when something will resolve it.
//
// It used to be applied on the strength of the user's preference alone: pick
// TAA or any upscaler and the projection matrix got a per-frame sub-pixel
// Halton offset. Nothing in a shipped game ever consumed it. ApplyTAA is called
// only from the editor, and the upscaler's Dispatch has exactly one call site,
// also in the editor -- so a built game rendered at full resolution with a
// sub-pixel wobble and no temporal accumulation. That is shimmer: strictly
// worse than leaving the setting off, and worse than that, it previewed
// CORRECTLY in the editor, so the one place it looked right was the only place
// anyone would have checked.
//
// The rule these pin is that "the user asked for TAA" and "TAA will happen" are
// different questions, and the jitter follows the second one.
#include "EnjinTest.h"
#include "Enjin/Renderer/HaltonSequence.h"

#include <cmath>

using namespace Enjin;
using Renderer::ShouldApplyTemporalJitter;

namespace {
constexpr u32 kAAOff = 0;
constexpr u32 kFXAA = 1;
constexpr u32 kTAA = 2;
constexpr u32 kUpscalerNone = 0;
constexpr u32 kFSR2 = 1;
constexpr u32 kDLSS = 2;
constexpr u32 kXeSS = 3;
} // namespace

ENJIN_TEST(TemporalJitter, test_taa_without_a_resolve_does_not_jitter) {
    // The shipped-game case, and the whole point of the change.
    ENJIN_EXPECT_FALSE(ShouldApplyTemporalJitter(kTAA, kUpscalerNone, false));
}

ENJIN_TEST(TemporalJitter, test_taa_with_a_resolve_jitters) {
    ENJIN_EXPECT_TRUE(ShouldApplyTemporalJitter(kTAA, kUpscalerNone, true));
}

ENJIN_TEST(TemporalJitter, test_no_upscaler_jitters_without_a_resolve) {
    // Every upscaler, because each one was individually selectable and each one
    // degraded a build the same way.
    ENJIN_EXPECT_FALSE(ShouldApplyTemporalJitter(kAAOff, kFSR2, false));
    ENJIN_EXPECT_FALSE(ShouldApplyTemporalJitter(kAAOff, kDLSS, false));
    ENJIN_EXPECT_FALSE(ShouldApplyTemporalJitter(kAAOff, kXeSS, false));
}

ENJIN_TEST(TemporalJitter, test_an_upscaler_with_a_resolve_jitters) {
    ENJIN_EXPECT_TRUE(ShouldApplyTemporalJitter(kAAOff, kFSR2, true));
    ENJIN_EXPECT_TRUE(ShouldApplyTemporalJitter(kAAOff, kDLSS, true));
    ENJIN_EXPECT_TRUE(ShouldApplyTemporalJitter(kAAOff, kXeSS, true));
}

ENJIN_TEST(TemporalJitter, test_a_resolve_alone_does_not_jitter) {
    // The consumer flag must not be able to turn jitter on by itself. If it
    // could, a runtime that sets it once and never clears it would jitter with
    // anti-aliasing switched off entirely.
    ENJIN_EXPECT_FALSE(ShouldApplyTemporalJitter(kAAOff, kUpscalerNone, true));
    ENJIN_EXPECT_FALSE(ShouldApplyTemporalJitter(kFXAA, kUpscalerNone, true));
}

ENJIN_TEST(TemporalJitter, test_fxaa_never_jitters) {
    // FXAA is a single-frame filter. It has no history to reproject into, so
    // jitter would be shimmer with no upside under any flag.
    ENJIN_EXPECT_FALSE(ShouldApplyTemporalJitter(kFXAA, kUpscalerNone, false));
    ENJIN_EXPECT_FALSE(ShouldApplyTemporalJitter(kFXAA, kUpscalerNone, true));
}

ENJIN_TEST(TemporalJitter, test_taa_and_an_upscaler_together_still_need_a_resolve) {
    // Both selected at once is a real configuration -- the editor offers it and
    // warns. Two wants still do not add up to a consumer.
    ENJIN_EXPECT_FALSE(ShouldApplyTemporalJitter(kTAA, kFSR2, false));
    ENJIN_EXPECT_TRUE(ShouldApplyTemporalJitter(kTAA, kFSR2, true));
}

ENJIN_TEST(TemporalJitter, test_the_jitter_offset_is_sub_pixel) {
    // Guards the thing the gate protects: a Halton offset is a fraction of one
    // pixel in NDC. If it ever exceeded a pixel, an unresolved jitter would
    // read as the whole image shaking rather than as shimmer.
    // Arrange
    const u32 width = 1920;
    const u32 height = 1080;
    const f32 pixelX = 2.0f / static_cast<f32>(width);
    const f32 pixelY = 2.0f / static_cast<f32>(height);

    // Act / Assert
    for (u32 frame = 0; frame < Renderer::kHaltonSampleCount * 2; ++frame) {
        const Math::Vector2 j = Renderer::HaltonJitter(frame, width, height);
        ENJIN_EXPECT_TRUE(std::abs(j.x) <= pixelX * 0.5f);
        ENJIN_EXPECT_TRUE(std::abs(j.y) <= pixelY * 0.5f);
    }
}

ENJIN_TEST(TemporalJitter, test_the_jitter_sequence_repeats_on_its_period) {
    // The sequence has to be periodic for a resolve to weight samples evenly.
    // Arrange
    const u32 w = 1280, h = 720;

    // Act
    const Math::Vector2 first = Renderer::HaltonJitter(3, w, h);
    const Math::Vector2 wrapped = Renderer::HaltonJitter(3 + Renderer::kHaltonSampleCount, w, h);

    // Assert
    ENJIN_EXPECT_TRUE(std::abs(first.x - wrapped.x) < 1e-6f);
    ENJIN_EXPECT_TRUE(std::abs(first.y - wrapped.y) < 1e-6f);
}

ENJIN_TEST(TemporalJitter, test_no_frame_in_the_sequence_is_a_zero_offset) {
    // A zero sample wastes a frame of the pattern: it re-renders a position the
    // history already has. HaltonJitter starts at index 1 to avoid it, and that
    // is easy to undo by "simplifying" the modulo.
    // Arrange
    const u32 w = 1280, h = 720;

    // Act / Assert
    for (u32 frame = 0; frame < Renderer::kHaltonSampleCount; ++frame) {
        const Math::Vector2 j = Renderer::HaltonJitter(frame, w, h);
        ENJIN_EXPECT_TRUE(std::abs(j.x) > 1e-9f || std::abs(j.y) > 1e-9f);
    }
}

ENJIN_TEST_MAIN()
