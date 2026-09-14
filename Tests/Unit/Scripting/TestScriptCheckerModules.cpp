// What --check-scripts counts as a module, and what it counts as an error.
//
// FR-0002 (Marty, 2026-09-13). `#include` in this engine is TEXTUAL, and the
// documented pattern for anything larger than one file is a single module
// assembled from several files with one entry point. The checker used to
// compile every .as as its own module, so every non-entry file failed on types
// its siblings declare. Measured across five real projects: Deep reported 138
// errors and had none, every one `Identifier 'X' is not a data type`. Its entry
// point compiled clean. The tool called the soundest multi-file project in the
// corpus the most broken one, and a real error would have arrived as number 139.
//
// Two things this pins, because both were got wrong on the way:
//
//  - An include target that is not on disk is NOT automatically missing. The
//    engine ships TegeBehavior.as, Math.as, StrUtil.as and the rest inside
//    itself. The first version of the missing-include check did not know that
//    and reported eleven errors across three real projects, every one an engine
//    api script used exactly as documented. Replacing 138 false errors with 11
//    would not have been a fix.
//  - A SCENE attaching a script file that does not exist is a real error and
//    was the one genuine defect in the whole corpus: a scene still pointed at a
//    script that had been moved to a parked folder, and the only symptom was
//    "class not found" at play time, in a different file from the one at fault.

#include "EnjinTest.h"
#include "Enjin/Scripting/ScriptChecker.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

using namespace Enjin;
using namespace Enjin::Scripting;

namespace {

namespace fs = std::filesystem;

fs::path MakeProject(const char* leaf) {
    fs::path dir = fs::temp_directory_path() / "enjin_checker_modules" / leaf;
    std::error_code ec;
    fs::remove_all(dir, ec);
    fs::create_directories(dir / "scripts", ec);
    fs::create_directories(dir / "scenes", ec);
    return dir;
}

void Write(const fs::path& p, const std::string& text) {
    std::ofstream f(p, std::ios::trunc);
    f << text;
}

bool HasSkipped(const ScriptCheckResult& r, const std::string& leaf) {
    for (const auto& s : r.skipped) {
        if (fs::path(s.file).filename().string() == leaf) return true;
    }
    return false;
}

bool HasErrorContaining(const ScriptCheckResult& r, const std::string& needle) {
    for (const auto& i : r.issues) {
        if (i.isError && i.message.find(needle) != std::string::npos) return true;
    }
    return false;
}

}  // namespace

// The shape that produced 138 false errors: one entry point, one sibling it
// includes, and a type declared in the sibling and used by the entry.
ENJIN_TEST(ScriptCheckerModules, AnIncludedFileIsNotCompiledAsItsOwnModule) {
    const fs::path dir = MakeProject("included");

    Write(dir / "scripts" / "Parts.as",
          "class Part {\n"
          "    int value = 7;\n"
          "}\n");

    // Uses Part, which only exists because of the textual include. Compiled
    // alone, Parts.as is fine; compiled alone, Entry.as is fine only WITH the
    // include -- and Parts.as must not be compiled alone at all, because in a
    // real multi-file module the dependency runs both ways.
    Write(dir / "scripts" / "Entry.as",
          "#include \"Parts.as\"\n"
          "class Entry : TegeBehavior {\n"
          "    Part p;\n"
          "    void OnStart() { p.value = 1; }\n"
          "}\n");

    const ScriptCheckResult r = CheckProjectScripts(dir.string());
    std::printf("    modules=%u skipped=%zu errors=%u\n",
                r.modulesChecked, r.skipped.size(), r.errorCount);
    for (const auto& i : r.issues) {
        std::printf("      %s: %s\n", i.file.c_str(), i.message.c_str());
    }

    ENJIN_EXPECT_TRUE(r.fatal.empty());
    ENJIN_EXPECT_EQ(r.modulesChecked, 1u);
    ENJIN_EXPECT_TRUE(HasSkipped(r, "Parts.as"));
    ENJIN_EXPECT_EQ(r.errorCount, 0u);

    std::error_code ec;
    fs::remove_all(dir, ec);
}

// Including an engine api script is correct and must not be reported. There is
// no file on disk for these; the engine carries them.
ENJIN_TEST(ScriptCheckerModules, IncludingAnEmbeddedApiScriptIsNotAMissingFile) {
    const fs::path dir = MakeProject("embedded");

    Write(dir / "scripts" / "UsesApi.as",
          "#include \"Math.as\"\n"
          "class UsesApi : TegeBehavior {\n"
          "    void OnStart() {}\n"
          "}\n");

    const ScriptCheckResult r = CheckProjectScripts(dir.string());
    std::printf("    modules=%u errors=%u\n", r.modulesChecked, r.errorCount);
    for (const auto& i : r.issues) {
        std::printf("      %s: %s\n", i.file.c_str(), i.message.c_str());
    }

    ENJIN_EXPECT_TRUE(!HasErrorContaining(r, "does not exist"));

    std::error_code ec;
    fs::remove_all(dir, ec);
}

// An include naming nothing at all IS an error: without this a typo silently
// removes a file from the module it was meant to join.
ENJIN_TEST(ScriptCheckerModules, AnIncludeNamingNothingIsAnError) {
    const fs::path dir = MakeProject("typo");

    Write(dir / "scripts" / "Typo.as",
          "#include \"NoSuchSibling.as\"\n"
          "class Typo : TegeBehavior { void OnStart() {} }\n");

    const ScriptCheckResult r = CheckProjectScripts(dir.string());
    std::printf("    errors=%u\n", r.errorCount);
    ENJIN_EXPECT_TRUE(HasErrorContaining(r, "NoSuchSibling.as"));

    std::error_code ec;
    fs::remove_all(dir, ec);
}

// A scene attaching a script that is not on disk. The real one found in the
// corpus, and the failure this tool exists to move earlier than play time.
ENJIN_TEST(ScriptCheckerModules, ASceneAttachingAMissingScriptIsAnError) {
    const fs::path dir = MakeProject("scene");

    Write(dir / "scripts" / "Present.as",
          "class Present : TegeBehavior { void OnStart() {} }\n");

    Write(dir / "scenes" / "Main.enjin",
          "{\n"
          "  \"version\": \"1.0\",\n"
          "  \"entities\": [\n"
          "    { \"id\": 1, \"scriptComponent\": { \"scripts\": [\n"
          "        { \"class\": \"Present\", \"path\": \"scripts/Present.as\" },\n"
          "        { \"class\": \"Parked\",  \"path\": \"scripts/Parked.as\" }\n"
          "    ] } }\n"
          "  ]\n"
          "}\n");

    const ScriptCheckResult r = CheckProjectScripts(dir.string());
    std::printf("    errors=%u\n", r.errorCount);
    for (const auto& i : r.issues) {
        std::printf("      %s: %s\n", i.file.c_str(), i.message.c_str());
    }

    ENJIN_EXPECT_TRUE(HasErrorContaining(r, "scripts/Parked.as"));
    // And the one that IS there must not be reported.
    ENJIN_EXPECT_TRUE(!HasErrorContaining(r, "scripts/Present.as"));

    std::error_code ec;
    fs::remove_all(dir, ec);
}

ENJIN_TEST_MAIN()
