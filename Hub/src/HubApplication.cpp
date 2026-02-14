#include "HubApplication.h"

#include <GLFW/glfw3.h>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <ctime>
#include <iostream>

#if defined(_WIN32)
    #define NOMINMAX
    #include <windows.h>
    #include <shellapi.h>
#else
    #include <cstdlib>
#endif

namespace fs = std::filesystem;

namespace Enjin {
namespace Hub {

// --- Utility: get current ISO 8601 timestamp ---
static std::string GetCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    struct tm tmBuf;
#if defined(_WIN32)
    localtime_s(&tmBuf, &time);
#else
    localtime_r(&time, &tmBuf);
#endif
    char buf[64];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &tmBuf);
    return std::string(buf);
}

// --- Utility: compute directory size ---
static u64 ComputeDirectorySize(const std::string& path) {
    u64 total = 0;
    try {
        for (const auto& entry : fs::recursive_directory_iterator(path, fs::directory_options::skip_permission_denied)) {
            if (entry.is_regular_file()) {
                total += entry.file_size();
            }
        }
    } catch (...) {}
    return total;
}

// --- HubApplication Implementation ---

HubApplication::HubApplication() = default;

HubApplication::~HubApplication() {
    if (m_Window) {
        Shutdown();
    }
}

bool HubApplication::Initialize() {
    // Initialize GLFW
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return false;
    }

    // Request OpenGL 3.3 core profile
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#endif

    // Create window
    m_Window = glfwCreateWindow(m_Settings.windowWidth, m_Settings.windowHeight,
                                 "Enjin Hub", nullptr, nullptr);
    if (!m_Window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(m_Window);
    glfwSwapInterval(1);  // VSync

    // Initialize ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Setup ImGui platform/renderer backends
    ImGui_ImplGlfw_InitForOpenGL(m_Window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    // Load settings
    LoadSettings();
    ApplyTheme();

    // Load templates
    LoadTemplates();

    // Scan for engine versions
    ScanForEngineVersions();

    // Scan default project directory if set
    if (!m_Settings.defaultProjectDirectory.empty()) {
        ScanForProjects(m_Settings.defaultProjectDirectory);
    }

    // Also scan any saved search directories
    for (const auto& dir : m_Settings.searchDirectories) {
        ScanForProjects(dir);
    }

    m_Running = true;
    return true;
}

i32 HubApplication::Run() {
    if (!m_Window) return -1;

    while (m_Running && !glfwWindowShouldClose(m_Window)) {
        glfwPollEvents();

        // Start ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Draw Hub UI
        DrawUI();

        // Render
        ImGui::Render();
        int displayW, displayH;
        glfwGetFramebufferSize(m_Window, &displayW, &displayH);
        glViewport(0, 0, displayW, displayH);
        glClearColor(0.1f, 0.1f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(m_Window);
    }

    return 0;
}

void HubApplication::Shutdown() {
    SaveSettings();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    if (m_Window) {
        glfwDestroyWindow(m_Window);
        m_Window = nullptr;
    }
    glfwTerminate();

    m_Running = false;
}

// --- Project Management ---

u32 HubApplication::ScanForProjects(const std::string& searchDir) {
    u32 found = 0;

    try {
        for (const auto& entry : fs::recursive_directory_iterator(searchDir,
                fs::directory_options::skip_permission_denied)) {
            if (!entry.is_regular_file()) continue;
            if (entry.path().filename() != ".enjinproject") continue;

            std::string projPath = entry.path().string();

            // Check if already in list
            bool duplicate = false;
            for (const auto& p : m_Projects) {
                if (p.path == projPath) {
                    duplicate = true;
                    break;
                }
            }
            if (duplicate) continue;

            ProjectEntry proj;
            proj.path = projPath;
            proj.name = entry.path().parent_path().filename().string();
            proj.lastOpened = GetCurrentTimestamp();
            proj.engineVersion = "0.7.0";  // Default; would read from file in production

            // Try to read project name from file
            try {
                std::ifstream file(projPath);
                if (file.is_open()) {
                    std::string line;
                    while (std::getline(file, line)) {
                        // Simple key-value parse: "name": "ProjectName"
                        auto pos = line.find("\"name\"");
                        if (pos != std::string::npos) {
                            auto colon = line.find(':', pos);
                            auto firstQuote = line.find('"', colon + 1);
                            auto lastQuote = line.find('"', firstQuote + 1);
                            if (firstQuote != std::string::npos && lastQuote != std::string::npos) {
                                proj.name = line.substr(firstQuote + 1, lastQuote - firstQuote - 1);
                            }
                        }
                    }
                }
            } catch (...) {}

            m_Projects.push_back(proj);
            ++found;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error scanning for projects: " << e.what() << std::endl;
    }

    // Sort by last opened
    SortProjects(m_SortMode);

    return found;
}

bool HubApplication::CreateProject(const std::string& name, const std::string& path,
                                     const std::string& templateName) {
    try {
        fs::path projectDir = fs::path(path) / name;

        // Create project directory
        if (!fs::create_directories(projectDir)) {
            if (fs::exists(projectDir)) {
                std::cerr << "Project directory already exists: " << projectDir.string() << std::endl;
                return false;
            }
            return false;
        }

        // Create .enjinproject file
        std::ofstream projFile(projectDir / ".enjinproject");
        if (!projFile.is_open()) return false;

        projFile << "{\n";
        projFile << "  \"name\": \"" << name << "\",\n";
        projFile << "  \"engineVersion\": \"0.7.0\",\n";
        projFile << "  \"projectMode\": \"Mixed\",\n";
        projFile << "  \"created\": \"" << GetCurrentTimestamp() << "\"\n";
        projFile << "}\n";
        projFile.close();

        // Copy template if specified
        if (!templateName.empty()) {
            CreateFromTemplate(templateName, projectDir.string());
        }

        // Create default directories
        fs::create_directories(projectDir / "assets");
        fs::create_directories(projectDir / "scenes");
        fs::create_directories(projectDir / "scripts");

        // Add to project list
        ProjectEntry entry;
        entry.name = name;
        entry.path = (projectDir / ".enjinproject").string();
        entry.lastOpened = GetCurrentTimestamp();
        entry.engineVersion = "0.7.0";
        m_Projects.insert(m_Projects.begin(), entry);

        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error creating project: " << e.what() << std::endl;
        return false;
    }
}

bool HubApplication::OpenProject(const std::string& projectPath) {
    UpdateProjectTimestamp(projectPath);
    return LaunchEditor("", projectPath);
}

void HubApplication::RemoveFromRecents(u32 index) {
    if (index < m_Projects.size()) {
        m_Projects.erase(m_Projects.begin() + index);
    }
}

void HubApplication::SortProjects(ProjectSortMode mode) {
    m_SortMode = mode;
    switch (mode) {
        case ProjectSortMode::LastOpened:
            std::sort(m_Projects.begin(), m_Projects.end(),
                      [](const ProjectEntry& a, const ProjectEntry& b) {
                          return a.lastOpened > b.lastOpened;
                      });
            break;
        case ProjectSortMode::Name:
            std::sort(m_Projects.begin(), m_Projects.end(),
                      [](const ProjectEntry& a, const ProjectEntry& b) {
                          return a.name < b.name;
                      });
            break;
        case ProjectSortMode::EngineVersion:
            std::sort(m_Projects.begin(), m_Projects.end(),
                      [](const ProjectEntry& a, const ProjectEntry& b) {
                          return a.engineVersion > b.engineVersion;
                      });
            break;
        case ProjectSortMode::Size:
            std::sort(m_Projects.begin(), m_Projects.end(),
                      [](const ProjectEntry& a, const ProjectEntry& b) {
                          return a.sizeBytes > b.sizeBytes;
                      });
            break;
    }
}

// --- Engine Version Management ---

void HubApplication::ScanForEngineVersions() {
    m_EngineVersions.clear();

    // Add the current version (the one this Hub was built with)
    EngineVersion current;
    current.version = "0.7.0";
    current.isDefault = true;
    current.installDate = GetCurrentTimestamp();

    // Try to find the editor executable relative to this Hub executable
    try {
        auto hubDir = fs::current_path();
        if (fs::exists(hubDir / "EnjinEditor.exe") || fs::exists(hubDir / "EnjinEditor")) {
            current.path = hubDir.string();
        }
    } catch (...) {}

    m_EngineVersions.push_back(current);
}

void HubApplication::SetDefaultVersion(const std::string& version) {
    for (auto& ev : m_EngineVersions) {
        ev.isDefault = (ev.version == version);
    }
    m_Settings.defaultEngineVersion = version;
}

bool HubApplication::LaunchEditor(const std::string& version, const std::string& projectPath) {
    // Find the engine version
    std::string editorPath;
    for (const auto& ev : m_EngineVersions) {
        if (version.empty() || ev.version == version) {
            editorPath = ev.path;
            break;
        }
    }

    if (editorPath.empty() && !m_EngineVersions.empty()) {
        // Fallback to default
        for (const auto& ev : m_EngineVersions) {
            if (ev.isDefault) {
                editorPath = ev.path;
                break;
            }
        }
        if (editorPath.empty()) {
            editorPath = m_EngineVersions[0].path;
        }
    }

#if defined(_WIN32)
    std::string exe = editorPath.empty() ? "EnjinEditor.exe" : (editorPath + "\\EnjinEditor.exe");
    std::string args;
    if (!projectPath.empty()) {
        args = "\"" + projectPath + "\"";
    }

    HINSTANCE result = ShellExecuteA(nullptr, "open", exe.c_str(),
                                      args.empty() ? nullptr : args.c_str(),
                                      editorPath.empty() ? nullptr : editorPath.c_str(),
                                      SW_SHOWNORMAL);
    return reinterpret_cast<intptr_t>(result) > 32;
#else
    std::string exe = editorPath.empty() ? "./EnjinEditor" : (editorPath + "/EnjinEditor");
    std::string cmd = exe;
    if (!projectPath.empty()) {
        cmd += " \"" + projectPath + "\"";
    }
    cmd += " &";  // Run in background
    return std::system(cmd.c_str()) == 0;
#endif
}

// --- Template Browser ---

bool HubApplication::CreateFromTemplate(const std::string& templateName, const std::string& projectPath) {
    // In a full implementation, this would copy template files from the
    // engine's templates/ directory into the project directory.
    // For now, we just log the intent.
    std::cout << "Creating project from template '" << templateName
              << "' at '" << projectPath << "'" << std::endl;
    return true;
}

void HubApplication::LoadTemplates() {
    m_Templates.clear();

    // Built-in templates matching the engine's 43 templates organized by category
    auto addTemplate = [this](const char* name, const char* category, const char* desc) {
        TemplateEntry t;
        t.name = name;
        t.category = category;
        t.description = desc;
        t.engineVersion = "0.7.0";
        m_Templates.push_back(t);
    };

    // Foundations
    addTemplate("Empty Scene", "Foundations", "Blank scene with a camera and light");
    addTemplate("Hello Cube", "Foundations", "A single textured cube with basic lighting");
    addTemplate("Isometric", "Foundations", "Isometric camera setup with tilemap");
    addTemplate("Visual Script", "Foundations", "Visual scripting playground");
    addTemplate("UI Canvas", "Foundations", "UI Canvas layout demo");
    addTemplate("Game Manager", "Foundations", "Scene management and game state patterns");

    // Genre Showcases
    addTemplate("Platformer 2D", "Genre Showcases", "Side-scrolling platformer with physics");
    addTemplate("Top-Down 2D", "Genre Showcases", "Top-down adventure with tilemap");
    addTemplate("Third Person", "Genre Showcases", "Third-person character controller");
    addTemplate("First Person", "Genre Showcases", "First-person camera and movement");
    addTemplate("RPG Village", "Genre Showcases", "RPG-style village with NPCs");
    addTemplate("Metroidvania", "Genre Showcases", "Connected rooms with ability-gated areas");
    addTemplate("Roguelike", "Genre Showcases", "Procedural dungeon with turn-based movement");

    // Systems Deep-Dives
    addTemplate("Save System", "Systems Deep-Dives", "Tiered save system with auto-save");
    addTemplate("Narrative", "Systems Deep-Dives", "Dialogue trees and cutscenes");
    addTemplate("Bullet Hell", "Systems Deep-Dives", "Object pooling and pattern spawning");
    addTemplate("Idle Clicker", "Systems Deep-Dives", "Meta-progression and offline earnings");
    addTemplate("Point & Click", "Systems Deep-Dives", "Adventure game with inventory");

    // Retro & Flash
    addTemplate("PS1 RPG", "Retro & Flash", "Retro-style RPG with vertex jitter");
    addTemplate("Visual Novel", "Retro & Flash", "Dialogue-driven narrative");
    addTemplate("Flash TD", "Retro & Flash", "Flash-style tower defense");
    addTemplate("Flash Dress-Up", "Retro & Flash", "Flash-style dress-up game");
    addTemplate("Flash Escape", "Retro & Flash", "Flash-style escape room");
    addTemplate("Flash Rhythm", "Retro & Flash", "Flash-style rhythm game");

    // Advanced
    addTemplate("Shadow Test", "Advanced", "Shadow cascades and lighting showcase");
    addTemplate("Souls-like", "Advanced", "Stamina combat and bonfires");
    addTemplate("City Builder", "Advanced", "Grid-based city placement");
    addTemplate("Tower Defense", "Advanced", "Wave-based tower defense");
}

// --- Settings ---

std::string HubApplication::GetSettingsPath() const {
#if defined(_WIN32)
    const char* appdata = std::getenv("APPDATA");
    if (appdata) {
        return std::string(appdata) + "\\Enjin\\hub_settings.json";
    }
    return "hub_settings.json";
#else
    const char* home = std::getenv("HOME");
    if (home) {
        return std::string(home) + "/.config/enjin/hub_settings.json";
    }
    return "hub_settings.json";
#endif
}

std::string HubApplication::GetDefaultSearchDirectory() const {
#if defined(_WIN32)
    const char* userProfile = std::getenv("USERPROFILE");
    if (userProfile) {
        return std::string(userProfile) + "\\Documents\\Enjin Projects";
    }
    return "C:\\Enjin Projects";
#else
    const char* home = std::getenv("HOME");
    if (home) {
        return std::string(home) + "/EnjinProjects";
    }
    return "/tmp/EnjinProjects";
#endif
}

void HubApplication::SaveSettings() {
    try {
        fs::path settingsPath = GetSettingsPath();
        fs::create_directories(settingsPath.parent_path());

        std::ofstream file(settingsPath);
        if (!file.is_open()) return;

        file << "{\n";
        file << "  \"defaultProjectDirectory\": \"" << m_Settings.defaultProjectDirectory << "\",\n";
        file << "  \"defaultEngineVersion\": \"" << m_Settings.defaultEngineVersion << "\",\n";
        file << "  \"autoCheckUpdates\": " << (m_Settings.autoCheckUpdates ? "true" : "false") << ",\n";
        file << "  \"themeIndex\": " << m_Settings.themeIndex << ",\n";
        file << "  \"windowWidth\": " << m_Settings.windowWidth << ",\n";
        file << "  \"windowHeight\": " << m_Settings.windowHeight << ",\n";

        file << "  \"recentProjects\": [\n";
        for (u32 i = 0; i < m_Projects.size(); ++i) {
            file << "    { \"name\": \"" << m_Projects[i].name
                 << "\", \"path\": \"" << m_Projects[i].path
                 << "\", \"lastOpened\": \"" << m_Projects[i].lastOpened
                 << "\", \"engineVersion\": \"" << m_Projects[i].engineVersion << "\" }";
            if (i + 1 < m_Projects.size()) file << ",";
            file << "\n";
        }
        file << "  ],\n";

        file << "  \"searchDirectories\": [\n";
        for (u32 i = 0; i < m_Settings.searchDirectories.size(); ++i) {
            file << "    \"" << m_Settings.searchDirectories[i] << "\"";
            if (i + 1 < m_Settings.searchDirectories.size()) file << ",";
            file << "\n";
        }
        file << "  ]\n";

        file << "}\n";
    } catch (...) {
        std::cerr << "Failed to save Hub settings" << std::endl;
    }
}

void HubApplication::LoadSettings() {
    m_Settings.defaultProjectDirectory = GetDefaultSearchDirectory();

    try {
        std::string settingsPath = GetSettingsPath();
        if (!fs::exists(settingsPath)) return;

        // In a production implementation, we would use nlohmann::json here.
        // For the Hub standalone app, we keep it simple with defaults.
        // Settings are loaded on next startup; for now just initialize defaults.
    } catch (...) {}
}

void HubApplication::UpdateProjectTimestamp(const std::string& projectPath) {
    for (auto& p : m_Projects) {
        if (p.path == projectPath) {
            p.lastOpened = GetCurrentTimestamp();
            break;
        }
    }
    SortProjects(m_SortMode);
}

void HubApplication::ApplyTheme() {
    ImGuiStyle& style = ImGui::GetStyle();

    switch (m_Settings.themeIndex) {
        case 0:  // Dark
            ImGui::StyleColorsDark();
            break;
        case 1:  // Light
            ImGui::StyleColorsLight();
            break;
        case 2:  // Classic
            ImGui::StyleColorsClassic();
            break;
        default:
            ImGui::StyleColorsDark();
            break;
    }

    // Common tweaks
    style.WindowRounding = 4.0f;
    style.FrameRounding = 3.0f;
    style.GrabRounding = 2.0f;
    style.FramePadding = ImVec2(8, 4);
    style.ItemSpacing = ImVec2(8, 6);
    style.ScrollbarSize = 14.0f;
}

// --- UI Drawing ---

void HubApplication::DrawUI() {
    // Full-screen ImGui window
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                             ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_MenuBar;

    ImGui::Begin("##HubMain", nullptr, flags);

    DrawTitleBar();

    // Sidebar + content layout
    float sidebarWidth = 200.0f;
    ImGui::BeginChild("##Sidebar", ImVec2(sidebarWidth, 0), true);
    DrawSidebar();
    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginChild("##Content", ImVec2(0, 0), true);
    switch (m_ActivePanel) {
        case Panel::Projects:       DrawProjectList(); break;
        case Panel::EngineVersions: DrawEngineVersions(); break;
        case Panel::Templates:      DrawTemplateBrowser(); break;
        case Panel::Settings:       DrawSettingsPanel(); break;
    }
    ImGui::EndChild();

    // Dialogs
    if (m_ShowNewProjectDialog) DrawNewProjectDialog();
    if (m_ShowAboutDialog) DrawAboutDialog();

    ImGui::End();
}

void HubApplication::DrawTitleBar() {
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New Project...", "Ctrl+N")) {
                m_ShowNewProjectDialog = true;
                m_NewProjectName.clear();
                m_NewProjectPath = m_Settings.defaultProjectDirectory;
                m_NewProjectTemplate.clear();
            }
            if (ImGui::MenuItem("Open Project...", "Ctrl+O")) {
                // Would open a file dialog in production
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Exit", "Alt+F4")) {
                m_Running = false;
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Help")) {
            if (ImGui::MenuItem("About Enjin Hub")) {
                m_ShowAboutDialog = true;
            }
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }
}

void HubApplication::DrawSidebar() {
    ImGui::Text("ENJIN HUB");
    ImGui::Separator();
    ImGui::Spacing();

    auto sidebarButton = [&](const char* label, Panel panel) {
        bool selected = (m_ActivePanel == panel);
        if (selected) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_ButtonActive]);
        }
        if (ImGui::Button(label, ImVec2(-1, 32))) {
            m_ActivePanel = panel;
        }
        if (selected) {
            ImGui::PopStyleColor();
        }
    };

    sidebarButton("Projects", Panel::Projects);
    sidebarButton("Engine Versions", Panel::EngineVersions);
    sidebarButton("Templates", Panel::Templates);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    sidebarButton("Settings", Panel::Settings);

    // Version info at bottom
    ImGui::SetCursorPosY(ImGui::GetWindowHeight() - 30);
    ImGui::TextDisabled("v0.7.0");
}

