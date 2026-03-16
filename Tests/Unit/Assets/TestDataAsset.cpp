#include "EnjinTest.h"
#include "Enjin/Assets/DataAsset.h"

#include <cstdio>
#include <string>
#include <vector>

using namespace Enjin;
using namespace Enjin::Assets;

// Helper: clear singleton state before each test group
static void ResetRegistry() {
    DataAssetRegistry::Get().Clear();
}

static DataAssetSchema MakeTestSchema() {
    DataAssetSchema schema;
    schema.name = "TestSchema";
    schema.description = "Test schema";
    schema.fields = {
        { "health",    DataFieldType::Float,   100.0f },
        { "name",      DataFieldType::String,  std::string("Unknown") },
        { "alive",     DataFieldType::Bool,    true },
        { "level",     DataFieldType::Int,     i32(1) },
        { "position",  DataFieldType::Vector3, Math::Vector3(0, 0, 0) }
    };
    return schema;
}

// ===========================================================================
// Schema Registration
// ===========================================================================

ENJIN_TEST(Schema, RegisterAndFind) {
    ResetRegistry();
    auto& reg = DataAssetRegistry::Get();
    reg.RegisterSchema(MakeTestSchema());
    const DataAssetSchema* found = reg.FindSchema("TestSchema");
    ENJIN_EXPECT_NOT_NULL(found);
    ENJIN_EXPECT_STR_EQ(found->name.c_str(), "TestSchema");
}

ENJIN_TEST(Schema, FindMissingReturnsNull) {
    ResetRegistry();
    ENJIN_EXPECT_TRUE(DataAssetRegistry::Get().FindSchema("Nonexistent") == nullptr);
}

ENJIN_TEST(Schema, RemoveSchema) {
    ResetRegistry();
    auto& reg = DataAssetRegistry::Get();
    reg.RegisterSchema(MakeTestSchema());
    reg.RemoveSchema("TestSchema");
    ENJIN_EXPECT_TRUE(reg.FindSchema("TestSchema") == nullptr);
}

ENJIN_TEST(Schema, GetAllSchemas) {
    ResetRegistry();
    auto& reg = DataAssetRegistry::Get();
    DataAssetSchema s1 = MakeTestSchema();
    DataAssetSchema s2;
    s2.name = "Other";
    s2.fields = { { "value", DataFieldType::Float, 0.0f } };
    reg.RegisterSchema(s1);
    reg.RegisterSchema(s2);
    auto all = reg.GetAllSchemas();
    ENJIN_EXPECT_EQ(all.size(), (size_t)2);
}

ENJIN_TEST(Schema, FieldCount) {
    ResetRegistry();
    auto& reg = DataAssetRegistry::Get();
    reg.RegisterSchema(MakeTestSchema());
    const DataAssetSchema* s = reg.FindSchema("TestSchema");
    ENJIN_ASSERT_NOT_NULL(s);
    ENJIN_EXPECT_EQ(s->fields.size(), (size_t)5);
}

// ===========================================================================
// Asset CRUD
// ===========================================================================

ENJIN_TEST(Asset, CreateAndFind) {
    ResetRegistry();
    auto& reg = DataAssetRegistry::Get();
    reg.RegisterSchema(MakeTestSchema());

    DataAsset asset;
    asset.name = "Player";
    asset.schemaName = "TestSchema";
    asset.values["health"] = 75.0f;
    asset.values["name"] = std::string("Hero");
    reg.CreateAsset(asset);

    const DataAsset* found = reg.FindAsset("Player");
    ENJIN_EXPECT_NOT_NULL(found);
    ENJIN_EXPECT_STR_EQ(found->name.c_str(), "Player");
}

ENJIN_TEST(Asset, FindMissingReturnsNull) {
    ResetRegistry();
    ENJIN_EXPECT_TRUE(DataAssetRegistry::Get().FindAsset("Ghost") == nullptr);
}

ENJIN_TEST(Asset, RemoveAsset) {
    ResetRegistry();
    auto& reg = DataAssetRegistry::Get();
    DataAsset asset;
    asset.name = "Temp";
    asset.schemaName = "test";
    reg.CreateAsset(asset);
    reg.RemoveAsset("Temp");
    ENJIN_EXPECT_TRUE(reg.FindAsset("Temp") == nullptr);
}

