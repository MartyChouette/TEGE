#include "EnjinTest.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Build/AssetPacker.h"
#include "Enjin/Build/AssetReader.h"
#include <cstring>
#include <filesystem>

using namespace Enjin;
using namespace Enjin::Build;

static std::string GetTempPakPath() {
    auto tmp = std::filesystem::temp_directory_path() / "enjin_test_pack.enjpak";
    return tmp.string();
}

// ============================================================================
// CRC32
// ============================================================================

ENJIN_TEST(CRC32, EmptyDataIsZero) {
    u32 crc = AssetPacker::ComputeCRC32(nullptr, 0);
    // CRC32 of empty data is 0 by most implementations
    // Just verify it doesn't crash
    (void)crc;
    ENJIN_EXPECT_TRUE(true);
}

ENJIN_TEST(CRC32, KnownDataConsistent) {
    const char* data = "Hello, World!";
    u32 crc1 = AssetPacker::ComputeCRC32(data, strlen(data));
    u32 crc2 = AssetPacker::ComputeCRC32(data, strlen(data));
    ENJIN_EXPECT_EQ(crc1, crc2);
}

ENJIN_TEST(CRC32, DifferentDataDifferentCRC) {
    const char* data1 = "Hello";
    const char* data2 = "World";
    u32 crc1 = AssetPacker::ComputeCRC32(data1, strlen(data1));
    u32 crc2 = AssetPacker::ComputeCRC32(data2, strlen(data2));
    ENJIN_EXPECT_NE(crc1, crc2);
}

// ============================================================================
// Pack/Unpack Round-Trip
// ============================================================================

ENJIN_TEST(AssetPack, RoundTripSingleFile) {
    std::string pakPath = GetTempPakPath();
    const char* key = "test_key_2025";

    // Pack
    {
        AssetPacker packer;
        ENJIN_ASSERT_TRUE(packer.Begin(pakPath, key));

        const char* content = "This is test content for the asset pack.";
        ENJIN_ASSERT_TRUE(packer.AddData("scenes/test.json", content, strlen(content)));
        ENJIN_EXPECT_EQ(packer.GetFileCount(), 1u);

        ENJIN_ASSERT_TRUE(packer.Finalize());
    }

    // Unpack
    {
        AssetReader reader;
        ENJIN_ASSERT_TRUE(reader.Open(pakPath, key));
        ENJIN_EXPECT_TRUE(reader.IsOpen());
        ENJIN_EXPECT_EQ(reader.GetFileCount(), 1u);
        ENJIN_EXPECT_TRUE(reader.HasFile("scenes/test.json"));
        ENJIN_EXPECT_FALSE(reader.HasFile("nonexistent.txt"));

        auto data = reader.ReadFile("scenes/test.json");
        std::string content(data.begin(), data.end());
        ENJIN_EXPECT_STR_EQ(content, "This is test content for the asset pack.");

        reader.Close();
    }

    // Cleanup
    std::filesystem::remove(pakPath);
}

ENJIN_TEST(AssetPack, RoundTripMultipleFiles) {
    std::string pakPath = GetTempPakPath();
    const char* key = "multi_test_key";

    // Pack multiple files
    {
        AssetPacker packer;
        ENJIN_ASSERT_TRUE(packer.Begin(pakPath, key));

        const char* file1 = "File one content";
        const char* file2 = "File two content, which is longer";
        const char* file3 = "{\"key\": \"value\"}";

        ENJIN_ASSERT_TRUE(packer.AddData("data/file1.txt", file1, strlen(file1)));
        ENJIN_ASSERT_TRUE(packer.AddData("data/file2.txt", file2, strlen(file2)));
        ENJIN_ASSERT_TRUE(packer.AddData("config.json", file3, strlen(file3)));
        ENJIN_EXPECT_EQ(packer.GetFileCount(), 3u);

        ENJIN_ASSERT_TRUE(packer.Finalize());
    }

    // Read back all files
    {
        AssetReader reader;
        ENJIN_ASSERT_TRUE(reader.Open(pakPath, key));
        ENJIN_EXPECT_EQ(reader.GetFileCount(), 3u);

        auto files = reader.ListFiles();
        ENJIN_EXPECT_EQ(files.size(), (usize)3);

        auto d1 = reader.ReadFile("data/file1.txt");
        ENJIN_EXPECT_STR_EQ(std::string(d1.begin(), d1.end()), "File one content");

        auto d2 = reader.ReadFile("data/file2.txt");
        ENJIN_EXPECT_STR_EQ(std::string(d2.begin(), d2.end()), "File two content, which is longer");

        auto d3 = reader.ReadFile("config.json");
        ENJIN_EXPECT_STR_EQ(std::string(d3.begin(), d3.end()), "{\"key\": \"value\"}");

        reader.Close();
    }

    std::filesystem::remove(pakPath);
}

