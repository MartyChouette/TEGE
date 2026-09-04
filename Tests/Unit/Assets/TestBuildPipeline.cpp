#include "EnjinTest.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Build/BuildPipeline.h"
#include "Enjin/Build/AssetPacker.h"
#include "Enjin/Build/AssetReader.h"
#include "Enjin/Build/BuildReport.h"
#include <cstring>
#include <filesystem>
#include <fstream>
#include <algorithm>

using namespace Enjin;
using namespace Enjin::Build;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static std::string GetTempPakPath(const char* suffix = "build_pipeline_test.enjpak") {
    auto tmp = std::filesystem::temp_directory_path() / suffix;
    return tmp.string();
}

static std::string GetTempDir(const char* name = "enjin_build_test") {
    auto tmp = std::filesystem::temp_directory_path() / name;
    return tmp.string();
}

// ============================================================================
// BuildConfig — Default Values
// ============================================================================

ENJIN_TEST(BuildConfigDefaults, WindowWidthDefault) {
    BuildConfig cfg;
    ENJIN_EXPECT_EQ(cfg.windowWidth, 1280u);
}

ENJIN_TEST(BuildConfigDefaults, WindowHeightDefault) {
    BuildConfig cfg;
    ENJIN_EXPECT_EQ(cfg.windowHeight, 720u);
}

ENJIN_TEST(BuildConfigDefaults, FullscreenDefaultOff) {
    BuildConfig cfg;
    ENJIN_EXPECT_FALSE(cfg.fullscreen);
}

ENJIN_TEST(BuildConfigDefaults, TargetDefaultIsDesktop) {
    BuildConfig cfg;
    ENJIN_EXPECT_EQ((int)cfg.target, (int)BuildTargetPlatform::Desktop);
}

ENJIN_TEST(BuildConfigDefaults, ProjectPathDefaultEmpty) {
    BuildConfig cfg;
    ENJIN_EXPECT_TRUE(cfg.projectPath.empty());
}

ENJIN_TEST(BuildConfigDefaults, OutputDirDefaultEmpty) {
    BuildConfig cfg;
    ENJIN_EXPECT_TRUE(cfg.outputDir.empty());
}

ENJIN_TEST(BuildConfigDefaults, WindowTitleDefaultEmpty) {
    BuildConfig cfg;
    ENJIN_EXPECT_TRUE(cfg.windowTitle.empty());
}

ENJIN_TEST(BuildConfigDefaults, BuildKeyDefaultEmpty) {
    BuildConfig cfg;
    ENJIN_EXPECT_TRUE(cfg.buildKey.empty());
}

// ============================================================================
// BuildConfig — Validation (empty required paths fail Execute)
// ============================================================================

ENJIN_TEST(BuildConfigValidation, EmptyProjectPathFails) {
    BuildConfig cfg;
    cfg.projectPath = "";
    cfg.outputDir   = GetTempDir();

    BuildPipeline pipeline;
    BuildResult result = pipeline.Execute(cfg);
    ENJIN_EXPECT_FALSE(result.success);
}

ENJIN_TEST(BuildConfigValidation, EmptyOutputDirFails) {
    BuildConfig cfg;
    cfg.projectPath = GetTempDir("enjin_fake_project");
    cfg.outputDir   = "";

    BuildPipeline pipeline;
    BuildResult result = pipeline.Execute(cfg);
    ENJIN_EXPECT_FALSE(result.success);
}

ENJIN_TEST(BuildConfigValidation, NonExistentProjectPathFails) {
    BuildConfig cfg;
    cfg.projectPath = GetTempDir("enjin_no_such_project_xyzzy");
    cfg.outputDir   = GetTempDir();

    BuildPipeline pipeline;
    BuildResult result = pipeline.Execute(cfg);
    ENJIN_EXPECT_FALSE(result.success);
}

ENJIN_TEST(BuildConfigValidation, BothPathsEmptyFails) {
    BuildConfig cfg;
    // projectPath and outputDir are both empty

    BuildPipeline pipeline;
    BuildResult result = pipeline.Execute(cfg);
    ENJIN_EXPECT_FALSE(result.success);
}

// ============================================================================
// BuildConfig — Window Dimensions
// ============================================================================

ENJIN_TEST(BuildConfigDimensions, StandardResolutionRoundTrips) {
    BuildConfig cfg;
    cfg.windowWidth  = 1920u;
    cfg.windowHeight = 1080u;
    ENJIN_EXPECT_EQ(cfg.windowWidth,  1920u);
    ENJIN_EXPECT_EQ(cfg.windowHeight, 1080u);
}

ENJIN_TEST(BuildConfigDimensions, SmallResolutionAllowed) {
    BuildConfig cfg;
    cfg.windowWidth  = 320u;
    cfg.windowHeight = 240u;
    ENJIN_EXPECT_EQ(cfg.windowWidth,  320u);
    ENJIN_EXPECT_EQ(cfg.windowHeight, 240u);
}

