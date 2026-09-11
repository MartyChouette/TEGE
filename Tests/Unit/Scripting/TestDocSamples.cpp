// Every AngelScript sample in the shipped documentation must compile.
//
// docs/TUTORIALS.md is 79 KB and is the main learning path, and essentially none
// of its script samples compiled. It taught an API dialect the engine does not
// have: `self` 55 times (no script has ever had it -- TegeBehavior exposes
// GetEntity()), `Vec3` 17 times (the type is Vector3), plus GetPosition(Entity),
// GetAxis(), Input_GetMousePosition() and fourteen named functions that exist
// nowhere. A beginner copying any of it got a compile error pointing at THEIR
// file, so the failure reads as "I did it wrong".
//
// tools/check_doc_api.py catches wrong NAMES. It cannot catch a wrong signature,
// a wrong argument count, or a type that only exists in the doc's imagination --
// it is a text scan, and every extension to it is another guess at what to look
// for. This does the only complete check there is: hand the sample to the real
// compiler, with the real bindings registered, and see what it says.
//
// A doc sample is usually a fragment, so it gets wrapped the way the engine
// wraps a user's script before deciding whether it compiles.
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

struct Sample {
    std::string doc;
    int line = 0;      // 1-based line of the opening fence, so a failure is clickable
    std::string code;
    // Declarations the fragment assumes exist, taken from a marker line directly
    // above the fence:
    //
    //     <!-- sample-context: float maxSpeed = 6.0f; -->
    //
    // A tutorial fragment that shows one technique should not have to spell out a
    // whole class to be checkable, and padding it out would hurt the tutorial.
    // But the assumption has to be WRITTEN DOWN somewhere, or "assumes context"
    // and "calls an API that does not exist" are the same failure -- which is how
    // 79 KB of samples calling imaginary functions stayed unnoticed. This puts it
    // next to the sample, in the file a doc author is already editing.
    std::string context;
};

// Pull every ```angelscript / ```as block out of a markdown file.
std::vector<Sample> ExtractSamples(const fs::path& path) {
    std::vector<Sample> out;
    std::ifstream file(path);
    if (!file.is_open()) return out;

    std::string line;
    int lineNo = 0;
    bool inBlock = false;
    Sample current;
    std::string pendingContext;
    while (std::getline(file, line)) {
        ++lineNo;
        // Trim a trailing \r so a CRLF checkout does not break the fence match.
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) line.pop_back();

        if (!inBlock) {
            const std::string kMarker = "<!-- sample-context:";
            const auto at = line.find(kMarker);
            if (at != std::string::npos) {
                const auto end = line.rfind("-->");
                if (end != std::string::npos && end > at + kMarker.size()) {
                    pendingContext = line.substr(at + kMarker.size(),
                                                 end - at - kMarker.size());
                }
                continue;
            }
            if (line == "```angelscript" || line == "```as") {
                inBlock = true;
                current = Sample{ path.filename().string(), lineNo, "", pendingContext };
                pendingContext.clear();
            } else if (!line.empty()) {
                // Any other content ends the marker's reach, so a context line
                // cannot silently apply to a block far below it.
                pendingContext.clear();
            }
            continue;
        }
        if (line == "```") {
            inBlock = false;
            if (!current.code.empty()) out.push_back(current);
            continue;
        }
        current.code += line;
        current.code += "\n";
    }
    return out;
}

bool Contains(const std::string& hay, const char* needle) {
    return hay.find(needle) != std::string::npos;
}

std::string Trim(const std::string& in) {
    const auto first = in.find_first_not_of(" \t");
    if (first == std::string::npos) return "";
    return in.substr(first, in.find_last_not_of(" \t") - first + 1);
}

// The two shapes a fragment can have, both of which the compiler can judge.
//
// A doc sample is written for a reader, not for a file: some blocks are whole
// classes, some are one or more methods, some are loose statements, and nothing
// marks which. Guessing from the text kept being wrong in a new way -- matching
// "int " put three blocks of statements at class scope and reported them as
// broken docs, and the tightened version read the "?" line of a multi-line
// ternary as a function definition. So do not guess: hand the compiler both
// placements and accept the sample if it is valid in either. That still catches
// everything the check exists for, because a wrong function name, a wrong
// argument count or an imaginary type fails in both.
struct Wrapping {
    std::string asMembers;      // methods and fields, straight into a class
    std::string asStatements;   // loose statements, inside OnStart
};

