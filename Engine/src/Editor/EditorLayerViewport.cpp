#include "Enjin/Editor/EditorLayer.h"
#include "Enjin/Editor/InspectorUndo.h"
#include "Enjin/Editor/ScenePicker.h"
#include "Enjin/Core/Version.h"
#include <imgui_internal.h>
#include <GLFW/glfw3.h>
#include <chrono>
#include "Enjin/Logging/Log.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Mesh.h"
#include "Enjin/ECS/Components/Material.h"
#include "Enjin/ECS/Components/Light.h"
#include "Enjin/ECS/Components/Name.h"
#include "Enjin/ECS/Components/Camera.h"
#include "Enjin/ECS/Components/Notes.h"
#include "Enjin/ECS/Components/Controllers/CharacterController.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/ECS/Components/VisualScript.h"
#include "Enjin/AI/BehaviorTree.h"
#include "Enjin/Gameplay/QuestFlow.h"
#include "Enjin/Gameplay/TieredSaveSystem.h"
#include "Enjin/Editor/PlayModeDiff.h"
#include "Enjin/Physics/PhysicsBackendType.h"
#include "Enjin/Physics/PhysicsBackendFactory.h"
#include "Enjin/Physics/PhysicsTypes2D.h"
#include "Enjin/ECS/Components/WeatherZone.h"
#include "Enjin/ECS/Components/WaterVolume.h"
#include "Enjin/ECS/Components/GrassVolume.h"
#include "Enjin/ECS/Components/ShrubVolume.h"
#include "Enjin/ECS/Components/TreeVolume.h"
#include "Enjin/ECS/Components/Terrain.h"
#include "Enjin/ECS/Components/Terrain2D.h"
#include "Enjin/ECS/Components/Vegetation.h"
#include "Enjin/ECS/Components/CameraTrigger.h"
#include "Enjin/ECS/Components/TemperatureZone.h"
#include "Enjin/ECS/Components/GravityZone.h"
#include "Enjin/ECS/Components/PostProcessVolume.h"
#include "Enjin/ECS/Components/FluidVolume.h"
#include "Enjin/ECS/Components/Text.h"
#include "Enjin/ECS/Components/IKComponents.h"
#include "Enjin/ECS/Components/Flower.h"
#ifndef _WIN32
#include <unistd.h>
#endif
#include "Enjin/ECS/Components/LOD.h"
#include "Enjin/ECS/Components/Script.h"
#include "Enjin/ECS/Components/Tween.h"
#include "Enjin/ECS/Components/Hierarchy.h"
#include "Enjin/ECS/Components/Skeleton.h"
#include "Enjin/Renderer/MeshSimplifier.h"
#include "Enjin/Renderer/Skybox.h"
#include "Enjin/ECS/Systems/RenderSystem.h"
#include "Enjin/Renderer/RayTracing/RTShadows.h"
#include "Enjin/Renderer/RayTracing/RTReflections.h"
#include "Enjin/Renderer/RayTracing/RTAmbientOcclusion.h"
#include "Enjin/Renderer/RayTracing/RTGlobalIllumination.h"
#include "Enjin/Renderer/RayTracing/PathTracer.h"
#include "Enjin/Renderer/RayTracing/SVGFDenoiser.h"
#include "Enjin/Renderer/RayTracing/OIDNDenoiser.h"
#include "Enjin/Renderer/RayTracing/RTCompositor.h"
#include "Enjin/Renderer/RayTracing/AccelerationStructureManager.h"
#include "Enjin/Renderer/SHLightProbe.h"
#include "Enjin/Renderer/SDFScene.h"
#include "Enjin/Renderer/OITManager.h"
#include "Enjin/Effects/TreeRenderer.h"
#include "Enjin/Effects/Weather.h"
#include "Enjin/Assets/SceneImporter.h"
#include "Enjin/Assets/FontLibrary.h"
#include "Enjin/Assets/AssetLibrary.h"
#include "Enjin/Assets/AssetMetadata.h"
#include "Enjin/Scene/SceneSerializer.h"
#include "Enjin/Renderer/MeshFactory.h"
#include "Enjin/Renderer/PostProcessing.h"
#include "Enjin/Platform/Input.h"
#include "Enjin/Platform/FileDialog.h"
#include "Enjin/Assets/Prefab.h"
#include "Enjin/Build/BuildPipeline.h"
#include "Enjin/Assets/DataAsset.h"
#include "Enjin/Plugin/PluginRepository.h"
#include "Enjin/Audio/AudioSystem.h"
#include "Enjin/Renderer/NormalMapGenerator.h"
#include "Enjin/Editor/SpriteContourTracer.h"
#include "Enjin/GUI/UICanvas.h"
#include "Enjin/GUI/UITemplates.h"
#include "Enjin/GUI/DialogueImportExport.h"
#include "Enjin/Assets/SWFLoader.h"
#include "Enjin/Effects/CurlNoiseSystem.h"
#include "Enjin/Scripting/ScriptBindings.h"
#include "Enjin/Scene/LevelStreaming.h"
#include "Enjin/Effects/VoronoiMeshFracture.h"
#include "Enjin/Effects/InteractiveWater.h"
#include "Enjin/Math/Math.h"
#include <stb_image.h>
#include <imgui.h>
#include <ImGuizmo.h>
#include <backends/imgui_impl_vulkan.h>
#include <vulkan/vulkan.h>
#include <sstream>
#include <fstream>
#include <filesystem>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
// Undefine Windows macros that collide with engine methods
#undef LoadImage
#undef CreateWindow
#undef min
#undef max
#else
#include <spawn.h>
#include <sys/wait.h>
#endif
#include <climits>
#include <cmath>
#include <algorithm>

namespace Enjin {
namespace Editor {

bool EditorLayer::SceneHasMouseLookController() const {
    if (!m_World) return false;
    if (!m_World->GetEntitiesWithComponent<ECS::FirstPersonController>().empty()) return true;
    if (!m_World->GetEntitiesWithComponent<ECS::ThirdPersonController>().empty()) return true;
    return false;
}


ImVec2 EditorLayer::ComputeAspectConstrainedSize(f32 availW, f32 availH, f32 aspect) {
    if (aspect <= 0.0f) return ImVec2(availW, availH);  // Free
    f32 panelAspect = availW / availH;
    if (panelAspect > aspect) {
        // Panel is wider than target — pillarbox (constrain width)
        return ImVec2(availH * aspect, availH);
    } else {
        // Panel is taller than target — letterbox (constrain height)
        return ImVec2(availW, availW / aspect);
    }
}

void EditorLayer::DrawViewportPanel() {
    bool panelOpen = true;
    // NoScrollbar/NoScrollWithMouse: the panel hosts an aspect-constrained render-target image.
    // If a scrollbar is ever allowed to appear it steals ~16px from the content region, which
    // shrinks the image, which removes the overflow, which removes the scrollbar — an every-frame
    // oscillation that resizes the render target each frame and makes the whole view visibly shake
    // (and thrashes TAA/render-target allocation). The wheel is the camera zoom, not panel scroll.
    ImGui::Begin("Scene", &panelOpen, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    if (!panelOpen) {
        SetPanelVisibility(EditorPanel::Viewport, false);
    }

    // Scene view mode buttons (Blender-style viewport shading)
    {
        auto modeBtn = [&](const char* label, const char* tooltip, SceneViewMode mode) {
            bool active = (m_SceneViewMode == mode);
            if (active) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
            if (ImGui::SmallButton(label)) {
                m_SceneViewMode = mode;
                if (m_Announcer.enabled) {
                    m_Announcer.Announce(std::string("View: ") + label, Accessibility::AnnouncePriority::Low);
                }
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tooltip);
            if (active) ImGui::PopStyleColor();
            ImGui::SameLine();
        };
        modeBtn("Wire",   "Wireframe only",                                    SceneViewMode::Wireframe);
        modeBtn("Solid",  "Flat shading, no lighting",                          SceneViewMode::Solid);
        modeBtn("Lit",    "Lighting, no shadows",                               SceneViewMode::Lit);
        modeBtn("Shaded", "Lighting + shadows (matches game view)",             SceneViewMode::LitShadows);
        modeBtn("Full",   "Shadows + outlines (post-processing in Game View)",  SceneViewMode::Full);
        ImGui::SameLine(0, 20);
    }

    // Aspect ratio dropdown
    {
        int current = static_cast<int>(m_SceneViewAspect);
        ImGui::SetNextItemWidth(90.0f);
        if (ImGui::Combo("##SceneAspect", &current, AspectRatioLabels, static_cast<int>(AspectRatio::Count))) {
            m_SceneViewAspect = static_cast<AspectRatio>(current);
        }
    }

    // Audio meter strip (shows per-bus VU levels during play mode)
    if (m_PlayMode.IsPlaying()) {
        DrawAudioMeterStrip();
    }

    // Update desired render target size from available content region
    ImVec2 availSize = ImGui::GetContentRegionAvail();
    if (availSize.x > 0 && availSize.y > 0) {
        m_EditorViewportWidth = static_cast<u32>(availSize.x);
        m_EditorViewportHeight = static_cast<u32>(availSize.y);
    }

    // Compute constrained image size
    f32 aspect = AspectRatioValues[static_cast<int>(m_SceneViewAspect)];
    ImVec2 imgSize = ComputeAspectConstrainedSize(
        static_cast<f32>(m_EditorViewportWidth),
        static_cast<f32>(m_EditorViewportHeight), aspect);

    // Center the image within the available space
    f32 padX = (static_cast<f32>(m_EditorViewportWidth) - imgSize.x) * 0.5f;
    f32 padY = (static_cast<f32>(m_EditorViewportHeight) - imgSize.y) * 0.5f;
    if (padX > 0.0f || padY > 0.0f) {
        ImVec2 cursorPos = ImGui::GetCursorPos();
        ImGui::SetCursorPos(ImVec2(cursorPos.x + padX, cursorPos.y + padY));
    }

    // Display the editor viewport render target as an ImGui image
    if (m_EditorViewportRT && m_EditorViewportRT->IsValid()) {
        VkDescriptorSet texId = m_EditorViewportRT->GetImGuiTextureID();
        if (texId != VK_NULL_HANDLE) {
            ImGui::Image(static_cast<ImTextureID>(reinterpret_cast<uintptr_t>(texId)), imgSize);

            // Track image screen bounds for gizmo/picking/overlay coordinate mapping
            ImVec2 imgMin = ImGui::GetItemRectMin();
            ImVec2 imgMax = ImGui::GetItemRectMax();
            m_EditorViewportImageMinX = imgMin.x;
            m_EditorViewportImageMinY = imgMin.y;
            m_EditorViewportImageMaxX = imgMax.x;
            m_EditorViewportImageMaxY = imgMax.y;
            m_EditorViewportHovered = ImGui::IsWindowHovered() && ImGui::IsMouseHoveringRect(imgMin, imgMax);
            m_EditorViewportFocused = ImGui::IsWindowFocused();

            // Drop target: accept asset drags onto scene viewport
            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
                    std::string dropPath(static_cast<const char*>(payload->Data));
                    std::filesystem::path fp(dropPath);
                    std::string ext = fp.extension().string();
                    std::transform(ext.begin(), ext.end(), ext.begin(),
                        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

                    if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" ||
                        ext == ".tga" || ext == ".bmp" || ext == ".svg") {
                        // Pick the entity under the drop location and assign as base color texture
                        Math::Vector2 mousePos = Input::GetMousePosition();
                        f32 vpW = imgMax.x - imgMin.x;
                        f32 vpH = imgMax.y - imgMin.y;
                        ECS::Entity target = ECS::INVALID_ENTITY;
                        if (vpW > 0 && vpH > 0 && m_World && m_Camera) {
                            target = ScenePicker::PickEntity(
                                m_World, m_Camera,
                                mousePos.x - imgMin.x,
                                mousePos.y - imgMin.y,
                                vpW, vpH);
                        }

                        if (target != ECS::INVALID_ENTITY && m_World) {
                            // Assign to material (3D) or sprite (2D)
                            auto* mat = m_World->GetComponent<ECS::MaterialComponent>(target);
                            if (mat) {
                                mat->baseColorTexturePath = dropPath;
                                mat->baseColorTexture = -1;
                                mat->InvalidateTextureCache();
                                if (m_RenderSystem) m_RenderSystem->ClearFailedTexture(dropPath);
                                SelectEntity(target);
                                ShowNotification("Assigned texture to " +
                                    (m_World->HasComponent<ECS::NameComponent>(target)
                                        ? m_World->GetComponent<ECS::NameComponent>(target)->name
                                        : std::string("entity")),
                                    NotificationType::Info);
                            } else {
                                auto* spr = m_World->GetComponent<ECS::Sprite2DComponent>(target);
                                if (spr) {
                                    spr->texturePath = dropPath;
                                    SelectEntity(target);
                                    ShowNotification("Assigned texture to sprite",
                                        NotificationType::Info);
                                }
                            }
                        } else if (m_PrimarySelected != ECS::INVALID_ENTITY && m_World) {
                            // No entity under cursor — assign to currently selected entity
                            auto* mat = m_World->GetComponent<ECS::MaterialComponent>(m_PrimarySelected);
                            if (mat) {
                                mat->baseColorTexturePath = dropPath;
                                mat->baseColorTexture = -1;
                                mat->InvalidateTextureCache();
                                if (m_RenderSystem) m_RenderSystem->ClearFailedTexture(dropPath);
                            } else {
                                auto* spr = m_World->GetComponent<ECS::Sprite2DComponent>(m_PrimarySelected);
                                if (spr) spr->texturePath = dropPath;
                            }
                        }
                    } else if (ext == ".gltf" || ext == ".glb" || ext == ".fbx" ||
                               ext == ".obj" || ext == ".dae" || ext == ".3ds") {
                        ImportModel(dropPath);
                    } else if (ext == ".enjprefab") {
                        auto prefab = Assets::PrefabManager::Get().LoadPrefab(dropPath);
                        if (prefab) {
                            ECS::Entity root = Assets::PrefabManager::Get().Instantiate(m_World, *prefab);
                            if (root != ECS::INVALID_ENTITY) SelectEntity(root);
                        }
                    } else if (ext == ".enjin" || ext == ".json") {
                        OpenScene(dropPath);
                    }
                }
                ImGui::EndDragDropTarget();
            }
        }
    } else {
        // Fallback: dark rect when RT is not available
        ImVec2 pos = ImGui::GetCursorScreenPos();
        ImGui::GetWindowDrawList()->AddRectFilled(
            pos, ImVec2(pos.x + availSize.x, pos.y + availSize.y),
            IM_COL32(30, 30, 30, 255));
        m_EditorViewportHovered = false;
        m_EditorViewportFocused = false;
    }

