#include "EnjinTest.h"
#include "Enjin/GUI/LocalizationBoot.h"
#include <fstream>
#include "Enjin/GUI/Localization.h"

#include <cstdio>
#include <string>
#include <unordered_map>
#include <vector>

using namespace Enjin;
using namespace Enjin::GUI;

// Helper: clear singleton state before each test group
static void ResetLocalization() {
    LocalizationManager::Get().Clear();
}

// ===========================================================================
// Singleton & Defaults
// ===========================================================================

ENJIN_TEST(Defaults, SingletonReturnsSameInstance) {
    auto& a = LocalizationManager::Get();
    auto& b = LocalizationManager::Get();
    ENJIN_EXPECT_EQ(&a, &b);
}

ENJIN_TEST(Defaults, DefaultLocaleIsEnglish) {
    ResetLocalization();
    ENJIN_EXPECT_STR_EQ(LocalizationManager::Get().GetCurrentLocale().c_str(), "en");
}

ENJIN_TEST(Defaults, ClearResetsToEnglish) {
    auto& mgr = LocalizationManager::Get();
    mgr.SetString("fr", "hello", "Bonjour");
    mgr.SetLocale("fr");
    mgr.Clear();
    ENJIN_EXPECT_STR_EQ(mgr.GetCurrentLocale().c_str(), "en");
}

// ===========================================================================
// String Management
// ===========================================================================

ENJIN_TEST(Strings, SetAndGetString) {
    ResetLocalization();
    auto& mgr = LocalizationManager::Get();
    mgr.SetString("en", "greeting", "Hello");
    ENJIN_EXPECT_STR_EQ(mgr.GetString("greeting").c_str(), "Hello");
}

ENJIN_TEST(Strings, MissingKeyReturnsFallback) {
    ResetLocalization();
    auto& mgr = LocalizationManager::Get();
    ENJIN_EXPECT_STR_EQ(mgr.GetString("nonexistent", "fallback").c_str(), "fallback");
}

ENJIN_TEST(Strings, MissingKeyReturnsKeyItself) {
    ResetLocalization();
    auto& mgr = LocalizationManager::Get();
    ENJIN_EXPECT_STR_EQ(mgr.GetString("missing_key").c_str(), "missing_key");
}

ENJIN_TEST(Strings, OverwriteExistingKey) {
    ResetLocalization();
    auto& mgr = LocalizationManager::Get();
    mgr.SetString("en", "key", "Old");
    mgr.SetString("en", "key", "New");
    ENJIN_EXPECT_STR_EQ(mgr.GetString("key").c_str(), "New");
}

ENJIN_TEST(Strings, HasStringTrueWhenPresent) {
    ResetLocalization();
    auto& mgr = LocalizationManager::Get();
    mgr.SetString("en", "exists", "yes");
    ENJIN_EXPECT_TRUE(mgr.HasString("exists"));
}

ENJIN_TEST(Strings, HasStringFalseWhenMissing) {
    ResetLocalization();
    auto& mgr = LocalizationManager::Get();
    ENJIN_EXPECT_FALSE(mgr.HasString("nope"));
}

ENJIN_TEST(Strings, GetStringCountReturnsCorrect) {
    ResetLocalization();
    auto& mgr = LocalizationManager::Get();
    mgr.SetString("en", "a", "A");
    mgr.SetString("en", "b", "B");
    mgr.SetString("en", "c", "C");
    ENJIN_EXPECT_EQ(mgr.GetStringCount("en"), 3u);
}

ENJIN_TEST(Strings, GetStringCountZeroForMissingLocale) {
    ResetLocalization();
    ENJIN_EXPECT_EQ(LocalizationManager::Get().GetStringCount("zz"), 0u);
}

