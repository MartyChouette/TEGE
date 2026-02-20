#include "Enjin/Build/NewgroundsGamePage.h"
#include "Enjin/Logging/Log.h"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>

namespace Enjin {
namespace Build {

// ============================================================================
// HTML Escaping (XSS protection)
// ============================================================================

std::string NewgroundsGamePage::EscapeHTML(const std::string& text) {
    std::string out;
    out.reserve(text.size());
    for (char c : text) {
        switch (c) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += "&quot;"; break;
            case '\'': out += "&#39;"; break;
            default: out += c;
        }
    }
    return out;
}

// Escape a string for use inside a JavaScript string literal (single-quoted)
static std::string EscapeJS(const std::string& text) {
    std::string out;
    out.reserve(text.size());
    for (char c : text) {
        switch (c) {
            case '\\': out += "\\\\"; break;
            case '\'': out += "\\'"; break;
            case '"': out += "\\\""; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '<': out += "\\x3c"; break;  // prevent </script> injection
            case '>': out += "\\x3e"; break;
            default: out += c;
        }
    }
    return out;
}

// Sanitize a CSS color value — strip anything that is not hex digits, #, or
// a limited set of characters used in rgb()/hsl() notation.
static std::string SanitizeCSSColor(const std::string& color) {
    std::string out;
    out.reserve(color.size());
    for (char c : color) {
        if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
            (c >= 'A' && c <= 'F') || c == '#' || c == '(' || c == ')' ||
            c == ',' || c == ' ' || c == '%' || c == '.' ||
            c == 'r' || c == 'g' || c == 'b' || c == 'a' ||
            c == 'h' || c == 's' || c == 'l') {
            out += c;
        }
    }
    return out;
}

// ============================================================================
// Generate (top-level export)
// ============================================================================

GamePageResult NewgroundsGamePage::Generate(const GamePageConfig& config) {
    GamePageResult result;

    if (config.outputDir.empty()) {
        result.error = "Output directory not specified";
        return result;
    }

    // Create output directory
    std::error_code ec;
    std::filesystem::create_directories(config.outputDir, ec);
    if (ec) {
        result.error = std::string("Failed to create output directory: ") + ec.message();
        return result;
    }

    // Generate sources
    std::string html = GenerateHTML(config);
    std::string css = GenerateCSS(config);
    std::string js = GenerateJS(config);

    // Write index.html
    std::string htmlPath = config.outputDir + "/index.html";
    {
        std::ofstream f(htmlPath);
        if (!f.is_open()) {
            result.error = "Failed to create index.html";
            return result;
        }
        f << html;
    }
    result.files.push_back("index.html");
    result.indexPath = htmlPath;

    // Write gamepage.css
    {
        std::string cssPath = config.outputDir + "/gamepage.css";
        std::ofstream f(cssPath);
        if (!f.is_open()) {
            result.error = "Failed to create gamepage.css";
            return result;
        }
        f << css;
        result.files.push_back("gamepage.css");
    }

    // Write gamepage.js
    {
        std::string jsPath = config.outputDir + "/gamepage.js";
        std::ofstream f(jsPath);
        if (!f.is_open()) {
            result.error = "Failed to create gamepage.js";
            return result;
        }
        f << js;
        result.files.push_back("gamepage.js");
    }

    // Copy thumbnail if specified
    if (!config.thumbnailPath.empty() && std::filesystem::exists(config.thumbnailPath)) {
        std::string ext = std::filesystem::path(config.thumbnailPath).extension().string();
        std::string dest = config.outputDir + "/thumbnail" + ext;
        std::filesystem::copy_file(config.thumbnailPath, dest,
                                    std::filesystem::copy_options::overwrite_existing, ec);
        if (!ec) {
            result.files.push_back("thumbnail" + ext);
        }
    }

    // Copy banner if specified
    if (!config.bannerPath.empty() && std::filesystem::exists(config.bannerPath)) {
        std::string ext = std::filesystem::path(config.bannerPath).extension().string();
        std::string dest = config.outputDir + "/banner" + ext;
        std::filesystem::copy_file(config.bannerPath, dest,
                                    std::filesystem::copy_options::overwrite_existing, ec);
        if (!ec) {
            result.files.push_back("banner" + ext);
        }
    }

    // Generate embed codes
    if (config.generateEmbedCode) {
        std::string embedCodes = GenerateEmbedCodes(config);
        // Split into standard and NG-specific (separated by double newline)
        auto sep = embedCodes.find("\n\n<!-- Newgrounds");
        if (sep != std::string::npos) {
            result.embedCode = embedCodes.substr(0, sep);
            result.ngEmbedCode = embedCodes.substr(sep + 2); // skip the double newline
        } else {
            result.embedCode = embedCodes;
            result.ngEmbedCode = embedCodes;
        }
    }

    result.success = true;

    ENJIN_LOG_INFO(Build, "Newgrounds game page exported: %zu files to %s",
                   result.files.size(), config.outputDir.c_str());
    return result;
}

// ============================================================================
// Meta Tags
// ============================================================================

std::string NewgroundsGamePage::GenerateMetaTags(const GamePageConfig& config) {
    std::string safeTitle = EscapeHTML(config.title);
    std::string safeAuthor = EscapeHTML(config.author);
    std::string safeDesc = EscapeHTML(config.description);

    std::ostringstream meta;

    meta << "  <meta charset=\"utf-8\">\n"
         << "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n"
         << "  <title>" << safeTitle << " by " << safeAuthor << "</title>\n"
         << "  <meta name=\"description\" content=\"" << safeDesc << "\">\n"
         << "  <meta name=\"author\" content=\"" << safeAuthor << "\">\n";

    // Open Graph tags for social sharing
    meta << "  <meta property=\"og:title\" content=\"" << safeTitle << "\">\n"
         << "  <meta property=\"og:type\" content=\"game\">\n"
         << "  <meta property=\"og:description\" content=\"" << safeDesc << "\">\n";

    if (!config.thumbnailPath.empty()) {
        std::string ext = std::filesystem::path(config.thumbnailPath).extension().string();
        meta << "  <meta property=\"og:image\" content=\"thumbnail" << EscapeHTML(ext) << "\">\n";
    }

    // Twitter card
    meta << "  <meta name=\"twitter:card\" content=\"summary_large_image\">\n"
         << "  <meta name=\"twitter:title\" content=\"" << safeTitle << "\">\n"
         << "  <meta name=\"twitter:description\" content=\"" << safeDesc << "\">\n";

    // Keywords from tags
    if (!config.tags.empty()) {
        meta << "  <meta name=\"keywords\" content=\"";
        for (usize i = 0; i < config.tags.size(); ++i) {
            if (i > 0) meta << ", ";
            meta << EscapeHTML(config.tags[i]);
        }
        meta << "\">\n";
    }

    return meta.str();
}

// ============================================================================
// Game Section (canvas area + preloader)
// ============================================================================

std::string NewgroundsGamePage::GenerateGameSection(const GamePageConfig& config) {
    u32 safeWidth = std::clamp(config.canvasWidth, 1u, 7680u);
    u32 safeHeight = std::clamp(config.canvasHeight, 1u, 4320u);
    std::string safeTitle = EscapeHTML(config.title);
    std::string safeAuthor = EscapeHTML(config.author);
    std::string safeVersion = EscapeHTML(config.version);

    std::ostringstream s;

    // Header
    s << "    <header class=\"game-header\">\n"
      << "      <h1 class=\"game-title\">" << safeTitle << "</h1>\n"
      << "      <div class=\"game-meta\">\n"
      << "        <span class=\"game-author\">by ";
    if (config.showAuthorLink) {
        s << "<a href=\"https://www.newgrounds.com\" target=\"_blank\" rel=\"noopener\">"
          << safeAuthor << "</a>";
    } else {
        s << safeAuthor;
    }
    s << "</span>\n"
      << "        <span class=\"game-version\">v" << safeVersion << "</span>\n"
      << "      </div>\n"
      << "    </header>\n\n";

    // Canvas wrapper
    s << "    <div class=\"game-canvas-wrapper\">\n"
      << "      <canvas id=\"game-canvas\" width=\"" << safeWidth
      << "\" height=\"" << safeHeight << "\" tabindex=\"0\"></canvas>\n";

    // Preloader overlay
    if (config.showPreloader) {
        s << "      <div id=\"preloader\" class=\"preloader-overlay\">\n";
        if (!config.thumbnailPath.empty()) {
            std::string ext = std::filesystem::path(config.thumbnailPath).extension().string();
            s << "        <img class=\"preloader-splash\" src=\"thumbnail"
              << EscapeHTML(ext) << "\" alt=\"" << safeTitle << "\">\n";
        } else {
            s << "        <div class=\"preloader-title\">" << safeTitle << "</div>\n";
        }
        s << "        <div class=\"preloader-progress-container\">\n"
          << "          <div id=\"progress-bar\" class=\"preloader-progress-bar\"></div>\n"
          << "        </div>\n"
          << "        <div id=\"progress-text\" class=\"preloader-text\">Loading...</div>\n"
          << "        <div id=\"click-to-play\" class=\"preloader-play-btn\" style=\"display:none;\">Click to Play</div>\n"
          << "      </div>\n";
    }

    // Fullscreen button
    if (config.showFullscreenButton) {
        s << "      <button id=\"fullscreen-btn\" class=\"fullscreen-btn\" title=\"Toggle Fullscreen\">\n"
          << "        <svg width=\"18\" height=\"18\" viewBox=\"0 0 20 20\" fill=\"currentColor\">\n"
          << "          <path d=\"M3 3h5v2H5v3H3V3zm9 0h5v5h-2V5h-3V3zM3 12h2v3h3v2H3v-5zm12 3h-3v2h5v-5h-2v3z\"/>\n"
          << "        </svg>\n"
          << "      </button>\n";
    }

    s << "    </div>\n\n";

    // Controls section
    if (config.showControls && !config.controlsText.empty()) {
        s << "    <div class=\"game-controls\">\n"
          << "      <h3>Controls</h3>\n"
          << "      <p>" << EscapeHTML(config.controlsText) << "</p>\n"
          << "    </div>\n\n";
    }

    // Description
    if (!config.description.empty()) {
        s << "    <div class=\"game-description\">\n"
          << "      <h3>About This Game</h3>\n"
          << "      <p>" << EscapeHTML(config.description) << "</p>\n";

        // Tags
        if (!config.tags.empty()) {
            s << "      <div class=\"game-tags\">\n";
            for (const auto& tag : config.tags) {
                s << "        <span class=\"tag\">" << EscapeHTML(tag) << "</span>\n";
            }
            s << "      </div>\n";
        }

        s << "    </div>\n\n";
    }

    return s.str();
}