Wrapping Wrap(const std::string& rawCode, const std::string& context) {
    // Hoist #include lines to the top. A sample that opens with
    // `#include "Math.as"` is written as the top of a script FILE, and wrapping
    // it put the directive inside a method body, where it is a syntax error --
    // so the sample was reported as broken documentation for a fault here.
    std::string includes, code;
    {
        std::istringstream in(rawCode);
        std::string l;
        while (std::getline(in, l)) {
            if (Trim(l).rfind("#include", 0) == 0) {
                includes += Trim(l);
                includes += "\n";
            } else {
                code += l;
                code += "\n";
            }
        }
    }

    // A whole class compiles as written; there is no second shape to try.
    if (Contains(code, "class ")) return Wrapping{ includes + code, "" };

    const std::string ctx = context.empty() ? std::string()
                                            : ("    " + context + "\n");
    return Wrapping{
        includes + "class DocSample : TegeBehavior {\n" + ctx + code + "\n}\n",
        includes + "class DocSample : TegeBehavior {\n" + ctx +
            "    void OnStart() {\n" + code + "\n    }\n}\n"
    };
}

// Samples that are deliberately not compilable AngelScript, matched on a
// distinctive substring. Every entry needs a reason -- this list is the place a
// broken sample would hide, so it stays short and specific.
bool IsProse(const std::string& code) {
    // Declaration-only reference listings, e.g. the "Common API Functions"
    // tables that show signatures rather than calls.
    if (Contains(code, "// Signature") || Contains(code, "// Declarations")) return true;
    // Snippets that are explicitly an excerpt: "..." ALONE on a line, standing in
    // for omitted code. Matching "..." anywhere also matched "// ... spawn logic"
    // in a comment, which skipped the entire coroutine tutorial -- every wait in
    // it named a function that has never existed (WaitSeconds for YieldSeconds),
    // and the block sat green for as long as the rule was that wide.
    std::istringstream lines(code);
    std::string line;
    while (std::getline(lines, line)) {
        if (Trim(line) == "...") return true;
    }
    return false;
}

} // namespace

ENJIN_TEST(DocSamples, EveryDocumentedScriptCompiles) {
    const fs::path docsDir = fs::path(ENJIN_REPO_ROOT) / "docs";
    if (!fs::is_directory(docsDir)) {
        ENJIN_SKIP("docs/ not found next to the repo root");
    }

    std::vector<Sample> samples;
    for (const char* name : { "TUTORIALS.md", "BEGINNERS_GUIDE.md", "USER_MANUAL.md",
                              "SCRIPTING_API.md", "API_REFERENCE.md", "FAQ.md" }) {
        const fs::path p = docsDir / name;
        if (!fs::exists(p)) continue;
        auto found = ExtractSamples(p);
        samples.insert(samples.end(), found.begin(), found.end());
    }

    // If the extractor ever stops finding anything, that is a silent pass waiting
    // to happen -- the docs have well over a hundred script blocks.
    ENJIN_ASSERT_TRUE(samples.size() > 40);

    // The AngelScript message callback logs through the engine logger, so
    // without this a compile failure has no text at all.
    Logger::Get().Initialize("test_doc_samples.log");

    ScriptEngine engine;
    ENJIN_ASSERT_TRUE(engine.Initialize());
    RegisterAllBindings(engine.GetASEngine());

    int compiled = 0, skipped = 0, failed = 0;
    for (size_t i = 0; i < samples.size(); ++i) {
        const auto& s = samples[i];
        if (IsProse(s.code)) { ++skipped; continue; }

        const std::string moduleName = "doc_sample_" + std::to_string(i);
        std::printf("    [block %zu] %s:%d\n", i, s.doc.c_str(), s.line);
        std::fflush(stdout);   // the compiler logs through a different stream

        const Wrapping w = Wrap(s.code, s.context);
        if (engine.CompileScriptFromMemory(moduleName, w.asMembers)) {
            ++compiled;
            continue;
        }
        if (!w.asStatements.empty() &&
            engine.CompileScriptFromMemory(moduleName + "_stmt", w.asStatements)) {
            ++compiled;
            continue;
        }
        ++failed;
        char buf[1024];
        std::snprintf(buf, sizeof(buf),
                      "%s:%d compiles neither as class members nor as statements: %s",
                      s.doc.c_str(), s.line, engine.GetLastError().c_str());
        EnjinTest::ReportFailureMsg(__FILE__, __LINE__, buf);
    }

    std::printf("    %d compiled, %d prose/elided, %d failed, of %zu blocks\n",
                compiled, skipped, failed, samples.size());
    ENJIN_EXPECT_EQ(failed, 0);
    // And the check has to be doing real work: a wrapper that silently compiled
    // nothing would report zero failures too.
    ENJIN_EXPECT_TRUE(compiled > 30);
    engine.Shutdown();
}

ENJIN_TEST_MAIN()
