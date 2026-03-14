#include "Enjin/Editor/EditorLayer.h"
#include "Enjin/Editor/InspectorUndo.h"
#include "Enjin/Editor/ScenePicker.h"
#include "Enjin/Core/Version.h"
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

// --- Undo-aware component removal helper ---

template<typename T>
void EditorLayer::RemoveComponentWithUndo(ECS::Entity entity, const std::string& componentKey,
                                           const std::string& componentName) {
    auto cmd = std::make_unique<RemoveComponentCommand>(
        m_World, entity, componentKey, componentName,
        [this, entity]() {
            if (m_World->HasComponent<T>(entity))
                m_World->RemoveComponent<T>(entity);
        }
    );
    m_UndoRedo.Execute(std::move(cmd));
    ShowNotification("Removed " + componentName + " (Ctrl+Z to undo)", NotificationType::Info);
}

void EditorLayer::EvaluatePostProcessVolumes(const Math::Vector3& cameraPosition) {
    if (!m_PostProcessing || !m_World) return;

    auto volumeEntities = m_World->GetEntitiesWithComponent<ECS::PostProcessVolumeComponent>();
    if (volumeEntities.empty()) return;

    // Save the global/panel PP settings as the base layer (restored each frame before blending)
    // The base settings come from the PostProcessing panel — we capture them once and
    // restore before blending so volume changes don't permanently modify panel values.
    auto& currentSettings = m_PostProcessing->GetSettings();

    // Collect active volumes with their blend weights
    struct VolumeEntry {
        const ECS::PostProcessVolumeComponent* vol;
        f32 blendWeight;
    };
    std::vector<VolumeEntry> activeVolumes;
    activeVolumes.reserve(volumeEntities.size());

    for (auto entity : volumeEntities) {
        auto* vol = m_World->GetComponent<ECS::PostProcessVolumeComponent>(entity);
        if (!vol || !vol->isActive) continue;

        Math::Vector3 center(0, 0, 0);
        if (!vol->isGlobal) {
            auto* transform = m_World->GetComponent<ECS::TransformComponent>(entity);
            if (!transform) continue;
            center = transform->position;
        }

        f32 w = vol->GetBlendWeight(center, cameraPosition);
        if (w <= 0.001f) continue;

        activeVolumes.push_back({ vol, w });
    }

    if (activeVolumes.empty()) return;

    // Sort by priority (lowest first — applied first, higher priority overrides)
    std::sort(activeVolumes.begin(), activeVolumes.end(),
        [](const VolumeEntry& a, const VolumeEntry& b) {
            return a.vol->priority < b.vol->priority;
        });

    // Start from current global settings and blend each volume on top
    Renderer::PostProcessSettings blended = currentSettings;
    for (auto& entry : activeVolumes) {
        ECS::BlendPostProcessSettings(blended, blended, entry.vol->settings,
            entry.blendWeight, entry.vol->overrideMask);
    }

    // Apply blended result
    currentSettings = blended;
}


void EditorLayer::DrawPostProcessVolumeComponent(ECS::Entity entity) {
    if (ImGui::CollapsingHeader("[PP] Post-Process Volume", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto* vol = m_World->GetComponent<ECS::PostProcessVolumeComponent>(entity);
        if (!vol) return;

        InspectorUndo::Checkbox(m_UndoRedo, "Active##PPVol", &vol->isActive);
        InspectorUndo::Checkbox(m_UndoRedo, "Global##PPVol", &vol->isGlobal);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Global volumes apply everywhere (ignore shape/position).\n"
                              "Use for scene-wide base settings.");
        }

        if (!vol->isGlobal) {
            // Shape
            const char* shapes[] = { "Box", "Sphere" };
            int shapeIdx = static_cast<int>(vol->shape);
            if (InspectorUndo::Combo(m_UndoRedo, "Shape##PPVol", &shapeIdx, shapes, 2)) {
                vol->shape = static_cast<ECS::PPVolumeShape>(shapeIdx);
            }

            // Half-extents / Radius
            if (vol->shape == ECS::PPVolumeShape::Sphere) {
                f32 radius = vol->halfExtents.x;
                if (InspectorUndo::DragFloat(m_UndoRedo, "Radius##PPVol", &radius, 0.5f, 0.1f, 500.0f)) {
                    vol->halfExtents = Math::Vector3(radius, radius, radius);
                }
            } else {
                f32 halfExt[3] = { vol->halfExtents.x, vol->halfExtents.y, vol->halfExtents.z };
                if (InspectorUndo::DragFloat3(m_UndoRedo, "Half Extents##PPVol", halfExt,
                        [vol](f32 x, f32 y, f32 z) { vol->halfExtents = Math::Vector3(x, y, z); },
                        0.5f, 0.1f, 500.0f)) {
                    vol->halfExtents = Math::Vector3(halfExt[0], halfExt[1], halfExt[2]);
                }
            }

            InspectorUndo::DragFloat(m_UndoRedo, "Blend Radius##PPVol", &vol->blendRadius,
                0.1f, 0.0f, 50.0f, "%.1f");
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Smooth transition distance at volume edges (world units).\n"
                                  "0 = hard boundary, larger = softer fade.");
            }
        }

        InspectorUndo::DragFloat(m_UndoRedo, "Weight##PPVol", &vol->weight,
            0.01f, 0.0f, 1.0f, "%.2f");
        ImGui::DragInt("Priority##PPVol", &vol->priority, 1, -100, 100);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Higher priority volumes blend on top of lower ones.");
        }

        ImGui::Separator();

        // Override mask checkboxes
        if (ImGui::TreeNode("Override Groups##PPVol")) {
            bool allOverride = vol->overrideMask == ECS::PostProcessVolumeComponent::OverrideAll;
            if (ImGui::Checkbox("Override All", &allOverride)) {
                vol->overrideMask = allOverride ? ECS::PostProcessVolumeComponent::OverrideAll : 0;
            }
            if (!allOverride) {
                auto checkGroup = [&](const char* label, u32 bit) {
                    bool on = (vol->overrideMask & bit) != 0;
                    if (ImGui::Checkbox(label, &on)) {
                        if (on) vol->overrideMask |= bit;
                        else vol->overrideMask &= ~bit;
                    }
                };
                checkGroup("Tone Mapping", ECS::PostProcessVolumeComponent::OverrideToneMapping);
                checkGroup("Bloom", ECS::PostProcessVolumeComponent::OverrideBloom);
                checkGroup("Vignette", ECS::PostProcessVolumeComponent::OverrideVignette);
                checkGroup("Chromatic Aberration", ECS::PostProcessVolumeComponent::OverrideChromaticAberr);
                checkGroup("Color Grading", ECS::PostProcessVolumeComponent::OverrideColorGrading);
                checkGroup("Film Grain", ECS::PostProcessVolumeComponent::OverrideFilmGrain);
                checkGroup("FXAA", ECS::PostProcessVolumeComponent::OverrideFXAA);
                checkGroup("Dithering", ECS::PostProcessVolumeComponent::OverrideDither);
                checkGroup("Color Quantize", ECS::PostProcessVolumeComponent::OverrideColorQuant);
                checkGroup("Res Downscale", ECS::PostProcessVolumeComponent::OverrideResDownscale);
                checkGroup("CRT", ECS::PostProcessVolumeComponent::OverrideCRT);
                checkGroup("CRT Phosphor", ECS::PostProcessVolumeComponent::OverrideCRTPhosphor);
                checkGroup("LUT", ECS::PostProcessVolumeComponent::OverrideLUT);
                checkGroup("VHS", ECS::PostProcessVolumeComponent::OverrideVHS);
                checkGroup("Palette Lock", ECS::PostProcessVolumeComponent::OverridePalette);
                checkGroup("Depth of Field", ECS::PostProcessVolumeComponent::OverrideDoF);
                checkGroup("Tilt-Shift", ECS::PostProcessVolumeComponent::OverrideTiltShift);
                checkGroup("Cel Outline", ECS::PostProcessVolumeComponent::OverrideCelOutline);
                checkGroup("Stipple", ECS::PostProcessVolumeComponent::OverrideStipple);
                checkGroup("God Rays", ECS::PostProcessVolumeComponent::OverrideGodRays);
                checkGroup("SSAO", ECS::PostProcessVolumeComponent::OverrideSSAO);
                checkGroup("Contact Shadows", ECS::PostProcessVolumeComponent::OverrideContactShadows);
                checkGroup("Caustics", ECS::PostProcessVolumeComponent::OverrideCaustics);
                checkGroup("Fog Shafts", ECS::PostProcessVolumeComponent::OverrideFogShafts);
            }
            ImGui::TreePop();
        }

        // Embedded PP settings
        if (ImGui::TreeNode("Post-Process Settings##PPVol")) {
            auto& s = vol->settings;

            // Tone mapping
            if (ImGui::TreeNode("Tone Mapping##PPVolSet")) {
                const char* modes[] = { "None", "Reinhard", "Reinhard Ext", "ACES", "Uncharted 2", "AgX" };
                int mode = static_cast<int>(s.toneMappingMode);
                if (ImGui::Combo("Mode##PPVolTM", &mode, modes, 6)) s.toneMappingMode = static_cast<u32>(mode);
                ImGui::DragFloat("Exposure##PPVolTM", &s.exposure, 0.01f, 0.0f, 10.0f);
                ImGui::DragFloat("Gamma##PPVolTM", &s.gamma, 0.01f, 0.1f, 5.0f);
                ImGui::DragFloat("White Point##PPVolTM", &s.whitePoint, 0.1f, 0.1f, 20.0f);
                ImGui::TreePop();
            }

            // Bloom
            if (ImGui::TreeNode("Bloom##PPVolSet")) {
                bool bloom = s.bloomEnabled != 0;
                if (ImGui::Checkbox("Enabled##PPVolBloom", &bloom)) s.bloomEnabled = bloom ? 1 : 0;
                ImGui::DragFloat("Threshold##PPVolBloom", &s.bloomThreshold, 0.01f, 0.0f, 10.0f);
                ImGui::DragFloat("Intensity##PPVolBloom", &s.bloomIntensity, 0.01f, 0.0f, 5.0f);
                ImGui::DragFloat("Radius##PPVolBloom", &s.bloomRadius, 0.001f, 0.0f, 0.1f);
                ImGui::TreePop();
            }

            // Vignette
            if (ImGui::TreeNode("Vignette##PPVolSet")) {
                bool vig = s.vignetteEnabled != 0;
                if (ImGui::Checkbox("Enabled##PPVolVig", &vig)) s.vignetteEnabled = vig ? 1 : 0;
                ImGui::DragFloat("Intensity##PPVolVig", &s.vignetteIntensity, 0.01f, 0.0f, 2.0f);
                ImGui::DragFloat("Smoothness##PPVolVig", &s.vignetteSmoothness, 0.01f, 0.0f, 1.0f);
                ImGui::TreePop();
            }

            // Chromatic Aberration
            if (ImGui::TreeNode("Chromatic Aberration##PPVolSet")) {
                bool ca = s.chromaticAberrationEnabled != 0;
                if (ImGui::Checkbox("Enabled##PPVolCA", &ca)) s.chromaticAberrationEnabled = ca ? 1 : 0;
                ImGui::DragFloat("Intensity##PPVolCA", &s.chromaticAberrationIntensity, 0.001f, 0.0f, 0.1f);
                ImGui::TreePop();
            }

            // Color Grading
            if (ImGui::TreeNode("Color Grading##PPVolSet")) {
                f32 col[3] = { s.colorFilter.x, s.colorFilter.y, s.colorFilter.z };
                if (ImGui::ColorEdit3("Color Filter##PPVolCG", col)) {
                    s.colorFilter = Math::Vector3(col[0], col[1], col[2]);
                }
                ImGui::DragFloat("Saturation##PPVolCG", &s.saturation, 0.01f, 0.0f, 3.0f);
                ImGui::DragFloat("Contrast##PPVolCG", &s.contrast, 0.01f, 0.0f, 3.0f);
                ImGui::DragFloat("Brightness##PPVolCG", &s.brightness, 0.01f, -1.0f, 1.0f);
                ImGui::TreePop();
            }

            // Film Grain
            if (ImGui::TreeNode("Film Grain##PPVolSet")) {
                bool fg = s.filmGrainEnabled != 0;
                if (ImGui::Checkbox("Enabled##PPVolFG", &fg)) s.filmGrainEnabled = fg ? 1 : 0;
                ImGui::DragFloat("Intensity##PPVolFG", &s.filmGrainIntensity, 0.001f, 0.0f, 0.5f);
                ImGui::TreePop();
            }

            // FXAA
            if (ImGui::TreeNode("FXAA##PPVolSet")) {
                bool fxaa = s.fxaaEnabled != 0;
                if (ImGui::Checkbox("Enabled##PPVolFXAA", &fxaa)) s.fxaaEnabled = fxaa ? 1 : 0;
                ImGui::TreePop();
            }

            // Depth of Field
            if (ImGui::TreeNode("Depth of Field##PPVolSet")) {
                bool dof = s.dofEnabled != 0;
                if (ImGui::Checkbox("Enabled##PPVolDoF", &dof)) s.dofEnabled = dof ? 1 : 0;
                ImGui::DragFloat("Focal Distance##PPVolDoF", &s.dofFocalDistance, 0.1f, 0.1f, 500.0f);
                ImGui::DragFloat("Focal Range##PPVolDoF", &s.dofFocalRange, 0.1f, 0.1f, 100.0f);
                ImGui::DragFloat("Near Blur##PPVolDoF", &s.dofNearBlurStrength, 0.01f, 0.0f, 1.0f);
                ImGui::DragFloat("Far Blur##PPVolDoF", &s.dofFarBlurStrength, 0.01f, 0.0f, 1.0f);
                ImGui::DragFloat("Bokeh Size##PPVolDoF", &s.dofBokehSize, 0.1f, 0.0f, 20.0f);
                ImGui::TreePop();
            }

            // Tilt-Shift
            if (ImGui::TreeNode("Tilt-Shift##PPVolSet")) {
                bool ts = s.tiltShiftEnabled != 0;
                if (ImGui::Checkbox("Enabled##PPVolTS", &ts)) s.tiltShiftEnabled = ts ? 1 : 0;
                ImGui::DragFloat("Focus Y##PPVolTS", &s.tiltShiftFocusY, 0.01f, 0.0f, 1.0f);
                ImGui::DragFloat("Band Width##PPVolTS", &s.tiltShiftBandWidth, 0.01f, 0.0f, 1.0f);
                ImGui::DragFloat("Blur Amount##PPVolTS", &s.tiltShiftBlurAmount, 0.1f, 0.0f, 10.0f);
                ImGui::TreePop();
            }

            // Cel Outline
            if (ImGui::TreeNode("Cel Outline##PPVolSet")) {
                bool cel = s.celOutlineEnabled != 0;
                if (ImGui::Checkbox("Enabled##PPVolCel", &cel)) s.celOutlineEnabled = cel ? 1 : 0;
                ImGui::DragFloat("Thickness##PPVolCel", &s.celOutlineThickness, 0.1f, 0.1f, 10.0f);
                ImGui::DragFloat("Threshold##PPVolCel", &s.celOutlineThreshold, 0.01f, 0.0f, 1.0f);
                f32 outColor[3] = { s.celOutlineColor.x, s.celOutlineColor.y, s.celOutlineColor.z };
                if (ImGui::ColorEdit3("Color##PPVolCel", outColor)) {
                    s.celOutlineColor = Math::Vector3(outColor[0], outColor[1], outColor[2]);
                }
                ImGui::TreePop();
            }

            ImGui::TreePop();
        }

        if (ImGui::Button("Remove##PostProcessVolume")) {
            RemoveComponentWithUndo<ECS::PostProcessVolumeComponent>(entity, "postProcessVolume", "Post-Process Volume");
        }
    }
}


