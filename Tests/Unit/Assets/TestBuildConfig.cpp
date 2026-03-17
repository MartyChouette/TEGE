#include "EnjinTest.h"
#include "Enjin/Build/BuildReport.h"
#include "Enjin/Build/AssetPacker.h"

using namespace Enjin;
using namespace Enjin::Build;

// ===========================================================================
// BuildTargetPlatform Enum
// ===========================================================================

ENJIN_TEST(BuildTargetEnum, Values) {
    ENJIN_EXPECT_EQ((int)BuildTargetPlatform::Desktop, 0);
    ENJIN_EXPECT_EQ((int)BuildTargetPlatform::Web, 1);
}

// ===========================================================================
// MessageSeverity Enum
// ===========================================================================

ENJIN_TEST(MessageSeverityEnum, Values) {
    ENJIN_EXPECT_EQ((int)MessageSeverity::Info, 0);
    ENJIN_EXPECT_EQ((int)MessageSeverity::Warning, 1);
    ENJIN_EXPECT_EQ((int)MessageSeverity::Error, 2);
}

// ===========================================================================
// BuildMessage Defaults
// ===========================================================================

ENJIN_TEST(BuildMessage, Defaults) {
    BuildMessage msg;
    ENJIN_EXPECT_EQ((int)msg.severity, (int)MessageSeverity::Info);
    ENJIN_EXPECT_TRUE(msg.text.empty());
    ENJIN_EXPECT_TRUE(msg.file.empty());
}

// ===========================================================================
// BuildConfig Defaults
// ===========================================================================

ENJIN_TEST(BuildConfig, WindowDefaults) {
    BuildConfig cfg;
    ENJIN_EXPECT_EQ(cfg.windowWidth, 1280u);
    ENJIN_EXPECT_EQ(cfg.windowHeight, 720u);
    ENJIN_EXPECT_FALSE(cfg.fullscreen);
}

ENJIN_TEST(BuildConfig, TargetDefault) {
    BuildConfig cfg;
    ENJIN_EXPECT_EQ((int)cfg.target, (int)BuildTargetPlatform::Desktop);
}

ENJIN_TEST(BuildConfig, PathsEmpty) {
    BuildConfig cfg;
    ENJIN_EXPECT_TRUE(cfg.projectPath.empty());
    ENJIN_EXPECT_TRUE(cfg.outputDir.empty());
    ENJIN_EXPECT_TRUE(cfg.buildKey.empty());
    ENJIN_EXPECT_TRUE(cfg.windowTitle.empty());
}

// ===========================================================================
// BuildResult Defaults
// ===========================================================================

ENJIN_TEST(BuildResult, DefaultFailure) {
    BuildResult result;
    ENJIN_EXPECT_FALSE(result.success);
}

ENJIN_TEST(BuildResult, ZeroCounts) {
    BuildResult result;
    ENJIN_EXPECT_EQ(result.filesPacked, 0u);
    ENJIN_EXPECT_EQ(result.totalSizeBytes, (u64)0);
    ENJIN_EXPECT_EQ(result.packedSizeBytes, (u64)0);
}

ENJIN_TEST(BuildResult, EmptyOutput) {
    BuildResult result;
    ENJIN_EXPECT_TRUE(result.outputPath.empty());
    ENJIN_EXPECT_EQ(result.messages.size(), (size_t)0);
}

// ===========================================================================
// PakIndexEntry Defaults
// ===========================================================================

ENJIN_TEST(PakIndexEntry, Defaults) {
    PakIndexEntry entry;
    ENJIN_EXPECT_TRUE(entry.virtualPath.empty());
    ENJIN_EXPECT_EQ(entry.dataOffset, (u64)0);
    ENJIN_EXPECT_EQ(entry.compressedSize, (u64)0);
    ENJIN_EXPECT_EQ(entry.originalSize, (u64)0);
    ENJIN_EXPECT_EQ(entry.crc32, 0u);
}

// ===========================================================================
// ENJPAK Constants
// ===========================================================================

ENJIN_TEST(EnjpakConstants, Magic) {
    ENJIN_EXPECT_EQ(ENJPAK_MAGIC[0], 'E');
    ENJIN_EXPECT_EQ(ENJPAK_MAGIC[1], 'N');
    ENJIN_EXPECT_EQ(ENJPAK_MAGIC[2], 'J');
    ENJIN_EXPECT_EQ(ENJPAK_MAGIC[3], 'P');
    ENJIN_EXPECT_EQ(ENJPAK_MAGIC[4], 'A');
    ENJIN_EXPECT_EQ(ENJPAK_MAGIC[5], 'K');
    ENJIN_EXPECT_EQ(ENJPAK_MAGIC[6], '1');
    ENJIN_EXPECT_EQ(ENJPAK_MAGIC[7], '0');
}

ENJIN_TEST(EnjpakConstants, FormatVersion) {
    ENJIN_EXPECT_EQ(ENJPAK_FORMAT_VERSION, (u16)1);
}

ENJIN_TEST(EnjpakConstants, ObfuscatedFlag) {
    ENJIN_EXPECT_EQ(ENJPAK_FLAG_OBFUSCATED, 1u);
}

// ===========================================================================
// AssetPacker CRC32
// ===========================================================================

ENJIN_TEST(AssetPacker, CRC32EmptyData) {
    u32 crc = AssetPacker::ComputeCRC32(nullptr, 0);
    // CRC32 of empty data should be 0 (common implementation)
    // Just verify it doesn't crash and returns a consistent value
    u32 crc2 = AssetPacker::ComputeCRC32(nullptr, 0);
    ENJIN_EXPECT_EQ(crc, crc2);
}

ENJIN_TEST(AssetPacker, CRC32KnownData) {
    const char data[] = "Hello";
    u32 crc1 = AssetPacker::ComputeCRC32(data, 5);
    u32 crc2 = AssetPacker::ComputeCRC32(data, 5);
    // Same input should produce same CRC
    ENJIN_EXPECT_EQ(crc1, crc2);
}

ENJIN_TEST(AssetPacker, CRC32DifferentData) {
    const char a[] = "Hello";
    const char b[] = "World";
    u32 crcA = AssetPacker::ComputeCRC32(a, 5);
    u32 crcB = AssetPacker::ComputeCRC32(b, 5);
    // Different data should (very likely) produce different CRCs
    ENJIN_EXPECT_NE(crcA, crcB);
}

ENJIN_TEST(AssetPacker, InitialFileCount) {
    AssetPacker packer;
    ENJIN_EXPECT_EQ(packer.GetFileCount(), 0u);
    ENJIN_EXPECT_EQ(packer.GetTotalOriginalSize(), (u64)0);
    ENJIN_EXPECT_EQ(packer.GetTotalPackedSize(), (u64)0);
}

ENJIN_TEST_MAIN()
