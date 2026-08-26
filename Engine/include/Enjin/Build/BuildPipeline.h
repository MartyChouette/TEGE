#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Build/BuildReport.h"
#include "Enjin/Build/AssetPacker.h"
#include <string>
#include <vector>
#include <set>
#include <functional>

namespace Enjin::Build {

// Orchestrates the full build: scan -> validate -> collect -> pack -> verify
class ENJIN_API BuildPipeline {
public:
    BuildPipeline() = default;

    BuildResult Execute(const BuildConfig& config);

    // Progress callback: (phase description, progress 0.0-1.0)
    using ProgressCallback = std::function<void(const std::string&, float)>;
    void SetProgressCallback(ProgressCallback cb) { m_ProgressCallback = std::move(cb); }

private:
    // Phase 1: Scan project manifest, list all scenes
    bool ScanProject(const std::string& projectPath);
    // Phase 2: Parse each scene JSON, collect asset refs, validate existence
    bool ValidateAssets();
    // Phase 3: Pack everything into .enjpak
    bool PackAssets(const std::string& outputDir, const std::string& key);
    // Phase 3 (alt): Copy loose files to output directory (no packing)
    bool CopyLooseFiles(const std::string& outputDir);
    // Phase 3.5 (pak mode): scripts + enjin_api + assets must also ship loose —
    // the script engine reads from the filesystem (pak script loading is
    // unimplemented) and script-referenced assets are invisible to scene scans
    void EmitLooseRuntimeFiles(const std::string& outputDir);
    // Phase 4: Copy player executable to output
    bool CopyPlayer(const std::string& outputDir);
    // Phase 5: Write build manifest (window title, resolution) into pack
    bool WriteBuildManifest(const BuildConfig& config, AssetPacker& packer);
    // Phase 5 (alt): Write game.manifest JSON for loose files mode
    bool WriteLooseManifest(const BuildConfig& config, const std::string& outputDir);
    // Phase 6: Verify — read back .enjpak, check CRC32s
    bool VerifyBuild(const std::string& pakPath, const std::string& key);
    // Phase 6 (alt): Verify loose files exist in output
    bool VerifyLooseBuild(const std::string& outputDir);

    // Scan project directory for script, audio, dialogue, prefab, and data asset files
    void ScanProjectDirectory();
    void AddMessage(MessageSeverity severity, const std::string& text, const std::string& file = "");
    void ReportProgress(const std::string& phase, float progress);

    BuildResult m_Result;
    BuildConfig m_Config;
    std::string m_ProjectDir;
    std::string m_ProjectName;

    // Collected scene and asset paths
    struct SceneInfo {
        std::string name;
        std::string relativePath;  // relative to project root
        std::string absolutePath;
        i32 buildIndex = -1;
        bool isStartScene = false;
    };
    std::vector<SceneInfo> m_Scenes;
    std::set<std::string> m_TexturePaths;   // absolute paths on disk
    std::set<std::string> m_ModelPaths;
    std::set<std::string> m_ScriptPaths;    // .as AngelScript files
    std::set<std::string> m_AudioPaths;     // .wav/.mp3/.ogg/.flac audio files
    std::set<std::string> m_DialoguePaths;  // .enjdlg dialogue assets
    std::set<std::string> m_PrefabPaths;    // .enjprefab prefab files
    std::set<std::string> m_DataAssetPaths; // .enjdata/.enjschema data assets

    // Game frame settings (read from project, written to manifest)
    u32 m_TargetFrameRate = 60;      // 0 = uncapped
    bool m_VSync = true;
    u32 m_BackgroundBehavior = 1;    // 0 = run normally, 1 = reduce to 30, 2 = pause

    // Physics backend selection (read from project, written to manifest)
    u32 m_PhysicsBackendType = 0;    // 0 = Auto, 1 = Jolt, 2 = Box2D
    u32 m_ProjectMode = 1;           // 0 = 2D, 1 = 3D, 2 = Mixed

    // Authorable startup flow, carried verbatim from the project to the game
    // manifest as a JSON string (empty when the project has none).
    std::string m_StartupFlowJson;

    ProgressCallback m_ProgressCallback;
};

} // namespace Enjin::Build