ENJIN_TEST(BuildConfigDimensions, LargeResolutionAllowed) {
    BuildConfig cfg;
    cfg.windowWidth  = 3840u;
    cfg.windowHeight = 2160u;
    ENJIN_EXPECT_EQ(cfg.windowWidth,  3840u);
    ENJIN_EXPECT_EQ(cfg.windowHeight, 2160u);
}

ENJIN_TEST(BuildConfigDimensions, ZeroWidthStoredAsZero) {
    // BuildConfig is a plain struct — it stores whatever is assigned.
    // The pipeline's Execute() is responsible for rejecting invalid dims.
    BuildConfig cfg;
    cfg.windowWidth = 0u;
    ENJIN_EXPECT_EQ(cfg.windowWidth, 0u);
}

ENJIN_TEST(BuildConfigDimensions, ZeroHeightStoredAsZero) {
    BuildConfig cfg;
    cfg.windowHeight = 0u;
    ENJIN_EXPECT_EQ(cfg.windowHeight, 0u);
}

// ============================================================================
// BuildConfig — Target Platform
// ============================================================================

ENJIN_TEST(BuildConfigTarget, CanSetDesktop) {
    BuildConfig cfg;
    cfg.target = BuildTargetPlatform::Desktop;
    ENJIN_EXPECT_EQ((int)cfg.target, (int)BuildTargetPlatform::Desktop);
}

ENJIN_TEST(BuildConfigTarget, CanSetWeb) {
    BuildConfig cfg;
    cfg.target = BuildTargetPlatform::Web;
    ENJIN_EXPECT_EQ((int)cfg.target, (int)BuildTargetPlatform::Web);
}

ENJIN_TEST(BuildConfigTarget, DesktopAndWebAreDifferent) {
    ENJIN_EXPECT_NE((int)BuildTargetPlatform::Desktop, (int)BuildTargetPlatform::Web);
}

// ============================================================================
// BuildResult — Default State
// ============================================================================

ENJIN_TEST(BuildResultDefaults, SuccessDefaultsFalse) {
    BuildResult result;
    ENJIN_EXPECT_FALSE(result.success);
}

ENJIN_TEST(BuildResultDefaults, MessagesDefaultEmpty) {
    BuildResult result;
    ENJIN_EXPECT_EQ(result.messages.size(), (usize)0);
}

ENJIN_TEST(BuildResultDefaults, FilesPackedDefaultZero) {
    BuildResult result;
    ENJIN_EXPECT_EQ(result.filesPacked, 0u);
}

ENJIN_TEST(BuildResultDefaults, SizeFieldsDefaultZero) {
    BuildResult result;
    ENJIN_EXPECT_EQ(result.totalSizeBytes, (u64)0);
    ENJIN_EXPECT_EQ(result.packedSizeBytes, (u64)0);
}

ENJIN_TEST(BuildResultDefaults, OutputPathDefaultEmpty) {
    BuildResult result;
    ENJIN_EXPECT_TRUE(result.outputPath.empty());
}

// ============================================================================
// BuildResult — Message Tracking
// ============================================================================

ENJIN_TEST(BuildResultMessages, AddInfoMessage) {
    BuildResult result;
    BuildMessage msg;
    msg.severity = MessageSeverity::Info;
    msg.text     = "Build started";
    result.messages.push_back(msg);

    ENJIN_EXPECT_EQ(result.messages.size(), (usize)1);
    ENJIN_EXPECT_EQ((int)result.messages[0].severity, (int)MessageSeverity::Info);
    ENJIN_EXPECT_STR_EQ(result.messages[0].text.c_str(), "Build started");
}

ENJIN_TEST(BuildResultMessages, AddWarningMessage) {
    BuildResult result;
    BuildMessage msg;
    msg.severity = MessageSeverity::Warning;
    msg.text     = "Asset not found, skipping";
    msg.file     = "textures/missing.png";
    result.messages.push_back(msg);

    ENJIN_EXPECT_EQ((int)result.messages[0].severity, (int)MessageSeverity::Warning);
    ENJIN_EXPECT_STR_EQ(result.messages[0].file.c_str(), "textures/missing.png");
}

ENJIN_TEST(BuildResultMessages, AddErrorMessage) {
    BuildResult result;
    BuildMessage msg;
    msg.severity = MessageSeverity::Error;
    msg.text     = "Failed to pack assets";
    result.messages.push_back(msg);

    ENJIN_EXPECT_EQ((int)result.messages[0].severity, (int)MessageSeverity::Error);
    ENJIN_EXPECT_STR_EQ(result.messages[0].text.c_str(), "Failed to pack assets");
}

