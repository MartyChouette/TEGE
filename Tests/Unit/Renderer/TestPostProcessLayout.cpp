// The C++ settings struct and the shader's uniform block must stay in step.
//
// PostProcessSettings is matched to postprocess.frag's `settings` block by
// POSITION -- there is no name-based binding at runtime. Adding a field to one
// side and not the other shifts every offset after it, so the shader starts
// reading neighbouring values and the symptom lands on whatever effect happens
// to sit further down the struct, not on the one that changed. That is a long
// afternoon in the renderer.
//
// CLAUDE.md documents the rule ("recompile ALL affected shaders AND regenerate
// ShaderData.h") and MaterialGPU has a static_assert enforcing its half of it.
// PostProcessSettings had nothing at all.
//
// The convention this leans on: every GPU-facing member of the C++ struct
// carries alignas, and the CPU-only ones after them (TAA, upscaler) do not. So
// the alignas members, in order, are exactly the uniform block.
#include "EnjinTest.h"
#include "Enjin/Renderer/PostProcessing.h"

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using namespace Enjin;

namespace {

// Source files, found relative to the repo. Tests run from the build tree, so
// this walks up rather than assuming a working directory.
std::string ReadRepoFile(const char* relative) {
    static const char* kPrefixes[] = { "", "../", "../../", "../../../",
                                       "../../../../", "../../../../../" };
    for (const char* p : kPrefixes) {
        std::ifstream f(std::string(p) + relative);
        if (f.is_open()) {
            std::stringstream ss;
            ss << f.rdbuf();
            return ss.str();
        }
    }
    return std::string();
}

std::string Trim(const std::string& s) {
    const usize a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return std::string();
    const usize b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

// Body of the first brace-balanced block starting at `from`.
std::string BalancedBody(const std::string& src, usize from) {
    const usize open = src.find('{', from);
    if (open == std::string::npos) return std::string();
    int depth = 0;
    for (usize k = open; k < src.size(); ++k) {
        if (src[k] == '{') ++depth;
        else if (src[k] == '}') {
            if (--depth == 0) return src.substr(open + 1, k - open - 1);
        }
    }
    return std::string();
}

bool StartsWith(const std::string& s, const char* p) {
    return s.rfind(p, 0) == 0;
}

// Member name from a GLSL declaration like "float watercolorEdge;".
std::string GlslMemberName(std::string line) {
    const usize slash = line.find("//");
    if (slash != std::string::npos) line = line.substr(0, slash);
    line = Trim(line);
    if (line.empty() || line.back() != ';') return std::string();

    static const char* kTypes[] = { "float ", "uint ", "int ", "bool ",
                                    "vec2 ", "vec3 ", "vec4 ",
                                    "ivec2 ", "ivec3 ", "ivec4 ",
                                    "uvec2 ", "uvec3 ", "uvec4 " };
    for (const char* t : kTypes) {
        if (!StartsWith(line, t)) continue;
        std::string rest = Trim(line.substr(std::string(t).size()));
        rest.pop_back();                       // the ';'
        rest = Trim(rest);
        if (rest.find_first_of(" \t*&[(") != std::string::npos) return std::string();
        return rest;
    }
    return std::string();
}

// Member name from a C++ declaration like
// "alignas(4) f32 watercolorEdge = 0.7f;".
std::string CppMemberName(std::string line) {
    const usize slash = line.find("//");
    if (slash != std::string::npos) line = line.substr(0, slash);
    line = Trim(line);
    if (!StartsWith(line, "alignas(")) return std::string();

    const usize close = line.find(')');
    if (close == std::string::npos) return std::string();
    std::string rest = Trim(line.substr(close + 1));

    // Drop the type token.
    const usize sp = rest.find(' ');
    if (sp == std::string::npos) return std::string();
    rest = Trim(rest.substr(sp));

    // Name runs to '=' or ';'.
    const usize stop = rest.find_first_of("=;");
    if (stop == std::string::npos) return std::string();
    const std::string name = Trim(rest.substr(0, stop));
    if (name.empty() || name.find_first_of(" \t*&[(") != std::string::npos) return std::string();
    return name;
}

std::vector<std::string> ShaderBlockMembers() {
    const std::string src = ReadRepoFile("Engine/shaders/postprocess.frag");
    std::vector<std::string> out;
    if (src.empty()) return out;
    const usize at = src.find("PostProcessSettings");
    if (at == std::string::npos) return out;

    std::istringstream body(BalancedBody(src, at));
    std::string line;
    while (std::getline(body, line)) {
        const std::string n = GlslMemberName(line);
        if (!n.empty()) out.push_back(n);
    }
    return out;
}

std::vector<std::string> StructGpuMembers() {
    const std::string src = ReadRepoFile("Engine/include/Enjin/Renderer/PostProcessing.h");
    std::vector<std::string> out;
    if (src.empty()) return out;
    const usize at = src.find("struct alignas(16) PostProcessSettings");
    if (at == std::string::npos) return out;

    std::istringstream body(BalancedBody(src, at));
    std::string line;
    while (std::getline(body, line)) {
        const std::string n = CppMemberName(line);
        if (!n.empty()) out.push_back(n);
    }
    return out;
}

} // namespace

ENJIN_TEST(PostProcessLayout, BothSourcesAreReachableFromTheTests) {
    // Arrange / Act / Assert: without this the comparisons below would pass
    // vacuously on two empty lists, which is worse than failing.
    ENJIN_EXPECT_TRUE(ShaderBlockMembers().size() > 50);
    ENJIN_EXPECT_TRUE(StructGpuMembers().size() > 50);
}

ENJIN_TEST(PostProcessLayout, TheUniformBlockAndTheStructDeclareTheSameMembersInTheSameOrder) {
    // Arrange: THE test. Matching is positional, so this catches an insertion
    // on either side -- the failure that otherwise shows up as some unrelated
    // effect reading a neighbouring field's value.
    const std::vector<std::string> glsl = ShaderBlockMembers();
    const std::vector<std::string> cpp = StructGpuMembers();
    ENJIN_ASSERT_TRUE(!glsl.empty() && !cpp.empty());

    // Act / Assert: count first, so a length mismatch reports as itself rather
    // than as a divergence at the end.
    ENJIN_EXPECT_TRUE(glsl.size() == cpp.size());

    const usize n = glsl.size() < cpp.size() ? glsl.size() : cpp.size();
    usize firstDivergence = n;
    for (usize i = 0; i < n; ++i) {
        if (glsl[i] != cpp[i]) { firstDivergence = i; break; }
    }
    ENJIN_EXPECT_TRUE(firstDivergence == n);
}

ENJIN_TEST(PostProcessLayout, TheCpuOnlySettingsStayAfterTheGpuBlock) {
    // Arrange: the convention the check above rests on. GPU-facing members
    // carry alignas; CPU-only ones (TAA, the upscaler) do not and must sit
    // after them, or the "alignas members are the uniform block" assumption
    // quietly stops being true.
    const std::string src = ReadRepoFile("Engine/include/Enjin/Renderer/PostProcessing.h");
    ENJIN_ASSERT_TRUE(!src.empty());
    const usize at = src.find("struct alignas(16) PostProcessSettings");
    ENJIN_ASSERT_TRUE(at != std::string::npos);

    // Act: the last alignas member must come before the first CPU-only one.
    const std::string body = BalancedBody(src, at);
    const usize lastGpu = body.rfind("alignas(");
    const usize taa = body.find("taaSharpness");

    // Assert
    ENJIN_ASSERT_TRUE(taa != std::string::npos);
    ENJIN_EXPECT_TRUE(lastGpu != std::string::npos);
    ENJIN_EXPECT_TRUE(lastGpu < taa);
}

ENJIN_TEST(PostProcessLayout, TheWatercolourControlsExistOnBothSides) {
    // Arrange: the effect was one enable flag and four hardcoded constants, so
    // it could only ever be a faint wash. These are the controls that make it
    // a look, and each has to be on both sides or the block is misaligned.
    const std::vector<std::string> glsl = ShaderBlockMembers();
    const std::vector<std::string> cpp = StructGpuMembers();

    const char* kNeeded[] = { "watercolorStrength", "watercolorEdge", "watercolorBleed",
                              "watercolorGranulation", "watercolorPaperScale",
                              "watercolorWobble", "watercolorLevels" };

    // Act / Assert
    for (const char* want : kNeeded) {
        bool inGlsl = false, inCpp = false;
        for (const auto& m : glsl) if (m == want) inGlsl = true;
        for (const auto& m : cpp) if (m == want) inCpp = true;
        ENJIN_EXPECT_TRUE(inGlsl);
        ENJIN_EXPECT_TRUE(inCpp);
    }
}

ENJIN_TEST(PostProcessLayout, PaperGrainDoesNotAnimate) {
    // Arrange: the old granulation multiplied its noise by settings.time, so
    // the paper crawled and a still scene shimmered like TV static. Paper does
    // not move, and this is easy to reintroduce by lifting a grain function
    // from a film-grain effect.
    const std::string src = ReadRepoFile("Engine/shaders/postprocess.frag");
    ENJIN_ASSERT_TRUE(!src.empty());
    const usize fn = src.find("vec3 applyWatercolor");
    ENJIN_ASSERT_TRUE(fn != std::string::npos);

    // Act: the function body, to its closing brace.
    const std::string body = BalancedBody(src, fn);
    ENJIN_ASSERT_TRUE(!body.empty());

    // Assert: the effect must not read the clock at all.
    ENJIN_EXPECT_TRUE(body.find("settings.time") == std::string::npos);
}

ENJIN_TEST_MAIN()
