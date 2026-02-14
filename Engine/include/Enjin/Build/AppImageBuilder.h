#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include <string>
#include <vector>
#include <functional>

namespace Enjin::Build {

// Configuration for building a Linux AppImage
struct AppImageConfig {
    std::string appName = "EnjinGame";
    std::string version = "1.0.0";
    std::string executablePath;         // Path to the built game executable
    std::string iconPath;               // Path to PNG icon (256x256 recommended)
    std::string description = "A game built with Enjin Engine";
    std::string gamePakPath;            // Path to game.enjpak

    // .desktop file fields
    std::string categories = "Game;";   // FreeDesktop categories (semicolon-separated)
    std::string genericName = "Game";
    std::string comment;                // Short description for tooltip

    // Additional files to include in the AppImage
    std::vector<std::string> extraFiles;        // Absolute paths to files
    std::vector<std::string> extraDirectories;  // Absolute paths to directories

    // AppImage tool path (empty = search PATH for appimagetool)
    std::string appImageToolPath;

    // Architecture (x86_64, aarch64)
    std::string architecture = "x86_64";
};

// Result of the AppImage build process
struct AppImageResult {
    bool success = false;
    std::string outputPath;             // Path to the generated .AppImage file
    std::string appDirPath;             // Path to the AppDir staging directory
    std::string error;
    std::vector<std::string> warnings;
    u64 sizeBytes = 0;                  // Size of the final AppImage
};

// Builds Linux AppImage packages from game builds
class ENJIN_API AppImageBuilder {
public:
    AppImageBuilder() = default;

    // Build an AppImage from the given configuration.
    // outputDir is the directory where the .AppImage file will be placed.
    AppImageResult Build(const AppImageConfig& config, const std::string& outputDir);

    // Progress callback: (phase description, progress 0.0-1.0)
    using ProgressCallback = std::function<void(const std::string&, f32)>;
    void SetProgressCallback(ProgressCallback cb) { m_ProgressCallback = std::move(cb); }

    // Check if appimagetool is available on the system
    static bool IsAppImageToolAvailable();

    // Validate an AppImage config before building
    static std::vector<std::string> ValidateConfig(const AppImageConfig& config);

private:
    // Create the AppDir directory structure
    bool CreateAppDir(const AppImageConfig& config, const std::string& appDir);

    // Generate the .desktop file
    bool GenerateDesktopFile(const AppImageConfig& config, const std::string& appDir);

    // Generate the AppRun script
    bool GenerateAppRunScript(const AppImageConfig& config, const std::string& appDir);

    // Copy the game executable and assets into AppDir
    bool CopyGameFiles(const AppImageConfig& config, const std::string& appDir);

    // Copy the icon into the correct locations
    bool CopyIcon(const AppImageConfig& config, const std::string& appDir);

    // Run appimagetool to create the final .AppImage
    bool RunAppImageTool(const AppImageConfig& config, const std::string& appDir,
                         const std::string& outputPath);

    void ReportProgress(const std::string& phase, f32 progress);

    ProgressCallback m_ProgressCallback;
};

} // namespace Enjin::Build