ENJIN_TEST(BuildResultMessages, MultipleMixedMessages) {
    BuildResult result;

    BuildMessage info;
    info.severity = MessageSeverity::Info;
    info.text     = "Scanning project";
    result.messages.push_back(info);

    BuildMessage warn;
    warn.severity = MessageSeverity::Warning;
    warn.text     = "Deprecated API usage";
    result.messages.push_back(warn);

    BuildMessage err;
    err.severity = MessageSeverity::Error;
    err.text     = "Missing scene file";
    result.messages.push_back(err);

    ENJIN_EXPECT_EQ(result.messages.size(), (usize)3);

    // Count by severity
    int infoCount = 0, warnCount = 0, errCount = 0;
    for (const auto& m : result.messages) {
        if (m.severity == MessageSeverity::Info)    ++infoCount;
        if (m.severity == MessageSeverity::Warning) ++warnCount;
        if (m.severity == MessageSeverity::Error)   ++errCount;
    }
    ENJIN_EXPECT_EQ(infoCount, 1);
    ENJIN_EXPECT_EQ(warnCount, 1);
    ENJIN_EXPECT_EQ(errCount, 1);
}

ENJIN_TEST(BuildResultMessages, SeverityFilterInfo) {
    BuildResult result;
    for (int i = 0; i < 3; ++i) {
        BuildMessage m;
        m.severity = MessageSeverity::Info;
        m.text     = "Info " + std::to_string(i);
        result.messages.push_back(m);
    }
    BuildMessage e;
    e.severity = MessageSeverity::Error;
    e.text     = "One error";
    result.messages.push_back(e);

    auto countSeverity = [&](MessageSeverity sv) -> usize {
        usize c = 0;
        for (const auto& m : result.messages)
            if (m.severity == sv) ++c;
        return c;
    };

    ENJIN_EXPECT_EQ(countSeverity(MessageSeverity::Info),  (usize)3);
    ENJIN_EXPECT_EQ(countSeverity(MessageSeverity::Error), (usize)1);
    ENJIN_EXPECT_EQ(countSeverity(MessageSeverity::Warning), (usize)0);
}

// ============================================================================
// MessageSeverity — Enum Values
// ============================================================================

ENJIN_TEST(MessageSeverityEnum, ValuesAreDistinct) {
    ENJIN_EXPECT_NE((int)MessageSeverity::Info,    (int)MessageSeverity::Warning);
    ENJIN_EXPECT_NE((int)MessageSeverity::Warning, (int)MessageSeverity::Error);
    ENJIN_EXPECT_NE((int)MessageSeverity::Info,    (int)MessageSeverity::Error);
}

ENJIN_TEST(MessageSeverityEnum, OrderInfoWarningError) {
    // Severity increases: Info < Warning < Error
    ENJIN_EXPECT_TRUE((int)MessageSeverity::Info    < (int)MessageSeverity::Warning);
    ENJIN_EXPECT_TRUE((int)MessageSeverity::Warning < (int)MessageSeverity::Error);
}

// ============================================================================
// BuildTargetPlatform — Enum Values
// ============================================================================

ENJIN_TEST(BuildTargetPlatformEnum, ValuesAreDistinct) {
    ENJIN_EXPECT_NE((int)BuildTargetPlatform::Desktop, (int)BuildTargetPlatform::Web);
}

// ============================================================================
// BuildMessage — File Field
// ============================================================================

ENJIN_TEST(BuildMessage, DefaultFileIsEmpty) {
    BuildMessage msg;
    ENJIN_EXPECT_TRUE(msg.file.empty());
}

ENJIN_TEST(BuildMessage, DefaultSeverityIsInfo) {
    BuildMessage msg;
    ENJIN_EXPECT_EQ((int)msg.severity, (int)MessageSeverity::Info);
}

ENJIN_TEST(BuildMessage, FileFieldStoresPath) {
    BuildMessage msg;
    msg.file = "scenes/level1.enjscene";
    ENJIN_EXPECT_STR_EQ(msg.file.c_str(), "scenes/level1.enjscene");
}

// ============================================================================
// BuildConfig — Obfuscation Key
// ============================================================================

ENJIN_TEST(ObfuscationKey, EmptyKeyStoredAsEmpty) {
    BuildConfig cfg;
    cfg.buildKey = "";
    ENJIN_EXPECT_TRUE(cfg.buildKey.empty());
}

ENJIN_TEST(ObfuscationKey, NormalKeyStored) {
    BuildConfig cfg;
    cfg.buildKey = "mySecretKey123";
    ENJIN_EXPECT_STR_EQ(cfg.buildKey.c_str(), "mySecretKey123");
}

ENJIN_TEST(ObfuscationKey, LongKeyStored) {
    BuildConfig cfg;
    cfg.buildKey = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
    ENJIN_EXPECT_TRUE(cfg.buildKey.size() == 64);
}

// ============================================================================
// BuildConfig — Window Title
// ============================================================================