ENJIN_TEST(Asset, GetAssetsBySchema) {
    ResetRegistry();
    auto& reg = DataAssetRegistry::Get();
    reg.RegisterSchema(MakeTestSchema());

    DataAsset a1; a1.name = "A"; a1.schemaName = "TestSchema";
    DataAsset a2; a2.name = "B"; a2.schemaName = "TestSchema";
    DataAsset a3; a3.name = "C"; a3.schemaName = "Other";
    reg.CreateAsset(a1);
    reg.CreateAsset(a2);
    reg.CreateAsset(a3);

    auto testAssets = reg.GetAssetsBySchema("TestSchema");
    ENJIN_EXPECT_EQ(testAssets.size(), (size_t)2);
}

ENJIN_TEST(Asset, GetAllAssets) {
    ResetRegistry();
    auto& reg = DataAssetRegistry::Get();
    DataAsset a1; a1.name = "X"; a1.schemaName = "S";
    DataAsset a2; a2.name = "Y"; a2.schemaName = "S";
    reg.CreateAsset(a1);
    reg.CreateAsset(a2);
    ENJIN_EXPECT_EQ(reg.GetAllAssets().size(), (size_t)2);
}

// ===========================================================================
// Typed Getters & Setters
// ===========================================================================

ENJIN_TEST(TypedAccess, GetFloatReturnsValue) {
    ResetRegistry();
    auto& reg = DataAssetRegistry::Get();
    DataAsset asset;
    asset.name = "Test";
    asset.values["hp"] = 42.5f;
    reg.CreateAsset(asset);
    ENJIN_EXPECT_FLOAT_EQ(reg.GetFloat("Test", "hp"), 42.5f);
}

ENJIN_TEST(TypedAccess, GetFloatFallbackOnMissing) {
    ResetRegistry();
    auto& reg = DataAssetRegistry::Get();
    ENJIN_EXPECT_FLOAT_EQ(reg.GetFloat("NoAsset", "field", 99.0f), 99.0f);
}

ENJIN_TEST(TypedAccess, GetIntReturnsValue) {
    ResetRegistry();
    auto& reg = DataAssetRegistry::Get();
    DataAsset asset;
    asset.name = "Test";
    asset.values["level"] = i32(5);
    reg.CreateAsset(asset);
    ENJIN_EXPECT_EQ(reg.GetInt("Test", "level"), 5);
}

ENJIN_TEST(TypedAccess, GetBoolReturnsValue) {
    ResetRegistry();
    auto& reg = DataAssetRegistry::Get();
    DataAsset asset;
    asset.name = "Test";
    asset.values["active"] = true;
    reg.CreateAsset(asset);
    ENJIN_EXPECT_TRUE(reg.GetBool("Test", "active"));
}

ENJIN_TEST(TypedAccess, GetStringReturnsValue) {
    ResetRegistry();
    auto& reg = DataAssetRegistry::Get();
    DataAsset asset;
    asset.name = "Test";
    asset.values["tag"] = std::string("hero");
    reg.CreateAsset(asset);
    ENJIN_EXPECT_STR_EQ(reg.GetString("Test", "tag").c_str(), "hero");
}

ENJIN_TEST(TypedAccess, GetVector3ReturnsValue) {
    ResetRegistry();
    auto& reg = DataAssetRegistry::Get();
    DataAsset asset;
    asset.name = "Test";
    asset.values["pos"] = Math::Vector3(1.0f, 2.0f, 3.0f);
    reg.CreateAsset(asset);
    Math::Vector3 v = reg.GetVector3("Test", "pos");
    ENJIN_EXPECT_FLOAT_EQ(v.x, 1.0f);
    ENJIN_EXPECT_FLOAT_EQ(v.y, 2.0f);
    ENJIN_EXPECT_FLOAT_EQ(v.z, 3.0f);
}

ENJIN_TEST(TypedAccess, SetFloatUpdatesValue) {
    ResetRegistry();
    auto& reg = DataAssetRegistry::Get();
    DataAsset asset;
    asset.name = "Test";
    asset.values["val"] = 1.0f;
    reg.CreateAsset(asset);
    reg.SetFloat("Test", "val", 99.0f);
    ENJIN_EXPECT_FLOAT_EQ(reg.GetFloat("Test", "val"), 99.0f);
}

ENJIN_TEST(TypedAccess, SetIntUpdatesValue) {
    ResetRegistry();
    auto& reg = DataAssetRegistry::Get();
    DataAsset asset;
    asset.name = "Test";
    asset.values["n"] = i32(0);
    reg.CreateAsset(asset);
    reg.SetInt("Test", "n", 42);
    ENJIN_EXPECT_EQ(reg.GetInt("Test", "n"), 42);
}

ENJIN_TEST(TypedAccess, FloatIntCoercion) {
    ResetRegistry();
    auto& reg = DataAssetRegistry::Get();
    DataAsset asset;
    asset.name = "Test";
    asset.values["val"] = i32(10);
    reg.CreateAsset(asset);
    // GetFloat should coerce i32 -> f32
    ENJIN_EXPECT_FLOAT_EQ(reg.GetFloat("Test", "val"), 10.0f);
}

