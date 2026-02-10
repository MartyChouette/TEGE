#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Build/BuildReport.h"
#include <string>
#include <vector>

namespace Enjin {
namespace Build {

// HTML5 export configuration
struct HTML5ExportConfig {
    std::string title = "My Game";
    u32 width = 550;
    u32 height = 400;
    std::string backgroundColor = "#000000";
    bool showPreloader = true;
    bool showFullscreenButton = true;
    bool generateEmbedCode = true;
    std::string customCSS;
    std::string faviconPath;
    std::string splashImagePath;
    std::string outputDir;
};

// HTML5 export result
struct HTML5ExportResult {
    bool success = false;
    std::string outputPath;
    std::string embedCode;
    std::vector<std::string> files;
    std::string error;
};

// Generates browser-ready HTML/JS/CSS templates for WASM builds
class ENJIN_API HTML5Exporter {
public:
    // Export HTML5 web shell
    static HTML5ExportResult Export(const HTML5ExportConfig& config,
                                    const BuildConfig& buildConfig);

private:
    // Generate individual files
    static std::string GenerateHTML(const HTML5ExportConfig& config,
                                    const BuildConfig& buildConfig);
    static std::string GeneratePreloaderJS(const HTML5ExportConfig& config);
    static std::string GenerateStyleCSS(const HTML5ExportConfig& config);
    static std::string GenerateEmbedSnippet(const HTML5ExportConfig& config);
};

} // namespace Build
} // namespace Enjin