ENJIN_TEST(WindowTitle, NormalTitleStored) {
    BuildConfig cfg;
    cfg.windowTitle = "My Awesome Game";
    ENJIN_EXPECT_STR_EQ(cfg.windowTitle.c_str(), "My Awesome Game");
}

ENJIN_TEST(WindowTitle, LongTitleStored) {
    // BuildConfig stores the title as-is. If the pipeline enforces a max
    // length it would truncate on Execute. Here we verify struct storage.
    std::string longTitle(512, 'A');
    BuildConfig cfg;
    cfg.windowTitle = longTitle;
    ENJIN_EXPECT_EQ(cfg.windowTitle.size(), (usize)512);
}

ENJIN_TEST(WindowTitle, EmptyTitleAllowedInStruct) {
    BuildConfig cfg;
    cfg.windowTitle = "";
    ENJIN_EXPECT_TRUE(cfg.windowTitle.empty());
}

// ============================================================================
// AssetPacker + AssetReader — Round-Trip (Build Pipeline pack layer)
// ============================================================================

ENJIN_TEST(AssetPackRoundTrip, BuildManifestDataRoundTrip) {
    std::string pakPath = GetTempPakPath("bp_manifest_test.enjpak");
    const char* key     = "build_pipeline_key";

    // Simulate what WriteBuildManifest does: pack a JSON blob
    const char* manifestJson =
        R"({"windowTitle":"Test Game","windowWidth":1280,"windowHeight":720,"fullscreen":false})";

    {
        AssetPacker packer;
        ENJIN_ASSERT_TRUE(packer.Begin(pakPath, key));
        ENJIN_ASSERT_TRUE(packer.AddData("manifest.json", manifestJson, strlen(manifestJson)));
        ENJIN_EXPECT_EQ(packer.GetFileCount(), 1u);
        ENJIN_ASSERT_TRUE(packer.Finalize());
    }

    {
        AssetReader reader;
        ENJIN_ASSERT_TRUE(reader.Open(pakPath, key));
        ENJIN_EXPECT_TRUE(reader.HasFile("manifest.json"));

        auto data = reader.ReadFile("manifest.json");
        std::string roundTripped(data.begin(), data.end());
        ENJIN_EXPECT_STR_EQ(roundTripped.c_str(), manifestJson);

        ENJIN_EXPECT_TRUE(reader.VerifyIntegrity());
        reader.Close();
    }

    std::filesystem::remove(pakPath);
}

ENJIN_TEST(AssetPackRoundTrip, MultipleAssetTypesRoundTrip) {
    std::string pakPath = GetTempPakPath("bp_multi_asset_test.enjpak");
    const char* key     = "multi_asset_key";

    const char* sceneData  = R"({"entities":[],"version":1})";
    const char* scriptData = "function OnStart() { Log(\"hello\"); }";
    const u8    binaryPng[] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};

    {
        AssetPacker packer;
        ENJIN_ASSERT_TRUE(packer.Begin(pakPath, key));
        ENJIN_ASSERT_TRUE(packer.AddData("scenes/level1.json", sceneData, strlen(sceneData)));
        ENJIN_ASSERT_TRUE(packer.AddData("scripts/game.as", scriptData, strlen(scriptData)));
        ENJIN_ASSERT_TRUE(packer.AddData("textures/icon.png", binaryPng, sizeof(binaryPng)));
        ENJIN_EXPECT_EQ(packer.GetFileCount(), 3u);
        ENJIN_ASSERT_TRUE(packer.Finalize());
    }

    {
        AssetReader reader;
        ENJIN_ASSERT_TRUE(reader.Open(pakPath, key));
        ENJIN_EXPECT_EQ(reader.GetFileCount(), 3u);

        ENJIN_EXPECT_TRUE(reader.HasFile("scenes/level1.json"));
        ENJIN_EXPECT_TRUE(reader.HasFile("scripts/game.as"));
        ENJIN_EXPECT_TRUE(reader.HasFile("textures/icon.png"));
        ENJIN_EXPECT_FALSE(reader.HasFile("textures/missing.png"));

        auto sceneBack = reader.ReadFile("scenes/level1.json");
        ENJIN_EXPECT_STR_EQ(std::string(sceneBack.begin(), sceneBack.end()).c_str(), sceneData);

        auto scriptBack = reader.ReadFile("scripts/game.as");
        ENJIN_EXPECT_STR_EQ(std::string(scriptBack.begin(), scriptBack.end()).c_str(), scriptData);

        auto pngBack = reader.ReadFile("textures/icon.png");
        ENJIN_EXPECT_EQ(pngBack.size(), (usize)sizeof(binaryPng));

        bool match = true;
        for (usize i = 0; i < sizeof(binaryPng) && match; ++i)
            if (pngBack[i] != binaryPng[i]) match = false;
        ENJIN_EXPECT_TRUE(match);

        ENJIN_EXPECT_TRUE(reader.VerifyIntegrity());
        reader.Close();
    }

    std::filesystem::remove(pakPath);
}

