// A schema type name that does not parse must not become String in silence.
//
// BUG-0001. DataFieldTypeFromString matched eight exact, case-sensitive names
// and ended `return DataFieldType::String`, so every typo, every case error and
// every invented name silently became String -- and there was no way for a
// caller to tell a field DECLARED String from one that failed to parse, because
// the information was gone before anything downstream could report it. The
// scalar getters then failed their holds_alternative check and returned the
// fallback, also in silence.
//
// Lowercase "int" is what a person writes by hand. Three shipped schemas had 42
// such fields, so DataAsset_GetInt returned 0 on every document in that game,
// every frame, with nothing in the log.
//
// The expensive part was not the zeros. Somebody hit this, reasonably guessed
// that arrays were being dropped by the Player's loader, and wrote the guess
// down -- in the engine's own shipped API text, from where it propagated
// outward as a delimiter convention and a hand-rolled Split built to route
// around a bug that never existed. The last test here is the one that disproves
// the folklore, so nobody has to take a comment's word for it again.

#include "EnjinTest.h"
#include "Enjin/Assets/DataAsset.h"

#include <cstdio>
#include <string>

using namespace Enjin;
using namespace Enjin::Assets;

ENJIN_TEST(SchemaTypeNames, CanonicalNamesParseExactly) {
    // Arrange / Act / Assert
    DataFieldTypeParse how = DataFieldTypeParse::Unknown;

    ENJIN_EXPECT_TRUE(DataFieldTypeFromString("Int", &how) == DataFieldType::Int);
    ENJIN_EXPECT_TRUE(how == DataFieldTypeParse::Exact);

    ENJIN_EXPECT_TRUE(DataFieldTypeFromString("StringArray", &how) == DataFieldType::StringArray);
    ENJIN_EXPECT_TRUE(how == DataFieldTypeParse::Exact);

    ENJIN_EXPECT_TRUE(DataFieldTypeFromString("FloatArray", &how) == DataFieldType::FloatArray);
    ENJIN_EXPECT_TRUE(how == DataFieldTypeParse::Exact);
}

ENJIN_TEST(SchemaTypeNames, LowercaseIsTakenAndReported) {
    // Arrange / Act: what somebody actually types into hand-written JSON.
    DataFieldTypeParse how = DataFieldTypeParse::Exact;
    const DataFieldType t = DataFieldTypeFromString("int", &how);

    // Assert: it does what it says, AND the author finds out once rather than
    // discovering a zero three weeks later. Both halves matter; taking it
    // silently would just be a friendlier flavour of the original bug.
    ENJIN_EXPECT_TRUE(t == DataFieldType::Int);
    ENJIN_EXPECT_TRUE(how == DataFieldTypeParse::CaseFixed);

    ENJIN_EXPECT_TRUE(DataFieldTypeFromString("bool", &how) == DataFieldType::Bool);
    ENJIN_EXPECT_TRUE(how == DataFieldTypeParse::CaseFixed);
    ENJIN_EXPECT_TRUE(DataFieldTypeFromString("stringarray", &how) == DataFieldType::StringArray);
    ENJIN_EXPECT_TRUE(how == DataFieldTypeParse::CaseFixed);
}

ENJIN_TEST(SchemaTypeNames, AnInventedTypeNameIsReportedRatherThanAssumed) {
    // Arrange / Act
    DataFieldTypeParse how = DataFieldTypeParse::Exact;
    const DataFieldType t = DataFieldTypeFromString("integer", &how);

    // Assert: still String, because changing the fallback would break every
    // schema that has been relying on it. What changed is that the caller now
    // knows, and the schema loader turns that into a line naming the schema,
    // the field and the bad string.
    ENJIN_EXPECT_TRUE(t == DataFieldType::String);
    ENJIN_EXPECT_TRUE(how == DataFieldTypeParse::Unknown);

    ENJIN_EXPECT_TRUE(DataFieldTypeFromString("", &how) == DataFieldType::String);
    ENJIN_EXPECT_TRUE(how == DataFieldTypeParse::Unknown);
    ENJIN_EXPECT_TRUE(DataFieldTypeFromString("Strng", &how) == DataFieldType::String);
    ENJIN_EXPECT_TRUE(how == DataFieldTypeParse::Unknown);
}

ENJIN_TEST(SchemaTypeNames, ASchemaWrittenInLowercaseNowReadsAsNumbers) {
    // Arrange: the exact shape that was broken -- a hand-written schema and a
    // matching asset.
    //
    // The registry is a singleton, so each test clears it rather than making
    // its own. Shared mutable state between tests is a trap, and Clear() is
    // what stops one test's assets answering another test's questions.
    DataAssetRegistry& registry = DataAssetRegistry::Get();
    registry.Clear();

    const std::string schema = R"({
        "name": "DocumentTemplate",
        "fields": [
            {"name": "type", "type": "int"},
            {"name": "supervision", "type": "int"},
            {"name": "title", "type": "string"}
        ]
    })";
    ENJIN_ASSERT_TRUE(registry.LoadSchemaFromString(schema, "document.enjschema"));

    const std::string asset = R"({
        "name": "memo_01",
        "schema": "DocumentTemplate",
        "values": {
            "type": {"type": "Int", "value": 3},
            "supervision": {"type": "Int", "value": 2},
            "title": {"type": "String", "value": "Memo"}
        }
    })";
    ENJIN_ASSERT_TRUE(registry.LoadAssetFromString(asset, "memo_01.enjdata"));

    // Act
    const i32 type = registry.GetInt("memo_01", "type", -1);
    const i32 supervision = registry.GetInt("memo_01", "supervision", -1);

    // Assert: 3 and 2, not 0. This read 0 on every document in a shipped game.
    std::printf("    type %d, supervision %d\n", type, supervision);
    ENJIN_EXPECT_EQ(type, 3);
    ENJIN_EXPECT_EQ(supervision, 2);
    ENJIN_EXPECT_TRUE(registry.GetString("memo_01", "title", "") == "Memo");
}

