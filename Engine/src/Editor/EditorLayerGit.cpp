#include "Enjin/Editor/EditorLayer.h"
#include "Enjin/Editor/InspectorUndo.h"
#include "Enjin/Editor/ScenePicker.h"
#include "Enjin/Core/Version.h"
#include <GLFW/glfw3.h>
#include <chrono>
#include "Enjin/Logging/Log.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Mesh.h"
#include "Enjin/ECS/Components/Material.h"
#include "Enjin/ECS/Components/Light.h"
#include "Enjin/ECS/Components/Name.h"
#include "Enjin/ECS/Components/Camera.h"
#include "Enjin/ECS/Components/Notes.h"
#include "Enjin/ECS/Components/Controllers/CharacterController.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/ECS/Components/VisualScript.h"
#include "Enjin/AI/BehaviorTree.h"
#include "Enjin/Gameplay/QuestFlow.h"
#include "Enjin/Gameplay/TieredSaveSystem.h"
#include "Enjin/Editor/PlayModeDiff.h"
#include "Enjin/Physics/PhysicsBackendType.h"
#include "Enjin/Physics/PhysicsBackendFactory.h"
#include "Enjin/Physics/PhysicsTypes2D.h"
#include "Enjin/ECS/Components/WeatherZone.h"
#include "Enjin/ECS/Components/WaterVolume.h"
#include "Enjin/ECS/Components/GrassVolume.h"
#include "Enjin/ECS/Components/ShrubVolume.h"
#include "Enjin/ECS/Components/TreeVolume.h"
#include "Enjin/ECS/Components/Terrain.h"
#include "Enjin/ECS/Components/Terrain2D.h"
#include "Enjin/ECS/Components/Vegetation.h"
#include "Enjin/ECS/Components/CameraTrigger.h"
#include "Enjin/ECS/Components/TemperatureZone.h"
#include "Enjin/ECS/Components/GravityZone.h"
#include "Enjin/ECS/Components/PostProcessVolume.h"
#include "Enjin/ECS/Components/FluidVolume.h"
#include "Enjin/ECS/Components/Text.h"
#include "Enjin/ECS/Components/IKComponents.h"
#include "Enjin/ECS/Components/Flower.h"
#ifndef _WIN32
#include <unistd.h>
#endif
#include "Enjin/ECS/Components/LOD.h"
#include "Enjin/ECS/Components/Script.h"
#include "Enjin/ECS/Components/Tween.h"
#include "Enjin/ECS/Components/Hierarchy.h"
#include "Enjin/ECS/Components/Skeleton.h"
#include "Enjin/Renderer/MeshSimplifier.h"
#include "Enjin/Renderer/Skybox.h"
#include "Enjin/ECS/Systems/RenderSystem.h"
#include "Enjin/Renderer/RayTracing/RTShadows.h"
#include "Enjin/Renderer/RayTracing/RTReflections.h"
#include "Enjin/Renderer/RayTracing/RTAmbientOcclusion.h"
#include "Enjin/Renderer/RayTracing/RTGlobalIllumination.h"
#include "Enjin/Renderer/RayTracing/PathTracer.h"
#include "Enjin/Renderer/RayTracing/SVGFDenoiser.h"
#include "Enjin/Renderer/RayTracing/OIDNDenoiser.h"
#include "Enjin/Renderer/RayTracing/RTCompositor.h"
#include "Enjin/Renderer/RayTracing/AccelerationStructureManager.h"
#include "Enjin/Renderer/SHLightProbe.h"
#include "Enjin/Renderer/SDFScene.h"
#include "Enjin/Renderer/OITManager.h"
#include "Enjin/Effects/TreeRenderer.h"
#include "Enjin/Effects/Weather.h"
#include "Enjin/Assets/SceneImporter.h"
#include "Enjin/Assets/FontLibrary.h"
#include "Enjin/Assets/AssetLibrary.h"
#include "Enjin/Assets/AssetMetadata.h"
#include "Enjin/Scene/SceneSerializer.h"
#include "Enjin/Renderer/MeshFactory.h"
#include "Enjin/Renderer/PostProcessing.h"
#include "Enjin/Platform/Input.h"
#include "Enjin/Platform/FileDialog.h"
#include "Enjin/Assets/Prefab.h"
#include "Enjin/Build/BuildPipeline.h"
#include "Enjin/Assets/DataAsset.h"
#include "Enjin/Plugin/PluginRepository.h"
#include "Enjin/Audio/AudioSystem.h"
#include "Enjin/Renderer/NormalMapGenerator.h"
#include "Enjin/Editor/SpriteContourTracer.h"
#include "Enjin/GUI/UICanvas.h"
#include "Enjin/GUI/UITemplates.h"
#include "Enjin/GUI/DialogueImportExport.h"
#include "Enjin/Assets/SWFLoader.h"
#include "Enjin/Effects/CurlNoiseSystem.h"
#include "Enjin/Scripting/ScriptBindings.h"
#include "Enjin/Scene/LevelStreaming.h"
#include "Enjin/Effects/VoronoiMeshFracture.h"
#include "Enjin/Effects/InteractiveWater.h"
#include "Enjin/Math/Math.h"
#include <stb_image.h>
#include <imgui.h>
#include <ImGuizmo.h>
#include <backends/imgui_impl_vulkan.h>
#include <vulkan/vulkan.h>
#include <sstream>
#include <fstream>
#include <filesystem>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
// Undefine Windows macros that collide with engine methods
#undef LoadImage
#undef CreateWindow
#undef min
#undef max
#else
#include <spawn.h>
#include <sys/wait.h>
#endif
#include <climits>
#include <cmath>
#include <algorithm>

