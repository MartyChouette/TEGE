#include "Enjin/Editor/EditorLayer.h"
#include "Enjin/Editor/InspectorUndo.h"
#include "Enjin/Editor/ScenePicker.h"
#include "Enjin/Renderer/Upscaling/IUpscaler.h"
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
#include "Enjin/Renderer/RayTracing/RTTemporalReuse.h"
#include "Enjin/Renderer/RayTracing/ReSTIR.h"
#include "Enjin/Renderer/RayTracing/RadianceCache.h"
#include "Enjin/Renderer/RayTracing/SurfelRadianceCache.h"
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
                ImGui::DragFloat("Depth Weight##PPVolCel", &s.celOutlineDepthWeight, 0.01f, 0.0f, 2.0f);
                ImGui::DragFloat("Normal Weight##PPVolCel", &s.celOutlineNormalWeight, 0.01f, 0.0f, 2.0f);
                ImGui::DragFloat("Curvature Weight##PPVolCel", &s.celOutlineCurvatureWeight, 0.01f, 0.0f, 2.0f);
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
    ImGui::SetNextWindowSize(ImVec2(800, 540), ImGuiCond_FirstUseEver);

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
    bool panelOpen = true;
    ImGui::Begin("Game View", &panelOpen, gameViewFlags);
    if (!panelOpen) {
        SetPanelVisibility(EditorPanel::GameView, false);
    }

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

    // Game View aspect ratio + frame rate controls (right side)
    ImGui::SameLine(ImGui::GetWindowWidth() - 380);
    {
        int current = static_cast<int>(m_GameViewAspect);
        ImGui::SetNextItemWidth(130.0f);
        if (ImGui::Combo("##GameAspect", &current, AspectRatioLabels, static_cast<int>(AspectRatio::Count))) {
            m_GameViewAspect = static_cast<AspectRatio>(current);
        }
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(90);
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
            // Calculate preview size using selected aspect ratio (default to 16:9 if Free)
            f32 gameAspect = AspectRatioValues[static_cast<int>(m_GameViewAspect)];
            if (gameAspect <= 0.0f) gameAspect = 16.0f / 9.0f;  // Free defaults to 16:9 for game view
            ImVec2 previewSize = ComputeAspectConstrainedSize(availSize.x, availSize.y, gameAspect);
            f32 previewWidth = previewSize.x;
            f32 previewHeight = previewSize.y;

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

            // Cache game view screen bounds for pause overlay
            m_GameViewScreenX = p0.x;
            m_GameViewScreenY = p0.y;
            m_GameViewScreenW = previewWidth;
            m_GameViewScreenH = previewHeight;

            // Display render target texture or fallback dark rect
            VkDescriptorSet texId = m_GameViewRenderTarget ? m_GameViewRenderTarget->GetImGuiTextureID() : VK_NULL_HANDLE;
            bool usedImage = false;
            if (texId != VK_NULL_HANDLE) {
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

            // Render parallax scrolling backgrounds (2D scenes, ImGui overlay)
            m_ParallaxSystem.Render(previewWidth, previewHeight);

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

    // Render pause menu inside the Game View panel (not as a top-level window)
    if (m_GameMenu.IsMenuOpen() && !m_FocusMode) {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        // Draw buttons centered in the game view preview area
        f32 btnW = 200.0f;
        f32 btnH = 30.0f;
        f32 spacing = 8.0f;
        const char* labels[] = {"Resume", "Options", "How to Play", "Quit to Menu"};
        const char* actions[] = {"resume", "options", "how_to_play", "quit_to_menu"};
        f32 totalH = 4 * btnH + 3 * spacing + 30.0f; // buttons + title
        f32 startX = m_GameViewScreenX + (m_GameViewScreenW - btnW) * 0.5f;
        f32 startY = m_GameViewScreenY + (m_GameViewScreenH - totalH) * 0.5f;

        // Title
        const char* title = "PAUSED";
        ImVec2 titleSize = ImGui::CalcTextSize(title);
        dl->AddText(ImVec2(m_GameViewScreenX + (m_GameViewScreenW - titleSize.x) * 0.5f, startY),
                    IM_COL32(255, 255, 255, 240), title);
        startY += 30.0f;

        // Buttons using invisible ImGui buttons for click handling
        for (int i = 0; i < 4; ++i) {
            ImVec2 bMin(startX, startY + i * (btnH + spacing));
            ImVec2 bMax(startX + btnW, bMin.y + btnH);
            bool hovered = ImGui::IsMouseHoveringRect(bMin, bMax);
            dl->AddRectFilled(bMin, bMax,
                hovered ? IM_COL32(80, 80, 120, 200) : IM_COL32(40, 40, 60, 180), 4.0f);
            ImVec2 labelSize = ImGui::CalcTextSize(labels[i]);
            dl->AddText(ImVec2(bMin.x + (btnW - labelSize.x) * 0.5f, bMin.y + (btnH - labelSize.y) * 0.5f),
                        IM_COL32(255, 255, 255, 230), labels[i]);
            if (hovered && ImGui::IsMouseClicked(0)) {
                if (m_GameMenu.GetCallback()) m_GameMenu.GetCallback()(actions[i]);
            }
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
        ImGui::TextDisabled("Bloom, color grading, film grain, depth of field, screen-space effects");
        auto& settings = m_PostProcessing->GetSettings();
        bool hasDepth = m_PostProcessing->IsDepthSourceReady();

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
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Glow around bright areas.\nThreshold controls which pixels bloom; intensity controls the strength.");

            if (settings.bloomEnabled) {
                ImGui::DragFloat("Threshold", &settings.bloomThreshold, 0.01f, 0.0f, 5.0f);
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Minimum brightness to trigger bloom (0 = everything blooms)");
                ImGui::DragFloat("Intensity##Bloom", &settings.bloomIntensity, 0.01f, 0.0f, 2.0f);
                ImGui::DragFloat("Radius", &settings.bloomRadius, 0.001f, 0.001f, 0.1f);
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Spread of the bloom glow");
            }
        }

        // Vignette
        if (ImGui::CollapsingHeader("Vignette")) {
            bool vignetteEnabled = settings.vignetteEnabled != 0;
            if (ImGui::Checkbox("Enabled##Vignette", &vignetteEnabled)) {
                settings.vignetteEnabled = vignetteEnabled ? 1 : 0;
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Darkens the edges and corners of the screen.\nSimulates lens falloff for a cinematic look.");

            if (settings.vignetteEnabled) {
                ImGui::DragFloat("Intensity##Vignette", &settings.vignetteIntensity, 0.01f, 0.0f, 2.0f);
                ImGui::DragFloat("Smoothness", &settings.vignetteSmoothness, 0.01f, 0.0f, 1.0f);
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("How gradually the darkening fades in from the edges");
            }
        }

        // Chromatic Aberration
        if (ImGui::CollapsingHeader("Chromatic Aberration")) {
            bool caEnabled = settings.chromaticAberrationEnabled != 0;
            if (ImGui::Checkbox("Enabled##CA", &caEnabled)) {
                settings.chromaticAberrationEnabled = caEnabled ? 1 : 0;
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Splits RGB channels near screen edges.\nSimulates cheap lens imperfections for a gritty or cinematic feel.");

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
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Adds animated film grain noise over the image.\nGives a cinematic / retro film look.");

            if (settings.filmGrainEnabled) {
                ImGui::DragFloat("Intensity##Grain", &settings.filmGrainIntensity, 0.001f, 0.0f, 0.2f);
            }
        }

        // Depth of Field
        if (ImGui::CollapsingHeader("Depth of Field")) {
            if (!hasDepth) ImGui::BeginDisabled();
            bool dofEnabled = settings.dofEnabled != 0;
            if (ImGui::Checkbox("Enabled##DOF", &dofEnabled)) {
                settings.dofEnabled = dofEnabled ? 1 : 0;
            }
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                ImGui::SetTooltip("Simulates camera focus: blurs objects outside the focal range.\nReads the depth buffer to compute Circle of Confusion per pixel.");

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
            if (!hasDepth) ImGui::EndDisabled();
        }

        // Tilt-Shift
        if (ImGui::CollapsingHeader("Tilt-Shift")) {
            if (!hasDepth) ImGui::BeginDisabled();
            bool tsEnabled = settings.tiltShiftEnabled != 0;
            if (ImGui::Checkbox("Enabled##TiltShift", &tsEnabled)) {
                settings.tiltShiftEnabled = tsEnabled ? 1 : 0;
            }
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                ImGui::SetTooltip("Miniature / diorama effect.\nBlurs top and bottom of the screen based on depth.");

            if (settings.tiltShiftEnabled) {
                ImGui::SliderFloat("Focus Y", &settings.tiltShiftFocusY, 0.0f, 1.0f);
                ImGui::DragFloat("Band Width", &settings.tiltShiftBandWidth, 0.01f, 0.01f, 1.0f);
                ImGui::DragFloat("Blur Amount", &settings.tiltShiftBlurAmount, 0.1f, 0.0f, 10.0f);
            }
            if (!hasDepth) ImGui::EndDisabled();
        }

        // Anti-Aliasing
        if (ImGui::CollapsingHeader("Anti-Aliasing")) {
            const char* aaModes[] = { "None", "FXAA", "TAA", "SMAA", "MSAA 2x", "MSAA 4x", "MSAA 8x" };
            int aaMode = static_cast<int>(settings.aaMode);
            if (ImGui::Combo("AA Mode", &aaMode, aaModes, IM_ARRAYSIZE(aaModes))) {
                u32 newMode = static_cast<u32>(aaMode);
                settings.aaMode = newMode;
                // Sync legacy fxaaEnabled flag
                settings.fxaaEnabled = (newMode == 1) ? 1 : 0;
                // MSAA modes (4/5/6) require render pass recreation via RenderSystem
                if (m_RenderSystem) {
                    m_RenderSystem->SetAAMode(newMode);
                }
            }

            // FXAA settings
            if (settings.aaMode == 1) {
                ImGui::DragFloat("Span Max", &settings.fxaaSpanMax, 0.5f, 2.0f, 16.0f);
                ImGui::DragFloat("Reduce Min", &settings.fxaaReduceMin, 0.001f, 0.0f, 0.1f, "%.4f");
                ImGui::DragFloat("Reduce Mul", &settings.fxaaReduceMul, 0.01f, 0.0f, 0.5f);
            }

            // SMAA info
            if (settings.aaMode == 3) {
                ImGui::TextDisabled("Enhanced morphological AA with edge walking");
                ImGui::TextDisabled("Higher quality than FXAA, no temporal artifacts");
            }

            // TAA settings (CPU-side config for the TAA compute pass)
            if (settings.aaMode == 2) {
                ImGui::SliderFloat("Sharpness", &settings.taaSharpness, 0.0f, 1.0f, "%.2f");
                ImGui::SliderFloat("Jitter Scale", &settings.taaJitterScale, 0.0f, 2.0f, "%.2f");
                ImGui::SliderFloat("Feedback Min", &settings.taaFeedbackMin, 0.0f, 1.0f, "%.2f");
                ImGui::SliderFloat("Feedback Max", &settings.taaFeedbackMax, 0.0f, 1.0f, "%.2f");
            }

            // MSAA info
            if (settings.aaMode >= 4 && settings.aaMode <= 6) {
                int sampleCount = 1 << (settings.aaMode - 3);  // 4->2x, 5->4x, 6->8x
                ImGui::TextDisabled("Hardware multisampling at %dx", sampleCount);
                ImGui::TextDisabled("Resolves to single-sample for post-processing");
                if (m_RenderSystem) {
                    u32 maxSamples = m_RenderSystem->GetMaxMSAASamples();
                    if (static_cast<u32>(sampleCount) > maxSamples) {
                        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f),
                            "GPU supports up to %dx MSAA", maxSamples);
                    }
                }
                ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f),
                    "MSAA and TAA are mutually exclusive");
            }
        }

        // Temporal Upscaling
        if (ImGui::CollapsingHeader("Upscaling")) {
            const char* upscalerTypes[] = { "None", "FSR 2 (Built-in)", "DLSS", "XeSS" };
            int upType = static_cast<int>(settings.upscalerType);
            if (ImGui::Combo("Upscaler", &upType, upscalerTypes, IM_ARRAYSIZE(upscalerTypes))) {
                settings.upscalerType = static_cast<u32>(upType);
                // Immediately sync to RenderSystem so upscaler resources are created/destroyed
                if (m_RenderSystem) {
                    m_RenderSystem->SetUpscalerType(settings.upscalerType);
                }
            }

            if (settings.upscalerType > 0) {
                const char* qualityModes[] = { "Performance (50%)", "Balanced (58%)", "Quality (67%)", "Ultra Quality (77%)" };
                int upQuality = static_cast<int>(settings.upscalerQuality);
                if (ImGui::Combo("Quality##Upscaler", &upQuality, qualityModes, IM_ARRAYSIZE(qualityModes))) {
                    settings.upscalerQuality = static_cast<u32>(upQuality);
                    if (m_RenderSystem) {
                        m_RenderSystem->SetUpscalerQuality(settings.upscalerQuality);
                    }
                }

                if (ImGui::SliderFloat("Sharpness##Upscaler", &settings.upscalerSharpness, 0.0f, 1.0f, "%.2f")) {
                    if (m_RenderSystem) {
                        m_RenderSystem->SetUpscalerSharpness(settings.upscalerSharpness);
                    }
                }

                // Availability indicators
                ImGui::Spacing();
                ImGui::TextDisabled("Backend availability:");
                ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "  FSR 2: Built-in (Lanczos + CAS)");

                // DLSS availability — green for SDK, yellow for built-in
                {
                    bool dlssActive = m_RenderSystem && m_RenderSystem->GetUpscaler() &&
                                      m_RenderSystem->GetUpscalerType() == 2;
#ifdef ENJIN_HAS_DLSS_SDK
                    ImVec4 dlssColor(0.4f, 1.0f, 0.4f, 1.0f);  // green = SDK linked
                    if (dlssActive) {
                        ImGui::TextColored(dlssColor, "  DLSS: %s (SDK)",
                                           m_RenderSystem->GetUpscaler()->GetName());
                    } else {
                        ImGui::TextColored(dlssColor, "  DLSS: SDK linked (NVIDIA GPUs)");
                    }
#else
                    ImVec4 dlssColor(1.0f, 0.9f, 0.5f, 1.0f);  // yellow = built-in fallback
                    if (dlssActive) {
                        ImGui::TextColored(dlssColor, "  DLSS: %s (Built-in)",
                                           m_RenderSystem->GetUpscaler()->GetName());
                    } else {
                        ImGui::TextColored(dlssColor, "  DLSS: Built-in (Lanczos + CAS, NVIDIA GPUs only)");
                    }
#endif
                }

                // XeSS availability — green for SDK, yellow for built-in
                {
                    bool xessActive = m_RenderSystem && m_RenderSystem->GetUpscaler() &&
                                      m_RenderSystem->GetUpscalerType() == 3;
#ifdef ENJIN_HAS_XESS_SDK
                    ImVec4 xessColor(0.4f, 1.0f, 0.4f, 1.0f);  // green = SDK linked
                    if (xessActive) {
                        ImGui::TextColored(xessColor, "  XeSS: %s (SDK)",
                                           m_RenderSystem->GetUpscaler()->GetName());
                    } else {
                        ImGui::TextColored(xessColor, "  XeSS: SDK linked (all GPUs via DP4a)");
                    }
#else
                    ImVec4 xessColor(1.0f, 0.9f, 0.5f, 1.0f);  // yellow = built-in fallback
                    if (xessActive) {
                        ImGui::TextColored(xessColor, "  XeSS: %s (Built-in)",
                                           m_RenderSystem->GetUpscaler()->GetName());
                    } else {
                        ImGui::TextColored(xessColor, "  XeSS: Built-in (Lanczos + CAS, all GPUs)");
                    }
#endif
                }

                // Pipeline description based on active backend
                ImGui::Spacing();
                if (settings.upscalerType == 1) {
                    ImGui::TextDisabled("Pipeline: TAA resolve (low-res) -> Lanczos upsample -> CAS sharpen");
                } else if (settings.upscalerType == 2) {
#ifdef ENJIN_HAS_DLSS_SDK
                    ImGui::TextDisabled("Pipeline: DLSS Super Resolution (Streamline SDK)");
#else
                    ImGui::TextDisabled("Pipeline: Lanczos upsample -> CAS sharpen (DLSS SDK not linked)");
#endif
                } else if (settings.upscalerType == 3) {
#ifdef ENJIN_HAS_XESS_SDK
                    ImGui::TextDisabled("Pipeline: XeSS temporal upscaling (Intel XeSS SDK)");
#else
                    ImGui::TextDisabled("Pipeline: Lanczos upsample -> CAS sharpen (XeSS SDK not linked)");
#endif
                }

                if (settings.upscalerType > 0 && settings.aaMode == 2) {
                    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "TAA runs at render resolution, upscaler handles upsampling");
                }
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
                const char* paletteModes[] = { "Per-Channel", "PICO-8", "Game Boy", "NES", "CGA", "C64" };
                int mode = static_cast<int>(settings.paletteMode);
                if (ImGui::Combo("Palette##PalMode", &mode, paletteModes, 6)) {
                    settings.paletteMode = static_cast<u32>(mode);
                }

                if (settings.paletteMode == 0) {
                    int colors = static_cast<int>(settings.paletteColors);
                    if (ImGui::SliderInt("Colors", &colors, 2, 256)) {
                        settings.paletteColors = static_cast<u32>(colors);
                    }
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Number of color levels per channel");
                } else {
                    ImGui::TextDisabled("Using named palette — color count is fixed");
                }
            }
        }

        // Normal Quantization (Pixel Art)
        if (ImGui::CollapsingHeader("Normal Quantization")) {
            f32 nqSteps = m_RenderSystem ? m_RenderSystem->GetNormalQuantizeSteps() : 0.0f;
            bool nqEnabled = nqSteps >= 4.0f;
            if (ImGui::Checkbox("Enabled##NormQuant", &nqEnabled)) {
                if (m_RenderSystem) m_RenderSystem->SetNormalQuantizeSteps(nqEnabled ? 6.0f : 0.0f);
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Snap normals to N cardinal directions (2D-in-3D faceted look)");
            if (nqEnabled && m_RenderSystem) {
                int steps = static_cast<int>(nqSteps);
                if (ImGui::SliderInt("Directions", &steps, 4, 16)) {
                    m_RenderSystem->SetNormalQuantizeSteps(static_cast<f32>(steps));
                }
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
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Edge detection on the depth buffer.\nDraws dark outlines around geometry for a toon/cel look.");
            if (outlineOn) {
                ImGui::SliderFloat("Outline Thickness##PP", &s.celOutlineThickness, 0.5f, 5.0f);
                ImGui::SliderFloat("Outline Threshold##PP", &s.celOutlineThreshold, 0.001f, 0.5f);
                ImGui::SliderFloat("Depth Weight##PP", &s.celOutlineDepthWeight, 0.0f, 2.0f);
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Weight of depth-based edge detection");
                ImGui::SliderFloat("Normal Weight##PP", &s.celOutlineNormalWeight, 0.0f, 2.0f);
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Weight of normal-based edge detection (catches surface detail depth misses)");
                ImGui::SliderFloat("Curvature Weight##PP", &s.celOutlineCurvatureWeight, 0.0f, 2.0f);
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Curvature-driven thickness variation (NPR pen/ink).\nThicker lines on curved edges, thinner on flat surfaces.");
                ImGui::ColorEdit3("Outline Color##PP", &s.celOutlineColor.x);
            }
        }

        // ================================================================
        // Depth-Based Effects (all read the scene depth buffer)
        // ================================================================
        ImGui::Spacing();
        ImGui::SeparatorText("Depth-Based Effects");
        if (!hasDepth) {
            ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.2f, 1.0f),
                "Scene depth not available — effects below will be inactive.");
        }

        // SSAO
        if (ImGui::CollapsingHeader("SSAO (Ambient Occlusion)")) {
            if (!hasDepth) ImGui::BeginDisabled();
            bool ssaoOn = settings.ssaoEnabled != 0;
            if (ImGui::Checkbox("Enabled##SSAO", &ssaoOn)) {
                settings.ssaoEnabled = ssaoOn ? 1 : 0;
            }
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                ImGui::SetTooltip("Screen-space ambient occlusion.\nDarkens creases and corners where light is occluded.\nReads the depth buffer to estimate local geometry.");
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
            if (!hasDepth) ImGui::EndDisabled();
        }

        // Contact Shadows
        if (ImGui::CollapsingHeader("Contact Shadows")) {
            if (!hasDepth) ImGui::BeginDisabled();
            bool csOn = settings.contactShadowsEnabled != 0;
            if (ImGui::Checkbox("Enabled##ContactShadows", &csOn)) {
                settings.contactShadowsEnabled = csOn ? 1 : 0;
            }
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                ImGui::SetTooltip("Screen-space ray-marched contact shadows.\nAdds fine shadow detail near object edges that shadow maps miss.");
            if (settings.contactShadowsEnabled) {
                ImGui::DragFloat("Ray Length##CS", &settings.contactShadowsLength, 0.001f, 0.001f, 0.5f, "%.4f");
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Screen-space ray march length (UV space)");
                int steps = static_cast<int>(settings.contactShadowsSteps);
                if (ImGui::SliderInt("Steps##CS", &steps, 4, 32)) {
                    settings.contactShadowsSteps = static_cast<u32>(steps);
                }
                ImGui::DragFloat("Intensity##CS", &settings.contactShadowsIntensity, 0.01f, 0.0f, 2.0f);
            }
            if (!hasDepth) ImGui::EndDisabled();
        }

        // God Rays
        if (ImGui::CollapsingHeader("God Rays")) {
            if (!hasDepth) ImGui::BeginDisabled();
            bool grOn = settings.godRaysEnabled != 0;
            if (ImGui::Checkbox("Enabled##GodRays", &grOn)) {
                settings.godRaysEnabled = grOn ? 1 : 0;
            }
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                ImGui::SetTooltip("Radial light shafts from a directional light source.\nRay-marches the depth buffer to detect occluders.");
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
            if (!hasDepth) ImGui::EndDisabled();
        }

        // Fake Caustics
        if (ImGui::CollapsingHeader("Fake Caustics")) {
            if (!hasDepth) ImGui::BeginDisabled();
            bool fcOn = settings.causticsEnabled != 0;
            if (ImGui::Checkbox("Enabled##Caustics", &fcOn)) {
                settings.causticsEnabled = fcOn ? 1 : 0;
            }
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                ImGui::SetTooltip("Procedural animated caustics projected below a water plane.\nUses depth to reconstruct world position for the pattern.");
            if (settings.causticsEnabled) {
                ImGui::DragFloat("Intensity##Caustics", &settings.causticsIntensity, 0.01f, 0.0f, 2.0f);
                ImGui::DragFloat("Scale##Caustics", &settings.causticsScale, 0.01f, 0.1f, 10.0f);
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Voronoi pattern scale");
                ImGui::DragFloat("Speed##Caustics", &settings.causticsSpeed, 0.01f, 0.0f, 5.0f);
                ImGui::DragFloat("Water Y##Caustics", &settings.causticsWaterY, 0.1f, -100.0f, 100.0f);
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("World-space Y height of the water surface");
            }
            if (!hasDepth) ImGui::EndDisabled();
        }

        // Fog Shafts
        if (ImGui::CollapsingHeader("Fog Shafts")) {
            if (!hasDepth) ImGui::BeginDisabled();
            bool fsOn = settings.fogShaftsEnabled != 0;
            if (ImGui::Checkbox("Enabled##FogShafts", &fsOn)) {
                settings.fogShaftsEnabled = fsOn ? 1 : 0;
            }
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                ImGui::SetTooltip("Volumetric-look fog shafts via depth ray march.\nCreates atmospheric haze between camera and geometry.");
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
            if (!hasDepth) ImGui::EndDisabled();
        }
    }
}

