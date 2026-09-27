// Loose files and JSON, for a game that reads data it did not pack.
//
// Everything a script could read before this came out of the pak: data assets,
// localisation, scenes. That covers what the developer ships and nothing a player
// writes. A game whose content is meant to be edited outside it (Shells' year
// files, dropped into years/ beside the exe) had no way to read one, so the
// engine's authored-day surface had nothing to feed it.
//
// Paths are relative to the game's root and cannot leave it: the project folder
// in the editor, the exe's folder in a built game. No absolute paths, no "..".
// Read only. A script that can write files anywhere near the player's disk is a
// different conversation.
//
// JSON follows the house pattern for collections (see DataAsset_ListAll): a
// parse returns a handle, and values are read by JSON pointer, with a count and
// an indexed getter for anything that holds more than one thing. No binding in
// this engine hands an array<T>@ back to script, and this does not start.
#include "Enjin/Scripting/ScriptBindings.h"
#include "Enjin/Scripting/ASCallConv.h"
#include "Enjin/Logging/Log.h"
#include "Enjin/Platform/Paths.h"
#include "Enjin/Build/AssetReader.h"
#include <nlohmann/json.hpp>
#include <angelscript.h>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>

using namespace Enjin;
using json = nlohmann::json;

extern bool ValidateScriptAssetPath(const std::string& path, const char* funcName);

#define AS_CHECK(expr) \
    do { int _r = (expr); if (_r < 0) { ENJIN_LOG_ERROR(Script, "AS registration failed (code %d) at %s:%d", _r, __FILE__, __LINE__); } } while(0)

namespace {

std::string s_FileRoot;
const Build::AssetReader* s_FileReader = nullptr;

// A year file is a few tens of kilobytes. This is a ceiling on a mistake, not a
// budget: someone pointing a script at a video should get a warning, not a hang.
constexpr std::streamoff kMaxFileBytes = 8 * 1024 * 1024;
// Documents stay alive until freed or until the scripts shut down. The cap only
// stops a script that parses in a loop from eating memory silently.
constexpr size_t kMaxLiveDocuments = 256;

// The pak copy of a path, for when no loose file answers. The same path rules
// as a loose read; a pak path is always forward-slashed and root-relative.
bool ReadFromPak(const std::string& path, const char* func, std::string* out) {
    if (!s_FileReader || !s_FileReader->IsOpen()) return false;
    if (!ValidateScriptAssetPath(path, func)) return false;
    std::string key = path;
    for (char& c : key) if (c == '\\') c = '/';
    if (!s_FileReader->HasFile(key)) return false;
    if (out) {
        const std::vector<u8> bytes = s_FileReader->ReadFile(key);
        if (static_cast<std::streamoff>(bytes.size()) > kMaxFileBytes) return false;
        out->assign(bytes.begin(), bytes.end());
    }
    return true;
}

std::unordered_map<int, json> s_Documents;
int s_NextDocument = 1;

std::string Resolve(const std::string& path, const char* func) {
    if (s_FileRoot.empty()) {
        if (!s_FileReader) {
            ENJIN_LOG_WARN(Script, "%s: no game root is set, so there is nowhere to read from", func);
        }
        return "";
    }
    if (!ValidateScriptAssetPath(path, func)) return "";
    return Platform::ResolveWithinRoot(s_FileRoot, path);
}

// A value by JSON pointer ("/days/5/weather"), or null when the document or the
// pointer does not exist. Never throws: a player's hand-edited file with a
// missing key is the normal case here, not an error.
const json* Find(int doc, const std::string& pointer) {
    const auto it = s_Documents.find(doc);
    if (it == s_Documents.end()) return nullptr;
    if (pointer.empty()) return &it->second;
    try {
        const json::json_pointer p(pointer);
        if (!it->second.contains(p)) return nullptr;
        return &it->second.at(p);
    } catch (const json::exception&) {
        return nullptr;
    }
}

} // namespace

// --- files -------------------------------------------------------------------

static bool File_Exists(const std::string& path) {
    const std::string full = Resolve(path, "File_Exists");
    if (!full.empty()) {
        std::ifstream f(full, std::ios::binary);
        if (f.good()) return true;
    }
    return ReadFromPak(path, "File_Exists", nullptr);
}

