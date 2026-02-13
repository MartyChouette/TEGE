#include "Enjin/Renderer/Vulkan/VulkanShader.h"
#include "Enjin/Logging/Log.h"
#include "Enjin/Core/Assert.h"
#include <fstream>
#include <cstring>
#include <cstdio>
#include <filesystem>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <sys/wait.h>
#endif

namespace Enjin {
namespace Renderer {

VulkanShader::VulkanShader(VulkanContext* context)
    : m_Context(context) {
}

VulkanShader::~VulkanShader() {
    Destroy();
}

bool VulkanShader::LoadFromSPIRV(const u8* data, usize size) {
    if (size % 4 != 0) {
        ENJIN_LOG_ERROR(Renderer, "SPIR-V data size must be multiple of 4");
        return false;
    }

    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = size;
    createInfo.pCode = reinterpret_cast<const u32*>(data);

    VkResult result = vkCreateShaderModule(m_Context->GetDevice(), &createInfo, nullptr, &m_Module);
    if (result != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create shader module: %d", result);
        return false;
    }

    return true;
}

bool VulkanShader::CompileFromGLSL(const std::string& source, VkShaderStageFlagBits stage) {
    m_Stage = stage;

    std::vector<u32> spirv;
    if (!ShaderCompiler::CompileGLSL(source, stage, spirv)) {
        return false;
    }

    return LoadFromSPIRV(reinterpret_cast<const u8*>(spirv.data()), spirv.size() * sizeof(u32));
}

bool VulkanShader::LoadFromFile(const std::string& filepath) {
    std::vector<u32> spirv;
    if (!ShaderCompiler::LoadSPIRV(filepath, spirv)) {
        return false;
    }

    return LoadFromSPIRV(reinterpret_cast<const u8*>(spirv.data()), spirv.size() * sizeof(u32));
}

void VulkanShader::Destroy() {
    if (m_Module != VK_NULL_HANDLE) {
        vkDestroyShaderModule(m_Context->GetDevice(), m_Module, nullptr);
        m_Module = VK_NULL_HANDLE;
    }
}

namespace ShaderCompiler {

bool CompileGLSL(const std::string& source, VkShaderStageFlagBits stage, std::vector<u32>& spirv) {
    // Determine file extension based on shader stage
    const char* ext = ".glsl";
    switch (stage) {
        case VK_SHADER_STAGE_VERTEX_BIT:   ext = ".vert"; break;
        case VK_SHADER_STAGE_FRAGMENT_BIT: ext = ".frag"; break;
        case VK_SHADER_STAGE_COMPUTE_BIT:  ext = ".comp"; break;
        case VK_SHADER_STAGE_GEOMETRY_BIT: ext = ".geom"; break;
        case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:    ext = ".tesc"; break;
        case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT: ext = ".tese"; break;
        default: break;
    }

    // Create temp files for source and output
    namespace fs = std::filesystem;
    fs::path tempDir = fs::temp_directory_path();
    fs::path srcPath = tempDir / ("enjin_shader_temp" + std::string(ext));
    fs::path spvPath = tempDir / "enjin_shader_temp.spv";

    // Write GLSL source to temp file
    {
        std::ofstream srcFile(srcPath, std::ios::out);
        if (!srcFile.is_open()) {
            ENJIN_LOG_ERROR(Renderer, "Failed to create temp shader file: %s", srcPath.string().c_str());
            return false;
        }
        srcFile << source;
    }

    // N1: Use CreateProcessA instead of std::system to prevent command injection
    int result = -1;
#ifdef _WIN32
    auto runProcess = [](const std::string& cmdLine) -> int {
        STARTUPINFOA si = {};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;
        PROCESS_INFORMATION pi = {};
        std::string cmd = cmdLine; // CreateProcessA needs mutable buffer
        if (!CreateProcessA(NULL, cmd.data(), NULL, NULL, FALSE,
                           CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
            return -1;
        }
        // S9: Use bounded timeout (60s) instead of INFINITE to prevent hangs
        DWORD waitResult = WaitForSingleObject(pi.hProcess, 60000);
        DWORD exitCode = 1;
        if (waitResult == WAIT_TIMEOUT) {
            ENJIN_LOG_ERROR(Renderer, "Shader compile process timed out after 60 seconds, terminating");
            TerminateProcess(pi.hProcess, 1);
        } else {
            GetExitCodeProcess(pi.hProcess, &exitCode);
        }
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return static_cast<int>(exitCode);
    };

    // Try glslangValidator first, then glslc
    std::string cmdLine = "glslangValidator -V \"" + srcPath.string() + "\" -o \"" + spvPath.string() + "\"";
    result = runProcess(cmdLine);
    if (result != 0) {
        cmdLine = "glslc \"" + srcPath.string() + "\" -o \"" + spvPath.string() + "\"";
        result = runProcess(cmdLine);
    }
#else
    // S18: Use fork/exec to avoid shell injection via file paths
    auto forkExec = [](const char* prog, const std::string& srcStr, const std::string& spvStr) -> int {
        pid_t pid = fork();
        if (pid == -1) return -1;
        if (pid == 0) {
            // Child: exec with args passed directly (no shell)
            execlp(prog, prog, "-V", srcStr.c_str(), "-o", spvStr.c_str(), nullptr);
            _exit(127); // exec failed
        }
        int status = 0;
        waitpid(pid, &status, 0);
        return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    };

    std::string srcStr = srcPath.string();
    std::string spvStr = spvPath.string();
    result = forkExec("glslangValidator", srcStr, spvStr);
    if (result != 0) {
        // glslc uses different flag syntax
        pid_t pid = fork();
        if (pid == -1) {
            result = -1;
        } else if (pid == 0) {
            execlp("glslc", "glslc", srcStr.c_str(), "-o", spvStr.c_str(), nullptr);
            _exit(127);
        } else {
            int status = 0;
            waitpid(pid, &status, 0);
            result = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
        }
    }
#endif

    // Clean up source file
    std::error_code ec;
    fs::remove(srcPath, ec);

    if (result != 0) {
        ENJIN_LOG_ERROR(Renderer, "GLSL compilation failed (ensure glslangValidator or glslc is in PATH)");
        fs::remove(spvPath, ec);
        return false;
    }

    // Read compiled SPIR-V
    if (!LoadSPIRV(spvPath.string(), spirv)) {
        ENJIN_LOG_ERROR(Renderer, "Failed to read compiled SPIR-V from: %s", spvPath.string().c_str());
        fs::remove(spvPath, ec);
        return false;
    }

    // Clean up output file
    fs::remove(spvPath, ec);

    ENJIN_LOG_INFO(Renderer, "GLSL compiled to SPIR-V (%zu words)", spirv.size());
    return true;
}

bool LoadSPIRV(const std::string& filepath, std::vector<u32>& spirv) {
    std::ifstream file(filepath, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        ENJIN_LOG_ERROR(Renderer, "Failed to open SPIR-V file: %s", filepath.c_str());
        return false;
    }

    usize fileSize = static_cast<usize>(file.tellg());
    if (fileSize % 4 != 0) {
        ENJIN_LOG_ERROR(Renderer, "SPIR-V file size must be multiple of 4");
        return false;
    }

    spirv.resize(fileSize / sizeof(u32));
    file.seekg(0);
    file.read(reinterpret_cast<char*>(spirv.data()), fileSize);
    file.close();

    return true;
}

bool SaveSPIRV(const std::string& filepath, const std::vector<u32>& spirv) {
    std::ofstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create SPIR-V file: %s", filepath.c_str());
        return false;
    }

    file.write(reinterpret_cast<const char*>(spirv.data()), spirv.size() * sizeof(u32));
    file.close();

    return true;
}

} // namespace ShaderCompiler

} // namespace Renderer
} // namespace Enjin
