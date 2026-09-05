#pragma once
// Compile a project's scripts without launching anything.
//
// Script errors reached the console and nowhere else, so a broken script
// surfaced at PLAY time as "class not found" in whatever scene referenced it --
// usually a different file from the one with the mistake. There was no way to
// ask "do these compile?" offline, which is why one project ships its own
// Python linter that checks calls against an API stub and, by its own
// admission, cannot catch a real type error.
//
// This is the engine answering the question itself, with the compiler that
// actually runs the scripts. One flag makes pre-commit hooks and CI possible
// for every project on the engine.
#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"

#include <string>
#include <vector>

namespace Enjin {
namespace Scripting {

struct ScriptCheckIssue {
    std::string file;
    i32 row = 0;
    i32 col = 0;
    std::string message;
    bool isError = false;
};

struct ScriptCheckResult {
    u32 modulesChecked = 0;
    u32 errorCount = 0;
    u32 warningCount = 0;
    std::vector<ScriptCheckIssue> issues;
    // Set when the check could not run at all -- a missing directory, an
    // engine that would not start. Distinct from "ran and found errors", so a
    // caller does not report a broken setup as a clean project.
    std::string fatal;

    bool Ok() const { return fatal.empty() && errorCount == 0; }
};

// Compiles every .as under the project's scripts directory.
//
// `projectPath` is a .enjinproject file or the directory containing one. The
// engine's own embedded api scripts are compiled alongside, because a project
// script that includes one has to see the same thing at check time as at play
// time -- checking against a stub is how a linter misses real errors.
//
// Compiles each file as its own module, matching the runtime's module naming
// (parentDir_stem), so an error is reported against the file a person edits.
ENJIN_API ScriptCheckResult CheckProjectScripts(const std::string& projectPath);

} // namespace Scripting
} // namespace Enjin
