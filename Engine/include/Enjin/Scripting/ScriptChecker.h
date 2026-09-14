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

// A file this check did NOT compile on its own, and who includes it.
//
// Reported rather than passed over in silence: an include-only file that no
// module includes is dead script, and an `#include` naming a file that does not
// exist has to be an error in its own right, or a typo'd include just makes a
// file quietly stop being checked.
struct ScriptCheckSkipped {
    std::string file;        // the include-only file
    std::string includedBy;  // the first module that includes it
};

struct ScriptCheckResult {
    u32 modulesChecked = 0;
    u32 errorCount = 0;
    u32 warningCount = 0;
    std::vector<ScriptCheckIssue> issues;
    std::vector<ScriptCheckSkipped> skipped;
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
// A FILE THAT ANOTHER FILE INCLUDES IS NOT A MODULE.
//
// `#include` in this engine is TEXTUAL, and the documented pattern for anything
// bigger than one file is a single module assembled from several, with one
// entry point. Every non-entry file in such a module names types its siblings
// declare, so compiling it alone cannot succeed -- and this used to compile
// every .as as its own module. Measured across five projects: Deep reported 138
// errors and has none, every one of them `Identifier 'X' is not a data type`
// naming a type a sibling file declares. Its entry point compiles clean. The
// tool called the soundest multi-file project in the corpus the most broken one,
// and a real error would have arrived as number 139.
//
// So the set of modules is "every .as that nothing else includes". One pass over
// the sources, exact rather than heuristic. Everything else is reported as
// skipped-because-included, with the includer named.
//
// Remaining modules compile as their own module, matching the runtime's naming
// (parentDir_stem), so an error is reported against the file a person edits.
ENJIN_API ScriptCheckResult CheckProjectScripts(const std::string& projectPath);

} // namespace Scripting
} // namespace Enjin
