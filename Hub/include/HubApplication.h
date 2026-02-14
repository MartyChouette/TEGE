#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <functional>

/**
 * @file HubApplication.h
 * @brief Enjin Hub - Project launcher and engine version manager
 *
 * A standalone application that manages Enjin projects and engine versions.
 * Uses GLFW + ImGui (OpenGL backend) for its UI -- no Vulkan dependency.
 */

// Use standard types directly (Hub is a standalone app, not linked to Enjin Core)
using u8  = std::uint8_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;
using i32 = std::int32_t;
using f32 = float;

struct GLFWwindow;

namespace Enjin {
namespace Hub {

// --- Data Structures ---

// A project entry in the Hub's project list
struct ProjectEntry {
    std::string name;
    std::string path;               // Absolute path to .enjinproject
    std::string lastOpened;          // ISO 8601 timestamp
    std::string engineVersion;       // Engine version this project was created with
    std::string thumbnailPath;       // Optional project thumbnail
    u64 sizeBytes = 0;              // Approximate project size on disk

    // Sorting helpers
    bool operator<(const ProjectEntry& other) const {
        return lastOpened > other.lastOpened;  // Most recent first
    }
};

// An installed engine version
struct EngineVersion {
    std::string version;             // Semantic version string (e.g., "0.7.0")
    std::string path;                // Absolute path to the engine installation
    bool isDefault = false;          // Whether this is the default version for new projects
    u64 sizeBytes = 0;              // Installation size on disk
    std::string installDate;         // ISO 8601 timestamp
};

// A project template
struct TemplateEntry {
    std::string name;
    std::string category;            // "Foundations", "Genre Showcases", etc.
    std::string description;
    std::string thumbnailPath;
    std::string engineVersion;       // Minimum engine version required
};

// Hub settings
struct HubSettings {
    std::string defaultProjectDirectory;   // Default path for new projects
    std::string defaultEngineVersion;      // Default engine version for new projects
    bool autoCheckUpdates = false;         // Stub: auto-update check
    i32 themeIndex = 0;                    // 0 = Dark, 1 = Light, 2 = Classic
    i32 windowWidth = 1100;
    i32 windowHeight = 700;

    // Recently used search directories
    std::vector<std::string> searchDirectories;
};

// Sort mode for project list
enum class ProjectSortMode : u8 {
    LastOpened,     // Most recently opened first
    Name,           // Alphabetical
    EngineVersion,  // By engine version
    Size            // By project size
};

/**
 * @brief Main Hub application class
 *
 * Manages the GLFW window, ImGui context, and all Hub functionality:
 * project management, engine version management, template browser,
 * and settings.
 */
class HubApplication {
public:
    HubApplication();
    ~HubApplication();

    /**
     * @brief Initialize the Hub application
     * Creates GLFW window, sets up OpenGL context, initializes ImGui.
     * @return true on success
     */
    bool Initialize();

    /**
     * @brief Run the main loop
     * @return Exit code (0 = success)
     */
    i32 Run();

    /**
     * @brief Shutdown and clean up resources
     */
    void Shutdown();

    // --- Project Management ---

    /**
     * @brief Scan a directory for .enjinproject files
     * @param searchDir Directory to scan (recursive)
     * @return Number of projects found
     */
    u32 ScanForProjects(const std::string& searchDir);

    /**
     * @brief Create a new project from a template
     * @param name Project name
     * @param path Directory to create the project in
     * @param templateName Name of the template to use (empty = blank project)
     * @return true on success
     */
    bool CreateProject(const std::string& name, const std::string& path,
                       const std::string& templateName = "");

    /**
     * @brief Open a project in the Enjin Editor
     * @param projectPath Path to the .enjinproject file
     * @return true if the editor was launched successfully
     */
    bool OpenProject(const std::string& projectPath);

    /**
     * @brief Remove a project from the recent list (does not delete files)
     * @param index Index in the project list
     */
    void RemoveFromRecents(u32 index);

    /**
     * @brief Get the current project list
     */
    const std::vector<ProjectEntry>& GetProjects() const { return m_Projects; }

    /**
     * @brief Sort the project list
     */
    void SortProjects(ProjectSortMode mode);

    // --- Engine Version Management ---

    /**
     * @brief Get all installed engine versions
     */
    const std::vector<EngineVersion>& GetInstalledVersions() const { return m_EngineVersions; }

    /**
     * @brief Scan for installed engine versions
     */
    void ScanForEngineVersions();

    /**
     * @brief Set the default engine version
     * @param version Version string to set as default
     */
    void SetDefaultVersion(const std::string& version);

    /**
     * @brief Launch the editor with a specific engine version and project
     * @param version Engine version to use
     * @param projectPath Path to the project (empty = open editor without project)
     * @return true if launch succeeded
     */
    bool LaunchEditor(const std::string& version, const std::string& projectPath = "");

    // --- Template Browser ---

    /**
     * @brief Get available project templates
     */
    const std::vector<TemplateEntry>& GetAvailableTemplates() const { return m_Templates; }

    /**
     * @brief Create a new project from a template
     * @param templateName Template to use
     * @param projectPath Destination directory
     * @return true on success
     */
    bool CreateFromTemplate(const std::string& templateName, const std::string& projectPath);

    // --- Settings ---

    HubSettings& GetSettings() { return m_Settings; }
    const HubSettings& GetSettings() const { return m_Settings; }

    /**
     * @brief Save settings to disk
     */
    void SaveSettings();

    /**
     * @brief Load settings from disk
     */
    void LoadSettings();

private:
    // --- UI Drawing ---
    void DrawUI();
    void DrawTitleBar();
    void DrawSidebar();
    void DrawProjectList();
    void DrawEngineVersions();
    void DrawTemplateBrowser();
    void DrawSettingsPanel();
    void DrawNewProjectDialog();
    void DrawAboutDialog();

    // --- Helpers ---
    void LoadTemplates();
    void UpdateProjectTimestamp(const std::string& projectPath);
    std::string GetSettingsPath() const;
    std::string GetDefaultSearchDirectory() const;
    void ApplyTheme();

    // --- State ---
    GLFWwindow* m_Window = nullptr;
    bool m_Running = false;

    // Active panel in sidebar
    enum class Panel : u8 {
        Projects,
        EngineVersions,
        Templates,
        Settings
    };
    Panel m_ActivePanel = Panel::Projects;

    // Data
    std::vector<ProjectEntry> m_Projects;
    std::vector<EngineVersion> m_EngineVersions;
    std::vector<TemplateEntry> m_Templates;
    HubSettings m_Settings;

    // UI state
    bool m_ShowNewProjectDialog = false;
    bool m_ShowAboutDialog = false;
    std::string m_NewProjectName;
    std::string m_NewProjectPath;
    std::string m_NewProjectTemplate;
    std::string m_SearchFilter;
    ProjectSortMode m_SortMode = ProjectSortMode::LastOpened;
};

} // namespace Hub
} // namespace Enjin
