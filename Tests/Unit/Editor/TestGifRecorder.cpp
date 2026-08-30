#include "EnjinTest.h"
#include "Enjin/Editor/GifRecorder.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <vector>

using namespace Enjin;
using namespace Enjin::Editor;

namespace {

std::string TempGifPath(const char* name) {
    return (std::filesystem::temp_directory_path() / name).string();
}

std::vector<u8> ReadAll(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    return std::vector<u8>((std::istreambuf_iterator<char>(f)),
                           std::istreambuf_iterator<char>());
}

// A frame with a moving red square on a blue field - forces real palette +
// LZW content rather than a degenerate single-color stream.
std::vector<u8> MakeFrame(u32 w, u32 h, u32 squareX) {
    std::vector<u8> rgba(static_cast<usize>(w) * h * 4);
    for (u32 y = 0; y < h; ++y)
        for (u32 x = 0; x < w; ++x) {
            u8* p = &rgba[(static_cast<usize>(y) * w + x) * 4];
            bool inSquare = x >= squareX && x < squareX + 16 && y >= 8 && y < 24;
            p[0] = inSquare ? 220 : 20;
            p[1] = static_cast<u8>((x * 3) & 0x3F);   // gradient = many colors
            p[2] = inSquare ? 30 : 200;
            p[3] = 255;
        }
    return rgba;
}

} // namespace

ENJIN_TEST(GifRecorder, WritesValidHeaderAndTrailer) {
    std::string path = TempGifPath("enjin_test_basic.gif");
    GifRecorder rec;
    ENJIN_ASSERT_TRUE(rec.Start(path, 64, 32, 0));
    ENJIN_EXPECT_TRUE(rec.IsRecording());

    for (u32 i = 0; i < 3; ++i) {
        auto frame = MakeFrame(64, 32, i * 12);
        rec.AddFrame(frame.data(), 66.0f);
    }
    ENJIN_EXPECT_EQ(rec.FrameCount(), 3u);
    rec.Stop();
    ENJIN_EXPECT_TRUE(!rec.IsRecording());

    std::vector<u8> data = ReadAll(path);
    ENJIN_ASSERT_TRUE(data.size() > 800);   // header + loop ext + 3 real frames
    // GIF89a signature
    ENJIN_EXPECT_EQ(data[0], 'G'); ENJIN_EXPECT_EQ(data[1], 'I');
    ENJIN_EXPECT_EQ(data[2], 'F'); ENJIN_EXPECT_EQ(data[3], '8');
    ENJIN_EXPECT_EQ(data[4], '9'); ENJIN_EXPECT_EQ(data[5], 'a');
    // Logical screen size 64x32 (little endian)
    ENJIN_EXPECT_EQ(data[6], 64u); ENJIN_EXPECT_EQ(data[7], 0u);
    ENJIN_EXPECT_EQ(data[8], 32u); ENJIN_EXPECT_EQ(data[9], 0u);
    // Trailer byte
    ENJIN_EXPECT_EQ(data.back(), 0x3Bu);
    // Three image descriptors (0x2C at sub-block-aligned spots is hard to scan
    // exactly; count graphic control extensions 0x21 0xF9 instead)
    u32 gces = 0;
    for (usize i = 0; i + 1 < data.size(); ++i)
        if (data[i] == 0x21 && data[i + 1] == 0xF9) ++gces;
    ENJIN_EXPECT_EQ(gces, 3u);

    std::remove(path.c_str());
}

ENJIN_TEST(GifRecorder, DownscaleShiftHalvesOutput) {
    std::string path = TempGifPath("enjin_test_half.gif");
    GifRecorder rec;
    ENJIN_ASSERT_TRUE(rec.Start(path, 64, 32, 1));   // half res -> 32x16
    auto frame = MakeFrame(64, 32, 4);
    rec.AddFrame(frame.data(), 100.0f);
    rec.Stop();

    std::vector<u8> data = ReadAll(path);
    ENJIN_ASSERT_TRUE(data.size() > 20);
    ENJIN_EXPECT_EQ(data[6], 32u);   // logical width halved
    ENJIN_EXPECT_EQ(data[8], 16u);   // logical height halved
    std::remove(path.c_str());
}

ENJIN_TEST(GifRecorder, StopWithoutStartIsSafe) {
    GifRecorder rec;
    rec.Stop();
    ENJIN_EXPECT_TRUE(!rec.IsRecording());
    ENJIN_EXPECT_EQ(rec.FrameCount(), 0u);
}

ENJIN_TEST(GifRecorder, LongRecordingStaysOpen) {
    // 40 frames exercises LZW dictionary resets and sub-block splitting.
    std::string path = TempGifPath("enjin_test_long.gif");
    GifRecorder rec;
    ENJIN_ASSERT_TRUE(rec.Start(path, 96, 48, 0));
    for (u32 i = 0; i < 40; ++i) {
        auto frame = MakeFrame(96, 48, (i * 2) % 80);
        rec.AddFrame(frame.data(), 50.0f);
    }
    ENJIN_EXPECT_EQ(rec.FrameCount(), 40u);
    rec.Stop();
    std::vector<u8> data = ReadAll(path);
    ENJIN_EXPECT_TRUE(data.size() > 4000);
    ENJIN_EXPECT_EQ(data.back(), 0x3Bu);
    // ENJIN_KEEP_GIF=1 leaves the file for external-decoder verification
    // (PIL round-trip in CI/probes) instead of deleting it.
    if (!std::getenv("ENJIN_KEEP_GIF")) std::remove(path.c_str());
}

ENJIN_TEST_MAIN()