void EditorLayer::DrawSettingsSection_RetroEffects() {
    // === RETRO EFFECTS (PS1/N64/PS2/GameCube presets) ===
    if (ImGui::CollapsingHeader("Retro Effects", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::TextDisabled("Vertex snapping, affine textures, scanlines, dithering, color reduction");
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
                    ImGui::DragFloat("Tape Dropout", &vhs.tapeDropout, 0.01f, 0.0f, 1.0f);
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Random horizontal signal loss bands (VHS degradation)");
                }
                ImGui::TreePop();
            }

            // Film Gate Weave
            if (ImGui::TreeNode("Film Gate Weave")) {
                auto& settings = m_PostProcessing->GetSettings();
                bool weaveOn = settings.filmGateWeaveEnabled != 0;
                if (ImGui::Checkbox("Enabled##GateWeave", &weaveOn)) {
                    settings.filmGateWeaveEnabled = weaveOn ? 1u : 0u;
                }
                if (weaveOn) {
                    ImGui::DragFloat("Intensity##GateWeave", &settings.filmGateWeaveIntensity, 0.0005f, 0.0f, 0.02f, "%.4f");
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("UV jitter amount (physical gate movement)");
                    ImGui::DragFloat("Speed##GateWeave", &settings.filmGateWeaveSpeed, 0.1f, 0.1f, 5.0f);
                }
                ImGui::TreePop();
            }

            // Light Leaks
            if (ImGui::TreeNode("Light Leaks")) {
                auto& settings = m_PostProcessing->GetSettings();
                bool leakOn = settings.lightLeakEnabled != 0;
                if (ImGui::Checkbox("Enabled##LightLeak", &leakOn)) {
                    settings.lightLeakEnabled = leakOn ? 1u : 0u;
                }
                if (leakOn) {
                    ImGui::DragFloat("Intensity##LightLeak", &settings.lightLeakIntensity, 0.01f, 0.0f, 1.0f);
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Additive warm glow strength");
                    ImGui::DragFloat("Speed##LightLeak", &settings.lightLeakSpeed, 0.05f, 0.0f, 2.0f);
                }
                ImGui::TreePop();
            }

            // TAM Hatching (NPR)
            if (ImGui::TreeNode("TAM Hatching")) {
                auto& settings = m_PostProcessing->GetSettings();
                bool tamOn = settings.tamHatchingEnabled != 0;
                if (ImGui::Checkbox("Enabled##TAM", &tamOn)) {
                    settings.tamHatchingEnabled = tamOn ? 1u : 0u;
                }
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Tonal Art Map: 6-tone luminance-based hatching patterns\nfor a pen & ink / etching illustration look");
                ImGui::TreePop();
            }

            // Watercolor
            if (ImGui::TreeNode("Watercolor")) {
                auto& settings = m_PostProcessing->GetSettings();
                bool wcOn = settings.watercolorEnabled != 0;
                if (ImGui::Checkbox("Enabled##Watercolor", &wcOn)) {
                    settings.watercolorEnabled = wcOn ? 1u : 0u;
                }
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Watercolor post-process: edge darkening,\npigment pooling, color granulation, wet-edge diffusion");
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

void EditorLayer::DrawSettingsSection_ArtStylePreset() {
    if (!m_RenderSystem) return;

    const char* presetNames[] = {
        "Realistic PBR",
        "Classic Blinn-Phong",
        "Hand-Painted",
        "Toon / Anime",
        "Low-Poly Retro",
        "Pixel Art",
        "NPR Sketch"
    };
    constexpr int presetCount = 7;

    int current = static_cast<int>(m_ArtStylePreset);
    if (ImGui::Combo("Art Style Preset", &current, presetNames, presetCount)) {
        m_ArtStylePreset = static_cast<u32>(current);

        // Build a SceneRenderSettings, apply the preset, then push to runtime
        Renderer::SceneRenderSettings settings =
            Renderer::SceneRenderSettings::CaptureFromRuntime(
                m_RenderSystem,
                m_PostProcessing ? &m_PostProcessing->GetSettings() : nullptr);
        Renderer::ApplyArtStylePreset(settings, m_ArtStylePreset);
        settings.ApplyToRuntime(
            m_RenderSystem,
            m_PostProcessing ? &m_PostProcessing->GetSettings() : nullptr);
    }
    ImGui::SetItemTooltip(
        "Apply a preconfigured rendering style.\n"
        "Individual settings can still be tweaked after choosing a preset.");
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
        ImGui::SetItemTooltip("None: no sky, clear color only.\nCubemap: 6-face texture for realistic skies.\nProcedural: gradient sky with sun direction.\nSolid Color: flat single color.");

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
        ImGui::TextDisabled("Cascaded shadow maps, soft shadows, per-entity dithering");
        bool shadows = m_RenderSystem->IsShadowsEnabled();
        if (ImGui::Checkbox("Shadows", &shadows)) {
            m_RenderSystem->SetShadowsEnabled(shadows);
        }
        ImGui::SetItemTooltip("Enable real-time shadow mapping for directional, point, and spot lights.");

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
            ImGui::SetItemTooltip("Shadow map texture resolution per light.\nHigher = sharper shadows, more VRAM.\n512 low, 2048 default, 4096 high-end.");

            // Shadow distance
            f32 shadowDist = m_RenderSystem->GetShadowDistance();
            if (ImGui::SliderFloat("Shadow Distance", &shadowDist, 10.0f, 500.0f, "%.0f")) {
                m_RenderSystem->SetShadowDistance(shadowDist);
            }
            ImGui::SetItemTooltip("Maximum distance from camera where shadows are rendered.\nFarther = more coverage but lower shadow detail per texel.");

            // Shadow strength
            f32 shadowStr = m_RenderSystem->GetShadowStrength();
            if (ImGui::SliderFloat("Shadow Strength", &shadowStr, 0.0f, 1.0f)) {
                m_RenderSystem->SetShadowStrength(shadowStr);
            }
            ImGui::SetItemTooltip("Shadow darkness. 1.0 = fully dark shadows, 0.0 = invisible.\nLower values soften the visual impact.");

            // Shadow softness
            f32 shadowSoft = m_RenderSystem->GetShadowSoftness();
            if (ImGui::SliderFloat("Shadow Softness", &shadowSoft, 0.0f, 5.0f, "%.1f")) {
                m_RenderSystem->SetShadowSoftness(shadowSoft);
            }
            ImGui::SetItemTooltip("PCF filter radius for soft shadow edges.\n0 = hard pixel edges, 1-5 = progressively softer penumbra.");

            // Progressive cascade updates
            bool cascadeProg = m_RenderSystem->IsCascadeProgressiveUpdate();
            if (ImGui::Checkbox("Progressive Cascade Updates", &cascadeProg)) {
                m_RenderSystem->SetCascadeProgressiveUpdate(cascadeProg);
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Far shadow cascades update every N frames instead of every frame.\nReduces shadow pass GPU cost.");
            if (cascadeProg) {
                int interval = static_cast<int>(m_RenderSystem->GetCascadeFarUpdateInterval());
                if (ImGui::SliderInt("Far Cascade Update Interval", &interval, 2, 8)) {
                    m_RenderSystem->SetCascadeFarUpdateInterval(static_cast<u32>(interval));
                }
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("How many frames between far cascade updates.\nHigher = cheaper but more shadow lag.");
            }
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
        ImGui::SetItemTooltip("Base color for ambient light applied uniformly to all surfaces.\nSimulates indirect light in the scene.");

        f32 ambientIntensity = m_RenderSystem->GetAmbientIntensity();
        if (ImGui::DragFloat("Ambient Intensity", &ambientIntensity, 0.05f, 0.0f, 5.0f)) {
            m_RenderSystem->SetAmbientIntensity(ambientIntensity);
        }
        ImGui::SetItemTooltip("Brightness of ambient light. 0 = pitch-black shadows,\nhigher values fill in areas not reached by direct lights.");
    }
}

void EditorLayer::DrawSettingsSection_ShadingModel() {
    if (!m_RenderSystem) return;

    if (ImGui::CollapsingHeader("Shading Model")) {
        const char* modelNames[] = { "Blinn-Phong", "PBR (GGX)" };
        int model = static_cast<int>(m_RenderSystem->GetShadingModel());
        if (ImGui::Combo("Specular Model", &model, modelNames, 2)) {
            m_RenderSystem->SetShadingModel(static_cast<u32>(model));
        }
        ImGui::SetItemTooltip("Blinn-Phong: classic game lighting, tighter highlights.\nPBR (GGX): physically-based, softer highlights with long tails.");

        bool fresnel = m_RenderSystem->IsFresnelEnabled();
        if (ImGui::Checkbox("Fresnel##Shading", &fresnel)) {
            m_RenderSystem->SetFresnelEnabled(fresnel);
        }
        ImGui::SetItemTooltip("Fresnel-Schlick: surfaces reflect more light at glancing angles.\nAdds rim/edge sheen. Metals look distinct from plastics.\nCost: negligible.");

        bool energyConserv = m_RenderSystem->IsEnergyConservation();
        if (ImGui::Checkbox("Energy Conservation##Shading", &energyConserv)) {
            m_RenderSystem->SetEnergyConservation(energyConserv);
        }
        ImGui::SetItemTooltip("Ensures light out never exceeds light in.\nShiny surfaces get dimmer diffuse to balance bright specular.\nPrevents unnaturally glowing surfaces. Cost: free.");

        bool geomTerm = m_RenderSystem->IsGeometryTerm();
        if (ImGui::Checkbox("Geometry Term##Shading", &geomTerm)) {
            m_RenderSystem->SetGeometryTerm(geomTerm);
        }
        ImGui::SetItemTooltip("Smith GGX microfacet self-shadowing.\nDarkens rough surfaces at grazing angles where micro-bumps\nblock light. Prevents flat-plastic look. Cost: minimal.");

        bool halfLambert = m_RenderSystem->IsHalfLambert();
        if (ImGui::Checkbox("Half-Lambert##Shading", &halfLambert)) {
            m_RenderSystem->SetHalfLambert(halfLambert);
        }
        ImGui::SetItemTooltip("Valve's Half-Lambert: NdotL*0.5+0.5.\nSofter light falloff, no harsh shadow terminator.\nUsed in TF2, Source Engine. Great for stylized/hand-painted.");

        ImGui::Separator();
        const char* rampNames[] = { "Off", "Smooth Step", "Warm Shadow", "Cool Shadow", "Anime" };
        int ramp = static_cast<int>(m_RenderSystem->GetLightRampMode());
        if (ImGui::Combo("Light Ramp", &ramp, rampNames, 5)) {
            m_RenderSystem->SetLightRampMode(static_cast<f32>(ramp));
        }
        ImGui::SetItemTooltip("Stylized light ramp replaces raw NdotL:\n"
            "Smooth Step: hand-painted (TF2/Genshin style)\n"
            "Warm Shadow: amber-tinted shadows\n"
            "Cool Shadow: blue/purple shadows\n"
            "Anime: hard 2-band with pink-purple shadow");
    }
}

void EditorLayer::DrawSettingsSection_DreamcastEffects() {
    if (!m_RenderSystem) return;

    if (ImGui::CollapsingHeader("Dreamcast Effects")) {
        ImGui::TextDisabled("Sphere environment maps, modifier volume shadows — Dreamcast/6th-gen era");
        // Spherical Environment Mapping
        bool sphereEnv = m_RenderSystem->IsSphereEnvMapEnabled();
        if (ImGui::Checkbox("Sphere Environment Map", &sphereEnv)) {
            m_RenderSystem->SetSphereEnvMapEnabled(sphereEnv);
        }
        ImGui::SetItemTooltip("Dreamcast-style spherical environment mapping (matcap).\n"
            "Adds a metallic sheen based on view-space normals.\n"
            "Classic look from Sonic Adventure, Soul Calibur, Power Stone.");

        if (sphereEnv) {
            f32 envStr = m_RenderSystem->GetSphereEnvStrength();
            if (ImGui::SliderFloat("Env Strength", &envStr, 0.0f, 1.0f)) {
                m_RenderSystem->SetSphereEnvStrength(envStr);
            }
            ImGui::SetItemTooltip("Intensity of the sphere environment sheen.\n"
                "0 = off, 0.3 = subtle, 0.5 = default, 1.0 = full Dreamcast gloss.");
        }

        ImGui::Separator();

        // Color Posterization
        f32 posterize = m_RenderSystem->GetPosterizeLevels();
        bool posterizeOn = (posterize > 1.5f);
        if (ImGui::Checkbox("Color Posterization", &posterizeOn)) {
            m_RenderSystem->SetPosterizeLevels(posterizeOn ? 16.0f : 0.0f);
        }
        ImGui::SetItemTooltip("Quantize final colors to a limited palette per channel.\n"
            "Simulates VQ texture compression and palettized rendering\n"
            "from Dreamcast / Saturn / PS1 era hardware.");

        if (posterizeOn) {
            if (posterize < 2.0f) posterize = 16.0f;
            if (ImGui::SliderFloat("Color Levels", &posterize, 4.0f, 256.0f, "%.0f")) {
                m_RenderSystem->SetPosterizeLevels(posterize);
            }
            ImGui::SetItemTooltip("Color steps per channel.\n"
                "4 = extreme banding (64 total colors)\n"
                "16 = retro feel (4096 colors, Dreamcast-like)\n"
                "64 = subtle banding\n256 = nearly invisible.");
        }
    }
}

void EditorLayer::DrawSettingsSection_CelShading() {
    if (!m_RenderSystem) return;

    // === CEL SHADING ===
    if (ImGui::CollapsingHeader("Cel Shading")) {
        ImGui::TextDisabled("Anime, cartoon, comic book styles — band quantization + edge outlines");
        bool celEnabled = m_RenderSystem->IsCelShadingEnabled();
        if (ImGui::Checkbox("Cel Shading##Rendering", &celEnabled)) {
            m_RenderSystem->SetCelShadingEnabled(celEnabled);
        }
        ImGui::SetItemTooltip("Cartoon/toon-style shading. Quantizes lighting into\ndiscrete bands for a hand-drawn look.");
        if (celEnabled) {
            f32 celBands = m_RenderSystem->GetCelDiffuseBands();
            if (ImGui::SliderFloat("Diffuse Bands##Cel", &celBands, 2.0f, 8.0f, "%.0f")) {
                m_RenderSystem->SetCelDiffuseBands(celBands);
            }
            ImGui::SetItemTooltip("Number of distinct brightness steps.\n2 = stark light/shadow, 4-5 = common toon look, 8 = subtle banding.");
            f32 celSpec = m_RenderSystem->GetCelSpecularCutoff();
            if (ImGui::SliderFloat("Specular Cutoff##Cel", &celSpec, 0.0f, 1.0f)) {
                m_RenderSystem->SetCelSpecularCutoff(celSpec);
            }
            ImGui::SetItemTooltip("Hard threshold for specular highlights.\n0 = smooth specular (no cutoff), >0 = sharp on/off highlight.\nHigher values shrink the highlight.");

            const char* shadowModes[] = { "Off", "Purple", "Blue", "Warm", "Neutral Cool" };
            int shadowMode = static_cast<int>(m_RenderSystem->GetCelShadowMode());
            if (ImGui::Combo("Shadow Tint##Cel", &shadowMode, shadowModes, 5)) {
                m_RenderSystem->SetCelShadowMode(static_cast<f32>(shadowMode));
            }
            ImGui::SetItemTooltip("Tint shadow areas instead of pure black.\nGives a more stylized anime/cartoon look.");
        }

        ImGui::Separator();
        bool geomOutlines = m_RenderSystem->IsGeometryOutlinesEnabled();
        if (ImGui::Checkbox("Geometry Outlines##Cel", &geomOutlines)) {
            m_RenderSystem->SetGeometryOutlinesEnabled(geomOutlines);
        }
        ImGui::SetItemTooltip("Inverted-hull outlines: extrude backfaces along normals\nfor thick, resolution-independent outlines around geometry.");
        if (geomOutlines) {
            f32 outWidth = m_RenderSystem->GetGeometryOutlineWidth();
            if (ImGui::SliderFloat("Outline Width##GeomOutline", &outWidth, 0.001f, 0.1f, "%.3f")) {
                m_RenderSystem->SetGeometryOutlineWidth(outWidth);
            }
            ImGui::SetItemTooltip("Extrusion distance in world units.\n0.01-0.03 typical for characters, 0.05+ for stylized look.");
            Math::Vector3 outColor = m_RenderSystem->GetGeometryOutlineColor();
            if (ImGui::ColorEdit3("Outline Color##GeomOutline", &outColor.x)) {
                m_RenderSystem->SetGeometryOutlineColor(outColor);
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
        ImGui::SetItemTooltip("Render triangle edges only. Useful for debugging mesh topology.");

        // HDR output
        ImGui::Separator();
        {
            auto* swapchain = m_RenderSystem->GetSwapchain();
            bool hdrAvailable = swapchain && swapchain->IsHDRFormatAvailable();
            bool hdrEnabled = m_RenderSystem->IsHDREnabled();

            if (!hdrAvailable && !hdrEnabled) {
                ImGui::BeginDisabled();
            }
            if (ImGui::Checkbox("HDR Output", &hdrEnabled)) {
                m_RenderSystem->SetHDREnabled(hdrEnabled);
                // Update post-process settings with actual HDR mode
                if (m_PostProcessing) {
                    m_PostProcessing->GetSettings().hdrOutputMode = m_RenderSystem->GetHDROutputMode();
                }
            }
            if (!hdrAvailable && !hdrEnabled) {
                ImGui::EndDisabled();
            }
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                ImGui::SetTooltip(hdrAvailable
                    ? "Enable HDR output on compatible displays.\n"
                      "Prefers scRGB (FP16 linear), falls back to HDR10 (PQ/ST.2084)."
                    : "HDR not available.\n"
                      "Enable Windows HDR in Settings > Display > HDR first.");
            }
            if (m_RenderSystem->IsHDREnabled()) {
                const char* modeNames[] = { "SDR", "scRGB (FP16)", "HDR10 (PQ)" };
                u32 mode = m_RenderSystem->GetHDROutputMode();
                ImGui::TextColored(ImVec4(0.5f, 0.9f, 0.5f, 1.0f), "Active: %s",
                    mode < 3 ? modeNames[mode] : "Unknown");
            } else if (!hdrAvailable) {
                ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "No HDR formats detected");
            }
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
            ImGui::TextDisabled("Hardware-accelerated shadows, reflections, AO, GI, path tracing");
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
                ImGui::SetItemTooltip("Enable hardware-accelerated ray tracing (VK_KHR_ray_tracing_pipeline).\nAdds RT shadows, reflections, AO, GI, translucency, and caustics.");

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
                                    ImGui::Checkbox("SDF Distance Fallback", &cfg.sdfFallback);
                                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Use SDF sphere tracing for reflections beyond RT BVH range");
                                    if (cfg.sdfFallback) {
                                        ImGui::SliderFloat("SDF Max Distance", &cfg.sdfMaxDistance, 100.0f, 2000.0f);
                                    }
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
                                    ImGui::SetItemTooltip("World-space radius for AO rays. Larger = softer, wider occlusion.");
                                    ImGui::DragFloat("AO Power", &cfg.power, 0.1f, 0.1f, 5.0f);
                                    ImGui::SetItemTooltip("Exponent applied to AO result. Higher = darker, more contrast.");
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
                                    ImGui::SetItemTooltip("Indirect light brightness multiplier.");
                                    int bounces = static_cast<int>(cfg.bounces);
                                    if (ImGui::SliderInt("Bounces", &bounces, 1, 4)) {
                                        cfg.bounces = static_cast<u32>(bounces);
                                    }
                                }
                                ImGui::TreePop();
                            }
                        }

                        // Radiance Cache (Screen-Space Irradiance Caching)
                        if (auto* radianceCache = m_RenderSystem->GetRadianceCache()) {
                            auto& cfg = radianceCache->GetConfig();
                            if (ImGui::TreeNode("Radiance Cache")) {
                                ImGui::Checkbox("Enabled##RadianceCache", &cfg.enabled);
                                if (ImGui::IsItemHovered()) ImGui::SetTooltip(
                                    "Screen-space radiance cache for indirect lighting.\n"
                                    "Caches GI results per tile and reuses across frames.\n"
                                    "Reduces GI cost when the camera is mostly stationary.\n"
                                    "Directional light is excluded (evaluated separately).");
                                if (cfg.enabled) {
                                    int tileSize = static_cast<int>(cfg.tileSize);
                                    const char* tileSizeLabels[] = { "16", "32", "64" };
                                    int tileSizeValues[] = { 16, 32, 64 };
                                    int tileSizeIdx = (tileSize == 16) ? 0 : (tileSize == 64) ? 2 : 1;
                                    if (ImGui::Combo("Tile Size##RC", &tileSizeIdx, tileSizeLabels, 3)) {
                                        cfg.tileSize = static_cast<u32>(tileSizeValues[tileSizeIdx]);
                                    }
                                    if (ImGui::IsItemHovered()) ImGui::SetTooltip(
                                        "Pixel size of each cache tile.\n"
                                        "Smaller = more accurate but more memory/compute.\n"
                                        "32 is a good default.");
                                    ImGui::DragFloat("Max Age##RC", &cfg.maxAge, 0.5f, 1.0f, 32.0f, "%.0f frames");
                                    if (ImGui::IsItemHovered()) ImGui::SetTooltip(
                                        "Maximum frames before a cache tile is forced to re-trace.\n"
                                        "Lower = fresher results but less savings.");
                                    ImGui::DragFloat("Depth Threshold##RC", &cfg.depthThreshold, 0.01f, 0.01f, 0.5f, "%.3f");
                                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Relative depth difference that invalidates a tile.");
                                    ImGui::DragFloat("Normal Threshold##RC", &cfg.normalThreshold, 0.01f, 0.5f, 1.0f, "%.3f");
                                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Dot product threshold for normal similarity.");
                                    ImGui::DragFloat("Hysteresis##RC", &cfg.hysteresis, 0.01f, 0.0f, 1.0f, "%.2f");
                                    if (ImGui::IsItemHovered()) ImGui::SetTooltip(
                                        "Temporal blend factor.\n"
                                        "0 = no reuse (always use fresh GI).\n"
                                        "1 = full reuse (never update from fresh GI).\n"
                                        "0.9 is a good default.");
                                    ImGui::Checkbox("Exclude Directional Light##RC", &cfg.excludeDirectional);
                                    if (ImGui::IsItemHovered()) ImGui::SetTooltip(
                                        "Keep sun/moon out of the cache.\n"
                                        "Critical for correct time-of-day transitions.");

                                    ImGui::Separator();
                                    ImGui::TextDisabled("Cache: %ux%u tiles (%u total)",
                                        radianceCache->GetTileCountX(),
                                        radianceCache->GetTileCountY(),
                                        radianceCache->GetTotalTileCount());

                                    if (ImGui::Button("Invalidate Cache##RC")) {
                                        radianceCache->InvalidateAll();
                                    }
                                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Force re-trace of all tiles next frame.");
                                }
                                ImGui::TreePop();
                            }
                        }

                        // Surfel Radiance Cache (World-Space Irradiance Caching)
                        if (auto* surfelCache = m_RenderSystem->GetSurfelRadianceCache()) {
                            auto& cfg = surfelCache->GetConfig();
                            if (ImGui::TreeNode("Surfel Radiance Cache")) {
                                ImGui::Checkbox("Enabled##SurfelRC", &cfg.enabled);
                                if (ImGui::IsItemHovered()) ImGui::SetTooltip(
                                    "World-space surfel-based radiance cache.\n"
                                    "Distributes surface elements (surfels) on visible geometry\n"
                                    "and caches indirect irradiance that persists across frames\n"
                                    "and camera movement. Supplements the screen-space cache.\n"
                                    "Directional light is excluded (evaluated separately).");
                                if (cfg.enabled) {
                                    int maxSurfels = static_cast<int>(cfg.maxSurfels);
                                    if (ImGui::DragInt("Max Surfels##SRC", &maxSurfels, 1024, 4096, 262144)) {
                                        cfg.maxSurfels = static_cast<u32>(maxSurfels);
                                    }
                                    if (ImGui::IsItemHovered()) ImGui::SetTooltip(
                                        "Maximum number of surfels (surface elements).\n"
                                        "64K is a good default for indie scenes.\n"
                                        "Higher = better coverage but more memory/compute.");
                                    ImGui::DragFloat("Surfel Radius##SRC", &cfg.surfelRadius, 0.05f, 0.1f, 5.0f, "%.2f");
                                    if (ImGui::IsItemHovered()) ImGui::SetTooltip(
                                        "World-space coverage radius per surfel.\n"
                                        "Smaller = more detail but needs more surfels.\n"
                                        "Larger = fewer surfels but lower detail.");
                                    ImGui::DragFloat("Camera Radius##SRC", &cfg.cameraRadius, 1.0f, 10.0f, 200.0f, "%.0f");
                                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Only maintain surfels within this radius of the camera.");
                                    ImGui::DragFloat("Blend Weight##SRC", &cfg.blendWeight, 0.01f, 0.0f, 1.0f, "%.2f");
                                    if (ImGui::IsItemHovered()) ImGui::SetTooltip(
                                        "Blend with screen-space cache.\n"
                                        "0 = screen-space only, 1 = surfel only.\n"
                                        "0.5 is a good starting point.");
                                    ImGui::DragFloat("Update Fraction##SRC", &cfg.updateFraction, 0.01f, 0.01f, 1.0f, "%.3f");
                                    if (ImGui::IsItemHovered()) ImGui::SetTooltip(
                                        "Fraction of surfels updated per frame.\n"
                                        "0.125 = 1/8 of surfels per frame (amortized).\n"
                                        "Higher = fresher results but more compute.");
                                    ImGui::DragFloat("Max Age##SRC", &cfg.maxAge, 1.0f, 4.0f, 128.0f, "%.0f frames");
                                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Frames before a surfel expires if not refreshed.");
                                    ImGui::DragFloat("Normal Threshold##SRC", &cfg.normalThreshold, 0.01f, 0.3f, 1.0f, "%.2f");
                                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Dot product threshold for surfel-surface alignment.");

                                    int raysPerSurfel = static_cast<int>(cfg.raysPerSurfel);
                                    if (ImGui::SliderInt("Rays/Surfel##SRC", &raysPerSurfel, 1, 4)) {
                                        cfg.raysPerSurfel = static_cast<u32>(raysPerSurfel);
                                    }
                                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Hemisphere rays traced per surfel per update (1-4).");

                                    int placementInterval = static_cast<int>(cfg.placementInterval);
                                    if (ImGui::SliderInt("Placement Interval##SRC", &placementInterval, 1, 16)) {
                                        cfg.placementInterval = static_cast<u32>(placementInterval);
                                    }
                                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Re-evaluate surfel placement every N frames.");

                                    ImGui::Checkbox("Exclude Directional Light##SRC", &cfg.excludeDirectional);
                                    if (ImGui::IsItemHovered()) ImGui::SetTooltip(
                                        "Keep sun/moon out of surfel irradiance.\n"
                                        "Critical for correct time-of-day transitions.");

                                    ImGui::Separator();
                                    ImGui::TextDisabled("Active surfels: %u / %u",
                                        surfelCache->GetActiveSurfelCount(),
                                        surfelCache->GetMaxSurfels());

                                    if (ImGui::Button("Invalidate Surfels##SRC")) {
                                        surfelCache->InvalidateAll();
                                    }
                                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Clear all surfels and start fresh.");
                                }
                                ImGui::TreePop();
                            }
                        }

                        // ReSTIR (Importance-Weighted Light Selection)
                        if (auto* restir = m_RenderSystem->GetReSTIR()) {
                            auto& cfg = restir->GetConfig();
                            if (ImGui::TreeNode("ReSTIR Light Selection")) {
                                ImGui::Checkbox("Enabled##ReSTIR", &cfg.enabled);
                                if (ImGui::IsItemHovered()) ImGui::SetTooltip(
                                    "Reservoir-based importance-weighted light selection.\n"
                                    "Selects the most impactful light per pixel for RT shadow/GI rays.\n"
                                    "Reduces noise when many lights are present.");
                                if (cfg.enabled) {
                                    int candidates = static_cast<int>(cfg.initialCandidates);
                                    if (ImGui::SliderInt("Candidates##ReSTIR", &candidates, 1, 32)) {
                                        cfg.initialCandidates = static_cast<u32>(candidates);
                                    }
                                    if (ImGui::IsItemHovered()) ImGui::SetTooltip(
                                        "Number of random light candidates to evaluate per pixel.\n"
                                        "Higher = better selection, more compute cost.\n"
                                        "8 is a good default for scenes with many lights.");
                                    ImGui::DragFloat("Distance Bias##ReSTIR", &cfg.distanceBias, 0.01f, 0.001f, 1.0f, "%.3f");
                                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Minimum distance for falloff calculation (prevents division by zero)");

                                    ImGui::Separator();
                                    ImGui::Text("Temporal Reuse");
                                    ImGui::Checkbox("Temporal Reuse##ReSTIR", &cfg.temporalReuse);
                                    if (ImGui::IsItemHovered()) ImGui::SetTooltip(
                                        "Reproject previous frame's light selection via motion vectors.\n"
                                        "Dramatically improves convergence across frames.\n"
                                        "Requires motion vector MRT output.");
                                    if (cfg.temporalReuse) {
                                        int maxHistory = static_cast<int>(cfg.temporalMaxHistory);
                                        if (ImGui::SliderInt("Max History##ReSTIR", &maxHistory, 5, 60)) {
                                            cfg.temporalMaxHistory = static_cast<u32>(maxHistory);
                                        }
                                        if (ImGui::IsItemHovered()) ImGui::SetTooltip(
                                            "Maximum temporal sample count (M_max).\n"
                                            "Higher = smoother but slower to adapt to lighting changes.\n"
                                            "20-30 is a good default.");
                                        ImGui::DragFloat("Depth Threshold##ReSTIRTemporal", &cfg.temporalDepthThreshold, 0.01f, 0.01f, 0.5f, "%.2f");
                                        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Relative depth ratio threshold for temporal reprojection validity");
                                        ImGui::DragFloat("Normal Threshold##ReSTIRTemporal", &cfg.temporalNormalThreshold, 0.01f, 0.5f, 1.0f, "%.2f");
                                        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Minimum normal dot product for temporal reprojection validity");
                                    }

                                    ImGui::Separator();
                                    ImGui::Text("Spatial Reuse");
                                    ImGui::Checkbox("Spatial Reuse##ReSTIR", &cfg.spatialReuse);
                                    if (ImGui::IsItemHovered()) ImGui::SetTooltip(
                                        "Share light selection between similar neighboring pixels.\n"
                                        "Improves convergence by leveraging spatial coherence.\n"
                                        "Small compute cost per additional neighbor.");
                                    if (cfg.spatialReuse) {
                                        int neighbors = static_cast<int>(cfg.spatialNeighbors);
                                        if (ImGui::SliderInt("Neighbors##ReSTIR", &neighbors, 1, 16)) {
                                            cfg.spatialNeighbors = static_cast<u32>(neighbors);
                                        }
                                        if (ImGui::IsItemHovered()) ImGui::SetTooltip(
                                            "Number of random neighbors to sample (K).\n"
                                            "Higher = better quality, more compute cost.\n"
                                            "5-8 is a good balance.");
                                        ImGui::DragFloat("Radius##ReSTIRSpatial", &cfg.spatialRadius, 1.0f, 5.0f, 100.0f, "%.0f px");
                                        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Screen-space search radius for neighbor sampling (pixels)");
                                        ImGui::DragFloat("Depth Threshold##ReSTIRSpatial", &cfg.spatialDepthThreshold, 0.01f, 0.01f, 0.5f, "%.2f");
                                        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Relative depth ratio threshold for neighbor similarity");
                                        ImGui::DragFloat("Normal Threshold##ReSTIRSpatial", &cfg.spatialNormalThreshold, 0.01f, 0.5f, 1.0f, "%.2f");
                                        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Minimum normal dot product for neighbor similarity");
                                    }
                                }
                                ImGui::TreePop();
                            }
                        }

                        // Temporal Reuse (motion-vector-based RT result reuse)
                        if (auto* temporalReuse = m_RenderSystem->GetRTTemporalReuse()) {
                            auto& cfg = temporalReuse->GetConfig();
                            if (ImGui::TreeNode("Temporal Reuse")) {
                                ImGui::Checkbox("Enabled##TemporalReuse", &cfg.enabled);
                                if (ImGui::IsItemHovered()) ImGui::SetTooltip(
                                    "Carry ray-traced results across frames using motion vectors.\n"
                                    "Reduces noise by blending with reprojected previous frame data.\n"
                                    "Disoccluded surfaces are automatically detected and re-traced.");
                                if (cfg.enabled) {
                                    ImGui::SliderFloat("History Blend", &cfg.historyLength, 0.0f, 0.99f, "%.2f");
                                    if (ImGui::IsItemHovered()) ImGui::SetTooltip(
                                        "How much previous frame data to keep.\n"
                                        "0.9 = 90%% history, 10%% new (smoothest, most lag).\n"
                                        "0.5 = 50/50 blend. Lower = more responsive, more noise.");
                                    ImGui::SliderFloat("Depth Threshold", &cfg.disocclusionThreshold, 0.01f, 0.5f, "%.2f");
                                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Depth ratio threshold for disocclusion detection.\nLower = stricter rejection of stale history.");
                                    ImGui::SliderFloat("Normal Threshold", &cfg.normalThreshold, 0.5f, 1.0f, "%.2f");
                                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Normal dot-product threshold.\nHigher = stricter rejection at surface edges.");
                                    ImGui::Separator();
                                    ImGui::Checkbox("Shadows##TReuse", &cfg.reuseShadows);
                                    ImGui::SameLine();
                                    ImGui::Checkbox("AO##TReuse", &cfg.reuseAO);
                                    ImGui::SameLine();
                                    ImGui::Checkbox("Reflections##TReuse", &cfg.reuseReflections);
                                    ImGui::SameLine();
                                    ImGui::Checkbox("GI##TReuse", &cfg.reuseGI);
                                    if (ImGui::Button("Reset History##TReuse")) {
                                        temporalReuse->ResetHistory();
                                    }
                                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Force re-trace all pixels next frame (e.g. after camera cut).");
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
                            ImGui::SetItemTooltip("Maximum light bounces per path.\nMore bounces = more realistic indirect light, slower convergence.\n4 is a good default, 8+ for interiors.");
                            int targetSPP = static_cast<int>(cfg.targetSPP);
                            if (ImGui::DragInt("Target SPP", &targetSPP, 16, 1, 65536)) {
                                cfg.targetSPP = static_cast<u32>(targetSPP);
                            }
                            ImGui::SetItemTooltip("Samples Per Pixel target for convergence.\nHigher = cleaner image, longer render time.\n256 preview, 1024 default, 4096+ production.");

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
                        ImGui::SetItemTooltip("SVGF: GPU temporal+spatial filter, fast, always available.\nOIDN: Intel ML denoiser, higher quality, runs on CPU.");
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

                // Simplified RT Materials
                {
                    ImGui::Separator();
                    auto settings = Renderer::SceneRenderSettings::CaptureFromRuntime(
                        m_RenderSystem, m_PostProcessing ? &m_PostProcessing->GetSettings() : nullptr);
                    bool changed = false;
                    if (ImGui::Checkbox("Simplified RT Materials", &settings.rtSimplifiedMaterials)) {
                        changed = true;
                    }
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Pre-bake material properties to reduce hit shader divergence");
                    if (settings.rtSimplifiedMaterials) {
                        int bounce = static_cast<int>(settings.rtSimplifyAfterBounce);
                        if (ImGui::SliderInt("Simplify After Bounce", &bounce, 0, 4)) {
                            settings.rtSimplifyAfterBounce = static_cast<u32>(bounce);
                            changed = true;
                        }
                        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Use simplified materials (no SSS/transmission) after this bounce depth");
                    }
                    if (changed) {
                        settings.ApplyToRuntime(m_RenderSystem,
                            m_PostProcessing ? &m_PostProcessing->GetSettings() : nullptr);
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
            ImGui::TextDisabled("Baked indirect lighting for static scenes — interiors, walkthroughs");
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
