// The Light Cookie Creator.
//
// A cookie (gobo) is a shape in a light's projection: window mullions, venetian
// blinds, leaf dapple. This panel is the authoring tool for them, which is what
// makes the feature shipped rather than merely present: the golden rule is that
// a person with no AI and no network reaches every capability through the editor.
//
// The preview is drawn as filled rects rather than uploaded as a texture. That
// is the same approach PixelEditor takes for its canvas, and it avoids owning a
// GPU image whose lifetime would have to be managed against the frames in flight
// for something that only exists while a window is open.

#include "Enjin/Editor/EditorLayer.h"
#include "Enjin/Editor/EditorTheme.h"
#include "Enjin/Renderer/LightCookie.h"
#include "Enjin/ECS/Components/Light.h"
#include "Enjin/ECS/Components/Name.h"
#include "Enjin/Platform/Paths.h"
#include "Enjin/Logging/Log.h"

#include <imgui.h>

#include <filesystem>

namespace Enjin {
namespace Editor {

namespace {

// The preview is drawn on a fixed grid regardless of the cookie's resolution, so
// a 1024 cookie does not try to place a million rects. Sampling rather than
// averaging is deliberate: a gobo is mostly hard edges and averaging would show
// a softer pattern than the light will actually cast.
constexpr u32 kPreviewCells = 64;

ImU32 GreyToColor(u8 v) {
    // The swapchain is B8G8R8A8_SRGB and ImGui writes vertex colours straight
    // through, so a value packed here is treated as linear and comes out about
    // three times lighter than authored. Convert to linear first, the same way
    // the creative-mode surface does.
    const f32 s = static_cast<f32>(v) / 255.0f;
    const f32 lin = (s <= 0.04045f) ? (s / 12.92f)
                                    : std::pow((s + 0.055f) / 1.055f, 2.4f);
    const u8 c = static_cast<u8>(lin * 255.0f + 0.5f);
    return IM_COL32(c, c, c, 255);
}

} // namespace

void EditorLayer::DrawCookiePreview(const std::vector<u8>& pixels, u32 res, f32 sizePx) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 origin = ImGui::GetCursorScreenPos();

    if (pixels.empty() || res == 0) {
        dl->AddRectFilled(origin, ImVec2(origin.x + sizePx, origin.y + sizePx),
                          IM_COL32(30, 30, 34, 255));
        ImGui::Dummy(ImVec2(sizePx, sizePx));
        return;
    }

    const u32 cells = kPreviewCells < res ? kPreviewCells : res;
    const f32 cell = sizePx / static_cast<f32>(cells);