// ============================================================================
// Sidebar (medals + scoreboard)
// ============================================================================

std::string NewgroundsGamePage::GenerateSidebar(const GamePageConfig& config) {
    std::ostringstream s;

    bool hasSidebar = (config.showMedals || config.showScoreboard) && !config.ngAppId.empty();
    if (!hasSidebar) return "";

    s << "    <aside class=\"sidebar\">\n";

    // Medals section
    if (config.showMedals) {
        s << "      <div class=\"sidebar-card\">\n"
          << "        <h3 class=\"sidebar-title\">\n"
          << "          <span class=\"sidebar-icon\">&#127942;</span> Medals\n"
          << "        </h3>\n"
          << "        <div id=\"medals-list\" class=\"medals-list\">\n"
          << "          <div class=\"sidebar-loading\">Loading medals...</div>\n"
          << "        </div>\n"
          << "        <div id=\"medals-progress\" class=\"medals-progress\">\n"
          << "          <div class=\"medals-progress-bar\">\n"
          << "            <div id=\"medals-progress-fill\" class=\"medals-progress-fill\"></div>\n"
          << "          </div>\n"
          << "          <span id=\"medals-progress-text\" class=\"medals-progress-text\">0 / 0</span>\n"
          << "        </div>\n"
          << "      </div>\n\n";
    }

    // Scoreboard section
    if (config.showScoreboard) {
        s << "      <div class=\"sidebar-card\">\n"
          << "        <h3 class=\"sidebar-title\">\n"
          << "          <span class=\"sidebar-icon\">&#127941;</span> Leaderboard\n"
          << "        </h3>\n"
          << "        <div id=\"scoreboard-selector\" class=\"scoreboard-selector\"></div>\n"
          << "        <ol id=\"scoreboard-list\" class=\"scoreboard-list\">\n"
          << "          <li class=\"sidebar-loading\">Loading scores...</li>\n"
          << "        </ol>\n"
          << "      </div>\n\n";
    }

    // Share buttons
    if (config.showShareButtons) {
        std::string safeTitle = EscapeHTML(config.title);
        s << "      <div class=\"sidebar-card\">\n"
          << "        <h3 class=\"sidebar-title\">\n"
          << "          <span class=\"sidebar-icon\">&#128279;</span> Share\n"
          << "        </h3>\n"
          << "        <div class=\"share-buttons\">\n"
          << "          <button class=\"share-btn share-twitter\" onclick=\"shareTwitter()\" title=\"Share on Twitter\">Twitter</button>\n"
          << "          <button class=\"share-btn share-copy\" onclick=\"copyLink()\" title=\"Copy Link\">Copy Link</button>\n"
          << "        </div>\n"
          << "      </div>\n\n";
    }

    s << "    </aside>\n";

    return s.str();
}

// ============================================================================
// Footer
// ============================================================================

std::string NewgroundsGamePage::GenerateFooter(const GamePageConfig& config) {
    std::string safeTitle = EscapeHTML(config.title);
    std::string safeAuthor = EscapeHTML(config.author);

    std::ostringstream s;

    s << "    <footer class=\"game-footer\">\n"
      << "      <div class=\"footer-left\">\n"
      << "        <span>" << safeTitle << " &copy; " << safeAuthor << "</span>\n"
      << "      </div>\n"
      << "      <div class=\"footer-right\">\n"
      << "        <span>Made with <a href=\"https://enjin.dev\" target=\"_blank\" rel=\"noopener\">Enjin Engine</a></span>\n";

    if (!config.ngAppId.empty()) {
        s << "        <span class=\"footer-sep\">|</span>\n"
          << "        <span>Hosted on <a href=\"https://www.newgrounds.com\" target=\"_blank\" rel=\"noopener\">Newgrounds</a></span>\n";
    }

    s << "      </div>\n"
      << "    </footer>\n";

    return s.str();
}

// ============================================================================
// GenerateHTML
// ============================================================================

std::string NewgroundsGamePage::GenerateHTML(const GamePageConfig& config) {
    std::ostringstream html;

    bool hasSidebar = (config.showMedals || config.showScoreboard) && !config.ngAppId.empty();

    html << "<!DOCTYPE html>\n"
         << "<html lang=\"en\">\n"
         << "<head>\n"
         << GenerateMetaTags(config)
         << "  <link rel=\"stylesheet\" href=\"gamepage.css\">\n"
         << "</head>\n"
         << "<body>\n"
         << "  <div class=\"page-wrapper\">\n"
         << "    <div class=\"main-content" << (hasSidebar ? " has-sidebar" : "") << "\">\n"
         << "      <div class=\"game-column\">\n"
         << GenerateGameSection(config)
         << "      </div>\n";

    // Sidebar
    if (hasSidebar) {
        html << GenerateSidebar(config);
    }

    html << "    </div>\n"
         << GenerateFooter(config)
         << "  </div>\n\n";

    // Toast notification container
    html << "  <div id=\"toast-container\" class=\"toast-container\"></div>\n\n";

    // Newgrounds.io SDK (load from CDN if app ID is set)
    if (!config.ngAppId.empty()) {
        html << "  <script src=\"https://cdn.newgrounds.io/ngio.min.js\"></script>\n";
    }

    // Main game page script
    html << "  <script src=\"gamepage.js\"></script>\n\n";

    // Emscripten module placeholder
    html << "  <script>\n"
         << "    var Module = {\n"
         << "      canvas: document.getElementById('game-canvas'),\n"
         << "      onRuntimeInitialized: function() {\n"
         << "        console.log('" << EscapeJS(config.title) << " - Engine initialized');\n"
         << "        if (typeof hidePreloader === 'function') hidePreloader();\n"
         << "      },\n"
         << "      setStatus: function(text) {\n"
         << "        if (text) console.log(text);\n"
         << "        if (typeof updateProgress === 'function') {\n"
         << "          var m = text.match(/([\\d.]+)\\/([\\d.]+)/);\n"
         << "          if (m) updateProgress(parseInt(m[1]) / parseInt(m[2]));\n"
         << "        }\n"
         << "      }\n"
         << "    };\n"
         << "  </script>\n"
         << "  <!-- Add the compiled WASM loader here: -->\n"
         << "  <!-- <script src=\"" << EscapeHTML(config.title) << ".js\"></script> -->\n"
         << "</body>\n"
         << "</html>\n";

    return html.str();
}

// ============================================================================
// Newgrounds JS Initialization
// ============================================================================

