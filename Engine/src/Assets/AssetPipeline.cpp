#include "Enjin/Assets/AssetPipeline.h"
#include "Enjin/Logging/Log.h"
#include "Enjin/Platform/Platform.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <system_error>

namespace Enjin {
namespace Assets {

namespace fs = std::filesystem;

namespace {

// Project-relative, forward slashes. Empty when p is not under root.
std::string RelativeToRoot(const fs::path& p, const fs::path& root) {
    std::error_code ec;
    fs::path rel = fs::relative(p.lexically_normal(), root.lexically_normal(), ec);
    if (ec || rel.empty()) return {};
    for (const auto& part : rel) {
        if (part == "..") return {};
    }
    return rel.generic_string();
}

std::string LowerExt(const fs::path& p) {
    std::string e = p.extension().string();
    std::transform(e.begin(), e.end(), e.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return e;
}

bool CopyOne(const fs::path& src, const fs::path& dest, std::vector<std::string>* copied) {
    std::error_code ec;
    fs::create_directories(dest.parent_path(), ec);
    if (!fs::exists(dest, ec)) {
        fs::copy_file(src, dest, fs::copy_options::overwrite_existing, ec);
        if (ec) return false;
    }
    if (copied) copied->push_back(dest.generic_string());
    return true;
}

std::string ReadTextFile(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    if (!in) return {};
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

// One reference found inside a model or material file.
struct Ref {
    std::string raw;    // exactly as written in the file, so a rewrite can find it
    fs::path resolved;  // the file on disk, or empty when it could not be found
};

// Resolve a reference written inside `owner` (relative to the owner's folder,
// or absolute).
fs::path ResolveRef(const std::string& raw, const fs::path& ownerDir) {
    if (raw.empty()) return {};
    fs::path r(raw);
    std::error_code ec;
    if (r.is_absolute()) return fs::exists(r, ec) ? r : fs::path{};
    fs::path joined = ownerDir / r;
    return fs::exists(joined, ec) ? joined : fs::path{};
}

// `mtllib a.mtl b.mtl` — one line can name several.
std::vector<std::string> ParseObjMtllibs(const std::string& text) {
    std::vector<std::string> out;
    std::istringstream lines(text);
    std::string line;
    while (std::getline(lines, line)) {
        std::istringstream ls(line);
        std::string tok;
        if (!(ls >> tok) || tok != "mtllib") continue;
        std::string name;
        while (ls >> name) out.push_back(name);
    }
    return out;
}

// `map_Kd -bm 1.0 textures/wall.png` — options come first, the path is the
// remainder, and it may contain spaces.
std::vector<std::string> ParseMtlMaps(const std::string& text) {
    static const char* kMapKeys[] = {
        "map_Kd", "map_Ka", "map_Ks", "map_Ke", "map_Ns", "map_d",
        "map_bump", "bump", "disp", "decal", "refl", "norm"
    };
    std::vector<std::string> out;
    std::istringstream lines(text);
    std::string line;
    while (std::getline(lines, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        std::istringstream ls(line);
        std::string key;
        if (!(ls >> key)) continue;
        bool isMap = false;
        for (const char* k : kMapKeys) {
            if (key == k) { isMap = true; break; }
        }
        if (!isMap) continue;

        // Skip "-opt value" pairs; the first token that is not an option, and
        // is not the value of one, starts the path.
        std::vector<std::string> toks;
        std::string t;
        while (ls >> t) toks.push_back(t);
        usize i = 0;
        while (i < toks.size() && !toks[i].empty() && toks[i][0] == '-') {
            i += 2;   // option plus its value; multi-value options are rare
        }
        if (i >= toks.size()) continue;
        std::string path = toks[i];
        for (usize j = i + 1; j < toks.size(); ++j) path += " " + toks[j];
        if (!path.empty()) out.push_back(path);
    }
    return out;
}

// glTF buffers[].uri and images[].uri, skipping embedded data: URIs. Parsed
// textually so this file needs no JSON dependency.
std::vector<std::string> ParseGltfUris(const std::string& text) {
    std::vector<std::string> out;
    const std::string key = "\"uri\"";
    usize pos = 0;
    while ((pos = text.find(key, pos)) != std::string::npos) {
        pos += key.size();
        usize colon = text.find(':', pos);
        if (colon == std::string::npos) break;
        usize q1 = text.find('"', colon);
        if (q1 == std::string::npos) break;
        usize q2 = q1 + 1;
        std::string value;
        while (q2 < text.size() && text[q2] != '"') {
            if (text[q2] == '\\' && q2 + 1 < text.size()) {   // \/ and friends
                ++q2;
                if (text[q2] != '/') value += '\\';
            }
            value += text[q2];
            ++q2;
        }
        pos = q2;
        if (value.rfind("data:", 0) == 0) continue;   // embedded, nothing to copy
        if (!value.empty()) out.push_back(value);
    }
    return out;
}

} // namespace

std::string CopyToProjectAssets(const std::string& srcPath,
                                const std::string& projectRoot,
                                const std::string& subdir)
{
    if (srcPath.empty() || projectRoot.empty()) return srcPath;

    std::error_code ec;
    fs::path src(srcPath);
    if (!fs::exists(src, ec)) return srcPath;

    fs::path root(projectRoot);

    // Already under the project root — no copy, just report it portably.
    std::string inside = RelativeToRoot(src, root);
    if (!inside.empty()) return inside;

    fs::path dest = root / subdir / src.filename();
    if (!CopyOne(src, dest, nullptr)) return srcPath;

    std::string rel = RelativeToRoot(dest, root);
    return rel.empty() ? srcPath : rel;
}

std::string CopyModelToProjectAssets(const std::string& srcPath,
                                     const std::string& projectRoot,
                                     const std::string& subdir,
                                     std::vector<std::string>* copiedFiles)
{
    if (srcPath.empty() || projectRoot.empty()) return srcPath;

    std::error_code ec;
    fs::path src = fs::path(srcPath);
    if (!fs::exists(src, ec)) return srcPath;

    fs::path root(projectRoot);

    // Already in the project: leave the files alone, just report portably.
    std::string inside = RelativeToRoot(src, root);
    if (!inside.empty()) return inside;

    const fs::path srcDir = src.parent_path();
    const fs::path destDir = root / subdir / src.stem();

    if (!CopyOne(src, destDir / src.filename(), copiedFiles)) return srcPath;

    // The engine's own import-settings sidecar, when the model has one.
    fs::path sidecar = src;
    sidecar += ".enjinasset";
    if (fs::exists(sidecar, ec)) CopyOne(sidecar, destDir / sidecar.filename(), copiedFiles);

    // Copy one reference and return what the referring file should now say.
    // Inside the model's folder: keep the layout. Outside or absolute: flatten
    // beside the model, because the original location will not exist for
    // anyone else.
    auto takeRef = [&](const std::string& raw, const fs::path& ownerDir) -> std::string {
        fs::path found = ResolveRef(raw, ownerDir);
        if (found.empty()) return {};   // dangling in the source; nothing to copy
        std::string within = RelativeToRoot(found, srcDir);
        fs::path dest = within.empty() ? (destDir / found.filename()) : (destDir / within);
        if (!CopyOne(found, dest, copiedFiles)) return {};
        return within.empty() ? found.filename().generic_string() : within;
    };

    // Rewrite a copied text file when a reference had to move. Rewriting the
    // COPY only; the source the user picked is never modified.
    auto rewrite = [](const fs::path& file, const std::vector<std::pair<std::string, std::string>>& subs) {
        if (subs.empty()) return;
        std::string text = ReadTextFile(file);
        if (text.empty()) return;
        bool changed = false;
        for (const auto& [from, to] : subs) {
            if (from == to) continue;
            usize at = 0;
            while ((at = text.find(from, at)) != std::string::npos) {
                text.replace(at, from.size(), to);
                at += to.size();
                changed = true;
            }
        }
        if (!changed) return;
        std::ofstream out(file, std::ios::binary | std::ios::trunc);
        if (out) out << text;
    };

    const std::string ext = LowerExt(src);

    if (ext == ".obj") {
        const std::string objText = ReadTextFile(src);
        std::vector<std::pair<std::string, std::string>> objSubs;

        for (const std::string& mtlRef : ParseObjMtllibs(objText)) {
            const std::string newRef = takeRef(mtlRef, srcDir);
            if (newRef.empty()) {
                ENJIN_LOG_WARN(Assets, "Model copy: '%s' names a material library that is missing: %s",
                               src.filename().string().c_str(), mtlRef.c_str());
                continue;
            }
            if (newRef != mtlRef) objSubs.push_back({mtlRef, newRef});

            // Materials name textures of their own, relative to the .mtl.
            fs::path mtlSrc = ResolveRef(mtlRef, srcDir);
            if (mtlSrc.empty()) continue;
            const std::string mtlText = ReadTextFile(mtlSrc);
            std::vector<std::pair<std::string, std::string>> mtlSubs;
            for (const std::string& tex : ParseMtlMaps(mtlText)) {
                const std::string newTex = takeRef(tex, mtlSrc.parent_path());
                if (newTex.empty()) {
                    ENJIN_LOG_WARN(Assets, "Model copy: material '%s' names a texture that is missing: %s",
                                   mtlSrc.filename().string().c_str(), tex.c_str());
                    continue;
                }
                if (newTex != tex) mtlSubs.push_back({tex, newTex});
            }
            rewrite(destDir / newRef, mtlSubs);
        }
        rewrite(destDir / src.filename(), objSubs);

    } else if (ext == ".gltf") {
        const std::string gltfText = ReadTextFile(src);
        std::vector<std::pair<std::string, std::string>> subs;
        for (const std::string& uri : ParseGltfUris(gltfText)) {
            const std::string newUri = takeRef(uri, srcDir);
            if (newUri.empty()) {
                ENJIN_LOG_WARN(Assets, "Model copy: '%s' names a resource that is missing: %s",
                               src.filename().string().c_str(), uri.c_str());
                continue;
            }
            if (newUri != uri) subs.push_back({uri, newUri});
        }
        rewrite(destDir / src.filename(), subs);
    }
    // .glb / .fbx carry their media inside the file. An .fbx with external
    // textures still works: the importer routes those through the material
    // texture paths, which CopyToProjectAssets already brings in.

    std::string rel = RelativeToRoot(destDir / src.filename(), root);
    return rel.empty() ? srcPath : rel;
}

} // namespace Assets
} // namespace Enjin