ENJIN_TEST(Strings, GetAllKeysReturnsSorted) {
    ResetLocalization();
    auto& mgr = LocalizationManager::Get();
    mgr.SetString("en", "zebra", "Z");
    mgr.SetString("en", "apple", "A");
    mgr.SetString("en", "mango", "M");
    auto keys = mgr.GetAllKeys();
    ENJIN_EXPECT_EQ(keys.size(), (size_t)3);
    ENJIN_EXPECT_STR_EQ(keys[0].c_str(), "apple");
    ENJIN_EXPECT_STR_EQ(keys[1].c_str(), "mango");
    ENJIN_EXPECT_STR_EQ(keys[2].c_str(), "zebra");
}

// ===========================================================================
// Locale Switching & Fallback
// ===========================================================================

ENJIN_TEST(Locale, SwitchLocale) {
    ResetLocalization();
    auto& mgr = LocalizationManager::Get();
    mgr.SetString("en", "greeting", "Hello");
    mgr.SetString("fr", "greeting", "Bonjour");
    mgr.SetLocale("fr");
    ENJIN_EXPECT_STR_EQ(mgr.GetString("greeting").c_str(), "Bonjour");
}

ENJIN_TEST(Locale, FallbackToEnglish) {
    ResetLocalization();
    auto& mgr = LocalizationManager::Get();
    mgr.SetString("en", "only_in_en", "English Only");
    mgr.SetString("fr", "other_key", "Autre");
    mgr.SetLocale("fr");
    // Key exists in English but not French — should fall back
    ENJIN_EXPECT_STR_EQ(mgr.GetString("only_in_en").c_str(), "English Only");
}

ENJIN_TEST(Locale, InvalidLocaleKeepsCurrent) {
    ResetLocalization();
    auto& mgr = LocalizationManager::Get();
    mgr.SetLocale("nonexistent_locale");
    ENJIN_EXPECT_STR_EQ(mgr.GetCurrentLocale().c_str(), "en");
}

ENJIN_TEST(Locale, AddLocale) {
    ResetLocalization();
    auto& mgr = LocalizationManager::Get();
    Locale ja;
    ja.code = "ja";
    ja.name = "Japanese";
    mgr.AddLocale(ja);
    auto locales = mgr.GetAvailableLocales();
    bool found = false;
    for (auto& loc : locales) {
        if (loc.code == "ja") found = true;
    }
    ENJIN_EXPECT_TRUE(found);
}

ENJIN_TEST(Locale, AddDuplicateLocaleIgnored) {
    ResetLocalization();
    auto& mgr = LocalizationManager::Get();
    Locale en1; en1.code = "en"; en1.name = "English1";
    Locale en2; en2.code = "en"; en2.name = "English2";
    mgr.AddLocale(en1);
    size_t count1 = mgr.GetAvailableLocales().size();
    mgr.AddLocale(en2);
    size_t count2 = mgr.GetAvailableLocales().size();
    ENJIN_EXPECT_EQ(count1, count2);
}

// ===========================================================================
// Parameterized Formatting
// ===========================================================================

ENJIN_TEST(Format, SingleParam) {
    ResetLocalization();
    auto& mgr = LocalizationManager::Get();
    mgr.SetString("en", "welcome", "Hello, {name}!");
    std::unordered_map<std::string, std::string> params;
    params["name"] = "Alice";
    ENJIN_EXPECT_STR_EQ(mgr.Format("welcome", params).c_str(), "Hello, Alice!");
}

ENJIN_TEST(Format, MultipleParams) {
    ResetLocalization();
    auto& mgr = LocalizationManager::Get();
    mgr.SetString("en", "score", "{player} scored {points} points");
    std::unordered_map<std::string, std::string> params;
    params["player"] = "Bob";
    params["points"] = "100";
    ENJIN_EXPECT_STR_EQ(mgr.Format("score", params).c_str(), "Bob scored 100 points");
}