std::string NewgroundsGamePage::GenerateNGInit(const GamePageConfig& config) {
    if (config.ngAppId.empty()) return "";

    std::string safeAppId = EscapeJS(config.ngAppId);
    std::string safeKey = EscapeJS(config.ngEncryptionKey);

    std::ostringstream js;

    js << "// ========================================\n"
       << "// Newgrounds.io API Integration\n"
       << "// ========================================\n\n"
       << "var ngIO = null;\n"
       << "var ngSession = null;\n"
       << "var ngMedals = [];\n"
       << "var ngScoreBoards = [];\n\n"
       << "function initNewgrounds() {\n"
       << "  if (typeof NGIO === 'undefined') {\n"
       << "    console.warn('Newgrounds.io SDK not loaded');\n"
       << "    return;\n"
       << "  }\n\n"
       << "  NGIO.init('" << safeAppId << "', '" << safeKey << "', {\n"
       << "    preloadMedals: true,\n"
       << "    preloadScoreBoards: true,\n"
       << "    preloadSaveSlots: false\n"
       << "  });\n\n"
       << "  NGIO.keepSessionAlive = true;\n\n"
       << "  NGIO.onLogin = function() {\n"
       << "    console.log('Newgrounds: Logged in as ' + NGIO.user.name);\n"
       << "    loadMedals();\n"
       << "    loadScoreBoards();\n"
       << "  };\n\n"
       << "  NGIO.onLogFail = function() {\n"
       << "    console.log('Newgrounds: Not logged in (guest mode)');\n"
       << "    loadMedals();\n"
       << "    loadScoreBoards();\n"
       << "  };\n"
       << "}\n\n";

    return js.str();
}

// ============================================================================
// Medal JS
// ============================================================================

std::string NewgroundsGamePage::GenerateMedalJS(const GamePageConfig& config) {
    if (config.ngAppId.empty() || !config.showMedals) return "";

    std::ostringstream js;

    js << "// ========================================\n"
       << "// Medal System\n"
       << "// ========================================\n\n"
       << "function loadMedals() {\n"
       << "  if (typeof NGIO === 'undefined' || !NGIO.medals) return;\n\n"
       << "  ngMedals = NGIO.medals;\n"
       << "  renderMedals();\n"
       << "}\n\n"
       << "function renderMedals() {\n"
       << "  var container = document.getElementById('medals-list');\n"
       << "  if (!container) return;\n\n"
       << "  if (!ngMedals || ngMedals.length === 0) {\n"
       << "    container.innerHTML = '<div class=\"sidebar-empty\">No medals available</div>';\n"
       << "    updateMedalProgress(0, 0);\n"
       << "    return;\n"
       << "  }\n\n"
       << "  var html = '';\n"
       << "  var unlocked = 0;\n"
       << "  for (var i = 0; i < ngMedals.length; i++) {\n"
       << "    var m = ngMedals[i];\n"
       << "    var isUnlocked = m.unlocked || false;\n"
       << "    if (isUnlocked) unlocked++;\n\n"
       << "    html += '<div class=\"medal-item' + (isUnlocked ? ' medal-unlocked' : ' medal-locked') + '\">';\n"
       << "    html += '<img class=\"medal-icon\" src=\"' + escapeAttr(m.icon || '') + '\" alt=\"' + escapeAttr(m.name || '') + '\" width=\"40\" height=\"40\">';\n"
       << "    html += '<div class=\"medal-info\">';\n"
       << "    html += '<div class=\"medal-name\">' + escapeHtml(m.name || '') + '</div>';\n"
       << "    html += '<div class=\"medal-desc\">' + escapeHtml(m.description || '') + '</div>';\n"
       << "    html += '</div>';\n"
       << "    html += '<div class=\"medal-value\">' + (m.value || 0) + 'pts</div>';\n"
       << "    html += '</div>';\n"
       << "  }\n\n"
       << "  container.innerHTML = html;\n"
       << "  updateMedalProgress(unlocked, ngMedals.length);\n"
       << "}\n\n"
       << "function updateMedalProgress(unlocked, total) {\n"
       << "  var fill = document.getElementById('medals-progress-fill');\n"
       << "  var text = document.getElementById('medals-progress-text');\n"
       << "  if (fill) fill.style.width = (total > 0 ? (unlocked / total * 100) : 0) + '%';\n"
       << "  if (text) text.textContent = unlocked + ' / ' + total;\n"
       << "}\n\n"
       << "// Called from game engine when a medal is unlocked\n"
       << "function onMedalUnlocked(medalId) {\n"
       << "  for (var i = 0; i < ngMedals.length; i++) {\n"
       << "    if (ngMedals[i].id === medalId) {\n"
       << "      ngMedals[i].unlocked = true;\n"
       << "      showToast('Medal Unlocked!', ngMedals[i].name, ngMedals[i].icon);\n"
       << "      renderMedals();\n"
       << "      break;\n"
       << "    }\n"
       << "  }\n"
       << "}\n\n";

    return js.str();
}

// ============================================================================
// Scoreboard JS
// ============================================================================

std::string NewgroundsGamePage::GenerateScoreboardJS(const GamePageConfig& config) {
    if (config.ngAppId.empty() || !config.showScoreboard) return "";

    std::ostringstream js;

    js << "// ========================================\n"
       << "// Scoreboard System\n"
       << "// ========================================\n\n"
       << "var currentBoardId = null;\n\n"
       << "function loadScoreBoards() {\n"
       << "  if (typeof NGIO === 'undefined' || !NGIO.scoreBoards) return;\n\n"
       << "  ngScoreBoards = NGIO.scoreBoards;\n"
       << "  renderScoreboardSelector();\n\n"
       << "  if (ngScoreBoards.length > 0) {\n"
       << "    selectScoreboard(ngScoreBoards[0].id);\n"
       << "  }\n"
       << "}\n\n"
       << "function renderScoreboardSelector() {\n"
       << "  var container = document.getElementById('scoreboard-selector');\n"
       << "  if (!container || ngScoreBoards.length <= 1) {\n"
       << "    if (container) container.style.display = 'none';\n"
       << "    return;\n"
       << "  }\n\n"
       << "  var html = '<select id=\"board-select\" class=\"board-select\" onchange=\"selectScoreboard(parseInt(this.value))\">';\n"
       << "  for (var i = 0; i < ngScoreBoards.length; i++) {\n"
       << "    var b = ngScoreBoards[i];\n"
       << "    html += '<option value=\"' + b.id + '\">' + escapeHtml(b.name || '') + '</option>';\n"
       << "  }\n"
       << "  html += '</select>';\n"
       << "  container.innerHTML = html;\n"
       << "}\n\n"
       << "function selectScoreboard(boardId) {\n"
       << "  currentBoardId = boardId;\n"
       << "  var list = document.getElementById('scoreboard-list');\n"
       << "  if (list) list.innerHTML = '<li class=\"sidebar-loading\">Loading scores...</li>';\n\n"
       << "  if (typeof NGIO === 'undefined') return;\n\n"
       << "  NGIO.callComponent('ScoreBoard.getScores', {\n"
       << "    id: boardId,\n"
       << "    limit: 10,\n"
       << "    period: 'A'\n"
       << "  }, function(result) {\n"
       << "    if (result.success && result.scores) {\n"
       << "      renderScores(result.scores);\n"
       << "    } else {\n"
       << "      renderScores([]);\n"
       << "    }\n"
       << "  });\n"
       << "}\n\n"
       << "function renderScores(scores) {\n"
       << "  var list = document.getElementById('scoreboard-list');\n"
       << "  if (!list) return;\n\n"
       << "  if (!scores || scores.length === 0) {\n"
       << "    list.innerHTML = '<li class=\"sidebar-empty\">No scores yet</li>';\n"
       << "    return;\n"
       << "  }\n\n"
       << "  var html = '';\n"
       << "  for (var i = 0; i < scores.length && i < 10; i++) {\n"
       << "    var s = scores[i];\n"
       << "    var rank = i + 1;\n"
       << "    var rankClass = rank <= 3 ? ' score-rank-' + rank : '';\n"
       << "    html += '<li class=\"score-entry' + rankClass + '\">';\n"
       << "    html += '<span class=\"score-rank\">' + rank + '</span>';\n"
       << "    html += '<span class=\"score-user\">' + escapeHtml(s.user ? s.user.name || 'Unknown' : 'Unknown') + '</span>';\n"
       << "    html += '<span class=\"score-value\">' + escapeHtml(s.formatted_value || String(s.value || 0)) + '</span>';\n"
       << "    html += '</li>';\n"
       << "  }\n"
       << "  list.innerHTML = html;\n"
       << "}\n\n";

    return js.str();
}

// ============================================================================
// Toast Notification JS
// ============================================================================

std::string NewgroundsGamePage::GenerateToastNotificationJS() {
    std::ostringstream js;

    js << "// ========================================\n"
       << "// Toast Notifications\n"
       << "// ========================================\n\n"
       << "function showToast(title, message, iconUrl) {\n"
       << "  var container = document.getElementById('toast-container');\n"
       << "  if (!container) return;\n\n"
       << "  var toast = document.createElement('div');\n"
       << "  toast.className = 'toast';\n\n"
       << "  var html = '';\n"
       << "  if (iconUrl) {\n"
       << "    html += '<img class=\"toast-icon\" src=\"' + escapeAttr(iconUrl) + '\" alt=\"\" width=\"32\" height=\"32\">';\n"
       << "  }\n"
       << "  html += '<div class=\"toast-content\">';\n"
       << "  html += '<div class=\"toast-title\">' + escapeHtml(title) + '</div>';\n"
       << "  html += '<div class=\"toast-message\">' + escapeHtml(message) + '</div>';\n"
       << "  html += '</div>';\n"
       << "  toast.innerHTML = html;\n\n"
       << "  container.appendChild(toast);\n\n"
       << "  // Trigger slide-in animation\n"
       << "  requestAnimationFrame(function() {\n"
       << "    toast.classList.add('toast-visible');\n"
       << "  });\n\n"
       << "  // Auto-dismiss after 4 seconds\n"
       << "  setTimeout(function() {\n"
       << "    toast.classList.remove('toast-visible');\n"
       << "    toast.classList.add('toast-hiding');\n"
       << "    setTimeout(function() {\n"
       << "      if (toast.parentNode) toast.parentNode.removeChild(toast);\n"
       << "    }, 400);\n"
       << "  }, 4000);\n"
       << "}\n\n";

    return js.str();
}

