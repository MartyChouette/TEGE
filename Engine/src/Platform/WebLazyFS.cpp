#include "Enjin/Platform/WebLazyFS.h"
#include "Enjin/Logging/Log.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

namespace {

Enjin::Platform::WebLazyFS::ReadFn s_Reader;
std::vector<std::string> s_Paths;          // index -> virtual path (JS nodes hold the index)
std::vector<Enjin::u8> s_Buffer;           // bytes of the file being materialized (transient)
Enjin::u32 s_Materialized = 0;             // currently resident
Enjin::u64 s_MaterializedBytes = 0;
Enjin::u64 s_BudgetBytes = 0;              // 0 = unlimited
Enjin::u32 s_Evictions = 0;

} // namespace

#ifdef __EMSCRIPTEN__

// C entry points the JS side calls while a stream op is materializing a node.
// Single-threaded WASM: the read is a synchronous re-entry from inside the
// caller's fread(), which is fine because the reader only touches the pak.
extern "C" {

EMSCRIPTEN_KEEPALIVE int enjin_lazyfs_materialize(int index) {
    s_Buffer.clear();
    if (!s_Reader || index < 0 || static_cast<size_t>(index) >= s_Paths.size()) return 0;
    const std::string& path = s_Paths[static_cast<size_t>(index)];
    s_Buffer = s_Reader(path);
    if (s_Buffer.empty()) {
        ENJIN_LOG_ERROR(Player, "LazyFS: reader produced no bytes for '%s'", path.c_str());
        return 0;
    }
    ++s_Materialized;
    s_MaterializedBytes += s_Buffer.size();
    ENJIN_LOG_INFO(Player, "LazyFS: materialized '%s' (%.1f KB, %u files / %.1f MB total)",
        path.c_str(), s_Buffer.size() / 1024.0, s_Materialized, s_MaterializedBytes / (1024.0 * 1024.0));
    return static_cast<int>(s_Buffer.size());
}

EMSCRIPTEN_KEEPALIVE const unsigned char* enjin_lazyfs_buffer() {
    return s_Buffer.data();
}

EMSCRIPTEN_KEEPALIVE void enjin_lazyfs_release() {
    std::vector<Enjin::u8>().swap(s_Buffer);
}

EMSCRIPTEN_KEEPALIVE double enjin_lazyfs_budget() {
    return static_cast<double>(s_BudgetBytes);
}

// JS demoted a resident node back to lazy to make room under the budget.
EMSCRIPTEN_KEEPALIVE void enjin_lazyfs_on_evict(int index, double bytes) {
    const Enjin::u64 b = static_cast<Enjin::u64>(bytes);
    s_MaterializedBytes = (s_MaterializedBytes >= b) ? s_MaterializedBytes - b : 0;
    if (s_Materialized > 0) --s_Materialized;
    ++s_Evictions;
    const char* path = (index >= 0 && static_cast<size_t>(index) < s_Paths.size()) ? s_Paths[static_cast<size_t>(index)].c_str() : "?";
    ENJIN_LOG_INFO(Player, "LazyFS: evicted '%s' (%.1f KB) for budget (%.1f / %.1f MB resident)",
        path, bytes / 1024.0, s_MaterializedBytes / (1024.0 * 1024.0), s_BudgetBytes / (1024.0 * 1024.0));
}

} // extern "C"

