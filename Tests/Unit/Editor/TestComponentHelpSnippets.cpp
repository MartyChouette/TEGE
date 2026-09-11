// The script snippet in a component's help panel is a promise, so compile it.
//
// `ComponentHelp` is the editor's answer to "what is this component and how do I
// use it", shown inline in the Inspector. Thirteen entries carry a script snippet,
// and every single one of them called `self`:
//
//     string s = RandomBag_Draw(self);
//
// `self` is the PARAMETER NAME in the registered declaration
// ("string RandomBag_Draw(uint64 self)"), copied out of the binding table and into
// a call site where nothing declares it. No script has ever had a `self`;
// TegeBehavior exposes GetEntity(). Three of the snippets also named functions
// that do not exist at all -- Transform_SetPosition, Rigidbody_AddForce and
// Camera_SetActive -- and the last of those was describing something no binding
// could do: `CameraComponent::isActive` and `priority` decide which camera the
// scene renders from and neither was reachable from script.
//
// This is the same failure as docs/TUTORIALS.md (see TestDocSamples) and it needs
// the same defence, because the text lives in a .cpp where no doc linter looks and
// nothing compiles it. A person reading the panel, typing what it says, and
// getting an error in THEIR file learns that they did it wrong.
#include "EnjinTest.h"
#include "Enjin/Editor/ComponentHelp.h"
#include "Enjin/Scripting/ScriptEngine.h"
#include "Enjin/Scripting/ScriptBindings.h"
#include "Enjin/Logging/Log.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using namespace Enjin;
using namespace Enjin::Editor;
using namespace Enjin::Scripting;

namespace {

bool Contains(const std::string& hay, const char* needle) {
    return hay.find(needle) != std::string::npos;
}

// Snippets are fragments, wrapped the way the engine wraps a user's script. A
// snippet that defines a method goes into the class as-is; loose statements go in
// a method first. Both shapes are tried, so the wrapper never has to guess (the
// same approach TestDocSamples settled on after guessing wrong twice).
std::string AsMembers(const std::string& code) {
    return "class HelpSample : TegeBehavior {\n" + code + "\n}\n";
}

std::string AsStatements(const std::string& code) {
    return "class HelpSample : TegeBehavior {\n    void OnStart() {\n" + code + "\n    }\n}\n";
}

} // namespace

ENJIN_TEST(ComponentHelpSnippets, EverySnippetCompiles) {
    // Arrange: the real script engine with the real bindings, and every help key.
    Logger::Get().Initialize("test_component_help.log");

    ScriptEngine engine;
    ENJIN_ASSERT_TRUE(engine.Initialize());
    RegisterAllBindings(engine.GetASEngine());

    const std::vector<const char*> keys = ComponentHelpKeys();
    // The table is large and static; if enumeration ever comes back empty this
    // test would pass having checked nothing.
    ENJIN_ASSERT_TRUE(keys.size() > 50);

    // Act + assert: compile each snippet.
    int withSnippet = 0, compiled = 0, failed = 0;
    for (size_t i = 0; i < keys.size(); ++i) {
        const ComponentHelp* help = GetComponentHelp(keys[i]);
        ENJIN_ASSERT_NOT_NULL(help);
        if (!help->scriptSnippet || help->scriptSnippet[0] == '\0') continue;

        ++withSnippet;
        const std::string code = help->scriptSnippet;
        const std::string moduleName = "help_" + std::to_string(i);

        if (engine.CompileScriptFromMemory(moduleName, AsMembers(code)) ||
            engine.CompileScriptFromMemory(moduleName + "_s", AsStatements(code))) {
            ++compiled;
            continue;
        }

        ++failed;
        char buf[512];
        std::snprintf(buf, sizeof(buf),
                      "help snippet for '%s' does not compile: %s",
                      keys[i], engine.GetLastError().c_str());
        EnjinTest::ReportFailureMsg(__FILE__, __LINE__, buf);
    }

    std::printf("    %d snippets, %d compiled, %d failed (of %zu help entries)\n",
                withSnippet, compiled, failed, keys.size());
    ENJIN_EXPECT_EQ(failed, 0);
    // And the walk has to have found snippets at all.
    ENJIN_EXPECT_TRUE(withSnippet >= 10);

    engine.Shutdown();
}

ENJIN_TEST(ComponentHelpSnippets, NoSnippetNamesSelf) {
    // Stated separately from "it compiles", because the two catch different
    // mistakes. `self` inside a comment compiles fine and still teaches the wrong
    // thing to the person reading the panel, which is what the snippet is FOR.
    const std::vector<const char*> keys = ComponentHelpKeys();
    ENJIN_ASSERT_TRUE(!keys.empty());

    int checked = 0;
    for (const char* key : keys) {
        const ComponentHelp* help = GetComponentHelp(key);
        if (!help || !help->scriptSnippet) continue;
        ++checked;

        const std::string code = help->scriptSnippet;
        const bool namesSelf = Contains(code, "(self)") || Contains(code, "(self,") ||
                               Contains(code, " self)") || Contains(code, " self,");
        if (namesSelf) {
            char buf[512];
            std::snprintf(buf, sizeof(buf),
                          "help snippet for '%s' passes `self`; use GetEntity()", key);
            EnjinTest::ReportFailureMsg(__FILE__, __LINE__, buf);
        }
        ENJIN_EXPECT_FALSE(namesSelf);
    }
    ENJIN_EXPECT_TRUE(checked >= 10);
}

ENJIN_TEST(ComponentHelpSnippets, EveryEntryAnswersWhatAndHow) {
    // The panel renders three things and the first two are the whole point of the
    // feature. An entry with a null whatItDoes renders as blank space under the
    // component header, which reads as a component nobody has documented -- and is
    // indistinguishable from one that has no help registered at all.
    const std::vector<const char*> keys = ComponentHelpKeys();
    ENJIN_ASSERT_TRUE(!keys.empty());

    for (const char* key : keys) {
        const ComponentHelp* help = GetComponentHelp(key);
        ENJIN_ASSERT_NOT_NULL(help);
        const bool hasWhat = help->whatItDoes && help->whatItDoes[0] != '\0';
        const bool hasHow  = help->howToUse   && help->howToUse[0]   != '\0';
        if (!hasWhat || !hasHow) {
            char buf[512];
            std::snprintf(buf, sizeof(buf), "help for '%s' is missing %s", key,
                          !hasWhat ? "whatItDoes" : "howToUse");
            EnjinTest::ReportFailureMsg(__FILE__, __LINE__, buf);
        }
        ENJIN_EXPECT_TRUE(hasWhat && hasHow);
    }
}

ENJIN_TEST_MAIN()
