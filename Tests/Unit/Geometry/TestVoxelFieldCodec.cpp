// Getting a distance field into and out of a scene file.
//
// A cave that decoded to something slightly different every save would drift
// out of shape over a week of editing, one reload at a time -- and nobody would
// be able to point at the edit that did it. So the round trip is asserted
// rather than assumed, and so is the refusal of anything corrupt.

#include "EnjinTest.h"
#include "Enjin/Geometry/VoxelFieldCodec.h"

#include <cmath>
#include <vector>

using namespace Enjin;
using namespace Enjin::Geometry;

namespace {

std::vector<f32> MakeField(usize n, f32 band) {
    // Arrange: long uniform runs at both extremes with a detailed shell in the
    // middle, which is the shape a real distance field has.
    std::vector<f32> f;
    f.reserve(n);
    for (usize i = 0; i < n; ++i) {
        const f32 t = static_cast<f32>(i) / static_cast<f32>(n - 1);
        if (t < 0.4f)      f.push_back(-band);
        else if (t > 0.6f) f.push_back(band);
        else               f.push_back(band * (t - 0.5f) * 10.0f);
    }
    return f;
}

} // namespace

ENJIN_TEST(VoxelFieldCodec, AFieldSurvivesARoundTripWithinItsQuantisationStep) {
    // Arrange
    const f32 band = 2.0f;
    const std::vector<f32> original = MakeField(4096, band);

    // Act
    const std::string text = EncodeVoxelField(original, band);
    std::vector<f32> restored;
    const bool ok = DecodeVoxelField(text, band, original.size(), restored);

    // Assert
    ENJIN_ASSERT_TRUE(ok);
    ENJIN_ASSERT_EQ(restored.size(), original.size());

    // One byte across the band, so the worst error is half a step.
    const f32 step = band / 127.0f;
    f32 worst = 0.0f;
    for (usize i = 0; i < original.size(); ++i) {
        worst = std::max(worst, std::fabs(restored[i] - original[i]));
    }
    ENJIN_EXPECT_TRUE(worst <= step);
}

ENJIN_TEST(VoxelFieldCodec, EncodingIsSmallEnoughToLiveInASceneFile) {
    // Arrange: a modest cave volume, 48 x 32 x 48.
    const f32 band = 2.0f;
    const usize count = 48 * 32 * 48;
    const std::vector<f32> field = MakeField(count, band);

    // Act
    const std::string text = EncodeVoxelField(field, band);

    // Assert: written as JSON floats this would be roughly a megabyte. The
    // whole point of the codec is that the long uniform runs collapse.
    const usize asFloatText = count * 8;   // a very generous "-1.234567," each
    ENJIN_EXPECT_TRUE(text.size() < asFloatText / 10);
    ENJIN_EXPECT_TRUE(!text.empty());
}

ENJIN_TEST(VoxelFieldCodec, RepeatedRoundTripsDoNotDriftTheField) {
    // Arrange: the failure this guards against is slow. One save losing a
    // little is invisible; ten saves turning a chamber into a different shape
    // is a bug nobody can trace back to a single action.
    const f32 band = 2.0f;
    std::vector<f32> field = MakeField(2048, band);

    // Act
    std::vector<f32> afterFirst;
    ENJIN_ASSERT_TRUE(DecodeVoxelField(EncodeVoxelField(field, band), band,
                                       field.size(), afterFirst));
    std::vector<f32> current = afterFirst;
    for (u32 i = 0; i < 10; ++i) {
        std::vector<f32> next;
        ENJIN_ASSERT_TRUE(DecodeVoxelField(EncodeVoxelField(current, band), band,
                                           current.size(), next));
        current = std::move(next);
    }

    // Assert: quantisation is idempotent, so after the first pass nothing moves
    // again however many times it is saved.
    ENJIN_EXPECT_TRUE(current == afterFirst);
}

ENJIN_TEST(VoxelFieldCodec, AFieldOfTheWrongLengthIsRefusedRatherThanPadded) {
    // Arrange
    const f32 band = 1.0f;
    const std::vector<f32> field = MakeField(256, band);
    const std::string text = EncodeVoxelField(field, band);

    // Act / Assert: a field that is the wrong size for its dimensions is a
    // corrupt scene. Filling in the difference would produce a cave with a wall
    // in a place nobody carved one, which is worse than an error.
    std::vector<f32> out;
    ENJIN_EXPECT_FALSE(DecodeVoxelField(text, band, 512, out));
    ENJIN_EXPECT_FALSE(DecodeVoxelField(text, band, 255, out));
    ENJIN_EXPECT_TRUE(out.empty());

    // And the right size still works, so the check is on the length and not on
    // something incidental.
    ENJIN_EXPECT_TRUE(DecodeVoxelField(text, band, 256, out));
}

ENJIN_TEST(VoxelFieldCodec, CorruptTextIsRefused) {
    // Arrange / Act / Assert
    std::vector<f32> out;
    ENJIN_EXPECT_FALSE(DecodeVoxelField("not base64 at all!!", 1.0f, 16, out));
    ENJIN_EXPECT_FALSE(DecodeVoxelField("", 1.0f, 16, out));
    // Valid base64 that decodes to an odd number of bytes cannot be
    // (count, value) pairs.
    ENJIN_EXPECT_FALSE(DecodeVoxelField("QUJD", 1.0f, 16, out));
}

ENJIN_TEST(VoxelFieldCodec, AnEmptyFieldEncodesToNothing) {
    // Arrange / Act / Assert: an uncarved volume writes no field key at all,
    // rather than a compressed block of "all air".
    ENJIN_EXPECT_TRUE(EncodeVoxelField({}, 1.0f).empty());
    ENJIN_EXPECT_TRUE(EncodeVoxelField({1.0f}, 0.0f).empty());
}

ENJIN_TEST(VoxelFieldCodec, ValuesOutsideTheBandAreClampedNotWrapped) {
    // Arrange
    const f32 band = 1.0f;
    const std::vector<f32> field = { -500.0f, 500.0f, 0.0f };

    // Act
    std::vector<f32> out;
    ENJIN_ASSERT_TRUE(DecodeVoxelField(EncodeVoxelField(field, band), band, 3, out));

    // Assert: a wrap would turn the deepest rock into open air, which is a
    // hole through the middle of a mountain.
    ENJIN_EXPECT_FLOAT_NEAR(out[0], -band, 0.01f);
    ENJIN_EXPECT_FLOAT_NEAR(out[1], band, 0.01f);
    ENJIN_EXPECT_FLOAT_NEAR(out[2], 0.0f, 0.01f);
}

ENJIN_TEST(VoxelFieldCodec, RunsLongerThanACountByteAreSplitAndRejoined) {
    // Arrange: 1000 identical values, where the run counter is one byte.
    const f32 band = 1.0f;
    const std::vector<f32> field(1000, -band);

    // Act
    std::vector<f32> out;
    ENJIN_ASSERT_TRUE(DecodeVoxelField(EncodeVoxelField(field, band), band, 1000, out));

    // Assert
    ENJIN_ASSERT_EQ(out.size(), (usize)1000);
    for (f32 v : out) ENJIN_EXPECT_FLOAT_NEAR(v, -band, 0.01f);
}

ENJIN_TEST_MAIN()