// ===========================================================================
// JSON Round-Trip
// ===========================================================================

ENJIN_TEST(JSON, SchemaRoundTrip) {
    ResetRegistry();
    auto& reg = DataAssetRegistry::Get();
    DataAssetSchema schema = MakeTestSchema();

    std::string path = "test_schema_tmp.enjschema";
    ENJIN_EXPECT_TRUE(reg.SaveSchema(schema, path));

    reg.Clear();
    ENJIN_EXPECT_TRUE(reg.LoadSchema(path));

    const DataAssetSchema* loaded = reg.FindSchema("TestSchema");
    ENJIN_EXPECT_NOT_NULL(loaded);
    ENJIN_EXPECT_EQ(loaded->fields.size(), (size_t)5);
    ENJIN_EXPECT_STR_EQ(loaded->fields[0].name.c_str(), "health");

    std::remove(path.c_str());
}

ENJIN_TEST(JSON, AssetRoundTrip) {
    ResetRegistry();
    auto& reg = DataAssetRegistry::Get();
    reg.RegisterSchema(MakeTestSchema());

    DataAsset asset;
    asset.name = "Warrior";
    asset.schemaName = "TestSchema";
    asset.values["health"] = 200.0f;
    asset.values["name"] = std::string("Conan");
    asset.values["alive"] = true;
    asset.values["level"] = i32(10);
    asset.values["position"] = Math::Vector3(5, 10, 15);

    std::string path = "test_asset_tmp.enjdata";
    ENJIN_EXPECT_TRUE(reg.SaveAsset(asset, path));

    reg.Clear();
    reg.RegisterSchema(MakeTestSchema());
    ENJIN_EXPECT_TRUE(reg.LoadAsset(path));

    ENJIN_EXPECT_FLOAT_EQ(reg.GetFloat("Warrior", "health"), 200.0f);
    ENJIN_EXPECT_STR_EQ(reg.GetString("Warrior", "name").c_str(), "Conan");
    ENJIN_EXPECT_TRUE(reg.GetBool("Warrior", "alive"));
    ENJIN_EXPECT_EQ(reg.GetInt("Warrior", "level"), 10);
    Math::Vector3 pos = reg.GetVector3("Warrior", "position");
    ENJIN_EXPECT_FLOAT_EQ(pos.x, 5.0f);
    ENJIN_EXPECT_FLOAT_EQ(pos.y, 10.0f);

    std::remove(path.c_str());
}

// ===========================================================================
// DataFieldType Helpers
// ===========================================================================

ENJIN_TEST(FieldType, StringConversions) {
    ENJIN_EXPECT_STR_EQ(DataFieldTypeToString(DataFieldType::String), "String");
    ENJIN_EXPECT_STR_EQ(DataFieldTypeToString(DataFieldType::Float), "Float");
    ENJIN_EXPECT_STR_EQ(DataFieldTypeToString(DataFieldType::Int), "Int");
    ENJIN_EXPECT_STR_EQ(DataFieldTypeToString(DataFieldType::Bool), "Bool");
    ENJIN_EXPECT_STR_EQ(DataFieldTypeToString(DataFieldType::Vector3), "Vector3");
}

ENJIN_TEST(FieldType, FromStringConversions) {
    ENJIN_EXPECT_EQ((int)DataFieldTypeFromString("String"), (int)DataFieldType::String);
    ENJIN_EXPECT_EQ((int)DataFieldTypeFromString("Float"), (int)DataFieldType::Float);
    ENJIN_EXPECT_EQ((int)DataFieldTypeFromString("Int"), (int)DataFieldType::Int);
    ENJIN_EXPECT_EQ((int)DataFieldTypeFromString("Bool"), (int)DataFieldType::Bool);
    ENJIN_EXPECT_EQ((int)DataFieldTypeFromString("Vector3"), (int)DataFieldType::Vector3);
}

// ===========================================================================
// Clear
// ===========================================================================

ENJIN_TEST(Clear, ClearRemovesEverything) {
    ResetRegistry();
    auto& reg = DataAssetRegistry::Get();
    reg.RegisterSchema(MakeTestSchema());
    DataAsset a; a.name = "X"; a.schemaName = "TestSchema";
    reg.CreateAsset(a);
    reg.Clear();
    ENJIN_EXPECT_EQ(reg.GetAllSchemas().size(), (size_t)0);
    ENJIN_EXPECT_EQ(reg.GetAllAssets().size(), (size_t)0);
}

ENJIN_TEST_MAIN()