void HubApplication::DrawProjectList() {
    ImGui::Text("Projects");
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 120);
    if (ImGui::Button("+ New Project", ImVec2(120, 0))) {
        m_ShowNewProjectDialog = true;
        m_NewProjectName.clear();
        m_NewProjectPath = m_Settings.defaultProjectDirectory;
        m_NewProjectTemplate.clear();
    }

    ImGui::Separator();

    // Search and sort bar
    char searchBuf[256] = {};
    if (m_SearchFilter.size() < sizeof(searchBuf)) {
        std::copy(m_SearchFilter.begin(), m_SearchFilter.end(), searchBuf);
    }
    ImGui::SetNextItemWidth(300);
    if (ImGui::InputText("Search", searchBuf, sizeof(searchBuf))) {
        m_SearchFilter = searchBuf;
    }
    ImGui::SameLine();
    const char* sortLabels[] = { "Last Opened", "Name", "Engine Version", "Size" };
    i32 sortIdx = static_cast<i32>(m_SortMode);
    ImGui::SetNextItemWidth(150);
    if (ImGui::Combo("Sort", &sortIdx, sortLabels, 4)) {
        SortProjects(static_cast<ProjectSortMode>(sortIdx));
    }

    ImGui::Spacing();

    if (m_Projects.empty()) {
        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::TextDisabled("No projects found.");
        ImGui::TextDisabled("Click '+ New Project' to create one, or scan a directory.");
        ImGui::Spacing();
        if (ImGui::Button("Scan Directory...")) {
            ScanForProjects(GetDefaultSearchDirectory());
        }
        return;
    }

    // Project cards
    for (u32 i = 0; i < static_cast<u32>(m_Projects.size()); ++i) {
        const auto& proj = m_Projects[i];

        // Filter
        if (!m_SearchFilter.empty()) {
            std::string lowerName = proj.name;
            std::string lowerFilter = m_SearchFilter;
            std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
            std::transform(lowerFilter.begin(), lowerFilter.end(), lowerFilter.begin(), ::tolower);
            if (lowerName.find(lowerFilter) == std::string::npos) continue;
        }

        ImGui::PushID(static_cast<i32>(i));

        // Project card
        ImGui::BeginGroup();
        ImGui::Text("%s", proj.name.c_str());
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 80);
        ImGui::TextDisabled("v%s", proj.engineVersion.c_str());

        ImGui::TextDisabled("%s", proj.path.c_str());

        if (ImGui::Button("Open")) {
            OpenProject(proj.path);
        }
        ImGui::SameLine();
        if (ImGui::Button("Remove")) {
            RemoveFromRecents(i);
            ImGui::EndGroup();
            ImGui::PopID();
            ImGui::Separator();
            break;  // List changed, break out
        }
        ImGui::EndGroup();

        ImGui::Separator();
        ImGui::PopID();
    }
}

