#include "Enjin/Plugin/PluginSDK.h"
#include <imgui.h>
#include <string>

class ExamplePlugin : public Enjin::Plugin::IPlugin {
public:
    const char* GetName() const override { return "ExamplePlugin"; }
    const char* GetVersion() const override { return "1.0.0"; }

    bool OnLoad() override { return true; }

    bool OnLoad(Enjin::Plugin::PluginContext& context) override {
        m_Context = context;
        return true;
    }

    void OnUnload() override {}

    void OnUpdate(f32 deltaTime) override {
        m_ElapsedTime += deltaTime;
    }

    void OnEditorUI() override {
        if (ImGui::Begin("Example Plugin")) {
            ImGui::Text("Hello from ExamplePlugin!");
            ImGui::Text("Elapsed: %.1f s", m_ElapsedTime);
            if (m_Context.world) {
                ImGui::Text("World is available");
            }
            ImGui::Separator();
            ImGui::TextWrapped("This is an example plugin showing how to use the Enjin Plugin SDK.");
        }
        ImGui::End();
    }

    void OnSaveState(std::string& outState) override {
        outState = std::to_string(m_ElapsedTime);
    }

    void OnRestoreState(const std::string& state) override {
        try { m_ElapsedTime = std::stof(state); } catch (...) {}
    }

private:
    Enjin::Plugin::PluginContext m_Context;
    f32 m_ElapsedTime = 0.0f;
};

ENJIN_IMPLEMENT_PLUGIN(ExamplePlugin)
