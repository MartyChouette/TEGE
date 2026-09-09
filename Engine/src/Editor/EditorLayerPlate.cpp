// Baking a pre-rendered background.
//
// This is the half of the technique that decides whether anyone can use it. The
// runtime can display a plate and its depth; without a bake button, producing
// one means exporting a depth buffer out of a DCC tool and matching its camera
// to the engine's by hand, which nobody is going to do twice. The engine
// already renders the scene from this exact camera, so it can hand you both
// images -- and because it is the same camera, the plate lines up by
// construction rather than by measurement.
//
// The workflow it exists to serve: build a room, point a camera at it, bake,
// then delete or hide the geometry. What remains costs a fullscreen quad to
// draw and can be as detailed as an offline render can be.

#include "Enjin/Editor/EditorLayer.h"
#include "Enjin/Editor/EditorWidgets.h"
#include "Enjin/ECS/Components/PreRenderedBackground.h"
#include "Enjin/ECS/Components/Camera.h"
#include "Enjin/ECS/Systems/RenderSystem.h"
#include "Enjin/Renderer/DepthPlate.h"
#include "Enjin/Renderer/RenderTarget.h"
#include "Enjin/Platform/Paths.h"
#include "Enjin/Logging/Log.h"

#include <imgui.h>

#include <algorithm>
#include <filesystem>
#include <limits>
#include <vector>

extern "C" {
    int stbi_write_png(const char* filename, int w, int h, int comp,
                       const void* data, int stride_in_bytes);
}

