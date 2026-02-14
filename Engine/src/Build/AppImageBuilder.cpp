#include "Enjin/Build/AppImageBuilder.h"
#include "Enjin/Logging/Log.h"

#include <filesystem>
#include <fstream>

#ifdef ENJIN_PLATFORM_LINUX
#include "Enjin/Platform/LinuxPlatform.h"
#include <sys/stat.h>
#endif

namespace Enjin::Build {

void AppImageBuilder::ReportProgress(const std::string& phase, f32 progress) {
    if (m_ProgressCallback) {
        m_ProgressCallback(phase, progress);
    }
}

bool AppImageBuilder::IsAppImageToolAvailable() {
#ifdef ENJIN_PLATFORM_LINUX
    return Platform::IsCommandAvailable("appimagetool");
#else
    return false;
#endif
}

std::vector<std::string> AppImageBuilder::ValidateConfig(const AppImageConfig& config) {
    std::vector<std::string> errors;

    if (config.appName.empty()) {
        errors.push_back("App name is required");
    }
    // Validate app name contains only safe chars
    for (char c : config.appName) {
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '_' || c == '-')) {
            errors.push_back("App name contains invalid characters (use alphanumeric, dash, underscore)");
            break;
        }
    }

    if (config.executablePath.empty()) {
        errors.push_back("Executable path is required");
    } else if (!std::filesystem::exists(config.executablePath)) {
        errors.push_back("Executable not found: " + config.executablePath);
    }

    if (!config.iconPath.empty() && !std::filesystem::exists(config.iconPath)) {
        errors.push_back("Icon file not found: " + config.iconPath);
    }

    if (!config.gamePakPath.empty() && !std::filesystem::exists(config.gamePakPath)) {
        errors.push_back("Game pack not found: " + config.gamePakPath);
    }

    return errors;
}

AppImageResult AppImageBuilder::Build(const AppImageConfig& config, const std::string& outputDir) {
    AppImageResult result;

    // Validate configuration
    auto errors = ValidateConfig(config);
    if (!errors.empty()) {
        result.error = errors[0];
        for (size_t i = 1; i < errors.size(); ++i) {
            result.warnings.push_back(errors[i]);
        }
        ENJIN_LOG_ERROR(Build, "AppImage config validation failed: %s", result.error.c_str());
        return result;
    }

    ReportProgress("Preparing AppDir", 0.0f);

    // Create output directory
    std::error_code ec;
    std::filesystem::create_directories(outputDir, ec);
    if (ec) {
        result.error = "Failed to create output directory: " + ec.message();
        return result;
    }

    // Create AppDir staging directory
    std::string appDir = outputDir + "/" + config.appName + ".AppDir";
    result.appDirPath = appDir;

    ReportProgress("Creating AppDir structure", 0.1f);

    if (!CreateAppDir(config, appDir)) {
        result.error = "Failed to create AppDir structure";
        return result;
    }

    ReportProgress("Generating desktop file", 0.2f);

    if (!GenerateDesktopFile(config, appDir)) {
        result.error = "Failed to generate .desktop file";
        return result;
    }

    ReportProgress("Generating AppRun script", 0.3f);

    if (!GenerateAppRunScript(config, appDir)) {
        result.error = "Failed to generate AppRun script";
        return result;
    }

    ReportProgress("Copying game files", 0.4f);

    if (!CopyGameFiles(config, appDir)) {
        result.error = "Failed to copy game files";
        return result;
    }

    ReportProgress("Setting up icon", 0.6f);

    if (!CopyIcon(config, appDir)) {
        // Icon failure is a warning, not an error
        result.warnings.push_back("Failed to copy icon; AppImage will use default");
    }

    // Generate the .AppImage file
    std::string outputPath = outputDir + "/" + config.appName + "-" +
                             config.version + "-" + config.architecture + ".AppImage";
    result.outputPath = outputPath;

    ReportProgress("Building AppImage", 0.7f);

    if (!RunAppImageTool(config, appDir, outputPath)) {
        // If appimagetool is not available, report but still succeed with AppDir
        result.warnings.push_back("appimagetool not available; AppDir created but AppImage not generated. "
                                  "Install appimagetool to create the final AppImage file.");
        result.success = true;
        ENJIN_LOG_WARN(Build, "AppDir created at %s but appimagetool not available", appDir.c_str());
        ReportProgress("Complete (AppDir only)", 1.0f);
        return result;
    }

    // Get final file size
    if (std::filesystem::exists(outputPath)) {
        result.sizeBytes = static_cast<u64>(std::filesystem::file_size(outputPath));
    }

    result.success = true;
    ReportProgress("Complete", 1.0f);
    ENJIN_LOG_INFO(Build, "AppImage built: %s (%.2f MB)",
                   outputPath.c_str(), static_cast<f64>(result.sizeBytes) / (1024.0 * 1024.0));

    return result;
}

