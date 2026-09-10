#include "Enjin/Editor/ScriptErrorLocation.h"

#include <cctype>
#include <cstdlib>
#include <cstring>

namespace Enjin {
namespace Editor {

bool ParseScriptErrorLocation(const std::string& message, std::string& outPath, int& outLine) {
    static const char* kExts[] = { ".angelscript", ".as" };

    auto isPathChar = [](char c) {
        // A Windows path keeps its drive colon, so ':' stays in. What ends a
        // path is the punctuation a message wraps it in.
        return !(c == ' ' || c == '\t' || c == '"' || c == '\'' ||
                 c == '(' || c == ')' || c == '[' || c == ']' ||
                 c == '<' || c == '>' || c == ',');
    };

    bool found = false;
    for (const char* ext : kExts) {
        const std::string::size_type extLen = std::strlen(ext);
        std::string::size_type from = 0;
        while (true) {
            std::string::size_type at = message.find(ext, from);
            if (at == std::string::npos) break;
            from = at + 1;

            // The extension has to END the name, or "sprites.assets" matches ".as".
            std::string::size_type after = at + extLen;
            if (after < message.size() &&
                (std::isalnum(static_cast<unsigned char>(message[after])) || message[after] == '_')) {
                continue;
            }

            // Walk back to the start of the path token.
            std::string::size_type start = at;
            while (start > 0 && isPathChar(message[start - 1])) --start;
            if (start == at) continue;   // ".as" with no name in front of it

            // Then the line, in either spelling: ":31" or " (31, 9)".
            std::string::size_type p = after;
            while (p < message.size() && message[p] == ' ') ++p;
            if (p >= message.size() || (message[p] != ':' && message[p] != '(')) continue;
            ++p;
            while (p < message.size() && message[p] == ' ') ++p;
            std::string::size_type digits = p;
            while (p < message.size() && std::isdigit(static_cast<unsigned char>(message[p]))) ++p;
            if (p == digits) continue;

            // The LAST location in the line wins: a message that quotes a path
            // and then reports one ends with the one it is reporting.
            outPath = message.substr(start, after - start);
            outLine = std::atoi(message.c_str() + digits);
            found = true;
        }
    }
    return found;
}

} // namespace Editor
} // namespace Enjin