ENJIN_TEST(Format, RepeatedParam) {
    ResetLocalization();
    auto& mgr = LocalizationManager::Get();
    mgr.SetString("en", "echo", "{x} and {x} again");
    std::unordered_map<std::string, std::string> params;
    params["x"] = "42";
    ENJIN_EXPECT_STR_EQ(mgr.Format("echo", params).c_str(), "42 and 42 again");
}

ENJIN_TEST(Format, MissingParamLeftAsIs) {
    ResetLocalization();
    auto& mgr = LocalizationManager::Get();
    mgr.SetString("en", "tmpl", "Hello {name}");
    std::unordered_map<std::string, std::string> params; // empty
    std::string result = mgr.Format("tmpl", params);
    // Missing param — placeholder left in string
    ENJIN_EXPECT_TRUE(result.find("{name}") != std::string::npos);
}

ENJIN_TEST(Format, NoParamsNoChange) {
    ResetLocalization();
    auto& mgr = LocalizationManager::Get();
    mgr.SetString("en", "plain", "No params here");
    std::unordered_map<std::string, std::string> params;
    ENJIN_EXPECT_STR_EQ(mgr.Format("plain", params).c_str(), "No params here");
}

// ===========================================================================
// JSON Round-Trip
// ===========================================================================

ENJIN_TEST(JSON, SaveAndLoadRoundTrip) {
    ResetLocalization();
    auto& mgr = LocalizationManager::Get();
    mgr.SetString("en", "hello", "Hello");
    mgr.SetString("en", "bye", "Goodbye");
    mgr.SetString("fr", "hello", "Bonjour");
    mgr.SetString("fr", "bye", "Au revoir");

    std::string path = "test_localization_tmp.json";
    ENJIN_EXPECT_TRUE(mgr.SaveToJSON(path));

    mgr.Clear();
    ENJIN_EXPECT_TRUE(mgr.LoadFromJSON(path));

    ENJIN_EXPECT_STR_EQ(mgr.GetString("hello").c_str(), "Hello");
    mgr.SetLocale("fr");
    ENJIN_EXPECT_STR_EQ(mgr.GetString("hello").c_str(), "Bonjour");
    ENJIN_EXPECT_STR_EQ(mgr.GetString("bye").c_str(), "Au revoir");

    std::remove(path.c_str());
}

ENJIN_TEST(JSON, LoadNonexistentFails) {
    ResetLocalization();
    ENJIN_EXPECT_FALSE(LocalizationManager::Get().LoadFromJSON("nonexistent_file_12345.json"));
}

// ===========================================================================
// CSV Round-Trip
// ===========================================================================

ENJIN_TEST(CSV, SaveAndLoadRoundTrip) {
    ResetLocalization();
    auto& mgr = LocalizationManager::Get();
    mgr.SetString("en", "item", "Sword");
    mgr.SetString("en", "desc", "A sharp blade");
    mgr.SetString("ja", "item", "Katana");
    mgr.SetString("ja", "desc", "Sharp blade");

    std::string path = "test_localization_tmp.csv";
    ENJIN_EXPECT_TRUE(mgr.SaveToCSV(path));

    mgr.Clear();
    ENJIN_EXPECT_TRUE(mgr.LoadFromCSV(path));

    ENJIN_EXPECT_STR_EQ(mgr.GetString("item").c_str(), "Sword");
    mgr.SetLocale("ja");
    ENJIN_EXPECT_STR_EQ(mgr.GetString("item").c_str(), "Katana");

    std::remove(path.c_str());
}

ENJIN_TEST(CSV, LoadNonexistentFails) {
    ResetLocalization();
    ENJIN_EXPECT_FALSE(LocalizationManager::Get().LoadFromCSV("nonexistent_12345.csv"));
}


