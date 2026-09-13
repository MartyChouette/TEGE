// The embedded enjin_api must match the enjin_api on disk.
//
// enjin_api/*.as are compiled into the engine as string literals
// (Engine/src/Scripting/EnjinApiEmbedded.cpp) so a project with no
// scripts/enjin_api/ of its own still has TegeBehavior, Timer, Tween, VNScene
// and the rest. Keeping the two in step was a step a person had to remember:
// edit the script, then run _gen_api.py.
//
// Forgetting it does not fail, and that is the whole problem. The loose file
// changes, the embedded copy does not, and a stale embedded copy is still a
// perfectly valid script -- so a project without an on-disk override quietly
// keeps running the OLD API. No error, no warning, and the symptom is a
// behaviour change in a file the author did not touch. It happened to VNScene.as
// and was caught only by grepping the generated file by hand.
//
// The build now regenerates the file (Engine/CMakeLists.txt, EnjinApiCodegen),
// which is the real fix. This is the backstop for the cases codegen cannot
// cover: a machine without Python, a generated file edited by hand, or somebody
// removing the custom command. Two layers, because the failure is silent and a
// silent failure deserves a belt and braces.

#include "EnjinTest.h"
#include "Enjin/Scripting/ScriptEngine.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using namespace Enjin;
namespace fs = std::filesystem;

namespace Enjin { namespace Scripting {
// Defined in the generated EnjinApiEmbedded.cpp.
const char* GetEmbeddedApiSource(const char* name);
}}

namespace {

std::string ReadFile(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

// The generator writes the source with a leading and a trailing newline and
// normalises line endings to \n. Compare on that basis rather than byte for
// byte, because a checkout with CRLF in the working tree is not a drift.
std::string Normalise(const std::string& in) {
    std::string out;
    out.reserve(in.size());
    for (char c : in) {
        if (c != '\r') out.push_back(c);
    }
    while (!out.empty() && out.back() == '\n') out.pop_back();
    usize start = 0;
    while (start < out.size() && out[start] == '\n') ++start;
    return out.substr(start);
}

} // namespace

ENJIN_TEST(EmbeddedApi, EveryScriptOnDiskIsEmbeddedAndUpToDate) {
    // Arrange
    const fs::path apiDir = fs::path(ENJIN_REPO_ROOT) / "enjin_api";
    if (!fs::is_directory(apiDir)) {
        ENJIN_SKIP("enjin_api/ not found next to the repo root");
        return;
    }

    std::vector<fs::path> scripts;
    for (const auto& entry : fs::directory_iterator(apiDir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".as") {
            scripts.push_back(entry.path());
        }
    }
    std::sort(scripts.begin(), scripts.end());

    // A finder that stops finding anything is a silent pass waiting to happen.
    ENJIN_ASSERT_TRUE(scripts.size() >= 5);

    // Act / Assert
    usize matched = 0;
    for (const fs::path& p : scripts) {
        const std::string name = p.filename().string();
        const char* embedded = Scripting::GetEmbeddedApiSource(name.c_str());

        if (embedded == nullptr) {
            char buf[512];
            std::snprintf(buf, sizeof(buf),
                          "enjin_api/%s exists on disk but is not embedded at all. "
                          "Run python _gen_api.py.", name.c_str());
            EnjinTest::ReportFailureMsg(__FILE__, __LINE__, buf);
            continue;
        }

        const std::string onDisk = Normalise(ReadFile(p));
        const std::string inBinary = Normalise(embedded);

        if (onDisk == inBinary) {
            ++matched;
            continue;
        }

        // Say WHERE they diverge. "These two 30 KB strings differ" is not
        // something a person can act on.
        usize line = 1, col = 1, i = 0;
        const usize shortest = onDisk.size() < inBinary.size() ? onDisk.size() : inBinary.size();
        while (i < shortest && onDisk[i] == inBinary[i]) {
            if (onDisk[i] == '\n') { ++line; col = 1; } else { ++col; }
            ++i;
        }

        char buf[768];
        std::snprintf(buf, sizeof(buf),
                      "enjin_api/%s does not match the copy embedded in the engine: "
                      "first difference at line %zu column %zu (disk %zu bytes, "
                      "embedded %zu bytes). The engine is running the OLD script for "
                      "any project without its own scripts/enjin_api/ override. "
                      "Run python _gen_api.py.",
                      name.c_str(), line, col, onDisk.size(), inBinary.size());
        EnjinTest::ReportFailureMsg(__FILE__, __LINE__, buf);
    }

    std::printf("    %zu of %zu enjin_api scripts embedded and current\n",
                matched, scripts.size());
    ENJIN_EXPECT_EQ(matched, scripts.size());
}

ENJIN_TEST(EmbeddedApi, TheBaseClassIsAlwaysAvailable) {
    // Arrange / Act: TegeBehavior is injected into every module that does not
    // mention it, so a build where it failed to embed does not produce a subtle
    // difference, it produces a project where no script compiles at all.
    const char* base = Scripting::GetEmbeddedApiSource("TegeBehavior.as");

    // Assert
    ENJIN_ASSERT_TRUE(base != nullptr);
    const std::string text(base);
    ENJIN_EXPECT_TRUE(text.find("class TegeBehavior") != std::string::npos);
}

ENJIN_TEST(EmbeddedApi, AnUnknownNameReturnsNothingRatherThanSomethingElse) {
    // Assert: a miss has to be a miss. Returning the first script, or an empty
    // string that compiles to nothing, would turn "this include does not exist"
    // into a mystery about why a class went missing.
    ENJIN_EXPECT_TRUE(Scripting::GetEmbeddedApiSource("NotAThing.as") == nullptr);
    ENJIN_EXPECT_TRUE(Scripting::GetEmbeddedApiSource("") == nullptr);
}

ENJIN_TEST_MAIN()
