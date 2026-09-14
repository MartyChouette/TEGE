#include "Enjin/Scripting/ScriptChecker.h"
#include "Enjin/Scripting/ScriptEngine.h"
#include "Enjin/Scripting/ScriptBindings.h"
#include "Enjin/Logging/Log.h"

#include <algorithm>
#include <filesystem>
#include <unordered_map>
#include <fstream>

namespace Enjin {
namespace Scripting {

// Defined in EnjinApiEmbedded.cpp. Declared here rather than pulled from a
// header because it has none -- ScriptEngine.cpp declares it locally too.
const char* GetEmbeddedApiSource(const char* name);


namespace {

namespace fs = std::filesystem;

// The project's scripts live here, the same place the runtime reads them from.
fs::path ResolveScriptDir(const std::string& projectPath, std::string& outError) {
    std::error_code ec;
    fs::path p(projectPath);

    if (fs::is_regular_file(p, ec)) p = p.parent_path();     // a .enjinproject file
    if (!fs::is_directory(p, ec)) {
        outError = "not a project path: " + projectPath;
        return {};
    }

    const fs::path scripts = p / "scripts";
    if (!fs::is_directory(scripts, ec)) {
        outError = "no scripts directory under " + p.string();
        return {};
    }
    return scripts;
}

// Every .as in the tree, sorted so the report reads the same twice running.
//
// enjin_api is compiled too rather than skipped: a project script that
// includes one has to see the real thing, and checking against a stub is
// precisely how a hand-rolled linter misses a genuine type error.
std::vector<fs::path> CollectScripts(const fs::path& root) {
    std::vector<fs::path> out;
    std::error_code ec;
    for (auto it = fs::recursive_directory_iterator(root, ec);
         it != fs::recursive_directory_iterator(); it.increment(ec)) {
        if (ec) break;
        if (!it->is_regular_file(ec)) continue;
        auto ext = it->path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(),
                       [](unsigned char c) { return static_cast<char>(::tolower(c)); });
        if (ext == ".as") out.push_back(it->path());
    }
    std::sort(out.begin(), out.end());
    return out;
}

// Script files the project's SCENES attach, and which of them are missing.
//
// This is the failure the tool was built for and did not check: a scene carries
// `"path": "scripts/Foo.as"` with a class name, and if that file is gone the
// only symptom is "class not found" at PLAY time, in whatever scene referenced
// it. Found in a real project -- a scene still attached a script that had been
// moved to a _parked_scripts folder, and nothing anywhere said so.
//
// Deliberately a text scan rather than a scene load: this runs before any
// window or renderer exists, and loading scenes would drag half the engine in
// to answer a question about file paths.
void CollectSceneScriptPaths(const fs::path& projectRoot,
                             std::vector<std::pair<std::string, std::string>>& outMissing) {
    std::error_code ec;
    const fs::path scenes = projectRoot / "scenes";
    if (!fs::is_directory(scenes, ec)) return;

    for (auto it = fs::recursive_directory_iterator(scenes, ec);
         it != fs::recursive_directory_iterator(); it.increment(ec)) {
        if (ec) break;
        if (!it->is_regular_file(ec)) continue;
        if (it->path().extension() != ".enjin") continue;

        std::ifstream in(it->path());
        if (!in.good()) continue;
        std::string text((std::istreambuf_iterator<char>(in)),
                         std::istreambuf_iterator<char>());

        const std::string key = "\"path\"";
        usize at = 0;
        while ((at = text.find(key, at)) != std::string::npos) {
            at += key.size();
            const auto colon = text.find(':', at);
            if (colon == std::string::npos) break;
            const auto open = text.find('"', colon);
            if (open == std::string::npos) break;
            const auto close = text.find('"', open + 1);
            if (close == std::string::npos) break;
            std::string rel = text.substr(open + 1, close - open - 1);
            at = close;
            if (rel.size() < 3 || rel.substr(rel.size() - 3) != ".as") continue;
            if (fs::exists(projectRoot / rel, ec)) continue;
            outMissing.emplace_back(it->path().filename().string(), rel);
        }
    }
}

// The quoted target of an `#include`, or empty for any other line.
//
// Deliberately dumb: it does not evaluate conditionals or macros, because
// neither exists here, and the engine's own include handling is the same
// textual splice. A commented-out include still counts as an include, which
// errs toward NOT compiling a file standalone -- the safe direction, since the
// cost of that is one unchecked file rather than a page of false errors.
std::string IncludeTargetOf(const std::string& line) {
    const auto hash = line.find("#include");
    if (hash == std::string::npos) return {};
    const auto open = line.find('"', hash + 8);
    if (open == std::string::npos) return {};
    const auto close = line.find('"', open + 1);
    if (close == std::string::npos) return {};
    return line.substr(open + 1, close - open - 1);
}

} // namespace

ScriptCheckResult CheckProjectScripts(const std::string& projectPath) {
    ScriptCheckResult result;

    const fs::path scriptDir = ResolveScriptDir(projectPath, result.fatal);
    if (!result.fatal.empty()) return result;

    const std::vector<fs::path> files = CollectScripts(scriptDir);
    if (files.empty()) {
        // Not an error: a project can legitimately have no scripts, and
        // failing here would break a CI job for a scriptless game.
        return result;
    }

    ScriptEngine engine;
    if (!engine.Initialize()) {
        result.fatal = "script engine failed to initialize";
        return result;
    }
    // The real bindings, not a stub. A checker that does not register them
    // reports every engine call as an unknown identifier.
    RegisterAllBindings(engine.GetASEngine());
    // Registering after Initialize marks AngelScript's return-value ABI work
    // stale, and only a new context redoes it (see ScriptEngine's note).
    engine.InvalidateContextPool();
    engine.SetScriptDirectory(scriptDir.string());

    // WHICH FILES ARE MODULES.
    //
    // `#include` is textual, so a file another file includes is part of THAT
    // file's module, not one of its own. Compiling it alone cannot work: it
    // names types its siblings declare. See the header for the measurement --
    // one project reported 138 errors and had none.
    //
    // The scan is exact rather than heuristic: read every source once, take the
    // quoted name out of each `#include`, and a file is a module iff nothing
    // named it. Includes are matched on FILENAME because that is how the
    // engine's own include resolver works.
    std::unordered_map<std::string, std::string> includedBy;   // filename -> includer
    std::vector<std::string> missingIncludes;                  // "includer -> target"
    for (const fs::path& f : files) {
        std::ifstream in(f);
        if (!in.good()) continue;
        std::string line;
        while (std::getline(in, line)) {
            const std::string name = IncludeTargetOf(line);
            if (name.empty()) continue;
            const std::string leaf = fs::path(name).filename().string();
            if (includedBy.find(leaf) == includedBy.end()) {
                includedBy[leaf] = f.filename().string();
            }
            // An include naming a file that is not there has to be an error in
            // its own right. Without this a typo'd include silently removes a
            // file from the module it was meant to join, and the file it points
            // at is simply never checked by anything.
            //
            // "Not there" has to mean not on disk AND NOT EMBEDDED. The engine
            // ships TegeBehavior.as, Math.as, StrUtil.as, StateMachine.as and
            // the rest inside itself, and a project includes them by name
            // without any file existing -- that is the documented arrangement,
            // with a project's own scripts/enjin_api/ overriding when present.
            // The first version of this check did not know that and reported
            // eleven errors across three real projects, every one of them an
            // engine api script being used exactly as intended. A checker that
            // invents errors is worse than one that misses them, because the
            // whole point of this work was that 138 false errors made a tool
            // useless.
            const bool onDisk = std::any_of(files.begin(), files.end(),
                [&leaf](const fs::path& c) { return c.filename().string() == leaf; });
            const bool embedded = GetEmbeddedApiSource(leaf.c_str()) != nullptr;
            if (!onDisk && !embedded) {
                missingIncludes.push_back(f.filename().string() + " -> " + name);
            }
        }
    }

    engine.BeginDiagnosticCapture();
    for (const fs::path& f : files) {
        const std::string leaf = f.filename().string();
        auto inc = includedBy.find(leaf);
        if (inc != includedBy.end()) {
            ScriptCheckSkipped sk;
            sk.file = f.string();
            sk.includedBy = inc->second;
            result.skipped.push_back(std::move(sk));
            continue;
        }
        engine.CompileScript(f.string());
        ++result.modulesChecked;
    }
    engine.EndDiagnosticCapture();

    // Scenes that attach a script file which is not on disk.
    {
        std::vector<std::pair<std::string, std::string>> missingSceneScripts;
        CollectSceneScriptPaths(scriptDir.parent_path(), missingSceneScripts);
        for (const auto& [scene, rel] : missingSceneScripts) {
            ScriptCheckIssue issue;
            issue.file = scene;
            issue.message = "scene attaches a script file that does not exist: " + rel;
            issue.isError = true;
            ++result.errorCount;
            result.issues.push_back(std::move(issue));
        }
    }

    for (const std::string& m : missingIncludes) {
        ScriptCheckIssue issue;
        const auto arrow = m.find(" -> ");
        issue.file = m.substr(0, arrow);
        issue.message = "#include names a file that does not exist: " + m.substr(arrow + 4);
        issue.isError = true;
        ++result.errorCount;
        result.issues.push_back(std::move(issue));
    }

    for (const auto& d : engine.GetDiagnostics()) {
        ScriptCheckIssue issue;
        issue.file = d.file;
        issue.row = d.row;
        issue.col = d.col;
        issue.message = d.message;
        issue.isError = d.isError;
        if (issue.isError) ++result.errorCount; else ++result.warningCount;
        result.issues.push_back(std::move(issue));
    }

    engine.Shutdown();
    return result;
}

} // namespace Scripting
} // namespace Enjin
