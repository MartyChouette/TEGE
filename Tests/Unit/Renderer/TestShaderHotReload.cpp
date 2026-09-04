// Hot-reloading a shader must actually use the shader you edited.
//
// Every effect renderer keeps its baked SPIR-V as a fallback and loads it in
// CreatePipeline. ReloadShaders compiles the edited GLSL, assigns it to
// m_VertexShader / m_FragmentShader, then calls CreatePipeline — which used to
// open by constructing a fresh VulkanShader and loading the BAKED data straight
// over the top. It then returned m_Pipeline != nullptr, which is true, so the
// caller logged "shaders reloaded" and nothing had changed.
//
// Syntax errors still surfaced, because the GLSL compile runs first and returns
// early on failure. That is what made the feature feel alive while every
// semantic edit to grass, trees, shrubs, particles, weather, fluid and sprites
// was being silently discarded.
//
// No test here can create a VkDevice, so these assert the source invariant that
// the fix establishes: a baked-SPIR-V load only ever writes an EMPTY slot, and a
// reload rebuilds against the pass the renderer is currently targeted at. Both
// fail against the original code.
#include "EnjinTest.h"

#include <fstream>
#include <filesystem>
#include <string>
#include <vector>

namespace {

namespace fs = std::filesystem;

// Tests/Fixtures -> Tests -> repo root.
fs::path EffectsSourceDir() {
    return fs::path(ENJIN_TEST_FIXTURES_DIR)
        .parent_path().parent_path() / "Engine" / "src" / "Effects";
}

const char* kRenderers[] = {
    "GrassRenderer", "TreeRenderer", "ShrubRenderer", "ParticleRenderer",
    "WeatherRenderer", "FluidRenderer", "SpriteBatchRenderer",
};

std::vector<std::string> ReadLines(const fs::path& p) {
    std::vector<std::string> lines;
    std::ifstream f(p);
    for (std::string l; std::getline(f, l); ) lines.push_back(l);
    return lines;
}

std::string Trim(const std::string& s) {
    const size_t a = s.find_first_not_of(" \t\r");
    if (a == std::string::npos) return "";
    return s.substr(a, s.find_last_not_of(" \t\r") - a + 1);
}

// "m_LitVertexShader = std::make_unique<..." -> "m_LitVertexShader"
std::string ShaderMemberAssignedOn(const std::string& line) {
    const std::string t = Trim(line);
    if (t.rfind("m_", 0) != 0) return "";
    const size_t eq = t.find(" = std::make_unique<Renderer::VulkanShader>");
    if (eq == std::string::npos) return "";
    const std::string name = t.substr(0, eq);
    return name.find("Shader") != std::string::npos ? name : "";
}

} // namespace

ENJIN_TEST(ShaderHotReload, BakedShaderIsOnlyLoadedIntoAnEmptySlot) {
    // Arrange: every site in the effect renderers that installs a shader object.
    for (const char* name : kRenderers) {
        const fs::path src = EffectsSourceDir() / (std::string(name) + ".cpp");
        const std::vector<std::string> lines = ReadLines(src);
        ENJIN_ASSERT_TRUE(!lines.empty());

        int checked = 0;
        for (size_t i = 0; i < lines.size(); ++i) {
            const std::string member = ShaderMemberAssignedOn(lines[i]);
            if (member.empty()) continue;

            // Act: look back for the guard that must dominate this assignment.
            // ReloadShaders moves its freshly compiled shader in via
            // std::move on a temp, so only the baked-load sites match above.
            const std::string guard = "if (!" + member + ")";
            bool guarded = false;
            for (size_t back = 1; back <= 8 && back <= i; ++back) {
                if (Trim(lines[i - back]).find(guard) != std::string::npos) {
                    guarded = true;
                    break;
                }
            }

            // Assert: unguarded, this clobbers whatever ReloadShaders just
            // compiled and the hot reload becomes a no-op that reports success.
            ENJIN_EXPECT_TRUE(guarded);
            ++checked;
        }

        // A renderer with no matches would pass vacuously.
        ENJIN_EXPECT_TRUE(checked > 0);
    }
}

ENJIN_TEST(ShaderHotReload, ReloadRebuildsAgainstThePassInUse) {
    // Arrange / Act: ReloadShaders used to call the swapchain-only
    // CreatePipeline unconditionally. When the editor has retargeted a renderer
    // at its offscreen pass (one colour attachment, not the swapchain's two),
    // that rebuilds the pipeline for the wrong pass — VUID-07609.
    for (const char* name : kRenderers) {
        const fs::path src = EffectsSourceDir() / (std::string(name) + ".cpp");
        std::ifstream f(src);
        const std::string text((std::istreambuf_iterator<char>(f)), {});
        ENJIN_ASSERT_TRUE(!text.empty());

        const size_t reload = text.find("::ReloadShaders(");
        ENJIN_ASSERT_TRUE(reload != std::string::npos);
        const std::string body = text.substr(reload);

        // Assert: the rebuild is conditional on the pass last built against.
        ENJIN_EXPECT_TRUE(body.find("m_LastRenderPass != VK_NULL_HANDLE") != std::string::npos);
        ENJIN_EXPECT_TRUE(body.find("CreatePipelineWithPass(m_LastRenderPass") != std::string::npos);
    }
}

ENJIN_TEST(ShaderHotReload, AFailedBakedLoadLeavesTheSlotEmpty) {
    // Arrange / Act: the guard above skips a non-empty slot, so a shader object
    // that was constructed but failed to load must not be left behind — it would
    // suppress the retry on every later call and the renderer would never draw.
    for (const char* name : kRenderers) {
        const fs::path src = EffectsSourceDir() / (std::string(name) + ".cpp");
        const std::vector<std::string> lines = ReadLines(src);
        ENJIN_ASSERT_TRUE(!lines.empty());

        for (size_t i = 0; i < lines.size(); ++i) {
            const std::string member = ShaderMemberAssignedOn(lines[i]);
            if (member.empty()) continue;

            // The failure branch runs within ~12 lines of the assignment.
            bool resets = false;
            for (size_t f2 = 1; f2 <= 12 && i + f2 < lines.size(); ++f2) {
                if (Trim(lines[i + f2]) == member + ".reset();") { resets = true; break; }
            }

            // Assert
            ENJIN_EXPECT_TRUE(resets);
        }
    }
}

ENJIN_TEST_MAIN()