// Creates the lazy MEMFS node. No JS-library helpers (UTF8ToString, malloc)
// are used so nothing extra has to be exported; the path is decoded straight
// from the heap. Returns 1 when a node was created, 0 if the path already
// exists or MEMFS refused.
EM_JS(int, enjin_lazyfs_js_register, (const char* vpathPtr, int index, double size), {
    var end = vpathPtr;
    while (HEAPU8[end]) end++;
    var vpath = new TextDecoder().decode(HEAPU8.subarray(vpathPtr, end));
    var full = '/' + vpath;
    try { if (FS.analyzePath(full).exists) return 0; } catch (e) {}
    var slash = full.lastIndexOf('/');
    var dir = slash > 0 ? full.substring(0, slash) : '/';
    var name = full.substring(slash + 1);
    try { FS.mkdirTree(dir); } catch (e) {}
    var node;
    try { node = FS.createFile(dir, name, {}, true, false); } catch (e) { return 0; }

    // Shared registry: resident nodes (for LRU eviction) + a use tick.
    if (!Module.enjinLazy) Module.enjinLazy = { resident: [], residentBytes: 0, tick: 0 };
    var reg = Module.enjinLazy;

    node.contents = null;
    node.enjinPakIndex = index;      // permanent: needed again after a demotion
    node.enjinLazyIndex = index;     // undefined while resident
    node.enjinSize = size;
    node.enjinLastUse = 0;

    var demote = function(v) {
        var bytes = v.contents ? v.contents.length : 0;
        v.contents = null;
        v.enjinLazyIndex = v.enjinPakIndex;
        reg.residentBytes -= bytes;
        _enjin_lazyfs_on_evict(v.enjinPakIndex, bytes);
    };

    var ensure = function() {
        node.enjinLastUse = ++reg.tick;
        if (node.enjinLazyIndex === undefined) return;
        // Budget: demote least-recently-used resident files until this one fits.
        var budget = _enjin_lazyfs_budget();
        if (budget > 0) {
            while (reg.residentBytes + node.enjinSize > budget && reg.resident.length > 0) {
                var vi = 0;
                for (var i = 1; i < reg.resident.length; i++) {
                    if (reg.resident[i].enjinLastUse < reg.resident[vi].enjinLastUse) vi = i;
                }
                var v = reg.resident[vi];
                reg.resident.splice(vi, 1);
                demote(v);
            }
        }
        var idx = node.enjinLazyIndex;
        node.enjinLazyIndex = undefined;
        var n = _enjin_lazyfs_materialize(idx);
        var ptr = _enjin_lazyfs_buffer();
        // HEAPU8 is re-read after the call: materializing may grow the heap.
        node.contents = n > 0 ? HEAPU8.slice(ptr, ptr + n) : new Uint8Array(0);
        _enjin_lazyfs_release();
        node.enjinSize = node.contents.length;
        reg.resident.push(node);
        reg.residentBytes += node.contents.length;
    };

    // stat() answers from the registered size without loading anything.
    Object.defineProperties(node, {
        usedBytes: {
            get: function() { return this.enjinSize; },
            set: function(v) { this.enjinSize = v; }
        }
    });

    // Every op that touches bytes materializes first. open/close/llseek are
    // left alone (MEMFS llseek only consults usedBytes) so an existence probe
    // (fopen + fclose, ifstream::is_open) or a size query (fseek END + ftell)
    // stays free; only read/mmap/... pull the bytes in.
    var ops = {};
    Object.keys(node.stream_ops).forEach(function(key) {
        var fn = node.stream_ops[key];
        if (key === 'open' || key === 'close' || key === 'llseek') { ops[key] = fn; return; }
        ops[key] = function() { ensure(); return fn.apply(null, arguments); };
    });
    node.stream_ops = ops;
    return 1;
});

#endif // __EMSCRIPTEN__

namespace Enjin::Platform::WebLazyFS {

void SetReader(ReadFn reader) {
    s_Reader = std::move(reader);
}

bool RegisterLazyFile(const std::string& virtualPath, u64 size) {
#ifdef __EMSCRIPTEN__
    if (virtualPath.empty() || virtualPath[0] == '/') return false;
    const int index = static_cast<int>(s_Paths.size());
    if (!enjin_lazyfs_js_register(virtualPath.c_str(), index, static_cast<double>(size))) return false;
    s_Paths.push_back(virtualPath);
    return true;
#else
    (void)virtualPath; (void)size;
    return false;
#endif
}

void SetBudgetBytes(u64 bytes) { s_BudgetBytes = bytes; }
u64 GetBudgetBytes() { return s_BudgetBytes; }
u32 GetEvictionCount() { return s_Evictions; }
u32 GetMaterializedCount() { return s_Materialized; }
u64 GetMaterializedBytes() { return s_MaterializedBytes; }
u32 GetRegisteredCount() { return static_cast<u32>(s_Paths.size()); }

} // namespace Enjin::Platform::WebLazyFS