void EditorLayer::DrawGameViewPanel() {
    // Set window to be larger by default for Game View
    ImGui::SetNextWindowSize(ImVec2(640, 480), ImGuiCond_FirstUseEver);

    // Add a colored title bar when playing, and lock the window so clicks go to the game
    bool isPlaying = m_PlayMode.IsPlaying();
    bool isPlayActive = !m_PlayMode.IsStopped();
    if (isPlaying) {
        ImGui::PushStyleColor(ImGuiCol_TitleBg, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_TitleBgActive, ImVec4(0.3f, 0.7f, 0.3f, 1.0f));
    }

    ImGuiWindowFlags gameViewFlags = 0;
    if (isPlayActive) {
        gameViewFlags |= ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize;
    }
    ImGui::Begin("Game View", nullptr, gameViewFlags);

    if (isPlaying) {
        ImGui::PopStyleColor(2);
    }

    // Gather all camera entities in the scene
    std::vector<ECS::Entity> cameraEntities;
    if (m_World) {
        for (ECS::Entity entity : m_World->GetEntitiesWithComponent<ECS::CameraComponent>()) {
            cameraEntities.push_back(entity);
        }
    }

    // Validate current selection
    if (m_SelectedGameCamera != ECS::INVALID_ENTITY) {
        bool found = false;
        for (ECS::Entity e : cameraEntities) {
            if (e == m_SelectedGameCamera) { found = true; break; }
        }
        if (!found) m_SelectedGameCamera = ECS::INVALID_ENTITY;
    }

    // Auto-select first camera if nothing selected
    if (m_SelectedGameCamera == ECS::INVALID_ENTITY && !cameraEntities.empty()) {
        m_SelectedGameCamera = cameraEntities[0];
    }

    ECS::Entity gameCameraEntity = m_SelectedGameCamera;
    ECS::CameraComponent* gameCameraComp = nullptr;
    ECS::TransformComponent* gameCameraTransform = nullptr;
    if (gameCameraEntity != ECS::INVALID_ENTITY && m_World) {
        gameCameraComp = m_World->GetComponent<ECS::CameraComponent>(gameCameraEntity);
        gameCameraTransform = m_World->GetComponent<ECS::TransformComponent>(gameCameraEntity);
    }

    // Show play mode status
    if (isPlaying) {
        ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "PLAYING");
        ImGui::SameLine();
        if (ImGui::Button("Stop")) {
            // Defer stop to next Update — calling Stop mid-Render destroys
            // the world and invalidates all entity/component pointers held
            // by this frame's local variables and already-recorded GPU commands.
            m_PendingPlayStop = true;
        }
    } else if (m_PlayMode.IsPaused()) {
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.3f, 1.0f), "PAUSED");
        ImGui::SameLine();
        if (ImGui::Button("Resume")) {
            StartPlayMode();
        }
        ImGui::SameLine();
        if (ImGui::Button("Stop")) {
            m_PendingPlayStop = true;
        }
    } else {
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "STOPPED");
        ImGui::SameLine();
        if (ImGui::Button("Play")) {
            m_PrePlayRenderSettings = Renderer::SceneRenderSettings::CaptureFromRuntime(
                m_RenderSystem, m_PostProcessing ? &m_PostProcessing->GetSettings() : nullptr);
            StartPlayMode();
            if (m_EditorSettings.autoFocusMode) {
                m_FocusMode = true;
                Input::SetMouseCaptured(true);
            }
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Focus (F11)")) {
        m_FocusMode = true;
        if (m_PlayMode.IsStopped()) {
            m_PrePlayRenderSettings = Renderer::SceneRenderSettings::CaptureFromRuntime(
                m_RenderSystem, m_PostProcessing ? &m_PostProcessing->GetSettings() : nullptr);
            StartPlayMode();
        }
        Input::SetMouseCaptured(true);
    }

    // Game View frame rate controls (right side)
    ImGui::SameLine(ImGui::GetWindowWidth() - 220);
    ImGui::SetNextItemWidth(80);
    const char* fpsOptions[] = { "Max", "24", "30", "60", "120", "144", "240" };
    ImGui::Combo("##GameFPS", &m_GameViewFPSIndex, fpsOptions, 7);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Game View frame rate limit");
    }
    ImGui::SameLine();
    if (ImGui::Checkbox("VSync##GameView", &m_GameViewVSync)) {
        // VSync overrides FPS to ~60
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Simulate VSync (caps game view to ~60 FPS)");
    }

    // (Evaluate Flower button moved to game view overlay — see DrawFlowerEvaluateOverlay)

    // Camera selector dropdown (when multiple cameras exist)
    if (cameraEntities.size() > 1) {
        std::string currentName = "None";
        if (m_SelectedGameCamera != ECS::INVALID_ENTITY && m_World->HasComponent<ECS::NameComponent>(m_SelectedGameCamera)) {
            currentName = m_World->GetComponent<ECS::NameComponent>(m_SelectedGameCamera)->name;
        } else if (m_SelectedGameCamera != ECS::INVALID_ENTITY) {
            currentName = "Camera (Entity " + std::to_string(m_SelectedGameCamera) + ")";
        }

        ImGui::SetNextItemWidth(200);
        if (ImGui::BeginCombo("Camera", currentName.c_str())) {
            for (ECS::Entity camEntity : cameraEntities) {
                std::string name;
                if (m_World->HasComponent<ECS::NameComponent>(camEntity)) {
                    name = m_World->GetComponent<ECS::NameComponent>(camEntity)->name;
                } else {
                    name = "Camera (Entity " + std::to_string(camEntity) + ")";
                }

                bool isSelected = (camEntity == m_SelectedGameCamera);
                if (ImGui::Selectable(name.c_str(), isSelected)) {
                    m_SelectedGameCamera = camEntity;
                    gameCameraEntity = camEntity;
                    gameCameraComp = m_World->GetComponent<ECS::CameraComponent>(camEntity);
                    gameCameraTransform = m_World->GetComponent<ECS::TransformComponent>(camEntity);
                }
                if (isSelected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
    }

    ImGui::Separator();

    if (gameCameraEntity == ECS::INVALID_ENTITY) {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.3f, 1.0f), "No Camera Found!");
        ImGui::TextWrapped("Add a Camera component to an entity to see the game view.");
        ImGui::Spacing();
        ImGui::TextWrapped("Go to Entity > Create Empty, then Add Component > Camera.");

        // Quick button to create a camera
        if (m_World && ImGui::Button("Create Game Camera")) {
            ECS::Entity camEntity = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(camEntity, "Game Camera");
            auto& transform = m_World->AddComponent<ECS::TransformComponent>(camEntity);
            transform.position = Math::Vector3(0, 2, 5);
            // Looking at origin (rotate 180 degrees around Y axis)
            transform.rotation = Math::Quaternion(Math::Vector3(0, 1, 0), Math::Radians(180.0f));
            auto& cam = m_World->AddComponent<ECS::CameraComponent>(camEntity);
            cam.isActive = true;
            cam.priority = 10;  // High priority to be the main camera
            cam.fieldOfView = 60.0f;
            cam.nearPlane = 0.1f;
            cam.farPlane = 1000.0f;
            SelectEntity(camEntity);
            ENJIN_LOG_INFO(Editor, "Created game camera entity");
        }
    } else {
        // Show camera info
        std::string cameraName = "Game Camera";
        if (m_World->HasComponent<ECS::NameComponent>(gameCameraEntity)) {
            cameraName = m_World->GetComponent<ECS::NameComponent>(gameCameraEntity)->name;
        }

        ImGui::Text("Camera: %s", cameraName.c_str());

        if (gameCameraComp) {
            ImGui::Text("FOV: %.1f", gameCameraComp->fieldOfView);
            ImGui::Text("Near: %.2f  Far: %.1f", gameCameraComp->nearPlane, gameCameraComp->farPlane);
            ImGui::Text("Priority: %d  Active: %s", gameCameraComp->priority, gameCameraComp->isActive ? "Yes" : "No");
        }

        if (gameCameraTransform) {
            ImGui::Text("Position: %.2f, %.2f, %.2f",
                gameCameraTransform->position.x,
                gameCameraTransform->position.y,
                gameCameraTransform->position.z);
        }

        ImGui::Separator();

        // Game View Preview
        ImVec2 availSize = ImGui::GetContentRegionAvail();
        if (availSize.x > 0 && availSize.y > 0) {
            // Calculate aspect ratio for preview area (default 16:9)
            f32 gameAspect = 16.0f / 9.0f;
            f32 previewWidth = availSize.x;
            f32 previewHeight = previewWidth / gameAspect;
            if (previewHeight > availSize.y) {
                previewHeight = availSize.y;
                previewWidth = previewHeight * gameAspect;
            }

            // Update desired render target size (actual resize deferred to RenderOffscreen)
            u32 targetW = static_cast<u32>(previewWidth);
            u32 targetH = static_cast<u32>(previewHeight);
            if (targetW > 0 && targetH > 0) {
                m_GameViewWidth = targetW;
                m_GameViewHeight = targetH;
            }

            // Center the preview
            ImVec2 pos = ImGui::GetCursorScreenPos();
            ImVec2 center((availSize.x - previewWidth) * 0.5f, 0);
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + center.x);

            ImDrawList* drawList = ImGui::GetWindowDrawList();
            ImVec2 p0(pos.x + center.x, pos.y);
            ImVec2 p1(p0.x + previewWidth, p0.y + previewHeight);

            // Display render target texture or fallback dark rect
            VkDescriptorSet texId = m_GameViewRenderTarget ? m_GameViewRenderTarget->GetImGuiTextureID() : VK_NULL_HANDLE;
            bool usedImage = false;
            if (texId != VK_NULL_HANDLE) {
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + center.x);
                ImGui::Image(static_cast<ImTextureID>(reinterpret_cast<uintptr_t>(texId)),
                             ImVec2(previewWidth, previewHeight));
                usedImage = true;
                m_GameViewImageMinX = p0.x;
                m_GameViewImageMinY = p0.y;
                m_GameViewImageMaxX = p1.x;
                m_GameViewImageMaxY = p1.y;
                m_GameViewHovered = ImGui::IsItemHovered();
                // Drop target: accept asset drags onto Game View
                if (ImGui::BeginDragDropTarget()) {
                    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
                        std::string dropPath(static_cast<const char*>(payload->Data));
                        std::filesystem::path fp(dropPath);
                        std::string ext = fp.extension().string();
                        std::transform(ext.begin(), ext.end(), ext.begin(),
                            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

                        if (ext == ".gltf" || ext == ".glb" || ext == ".fbx" ||
                            ext == ".obj" || ext == ".dae" || ext == ".3ds") {
                            ImportModel(dropPath);
                        } else if (ext == ".enjprefab") {
                            auto prefab = Assets::PrefabManager::Get().LoadPrefab(dropPath);
                            if (prefab) {
                                ECS::Entity root = Assets::PrefabManager::Get().Instantiate(m_World, *prefab);
                                if (root != ECS::INVALID_ENTITY) SelectEntity(root);
                            }
                        } else if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" ||
                                   ext == ".tga" || ext == ".bmp" || ext == ".svg") {
                            ECS::Entity sel = m_PrimarySelected;
                            if (sel != ECS::INVALID_ENTITY && m_World) {
                                auto* mat = m_World->GetComponent<ECS::MaterialComponent>(sel);
                                if (mat) {
                                    mat->baseColorTexturePath = dropPath;
                                    mat->baseColorTexture = -1;
                                    if (m_RenderSystem) m_RenderSystem->ClearFailedTexture(dropPath);
                                } else {
                                    auto* spr = m_World->GetComponent<ECS::Sprite2DComponent>(sel);
                                    if (spr) spr->texturePath = dropPath;
                                }
                            }
                        } else if (ext == ".enjin" || ext == ".json") {
                            OpenScene(dropPath);
                        } else if (ext == ".wav" || ext == ".mp3" || ext == ".ogg" || ext == ".flac") {
                            ECS::Entity sel = m_PrimarySelected;
                            if (sel != ECS::INVALID_ENTITY && m_World) {
                                auto* audio = m_World->GetComponent<ECS::AudioSourceComponent>(sel);
                                if (audio) audio->clipPath = dropPath;
                            }
                        } else if (ext == ".as") {
                            ECS::Entity sel = m_PrimarySelected;
                            if (sel != ECS::INVALID_ENTITY && m_World) {
                                auto* script = m_World->GetComponent<ECS::ScriptComponent>(sel);
                                if (script && !script->scripts.empty()) {
                                    script->scripts[0].scriptPath = dropPath;
                                }
                            }
                        }
                    }
                    ImGui::EndDragDropTarget();
                }
            } else {
                drawList->AddRectFilled(p0, p1, IM_COL32(20, 20, 30, 255));
            }

            // Weather/grass/fog are now rendered in RenderOffscreen() inside the render target pass.
            // Here we only draw ImGui overlays (lightning flash, water).

            // Find weather zone for lightning overlay
            ECS::WeatherZoneComponent* activeWeatherZone = nullptr;
            i32 bestWeatherPriority = INT_MIN;

            if (m_World && gameCameraTransform) {
                for (ECS::Entity entity : m_World->GetEntitiesWithComponent<ECS::WeatherZoneComponent>()) {
                    auto* zone = m_World->GetComponent<ECS::WeatherZoneComponent>(entity);
                    auto* zoneTransform = m_World->GetComponent<ECS::TransformComponent>(entity);
                    if (zone && zoneTransform && zone->priority > bestWeatherPriority) {
                        if (zone->ContainsPoint(zoneTransform->position, gameCameraTransform->position)) {
                            activeWeatherZone = zone;
                            bestWeatherPriority = zone->priority;
                        }
                    }
                }
            }

            // Lightning flash overlay (ImGui, not Vulkan)
            if (activeWeatherZone && activeWeatherZone->weatherType == 6 &&
                activeWeatherZone->lightningEnabled && m_WeatherSystem.IsLightningActive()) {
                f32 intensity = m_WeatherSystem.GetLightningIntensity();
                u8 flashAlpha = static_cast<u8>(intensity * 200.0f);
                ImU32 flashColor = IM_COL32(255, 255, 255, flashAlpha);
                drawList->AddRectFilled(p0, p1, flashColor);
            }

            // Water is now rendered as a 3D mesh in RenderToTarget (no ImGui overlay needed)

            // Preview area border
            drawList->AddRect(p0, p1, IM_COL32(100, 100, 100, 255));

            // Status text overlay
            const char* previewText = isPlaying ? "Game Running" : "Game Preview";
            if (activeWeatherZone && activeWeatherZone->weatherType > 0) {
                previewText = isPlaying ? "Game Running (Weather Active)" : "Preview (Weather Active)";
            }
            ImVec2 textSize = ImGui::CalcTextSize(previewText);
            ImVec2 textPos((p0.x + p1.x - textSize.x) * 0.5f, p0.y + 10);
            drawList->AddText(textPos, IM_COL32(200, 200, 200, 200), previewText);

            // Interaction hint during play mode (centered near bottom of preview)
            if (isPlaying && SceneHasMouseLookController()) {
                const char* hintText = m_GameViewMouseCaptured
                    ? "Press ESC to pause"
                    : "Click to capture mouse";
                ImVec2 hintSize = ImGui::CalcTextSize(hintText);
                ImVec2 hintPos((p0.x + p1.x - hintSize.x) * 0.5f, p1.y - 40);
                // Semi-transparent background pill
                ImVec2 pillMin(hintPos.x - 8, hintPos.y - 4);
                ImVec2 pillMax(hintPos.x + hintSize.x + 8, hintPos.y + hintSize.y + 4);
                drawList->AddRectFilled(pillMin, pillMax, IM_COL32(0, 0, 0, 140), 6.0f);
                drawList->AddText(hintPos, IM_COL32(255, 255, 255, 200), hintText);
            }

            // Debug: zone detection status at bottom of preview
            char debugBuf[128];
            if (activeWeatherZone) {
                const char* wNames[] = {"Clear","Cloudy","Rain","HeavyRain","Snow","Fog","Storm"};
                const char* wn = (activeWeatherZone->weatherType < 7) ? wNames[activeWeatherZone->weatherType] : "?";
                snprintf(debugBuf, sizeof(debugBuf), "Zone: %s | Particles: %u", wn, m_WeatherSystem.GetActiveParticleCount());
            } else {
                snprintf(debugBuf, sizeof(debugBuf), "No weather zone at camera");
            }
            ImVec2 dbgSize = ImGui::CalcTextSize(debugBuf);
            ImVec2 dbgPos((p0.x + p1.x - dbgSize.x) * 0.5f, p1.y - 20);
            drawList->AddText(dbgPos, IM_COL32(180, 180, 100, 200), debugBuf);

            // Render flower particles as projected shapes in game view
            // Liquid particles render as elongated streaks, burst particles as circles
            if (m_PlayMode.IsPlaying() && gameCameraComp && gameCameraTransform) {
                auto* flowerSys = m_PlayMode.GetFlowerSystem();
                const auto& particles = flowerSys->GetParticles();
                if (!particles.empty()) {
                    // Clip all particle draws to the game view rectangle
                    drawList->PushClipRect(p0, p1, true);
                    // Build view-projection matrix from game camera
                    Renderer::Camera projCam;
                    f32 camAspect = gameCameraComp->GetAspectRatio(m_GameViewWidth, m_GameViewHeight);
                    projCam.SetPerspective(gameCameraComp->fieldOfView, camAspect,
                                           gameCameraComp->nearPlane, gameCameraComp->farPlane);
                    projCam.SetPosition(gameCameraTransform->position);
                    Math::Vector3 fwd = gameCameraTransform->rotation.Rotate(Math::Vector3(0, 0, -1));
                    Math::Vector3 camUp = gameCameraTransform->rotation.Rotate(Math::Vector3(0, 1, 0));
                    projCam.SetLookAt(gameCameraTransform->position,
                                      gameCameraTransform->position + fwd, camUp);
                    Math::Matrix4 vp = projCam.GetProjectionMatrix() * projCam.GetViewMatrix();

                    f32 gvW = p1.x - p0.x;
                    f32 gvH = p1.y - p0.y;
                    for (const auto& fp : particles) {
                        // Skip NaN particles
                        if (std::isnan(fp.position.x) || std::isnan(fp.position.y) || std::isnan(fp.position.z)) continue;
                        // Project 3D position to clip space
                        Math::Vector4 clip = vp * Math::Vector4(fp.position.x, fp.position.y, fp.position.z, 1.0f);
                        if (clip.w <= 0.01f) continue;
                        f32 ndcX = clip.x / clip.w;
                        f32 ndcY = clip.y / clip.w;
                        if (ndcX < -1.5f || ndcX > 1.5f || ndcY < -1.5f || ndcY > 1.5f) continue;
                        f32 sx = p0.x + (ndcX * 0.5f + 0.5f) * gvW;
                        // Vulkan projection already flips Y — no extra inversion needed
                        f32 sy = p0.y + (ndcY * 0.5f + 0.5f) * gvH;

                        f32 t = fp.lifetime / fp.maxLifetime;
                        f32 alpha = (1.0f - t * t) * 255.0f;
                        f32 perspScale = 1.0f / (clip.w * 0.3f + 0.3f);
                        f32 radius = fp.scale * 200.0f * perspScale;
                        if (radius < 1.5f) radius = 1.5f;
                        if (radius > 30.0f) radius = 30.0f;
                        int r = static_cast<int>(fp.color.x * 255);
                        int g = static_cast<int>(fp.color.y * 255);
                        int b = static_cast<int>(fp.color.z * 255);
                        int a = static_cast<int>(alpha);
                        ImU32 col = IM_COL32(r, g, b, a);

                        if (fp.isLiquid) {
                            // Liquid streak: longer trail scaled by velocity for drippy look
                            f32 velLen = fp.velocity.Length();
                            f32 trailTime = 0.06f + velLen * 0.008f; // longer trail at high speed
                            Math::Vector3 tailPos = fp.position - fp.velocity * trailTime;
                            Math::Vector4 tailClip = vp * Math::Vector4(tailPos.x, tailPos.y, tailPos.z, 1.0f);
                            f32 tx = sx, ty = sy;
                            if (tailClip.w > 0.01f) {
                                f32 tndcX = tailClip.x / tailClip.w;
                                f32 tndcY = tailClip.y / tailClip.w;
                                tx = p0.x + (tndcX * 0.5f + 0.5f) * gvW;
                                ty = p0.y + (tndcY * 0.5f + 0.5f) * gvH;
                            }
                            // Thick at head, thin at tail (draw two lines for taper)
                            f32 thickness = radius * 1.2f;
                            if (thickness < 2.5f) thickness = 2.5f;
                            drawList->AddLine(ImVec2(sx, sy), ImVec2(tx, ty), col, thickness);
                            // Fat droplet head
                            drawList->AddCircleFilled(ImVec2(sx, sy), radius * 0.9f, col);
                        } else {
                            drawList->AddCircleFilled(ImVec2(sx, sy), radius, col);
                        }
                    }
                    drawList->PopClipRect();
                }
            }

            // Flower Evaluate button overlay in game view
            if (m_PlayMode.IsPlaying() && m_World) {
                bool hasFlowerStem = !m_World->GetEntitiesWithComponent<ECS::FlowerStemComponent>().empty();
                if (hasFlowerStem) {
                    f32 gvW = p1.x - p0.x;
                    f32 btnW = 100.0f, btnH = 28.0f;
                    ImVec2 btnPos(p0.x + gvW * 0.5f - btnW * 0.5f, p1.y - 42.0f);
                    ImVec2 btnEnd(btnPos.x + btnW, btnPos.y + btnH);
                    // Background
                    drawList->AddRectFilled(btnPos, btnEnd, IM_COL32(20, 20, 20, 190), 6.0f);
                    drawList->AddRect(btnPos, btnEnd, IM_COL32(180, 180, 180, 200), 6.0f);
                    // Text
                    const char* btnText = "Evaluate";
                    ImVec2 textSize = ImGui::CalcTextSize(btnText);
                    ImVec2 textPos(btnPos.x + (btnW - textSize.x) * 0.5f,
                                  btnPos.y + (btnH - textSize.y) * 0.5f);
                    drawList->AddText(textPos, IM_COL32(255, 255, 255, 230), btnText);
                    // Click detection
                    ImVec2 mousePos = ImGui::GetMousePos();
                    if (mousePos.x >= btnPos.x && mousePos.x <= btnEnd.x &&
                        mousePos.y >= btnPos.y && mousePos.y <= btnEnd.y &&
                        ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                        m_PlayMode.GetFlowerSystem()->Evaluate();
                    }
                }
            }

            // Reserve space only if we didn't use ImGui::Image (which reserves its own)
            if (!usedImage) {
                ImGui::Dummy(ImVec2(previewWidth, previewHeight));
                m_GameViewHovered = false;
            }
        }

        ImGui::Spacing();

        // Debug info: show zone detection status
        if (gameCameraTransform) {
            ImGui::TextDisabled("Camera pos: (%.1f, %.1f, %.1f)",
                gameCameraTransform->position.x, gameCameraTransform->position.y, gameCameraTransform->position.z);
        } else if (!gameCameraComp) {
            ImGui::TextDisabled("No camera entity in scene");
        }
        if (!m_GameViewRenderTarget || !m_GameViewRenderTarget->IsValid()) {
            ImGui::TextDisabled("Render target unavailable - using fallback preview");
        }
    }

    ImGui::End();
}


