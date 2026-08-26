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
    bool zipOutput = true;              // Package output as .zip for itch.io / Newgrounds upload
    std::string customCSS;
    std::string faviconPath;
    std::string splashImagePath;
    std::string outputDir;
};

// HTML5 export result
struct HTML5ExportResult {
    bool success = false;
    std::string outputPath;
    std::string zipPath;        // Path to .zip file (empty if zip not requested)
    std::string embedCode;
    std::vector<std::string> files;
    std::string error;
};

// Web portal targets. Presets apply a portal's recommended canvas dimensions and
// shell options so an export drops straight into that portal without fiddling.
enum class WebPortal {
    Generic = 0,   // plain shell, author's own dimensions
    Itch,          // itch.io — 1280x720, fullscreen button, zip for upload
    Newgrounds,    // Newgrounds — 800x600, iframe embed snippet
    Poki,          // Poki — 1280x720 fullscreen, no preloader chrome
    CrazyGames,    // CrazyGames — 1280x720 fullscreen
    GameJolt,      // GameJolt — 800x600, zip for upload
    Count
};

// itch.io publish settings (butler push target).
struct ItchPublishConfig {
    std::string user;              // itch.io account name (the part before the slash)
    std::string game;              // game page slug (the part after the slash)
    std::string channel = "html5"; // butler channel (html5 = playable-in-browser)
    std::string userVersion;       // optional --userversion tag (empty = butler auto)
};

// Generates browser-ready HTML/JS/CSS templates for WASM builds
class ENJIN_API HTML5Exporter {
public:
    // Export HTML5 web shell (generates template files + optionally invokes Emscripten build)
    static HTML5ExportResult Export(const HTML5ExportConfig& config,
                                    const BuildConfig& buildConfig);

    // Invoke Emscripten to compile the engine into WASM.
    // Requires emcc to be on PATH (via emsdk activation).
    // outputDir: where to place game.js + game.wasm + game.data
    // enjpakPath: path to the packed .enjpak to preload
    // Returns true if the build succeeded.
    static bool InvokeEmscriptenBuild(const std::string& outputDir,
                                       const std::string& enjpakPath);

    // Apply a web portal's recommended defaults (canvas size + shell options) to
    // an export config. Only touches portal-driven fields; leaves title/paths.
    static void ApplyPortalPreset(HTML5ExportConfig& config, WebPortal portal);

    // Human-readable portal name (for UI dropdowns / logs).
    static const char* PortalName(WebPortal portal);

    // Publish an exported build to itch.io via the butler CLI. pathToPush is the
    // export directory (or its .zip). Requires butler on PATH and an authenticated
    // butler login (`butler login`). itch identifiers are validated to a safe
    // charset before use. Returns true on a zero exit; outError carries the reason
    // on failure. This uploads to an external service — caller must confirm intent.
    static bool PublishToItch(const std::string& pathToPush,
                              const ItchPublishConfig& itch,
                              std::string& outError);

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