bool AppImageBuilder::CreateAppDir(const AppImageConfig& config, const std::string& appDir) {
    std::error_code ec;

    // Remove existing AppDir
    if (std::filesystem::exists(appDir)) {
        std::filesystem::remove_all(appDir, ec);
        if (ec) {
            ENJIN_LOG_ERROR(Build, "Failed to remove existing AppDir: %s", ec.message().c_str());
            return false;
        }
    }

    // Create directory structure following AppImage spec
    // usr/bin — executables
    // usr/lib — shared libraries
    // usr/share/applications — .desktop file
    // usr/share/icons/hicolor/256x256/apps — icon
    // usr/share/enjin — game data
    std::vector<std::string> dirs = {
        appDir + "/usr/bin",
        appDir + "/usr/lib",
        appDir + "/usr/share/applications",
        appDir + "/usr/share/icons/hicolor/256x256/apps",
        appDir + "/usr/share/enjin"
    };

    for (const auto& dir : dirs) {
        std::filesystem::create_directories(dir, ec);
        if (ec) {
            ENJIN_LOG_ERROR(Build, "Failed to create directory: %s (%s)", dir.c_str(), ec.message().c_str());
            return false;
        }
    }

    (void)config;
    return true;
}

bool AppImageBuilder::GenerateDesktopFile(const AppImageConfig& config, const std::string& appDir) {
    std::string desktopPath = appDir + "/usr/share/applications/" + config.appName + ".desktop";
    std::ofstream file(desktopPath);
    if (!file.is_open()) {
        ENJIN_LOG_ERROR(Build, "Failed to create .desktop file: %s", desktopPath.c_str());
        return false;
    }

    file << "[Desktop Entry]\n";
    file << "Type=Application\n";
    file << "Name=" << config.appName << "\n";
    if (!config.genericName.empty()) {
        file << "GenericName=" << config.genericName << "\n";
    }
    if (!config.comment.empty()) {
        file << "Comment=" << config.comment << "\n";
    } else if (!config.description.empty()) {
        file << "Comment=" << config.description << "\n";
    }
    file << "Exec=" << config.appName << "\n";
    file << "Icon=" << config.appName << "\n";
    file << "Terminal=false\n";
    file << "Categories=" << config.categories << "\n";
    file << "StartupWMClass=" << config.appName << "\n";

    file.close();

    // Also create a symlink at the AppDir root (required by AppImage spec)
    std::error_code ec;
    std::string rootDesktop = appDir + "/" + config.appName + ".desktop";
    std::filesystem::copy_file(desktopPath, rootDesktop, ec);
    if (ec) {
        ENJIN_LOG_WARN(Build, "Failed to copy .desktop to AppDir root: %s", ec.message().c_str());
    }

    return true;
}

bool AppImageBuilder::GenerateAppRunScript(const AppImageConfig& config, const std::string& appDir) {
    std::string appRunPath = appDir + "/AppRun";
    std::ofstream file(appRunPath);
    if (!file.is_open()) {
        ENJIN_LOG_ERROR(Build, "Failed to create AppRun script: %s", appRunPath.c_str());
        return false;
    }

    file << "#!/bin/bash\n";
    file << "# AppRun script for " << config.appName << "\n";
    file << "# Generated by Enjin Engine build pipeline\n\n";

    file << "SELF=$(readlink -f \"$0\")\n";
    file << "HERE=${SELF%/*}\n\n";

    // Set up library path to include bundled libs
    file << "export LD_LIBRARY_PATH=\"${HERE}/usr/lib:${LD_LIBRARY_PATH}\"\n";

    // Set working directory to the game data directory so assets resolve
    file << "cd \"${HERE}/usr/share/enjin\"\n\n";

    // Execute the game
    file << "exec \"${HERE}/usr/bin/" << config.appName << "\" \"$@\"\n";

    file.close();

    // Make executable
#ifdef ENJIN_PLATFORM_LINUX
    chmod(appRunPath.c_str(), 0755);
#else
    (void)config;
#endif

    return true;
}