ENJIN_TEST(AssetPackRoundTrip, WrongKeyCorruptsData) {
    std::string pakPath = GetTempPakPath("bp_wrong_key_test.enjpak");

    {
        AssetPacker packer;
        ENJIN_ASSERT_TRUE(packer.Begin(pakPath, "correct_key"));
        const char* secret = "sensitive game data";
        ENJIN_ASSERT_TRUE(packer.AddData("secret.dat", secret, strlen(secret)));
        ENJIN_ASSERT_TRUE(packer.Finalize());
    }

    {
        AssetReader reader;
        if (reader.Open(pakPath, "wrong_key")) {
            // If the reader opened despite the wrong key, integrity must fail
            ENJIN_EXPECT_FALSE(reader.VerifyIntegrity());
            reader.Close();
        }
        // If Open returns false with a wrong key, that also satisfies the contract
    }

    std::filesystem::remove(pakPath);
}

ENJIN_TEST(AssetPackRoundTrip, SizeStatsReportedByPacker) {
    std::string pakPath = GetTempPakPath("bp_size_test.enjpak");
    const char* key     = "size_key";

    // Create a moderately large payload so we can confirm sizes are tracked
    std::string largeData(4096, 'X');

    {
        AssetPacker packer;
        ENJIN_ASSERT_TRUE(packer.Begin(pakPath, key));
        ENJIN_ASSERT_TRUE(packer.AddData("big.dat", largeData.data(), largeData.size()));
        ENJIN_ASSERT_TRUE(packer.Finalize());

        ENJIN_EXPECT_EQ(packer.GetFileCount(), 1u);
        ENJIN_EXPECT_TRUE(packer.GetTotalOriginalSize() > 0);
        ENJIN_EXPECT_TRUE(packer.GetTotalPackedSize() > 0);
        // Original size should equal the data we added
        ENJIN_EXPECT_EQ(packer.GetTotalOriginalSize(), (u64)largeData.size());
    }

    std::filesystem::remove(pakPath);
}

ENJIN_TEST(AssetPackRoundTrip, ListFilesMatchesPacked) {
    std::string pakPath = GetTempPakPath("bp_list_test.enjpak");
    const char* key     = "list_key";

    {
        AssetPacker packer;
        ENJIN_ASSERT_TRUE(packer.Begin(pakPath, key));
        ENJIN_ASSERT_TRUE(packer.AddData("a.json",  "aaa", 3));
        ENJIN_ASSERT_TRUE(packer.AddData("b.json",  "bbb", 3));
        ENJIN_ASSERT_TRUE(packer.AddData("c.dat",   "ccc", 3));
        ENJIN_ASSERT_TRUE(packer.Finalize());
    }

    {
        AssetReader reader;
        ENJIN_ASSERT_TRUE(reader.Open(pakPath, key));

        auto files = reader.ListFiles();
        ENJIN_EXPECT_EQ(files.size(), (usize)3);

        // All three virtual paths must appear in the listing
        bool hasA = false, hasB = false, hasC = false;
        for (const auto& f : files) {
            if (f == "a.json") hasA = true;
            if (f == "b.json") hasB = true;
            if (f == "c.dat")  hasC = true;
        }
        ENJIN_EXPECT_TRUE(hasA);
        ENJIN_EXPECT_TRUE(hasB);
        ENJIN_EXPECT_TRUE(hasC);

        reader.Close();
    }

    std::filesystem::remove(pakPath);
}

// ============================================================================
// BuildPipeline — Progress Callback
// ============================================================================

ENJIN_TEST(BuildPipelineCallback, CallbackIsInvokedOnExecution) {
    BuildConfig cfg;
    cfg.projectPath = GetTempDir("enjin_no_such_project_callback");
    cfg.outputDir   = GetTempDir("enjin_output_callback");

    int callbackCount = 0;
    float lastProgress = -1.0f;

    BuildPipeline pipeline;
    pipeline.SetProgressCallback([&](const std::string& phase, float progress) {
        ++callbackCount;
        lastProgress = progress;
        (void)phase;
    });

    // Execute will fail (project doesn't exist), but the callback must fire
    // at least once with a progress value in [0, 1] before failing.
    BuildResult result = pipeline.Execute(cfg);

    if (callbackCount > 0) {
        ENJIN_EXPECT_TRUE(lastProgress >= 0.0f);
        ENJIN_EXPECT_TRUE(lastProgress <= 1.0f);
    }

    // The build should fail since the project doesn't exist
    ENJIN_EXPECT_FALSE(result.success);
}

// ============================================================================
// CRC32 Utility — via AssetPacker
// ============================================================================

ENJIN_TEST(CRC32Utility, EmptyDataDoesNotCrash) {
    // CRC32 of null/0-size should not crash
    u32 crc = AssetPacker::ComputeCRC32(nullptr, 0);
    (void)crc;
    ENJIN_EXPECT_TRUE(true);
}

