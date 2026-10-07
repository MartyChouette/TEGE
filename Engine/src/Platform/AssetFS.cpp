#include "Enjin/Platform/AssetFS.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <mutex>

namespace Enjin::Platform::AssetFS {

namespace {

struct MountState {
    bool mounted = false;
    std::string root;            // normalized, forward slashes, no trailing slash
    PackageReader reader;
    PackageContains contains;
};

// Loaders run on worker threads too (texture and mesh streaming), so the
// mount is guarded; reads copy the callables out and call them unlocked
std::mutex& Mutex() { static std::mutex m; return m; }
MountState& State() { static MountState s; return s; }

std::string Normalize(const std::string& path) {
    std::string p = std::filesystem::path(path).lexically_normal().generic_string();
    while (p.size() > 2 && p.compare(0, 2, "./") == 0) p.erase(0, 2);
    if (p == ".") p.clear();
    while (p.size() > 1 && p.back() == '/') p.pop_back();
    return p;
}

bool SameChar(char a, char b) {
#if defined(_WIN32)
    // Windows paths compare without case, and the root comes from the OS
    // while loaders build theirs from strings
    return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
#else
    return a == b;
#endif
}

bool HasPrefix(const std::string& s, const std::string& prefix) {
    if (s.size() < prefix.size()) return false;
    for (size_t i = 0; i < prefix.size(); ++i)
        if (!SameChar(s[i], prefix[i])) return false;
    return true;
}

// Paths arrive as UTF-8. std::filesystem::u8path did this conversion and is
// deprecated in C++20; a char8_t string is the replacement.
std::filesystem::path Utf8Path(const std::string& path) {
    return std::filesystem::path(std::u8string(path.begin(), path.end()));
}

bool ReadDisk(const std::string& path, std::vector<u8>& out) {
    std::ifstream in(Utf8Path(path), std::ios::binary | std::ios::ate);
    if (!in) return false;
    const std::streamoff size = in.tellg();
    if (size < 0) return false;
    out.resize(static_cast<size_t>(size));
    in.seekg(0);
    if (size > 0 && !in.read(reinterpret_cast<char*>(out.data()), size)) return false;
    return true;
}

} // namespace

void Mount(const std::string& root, PackageReader reader, PackageContains contains) {
    std::lock_guard<std::mutex> lock(Mutex());
    auto& s = State();
    s.mounted = static_cast<bool>(reader) && static_cast<bool>(contains);
    s.root = Normalize(root);
    s.reader = std::move(reader);
    s.contains = std::move(contains);
}

void Unmount() {
    std::lock_guard<std::mutex> lock(Mutex());
    State() = MountState{};
}

bool IsMounted() {
    std::lock_guard<std::mutex> lock(Mutex());
    return State().mounted;
}

std::string ToVirtualPath(const std::string& path) {
    std::string root;
    {
        std::lock_guard<std::mutex> lock(Mutex());
        if (!State().mounted) return {};
        root = State().root;
    }
    const std::string p = Normalize(path);
    if (p.empty()) return {};
    const std::filesystem::path fp(p);
    if (!fp.is_absolute() && !fp.has_root_name()) {
        // Relative paths are package paths already, unless they climb out
        if (p.compare(0, 3, "../") == 0 || p == "..") return {};
        return p;
    }
    if (root.empty() || !HasPrefix(p, root)) return {};
    if (p.size() == root.size()) return {};
    if (p[root.size()] != '/') return {};   // "/game2" is not under "/game"
    return p.substr(root.size() + 1);
}

bool ReadBytes(const std::string& path, std::vector<u8>& out) {
    const std::string vpath = ToVirtualPath(path);
    if (!vpath.empty()) {
        PackageReader reader;
        {
            std::lock_guard<std::mutex> lock(Mutex());
            reader = State().reader;
        }
        if (reader && reader(vpath, out)) return true;
    }
    return ReadDisk(path, out);
}

bool ReadText(const std::string& path, std::string& out) {
    std::vector<u8> bytes;
    if (!ReadBytes(path, bytes)) return false;
    out.assign(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    return true;
}

bool Exists(const std::string& path) {
    const std::string vpath = ToVirtualPath(path);
    if (!vpath.empty()) {
        PackageContains contains;
        {
            std::lock_guard<std::mutex> lock(Mutex());
            contains = State().contains;
        }
        if (contains && contains(vpath)) return true;
    }
    std::error_code ec;
    return std::filesystem::is_regular_file(Utf8Path(path), ec);
}

} // namespace Enjin::Platform::AssetFS
