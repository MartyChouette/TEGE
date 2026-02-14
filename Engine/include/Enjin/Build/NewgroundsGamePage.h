#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include <string>
#include <vector>

namespace Enjin {
namespace Build {

// Configuration for generating a Newgrounds-style game page
struct GamePageConfig {
    // Game metadata
    std::string title = "My Game";
    std::string author = "Anonymous";
    std::string description;
    std::string version = "1.0";
    std::vector<std::string> tags;
    std::string thumbnailPath;     // 315x250 game thumbnail
    std::string bannerPath;        // 700x90 banner (optional)

    // Newgrounds integration
    std::string ngAppId;           // Newgrounds app ID
    std::string ngEncryptionKey;   // AES key for API calls
    bool showMedals = true;        // Medal sidebar
    bool showScoreboard = true;    // Leaderboard sidebar
    bool showAuthorLink = true;    // Link to author's NG profile

    // Page layout
    u32 canvasWidth = 800;
    u32 canvasHeight = 600;
    std::string backgroundColor = "#1a1a2e";  // Dark theme
    std::string accentColor = "#e94560";       // Newgrounds-style red
    std::string textColor = "#eaeaea";

    // Features
    bool showPreloader = true;
    bool showFullscreenButton = true;
    bool showShareButtons = false;
    bool responsiveScaling = true;
    bool showControls = true;      // "Controls" section below game
    std::string controlsText;      // "WASD to move, Space to jump"

    // Export
    std::string outputDir;
    bool generateEmbedCode = true;
    bool minifyOutput = false;
};

// Result of game page generation
struct GamePageResult {
    bool success = false;
    std::string indexPath;         // Path to index.html
    std::string embedCode;         // iframe embed snippet
    std::string ngEmbedCode;       // Newgrounds-specific embed
    std::vector<std::string> files; // All generated files
    std::string error;
};

// Generates a full Newgrounds-style game page with medals, scoreboards,
// dark theme, responsive layout, and social sharing support
class ENJIN_API NewgroundsGamePage {
public:
    // Generate a full game page (writes files to outputDir)
    static GamePageResult Generate(const GamePageConfig& config);

    // Individual page components (return generated source strings)
    static std::string GenerateHTML(const GamePageConfig& config);
    static std::string GenerateCSS(const GamePageConfig& config);
    static std::string GenerateJS(const GamePageConfig& config);
    static std::string GenerateEmbedCodes(const GamePageConfig& config);

    // ImGui editor panels for build dialog
    static void DrawConfigPanel(GamePageConfig& config);
    static void DrawPreview(const GamePageConfig& config);

private:
    // HTML helpers
    static std::string EscapeHTML(const std::string& text);
    static std::string GenerateMetaTags(const GamePageConfig& config);
    static std::string GenerateGameSection(const GamePageConfig& config);
    static std::string GenerateSidebar(const GamePageConfig& config);
    static std::string GenerateFooter(const GamePageConfig& config);

    // Newgrounds JS helpers
    static std::string GenerateNGInit(const GamePageConfig& config);
    static std::string GenerateMedalJS(const GamePageConfig& config);
    static std::string GenerateScoreboardJS(const GamePageConfig& config);
    static std::string GenerateToastNotificationJS();
};

} // namespace Build
} // namespace Enjin