    // --- Floating viewport toolbar (gizmo mode + space), top-left over the image ---
    // Drawn last so it sits above the image; vector icons via the draw list
    // (same pattern as the menu-bar transport buttons). Keys 1/2/3/4 still work.
    if (m_EditorViewportRT && m_EditorViewportRT->IsValid() &&
        m_EditorViewportImageMaxX > m_EditorViewportImageMinX) {
        const f32 btn = 26.0f;
        const f32 pad = 3.0f;
        ImVec2 origin(m_EditorViewportImageMinX + 8.0f, m_EditorViewportImageMinY + 8.0f);
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const ImVec4 accent = ImGui::GetStyleColorVec4(ImGuiCol_CheckMark);
        const ImU32 accentCol = ImGui::GetColorU32(accent);
        const ImU32 idleCol = ImGui::GetColorU32(ImGuiCol_Text);

        // Toolbar backing pill: 3 icon buttons + space toggle
        f32 spaceW = ImGui::CalcTextSize(m_GizmoSpace == GizmoSpace::World ? "World" : "Local").x + 14.0f;
        ImVec2 barMax(origin.x + pad * 2.0f + btn * 3.0f + pad * 2.0f + 6.0f + spaceW,
                      origin.y + btn + pad * 2.0f);
        dl->AddRectFilled(ImVec2(origin.x - pad, origin.y - pad), barMax,
                          ImGui::GetColorU32(ImGuiCol_WindowBg, 0.85f), 6.0f);
        dl->AddRect(ImVec2(origin.x - pad, origin.y - pad), barMax,
                    ImGui::GetColorU32(ImGuiCol_Border), 6.0f);
        m_ViewportToolbarMinX = origin.x - pad;
        m_ViewportToolbarMinY = origin.y - pad;
        m_ViewportToolbarMaxX = barMax.x;
        m_ViewportToolbarMaxY = barMax.y;

        // icon: 0 = translate cross-arrows, 1 = rotate circle, 2 = scale boxes
        auto gizmoButton = [&](const char* id, int icon, bool active, const char* tip) {
            if (active) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(accent.x, accent.y, accent.z, 0.28f));
            }
            bool clicked = ImGui::Button(id, ImVec2(btn, btn));
            if (active) ImGui::PopStyleColor();
            ImGui::SetItemTooltip("%s", tip);
            ImVec2 mn = ImGui::GetItemRectMin();
            ImVec2 mx = ImGui::GetItemRectMax();
            ImVec2 c((mn.x + mx.x) * 0.5f, (mn.y + mx.y) * 0.5f);
            const ImU32 col = active ? accentCol : idleCol;
            const f32 s = btn * 0.28f;
            if (icon == 0) {
                dl->AddLine(ImVec2(c.x - s, c.y), ImVec2(c.x + s, c.y), col, 1.5f);
                dl->AddLine(ImVec2(c.x, c.y - s), ImVec2(c.x, c.y + s), col, 1.5f);
                const f32 a = 3.0f;
                dl->AddTriangleFilled(ImVec2(c.x + s, c.y - a), ImVec2(c.x + s, c.y + a), ImVec2(c.x + s + a, c.y), col);
                dl->AddTriangleFilled(ImVec2(c.x - s, c.y - a), ImVec2(c.x - s, c.y + a), ImVec2(c.x - s - a, c.y), col);
                dl->AddTriangleFilled(ImVec2(c.x - a, c.y - s), ImVec2(c.x + a, c.y - s), ImVec2(c.x, c.y - s - a), col);
                dl->AddTriangleFilled(ImVec2(c.x - a, c.y + s), ImVec2(c.x + a, c.y + s), ImVec2(c.x, c.y + s + a), col);
            } else if (icon == 1) {
                dl->AddCircle(c, s, col, 0, 1.6f);
                dl->AddTriangleFilled(ImVec2(c.x + s - 3.0f, c.y - 4.0f), ImVec2(c.x + s + 3.0f, c.y - 4.0f), ImVec2(c.x + s, c.y + 1.0f), col);
            } else {
                dl->AddRect(ImVec2(c.x - s, c.y - s * 0.2f), ImVec2(c.x + s * 0.2f, c.y + s), col, 0.0f, 0, 1.5f);
                dl->AddRect(ImVec2(c.x - s * 0.2f, c.y - s), ImVec2(c.x + s, c.y + s * 0.2f), col, 0.0f, 0, 1.5f);
            }
            return clicked;
        };

        ImGui::SetCursorScreenPos(origin);
        if (gizmoButton("##vpTranslate", 0, m_GizmoOperation == GizmoOperation::Translate, "Translate (1)")) {
            m_GizmoOperation = GizmoOperation::Translate;
        }
        ImGui::SameLine(0.0f, pad);
        if (gizmoButton("##vpRotate", 1, m_GizmoOperation == GizmoOperation::Rotate, "Rotate (2)")) {
            m_GizmoOperation = GizmoOperation::Rotate;
        }
        ImGui::SameLine(0.0f, pad);
        if (gizmoButton("##vpScale", 2, m_GizmoOperation == GizmoOperation::Scale, "Scale (3)")) {
            m_GizmoOperation = GizmoOperation::Scale;
        }
        ImGui::SameLine(0.0f, pad + 6.0f);
        if (ImGui::Button(m_GizmoSpace == GizmoSpace::World ? "World" : "Local", ImVec2(spaceW, btn))) {
            m_GizmoSpace = (m_GizmoSpace == GizmoSpace::World) ? GizmoSpace::Local : GizmoSpace::World;
        }
        ImGui::SetItemTooltip("Gizmo space (4)");
    }

    ImGui::End();
}


void EditorLayer::HandleViewportPicking() {
    // Don't pick if mouse is outside the editor viewport
    if (!m_EditorViewportHovered) {
        // Cancel any active marquee if mouse enters a panel
        if (m_MarqueeDragging) {
            m_MarqueeDragging = false;
        }
        return;
    }

    if (!m_World || !m_Camera) {
        return;
    }

    Math::Vector2 mousePos = Input::GetMousePosition();
    f32 vpW = m_EditorViewportImageMaxX - m_EditorViewportImageMinX;
    f32 vpH = m_EditorViewportImageMaxY - m_EditorViewportImageMinY;
    if (vpW <= 0 || vpH <= 0) return;

    // Don't pick or start a marquee through the floating viewport toolbar
    if (!m_MarqueeDragging &&
        mousePos.x >= m_ViewportToolbarMinX && mousePos.x <= m_ViewportToolbarMaxX &&
        mousePos.y >= m_ViewportToolbarMinY && mousePos.y <= m_ViewportToolbarMaxY) {
        return;
    }

    bool ctrlHeld = Input::IsKeyDown(KeyCode::LeftControl) || Input::IsKeyDown(KeyCode::RightControl);

    // --- Marquee drag handling ---
    if (Input::IsMouseButtonPressed(MouseButton::Left) && !ImGuizmo::IsOver()) {
        // Start potential marquee drag
        m_MarqueeStart = ImVec2(mousePos.x, mousePos.y);
        m_MarqueeEnd = m_MarqueeStart;
        m_MarqueeDragging = true;
    }

    if (m_MarqueeDragging && Input::IsMouseButtonDown(MouseButton::Left)) {
        m_MarqueeEnd = ImVec2(mousePos.x, mousePos.y);
    }

    if (m_MarqueeDragging && !Input::IsMouseButtonDown(MouseButton::Left)) {
        m_MarqueeDragging = false;

        // Check if it was a real drag (> 5 pixels) or just a click
        f32 dx = m_MarqueeEnd.x - m_MarqueeStart.x;
        f32 dy = m_MarqueeEnd.y - m_MarqueeStart.y;
        f32 dragDist = std::sqrt(dx * dx + dy * dy);

        if (dragDist > 5.0f) {
            // Marquee selection
            ImVec2 rectMin(std::min(m_MarqueeStart.x, m_MarqueeEnd.x),
                           std::min(m_MarqueeStart.y, m_MarqueeEnd.y));
            ImVec2 rectMax(std::max(m_MarqueeStart.x, m_MarqueeEnd.x),
                           std::max(m_MarqueeStart.y, m_MarqueeEnd.y));

            if (!ctrlHeld) {
                ClearSelection();
            }
            SelectEntitiesInRect(rectMin, rectMax);
            return;
        }

        // It was a click — fall through to pick logic

        // --- Bone picking: if a skeletal entity is selected with showBones, check bone joints first ---
        if (m_PrimarySelected != ECS::INVALID_ENTITY &&
            m_World->HasComponent<ECS::AnimatorComponent>(m_PrimarySelected)) {
            auto* animComp = m_World->GetComponent<ECS::AnimatorComponent>(m_PrimarySelected);
            if (animComp && animComp->showBones) {
                const auto* skeleton = animComp->animator.GetSkeleton();
                const auto& pose = animComp->animator.GetCurrentPose();
                if (skeleton && !skeleton->bones.empty() &&
                    pose.worldTransforms.size() == skeleton->bones.size()) {
                    Math::Matrix4 entityWorld = ECS::ComputeWorldMatrix(m_World, m_PrimarySelected);
                    Math::Matrix4 viewMat = m_Camera->GetViewMatrix();
                    Math::Matrix4 projMat = m_Camera->GetProjectionMatrix();
                    Math::Matrix4 viewProj = projMat * viewMat;

                    f32 bestDist = 15.0f; // 15 pixel threshold (increased for dense face/hand clusters)
                    i32 bestBone = -1;

                    for (usize i = 0; i < skeleton->bones.size(); ++i) {
                        Math::Matrix4 boneWorld = entityWorld * pose.worldTransforms[i];
                        Math::Vector3 bonePos(boneWorld.m[12], boneWorld.m[13], boneWorld.m[14]);

                        // Project to screen
                        Math::Vector4 clipPos = viewProj * Math::Vector4(bonePos.x, bonePos.y, bonePos.z, 1.0f);
                        if (clipPos.w <= 0.001f) continue;
                        f32 ndcX = clipPos.x / clipPos.w;
                        f32 ndcY = clipPos.y / clipPos.w;
                        f32 ndcZ = clipPos.z / clipPos.w;
                        if (ndcZ < 0.0f || ndcZ > 1.0f) continue;
                        f32 screenX = (ndcX + 1.0f) * 0.5f * vpW + m_EditorViewportImageMinX;
                        f32 screenY = (ndcY + 1.0f) * 0.5f * vpH + m_EditorViewportImageMinY;

                        f32 dx2 = screenX - mousePos.x;
                        f32 dy2 = screenY - mousePos.y;
                        f32 dist = std::sqrt(dx2 * dx2 + dy2 * dy2);
                        if (dist < bestDist) {
                            bestDist = dist;
                            bestBone = static_cast<i32>(i);
                        }
                    }

                    if (bestBone >= 0) {
                        animComp->selectedBoneIndex = bestBone;
                        return; // Bone picked — don't fall through to entity picking
                    }
                }
            }
        }

        // Double-click detection
        static f64 lastClickTime = 0.0;
        static ECS::Entity lastClickedEntity = ECS::INVALID_ENTITY;
        const f64 doubleClickTime = 0.3;

        ECS::Entity picked = ScenePicker::PickEntity(
            m_World, m_Camera,
            mousePos.x - m_EditorViewportImageMinX,
            mousePos.y - m_EditorViewportImageMinY,
            vpW, vpH
        );

        f64 currentTime = ImGui::GetTime();

        if (picked != ECS::INVALID_ENTITY) {
            if (picked == lastClickedEntity && (currentTime - lastClickTime) < doubleClickTime) {
                FocusOnEntity(picked);
                lastClickedEntity = ECS::INVALID_ENTITY;
            } else {
                if (ctrlHeld) {
                    // Toggle selection
                    if (IsSelected(picked)) {
                        DeselectEntity(picked);
                    } else {
                        SelectEntity(picked, true);
                    }
                } else {
                    SelectEntity(picked);
                }
                lastClickedEntity = picked;
                lastClickTime = currentTime;
                ENJIN_LOG_DEBUG(Editor, "Selected entity %llu", (unsigned long long)picked);
            }
        } else {
            // Clicked on empty space
            if (!ctrlHeld) {
                ClearSelection();
            }
            lastClickedEntity = ECS::INVALID_ENTITY;
        }
    }
}