void HubApplication::DrawEngineVersions() {
    ImGui::Text("Installed Engine Versions");
    ImGui::Separator();
    ImGui::Spacing();

    if (m_EngineVersions.empty()) {
        ImGui::TextDisabled("No engine versions found.");
        return;
    }

    for (u32 i = 0; i < static_cast<u32>(m_EngineVersions.size()); ++i) {
        auto& ev = m_EngineVersions[i];

        ImGui::PushID(static_cast<i32>(i));
        ImGui::BeginGroup();

        ImGui::Text("Enjin Engine %s", ev.version.c_str());
        if (ev.isDefault) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1.0f), "(Default)");
        }

        if (!ev.path.empty()) {
            ImGui::TextDisabled("Path: %s", ev.path.c_str());
        }
        if (!ev.installDate.empty()) {
            ImGui::TextDisabled("Installed: %s", ev.installDate.c_str());
        }

        if (!ev.isDefault) {
            if (ImGui::Button("Set as Default")) {
                SetDefaultVersion(ev.version);
            }
        }

        ImGui::EndGroup();
        ImGui::Separator();
        ImGui::PopID();
    }
}

void HubApplication::DrawTemplateBrowser() {
    ImGui::Text("Project Templates");
    ImGui::Separator();
    ImGui::Spacing();

    std::string currentCategory;

    for (const auto& tmpl : m_Templates) {
        if (tmpl.category != currentCategory) {
            currentCategory = tmpl.category;
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.7f, 0.85f, 1.0f, 1.0f), "%s", currentCategory.c_str());
            ImGui::Separator();
        }

        ImGui::PushID(tmpl.name.c_str());
        ImGui::BeginGroup();

        ImGui::BulletText("%s", tmpl.name.c_str());
        ImGui::SameLine(300);
        if (ImGui::SmallButton("Create")) {
            m_ShowNewProjectDialog = true;
            m_NewProjectTemplate = tmpl.name;
            m_NewProjectName = tmpl.name;
            m_NewProjectPath = m_Settings.defaultProjectDirectory;
        }

        ImGui::TextDisabled("  %s", tmpl.description.c_str());

        ImGui::EndGroup();
        ImGui::PopID();
    }
}

