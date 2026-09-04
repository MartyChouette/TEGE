#include "Enjin/Platform/Platform.h"

#if ENJIN_PLATFORM_WEB

#include "Enjin/Renderer/WebGPU/WebGPUShaderCompiler.h"
#include "Enjin/Logging/Log.h"

namespace Enjin {
namespace Renderer {

WebGPUShaderCompiler::WebGPUShaderCompiler(WGPUDevice device) : m_Device(device) {}

WGPUShaderModule WebGPUShaderCompiler::CompileWGSL(const std::string& source, const char* label) {
    m_LastError.clear();

    WGPUShaderSourceWGSL wgslDesc = {};
    wgslDesc.chain.sType = WGPUSType_ShaderSourceWGSL;
    wgslDesc.code = {source.c_str(), source.size()};

    WGPUShaderModuleDescriptor desc = {};
    desc.nextInChain = &wgslDesc.chain;
    desc.label = {label ? label : "", WGPU_STRLEN};

    WGPUShaderModule module = wgpuDeviceCreateShaderModule(m_Device, &desc);
    if (!module) {
        m_LastError = "Failed to create shader module";
        if (label) {
            m_LastError += " '";
            m_LastError += label;
            m_LastError += "'";
        }
        ENJIN_LOG_ERROR(Core, "WebGPU shader compile failed: %s", m_LastError.c_str());
    }
    return module;
}


} // namespace Renderer
} // namespace Enjin

#endif // ENJIN_PLATFORM_WEB