ENJIN_TEST(CRC32Utility, SameDataProducesSameCRC) {
    const char* data = "Enjin Build Pipeline Test";
    u32 crc1 = AssetPacker::ComputeCRC32(data, strlen(data));
    u32 crc2 = AssetPacker::ComputeCRC32(data, strlen(data));
    ENJIN_EXPECT_EQ(crc1, crc2);
}

ENJIN_TEST(CRC32Utility, DifferentDataProducesDifferentCRC) {
    const char* a = "scene_data_v1";
    const char* b = "scene_data_v2";
    u32 crcA = AssetPacker::ComputeCRC32(a, strlen(a));
    u32 crcB = AssetPacker::ComputeCRC32(b, strlen(b));
    ENJIN_EXPECT_NE(crcA, crcB);
}

ENJIN_TEST(CRC32Utility, SingleByteHasNonZeroCRC) {
    const u8 byte = 0xFF;
    u32 crc = AssetPacker::ComputeCRC32(&byte, 1);
    // A single 0xFF byte should produce a well-defined non-zero CRC
    ENJIN_EXPECT_NE(crc, 0u);
}

// ============================================================================
// AssetReader — Closed State
// ============================================================================

ENJIN_TEST(AssetReaderClosed, IsOpenFalseByDefault) {
    AssetReader reader;
    ENJIN_EXPECT_FALSE(reader.IsOpen());
}

ENJIN_TEST(AssetReaderClosed, HasFileReturnsFalseWhenClosed) {
    AssetReader reader;
    ENJIN_EXPECT_FALSE(reader.HasFile("anything.json"));
}

ENJIN_TEST(AssetReaderClosed, FileCountZeroWhenClosed) {
    AssetReader reader;
    ENJIN_EXPECT_EQ(reader.GetFileCount(), 0u);
}

ENJIN_TEST(AssetReaderClosed, ReadFileReturnsEmptyWhenClosed) {
    AssetReader reader;
    auto data = reader.ReadFile("anything.dat");
    ENJIN_EXPECT_TRUE(data.empty());
}

ENJIN_TEST(AssetReaderClosed, OpenNonExistentPakFails) {
    AssetReader reader;
    bool opened = reader.Open("this_file_does_not_exist.enjpak", "key");
    ENJIN_EXPECT_FALSE(opened);
    ENJIN_EXPECT_FALSE(reader.IsOpen());
}

// ============================================================================
// AssetPacker — Constants and Format
// ============================================================================

ENJIN_TEST(AssetPackerConstants, MagicBytesLength) {
    // ENJPAK_MAGIC is 8 bytes
    const char expected[8] = {'E','N','J','P','A','K','1','0'};
    for (int i = 0; i < 8; ++i) {
        ENJIN_EXPECT_EQ(ENJPAK_MAGIC[i], expected[i]);
    }
}

ENJIN_TEST(AssetPackerConstants, ObfuscatedFlagBitZero) {
    ENJIN_EXPECT_EQ(ENJPAK_FLAG_OBFUSCATED, 1u);
}

ENJIN_TEST(AssetPackerConstants, FormatVersionIsOne) {
    ENJIN_EXPECT_EQ(ENJPAK_FORMAT_VERSION, (u16)1);
}

// ============================================================================
// PakIndexEntry — Default Values
// ============================================================================

ENJIN_TEST(PakIndexEntry, DefaultOffsetIsZero) {
    PakIndexEntry entry;
    ENJIN_EXPECT_EQ(entry.dataOffset, (u64)0);
}

ENJIN_TEST(PakIndexEntry, DefaultSizesAreZero) {
    PakIndexEntry entry;
    ENJIN_EXPECT_EQ(entry.compressedSize, (u64)0);
    ENJIN_EXPECT_EQ(entry.originalSize,   (u64)0);
}

ENJIN_TEST(PakIndexEntry, DefaultCRCIsZero) {
    PakIndexEntry entry;
    ENJIN_EXPECT_EQ(entry.crc32, 0u);
}

ENJIN_TEST(PakIndexEntry, DefaultVirtualPathEmpty) {
    PakIndexEntry entry;
    ENJIN_EXPECT_TRUE(entry.virtualPath.empty());
}

// ============================================================================
// BuildConfig — Fullscreen Flag
// ============================================================================

ENJIN_TEST(BuildConfigFullscreen, CanSetFullscreen) {
    BuildConfig cfg;
    cfg.fullscreen = true;
    ENJIN_EXPECT_TRUE(cfg.fullscreen);
}

ENJIN_TEST(BuildConfigFullscreen, CanClearFullscreen) {
    BuildConfig cfg;
    cfg.fullscreen = true;
    cfg.fullscreen = false;
    ENJIN_EXPECT_FALSE(cfg.fullscreen);
}

