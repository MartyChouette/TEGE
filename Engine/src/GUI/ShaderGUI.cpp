#include "Enjin/GUI/ShaderGUI.h"
#include "Enjin/Renderer/Materials/MaterialSystem.h"
#include "Enjin/Logging/Log.h"
#include <imgui.h>

namespace Enjin {
namespace GUI {

ShaderGUI::ShaderGUI() {
}

ShaderGUI::~ShaderGUI() {
}

bool ShaderGUI::Initialize() {
    ENJIN_LOG_INFO(Renderer, "Shader GUI initialized");
    return true;
}

void ShaderGUI::Render() {
    if (!m_ShowMaterialEditor && !m_ShowShaderEditor) {
        return;
    }

    if (m_ShowMaterialEditor) {
        RenderMaterialEditor();
    }

    if (m_ShowShaderEditor) {
        RenderShaderEditor();
    }
}

void ShaderGUI::RegisterMaterial(Renderer::MaterialInstance* material) {
    if (material) {
        m_Materials.push_back(material);
        ENJIN_LOG_DEBUG(Renderer, "Registered material for GUI editing");
    }
}

void ShaderGUI::RenderMaterialEditor() {
    ImGui::Begin("Material Editor", &m_ShowMaterialEditor);

    if (m_Materials.empty()) {
        ImGui::TextDisabled("No materials registered");
        ImGui::End();
        return;
    }

    for (usize i = 0; i < m_Materials.size(); ++i) {
        auto* material = m_Materials[i];
        if (!material) continue;

        const auto& def = material->GetDefinition();
        if (ImGui::TreeNode(reinterpret_cast<void*>(static_cast<uintptr_t>(i)), "%s", def.name.c_str())) {
            ImGui::TextDisabled("Vertex: %s", def.vertexShader.c_str());
            ImGui::TextDisabled("Fragment: %s", def.fragmentShader.c_str());

            ImGui::Separator();

            // Render editable parameters
            for (auto& [paramName, param] : def.parameters) {
                auto* p = material->GetParameter(paramName);
                if (!p) continue;

                switch (p->type) {
                    case Renderer::MaterialParamType::Float: {
                        f32 val = p->floatValue;
                        if (ImGui::SliderFloat(paramName.c_str(), &val, 0.0f, 1.0f)) {
                            material->SetParameter(paramName, val);
                        }
                        break;
                    }
                    case Renderer::MaterialParamType::Vector4: {
                        f32 col[4] = { p->vec4Value.x, p->vec4Value.y, p->vec4Value.z, p->vec4Value.w };
                        if (ImGui::ColorEdit4(paramName.c_str(), col)) {
                            material->SetParameter(paramName, Math::Vector4(col[0], col[1], col[2], col[3]));
                        }
                        break;
                    }
                    default:
                        ImGui::Text("%s: (unsupported type)", paramName.c_str());
                        break;
                }
            }

            // Pipeline state display
            if (ImGui::TreeNode("Pipeline State")) {
                ImGui::Text("Depth Test: %s", def.depthTest ? "On" : "Off");
                ImGui::Text("Depth Write: %s", def.depthWrite ? "On" : "Off");
                ImGui::Text("Blend: %s", def.blendEnable ? "On" : "Off");
                ImGui::Text("Hot Reload: %s", def.hotReloadEnabled ? "On" : "Off");
                ImGui::TreePop();
            }

            if (ImGui::Button("Reload")) {
                material->Reload();
            }

            ImGui::TreePop();
        }
    }

    ImGui::End();
}

void ShaderGUI::RenderShaderEditor() {
    ImGui::Begin("Shader Editor", &m_ShowShaderEditor);

    ImGui::TextWrapped("Shader source editing is available via external text editor. "
                       "Use hot-reload to see changes in real-time.");
    ImGui::Separator();

    if (!m_Materials.empty()) {
        ImGui::Text("Loaded Shaders:");
        for (auto* material : m_Materials) {
            if (!material) continue;
            const auto& def = material->GetDefinition();
            ImGui::BulletText("%s", def.name.c_str());
            if (!def.vertexShader.empty()) {
                ImGui::TextDisabled("  Vert: %s", def.vertexShader.c_str());
            }
            if (!def.fragmentShader.empty()) {
                ImGui::TextDisabled("  Frag: %s", def.fragmentShader.c_str());
            }
        }
    }

    ImGui::End();
}

void ShaderGUI::RenderParameterControl(const std::string& name, f32& value, f32 min, f32 max) {
    ImGui::SliderFloat(name.c_str(), &value, min, max);
}

void ShaderGUI::RenderParameterControl(const std::string& name, Math::Vector4& value) {
    f32 col[4] = { value.x, value.y, value.z, value.w };
    if (ImGui::ColorEdit4(name.c_str(), col)) {
        value = Math::Vector4(col[0], col[1], col[2], col[3]);
    }
}

} // namespace GUI
} // namespace Enjin
