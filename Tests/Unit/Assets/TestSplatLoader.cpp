// Gaussian splat loader tests: synthetic .ply (INRIA layout) and raw .spz
// payloads, verifying the decode math (SH DC -> color, sigmoid opacity,
// exp scale, quaternion order) and the mesh-ply rejection.

#include "EnjinTest.h"
#include "Enjin/Assets/SplatLoader.h"

#include <cmath>
#include <cstring>
#include <string>
#include <vector>

using namespace Enjin;

namespace {

// Build a minimal INRIA splat ply with `n` identical records
std::vector<u8> MakeSplatPly(int n, float px, float py, float pz,
                             float dc, float opacityLogit, float logScale,
                             float qw, float qx, float qy, float qz) {
    std::string header =
        "ply\n"
        "format binary_little_endian 1.0\n"
        "element vertex " + std::to_string(n) + "\n"
        "property float x\nproperty float y\nproperty float z\n"
        "property float f_dc_0\nproperty float f_dc_1\nproperty float f_dc_2\n"
        "property float opacity\n"
        "property float scale_0\nproperty float scale_1\nproperty float scale_2\n"
        "property float rot_0\nproperty float rot_1\nproperty float rot_2\nproperty float rot_3\n"
        "end_header\n";
    std::vector<u8> out(header.begin(), header.end());
    float rec[14] = {px, py, pz, dc, dc, dc, opacityLogit,
                     logScale, logScale, logScale, qw, qx, qy, qz};
    for (int i = 0; i < n; ++i) {
        const u8* p = reinterpret_cast<const u8*>(rec);
        out.insert(out.end(), p, p + sizeof(rec));
    }
    return out;
}

} // namespace