    for (u32 y = 0; y < cells; ++y) {
        // Nearest-sample from the full-resolution cookie.
        const u32 sy = (y * res) / cells;
        for (u32 x = 0; x < cells; ++x) {
            const u32 sx = (x * res) / cells;
            const u8 v = pixels[static_cast<usize>(sy) * res + sx];
            const ImVec2 a(origin.x + static_cast<f32>(x) * cell,
                           origin.y + static_cast<f32>(y) * cell);
            // +1 on the far corner: without it the rects leave hairline gaps at
            // fractional cell sizes and the preview looks like a mesh.
            const ImVec2 b(a.x + cell + 1.0f, a.y + cell + 1.0f);
            dl->AddRectFilled(a, b, GreyToColor(v));
        }
    }
    dl->AddRect(origin, ImVec2(origin.x + sizePx, origin.y + sizePx),
                Theme::SwatchBorder);
    ImGui::Dummy(ImVec2(sizePx, sizePx));
}

void EditorLayer::DrawCookieCreatorWindow() {
    if (!m_ShowCookieCreator) return;

    const f32 s = ImGui::GetIO().FontGlobalScale;
    OpenToolPanel(720.0f, 620.0f);
    if (!ImGui::Begin("Light Cookie Creator", &m_ShowCookieCreator)) {
        ImGui::End();
        return;
    }

    ImGui::TextWrapped("A cookie is a shape in a light's projection: window bars, blinds, "
                       "leaves. Build one here, then apply it to a spot light.");
    ImGui::Separator();

    // --- Pattern picker, by eye rather than by name. Thumbnails are generated
    // --- once from default params; they never change, so they are cached.
    static std::vector<std::vector<u8>> s_Thumbs;
    if (s_Thumbs.empty()) {
        s_Thumbs.resize(Renderer::kCookiePatternCount);
        for (u32 i = 0; i < Renderer::kCookiePatternCount; ++i) {
            Renderer::CookieParams tp;
            tp.pattern = static_cast<Renderer::CookiePattern>(i);
            tp.resolution = 32;
            Renderer::GenerateCookie(tp, s_Thumbs[i]);
        }
    }

    ImGui::Text("Pattern");
    const f32 thumb = 56.0f * s;
    for (u32 i = 0; i < Renderer::kCookiePatternCount; ++i) {
        const auto pattern = static_cast<Renderer::CookiePattern>(i);
        if (i % 4 != 0) ImGui::SameLine();
        ImGui::BeginGroup();
        ImGui::PushID(static_cast<int>(i));

        const bool selected = (m_CookieDraft.pattern == pattern);
        const ImVec2 cursor = ImGui::GetCursorScreenPos();
        DrawCookiePreview(s_Thumbs[i], 32, thumb);
        if (selected) {
            ImGui::GetWindowDrawList()->AddRect(
                cursor, ImVec2(cursor.x + thumb, cursor.y + thumb),
                Theme::SwatchSelected, 0.0f, 0, 2.0f * s);
        }
        // An invisible button over the thumbnail, so the picture is the control.
        ImGui::SetCursorScreenPos(cursor);
        if (ImGui::InvisibleButton("pick", ImVec2(thumb, thumb))) {
            m_CookieDraft.pattern = pattern;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", Renderer::CookiePatternName(pattern));
        }
        ImGui::TextUnformatted(Renderer::CookiePatternName(pattern));
        ImGui::PopID();
        ImGui::EndGroup();
    }

    ImGui::Separator();

    // --- Controls on the left, live preview on the right.
    ImGui::BeginGroup();
    ImGui::PushItemWidth(220.0f * s);

    int res = static_cast<int>(m_CookieDraft.resolution);
    if (ImGui::SliderInt("Resolution", &res, static_cast<int>(Renderer::kCookieResolutionMin),
                         static_cast<int>(Renderer::kCookieResolutionMax))) {
        m_CookieDraft.resolution = static_cast<u32>(res);
    }
    ImGui::SliderFloat("Columns", &m_CookieDraft.columns, 0.25f, 16.0f, "%.2f");
    ImGui::SliderFloat("Rows", &m_CookieDraft.rows, 0.25f, 16.0f, "%.2f");
    ImGui::SliderFloat("Bar Width", &m_CookieDraft.barWidth, 0.0f, 0.49f, "%.3f");
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Thickness of the dark structure. For the organic patterns "
                          "this is how much of the light gets blocked.");
    }
    ImGui::SliderFloat("Softness", &m_CookieDraft.softness, 0.0f, 0.5f, "%.3f");
    ImGui::SliderFloat("Rotation", &m_CookieDraft.rotation, 0.0f, 360.0f, "%.1f deg");
    ImGui::SliderFloat("Contrast", &m_CookieDraft.contrast, 0.0f, 4.0f, "%.2f");
    ImGui::SliderFloat("Brightness", &m_CookieDraft.brightness, -1.0f, 1.0f, "%.2f");
    ImGui::SliderFloat("Vignette", &m_CookieDraft.vignette, 0.0f, 1.0f, "%.2f");
    ImGui::Checkbox("Invert", &m_CookieDraft.invert);

    int seed = static_cast<int>(m_CookieDraft.seed);
    if (ImGui::InputInt("Seed", &seed)) {
        m_CookieDraft.seed = static_cast<u32>(seed < 0 ? 0 : seed);
    }
    ImGui::SameLine();
    if (ImGui::Button("Roll")) {
        m_CookieDraft.seed = m_CookieDraft.seed * 1664525u + 1013904223u;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Only the organic patterns (dapple, caustics) use the seed.");
    }

    ImGui::PopItemWidth();
    ImGui::EndGroup();

    ImGui::SameLine();

    ImGui::BeginGroup();
    // Regenerate only when the recipe actually changed. Clamping first matters:
    // the comparison is against the clamped draft, so a slider parked at a
    // clamped limit does not regenerate every frame.
    Renderer::CookieParams wanted = m_CookieDraft;
    Renderer::ClampCookieParams(wanted);
    const bool changed =
        !m_CookiePreviewValid ||
        wanted.pattern != m_CookiePreviewOf.pattern ||
        wanted.resolution != m_CookiePreviewOf.resolution ||
        wanted.columns != m_CookiePreviewOf.columns ||
        wanted.rows != m_CookiePreviewOf.rows ||
        wanted.barWidth != m_CookiePreviewOf.barWidth ||
        wanted.softness != m_CookiePreviewOf.softness ||
        wanted.rotation != m_CookiePreviewOf.rotation ||
        wanted.contrast != m_CookiePreviewOf.contrast ||
        wanted.brightness != m_CookiePreviewOf.brightness ||
        wanted.vignette != m_CookiePreviewOf.vignette ||
        wanted.invert != m_CookiePreviewOf.invert ||
        wanted.seed != m_CookiePreviewOf.seed;
    if (changed) {
        Renderer::GenerateCookie(wanted, m_CookiePreview);
        m_CookiePreviewOf = wanted;
        m_CookiePreviewValid = true;
    }

    ImGui::Text("Preview");
    DrawCookiePreview(m_CookiePreview, wanted.resolution, 320.0f * s);
    ImGui::TextDisabled("%u x %u", wanted.resolution, wanted.resolution);
    ImGui::EndGroup();

    ImGui::Separator();

    // --- Apply to the selected light.
    ECS::LightComponent* light = nullptr;
    const char* lightName = "";
    if (m_World && GetSelectedEntity() != ECS::INVALID_ENTITY &&
        m_World->IsValid(GetSelectedEntity())) {
        light = m_World->GetComponent<ECS::LightComponent>(GetSelectedEntity());
        if (const auto* n = m_World->GetComponent<ECS::NameComponent>(GetSelectedEntity())) {
            lightName = n->name.c_str();
        }
    }

    if (!light) {
        ImGui::TextDisabled("Select a light in the scene to apply this cookie to it.");
    } else {
        if (ImGui::Button("Apply to selected light")) {
            light->cookie = wanted;
            light->cookieEnabled = true;
            // The recipe is the source of truth; a stale baked path would win
            // over the cookie just authored.
            light->cookieTexturePath.clear();
            m_CookieStatus = std::string("Applied to '") + lightName + "'.";
        }
        ImGui::SameLine();
        if (ImGui::Button("Load from selected light")) {
            m_CookieDraft = light->cookie;
            m_CookiePreviewValid = false;
            m_CookieStatus = "Loaded the cookie from the selected light.";
        }
        if (light->type != ECS::LightType::Spot) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.35f, 1.0f),
                               "This light is not a Spot; cookies need a cone to project through.");
        }
    }

    // --- Save a copy to the project, for hand-editing or sharing.
    ImGui::Spacing();
    ImGui::PushItemWidth(220.0f * s);
    char nameBuf[128];
    std::snprintf(nameBuf, sizeof(nameBuf), "%s", m_CookieSaveName.c_str());
    if (ImGui::InputText("File name", nameBuf, sizeof(nameBuf))) {
        m_CookieSaveName = nameBuf;
    }
    ImGui::PopItemWidth();
    ImGui::SameLine();

    const bool haveProject = !m_SceneManager.GetProjectPath().empty();
    if (!haveProject) ImGui::BeginDisabled();
    if (ImGui::Button("Save PNG to project")) {
        // Saved under assets/cookies/ so the asset browser finds it and a build
        // packs it, rather than into whatever the process CWD happens to be --
        // which in the editor is the exe directory, not the project.
        const std::filesystem::path manifest(m_SceneManager.GetProjectPath());
        const std::filesystem::path root = manifest.parent_path();
        std::string leaf = m_CookieSaveName.empty() ? std::string("cookie") : m_CookieSaveName;
        if (!Platform::IsSafeFileName(leaf)) {
            m_CookieStatus = "That file name is not allowed.";
        } else {
            std::error_code ec;
            std::filesystem::create_directories(root / "assets" / "cookies", ec);
            const std::filesystem::path out = root / "assets" / "cookies" / (leaf + ".png");
            if (Renderer::WriteCookiePNG(out.string(), wanted, m_CookiePreview)) {
                m_CookieStatus = "Saved " + out.string();
                ENJIN_LOG_INFO(Editor, "Cookie written to '%s'", out.string().c_str());
            } else {
                m_CookieStatus = "Could not write " + out.string();
            }
        }
    }
    if (!haveProject) {
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::TextDisabled("(open a project to save)");
    }

    if (!m_CookieStatus.empty()) {
        ImGui::TextWrapped("%s", m_CookieStatus.c_str());
    }

    ImGui::End();
}

} // namespace Editor
} // namespace Enjin