ImDrawList* EditorLayer::GetViewportOverlayDrawList() {
    // Viewport overlays (gizmos, marquee, frustums) draw into the Scene window's
    // own draw list so dialogs and popups — separate windows drawn later — layer
    // above them. The foreground draw list paints over every window, which made
    // gizmos bleed through dialogs. Fallback covers frames before "Scene" exists.
    ImGuiWindow* sceneWindow = ImGui::FindWindowByName("Scene");
    return sceneWindow ? sceneWindow->DrawList : ImGui::GetForegroundDrawList();
}

void EditorLayer::DrawMarqueeRect() {
    if (!m_MarqueeDragging) return;

    f32 dx = m_MarqueeEnd.x - m_MarqueeStart.x;
    f32 dy = m_MarqueeEnd.y - m_MarqueeStart.y;
    f32 dragDist = std::sqrt(dx * dx + dy * dy);
    if (dragDist <= 5.0f) return;

    ImDrawList* dl = GetViewportOverlayDrawList();
    dl->AddRect(m_MarqueeStart, m_MarqueeEnd, IM_COL32(100, 150, 255, 200));
    dl->AddRectFilled(m_MarqueeStart, m_MarqueeEnd, IM_COL32(100, 150, 255, 40));
}

void EditorLayer::DrawGizmos() {
    if (m_SelectedEntities.empty() || !m_World || !m_Camera) {
        return;
    }

    // Don't draw gizmos when popups/modals are open — ImGuizmo reads raw mouse
    // position and would otherwise respond to drags meant for the popup
    if (ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel)) {
        return;
    }

    // Get editor viewport size from the docked Scene window
    f32 vpW = m_EditorViewportImageMaxX - m_EditorViewportImageMinX;
    f32 vpH = m_EditorViewportImageMaxY - m_EditorViewportImageMinY;
    if (vpW <= 0 || vpH <= 0) {
        return;
    }

    // Compute gizmo position: centroid for multi-select, entity position for single
    Math::Vector3 gizmoPos(0.0f, 0.0f, 0.0f);
    Math::Quaternion gizmoRot;
    Math::Vector3 gizmoScale(1.0f, 1.0f, 1.0f);
    u32 transformCount = 0;

    if (m_SelectedEntities.size() == 1) {
        auto* transform = m_World->GetComponent<ECS::TransformComponent>(m_PrimarySelected);
        if (!transform) return;
        gizmoPos = transform->position;
        gizmoRot = transform->rotation;
        gizmoScale = transform->scale;
        transformCount = 1;
    } else {
        // Multi-select: centroid position, primary entity rotation for local mode
        for (ECS::Entity e : m_SelectedEntities) {
            auto* t = m_World->GetComponent<ECS::TransformComponent>(e);
            if (t) {
                gizmoPos = gizmoPos + t->position;
                transformCount++;
            }
        }
        if (transformCount == 0) return;
        gizmoPos = gizmoPos * (1.0f / static_cast<f32>(transformCount));
        // Use primary entity's rotation so LOCAL mode gizmo aligns to it
        auto* primaryTransform = m_World->GetComponent<ECS::TransformComponent>(m_PrimarySelected);
        if (primaryTransform) {
            gizmoRot = primaryTransform->rotation;
        }
    }

    // Draw into the Scene window's draw list (layers under dialogs/popups)
    // and tell ImGuizmo the Scene window is the interaction target
    ImGuizmo::SetOrthographic(m_CameraController && m_CameraController->IsOrthographic());
    ImGuizmo::SetDrawlist(GetViewportOverlayDrawList());
    ImGuizmo::SetRect(m_EditorViewportImageMinX, m_EditorViewportImageMinY, vpW, vpH);
    ImGuiWindow* sceneWindow = ImGui::FindWindowByName("Scene");
    if (sceneWindow) {
        ImGuizmo::SetAlternativeWindow(sceneWindow);
    }

    // Get camera matrices
    Math::Matrix4 viewMat = m_Camera->GetViewMatrix();
    Math::Matrix4 projMat = m_Camera->GetProjectionMatrix();
    projMat.m[5] *= -1.0f; // ImGuizmo expects OpenGL Y-up

    // Build gizmo transform matrix
    Math::Matrix4 entityMat = Math::Matrix4::Translation(gizmoPos) *
                               gizmoRot.ToMatrix() *
                               Math::Matrix4::Scale(gizmoScale);

    // Determine ImGuizmo operation
    ImGuizmo::OPERATION op = ImGuizmo::TRANSLATE;
    switch (m_GizmoOperation) {
        case GizmoOperation::Translate: op = ImGuizmo::TRANSLATE; break;
        case GizmoOperation::Rotate: op = ImGuizmo::ROTATE; break;
        case GizmoOperation::Scale: op = ImGuizmo::SCALE; break;
    }

    ImGuizmo::MODE mode = (m_GizmoSpace == GizmoSpace::Local) ? ImGuizmo::LOCAL : ImGuizmo::WORLD;

    // Snap values
    f32 snapValues[3] = { 0.0f, 0.0f, 0.0f };
    if (m_UseSnap) {
        switch (m_GizmoOperation) {
            case GizmoOperation::Translate:
                snapValues[0] = snapValues[1] = snapValues[2] = m_TranslateSnap; break;
            case GizmoOperation::Rotate:
                snapValues[0] = snapValues[1] = snapValues[2] = m_RotateSnap; break;
            case GizmoOperation::Scale:
                snapValues[0] = snapValues[1] = snapValues[2] = m_ScaleSnap; break;
        }
    }

    // --- Bone Gizmo: when a bone is selected on a skeletal entity, the gizmo
    // manipulates the bone instead of the entity. Bones only support rotation —
    // translating/scaling a skeletal joint would break the rig (bind pose
    // anchors them).
    if (m_PrimarySelected != ECS::INVALID_ENTITY &&
        m_World->HasComponent<ECS::AnimatorComponent>(m_PrimarySelected)) {
        auto* animComp = m_World->GetComponent<ECS::AnimatorComponent>(m_PrimarySelected);
        if (animComp && animComp->showBones && animComp->selectedBoneIndex >= 0) {
            const auto* skeleton = animComp->animator.GetSkeleton();
            const auto& pose = animComp->animator.GetCurrentPose();
            i32 bi = animComp->selectedBoneIndex;
            if (skeleton && bi < static_cast<i32>(skeleton->bones.size()) &&
                pose.worldTransforms.size() == skeleton->bones.size()) {

                Math::Matrix4 entityWorld = ECS::ComputeWorldMatrix(m_World, m_PrimarySelected);
                Math::Matrix4 boneWorldMat = entityWorld * pose.worldTransforms[bi];

                // Parent world matrix is needed to convert the manipulated
                // world-space bone matrix back into the bone's parent-local
                // space (which is what SetBoneLocalRotation expects).
                i32 parentIdx = skeleton->bones[bi].parentIndex;
                Math::Matrix4 parentWorldMat = (parentIdx >= 0)
                    ? entityWorld * pose.worldTransforms[parentIdx]
                    : entityWorld;

                if (ImGuizmo::Manipulate(viewMat.m, projMat.m, ImGuizmo::ROTATE,
                        ImGuizmo::LOCAL, boneWorldMat.m, nullptr,
                        m_UseSnap ? snapValues : nullptr)) {
                    Math::Matrix4 newLocal = parentWorldMat.Inverse() * boneWorldMat;
                    Math::Quaternion newLocalRot = Math::Quaternion::FromMatrix(newLocal);
                    const std::string& boneName = skeleton->bones[bi].name;
                    animComp->animator.SetBoneLocalRotation(boneName, newLocalRot);
                }
                return; // Bone gizmo replaces entity gizmo while a bone is selected
            }
        }
    }

    // Track gizmo drag start/end for undo/redo
    bool gizmoActive = ImGuizmo::IsUsing();
    if (gizmoActive && !m_GizmoDragging) {
        m_GizmoDragging = true;
        m_GizmoStartTransform = entityMat;
    }

    // Draw and manipulate gizmo
    Math::Matrix4 prevMat = entityMat;
    if (ImGuizmo::Manipulate(viewMat.m, projMat.m, op, mode, entityMat.m,
                              nullptr, m_UseSnap ? snapValues : nullptr)) {
        // Compute delta between old and new gizmo matrix
        f32 newTrans[3], newRot[3], newScale[3];
        ImGuizmo::DecomposeMatrixToComponents(entityMat.m, newTrans, newRot, newScale);
        f32 oldTrans[3], oldRot[3], oldScale[3];
        ImGuizmo::DecomposeMatrixToComponents(prevMat.m, oldTrans, oldRot, oldScale);

        Math::Vector3 deltaPos(newTrans[0] - oldTrans[0], newTrans[1] - oldTrans[1], newTrans[2] - oldTrans[2]);
        Math::Vector3 deltaRot(newRot[0] - oldRot[0], newRot[1] - oldRot[1], newRot[2] - oldRot[2]);
        Math::Vector3 deltaScale(
            oldScale[0] != 0.0f ? newScale[0] / oldScale[0] : 1.0f,
            oldScale[1] != 0.0f ? newScale[1] / oldScale[1] : 1.0f,
            oldScale[2] != 0.0f ? newScale[2] / oldScale[2] : 1.0f);

        if (m_SelectedEntities.size() == 1) {
            // Single entity: apply directly
            auto* transform = m_World->GetComponent<ECS::TransformComponent>(m_PrimarySelected);
            if (transform) {
                transform->position = Math::Vector3(newTrans[0], newTrans[1], newTrans[2]);
                transform->scale = Math::Vector3(newScale[0], newScale[1], newScale[2]);
                f32 rx = Math::Radians(newRot[0]);
                f32 ry = Math::Radians(newRot[1]);
                f32 rz = Math::Radians(newRot[2]);
                transform->rotation = Math::Quaternion(Math::Vector3(0, 1, 0), ry)
                                    * Math::Quaternion(Math::Vector3(1, 0, 0), rx)
                                    * Math::Quaternion(Math::Vector3(0, 0, 1), rz);
                if (m_CollabSystem.IsActive()) {
                    Math::Vector3 euler = transform->rotation.ToEuler();
                    m_CollabSystem.OnTransformChanged(m_PrimarySelected,
                        transform->position, euler, transform->scale);
                }
            }
        } else {
            // Multi-entity: apply delta to all selected
            for (ECS::Entity e : m_SelectedEntities) {
                auto* t = m_World->GetComponent<ECS::TransformComponent>(e);
                if (!t) continue;
                t->position = t->position + deltaPos;
                t->scale = Math::Vector3(t->scale.x * deltaScale.x,
                                         t->scale.y * deltaScale.y,
                                         t->scale.z * deltaScale.z);
                if (deltaRot.x != 0.0f || deltaRot.y != 0.0f || deltaRot.z != 0.0f) {
                    Math::Quaternion dr = Math::Quaternion(Math::Vector3(0,1,0), Math::Radians(deltaRot.y))
                                        * Math::Quaternion(Math::Vector3(1,0,0), Math::Radians(deltaRot.x))
                                        * Math::Quaternion(Math::Vector3(0,0,1), Math::Radians(deltaRot.z));
                    t->rotation = dr * t->rotation;
                }
                if (m_CollabSystem.IsActive()) {
                    Math::Vector3 euler = t->rotation.ToEuler();
                    m_CollabSystem.OnTransformChanged(e, t->position, euler, t->scale);
                }
            }
        }

        // Surface snap: project moved entities onto terrain/sphere surfaces
        if (m_SurfaceSnap && m_GizmoOperation == GizmoOperation::Translate) {
            auto terrainEntities = m_World->GetEntitiesWithComponent<ECS::TerrainComponent>();
            auto gravityEntities = m_World->GetEntitiesWithComponent<ECS::GravityZoneComponent>();

            for (ECS::Entity e : m_SelectedEntities) {
                auto* t = m_World->GetComponent<ECS::TransformComponent>(e);
                if (!t) continue;

                Math::Vector3 bestPos = t->position;
                Math::Vector3 bestNormal(0, 1, 0);
                f32 bestDistSq = FLT_MAX;
                bool foundSurface = false;

                // Check terrain surfaces
                for (ECS::Entity te : terrainEntities) {
                    if (te == e) continue;
                    auto* terrain = m_World->GetComponent<ECS::TerrainComponent>(te);
                    auto* terrainXf = m_World->GetComponent<ECS::TransformComponent>(te);
                    if (!terrain || !terrainXf || terrain->heightmap.empty()) continue;

                    // Convert entity position to terrain-local coordinates
                    Math::Vector3 localPos = t->position - terrainXf->position;
                    f32 gridW = static_cast<f32>(terrain->gridWidth);
                    f32 gridH = static_cast<f32>(terrain->gridHeight);
                    f32 cs = terrain->cellSize;

                    // Terrain grid spans [0, gridWidth*cellSize] x [0, gridHeight*cellSize]
                    f32 fx = localPos.x / cs;
                    f32 fz = localPos.z / cs;
                    if (fx < 0.0f || fx >= gridW - 1.0f || fz < 0.0f || fz >= gridH - 1.0f) continue;

                    // Bilinear interpolation of height
                    u32 ix = static_cast<u32>(fx);
                    u32 iz = static_cast<u32>(fz);
                    f32 fracX = fx - static_cast<f32>(ix);
                    f32 fracZ = fz - static_cast<f32>(iz);

                    f32 h00 = terrain->GetHeight(ix, iz);
                    f32 h10 = terrain->GetHeight(ix + 1, iz);
                    f32 h01 = terrain->GetHeight(ix, iz + 1);
                    f32 h11 = terrain->GetHeight(ix + 1, iz + 1);
                    f32 h = h00 * (1.0f - fracX) * (1.0f - fracZ)
                          + h10 * fracX * (1.0f - fracZ)
                          + h01 * (1.0f - fracX) * fracZ
                          + h11 * fracX * fracZ;

                    Math::Vector3 surfacePos(t->position.x, terrainXf->position.y + h, t->position.z);

                    // Compute normal from heightmap gradient
                    f32 hL = terrain->GetHeight(ix > 0 ? ix - 1 : ix, iz);
                    f32 hR = terrain->GetHeight(ix < terrain->gridWidth - 1 ? ix + 1 : ix, iz);
                    f32 hB = terrain->GetHeight(ix, iz > 0 ? iz - 1 : iz);
                    f32 hF = terrain->GetHeight(ix, iz < terrain->gridHeight - 1 ? iz + 1 : iz);
                    Math::Vector3 normal = Math::Vector3(hL - hR, 2.0f * cs, hB - hF).Normalized();

                    f32 dSq = (surfacePos - t->position).LengthSquared();
                    if (dSq < bestDistSq) {
                        bestDistSq = dSq;
                        bestPos = surfacePos;
                        bestNormal = normal;
                        foundSurface = true;
                    }
                }

                // Check sphere gravity zones
                for (ECS::Entity ge : gravityEntities) {
                    if (ge == e) continue;
                    auto* gz = m_World->GetComponent<ECS::GravityZoneComponent>(ge);
                    auto* gzXf = m_World->GetComponent<ECS::TransformComponent>(ge);
                    if (!gz || !gzXf) continue;
                    if (gz->shape != ECS::GravityZoneShape::Sphere || gz->mode != ECS::GravityZoneMode::Point) continue;

                    Math::Vector3 center = gzXf->position;
                    f32 radius = gz->halfExtents.x;
                    Math::Vector3 dir = t->position - center;
                    f32 dist = dir.Length();
                    if (dist < 0.001f) continue;

                    Math::Vector3 normal = dir * (1.0f / dist);
                    Math::Vector3 surfacePos = center + normal * radius;

                    f32 dSq = (surfacePos - t->position).LengthSquared();
                    if (dSq < bestDistSq) {
                        bestDistSq = dSq;
                        bestPos = surfacePos;
                        bestNormal = normal;
                        foundSurface = true;
                    }
                }

                if (foundSurface) {
                    t->position = bestPos;

                    if (m_SurfaceAlignNormal) {
                        Math::Quaternion oldRotation = t->rotation;
                        Math::Quaternion alignRot = Math::Quaternion::FromToRotation(
                            Math::Vector3(0, 1, 0), bestNormal);

                        // Preserve the entity's existing yaw (facing direction)
                        Math::Vector3 oldForward = oldRotation.Rotate(Math::Vector3(0, 0, 1));
                        Math::Vector3 projForward = oldForward - bestNormal * oldForward.Dot(bestNormal);
                        if (projForward.Length() > 0.001f) {
                            projForward = projForward.Normalized();
                            Math::Vector3 alignedForward = alignRot.Rotate(Math::Vector3(0, 0, 1));
                            Math::Quaternion yawCorrection = Math::Quaternion::FromToRotation(
                                alignedForward, projForward);
                            t->rotation = yawCorrection * alignRot;
                        } else {
                            t->rotation = alignRot;
                        }
                    }
                }
            }
        }
    }

    // Gizmo drag ended - create undo command for primary entity
    if (!gizmoActive && m_GizmoDragging) {
        m_GizmoDragging = false;

        if (m_SelectedEntities.size() == 1) {
            auto* transform = m_World->GetComponent<ECS::TransformComponent>(m_PrimarySelected);
            if (transform) {
                f32 oldT[3], oldR[3], oldS[3];
                ImGuizmo::DecomposeMatrixToComponents(m_GizmoStartTransform.m, oldT, oldR, oldS);

                ECS::TransformComponent oldTransform;
                oldTransform.position = Math::Vector3(oldT[0], oldT[1], oldT[2]);
                oldTransform.scale = Math::Vector3(oldS[0], oldS[1], oldS[2]);
                oldTransform.rotation = Math::Quaternion(Math::Vector3(0,1,0), Math::Radians(oldR[1]))
                                      * Math::Quaternion(Math::Vector3(1,0,0), Math::Radians(oldR[0]))
                                      * Math::Quaternion(Math::Vector3(0,0,1), Math::Radians(oldR[2]));

                auto cmd = std::make_unique<TransformCommand>(m_World, m_PrimarySelected, oldTransform, *transform);
                m_UndoRedo.Execute(std::move(cmd));

                // Record mode: capture keyframe on gizmo drag end
                if (m_FlashTimelineEditor.IsRecordMode() && m_FlashTimelineEditor.GetTimeline()) {
                    m_FlashTimelineEditor.CaptureKeyframe(m_World, m_PrimarySelected);
                }
            }
        }
        // Note: multi-entity undo is not tracked per-entity to keep complexity manageable

        // VWS: fold the moved transform(s) into the active override layer. Fires
        // once per drag (not per manipulated frame) and no-ops without an active
        // layer. The inspector's own diff would miss this when it's closed.
        RecordLayerEditForSelection("transform");
    }
}

