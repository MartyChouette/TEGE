#pragma once
// WHOLE_FILE_WEBGPU_GUARD
#if !ENJIN_RENDERER_WEBGPU

#include "Enjin/Platform/Platform.h"
#include "Enjin/Renderer/RenderPipeline/RenderPipeline.h"
#include <string>
#include <functional>
#include <unordered_map>

namespace Enjin {
namespace Renderer {

// Render script - allows scripting rendering behavior
// DESIGN: Easy to inject custom rendering logic via scripts
class ENJIN_API RenderScript {
public:
    RenderScript(RenderPipeline* pipeline);
    ~RenderScript();

    // Execute script code (Lua, Python, or custom DSL)
    bool Execute(const std::string& scriptCode);
    bool ExecuteFile(const std::string& filepath);

    // Register C++ functions for script access
    void RegisterFunction(const std::string& name, std::function<void()> func);
    void RegisterFunction(const std::string& name, std::function<void(f32)> func);
    void RegisterFunction(const std::string& name, std::function<f32()> func);

    // Script API - built-in commands available to scripts
    void API_SetMaterialParam(const std::string& materialName, const std::string& paramName, f32 value);
    void API_SetMaterialParamVec4(const std::string& materialName, const std::string& paramName, const Math::Vector4& value);
    void API_ReloadMaterial(const std::string& materialName);
    void API_SetLineWidth(f32 width);
    void API_SetDepthTest(bool enable);
    void API_SetCullMode(const std::string& mode); // "none", "front", "back", "both"
    void API_EnableWireframe(bool enable);

private:
    RenderPipeline* m_Pipeline = nullptr;
    std::unordered_map<std::string, std::function<void()>> m_VoidFunctions;
    std::unordered_map<std::string, std::function<void(f32)>> m_FloatFunctions;
    std::unordered_map<std::string, std::function<f32()>> m_RetFloatFunctions;

    bool ParseAndExecuteLine(const std::string& line);
};

} // namespace Renderer
} // namespace Enjin
#endif // !ENJIN_RENDERER_WEBGPU