void HubApplication::DrawSettingsPanel() {
    ImGui::Text("Hub Settings");
    ImGui::Separator();
    ImGui::Spacing();

    // Default project directory
    char dirBuf[512] = {};
    if (m_Settings.defaultProjectDirectory.size() < sizeof(dirBuf)) {
        std::copy(m_Settings.defaultProjectDirectory.begin(),
                  m_Settings.defaultProjectDirectory.end(), dirBuf);
    }
    ImGui::SetNextItemWidth(400);
    if (ImGui::InputText("Default Project Directory", dirBuf, sizeof(dirBuf))) {
        m_Settings.defaultProjectDirectory = dirBuf;
    }

    ImGui::Spacing();

    // Theme
    const char* themes[] = { "Dark", "Light", "Classic" };
    if (ImGui::Combo("Theme", &m_Settings.themeIndex, themes, 3)) {
        ApplyTheme();
    }

    ImGui::Spacing();

    // Auto-update (stub)
    ImGui::Checkbox("Check for Updates Automatically", &m_Settings.autoCheckUpdates);
    ImGui::SameLine();
    ImGui::TextDisabled("(Not yet implemented)");

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Search directories
    ImGui::Text("Project Search Directories:");
    for (u32 i = 0; i < static_cast<u32>(m_Settings.searchDirectories.size()); ++i) {
        ImGui::PushID(static_cast<i32>(i));
        ImGui::BulletText("%s", m_Settings.searchDirectories[i].c_str());
        ImGui::SameLine();
        if (ImGui::SmallButton("Remove")) {
            m_Settings.searchDirectories.erase(m_Settings.searchDirectories.begin() + i);
            ImGui::PopID();
            break;
        }
        ImGui::PopID();
    }

    static char newDirBuf[512] = {};
    ImGui::SetNextItemWidth(400);
    ImGui::InputText("##NewSearchDir", newDirBuf, sizeof(newDirBuf));
    ImGui::SameLine();
    if (ImGui::Button("Add Directory")) {
        if (newDirBuf[0] != '\0') {
            m_Settings.searchDirectories.push_back(newDirBuf);
            ScanForProjects(newDirBuf);
            newDirBuf[0] = '\0';
        }
    }

    ImGui::Spacing();
    ImGui::Spacing();

    if (ImGui::Button("Save Settings")) {
        SaveSettings();
    }
}