void EditorLayer::BuildGridMesh() {
    if (!m_Renderer) return;

    bool is2D = m_SceneManager.GetProjectMode() == Scene::ProjectMode::Mode2D;
    f32 halfExtent = m_GridSize * 0.5f;
    f32 step = m_GridSize / static_cast<f32>(m_GridLines);
    i32 halfLines = m_GridLines / 2;

    // Count lines: (gridLines+1) per axis, minus 1 axis line each = regular lines
    // Layout: [regular lines] [X axis] [Y/Z axis]
    u32 regularPerAxis = static_cast<u32>(m_GridLines); // lines excluding i=0
    u32 regularLines = regularPerAxis * 2;
    u32 totalVertices = (regularLines + 2) * 2; // +2 for axis lines, *2 verts per line

    m_GridRegularCount = regularLines * 2;
    m_GridAxisXStart = m_GridRegularCount;
    m_GridAxisZStart = m_GridRegularCount + 2;
    m_GridVertexCount = totalVertices;

    // 24 floats per vertex: pos(3) normal(3) uv(2) color(4) tangent(4) boneWeights(4) boneIndices(4)
    // Must match pipeline vertex stride (sizeof(f32) * 24 = 96 bytes)
    constexpr u32 FLOATS_PER_VERT = 24;
    std::vector<f32> verts(totalVertices * FLOATS_PER_VERT, 0.0f);
    u32 v = 0;

    auto addVert = [&](f32 x, f32 y, f32 z) {
        usize base = static_cast<usize>(v) * FLOATS_PER_VERT;
        verts[base + 0] = x;
        verts[base + 1] = y;
        verts[base + 2] = z;
        verts[base + 4] = 1.0f; // normal Y
        verts[base + 8]  = 1.0f; // vertex color R
        verts[base + 9]  = 1.0f; // vertex color G
        verts[base + 10] = 1.0f; // vertex color B
        verts[base + 11] = 1.0f; // vertex color A
        // boneWeights (base+16..19) and boneIndices (base+20..23) stay zero
        v++;
    };

    if (is2D) {
        // 2D mode: grid in XY plane (Z=0)
        // X-parallel lines (horizontal)
        for (i32 i = -halfLines; i <= halfLines; ++i) {
            if (i == 0) continue;
            f32 y = static_cast<f32>(i) * step;
            addVert(-halfExtent, y, 0.0f);
            addVert( halfExtent, y, 0.0f);
        }
        // Y-parallel lines (vertical)
        for (i32 i = -halfLines; i <= halfLines; ++i) {
            if (i == 0) continue;
            f32 x = static_cast<f32>(i) * step;
            addVert(x, -halfExtent, 0.0f);
            addVert(x,  halfExtent, 0.0f);
        }

        // X axis line (horizontal, at y=0)
        addVert(-halfExtent, 0.0f, 0.0f);
        addVert( halfExtent, 0.0f, 0.0f);

        // Y axis line (vertical, at x=0)
        addVert(0.0f, -halfExtent, 0.0f);
        addVert(0.0f,  halfExtent, 0.0f);
    } else {
        // 3D/Mixed mode: grid in XZ plane (Y=0)
        // X-parallel lines
        for (i32 i = -halfLines; i <= halfLines; ++i) {
            if (i == 0) continue;
            f32 z = static_cast<f32>(i) * step;
            addVert(-halfExtent, 0.0f, z);
            addVert( halfExtent, 0.0f, z);
        }
        // Z-parallel lines
        for (i32 i = -halfLines; i <= halfLines; ++i) {
            if (i == 0) continue;
            f32 x = static_cast<f32>(i) * step;
            addVert(x, 0.0f, -halfExtent);
            addVert(x, 0.0f,  halfExtent);
        }

        // X axis line (at z=0, runs along X)
        addVert(-halfExtent, 0.0f, 0.0f);
        addVert( halfExtent, 0.0f, 0.0f);

        // Z axis line (at x=0, runs along Z)
        addVert(0.0f, 0.0f, -halfExtent);
        addVert(0.0f, 0.0f,  halfExtent);
    }

    usize bufferSize = verts.size() * sizeof(f32);
    m_GridVertexBuffer = std::make_unique<Renderer::VulkanBuffer>(m_Renderer->GetContext());
    m_GridVertexBuffer->Create(bufferSize, Renderer::BufferUsage::Vertex, true);
    m_GridVertexBuffer->UploadData(verts.data(), bufferSize);

    m_BuiltGridSize = m_GridSize;
    m_BuiltGridLines = m_GridLines;
    m_BuiltGridIs2D = is2D;
}

