#include "Enjin/GUI/FontScripts.h"

#include <cctype>

namespace Enjin {
namespace GUI {

AtlasScript ScriptForLocale(const std::string& localeCode) {
    // The language subtag is everything before the first separator. BCP 47
    // writes "ja-JP"; POSIX and some project files write "ja_JP"; either has
    // to reach the same atlas.
    std::string lang;
    for (char c : localeCode) {
        if (c == '-' || c == '_' || c == '.') break;
        lang.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }

    if (lang == "ja") return AtlasScript::Japanese;
    if (lang == "zh") return AtlasScript::ChineseFull;
    if (lang == "ko") return AtlasScript::Korean;
    if (lang == "th") return AtlasScript::Thai;
    if (lang == "vi") return AtlasScript::Vietnamese;

    // Everything else, including an empty or unrecognised code. European
    // coverage is baked unconditionally, so this is never "no glyphs" -- it is
    // exactly what every project gets today.
    return AtlasScript::EuropeanOnly;
}

} // namespace GUI
} // namespace Enjin