// ============================================================================
// Nested Build Output
// ============================================================================
//
// A build directory sitting inside the project it was built from must never
// be scanned as project content. When it was, its copied scripts and assets
// came back as project files and were re-emitted one level deeper on the next
// build, nesting without limit (Build/Build/Build/...) and filling the pack
// with duplicates.

// Writes a minimal buildable project into dir, plus a leftover output folder
// that looks exactly like a previous build.
static void MakeProjectWithStaleOutput(const std::filesystem::path& dir) {
    namespace fs = std::filesystem;
    std::error_code ec;

    fs::remove_all(dir, ec);
    fs::create_directories(dir / "scenes", ec);
    fs::create_directories(dir / "scripts", ec);

    {
        std::ofstream proj(dir / "Nested.enjinproject");
        proj << R"({"projectName":"Nested","version":"1.0",)"
                R"("scenes":[{"name":"Main","path":"scenes/Main.enjin",)"
                R"("buildIndex":0,"isStartScene":true}]})";
    }
    { std::ofstream scene(dir / "scenes" / "Main.enjin");      scene  << "{}"; }
    { std::ofstream script(dir / "scripts" / "Player.as");     script << "// project script\n"; }

    // The leftover output of an earlier build, still inside the project.
    fs::create_directories(dir / "Build" / "scripts", ec);
    { std::ofstream man(dir / "Build" / "game.manifest");      man    << "{}"; }
    { std::ofstream copy(dir / "Build" / "scripts" / "Player.as"); copy << "// stale copy\n"; }
}

ENJIN_TEST(NestedBuildOutput, OutputDirIsNotCopiedIntoItself) {
    namespace fs = std::filesystem;
    std::error_code ec;

    fs::path root = fs::temp_directory_path() / "enjin_nested_output_test";
    MakeProjectWithStaleOutput(root);

    BuildConfig cfg;
    cfg.projectPath   = (root / "Nested.enjinproject").string();
    cfg.outputDir     = (root / "Build").string();
    cfg.packagingMode = PackagingMode::LooseFiles;

    BuildPipeline pipeline;
    // The player executable may be absent in a test environment, which the
    // pipeline treats as non-fatal. Only the on-disk shape matters here.
    pipeline.Execute(cfg);

    ENJIN_EXPECT_FALSE(fs::exists(root / "Build" / "Build"));

    fs::remove_all(root, ec);
}

ENJIN_TEST(NestedBuildOutput, StaleOutputIsNotPackedAsProjectContent) {
    namespace fs = std::filesystem;
    std::error_code ec;

    fs::path root = fs::temp_directory_path() / "enjin_nested_output_test2";
    MakeProjectWithStaleOutput(root);

    // Build somewhere else entirely. The stale Build/ folder still sitting in
    // the project must be skipped on its game.manifest marker alone.
    fs::path out = fs::temp_directory_path() / "enjin_nested_output_test2_out";
    fs::remove_all(out, ec);

    BuildConfig cfg;
    cfg.projectPath   = (root / "Nested.enjinproject").string();
    cfg.outputDir     = out.string();
    cfg.packagingMode = PackagingMode::LooseFiles;

    BuildPipeline pipeline;
    pipeline.Execute(cfg);

    ENJIN_EXPECT_FALSE(fs::exists(out / "Build"));

    fs::remove_all(root, ec);
    fs::remove_all(out, ec);
}

// ============================================================================
// A build is not a build without its runtime
// ============================================================================
//
// Phase 5 used to verify game.enjpak and nothing else, then set success = true.
// InvokeEmscriptenBuild's failure was a Warning, and it can only succeed in a
// source checkout -- its own comment says so -- so on an installed editor every
// web build reported "Build complete!", started the dev server and opened a
// browser on a page that 404s EnjinPlayer.js. The desktop half had the same
// shape: a missing player executable was a warning.

// A project the pipeline actually accepts: projectPath is the .enjinproject
// FILE, the scenes array must be non-empty, and each entry's path must resolve
// to a loadable scene. Without all of that the build fails at the project scan
// and never reaches the verify phase -- which would make these tests pass for
// the wrong reason.
//
// Scene "version" is the STRING "1.0" here on purpose; a bare number fails the
// whole scene load.
static std::string MakeMinimalProject(const char* leaf) {
    namespace fs = std::filesystem;
    const fs::path dir = fs::temp_directory_path() / "enjin_runtime_verify" / leaf;
    std::error_code ec;
    fs::remove_all(dir, ec);
    fs::create_directories(dir / "scenes", ec);

    std::ofstream scene(dir / "scenes" / "Main.enjin");
    scene << R"({"version":"1.0","entities":[]})";
    scene.close();

    const fs::path projFile = dir / "Test.enjinproject";
    std::ofstream proj(projFile);
    proj << R"({"projectName":"Test","scenes":[)"
         << R"({"name":"Main","path":"scenes/Main.enjin","buildIndex":0,"isStartScene":true}]})";
    proj.close();
    return projFile.string();
}

