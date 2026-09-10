// A Refresh button is a confession: the panel knows its data can go stale and has
// decided that is the user's problem, on a schedule the user has no way to know.
// Thirty editor panels shipped eight of them, because there was no shared way for
// a panel to learn that its source moved. These cover the two things that replace
// them -- a version counter on the SOURCE (so any number of views notice, not just
// the one that happens to be the sole mutator) and one throttled disk watch.
//
// Only the parts that do not touch a disk are tested here. The watch's stat loop
// is verified by running the editor against a project and editing a file under it
// from outside; that is an integration check, not a unit one.
#include "EnjinTest.h"
#include "Enjin/Editor/EditorWatch.h"
#include "Enjin/Assets/DataAsset.h"
#include <string>
#include <vector>

using namespace Enjin;

// ---------------------------------------------------------------------------
// The tree fingerprint
// ---------------------------------------------------------------------------

namespace {

// Fold a whole directory listing the way PollTree does.
u64 Fingerprint(const std::vector<std::pair<std::string, i64>>& entries) {
    u64 f = 0;
    for (const auto& [path, mtime] : entries) f = Editor::FoldWatchEntry(f, path, mtime);
    return f;
}

} // namespace

ENJIN_TEST(EditorWatch, IdenticalListingsFingerprintTheSame) {
    // Arrange / Act
    const std::vector<std::pair<std::string, i64>> listing = {
        {"data/letters/matriarch.enjdata", 1000},
        {"data/letters/steward.enjdata",   2000},
    };

    // Assert
    ENJIN_EXPECT_EQ(Fingerprint(listing), Fingerprint(listing));
}

ENJIN_TEST(EditorWatch, IterationOrderIsNotAChange) {
    // A directory iterator is free to return entries in a different order on a
    // different filesystem, and the same tree read twice must not look edited.
    const std::vector<std::pair<std::string, i64>> a = {
        {"a.enjdata", 1}, {"b.enjdata", 2}, {"c.enjdata", 3},
    };
    const std::vector<std::pair<std::string, i64>> b = {
        {"c.enjdata", 3}, {"a.enjdata", 1}, {"b.enjdata", 2},
    };
    ENJIN_EXPECT_EQ(Fingerprint(a), Fingerprint(b));
}

ENJIN_TEST(EditorWatch, AWriteChangesTheFingerprint) {
    const std::vector<std::pair<std::string, i64>> before = {
        {"a.enjdata", 1}, {"b.enjdata", 2},
    };
    const std::vector<std::pair<std::string, i64>> after = {
        {"a.enjdata", 1}, {"b.enjdata", 99},   // b was saved
    };
    ENJIN_EXPECT_TRUE(Fingerprint(before) != Fingerprint(after));
}

ENJIN_TEST(EditorWatch, AnAddedFileChangesTheFingerprint) {
    const std::vector<std::pair<std::string, i64>> before = {
        {"a.enjdata", 1},
    };
    const std::vector<std::pair<std::string, i64>> after = {
        {"a.enjdata", 1}, {"courier.enjdata", 5},
    };
    ENJIN_EXPECT_TRUE(Fingerprint(before) != Fingerprint(after));
}

ENJIN_TEST(EditorWatch, ARemovedFileChangesTheFingerprint) {
    const std::vector<std::pair<std::string, i64>> before = {
        {"a.enjdata", 1}, {"b.enjdata", 2},
    };
    const std::vector<std::pair<std::string, i64>> after = {
        {"a.enjdata", 1},
    };
    ENJIN_EXPECT_TRUE(Fingerprint(before) != Fingerprint(after));
}

ENJIN_TEST(EditorWatch, TwoFilesSwappingMTimesIsAChange) {
    // The obvious cheap fold -- sum the mtimes -- calls this pair identical.
    // Mixing the path into each entry before folding is what stops that.
    const std::vector<std::pair<std::string, i64>> before = {
        {"a.enjdata", 1}, {"b.enjdata", 2},
    };
    const std::vector<std::pair<std::string, i64>> after = {
        {"a.enjdata", 2}, {"b.enjdata", 1},
    };
    ENJIN_EXPECT_TRUE(Fingerprint(before) != Fingerprint(after));
}

ENJIN_TEST(EditorWatch, AnEmptyTreeFingerprintsToZero) {
    ENJIN_EXPECT_EQ(Fingerprint({}), static_cast<u64>(0));
}

// ---------------------------------------------------------------------------
// The version counter on the source
// ---------------------------------------------------------------------------

ENJIN_TEST(EditorWatch, RegistryVersionMovesOnEveryMutation) {
    // Arrange: the registry is a singleton, so start from whatever it holds and
    // measure deltas rather than absolute values.
    auto& registry = Assets::DataAssetRegistry::Get();
    registry.Clear();
    const u64 start = registry.Version();

    // Act / Assert: each mutation is one observable step.
    Assets::DataAssetSchema schema;
    schema.name = "Dictation";
    registry.RegisterSchema(schema);
    const u64 afterSchema = registry.Version();
    ENJIN_EXPECT_TRUE(afterSchema > start);

    Assets::DataAsset asset;
    asset.name = "matriarch";
    asset.schemaName = "Dictation";
    asset.values["seconds"] = 42.5f;
    registry.CreateAsset(asset);
    const u64 afterCreate = registry.Version();
    ENJIN_EXPECT_TRUE(afterCreate > afterSchema);

    registry.SetFloat("matriarch", "seconds", 8.0f);
    const u64 afterWrite = registry.Version();
    ENJIN_EXPECT_TRUE(afterWrite > afterCreate);

    registry.RemoveAsset("matriarch");
    ENJIN_EXPECT_TRUE(registry.Version() > afterWrite);

    registry.Clear();
}

