#include "Enjin/Build/HTML5Exporter.h"
#include "Enjin/Logging/Log.h"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>

namespace Enjin {
namespace Build {

// ============================================================================
// Export
// ============================================================================

HTML5ExportResult HTML5Exporter::Export(const HTML5ExportConfig& config,
                                        const BuildConfig& buildConfig) {
    HTML5ExportResult result;

    if (config.outputDir.empty()) {
        result.error = "Output directory not specified";
        return result;
    }

    // Create output directory
    std::filesystem::create_directories(config.outputDir);

    // Generate files
    std::string html = GenerateHTML(config, buildConfig);
    std::string preloaderJS = GeneratePreloaderJS(config);
    std::string css = GenerateStyleCSS(config);

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

    // Write preloader.js
    if (config.showPreloader) {
        std::string jsPath = config.outputDir + "/preloader.js";
        std::ofstream f(jsPath);
        if (f.is_open()) {
            f << preloaderJS;
            result.files.push_back("preloader.js");
        }
    }

    // Write style.css
    {
        std::string cssPath = config.outputDir + "/style.css";
        std::ofstream f(cssPath);
        if (f.is_open()) {
            f << css;
            result.files.push_back("style.css");
        }
    }

    // Generate embed code
    if (config.generateEmbedCode) {
        result.embedCode = GenerateEmbedSnippet(config);
    }

    // Copy favicon if specified
    if (!config.faviconPath.empty() && std::filesystem::exists(config.faviconPath)) {
        std::string dest = config.outputDir + "/favicon.ico";
        std::filesystem::copy_file(config.faviconPath, dest,
                                    std::filesystem::copy_options::overwrite_existing);
        result.files.push_back("favicon.ico");
    }

    // Copy splash image if specified
    if (!config.splashImagePath.empty() && std::filesystem::exists(config.splashImagePath)) {
        std::string ext = std::filesystem::path(config.splashImagePath).extension().string();
        std::string dest = config.outputDir + "/splash" + ext;
        std::filesystem::copy_file(config.splashImagePath, dest,
                                    std::filesystem::copy_options::overwrite_existing);
        result.files.push_back("splash" + ext);
    }

    result.success = true;
    result.outputPath = config.outputDir;

    ENJIN_LOG_INFO(Build, "HTML5 export complete: %zu files to %s",
                   result.files.size(), config.outputDir.c_str());
    return result;
}

// ============================================================================
// HTML Generation
// ============================================================================

// N5: HTML escape to prevent XSS via config fields
static std::string HtmlEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
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

std::string HTML5Exporter::GenerateHTML(const HTML5ExportConfig& config,
                                         const BuildConfig& buildConfig) {
    std::string safeTitle = HtmlEscape(config.title);

    // Clamp dimensions to reasonable ranges to prevent injection of extreme values
    u32 safeWidth = std::clamp(config.width, 1u, 7680u);
    u32 safeHeight = std::clamp(config.height, 1u, 4320u);

    std::ostringstream html;

    html << "<!DOCTYPE html>\n"
         << "<html lang=\"en\">\n"
         << "<head>\n"
         << "  <meta charset=\"utf-8\">\n"
         << "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no\">\n"
         << "  <title>" << safeTitle << "</title>\n";

    // OG meta tags
    html << "  <meta property=\"og:title\" content=\"" << safeTitle << "\">\n"
         << "  <meta property=\"og:type\" content=\"game\">\n";

    // Favicon
    if (!config.faviconPath.empty()) {
        html << "  <link rel=\"icon\" href=\"favicon.ico\" type=\"image/x-icon\">\n";
    }

    // CSS
    html << "  <link rel=\"stylesheet\" href=\"style.css\">\n";

    // Custom CSS — S4: strip sequences that could break out of style tag
    if (!config.customCSS.empty()) {
        std::string safeCSS = config.customCSS;
        // Remove </style> (case-insensitive) and angle brackets to prevent XSS
        for (auto& c : safeCSS) {
            if (c == '<' || c == '>') c = ' ';
        }
        html << "  <style>\n" << safeCSS << "\n  </style>\n";
    }

    html << "</head>\n"
         << "<body>\n"
         << "  <div id=\"game-container\">\n"
         << "    <canvas id=\"game-canvas\" width=\"" << safeWidth
         << "\" height=\"" << safeHeight << "\" tabindex=\"0\"></canvas>\n";

    // Preloader overlay
    if (config.showPreloader) {
        html << "    <div id=\"preloader\">\n";
        if (!config.splashImagePath.empty()) {
            std::string ext = std::filesystem::path(config.splashImagePath).extension().string();
            html << "      <img id=\"splash\" src=\"splash" << ext << "\" alt=\"Loading...\">\n";
        }
        html << "      <div id=\"progress-container\">\n"
             << "        <div id=\"progress-bar\"></div>\n"
             << "      </div>\n"
             << "      <div id=\"progress-text\">Loading...</div>\n"
             << "      <div id=\"click-to-play\" style=\"display:none;\">Click to Play</div>\n"
             << "    </div>\n";
    }

    // Fullscreen button
    if (config.showFullscreenButton) {
        html << "    <button id=\"fullscreen-btn\" title=\"Fullscreen\">\n"
             << "      <svg width=\"20\" height=\"20\" viewBox=\"0 0 20 20\" fill=\"white\">\n"
             << "        <path d=\"M3 3h5v2H5v3H3V3zm9 0h5v5h-2V5h-3V3zM3 12h2v3h3v2H3v-5zm12 3h-3v2h5v-5h-2v3z\"/>\n"
             << "      </svg>\n"
             << "    </button>\n";
    }

    html << "  </div>\n\n";

    // Scripts
    if (config.showPreloader) {
        html << "  <script src=\"preloader.js\"></script>\n";
    }

    // Emscripten module placeholder
    html << "  <script>\n"
         << "    // Emscripten module configuration (loaded when WASM build is available)\n"
         << "    var Module = {\n"
         << "      canvas: document.getElementById('game-canvas'),\n"
         << "      onRuntimeInitialized: function() {\n"
         << "        console.log('" << safeTitle << " - Engine initialized');\n";
    if (config.showPreloader) {
        html << "        if (typeof hidePreloader === 'function') hidePreloader();\n";
    }
    html << "      },\n"
         << "      setStatus: function(text) {\n"
         << "        if (text) console.log(text);\n";
    if (config.showPreloader) {
        html << "        if (typeof updateProgress === 'function') {\n"
             << "          var m = text.match(/([\\d.]+)\\/([\\d.]+)/);\n"
             << "          if (m) updateProgress(parseInt(m[1]) / parseInt(m[2]));\n"
             << "        }\n";
    }
    html << "      }\n"
         << "    };\n";

    // Fullscreen handler
    if (config.showFullscreenButton) {
        html << "    document.getElementById('fullscreen-btn').addEventListener('click', function() {\n"
             << "      var el = document.getElementById('game-container');\n"
             << "      if (el.requestFullscreen) el.requestFullscreen();\n"
             << "      else if (el.webkitRequestFullscreen) el.webkitRequestFullscreen();\n"
             << "      else if (el.mozRequestFullScreen) el.mozRequestFullScreen();\n"
             << "    });\n";
    }

    html << "  </script>\n"
         << "  <!-- Add the compiled WASM loader here: -->\n"
         << "  <!-- <script src=\"" << config.title << ".js\"></script> -->\n"
         << "</body>\n"
         << "</html>\n";

    return html.str();
}

// ============================================================================
// Preloader JS
// ============================================================================

std::string HTML5Exporter::GeneratePreloaderJS(const HTML5ExportConfig& config) {
    std::ostringstream js;

    js << "// Enjin Engine - Preloader\n"
       << "(function() {\n"
       << "  'use strict';\n\n"
       << "  var progressBar = document.getElementById('progress-bar');\n"
       << "  var progressText = document.getElementById('progress-text');\n"
       << "  var preloader = document.getElementById('preloader');\n"
       << "  var clickToPlay = document.getElementById('click-to-play');\n\n"
       << "  // Update progress bar (0.0 - 1.0)\n"
       << "  window.updateProgress = function(ratio) {\n"
       << "    ratio = Math.max(0, Math.min(1, ratio));\n"
       << "    if (progressBar) progressBar.style.width = (ratio * 100) + '%';\n"
       << "    if (progressText) progressText.textContent = 'Loading... ' + Math.floor(ratio * 100) + '%';\n"
       << "  };\n\n"
       << "  // Hide preloader with fade out\n"
       << "  window.hidePreloader = function() {\n"
       << "    if (!preloader) return;\n"
       << "    // Show click-to-play interstitial (required for audio context)\n"
       << "    if (progressBar) progressBar.style.width = '100%';\n"
       << "    if (progressText) progressText.style.display = 'none';\n"
       << "    if (document.getElementById('progress-container'))\n"
       << "      document.getElementById('progress-container').style.display = 'none';\n"
       << "    if (clickToPlay) {\n"
       << "      clickToPlay.style.display = 'block';\n"
       << "      clickToPlay.addEventListener('click', function() {\n"
       << "        // Resume audio context\n"
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
       << "  // Simulate progress for demo (remove when using real WASM)\n"
       << "  var demoProgress = 0;\n"
       << "  var demoInterval = setInterval(function() {\n"
       << "    demoProgress += 0.02;\n"
       << "    window.updateProgress(demoProgress);\n"
       << "    if (demoProgress >= 1.0) {\n"
       << "      clearInterval(demoInterval);\n"
       << "      window.hidePreloader();\n"
       << "    }\n"
       << "  }, 50);\n"
       << "})();\n";

    return js.str();
}

// ============================================================================
// CSS Generation
// ============================================================================

std::string HTML5Exporter::GenerateStyleCSS(const HTML5ExportConfig& config) {
    u32 safeWidth = std::clamp(config.width, 1u, 7680u);
    u32 safeHeight = std::clamp(config.height, 1u, 4320u);
    std::ostringstream css;

    css << "/* Enjin Engine - HTML5 Export Styles */\n"
        << "* { margin: 0; padding: 0; box-sizing: border-box; }\n\n"
        << "html, body {\n"
        << "  width: 100%;\n"
        << "  height: 100%;\n"
        << "  overflow: hidden;\n"
        << "  background: " << config.backgroundColor << ";\n"
        << "  font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;\n"
        << "}\n\n"
        << "#game-container {\n"
        << "  display: flex;\n"
        << "  justify-content: center;\n"
        << "  align-items: center;\n"
        << "  width: 100%;\n"
        << "  height: 100%;\n"
        << "  position: relative;\n"
        << "}\n\n"
        << "#game-canvas {\n"
        << "  max-width: 100%;\n"
        << "  max-height: 100%;\n"
        << "  width: " << safeWidth << "px;\n"
        << "  height: " << safeHeight << "px;\n"
        << "  image-rendering: pixelated;\n"
        << "  image-rendering: crisp-edges;\n"
        << "  outline: none;\n"
        << "}\n\n"
        << "/* Responsive scaling */\n"
        << "@media (max-width: " << safeWidth << "px), (max-height: " << safeHeight << "px) {\n"
        << "  #game-canvas {\n"
        << "    width: 100vw;\n"
        << "    height: " << (f32)safeHeight / (f32)safeWidth * 100.0f << "vw;\n"
        << "    max-height: 100vh;\n"
        << "  }\n"
        << "}\n\n";

    // Preloader styles
    if (config.showPreloader) {
        css << "/* Preloader */\n"
            << "#preloader {\n"
            << "  position: absolute;\n"
            << "  top: 0; left: 0; right: 0; bottom: 0;\n"
            << "  display: flex;\n"
            << "  flex-direction: column;\n"
            << "  justify-content: center;\n"
            << "  align-items: center;\n"
            << "  background: " << config.backgroundColor << ";\n"
            << "  z-index: 100;\n"
            << "}\n\n"
            << "#splash {\n"
            << "  max-width: 300px;\n"
            << "  max-height: 200px;\n"
            << "  margin-bottom: 30px;\n"
            << "}\n\n"
            << "#progress-container {\n"
            << "  width: 260px;\n"
            << "  height: 8px;\n"
            << "  background: rgba(255,255,255,0.15);\n"
            << "  border-radius: 4px;\n"
            << "  overflow: hidden;\n"
            << "  margin: 10px 0;\n"
            << "}\n\n"
            << "#progress-bar {\n"
            << "  width: 0%;\n"
            << "  height: 100%;\n"
            << "  background: linear-gradient(90deg, #4a9eff, #7bb8ff);\n"
            << "  border-radius: 4px;\n"
            << "  transition: width 0.2s ease;\n"
            << "}\n\n"
            << "#progress-text {\n"
            << "  color: rgba(255,255,255,0.6);\n"
            << "  font-size: 13px;\n"
            << "  margin-top: 8px;\n"
            << "}\n\n"
            << "#click-to-play {\n"
            << "  color: white;\n"
            << "  font-size: 22px;\n"
            << "  cursor: pointer;\n"
            << "  padding: 14px 40px;\n"
            << "  border: 2px solid rgba(255,255,255,0.4);\n"
            << "  border-radius: 8px;\n"
            << "  transition: all 0.2s ease;\n"
            << "  user-select: none;\n"
            << "}\n\n"
            << "#click-to-play:hover {\n"
            << "  background: rgba(255,255,255,0.1);\n"
            << "  border-color: rgba(255,255,255,0.7);\n"
            << "}\n\n";
    }

    // Fullscreen button
    if (config.showFullscreenButton) {
        css << "/* Fullscreen button */\n"
            << "#fullscreen-btn {\n"
            << "  position: absolute;\n"
            << "  bottom: 10px;\n"
            << "  right: 10px;\n"
            << "  background: rgba(0,0,0,0.5);\n"
            << "  border: none;\n"
            << "  border-radius: 4px;\n"
            << "  padding: 6px 8px;\n"
            << "  cursor: pointer;\n"
            << "  opacity: 0.4;\n"
            << "  transition: opacity 0.2s;\n"
            << "  z-index: 50;\n"
            << "}\n\n"
            << "#fullscreen-btn:hover {\n"
            << "  opacity: 0.9;\n"
            << "}\n\n";
    }

    // Fullscreen mode
    css << "/* Fullscreen mode */\n"
        << "#game-container:fullscreen #game-canvas,\n"
        << "#game-container:-webkit-full-screen #game-canvas {\n"
        << "  width: 100vw;\n"
        << "  height: 100vh;\n"
        << "  max-width: 100vw;\n"
        << "  max-height: 100vh;\n"
        << "}\n";

    return css.str();
}

// ============================================================================
// Embed Snippet
// ============================================================================

std::string HTML5Exporter::GenerateEmbedSnippet(const HTML5ExportConfig& config) {
    u32 safeWidth = std::clamp(config.width, 1u, 7680u);
    u32 safeHeight = std::clamp(config.height, 1u, 4320u);
    std::string safeTitle = HtmlEscape(config.title);
    std::ostringstream embed;

    // iframe embed (Newgrounds-compatible)
    embed << "<!-- Embed Code (iframe) -->\n"
          << "<iframe src=\"index.html\" width=\"" << safeWidth
          << "\" height=\"" << safeHeight << "\" "
          << "frameborder=\"0\" scrolling=\"no\" "
          << "allowfullscreen=\"true\" "
          << "allow=\"autoplay; fullscreen; gamepad\" "
          << "style=\"border:none;\"></iframe>\n\n"
          << "<!-- Embed Code (Newgrounds-compatible) -->\n"
          << "<div id=\"" << safeTitle << "\" style=\"width:" << safeWidth
          << "px;height:" << safeHeight << "px;\">\n"
          << "  <iframe src=\"index.html\" width=\"100%\" height=\"100%\" "
          << "frameborder=\"0\" allowfullscreen></iframe>\n"
          << "</div>\n";

    return embed.str();
}

} // namespace Build
} // namespace Enjin