void EditorLayer::DrawGrid() {
    if (!m_ShowGrid || !m_Camera || !m_Renderer || !m_RenderSystem) {
        return;
    }

    bool is2D = m_SceneManager.GetProjectMode() == Scene::ProjectMode::Mode2D;

    // Rebuild mesh when grid settings or orientation change
    if (!m_GridVertexBuffer || m_GridSize != m_BuiltGridSize ||
        m_GridLines != m_BuiltGridLines || is2D != m_BuiltGridIs2D) {
        BuildGridMesh();
    }

    if (!m_GridVertexBuffer || m_GridVertexCount == 0) return;

    // Regular grid lines (gray, semi-transparent)
    m_RenderSystem->RenderGridLines(m_GridVertexBuffer.get(), m_GridRegularCount,
        0, Math::Vector3(0.22f, 0.22f, 0.22f), 0.47f);

    // X axis (red) — same in both modes
    m_RenderSystem->RenderGridLines(m_GridVertexBuffer.get(), 2,
        m_GridAxisXStart, Math::Vector3(0.7f, 0.24f, 0.24f), 0.8f);

    // Second axis: Y (green) in 2D mode, Z (blue) in 3D mode
    if (is2D) {
        m_RenderSystem->RenderGridLines(m_GridVertexBuffer.get(), 2,
            m_GridAxisZStart, Math::Vector3(0.24f, 0.7f, 0.24f), 0.8f);
    } else {
        m_RenderSystem->RenderGridLines(m_GridVertexBuffer.get(), 2,
            m_GridAxisZStart, Math::Vector3(0.24f, 0.24f, 0.7f), 0.8f);
    }
}

void EditorLayer::FocusOnEntity(ECS::Entity entity) {
    if (!m_Camera || !m_CameraController || !m_World) {
        return;
    }
    if (!m_World->IsValid(entity)) return;

    // Get entity transform
    auto* transform = m_World->GetComponent<ECS::TransformComponent>(entity);
    if (!transform) {
        return;
    }

    // WORLD-space focus: imported meshes live under a scaled import root, so
    // local position/scale point at raw FBX coordinates — F used to fly the
    // camera hundreds of units off (often INSIDE an unscaled model, seeing only
    // its shadow). Everything below goes through ComputeWorldMatrix.
    auto worldPointOf = [&](ECS::Entity e, const Math::Vector3& p) {
        Math::Matrix4 wm = ECS::ComputeWorldMatrix(m_World, e);
        Math::Vector4 r = wm * Math::Vector4(p.x, p.y, p.z, 1.0f);
        return Math::Vector3(r.x, r.y, r.z);
    };
    auto worldMaxScaleOf = [&](ECS::Entity e) {
        Math::Matrix4 wm = ECS::ComputeWorldMatrix(m_World, e);
        return Math::Max(
            Math::Vector3(wm.m[0], wm.m[1], wm.m[2]).Length(),
            Math::Max(Math::Vector3(wm.m[4], wm.m[5], wm.m[6]).Length(),
                      Math::Vector3(wm.m[8], wm.m[9], wm.m[10]).Length()));
    };

    // Calculate bounding size for appropriate distance
    f32 boundingSize = 2.0f;  // Default size
    Math::Vector3 targetPos = worldPointOf(entity, Math::Vector3(0.0f));

    // If entity has a box collider, use its AABB for accurate sizing
    // (collider sizes are WORLD space — no scale multiplication)
    if (m_World->HasComponent<ECS::BoxColliderComponent>(entity)) {
        auto* collider = m_World->GetComponent<ECS::BoxColliderComponent>(entity);
        boundingSize = Math::Max(collider->size.x, Math::Max(collider->size.y, collider->size.z));
        targetPos = worldPointOf(entity, Math::Vector3(0.0f)) + collider->center;
    } else if (m_World->HasComponent<ECS::MeshComponent>(entity)) {
        // Estimate from mesh vertices (local bounds -> world center + scale)
        auto* mesh = m_World->GetComponent<ECS::MeshComponent>(entity);
        if (mesh && mesh->IsValid()) {
            Math::Vector3 minB(FLT_MAX, FLT_MAX, FLT_MAX);
            Math::Vector3 maxB(-FLT_MAX, -FLT_MAX, -FLT_MAX);
            for (const auto& v : mesh->vertices) {
                if (v.position.x < minB.x) minB.x = v.position.x;
                if (v.position.y < minB.y) minB.y = v.position.y;
                if (v.position.z < minB.z) minB.z = v.position.z;
                if (v.position.x > maxB.x) maxB.x = v.position.x;
                if (v.position.y > maxB.y) maxB.y = v.position.y;
                if (v.position.z > maxB.z) maxB.z = v.position.z;
            }
            Math::Vector3 size = maxB - minB;
            boundingSize = Math::Max(size.x, Math::Max(size.y, size.z)) * worldMaxScaleOf(entity);
            targetPos = worldPointOf(entity, (minB + maxB) * 0.5f);
        }
    } else {
        // Container node: scan children for mesh bounds (colliders first, then meshes)
        Math::Vector3 globalMin(FLT_MAX, FLT_MAX, FLT_MAX);
        Math::Vector3 globalMax(-FLT_MAX, -FLT_MAX, -FLT_MAX);
        bool foundChild = false;
        auto growBounds = [&](const Math::Vector3& p) {
            globalMin.x = Math::Min(globalMin.x, p.x);
            globalMin.y = Math::Min(globalMin.y, p.y);
            globalMin.z = Math::Min(globalMin.z, p.z);
            globalMax.x = Math::Max(globalMax.x, p.x);
            globalMax.y = Math::Max(globalMax.y, p.y);
            globalMax.z = Math::Max(globalMax.z, p.z);
        };

        for (ECS::Entity child : ECS::GetChildren(m_World, entity)) {
            auto* childTransform = m_World->GetComponent<ECS::TransformComponent>(child);
            if (!childTransform) continue;

            // Try box collider first (world-space sizes)
            auto* childCollider = m_World->GetComponent<ECS::BoxColliderComponent>(child);
            if (childCollider) {
                Math::Vector3 ctr = worldPointOf(child, Math::Vector3(0.0f)) + childCollider->center;
                Math::Vector3 half = childCollider->size * 0.5f;
                growBounds(ctr - half);
                growBounds(ctr + half);
                foundChild = true;
                continue;
            }

            // Fall back to mesh bounds (local AABB corners through the world matrix)
            auto* childMesh = m_World->GetComponent<ECS::MeshComponent>(child);
            if (childMesh && childMesh->IsValid()) {
                Math::Vector3 minB(FLT_MAX, FLT_MAX, FLT_MAX);
                Math::Vector3 maxB(-FLT_MAX, -FLT_MAX, -FLT_MAX);
                for (const auto& v : childMesh->vertices) {
                    if (v.position.x < minB.x) minB.x = v.position.x;
                    if (v.position.y < minB.y) minB.y = v.position.y;
                    if (v.position.z < minB.z) minB.z = v.position.z;
                    if (v.position.x > maxB.x) maxB.x = v.position.x;
                    if (v.position.y > maxB.y) maxB.y = v.position.y;
                    if (v.position.z > maxB.z) maxB.z = v.position.z;
                }
                growBounds(worldPointOf(child, minB));
                growBounds(worldPointOf(child, maxB));
                foundChild = true;
            }
        }

        if (foundChild) {
            Math::Vector3 size = globalMax - globalMin;
            boundingSize = Math::Max(size.x, Math::Max(size.y, size.z));
            targetPos = (globalMin + globalMax) * 0.5f;
        }
    }

    if (boundingSize < 0.01f) boundingSize = 2.0f;

    // Calculate camera distance based on bounding size. Generous max: a 200-unit
    // clamp left the camera INSIDE anything bigger (unscaled cm-unit FBX imports
    // are ~400 units tall) — F must be able to frame whatever exists.
    f32 distance = boundingSize * 2.5f;
    distance = Math::Clamp(distance, 2.0f, 5000.0f);

    // Get current camera direction (maintain viewing angle)
    Math::Vector3 cameraForward = m_Camera->GetForward();
    Math::Vector3 newCameraPos = targetPos - cameraForward * distance;

    // Set camera position and update orbit target
    m_Camera->SetPosition(newCameraPos);
    m_Camera->SetLookAt(newCameraPos, targetPos, Math::Vector3(0.0f, 1.0f, 0.0f));

    // Update controller's orbit target and sync orientation
    m_CameraController->SetOrbitTarget(targetPos);
    m_CameraController->SetOrbitDistance(distance);
    m_CameraController->SyncFromCamera();

    ENJIN_LOG_INFO(Editor, "Focused on entity %llu at (%.2f, %.2f, %.2f)",
        (unsigned long long)entity, targetPos.x, targetPos.y, targetPos.z);
}


void EditorLayer::FocusOnSelection() {
    if (!m_Camera || !m_CameraController || !m_World || m_SelectedEntities.empty()) return;

    if (m_SelectedEntities.size() == 1) {
        FocusOnEntity(*m_SelectedEntities.begin());
        return;
    }

    // Compute centroid of all selected entities
    Math::Vector3 centroid(0.0f, 0.0f, 0.0f);
    u32 count = 0;
    for (ECS::Entity e : m_SelectedEntities) {
        if (!m_World->IsValid(e)) continue;
        auto* transform = m_World->GetComponent<ECS::TransformComponent>(e);
        if (transform) {
            centroid = centroid + transform->position;
            count++;
        }
    }
    if (count == 0) return;
    centroid = centroid * (1.0f / static_cast<f32>(count));

    // Focus camera on centroid
    f32 distance = 10.0f;
    Math::Vector3 cameraForward = m_Camera->GetForward();
    Math::Vector3 newCameraPos = centroid - cameraForward * distance;

    m_Camera->SetPosition(newCameraPos);
    m_Camera->SetLookAt(newCameraPos, centroid, Math::Vector3(0.0f, 1.0f, 0.0f));
    m_CameraController->SetOrbitTarget(centroid);
    m_CameraController->SetOrbitDistance(distance);
    m_CameraController->SyncFromCamera();

    ENJIN_LOG_INFO(Editor, "Focused on %zu entities at centroid (%.2f, %.2f, %.2f)",
        m_SelectedEntities.size(), centroid.x, centroid.y, centroid.z);
}