// What the schema type name does NOT do.
//
// BUG-0001 claimed that three shipped schemas declaring "int" made
// DataAsset_GetInt return 0 on every document in that game. That part is not
// true, and it is worth pinning down rather than leaving as a believed fact.
//
// Asset values are deserialised from their OWN JSON type, not from the schema:
// a bare 3 in an .enjdata becomes an i32 whatever the schema says, and GetInt
// finds it. The schema field type sets the DEFAULT value for a field and drives
// the editor UI; it does not coerce values on load. So Ink_Ribbon's documents
// read their real numbers today and read them before this fix too.
//
// The parse bug is still real and still worth fixing -- an unrecognised type
// name silently becoming String is a wrong answer given confidently, and it
// misleads the editor and anything that inspects a schema. But the blast radius
// was smaller than reported, and a fix that claims to have repaired gameplay
// values it never broke is its own kind of false premise.
ENJIN_TEST(SchemaTypeNames, ValuesTakeTheirTypeFromTheDataNotTheSchema) {
    // Arrange: exactly the Ink_Ribbon shape -- a lowercase schema, and an asset
    // whose values are bare JSON rather than tagged objects.
    DataAssetRegistry& registry = DataAssetRegistry::Get();
    registry.Clear();

    const std::string schema = R"({
        "name": "DocumentTemplate",
        "fields": [
            {"name": "type", "type": "int"},
            {"name": "supervision", "type": "int"},
            {"name": "text", "type": "string"}
        ]
    })";
    ENJIN_ASSERT_TRUE(registry.LoadSchemaFromString(schema, "document.enjschema"));

    const std::string asset = R"({
        "name": "day3_aguien1",
        "schema": "DocumentTemplate",
        "values": { "type": 2, "supervision": 3, "text": "Dictation follows." }
    })";
    ENJIN_ASSERT_TRUE(registry.LoadAssetFromString(asset, "day3_aguien1.enjdata"));

    // Act / Assert: the real values, from the JSON types, regardless of the
    // schema spelling.
    std::printf("    bare-value asset: type %d, supervision %d\n",
                registry.GetInt("day3_aguien1", "type", -1),
                registry.GetInt("day3_aguien1", "supervision", -1));
    ENJIN_EXPECT_EQ(registry.GetInt("day3_aguien1", "type", -1), 2);
    ENJIN_EXPECT_EQ(registry.GetInt("day3_aguien1", "supervision", -1), 3);
    ENJIN_EXPECT_TRUE(registry.GetString("day3_aguien1", "text", "") == "Dictation follows.");
}

ENJIN_TEST(SchemaTypeNames, ArraysSurviveARoundTripWhichIsTheFolkloreDisproved) {
    // Arrange: the claim written into the engine's own API text was that the
    // Player's .enjdata loader understands only string / float / int / bool and
    // silently DROPS StringArray and FloatArray, so any list that must survive
    // a build has to travel as a joined string.
    //
    // There is one parser. The editor and the Player both call
    // LoadAssetFromString; BuildPipeline packs .enjdata as a byte copy without
    // looking inside. So a round trip through serialise and deserialise is the
    // whole question, and it is answerable here rather than by reading a
    // comment.
    DataAssetRegistry& registry = DataAssetRegistry::Get();
    registry.Clear();

    const std::string schema = R"({
        "name": "Conversation",
        "fields": [
            {"name": "beats", "type": "StringArray"},
            {"name": "timings", "type": "FloatArray"}
        ]
    })";
    ENJIN_ASSERT_TRUE(registry.LoadSchemaFromString(schema, "conversation.enjschema"));

    const std::string asset = R"({
        "name": "first_evening",
        "schema": "Conversation",
        "values": {
            "beats": {"type": "StringArray", "value": ["hello", "goodbye", "wait"]},
            "timings": {"type": "FloatArray", "value": [0.5, 1.25, 2.0]}
        }
    })";
    ENJIN_ASSERT_TRUE(registry.LoadAssetFromString(asset, "first_evening.enjdata"));

    // Act
    const usize beatCount = registry.GetArrayLength("first_evening", "beats");
    const usize timingCount = registry.GetArrayLength("first_evening", "timings");

    // Assert
    std::printf("    %zu beats, %zu timings survived\n", beatCount, timingCount);
    ENJIN_EXPECT_EQ(beatCount, usize{3});
    ENJIN_EXPECT_EQ(timingCount, usize{3});
    ENJIN_EXPECT_TRUE(registry.GetStringAt("first_evening", "beats", 1) == "goodbye");
    ENJIN_EXPECT_FLOAT_NEAR(registry.GetFloatAt("first_evening", "timings", 2), 2.0f, 1.0e-5f);

    // And the same bytes loaded a second time, which is exactly what surviving
    // a build means here. BuildPipeline packs .enjdata with packer.AddFile, a
    // byte copy that never parses, and the Player then calls the same
    // LoadAssetFromString the editor does. There is no second loader to drop
    // anything, so if the arrays arrive once they arrive every time.
    registry.Clear();
    ENJIN_ASSERT_TRUE(registry.LoadSchemaFromString(schema, "conversation.enjschema"));
    ENJIN_ASSERT_TRUE(registry.LoadAssetFromString(asset, "first_evening.enjdata"));
    ENJIN_EXPECT_EQ(registry.GetArrayLength("first_evening", "beats"), usize{3});
    ENJIN_EXPECT_TRUE(registry.GetStringAt("first_evening", "beats", 2) == "wait");
}

ENJIN_TEST_MAIN()