// ============================================================================
// GenerateJS
// ============================================================================

std::string NewgroundsGamePage::GenerateJS(const GamePageConfig& config) {
    std::ostringstream js;

    js << "// Enjin Engine - Newgrounds Game Page\n"
       << "// Generated by NewgroundsGamePage\n"
       << "(function() {\n"
       << "  'use strict';\n\n";

    // Utility: HTML escape for dynamic content (XSS protection)
    js << "  // ========================================\n"
       << "  // Utility Functions\n"
       << "  // ========================================\n\n"
       << "  var escapeDiv = document.createElement('div');\n"
       << "  window.escapeHtml = function(text) {\n"
       << "    escapeDiv.textContent = text;\n"
       << "    return escapeDiv.innerHTML;\n"
       << "  };\n\n"
       << "  window.escapeAttr = function(text) {\n"
       << "    return String(text).replace(/&/g,'&amp;').replace(/\"/g,'&quot;').replace(/'/g,'&#39;').replace(/</g,'&lt;').replace(/>/g,'&gt;');\n"
       << "  };\n\n";

    // Preloader
    if (config.showPreloader) {
        js << "  // ========================================\n"
           << "  // Preloader\n"
           << "  // ========================================\n\n"
           << "  var progressBar = document.getElementById('progress-bar');\n"
           << "  var progressText = document.getElementById('progress-text');\n"
           << "  var preloader = document.getElementById('preloader');\n"
           << "  var clickToPlay = document.getElementById('click-to-play');\n\n"
           << "  window.updateProgress = function(ratio) {\n"
           << "    ratio = Math.max(0, Math.min(1, ratio));\n"
           << "    if (progressBar) progressBar.style.width = (ratio * 100) + '%';\n"
           << "    if (progressText) progressText.textContent = 'Loading... ' + Math.floor(ratio * 100) + '%';\n"
           << "  };\n\n"
           << "  window.hidePreloader = function() {\n"
           << "    if (!preloader) return;\n"
           << "    if (progressBar) progressBar.style.width = '100%';\n"
           << "    if (progressText) progressText.style.display = 'none';\n"
           << "    var pc = preloader.querySelector('.preloader-progress-container');\n"
           << "    if (pc) pc.style.display = 'none';\n"
           << "    if (clickToPlay) {\n"
           << "      clickToPlay.style.display = 'block';\n"
           << "      clickToPlay.addEventListener('click', function() {\n"
           << "        // Resume audio context (required by browsers)\n"
           << "        if (typeof Module !== 'undefined' && Module._resumeAudio)\n"
           << "          Module._resumeAudio();\n"
           << "        // Fade out preloader\n"
           << "        preloader.style.transition = 'opacity 0.5s ease';\n"
           << "        preloader.style.opacity = '0';\n"
           << "        setTimeout(function() {\n"
           << "          preloader.style.display = 'none';\n"
           << "          document.getElementById('game-canvas').focus();\n"
           << "        }, 500);\n"
           << "      });\n"
           << "    }\n"
           << "  };\n\n"
           << "  // Simulate progress for preview (remove when using real WASM)\n"
           << "  var demoProgress = 0;\n"
           << "  var demoInterval = setInterval(function() {\n"
           << "    demoProgress += 0.02;\n"
           << "    window.updateProgress(demoProgress);\n"
           << "    if (demoProgress >= 1.0) {\n"
           << "      clearInterval(demoInterval);\n"
           << "      window.hidePreloader();\n"
           << "    }\n"
           << "  }, 50);\n\n";
    }

    // Fullscreen toggle
    if (config.showFullscreenButton) {
        js << "  // ========================================\n"
           << "  // Fullscreen Toggle\n"
           << "  // ========================================\n\n"
           << "  var fsBtn = document.getElementById('fullscreen-btn');\n"
           << "  if (fsBtn) {\n"
           << "    fsBtn.addEventListener('click', function() {\n"
           << "      var el = document.querySelector('.game-canvas-wrapper');\n"
           << "      if (!el) return;\n"
           << "      if (document.fullscreenElement || document.webkitFullscreenElement) {\n"
           << "        if (document.exitFullscreen) document.exitFullscreen();\n"
           << "        else if (document.webkitExitFullscreen) document.webkitExitFullscreen();\n"
           << "      } else {\n"
           << "        if (el.requestFullscreen) el.requestFullscreen();\n"
           << "        else if (el.webkitRequestFullscreen) el.webkitRequestFullscreen();\n"
           << "      }\n"
           << "    });\n"
           << "  }\n\n";
    }

    // Share buttons
    if (config.showShareButtons) {
        std::string safeTitle = EscapeJS(config.title);
        js << "  // ========================================\n"
           << "  // Share Functions\n"
           << "  // ========================================\n\n"
           << "  window.shareTwitter = function() {\n"
           << "    var text = encodeURIComponent('Check out " << safeTitle << "!');\n"
           << "    var url = encodeURIComponent(window.location.href);\n"
           << "    window.open('https://twitter.com/intent/tweet?text=' + text + '&url=' + url, '_blank', 'width=550,height=420');\n"
           << "  };\n\n"
           << "  window.copyLink = function() {\n"
           << "    navigator.clipboard.writeText(window.location.href).then(function() {\n"
           << "      showToast('Link Copied', 'URL copied to clipboard');\n"
           << "    });\n"
           << "  };\n\n";
    }

    // Close the IIFE, then add global functions that need to be accessible outside
    js << "})();\n\n";

    // Toast notifications (global scope, used by medal system and share)
    js << GenerateToastNotificationJS();

    // Newgrounds integration (global scope)
    js << GenerateNGInit(config);
    js << GenerateMedalJS(config);
    js << GenerateScoreboardJS(config);

    // Initialize NG on page load
    if (!config.ngAppId.empty()) {
        js << "// Initialize Newgrounds on page load\n"
           << "if (document.readyState === 'loading') {\n"
           << "  document.addEventListener('DOMContentLoaded', initNewgrounds);\n"
           << "} else {\n"
           << "  initNewgrounds();\n"
           << "}\n";
    }

    return js.str();
}

// ============================================================================
// GenerateCSS
// ============================================================================