void EditorLayer::ApplyBrush(ECS::TerrainComponent* terrain,
                              const ECS::TransformComponent* transform,
                              const Math::Vector3& worldHit, f32 deltaTime) {
    if (!terrain || terrain->heightmap.empty()) return;

    Math::Vector3 origin = transform ? transform->position : Math::Vector3(0.0f);

    // Convert hit to grid coordinates
    f32 localX = worldHit.x - origin.x;
    f32 localZ = worldHit.z - origin.z;
    f32 centerGX = localX / terrain->cellSize;
    f32 centerGZ = localZ / terrain->cellSize;

    f32 radiusCells = m_TerrainBrush.radius / terrain->cellSize;
    i32 minX = static_cast<i32>(std::floor(centerGX - radiusCells));
    i32 maxX = static_cast<i32>(std::ceil(centerGX + radiusCells));
    i32 minZ = static_cast<i32>(std::floor(centerGZ - radiusCells));
    i32 maxZ = static_cast<i32>(std::ceil(centerGZ + radiusCells));

    minX = std::max(minX, 0);
    maxX = std::min(maxX, static_cast<i32>(terrain->gridWidth) - 1);
    minZ = std::max(minZ, 0);
    maxZ = std::min(maxZ, static_cast<i32>(terrain->gridHeight) - 1);

    // For Smooth mode: pre-compute neighbor averages
    std::vector<f32> avgCache;
    if (m_TerrainBrush.mode == TerrainBrushMode::Smooth) {
        usize count = static_cast<usize>((maxX - minX + 1)) * (maxZ - minZ + 1);
        avgCache.resize(count);
        for (i32 z = minZ; z <= maxZ; ++z) {
            for (i32 x = minX; x <= maxX; ++x) {
                f32 sum = 0.0f;
                i32 n = 0;
                for (i32 dz = -1; dz <= 1; ++dz) {
                    for (i32 dx = -1; dx <= 1; ++dx) {
                        i32 nx = x + dx, nz = z + dz;
                        if (nx >= 0 && nx < static_cast<i32>(terrain->gridWidth) &&
                            nz >= 0 && nz < static_cast<i32>(terrain->gridHeight)) {
                            sum += terrain->GetHeight(static_cast<u32>(nx), static_cast<u32>(nz));
                            ++n;
                        }
                    }
                }
                usize idx = static_cast<usize>((z - minZ) * (maxX - minX + 1) + (x - minX));
                avgCache[idx] = (n > 0) ? sum / static_cast<f32>(n) : 0.0f;
            }
        }
    }

    for (i32 z = minZ; z <= maxZ; ++z) {
        for (i32 x = minX; x <= maxX; ++x) {
            f32 dx = static_cast<f32>(x) - centerGX;
            f32 dz = static_cast<f32>(z) - centerGZ;
            f32 dist = std::sqrt(dx * dx + dz * dz);

            if (dist > radiusCells) continue;

            // Smoothstep falloff
            f32 t = dist / radiusCells;
            f32 smoothT = t * t * (3.0f - 2.0f * t);  // smoothstep(0, 1, t)
            f32 weight = m_TerrainBrush.strength * (1.0f - smoothT * m_TerrainBrush.falloff);

            u32 ux = static_cast<u32>(x);
            u32 uz = static_cast<u32>(z);
            f32 h = terrain->GetHeight(ux, uz);

            switch (m_TerrainBrush.mode) {
                case TerrainBrushMode::Raise:
                    h += weight * deltaTime;
                    break;
                case TerrainBrushMode::Lower:
                    h -= weight * deltaTime;
                    break;
                case TerrainBrushMode::Flatten: {
                    f32 diff = m_TerrainBrush.flattenHeight - h;
                    h += diff * weight * deltaTime;
                    break;
                }
                case TerrainBrushMode::Smooth: {
                    usize idx = static_cast<usize>((z - minZ) * (maxX - minX + 1) + (x - minX));
                    f32 avg = avgCache[idx];
                    h += (avg - h) * weight * deltaTime;
                    break;
                }
                case TerrainBrushMode::Paint: {
                    // Paint splatmap instead of modifying height
                    usize cellIdx = static_cast<usize>(uz) * terrain->gridWidth + ux;
                    usize splatBase = cellIdx * 4;
                    if (splatBase + 3 < terrain->splatmap.size()) {
                        terrain->splatmap[splatBase + m_TerrainBrush.paintLayer] += weight * deltaTime;
                        // Normalize weights so they sum to 1
                        f32 total = 0.0f;
                        for (u32 l = 0; l < 4; ++l)
                            total += terrain->splatmap[splatBase + l];
                        if (total > 0.0f) {
                            for (u32 l = 0; l < 4; ++l)
                                terrain->splatmap[splatBase + l] /= total;
                        }
                    }
                    terrain->meshDirty = true;
                    continue;  // Skip SetHeight for paint mode
                }
            }

            h = std::max(0.0f, std::min(terrain->maxHeight, h));
            terrain->SetHeight(ux, uz, h);
        }
    }
}

void EditorLayer::ApplyBrush2D(ECS::Terrain2DComponent* terrain2d,
                                const ECS::TransformComponent* transform,
                                const Math::Vector3& worldHit) {
    if (!terrain2d || terrain2d->controlPoints.empty()) return;

    Math::Vector3 origin = transform ? transform->position : Math::Vector3(0.0f);
    f32 localX = worldHit.x - origin.x;
    f32 localY = worldHit.y - origin.y;

    f32 grabRadius = m_TerrainBrush.radius;

    if (Input::IsMouseButtonPressed(MouseButton::Left)) {
        // Find nearest control point
        f32 bestDist = grabRadius;
        m_Dragging2DPoint = -1;
        for (usize i = 0; i < terrain2d->controlPoints.size(); ++i) {
            f32 dx = terrain2d->controlPoints[i].x - localX;
            f32 dy = terrain2d->controlPoints[i].y - localY;
            f32 d = std::sqrt(dx * dx + dy * dy);
            if (d < bestDist) {
                bestDist = d;
                m_Dragging2DPoint = static_cast<i32>(i);
            }
        }
    }

    if (m_Dragging2DPoint >= 0 && Input::IsMouseButtonDown(MouseButton::Left)) {
        if (m_Dragging2DPoint < static_cast<i32>(terrain2d->controlPoints.size())) {
            terrain2d->controlPoints[static_cast<usize>(m_Dragging2DPoint)] =
                Math::Vector2(localX, localY);
            terrain2d->SortPoints();
            terrain2d->meshDirty = true;
        }
    }

    if (Input::IsMouseButtonReleased(MouseButton::Left)) {
        m_Dragging2DPoint = -1;
    }
}

void EditorLayer::HandleTerrainBrush(f32 deltaTime) {
    if (!m_World || !m_Camera || !m_Renderer) return;
    if (WantsMouseInput()) return;

    // Determine which terrain entity we're editing
    ECS::Entity target = m_PrimarySelected;
    if (target == ECS::INVALID_ENTITY) {
        m_BrushHitValid = false;
        return;
    }

    auto* terrain3D = m_World->GetComponent<ECS::TerrainComponent>(target);
    auto* terrain2D = m_World->GetComponent<ECS::Terrain2DComponent>(target);
    auto* transform = m_World->GetComponent<ECS::TransformComponent>(target);

    if (!terrain3D && !terrain2D) {
        m_BrushHitValid = false;
        return;
    }

    Math::Vector2 mousePos = Input::GetMousePosition();
    f32 bvpW = m_EditorViewportImageMaxX - m_EditorViewportImageMinX;
    f32 bvpH = m_EditorViewportImageMaxY - m_EditorViewportImageMinY;
    if (bvpW <= 0 || bvpH <= 0) return;

    Ray ray = ScenePicker::ScreenToRay(m_Camera,
                                        mousePos.x - m_EditorViewportImageMinX,
                                        mousePos.y - m_EditorViewportImageMinY,
                                        bvpW, bvpH);

    if (terrain3D) {
        Math::Vector3 hitPoint;
        if (RaycastTerrain(ray, terrain3D, transform, hitPoint)) {
            m_BrushHitPoint = hitPoint;
            m_BrushHitValid = true;

            if (Input::IsMouseButtonPressed(MouseButton::Left)) {
                m_BrushActive = true;
                m_TerrainUndoHeightmapSnapshot = terrain3D->heightmap;
                m_TerrainUndoSplatmapSnapshot = terrain3D->splatmap;
            }

            if (m_BrushActive && Input::IsMouseButtonDown(MouseButton::Left)) {
                ApplyBrush(terrain3D, transform, hitPoint, deltaTime);
            }
        } else {
            m_BrushHitValid = false;
        }

        if (Input::IsMouseButtonReleased(MouseButton::Left) && m_BrushActive) {
            if (m_TerrainUndoHeightmapSnapshot != terrain3D->heightmap ||
                m_TerrainUndoSplatmapSnapshot != terrain3D->splatmap) {
                m_UndoRedo.Execute(std::make_unique<TerrainSculptCommand>(
                    m_World, target,
                    std::move(m_TerrainUndoHeightmapSnapshot), terrain3D->heightmap,
                    std::move(m_TerrainUndoSplatmapSnapshot), terrain3D->splatmap));
            }
            m_TerrainUndoHeightmapSnapshot.clear();
            m_TerrainUndoSplatmapSnapshot.clear();
            m_BrushActive = false;
        }
    } else if (terrain2D) {
        // Raycast to XY plane (Z = entity Z position)
        Math::Vector3 origin = transform ? transform->position : Math::Vector3(0.0f);
        f32 planeZ = origin.z;

        if (std::abs(ray.direction.z) > 1e-6f) {
            f32 t = (planeZ - ray.origin.z) / ray.direction.z;
            if (t > 0.0f) {
                Math::Vector3 hitPoint = ray.origin + ray.direction * t;
                m_BrushHitPoint = hitPoint;
                m_BrushHitValid = true;
                ApplyBrush2D(terrain2D, transform, hitPoint);
            } else {
                m_BrushHitValid = false;
            }
        } else {
            m_BrushHitValid = false;
        }
    }
}

// --- ImGui texture cache for sprite/tilemap previews ---

