#include "Enjin/Scripting/ScriptChecker.h"
#include "Enjin/Scripting/ScriptEngine.h"
#include "Enjin/Scripting/ScriptBindings.h"
#include "Enjin/Logging/Log.h"

#include <algorithm>
#include <filesystem>

namespace Enjin {
namespace Scripting {

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

    engine.BeginDiagnosticCapture();
    for (const fs::path& f : files) {
        engine.CompileScript(f.string());
        ++result.modulesChecked;
    }
    engine.EndDiagnosticCapture();

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