ENJIN_TEST(SplatLoader, PlyDecodesPositionColorOpacityScale) {
    // Arrange: one splat at (1,2,3), SH DC 0 (mid grey), logit 0 (opacity 0.5),
    // log-scale 0 (scale 1), identity rotation. flipYZ OFF to check raw decode.
    auto ply = MakeSplatPly(1, 1.0f, 2.0f, 3.0f, 0.0f, 0.0f, 0.0f,
                            1.0f, 0.0f, 0.0f, 0.0f);

    // Act
    auto data = Assets::SplatLoader::ParsePly(ply.data(), ply.size(), 1000, false);

    // Assert
    ENJIN_ASSERT_TRUE(data.Valid());
    ENJIN_ASSERT_EQ((int)data.splats.size(), 1);
    const auto& s = data.splats[0];
    ENJIN_EXPECT_FLOAT_NEAR(s.px, 1.0f, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(s.py, 2.0f, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(s.pz, 3.0f, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(s.r, 0.5f, 0.001f);      // SH DC 0 -> 0.5
    ENJIN_EXPECT_FLOAT_NEAR(s.opacity, 0.5f, 0.001f); // sigmoid(0)
    ENJIN_EXPECT_FLOAT_NEAR(s.sx, 1.0f, 0.001f);      // exp(0)
    ENJIN_EXPECT_FLOAT_NEAR(s.qw, 1.0f, 0.001f);      // rot_0 = w
    ENJIN_EXPECT_FLOAT_NEAR(s.qx, 0.0f, 0.001f);
}

ENJIN_TEST(SplatLoader, PlyFlipYZConvertsFrame) {
    // Arrange/Act: same splat with the COLMAP->engine flip enabled.
    auto ply = MakeSplatPly(1, 1.0f, 2.0f, 3.0f, 0.0f, 0.0f, 0.0f,
                            1.0f, 0.0f, 0.0f, 0.0f);
    auto data = Assets::SplatLoader::ParsePly(ply.data(), ply.size(), 1000, true);

    // Assert: y and z negate; identity rotation conjugates to identity (up to sign).
    ENJIN_ASSERT_TRUE(data.Valid());
    const auto& s = data.splats[0];
    ENJIN_EXPECT_FLOAT_NEAR(s.py, -2.0f, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(s.pz, -3.0f, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(std::fabs(s.qw), 1.0f, 0.001f);
}

ENJIN_TEST(SplatLoader, PlyCapsAtMaxSplats) {
    auto ply = MakeSplatPly(10, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0);
    auto data = Assets::SplatLoader::ParsePly(ply.data(), ply.size(), 4, false);
    ENJIN_ASSERT_TRUE(data.Valid());
    ENJIN_EXPECT_EQ((int)data.splats.size(), 4);
}

ENJIN_TEST(SplatLoader, MeshPlyIsRejectedWithClearError) {
    // Arrange: a plain mesh ply (x/y/z only) - must NOT parse as splats.
    std::string header =
        "ply\nformat binary_little_endian 1.0\n"
        "element vertex 1\n"
        "property float x\nproperty float y\nproperty float z\n"
        "end_header\n";
    std::vector<u8> ply(header.begin(), header.end());
    float rec[3] = {0, 0, 0};
    ply.insert(ply.end(), reinterpret_cast<u8*>(rec), reinterpret_cast<u8*>(rec) + sizeof(rec));

    // Act
    auto data = Assets::SplatLoader::ParsePly(ply.data(), ply.size(), 1000, false);

    // Assert
    ENJIN_EXPECT_FALSE(data.Valid());
    ENJIN_EXPECT_TRUE(data.error.find("not a 3D Gaussian splat") != std::string::npos);
}

ENJIN_TEST(SplatLoader, SpzRawDecodesPackedPoint) {
    // Arrange: one point in the packed layout, 12 fractional bits.
    std::vector<u8> raw;
    auto push32 = [&raw](u32 v) {
        raw.push_back(v & 0xff); raw.push_back((v >> 8) & 0xff);
        raw.push_back((v >> 16) & 0xff); raw.push_back((v >> 24) & 0xff);
    };
    push32(0x5053474e);   // magic 'NGSP'
    push32(2);            // version
    push32(1);            // numPoints
    raw.push_back(0);     // shDegree
    raw.push_back(12);    // fractionalBits
    raw.push_back(0);     // flags
    raw.push_back(0);     // reserved
    // position (1.0, -0.5, 2.0) in 12-bit fixed point, int24 little-endian
    auto push24 = [&raw](i32 v) {
        raw.push_back(v & 0xff); raw.push_back((v >> 8) & 0xff); raw.push_back((v >> 16) & 0xff);
    };
    push24(1 << 12);          // 1.0
    push24(-(1 << 11));       // -0.5
    push24(2 << 12);          // 2.0
    raw.push_back(255);       // alpha = 1.0
    // colors: SH DC 0 -> stored 127/128ish; use exactly 0.5*255 rounded
    raw.push_back(128); raw.push_back(128); raw.push_back(128);
    // scales: v/16-10 = 0 -> v = 160 -> scale exp(0) = 1
    raw.push_back(160); raw.push_back(160); raw.push_back(160);
    // rotation: xyz = 0 -> v = 127.5 -> use 128 (~0.004), w ~= 1
    raw.push_back(128); raw.push_back(128); raw.push_back(128);

    // Act (flip off: raw frame)
    auto data = Assets::SplatLoader::ParseSpzRaw(raw.data(), raw.size(), 1000, false);

    // Assert
    ENJIN_ASSERT_TRUE(data.Valid());
    const auto& s = data.splats[0];
    ENJIN_EXPECT_FLOAT_NEAR(s.px, 1.0f, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(s.py, -0.5f, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(s.pz, 2.0f, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(s.opacity, 1.0f, 0.01f);
    ENJIN_EXPECT_FLOAT_NEAR(s.sx, 1.0f, 0.01f);
    ENJIN_EXPECT_TRUE(s.qw > 0.99f);
    // grey-ish color from mid-range SH
    ENJIN_EXPECT_TRUE(s.r > 0.4f && s.r < 0.6f);
}

ENJIN_TEST(SplatLoader, SpzRejectsBadMagicAndTruncation) {
    std::vector<u8> junk(32, 0);
    auto bad = Assets::SplatLoader::ParseSpzRaw(junk.data(), junk.size(), 1000, false);
    ENJIN_EXPECT_FALSE(bad.Valid());
    std::vector<u8> tiny(4, 0);
    auto small = Assets::SplatLoader::ParseSpzRaw(tiny.data(), tiny.size(), 1000, false);
    ENJIN_EXPECT_FALSE(small.Valid());
}

ENJIN_TEST_MAIN()