VkDescriptorSet EditorLayer::GetImGuiTexture(const std::string& path) {
    if (path.empty() || !m_RenderSystem) return VK_NULL_HANDLE;

    auto it = m_ImGuiTextureCache.find(path);
    if (it != m_ImGuiTextureCache.end()) {
        return it->second;
    }

    auto tex = m_RenderSystem->LoadTexture(path);
    if (!tex || !tex->IsValid()) return VK_NULL_HANDLE;

    VkDescriptorSet ds = ImGui_ImplVulkan_AddTexture(
        tex->GetSampler(), tex->GetImageView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    if (ds != VK_NULL_HANDLE) {
        m_ImGuiTextureCache[path] = ds;
    }
    return ds;
}

VkDescriptorSet EditorLayer::GetAssetThumbnail(const std::string& path) {
    if (path.empty() || !m_RenderSystem) return VK_NULL_HANDLE;

    // Check if already in ImGui texture cache (thumbnails use "thumb:" prefix key)
    std::string cacheKey = "thumb:" + path;
    auto it = m_ImGuiTextureCache.find(cacheKey);
    if (it != m_ImGuiTextureCache.end()) {
        return it->second;
    }

    // Generate thumbnail on CPU
    Assets::ThumbnailData thumb = m_ThumbnailGenerator.Generate(path, Assets::ThumbnailSize::Medium);
    if (!thumb.valid || thumb.pixels.empty()) return VK_NULL_HANDLE;

    // Upload to GPU as a texture
    auto tex = std::make_shared<Renderer::Texture>(m_Renderer->GetContext());
    if (!tex->CreateFromData(thumb.pixels.data(), thumb.width, thumb.height, 4,
                              VK_FORMAT_R8G8B8A8_SRGB)) {
        return VK_NULL_HANDLE;
    }

    // Register with ImGui
    VkDescriptorSet ds = ImGui_ImplVulkan_AddTexture(
        tex->GetSampler(), tex->GetImageView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    if (ds != VK_NULL_HANDLE) {
        m_ImGuiTextureCache[cacheKey] = ds;
        m_ThumbnailTextures.push_back(tex); // Keep GPU texture alive
    }
    return ds;
}

void EditorLayer::CleanupImGuiTextureCache() {
    for (auto& [path, ds] : m_ImGuiTextureCache) {
        if (ds != VK_NULL_HANDLE) {
            ImGui_ImplVulkan_RemoveTexture(ds);
        }
    }
    m_ImGuiTextureCache.clear();
    m_ThumbnailTextures.clear();
    m_ThumbnailGenerator.ClearCache();
}


void EditorLayer::HandleTilemapBrush() {
    if (!m_World || !m_Camera || !m_Renderer) return;
    if (WantsMouseInput()) return;

    ECS::Entity target = m_PrimarySelected;
    if (target == ECS::INVALID_ENTITY) return;

    auto* tilemap = m_World->GetComponent<ECS::TilemapComponent>(target);
    auto* transform = m_World->GetComponent<ECS::TransformComponent>(target);
    if (!tilemap || tilemap->width == 0 || tilemap->height == 0) return;

    Math::Vector2 mousePos = Input::GetMousePosition();
    f32 tvpW = m_EditorViewportImageMaxX - m_EditorViewportImageMinX;
    f32 tvpH = m_EditorViewportImageMaxY - m_EditorViewportImageMinY;
    if (tvpW <= 0 || tvpH <= 0) return;

    Ray ray = ScenePicker::ScreenToRay(m_Camera,
                                        mousePos.x - m_EditorViewportImageMinX,
                                        mousePos.y - m_EditorViewportImageMinY,
                                        tvpW, tvpH);

    // Intersect ray with XY plane at entity Z position (same as 2D terrain)
    Math::Vector3 origin = transform ? transform->position : Math::Vector3(0.0f);
    f32 planeZ = origin.z;

    if (std::abs(ray.direction.z) < 1e-6f) return;

    f32 t = (planeZ - ray.origin.z) / ray.direction.z;
    if (t <= 0.0f) return;

    Math::Vector3 hitPoint = ray.origin + ray.direction * t;

    // Convert world hit to grid coordinates
    f32 localX = hitPoint.x - origin.x;
    f32 localY = hitPoint.y - origin.y;
    i32 col = static_cast<i32>(std::floor(localX / tilemap->worldTileWidth));
    i32 row = static_cast<i32>(std::floor(localY / tilemap->worldTileHeight));

    if (col < 0 || row < 0 || col >= static_cast<i32>(tilemap->width) ||
        row >= static_cast<i32>(tilemap->height)) {
        return;
    }

    // Begin brush stroke on press
    if (Input::IsMouseButtonPressed(MouseButton::Left) || Input::IsMouseButtonPressed(MouseButton::Right)) {
        m_TilemapBrushActive = true;
        m_TilemapPaintChanges.clear();
        m_TilemapPaintCellIndex.clear();
    }

    u32 ux = static_cast<u32>(col);
    u32 uy = static_cast<u32>(row);

    // Left-click/drag: paint tile
    if (Input::IsMouseButtonDown(MouseButton::Left)) {
        i32 oldIdx = tilemap->GetTile(ux, uy);
        i32 newIdx = m_TileBrushIndex;
        if (oldIdx != newIdx) {
            u64 key = static_cast<u64>(uy) * 65536 + ux;
            auto it = m_TilemapPaintCellIndex.find(key);
            if (it != m_TilemapPaintCellIndex.end()) {
                m_TilemapPaintChanges[it->second].newIndex = newIdx;
            } else {
                m_TilemapPaintCellIndex[key] = m_TilemapPaintChanges.size();
                m_TilemapPaintChanges.push_back({ux, uy, oldIdx, newIdx});
            }
            tilemap->SetTile(ux, uy, newIdx);
        }
    }
    // Right-click/drag: erase tile
    else if (Input::IsMouseButtonDown(MouseButton::Right)) {
        i32 oldIdx = tilemap->GetTile(ux, uy);
        i32 newIdx = -1;
        if (oldIdx != newIdx) {
            u64 key = static_cast<u64>(uy) * 65536 + ux;
            auto it = m_TilemapPaintCellIndex.find(key);
            if (it != m_TilemapPaintCellIndex.end()) {
                m_TilemapPaintChanges[it->second].newIndex = newIdx;
            } else {
                m_TilemapPaintCellIndex[key] = m_TilemapPaintChanges.size();
                m_TilemapPaintChanges.push_back({ux, uy, oldIdx, newIdx});
            }
            tilemap->SetTile(ux, uy, newIdx);
        }
    }

    // End brush stroke on release
    if (m_TilemapBrushActive &&
        (Input::IsMouseButtonReleased(MouseButton::Left) || Input::IsMouseButtonReleased(MouseButton::Right))) {
        if (!m_TilemapPaintChanges.empty()) {
            m_UndoRedo.Execute(std::make_unique<TilemapPaintCommand>(
                m_World, target, std::move(m_TilemapPaintChanges)));
        }
        m_TilemapPaintChanges.clear();
        m_TilemapPaintCellIndex.clear();
        m_TilemapBrushActive = false;
    }
}

void EditorLayer::DrawUIEditorOverlay() {
    if (!m_World || m_UIEditCanvasEntity == ECS::INVALID_ENTITY) return;

    // Validate entity still exists
    if (!m_World->HasComponent<GUI::UICanvasComponent>(m_UIEditCanvasEntity)) {
        m_UIEditMode = false;
        m_UIEditCanvasEntity = ECS::INVALID_ENTITY;
        return;
    }

    auto* canvas = m_World->GetComponent<GUI::UICanvasComponent>(m_UIEditCanvasEntity);
    if (!canvas) return;

    f32 gvW = m_GameViewImageMaxX - m_GameViewImageMinX;
    f32 gvH = m_GameViewImageMaxY - m_GameViewImageMinY;
    if (gvW <= 0 || gvH <= 0) return;

    // Compute layout in Game View local space (0..gvW, 0..gvH)
    m_UISystem.ComputeLayoutForCanvas(*canvas, gvW, gvH);

    // Re-validate canvas pointer after layout computation (ECS storage may have reallocated)
    canvas = m_World->GetComponent<GUI::UICanvasComponent>(m_UIEditCanvasEntity);
    if (!canvas || canvas->elements.empty()) return;

    f32 offsetX = m_GameViewImageMinX;
    f32 offsetY = m_GameViewImageMinY;

    // Offset all computed rects to screen space for rendering
    for (auto& element : canvas->elements) {
        element.computedRect.x += offsetX;
        element.computedRect.y += offsetY;
    }

    // Render the canvas preview using the existing widget renderers
    m_UISystem.RenderCanvasPreview(*canvas);

    ImDrawList* dl = ImGui::GetForegroundDrawList();

    // Draw selection outline and handles for selected element
    auto* selElem = canvas->GetElement(m_UIEditSelectedElementId);
    if (selElem) {
        const auto& r = selElem->computedRect;

        // Selection outline (cyan, 2px)
        ImU32 selColor = IM_COL32(0, 255, 255, 200);
        dl->AddRect(ImVec2(r.x, r.y), ImVec2(r.x + r.w, r.y + r.h), selColor, 0.0f, 0, 2.0f);

        // 8 resize handles (white squares)
        constexpr f32 hs = 4.0f;
        ImU32 handleColor = IM_COL32(255, 255, 255, 230);
        ImU32 handleBorder = IM_COL32(0, 0, 0, 200);

        auto drawHandle = [&](f32 cx, f32 cy) {
            dl->AddRectFilled(ImVec2(cx - hs, cy - hs), ImVec2(cx + hs, cy + hs), handleColor);
            dl->AddRect(ImVec2(cx - hs, cy - hs), ImVec2(cx + hs, cy + hs), handleBorder);
        };

        f32 left = r.x, right = r.x + r.w, top = r.y, bottom = r.y + r.h;
        f32 midX = r.x + r.w * 0.5f, midY = r.y + r.h * 0.5f;

        drawHandle(left, top);       drawHandle(right, top);
        drawHandle(left, bottom);    drawHandle(right, bottom);
        drawHandle(midX, top);       drawHandle(midX, bottom);
        drawHandle(left, midY);      drawHandle(right, midY);

        // Element name label above selection
        std::string label = selElem->name;
        ImFont* font = ImGui::GetFont();
        ImVec2 textSize = font->CalcTextSizeA(12.0f, FLT_MAX, 0.0f, label.c_str());
        f32 labelX = r.x;
        f32 labelY = r.y - textSize.y - 4.0f;
        dl->AddRectFilled(ImVec2(labelX - 2, labelY - 1), ImVec2(labelX + textSize.x + 4, labelY + textSize.y + 1),
                          IM_COL32(0, 0, 0, 180));
        dl->AddText(font, 12.0f, ImVec2(labelX, labelY), selColor, label.c_str());
    }

    // --- Snap guides (shown during drag) ---
    if (selElem && m_UIEditDragMode != UIEditDragMode::None) {
        const auto& sr = selElem->computedRect;
        f32 selCX = sr.x + sr.w * 0.5f;
        f32 selCY = sr.y + sr.h * 0.5f;
        f32 selLeft = sr.x, selRight = sr.x + sr.w;
        f32 selTop = sr.y, selBottom = sr.y + sr.h;

        // Canvas center guides (cyan dashed)
        f32 canvasCX = offsetX + gvW * 0.5f;
        f32 canvasCY = offsetY + gvH * 0.5f;
        constexpr f32 snapTolerance = 5.0f;
        ImU32 snapColor = IM_COL32(0, 220, 255, 160);

        if (std::abs(selCX - canvasCX) < snapTolerance) {
            dl->AddLine(ImVec2(canvasCX, offsetY), ImVec2(canvasCX, offsetY + gvH), snapColor, 1.0f);
        }
        if (std::abs(selCY - canvasCY) < snapTolerance) {
            dl->AddLine(ImVec2(offsetX, canvasCY), ImVec2(offsetX + gvW, canvasCY), snapColor, 1.0f);
        }

        // Element-to-element edge alignment guides (pink)
        constexpr f32 edgeTolerance = 3.0f;
        ImU32 edgeColor = IM_COL32(255, 100, 200, 160);

        for (const auto& other : canvas->elements) {
            if (other.id == selElem->id || !other.visible) continue;
            const auto& or_ = other.computedRect;
            f32 oLeft = or_.x, oRight = or_.x + or_.w;
            f32 oTop = or_.y, oBottom = or_.y + or_.h;

            // Left-to-left
            if (std::abs(selLeft - oLeft) < edgeTolerance)
                dl->AddLine(ImVec2(oLeft, std::min(selTop, oTop) - 5), ImVec2(oLeft, std::max(selBottom, oBottom) + 5), edgeColor, 1.0f);
            // Right-to-right
            if (std::abs(selRight - oRight) < edgeTolerance)
                dl->AddLine(ImVec2(oRight, std::min(selTop, oTop) - 5), ImVec2(oRight, std::max(selBottom, oBottom) + 5), edgeColor, 1.0f);
            // Top-to-top
            if (std::abs(selTop - oTop) < edgeTolerance)
                dl->AddLine(ImVec2(std::min(selLeft, oLeft) - 5, oTop), ImVec2(std::max(selRight, oRight) + 5, oTop), edgeColor, 1.0f);
            // Bottom-to-bottom
            if (std::abs(selBottom - oBottom) < edgeTolerance)
                dl->AddLine(ImVec2(std::min(selLeft, oLeft) - 5, oBottom), ImVec2(std::max(selRight, oRight) + 5, oBottom), edgeColor, 1.0f);
            // Left-to-right
            if (std::abs(selLeft - oRight) < edgeTolerance)
                dl->AddLine(ImVec2(oRight, std::min(selTop, oTop) - 5), ImVec2(oRight, std::max(selBottom, oBottom) + 5), edgeColor, 1.0f);
            // Right-to-left
            if (std::abs(selRight - oLeft) < edgeTolerance)
                dl->AddLine(ImVec2(oLeft, std::min(selTop, oTop) - 5), ImVec2(oLeft, std::max(selBottom, oBottom) + 5), edgeColor, 1.0f);
        }
    }

    // "UI EDIT MODE" indicator in top-left of game view
    ImFont* font = ImGui::GetFont();
    ImVec2 indicatorPos(offsetX + 8.0f, offsetY + 8.0f);
    dl->AddRectFilled(ImVec2(indicatorPos.x - 2, indicatorPos.y - 1),
                      ImVec2(indicatorPos.x + 102, indicatorPos.y + 15),
                      IM_COL32(0, 0, 0, 160));
    dl->AddText(font, 13.0f, indicatorPos, IM_COL32(0, 255, 200, 220), "UI EDIT MODE");

    // Restore computed rects back to local space
    for (auto& element : canvas->elements) {
        element.computedRect.x -= offsetX;
        element.computedRect.y -= offsetY;
    }
}

void EditorLayer::HandleUIEditorInput() {
    if (!m_World || m_UIEditCanvasEntity == ECS::INVALID_ENTITY) return;

    auto* canvas = m_World->GetComponent<GUI::UICanvasComponent>(m_UIEditCanvasEntity);
    if (!canvas) {
        m_UIEditMode = false;
        return;
    }

    ImGuiIO& io = ImGui::GetIO();
    f32 mouseX = io.MousePos.x;
    f32 mouseY = io.MousePos.y;

    // Guard: only process when mouse is within Game View bounds
    bool inGameView = (mouseX >= m_GameViewImageMinX && mouseX <= m_GameViewImageMaxX &&
                       mouseY >= m_GameViewImageMinY && mouseY <= m_GameViewImageMaxY);
    if (!inGameView && !ImGui::IsMouseDown(0)) {
        // Allow finishing drag outside game view but don't start new interactions
        if (m_UIEditDragMode != UIEditDragMode::None && !ImGui::IsMouseDown(0)) {
            m_UIEditDragMode = UIEditDragMode::None;
        }
        return;
    }

    f32 gvW = m_GameViewImageMaxX - m_GameViewImageMinX;
    f32 gvH = m_GameViewImageMaxY - m_GameViewImageMinY;
    if (gvW <= 0 || gvH <= 0) return;

    // Compute layout to get current rects in local space (0..gvW)
    m_UISystem.ComputeLayoutForCanvas(*canvas, gvW, gvH);

    f32 localX = mouseX - m_GameViewImageMinX;
    f32 localY = mouseY - m_GameViewImageMinY;

    // Convert mouse delta to design-space pixels
    f32 scaleFactor = std::min(gvW / canvas->designWidth, gvH / canvas->designHeight);
    if (canvas->scaleMode == GUI::UIScaleMode::ConstantPixelSize) scaleFactor = 1.0f;

    // Handle active drag
    if (m_UIEditDragMode != UIEditDragMode::None && ImGui::IsMouseDown(0)) {
        auto* dragElem = canvas->GetElement(m_UIEditSelectedElementId);
        if (dragElem) {
            f32 deltaX = (mouseX - m_UIEditDragStart.x) / scaleFactor;
            f32 deltaY = (mouseY - m_UIEditDragStart.y) / scaleFactor;

            const auto& startAnchor = m_UIEditDragStartAnchor;

            if (m_UIEditDragMode == UIEditDragMode::Move) {
                dragElem->anchor.offsetLeft   = startAnchor.offsetLeft   + deltaX;
                dragElem->anchor.offsetRight  = startAnchor.offsetRight  + deltaX;
                dragElem->anchor.offsetTop    = startAnchor.offsetTop    + deltaY;
                dragElem->anchor.offsetBottom = startAnchor.offsetBottom + deltaY;
            } else {
                // Resize — apply delta to specific anchor offsets
                bool affectsLeft   = (m_UIEditDragMode == UIEditDragMode::ResizeLeft ||
                                      m_UIEditDragMode == UIEditDragMode::ResizeTL ||
                                      m_UIEditDragMode == UIEditDragMode::ResizeBL);
                bool affectsRight  = (m_UIEditDragMode == UIEditDragMode::ResizeRight ||
                                      m_UIEditDragMode == UIEditDragMode::ResizeTR ||
                                      m_UIEditDragMode == UIEditDragMode::ResizeBR);
                bool affectsTop    = (m_UIEditDragMode == UIEditDragMode::ResizeTop ||
                                      m_UIEditDragMode == UIEditDragMode::ResizeTL ||
                                      m_UIEditDragMode == UIEditDragMode::ResizeTR);
                bool affectsBottom = (m_UIEditDragMode == UIEditDragMode::ResizeBottom ||
                                      m_UIEditDragMode == UIEditDragMode::ResizeBL ||
                                      m_UIEditDragMode == UIEditDragMode::ResizeBR);

                if (affectsLeft)   dragElem->anchor.offsetLeft   = startAnchor.offsetLeft   + deltaX;
                if (affectsRight)  dragElem->anchor.offsetRight  = startAnchor.offsetRight  + deltaX;
                if (affectsTop)    dragElem->anchor.offsetTop    = startAnchor.offsetTop    + deltaY;
                if (affectsBottom) dragElem->anchor.offsetBottom = startAnchor.offsetBottom + deltaY;
            }
        }
        return;
    }

    // End drag — commit undo command
    if (m_UIEditDragMode != UIEditDragMode::None && !ImGui::IsMouseDown(0)) {
        auto* dragElem = canvas->GetElement(m_UIEditSelectedElementId);
        if (dragElem) {
            m_UndoRedo.Execute(std::make_unique<UIAnchorEditCommand>(
                m_World, m_UIEditCanvasEntity, m_UIEditSelectedElementId,
                m_UIEditDragStartAnchor, dragElem->anchor,
                m_UIEditDragMode == UIEditDragMode::Move ? "UI Move" : "UI Resize"));
        }
        m_UIEditDragMode = UIEditDragMode::None;
        return;
    }

    // Cursor feedback for resize handles on selected element
    auto* selElem = canvas->GetElement(m_UIEditSelectedElementId);
    if (selElem && !ImGui::IsMouseDown(0)) {
        UIEditDragMode handleHit = UIEditorHitTestHandles(localX, localY, selElem->computedRect);
        switch (handleHit) {
            case UIEditDragMode::ResizeTL: case UIEditDragMode::ResizeBR:
                ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNWSE); break;
            case UIEditDragMode::ResizeTR: case UIEditDragMode::ResizeBL:
                ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNESW); break;
            case UIEditDragMode::ResizeLeft: case UIEditDragMode::ResizeRight:
                ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW); break;
            case UIEditDragMode::ResizeTop: case UIEditDragMode::ResizeBottom:
                ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS); break;
            default: break;
        }
    }

    // Left click — start drag or select
    if (ImGui::IsMouseClicked(0) && inGameView) {
        // First check resize handles on selected element
        if (selElem) {
            UIEditDragMode handleHit = UIEditorHitTestHandles(localX, localY, selElem->computedRect);
            if (handleHit != UIEditDragMode::None) {
                m_UIEditDragMode = handleHit;
                m_UIEditDragStart = ImVec2(mouseX, mouseY);
                m_UIEditDragStartAnchor = selElem->anchor;
                return;
            }
        }

        // Hit test elements in reverse order (front-to-back)
        bool hitAny = false;
        for (i32 i = static_cast<i32>(canvas->elements.size()) - 1; i >= 0; --i) {
            auto& elem = canvas->elements[static_cast<usize>(i)];
            if (!elem.visible) continue;
            if (elem.computedRect.Contains(localX, localY)) {
                m_UIEditSelectedElementId = elem.id;
                hitAny = true;

                // Start move drag
                m_UIEditDragMode = UIEditDragMode::Move;
                m_UIEditDragStart = ImVec2(mouseX, mouseY);
                m_UIEditDragStartAnchor = elem.anchor;
                break;
            }
        }

        if (!hitAny) {
            m_UIEditSelectedElementId = 0;
        }
    }

    // --- Keyboard shortcuts (only when Game View is hovered and no ImGui text input is active) ---
    if (inGameView && !io.WantTextInput) {
        auto* kbSel = canvas->GetElement(m_UIEditSelectedElementId);

        // Delete — remove selected element (undoable)
        if (kbSel && ImGui::IsKeyPressed(ImGuiKey_Delete)) {
            m_UndoRedo.Execute(std::make_unique<UIElementDeleteCommand>(
                m_World, m_UIEditCanvasEntity, *kbSel));
            m_UIEditSelectedElementId = 0;
        }

        // Ctrl+D — duplicate selected element
        if (kbSel && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_D)) {
            u32 newId = canvas->DuplicateElement(m_UIEditSelectedElementId);
            if (newId) m_UIEditSelectedElementId = newId;
        }

        // Arrow keys — nudge selected element (1px, or 10px with Shift)
        if (kbSel) {
            f32 nudge = io.KeyShift ? 10.0f : 1.0f;
            GUI::UIAnchor oldAnchor = kbSel->anchor;
            bool nudged = false;

            if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) {
                kbSel->anchor.offsetLeft  -= nudge;
                kbSel->anchor.offsetRight -= nudge;
                nudged = true;
            }
            if (ImGui::IsKeyPressed(ImGuiKey_RightArrow)) {
                kbSel->anchor.offsetLeft  += nudge;
                kbSel->anchor.offsetRight += nudge;
                nudged = true;
            }
            if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) {
                kbSel->anchor.offsetTop    -= nudge;
                kbSel->anchor.offsetBottom -= nudge;
                nudged = true;
            }
            if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) {
                kbSel->anchor.offsetTop    += nudge;
                kbSel->anchor.offsetBottom += nudge;
                nudged = true;
            }
            if (nudged) {
                m_UndoRedo.Execute(std::make_unique<UIAnchorEditCommand>(
                    m_World, m_UIEditCanvasEntity, m_UIEditSelectedElementId,
                    oldAnchor, kbSel->anchor, "UI Nudge"));
            }
        }

        // Escape — deselect
        if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            m_UIEditSelectedElementId = 0;
        }
    }

    // Right-click context menu — add new elements at click position
    if (ImGui::IsMouseClicked(1) && inGameView) {
        ImGui::OpenPopup("UIEditorContextMenu");
    }

    if (ImGui::BeginPopup("UIEditorContextMenu")) {
        f32 designX, designY;
        UIEditorScreenToDesign(mouseX, mouseY, designX, designY);

        ImGui::TextDisabled("Add UI Element");
        ImGui::Separator();

        auto addAtPosition = [&](GUI::UIWidgetType type, const char* name, f32 defaultW, f32 defaultH) {
            u32 id = canvas->AddElement(type, name);
            auto* newElem = canvas->GetElement(id);
            if (newElem) {
                newElem->anchor.anchorMin = Math::Vector2(0.0f, 0.0f);
                newElem->anchor.anchorMax = Math::Vector2(0.0f, 0.0f);
                newElem->anchor.offsetLeft   = designX;
                newElem->anchor.offsetRight  = designX + defaultW;
                newElem->anchor.offsetTop    = designY;
                newElem->anchor.offsetBottom = designY + defaultH;
                if (type == GUI::UIWidgetType::Button) newElem->data.text = "Button";
                if (type == GUI::UIWidgetType::Label)  newElem->data.text = "Label";
                m_UIEditSelectedElementId = id;
            }
        };

        if (ImGui::MenuItem("Panel"))       addAtPosition(GUI::UIWidgetType::Panel,       "Panel",       200, 150);
        if (ImGui::MenuItem("Button"))      addAtPosition(GUI::UIWidgetType::Button,      "Button",      160, 40);
        if (ImGui::MenuItem("Label"))       addAtPosition(GUI::UIWidgetType::Label,       "Label",       200, 30);
        if (ImGui::MenuItem("Image"))       addAtPosition(GUI::UIWidgetType::Image,       "Image",       100, 100);
        if (ImGui::MenuItem("ProgressBar")) addAtPosition(GUI::UIWidgetType::ProgressBar, "ProgressBar", 200, 24);
        if (ImGui::MenuItem("Slider"))      addAtPosition(GUI::UIWidgetType::Slider,      "Slider",      200, 30);
        if (ImGui::MenuItem("Checkbox"))    addAtPosition(GUI::UIWidgetType::Checkbox,    "Checkbox",    120, 24);
        if (ImGui::MenuItem("Toggle"))      addAtPosition(GUI::UIWidgetType::Toggle,      "Toggle",      120, 24);
        ImGui::EndPopup();
    }
}