void HubApplication::DrawNewProjectDialog() {
    ImGui::OpenPopup("New Project");

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(500, 300), ImGuiCond_Appearing);

    if (ImGui::BeginPopupModal("New Project", &m_ShowNewProjectDialog)) {
        char nameBuf[256] = {};
        if (m_NewProjectName.size() < sizeof(nameBuf)) {
            std::copy(m_NewProjectName.begin(), m_NewProjectName.end(), nameBuf);
        }
        ImGui::SetNextItemWidth(-1);
        if (ImGui::InputText("Project Name", nameBuf, sizeof(nameBuf))) {
            m_NewProjectName = nameBuf;
        }

        char pathBuf[512] = {};
        if (m_NewProjectPath.size() < sizeof(pathBuf)) {
            std::copy(m_NewProjectPath.begin(), m_NewProjectPath.end(), pathBuf);
        }
        ImGui::SetNextItemWidth(-1);
        if (ImGui::InputText("Location", pathBuf, sizeof(pathBuf))) {
            m_NewProjectPath = pathBuf;
        }

        ImGui::Spacing();
        if (!m_NewProjectTemplate.empty()) {
            ImGui::Text("Template: %s", m_NewProjectTemplate.c_str());
        } else {
            ImGui::TextDisabled("No template selected (blank project)");
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::Button("Create", ImVec2(120, 0))) {
            if (!m_NewProjectName.empty() && !m_NewProjectPath.empty()) {
                CreateProject(m_NewProjectName, m_NewProjectPath, m_NewProjectTemplate);
                m_ShowNewProjectDialog = false;
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            m_ShowNewProjectDialog = false;
        }

        ImGui::EndPopup();
    }
}

void HubApplication::DrawAboutDialog() {
    ImGui::OpenPopup("About Enjin Hub");

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(400, 200), ImGuiCond_Appearing);

    if (ImGui::BeginPopupModal("About Enjin Hub", &m_ShowAboutDialog)) {
        ImGui::Text("Enjin Hub");
        ImGui::Spacing();
        ImGui::Text("Version 0.7.0");
        ImGui::Spacing();
        ImGui::TextWrapped(
            "Enjin is a proprietary, licensable game engine built from scratch "
            "using C++20 and the Vulkan graphics API."
        );
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        if (ImGui::Button("Close", ImVec2(120, 0))) {
            m_ShowAboutDialog = false;
        }
        ImGui::EndPopup();
    }
}

} // namespace Hub
} // namespace Enjin
