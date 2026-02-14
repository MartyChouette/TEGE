#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include <string>
#include <vector>

namespace Enjin {
namespace Editor {

// ============================================================================
// GIT FILE STATUS
// ============================================================================

/// Status of a single file in the git working tree
struct GitFileEntry {
    std::string path;
    enum class Status : u8 {
        Modified,
        Added,
        Deleted,
        Renamed,
        Untracked,
        Staged
    } status = Status::Untracked;
    bool staged = false;
};

// ============================================================================
// GIT LOG ENTRY
// ============================================================================

/// A single commit from git log
struct GitCommitEntry {
    std::string hash;        // Short hash
    std::string message;     // Commit message (first line)
    std::string author;      // Author name
    std::string date;        // Relative date string
};

// ============================================================================
// GIT INTEGRATION
// ============================================================================

/// Standalone Git integration class for the Enjin editor.
/// Encapsulates all git operations using CreateProcessA (Windows) or
/// posix_spawn (POSIX) — never std::system().
///
/// Usage:
///   1. Call Initialize(projectDir) to detect git and repo root.
///   2. Call Refresh() periodically (e.g., every 5 seconds).
///   3. Use StageFile/UnstageFile/Commit/Push/Pull for operations.
///   4. Call DrawGitPanel() to render the ImGui panel.
class ENJIN_API GitIntegration {
public:
    GitIntegration() = default;
    ~GitIntegration() = default;

    // --- Initialization ---

    /// Initialize git integration for the given project directory.
    /// Detects if git is available and finds the repo root.
    void Initialize(const std::string& projectDir);

    /// Is git installed and available in PATH?
    bool IsGitAvailable() const { return m_GitAvailable; }

    /// Is the project inside a git repository?
    bool IsGitRepository() const { return !m_RepoRoot.empty(); }

    /// Get the repository root path
    const std::string& GetRepoRoot() const { return m_RepoRoot; }

    // --- Refresh (call periodically, e.g., every 5 seconds) ---

    /// Poll git status, branch, log, and branch list.
    /// Safe to call frequently — internally rate-limited.
    void Refresh(f32 deltaTime);

    /// Force an immediate refresh (bypasses rate limiting)
    void ForceRefresh();

    // --- Status queries ---

    /// Current branch name
    const std::string& GetCurrentBranch() const { return m_Branch; }

    /// List of all local and remote branches
    const std::vector<std::string>& GetBranches() const { return m_Branches; }

    /// File status list (modified/added/deleted/staged/untracked)
    const std::vector<GitFileEntry>& GetFileStatus() const { return m_Files; }

    /// Recent commit log
    const std::vector<GitCommitEntry>& GetLog() const { return m_Log; }

    /// Last error message (empty if no error)
    const std::string& GetLastError() const { return m_LastError; }

    /// Get diff for a specific file
    std::string GetDiff(const std::string& filePath) const;

    // --- Operations ---

    /// Stage a file for commit (git add)
    void StageFile(const std::string& path);

    /// Unstage a file (git reset HEAD)
    void UnstageFile(const std::string& path);

    /// Stage all changes (git add -A)
    void StageAll();

    /// Unstage all changes (git reset HEAD)
    void UnstageAll();

    /// Commit staged changes with the given message
    bool Commit(const std::string& message);

    /// Push to remote
    bool Push();

    /// Pull from remote
    bool Pull();

    /// Fetch from all remotes
    bool Fetch();

    /// Switch to a branch
    bool SwitchBranch(const std::string& branchName);

    /// Create and switch to a new branch
    bool CreateBranch(const std::string& branchName);

    /// Initialize a new git repository in the project directory
    bool InitRepository(const std::string& directory);

    // --- ImGui Panel ---

    /// Draw the full Git integration panel.
    /// Includes branch display, file status, stage/unstage, commit,
    /// push/pull/fetch, diff viewer, and commit log.
    void DrawGitPanel();

    /// Clear the last error message
    void ClearError() { m_LastError.clear(); }

private:
    // --- Internal helpers ---

    /// Run a git command and capture stdout.
    /// Uses CreateProcessA on Windows, posix_spawn on POSIX.
    /// workingDir defaults to m_RepoRoot if empty.
    static std::string RunGitCommand(const std::string& args, const std::string& workingDir);

    /// Parse `git status --porcelain` output into GitFileEntry list
    void ParseStatusOutput(const std::string& output);

    /// Parse `git log` output into GitCommitEntry list
    void ParseLogOutput(const std::string& output);

    // --- State ---
    bool m_GitAvailable = false;
    std::string m_RepoRoot;
    std::string m_ProjectDir;

    // Cached data
    std::string m_Branch;
    std::vector<std::string> m_Branches;
    std::vector<GitFileEntry> m_Files;
    std::vector<GitCommitEntry> m_Log;
    std::string m_LastError;

    // Refresh rate limiting
    f32 m_RefreshTimer = 0.0f;
    static constexpr f32 REFRESH_INTERVAL = 5.0f;
    bool m_NeedsRefresh = true;

    // ImGui panel state
    char m_CommitMsgBuf[1024] = {};
    std::string m_DiffText;
    std::string m_DiffFilePath;
    bool m_ShowDiffViewer = false;
    char m_NewBranchBuf[128] = {};
    bool m_ShowNewBranchInput = false;
};

} // namespace Editor
} // namespace Enjin