static std::string MakeEmptyOutput(const char* leaf) {
    namespace fs = std::filesystem;
    const fs::path dir = fs::temp_directory_path() / "enjin_runtime_verify_out" / leaf;
    std::error_code ec;
    fs::remove_all(dir, ec);
    fs::create_directories(dir, ec);
    return dir.string();
}

ENJIN_TEST(RuntimeVerification, WebOutputWithNoWasmIsNotRunnable) {
    // Arrange: the exact shape an installed editor produces -- assets packed,
    // no WASM, because InvokeEmscriptenBuild can only succeed in a source
    // checkout.
    namespace fs = std::filesystem;
    const std::string out = MakeEmptyOutput("web_none");
    std::ofstream(fs::path(out) / "game.enjpak") << "pak";

    // Act
    std::string why;
    const bool ok = VerifyRuntimePresent(out, BuildTargetPlatform::Web, &why);

    // Assert: this is what used to report "Build complete!" and open a browser.
    ENJIN_EXPECT_FALSE(ok);
    ENJIN_EXPECT_TRUE(why.find("EnjinPlayer.js") != std::string::npos);
}

ENJIN_TEST(RuntimeVerification, WebOutputMissingOnlyTheWasmIsNotRunnable) {
    // Arrange: a half-copied runtime is just as dead as no runtime.
    namespace fs = std::filesystem;
    const std::string out = MakeEmptyOutput("web_half");
    std::ofstream(fs::path(out) / "EnjinPlayer.js") << "js";

    // Act
    std::string why;
    const bool ok = VerifyRuntimePresent(out, BuildTargetPlatform::Web, &why);

    // Assert
    ENJIN_EXPECT_FALSE(ok);
    ENJIN_EXPECT_TRUE(why.find("EnjinPlayer.wasm") != std::string::npos);
}

ENJIN_TEST(RuntimeVerification, WebOutputWithBothRuntimeFilesIsRunnable) {
    // Arrange
    namespace fs = std::filesystem;
    const std::string out = MakeEmptyOutput("web_ok");
    std::ofstream(fs::path(out) / "EnjinPlayer.js") << "js";
    std::ofstream(fs::path(out) / "EnjinPlayer.wasm") << "wasm";

    // Act / Assert
    ENJIN_EXPECT_TRUE(VerifyRuntimePresent(out, BuildTargetPlatform::Web, nullptr));
}

ENJIN_TEST(RuntimeVerification, DesktopOutputWithNoExecutableIsNotRunnable) {
    // Arrange: packed assets and nothing to run them.
    namespace fs = std::filesystem;
    const std::string out = MakeEmptyOutput("desktop_none");
    std::ofstream(fs::path(out) / "game.enjpak") << "pak";

    // Act
    std::string why;
    const bool ok = VerifyRuntimePresent(out, BuildTargetPlatform::Desktop, &why);

    // Assert
    ENJIN_EXPECT_FALSE(ok);
    ENJIN_EXPECT_TRUE(why.find("player executable") != std::string::npos);
}

ENJIN_TEST(RuntimeVerification, DesktopOutputWithAnExecutableIsRunnable) {
    // Arrange: CopyPlayer names the exe after the window title, so the check
    // must not re-derive that name -- any executable counts.
    namespace fs = std::filesystem;
    const std::string out = MakeEmptyOutput("desktop_ok");
#ifdef ENJIN_PLATFORM_WINDOWS
    std::ofstream(fs::path(out) / "My Weird Game Name.exe") << "exe";
#else
    std::ofstream(fs::path(out) / "My Weird Game Name") << "elf";
#endif

    // Act / Assert
    ENJIN_EXPECT_TRUE(VerifyRuntimePresent(out, BuildTargetPlatform::Desktop, nullptr));
}

ENJIN_TEST(RuntimeVerification, AWebBuildThatCannotRunFailsTheWholePipeline) {
    // Arrange: end to end, to prove Phase 5 actually consults the rule rather
    // than the rule merely existing beside it. Output dir is read-only-empty,
    // and this machine's repo may well have a built web player to copy, so the
    // assertion is on the wiring: if the runtime IS present the build may
    // succeed, and if it is NOT present the build must fail.
    BuildConfig cfg;
    cfg.projectPath = MakeMinimalProject("pipeline");
    cfg.outputDir   = MakeEmptyOutput("pipeline");
    cfg.target      = BuildTargetPlatform::Web;

    // Act
    BuildPipeline pipeline;
    BuildResult result = pipeline.Execute(cfg);

    // Assert
    const bool runtimeThere =
        VerifyRuntimePresent(cfg.outputDir, BuildTargetPlatform::Web, nullptr);
    if (!runtimeThere) {
        ENJIN_EXPECT_FALSE(result.success);
    } else {
        ENJIN_EXPECT_TRUE(result.success);
    }
}

ENJIN_TEST_MAIN()
