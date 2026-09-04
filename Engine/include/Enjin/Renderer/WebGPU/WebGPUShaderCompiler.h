#pragma once

#include "Enjin/Platform/Platform.h"

#if ENJIN_PLATFORM_WEB

#include "Enjin/Platform/Types.h"
#include "Enjin/Renderer/WebGPU/WebGPUTypes.h"
#include <string>

namespace Enjin {
namespace Renderer {

// Compiles WGSL source into WGPUShaderModule objects.
//
// The WGSL the web build actually ships is embedded in WebShaderData.h and in
// the few systems that carry their own (WebGPUParticleSystem,
// WebGPUVegetationSystem). There is no .wgsl tree on disk: there used to be
// one, this header claimed it was the source, and it had drifted 233 lines
// from what shipped -- missing snowParams, uvScrollU, matcapTex and
// scrollReflStrength, and using @group(2) @binding(6) for something else
// entirely. Editing it changed nothing, which is the worst way for a file to
// be wrong. Run tools/check_wgsl.mjs after editing the embedded strings.
class ENJIN_API WebGPUShaderCompiler {
public:
    explicit WebGPUShaderCompiler(WGPUDevice device);
    ~WebGPUShaderCompiler() = default;

    // Compile WGSL source code into a shader module.
    // Returns null handle on failure (error logged).
    WGPUShaderModule CompileWGSL(const std::string& source, const char* label = nullptr);

    // Get the last compilation error message (empty if none).
    const std::string& GetLastError() const { return m_LastError; }

private:
    WGPUDevice m_Device = nullptr;
    std::string m_LastError;
};

} // namespace Renderer
} // namespace Enjin

#endif // ENJIN_PLATFORM_WEB
