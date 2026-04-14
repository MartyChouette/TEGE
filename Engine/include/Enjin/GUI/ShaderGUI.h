#pragma once
// WHOLE_FILE_WEBGPU_GUARD
#if !ENJIN_RENDERER_WEBGPU

#include "Enjin/Platform/Platform.h"
#include "Enjin/Renderer/Materials/MaterialSystem.h"
#include <string>
#include <functional>
#include <unordered_map>

namespace Enjin {
namespace GUI {

/**
 * @brief Shader GUI Interface
 * 
 * Simple interface for modifying shader parameters in real-time.
 * Provides:
 * - Material parameter editing
 * - Shader hot-reload controls
 * - Pipeline state tweaking
 * - Real-time preview
 */
class ENJIN_API ShaderGUI {
public:
    ShaderGUI();
    ~ShaderGUI();

    /**
     * @brief Initialize GUI system
     */
    bool Initialize();

    /**
     * @brief Render GUI
     */
    void Render();

    /**
     * @brief Register material for editing
     */
    void RegisterMaterial(Renderer::MaterialInstance* material);

    /**
     * @brief Show material editor
     */
    void ShowMaterialEditor(bool show) { m_ShowMaterialEditor = show; }

    /**
     * @brief Show shader editor
     */
    void ShowShaderEditor(bool show) { m_ShowShaderEditor = show; }

private:
    bool m_ShowMaterialEditor = false;
    bool m_ShowShaderEditor = false;
    std::vector<Renderer::MaterialInstance*> m_Materials;
    
    void RenderMaterialEditor();
    void RenderShaderEditor();
    void RenderParameterControl(const std::string& name, f32& value, f32 min, f32 max);
    void RenderParameterControl(const std::string& name, Math::Vector4& value);
};

} // namespace GUI
} // namespace Enjin
#endif // !ENJIN_RENDERER_WEBGPU
