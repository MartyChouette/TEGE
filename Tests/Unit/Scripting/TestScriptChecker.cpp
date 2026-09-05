// Compiling a project's scripts without launching anything.
//
// Script errors reached the console and nowhere else, so a broken script
// surfaced at PLAY time as "class not found" in whatever scene referenced it --
// usually a different file from the one with the mistake. There was no way to
// ask "do these compile?" offline.
//
// One project's write-up records the consequence: it ships its own Python
// linter that checks calls against an API stub, and says plainly that it
// "cannot catch real type errors". This checks with the compiler that actually
// runs the scripts, so it can.
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

// A throwaway project on disk. Integration tests clean up after themselves.
struct TempProject {
    fs::path root;

    explicit TempProject(const char* name) {
        root = fs::temp_directory_path() / name;
        std::error_code ec;
        fs::remove_all(root, ec);
        fs::create_directories(root / "scripts", ec);
        std::ofstream(root / "Test.enjinproject") << R"({"version":"1.0","name":"Test"})";
    }

    ~TempProject() {
        std::error_code ec;
        fs::remove_all(root, ec);
    }

    void Script(const char* name, const std::string& body) const {
        std::ofstream(root / "scripts" / name) << body;
    }

    std::string Path() const { return root.string(); }
};

bool MentionsFile(const ScriptCheckResult& r, const char* stem) {
    for (const auto& i : r.issues) {
        if (i.file.find(stem) != std::string::npos) return true;
    }
    return false;
}

} // namespace

ENJIN_TEST(ScriptChecker, ACleanProjectReportsNoErrors) {
    // Arrange
    TempProject p("enjin_check_clean");
    p.Script("Good.as",
             "class Good : TegeBehavior {\n"
             "    int count = 0;\n"
             "    void OnUpdate(float dt) { count += 1; }\n"
             "}\n");

    // Act
    const ScriptCheckResult r = CheckProjectScripts(p.Path());

    // Assert
    ENJIN_EXPECT_TRUE(r.fatal.empty());
    ENJIN_EXPECT_TRUE(r.modulesChecked == 1);
    ENJIN_EXPECT_TRUE(r.errorCount == 0);
    ENJIN_EXPECT_TRUE(r.Ok());
}

ENJIN_TEST(ScriptChecker, ATypeErrorIsCaught) {
    // Arrange: this is the class of mistake a stub-based linter cannot see,
    // because it needs the real compiler and the real bindings.
    TempProject p("enjin_check_type");
    p.Script("Bad.as",
             "class Bad : TegeBehavior {\n"
             "    void OnStart() { int x = \"not an int\"; }\n"
             "}\n");

    // Act
    const ScriptCheckResult r = CheckProjectScripts(p.Path());

    // Assert
    ENJIN_EXPECT_TRUE(r.fatal.empty());
    ENJIN_EXPECT_TRUE(r.errorCount > 0);
    ENJIN_EXPECT_TRUE(!r.Ok());
}

ENJIN_TEST(ScriptChecker, EveryErrorCarriesItsFileLineAndColumn) {
    // Arrange: the whole point is that a person can jump to the mistake. A
    // count with no location is barely better than "class not found".
    TempProject p("enjin_check_loc");
    p.Script("Located.as",
             "class Located : TegeBehavior {\n"
             "    void OnStart() {\n"
             "        Undefined_Function_Here(1);\n"
             "    }\n"
             "}\n");

    // Act
    const ScriptCheckResult r = CheckProjectScripts(p.Path());

    // Assert
    ENJIN_ASSERT_TRUE(r.errorCount > 0);
    ENJIN_EXPECT_TRUE(MentionsFile(r, "Located"));
    bool located = false;
    for (const auto& i : r.issues) {
        if (i.isError && i.row > 0 && !i.message.empty()) located = true;
    }
    ENJIN_EXPECT_TRUE(located);
}

ENJIN_TEST(ScriptChecker, SeveralErrorsInOneFileAreAllReported) {
    // Arrange: stopping at the first would send someone round the loop once
    // per mistake, and the loop is "launch the editor and press play".
    TempProject p("enjin_check_multi");
    p.Script("Multi.as",
             "class Multi : TegeBehavior {\n"
             "    void OnStart() {\n"
             "        Undefined_One(1);\n"
             "        Undefined_Two(2);\n"
             "    }\n"
             "}\n");

    // Act
    const ScriptCheckResult r = CheckProjectScripts(p.Path());

    // Assert
    ENJIN_EXPECT_TRUE(r.errorCount >= 2);
}

ENJIN_TEST(ScriptChecker, RealEngineBindingsAreRegistered) {
    // Arrange: a checker that does not register the bindings reports every
    // engine call as an unknown identifier, which would make it useless noise.
    TempProject p("enjin_check_bindings");
    p.Script("UsesEngine.as",
             "class UsesEngine : TegeBehavior {\n"
             "    void OnStart() { Debug_Log(\"hello\"); }\n"
             "}\n");

    // Act
    const ScriptCheckResult r = CheckProjectScripts(p.Path());

    // Assert
    ENJIN_EXPECT_TRUE(r.errorCount == 0);
}

ENJIN_TEST(ScriptChecker, EveryScriptIsCheckedNotJustTheFirst) {
    // Arrange: an error in the second file must not be missed because the
    // first one compiled.
    TempProject p("enjin_check_all");
    p.Script("First.as", "class First : TegeBehavior { void OnStart() {} }\n");
    p.Script("Second.as",
             "class Second : TegeBehavior {\n"
             "    void OnStart() { Undefined_Thing(); }\n"
             "}\n");

    // Act
    const ScriptCheckResult r = CheckProjectScripts(p.Path());

    // Assert
    ENJIN_EXPECT_TRUE(r.modulesChecked == 2);
    ENJIN_EXPECT_TRUE(r.errorCount > 0);
    ENJIN_EXPECT_TRUE(MentionsFile(r, "Second"));
}

ENJIN_TEST(ScriptChecker, AProjectWithNoScriptsIsNotAFailure) {
    // Arrange: a scriptless game is legitimate, and failing here would break
    // CI for it.
    TempProject p("enjin_check_empty");

    // Act
    const ScriptCheckResult r = CheckProjectScripts(p.Path());

    // Assert
    ENJIN_EXPECT_TRUE(r.fatal.empty());
    ENJIN_EXPECT_TRUE(r.modulesChecked == 0);
    ENJIN_EXPECT_TRUE(r.Ok());
}

ENJIN_TEST(ScriptChecker, AMissingProjectIsFatalNotClean) {
    // Arrange: a typo'd path must not report success. That would make a CI
    // job green while checking nothing at all -- the worst outcome available.
    // Act
    const ScriptCheckResult r = CheckProjectScripts("Z:/definitely/not/here_98765");

    // Assert
    ENJIN_EXPECT_TRUE(!r.fatal.empty());
    ENJIN_EXPECT_TRUE(!r.Ok());
}

ENJIN_TEST_MAIN()
