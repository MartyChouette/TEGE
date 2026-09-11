// View > Generate Documentation had never produced a file, and said it had.
//
// Three faults, each enough on its own:
//
//   1. The menu called GenerateAll() with no argument. Its asIScriptEngine*
//      parameter has a default of nullptr, and the script-API half is inside
//      `if (scriptEngine)`, so the largest document it makes -- a reference to
//      1200+ registered functions -- was skipped with nothing logged.
//   2. Output went to the bare relative "docs/generated", resolved against the
//      process CWD, which for the editor is its own exe directory. A success
//      would have written into build/bin/Release/docs/generated.
//   3. GenerateComponentDocs looked for "Engine/include/Enjin/ECS/Components"
//      relative to that same CWD, and its "fall back to the current working
//      directory" retry checked the IDENTICAL path. So it failed on every run
//      outside a shell sitting in the repo root, and because GenerateAll ANDs the
//      results, one unavailable input reported the whole batch as failed.
//
// The result a person saw was a menu item that did nothing, with the reason in a
// log panel they had no reason to open. These tests hold the generator to the
// three things that were wrong: it finds its source root from anywhere, it
// reports a missing optional input as skipped rather than failed, and it refuses
// to quietly generate less than it was asked for.
#include "EnjinTest.h"
#include "Enjin/Editor/DocGenerator.h"
#include "Enjin/Scripting/ScriptEngine.h"
#include "Enjin/Scripting/ScriptBindings.h"
#include "Enjin/Logging/Log.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

using namespace Enjin;
using namespace Enjin::Editor;
using namespace Enjin::Scripting;
namespace fs = std::filesystem;

namespace {

fs::path TempOut(const char* leaf) {
    fs::path dir = fs::temp_directory_path() / "enjin_docgen_test" / leaf;
    std::error_code ec;
    fs::remove_all(dir, ec);
    fs::create_directories(dir, ec);
    return dir;
}

std::string ReadAll(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    if (!f) return {};
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

} // namespace

ENJIN_TEST(DocGenerator, ScriptApiReferenceIsGeneratedAndComplete) {
    // Arrange: a fully-bound throwaway engine, exactly what the editor builds for
    // reflection, and a temp output directory.
    Logger::Get().Initialize("test_docgen.log");
    const fs::path out = TempOut("api");

    ScriptEngine se;
    ENJIN_ASSERT_TRUE(se.Initialize());
    RegisterAllBindings(se.GetASEngine());

    DocGenerator gen;
    gen.SetOutputDirectory(out.string());

    // Act
    const bool ok = gen.GenerateScriptAPIDocs(se.GetASEngine());

    // Assert: the file exists and lists every function the engine registered.
    ENJIN_ASSERT_TRUE(ok);
    const std::string md = ReadAll(out / "SCRIPTING_API.md");
    ENJIN_ASSERT_TRUE(!md.empty());

    const asUINT registered = se.GetASEngine()->GetGlobalFunctionCount();
    ENJIN_ASSERT_TRUE(registered > 1000);

    // One bullet per function, so the count in the footer must match the engine.
    std::ostringstream expect;
    expect << "*Generated " << registered << " functions across ";
    ENJIN_EXPECT_TRUE(md.find(expect.str()) != std::string::npos);

    // And spot-check that a few real names actually made it in, because a footer
    // count is satisfied by a file full of the wrong names.
    ENJIN_EXPECT_TRUE(md.find("Audio_GetTime") != std::string::npos);
    ENJIN_EXPECT_TRUE(md.find("DataAsset_GetArrayLength") != std::string::npos);
    ENJIN_EXPECT_TRUE(md.find("Camera_MakeActive") != std::string::npos);

    se.Shutdown();
}

ENJIN_TEST(DocGenerator, GenerateAllWithoutAScriptEngineIsAFailureNotASkip) {
    // The exact mistake the menu made. Passing nothing must not look like success:
    // the API reference is most of the value here, and dropping it silently is how
    // this shipped broken.
    const fs::path out = TempOut("noengine");
    DocGenerator gen;
    gen.SetOutputDirectory(out.string());

    const bool ok = gen.GenerateAll();   // no engine, the default argument

    ENJIN_EXPECT_FALSE(ok);
    ENJIN_EXPECT_TRUE(gen.GetLastError().find("script engine") != std::string::npos);
    ENJIN_EXPECT_FALSE(fs::exists(out / "SCRIPTING_API.md"));
}

ENJIN_TEST(DocGenerator, ComponentDocsFindTheSourceTreeFromAnywhere) {
    // The generator must not care what the CWD is. The test binary runs from the
    // build tree, which is the situation that used to fail.
    const fs::path out = TempOut("components");
    DocGenerator gen;
    gen.SetOutputDirectory(out.string());
    gen.SetSourceRoot(ENJIN_REPO_ROOT);

    const bool ok = gen.GenerateComponentDocs();

    ENJIN_ASSERT_TRUE(ok);
    ENJIN_EXPECT_TRUE(gen.GetSkippedReason().empty());
    const std::string md = ReadAll(out / "COMPONENTS.md");
    ENJIN_ASSERT_TRUE(!md.empty());
    // TransformComponent is the one component every scene has.
    ENJIN_EXPECT_TRUE(md.find("TransformComponent") != std::string::npos);
}

ENJIN_TEST(DocGenerator, AMissingSourceTreeIsSkippedWithAReasonNotFailed) {
    // An installed editor has no engine headers, and that is not a malfunction.
    // Reporting it as a failure is what made the whole batch return false, so the
    // distinction is the fix, not a nicety.
    const fs::path out = TempOut("nosource");
    DocGenerator gen;
    gen.SetOutputDirectory(out.string());
    // A directory that exists and definitely holds no engine source.
    gen.SetSourceRoot(out.string());

    const bool ok = gen.GenerateComponentDocs();

    ENJIN_EXPECT_TRUE(ok);                                  // not a failure
    ENJIN_EXPECT_TRUE(!gen.GetSkippedReason().empty());     // but it says so
    ENJIN_EXPECT_TRUE(gen.GetSkippedReason().find("source tree") != std::string::npos);
    ENJIN_EXPECT_FALSE(fs::exists(out / "COMPONENTS.md"));
}

ENJIN_TEST(DocGenerator, EveryDocumentIsWrittenWhereItWasAsked) {
    // The CWD bug in one assertion: whatever path the caller sets is where the
    // files land, and GetGeneratedFiles names them all so a caller can say where.
    Logger::Get().Initialize("test_docgen.log");
    const fs::path out = TempOut("all");

    ScriptEngine se;
    ENJIN_ASSERT_TRUE(se.Initialize());
    RegisterAllBindings(se.GetASEngine());

    DocGenerator gen;
    gen.SetOutputDirectory(out.string());
    gen.SetSourceRoot(ENJIN_REPO_ROOT);

    const bool ok = gen.GenerateAll(se.GetASEngine());
    se.Shutdown();

    ENJIN_ASSERT_TRUE(ok);
    // components + script api + visual script nodes + data assets + index
    ENJIN_EXPECT_EQ(gen.GetGeneratedFiles().size(), static_cast<size_t>(5));
    for (const std::string& f : gen.GetGeneratedFiles()) {
        ENJIN_EXPECT_TRUE(fs::exists(f));
        // Under the requested directory, not the working directory.
        ENJIN_EXPECT_TRUE(f.find(out.filename().string()) != std::string::npos);
    }
    ENJIN_EXPECT_TRUE(fs::exists(out / "INDEX.md"));
}

ENJIN_TEST_MAIN()