// ===========================================================================
// Loading a table that is not a loose file, and booting from a project block.
//
// LocalizationManager looked complete and had ZERO consumers: nothing in the
// editor or either player ever called SetLocale or a loader, so the table was
// always empty and every lookup returned its own key back. Two things blocked
// it.
//
// First, both loaders were std::ifstream-only. The web player has no loose
// files and AssetReader hands back a byte vector, not a path, so a shipped
// game physically could not load a table however good the table was.
//
// Second, nothing read a project's localization settings at all.
// ApplyLocalizationSettings is now the single boot call all three runtimes make.
// ===========================================================================

namespace {

// The manager is a process-wide singleton, so a test that changes the locale
// has to put it back or it leaks into the next one.
struct LocalizationScope {
    LocalizationScope() { ResetLocalization(); }
    ~LocalizationScope() { ResetLocalization(); }
};

bool LoadCsvFromMemory(const std::string& csv) {
    return LocalizationManager::Get().LoadFromMemory(
        reinterpret_cast<const Enjin::u8*>(csv.data()), csv.size(),
        LocalizationManager::TableFormat::Csv, "<test>");
}

} // namespace

ENJIN_TEST(Memory, ACsvTableLoadsWithoutTouchingTheFilesystem) {
    // Arrange: exactly the bytes AssetReader hands back inside a packed game.
    LocalizationScope scope;
    const std::string csv = "key,en,fr\ngreeting,Hello,Bonjour\nquit,Quit,Quitter\n";

    // Act
    const bool ok = LoadCsvFromMemory(csv);

    // Assert
    ENJIN_ASSERT_TRUE(ok);
    LocalizationManager& mgr = LocalizationManager::Get();
    mgr.SetLocale("fr");
    ENJIN_EXPECT_STR_EQ(mgr.GetString("greeting").c_str(), "Bonjour");
    ENJIN_EXPECT_STR_EQ(mgr.GetString("quit").c_str(), "Quitter");
}

ENJIN_TEST(Memory, AJsonTableLoadsWithoutTouchingTheFilesystem) {
    // Arrange
    LocalizationScope scope;
    const std::string json = "{\"en\":{\"greeting\":\"Hello\"},\"fr\":{\"greeting\":\"Bonjour\"}}";

    // Act
    const bool ok = LocalizationManager::Get().LoadFromMemory(
        reinterpret_cast<const Enjin::u8*>(json.data()), json.size(),
        LocalizationManager::TableFormat::Json, "<test>");

    // Assert
    ENJIN_ASSERT_TRUE(ok);
    LocalizationManager::Get().SetLocale("fr");
    ENJIN_EXPECT_STR_EQ(LocalizationManager::Get().GetString("greeting").c_str(), "Bonjour");
}

ENJIN_TEST(Memory, AnEmptyBufferIsRejectedRatherThanParsedAsAnEmptyTable) {
    // Arrange: a missing pak entry reads back empty, and silently "succeeding"
    // there would look like a project with no strings rather than a load error.
    LocalizationScope scope;

    // Act / Assert
    ENJIN_EXPECT_FALSE(LocalizationManager::Get().LoadFromMemory(
        nullptr, 0, LocalizationManager::TableFormat::Csv, "<test>"));
}

ENJIN_TEST(Memory, TheFileAndMemoryLoadersAgree) {
    // Arrange: both entry points must run the same parser, or one table would
    // mean one thing in the editor and another in a shipped game.
    const std::string csv = "key,en,fr\ngreeting,Hello,Bonjour\n";
    const std::string path = "loc_agree_test.csv";
    {
        std::ofstream f(path);
        f << csv;
    }

    // Act
    ResetLocalization();
    ENJIN_ASSERT_TRUE(LocalizationManager::Get().LoadFromCSV(path));
    LocalizationManager::Get().SetLocale("fr");
    const std::string fromFile = LocalizationManager::Get().GetString("greeting");

    ResetLocalization();
    ENJIN_ASSERT_TRUE(LoadCsvFromMemory(csv));
    LocalizationManager::Get().SetLocale("fr");
    const std::string fromMemory = LocalizationManager::Get().GetString("greeting");

    std::remove(path.c_str());
    ResetLocalization();

    // Assert
    ENJIN_EXPECT_STR_EQ(fromFile.c_str(), fromMemory.c_str());
    ENJIN_EXPECT_STR_EQ(fromFile.c_str(), "Bonjour");
}

