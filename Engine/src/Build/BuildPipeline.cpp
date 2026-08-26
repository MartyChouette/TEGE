#include "Enjin/Build/BuildPipeline.h"
#include "Enjin/Build/AssetPacker.h"
#include "Enjin/Build/AssetReader.h"
#include "Enjin/Build/HTML5Exporter.h"
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
    m_ScriptPaths.clear();
    m_AudioPaths.clear();
    m_DialoguePaths.clear();
    m_PrefabPaths.clear();
    m_DataAssetPaths.clear();

    // Validate build config dimensions
    if (config.windowWidth < 1 || config.windowWidth > 7680 ||
        config.windowHeight < 1 || config.windowHeight > 4320) {
        m_Result.success = false;
        AddMessage(MessageSeverity::Error, "Invalid window dimensions: " +
                   std::to_string(config.windowWidth) + "x" + std::to_string(config.windowHeight) +
                   " (must be 1-7680 x 1-4320)");
        return m_Result;
    }

    AddMessage(MessageSeverity::Info, "Starting build...");

    // Phase 1: Scan project
    ReportProgress("Scanning project", 0.0f);
    if (!ScanProject(config.projectPath)) {
        m_Result.success = false;
        AddMessage(MessageSeverity::Error, "Build failed: project scan failed");
        return m_Result;
    }

    // Phase 1b: Scan project directory for all asset files
    ReportProgress("Scanning assets", 0.1f);
    ScanProjectDirectory();

    // Phase 2: Validate assets
    ReportProgress("Validating assets", 0.15f);
    if (!ValidateAssets()) {
        m_Result.success = false;
        AddMessage(MessageSeverity::Error, "Build failed: asset validation failed");
        return m_Result;
    }

    // Phase 3: Pack or copy assets based on packaging mode
    ReportProgress("Packing assets", 0.3f);
    if (config.packagingMode == PackagingMode::LooseFiles) {
        if (!CopyLooseFiles(config.outputDir)) {
            m_Result.success = false;
            AddMessage(MessageSeverity::Error, "Build failed: file copy failed");
            return m_Result;
        }
    } else {
        // Packed or PackedOpen — PackedOpen forces empty key
        std::string effectiveKey = config.buildKey;
        if (config.packagingMode == PackagingMode::PackedOpen) {
            effectiveKey = "";
        }
        if (!PackAssets(config.outputDir, effectiveKey)) {
            m_Result.success = false;
            AddMessage(MessageSeverity::Error, "Build failed: packing failed");
            return m_Result;
        }
    }

    // Phase 4: Copy player (desktop) or invoke Emscripten build (web)
    ReportProgress("Building player", 0.8f);
    if (config.target == BuildTargetPlatform::Web) {
        std::string pakPath = (fs::path(config.outputDir) / "game.enjpak").string();
        if (!HTML5Exporter::InvokeEmscriptenBuild(config.outputDir, pakPath)) {
            AddMessage(MessageSeverity::Warning, "Emscripten build failed — WASM output may be missing. Ensure Emscripten SDK is installed.");
        }

        // Generate HTML shell
        HTML5ExportConfig htmlConfig;
        htmlConfig.outputDir = config.outputDir;
        htmlConfig.title = config.windowTitle.empty() ? "Enjin Game" : config.windowTitle;
        htmlConfig.width = config.windowWidth;
        htmlConfig.height = config.windowHeight;
        auto htmlResult = HTML5Exporter::Export(htmlConfig, config);
        if (!htmlResult.success) {
            AddMessage(MessageSeverity::Warning, "HTML export failed: " + htmlResult.error);
        } else {
            AddMessage(MessageSeverity::Info, "Web export: " + htmlResult.outputPath);
        }
    } else {
        // Scripts, the enjin_api headers, and script-referenced assets (audio
        // one-shots etc., invisible to scene scanning) always ship as loose
        // files next to the executable. The script engine can now also read
        // scripts from the .enjpak asset pack (ScriptEngine::SetAssetReader) as
        // a fallback, but the loose copies are what the runtime loads today.
        EmitLooseRuntimeFiles(config.outputDir);
        if (!CopyPlayer(config.outputDir)) {
            // Non-fatal: warn but continue (user may not have built the player)
            AddMessage(MessageSeverity::Warning, "Player executable not found. Build the player target (EnjinPlayer) separately.");
        }
    }

    // Phase 5: Verify
    ReportProgress("Verifying build", 0.9f);
    if (config.packagingMode == PackagingMode::LooseFiles) {
        if (!VerifyLooseBuild(config.outputDir)) {
            m_Result.success = false;
            AddMessage(MessageSeverity::Error, "Build failed: verification failed");
            return m_Result;
        }
    } else {
        std::string pakPath = (fs::path(config.outputDir) / "game.enjpak").string();
        std::string effectiveKey = config.buildKey;
        if (config.packagingMode == PackagingMode::PackedOpen) {
            effectiveKey = "";
        }
        if (!VerifyBuild(pakPath, effectiveKey)) {
            m_Result.success = false;
            AddMessage(MessageSeverity::Error, "Build failed: verification failed");
            return m_Result;
        }
    }

    ReportProgress("Complete", 1.0f);
    m_Result.success = true;
    m_Result.outputPath = config.outputDir;
    if (config.packagingMode == PackagingMode::LooseFiles) {
        AddMessage(MessageSeverity::Info, "Build succeeded: " + std::to_string(m_Result.filesPacked) +
                   " files copied (" + std::to_string(m_Result.totalSizeBytes) + " bytes)");
    } else {
        AddMessage(MessageSeverity::Info, "Build succeeded: " + std::to_string(m_Result.filesPacked) +
                   " files packed (" + std::to_string(m_Result.totalSizeBytes) + " -> " +
                   std::to_string(m_Result.packedSizeBytes) + " bytes)");
    }

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
            // In-memory only — the .enjinproject is never rewritten here. Matches
            // SceneManager::NormalizeSceneList's election (lowest build index wins),
            // so this branch only fires for hand-edited manifests.
            SceneInfo* pick = nullptr;
            for (auto& scene : m_Scenes) {
                if (scene.buildIndex < 0) continue;
                if (!pick || scene.buildIndex < pick->buildIndex) {
                    pick = &scene;
                }
            }
            if (!pick) {
                pick = &m_Scenes[0];
            }
            pick->isStartScene = true;
            AddMessage(MessageSeverity::Warning,
                       "No start scene designated in project manifest. Using '" + pick->name +
                       "' (lowest build index). Set one in the Scenes panel.");
        }

        // Read frame settings from project
        if (root.contains("frameSettings") && root["frameSettings"].is_object()) {
            const auto& fs = root["frameSettings"];
            m_TargetFrameRate = fs.value("targetFrameRate", 60u);
            m_VSync = fs.value("vSync", true);
            m_BackgroundBehavior = fs.value("backgroundBehavior", 1u);
        } else {
            // Defaults
            m_TargetFrameRate = 60;
            m_VSync = true;
            m_BackgroundBehavior = 1;
        }

        // Read physics backend and project mode
        m_PhysicsBackendType = root.value("physicsBackend", 0u);  // 0 = Auto
        m_ProjectMode = root.value("projectMode", 1u);            // 1 = 3D

        // Carry the authorable startup flow (boot sequence) through to the game
        // manifest verbatim, so the player runs it. Absent = the classic default.
        if (root.contains("startupFlow") && root["startupFlow"].is_array()) {
            m_StartupFlowJson = root["startupFlow"].dump();
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
        AddMessage(MessageSeverity::Info, "Checking scene: '" + scene.absolutePath + "' (relative: '" + scene.relativePath + "', projectDir: '" + m_ProjectDir + "')");
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

                // Path traversal guard — validates and resolves an asset path
                fs::path projectNorm = fs::path(m_ProjectDir).lexically_normal();
                auto validateAssetPath = [&](const std::string& relPath,
                                             std::set<std::string>& destSet,
                                             const char* assetType,
                                             bool required = false) {
                    if (relPath.empty()) return;
                    std::string absPath = (fs::path(m_ProjectDir) / relPath).string();
                    if (!fs::exists(absPath)) {
                        // Try relative to scene file
                        absPath = (fs::path(scene.absolutePath).parent_path() / relPath).string();
                    }
                    // Security: validate path stays within project directory
                    fs::path normalized = fs::path(absPath).lexically_normal();
                    auto rel = normalized.lexically_relative(projectNorm);
                    if (rel.string().find("..") == 0) {
                        AddMessage(MessageSeverity::Error,
                                   "Path traversal blocked: " + relPath +
                                   " resolves outside project directory (in " + scene.name + ")", relPath);
                        allValid = false;
                        return;
                    }
                    if (fs::exists(absPath)) {
                        destSet.insert(absPath);
                    } else if (required) {
                        AddMessage(MessageSeverity::Error,
                                   std::string("Missing ") + assetType + ": " + relPath +
                                   " (referenced in " + scene.name + ")", relPath);
                        allValid = false;
                    }
                };

                for (const auto& entity : entities) {
                    // Material texture paths
                    if (entity.contains("material")) {
                        const auto& mat = entity["material"];
                        for (const char* key : {"baseColorTexturePath", "normalTexturePath",
                                                "heightTexturePath", "metallicRoughnessTexturePath",
                                                "emissiveTexturePath"}) {
                            if (mat.contains(key)) {
                                validateAssetPath(mat[key].get<std::string>(), m_TexturePaths, "texture", true);
                            }
                        }
                    }

                    // Sprite2D texture path
                    if (entity.contains("sprite2D")) {
                        const auto& sprite = entity["sprite2D"];
                        if (sprite.contains("texturePath")) {
                            validateAssetPath(sprite["texturePath"].get<std::string>(), m_TexturePaths, "sprite texture");
                        }
                    }

                    // Audio source file paths
                    if (entity.contains("audioSource")) {
                        const auto& audio = entity["audioSource"];
                        if (audio.contains("filePath")) {
                            validateAssetPath(audio["filePath"].get<std::string>(), m_AudioPaths, "audio file");
                        }
                    }

                    // Script component paths
                    if (entity.contains("scriptComponent")) {
                        const auto& sc = entity["scriptComponent"];
                        if (sc.contains("scripts") && sc["scripts"].is_array()) {
                            for (const auto& script : sc["scripts"]) {
                                if (script.contains("path")) {
                                    validateAssetPath(script["path"].get<std::string>(), m_ScriptPaths, "script");
                                }
                            }
                        }
                    }

                    // Tilemap tileset texture
                    if (entity.contains("tilemap")) {
                        const auto& tm = entity["tilemap"];
                        if (tm.contains("tilesetPath")) {
                            validateAssetPath(tm["tilesetPath"].get<std::string>(), m_TexturePaths, "tileset texture");
                        }
                    }

                    // Tree volume textures
                    if (entity.contains("treeVolume")) {
                        const auto& tree = entity["treeVolume"];
                        for (const char* key : {"barkTexturePath", "canopyTexturePath"}) {
                            if (tree.contains(key)) {
                                validateAssetPath(tree[key].get<std::string>(), m_TexturePaths, "tree texture");
                            }
                        }
                    }

                    // Shrub volume custom asset
                    if (entity.contains("shrubVolume")) {
                        const auto& shrub = entity["shrubVolume"];
                        if (shrub.contains("customAssetPath")) {
                            validateAssetPath(shrub["customAssetPath"].get<std::string>(), m_TexturePaths, "shrub asset");
                        }
                    }

                    // Weather zone custom precipitation sprites
                    if (entity.contains("weatherZone")) {
                        const auto& wz = entity["weatherZone"];
                        for (const char* key : {"rainTexturePath", "snowTexturePath"}) {
                            if (wz.contains(key)) {
                                validateAssetPath(wz[key].get<std::string>(), m_TexturePaths, "weather sprite");
                            }
                        }
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

    // Pack scripts
    for (const auto& path : m_ScriptPaths) {
        auto relPath = fs::relative(fs::path(path), fs::path(m_ProjectDir));
        if (!packer.AddFile(relPath.generic_string(), path)) {
            AddMessage(MessageSeverity::Warning, "Failed to pack script: " + path);
        }
    }

    // Pack audio files
    for (const auto& path : m_AudioPaths) {
        auto relPath = fs::relative(fs::path(path), fs::path(m_ProjectDir));
        if (!packer.AddFile(relPath.generic_string(), path)) {
            AddMessage(MessageSeverity::Warning, "Failed to pack audio: " + path);
        }
    }

    // Pack dialogue files
    for (const auto& path : m_DialoguePaths) {
        auto relPath = fs::relative(fs::path(path), fs::path(m_ProjectDir));
        if (!packer.AddFile(relPath.generic_string(), path)) {
            AddMessage(MessageSeverity::Warning, "Failed to pack dialogue: " + path);
        }
    }

    // Pack prefab files
    for (const auto& path : m_PrefabPaths) {
        auto relPath = fs::relative(fs::path(path), fs::path(m_ProjectDir));
        if (!packer.AddFile(relPath.generic_string(), path)) {
            AddMessage(MessageSeverity::Warning, "Failed to pack prefab: " + path);
        }
    }

    // Pack data asset files (.enjdata, .enjschema)
    for (const auto& path : m_DataAssetPaths) {
        auto relPath = fs::relative(fs::path(path), fs::path(m_ProjectDir));
        if (!packer.AddFile(relPath.generic_string(), path)) {
            AddMessage(MessageSeverity::Warning, "Failed to pack data asset: " + path);
        }
    }

    // Pack 3D model files (.gltf, .glb, .fbx, .obj, .dae, .3ds, .ply, .vox)
    for (const auto& path : m_ModelPaths) {
        auto relPath = fs::relative(fs::path(path), fs::path(m_ProjectDir));
        if (!packer.AddFile(relPath.generic_string(), path)) {
            AddMessage(MessageSeverity::Warning, "Failed to pack model: " + path);
        }
    }

    // Pack window icon if present in project directory
    // Check for icon.png next to the project file (standard convention)
    std::string iconPath = (fs::path(m_ProjectDir) / "icon.png").string();
    if (fs::exists(iconPath)) {
        if (!packer.AddFile("icon.png", iconPath)) {
            AddMessage(MessageSeverity::Warning, "Failed to pack window icon: " + iconPath);
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

void BuildPipeline::EmitLooseRuntimeFiles(const std::string& outputDir) {
    std::error_code ec;
    u32 copied = 0;

    auto copyRel = [&](const std::string& absPath) {
        auto rel = fs::relative(fs::path(absPath), fs::path(m_ProjectDir), ec);
        if (ec || rel.empty()) return;
        fs::path dest = fs::path(outputDir) / rel;
        fs::create_directories(dest.parent_path(), ec);
        if (fs::copy_file(absPath, dest, fs::copy_options::overwrite_existing, ec)) {
            ++copied;
        }
    };

    // Scene-referenced scripts
    for (const auto& path : m_ScriptPaths) {
        copyRel(path);
    }

    // enjin_api script headers (TegeBehavior.as etc. — needed for #include
    // resolution and the auto-injected base class)
    fs::path apiDir = fs::path(m_ProjectDir) / "scripts" / "enjin_api";
    if (fs::exists(apiDir, ec)) {
        fs::create_directories(fs::path(outputDir) / "scripts" / "enjin_api", ec);
        fs::copy(apiDir, fs::path(outputDir) / "scripts" / "enjin_api",
                 fs::copy_options::recursive | fs::copy_options::overwrite_existing, ec);
        ++copied;
    }

    // The whole assets/ directory: script-referenced files (audio one-shots,
    // UI images set at runtime) can't be discovered by scene scanning
    fs::path assetsDir = fs::path(m_ProjectDir) / "assets";
    if (fs::exists(assetsDir, ec)) {
        fs::create_directories(fs::path(outputDir) / "assets", ec);
        fs::copy(assetsDir, fs::path(outputDir) / "assets",
                 fs::copy_options::recursive | fs::copy_options::overwrite_existing, ec);
        ++copied;
    }

    AddMessage(MessageSeverity::Info,
               "Emitted loose runtime files (scripts + enjin_api + assets) to " + outputDir);
}

bool BuildPipeline::CopyPlayer(const std::string& outputDir) {
    // Look for the player executable relative to the editor executable
    std::string exeDir = Platform::GetExecutableDirectory();

    // Possible player exe locations
    std::vector<std::string> candidates;
#ifdef ENJIN_PLATFORM_WINDOWS
    candidates.push_back((fs::path(exeDir) / "EnjinPlayer.exe").string());
    candidates.push_back((fs::path(exeDir) / ".." / "EnjinPlayer.exe").string());
    candidates.push_back((fs::path(exeDir) / ".." / "Release" / "EnjinPlayer.exe").string());
    candidates.push_back((fs::path(exeDir) / ".." / "Debug" / "EnjinPlayer.exe").string());
    candidates.push_back((fs::path(exeDir) / ".." / ".." / "bin" / "Release" / "EnjinPlayer.exe").string());
    candidates.push_back((fs::path(exeDir) / ".." / ".." / "bin" / "Debug" / "EnjinPlayer.exe").string());
    candidates.push_back((fs::path(exeDir) / "bin" / "Release" / "EnjinPlayer.exe").string());
    candidates.push_back((fs::path(exeDir) / "bin" / "Debug" / "EnjinPlayer.exe").string());
#else
    candidates.push_back((fs::path(exeDir) / "EnjinPlayer").string());
    candidates.push_back((fs::path(exeDir) / ".." / "EnjinPlayer").string());
    candidates.push_back((fs::path(exeDir) / ".." / ".." / "bin" / "EnjinPlayer").string());
    candidates.push_back((fs::path(exeDir) / "bin" / "EnjinPlayer").string());
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
    manifest["engineSplash"] = config.engineSplash;
    manifest["projectName"] = m_ProjectName;

    // Find start scene
    for (const auto& scene : m_Scenes) {
        if (scene.isStartScene) {
            manifest["startScene"] = scene.relativePath;
            break;
        }
    }

    // Frame settings
    nlohmann::json frameSettings;
    frameSettings["targetFrameRate"] = m_TargetFrameRate;
    frameSettings["vSync"] = m_VSync;
    frameSettings["backgroundBehavior"] = m_BackgroundBehavior;
    manifest["frameSettings"] = frameSettings;

    // Physics backend and project mode
    manifest["physicsBackend"] = m_PhysicsBackendType;
    manifest["projectMode"] = m_ProjectMode;

    // Startup flow (if the project authored one)
    if (!m_StartupFlowJson.empty()) {
        manifest["startupFlow"] = nlohmann::json::parse(m_StartupFlowJson);
    }

    std::string jsonStr = manifest.dump(2);
    if (!packer.AddData("_build/manifest.json",
                        jsonStr.data(),
                        jsonStr.size())) {
        return false;
    }

    // Pack a default accessibility.json so the player can load and save settings
    nlohmann::json accessDefaults;
    accessDefaults["colorblindMode"] = 0;
    accessDefaults["colorblindStrength"] = 1.0f;
    accessDefaults["screenBrightness"] = 0.0f;
    accessDefaults["screenContrast"] = 1.0f;
    accessDefaults["reducedMotion"] = false;
    accessDefaults["disableScreenShake"] = false;
    accessDefaults["disableFOVEffects"] = false;
    accessDefaults["disableFlashingLights"] = false;
    accessDefaults["subtitlesEnabled"] = false;
    accessDefaults["closedCaptionsEnabled"] = false;
    accessDefaults["subtitleFontSize"] = 24.0f;
    accessDefaults["subtitleBgOpacity"] = 0.7f;
    accessDefaults["fontScale"] = 1.0f;
    accessDefaults["dyslexiaFriendly"] = false;
    std::string accessStr = accessDefaults.dump(2);
    packer.AddData("accessibility.json", accessStr.data(), accessStr.size());

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

bool BuildPipeline::CopyLooseFiles(const std::string& outputDir) {
    // Create output directory
    try {
        fs::create_directories(outputDir);
    } catch (const std::exception& e) {
        AddMessage(MessageSeverity::Error, std::string("Cannot create output dir: ") + e.what());
        return false;
    }

    u32 filesCopied = 0;
    u64 totalBytes = 0;

    // Helper: copy a file to outputDir preserving its relative path from project root
    auto copyFileRelative = [&](const std::string& absPath, const std::string& virtualPath) -> bool {
        fs::path destPath = fs::path(outputDir) / virtualPath;
        try {
            fs::create_directories(destPath.parent_path());
            fs::copy_file(absPath, destPath, fs::copy_options::overwrite_existing);
            totalBytes += fs::file_size(destPath);
            filesCopied++;
            return true;
        } catch (const std::exception& e) {
            AddMessage(MessageSeverity::Warning, "Failed to copy: " + virtualPath + " (" + e.what() + ")");
            return false;
        }
    };

    // Copy project manifest
    if (!copyFileRelative(m_Config.projectPath, "project.enjinproject")) {
        AddMessage(MessageSeverity::Error, "Failed to copy project manifest");
        return false;
    }

    // Copy all scenes
    float sceneProgress = 0.0f;
    float sceneStep = m_Scenes.empty() ? 0.0f : 0.3f / static_cast<float>(m_Scenes.size());
    for (auto& scene : m_Scenes) {
        ReportProgress("Copying scene: " + scene.name, 0.3f + sceneProgress);
        sceneProgress += sceneStep;
        if (!copyFileRelative(scene.absolutePath, scene.relativePath)) {
            AddMessage(MessageSeverity::Error, "Failed to copy scene: " + scene.relativePath);
            return false;
        }
    }

    // Helper: copy a set of asset files, computing relative paths from project dir
    auto copyAssetSet = [&](const std::set<std::string>& paths, const char* label) {
        for (const auto& path : paths) {
            auto relPath = fs::relative(fs::path(path), fs::path(m_ProjectDir));
            std::string virtualPath = relPath.generic_string();
            if (!copyFileRelative(path, virtualPath)) {
                AddMessage(MessageSeverity::Warning, std::string("Failed to copy ") + label + ": " + path);
            }
        }
    };

    ReportProgress("Copying textures", 0.6f);
    copyAssetSet(m_TexturePaths, "texture");

    ReportProgress("Copying scripts", 0.65f);
    copyAssetSet(m_ScriptPaths, "script");

    ReportProgress("Copying audio", 0.7f);
    copyAssetSet(m_AudioPaths, "audio");

    copyAssetSet(m_DialoguePaths, "dialogue");
    copyAssetSet(m_PrefabPaths, "prefab");
    copyAssetSet(m_DataAssetPaths, "data asset");
    copyAssetSet(m_ModelPaths, "model");

    // Copy window icon if present
    std::string iconPath = (fs::path(m_ProjectDir) / "icon.png").string();
    if (fs::exists(iconPath)) {
        copyFileRelative(iconPath, "icon.png");
    }

    // Write game.manifest JSON (replaces the in-pak _build/manifest.json)
    if (!WriteLooseManifest(m_Config, outputDir)) {
        AddMessage(MessageSeverity::Error, "Failed to write game manifest");
        return false;
    }

    m_Result.filesPacked = filesCopied;
    m_Result.totalSizeBytes = totalBytes;
    m_Result.packedSizeBytes = totalBytes; // No compression for loose files

    AddMessage(MessageSeverity::Info, "Copied " + std::to_string(filesCopied) + " files to " + outputDir);
    return true;
}

bool BuildPipeline::WriteLooseManifest(const BuildConfig& config, const std::string& outputDir) {
    nlohmann::json manifest;
    manifest["windowTitle"] = config.windowTitle.empty() ? m_ProjectName : config.windowTitle;
    manifest["windowWidth"] = config.windowWidth;
    manifest["windowHeight"] = config.windowHeight;
    manifest["fullscreen"] = config.fullscreen;
    manifest["engineSplash"] = config.engineSplash;
    manifest["projectName"] = m_ProjectName;

    // Find start scene
    for (const auto& scene : m_Scenes) {
        if (scene.isStartScene) {
            manifest["startScene"] = scene.relativePath;
            break;
        }
    }

    // Frame settings
    nlohmann::json frameSettings;
    frameSettings["targetFrameRate"] = m_TargetFrameRate;
    frameSettings["vSync"] = m_VSync;
    frameSettings["backgroundBehavior"] = m_BackgroundBehavior;
    manifest["frameSettings"] = frameSettings;

    // Physics backend and project mode
    manifest["physicsBackend"] = m_PhysicsBackendType;
    manifest["projectMode"] = m_ProjectMode;

    // Startup flow (if the project authored one)
    if (!m_StartupFlowJson.empty()) {
        manifest["startupFlow"] = nlohmann::json::parse(m_StartupFlowJson);
    }

    std::string manifestPath = (fs::path(outputDir) / "game.manifest").string();
    try {
        std::ofstream file(manifestPath);
        if (!file.is_open()) {
            AddMessage(MessageSeverity::Error, "Cannot create game.manifest at: " + manifestPath);
            return false;
        }
        file << manifest.dump(2);
        file.close();
        return true;
    } catch (const std::exception& e) {
        AddMessage(MessageSeverity::Error, std::string("Error writing game.manifest: ") + e.what());
        return false;
    }
}

bool BuildPipeline::VerifyLooseBuild(const std::string& outputDir) {
    // Verify game.manifest exists
    std::string manifestPath = (fs::path(outputDir) / "game.manifest").string();
    if (!fs::exists(manifestPath)) {
        AddMessage(MessageSeverity::Error, "game.manifest missing from output directory");
        return false;
    }

    // Verify project file
    std::string projPath = (fs::path(outputDir) / "project.enjinproject").string();
    if (!fs::exists(projPath)) {
        AddMessage(MessageSeverity::Error, "Project file missing from output directory");
        return false;
    }

    // Verify all scenes
    for (const auto& scene : m_Scenes) {
        std::string scenePath = (fs::path(outputDir) / scene.relativePath).string();
        if (!fs::exists(scenePath)) {
            AddMessage(MessageSeverity::Error, "Scene missing from output: " + scene.relativePath);
            return false;
        }
    }

    AddMessage(MessageSeverity::Info, "Loose build verification passed");
    return true;
}

void BuildPipeline::ScanProjectDirectory() {
    if (m_ProjectDir.empty()) return;

    // File extension to asset set mapping
    struct ExtMapping {
        const char* ext;
        std::set<std::string>* target;
    };

    ExtMapping mappings[] = {
        { ".as",         &m_ScriptPaths },
        { ".wav",        &m_AudioPaths },
        { ".mp3",        &m_AudioPaths },
        { ".ogg",        &m_AudioPaths },
        { ".flac",       &m_AudioPaths },
        { ".enjdlg",     &m_DialoguePaths },
        { ".enjprefab",  &m_PrefabPaths },
        { ".enjdata",    &m_DataAssetPaths },
        { ".enjschema",  &m_DataAssetPaths },
        { ".csv",        &m_DataAssetPaths },     // F7: Localization files
        { ".gltf",       &m_ModelPaths },          // F17: 3D model files
        { ".glb",        &m_ModelPaths },
        { ".fbx",        &m_ModelPaths },
        { ".obj",        &m_ModelPaths },
        { ".dae",        &m_ModelPaths },
        { ".3ds",        &m_ModelPaths },
        { ".ply",        &m_ModelPaths },
        { ".vox",        &m_ModelPaths },
        { ".svg",        &m_TexturePaths },        // F23: SVG files
        { ".png",        &m_TexturePaths },        // Image textures
        { ".jpg",        &m_TexturePaths },
        { ".jpeg",       &m_TexturePaths },
        { ".bmp",        &m_TexturePaths },
        { ".tga",        &m_TexturePaths },
        { ".hdr",        &m_TexturePaths },
        { ".enjshader",  &m_DataAssetPaths },     // Graph system assets
        { ".enjaudiopkg", &m_DataAssetPaths },
        { ".enjparticle", &m_DataAssetPaths },
    };

    try {
        for (auto& entry : fs::recursive_directory_iterator(m_ProjectDir,
                 fs::directory_options::skip_permission_denied)) {
            if (!entry.is_regular_file()) continue;

            std::string ext = entry.path().extension().string();
            // Lowercase the extension for comparison
            for (auto& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

            for (const auto& mapping : mappings) {
                if (ext == mapping.ext) {
                    mapping.target->insert(entry.path().string());
                    break;
                }
            }
        }
    } catch (const std::exception& e) {
        AddMessage(MessageSeverity::Warning,
                   std::string("Error scanning project directory: ") + e.what());
    }

    AddMessage(MessageSeverity::Info,
               "Directory scan: " +
               std::to_string(m_ScriptPaths.size()) + " scripts, " +
               std::to_string(m_AudioPaths.size()) + " audio, " +
               std::to_string(m_DialoguePaths.size()) + " dialogues, " +
               std::to_string(m_PrefabPaths.size()) + " prefabs, " +
               std::to_string(m_DataAssetPaths.size()) + " data assets, " +
               std::to_string(m_ModelPaths.size()) + " models");
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