bool AppImageBuilder::CopyGameFiles(const AppImageConfig& config, const std::string& appDir) {
    std::error_code ec;

    // Copy executable
    std::string destExe = appDir + "/usr/bin/" + config.appName;
    std::filesystem::copy_file(config.executablePath, destExe,
                                std::filesystem::copy_options::overwrite_existing, ec);
    if (ec) {
        ENJIN_LOG_ERROR(Build, "Failed to copy executable: %s", ec.message().c_str());
        return false;
    }

    // Make executable
#ifdef ENJIN_PLATFORM_LINUX
    chmod(destExe.c_str(), 0755);
#endif

    // Copy game pack
    if (!config.gamePakPath.empty() && std::filesystem::exists(config.gamePakPath)) {
        std::string destPak = appDir + "/usr/share/enjin/game.enjpak";
        std::filesystem::copy_file(config.gamePakPath, destPak,
                                    std::filesystem::copy_options::overwrite_existing, ec);
        if (ec) {
            ENJIN_LOG_ERROR(Build, "Failed to copy game pack: %s", ec.message().c_str());
            return false;
        }
    }

    // Copy extra files
    for (const auto& filePath : config.extraFiles) {
        if (!std::filesystem::exists(filePath)) {
            ENJIN_LOG_WARN(Build, "Extra file not found: %s", filePath.c_str());
            continue;
        }
        std::string filename = std::filesystem::path(filePath).filename().string();
        std::string dest = appDir + "/usr/share/enjin/" + filename;
        std::filesystem::copy_file(filePath, dest,
                                    std::filesystem::copy_options::overwrite_existing, ec);
        if (ec) {
            ENJIN_LOG_WARN(Build, "Failed to copy extra file %s: %s", filePath.c_str(), ec.message().c_str());
        }
    }

    // Copy extra directories
    for (const auto& dirPath : config.extraDirectories) {
        if (!std::filesystem::is_directory(dirPath)) {
            ENJIN_LOG_WARN(Build, "Extra directory not found: %s", dirPath.c_str());
            continue;
        }
        std::string dirName = std::filesystem::path(dirPath).filename().string();
        std::string dest = appDir + "/usr/share/enjin/" + dirName;
        std::filesystem::copy(dirPath, dest,
                               std::filesystem::copy_options::recursive |
                               std::filesystem::copy_options::overwrite_existing, ec);
        if (ec) {
            ENJIN_LOG_WARN(Build, "Failed to copy extra directory %s: %s", dirPath.c_str(), ec.message().c_str());
        }
    }

    return true;
}

bool AppImageBuilder::CopyIcon(const AppImageConfig& config, const std::string& appDir) {
    if (config.iconPath.empty() || !std::filesystem::exists(config.iconPath)) {
        return false;
    }

    std::error_code ec;

    // Copy to hicolor icon directory
    std::string iconDest = appDir + "/usr/share/icons/hicolor/256x256/apps/" + config.appName + ".png";
    std::filesystem::copy_file(config.iconPath, iconDest,
                                std::filesystem::copy_options::overwrite_existing, ec);
    if (ec) {
        ENJIN_LOG_WARN(Build, "Failed to copy icon to hicolor: %s", ec.message().c_str());
        return false;
    }

    // Also copy to AppDir root (required by AppImage spec)
    std::string rootIcon = appDir + "/" + config.appName + ".png";
    std::filesystem::copy_file(config.iconPath, rootIcon,
                                std::filesystem::copy_options::overwrite_existing, ec);
    if (ec) {
        ENJIN_LOG_WARN(Build, "Failed to copy icon to AppDir root: %s", ec.message().c_str());
    }

    // Create .DirIcon symlink (some AppImage runtimes look for this)
    std::string dirIcon = appDir + "/.DirIcon";
    if (!std::filesystem::exists(dirIcon)) {
        std::filesystem::copy_file(config.iconPath, dirIcon,
                                    std::filesystem::copy_options::overwrite_existing, ec);
    }

    return true;
}

bool AppImageBuilder::RunAppImageTool(const AppImageConfig& config, const std::string& appDir,
                                       const std::string& outputPath) {
#ifdef ENJIN_PLATFORM_LINUX
    std::string tool = config.appImageToolPath;
    if (tool.empty()) {
        // Search PATH
        if (!Platform::IsCommandAvailable("appimagetool")) {
            ENJIN_LOG_WARN(Build, "appimagetool not found on PATH");
            return false;
        }
        tool = "appimagetool";
    }

    // Set ARCH environment variable for appimagetool
    setenv("ARCH", config.architecture.c_str(), 1);

    auto result = Platform::LaunchProcess(tool, {appDir, outputPath}, 120000);
    if (!result.success) {
        ENJIN_LOG_ERROR(Build, "appimagetool failed (exit %d): %s",
                        result.exitCode, result.stderrOutput.c_str());
        return false;
    }

    return std::filesystem::exists(outputPath);
#else
    (void)config;
    (void)appDir;
    (void)outputPath;
    ENJIN_LOG_WARN(Build, "AppImage generation is only supported on Linux");
    return false;
#endif
}

} // namespace Enjin::Build