// ============================================================================
// Section drawer methods for Rendering, Post Processing, and Retro Effects
// ============================================================================

void EditorLayer::DrawSettingsSection_PostProcessing() {
    if (!m_PostProcessing) {
        ImGui::TextDisabled("Post-processing not initialized");
        return;
    }

    if (ImGui::CollapsingHeader("Post Processing", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto& settings = m_PostProcessing->GetSettings();

        // Tone Mapping
        if (ImGui::CollapsingHeader("Tone Mapping", ImGuiTreeNodeFlags_DefaultOpen)) {
            const char* toneMappingModes[] = { "None", "Reinhard", "Reinhard Extended", "ACES", "Uncharted 2", "AgX" };
            int currentMode = static_cast<int>(settings.toneMappingMode);
            if (ImGui::Combo("Mode", &currentMode, toneMappingModes, 6)) {
                settings.toneMappingMode = static_cast<u32>(currentMode);
            }

            ImGui::DragFloat("Exposure", &settings.exposure, 0.01f, 0.01f, 10.0f);
            ImGui::DragFloat("Gamma", &settings.gamma, 0.01f, 1.0f, 3.0f);

            if (settings.toneMappingMode == 2) { // Reinhard Extended
                ImGui::DragFloat("White Point", &settings.whitePoint, 0.1f, 1.0f, 20.0f);
            }
        }

        // Bloom
        if (ImGui::CollapsingHeader("Bloom")) {
            bool bloomEnabled = settings.bloomEnabled != 0;
            if (ImGui::Checkbox("Enabled##Bloom", &bloomEnabled)) {
                settings.bloomEnabled = bloomEnabled ? 1 : 0;
            }

            if (settings.bloomEnabled) {
                ImGui::DragFloat("Threshold", &settings.bloomThreshold, 0.01f, 0.0f, 5.0f);
                ImGui::DragFloat("Intensity##Bloom", &settings.bloomIntensity, 0.01f, 0.0f, 2.0f);
                ImGui::DragFloat("Radius", &settings.bloomRadius, 0.001f, 0.001f, 0.1f);
            }
        }

        // Vignette
        if (ImGui::CollapsingHeader("Vignette")) {
            bool vignetteEnabled = settings.vignetteEnabled != 0;
            if (ImGui::Checkbox("Enabled##Vignette", &vignetteEnabled)) {
                settings.vignetteEnabled = vignetteEnabled ? 1 : 0;
            }

            if (settings.vignetteEnabled) {
                ImGui::DragFloat("Intensity##Vignette", &settings.vignetteIntensity, 0.01f, 0.0f, 2.0f);
                ImGui::DragFloat("Smoothness", &settings.vignetteSmoothness, 0.01f, 0.0f, 1.0f);
            }
        }

        // Chromatic Aberration
        if (ImGui::CollapsingHeader("Chromatic Aberration")) {
            bool caEnabled = settings.chromaticAberrationEnabled != 0;
            if (ImGui::Checkbox("Enabled##CA", &caEnabled)) {
                settings.chromaticAberrationEnabled = caEnabled ? 1 : 0;
            }

            if (settings.chromaticAberrationEnabled) {
                ImGui::DragFloat("Intensity##CA", &settings.chromaticAberrationIntensity, 0.001f, 0.0f, 0.05f);
            }
        }

        // Color Grading
        if (ImGui::CollapsingHeader("Color Grading")) {
            f32 colorFilter[3] = { settings.colorFilter.x, settings.colorFilter.y, settings.colorFilter.z };
            if (ImGui::ColorEdit3("Color Filter", colorFilter)) {
                settings.colorFilter = Math::Vector3(colorFilter[0], colorFilter[1], colorFilter[2]);
            }

            ImGui::DragFloat("Saturation", &settings.saturation, 0.01f, 0.0f, 2.0f);
            ImGui::DragFloat("Contrast", &settings.contrast, 0.01f, 0.5f, 2.0f);
            ImGui::DragFloat("Brightness", &settings.brightness, 0.01f, -1.0f, 1.0f);
        }

        // Film Grain
        if (ImGui::CollapsingHeader("Film Grain")) {
            bool grainEnabled = settings.filmGrainEnabled != 0;
            if (ImGui::Checkbox("Enabled##Grain", &grainEnabled)) {
                settings.filmGrainEnabled = grainEnabled ? 1 : 0;
            }

            if (settings.filmGrainEnabled) {
                ImGui::DragFloat("Intensity##Grain", &settings.filmGrainIntensity, 0.001f, 0.0f, 0.2f);
            }
        }

        // Depth of Field
        if (ImGui::CollapsingHeader("Depth of Field")) {
            bool dofEnabled = settings.dofEnabled != 0;
            if (ImGui::Checkbox("Enabled##DOF", &dofEnabled)) {
                settings.dofEnabled = dofEnabled ? 1 : 0;
            }

            if (settings.dofEnabled) {
                ImGui::DragFloat("Focal Distance", &settings.dofFocalDistance, 0.5f, 0.1f, 500.0f);
                ImGui::DragFloat("Focal Range", &settings.dofFocalRange, 0.1f, 0.0f, 50.0f);
                ImGui::DragFloat("Near Blur", &settings.dofNearBlurStrength, 0.01f, 0.0f, 1.0f);
                ImGui::DragFloat("Far Blur", &settings.dofFarBlurStrength, 0.01f, 0.0f, 1.0f);
                ImGui::DragFloat("Bokeh Size", &settings.dofBokehSize, 0.1f, 0.5f, 16.0f);

                const char* apertureShapes[] = { "Circle", "Hexagon", "Octagon" };
                int shape = static_cast<int>(settings.dofApertureShape);
                if (ImGui::Combo("Aperture Shape", &shape, apertureShapes, 3)) {
                    settings.dofApertureShape = static_cast<u32>(shape);
                }

                bool debugCoC = settings.dofDebugCoC != 0;
                if (ImGui::Checkbox("Debug CoC", &debugCoC)) {
                    settings.dofDebugCoC = debugCoC ? 1 : 0;
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Visualize Circle of Confusion (red=near blur, green=in focus, blue=far blur)");
                }
            }
        }

        // Tilt-Shift
        if (ImGui::CollapsingHeader("Tilt-Shift")) {
            bool tsEnabled = settings.tiltShiftEnabled != 0;
            if (ImGui::Checkbox("Enabled##TiltShift", &tsEnabled)) {
                settings.tiltShiftEnabled = tsEnabled ? 1 : 0;
            }

            if (settings.tiltShiftEnabled) {
                ImGui::SliderFloat("Focus Y", &settings.tiltShiftFocusY, 0.0f, 1.0f);
                ImGui::DragFloat("Band Width", &settings.tiltShiftBandWidth, 0.01f, 0.01f, 1.0f);
                ImGui::DragFloat("Blur Amount", &settings.tiltShiftBlurAmount, 0.1f, 0.0f, 10.0f);
            }
        }

        // Anti-Aliasing
        if (ImGui::CollapsingHeader("Anti-Aliasing")) {
            const char* aaModes[] = { "None", "FXAA", "TAA", "SMAA" };
            int aaMode = static_cast<int>(settings.aaMode);
            if (ImGui::Combo("AA Mode", &aaMode, aaModes, IM_ARRAYSIZE(aaModes))) {
                settings.aaMode = static_cast<u32>(aaMode);
                // Sync legacy fxaaEnabled flag
                settings.fxaaEnabled = (settings.aaMode == 1) ? 1 : 0;
            }

            // FXAA settings
            if (settings.aaMode == 1) {
                ImGui::DragFloat("Span Max", &settings.fxaaSpanMax, 0.5f, 2.0f, 16.0f);
                ImGui::DragFloat("Reduce Min", &settings.fxaaReduceMin, 0.001f, 0.0f, 0.1f, "%.4f");
                ImGui::DragFloat("Reduce Mul", &settings.fxaaReduceMul, 0.01f, 0.0f, 0.5f);
            }

            // TAA settings (CPU-side config for the TAA compute pass)
            if (settings.aaMode == 2) {
                ImGui::SliderFloat("Sharpness", &settings.taaSharpness, 0.0f, 1.0f, "%.2f");
                ImGui::SliderFloat("Jitter Scale", &settings.taaJitterScale, 0.0f, 2.0f, "%.2f");
                ImGui::SliderFloat("Feedback Min", &settings.taaFeedbackMin, 0.0f, 1.0f, "%.2f");
                ImGui::SliderFloat("Feedback Max", &settings.taaFeedbackMax, 0.0f, 1.0f, "%.2f");
            }
        }

        // LUT Color Grading
        if (ImGui::CollapsingHeader("LUT Color Grading")) {
            bool lutEnabled = settings.lutEnabled != 0;
            if (ImGui::Checkbox("Enabled##LUT", &lutEnabled)) {
                settings.lutEnabled = lutEnabled ? 1 : 0;
            }

            if (settings.lutEnabled) {
                ImGui::DragFloat("Strength##LUT", &settings.lutStrength, 0.01f, 0.0f, 1.0f);

                if (m_PostProcessing->IsLUTLoaded()) {
                    std::string lutPath = m_PostProcessing->GetLUTPath();
                    // Show just the filename
                    size_t lastSlash = lutPath.find_last_of("/\\");
                    std::string filename = (lastSlash != std::string::npos) ? lutPath.substr(lastSlash + 1) : lutPath;
                    ImGui::Text("Loaded: %s", filename.c_str());

                    if (ImGui::Button("Clear LUT")) {
                        m_PostProcessing->ClearLUT();
                        settings.lutEnabled = 0;
                    }
                }

                ImGui::SameLine();
                if (ImGui::Button("Load LUT")) {
                    std::string path = FileDialog::OpenFile("Load LUT", {{ "PNG Images", "*.png" }});
                    if (!path.empty()) {
                        m_PostProcessing->LoadLUT(path);
                    }
                }
            }
        }

        // Color Palette Lock
        if (ImGui::CollapsingHeader("Palette Lock")) {
            bool paletteEnabled = settings.paletteEnabled != 0;
            if (ImGui::Checkbox("Enabled##Palette", &paletteEnabled)) {
                settings.paletteEnabled = paletteEnabled ? 1 : 0;
            }

            if (settings.paletteEnabled) {
                int colors = static_cast<int>(settings.paletteColors);
                if (ImGui::SliderInt("Colors", &colors, 2, 256)) {
                    settings.paletteColors = static_cast<u32>(colors);
                }
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Number of color levels per channel");
            }
        }

        // Stipple / Dither
        if (ImGui::CollapsingHeader("Stipple / Dither")) {
            bool stippleEnabled = settings.stippleEnabled != 0;
            if (ImGui::Checkbox("Enabled##Stipple", &stippleEnabled)) {
                settings.stippleEnabled = stippleEnabled ? 1 : 0;
            }

            if (settings.stippleEnabled) {
                const char* stipplePatterns[] = {
                    "Bayer 4x4", "Bayer 8x8", "Blue Noise", "Halftone",
                    "Crosshatch", "Overlook", "Ordered 2x2", "Floyd-Steinberg"
                };
                ImGui::Text("Patterns (combine any):");
                for (int i = 0; i < 8; i++) {
                    bool on = (settings.stipplePatternMask & (1u << i)) != 0;
                    if (ImGui::Checkbox(stipplePatterns[i], &on)) {
                        if (on) settings.stipplePatternMask |= (1u << i);
                        else    settings.stipplePatternMask &= ~(1u << i);
                    }
                    if (i % 2 == 0 && i < 7) ImGui::SameLine(200.0f);
                }

                const char* colorModes[] = { "Monochrome", "Duo-Tone", "Full Color" };
                int currentColorMode = static_cast<int>(settings.stippleColorMode);
                if (ImGui::Combo("Color Mode##Stipple", &currentColorMode, colorModes, 3)) {
                    settings.stippleColorMode = static_cast<u32>(currentColorMode);
                }

                ImGui::DragFloat("Scale##Stipple", &settings.stippleScale, 0.1f, 0.5f, 8.0f);
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Pattern pixel scale (1.0 = native resolution)");

                ImGui::DragFloat("Density##Stipple", &settings.stippleDensity, 0.01f, 0.0f, 1.0f);
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Threshold bias — lower = more dots, higher = fewer dots");

                ImGui::DragFloat("Strength##Stipple", &settings.stippleStrength, 0.01f, 0.0f, 1.0f);
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Blend with original image (0 = no effect, 1 = full stipple)");

                // Show color pickers for mono and duo-tone modes
                if (settings.stippleColorMode <= 1) {
                    f32 fg[3] = { settings.stippleFgColor.x, settings.stippleFgColor.y, settings.stippleFgColor.z };
                    if (ImGui::ColorEdit3("Foreground##Stipple", fg)) {
                        settings.stippleFgColor = Math::Vector3(fg[0], fg[1], fg[2]);
                    }
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Ink / dot color");

                    f32 bg[3] = { settings.stippleBgColor.x, settings.stippleBgColor.y, settings.stippleBgColor.z };
                    if (ImGui::ColorEdit3("Background##Stipple", bg)) {
                        settings.stippleBgColor = Math::Vector3(bg[0], bg[1], bg[2]);
                    }
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Paper / gap color");
                }
            }
        }

        // Cel outline
        {
            auto& s = m_PostProcessing->GetSettings();
            bool outlineOn = s.celOutlineEnabled != 0;
            if (ImGui::Checkbox("Cel Outline##PP", &outlineOn)) {
                s.celOutlineEnabled = outlineOn ? 1 : 0;
            }
            if (outlineOn) {
                ImGui::SliderFloat("Outline Thickness##PP", &s.celOutlineThickness, 0.5f, 5.0f);
                ImGui::SliderFloat("Outline Threshold##PP", &s.celOutlineThreshold, 0.001f, 0.5f);
                ImGui::ColorEdit3("Outline Color##PP", &s.celOutlineColor.x);
            }
        }

        // ================================================================
        // Screen-Space Effects
        // ================================================================

        // SSAO
        if (ImGui::CollapsingHeader("SSAO (Ambient Occlusion)")) {
            bool ssaoOn = settings.ssaoEnabled != 0;
            if (ImGui::Checkbox("Enabled##SSAO", &ssaoOn)) {
                settings.ssaoEnabled = ssaoOn ? 1 : 0;
            }
            if (settings.ssaoEnabled) {
                ImGui::DragFloat("Radius##SSAO", &settings.ssaoRadius, 0.01f, 0.01f, 5.0f);
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("World-space sample radius");
                ImGui::DragFloat("Intensity##SSAO", &settings.ssaoIntensity, 0.01f, 0.0f, 5.0f);
                ImGui::DragFloat("Bias##SSAO", &settings.ssaoBias, 0.001f, 0.0f, 0.1f, "%.4f");
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Depth bias to reduce self-occlusion");
                int samples = static_cast<int>(settings.ssaoSamples);
                if (ImGui::SliderInt("Samples##SSAO", &samples, 4, 32)) {
                    settings.ssaoSamples = static_cast<u32>(samples);
                }
            }
        }

        // Contact Shadows
        if (ImGui::CollapsingHeader("Contact Shadows")) {
            bool csOn = settings.contactShadowsEnabled != 0;
            if (ImGui::Checkbox("Enabled##ContactShadows", &csOn)) {
                settings.contactShadowsEnabled = csOn ? 1 : 0;
            }
            if (settings.contactShadowsEnabled) {
                ImGui::DragFloat("Ray Length##CS", &settings.contactShadowsLength, 0.001f, 0.001f, 0.5f, "%.4f");
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Screen-space ray march length (UV space)");
                int steps = static_cast<int>(settings.contactShadowsSteps);
                if (ImGui::SliderInt("Steps##CS", &steps, 4, 32)) {
                    settings.contactShadowsSteps = static_cast<u32>(steps);
                }
                ImGui::DragFloat("Intensity##CS", &settings.contactShadowsIntensity, 0.01f, 0.0f, 2.0f);
            }
        }

        // God Rays
        if (ImGui::CollapsingHeader("God Rays")) {
            bool grOn = settings.godRaysEnabled != 0;
            if (ImGui::Checkbox("Enabled##GodRays", &grOn)) {
                settings.godRaysEnabled = grOn ? 1 : 0;
            }
            if (settings.godRaysEnabled) {
                ImGui::DragFloat("Intensity##GR", &settings.godRaysIntensity, 0.01f, 0.0f, 2.0f);
                ImGui::DragFloat("Decay##GR", &settings.godRaysDecay, 0.001f, 0.9f, 1.0f, "%.4f");
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Per-sample falloff (closer to 1 = longer rays)");
                ImGui::DragFloat("Density##GR", &settings.godRaysDensity, 0.01f, 0.1f, 3.0f);
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Sample spacing multiplier");
                int samples = static_cast<int>(settings.godRaysSamples);
                if (ImGui::SliderInt("Samples##GR", &samples, 16, 128)) {
                    settings.godRaysSamples = static_cast<u32>(samples);
                }
                ImGui::DragFloat("Weight##GR", &settings.godRaysWeight, 0.001f, 0.001f, 0.1f, "%.4f");
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Per-sample contribution weight");
            }
        }

        // Fake Caustics
        if (ImGui::CollapsingHeader("Fake Caustics")) {
            bool fcOn = settings.causticsEnabled != 0;
            if (ImGui::Checkbox("Enabled##Caustics", &fcOn)) {
                settings.causticsEnabled = fcOn ? 1 : 0;
            }
            if (settings.causticsEnabled) {
                ImGui::DragFloat("Intensity##Caustics", &settings.causticsIntensity, 0.01f, 0.0f, 2.0f);
                ImGui::DragFloat("Scale##Caustics", &settings.causticsScale, 0.01f, 0.1f, 10.0f);
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Voronoi pattern scale");
                ImGui::DragFloat("Speed##Caustics", &settings.causticsSpeed, 0.01f, 0.0f, 5.0f);
                ImGui::DragFloat("Water Y##Caustics", &settings.causticsWaterY, 0.1f, -100.0f, 100.0f);
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("World-space Y height of the water surface");
            }
        }

        // Fog Shafts
        if (ImGui::CollapsingHeader("Fog Shafts")) {
            bool fsOn = settings.fogShaftsEnabled != 0;
            if (ImGui::Checkbox("Enabled##FogShafts", &fsOn)) {
                settings.fogShaftsEnabled = fsOn ? 1 : 0;
            }
            if (settings.fogShaftsEnabled) {
                ImGui::DragFloat("Intensity##FS", &settings.fogShaftsIntensity, 0.01f, 0.0f, 2.0f);
                ImGui::DragFloat("Fog Density##FS", &settings.fogShaftsDensity, 0.001f, 0.001f, 0.5f, "%.4f");
                ImGui::DragFloat("Decay##FS", &settings.fogShaftsDecay, 0.001f, 0.8f, 1.0f, "%.4f");
                int samples = static_cast<int>(settings.fogShaftsSamples);
                if (ImGui::SliderInt("Samples##FS", &samples, 4, 32)) {
                    settings.fogShaftsSamples = static_cast<u32>(samples);
                }
                ImGui::DragFloat("Max Distance##FS", &settings.fogShaftsMaxDistance, 1.0f, 5.0f, 500.0f);
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("World-space max march distance");
            }
        }
    }
}

void EditorLayer::DrawSettingsSection_RetroEffects() {
    // === RETRO EFFECTS (PS1/N64/PS2/GameCube presets) ===
    if (ImGui::CollapsingHeader("Retro Effects", ImGuiTreeNodeFlags_DefaultOpen)) {
        bool retroEnabled = m_RetroEffects.IsEnabled();
        if (ImGui::Checkbox("Enable Retro Effects", &retroEnabled)) {
            m_RetroEffects.SetEnabled(retroEnabled);
        }

        if (retroEnabled) {
            ImGui::Text("Console Presets:");
            if (ImGui::Button("PS1")) { m_RetroEffects.ApplyPS1Preset(); }
            ImGui::SameLine();
            if (ImGui::Button("N64")) { m_RetroEffects.ApplyN64Preset(); }
            ImGui::SameLine();
            if (ImGui::Button("PS2")) { m_RetroEffects.ApplyPS2Preset(); }
            ImGui::SameLine();
            if (ImGui::Button("GameCube")) { m_RetroEffects.ApplyGameCubePreset(); }

            if (ImGui::Button("SNES")) { m_RetroEffects.ApplySNESPreset(); }
            ImGui::SameLine();
            if (ImGui::Button("Dreamcast")) { m_RetroEffects.ApplyDreamcastPreset(); }
            ImGui::SameLine();
            if (ImGui::Button("Saturn")) { m_RetroEffects.ApplySaturnPreset(); }
            ImGui::SameLine();
            if (ImGui::Button("3DO")) { m_RetroEffects.Apply3DOPreset(); }

            if (ImGui::Button("NES")) { m_RetroEffects.ApplyNESPreset(); }
            ImGui::SameLine();
            if (ImGui::Button("Game Boy")) { m_RetroEffects.ApplyGameBoyPreset(); }
            ImGui::SameLine();
            if (ImGui::Button("GBA")) { m_RetroEffects.ApplyGBAPreset(); }
            ImGui::SameLine();
            if (ImGui::Button("Genesis")) { m_RetroEffects.ApplyGenesisPreset(); }

            if (ImGui::Button("Master System")) { m_RetroEffects.ApplyMasterSystemPreset(); }
            ImGui::SameLine();
            if (ImGui::Button("PSP")) { m_RetroEffects.ApplyPSPPreset(); }
            ImGui::SameLine();
            if (ImGui::Button("DOS/VGA")) { m_RetroEffects.ApplyDOSVGAPreset(); }
            ImGui::SameLine();
            if (ImGui::Button("PC Engine")) { m_RetroEffects.ApplyPCEnginePreset(); }

            if (ImGui::Button("Virtual Boy")) { m_RetroEffects.ApplyVirtualBoyPreset(); }
            ImGui::SameLine();
            if (ImGui::Button("Neo Geo")) { m_RetroEffects.ApplyNeoGeoPreset(); }
            ImGui::SameLine();
            if (ImGui::Button("Atari 2600")) { m_RetroEffects.ApplyAtari2600Preset(); }
            ImGui::SameLine();
            if (ImGui::Button("Xbox")) { m_RetroEffects.ApplyXboxPreset(); }

            if (ImGui::Button("Clear All")) { m_RetroEffects.ClearAllEffects(); }

            ImGui::Separator();

            // Resolution settings
            if (ImGui::TreeNode("Resolution")) {
                auto& res = m_RetroEffects.GetResolution();
                int width = static_cast<int>(res.renderWidth);
                int height = static_cast<int>(res.renderHeight);
                if (ImGui::DragInt("Render Width", &width, 1, 160, 1920)) {
                    res.renderWidth = static_cast<u32>(width);
                }
                if (ImGui::DragInt("Render Height", &height, 1, 120, 1080)) {
                    res.renderHeight = static_cast<u32>(height);
                }
                ImGui::Checkbox("Point Filtering", &res.pointFiltering);
                ImGui::Checkbox("Integer Scaling", &res.integerScaling);
                ImGui::DragFloat("Aspect Ratio", &res.aspectRatio, 0.01f, 1.0f, 2.5f);
                ImGui::TreePop();
            }

            // Dithering
            if (ImGui::TreeNode("Dithering")) {
                const char* ditherPatterns[] = { "None", "Bayer 2x2", "Bayer 4x4", "Bayer 8x8", "Blue Noise", "Ordered" };
                int currentDither = static_cast<int>(m_RetroEffects.GetDitherPattern());
                if (ImGui::Combo("Pattern", &currentDither, ditherPatterns, 6)) {
                    m_RetroEffects.SetDitherPattern(static_cast<Effects::DitherPattern>(currentDither));
                }
                ImGui::TreePop();
            }

            // Color Mode
            if (ImGui::TreeNode("Color Mode")) {
                const char* colorModes[] = { "True Color (24-bit)", "High Color (16-bit)", "256 Colors", "16 Colors", "Monochrome" };
                int currentMode = static_cast<int>(m_RetroEffects.GetColorMode());
                if (ImGui::Combo("Mode", &currentMode, colorModes, 5)) {
                    m_RetroEffects.SetColorMode(static_cast<Effects::ColorMode>(currentMode));
                }
                ImGui::TreePop();
            }

            // Vertex Jitter (PS1 style)
            if (ImGui::TreeNode("Vertex Jitter (PS1)")) {
                auto& jitter = m_RetroEffects.GetVertexJitter();
                ImGui::Checkbox("Enabled##Jitter", &jitter.enabled);
                if (jitter.enabled) {
                    ImGui::DragFloat("Amount", &jitter.jitterAmount, 0.1f, 0.0f, 5.0f);
                    ImGui::Checkbox("Snap to Grid", &jitter.snapToGrid);
                    int gridRes = static_cast<int>(jitter.gridResolution);
                    if (ImGui::DragInt("Grid Resolution", &gridRes, 1, 80, 320)) {
                        jitter.gridResolution = static_cast<u32>(gridRes);
                    }
                }
                ImGui::TreePop();
            }

            // Affine Texture Warping (PS1)
            if (ImGui::TreeNode("Affine Warping (PS1)")) {
                auto& affine = m_RetroEffects.GetAffineSettings();
                ImGui::Checkbox("Enabled##Affine", &affine.enabled);
                if (affine.enabled) {
                    ImGui::DragFloat("Warp Strength", &affine.warpStrength, 0.1f, 0.0f, 2.0f);
                    ImGui::Checkbox("Vertex Snapping", &affine.vertexSnapping);
                    ImGui::DragFloat("Snap Grid Size", &affine.snapGridSize, 0.1f, 0.5f, 4.0f);
                }
                ImGui::TreePop();
            }

            // CRT Filter
            if (ImGui::TreeNode("CRT Filter")) {
                auto& crt = m_RetroEffects.GetCRTSettings();
                ImGui::Checkbox("Enabled##CRT", &crt.enabled);
                if (crt.enabled) {
                    ImGui::DragFloat("Scanline Intensity", &crt.scanlineIntensity, 0.01f, 0.0f, 1.0f);
                    ImGui::DragFloat("Scanline Width", &crt.scanlineWidth, 0.1f, 0.5f, 3.0f);
                    ImGui::Checkbox("Curved Screen", &crt.curvedScreen);
                    if (crt.curvedScreen) {
                        ImGui::DragFloat("Curvature", &crt.curvature, 0.01f, 0.0f, 0.5f);
                    }
                    ImGui::DragFloat("Vignette", &crt.vignette, 0.01f, 0.0f, 1.0f);
                    ImGui::Checkbox("Phosphor Glow", &crt.phosphorGlow);
                    if (crt.phosphorGlow) {
                        ImGui::DragFloat("Glow Strength", &crt.glowStrength, 0.01f, 0.0f, 1.0f);
                    }
                }
                ImGui::TreePop();
            }

            // CRT Phosphor Subpixel Blending
            if (ImGui::TreeNode("CRT Phosphor")) {
                auto& crt = m_RetroEffects.GetCRTSettings();

                // CRT Model Preset dropdown
                const char* crtModels[] = {
                    "Custom", "Sony Trinitron KV-27V42", "Sony PVM-20M4U", "JVC TM-H150CG",
                    "Toshiba 14AF46", "Sony GDM-FW900", "ViewSonic G810", "NEC MultiSync FE2111SB",
                    "Generic 15kHz Arcade", "Wells Gardner K7000", "Commodore 1084S"
                };
                static int selectedModel = 0;
                if (ImGui::Combo("CRT Model", &selectedModel, crtModels, 11)) {
                    if (selectedModel > 0) {
                        m_RetroEffects.ApplyCRTModelPreset(static_cast<Effects::CRTModel>(selectedModel));
                    }
                }

                const char* maskTypes[] = { "Aperture Grille", "Shadow Mask", "Slot Mask" };
                int maskType = static_cast<int>(crt.maskType);
                if (ImGui::Combo("Mask Type", &maskType, maskTypes, 3)) {
                    crt.maskType = static_cast<u32>(maskType);
                    selectedModel = 0; // Switch to Custom when manually editing
                }
                ImGui::DragFloat("Mask Pitch", &crt.maskPitch, 0.1f, 0.5f, 4.0f);
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Spacing between RGB triplets in pixels");
                ImGui::DragFloat("Bloom Radius##Phosphor", &crt.bloomRadius, 0.1f, 0.5f, 5.0f);
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("How far phosphor light bleeds between dots");
                ImGui::DragFloat("Bloom Strength##Phosphor", &crt.bloomStrength, 0.01f, 0.0f, 1.0f);
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Intensity of phosphor bleeding effect");
                ImGui::DragFloat("Bloom Sigma##Phosphor", &crt.bloomSigma, 0.01f, 0.3f, 2.0f);
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Gaussian spread: higher = softer bloom (>= 0.5 for dither blending)");
                ImGui::DragFloat("TV Lines##Phosphor", &crt.tvl, 10.0f, 100.0f, 1200.0f);
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Horizontal resolution measure of the CRT display");
                ImGui::TreePop();
            }

            // VHS Filter
            if (ImGui::TreeNode("VHS Filter")) {
                auto& vhs = m_RetroEffects.GetVHSSettings();
                ImGui::Checkbox("Enabled##VHS", &vhs.enabled);
                if (vhs.enabled) {
                    ImGui::DragFloat("Tracking Intensity", &vhs.trackingIntensity, 0.01f, 0.0f, 1.0f);
                    ImGui::DragFloat("Tracking Speed", &vhs.trackingSpeed, 0.1f, 0.1f, 5.0f);
                    ImGui::DragFloat("Wobble Intensity", &vhs.wobbleIntensity, 0.0005f, 0.0f, 0.02f, "%.4f");
                    ImGui::DragFloat("Wobble Speed", &vhs.wobbleSpeed, 0.1f, 0.1f, 10.0f);
                    ImGui::DragFloat("Color Bleed", &vhs.colorBleedAmount, 0.0005f, 0.0f, 0.02f, "%.4f");
                    ImGui::DragFloat("Noise Intensity", &vhs.noiseIntensity, 0.005f, 0.0f, 0.3f);
                    ImGui::DragFloat("Blue Shift", &vhs.blueShift, 0.01f, 0.0f, 0.3f);
                    ImGui::Checkbox("Screen Tear", &vhs.screenTear);
                    ImGui::Checkbox("Interlacing", &vhs.interlacing);
                }
                ImGui::TreePop();
            }

            // Global Gouraud-only mode
            if (ImGui::TreeNode("Global Retro Shading")) {
                bool gouraud = m_RetroEffects.GetGouraudOnly();
                if (ImGui::Checkbox("Force Gouraud Shading", &gouraud)) {
                    m_RetroEffects.SetGouraudOnly(gouraud);
                }
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Use vertex-interpolated lighting for all entities (faceted look)");
                ImGui::TreePop();
            }

            // Retro Fog
            if (ImGui::TreeNode("Fog (Distance)")) {
                auto& fog = m_RetroEffects.GetFogSettings();
                ImGui::Checkbox("Enabled##Fog", &fog.enabled);
                if (fog.enabled) {
                    f32 fogColor[3] = { fog.color.x, fog.color.y, fog.color.z };
                    if (ImGui::ColorEdit3("Color##Fog", fogColor)) {
                        fog.color = Math::Vector3(fogColor[0], fogColor[1], fogColor[2]);
                    }
                    ImGui::DragFloat("Start Distance", &fog.start, 0.5f, 0.0f, 100.0f);
                    ImGui::DragFloat("End Distance", &fog.end, 0.5f, 1.0f, 200.0f);
                    ImGui::Checkbox("Hard Cutoff", &fog.hardCutoff);
                    if (fog.hardCutoff) {
                        ImGui::DragFloat("Cutoff Distance", &fog.cutoffDistance, 1.0f, 10.0f, 200.0f);
                    }
                }
                ImGui::TreePop();
            }
        }
    }
}

void EditorLayer::DrawSettingsSection_Skybox() {
    if (!m_RenderSystem) return;

    // === SKYBOX ===
    if (ImGui::CollapsingHeader("Skybox", ImGuiTreeNodeFlags_DefaultOpen)) {
        Renderer::SkyboxConfig config = m_RenderSystem->GetSkyboxConfig();
        bool changed = false;

        // Type combo
        int typeIdx = static_cast<int>(config.type);
        const char* skyboxTypes[] = { "None", "Cubemap", "Procedural", "Solid Color" };
        if (ImGui::Combo("Type", &typeIdx, skyboxTypes, 4)) {
            config.type = static_cast<Renderer::SkyboxType>(typeIdx);
            changed = true;
        }

        ImGui::Separator();

        // Procedural sky controls
        if (config.type == Renderer::SkyboxType::Procedural) {
            // Presets
            ImGui::Text("Presets:");
            if (ImGui::Button("Midday")) {
                config.topColor = Math::Vector3(0.1f, 0.3f, 0.8f);
                config.horizonColor = Math::Vector3(0.5f, 0.7f, 1.0f);
                config.bottomColor = Math::Vector3(0.8f, 0.85f, 0.9f);
                config.sunDirection = Math::Vector3(0.0f, 1.0f, 0.0f);
                changed = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("Sunset")) {
                config.topColor = Math::Vector3(0.1f, 0.1f, 0.4f);
                config.horizonColor = Math::Vector3(0.9f, 0.4f, 0.1f);
                config.bottomColor = Math::Vector3(0.95f, 0.6f, 0.2f);
                config.sunDirection = Math::Vector3(0.8f, 0.1f, 0.3f);
                changed = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("Dawn")) {
                config.topColor = Math::Vector3(0.15f, 0.15f, 0.5f);
                config.horizonColor = Math::Vector3(0.8f, 0.5f, 0.3f);
                config.bottomColor = Math::Vector3(0.6f, 0.4f, 0.3f);
                config.sunDirection = Math::Vector3(-0.8f, 0.15f, 0.2f);
                changed = true;
            }
            if (ImGui::Button("Night")) {
                config.topColor = Math::Vector3(0.01f, 0.01f, 0.05f);
                config.horizonColor = Math::Vector3(0.05f, 0.05f, 0.15f);
                config.bottomColor = Math::Vector3(0.02f, 0.02f, 0.08f);
                config.sunDirection = Math::Vector3(0.0f, -1.0f, 0.0f);
                changed = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("Overcast")) {
                config.topColor = Math::Vector3(0.5f, 0.5f, 0.55f);
                config.horizonColor = Math::Vector3(0.6f, 0.6f, 0.63f);
                config.bottomColor = Math::Vector3(0.55f, 0.55f, 0.58f);
                config.sunDirection = Math::Vector3(0.3f, 0.7f, 0.2f);
                changed = true;
            }

            ImGui::Separator();
            ImGui::Text("Colors:");
            changed |= ImGui::ColorEdit3("Top Color", &config.topColor.x);
            changed |= ImGui::ColorEdit3("Horizon Color", &config.horizonColor.x);
            changed |= ImGui::ColorEdit3("Bottom Color", &config.bottomColor.x);

            ImGui::Separator();
            changed |= ImGui::DragFloat3("Sun Direction", &config.sunDirection.x, 0.01f, -1.0f, 1.0f);
        }

        // Solid color controls
        if (config.type == Renderer::SkyboxType::SolidColor) {
            changed |= ImGui::ColorEdit3("Sky Color", &config.solidColor.x);
        }

        // Cubemap controls
        if (config.type == Renderer::SkyboxType::Cubemap) {
            ImGui::Text("Cubemap Faces:");
            const char* faceLabels[] = { "Right (+X)", "Left (-X)", "Top (+Y)", "Bottom (-Y)", "Front (+Z)", "Back (-Z)" };
            for (int i = 0; i < 6; ++i) {
                char buf[256];
                strncpy(buf, config.cubemapPaths[i].c_str(), sizeof(buf) - 1);
                buf[sizeof(buf) - 1] = '\0';
                if (ImGui::InputText(faceLabels[i], buf, sizeof(buf))) {
                    config.cubemapPaths[i] = buf;
                    changed = true;
                }
            }
        }

        // Rotation slider for all non-None types
        if (config.type != Renderer::SkyboxType::None) {
            ImGui::Separator();
            changed |= ImGui::SliderFloat("Rotation", &config.rotation, 0.0f, 360.0f, "%.1f deg");
        }

        if (changed) {
            m_RenderSystem->SetSkybox(config);
        }
    }
}

void EditorLayer::DrawSettingsSection_Shadows() {
    if (!m_RenderSystem) return;

    // === SHADOWS ===
    if (ImGui::CollapsingHeader("Shadows", ImGuiTreeNodeFlags_DefaultOpen)) {
        bool shadows = m_RenderSystem->IsShadowsEnabled();
        if (ImGui::Checkbox("Shadows", &shadows)) {
            m_RenderSystem->SetShadowsEnabled(shadows);
        }

        if (shadows) {
            // Shadow resolution
            const char* resOptions[] = { "512", "1024", "2048", "4096" };
            const u32 resValues[] = { 512, 1024, 2048, 4096 };
            u32 currentRes = m_RenderSystem->GetShadowResolution();
            int resIdx = 2; // default to 2048
            for (int i = 0; i < 4; ++i) {
                if (resValues[i] == currentRes) { resIdx = i; break; }
            }
            if (ImGui::Combo("Shadow Resolution", &resIdx, resOptions, 4)) {
                m_RenderSystem->SetShadowResolution(resValues[resIdx]);
            }

            // Shadow distance
            f32 shadowDist = m_RenderSystem->GetShadowDistance();
            if (ImGui::SliderFloat("Shadow Distance", &shadowDist, 10.0f, 500.0f, "%.0f")) {
                m_RenderSystem->SetShadowDistance(shadowDist);
            }

            // Shadow strength
            f32 shadowStr = m_RenderSystem->GetShadowStrength();
            if (ImGui::SliderFloat("Shadow Strength", &shadowStr, 0.0f, 1.0f)) {
                m_RenderSystem->SetShadowStrength(shadowStr);
            }

            // Shadow softness
            f32 shadowSoft = m_RenderSystem->GetShadowSoftness();
            if (ImGui::SliderFloat("Shadow Softness", &shadowSoft, 0.0f, 5.0f, "%.1f")) {
                m_RenderSystem->SetShadowSoftness(shadowSoft);
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("0 = hard edges, 1-5 = soft penumbra radius");
        }
    }
}

void EditorLayer::DrawSettingsSection_AmbientLighting() {
    if (!m_RenderSystem) return;

    // === AMBIENT LIGHTING ===
    if (ImGui::CollapsingHeader("Ambient Lighting")) {
        Math::Vector3 ambientColor = m_RenderSystem->GetAmbientColor();
        f32 ambient[3] = { ambientColor.x, ambientColor.y, ambientColor.z };
        if (ImGui::ColorEdit3("Ambient Color", ambient)) {
            m_RenderSystem->SetAmbientColor(Math::Vector3(ambient[0], ambient[1], ambient[2]));
        }

        f32 ambientIntensity = m_RenderSystem->GetAmbientIntensity();
        if (ImGui::DragFloat("Ambient Intensity", &ambientIntensity, 0.05f, 0.0f, 5.0f)) {
            m_RenderSystem->SetAmbientIntensity(ambientIntensity);
        }
    }
}

void EditorLayer::DrawSettingsSection_CelShading() {
    if (!m_RenderSystem) return;

    // === CEL SHADING ===
    if (ImGui::CollapsingHeader("Cel Shading")) {
        bool celEnabled = m_RenderSystem->IsCelShadingEnabled();
        if (ImGui::Checkbox("Cel Shading##Rendering", &celEnabled)) {
            m_RenderSystem->SetCelShadingEnabled(celEnabled);
        }
        if (celEnabled) {
            f32 celBands = m_RenderSystem->GetCelDiffuseBands();
            if (ImGui::SliderFloat("Diffuse Bands##Cel", &celBands, 2.0f, 8.0f, "%.0f")) {
                m_RenderSystem->SetCelDiffuseBands(celBands);
            }
            f32 celSpec = m_RenderSystem->GetCelSpecularCutoff();
            if (ImGui::SliderFloat("Specular Cutoff##Cel", &celSpec, 0.0f, 1.0f)) {
                m_RenderSystem->SetCelSpecularCutoff(celSpec);
            }
        }
    }
}

void EditorLayer::DrawSettingsSection_DisplayOptions() {
    if (!m_RenderSystem) return;

    // === DISPLAY OPTIONS ===
    if (ImGui::CollapsingHeader("Display Options")) {
        // Backface culling
        bool culling = m_RenderSystem->IsBackfaceCullingEnabled();
        if (ImGui::Checkbox("Backface Culling", &culling)) {
            m_RenderSystem->SetBackfaceCullingEnabled(culling);
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Cull back-facing triangles for better performance");

        // Wireframe
        bool wireframe = m_RenderSystem->IsWireframeEnabled();
        if (ImGui::Checkbox("Wireframe", &wireframe)) {
            m_RenderSystem->SetWireframeEnabled(wireframe);
        }
    }

    // === SDF SCENE ===
    {
        if (ImGui::CollapsingHeader("SDF Primitives")) {
            if (auto* sdfScene = m_RenderSystem->GetSDFScene()) {
                ImGui::Text("Objects: %u", sdfScene->GetObjectCount());
                ImGui::TextDisabled("CPU-side SDF evaluation. GPU ray march requires compute shader.");

                if (ImGui::Button("Add Sphere##SDF")) {
                    Renderer::SDFObject obj;
                    obj.type = Renderer::SDFPrimitive::Sphere;
                    obj.scale = Math::Vector3(1.0f, 1.0f, 1.0f);
                    sdfScene->AddObject(obj);
                }
                ImGui::SameLine();
                if (ImGui::Button("Add Box##SDF")) {
                    Renderer::SDFObject obj;
                    obj.type = Renderer::SDFPrimitive::Box;
                    obj.scale = Math::Vector3(1.0f, 1.0f, 1.0f);
                    sdfScene->AddObject(obj);
                }
                ImGui::SameLine();
                if (ImGui::Button("Clear##SDF")) {
                    sdfScene->Clear();
                }
            } else {
                ImGui::TextDisabled("SDF scene not initialized");
            }
        }
    }

    // === ORDER-INDEPENDENT TRANSPARENCY ===
    {
        if (ImGui::CollapsingHeader("Transparency (OIT)")) {
            bool oitEnabled = m_RenderSystem->IsOITEnabled();
            if (ImGui::Checkbox("Enable Weighted Blended OIT", &oitEnabled)) {
                m_RenderSystem->SetOITEnabled(oitEnabled);
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip(
                "Weighted Blended Order-Independent Transparency\n"
                "(McGuire & Bavoil 2013).\n"
                "Requires composite shader — stub until SPIR-V compiled.");
        }
    }

    // === PER-SCENE OVERRIDE & PROJECT DEFAULTS ===
    if (ImGui::CollapsingHeader("Scene Overrides")) {
        if (ImGui::Checkbox("Use Project Defaults", &m_CurrentSceneUsesProjectDefaults)) {
            if (m_CurrentSceneUsesProjectDefaults) {
                m_SceneManager.GetDefaultRenderSettings().ApplyToRuntime(
                    m_RenderSystem, m_PostProcessing ? &m_PostProcessing->GetSettings() : nullptr);
            }
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip(
            "When checked, this scene uses the project's default rendering settings.\n"
            "Uncheck to override settings per-scene.");

        if (m_CurrentSceneUsesProjectDefaults) {
            ImGui::TextColored(ImVec4(0.6f, 0.8f, 0.6f, 1.0f), "This scene uses project default rendering settings");
        }

        ImGui::Spacing();

        if (ImGui::Button("Set Current as Project Default")) {
            auto current = Renderer::SceneRenderSettings::CaptureFromRuntime(
                m_RenderSystem, m_PostProcessing ? &m_PostProcessing->GetSettings() : nullptr);
            m_SceneManager.SetDefaultRenderSettings(current);
            if (!m_SceneManager.GetProjectPath().empty() && !m_SceneManager.SaveProject()) {
                ShowNotification("Failed to save project settings", NotificationType::Error);
            }
            ENJIN_LOG_INFO(Editor, "Saved current rendering settings as project default");
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip(
            "Save the current rendering settings as the project-level default.\n"
            "New scenes and scenes using project defaults will use these settings.");

        if (!m_CurrentSceneUsesProjectDefaults) {
            ImGui::SameLine();
            if (ImGui::Button("Reset to Project Default")) {
                m_SceneManager.GetDefaultRenderSettings().ApplyToRuntime(
                    m_RenderSystem, m_PostProcessing ? &m_PostProcessing->GetSettings() : nullptr);
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip(
                "Reset this scene's rendering settings to the project default values.");
        }
    }
}

void EditorLayer::DrawSettingsSection_RayTracing() {
    if (!m_RenderSystem) return;

    // === RAY TRACING ===
    {
        bool rtSupported = m_RenderSystem->IsRayTracingSupported();
        if (ImGui::CollapsingHeader("Ray Tracing")) {
            if (rtSupported) {
                ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.3f, 1.0f), "Supported");
            } else {
                ImGui::TextColored(ImVec4(0.9f, 0.3f, 0.3f, 1.0f), "Not Supported");
                ImGui::TextDisabled("GPU does not support VK_KHR_ray_tracing_pipeline");
            }

            if (rtSupported) {
                bool rtEnabled = m_RenderSystem->IsRayTracingEnabled();
                if (ImGui::Checkbox("Enable Ray Tracing", &rtEnabled)) {
                    m_RenderSystem->SetRayTracingEnabled(rtEnabled);
                }

                if (rtEnabled) {
                    // Mode selection
                    const char* modeNames[] = { "Hybrid", "Path Trace" };
                    int rtMode = static_cast<int>(m_RenderSystem->GetRTMode());
                    if (ImGui::Combo("RT Mode", &rtMode, modeNames, 2)) {
                        m_RenderSystem->SetRTMode(static_cast<u32>(rtMode));
                    }
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip(
                        "Hybrid: RT shadows/reflections/AO/GI composited with raster.\n"
                        "Path Trace: Progressive offline-quality renderer.");

                    if (rtMode == 0) {
                        // --- Hybrid mode sub-sections ---

                        // RT Shadows
                        if (auto* rtShadows = m_RenderSystem->GetRTShadows()) {
                            auto& cfg = rtShadows->GetConfig();
                            if (ImGui::TreeNode("RT Shadows")) {
                                ImGui::Checkbox("Enabled##RTShadow", &cfg.enabled);
                                if (cfg.enabled) {
                                    ImGui::DragFloat("Max Distance##RTShadow", &cfg.maxDistance, 1.0f, 1.0f, 500.0f);
                                    ImGui::DragFloat("Soft Radius##RTShadow", &cfg.radius, 0.001f, 0.0f, 0.5f, "%.3f");
                                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Penumbra radius for soft shadows (0 = hard)");
                                }
                                ImGui::TreePop();
                            }
                        }

                        // RT Reflections
                        if (auto* rtReflect = m_RenderSystem->GetRTReflections()) {
                            auto& cfg = rtReflect->GetConfig();
                            if (ImGui::TreeNode("RT Reflections")) {
                                ImGui::Checkbox("Enabled##RTReflect", &cfg.enabled);
                                if (cfg.enabled) {
                                    ImGui::DragFloat("Max Distance##RTReflect", &cfg.maxDistance, 1.0f, 1.0f, 500.0f);
                                    ImGui::SliderFloat("Roughness Threshold", &cfg.roughnessThreshold, 0.0f, 1.0f);
                                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Skip reflections for surfaces rougher than this");
                                }
                                ImGui::TreePop();
                            }
                        }

                        // RT Ambient Occlusion
                        if (auto* rtAO = m_RenderSystem->GetRTAO()) {
                            auto& cfg = rtAO->GetConfig();
                            if (ImGui::TreeNode("RT Ambient Occlusion")) {
                                ImGui::Checkbox("Enabled##RTAO", &cfg.enabled);
                                if (cfg.enabled) {
                                    ImGui::DragFloat("AO Radius", &cfg.radius, 0.1f, 0.1f, 20.0f);
                                    ImGui::DragFloat("AO Power", &cfg.power, 0.1f, 0.1f, 5.0f);
                                }
                                ImGui::TreePop();
                            }
                        }

                        // RT Global Illumination
                        if (auto* rtGI = m_RenderSystem->GetRTGI()) {
                            auto& cfg = rtGI->GetConfig();
                            if (ImGui::TreeNode("RT Global Illumination")) {
                                ImGui::Checkbox("Enabled##RTGI", &cfg.enabled);
                                if (cfg.enabled) {
                                    ImGui::DragFloat("Max Distance##RTGI", &cfg.maxDistance, 1.0f, 1.0f, 200.0f);
                                    ImGui::DragFloat("GI Intensity", &cfg.intensity, 0.1f, 0.0f, 5.0f);
                                    int bounces = static_cast<int>(cfg.bounces);
                                    if (ImGui::SliderInt("Bounces", &bounces, 1, 4)) {
                                        cfg.bounces = static_cast<u32>(bounces);
                                    }
                                }
                                ImGui::TreePop();
                            }
                        }

                        // Composite strengths
                        if (auto* compositor = m_RenderSystem->GetRTCompositor()) {
                            auto& cfg = compositor->GetConfig();
                            if (ImGui::TreeNode("Composite Strengths")) {
                                ImGui::SliderFloat("Shadow##RTComp", &cfg.shadowStrength, 0.0f, 1.0f);
                                ImGui::SliderFloat("Reflection##RTComp", &cfg.reflectionStrength, 0.0f, 1.0f);
                                ImGui::SliderFloat("AO##RTComp", &cfg.aoStrength, 0.0f, 1.0f);
                                ImGui::SliderFloat("GI##RTComp", &cfg.giStrength, 0.0f, 1.0f);
                                ImGui::TreePop();
                            }
                        }
                    } else {
                        // --- Path Trace mode ---
                        if (auto* pathTracer = m_RenderSystem->GetPathTracer()) {
                            auto& cfg = pathTracer->GetConfig();
                            int maxBounces = static_cast<int>(cfg.maxBounces);
                            if (ImGui::SliderInt("Max Bounces", &maxBounces, 1, 16)) {
                                cfg.maxBounces = static_cast<u32>(maxBounces);
                            }
                            int targetSPP = static_cast<int>(cfg.targetSPP);
                            if (ImGui::DragInt("Target SPP", &targetSPP, 16, 1, 65536)) {
                                cfg.targetSPP = static_cast<u32>(targetSPP);
                            }

                            ImGui::Separator();

                            // Firefly clamp
                            ImGui::DragFloat("Firefly Clamp", &cfg.fireflyClampValue, 0.5f, 0.0f, 1000.0f, "%.1f");
                            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Max radiance per sample. Lower values reduce fireflies but may dim highlights.");

                            // NEE toggle
                            if (ImGui::Checkbox("Next Event Estimation (NEE)", &cfg.enableNEE)) {
                                pathTracer->ResetAccumulation();
                            }
                            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Sample lights directly each bounce for faster convergence.");

                            // MIS toggle
                            if (ImGui::Checkbox("Multiple Importance Sampling (MIS)", &cfg.enableMIS)) {
                                pathTracer->ResetAccumulation();
                            }
                            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Balance BRDF and light sampling weights for lower variance.");

                            // Russian Roulette
                            if (ImGui::TreeNode("Russian Roulette")) {
                                int rrMinBounce = static_cast<int>(cfg.russianRouletteMinBounce);
                                if (ImGui::SliderInt("Min Bounce##RR", &rrMinBounce, 0, 16)) {
                                    cfg.russianRouletteMinBounce = static_cast<f32>(rrMinBounce);
                                }
                                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Start Russian Roulette path termination after this many bounces.");
                                ImGui::SliderFloat("Min Survival Prob##RR", &cfg.russianRouletteMinProb, 0.0f, 1.0f, "%.3f");
                                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Minimum probability a path survives at each bounce (lower = more aggressive termination).");
                                ImGui::TreePop();
                            }

                            ImGui::Separator();

                            // Progress
                            u32 accumulated = pathTracer->GetAccumulatedSamples();
                            f32 progress = (cfg.targetSPP > 0)
                                ? static_cast<f32>(accumulated) / static_cast<f32>(cfg.targetSPP)
                                : 0.0f;
                            char overlay[64];
                            snprintf(overlay, sizeof(overlay), "%u / %u SPP", accumulated, cfg.targetSPP);
                            ImGui::ProgressBar(progress, ImVec2(-1, 0), overlay);

                            if (pathTracer->IsConverged()) {
                                ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.3f, 1.0f), "Converged");
                            }

                            if (ImGui::Button("Reset Accumulation")) {
                                pathTracer->ResetAccumulation();
                            }
                        }
                    }

                    // Denoiser settings
                    {
                        int denoiserType = static_cast<int>(m_RenderSystem->GetDenoiserType());
                        const char* denoiserNames[] = { "SVGF (GPU)", "OIDN (Intel)" };
                        int maxDenoiser = (m_RenderSystem->GetOIDNDenoiser()) ? 1 : 0;
                        if (ImGui::Combo("Denoiser", &denoiserType, denoiserNames, maxDenoiser + 1)) {
                            m_RenderSystem->SetDenoiserType(static_cast<u32>(denoiserType));
                        }
                    }

                    // SVGF settings
                    if (m_RenderSystem->GetDenoiserType() == 0) {
                        if (auto* denoiser = m_RenderSystem->GetSVGFDenoiser()) {
                            auto& cfg = denoiser->GetConfig();
                            if (ImGui::TreeNode("SVGF Settings")) {
                                ImGui::SliderFloat("Temporal Alpha", &cfg.temporalAlpha, 0.01f, 0.5f, "%.3f");
                                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Lower = more temporal history (smoother but more ghosting)");
                                int iterations = static_cast<int>(cfg.atrousIterations);
                                if (ImGui::SliderInt("A-Trous Iterations", &iterations, 1, 8)) {
                                    cfg.atrousIterations = static_cast<u32>(iterations);
                                }
                                if (ImGui::Button("Reset History##SVGF")) {
                                    denoiser->ResetHistory();
                                }
                                ImGui::TreePop();
                            }
                        }
                    }

                    // OIDN settings
                    if (m_RenderSystem->GetDenoiserType() == 1) {
                        if (auto* oidn = m_RenderSystem->GetOIDNDenoiser()) {
                            auto& cfg = oidn->GetConfig();
                            if (ImGui::TreeNode("OIDN Settings")) {
                                int quality = static_cast<int>(cfg.quality);
                                const char* qualityNames[] = { "Fast", "Default", "High" };
                                if (ImGui::Combo("Quality", &quality, qualityNames, 3)) {
                                    cfg.quality = static_cast<Renderer::OIDNQuality>(quality);
                                }
                                ImGui::SliderFloat("Input Scale", &cfg.inputScale, 0.0f, 10.0f, "%.2f");
                                if (ImGui::IsItemHovered()) ImGui::SetTooltip("0 = auto-detect, >0 = manual scale for HDR input");
                                ImGui::TreePop();
                            }
                        }
                    }
                }

                // Stats
                if (auto* asManager = m_RenderSystem->GetASManager()) {
                    ImGui::Separator();
                    ImGui::Text("BLAS Count: %u", asManager->GetBLASCount());
                    ImGui::Text("Instance Count: %u", asManager->GetInstanceCount());
                }
            }
        }
    }
}

void EditorLayer::DrawSettingsSection_LightProbes() {
    if (!m_RenderSystem) return;

    // === LIGHT PROBES ===
    {
        if (ImGui::CollapsingHeader("Light Probes")) {
            if (auto* shLighting = m_RenderSystem->GetSHLighting()) {
                auto& grid = shLighting->GetGrid();
                u32 probeCount = shLighting->GetProbeCount();
                u32 bakedCount = 0;
                for (auto& p : shLighting->GetProbes()) { if (p.baked) ++bakedCount; }

                ImGui::Text("Probes: %u (%u baked)", probeCount, bakedCount);

                // Visualization toggle
                ImGui::Checkbox("Show Probes in Viewport##SHViz", &m_ShowSHProbes);
                ImGui::SameLine();
                ImGui::Checkbox("Show Grid Bounds##SHGrid", &m_ShowSHGridBounds);

                if (ImGui::TreeNode("Grid Configuration")) {
                    f32 boundsMin[3] = { grid.boundsMin.x, grid.boundsMin.y, grid.boundsMin.z };
                    if (ImGui::DragFloat3("Bounds Min", boundsMin, 0.5f)) {
                        grid.boundsMin = Math::Vector3(boundsMin[0], boundsMin[1], boundsMin[2]);
                    }
                    f32 boundsMax[3] = { grid.boundsMax.x, grid.boundsMax.y, grid.boundsMax.z };
                    if (ImGui::DragFloat3("Bounds Max", boundsMax, 0.5f)) {
                        grid.boundsMax = Math::Vector3(boundsMax[0], boundsMax[1], boundsMax[2]);
                    }

                    int resX = static_cast<int>(grid.resolutionX);
                    int resY = static_cast<int>(grid.resolutionY);
                    int resZ = static_cast<int>(grid.resolutionZ);
                    if (ImGui::SliderInt("Resolution X", &resX, 1, 16)) grid.resolutionX = static_cast<u32>(resX);
                    if (ImGui::SliderInt("Resolution Y", &resY, 1, 8)) grid.resolutionY = static_cast<u32>(resY);
                    if (ImGui::SliderInt("Resolution Z", &resZ, 1, 16)) grid.resolutionZ = static_cast<u32>(resZ);

                    if (ImGui::Button("Generate Grid")) {
                        shLighting->GenerateGridProbes();
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Bake All")) {
                        shLighting->BakeAll(m_World);
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Clear##SHProbes")) {
                        shLighting->Clear();
                    }

                    ImGui::TreePop();
                }

                // Per-probe list
                if (probeCount > 0 && ImGui::TreeNode("Probe List")) {
                    for (auto& probe : shLighting->GetProbes()) {
                        ImGui::PushID(static_cast<int>(probe.id));
                        ImVec4 statusColor = probe.baked
                            ? ImVec4(0.2f, 0.8f, 0.3f, 1.0f)
                            : ImVec4(0.8f, 0.3f, 0.2f, 1.0f);
                        ImGui::TextColored(statusColor, "%s", probe.baked ? "[BAKED]" : "[EMPTY]");
                        ImGui::SameLine();
                        ImGui::Text("Probe %u (%.1f, %.1f, %.1f)", probe.id,
                            probe.position.x, probe.position.y, probe.position.z);

                        // Per-probe bake button
                        ImGui::SameLine();
                        if (ImGui::SmallButton("Bake")) {
                            shLighting->BakeProbe(probe.id, m_World);
                        }
                        ImGui::SameLine();
                        if (ImGui::SmallButton("Delete")) {
                            shLighting->RemoveProbe(probe.id);
                            ImGui::PopID();
                            break; // Iterator invalidated
                        }

                        // Show irradiance preview for baked probes
                        if (probe.baked) {
                            f32 l0r = probe.coefficientsR[0] * 0.282095f;
                            f32 l0g = probe.coefficientsG[0] * 0.282095f;
                            f32 l0b = probe.coefficientsB[0] * 0.282095f;
                            ImGui::SameLine();
                            ImGui::ColorButton("##irrad", ImVec4(
                                std::clamp(l0r, 0.0f, 1.0f),
                                std::clamp(l0g, 0.0f, 1.0f),
                                std::clamp(l0b, 0.0f, 1.0f), 1.0f),
                                ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoPicker,
                                ImVec2(14, 14));
                        }
                        ImGui::PopID();
                    }
                    ImGui::TreePop();
                }

                // Manual probe placement
                if (ImGui::TreeNode("Add Probe")) {
                    static f32 newProbePos[3] = { 0.0f, 1.0f, 0.0f };
                    ImGui::DragFloat3("Position##NewProbe", newProbePos, 0.1f);
                    if (ImGui::Button("Add Probe at Position")) {
                        shLighting->AddProbe(Math::Vector3(newProbePos[0], newProbePos[1], newProbePos[2]));
                    }
                    ImGui::TreePop();
                }
            } else {
                ImGui::TextDisabled("SH Lighting system not initialized");
            }
        }
    }
}



} // namespace Editor
} // namespace Enjin