namespace Enjin {
namespace Editor {

bool EditorLayer::BakeBackgroundPlate(std::string& outStatus) {
    if (!m_GameViewRenderTarget || !m_RenderSystem) {
        outStatus = "No game view to bake from.";
        return false;
    }
    if (m_SceneManager.GetProjectPath().empty()) {
        outStatus = "Open a project first: a plate is saved into it.";
        return false;
    }

    auto* camera = m_RenderSystem->GetCamera();
    if (!camera) {
        outStatus = "No active camera.";
        return false;
    }

    const u32 w = m_GameViewRenderTarget->GetWidth();
    const u32 h = m_GameViewRenderTarget->GetHeight();

    // Colour comes from the FINAL target and depth from the SCENE target, and
    // they are two different images on purpose. The game view target holds the
    // post-processed picture -- which is what a plate should show, tonemapping
    // and all -- but post-processing is a fullscreen blit that never writes
    // depth, so its depth attachment is whatever it was cleared to. The scene
    // target is where the geometry pass actually ran.
    std::vector<u8> colour = m_GameViewRenderTarget->CaptureToPixels();
    std::vector<f32> depth = m_SceneRenderTarget ? m_SceneRenderTarget->CaptureDepthToPixels()
                                                 : std::vector<f32>{};

    if (colour.empty() || w == 0 || h == 0) {
        outStatus = "Readback failed (is the Game View drawing?).";
        return false;
    }
    if (depth.empty()) {
        outStatus = "No scene depth to read (the scene target is not rendering).";
        return false;
    }
    if (!m_SceneRenderTarget || m_SceneRenderTarget->GetWidth() != w ||
        m_SceneRenderTarget->GetHeight() != h) {
        // They are normally the same size; if they ever diverge the plate and
        // its depth would describe different framings, which is worse than
        // refusing.
        outStatus = "Scene and game view sizes differ; cannot pair colour with depth.";
        return false;
    }
    if (depth.size() != static_cast<usize>(w) * h) {
        outStatus = "Depth readback size did not match the view.";
        return false;
    }

    // The projection the depth buffer was written with. Everything below is
    // solved against THIS, so the plate cannot disagree with the camera it came
    // from -- which is the failure that makes hand-exported depth plates so
    // miserable to line up.
    const Math::Matrix4 proj = camera->GetProjectionMatrix();

    // The plate's range is measured from the scene rather than assumed. Using
    // the camera's near and far instead would spend almost all 24 bits of
    // precision on empty space beyond the room: a 0.1-to-1000 range puts a
    // whole building inside a handful of values.
    f32 nearest = std::numeric_limits<f32>::max();
    f32 furthest = 0.0f;
    {
        // A first pass purely to find what the shot actually contains. Pixels
        // at the far plane are sky, not geometry, and including them would
        // stretch the range to the horizon for the sake of nothing.
        Renderer::PlateDepthMapping probe;
        probe.inverse = true;
        probe.a = 0.0f;
        probe.b = 0.0f;
        const Renderer::PlateDepthMapping full =
            Renderer::SolvePlateDepthMapping(proj, camera->GetNearPlane(), camera->GetFarPlane());
        probe = full;

        for (f32 d : depth) {
            if (!(d < 1.0f)) continue;              // cleared / sky
            const f32 dist = Renderer::InvertPlateDepth(probe, d);
            if (!(dist > 0.0f)) continue;
            nearest = std::min(nearest, dist);
            furthest = std::max(furthest, dist);
        }
    }

    if (!(furthest > 0.0f) || !(nearest < furthest)) {
        outStatus = "Nothing in view to bake (the shot is empty sky).";
        return false;
    }

    // A little air at both ends. A character standing exactly on the nearest
    // baked surface would otherwise sit on the boundary of the encoding, where
    // rounding decides whether it is in front or behind.
    const f32 pad = (furthest - nearest) * 0.02f + 0.01f;
    const f32 plateNear = std::max(0.001f, nearest - pad);
    const f32 plateFar = furthest + pad;

    const Renderer::PlateDepthMapping mapping =
        Renderer::SolvePlateDepthMapping(proj, camera->GetNearPlane(), camera->GetFarPlane());

    // Pack the depth: distance -> [0,1] over the plate's range -> 24 bits split
    // across R, G and B. Alpha carries whether the pixel had geometry at all,
    // which the runtime does not read today but makes the image inspectable.
    std::vector<u8> depthImage(static_cast<usize>(w) * h * 4, 0);
    for (usize i = 0; i < depth.size(); ++i) {
        const f32 raw = depth[i];
        u32 packed;
        u8 alpha;
        if (!(raw < 1.0f)) {
            // Sky. Pushed to the very back so live geometry always wins there,
            // which is what makes a doorway onto nothing still work.
            packed = 0xFFFFFFu;
            alpha = 0;
        } else {
            const f32 dist = Renderer::InvertPlateDepth(mapping, raw);
            packed = Renderer::PackPlateDepth24(
                Renderer::NormalizePlateDistance(dist, plateNear, plateFar));
            alpha = 255;
        }
        depthImage[i * 4 + 0] = static_cast<u8>((packed >> 16) & 0xFF);
        depthImage[i * 4 + 1] = static_cast<u8>((packed >> 8) & 0xFF);
        depthImage[i * 4 + 2] = static_cast<u8>(packed & 0xFF);
        depthImage[i * 4 + 3] = alpha;
    }

    std::string leaf = m_PlateBakeName.empty() ? std::string("plate") : m_PlateBakeName;
    if (!Platform::IsSafeFileName(leaf)) {
        outStatus = "That file name is not allowed.";
        return false;
    }

    namespace fs = std::filesystem;
    const fs::path root = fs::path(m_SceneManager.GetProjectPath()).parent_path();
    const fs::path dir = root / "assets" / "plates";
    std::error_code ec;
    fs::create_directories(dir, ec);

    const fs::path colourOut = dir / (leaf + ".png");
    const fs::path depthOut = dir / (leaf + "_depth.png");

    const int stride = static_cast<int>(w) * 4;
    if (!stbi_write_png(colourOut.string().c_str(), static_cast<int>(w), static_cast<int>(h), 4,
                        colour.data(), stride)) {
        outStatus = "Could not write " + colourOut.string();
        return false;
    }
    if (!stbi_write_png(depthOut.string().c_str(), static_cast<int>(w), static_cast<int>(h), 4,
                        depthImage.data(), stride)) {
        outStatus = "Could not write " + depthOut.string();
        return false;
    }

    // Attach it to the camera that produced it. Doing this by hand means typing
    // two paths and two distances that have to match the bake exactly, and the
    // distances are the half nobody would guess right.
    if (m_World) {
        ECS::Entity camEntity = ECS::Entity{};
        bool found = false;
        for (ECS::Entity e : m_World->GetEntitiesWithComponent<ECS::CameraComponent>()) {
            const auto* c = m_World->GetComponent<ECS::CameraComponent>(e);
            if (c && c->isActive) { camEntity = e; found = true; break; }
        }
        if (found) {
            if (!m_World->HasComponent<ECS::PreRenderedBackgroundComponent>(camEntity)) {
                m_World->AddComponent<ECS::PreRenderedBackgroundComponent>(
                    camEntity, ECS::PreRenderedBackgroundComponent{});
            }
            auto* bg = m_World->GetComponent<ECS::PreRenderedBackgroundComponent>(camEntity);
            if (bg) {
                bg->platePath = "assets/plates/" + leaf + ".png";
                bg->depthPath = "assets/plates/" + leaf + "_depth.png";
                bg->depthNear = plateNear;
                bg->depthFar = plateFar;
                bg->enabled = true;
                // Left INVISIBLE on purpose. The scene it was baked from is
                // still standing, so showing the plate immediately would draw a
                // picture of the room over the room and look like nothing
                // happened. Hide the geometry, then tick Visible.
                bg->visible = false;
            }
        }
    }

    // Plate textures are cached by path, and a re-bake writes the same paths.
    // Without dropping the cache the editor would keep showing the previous
    // bake and the button would look broken.
    m_RenderSystem->ClearPreRenderedPlates();

    outStatus = "Baked " + colourOut.filename().string() + " + depth (" +
                std::to_string(static_cast<int>(plateNear)) + " to " +
                std::to_string(static_cast<int>(plateFar)) + " units). " +
                "Hide the geometry, then tick Visible on the camera.";
    ENJIN_LOG_INFO(Editor, "Baked background plate '%s' (%ux%u, range %.3f..%.3f)",
                   colourOut.string().c_str(), w, h, plateNear, plateFar);
    return true;
}

void EditorLayer::DrawBackgroundPlateBakerWindow() {
    if (!m_ShowPlateBaker) return;

    ImGui::SetNextWindowSize(ImVec2(430.0f, 0.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Bake Background Plate", &m_ShowPlateBaker)) {
        ImGui::End();
        return;
    }

    ImGui::TextWrapped("Render this shot once, keep the picture, and throw the geometry away. "
                       "The plate carries the depth it was rendered at, so characters walk "
                       "BEHIND what it shows -- the trick Resident Evil and Final Fantasy VII "
                       "were built on.");
    ImGui::Spacing();
    ImGui::TextWrapped("The plate is bound to the camera it was baked from, because it is only "
                       "correct from there. Point another camera at another room and bake again.");
    ImGui::Spacing();

    ImGui::PushItemWidth(220.0f);
    char nameBuf[64];
    std::snprintf(nameBuf, sizeof(nameBuf), "%s", m_PlateBakeName.c_str());
    if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf))) m_PlateBakeName = nameBuf;
    ImGui::PopItemWidth();
    ImGui::TextDisabled("Saved to assets/plates/<name>.png and <name>_depth.png");

    ImGui::Spacing();
    if (ImGui::Button("Bake from the Game View", ImVec2(220.0f, 0.0f))) {
        std::string status;
        const bool ok = BakeBackgroundPlate(status);
        m_PlateBakeStatus = status;
        ShowNotification(status, ok ? NotificationType::Success : NotificationType::Error);
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Captures the Game View's colour AND depth at its current resolution.\n"
                          "Render it as large and as slowly as you like first -- none of that\n"
                          "cost is paid again when the plate is displayed.");
    }

    if (!m_PlateBakeStatus.empty()) {
        ImGui::Spacing();
        ImGui::TextWrapped("%s", m_PlateBakeStatus.c_str());
    }

    ImGui::Spacing();
    ImGui::SeparatorText("After baking");
    ImGui::TextWrapped("1. Hide or delete the geometry the plate replaced.\n"
                       "2. Tick Visible on the camera's Pre-Rendered Background.\n"
                       "3. Walk a character through it and check the occlusion.");

    ImGui::End();
}

} // namespace Editor
} // namespace Enjin