std::string NewgroundsGamePage::GenerateCSS(const GamePageConfig& config) {
    u32 safeWidth = std::clamp(config.canvasWidth, 1u, 7680u);
    u32 safeHeight = std::clamp(config.canvasHeight, 1u, 4320u);
    std::string safeBg = SanitizeCSSColor(config.backgroundColor);
    std::string safeAccent = SanitizeCSSColor(config.accentColor);
    std::string safeText = SanitizeCSSColor(config.textColor);

    std::ostringstream css;

    // ---- Custom Properties ----
    css << "/* Enjin Engine - Newgrounds Game Page Styles */\n"
        << ":root {\n"
        << "  --bg: " << safeBg << ";\n"
        << "  --bg-lighter: " << safeBg << "18;\n"
        << "  --accent: " << safeAccent << ";\n"
        << "  --accent-dim: " << safeAccent << "80;\n"
        << "  --text: " << safeText << ";\n"
        << "  --text-dim: " << safeText << "99;\n"
        << "  --card: #16213e;\n"
        << "  --card-border: #0f3460;\n"
        << "  --canvas-width: " << safeWidth << "px;\n"
        << "  --canvas-height: " << safeHeight << "px;\n"
        << "}\n\n";

    // ---- Reset & Base ----
    css << "*, *::before, *::after { margin: 0; padding: 0; box-sizing: border-box; }\n\n"
        << "html, body {\n"
        << "  width: 100%;\n"
        << "  min-height: 100vh;\n"
        << "  background: var(--bg);\n"
        << "  color: var(--text);\n"
        << "  font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, 'Helvetica Neue', Arial, sans-serif;\n"
        << "  font-size: 14px;\n"
        << "  line-height: 1.5;\n"
        << "  -webkit-font-smoothing: antialiased;\n"
        << "}\n\n"
        << "a { color: var(--accent); text-decoration: none; }\n"
        << "a:hover { text-decoration: underline; }\n\n";

    // ---- Page Layout ----
    css << "/* Page Layout */\n"
        << ".page-wrapper {\n"
        << "  max-width: 1200px;\n"
        << "  margin: 0 auto;\n"
        << "  padding: 20px;\n"
        << "}\n\n"
        << ".main-content {\n"
        << "  display: flex;\n"
        << "  gap: 20px;\n"
        << "  align-items: flex-start;\n"
        << "}\n\n"
        << ".game-column {\n"
        << "  flex: 1;\n"
        << "  min-width: 0;\n"
        << "}\n\n";

    // ---- Header ----
    css << "/* Game Header */\n"
        << ".game-header {\n"
        << "  margin-bottom: 16px;\n"
        << "}\n\n"
        << ".game-title {\n"
        << "  font-size: 28px;\n"
        << "  font-weight: 700;\n"
        << "  color: var(--text);\n"
        << "  margin-bottom: 4px;\n"
        << "}\n\n"
        << ".game-meta {\n"
        << "  display: flex;\n"
        << "  align-items: center;\n"
        << "  gap: 12px;\n"
        << "  font-size: 13px;\n"
        << "  color: var(--text-dim);\n"
        << "}\n\n"
        << ".game-author a { color: var(--accent); }\n"
        << ".game-version {\n"
        << "  background: var(--card);\n"
        << "  padding: 2px 8px;\n"
        << "  border-radius: 4px;\n"
        << "  font-size: 11px;\n"
        << "}\n\n";

    // ---- Canvas Area ----
    css << "/* Canvas Area */\n"
        << ".game-canvas-wrapper {\n"
        << "  position: relative;\n"
        << "  width: var(--canvas-width);\n"
        << "  max-width: 100%;\n"
        << "  background: #000;\n"
        << "  border-radius: 8px;\n"
        << "  overflow: hidden;\n"
        << "  box-shadow: 0 0 30px " << safeAccent << "30, 0 4px 20px rgba(0,0,0,0.5);\n"
        << "}\n\n"
        << "#game-canvas {\n"
        << "  display: block;\n"
        << "  width: 100%;\n"
        << "  height: auto;\n"
        << "  aspect-ratio: " << safeWidth << " / " << safeHeight << ";\n"
        << "  outline: none;\n"
        << "  image-rendering: pixelated;\n"
        << "  image-rendering: crisp-edges;\n"
        << "}\n\n";

    // ---- Fullscreen Button ----
    css << "/* Fullscreen Button */\n"
        << ".fullscreen-btn {\n"
        << "  position: absolute;\n"
        << "  bottom: 8px;\n"
        << "  right: 8px;\n"
        << "  background: rgba(0,0,0,0.6);\n"
        << "  border: 1px solid rgba(255,255,255,0.15);\n"
        << "  border-radius: 4px;\n"
        << "  padding: 5px 7px;\n"
        << "  cursor: pointer;\n"
        << "  color: rgba(255,255,255,0.5);\n"
        << "  opacity: 0;\n"
        << "  transition: opacity 0.2s, color 0.2s;\n"
        << "  z-index: 10;\n"
        << "  line-height: 1;\n"
        << "}\n\n"
        << ".game-canvas-wrapper:hover .fullscreen-btn { opacity: 1; }\n"
        << ".fullscreen-btn:hover { color: white; background: rgba(0,0,0,0.8); }\n\n"
        << ".game-canvas-wrapper:fullscreen #game-canvas,\n"
        << ".game-canvas-wrapper:-webkit-full-screen #game-canvas {\n"
        << "  width: 100vw;\n"
        << "  height: 100vh;\n"
        << "  object-fit: contain;\n"
        << "}\n\n";

    // ---- Preloader ----
    css << "/* Preloader */\n"
        << ".preloader-overlay {\n"
        << "  position: absolute;\n"
        << "  inset: 0;\n"
        << "  display: flex;\n"
        << "  flex-direction: column;\n"
        << "  justify-content: center;\n"
        << "  align-items: center;\n"
        << "  background: var(--bg);\n"
        << "  z-index: 50;\n"
        << "}\n\n"
        << ".preloader-splash {\n"
        << "  max-width: 200px;\n"
        << "  max-height: 160px;\n"
        << "  margin-bottom: 24px;\n"
        << "  border-radius: 8px;\n"
        << "}\n\n"
        << ".preloader-title {\n"
        << "  font-size: 24px;\n"
        << "  font-weight: 700;\n"
        << "  color: var(--text);\n"
        << "  margin-bottom: 24px;\n"
        << "}\n\n"
        << ".preloader-progress-container {\n"
        << "  width: 240px;\n"
        << "  height: 6px;\n"
        << "  background: rgba(255,255,255,0.1);\n"
        << "  border-radius: 3px;\n"
        << "  overflow: hidden;\n"
        << "  margin: 8px 0;\n"
        << "}\n\n"
        << ".preloader-progress-bar {\n"
        << "  width: 0%;\n"
        << "  height: 100%;\n"
        << "  background: linear-gradient(90deg, var(--accent), var(--accent-dim));\n"
        << "  border-radius: 3px;\n"
        << "  transition: width 0.2s ease;\n"
        << "}\n\n"
        << ".preloader-text {\n"
        << "  color: var(--text-dim);\n"
        << "  font-size: 12px;\n"
        << "  margin-top: 6px;\n"
        << "}\n\n"
        << ".preloader-play-btn {\n"
        << "  color: var(--text);\n"
        << "  font-size: 18px;\n"
        << "  font-weight: 600;\n"
        << "  cursor: pointer;\n"
        << "  padding: 12px 36px;\n"
        << "  border: 2px solid var(--accent-dim);\n"
        << "  border-radius: 6px;\n"
        << "  transition: all 0.2s ease;\n"
        << "  user-select: none;\n"
        << "}\n\n"
        << ".preloader-play-btn:hover {\n"
        << "  background: var(--accent);\n"
        << "  border-color: var(--accent);\n"
        << "  color: white;\n"
        << "}\n\n";

    // ---- Controls & Description ----
    css << "/* Controls & Description */\n"
        << ".game-controls, .game-description {\n"
        << "  background: var(--card);\n"
        << "  border: 1px solid var(--card-border);\n"
        << "  border-radius: 8px;\n"
        << "  padding: 16px 20px;\n"
        << "  margin-top: 16px;\n"
        << "}\n\n"
        << ".game-controls h3, .game-description h3 {\n"
        << "  font-size: 14px;\n"
        << "  font-weight: 600;\n"
        << "  text-transform: uppercase;\n"
        << "  letter-spacing: 0.5px;\n"
        << "  color: var(--accent);\n"
        << "  margin-bottom: 8px;\n"
        << "}\n\n"
        << ".game-controls p, .game-description p {\n"
        << "  color: var(--text-dim);\n"
        << "  font-size: 13px;\n"
        << "  line-height: 1.6;\n"
        << "}\n\n"
        << ".game-tags {\n"
        << "  display: flex;\n"
        << "  flex-wrap: wrap;\n"
        << "  gap: 6px;\n"
        << "  margin-top: 12px;\n"
        << "}\n\n"
        << ".tag {\n"
        << "  background: var(--bg);\n"
        << "  border: 1px solid var(--card-border);\n"
        << "  padding: 3px 10px;\n"
        << "  border-radius: 12px;\n"
        << "  font-size: 11px;\n"
        << "  color: var(--text-dim);\n"
        << "}\n\n";

    // ---- Sidebar ----
    css << "/* Sidebar */\n"
        << ".sidebar {\n"
        << "  width: 280px;\n"
        << "  flex-shrink: 0;\n"
        << "  display: flex;\n"
        << "  flex-direction: column;\n"
        << "  gap: 16px;\n"
        << "}\n\n"
        << ".sidebar-card {\n"
        << "  background: var(--card);\n"
        << "  border: 1px solid var(--card-border);\n"
        << "  border-radius: 8px;\n"
        << "  padding: 16px;\n"
        << "}\n\n"
        << ".sidebar-title {\n"
        << "  font-size: 14px;\n"
        << "  font-weight: 600;\n"
        << "  text-transform: uppercase;\n"
        << "  letter-spacing: 0.5px;\n"
        << "  color: var(--accent);\n"
        << "  margin-bottom: 12px;\n"
        << "  display: flex;\n"
        << "  align-items: center;\n"
        << "  gap: 6px;\n"
        << "}\n\n"
        << ".sidebar-icon { font-size: 16px; }\n\n"
        << ".sidebar-loading, .sidebar-empty {\n"
        << "  color: var(--text-dim);\n"
        << "  font-size: 12px;\n"
        << "  font-style: italic;\n"
        << "  padding: 8px 0;\n"
        << "  list-style: none;\n"
        << "}\n\n";

    // ---- Medals ----
    css << "/* Medals */\n"
        << ".medals-list {\n"
        << "  display: flex;\n"
        << "  flex-direction: column;\n"
        << "  gap: 8px;\n"
        << "  max-height: 320px;\n"
        << "  overflow-y: auto;\n"
        << "}\n\n"
        << ".medal-item {\n"
        << "  display: flex;\n"
        << "  align-items: center;\n"
        << "  gap: 10px;\n"
        << "  padding: 8px;\n"
        << "  border-radius: 6px;\n"
        << "  background: rgba(0,0,0,0.2);\n"
        << "  transition: background 0.15s;\n"
        << "}\n\n"
        << ".medal-item:hover { background: rgba(0,0,0,0.35); }\n\n"
        << ".medal-icon {\n"
        << "  width: 40px;\n"
        << "  height: 40px;\n"
        << "  border-radius: 4px;\n"
        << "  flex-shrink: 0;\n"
        << "}\n\n"
        << ".medal-locked .medal-icon {\n"
        << "  filter: grayscale(100%) brightness(0.5);\n"
        << "}\n\n"
        << ".medal-unlocked .medal-icon {\n"
        << "  filter: none;\n"
        << "  box-shadow: 0 0 8px var(--accent-dim);\n"
        << "}\n\n"
        << ".medal-info { flex: 1; min-width: 0; }\n\n"
        << ".medal-name {\n"
        << "  font-size: 13px;\n"
        << "  font-weight: 600;\n"
        << "  color: var(--text);\n"
        << "  white-space: nowrap;\n"
        << "  overflow: hidden;\n"
        << "  text-overflow: ellipsis;\n"
        << "}\n\n"
        << ".medal-locked .medal-name { color: var(--text-dim); }\n\n"
        << ".medal-desc {\n"
        << "  font-size: 11px;\n"
        << "  color: var(--text-dim);\n"
        << "  white-space: nowrap;\n"
        << "  overflow: hidden;\n"
        << "  text-overflow: ellipsis;\n"
        << "}\n\n"
        << ".medal-value {\n"
        << "  font-size: 11px;\n"
        << "  font-weight: 600;\n"
        << "  color: var(--accent);\n"
        << "  flex-shrink: 0;\n"
        << "}\n\n"
        << ".medal-locked .medal-value { color: var(--text-dim); }\n\n"
        << ".medals-progress {\n"
        << "  display: flex;\n"
        << "  align-items: center;\n"
        << "  gap: 8px;\n"
        << "  margin-top: 12px;\n"
        << "  padding-top: 12px;\n"
        << "  border-top: 1px solid var(--card-border);\n"
        << "}\n\n"
        << ".medals-progress-bar {\n"
        << "  flex: 1;\n"
        << "  height: 4px;\n"
        << "  background: rgba(255,255,255,0.1);\n"
        << "  border-radius: 2px;\n"
        << "  overflow: hidden;\n"
        << "}\n\n"
        << ".medals-progress-fill {\n"
        << "  width: 0%;\n"
        << "  height: 100%;\n"
        << "  background: var(--accent);\n"
        << "  border-radius: 2px;\n"
        << "  transition: width 0.4s ease;\n"
        << "}\n\n"
        << ".medals-progress-text {\n"
        << "  font-size: 11px;\n"
        << "  color: var(--text-dim);\n"
        << "  flex-shrink: 0;\n"
        << "}\n\n";

    // ---- Scoreboard ----
    css << "/* Scoreboard */\n"
        << ".board-select {\n"
        << "  width: 100%;\n"
        << "  padding: 6px 10px;\n"
        << "  background: var(--bg);\n"
        << "  border: 1px solid var(--card-border);\n"
        << "  border-radius: 4px;\n"
        << "  color: var(--text);\n"
        << "  font-size: 12px;\n"
        << "  margin-bottom: 10px;\n"
        << "  cursor: pointer;\n"
        << "}\n\n"
        << ".scoreboard-list {\n"
        << "  list-style: none;\n"
        << "  display: flex;\n"
        << "  flex-direction: column;\n"
        << "  gap: 2px;\n"
        << "}\n\n"
        << ".score-entry {\n"
        << "  display: flex;\n"
        << "  align-items: center;\n"
        << "  gap: 8px;\n"
        << "  padding: 6px 8px;\n"
        << "  border-radius: 4px;\n"
        << "  font-size: 12px;\n"
        << "}\n\n"
        << ".score-entry:hover { background: rgba(0,0,0,0.2); }\n\n"
        << ".score-rank {\n"
        << "  width: 24px;\n"
        << "  text-align: center;\n"
        << "  font-weight: 700;\n"
        << "  color: var(--text-dim);\n"
        << "  flex-shrink: 0;\n"
        << "}\n\n"
        << ".score-rank-1 .score-rank { color: #ffd700; }\n"
        << ".score-rank-2 .score-rank { color: #c0c0c0; }\n"
        << ".score-rank-3 .score-rank { color: #cd7f32; }\n\n"
        << ".score-user {\n"
        << "  flex: 1;\n"
        << "  color: var(--text);\n"
        << "  white-space: nowrap;\n"
        << "  overflow: hidden;\n"
        << "  text-overflow: ellipsis;\n"
        << "}\n\n"
        << ".score-value {\n"
        << "  font-weight: 600;\n"
        << "  color: var(--accent);\n"
        << "  flex-shrink: 0;\n"
        << "}\n\n";

    // ---- Share Buttons ----
    css << "/* Share Buttons */\n"
        << ".share-buttons {\n"
        << "  display: flex;\n"
        << "  gap: 8px;\n"
        << "}\n\n"
        << ".share-btn {\n"
        << "  flex: 1;\n"
        << "  padding: 8px 12px;\n"
        << "  border: 1px solid var(--card-border);\n"
        << "  border-radius: 6px;\n"
        << "  background: var(--bg);\n"
        << "  color: var(--text);\n"
        << "  font-size: 12px;\n"
        << "  font-weight: 600;\n"
        << "  cursor: pointer;\n"
        << "  transition: all 0.15s;\n"
        << "  text-align: center;\n"
        << "}\n\n"
        << ".share-btn:hover {\n"
        << "  background: var(--accent);\n"
        << "  border-color: var(--accent);\n"
        << "  color: white;\n"
        << "}\n\n";

    // ---- Toast Notifications ----
    css << "/* Toast Notifications */\n"
        << ".toast-container {\n"
        << "  position: fixed;\n"
        << "  top: 20px;\n"
        << "  right: 20px;\n"
        << "  z-index: 9999;\n"
        << "  display: flex;\n"
        << "  flex-direction: column;\n"
        << "  gap: 8px;\n"
        << "  pointer-events: none;\n"
        << "}\n\n"
        << ".toast {\n"
        << "  display: flex;\n"
        << "  align-items: center;\n"
        << "  gap: 10px;\n"
        << "  background: var(--card);\n"
        << "  border: 1px solid var(--accent);\n"
        << "  border-radius: 8px;\n"
        << "  padding: 12px 16px;\n"
        << "  box-shadow: 0 4px 20px rgba(0,0,0,0.4);\n"
        << "  pointer-events: auto;\n"
        << "  transform: translateX(120%);\n"
        << "  transition: transform 0.3s cubic-bezier(0.175, 0.885, 0.32, 1.275);\n"
        << "  min-width: 260px;\n"
        << "  max-width: 360px;\n"
        << "}\n\n"
        << ".toast-visible { transform: translateX(0); }\n"
        << ".toast-hiding {\n"
        << "  transform: translateX(120%);\n"
        << "  transition: transform 0.3s ease-in;\n"
        << "}\n\n"
        << ".toast-icon {\n"
        << "  width: 32px;\n"
        << "  height: 32px;\n"
        << "  border-radius: 4px;\n"
        << "  flex-shrink: 0;\n"
        << "}\n\n"
        << ".toast-content { flex: 1; }\n\n"
        << ".toast-title {\n"
        << "  font-size: 12px;\n"
        << "  font-weight: 700;\n"
        << "  color: var(--accent);\n"
        << "  text-transform: uppercase;\n"
        << "  letter-spacing: 0.5px;\n"
        << "}\n\n"
        << ".toast-message {\n"
        << "  font-size: 13px;\n"
        << "  color: var(--text);\n"
        << "}\n\n";

    // ---- Footer ----
    css << "/* Footer */\n"
        << ".game-footer {\n"
        << "  display: flex;\n"
        << "  justify-content: space-between;\n"
        << "  align-items: center;\n"
        << "  padding: 16px 0;\n"
        << "  margin-top: 24px;\n"
        << "  border-top: 1px solid var(--card-border);\n"
        << "  font-size: 12px;\n"
        << "  color: var(--text-dim);\n"
        << "}\n\n"
        << ".footer-right {\n"
        << "  display: flex;\n"
        << "  align-items: center;\n"
        << "  gap: 4px;\n"
        << "}\n\n"
        << ".footer-sep { margin: 0 4px; opacity: 0.4; }\n\n";

    // ---- Responsive ----
    css << "/* Responsive Layout */\n"
        << "@media (max-width: 900px) {\n"
        << "  .main-content {\n"
        << "    flex-direction: column;\n"
        << "  }\n\n"
        << "  .sidebar {\n"
        << "    width: 100%;\n"
        << "    flex-direction: row;\n"
        << "    flex-wrap: wrap;\n"
        << "  }\n\n"
        << "  .sidebar-card {\n"
        << "    flex: 1;\n"
        << "    min-width: 240px;\n"
        << "  }\n\n"
        << "  .game-footer {\n"
        << "    flex-direction: column;\n"
        << "    gap: 8px;\n"
        << "    text-align: center;\n"
        << "  }\n"
        << "}\n\n"
        << "@media (max-width: 480px) {\n"
        << "  .page-wrapper { padding: 12px; }\n"
        << "  .game-title { font-size: 22px; }\n"
        << "  .sidebar-card { min-width: 100%; }\n"
        << "}\n\n";

    // ---- Scrollbar ----
    css << "/* Custom Scrollbar */\n"
        << ".medals-list::-webkit-scrollbar { width: 4px; }\n"
        << ".medals-list::-webkit-scrollbar-track { background: transparent; }\n"
        << ".medals-list::-webkit-scrollbar-thumb {\n"
        << "  background: var(--card-border);\n"
        << "  border-radius: 2px;\n"
        << "}\n"
        << ".medals-list::-webkit-scrollbar-thumb:hover { background: var(--accent-dim); }\n";

    return css.str();
}

