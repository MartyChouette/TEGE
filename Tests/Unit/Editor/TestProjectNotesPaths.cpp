// Every file path CLAUDE.md points at must exist.
//
// CLAUDE.md is the first thing any contributor -- and every AI session -- reads
// about this engine, and it is the one document nothing checks. On 2026-09-13 a
// sweep of its factual claims found six wrong at once: the test counts were a
// third of the truth, a shipped surfaceParam1 band was unlisted, the WGSL count
// was off by one, a worked example argued a feature had no UI when it has had a
// slider and a button for months, the particle circle-mask constants quoted one
// backend's numbers as though all four shared them, and the thread-safety
// section sent people to `docs/AUDIT_2026_04_12.md`, which does not exist in the
// repo or anywhere in its history.
//
// Five of those six need a person to notice. The sixth -- a path to nothing --
// is mechanical, so it gets checked here forever instead. That is the only kind
// of documentation rot a test can honestly own, and it is worth owning: a
// pointer to a missing file is worse than no pointer, because the reader spends
// their time looking for it.

#include "EnjinTest.h"
#include "Enjin/Platform/Types.h"

#include <cstddef>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

namespace {

// The repo root, found by walking up from wherever ctest launched us.
std::string RepoRoot() {
    const char* ups[] = { "", "../", "../../", "../../../", "../../../../" };
    for (const char* up : ups) {
        const std::string probe = std::string(up) + "CLAUDE.md";
        std::ifstream f(probe);
        if (f.good()) return std::string(up);
    }
    return std::string();
}

bool Exists(const std::string& path) {
    std::ifstream f(path);
    if (f.good()) return true;
    // A directory opens as a stream on some runtimes and not others; try it as
    // a listing target too rather than reporting a real directory as missing.
    std::ifstream d(path + "/.");
    return d.good();
}

// An include-style path is written the way a #include writes it, and resolves
// against the include roots rather than the repo root. Not a broken link.
const char* kIncludeRoots[] = {
    "Engine/include/", "Core/include/", "Engine/src/", "Core/src/",
};

bool ResolvesSomewhere(const std::string& root, const std::string& path) {
    if (Exists(root + path)) return true;
    for (const char* inc : kIncludeRoots) {
        if (Exists(root + inc + path)) return true;
    }
    return false;
}

// Things that look like paths and are not ours to resolve.
bool IsNotOurs(const std::string& p) {
    if (p.empty()) return true;
    if (p[0] == '~') return true;                       // the user's home
    if (p.find('*') != std::string::npos) return true;  // a glob being quoted
    if (p.rfind("http", 0) == 0) return true;
    if (p.rfind("build/", 0) == 0) return true;         // build output
    if (p.rfind("build-web/", 0) == 0) return true;
    if (p.rfind("_docs_internal/", 0) == 0) return true; // untracked by design
    return false;
}

bool EndsWithKnownExtension(const std::string& p) {
    static const char* exts[] = {
        ".md", ".h", ".cpp", ".py", ".json", ".mjs", ".frag", ".vert",
        ".comp", ".glsl", ".enjin", ".enjinproject", ".patch",
    };
    for (const char* e : exts) {
        const std::string ext(e);
        if (p.size() >= ext.size() &&
            p.compare(p.size() - ext.size(), ext.size(), ext) == 0) {
            return true;
        }
    }
    return false;
}

}  // namespace

ENJIN_TEST(ProjectNotes, EveryPathCLAUDEMdMentionsResolves) {
    const std::string root = RepoRoot();
    if (root.empty() && !Exists("CLAUDE.md")) {
        ENJIN_SKIP("CLAUDE.md not found from the test working directory");
        return;
    }

    std::ifstream in(root + "CLAUDE.md");
    ENJIN_ASSERT_TRUE(in.good());
    std::string text((std::istreambuf_iterator<char>(in)),
                     std::istreambuf_iterator<char>());

    // Scan LINE BY LINE, skipping fenced code blocks.
    //
    // The first version of this walked the whole file pairing backticks off
    // against each other, which goes out of phase the moment it meets a ```
    // fence: three backticks are an odd number, so from there on it read prose
    // as code spans and code spans as prose. It found 8 of the 17 paths this
    // file names -- and passed, because the "is the check doing anything" floor
    // had been set to the broken number it happened to produce. A guard
    // calibrated against its own bug is not a guard.
    std::vector<std::string> missing;
    std::vector<std::string> checked;

    std::size_t lineStart = 0;
    bool inFence = false;
    while (lineStart <= text.size()) {
        std::size_t lineEnd = text.find(char(10), lineStart);
        if (lineEnd == std::string::npos) lineEnd = text.size();
        const std::string line = text.substr(lineStart, lineEnd - lineStart);
        lineStart = lineEnd + 1;

        if (line.rfind("```", 0) == 0) { inFence = !inFence; continue; }
        if (inFence) continue;

        std::size_t i = 0;
        while (true) {
            const std::size_t open = line.find('`', i);
            if (open == std::string::npos) break;
            const std::size_t close = line.find('`', open + 1);
            if (close == std::string::npos) break;
            const std::string span = line.substr(open + 1, close - open - 1);
            i = close + 1;

            if (span.find('/') == std::string::npos) continue;
            if (span.find(' ') != std::string::npos) continue;
            if (!EndsWithKnownExtension(span)) continue;
            if (IsNotOurs(span)) continue;

            checked.push_back(span);
            if (!ResolvesSomewhere(root, span)) missing.push_back(span);
        }
        if (lineEnd == text.size()) break;
    }

    std::printf("    checked %zu paths named in CLAUDE.md\n", checked.size());
    for (const std::string& m : missing) {
        std::printf("    MISSING: %s\n", m.c_str());
    }

    // If this fires, either the file moved and the note did not, or the note
    // was never true. Both are the same fix: make the document say where the
    // thing actually is, or stop pointing at it.
    ENJIN_EXPECT_EQ(missing.size(), std::size_t(0));

    // And the check itself has to be doing something. A scanner that silently
    // stops matching leaves this green forever while checking nothing, which is
    // the exact failure mode this whole sweep exists to catch -- and is what the
    // first version of this test did. The floor is set well above what the
    // broken scanner produced, and deliberately below the current count so that
    // deleting one reference is a documentation change rather than a red build.
    ENJIN_EXPECT_TRUE(checked.size() >= 12);
}

ENJIN_TEST_MAIN()
