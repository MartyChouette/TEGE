#include "Enjin/Build/BuildPipeline.h"
#include "Enjin/Build/AssetPacker.h"
#include "Enjin/Build/AssetReader.h"
#include "Enjin/Logging/Log.h"
#include "Enjin/Platform/Paths.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>
#include <set>

namespace fs = std::filesystem;

namespace Enjin::Build {

BuildResult BuildPipeline::Execute(const BuildConfig& config) {
    m_Result = BuildResult{};
    m_Config = config;
    m_Scenes.clear();
    m_TexturePaths.clear();
    m_ModelPaths.clear();

    AddMessage(MessageSeverity::Info, "Starting build...");

    // Phase 1: Scan project
    ReportProgress("Scanning project", 0.0f);
    if (!ScanProject(config.projectPath)) {
        m_Result.success = false;
        AddMessage(MessageSeverity::Error, "Build failed: project scan failed");
        return m_Result;
    }

    // Phase 2: Validate assets
    ReportProgress("Validating assets", 0.15f);
    if (!ValidateAssets()) {
        m_Result.success = false;
        AddMessage(MessageSeverity::Error, "Build failed: asset validation failed");
        return m_Result;
    }

    // Phase 3: Pack assets
    ReportProgress("Packing assets", 0.3f);
    if (!PackAssets(config.outputDir, config.buildKey)) {
        m_Result.success = false;
        AddMessage(MessageSeverity::Error, "Build failed: packing failed");
        return m_Result;
    }

    // Phase 4: Copy player
    ReportProgress("Copying player", 0.8f);
    if (!CopyPlayer(config.outputDir)) {
        // Non-fatal: warn but continue (user may not have built the player)
        AddMessage(MessageSeverity::Warning, "Player executable not found. Build the player target (EnjinPlayer) separately.");
    }

    // Phase 5: Verify
    ReportProgress("Verifying build", 0.9f);
    std::string pakPath = (fs::path(config.outputDir) / "game.enjpak").string();
    if (!VerifyBuild(pakPath, config.buildKey)) {
        m_Result.success = false;
        AddMessage(MessageSeverity::Error, "Build failed: verification failed");
        return m_Result;
    }

    ReportProgress("Complete", 1.0f);
    m_Result.success = true;
    m_Result.outputPath = config.outputDir;
    AddMessage(MessageSeverity::Info, "Build succeeded: " + std::to_string(m_Result.filesPacked) +
               " files packed (" + std::to_string(m_Result.totalSizeBytes) + " -> " +
               std::to_string(m_Result.packedSizeBytes) + " bytes)");

    return m_Result;
}

bool BuildPipeline::ScanProject(const std::string& projectPath) {
    if (!fs::exists(projectPath)) {
        AddMessage(MessageSeverity::Error, "Project file does not exist", projectPath);
        return false;
    }

    m_ProjectDir = fs::path(projectPath).parent_path().string();

    try {
        std::ifstream file(projectPath);
        if (!file.is_open()) {
            AddMessage(MessageSeverity::Error, "Cannot open project file", projectPath);
            return false;
        }

        nlohmann::json root;
        file >> root;
        file.close();

        m_ProjectName = root.value("projectName", "Untitled");

        if (!root.contains("scenes") || !root["scenes"].is_array()) {
            AddMessage(MessageSeverity::Error, "Project has no scenes array", projectPath);
            return false;
        }

        bool hasStartScene = false;
        std::set<i32> buildIndices;

        for (const auto& sceneJson : root["scenes"]) {
            SceneInfo info;
            info.name = sceneJson.value("name", "Unnamed");
            info.relativePath = sceneJson.value("path", "");
            info.buildIndex = sceneJson.value("buildIndex", -1);
            info.isStartScene = sceneJson.value("isStartScene", false);

            if (info.relativePath.empty()) {
                AddMessage(MessageSeverity::Warning, "Scene '" + info.name + "' has empty path, skipping");
                continue;
            }

            // Resolve absolute path
            info.absolutePath = (fs::path(m_ProjectDir) / info.relativePath).string();

            if (info.isStartScene) hasStartScene = true;

            // Check for duplicate build indices
            if (info.buildIndex >= 0) {
                if (buildIndices.count(info.buildIndex)) {
                    AddMessage(MessageSeverity::Warning,
                               "Duplicate build index " + std::to_string(info.buildIndex) +
                               " on scene '" + info.name + "'");
                }
                buildIndices.insert(info.buildIndex);
            }

            m_Scenes.push_back(std::move(info));
        }

        if (m_Scenes.empty()) {
            AddMessage(MessageSeverity::Error, "No valid scenes in project");
            return false;
        }

        if (!hasStartScene) {
            AddMessage(MessageSeverity::Warning, "No start scene designated. First scene will be used as start.");
            m_Scenes[0].isStartScene = true;
        }

        AddMessage(MessageSeverity::Info, "Found " + std::to_string(m_Scenes.size()) + " scenes in project '" + m_ProjectName + "'");
        return true;

    } catch (const std::exception& e) {
        AddMessage(MessageSeverity::Error, std::string("Error parsing project: ") + e.what(), projectPath);
        return false;
    }
}

bool BuildPipeline::ValidateAssets() {
    bool allValid = true;

    for (auto& scene : m_Scenes) {
        if (!fs::exists(scene.absolutePath)) {
            AddMessage(MessageSeverity::Error, "Scene file not found: " + scene.relativePath, scene.absolutePath);
            allValid = false;
            continue;
        }

        // Parse scene JSON to find asset references
        try {
            std::ifstream file(scene.absolutePath);
            if (!file.is_open()) {
                AddMessage(MessageSeverity::Error, "Cannot open scene: " + scene.relativePath, scene.absolutePath);
                allValid = false;
                continue;
            }

            nlohmann::json sceneRoot;
            file >> sceneRoot;
            file.close();

            // Check file size warning
            auto fileSize = fs::file_size(scene.absolutePath);
            if (fileSize > 100 * 1024 * 1024) {
                AddMessage(MessageSeverity::Warning,
                           "Scene '" + scene.name + "' is very large (" +
                           std::to_string(fileSize / (1024 * 1024)) + " MB)");
            }

            // Walk entities looking for texture paths
            if (sceneRoot.contains("entities") && sceneRoot["entities"].is_array()) {
                auto& entities = sceneRoot["entities"];

                if (entities.size() > 10000) {
                    AddMessage(MessageSeverity::Warning,
                               "Scene '" + scene.name + "' has " +
                               std::to_string(entities.size()) + " entities (>10k)");
                }

                for (const auto& entity : entities) {
                    if (!entity.contains("components")) continue;
                    const auto& comps = entity["components"];

                    // Material texture paths
                    if (comps.contains("material")) {
                        const auto& mat = comps["material"];

                        auto checkTexture = [&](const std::string& key) {
                            if (mat.contains(key)) {
                                std::string texPath = mat[key].get<std::string>();
                                if (!texPath.empty()) {
                                    // Resolve relative to project dir
                                    std::string absPath = (fs::path(m_ProjectDir) / texPath).string();
                                    if (!fs::exists(absPath)) {
                                        // Try relative to scene file
                                        absPath = (fs::path(scene.absolutePath).parent_path() / texPath).string();
                                    }
                                    if (fs::exists(absPath)) {
                                        m_TexturePaths.insert(absPath);
                                    } else {
                                        AddMessage(MessageSeverity::Error,
                                                   "Missing texture: " + texPath +
                                                   " (referenced in " + scene.name + ")", texPath);
                                        allValid = false;
                                    }
                                }
                            }
                        };

                        checkTexture("baseColorTexturePath");
                        checkTexture("normalTexturePath");
                        checkTexture("heightTexturePath");
                    }
                }
            }

        } catch (const std::exception& e) {
            AddMessage(MessageSeverity::Error,
                       "Error parsing scene '" + scene.name + "': " + e.what(),
                       scene.absolutePath);
            allValid = false;
        }
    }

    AddMessage(MessageSeverity::Info,
               "Asset validation: " + std::to_string(m_TexturePaths.size()) + " textures found");
    return allValid;
}

bool BuildPipeline::PackAssets(const std::string& outputDir, const std::string& key) {
    // Create output directory
    try {
        fs::create_directories(outputDir);
    } catch (const std::exception& e) {
        AddMessage(MessageSeverity::Error, std::string("Cannot create output dir: ") + e.what());
        return false;
    }

    std::string pakPath = (fs::path(outputDir) / "game.enjpak").string();

    AssetPacker packer;
    if (!packer.Begin(pakPath, key)) {
        AddMessage(MessageSeverity::Error, "Failed to begin packing");
        return false;
    }

    // Pack project manifest
    if (!packer.AddFile("project.enjinproject", m_Config.projectPath)) {
        AddMessage(MessageSeverity::Error, "Failed to pack project manifest");
        return false;
    }

    // Pack all scenes
    float sceneProgress = 0.0f;
    float sceneStep = m_Scenes.empty() ? 0.0f : 0.3f / static_cast<float>(m_Scenes.size());
    for (auto& scene : m_Scenes) {
        ReportProgress("Packing scene: " + scene.name, 0.3f + sceneProgress);
        sceneProgress += sceneStep;

        if (!packer.AddFile(scene.relativePath, scene.absolutePath)) {
            AddMessage(MessageSeverity::Error, "Failed to pack scene: " + scene.relativePath);
            return false;
        }
    }

    // Pack all textures
    float texProgress = 0.0f;
    float texStep = m_TexturePaths.empty() ? 0.0f : 0.2f / static_cast<float>(m_TexturePaths.size());
    for (const auto& texPath : m_TexturePaths) {
        ReportProgress("Packing textures", 0.6f + texProgress);
        texProgress += texStep;

        // Virtual path relative to project dir
        std::string virtualPath;
        auto relPath = fs::relative(fs::path(texPath), fs::path(m_ProjectDir));
        if (!relPath.empty()) {
            virtualPath = relPath.generic_string();
        } else {
            virtualPath = fs::path(texPath).filename().string();
        }

        if (!packer.AddFile(virtualPath, texPath)) {
            AddMessage(MessageSeverity::Warning, "Failed to pack texture: " + texPath);
        }
    }

    // Write build manifest
    if (!WriteBuildManifest(m_Config, packer)) {
        AddMessage(MessageSeverity::Error, "Failed to write build manifest");
        return false;
    }

    if (!packer.Finalize()) {
        AddMessage(MessageSeverity::Error, "Failed to finalize pack file");
        return false;
    }

    m_Result.filesPacked = packer.GetFileCount();
    m_Result.totalSizeBytes = packer.GetTotalOriginalSize();
    m_Result.packedSizeBytes = packer.GetTotalPackedSize();

    AddMessage(MessageSeverity::Info, "Packed " + std::to_string(m_Result.filesPacked) + " files to " + pakPath);
    return true;
}

bool BuildPipeline::CopyPlayer(const std::string& outputDir) {
    // Look for the player executable relative to the editor executable
    std::string exeDir = Platform::GetExecutableDirectory();

    // Possible player exe locations
    std::vector<std::string> candidates;
#ifdef ENJIN_PLATFORM_WINDOWS
    candidates.push_back((fs::path(exeDir) / "EnjinPlayer.exe").string());
    candidates.push_back((fs::path(exeDir) / ".." / "EnjinPlayer.exe").string());
    // Also check build directories
    candidates.push_back((fs::path(exeDir) / ".." / "Release" / "EnjinPlayer.exe").string());
    candidates.push_back((fs::path(exeDir) / ".." / "Debug" / "EnjinPlayer.exe").string());
#else
    candidates.push_back((fs::path(exeDir) / "EnjinPlayer").string());
    candidates.push_back((fs::path(exeDir) / ".." / "EnjinPlayer").string());
#endif

    for (const auto& candidate : candidates) {
        if (fs::exists(candidate)) {
            std::string destName = m_Config.windowTitle.empty() ? "Game" : m_Config.windowTitle;
            // Sanitize filename
            for (auto& c : destName) {
                if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' ||
                    c == '"' || c == '<' || c == '>' || c == '|') {
                    c = '_';
                }
            }
#ifdef ENJIN_PLATFORM_WINDOWS
            destName += ".exe";
#endif
            std::string destPath = (fs::path(outputDir) / destName).string();
            try {
                fs::copy_file(candidate, destPath, fs::copy_options::overwrite_existing);
                AddMessage(MessageSeverity::Info, "Copied player to: " + destPath);
                return true;
            } catch (const std::exception& e) {
                AddMessage(MessageSeverity::Warning, std::string("Failed to copy player: ") + e.what());
            }
        }
    }