// ============================================================================
// GenerateEmbedCodes
// ============================================================================

std::string NewgroundsGamePage::GenerateEmbedCodes(const GamePageConfig& config) {
    u32 safeWidth = std::clamp(config.canvasWidth, 1u, 7680u);
    u32 safeHeight = std::clamp(config.canvasHeight, 1u, 4320u);
    std::string safeTitle = EscapeHTML(config.title);

    // Sanitize title for use as an HTML id attribute (alphanumeric + hyphens only)
    std::string safeId;
    for (char c : config.title) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_') {
            safeId += c;
        } else if (c == ' ') {
            safeId += '-';
        }
    }
    if (safeId.empty()) safeId = "game";

    std::ostringstream embed;

    // Standard iframe embed
    embed << "<!-- Standard Embed (iframe) -->\n"
          << "<iframe src=\"index.html\" width=\"" << safeWidth
          << "\" height=\"" << safeHeight << "\" "
          << "frameborder=\"0\" scrolling=\"no\" "
          << "allowfullscreen=\"true\" "
          << "allow=\"autoplay; fullscreen; gamepad\" "
          << "style=\"border:none;\"></iframe>\n";

    // Newgrounds-compatible responsive embed
    embed << "\n<!-- Newgrounds Container Embed -->\n"
          << "<div id=\"game-" << safeId << "\" style=\""
          << "position:relative; width:100%; max-width:" << safeWidth << "px; "
          << "aspect-ratio:" << safeWidth << "/" << safeHeight << "; "
          << "margin:0 auto; background:#000; border-radius:8px; overflow:hidden;\">\n"
          << "  <iframe src=\"index.html\" "
          << "style=\"position:absolute; top:0; left:0; width:100%; height:100%; border:none;\" "
          << "allowfullscreen=\"true\" "
          << "allow=\"autoplay; fullscreen; gamepad\"></iframe>\n"
          << "</div>\n";

    return embed.str();
}