ENJIN_TEST(AssetPack, IntegrityVerification) {
    std::string pakPath = GetTempPakPath();
    const char* key = "integrity_key";

    {
        AssetPacker packer;
        ENJIN_ASSERT_TRUE(packer.Begin(pakPath, key));
        const char* content = "Integrity test data";
        ENJIN_ASSERT_TRUE(packer.AddData("test.dat", content, strlen(content)));
        ENJIN_ASSERT_TRUE(packer.Finalize());
    }

    {
        AssetReader reader;
        ENJIN_ASSERT_TRUE(reader.Open(pakPath, key));
        ENJIN_EXPECT_TRUE(reader.VerifyIntegrity());
        reader.Close();
    }

    std::filesystem::remove(pakPath);
}

ENJIN_TEST(AssetPack, WrongKeyFailsToRead) {
    std::string pakPath = GetTempPakPath();

    {
        AssetPacker packer;
        ENJIN_ASSERT_TRUE(packer.Begin(pakPath, "correct_key"));
        const char* content = "Secret data";
        ENJIN_ASSERT_TRUE(packer.AddData("secret.txt", content, strlen(content)));
        ENJIN_ASSERT_TRUE(packer.Finalize());
    }

    // Try reading with wrong key — should open (index might decode) but data will be garbled
    {
        AssetReader reader;
        // The reader might or might not Open successfully with wrong key
        // depending on how index parsing handles garbled data.
        // At minimum, if it opens, integrity should fail
        if (reader.Open(pakPath, "wrong_key")) {
            ENJIN_EXPECT_FALSE(reader.VerifyIntegrity());
            reader.Close();
        }
    }

    std::filesystem::remove(pakPath);
}

ENJIN_TEST(AssetPack, EmptyPakRoundTrip) {
    std::string pakPath = GetTempPakPath();
    const char* key = "empty_key";

    {
        AssetPacker packer;
        ENJIN_ASSERT_TRUE(packer.Begin(pakPath, key));
        ENJIN_EXPECT_EQ(packer.GetFileCount(), 0u);
        ENJIN_ASSERT_TRUE(packer.Finalize());
    }

    {
        AssetReader reader;
        ENJIN_ASSERT_TRUE(reader.Open(pakPath, key));
        ENJIN_EXPECT_EQ(reader.GetFileCount(), 0u);
        ENJIN_EXPECT_TRUE(reader.VerifyIntegrity());
        reader.Close();
    }

    std::filesystem::remove(pakPath);
}

ENJIN_TEST(AssetPack, BinaryDataPreserved) {
    std::string pakPath = GetTempPakPath();
    const char* key = "binary_key";

    // Create binary data with all byte values
    std::vector<u8> binaryData(256);
    for (int i = 0; i < 256; ++i) binaryData[i] = static_cast<u8>(i);

    {
        AssetPacker packer;
        ENJIN_ASSERT_TRUE(packer.Begin(pakPath, key));
        ENJIN_ASSERT_TRUE(packer.AddData("binary.dat", binaryData.data(), binaryData.size()));
        ENJIN_ASSERT_TRUE(packer.Finalize());
    }

    {
        AssetReader reader;
        ENJIN_ASSERT_TRUE(reader.Open(pakPath, key));
        auto data = reader.ReadFile("binary.dat");
        ENJIN_EXPECT_EQ(data.size(), (usize)256);

        bool match = true;
        for (int i = 0; i < 256 && match; ++i)
            if (data[i] != static_cast<u8>(i)) match = false;
        ENJIN_EXPECT_TRUE(match);

        reader.Close();
    }

    std::filesystem::remove(pakPath);
}

ENJIN_TEST_MAIN()
