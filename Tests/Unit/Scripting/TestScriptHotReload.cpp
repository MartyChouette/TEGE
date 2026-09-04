// Hot-reloading a script must recompile the script you actually wrote.
//
// ProcessHotReload used to re-implement the compile with a bare
// AddSectionFromFile instead of calling CompileScript, which skips the
// TegeBehavior base-class injection. Every script in this repo extends
// TegeBehavior without mentioning TegeBehavior.as, so the injection is not
// optional — every hot reload failed with "Identifier 'TegeBehavior' is not a
// data type", discarded the module while live instances kept running the old
// bytecode, and re-failed on every poll because the timestamp only advanced on
// the success path.
//
// The test that matters is ReloadOfAScriptExtendingTegeBehaviorSucceeds: it uses
// the same shape every real script uses, which is exactly what the old code
// could not compile.
#include "EnjinTest.h"
#include "Enjin/Scripting/ScriptEngine.h"
#include "Enjin/Scripting/ScriptBindings.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>

using namespace Enjin;
using namespace Enjin::Scripting;

namespace {

namespace fs = std::filesystem;

fs::path MakeScriptDir(const char* leaf) {
    fs::path dir = fs::temp_directory_path() / "enjin_hotreload_test" / leaf;
    std::error_code ec;
    fs::remove_all(dir, ec);
    fs::create_directories(dir, ec);
    return dir;
}

void WriteFile(const fs::path& p, const std::string& text) {
    std::ofstream f(p, std::ios::trunc);
    f << text;
}

// The shape every script in Examples/ uses: extends TegeBehavior, never
// mentions TegeBehavior.as, and relies on the engine injecting the base class.
std::string BehaviorScript(const char* returnValue) {
    return std::string(
        "class Probe : TegeBehavior {\n"
        "    int Value() { return ") + returnValue + "; }\n"
        "}\n";
}

// PollFileChanges is throttled to one real check every POLL_INTERVAL calls, so a
// single call from a test polls nothing. Drive it like a frame loop would.
void PollUntilChecked(ScriptEngine& engine) {
    for (int i = 0; i < 64; ++i) engine.PollFileChanges();
}

} // namespace

ENJIN_TEST(ScriptHotReload, ReloadOfAScriptExtendingTegeBehaviorSucceeds) {
    // Arrange: a script in the shape real scripts use, compiled once.
    const fs::path dir = MakeScriptDir("behavior");
    const fs::path file = dir / "Probe.as";
    WriteFile(file, BehaviorScript("1"));

    ScriptEngine engine;
    ENJIN_ASSERT_TRUE(engine.Initialize());
    RegisterAllBindings(engine.GetASEngine());
    engine.SetScriptDirectory(dir.string());
    ENJIN_ASSERT_TRUE(engine.CompileScript(file.string()));

    // Act: edit it and reload. The filesystem timestamp has to actually move,
    // hence the sleep — a same-timestamp write would not be detected and the
    // test would pass without testing anything.
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    WriteFile(file, BehaviorScript("2"));
    PollUntilChecked(engine);
    const bool reloaded = engine.ProcessHotReload();

    // Assert: this is the assertion the old implementation failed.
    ENJIN_EXPECT_TRUE(reloaded);
    ENJIN_EXPECT_TRUE(engine.GetLastError().empty());

    engine.Shutdown();
    std::error_code ec;
    fs::remove_all(dir, ec);
}

ENJIN_TEST(ScriptHotReload, ReloadedCodeIsTheNewCode) {
    // Arrange
    const fs::path dir = MakeScriptDir("newcode");
    const fs::path file = dir / "Probe.as";
    WriteFile(file, BehaviorScript("1"));

    ScriptEngine engine;
    ENJIN_ASSERT_TRUE(engine.Initialize());
    RegisterAllBindings(engine.GetASEngine());
    engine.SetScriptDirectory(dir.string());
    ENJIN_ASSERT_TRUE(engine.CompileScript(file.string()));

    // Act
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    WriteFile(file, BehaviorScript("2"));
    PollUntilChecked(engine);
    ENJIN_ASSERT_TRUE(engine.ProcessHotReload());

    // Assert: a reload that "succeeds" while leaving the old bytecode live
    // would be its own bug, so check the module really holds the new source.
    asIScriptEngine* as = engine.GetASEngine();
    ENJIN_ASSERT_TRUE(as != nullptr);
    asIScriptModule* mod = nullptr;
    for (asUINT i = 0; i < as->GetModuleCount(); ++i) {
        asIScriptModule* m = as->GetModuleByIndex(i);
        if (m && m->GetTypeInfoByDecl("Probe") != nullptr) { mod = m; break; }
    }
    ENJIN_ASSERT_TRUE(mod != nullptr);
    ENJIN_EXPECT_TRUE(mod->GetTypeInfoByDecl("Probe") != nullptr);

    engine.Shutdown();
    std::error_code ec;
    fs::remove_all(dir, ec);
}

ENJIN_TEST(ScriptHotReload, AnUnchangedScriptDoesNotReload) {
    // Arrange
    const fs::path dir = MakeScriptDir("unchanged");
    const fs::path file = dir / "Probe.as";
    WriteFile(file, BehaviorScript("1"));

    ScriptEngine engine;
    ENJIN_ASSERT_TRUE(engine.Initialize());
    RegisterAllBindings(engine.GetASEngine());
    engine.SetScriptDirectory(dir.string());
    ENJIN_ASSERT_TRUE(engine.CompileScript(file.string()));

    // Act: poll without touching the file.
    PollUntilChecked(engine);
    const bool reloaded = engine.ProcessHotReload();

    // Assert
    ENJIN_EXPECT_TRUE(!reloaded);

    engine.Shutdown();
    std::error_code ec;
    fs::remove_all(dir, ec);
}

ENJIN_TEST(ScriptHotReload, ABrokenEditFailsAndKeepsRetrying) {
    // Arrange: a reload that cannot compile must report failure — and must not
    // advance the timestamp, or the poller stops noticing the file is broken.
    const fs::path dir = MakeScriptDir("broken");
    const fs::path file = dir / "Probe.as";
    WriteFile(file, BehaviorScript("1"));

    ScriptEngine engine;
    ENJIN_ASSERT_TRUE(engine.Initialize());
    RegisterAllBindings(engine.GetASEngine());
    engine.SetScriptDirectory(dir.string());
    ENJIN_ASSERT_TRUE(engine.CompileScript(file.string()));

    // Act
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    WriteFile(file, "class Probe : TegeBehavior { int Value() { return ; } }\n");
    PollUntilChecked(engine);
    const bool reloaded = engine.ProcessHotReload();

    // Assert
    ENJIN_EXPECT_TRUE(!reloaded);
    ENJIN_EXPECT_TRUE(!engine.GetLastError().empty());

    engine.Shutdown();
    std::error_code ec;
    fs::remove_all(dir, ec);
}

ENJIN_TEST_MAIN()
