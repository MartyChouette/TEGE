#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include <string>
#include <unordered_map>
#include <vector>

namespace Enjin {
namespace Editor {

// One throttled disk watch for the whole editor.
//
// A panel registers a path and reads back a version that bumps when the thing on
// disk moves. It never gets a callback and it never owns a timer, because the
// alternative -- every panel growing its own stat loop, or shipping a Refresh
// button and asking the user to guess when to press it -- is what this replaces.
//
// The logic is `CheckExternalSceneChange`'s, which was correct and hard-wired to
// one file: stat at roughly 1 Hz, compare against a recorded baseline, report a
// change. What is new is that anything can register, and that a whole subtree can
// be watched, which is what a panel backed by a directory of files actually needs.
//
// Cost control: at most ONE tree is walked per tick, round-robin, so N watched
// trees cost one walk per tick rather than N. Single files are stat'd every tick;
// a stat is cheap and there are only ever a handful.
class ENJIN_API EditorWatch {
public:
    // A handle for reading the version back. Zero is never a valid watch.
    using Handle = u32;

    // Watch one file. Re-registering the same path returns the same handle, so a
    // panel can call this every frame without accumulating watches.
    Handle WatchFile(const std::string& path);

    // Watch every file under `root` whose extension matches `extension`
    // (".enjdata", with the dot). The version bumps when a matching file is
    // added, removed, or written. `extension` empty watches every file.
    Handle WatchTree(const std::string& root, const std::string& extension);

    // Bumps when the watched thing changes. A caller compares it against what it
    // last saw; it is deliberately not a boolean, so any number of readers can
    // each notice the same change independently.
    u64 Version(Handle handle) const;

    // Accept the current state as the baseline without bumping the version. For a
    // reader that just wrote the file itself and does not want to see its own edit
    // come back as an external change.
    void Rebaseline(Handle handle);

    void Unwatch(Handle handle);
    void Clear();

    // Called once per editor frame. Does the throttling itself.
    void Update(f32 deltaTime);

    usize WatchCount() const { return m_Watches.size(); }

private:
    struct Watch {
        std::string path;        // file path, or tree root
        std::string extension;   // empty for a file watch
        bool isTree = false;
        u64 fingerprint = 0;     // what the last poll saw
        u64 version = 0;
        bool sampled = false;    // false until the first poll, so the first
                                 // fingerprint is a baseline and not a "change"
    };

    u64 PollFile(const Watch& w) const;
    u64 PollTree(const Watch& w) const;
    void Poll(Watch& w, bool rebaselineOnly);

    std::unordered_map<Handle, Watch> m_Watches;
    Handle m_NextHandle = 1;
    f32 m_Timer = 0.0f;
    usize m_TreeCursor = 0;      // round-robin over the tree watches
};

// Fold one (path, mtime) pair into a running fingerprint. Order-independent, so a
// directory iterator that returns entries in a different order on a different
// filesystem does not read as a change. Free and pure so it can be tested without
// touching a disk.
ENJIN_API u64 FoldWatchEntry(u64 running, const std::string& path, i64 mtimeTicks);

} // namespace Editor
} // namespace Enjin
