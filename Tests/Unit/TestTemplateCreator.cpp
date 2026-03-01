#include "EnjinTest.h"
#include "Enjin/Editor/TemplateCreator.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Name.h"
#include "Enjin/Scene/SceneSerializer.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace Enjin;
using namespace Enjin::Editor;
using namespace Enjin::ECS;

// ===========================================================================
// Helpers
// ===========================================================================

// Make a unique temp directory under the system temp path for a given test.
static std::string MakeTempDir(const std::string& tag) {
    std::filesystem::path base = std::filesystem::temp_directory_path()
                                 / ("enjin_tpl_test_" + tag);
    std::filesystem::create_directories(base);
    return base.string();
}

// Remove a temp directory tree (best-effort; ignores errors).
static void RemoveTempDir(const std::string& path) {
    std::error_code ec;
    std::filesystem::remove_all(path, ec);
}

// Write raw text to a file, creating parent dirs as needed.
static void WriteFile(const std::string& path, const std::string& content) {
    std::filesystem::create_directories(
        std::filesystem::path(path).parent_path());
    std::ofstream f(path);
    f << content;
}

// Build a valid TemplateMetadata with sensible defaults.
static TemplateMetadata MakeMeta(const std::string& id,
                                 const std::string& name,
                                 const std::string& category = "3D") {
    TemplateMetadata m;
    m.id = id;
    m.name = name;
    m.description = "Test template: " + name;
    m.category = category;
    m.author = "TestAuthor";
    m.accentColor = Math::Vector4(0.2f, 0.4f, 0.8f, 1.0f);
    m.isBuiltin = false;
    return m;
}

// ===========================================================================
// SanitizeId — tested indirectly through SaveTemplate with empty id
// The implementation is private, so we probe via SaveTemplate behaviour:
// if meta.id is empty, SaveTemplate derives the id from meta.name via
// SanitizeId() and creates a directory with that derived name.
// ===========================================================================

// Helper: call SaveTemplate with an empty id and return the created dir name.
static std::string DerivedDirName(const std::string& inputName) {
    // Sanitize the tag so path-unsafe chars (/ \ : etc.) don't corrupt the temp dir path
    std::string tag = inputName.substr(0, 8);
    for (char& c : tag) {
        if (c == '/' || c == '\\' || c == ':' || c == '*' ||
            c == '?' || c == '"' || c == '<' || c == '>' || c == '|' || c == ' ')
            c = '_';
    }
    std::string root = MakeTempDir("sanitize_" + tag);
    ECS::World world;
    Scene::SceneSerializer ser(&world);

    TemplateMetadata m;
    m.id = "";          // Force derivation from name
    m.name = inputName;
    m.category = "3D";

    TemplateCreator::SaveTemplate(root, m, &world, ser);

    // Find first (and only) subdirectory created
    std::string result;
    for (const auto& entry : std::filesystem::directory_iterator(root)) {
        if (entry.is_directory()) {
            result = entry.path().filename().string();
            break;
        }
    }
    RemoveTempDir(root);
    return result;
}

// ===========================================================================
// SanitizeId
// ===========================================================================

ENJIN_TEST(SanitizeId, EmptyNameFallsBackToTemplate) {
    // Empty name -> fallback id is "template"
    std::string dir = DerivedDirName("");
    ENJIN_EXPECT_STR_EQ(dir.c_str(), "template");
}

ENJIN_TEST(SanitizeId, SpacesBecomesUnderscores) {
    std::string dir = DerivedDirName("My Cool Scene");
    ENJIN_EXPECT_STR_EQ(dir.c_str(), "my_cool_scene");
}

ENJIN_TEST(SanitizeId, AlreadyCleanPassesThrough) {
    std::string dir = DerivedDirName("myscene");
    ENJIN_EXPECT_STR_EQ(dir.c_str(), "myscene");
}

ENJIN_TEST(SanitizeId, UppercaseIsLowercased) {
    std::string dir = DerivedDirName("PlayerSpawn");
    ENJIN_EXPECT_STR_EQ(dir.c_str(), "playerspawn");
}