// ============================================================================
// PARTICLE EDITOR PANEL
// ============================================================================


void EditorLayer::HandleKeyboardGizmoNudge() {
    if (m_PrimarySelected == ECS::INVALID_ENTITY || !m_World) return;
    if (!m_World->IsValid(m_PrimarySelected)) return;

    // Don't nudge entities locked by others
    if (m_SceneLockManager.IsEntityLockedByOther(m_PrimarySelected)) return;

    auto* transform = m_World->GetComponent<ECS::TransformComponent>(m_PrimarySelected);
    if (!transform) return;

    bool ctrl = Input::IsKeyDown(KeyCode::LeftControl);
    bool anyArrow = false;
    Math::Vector3 nudge(0, 0, 0);
    f32 amount = ctrl ? m_EditorSettings.gizmoNudgeFine : m_EditorSettings.gizmoNudgeAmount;

    if (m_GizmoOperation == GizmoOperation::Translate) {
        if (Input::IsKeyPressed(KeyCode::Left))  { nudge.x -= amount; anyArrow = true; }
        if (Input::IsKeyPressed(KeyCode::Right)) { nudge.x += amount; anyArrow = true; }
        if (Input::IsKeyPressed(KeyCode::Up))    { nudge.z -= amount; anyArrow = true; }
        if (Input::IsKeyPressed(KeyCode::Down))  { nudge.z += amount; anyArrow = true; }
        // Page Up/Down for Y axis
        if (Input::IsKeyPressed(KeyCode::PageUp))   { nudge.y += amount; anyArrow = true; }
        if (Input::IsKeyPressed(KeyCode::PageDown))  { nudge.y -= amount; anyArrow = true; }
    } else if (m_GizmoOperation == GizmoOperation::Rotate) {
        f32 deg = ctrl ? 1.0f : m_EditorSettings.gizmoRotateNudge;
        if (Input::IsKeyPressed(KeyCode::Left))  { transform->rotation.y -= deg; anyArrow = true; }
        if (Input::IsKeyPressed(KeyCode::Right)) { transform->rotation.y += deg; anyArrow = true; }
        if (Input::IsKeyPressed(KeyCode::Up))    { transform->rotation.x -= deg; anyArrow = true; }
        if (Input::IsKeyPressed(KeyCode::Down))  { transform->rotation.x += deg; anyArrow = true; }
    } else if (m_GizmoOperation == GizmoOperation::Scale) {
        f32 scaleStep = ctrl ? 0.01f : 0.1f;
        if (Input::IsKeyPressed(KeyCode::Left))  { transform->scale.x -= scaleStep; anyArrow = true; }
        if (Input::IsKeyPressed(KeyCode::Right)) { transform->scale.x += scaleStep; anyArrow = true; }
        if (Input::IsKeyPressed(KeyCode::Up))    { transform->scale.y += scaleStep; anyArrow = true; }
        if (Input::IsKeyPressed(KeyCode::Down))  { transform->scale.y -= scaleStep; anyArrow = true; }
        if (Input::IsKeyPressed(KeyCode::PageUp))   { transform->scale.z += scaleStep; anyArrow = true; }
        if (Input::IsKeyPressed(KeyCode::PageDown))  { transform->scale.z -= scaleStep; anyArrow = true; }
    }

    if (anyArrow && m_GizmoOperation == GizmoOperation::Translate) {
        transform->position = transform->position + nudge;
    }
}

// ============================================================================
// Flash Timeline Panel
// ============================================================================


} // namespace Editor
} // namespace Enjin
