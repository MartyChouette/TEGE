#include "Enjin/Renderer/Scripting/RenderScript.h"
#include "Enjin/Logging/Log.h"
#include <vulkan/vulkan.h>
#include <fstream>
#include <sstream>
#include <algorithm>

namespace Enjin {
namespace Renderer {

RenderScript::RenderScript(RenderPipeline* pipeline)
    : m_Pipeline(pipeline) {
}

RenderScript::~RenderScript() {
}

bool RenderScript::Execute(const std::string& scriptCode) {
    if (!m_Pipeline) {
        ENJIN_LOG_ERROR(Renderer, "RenderScript: No pipeline assigned");
        return false;
    }

    // Parse and execute line by line
    std::istringstream stream(scriptCode);
    std::string line;
    u32 lineNum = 0;
    bool success = true;

    while (std::getline(stream, line)) {
        lineNum++;
        // Trim whitespace
        size_t first = line.find_first_not_of(" \t");
        if (first == std::string::npos) continue;
        line = line.substr(first);

        // Skip comments and empty lines
        if (line.empty() || line[0] == '#' || (line.size() >= 2 && line[0] == '/' && line[1] == '/')) {
            continue;
        }

        if (!ParseAndExecuteLine(line)) {
            ENJIN_LOG_WARN(Renderer, "RenderScript: Error on line %u: %s", lineNum, line.c_str());
            success = false;
        }
    }

    return success;
}

bool RenderScript::ExecuteFile(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        ENJIN_LOG_ERROR(Renderer, "RenderScript: Failed to open file: %s", filepath.c_str());
        return false;
    }

    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());
    ENJIN_LOG_INFO(Renderer, "RenderScript: Executing file: %s", filepath.c_str());
    return Execute(content);
}

void RenderScript::RegisterFunction(const std::string& name, std::function<void()> func) {
    m_VoidFunctions[name] = std::move(func);
}

void RenderScript::RegisterFunction(const std::string& name, std::function<void(f32)> func) {
    m_FloatFunctions[name] = std::move(func);
}

void RenderScript::RegisterFunction(const std::string& name, std::function<f32()> func) {
    m_RetFloatFunctions[name] = std::move(func);
}

bool RenderScript::ParseAndExecuteLine(const std::string& line) {
    // Simple command parser: "command arg1 arg2 ..."
    std::istringstream iss(line);
    std::string command;
    iss >> command;

    // Convert command to lowercase for case-insensitive matching
    std::string cmdLower = command;
    std::transform(cmdLower.begin(), cmdLower.end(), cmdLower.begin(), ::tolower);

    // Built-in commands
    if (cmdLower == "set_material_param" || cmdLower == "setmaterialparam") {
        std::string matName, paramName;
        f32 value;
        if (iss >> matName >> paramName >> value) {
            API_SetMaterialParam(matName, paramName, value);
            return true;
        }
        return false;
    }
    if (cmdLower == "reload_material" || cmdLower == "reloadmaterial") {
        std::string matName;
        if (iss >> matName) {
            API_ReloadMaterial(matName);
            return true;
        }
        return false;
    }
    if (cmdLower == "set_line_width" || cmdLower == "linewidth") {
        f32 width;
        if (iss >> width) {
            API_SetLineWidth(width);
            return true;
        }
        return false;
    }
    if (cmdLower == "set_depth_test" || cmdLower == "depthtest") {
        std::string val;
        if (iss >> val) {
            API_SetDepthTest(val == "true" || val == "1" || val == "on");
            return true;
        }
        return false;
    }
    if (cmdLower == "set_cull_mode" || cmdLower == "cullmode") {
        std::string mode;
        if (iss >> mode) {
            API_SetCullMode(mode);
            return true;
        }
        return false;
    }
    if (cmdLower == "wireframe") {
        std::string val;
        if (iss >> val) {
            API_EnableWireframe(val == "true" || val == "1" || val == "on");
            return true;
        }
        return false;
    }

    // Check user-registered functions
    auto voidIt = m_VoidFunctions.find(command);
    if (voidIt != m_VoidFunctions.end()) {
        voidIt->second();
        return true;
    }

    auto floatIt = m_FloatFunctions.find(command);
    if (floatIt != m_FloatFunctions.end()) {
        f32 arg;
        if (iss >> arg) {
            floatIt->second(arg);
            return true;
        }
        return false;
    }

    ENJIN_LOG_WARN(Renderer, "RenderScript: Unknown command: %s", command.c_str());
    return false;
}

// Script API implementations
void RenderScript::API_SetMaterialParam(const std::string& materialName, const std::string& paramName, f32 value) {
    if (!m_Pipeline) return;
    auto* mat = m_Pipeline->GetMaterial(materialName);
    if (mat) {
        mat->floatParams[paramName] = value;
        ENJIN_LOG_DEBUG(Renderer, "RenderScript: Set %s.%s = %.3f", materialName.c_str(), paramName.c_str(), value);
    } else {
        ENJIN_LOG_WARN(Renderer, "RenderScript: Material not found: %s", materialName.c_str());
    }
}

void RenderScript::API_SetMaterialParamVec4(const std::string& materialName, const std::string& paramName, const Math::Vector4& value) {
    if (!m_Pipeline) return;
    auto* mat = m_Pipeline->GetMaterial(materialName);
    if (mat) {
        mat->vectorParams[paramName] = value;
    }
}

void RenderScript::API_ReloadMaterial(const std::string& materialName) {
    if (!m_Pipeline) return;
    auto* mat = m_Pipeline->GetMaterial(materialName);
    if (mat) {
        m_Pipeline->ReloadShader(mat->shaderPath);
        ENJIN_LOG_INFO(Renderer, "RenderScript: Reloaded material: %s", materialName.c_str());
    }
}

void RenderScript::API_SetLineWidth(f32 width) {
    if (!m_Pipeline) return;
    RenderPipeline::PipelineState state;
    state.lineWidth = width;
    m_Pipeline->SetPipelineState(state);
}

void RenderScript::API_SetDepthTest(bool enable) {
    if (!m_Pipeline) return;
    RenderPipeline::PipelineState state;
    state.depthTest = enable;
    m_Pipeline->SetPipelineState(state);
}

void RenderScript::API_SetCullMode(const std::string& mode) {
    if (!m_Pipeline) return;
    RenderPipeline::PipelineState state;
    if (mode == "none") {
        state.cullMode = VK_CULL_MODE_NONE;
    } else if (mode == "front") {
        state.cullMode = VK_CULL_MODE_FRONT_BIT;
    } else if (mode == "back") {
        state.cullMode = VK_CULL_MODE_BACK_BIT;
    } else if (mode == "both") {
        state.cullMode = VK_CULL_MODE_FRONT_AND_BACK;
    }
    m_Pipeline->SetPipelineState(state);
}

void RenderScript::API_EnableWireframe(bool enable) {
    if (!m_Pipeline) return;
    RenderPipeline::PipelineState state;
    state.polygonMode = enable ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL;
    m_Pipeline->SetPipelineState(state);
}

} // namespace Renderer
} // namespace Enjin
