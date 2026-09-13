// Every script we ship as an example must compile.
//
// The demo scripts are the second place a beginner learns this API from, after
// the docs -- and unlike the docs, they are handed to people as a working
// project to open and press Play on. A demo whose script fails to compile is
// worse than a missing demo: the error points into a file the person did not
// write, in a project we told them was finished.
//
// This exists because a script in Examples/RoomAcoustics was written calling
// Entity_FindByName, Camera_GetPosition and Log -- three functions that have
// never existed in this engine. Nothing would have caught it. TestDocSamples
// compiles the snippets in docs/, tools/check_doc_api.py text-scans names in
// markdown, and neither one has ever looked at a .as file on disk. The same
// mistake in a shipped example was simply not checked for.
//
// Same method as TestDocSamples, and for the same reason: the only complete
// check for "does this API exist" is the real compiler with the real bindings
// registered. A name scan cannot see a wrong argument count or a type that only
// exists in the author's head.
#include "EnjinTest.h"
#include "Enjin/Scripting/ScriptEngine.h"
#include "Enjin/Scripting/ScriptBindings.h"
#include "Enjin/Logging/Log.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using namespace Enjin;
using namespace Enjin::Scripting;
namespace fs = std::filesystem;

namespace {

std::string ReadFile(const fs::path& p) {
    std::ifstream f(p);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

// Script module names are parentDir_stem, and anything creating instances has
// to derive the name the same way. Matching it here means a module that only
// compiles under some other name fails where it should.
std::string ModuleName(const fs::path& p) {
    return p.parent_path().filename().string() + "_" + p.stem().string();
}

// Skipped, with a reason for each.
bool ShouldSkip(const fs::path& p, std::string& why) {
    const std::string path = p.generic_string();

    // A project's own scripts/enjin_api/ overrides the embedded copies, so these
    // ARE the library rather than a user of it -- they are compiled as includes
    // into other modules, not as modules of their own, and TegeBehavior.as in
    // particular is the base class every other script gets injected with.
    if (path.find("/enjin_api/") != std::string::npos) {
        why = "engine API library, compiled as an include";
        return true;
    }

    // BuildPipeline emits loose scripts/ beside every exported game, so a built
    // demo carries a byte-identical copy of its own source. Compiling those
    // again reports one real failure several times and makes the count depend on
    // how recently somebody pressed Build.
    if (path.find("/Build/") != std::string::npos) {
        why = "copy emitted beside an exported build";
        return true;
    }

    return false;
}

} // namespace

ENJIN_TEST(ExampleScripts, EveryShippedExampleScriptCompiles) {
    // Arrange
    const fs::path examples = fs::path(ENJIN_REPO_ROOT) / "Examples";
    if (!fs::is_directory(examples)) {
        ENJIN_SKIP("Examples/ not found next to the repo root");
        return;
    }

    std::vector<fs::path> scripts;
    for (const auto& entry : fs::recursive_directory_iterator(examples)) {
        if (!entry.is_regular_file()) continue;
        if (entry.path().extension() != ".as") continue;
        scripts.push_back(entry.path());
    }
    std::sort(scripts.begin(), scripts.end());

    // A finder that stops finding anything is a silent pass waiting to happen.
    ENJIN_ASSERT_TRUE(scripts.size() >= 10);

    // The AngelScript message callback logs through the engine logger, so
    // without this a compile failure has no text at all.
    Logger::Get().Initialize("test_example_scripts.log");

    ScriptEngine engine;
    ENJIN_ASSERT_TRUE(engine.Initialize());
    RegisterAllBindings(engine.GetASEngine());

    // The include callback resolves #include against a script root, and every
    // example keeps its scripts in its own scripts/ directory.
    int compiled = 0, skipped = 0, failed = 0;

    // Act
    for (const fs::path& p : scripts) {
        std::string why;
        if (ShouldSkip(p, why)) {
            std::printf("    skip  %s  (%s)\n",
                        fs::relative(p, examples).generic_string().c_str(), why.c_str());
            ++skipped;
            continue;
        }

        engine.SetScriptDirectory(p.parent_path().string());

        const std::string source = ReadFile(p);
        const std::string rel = fs::relative(p, examples).generic_string();
        std::printf("    %s\n", rel.c_str());
        std::fflush(stdout);   // the compiler logs through a different stream

        if (engine.CompileScriptFromMemory(ModuleName(p), source)) {
            ++compiled;
            continue;
        }

        ++failed;
        char buf[1024];
        std::snprintf(buf, sizeof(buf), "Examples/%s does not compile: %s",
                      rel.c_str(), engine.GetLastError().c_str());
        EnjinTest::ReportFailureMsg(__FILE__, __LINE__, buf);
    }

    // Assert
    std::printf("    %d compiled, %d skipped, %d failed, of %zu scripts\n",
                compiled, skipped, failed, scripts.size());
    ENJIN_EXPECT_EQ(failed, 0);
    ENJIN_EXPECT_TRUE(compiled >= 8);

    engine.Shutdown();
}

// The one that goes with the demo, named so a failure says what broke.
ENJIN_TEST(ExampleScripts, TheRoomAcousticsDemoHasAWorkingTrigger) {
    // Arrange
    const fs::path script =
        fs::path(ENJIN_REPO_ROOT) / "Examples/RoomAcoustics/scripts/RoomClap.as";
    if (!fs::exists(script)) {
        ENJIN_SKIP("RoomClap.as not found");
        return;
    }

    Logger::Get().Initialize("test_example_scripts.log");
    ScriptEngine engine;
    ENJIN_ASSERT_TRUE(engine.Initialize());
    RegisterAllBindings(engine.GetASEngine());
    engine.SetScriptDirectory(script.parent_path().string());

    // Act
    const bool ok = engine.CompileScriptFromMemory("scripts_RoomClap", ReadFile(script));

    // Assert: a reverb demo with no way to make a sound demonstrates nothing --
    // three looping drones fill the gap the tail lives in, so the room's answer
    // is never actually audible. The trigger is the demo.
    if (!ok) std::printf("    %s\n", engine.GetLastError().c_str());
    ENJIN_EXPECT_TRUE(ok);

    // The clip it fires has to be on disk. A trigger wired to a missing file is
    // the same silence as no trigger, arrived at by a longer route.
    const fs::path clip =
        fs::path(ENJIN_REPO_ROOT) / "Examples/RoomAcoustics/assets/clap.wav";
    ENJIN_EXPECT_TRUE(fs::exists(clip));

    engine.Shutdown();
}

ENJIN_TEST_MAIN()