namespace Enjin {
namespace Editor {

// S19/S20/S23: Shell-escape a string for safe interpolation into shell commands (Unix only).
// Wraps the string in single quotes and escapes any embedded single quotes.
#ifndef _WIN32
static std::string ShellEscape(const std::string& s) {
    std::string result = "'";
    for (char c : s) {
        if (c == '\'') result += "'\\''";
        else result += c;
    }
    result += "'";
    return result;
}
#endif

// --- Git Integration ---

std::string EditorLayer::RunGitCommand(const std::string& args, const std::string& workingDir) {
    std::string result;
#ifdef _WIN32
    // Use CreateProcessA to avoid shell command injection via workingDir/args
    SECURITY_ATTRIBUTES sa = {};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    HANDLE hReadPipe = NULL, hWritePipe = NULL;
    if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0)) return "";
    SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si = {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdOutput = hWritePipe;
    si.hStdError = hWritePipe;
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi = {};

    std::string cmdLine = "git " + args;
    if (!CreateProcessA(NULL, cmdLine.data(), NULL, NULL, TRUE,
                        CREATE_NO_WINDOW, NULL, workingDir.c_str(), &si, &pi)) {
        CloseHandle(hReadPipe);
        CloseHandle(hWritePipe);
        return "";
    }
    CloseHandle(hWritePipe);

    char buffer[4096];
    DWORD bytesRead = 0;
    while (ReadFile(hReadPipe, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
        buffer[bytesRead] = '\0';
        result += buffer;
    }
    CloseHandle(hReadPipe);
    WaitForSingleObject(pi.hProcess, 5000);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
#else
    // S-H8: Use posix_spawn with pipe to capture output safely (no shell injection)
    {
        int pipefd[2];
        if (pipe(pipefd) != 0) return "";

        posix_spawn_file_actions_t actions;
        posix_spawn_file_actions_init(&actions);
        posix_spawn_file_actions_adddup2(&actions, pipefd[1], STDOUT_FILENO);
        posix_spawn_file_actions_adddup2(&actions, pipefd[1], STDERR_FILENO);
        posix_spawn_file_actions_addclose(&actions, pipefd[0]);

        // Shell-escape workingDir; args is constructed internally (not user input)
        std::string cmd = "cd " + ShellEscape(workingDir) + " && git " + args + " 2>&1";
        const char* argv[] = { "/bin/sh", "-c", cmd.c_str(), nullptr };

        pid_t pid = 0;
        extern char** environ;
        int spawnResult = posix_spawnp(&pid, "/bin/sh", &actions, nullptr,
                                       const_cast<char**>(argv), environ);
        posix_spawn_file_actions_destroy(&actions);
        close(pipefd[1]);

        if (spawnResult == 0) {
            char buffer[4096];
            ssize_t n;
            while ((n = read(pipefd[0], buffer, sizeof(buffer) - 1)) > 0) {
                buffer[n] = '\0';
                result += buffer;
            }
            int status = 0;
            waitpid(pid, &status, 0);
        }
        close(pipefd[0]);
    }
#endif
    // Trim trailing newline
    while (!result.empty() && (result.back() == '\n' || result.back() == '\r'))
        result.pop_back();
    return result;
}

void EditorLayer::DetectGitRepo() {
    // Check if git is available
    std::string version = RunGitCommand("--version", ".");
    m_GitAvailable = (version.find("git version") != std::string::npos);
    if (!m_GitAvailable) return;

    // Find project directory
    namespace fs = std::filesystem;
    std::string projPath = m_SceneManager.GetProjectPath();
    std::string projDir;
    if (!projPath.empty()) {
        projDir = fs::path(projPath).parent_path().string();
    }
    if (projDir.empty()) {
        projDir = fs::current_path().string();
    }

    // Check for git repo
    std::string repoRoot = RunGitCommand("rev-parse --show-toplevel", projDir);
    if (repoRoot.find("fatal") != std::string::npos || repoRoot.empty()) {
        m_GitRepoRoot.clear();
        return;
    }
    m_GitRepoRoot = repoRoot;
}

void EditorLayer::RefreshGitStatus() {
    if (m_GitRepoRoot.empty()) return;

    m_GitLastError.clear();

    // Current branch
    m_GitBranch = RunGitCommand("rev-parse --abbrev-ref HEAD", m_GitRepoRoot);

    // Branch list
    m_GitBranches.clear();
    std::string branchOutput = RunGitCommand("branch --format=\"%(refname:short)\"", m_GitRepoRoot);
    std::istringstream branchStream(branchOutput);
    std::string line;
    while (std::getline(branchStream, line)) {
        if (!line.empty()) m_GitBranches.push_back(line);
    }

    // File status
    m_GitFiles.clear();
    std::string statusOutput = RunGitCommand("status --porcelain", m_GitRepoRoot);
    std::istringstream statusStream(statusOutput);
    while (std::getline(statusStream, line)) {
        if (line.size() < 4) continue;
        char indexCode = line[0];
        char workCode = line[1];
        std::string path = line.substr(3);

        GitFileStatus entry;
        entry.path = path;

        // Determine status from index/working tree codes
        if (indexCode == '?' && workCode == '?') {
            entry.status = GitFileStatus::Status::Untracked;
            entry.staged = false;
        } else if (indexCode == 'A') {
            entry.status = GitFileStatus::Status::Added;
            entry.staged = true;
        } else if (indexCode == 'D' || workCode == 'D') {
            entry.status = GitFileStatus::Status::Deleted;
            entry.staged = (indexCode == 'D');
        } else if (indexCode == 'R') {
            entry.status = GitFileStatus::Status::Renamed;
            entry.staged = true;
        } else if (indexCode == 'M' || workCode == 'M') {
            entry.status = GitFileStatus::Status::Modified;
            entry.staged = (indexCode == 'M');
        } else {
            entry.status = GitFileStatus::Status::Modified;
            entry.staged = (indexCode != ' ' && indexCode != '?');
        }

        m_GitFiles.push_back(entry);
    }

    // Log (last 20 commits)
    m_GitLog.clear();
    std::string logOutput = RunGitCommand("log --oneline --format=\"%h|%s|%an|%ar\" -20", m_GitRepoRoot);
    std::istringstream logStream(logOutput);
    while (std::getline(logStream, line)) {
        if (line.empty()) continue;
        GitLogEntry entry;
        size_t p1 = line.find('|');
        size_t p2 = (p1 != std::string::npos) ? line.find('|', p1 + 1) : std::string::npos;
        size_t p3 = (p2 != std::string::npos) ? line.find('|', p2 + 1) : std::string::npos;
        if (p1 != std::string::npos && p2 != std::string::npos && p3 != std::string::npos) {
            entry.hash = line.substr(0, p1);
            entry.message = line.substr(p1 + 1, p2 - p1 - 1);
            entry.author = line.substr(p2 + 1, p3 - p2 - 1);
            entry.date = line.substr(p3 + 1);
            m_GitLog.push_back(entry);
        }
    }

    m_GitNeedsRefresh = false;
}

void EditorLayer::GitStageFile(const std::string& path) {
    RunGitCommand("add \"" + path + "\"", m_GitRepoRoot);
    m_GitNeedsRefresh = true;
}

void EditorLayer::GitUnstageFile(const std::string& path) {
    RunGitCommand("reset HEAD \"" + path + "\"", m_GitRepoRoot);
    m_GitNeedsRefresh = true;
}

void EditorLayer::GitStageAll() {
    RunGitCommand("add -A", m_GitRepoRoot);
    m_GitNeedsRefresh = true;
}

void EditorLayer::GitUnstageAll() {
    RunGitCommand("reset HEAD", m_GitRepoRoot);
    m_GitNeedsRefresh = true;
}

void EditorLayer::GitCommit() {
    if (m_GitCommitMsg[0] == '\0') {
        m_GitLastError = "Commit message cannot be empty";
        return;
    }
    // Escape quotes in message
    std::string msg(m_GitCommitMsg);
    std::string escaped;
    for (char c : msg) {
        if (c == '"') escaped += "\\\"";
        else escaped += c;
    }
    std::string output = RunGitCommand("commit -m \"" + escaped + "\"", m_GitRepoRoot);
    if (output.find("error") != std::string::npos || output.find("fatal") != std::string::npos) {
        m_GitLastError = output;
    } else {
        m_GitCommitMsg[0] = '\0';
    }
    m_GitNeedsRefresh = true;
}

void EditorLayer::GitPush() {
    std::string output = RunGitCommand("push", m_GitRepoRoot);
    if (output.find("error") != std::string::npos || output.find("fatal") != std::string::npos ||
        output.find("rejected") != std::string::npos) {
        m_GitLastError = output;
    }
    m_GitNeedsRefresh = true;
}

void EditorLayer::GitPull() {
    std::string output = RunGitCommand("pull", m_GitRepoRoot);
    if (output.find("CONFLICT") != std::string::npos || output.find("error") != std::string::npos ||
        output.find("fatal") != std::string::npos) {
        m_GitLastError = output;
    }
    m_GitNeedsRefresh = true;
}

void EditorLayer::GitFetch() {
    std::string output = RunGitCommand("fetch --all", m_GitRepoRoot);
    if (output.find("error") != std::string::npos || output.find("fatal") != std::string::npos) {
        m_GitLastError = output;
    }
    m_GitNeedsRefresh = true;
}

void EditorLayer::GitSwitchBranch(const std::string& branch) {
    std::string output = RunGitCommand("checkout \"" + branch + "\"", m_GitRepoRoot);
    if (output.find("error") != std::string::npos || output.find("fatal") != std::string::npos) {
        m_GitLastError = output;
    }
    m_GitNeedsRefresh = true;
}

void EditorLayer::DrawGitIntegrationPanel() {
    ImGui::Begin("Git Integration", nullptr, ImGuiWindowFlags_None);

    // Auto-detect repo on first open
    if (m_GitNeedsRefresh && m_GitRepoRoot.empty()) {
        DetectGitRepo();
        if (!m_GitRepoRoot.empty()) {
            RefreshGitStatus();
        }
    }

    // No git available
    if (!m_GitAvailable) {
        DrawEmptyState("[G]", "Git Not Found",
            "Git is not installed or not in PATH.\nPlease install Git to use version control.",
            nullptr, nullptr);
        ImGui::End();
        return;
    }

    // No repo detected
    if (m_GitRepoRoot.empty()) {
        DrawEmptyState("[G]", "No Git Repository",
            "This project is not inside a Git repository.",
            "Init Repository", [this]() {
                namespace fs = std::filesystem;
                std::string projPath = m_SceneManager.GetProjectPath();
                std::string projDir;
                if (!projPath.empty()) {
                    projDir = fs::path(projPath).parent_path().string();
                }
                if (projDir.empty()) projDir = fs::current_path().string();
                RunGitCommand("init", projDir);
                m_GitRepoRoot = projDir;
                m_GitNeedsRefresh = true;
            });
        ImGui::End();
        return;
    }

    // Auto-refresh every 5 seconds
    m_GitRefreshTimer += ImGui::GetIO().DeltaTime;
    if (m_GitNeedsRefresh || m_GitRefreshTimer >= 5.0f) {
        RefreshGitStatus();
        m_GitRefreshTimer = 0.0f;
    }

    // Error bar
    if (!m_GitLastError.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
        ImGui::TextWrapped("%s", m_GitLastError.c_str());
        ImGui::PopStyleColor();
        if (ImGui::SmallButton("Dismiss")) m_GitLastError.clear();
        ImGui::Separator();
    }

    // Branch bar
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.8f, 1.0f, 1.0f));
    ImGui::Text("Branch: %s", m_GitBranch.c_str());
    ImGui::PopStyleColor();
    ImGui::SameLine();