// ============================================================================
// DrawConfigPanel (ImGui)
// ============================================================================

void NewgroundsGamePage::DrawConfigPanel(GamePageConfig& config) {
#ifdef IMGUI_VERSION
    ImGui::PushID("GamePageConfig");

    // -- Game Metadata --
    if (ImGui::CollapsingHeader("Game Metadata", ImGuiTreeNodeFlags_DefaultOpen)) {
        char titleBuf[256];
        snprintf(titleBuf, sizeof(titleBuf), "%s", config.title.c_str());
        if (ImGui::InputText("Title", titleBuf, sizeof(titleBuf))) {
            config.title = titleBuf;
        }

        char authorBuf[128];
        snprintf(authorBuf, sizeof(authorBuf), "%s", config.author.c_str());
        if (ImGui::InputText("Author", authorBuf, sizeof(authorBuf))) {
            config.author = authorBuf;
        }

        char descBuf[1024];
        snprintf(descBuf, sizeof(descBuf), "%s", config.description.c_str());
        if (ImGui::InputTextMultiline("Description", descBuf, sizeof(descBuf),
                                       ImVec2(-1, 60))) {
            config.description = descBuf;
        }

        char versionBuf[32];
        snprintf(versionBuf, sizeof(versionBuf), "%s", config.version.c_str());
        if (ImGui::InputText("Version", versionBuf, sizeof(versionBuf))) {
            config.version = versionBuf;
        }

        // Tags
        ImGui::Text("Tags:");
        ImGui::SameLine();
        for (usize i = 0; i < config.tags.size(); ++i) {
            ImGui::PushID(static_cast<int>(i));
            ImGui::SameLine();
            ImGui::Text("[%s]", config.tags[i].c_str());
            ImGui::SameLine();
            if (ImGui::SmallButton("x")) {
                config.tags.erase(config.tags.begin() + static_cast<std::ptrdiff_t>(i));
                ImGui::PopID();
                break;
            }
            ImGui::PopID();
        }

        static char newTag[64] = "";
        ImGui::SetNextItemWidth(120);
        if (ImGui::InputText("##newtag", newTag, sizeof(newTag),
                              ImGuiInputTextFlags_EnterReturnsTrue)) {
            if (newTag[0] != '\0') {
                config.tags.push_back(newTag);
                newTag[0] = '\0';
            }
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Add Tag")) {
            if (newTag[0] != '\0') {
                config.tags.push_back(newTag);
                newTag[0] = '\0';
            }
        }

        // File paths
        char thumbBuf[512];
        snprintf(thumbBuf, sizeof(thumbBuf), "%s", config.thumbnailPath.c_str());
        if (ImGui::InputText("Thumbnail (315x250)", thumbBuf, sizeof(thumbBuf))) {
            config.thumbnailPath = thumbBuf;
        }

        char bannerBuf[512];
        snprintf(bannerBuf, sizeof(bannerBuf), "%s", config.bannerPath.c_str());
        if (ImGui::InputText("Banner (700x90)", bannerBuf, sizeof(bannerBuf))) {
            config.bannerPath = bannerBuf;
        }
    }

    // -- Newgrounds Integration --
    if (ImGui::CollapsingHeader("Newgrounds Integration")) {
        char appIdBuf[128];
        snprintf(appIdBuf, sizeof(appIdBuf), "%s", config.ngAppId.c_str());
        if (ImGui::InputText("App ID", appIdBuf, sizeof(appIdBuf))) {
            config.ngAppId = appIdBuf;
        }

        char keyBuf[256];
        snprintf(keyBuf, sizeof(keyBuf), "%s", config.ngEncryptionKey.c_str());
        if (ImGui::InputText("Encryption Key", keyBuf, sizeof(keyBuf),
                              ImGuiInputTextFlags_Password)) {
            config.ngEncryptionKey = keyBuf;
        }

        ImGui::Checkbox("Show Medals", &config.showMedals);
        ImGui::Checkbox("Show Scoreboard", &config.showScoreboard);
        ImGui::Checkbox("Show Author Link", &config.showAuthorLink);
    }

    // -- Page Layout --
    if (ImGui::CollapsingHeader("Page Layout")) {
        int w = static_cast<int>(config.canvasWidth);
        int h = static_cast<int>(config.canvasHeight);
        if (ImGui::InputInt("Canvas Width", &w)) {
            config.canvasWidth = static_cast<u32>(std::clamp(w, 1, 7680));
        }
        if (ImGui::InputInt("Canvas Height", &h)) {
            config.canvasHeight = static_cast<u32>(std::clamp(h, 1, 4320));
        }

        // Color pickers (parse hex to float array)
        auto hexToFloat = [](const std::string& hex, float out[3]) {
            unsigned int r = 0, g = 0, b = 0;
            if (hex.size() >= 7 && hex[0] == '#') {
                r = std::stoul(hex.substr(1, 2), nullptr, 16);
                g = std::stoul(hex.substr(3, 2), nullptr, 16);
                b = std::stoul(hex.substr(5, 2), nullptr, 16);
            }
            out[0] = r / 255.0f;
            out[1] = g / 255.0f;
            out[2] = b / 255.0f;
        };
        auto floatToHex = [](const float c[3]) -> std::string {
            char buf[8];
            snprintf(buf, sizeof(buf), "#%02x%02x%02x",
                     static_cast<int>(c[0] * 255.0f + 0.5f),
                     static_cast<int>(c[1] * 255.0f + 0.5f),
                     static_cast<int>(c[2] * 255.0f + 0.5f));
            return buf;
        };

        float bgCol[3], accentCol[3], textCol[3];
        hexToFloat(config.backgroundColor, bgCol);
        hexToFloat(config.accentColor, accentCol);
        hexToFloat(config.textColor, textCol);

        if (ImGui::ColorEdit3("Background", bgCol))
            config.backgroundColor = floatToHex(bgCol);
        if (ImGui::ColorEdit3("Accent", accentCol))
            config.accentColor = floatToHex(accentCol);
        if (ImGui::ColorEdit3("Text", textCol))
            config.textColor = floatToHex(textCol);
    }

    // -- Features --
    if (ImGui::CollapsingHeader("Features", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox("Show Preloader", &config.showPreloader);
        ImGui::Checkbox("Show Fullscreen Button", &config.showFullscreenButton);
        ImGui::Checkbox("Show Share Buttons", &config.showShareButtons);
        ImGui::Checkbox("Responsive Scaling", &config.responsiveScaling);
        ImGui::Checkbox("Show Controls Section", &config.showControls);

        if (config.showControls) {
            char ctrlBuf[512];
            snprintf(ctrlBuf, sizeof(ctrlBuf), "%s", config.controlsText.c_str());
            if (ImGui::InputTextMultiline("Controls Text", ctrlBuf, sizeof(ctrlBuf),
                                           ImVec2(-1, 40))) {
                config.controlsText = ctrlBuf;
            }
        }

        ImGui::Checkbox("Generate Embed Code", &config.generateEmbedCode);
        ImGui::Checkbox("Minify Output", &config.minifyOutput);
    }

    // -- Output --
    if (ImGui::CollapsingHeader("Output", ImGuiTreeNodeFlags_DefaultOpen)) {
        char outBuf[512];
        snprintf(outBuf, sizeof(outBuf), "%s", config.outputDir.c_str());
        if (ImGui::InputText("Output Directory", outBuf, sizeof(outBuf))) {
            config.outputDir = outBuf;
        }
    }

    ImGui::PopID();
#else
    (void)config;
#endif
}

// ============================================================================
// DrawPreview (ImGui)
// ============================================================================

void NewgroundsGamePage::DrawPreview(const GamePageConfig& config) {
#ifdef IMGUI_VERSION
    ImGui::PushID("GamePagePreview");

    ImGui::Text("Page Preview");
    ImGui::Separator();

    // Simulated page layout using ImGui drawing
    ImVec2 avail = ImGui::GetContentRegionAvail();
    float previewWidth = std::min(avail.x, 600.0f);
    float previewHeight = previewWidth * 0.6f;

    ImVec2 origin = ImGui::GetCursorScreenPos();
    ImDrawList* draw = ImGui::GetWindowDrawList();

    // Parse background color
    auto hexToU32 = [](const std::string& hex) -> ImU32 {
        unsigned int r = 26, g = 26, b = 46;
        if (hex.size() >= 7 && hex[0] == '#') {
            r = std::stoul(hex.substr(1, 2), nullptr, 16);
            g = std::stoul(hex.substr(3, 2), nullptr, 16);
            b = std::stoul(hex.substr(5, 2), nullptr, 16);
        }
        return IM_COL32(r, g, b, 255);
    };

    auto hexToU32Alpha = [](const std::string& hex, u8 alpha) -> ImU32 {
        unsigned int r = 233, g = 69, b = 96;
        if (hex.size() >= 7 && hex[0] == '#') {
            r = std::stoul(hex.substr(1, 2), nullptr, 16);
            g = std::stoul(hex.substr(3, 2), nullptr, 16);
            b = std::stoul(hex.substr(5, 2), nullptr, 16);
        }
        return IM_COL32(r, g, b, alpha);
    };

    ImU32 bgColor = hexToU32(config.backgroundColor);
    ImU32 accentColor = hexToU32Alpha(config.accentColor, 255);
    ImU32 cardColor = IM_COL32(22, 33, 62, 255);
    ImU32 textColor = IM_COL32(234, 234, 234, 255);
    ImU32 textDim = IM_COL32(234, 234, 234, 150);

    // Background
    draw->AddRectFilled(origin,
                        ImVec2(origin.x + previewWidth, origin.y + previewHeight),
                        bgColor, 4.0f);

    float margin = 12.0f;
    float x = origin.x + margin;
    float y = origin.y + margin;

    bool hasSidebar = (config.showMedals || config.showScoreboard) && !config.ngAppId.empty();
    float sidebarWidth = hasSidebar ? 80.0f : 0.0f;
    float gameWidth = previewWidth - margin * 2.0f - (hasSidebar ? sidebarWidth + 8.0f : 0.0f);

    // Title
    draw->AddText(ImVec2(x, y), textColor, config.title.c_str());
    y += 18.0f;

    // Author
    std::string byline = std::string("by ") + config.author;
    draw->AddText(ImVec2(x, y), textDim, byline.c_str());
    y += 16.0f;

    // Canvas area (with glow)
    float canvasHeight = gameWidth * 0.55f;
    draw->AddRectFilled(ImVec2(x - 1, y - 1),
                        ImVec2(x + gameWidth + 1, y + canvasHeight + 1),
                        hexToU32Alpha(config.accentColor, 40), 4.0f);
    draw->AddRectFilled(ImVec2(x, y),
                        ImVec2(x + gameWidth, y + canvasHeight),
                        IM_COL32(0, 0, 0, 255), 3.0f);

    // "GAME" label centered
    const char* gameLabel = "GAME";
    ImVec2 labelSize = ImGui::CalcTextSize(gameLabel);
    draw->AddText(ImVec2(x + (gameWidth - labelSize.x) * 0.5f,
                         y + (canvasHeight - labelSize.y) * 0.5f),
                  textDim, gameLabel);

    // Sidebar preview
    if (hasSidebar) {
        float sx = x + gameWidth + 8.0f;
        float sy = y;

        // Medal card
        if (config.showMedals) {
            float cardH = canvasHeight * 0.55f;
            draw->AddRectFilled(ImVec2(sx, sy),
                                ImVec2(sx + sidebarWidth, sy + cardH),
                                cardColor, 3.0f);
            draw->AddText(ImVec2(sx + 4, sy + 3), accentColor, "MEDALS");

            // Medal placeholders
            for (int i = 0; i < 3; i++) {
                float my = sy + 18.0f + i * 14.0f;
                draw->AddRectFilled(ImVec2(sx + 4, my),
                                    ImVec2(sx + 14, my + 10),
                                    IM_COL32(80, 80, 80, 255), 2.0f);
                draw->AddRectFilled(ImVec2(sx + 18, my + 2),
                                    ImVec2(sx + sidebarWidth - 4, my + 8),
                                    IM_COL32(60, 60, 60, 255), 1.0f);
            }

            sy += cardH + 6.0f;
        }

        // Scoreboard card
        if (config.showScoreboard) {
            float remaining = (y + canvasHeight) - sy;
            if (remaining > 20.0f) {
                draw->AddRectFilled(ImVec2(sx, sy),
                                    ImVec2(sx + sidebarWidth, sy + remaining),
                                    cardColor, 3.0f);
                draw->AddText(ImVec2(sx + 4, sy + 3), accentColor, "SCORES");

                for (int i = 0; i < 3; i++) {
                    float scy = sy + 18.0f + i * 12.0f;
                    char rankBuf[4];
                    snprintf(rankBuf, sizeof(rankBuf), "%d.", i + 1);
                    draw->AddText(ImVec2(sx + 4, scy), textDim, rankBuf);
                    draw->AddRectFilled(ImVec2(sx + 18, scy + 2),
                                        ImVec2(sx + sidebarWidth - 4, scy + 8),
                                        IM_COL32(60, 60, 60, 255), 1.0f);
                }
            }
        }
    }

    y += canvasHeight + 6.0f;

    // Controls preview
    if (config.showControls && !config.controlsText.empty()) {
        float cardH = 24.0f;
        draw->AddRectFilled(ImVec2(x, y),
                            ImVec2(x + gameWidth, y + cardH),
                            cardColor, 3.0f);
        draw->AddText(ImVec2(x + 6, y + 3), accentColor, "CONTROLS");
        y += cardH + 4.0f;
    }

    // Footer line
    float footerY = origin.y + previewHeight - 16.0f;
    draw->AddLine(ImVec2(origin.x + margin, footerY),
                  ImVec2(origin.x + previewWidth - margin, footerY),
                  IM_COL32(15, 52, 96, 255));
    draw->AddText(ImVec2(origin.x + margin, footerY + 2), textDim, "Made with Enjin Engine");

    // Reserve space
    ImGui::Dummy(ImVec2(previewWidth, previewHeight));

    // Stats below preview
    ImGui::Spacing();
    ImGui::TextDisabled("Canvas: %ux%u | Sidebar: %s | NG: %s",
                        config.canvasWidth, config.canvasHeight,
                        hasSidebar ? "Yes" : "No",
                        config.ngAppId.empty() ? "Not configured" : "Connected");

    ImGui::PopID();
#else
    (void)config;
#endif
}

} // namespace Build
} // namespace Enjin