ENJIN_TEST(ProjectBoot, ALocalizationBlockRegistersItsLocalesAndSelectsTheDefault) {
    // Arrange: the block as it appears in a .enjinproject, and verbatim in an
    // exported game's manifest.
    LocalizationScope scope;

    // Act
    Enjin::GUI::ApplyLocalizationSettings("{\"defaultLocale\":\"fr\",\"locales\":[{\"code\":\"en\",\"name\":\"English\"},{\"code\":\"fr\",\"name\":\"Francais\"}]}", std::string(), nullptr);

    // Assert
    ENJIN_EXPECT_STR_EQ(LocalizationManager::Get().GetCurrentLocale().c_str(), "fr");
    const std::vector<Locale> locales = LocalizationManager::Get().GetAvailableLocales();
    bool sawFrenchName = false;
    for (const auto& l : locales) {
        if (l.code == "fr" && l.name == "Francais") sawFrenchName = true;
    }
    ENJIN_EXPECT_TRUE(sawFrenchName);
}

ENJIN_TEST(ProjectBoot, TheLocaleIsSelectedAfterTablesLoad) {
    // Arrange: order matters. Selecting the locale before the tables exist
    // would leave the first frame resolving against an empty table.
    LocalizationScope scope;
    const std::string path = "loc_boot_test.csv";
    {
        std::ofstream f(path);
        f << "key,en,fr\ngreeting,Hello,Bonjour\n";
    }

    // Act
    const Enjin::u32 loaded =
        Enjin::GUI::ApplyLocalizationSettings("{\"defaultLocale\":\"fr\",\"tables\":[\"loc_boot_test.csv\"]}", std::string(), nullptr);
    const std::string got = LocalizationManager::Get().GetString("greeting");
    std::remove(path.c_str());

    // Assert
    ENJIN_EXPECT_TRUE(loaded == 1u);
    ENJIN_EXPECT_STR_EQ(got.c_str(), "Bonjour");
}

ENJIN_TEST(ProjectBoot, AnAbsentBlockLeavesEveryLookupOnItsFallback) {
    // Arrange: this is every existing untranslated project, and it must behave
    // exactly as it did before any of this existed.
    LocalizationScope scope;

    // Act
    const Enjin::u32 loaded = Enjin::GUI::ApplyLocalizationSettings("", std::string(), nullptr);

    // Assert: the call-site literal survives, and a bare key returns itself.
    ENJIN_EXPECT_TRUE(loaded == 0u);
    ENJIN_EXPECT_STR_EQ(
        LocalizationManager::Get().GetString("menu.new_game", "New Game").c_str(), "New Game");
    ENJIN_EXPECT_STR_EQ(
        LocalizationManager::Get().GetString("menu.new_game").c_str(), "menu.new_game");
}

ENJIN_TEST(ProjectBoot, AMalformedBlockIsIgnoredRatherThanCrashing) {
    // Arrange: a hand-edited .enjinproject is a real thing.
    LocalizationScope scope;

    // Act / Assert
    ENJIN_EXPECT_TRUE(
        Enjin::GUI::ApplyLocalizationSettings("{not json", std::string(), nullptr) == 0u);
    ENJIN_EXPECT_TRUE(
        Enjin::GUI::ApplyLocalizationSettings("[]", std::string(), nullptr) == 0u);
    ENJIN_EXPECT_TRUE(
        Enjin::GUI::ApplyLocalizationSettings("{\"tables\":[\"missing_98765.csv\"]}", std::string(), nullptr) == 0u);
}

ENJIN_TEST_MAIN()