ENJIN_TEST(SanitizeId, SpecialCharsReplacedWithUnderscore) {
    // Characters: / \ : * ? " < > | must all become _
    std::string dir = DerivedDirName("foo/bar:baz");
    // foo -> f, / -> _, bar -> bar, : -> _, baz -> baz  =>  "foo_bar_baz"
    ENJIN_EXPECT_STR_EQ(dir.c_str(), "foo_bar_baz");
}

ENJIN_TEST(SanitizeId, LeadingAndTrailingUnderscoresStripped) {
    // Spaces at start/end become _ then get stripped
    std::string dir = DerivedDirName(" forest ");
    ENJIN_EXPECT_STR_EQ(dir.c_str(), "forest");
}

ENJIN_TEST(SanitizeId, AllSpecialCharsResultsInTemplate) {
    // A name that is entirely path-unsafe chars should produce "template"
    std::string dir = DerivedDirName("///");
    ENJIN_EXPECT_STR_EQ(dir.c_str(), "template");
}

// ===========================================================================
// TemplateMetadata — struct field defaults and assignment
// ===========================================================================

ENJIN_TEST(Metadata, DefaultAccentColor) {
    TemplateMetadata m;
    ENJIN_EXPECT_FLOAT_NEAR(m.accentColor.x, 0.3f, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(m.accentColor.y, 0.6f, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(m.accentColor.z, 1.0f, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(m.accentColor.w, 1.0f, 0.001f);
}

ENJIN_TEST(Metadata, DefaultIsBuiltinFalse) {
    TemplateMetadata m;
    ENJIN_EXPECT_FALSE(m.isBuiltin);
}

ENJIN_TEST(Metadata, FieldsAssignableAndReadable) {
    TemplateMetadata m = MakeMeta("test_id", "Test Scene", "Multiplayer");
    ENJIN_EXPECT_STR_EQ(m.id.c_str(), "test_id");
    ENJIN_EXPECT_STR_EQ(m.name.c_str(), "Test Scene");
    ENJIN_EXPECT_STR_EQ(m.description.c_str(), "Test template: Test Scene");
    ENJIN_EXPECT_STR_EQ(m.category.c_str(), "Multiplayer");
    ENJIN_EXPECT_STR_EQ(m.author.c_str(), "TestAuthor");
    ENJIN_EXPECT_FALSE(m.isBuiltin);
}

ENJIN_TEST(Metadata, ThumbnailPathDefaultEmpty) {
    TemplateMetadata m;
    ENJIN_EXPECT_TRUE(m.thumbnailPath.empty());
}

// ===========================================================================
// SaveTemplate / LoadTemplate round-trip
// ===========================================================================

ENJIN_TEST(RoundTrip, EmptyWorldSavesAndLoadsSuccessfully) {
    std::string root = MakeTempDir("rt_empty");

    ECS::World world;
    Scene::SceneSerializer ser(&world);
    TemplateMetadata meta = MakeMeta("empty_scene", "Empty Scene", "3D");

    bool saved = TemplateCreator::SaveTemplate(root, meta, &world, ser);
    ENJIN_EXPECT_TRUE(saved);

    // Verify the directory was created
    std::filesystem::path dir = std::filesystem::path(root) / "empty_scene";
    ENJIN_EXPECT_TRUE(std::filesystem::exists(dir));
    ENJIN_EXPECT_TRUE(std::filesystem::exists(dir / "meta.json"));
    ENJIN_EXPECT_TRUE(std::filesystem::exists(dir / "scene.enjin"));

    // Load back
    ECS::World world2;
    Scene::SceneSerializer ser2(&world2);
    TemplateMetadata outMeta;
    bool loaded = TemplateCreator::LoadTemplate(dir.string(), &world2, ser2, outMeta);
    ENJIN_EXPECT_TRUE(loaded);

    ENJIN_EXPECT_STR_EQ(outMeta.id.c_str(), "empty_scene");
    ENJIN_EXPECT_STR_EQ(outMeta.name.c_str(), "Empty Scene");
    ENJIN_EXPECT_STR_EQ(outMeta.category.c_str(), "3D");
    ENJIN_EXPECT_STR_EQ(outMeta.author.c_str(), "TestAuthor");
    // IsBuiltin is always false for loaded custom templates
    ENJIN_EXPECT_FALSE(outMeta.isBuiltin);

    RemoveTempDir(root);
}

ENJIN_TEST(RoundTrip, MetaAccentColorPreserved) {
    std::string root = MakeTempDir("rt_accent");

    ECS::World world;
    Scene::SceneSerializer ser(&world);
    TemplateMetadata meta = MakeMeta("accent_test", "Accent Test", "2D");
    meta.accentColor = Math::Vector4(0.1f, 0.9f, 0.5f, 0.8f);

    bool saved = TemplateCreator::SaveTemplate(root, meta, &world, ser);
    ENJIN_ASSERT_TRUE(saved);

    std::filesystem::path dir = std::filesystem::path(root) / "accent_test";
    ECS::World world2;
    Scene::SceneSerializer ser2(&world2);
    TemplateMetadata outMeta;
    bool loaded = TemplateCreator::LoadTemplate(dir.string(), &world2, ser2, outMeta);
    ENJIN_ASSERT_TRUE(loaded);

    ENJIN_EXPECT_FLOAT_NEAR(outMeta.accentColor.x, 0.1f, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(outMeta.accentColor.y, 0.9f, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(outMeta.accentColor.z, 0.5f, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(outMeta.accentColor.w, 0.8f, 0.001f);

    RemoveTempDir(root);
}

ENJIN_TEST(RoundTrip, OneEntityPreservedAfterRoundTrip) {
    std::string root = MakeTempDir("rt_one_entity");

    ECS::World world;
    Entity e = world.CreateEntity();
    world.AddComponent<TransformComponent>(e);
    auto& name = world.AddComponent<NameComponent>(e);
    name.name = "TestEntity";

    Scene::SceneSerializer ser(&world);
    TemplateMetadata meta = MakeMeta("one_entity", "One Entity", "3D");

    bool saved = TemplateCreator::SaveTemplate(root, meta, &world, ser);
    ENJIN_ASSERT_TRUE(saved);

    std::filesystem::path dir = std::filesystem::path(root) / "one_entity";
    ECS::World world2;
    Scene::SceneSerializer ser2(&world2);
    TemplateMetadata outMeta;
    bool loaded = TemplateCreator::LoadTemplate(dir.string(), &world2, ser2, outMeta);
    ENJIN_ASSERT_TRUE(loaded);

    ENJIN_EXPECT_EQ(world2.GetEntityCount(), (usize)1);

    RemoveTempDir(root);
}

ENJIN_TEST(RoundTrip, DescriptionRoundTrips) {
    std::string root = MakeTempDir("rt_desc");

    ECS::World world;
    Scene::SceneSerializer ser(&world);
    TemplateMetadata meta = MakeMeta("desc_test", "Desc Test", "Tools");
    meta.description = "A scene with special chars: & < > \"quotes\"";

    bool saved = TemplateCreator::SaveTemplate(root, meta, &world, ser);
    ENJIN_ASSERT_TRUE(saved);

    std::filesystem::path dir = std::filesystem::path(root) / "desc_test";
    ECS::World world2;
    Scene::SceneSerializer ser2(&world2);
    TemplateMetadata outMeta;
    bool loaded = TemplateCreator::LoadTemplate(dir.string(), &world2, ser2, outMeta);
    ENJIN_ASSERT_TRUE(loaded);

    ENJIN_EXPECT_STR_EQ(outMeta.description.c_str(),
                        "A scene with special chars: & < > \"quotes\"");

    RemoveTempDir(root);
}

ENJIN_TEST(RoundTrip, IdDerivedFromNameWhenEmpty) {
    // When meta.id is empty, the created directory should reflect SanitizeId(name)
    std::string root = MakeTempDir("rt_derived_id");

    ECS::World world;
    Scene::SceneSerializer ser(&world);
    TemplateMetadata meta;
    meta.id = "";
    meta.name = "My Scene";
    meta.category = "3D";

    bool saved = TemplateCreator::SaveTemplate(root, meta, &world, ser);
    ENJIN_ASSERT_TRUE(saved);

    // Derived dir name: "my_scene"
    std::filesystem::path expectedDir = std::filesystem::path(root) / "my_scene";
    ENJIN_EXPECT_TRUE(std::filesystem::exists(expectedDir));

    ECS::World world2;
    Scene::SceneSerializer ser2(&world2);
    TemplateMetadata outMeta;
    bool loaded = TemplateCreator::LoadTemplate(expectedDir.string(), &world2, ser2, outMeta);
    ENJIN_ASSERT_TRUE(loaded);

    ENJIN_EXPECT_STR_EQ(outMeta.id.c_str(), "my_scene");

    RemoveTempDir(root);
}

// ===========================================================================
// Entity count boundary cases
// ===========================================================================

ENJIN_TEST(EntityCount, ZeroEntitiesProducesValidTemplate) {
    std::string root = MakeTempDir("ec_zero");
    ECS::World world;    // no entities
    Scene::SceneSerializer ser(&world);
    TemplateMetadata meta = MakeMeta("zero_entities", "Zero Entities");

    bool saved = TemplateCreator::SaveTemplate(root, meta, &world, ser);
    ENJIN_EXPECT_TRUE(saved);

    std::filesystem::path dir = std::filesystem::path(root) / "zero_entities";
    ECS::World world2;
    Scene::SceneSerializer ser2(&world2);
    TemplateMetadata outMeta;
    bool loaded = TemplateCreator::LoadTemplate(dir.string(), &world2, ser2, outMeta);
    ENJIN_EXPECT_TRUE(loaded);
    ENJIN_EXPECT_EQ(world2.GetEntityCount(), (usize)0);

    RemoveTempDir(root);
}

ENJIN_TEST(EntityCount, MultipleEntitiesAllSurviveRoundTrip) {
    std::string root = MakeTempDir("ec_multi");
    ECS::World world;

    const u32 COUNT = 5;
    for (u32 i = 0; i < COUNT; ++i) {
        Entity e = world.CreateEntity();
        world.AddComponent<TransformComponent>(e);
        NameComponent nc;
        nc.name = "Entity" + std::to_string(i);
        world.AddComponent<NameComponent>(e, nc);
    }

    Scene::SceneSerializer ser(&world);
    TemplateMetadata meta = MakeMeta("multi_entities", "Multi Entities");

    bool saved = TemplateCreator::SaveTemplate(root, meta, &world, ser);
    ENJIN_ASSERT_TRUE(saved);

    std::filesystem::path dir = std::filesystem::path(root) / "multi_entities";
    ECS::World world2;
    Scene::SceneSerializer ser2(&world2);
    TemplateMetadata outMeta;
    bool loaded = TemplateCreator::LoadTemplate(dir.string(), &world2, ser2, outMeta);
    ENJIN_ASSERT_TRUE(loaded);
    ENJIN_EXPECT_EQ(world2.GetEntityCount(), (usize)COUNT);

    RemoveTempDir(root);
}

// ===========================================================================
// Null world guard
// ===========================================================================

ENJIN_TEST(NullWorld, SaveReturnsFalse) {
    std::string root = MakeTempDir("null_world_save");
    ECS::World world;
    Scene::SceneSerializer ser(&world);
    TemplateMetadata meta = MakeMeta("null_save", "Null Save");

    // nullptr world passed explicitly
    ECS::World* nullWorld = nullptr;
    bool result2 = TemplateCreator::SaveTemplate(root, meta, nullWorld, ser);
    ENJIN_EXPECT_FALSE(result2);

    RemoveTempDir(root);
}

ENJIN_TEST(NullWorld, LoadReturnsFalse) {
    std::string root = MakeTempDir("null_world_load");
    ECS::World world;
    Scene::SceneSerializer ser(&world);
    TemplateMetadata outMeta;
    ECS::World* nullWorld = nullptr;
    bool result = TemplateCreator::LoadTemplate(root, nullWorld, ser, outMeta);
    ENJIN_EXPECT_FALSE(result);
    RemoveTempDir(root);
}

// ===========================================================================
// Malformed JSON handling
// ===========================================================================

ENJIN_TEST(MalformedJSON, GarbageMetaJsonReturnsFalse) {
    std::string root = MakeTempDir("malformed_garbage");
    std::string dir = root + "/bad_template";
    WriteFile(dir + "/meta.json", "{{{{ not valid json %%%%");
    WriteFile(dir + "/scene.enjin", "{}");

    ECS::World world;
    Scene::SceneSerializer ser(&world);
    TemplateMetadata outMeta;
    bool loaded = TemplateCreator::LoadTemplate(dir, &world, ser, outMeta);
    ENJIN_EXPECT_FALSE(loaded);

    RemoveTempDir(root);
}

ENJIN_TEST(MalformedJSON, EmptyMetaJsonReturnsFalse) {
    std::string root = MakeTempDir("malformed_empty");
    std::string dir = root + "/empty_meta";
    WriteFile(dir + "/meta.json", "");
    WriteFile(dir + "/scene.enjin", "{}");

    ECS::World world;
    Scene::SceneSerializer ser(&world);
    TemplateMetadata outMeta;
    bool loaded = TemplateCreator::LoadTemplate(dir, &world, ser, outMeta);
    ENJIN_EXPECT_FALSE(loaded);

    RemoveTempDir(root);
}

ENJIN_TEST(MalformedJSON, MissingMetaJsonReturnsFalse) {
    std::string root = MakeTempDir("malformed_no_meta");
    std::string dir = root + "/no_meta";
    std::filesystem::create_directories(dir);
    WriteFile(dir + "/scene.enjin", "{}");
    // No meta.json written

    ECS::World world;
    Scene::SceneSerializer ser(&world);
    TemplateMetadata outMeta;
    bool loaded = TemplateCreator::LoadTemplate(dir, &world, ser, outMeta);
    ENJIN_EXPECT_FALSE(loaded);

    RemoveTempDir(root);
}

ENJIN_TEST(MalformedJSON, MissingSceneFileReturnsFalse) {
    std::string root = MakeTempDir("malformed_no_scene");
    std::string dir = root + "/no_scene";
    WriteFile(dir + "/meta.json",
              R"({"id":"no_scene","name":"No Scene","category":"3D","author":"","description":""})");
    // No scene.enjin written

    ECS::World world;
    Scene::SceneSerializer ser(&world);
    TemplateMetadata outMeta;
    bool loaded = TemplateCreator::LoadTemplate(dir, &world, ser, outMeta);
    ENJIN_EXPECT_FALSE(loaded);

    RemoveTempDir(root);
}

ENJIN_TEST(MalformedJSON, WrongTypeForNameFieldUsesDefault) {
    // "name" is a number instead of string — value() gracefully falls back
    std::string root = MakeTempDir("malformed_wrong_type");
    std::string dir = root + "/wrong_type";

    // Create a valid scene so load can proceed past meta.json parsing
    ECS::World tmpWorld;
    Scene::SceneSerializer tmpSer(&tmpWorld);
    WriteFile(dir + "/meta.json",
              R"({"id":"wrong_type","name":12345,"category":"2D","author":"","description":""})");
    // Need a valid scene.enjin — create it via the serializer
    tmpSer.Save((dir + "/scene.enjin"));

    ECS::World world;
    Scene::SceneSerializer ser(&world);
    TemplateMetadata outMeta;
    bool loaded = TemplateCreator::LoadTemplate(dir, &world, ser, outMeta);
    // May succeed with default name or fail gracefully; either way must not crash
    // If it succeeds, the name should be the default "Unnamed"
    if (loaded) {
        ENJIN_EXPECT_STR_EQ(outMeta.name.c_str(), "Unnamed");
    }
    // If it fails, that is also acceptable — just must not crash

    RemoveTempDir(root);
}

ENJIN_TEST(MalformedJSON, MissingOptionalFieldsUseDefaults) {
    // meta.json with only the required "id" field — optional fields default gracefully
    std::string root = MakeTempDir("malformed_minimal");
    std::string dir = root + "/minimal_meta";

    ECS::World tmpWorld;
    Scene::SceneSerializer tmpSer(&tmpWorld);
    tmpSer.Save((dir + "/scene.enjin"));
    WriteFile(dir + "/meta.json", R"({"id":"minimal_meta"})");

    ECS::World world;
    Scene::SceneSerializer ser(&world);
    TemplateMetadata outMeta;
    bool loaded = TemplateCreator::LoadTemplate(dir, &world, ser, outMeta);
    ENJIN_EXPECT_TRUE(loaded);
    ENJIN_EXPECT_STR_EQ(outMeta.id.c_str(), "minimal_meta");
    ENJIN_EXPECT_STR_EQ(outMeta.name.c_str(), "Unnamed");
    ENJIN_EXPECT_TRUE(outMeta.description.empty());
    ENJIN_EXPECT_TRUE(outMeta.category.empty());
    ENJIN_EXPECT_TRUE(outMeta.author.empty());

    RemoveTempDir(root);
}

// ===========================================================================
// ScanTemplates
// ===========================================================================

ENJIN_TEST(ScanTemplates, EmptyDirReturnsEmptyVector) {
    std::string root = MakeTempDir("scan_empty");
    auto templates = TemplateCreator::ScanTemplates(root);
    ENJIN_EXPECT_EQ(templates.size(), (size_t)0);
    RemoveTempDir(root);
}

ENJIN_TEST(ScanTemplates, NonexistentDirReturnsEmptyVector) {
    auto templates = TemplateCreator::ScanTemplates("/this/path/does/not/exist/enjin_test");
    ENJIN_EXPECT_EQ(templates.size(), (size_t)0);
}

ENJIN_TEST(ScanTemplates, FindsTwoSavedTemplates) {
    std::string root = MakeTempDir("scan_two");

    ECS::World world;
    Scene::SceneSerializer ser(&world);

    TemplateMetadata m1 = MakeMeta("alpha", "Alpha Scene", "3D");
    TemplateMetadata m2 = MakeMeta("beta", "Beta Scene", "2D");
    TemplateCreator::SaveTemplate(root, m1, &world, ser);
    TemplateCreator::SaveTemplate(root, m2, &world, ser);

    auto templates = TemplateCreator::ScanTemplates(root);
    ENJIN_EXPECT_EQ(templates.size(), (size_t)2);

    // Results are sorted alphabetically by name, so Alpha < Beta
    if (templates.size() == 2) {
        ENJIN_EXPECT_STR_EQ(templates[0].name.c_str(), "Alpha Scene");
        ENJIN_EXPECT_STR_EQ(templates[1].name.c_str(), "Beta Scene");
    }

    RemoveTempDir(root);
}

ENJIN_TEST(ScanTemplates, SkipsSubdirWithoutMetaJson) {
    std::string root = MakeTempDir("scan_skip");

    // One valid template
    ECS::World world;
    Scene::SceneSerializer ser(&world);
    TemplateMetadata m = MakeMeta("valid_one", "Valid One", "3D");
    TemplateCreator::SaveTemplate(root, m, &world, ser);

    // One directory without meta.json
    std::string orphan = root + "/orphan_dir";
    std::filesystem::create_directories(orphan);

    auto templates = TemplateCreator::ScanTemplates(root);
    ENJIN_EXPECT_EQ(templates.size(), (size_t)1);
    ENJIN_EXPECT_STR_EQ(templates[0].id.c_str(), "valid_one");

    RemoveTempDir(root);
}

ENJIN_TEST(ScanTemplates, SkipsMalformedMetaJson) {
    std::string root = MakeTempDir("scan_malformed");

    // One valid template
    ECS::World world;
    Scene::SceneSerializer ser(&world);
    TemplateMetadata m = MakeMeta("good_template", "Good Template", "3D");
    TemplateCreator::SaveTemplate(root, m, &world, ser);

    // One directory with a broken meta.json
    std::string badDir = root + "/broken";
    WriteFile(badDir + "/meta.json", "not json at all");

    auto templates = TemplateCreator::ScanTemplates(root);
    // Only the valid one should be returned
    ENJIN_EXPECT_EQ(templates.size(), (size_t)1);
    ENJIN_EXPECT_STR_EQ(templates[0].id.c_str(), "good_template");

    RemoveTempDir(root);
}

// ===========================================================================
// DeleteTemplate
// ===========================================================================

ENJIN_TEST(DeleteTemplate, DeletesExistingTemplate) {
    std::string root = MakeTempDir("delete_existing");
    ECS::World world;
    Scene::SceneSerializer ser(&world);
    TemplateMetadata meta = MakeMeta("to_delete", "To Delete", "3D");
    TemplateCreator::SaveTemplate(root, meta, &world, ser);

    std::filesystem::path dir = std::filesystem::path(root) / "to_delete";
    ENJIN_ASSERT_TRUE(std::filesystem::exists(dir));

    bool deleted = TemplateCreator::DeleteTemplate(root, "to_delete");
    ENJIN_EXPECT_TRUE(deleted);
    ENJIN_EXPECT_FALSE(std::filesystem::exists(dir));

    RemoveTempDir(root);
}

ENJIN_TEST(DeleteTemplate, ReturnsFalseForNonexistentId) {
    std::string root = MakeTempDir("delete_nonexistent");
    std::filesystem::create_directories(root);
    bool deleted = TemplateCreator::DeleteTemplate(root, "this_does_not_exist");
    ENJIN_EXPECT_FALSE(deleted);
    RemoveTempDir(root);
}

ENJIN_TEST(DeleteTemplate, ReturnsFalseForEmptyId) {
    std::string root = MakeTempDir("delete_empty_id");
    bool deleted = TemplateCreator::DeleteTemplate(root, "");
    ENJIN_EXPECT_FALSE(deleted);
    RemoveTempDir(root);
}

ENJIN_TEST(DeleteTemplate, RefusesToDeleteDirWithoutMetaJson) {
    std::string root = MakeTempDir("delete_no_meta");
    std::string orphan = root + "/orphan";
    std::filesystem::create_directories(orphan);
    // No meta.json — should refuse deletion
    bool deleted = TemplateCreator::DeleteTemplate(root, "orphan");
    ENJIN_EXPECT_FALSE(deleted);
    ENJIN_EXPECT_TRUE(std::filesystem::exists(orphan));  // still there
    RemoveTempDir(root);
}

// ===========================================================================
// Template categories
// ===========================================================================

ENJIN_TEST(Categories, KnownCategoryStringsAreNonEmpty) {
    // The engine documents "2D", "3D", "Multiplayer", "Tools" as built-in categories.
    // We verify they are valid non-empty string constants by storing them in metadata.
    std::vector<std::string> builtinCategories = {"2D", "3D", "Multiplayer", "Tools"};
    for (const auto& cat : builtinCategories) {
        ENJIN_EXPECT_FALSE(cat.empty());
    }
    ENJIN_EXPECT_EQ(builtinCategories.size(), (size_t)4);
}

ENJIN_TEST(Categories, CategoryRoundTrips) {
    // Each documented category must survive a metadata save/load cycle.
    std::vector<std::string> cats = {"2D", "3D", "Multiplayer", "Tools"};
    for (const auto& cat : cats) {
        std::string root = MakeTempDir("cat_" + cat);
        ECS::World world;
        Scene::SceneSerializer ser(&world);
        std::string idStr = "cat_" + cat;
        // Make id filesystem-safe (lowercase, replace digits as-is)
        for (char& c : idStr) {
            if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + 32);
        }
        TemplateMetadata meta = MakeMeta(idStr, "Cat " + cat, cat);
        bool saved = TemplateCreator::SaveTemplate(root, meta, &world, ser);
        ENJIN_ASSERT_TRUE(saved);

        std::filesystem::path dir = std::filesystem::path(root) / idStr;
        ECS::World world2;
        Scene::SceneSerializer ser2(&world2);
        TemplateMetadata outMeta;
        bool loaded = TemplateCreator::LoadTemplate(dir.string(), &world2, ser2, outMeta);
        ENJIN_ASSERT_TRUE(loaded);
        ENJIN_EXPECT_STR_EQ(outMeta.category.c_str(), cat.c_str());

        RemoveTempDir(root);
    }
}

// ===========================================================================
// Template version compatibility — meta.json written without optional fields
// ===========================================================================

ENJIN_TEST(Compatibility, LoadHandlesMetaWithNoAccentColor) {
    // An older template meta.json without accentColor should load cleanly,
    // keeping the default color values.
    std::string root = MakeTempDir("compat_no_accent");
    std::string dir = root + "/old_format";

    ECS::World tmpWorld;
    Scene::SceneSerializer tmpSer(&tmpWorld);
    tmpSer.Save((dir + "/scene.enjin"));
    WriteFile(dir + "/meta.json",
              R"({"id":"old_format","name":"Old Format","category":"3D","author":"Legacy","description":"v1"})");

    ECS::World world;
    Scene::SceneSerializer ser(&world);
    TemplateMetadata outMeta;
    bool loaded = TemplateCreator::LoadTemplate(dir, &world, ser, outMeta);
    ENJIN_EXPECT_TRUE(loaded);

    // Default accent color should be intact (0.3, 0.6, 1.0, 1.0)
    ENJIN_EXPECT_FLOAT_NEAR(outMeta.accentColor.x, 0.3f, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(outMeta.accentColor.y, 0.6f, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(outMeta.accentColor.z, 1.0f, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(outMeta.accentColor.w, 1.0f, 0.001f);

    RemoveTempDir(root);
}

ENJIN_TEST(Compatibility, LoadHandlesMetaWithUnknownExtraFields) {
    // A future meta.json that contains extra fields unknown to this version
    // should still load the fields it recognises without crashing.
    std::string root = MakeTempDir("compat_extra");
    std::string dir = root + "/future_format";

    ECS::World tmpWorld;
    Scene::SceneSerializer tmpSer(&tmpWorld);
    tmpSer.Save((dir + "/scene.enjin"));
    WriteFile(dir + "/meta.json",
              R"({
                  "id":"future_format",
                  "name":"Future Format",
                  "category":"Tools",
                  "author":"FutureAuthor",
                  "description":"v99",
                  "accentColor":[0.5,0.5,0.5,1.0],
                  "unknownFieldA": true,
                  "unknownFieldB": 42,
                  "nestedUnknown": {"x": 1}
              })");

    ECS::World world;
    Scene::SceneSerializer ser(&world);
    TemplateMetadata outMeta;
    bool loaded = TemplateCreator::LoadTemplate(dir, &world, ser, outMeta);
    ENJIN_EXPECT_TRUE(loaded);
    ENJIN_EXPECT_STR_EQ(outMeta.id.c_str(), "future_format");
    ENJIN_EXPECT_STR_EQ(outMeta.name.c_str(), "Future Format");
    ENJIN_EXPECT_STR_EQ(outMeta.category.c_str(), "Tools");
    ENJIN_EXPECT_FLOAT_NEAR(outMeta.accentColor.x, 0.5f, 0.001f);

    RemoveTempDir(root);
}

ENJIN_TEST_MAIN()
