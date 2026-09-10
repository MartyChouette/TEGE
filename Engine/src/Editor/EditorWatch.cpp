#include "Enjin/Editor/EditorWatch.h"
#include "Enjin/Logging/Log.h"
#include <algorithm>
#include <filesystem>

namespace fs = std::filesystem;

namespace Enjin {
namespace Editor {

namespace {

// A directory poll costs a walk, so only one tree is walked per tick. A file poll
// is a stat, so all of those go every tick.
constexpr f32 kPollInterval = 1.0f;   // the scene watcher's proven ~1 Hz

i64 MTimeTicks(const fs::path& p) {
    std::error_code ec;
    auto t = fs::last_write_time(p, ec);
    if (ec) return 0;
    return static_cast<i64>(t.time_since_epoch().count());
}

} // namespace

u64 FoldWatchEntry(u64 running, const std::string& path, i64 mtimeTicks) {
    // FNV-1a over the path, mixed with the mtime, then XOR-folded into the total.
    // XOR because the fold has to be order-independent: two directory iterators
    // that return the same files in a different order describe the same tree.
    u64 h = 1469598103934665603ULL;
    for (unsigned char c : path) { h ^= c; h *= 1099511628211ULL; }
    h ^= static_cast<u64>(mtimeTicks);
    h *= 1099511628211ULL;
    return running ^ h;
}

EditorWatch::Handle EditorWatch::WatchFile(const std::string& path) {
    for (auto& [handle, w] : m_Watches)
        if (!w.isTree && w.path == path) return handle;

    Watch w;
    w.path = path;
    const Handle handle = m_NextHandle++;
    m_Watches.emplace(handle, std::move(w));
    Poll(m_Watches[handle], /*rebaselineOnly=*/true);
    return handle;
}

EditorWatch::Handle EditorWatch::WatchTree(const std::string& root, const std::string& extension) {
    for (auto& [handle, w] : m_Watches)
        if (w.isTree && w.path == root && w.extension == extension) return handle;

    Watch w;
    w.path = root;
    w.extension = extension;
    w.isTree = true;
    const Handle handle = m_NextHandle++;
    m_Watches.emplace(handle, std::move(w));
    Poll(m_Watches[handle], /*rebaselineOnly=*/true);
    return handle;
}

u64 EditorWatch::Version(Handle handle) const {
    auto it = m_Watches.find(handle);
    return it == m_Watches.end() ? 0 : it->second.version;
}

void EditorWatch::Rebaseline(Handle handle) {
    auto it = m_Watches.find(handle);
    if (it != m_Watches.end()) Poll(it->second, /*rebaselineOnly=*/true);
}

void EditorWatch::Unwatch(Handle handle) { m_Watches.erase(handle); }

void EditorWatch::Clear() {
    m_Watches.clear();
    m_TreeCursor = 0;
}

u64 EditorWatch::PollFile(const Watch& w) const {
    std::error_code ec;
    if (!fs::exists(w.path, ec) || ec) return 0;   // 0 = absent, a legitimate state
    return FoldWatchEntry(0, w.path, MTimeTicks(w.path));
}

u64 EditorWatch::PollTree(const Watch& w) const {
    std::error_code ec;
    if (!fs::is_directory(w.path, ec) || ec) return 0;

    u64 fingerprint = 0;
    for (fs::recursive_directory_iterator it(w.path, fs::directory_options::skip_permission_denied, ec), end;
         it != end; it.increment(ec)) {
        if (ec) break;
        if (!it->is_regular_file(ec)) continue;
        if (!w.extension.empty() && it->path().extension() != w.extension) continue;
        fingerprint = FoldWatchEntry(fingerprint, it->path().string(), MTimeTicks(it->path()));
    }
    return fingerprint;
}

void EditorWatch::Poll(Watch& w, bool rebaselineOnly) {
    const u64 next = w.isTree ? PollTree(w) : PollFile(w);
    // The first poll establishes the baseline. Without this every watch would
    // report a change on the frame it was registered, and a panel that registers
    // on first draw would rebuild itself once for nothing.
    if (!rebaselineOnly && w.sampled && next != w.fingerprint) ++w.version;
    w.fingerprint = next;
    w.sampled = true;
}

void EditorWatch::Update(f32 deltaTime) {
    if (m_Watches.empty()) return;

    m_Timer += deltaTime;
    if (m_Timer < kPollInterval) return;
    m_Timer = 0.0f;

    // Every file watch, then exactly one tree watch. Handles are monotonic, so
    // ordering by handle gives the round-robin a stable sequence even though the
    // map itself is unordered.
    std::vector<Handle> trees;
    trees.reserve(m_Watches.size());
    for (auto& [handle, w] : m_Watches) {
        if (w.isTree) trees.push_back(handle);
        else Poll(w, /*rebaselineOnly=*/false);
    }
    if (trees.empty()) return;

    std::sort(trees.begin(), trees.end());
    if (m_TreeCursor >= trees.size()) m_TreeCursor = 0;
    Poll(m_Watches[trees[m_TreeCursor]], /*rebaselineOnly=*/false);
    m_TreeCursor = (m_TreeCursor + 1) % trees.size();
}

} // namespace Editor
} // namespace Enjin
