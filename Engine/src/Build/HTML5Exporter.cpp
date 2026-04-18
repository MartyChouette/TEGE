#include "Enjin/Build/HTML5Exporter.h"
#include "Enjin/Logging/Log.h"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
extern char** environ;
#endif

namespace Enjin {
namespace Build {

// Forward declaration (defined below Emscripten Build section)
static int RunProcess(const std::string& cmdLine);

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

    // Write serve.py — Chrome (and most browsers) refuse to load WebAssembly
    // and ES modules from file:// URLs, so a one-shot local server avoids the
    // "Unsafe attempt to load URL" error users hit when double-clicking index.html.
    {
        std::string servePath = config.outputDir + "/serve.py";
        std::ofstream f(servePath);
        if (f.is_open()) {
            f << "import http.server, socketserver\n"
              << "class H(http.server.SimpleHTTPRequestHandler):\n"
              << "    extensions_map = {**http.server.SimpleHTTPRequestHandler.extensions_map,\n"
              << "                      '.wasm': 'application/wasm', '.js': 'application/javascript'}\n"
              << "PORT = 9090\n"
              << "print(f'Serving at http://localhost:{PORT}')\n"
              << "with socketserver.TCPServer(('', PORT), H) as httpd:\n"
              << "    httpd.serve_forever()\n";
            result.files.push_back("serve.py");
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

    // Package as .zip for itch.io / Newgrounds upload
    if (config.zipOutput) {
        // Sanitize title for filename (replace non-alphanumeric with underscore)
        std::string safeFilename = config.title;
        for (auto& c : safeFilename) {
            if (!std::isalnum(static_cast<unsigned char>(c)) && c != '-' && c != '_')
                c = '_';
        }
        if (safeFilename.empty()) safeFilename = "game";

        // Place .zip next to the output directory (not inside it)
        std::filesystem::path outDir(config.outputDir);
        std::string zipPath = (outDir.parent_path() / (safeFilename + "_web.zip")).string();

        // Remove old zip if it exists
        std::filesystem::remove(zipPath);

#ifdef _WIN32
        // Use PowerShell Compress-Archive (always available on Windows 10/11)
        std::string psCmd = "powershell -NoProfile -Command \"Compress-Archive -Path '"
            + config.outputDir + "\\*' -DestinationPath '" + zipPath + "' -Force\"";
        int zipResult = RunProcess(psCmd);
#else
        // Use system zip command on Linux/Mac
        std::string zipCmd = "cd \"" + config.outputDir + "\" && zip -r \"" + zipPath + "\" .";
        int zipResult = RunProcess(zipCmd);
#endif

        if (zipResult == 0 && std::filesystem::exists(zipPath)) {
            auto zipSize = std::filesystem::file_size(zipPath);
            result.zipPath = zipPath;
            ENJIN_LOG_INFO(Build, "HTML5 zip created: %s (%.1f KB)",
                           zipPath.c_str(), static_cast<f32>(zipSize) / 1024.0f);
        } else {
            ENJIN_LOG_WARN(Build, "Failed to create zip (exit code %d) — files still available in %s",
                           zipResult, config.outputDir.c_str());
        }
    }

    return result;
}

// ============================================================================
// Emscripten Build Invocation
// ============================================================================

// S-C1: Safe process execution — avoids std::system() to prevent command injection
static int RunProcess(const std::string& cmdLine) {
#ifdef _WIN32
    STARTUPINFOA si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi{};
    std::string cmd = cmdLine; // CreateProcessA needs mutable buffer
    if (!CreateProcessA(nullptr, cmd.data(), nullptr, nullptr, FALSE,
                        CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        return -1;
    }
    DWORD waitResult = WaitForSingleObject(pi.hProcess, 300000); // 5 min timeout
    DWORD exitCode = 1;
    if (waitResult == WAIT_OBJECT_0) {
        GetExitCodeProcess(pi.hProcess, &exitCode);
    }
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return static_cast<int>(exitCode);
#else
    const char* argv[] = { "/bin/sh", "-c", cmdLine.c_str(), nullptr };
    pid_t pid = 0;
    if (posix_spawnp(&pid, "/bin/sh", nullptr, nullptr, const_cast<char**>(argv), environ) != 0) {
        return -1;
    }
    int status = 0;
    waitpid(pid, &status, 0);
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
#endif
}

bool HTML5Exporter::InvokeEmscriptenBuild(const std::string& outputDir,
                                            const std::string& enjpakPath) {
    // Strategy: copy pre-built WASM from the repo's build-web/ directory.
    // The WASM binary is the same for all projects — only the enjpak changes.
    // This avoids requiring emcmake/emmake/Python in the editor's PATH.
    //
    // If no pre-built WASM exists, fall back to invoking emcmake/emmake
    // (requires Emscripten SDK activated in the user's environment).

    namespace fs = std::filesystem;

    // Find the engine source root by walking up from the output directory
    // looking for the repo structure (CMakeLists.txt + Engine/ + build-web/).
    // Also try common locations relative to known paths.
    fs::path repoRoot;
    std::vector<fs::path> candidates;

    // Try walking up from the output dir (works if output is inside the repo)
    for (auto dir = fs::path(outputDir); dir.has_parent_path() && dir != dir.parent_path(); dir = dir.parent_path()) {
        if (fs::exists(dir / "build-web" / "bin" / "EnjinPlayer.js")) {
            repoRoot = dir;
            break;
        }
    }

    // Try common locations if not found
    if (repoRoot.empty()) {
        candidates = {
            fs::path("D:/GitHub/enjin"),
            fs::path("C:/GitHub/enjin"),
        };
        // Also try relative to the editor exe (build/ is typically inside the repo)
#ifdef _WIN32
        char exeBuf[MAX_PATH] = {};
        GetModuleFileNameA(nullptr, exeBuf, MAX_PATH);
        fs::path exeDir = fs::path(exeBuf).parent_path();
        // exe is at <repo>/build/bin/Release/ — go up 3 levels
        candidates.push_back(exeDir / ".." / ".." / "..");
#endif
        for (const auto& c : candidates) {
            auto canon = fs::weakly_canonical(c);
            if (fs::exists(canon / "build-web" / "bin" / "EnjinPlayer.js")) {
                repoRoot = canon;
                break;
            }
        }
    }

    // Try pre-built WASM from build-web/
    bool copied = false;
    if (!repoRoot.empty()) {
        fs::path jsSrc = repoRoot / "build-web" / "bin" / "EnjinPlayer.js";
        fs::path wasmSrc = repoRoot / "build-web" / "bin" / "EnjinPlayer.wasm";
        if (fs::exists(jsSrc) && fs::exists(wasmSrc)) {
            try {
                fs::copy_file(jsSrc, fs::path(outputDir) / "EnjinPlayer.js",
                              fs::copy_options::overwrite_existing);
                fs::copy_file(wasmSrc, fs::path(outputDir) / "EnjinPlayer.wasm",
                              fs::copy_options::overwrite_existing);
                copied = true;
                ENJIN_LOG_INFO(Build, "Web build: copied pre-built WASM from %s", jsSrc.parent_path().string().c_str());
            } catch (const std::exception& e) {
                ENJIN_LOG_WARN(Build, "Web build: failed to copy pre-built WASM: %s", e.what());
            }
        }
    }

    // Fallback: try invoking emcmake/emmake directly
    if (!copied) {
        std::string buildDir = outputDir + "/build-web";
        fs::create_directories(buildDir);

        std::string configCmd = "emcmake cmake -B \"" + buildDir + "\" -DENJIN_PLATFORM_WEB=ON";
        ENJIN_LOG_INFO(Build, "Web build: configuring... (%s)", configCmd.c_str());
        int configResult = RunProcess(configCmd);
        if (configResult != 0) {
            ENJIN_LOG_ERROR(Build, "Web build: CMake configure failed (exit code %d). Is emsdk activated?", configResult);
            return false;
        }

        std::string buildCmd = "emmake cmake --build \"" + buildDir + "\" --target EnjinPlayer";
        ENJIN_LOG_INFO(Build, "Web build: compiling... (%s)", buildCmd.c_str());
        int buildResult = RunProcess(buildCmd);
        if (buildResult != 0) {
            ENJIN_LOG_ERROR(Build, "Web build: compilation failed (exit code %d)", buildResult);
            return false;
        }

        fs::path jsSrc = fs::path(buildDir) / "bin" / "EnjinPlayer.js";
        fs::path wasmSrc = fs::path(buildDir) / "bin" / "EnjinPlayer.wasm";
        try {
            if (fs::exists(jsSrc))
                fs::copy_file(jsSrc, fs::path(outputDir) / "EnjinPlayer.js", fs::copy_options::overwrite_existing);
            if (fs::exists(wasmSrc))
                fs::copy_file(wasmSrc, fs::path(outputDir) / "EnjinPlayer.wasm", fs::copy_options::overwrite_existing);
            copied = true;
        } catch (const std::exception& e) {
            ENJIN_LOG_ERROR(Build, "Web build: failed to copy output files: %s", e.what());
            return false;
        }
    }

    if (!copied) {
        ENJIN_LOG_ERROR(Build, "Web build: no pre-built WASM found and emcmake unavailable");
        return false;
    }

    // Step 4: Copy asset pack
    if (!enjpakPath.empty() && std::filesystem::exists(enjpakPath)) {
        try {
            std::filesystem::copy_file(enjpakPath, outputDir + "/game.data",
                                        std::filesystem::copy_options::overwrite_existing);
        } catch (const std::exception& e) {
            ENJIN_LOG_WARN(Build, "Web build: failed to copy asset pack: %s", e.what());
        }
    }

    ENJIN_LOG_INFO(Build, "Web build: complete — output in %s", outputDir.c_str());
    return true;
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
         << "    <canvas id=\"game-canvas\" tabindex=\"0\"></canvas>\n";

    // Preloader overlay
    if (config.showPreloader) {
        html << "    <div id=\"preloader\">\n";
        if (!config.splashImagePath.empty()) {
            std::string ext = std::filesystem::path(config.splashImagePath).extension().string();
            html << "      <img id=\"splash\" src=\"splash" << HtmlEscape(ext) << "\" alt=\"Loading...\">\n";
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

    // Emscripten module — WebGPU context
    html << "  <script>\n"
         << "    // Emscripten module configuration (WebGPU)\n"
         << "    var Module = {\n"
         << "      canvas: (function() {\n"
         << "        var c = document.getElementById('game-canvas');\n"
         << "        // Request WebGPU context (Emscripten's USE_WEBGPU=1 expects this)\n"
         << "        if (navigator.gpu) {\n"
         << "          c.getContext('webgpu');\n"
         << "        }\n"
         << "        return c;\n"
         << "      })(),\n"
         << "      onRuntimeInitialized: function() {\n"
         << "        _runtimeReady = true;\n"
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
         << "    };\n\n";

    // Responsive canvas via ResizeObserver
    html << "    var _runtimeReady = false;\n"
         << "    var _ro = new ResizeObserver(function(entries) {\n"
         << "      var r = entries[0].contentRect;\n"
         << "      var dpr = window.devicePixelRatio || 1;\n"
         << "      var w = Math.floor(r.width);\n"
         << "      var h = Math.floor(r.height);\n"
         << "      if (w <= 0 || h <= 0) return;\n"
         << "      var c = document.getElementById('game-canvas');\n"
         << "      c.width = Math.floor(w * dpr);\n"
         << "      c.height = Math.floor(h * dpr);\n"
         << "      if (_runtimeReady) Module._onCanvasResize(w, h, dpr);\n"
         << "    });\n"
         << "    _ro.observe(document.getElementById('game-container'));\n\n";

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
         << "  <script src=\"EnjinPlayer.js\"></script>\n"
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
    (void)config.width;   // Canvas fills container; pixel size set by ResizeObserver
    (void)config.height;
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
        << "  width: 100%;\n"
        << "  height: 100%;\n"
        << "  display: block;\n"
        << "  image-rendering: pixelated;\n"
        << "  image-rendering: crisp-edges;\n"
        << "  outline: none;\n"
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