    // Branch switcher
    ImGui::SetNextItemWidth(150);
    if (ImGui::BeginCombo("##gitbranch", m_GitBranch.c_str())) {
        for (const auto& branch : m_GitBranches) {
            bool isSelected = (branch == m_GitBranch);
            if (ImGui::Selectable(branch.c_str(), isSelected)) {
                if (branch != m_GitBranch) {
                    GitSwitchBranch(branch);
                }
            }
            if (isSelected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    if (ImGui::Button("Refresh")) {
        m_GitNeedsRefresh = true;
    }

    ImGui::Separator();

    // Changes section
    u32 stagedCount = 0, unstagedCount = 0;
    for (const auto& f : m_GitFiles) {
        if (f.staged) stagedCount++;
        else unstagedCount++;
    }

    if (ImGui::CollapsingHeader(("Changes (" + std::to_string(m_GitFiles.size()) + ")###gitchanges").c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
        // Stage All / Unstage All buttons
        if (unstagedCount > 0) {
            if (ImGui::SmallButton("Stage All")) GitStageAll();
            ImGui::SameLine();
        }
        if (stagedCount > 0) {
            if (ImGui::SmallButton("Unstage All")) GitUnstageAll();
        }

        // File list table
        if (!m_GitFiles.empty() && ImGui::BeginTable("##gitfiles", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH)) {
            ImGui::TableSetupColumn("S", ImGuiTableColumnFlags_WidthFixed, 30.0f);
            ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, 80.0f);
            ImGui::TableSetupColumn("Path", ImGuiTableColumnFlags_WidthStretch);

            for (size_t i = 0; i < m_GitFiles.size(); i++) {
                auto& file = m_GitFiles[i];
                ImGui::TableNextRow();

                // Checkbox (stage/unstage)
                ImGui::TableNextColumn();
                bool staged = file.staged;
                ImGui::PushID(static_cast<int>(i));
                if (ImGui::Checkbox("##stage", &staged)) {
                    if (staged) GitStageFile(file.path);
                    else GitUnstageFile(file.path);
                }
                ImGui::PopID();

                // Status label (color-coded)
                ImGui::TableNextColumn();
                const char* statusLabel = "???";
                ImVec4 statusColor(1, 1, 1, 1);
                switch (file.status) {
                    case GitFileStatus::Status::Modified:
                        statusLabel = "Modified"; statusColor = ImVec4(1.0f, 0.7f, 0.2f, 1.0f); break;
                    case GitFileStatus::Status::Added:
                        statusLabel = "Added";    statusColor = ImVec4(0.3f, 1.0f, 0.3f, 1.0f); break;
                    case GitFileStatus::Status::Deleted:
                        statusLabel = "Deleted";  statusColor = ImVec4(1.0f, 0.3f, 0.3f, 1.0f); break;
                    case GitFileStatus::Status::Renamed:
                        statusLabel = "Renamed";  statusColor = ImVec4(0.6f, 0.6f, 1.0f, 1.0f); break;
                    case GitFileStatus::Status::Untracked:
                        statusLabel = "Untracked"; statusColor = ImVec4(0.6f, 0.6f, 0.6f, 1.0f); break;
                    case GitFileStatus::Status::Staged:
                        statusLabel = "Staged";   statusColor = ImVec4(0.3f, 1.0f, 0.3f, 1.0f); break;
                }
                ImGui::PushStyleColor(ImGuiCol_Text, statusColor);
                ImGui::TextUnformatted(statusLabel);
                ImGui::PopStyleColor();

                // File path
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(file.path.c_str());
            }
            ImGui::EndTable();
        }
    }

    ImGui::Separator();

    // Commit section
    if (ImGui::CollapsingHeader("Commit###gitcommit", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::InputTextMultiline("##commitmsg", m_GitCommitMsg, sizeof(m_GitCommitMsg),
            ImVec2(-1, ImGui::GetTextLineHeight() * 4));
        bool canCommit = (stagedCount > 0 && m_GitCommitMsg[0] != '\0');
        if (!canCommit) ImGui::BeginDisabled();
        if (ImGui::Button("Commit", ImVec2(-1, 0))) {
            GitCommit();
        }
        if (!canCommit) ImGui::EndDisabled();
    }

    ImGui::Separator();

    // Remote section
    if (ImGui::CollapsingHeader("Remote###gitremote", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::Button("Push")) GitPush();
        ImGui::SameLine();
        if (ImGui::Button("Pull")) GitPull();
        ImGui::SameLine();
        if (ImGui::Button("Fetch")) GitFetch();
    }

    ImGui::Separator();

    // Log section
    if (ImGui::CollapsingHeader("History###gitlog", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (!m_GitLog.empty() && ImGui::BeginTable("##gitlog", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_ScrollY,
                ImVec2(0, ImGui::GetTextLineHeightWithSpacing() * 10))) {
            ImGui::TableSetupColumn("Hash", ImGuiTableColumnFlags_WidthFixed, 65.0f);
            ImGui::TableSetupColumn("Message", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Author", ImGuiTableColumnFlags_WidthFixed, 100.0f);
            ImGui::TableSetupColumn("Date", ImGuiTableColumnFlags_WidthFixed, 100.0f);
            ImGui::TableHeadersRow();

            for (const auto& entry : m_GitLog) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.9f, 0.3f, 1.0f));
                ImGui::TextUnformatted(entry.hash.c_str());
                ImGui::PopStyleColor();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(entry.message.c_str());
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(entry.author.c_str());
                ImGui::TableNextColumn();
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
                ImGui::TextUnformatted(entry.date.c_str());
                ImGui::PopStyleColor();
            }
            ImGui::EndTable();
        } else if (m_GitLog.empty()) {
            ImGui::TextDisabled("No commits yet");
        }
    }

    ImGui::End();
}

} // namespace Editor
} // namespace Enjin
