#pragma once

#include "Enjin/Platform/Platform.h"
#include <string>

namespace Enjin {
namespace Editor {

// Pull the script file and line out of a script error message.
//
// A script error already knows where it happened, and it says so in one of two
// shapes:
//
//   compile:  scripts/Player.as (31, 9): Expected ';'
//   runtime:  Null pointer access in void Player::OnUpdate(float) at scripts/Player.as:31:9
//
// The console line carrying either of those also carries the C++ site of the
// log call ("ScriptSystem.cpp:93"), so this looks for a SCRIPT extension rather
// than for the first thing shaped like file:line. Anchoring on the first
// file:line would send every jump to the engine's own source, which the person
// reading the console did not write and cannot open.
//
// Purely lexical: no filesystem access. Returns false and leaves the outputs
// alone when the message names no script location, which is most messages.
ENJIN_API bool ParseScriptErrorLocation(const std::string& message,
                                        std::string& outPath, int& outLine);

} // namespace Editor
} // namespace Enjin