    AddMessage(MessageSeverity::Warning,
               "Player executable not found. Build the EnjinPlayer CMake target first.");
    return false;
}

bool BuildPipeline::WriteBuildManifest(const BuildConfig& config, AssetPacker& packer) {
    nlohmann::json manifest;
    manifest["windowTitle"] = config.windowTitle.empty() ? m_ProjectName : config.windowTitle;
    manifest["windowWidth"] = config.windowWidth;
    manifest["windowHeight"] = config.windowHeight;
    manifest["fullscreen"] = config.fullscreen;
    manifest["projectName"] = m_ProjectName;

    // Find start scene
    for (const auto& scene : m_Scenes) {
        if (scene.isStartScene) {
            manifest["startScene"] = scene.relativePath;
            break;
        }
    }

    std::string jsonStr = manifest.dump(2);
    if (!packer.AddData("_build/manifest.json",
                        jsonStr.data(),
                        jsonStr.size())) {
        return false;
    }

    return true;
}

bool BuildPipeline::VerifyBuild(const std::string& pakPath, const std::string& key) {
    AssetReader reader;
    if (!reader.Open(pakPath, key)) {
        AddMessage(MessageSeverity::Error, "Cannot open pack for verification");
        return false;
    }

    // Verify we can read the manifest
    if (!reader.HasFile("_build/manifest.json")) {
        AddMessage(MessageSeverity::Error, "Build manifest missing from pack");
        reader.Close();
        return false;
    }

    // Verify project file
    if (!reader.HasFile("project.enjinproject")) {
        AddMessage(MessageSeverity::Error, "Project file missing from pack");
        reader.Close();
        return false;
    }

    // Verify all scenes
    for (const auto& scene : m_Scenes) {
        if (!reader.HasFile(scene.relativePath)) {
            AddMessage(MessageSeverity::Error, "Scene missing from pack: " + scene.relativePath);
            reader.Close();
            return false;
        }
    }

    // Full integrity check
    if (!reader.VerifyIntegrity()) {
        AddMessage(MessageSeverity::Error, "Pack integrity verification failed");
        reader.Close();
        return false;
    }

    AddMessage(MessageSeverity::Info, "Build verification passed (" +
               std::to_string(reader.GetFileCount()) + " files verified)");
    reader.Close();
    return true;
}

void BuildPipeline::AddMessage(MessageSeverity severity, const std::string& text, const std::string& file) {
    m_Result.messages.push_back({severity, text, file});

    switch (severity) {
        case MessageSeverity::Info:
            ENJIN_LOG_INFO(Build, "%s", text.c_str());
            break;
        case MessageSeverity::Warning:
            ENJIN_LOG_WARN(Build, "%s", text.c_str());
            break;
        case MessageSeverity::Error:
            ENJIN_LOG_ERROR(Build, "%s", text.c_str());
            break;
    }
}

void BuildPipeline::ReportProgress(const std::string& phase, float progress) {
    if (m_ProgressCallback) {
        m_ProgressCallback(phase, progress);
    }
}

} // namespace Enjin::Build
