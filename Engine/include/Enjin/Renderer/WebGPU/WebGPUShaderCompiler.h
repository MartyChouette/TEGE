#pragma once

#include "Enjin/Platform/Platform.h"

#if ENJIN_PLATFORM_WEB

#include "Enjin/Platform/Types.h"
#include "Enjin/Renderer/WebGPU/WebGPUTypes.h"
#include <string>

namespace Enjin {
namespace Renderer {

// Loads WGSL shader source and creates WGPUShaderModule objects.
// Shaders are maintained as .wgsl files in Engine/shaders/wgsl/.
class ENJIN_API WebGPUShaderCompiler {
public:
    explicit WebGPUShaderCompiler(WGPUDevice device);
    ~WebGPUShaderCompiler() = default;

    // Compile WGSL source code into a shader module.
    // Returns null handle on failure (error logged).
    WGPUShaderModule CompileWGSL(const std::string& source, const char* label = nullptr);

    // Load a .wgsl file from disk and compile it.
    WGPUShaderModule CompileFile(const std::string& path, const char* label = nullptr);

    // Get the last compilation error message (empty if none).
    const std::string& GetLastError() const { return m_LastError; }

private:
    WGPUDevice m_Device = nullptr;
    std::string m_LastError;
};

} // namespace Renderer
} // namespace Enjin

#endif // ENJIN_PLATFORM_WEB