ENJIN_TEST(EditorWatch, RegistryVersionHoldsStillWhileNothingChanges) {
    // The whole point of a counter over a dirty flag: a reader that has caught up
    // must stay caught up across any number of pure reads, from any number of
    // readers. A flag owned by one viewer cannot promise that.
    auto& registry = Assets::DataAssetRegistry::Get();
    registry.Clear();
    Assets::DataAsset asset;
    asset.name = "steward";
    registry.CreateAsset(asset);

    const u64 seen = registry.Version();
    registry.FindAsset("steward");
    registry.GetAllAssets();
    registry.GetAllSchemas();
    registry.GetFloat("steward", "seconds", 0.0f);
    registry.GetAssetsBySchema("Dictation");
    ENJIN_EXPECT_EQ(registry.Version(), seen);

    // And a write nobody asked for -- setting a field on an asset that does not
    // exist -- must not move it either, or every reader rebuilds for nothing.
    registry.SetFloat("nobody", "seconds", 1.0f);
    ENJIN_EXPECT_EQ(registry.Version(), seen);

    registry.Clear();
}

// ---------------------------------------------------------------------------
// The data format
// ---------------------------------------------------------------------------

ENJIN_TEST(EditorWatch, AHandWrittenRecordLoads) {
    // .enjdata exists so data can be authored outside code, so it has to read
    // what a person writes. The canonical form is tagged --
    // {"type":"Float","value":42.5} -- and the untagged form used to THROW out of
    // the value reader, which is caught around the whole file: one bare number
    // took every record in that file with it. (The player's own parser, dead
    // since it shipped, read only the untagged form, so the two halves of the
    // engine disagreed about the format as well.)
    auto& registry = Assets::DataAssetRegistry::Get();
    registry.Clear();

    const std::string bare =
        R"({"name":"matriarch","schema":"Dictation","values":{)"
        R"("speaker":"matriarch","seconds":42.5,"takes":3,"final":true}})";
    ENJIN_ASSERT_TRUE(registry.LoadAssetFromString(bare, "letters/matriarch.enjdata"));

    ENJIN_EXPECT_EQ(registry.GetString("matriarch", "speaker"), std::string("matriarch"));
    ENJIN_EXPECT_TRUE(registry.GetFloat("matriarch", "seconds") > 42.4f);
    ENJIN_EXPECT_EQ(registry.GetInt("matriarch", "takes"), 3);
    ENJIN_EXPECT_TRUE(registry.GetBool("matriarch", "final"));

    registry.Clear();
}

ENJIN_TEST(EditorWatch, ATaggedRecordStillLoads) {
    auto& registry = Assets::DataAssetRegistry::Get();
    registry.Clear();

    const std::string tagged =
        R"({"name":"steward","schema":"Dictation","values":{)"
        R"("speaker":{"type":"String","value":"steward"},)"
        R"("seconds":{"type":"Float","value":8.0}}})";
    ENJIN_ASSERT_TRUE(registry.LoadAssetFromString(tagged, "letters/steward.enjdata"));

    ENJIN_EXPECT_EQ(registry.GetString("steward", "speaker"), std::string("steward"));
    ENJIN_EXPECT_TRUE(registry.GetFloat("steward", "seconds") > 7.9f);

    registry.Clear();
}

ENJIN_TEST(EditorWatch, OneBadFieldDoesNotTakeTheWholeRecord) {
    auto& registry = Assets::DataAssetRegistry::Get();
    registry.Clear();

    // A null in the middle of an otherwise good record.
    const std::string mixed =
        R"({"name":"courier","schema":"Dictation","values":{)"
        R"("speaker":"courier","notes":null,"seconds":8.0}})";
    ENJIN_ASSERT_TRUE(registry.LoadAssetFromString(mixed, "letters/courier.enjdata"));
    ENJIN_EXPECT_EQ(registry.GetString("courier", "speaker"), std::string("courier"));
    ENJIN_EXPECT_TRUE(registry.GetFloat("courier", "seconds") > 7.9f);

    registry.Clear();
}

ENJIN_TEST(EditorWatch, ARecordWithNoNameIsRefusedAndSaysSo) {
    auto& registry = Assets::DataAssetRegistry::Get();
    registry.Clear();
    const u64 before = registry.Version();
    ENJIN_EXPECT_FALSE(registry.LoadAssetFromString(R"({"schema":"Dictation"})", "broken.enjdata"));
    ENJIN_EXPECT_EQ(registry.Version(), before);   // and it did not half-create one
}

ENJIN_TEST_MAIN()