// The whole file as a string, or "" when it is missing, unreadable, too big or
// outside the root. File_Exists tells a missing file from an empty one.
static std::string File_ReadText(const std::string& path) {
    const std::string full = Resolve(path, "File_ReadText");
    std::ifstream f;
    if (!full.empty()) f.open(full, std::ios::binary | std::ios::ate);
    if (!f) {
        std::string text;
        return ReadFromPak(path, "File_ReadText", &text) ? text : std::string();
    }
    const std::streamoff size = f.tellg();
    if (size > kMaxFileBytes) {
        ENJIN_LOG_WARN(Script, "File_ReadText: %s is %lld bytes, over the %lld byte limit",
                       path.c_str(), static_cast<long long>(size), static_cast<long long>(kMaxFileBytes));
        return "";
    }
    f.seekg(0);
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

// --- JSON --------------------------------------------------------------------

// A handle above zero, or 0 when the text is not JSON. The parse error goes to
// the log with its byte position, because "it did not load" is useless to
// someone who hand-edited a file.
static int Json_Parse(const std::string& text) {
    if (s_Documents.size() >= kMaxLiveDocuments) {
        ENJIN_LOG_WARN(Script, "Json_Parse: %zu documents are open and none were freed; "
                               "call Json_Free when done with one", s_Documents.size());
        return 0;
    }
    json doc = json::parse(text, nullptr, false);
    if (doc.is_discarded()) {
        // Parse again with exceptions for the message; the first pass kept the
        // common failure off the exception path.
        try { (void)json::parse(text); }
        catch (const json::parse_error& e) {
            ENJIN_LOG_WARN(Script, "Json_Parse: %s", e.what());
        }
        return 0;
    }
    const int handle = s_NextDocument++;
    s_Documents.emplace(handle, std::move(doc));
    return handle;
}

static void Json_Free(int doc) {
    s_Documents.erase(doc);
}

static bool Json_Has(int doc, const std::string& pointer) {
    return Find(doc, pointer) != nullptr;
}

// Each getter answers the fallback when the value is missing OR is the wrong
// type. A string where a number belongs is a player's typo, and the game should
// carry on with its default rather than read garbage.
static std::string Json_GetString(int doc, const std::string& pointer, const std::string& fallback) {
    const json* v = Find(doc, pointer);
    return (v && v->is_string()) ? v->get<std::string>() : fallback;
}
static int Json_GetInt(int doc, const std::string& pointer, int fallback) {
    const json* v = Find(doc, pointer);
    if (!v || !v->is_number()) return fallback;
    // Through double and clamped: a 1e20 in a hand-edited file must not become
    // whatever an out-of-range float-to-int conversion happens to produce.
    const double d = v->get<double>();
    if (d >= 2147483647.0) return 2147483647;
    if (d <= -2147483648.0) return -2147483647 - 1;
    return static_cast<int>(d);
}
static float Json_GetFloat(int doc, const std::string& pointer, float fallback) {
    const json* v = Find(doc, pointer);
    return (v && v->is_number()) ? v->get<float>() : fallback;
}
static bool Json_GetBool(int doc, const std::string& pointer, bool fallback) {
    const json* v = Find(doc, pointer);
    return (v && v->is_boolean()) ? v->get<bool>() : fallback;
}

// How many elements an array has or how many keys an object has; 0 for
// anything else, including a pointer that is not there.
static int Json_GetCount(int doc, const std::string& pointer) {
    const json* v = Find(doc, pointer);
    return (v && (v->is_array() || v->is_object())) ? static_cast<int>(v->size()) : 0;
}

// The index-th key of an object, so a script can walk one whose keys it does not
// know in advance (a year's days are keyed by day number and most are missing).
// Keys come back in sorted order, not file order.
static std::string Json_GetKeyAt(int doc, const std::string& pointer, int index) {
    const json* v = Find(doc, pointer);
    if (!v || !v->is_object() || index < 0 || index >= static_cast<int>(v->size())) return "";
    auto it = v->begin();
    std::advance(it, index);
    return it.key();
}

namespace Enjin {
namespace Scripting {

void SetBindingsFileRoot(const std::string& absoluteRoot) {
    s_FileRoot = absoluteRoot;
}

void SetBindingsFileAssetReader(const Build::AssetReader* reader) {
    s_FileReader = reader;
}

void ClearBindingsJsonDocuments() {
    s_Documents.clear();
}

void RegisterFileBindings(asIScriptEngine* engine) {
    AS_CHECK(engine->RegisterGlobalFunction("bool File_Exists(const string &in)",
        ENJIN_AS_FN(File_Exists), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("string File_ReadText(const string &in)",
        ENJIN_AS_FN(File_ReadText), ENJIN_AS_CALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction("int Json_Parse(const string &in)",
        ENJIN_AS_FN(Json_Parse), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Json_Free(int)",
        ENJIN_AS_FN(Json_Free), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool Json_Has(int, const string &in)",
        ENJIN_AS_FN(Json_Has), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("string Json_GetString(int, const string &in, const string &in)",
        ENJIN_AS_FN(Json_GetString), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("int Json_GetInt(int, const string &in, int)",
        ENJIN_AS_FN(Json_GetInt), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float Json_GetFloat(int, const string &in, float)",
        ENJIN_AS_FN(Json_GetFloat), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool Json_GetBool(int, const string &in, bool)",
        ENJIN_AS_FN(Json_GetBool), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("int Json_GetCount(int, const string &in)",
        ENJIN_AS_FN(Json_GetCount), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("string Json_GetKeyAt(int, const string &in, int)",
        ENJIN_AS_FN(Json_GetKeyAt), ENJIN_AS_CALL_CDECL));
}

} // namespace Scripting
} // namespace Enjin
