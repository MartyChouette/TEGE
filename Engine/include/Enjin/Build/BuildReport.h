#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include <string>
#include <vector>

namespace Enjin::Build {

// Build target platform (matches Renderer::BuildTarget values)
enum class BuildTargetPlatform : u8 {
    Desktop,    // Windows/Linux/macOS native
    Web         // WebAssembly + WebGPU
};

enum class MessageSeverity : u8 {
    Info,
    Warning,
    Error
};

struct BuildMessage {
    MessageSeverity severity = MessageSeverity::Info;
    std::string text;
    std::string file;  // related file path (optional)
};

struct BuildConfig {
    std::string projectPath;     // .enjinproject file
    std::string outputDir;       // output folder
    std::string buildKey;        // obfuscation key (default provided if empty)
    std::string windowTitle;     // game window title
    u32 windowWidth = 1280;
    u32 windowHeight = 720;
    bool fullscreen = false;
    BuildTargetPlatform target = BuildTargetPlatform::Desktop;
};

struct BuildResult {
    bool success = false;
    std::vector<BuildMessage> messages;
    u32 filesPacked = 0;
    u64 totalSizeBytes = 0;
    u64 packedSizeBytes = 0;
    std::string outputPath;
};

} // namespace Enjin::Build
